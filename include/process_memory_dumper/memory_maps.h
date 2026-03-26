#ifndef MEMORY_MAPS_H
#define MEMORY_MAPS_H

#include <optional>
#include <vector>

#include "memory_reader.h"

namespace process_memory_dump {

    class MemoryMapsParser {
    public:
        /**
         * @brief Выполняет разбор файла /proc/<pid>/maps и формирует список регионов памяти процесса.
         *
         * @param pid Идентификатор процесса, для которого требуется получить карту памяти.
         * @return Список регионов памяти при успешном разборе; std::nullopt в случае ошибки.
         */
        static std::optional<std::vector<MemoryRegion>> Parse(pid_t pid);

        /**
         * @brief Фильтрует список регионов памяти, оставляя только доступные для чтения.
         *
         * @param regions Исходный список регионов памяти процесса.
         * @return Список регионов, для которых установлен флаг чтения.
         */
        static std::vector<MemoryRegion> FilterReadable(
            const std::vector<MemoryRegion>& regions
        );

        /**
         * @brief Проверяет, нужно ли пропустить указанный регион при обработке.
         *
         * @param region Регион памяти, для которого выполняется проверка.
         * @return true, если регион следует пропустить; false в противном случае.
         */
        static bool ShouldSkipRegion(const MemoryRegion& region);
    };

} // namespace process_memory_dump

#endif // MEMORY_MAPS_H