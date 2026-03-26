#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "process_memory_dumper/memory_maps.h"

namespace process_memory_dump {

std::optional<std::vector<MemoryRegion>> MemoryMapsParser::Parse(pid_t pid) {
    std::vector<MemoryRegion> regions;
    const std::string maps_path = "/proc/" + std::to_string(pid) + "/maps";

    std::ifstream maps(maps_path);
    if (!maps) {
        std::cerr << "Cannot open " << maps_path << '\n';
        return std::nullopt;
    }

    std::string line;
    while (std::getline(maps, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string addr_range;
        std::string perms;
        std::string offset;
        std::string dev;
        std::string inode;

        if (!(iss >> addr_range >> perms >> offset >> dev >> inode)) {
            std::cerr << "Skipping malformed maps line: " << line << '\n';
            continue;
        }

        const size_t dash_pos = addr_range.find('-');
        if (dash_pos == std::string::npos) {
            std::cerr << "Skipping maps line with invalid address range: " << line << '\n';
            continue;
        }

        MemoryRegion region;
        try {
            region.start = static_cast<uintptr_t>(
                std::stoull(addr_range.substr(0, dash_pos), nullptr, 16)
            );
            region.end = static_cast<uintptr_t>(
                std::stoull(addr_range.substr(dash_pos + 1), nullptr, 16)
            );
        } catch (const std::exception& ex) {
            std::cerr << "Skipping maps line due to parse error: " << ex.what()
                      << " | line: " << line << '\n';
            continue;
        }

        if (region.end <= region.start) {
            continue;
        }

        if (perms.size() < 4) {
            std::cerr << "Skipping maps line with invalid perms: " << line << '\n';
            continue;
        }

        region.readable = (perms[0] == 'r');
        region.writable = (perms[1] == 'w');
        region.executable = (perms[2] == 'x');
        region.is_private = (perms[3] == 'p');

        std::getline(iss >> std::ws, region.pathname);

        regions.push_back(std::move(region));
    }

    return regions;
}

std::vector<MemoryRegion> MemoryMapsParser::FilterReadable(
    const std::vector<MemoryRegion>& regions
) {
    std::vector<MemoryRegion> result;
    result.reserve(regions.size());

    for (const auto& region : regions) {
        if (region.readable && region.Size() > 0U) {
            result.push_back(region);
        }
    }

    return result;
}

bool MemoryMapsParser::ShouldSkipRegion(const MemoryRegion& region) {
    if (region.Size() == 0U) {
        return true;
    }

    static const std::vector<std::string> skip_list = {
        "[vsyscall]",
        "[vvar]",
    };

    for (const auto& skip : skip_list) {
        if (region.pathname == skip) {
            return true;
        }
    }

    return false;
}

} // namespace process_memory_dump