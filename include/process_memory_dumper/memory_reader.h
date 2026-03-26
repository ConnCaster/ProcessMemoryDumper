#ifndef MEMORY_READER_H
#define MEMORY_READER_H

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace process_memory_dump {

/**
 * @brief Описывает регион виртуальной памяти процесса.
 *
 * Структура хранит границы региона памяти, его права доступа
 * и путь, связанный с данным отображением в адресном пространстве процесса.
 */
struct MemoryRegion {
    uintptr_t start = 0;
    uintptr_t end = 0;
    bool readable = false;
    bool writable = false;
    bool executable = false;
    bool is_private = false;
    std::string pathname;

    /**
     * @brief Возвращает размер региона памяти в байтах.
     *
     * @return Размер региона памяти в байтах.
     */
    [[nodiscard]] size_t Size() const {
        return (end > start) ? static_cast<size_t>(end - start) : 0U;
    }

    /**
     * @brief Возвращает строковое представление прав доступа региона памяти.
     *
     * Формирует строку в формате, аналогичном представлению прав
     * в /proc/<pid>/maps, например "rwxp".
     *
     * @return Строка с правами доступа региона памяти.
     */
    [[nodiscard]] std::string PermissionsString() const {
        std::string perms(4, '-');
        perms[0] = readable ? 'r' : '-';
        perms[1] = writable ? 'w' : '-';
        perms[2] = executable ? 'x' : '-';
        perms[3] = is_private ? 'p' : 's';
        return perms;
    }
};

/**
 * @brief Хранит статистику последней операции чтения памяти.
 *
 * Содержит сведения об объеме запрошенных и прочитанных данных,
 * количестве байт, заполненных нулями, а также числе успешно
 * и неуспешно обработанных регионов памяти.
 */
struct ReadStats {
    size_t total_bytes_requested = 0;
    size_t total_bytes_read = 0;
    size_t total_bytes_zero_filled = 0;
    size_t regions_attempted = 0;
    size_t regions_success = 0;
    size_t regions_failed = 0;
};

/**
 * @brief Абстрактный интерфейс для чтения памяти процесса.
 *
 * Определяет общий набор методов для получения карты памяти,
 * чтения отдельных регионов и создания дампа процесса
 * независимо от конкретного механизма доступа к памяти.
 */
class IMemoryReader {
public:
    /**
     * @brief Освобождает ресурсы объекта чтения памяти.
     */
    virtual ~IMemoryReader() = default;

    /**
     * @brief Считывает содержимое указанного региона памяти в буфер.
     *
     * @param pid Идентификатор целевого процесса.
     * @param region Описание региона памяти, который требуется прочитать.
     * @param out Буфер, в который будет записано содержимое региона.
     * @return true, если чтение выполнено успешно; false в случае ошибки.
     */
    virtual bool ReadRegion(
        pid_t pid,
        const MemoryRegion& region,
        std::vector<uint8_t>& out
    ) = 0;

    /**
     * @brief Получает список регионов памяти целевого процесса.
     *
     * @param pid Идентификатор процесса, для которого требуется получить карту памяти.
     * @return Список регионов памяти при успешном получении;
     *         std::nullopt в случае ошибки.
     */
    virtual std::optional<std::vector<MemoryRegion>> GetMemoryMaps(pid_t pid) = 0;

    /**
     * @brief Выполняет дамп памяти процесса в указанный файл.
     *
     * @param pid Идентификатор целевого процесса.
     * @param output_path Путь к выходному файлу дампа.
     * @return true, если дамп выполнен успешно; false в случае ошибки.
     */
    virtual bool DumpProcess(pid_t pid, const std::string& output_path) = 0;

    /**
     * @brief Возвращает название используемого метода чтения памяти.
     *
     * @return Строка с названием метода чтения памяти.
     */
    virtual const char* GetMethodName() const = 0;

    /**
     * @brief Возвращает статистику последней операции чтения или дампа.
     *
     * @return Структура со статистикой последней операции.
     */
    virtual ReadStats GetLastStats() const = 0;
};

} // namespace process_memory_dump

#endif // MEMORY_READER_H