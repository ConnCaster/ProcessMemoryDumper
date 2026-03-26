#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "process_memory_dumper/memory_maps.h"
#include "process_memory_dumper/proc_mem_reader.h"

namespace process_memory_dump {
namespace {

constexpr size_t kMaxBufferedRegion = 128U * 1024U * 1024U;

size_t GetPageSize() {
    const long value = ::sysconf(_SC_PAGESIZE);
    return (value > 0) ? static_cast<size_t>(value) : 4096U;
}

std::string MakeMemPath(pid_t pid) {
    return "/proc/" + std::to_string(pid) + "/mem";
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
    return error_code == EIO || error_code == EFAULT;
}

} // namespace

ProcMemReader::ProcMemReader() {
    resetStats();
}

ProcMemReader::~ProcMemReader() = default;

void ProcMemReader::resetStats() {
    last_stats_ = {};
}

bool ProcMemReader::CanAccessProcess(pid_t pid) {
    const std::string mem_path = MakeMemPath(pid);
    const int fd = ::open(mem_path.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    auto regions_opt = MemoryMapsParser::Parse(pid);
    if (!regions_opt.has_value()) {
        ::close(fd);
        return false;
    }

    const auto readable = MemoryMapsParser::FilterReadable(*regions_opt);
    uint8_t probe = 0;

    for (const auto& region : readable) {
        if (MemoryMapsParser::ShouldSkipRegion(region) || region.Size() == 0U) {
            continue;
        }

        errno = 0;
        const ssize_t result = ::pread(
            fd,
            &probe,
            1,
            static_cast<off_t>(region.start)
        );

        if (result > 0) {
            ::close(fd);
            return true;
        }

        if (result == -1 && (errno == EPERM || errno == EACCES || errno == ESRCH)) {
            ::close(fd);
            return false;
        }
    }

    ::close(fd);
    return false;
}

bool ProcMemReader::ReadRegion(
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

    const std::string mem_path = MakeMemPath(pid);
    const int fd = ::open(mem_path.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Cannot open " << mem_path << ": " << std::strerror(errno) << '\n';
        return false;
    }

    try {
        out.reserve(region.Size());
    } catch (const std::exception& ex) {
        ::close(fd);
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

    const bool ok = DumpRegionWithWriter(fd, region, writer, had_zero_fill, bytes_written);
    ::close(fd);
    return ok;
}

std::optional<std::vector<MemoryRegion>> ProcMemReader::GetMemoryMaps(pid_t pid) {
    return MemoryMapsParser::Parse(pid);
}

bool ProcMemReader::DumpRegionWithWriter(
    int fd,
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
    std::vector<uint8_t> page_buffer(page_size, 0);
    std::vector<uint8_t> zero_page(page_size, 0);

    size_t offset = 0;

    while (offset < region.Size()) {
        const size_t page_len = std::min(page_size, region.Size() - offset);
        const off_t file_offset = static_cast<off_t>(region.start + offset);

        errno = 0;
        const ssize_t result = ::pread(
            fd,
            page_buffer.data(),
            page_len,
            file_offset
        );

        if (result > 0) {
            const size_t got = static_cast<size_t>(result);

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

            offset += page_len;
            continue;
        }

        if (result == 0) {
            if (!writer(zero_page.data(), page_len)) {
                return false;
            }

            last_stats_.total_bytes_zero_filled += page_len;
            bytes_written += page_len;
            had_zero_fill = true;
            offset += page_len;
            continue;
        }

        const int read_errno = errno;
        if (!IsRecoverableReadError(read_errno)) {
            return false;
        }

        if (!writer(zero_page.data(), page_len)) {
            return false;
        }

        last_stats_.total_bytes_zero_filled += page_len;
        bytes_written += page_len;
        had_zero_fill = true;
        offset += page_len;
    }

    return true;
}

bool ProcMemReader::DumpProcess(pid_t pid, const std::string& output_path) {
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

    const std::string mem_path = MakeMemPath(pid);
    const int fd = ::open(mem_path.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Cannot open " << mem_path << ": " << std::strerror(errno) << '\n';
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
                ::close(fd);
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

        const bool ok = DumpRegionWithWriter(fd, region, writer, had_zero_fill, bytes_written);
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
            ::close(fd);
            std::cerr << "Failed to write metadata line\n";
            return false;
        }

        if (!out.good()) {
            ::close(fd);
            std::cerr << "Write error while writing dump file\n";
            return false;
        }
    }

    ::close(fd);

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