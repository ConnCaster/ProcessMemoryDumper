#ifndef PROC_MEM_READER_H
#define PROC_MEM_READER_H

#include <functional>
#include <optional>
#include <vector>

#include "memory_reader.h"

namespace process_memory_dump {

class ProcMemReader : public IMemoryReader {
public:
    /**
     * @brief Создает объект чтения памяти процесса через файл /proc/<pid>/mem.
     */
    ProcMemReader();
    ~ProcMemReader() override;

    /**
     * @brief Считывает содержимое указанного региона памяти процесса в буфер.
     *
     * @param pid Идентификатор целевого процесса.
     * @param region Описание региона памяти, который необходимо прочитать.
     * @param out Буфер, в который будет записано содержимое региона.
     * @return true, если чтение региона выполнено успешно; false в случае ошибки.
     */
    bool ReadRegion(
        pid_t pid,
        const MemoryRegion& region,
        std::vector<uint8_t>& out
    ) override;

    /**
     * @brief Получает список регионов памяти целевого процесса.
     *
     * @param pid Идентификатор процесса, для которого требуется получить карту памяти.
     * @return Список регионов памяти при успешном получении; std::nullopt в случае ошибки.
     */
    std::optional<std::vector<MemoryRegion>> GetMemoryMaps(pid_t pid) override;

    /**
     * @brief Выполняет дамп всех доступных для чтения регионов памяти процесса в файл.
     *
     * @param pid Идентификатор целевого процесса.
     * @param output_path Путь к выходному файлу, в который будет записан дамп памяти.
     * @return true, если дамп выполнен успешно; false в случае ошибки.
     */
    bool DumpProcess(pid_t pid, const std::string& output_path) override;

    /**
     * @brief Возвращает название используемого метода чтения памяти.
     *
     * @return Строка с названием метода чтения.
     */
    const char* GetMethodName() const override {
        return "/proc/pid/mem";
    }

    /**
     * @brief Возвращает статистику последней операции чтения или дампа.
     *
     * @return Структура со статистикой чтения памяти.
     */
    ReadStats GetLastStats() const override {
        return last_stats_;
    }

    /**
     * @brief Проверяет, доступно ли чтение памяти указанного процесса через файл /proc/<pid>/mem.
     *
     * @param pid Идентификатор процесса, доступ к памяти которого необходимо проверить.
     * @return true, если чтение памяти возможно; false в противном случае.
     */
    static bool CanAccessProcess(pid_t pid);

private:
    using ChunkWriter = std::function<bool(const uint8_t*, size_t)>;

    ReadStats last_stats_{};

    /**
     * @brief Сбрасывает статистику последней операции чтения.
     */
    void resetStats();

    /**
     * @brief Считывает регион памяти по частям и передает данные в пользовательский обработчик.
     *
     * @param fd Файловый дескриптор открытого файла /proc/<pid>/mem.
     * @param region Описание считываемого региона памяти.
     * @param writer Функция обратного вызова для записи очередной порции считанных данных.
     * @param had_zero_fill Признак того, что часть данных была заполнена нулями.
     * @param bytes_written Количество байт, успешно переданных обработчику.
     * @return true, если чтение региона завершено успешно; false в случае ошибки.
     */
    bool DumpRegionWithWriter(
        int fd,
        const MemoryRegion& region,
        const ChunkWriter& writer,
        bool& had_zero_fill,
        size_t& bytes_written
    );
};

} // namespace process_memory_dump

#endif // PROC_MEM_READER_H