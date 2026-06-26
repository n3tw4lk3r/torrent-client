#include "utils/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iostream>

namespace tclient {

std::filesystem::path Logger::kLogPath =
    std::filesystem::path(PROJECT_SOURCE_DIR) / "logs/tclient.log";

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    if (!std::filesystem::exists(kLogPath)) {
        std::filesystem::create_directory(kLogPath.parent_path());
    }

    log_file.open(kLogPath, std::ios::trunc);

    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file: " << kLogPath << std::endl;
        exit(0);
    }
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex);

    if (log_file.is_open()) {
        log_file.flush();
        log_file.close();
    }
}

void Logger::WriteToFile(std::string_view level, std::string_view message) {
    if (!log_file.is_open()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    char time_string[20];
    struct tm time_info;
    localtime_r(&time, &time_info);

    std::strftime(
        time_string,
        sizeof(time_string),
        "[%H:%M:%S]",
        &time_info
    );

    log_file
        << time_string
        << " ["
        << level
        << "] "
        << message
        << std::endl;

    log_file.flush();
}

void Logger::AddMessageInternal(std::string_view message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    char time_string[20];
    struct tm time_info;
    localtime_r(&time, &time_info);

    std::strftime(
        time_string,
        sizeof(time_string),
        "[%H:%M:%S]",
        &time_info
    );

    std::string full_message =
        std::string(time_string) + " " + std::string(message);

    messages.push_back(full_message);

    if (messages.size() > kMaxMessages) {
        messages.erase(
            messages.begin(),
            messages.begin() + static_cast<std::ptrdiff_t>(kTrimCount)
        );
    }
}

void Logger::LogUi(std::string_view message) {
    auto& logger = Instance();

    std::lock_guard<std::mutex> lock(logger.mutex);

    logger.AddMessageInternal(message);
    logger.WriteToFile("UI", message);
}

void Logger::LogError(std::string_view message) {
    auto& logger = Instance();

    std::lock_guard<std::mutex> lock(logger.mutex);

    logger.WriteToFile("ERROR", message);
}

std::vector<std::string> Logger::GetMessages(size_t max_count) {
    auto& logger = Instance();

    std::lock_guard<std::mutex> lock(logger.mutex);

    if (logger.messages.size() <= max_count) {
        return logger.messages;
    }

    return std::vector<std::string>(
        logger.messages.end() - static_cast<std::ptrdiff_t>(max_count),
        logger.messages.end()
    );
}

} // namespace tclient

