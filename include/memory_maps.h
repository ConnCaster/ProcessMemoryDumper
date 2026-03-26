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

namespace process_memory_dump {

    class MemoryMapsParser {
    public:
        // Парсинг /proc/[pid]/maps
        static std::optional<std::vector<MemoryRegion>> Parse(pid_t pid);

        // Фильтрация регионов (например, только читаемые)
        static std::vector<MemoryRegion> FilterReadable(const std::vector<MemoryRegion>& regions);

        // Пропуск специальных регионов (vsyscall, vdso и т.д.)
        static bool ShouldSkipRegion(const MemoryRegion& region);
    };

} // namespace process_memory_dump

#endif //MEMORY_MAPS_H
