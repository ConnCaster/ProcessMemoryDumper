#include "memory_maps.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace edr {

std::optional<std::vector<MemoryRegion>> MemoryMapsParser::parse(pid_t pid) {
    std::vector<MemoryRegion> regions;
    std::string maps_path = "/proc/" + std::to_string(pid) + "/maps";
    
    std::ifstream maps(maps_path);
    if (!maps) {
        std::cerr << "Cannot open " << maps_path << std::endl;
        return std::nullopt;
    }

    std::string line;
    while (std::getline(maps, line)) {
        MemoryRegion region;
        std::istringstream iss(line);
        
        // Формат: start-end perms offset dev inode pathname
        std::string addr_range, perms, offset, dev, inode;
        
        iss >> addr_range >> perms >> offset >> dev >> inode;
        
        // Парсим адрес
        size_t dash_pos = addr_range.find('-');
        region.start = std::stoull(addr_range.substr(0, dash_pos), nullptr, 16);
        region.end = std::stoull(addr_range.substr(dash_pos + 1), nullptr, 16);
        
        // Парсим права
        region.readable = (perms[0] == 'r');
        region.writable = (perms[1] == 'w');
        region.executable = (perms[2] == 'x');
        region.is_private = (perms[3] == 'p');
        
        // Остаток строки - pathname
        std::getline(iss >> std::ws, region.pathname);
        
        regions.push_back(region);
    }

    return regions;
}

std::vector<MemoryRegion> MemoryMapsParser::filterReadable(
    const std::vector<MemoryRegion>& regions) {
    
    std::vector<MemoryRegion> result;
    for (const auto& r : regions) {
        if (r.readable) {
            result.push_back(r);
        }
    }
    return result;
}

bool MemoryMapsParser::shouldSkipRegion(const MemoryRegion& region) {
    // Пропускаем проблемные регионы
    static const std::vector<std::string> skip_list = {
        "[vsyscall]",  // Может вызвать segfault
        "[vvar]",
    };
    
    for (const auto& skip : skip_list) {
        if (region.pathname.find(skip) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

} // namespace edr