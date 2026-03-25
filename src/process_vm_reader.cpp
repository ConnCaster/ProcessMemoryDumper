#include "process_vm_reader.h"
#include "memory_maps.h"
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>

namespace edr {

ProcessVmReader::ProcessVmReader() {
    resetStats();
}

ProcessVmReader::~ProcessVmReader() = default;

void ProcessVmReader::resetStats() {
    last_stats_ = {0, 0, 0, 0, 0};
}

bool ProcessVmReader::isAvailable() {
    // Проверяем наличие системного вызова (ядро >= 3.2)
    // Также проверяем, что мы можем прочитать память процесса 1 (init)
    // Это быстрая проверка без побочных эффектов
    char buffer[8];
    struct iovec local{buffer, sizeof(buffer)};
    struct iovec remote{(void*)0x100000000, sizeof(buffer)}; // Заведомо неверный адрес
    
    // Если syscall существует, вернется -1 с EFAULT, а не ENOSYS
    long ret = process_vm_readv(1, &local, 1, &remote, 1, 0);
    return (ret == -1 && errno == EFAULT) || (ret >= 0);
}

bool ProcessVmReader::readRegion(pid_t pid, const MemoryRegion& region, 
                                  std::vector<uint8_t>& out) {
    if (!region.readable) {
        return false;
    }

    out.resize(region.size());
    
    // Scatter-Gather: один вызов на регион
    struct iovec local{out.data(), out.size()};
    struct iovec remote{(void*)region.start, out.size()};

    last_stats_.total_bytes_requested += out.size();
    last_stats_.regions_attempted++;

    ssize_t result = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    
    if (result == -1) {
        last_stats_.regions_failed++;
        // EIO означает, что страница вытеснена в swap или не отображена
        if (errno == EIO) {
            std::cerr << "Page not available (swap/unmapped): " 
                      << std::hex << region.start << std::dec << std::endl;
        }
        return false;
    }

    last_stats_.total_bytes_read += result;
    last_stats_.regions_success++;
    
    // Обрезаем, если прочитали меньше (например, конец региона недоступен)
    if ((size_t)result < out.size()) {
        out.resize(result);
    }
    
    return true;
}

std::optional<std::vector<MemoryRegion>> ProcessVmReader::getMemoryMaps(pid_t pid) {
    return MemoryMapsParser::parse(pid);
}

bool ProcessVmReader::dumpProcess(pid_t pid, const std::string& output_path) {
    resetStats();
    
    auto regions = getMemoryMaps(pid);
    if (!regions.has_value()) {
        return false;
    }
    auto readable = MemoryMapsParser::filterReadable(*regions);
    
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open output file: " << output_path << std::endl;
        return false;
    }

    std::cout << "Dumping process " << pid << " using " << getMethodName() << std::endl;
    std::cout << "Found " << regions->size() << " regions, "
              << readable.size() << " readable" << std::endl;

    size_t region_num = 0;
    for (const auto& region : readable) {
        if (MemoryMapsParser::shouldSkipRegion(region)) {
            std::cout << "  Skipping: " << region.pathname << std::endl;
            continue;
        }

        std::vector<uint8_t> data;
        if (readRegion(pid, region, data)) {
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

} // namespace edr