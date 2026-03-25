//
// Created by user on 24.02.26.
//

#ifndef MEMORY_MAPS_H
#define MEMORY_MAPS_H

#pragma once
#include <optional>

#include "memory_reader.h"
#include <vector>
#include <string>

namespace edr {

    class MemoryMapsParser {
    public:
        // Парсинг /proc/[pid]/maps
        static std::optional<std::vector<MemoryRegion>> parse(pid_t pid);

        // Фильтрация регионов (например, только читаемые)
        static std::vector<MemoryRegion> filterReadable(
            const std::vector<MemoryRegion>& regions);

        // Пропуск специальных регионов (vsyscall, vdso и т.д.)
        static bool shouldSkipRegion(const MemoryRegion& region);
    };

} // namespace edr

#endif //MEMORY_MAPS_H
