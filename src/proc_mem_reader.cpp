#include "proc_mem_reader.h"
#include "memory_maps.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>

namespace process_memory_dump {

ProcMemReader::ProcMemReader() {
    resetStats();
}

ProcMemReader::~ProcMemReader() = default;

void ProcMemReader::resetStats() {
    last_stats_ = {0, 0, 0, 0, 0};
}

bool ProcMemReader::isAvailable() {
    // Проверяем, можем ли открыть /proc/1/mem
    int fd = open("/proc/1/mem", O_RDONLY);
    if (fd != -1) {
        close(fd);
        return true;
    }
    return false;
}

bool ProcMemReader::ReadRegion(pid_t pid, const MemoryRegion& region, std::vector<uint8_t>& out) {
    std::string mem_path = "/proc/" + std::to_string(pid) + "/mem";
    
    int fd = open(mem_path.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    out.resize(region.Size());
    last_stats_.total_bytes_requested += out.size();
    last_stats_.regions_attempted++;

    // Читаем постранично для обработки EIO
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t offset = 0;
    size_t total_read = 0;

    while (offset < out.size()) {
        size_t chunk = std::min(page_size, out.size() - offset);
        ssize_t result = pread(fd, out.data() + offset, chunk, region.start + offset);
        
        if (result == -1) {
            if (errno == EIO) {
                // Страница недоступна, заполняем нулями
                std::memset(out.data() + offset, 0, chunk);
                offset += chunk;
                continue;
            }
            close(fd);
            last_stats_.regions_failed++;
            return false;
        }
        
        total_read += result;
        offset += result;
    }

    close(fd);
    last_stats_.total_bytes_read += total_read;
    last_stats_.regions_success++;
    return true;
}

std::optional<std::vector<MemoryRegion>> ProcMemReader::GetMemoryMaps(pid_t pid) {
    return MemoryMapsParser::Parse(pid);
}

bool ProcMemReader::DumpProcess(pid_t pid, const std::string& output_path) {
    // Реализация аналогична ProcessVmReader
    // Можно вынести в базовый класс для переиспользования
    resetStats();
    
    auto regions = GetMemoryMaps(pid);
    if (!regions.has_value()) {
        return false;
    }
    auto readable = MemoryMapsParser::FilterReadable(*regions);
    
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        return false;
    }

    std::cout << "Dumping process " << pid << " using " << GetMethodName() << std::endl;
    std::cout << "Found " << regions->size() << " regions, "
              << readable.size() << " readable" << std::endl;

    size_t region_num = 0;
    for (const auto& region : readable) {
        if (MemoryMapsParser::ShouldSkipRegion(region)) {
            std::cout << "  Skipping: " << region.pathname << std::endl;
            continue;
        }

        std::vector<uint8_t> data;
        if (ReadRegion(pid, region, data)) {
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            std::cout << "  [" << ++region_num << "] "
                      << std::hex << region.start << "-" << region.end
                      << std::dec << " (" << data.size() << " bytes) "
                      << region.pathname << std::endl;
        } else {
            std::cout << "  [FAIL] " << std::hex << region.start
                      << std::dec << " " << region.pathname << std::endl;
        }
    }

    std::cout << "\nStats:" << std::endl;
    std::cout << "  Total requested: " << last_stats_.total_bytes_requested << " bytes" << std::endl;
    std::cout << "  Total read: " << last_stats_.total_bytes_read << " bytes" << std::endl;
    std::cout << "  Success: " << last_stats_.regions_success
              << "/" << last_stats_.regions_attempted << " regions" << std::endl;

    return true;
}

} // namespace process_memory_dump