#include "process_memory_dumper/memory_reader.h"
#include "process_memory_dumper/proc_mem_reader.h"
#include "process_memory_dumper/process_vm_reader.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace process_memory_dump {

std::unique_ptr<IMemoryReader> CreateMemoryReader(pid_t target_pid) {
    if (ProcessVmReader::CanAccessProcess(target_pid)) {
        std::cout << "Using: process_vm_readv\n";
        return std::make_unique<ProcessVmReader>();
    }

    if (ProcMemReader::CanAccessProcess(target_pid)) {
        std::cout << "Using: /proc/pid/mem (fallback)\n";
        return std::make_unique<ProcMemReader>();
    }

    return nullptr;
}

} // namespace process_memory_dump

void PrintUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <pid> [output_file]\n";
    std::cout << "  pid         - Target process ID\n";
    std::cout << "  output_file - Optional dump output path (default: dump.bin)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    const pid_t target_pid = static_cast<pid_t>(std::atoi(argv[1]));
    const std::string output_file = (argc >= 3) ? argv[2] : "dump.bin";

    if (target_pid <= 0) {
        std::cerr << "Invalid PID\n";
        return 1;
    }

    auto reader = process_memory_dump::CreateMemoryReader(target_pid);
    if (!reader) {
        std::cerr << "No usable memory reading method for PID " << target_pid << '\n';
        std::cerr << "You may need CAP_SYS_PTRACE, root privileges, or a compatible ptrace_scope policy\n";
        return 1;
    }

    std::cout << "Memory Reader: " << reader->GetMethodName() << '\n';
    std::cout << "Target PID: " << target_pid << '\n';
    std::cout << "Output: " << output_file << '\n';
    std::cout << "Metadata: " << output_file << ".map\n";
    std::cout << "========================================\n";

    if (!reader->DumpProcess(target_pid, output_file)) {
        std::cerr << "Dump failed!\n";
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "Dump completed successfully!\n";

    const auto stats = reader->GetLastStats();
    std::cout << "\nFinal Stats:\n";
    std::cout << "  Bytes requested: " << stats.total_bytes_requested << '\n';
    std::cout << "  Bytes read: " << stats.total_bytes_read << '\n';
    std::cout << "  Zero-filled bytes: " << stats.total_bytes_zero_filled << '\n';
    std::cout << "  Regions: " << stats.regions_success
              << "/" << stats.regions_attempted << " successful\n";

    return 0;
}