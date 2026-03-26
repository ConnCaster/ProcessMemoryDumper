//
// Created by user on 24.02.26.
//

#ifndef PROC_MEM_READER_H
#define PROC_MEM_READER_H

#pragma once
#include "memory_reader.h"

namespace process_memory_dump {

    class ProcMemReader : public IMemoryReader {
    public:
        ProcMemReader();
        ~ProcMemReader() override;

        bool ReadRegion(pid_t pid, const MemoryRegion& region, std::vector<uint8_t>& out) override;
        std::optional<std::vector<MemoryRegion>> GetMemoryMaps(pid_t pid) override;
        bool DumpProcess(pid_t pid, const std::string& output_path) override;
        const char* GetMethodName() const override { return "/proc/pid/mem"; }
        ReadStats GetLastStats() const override { return last_stats_; }

        static bool isAvailable();

    private:
        ReadStats last_stats_;
        void resetStats();
    };

} // namespace process_memory_dump

#endif //PROC_MEM_READER_H
