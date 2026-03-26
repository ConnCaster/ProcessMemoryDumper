//
// Created by user on 24.02.26.
//

#ifndef MEMORY_READER_H
#define MEMORY_READER_H

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace process_memory_dump {

    // Описание региона памяти из /proc/pid/maps
    struct MemoryRegion {
        uintptr_t start;
        uintptr_t end;
        bool readable;
        bool writable;
        bool executable;
        bool is_private;
        std::string pathname;

        size_t Size() const { return end - start; }
    };

    // Статистика чтения
    struct ReadStats {
        size_t total_bytes_requested;
        size_t total_bytes_read;
        size_t regions_attempted;
        size_t regions_success;
        size_t regions_failed;
    };

    // Абстрактный класс для чтения памяти (Strategy Pattern)
    class IMemoryReader {
    public:
        virtual ~IMemoryReader() = default;

        // Чтение региона памяти
        virtual bool ReadRegion(pid_t pid, const MemoryRegion& region, std::vector<uint8_t>& out) = 0;

        // Получение списка регионов памяти
        virtual std::optional<std::vector<MemoryRegion>> GetMemoryMaps(pid_t pid) = 0;

        // Полный дамп всех читаемых регионов
        virtual bool DumpProcess(pid_t pid, const std::string& output_path) = 0;

        // Название метода (для логирования)
        virtual const char* GetMethodName() const = 0;

        // Статистика последней операции
        virtual ReadStats GetLastStats() const = 0;
    };

} // namespace process_memory_dump

#endif //MEMORY_READER_H
