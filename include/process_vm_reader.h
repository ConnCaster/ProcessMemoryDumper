//
// Created by user on 24.02.26.
//

#ifndef PROCESS_VM_READER_H
#define PROCESS_VM_READER_H

#pragma once
#include <optional>

#include "memory_reader.h"

namespace process_memory_dump {

    class ProcessVmReader : public IMemoryReader {
    public:
        ProcessVmReader();
        ~ProcessVmReader() override;

        bool ReadRegion(pid_t pid, const MemoryRegion& region, std::vector<uint8_t>& out) override;
        std::optional<std::vector<MemoryRegion>> GetMemoryMaps(pid_t pid) override;
        bool DumpProcess(pid_t pid, const std::string& output_path) override;
        const char* GetMethodName() const override { return "process_vm_readv"; }
        ReadStats GetLastStats() const override { return last_stats_; }

        // Проверка доступности метода на текущей системе
        static bool isAvailable();

    private:
        ReadStats last_stats_;
        void resetStats();
    };

} // namespace process_memory_dump

#endif //PROCESS_VM_READER_H
