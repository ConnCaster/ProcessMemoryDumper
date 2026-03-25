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

namespace edr {

    // Описание региона памяти из /proc/pid/maps
    struct MemoryRegion {
        uintptr_t start;
        uintptr_t end;
        bool readable;
        bool writable;
        bool executable;
        bool is_private;
        std::string pathname;

        size_t size() const { return end - start; }
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

        // Чтение памяти по адресу
        // virtual bool read(pid_t pid, uintptr_t addr, void* buffer, size_t len) = 0;

        // Чтение региона памяти
        virtual bool readRegion(pid_t pid, const MemoryRegion& region, std::vector<uint8_t>& out) = 0;

        // Получение списка регионов памяти
        virtual std::optional<std::vector<MemoryRegion>> getMemoryMaps(pid_t pid) = 0;

        // Полный дамп всех читаемых регионов
        virtual bool dumpProcess(pid_t pid, const std::string& output_path) = 0;

        // Название метода (для логирования)
        virtual const char* getMethodName() const = 0;

        // Статистика последней операции
        virtual ReadStats getLastStats() const = 0;
    };

} // namespace edr

#endif //MEMORY_READER_H
