#include <cerrno>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

#include "process_memory_dumper/memory_maps.h"
#include "process_memory_dumper/process_vm_reader.h"

namespace process_memory_dump {
namespace {

constexpr size_t kProbeBytes = 16U;
constexpr size_t kFastChunkSize = 256U * 1024U;
constexpr size_t kMaxBufferedRegion = 128U * 1024U * 1024U;

size_t GetPageSize() {
    const long value = ::sysconf(_SC_PAGESIZE);
    return (value > 0) ? static_cast<size_t>(value) : 4096U;
}

std::string MakeMapPath(const std::string& output_path) {
    return output_path + ".map";
}

bool WriteMapHeader(std::ofstream& map, const char* method, pid_t pid) {
    map << "# process_memory_dump metadata\n";
    map << "# method: " << method << '\n';
    map << "# pid: " << pid << '\n';
    map << "# columns: start-end perms requested_bytes written_bytes status pathname\n";
    return map.good();
}

bool WriteMapLine(
    std::ofstream& map,
    const MemoryRegion& region,
    size_t requested_bytes,
    size_t written_bytes,
    const std::string& status
) {
    map << std::hex << region.start << '-' << region.end
        << std::dec << ' '
        << region.PermissionsString() << ' '
        << requested_bytes << ' '
        << written_bytes << ' '
        << status;

    if (!region.pathname.empty()) {
        map << ' ' << region.pathname;
    }

    map << '\n';
    return map.good();
}

bool IsRecoverableReadError(int error_code) {
    return error_code == EFAULT || error_code == EIO || error_code == ENOMEM;
}

} // namespace

ProcessVmReader::ProcessVmReader() {
    resetStats();
}

ProcessVmReader::~ProcessVmReader() = default;

void ProcessVmReader::resetStats() {
    last_stats_ = {};
}

bool ProcessVmReader::CanAccessProcess(pid_t pid) {
    auto regions_opt = MemoryMapsParser::Parse(pid);
    if (!regions_opt.has_value()) {
        return false;
    }

    const auto readable = MemoryMapsParser::FilterReadable(*regions_opt);
    std::vector<uint8_t> probe_buffer(kProbeBytes, 0);

    for (const auto& region : readable) {
        if (MemoryMapsParser::ShouldSkipRegion(region) || region.Size() == 0U) {
            continue;
        }

        const size_t probe_size = std::min(kProbeBytes, region.Size());

        struct iovec local {
            probe_buffer.data(),
            probe_size
        };

        struct iovec remote {
            reinterpret_cast<void*>(region.start),
            probe_size
        };

        errno = 0;
        const ssize_t result = ::process_vm_readv(pid, &local, 1, &remote, 1, 0);
        if (result > 0) {
            return true;
        }

        if (result == -1 && (errno == EPERM || errno == ESRCH || errno == ENOSYS || errno == EINVAL)) {
            return false;
        }
    }

    return false;
}

bool ProcessVmReader::ReadRegion(
    pid_t pid,
    const MemoryRegion& region,
    std::vector<uint8_t>& out
) {
    resetStats();

    out.clear();
    if (!region.readable || region.Size() == 0U) {
        return false;
    }

    if (region.Size() > kMaxBufferedRegion) {
        std::cerr << "ReadRegion refused: region is too large for in-memory buffering ("
                  << region.Size() << " bytes). Use DumpProcess instead.\n";
        return false;
    }

    try {
        out.reserve(region.Size());
    } catch (const std::exception& ex) {
        std::cerr << "Failed to reserve buffer for region: " << ex.what() << '\n';
        return false;
    }

    bool had_zero_fill = false;
    size_t bytes_written = 0;

    const auto writer = [&out](const uint8_t* data, size_t size) -> bool {
        try {
            out.insert(out.end(), data, data + size);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    };

    return DumpRegionWithWriter(pid, region, writer, had_zero_fill, bytes_written);
}

std::optional<std::vector<MemoryRegion>> ProcessVmReader::GetMemoryMaps(pid_t pid) {
    return MemoryMapsParser::Parse(pid);
}

bool ProcessVmReader::DumpRegionWithWriter(
    pid_t pid,
    const MemoryRegion& region,
    const ChunkWriter& writer,
    bool& had_zero_fill,
    size_t& bytes_written
) {
    had_zero_fill = false;
    bytes_written = 0;

    if (!region.readable || region.Size() == 0U) {
        return false;
    }

    last_stats_.total_bytes_requested += region.Size();

    const size_t page_size = GetPageSize();
    std::vector<uint8_t> fast_buffer(kFastChunkSize, 0);
    std::vector<uint8_t> page_buffer(page_size, 0);
    std::vector<uint8_t> zero_page(page_size, 0);

    uintptr_t current = region.start;

    while (current < region.end) {
        const size_t remaining = static_cast<size_t>(region.end - current);
        const size_t chunk_size = std::min(kFastChunkSize, remaining);

        struct iovec local {
            fast_buffer.data(),
            chunk_size
        };
        struct iovec remote {
            reinterpret_cast<void*>(current),
            chunk_size
        };

        errno = 0;
        const ssize_t fast_result = ::process_vm_readv(pid, &local, 1, &remote, 1, 0);

        if (fast_result > 0) {
            const size_t got = static_cast<size_t>(fast_result);
            if (!writer(fast_buffer.data(), got)) {
                return false;
            }

            last_stats_.total_bytes_read += got;
            bytes_written += got;
            current += got;
            continue;
        }

        const int fast_errno = errno;

        if (fast_result == -1 && !IsRecoverableReadError(fast_errno)) {
            return false;
        }

        size_t processed_in_chunk = 0;
        while (processed_in_chunk < chunk_size) {
            const size_t page_len = std::min(page_size, chunk_size - processed_in_chunk);
            const uintptr_t page_addr = current + processed_in_chunk;

            struct iovec page_local {
                page_buffer.data(),
                page_len
            };
            struct iovec page_remote {
                reinterpret_cast<void*>(page_addr),
                page_len
            };

            errno = 0;
            const ssize_t page_result = ::process_vm_readv(pid, &page_local, 1, &page_remote, 1, 0);

            if (page_result > 0) {
                const size_t got = static_cast<size_t>(page_result);

                if (!writer(page_buffer.data(), got)) {
                    return false;
                }

                last_stats_.total_bytes_read += got;
                bytes_written += got;

                if (got < page_len) {
                    const size_t missing = page_len - got;
                    if (!writer(zero_page.data(), missing)) {
                        return false;
                    }

                    last_stats_.total_bytes_zero_filled += missing;
                    bytes_written += missing;
                    had_zero_fill = true;
                }
            } else {
                const int page_errno = errno;

                if (page_result == -1 && !IsRecoverableReadError(page_errno) && page_errno != 0) {
                    return false;
                }

                if (!writer(zero_page.data(), page_len)) {
                    return false;
                }

                last_stats_.total_bytes_zero_filled += page_len;
                bytes_written += page_len;
                had_zero_fill = true;
            }

            processed_in_chunk += page_len;
        }

        current += chunk_size;
    }

    return true;
}

bool ProcessVmReader::DumpProcess(pid_t pid, const std::string& output_path) {
    resetStats();

    auto regions_opt = GetMemoryMaps(pid);
    if (!regions_opt.has_value()) {
        return false;
    }

    const auto readable = MemoryMapsParser::FilterReadable(*regions_opt);

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Cannot open output file: " << output_path << '\n';
        return false;
    }

    const std::string map_path = MakeMapPath(output_path);
    std::ofstream map(map_path, std::ios::trunc);
    if (!map) {
        std::cerr << "Cannot open metadata file: " << map_path << '\n';
        return false;
    }

    if (!WriteMapHeader(map, GetMethodName(), pid)) {
        std::cerr << "Failed to write metadata header\n";
        return false;
    }

    std::cout << "Dumping process " << pid << " using " << GetMethodName() << '\n';
    std::cout << "Found " << regions_opt->size() << " regions, "
              << readable.size() << " readable\n";

    bool any_success = false;
    size_t region_num = 0;

    for (const auto& region : readable) {
        if (MemoryMapsParser::ShouldSkipRegion(region)) {
            std::cout << "  Skipping: " << region.pathname << '\n';
            if (!WriteMapLine(map, region, region.Size(), 0, "skipped")) {
                std::cerr << "Failed to write metadata for skipped region\n";
                return false;
            }
            continue;
        }

        ++last_stats_.regions_attempted;

        bool had_zero_fill = false;
        size_t bytes_written = 0;

        const auto writer = [&out](const uint8_t* data, size_t size) -> bool {
            out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
            return out.good();
        };

        const bool ok = DumpRegionWithWriter(pid, region, writer, had_zero_fill, bytes_written);
        const std::string status = ok
            ? (had_zero_fill ? "partial-zero-fill" : "ok")
            : "failed";

        if (ok) {
            ++last_stats_.regions_success;
            any_success = true;
            std::cout << "  [" << ++region_num << "] "
                      << std::hex << region.start << "-" << region.end
                      << std::dec << " (" << bytes_written << " bytes) "
                      << status
                      << (region.pathname.empty() ? "" : " ")
                      << region.pathname
                      << '\n';
        } else {
            ++last_stats_.regions_failed;
            std::cout << "  [FAIL] "
                      << std::hex << region.start << "-" << region.end
                      << std::dec << ' '
                      << (region.pathname.empty() ? "" : region.pathname)
                      << '\n';
        }

        if (!WriteMapLine(map, region, region.Size(), bytes_written, status)) {
            std::cerr << "Failed to write metadata line\n";
            return false;
        }

        if (!out.good()) {
            std::cerr << "Write error while writing dump file\n";
            return false;
        }
    }

    out.flush();
    map.flush();

    if (!out.good() || !map.good()) {
        std::cerr << "Flush error while finalizing output files\n";
        return false;
    }

    std::cout << "\nStats:\n";
    std::cout << "  Total requested: " << last_stats_.total_bytes_requested << " bytes\n";
    std::cout << "  Total read: " << last_stats_.total_bytes_read << " bytes\n";
    std::cout << "  Zero-filled: " << last_stats_.total_bytes_zero_filled << " bytes\n";
    std::cout << "  Success: " << last_stats_.regions_success
              << "/" << last_stats_.regions_attempted << " regions\n";
    std::cout << "  Metadata: " << map_path << '\n';

    return any_success;
}

} // namespace process_memory_dump