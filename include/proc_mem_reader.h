//
// Created by user on 24.02.26.
//

#ifndef PROC_MEM_READER_H
#define PROC_MEM_READER_H

#pragma once
#include "memory_reader.h"

namespace edr {

    class ProcMemReader : public IMemoryReader {
    public:
        ProcMemReader();
        ~ProcMemReader() override;

        // bool read(pid_t pid, uintptr_t addr, void* buffer, size_t len) override;
        bool readRegion(pid_t pid, const MemoryRegion& region, std::vector<uint8_t>& out) override;
        std::optional<std::vector<MemoryRegion>> getMemoryMaps(pid_t pid) override;
        bool dumpProcess(pid_t pid, const std::string& output_path) override;
        const char* getMethodName() const override { return "/proc/pid/mem"; }
        ReadStats getLastStats() const override { return last_stats_; }

        static bool isAvailable();

    private:
        ReadStats last_stats_;
        void resetStats();
    };

} // namespace edr

#endif //PROC_MEM_READER_H
