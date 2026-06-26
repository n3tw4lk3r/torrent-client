#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

namespace tclient {

class Logger {
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void LogUi(std::string_view message);
    static void LogError(std::string_view message);
    static std::vector<std::string> GetMessages(size_t max_count = 50);

private:
    static Logger& Instance();

    Logger();
    ~Logger();

    void AddMessageInternal(std::string_view message);
    void WriteToFile(std::string_view level, std::string_view message);

private:
    static constexpr size_t kMaxMessages = 1'000;
    static constexpr size_t kTrimCount = 500;
    static std::filesystem::path kLogPath;

    std::mutex mutex;
    std::vector<std::string> messages;
    std::ofstream log_file;
};

} // namespace tclient

