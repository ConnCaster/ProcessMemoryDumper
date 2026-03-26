#include "memory_reader.h"
#include "process_vm_reader.h"
#include "proc_mem_reader.h"
#include <iostream>
#include <memory>
#include <cstdlib>

// Фабрика для выбора лучшего доступного метода
std::unique_ptr<process_memory_dump::IMemoryReader> CreateMemoryReader() {
    // Приоритет 1: process_vm_readv (быстрее, Scatter-Gather)
    if (process_memory_dump::ProcessVmReader::isAvailable()) {
        std::cout << "Using: process_vm_readv (Scatter-Gather I/O)" << std::endl;
        return std::make_unique<process_memory_dump::ProcessVmReader>();
    }

    // Приоритет 2: /proc/pid/mem (фоллбэк)
    if (process_memory_dump::ProcMemReader::isAvailable()) {
        std::cout << "Using: /proc/pid/mem (fallback)" << std::endl;
        return std::make_unique<process_memory_dump::ProcMemReader>();
    }

    return nullptr;
}

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <pid> [output_file]" << std::endl;
    std::cout << "  pid         - Target process ID" << std::endl;
    std::cout << "  output_file - Optional dump output path" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    pid_t target_pid = std::atoi(argv[1]);
    std::string output_file = (argc >= 3) ? argv[2] : "dump.bin";

    if (target_pid <= 0) {
        std::cerr << "Invalid PID" << std::endl;
        return 1;
    }

    // Создаем читатель через фабрику
    auto reader = CreateMemoryReader();
    if (!reader) {
        std::cerr << "No memory reading method available!" << std::endl;
        std::cerr << "You may need CAP_SYS_PTRACE or root privileges" << std::endl;
        return 1;
    }

    std::cout << "Memory Reader: " << reader->GetMethodName() << std::endl;
    std::cout << "Target PID: " << target_pid << std::endl;
    std::cout << "Output: " << output_file << std::endl;
    std::cout << "========================================" << std::endl;

    // Выполняем дамп
    if (!reader->DumpProcess(target_pid, output_file)) {
        std::cerr << "Dump failed!" << std::endl;
        return 1;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "Dump completed successfully!" << std::endl;

    // Вывод статистики
    auto stats = reader->GetLastStats();
    std::cout << "\nFinal Stats:" << std::endl;
    std::cout << "  Bytes requested: " << stats.total_bytes_requested << std::endl;
    std::cout << "  Bytes read: " << stats.total_bytes_read << std::endl;
    std::cout << "  Regions: " << stats.regions_success
              << "/" << stats.regions_attempted << " successful" << std::endl;

    return 0;
}