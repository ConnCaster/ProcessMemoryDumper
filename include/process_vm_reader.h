//
// Created by user on 24.02.26.
//

#ifndef PROCESS_VM_READER_H
#define PROCESS_VM_READER_H

#pragma once
#include <optional>

#include "memory_reader.h"

namespace edr {

    class ProcessVmReader : public IMemoryReader {
    public:
        ProcessVmReader();
        ~ProcessVmReader() override;

        // bool read(pid_t pid, uintptr_t addr, void* buffer, size_t len) override;
        bool readRegion(pid_t pid, const MemoryRegion& region, std::vector<uint8_t>& out) override;
        std::optional<std::vector<MemoryRegion>> getMemoryMaps(pid_t pid) override;
        bool dumpProcess(pid_t pid, const std::string& output_path) override;
        const char* getMethodName() const override { return "process_vm_readv"; }
        ReadStats getLastStats() const override { return last_stats_; }

        // Проверка доступности метода на текущей системе
        static bool isAvailable();

    private:
        ReadStats last_stats_;
        void resetStats();
    };

} // namespace edr

#endif //PROCESS_VM_READER_H
