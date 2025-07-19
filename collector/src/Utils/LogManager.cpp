#include "LogManager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

namespace PulseOne {

LogManager::LogManager() {}
LogManager::~LogManager() {
    flushAll();
}

LogManager& PulseOne::LogManager::getInstance() {
    static LogManager instance;
    return instance;
}

std::string LogManager::getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    localtime_r(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y%m%d");
    return oss.str();
}

std::string LogManager::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    localtime_r(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%H:%M:%S");
    return oss.str();
}

std::string LogManager::buildLogPath(const std::string& category) {
    std::string date = getCurrentDate();
    std::string baseDir;

    if (category.rfind("packet_", 0) == 0) {
        baseDir = "logs/packets/" + date + "/" + category.substr(7);
    } else {
        baseDir = "logs/" + date;
    }

    fs::path fullPath = (category.rfind("packet_", 0) == 0)
        ? fs::path(baseDir + ".log")
        : fs::path(baseDir + "/" + category + ".log");

    if (!fs::exists(fullPath.parent_path())) {
        fs::create_directories(fullPath.parent_path());
    }

    return fullPath.string();
}

bool LogManager::shouldLog(LogLevel level) const {
    return static_cast<int>(level) >= static_cast<int>(minLevel_);
}

void LogManager::writeToFile(const std::string& filePath, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 콘솔에도 출력
    std::cout << message << std::endl;
    
    std::ofstream& stream = logFiles_[filePath];
    if (!stream.is_open()) {
        stream.open(filePath, std::ios::app);
    }
    stream << message << std::endl;
}

// 기존 코드 호환 메소드들
void LogManager::Info(const std::string& message) {
    log(defaultCategory_, LogLevel::INFO, message);
}

void LogManager::Warn(const std::string& message) {
    log(defaultCategory_, LogLevel::WARN, message);
}

void LogManager::Error(const std::string& message) {
    log(defaultCategory_, LogLevel::ERROR, message);
}

void LogManager::Fatal(const std::string& message) {
    log(defaultCategory_, LogLevel::FATAL, message);
}

void LogManager::Debug(const std::string& message) {
    log(defaultCategory_, LogLevel::DEBUG, message);
}

// 확장 로그 메소드들
void LogManager::log(const std::string& category, LogLevel level, const std::string& message) {
    if (!shouldLog(level)) return;
    
    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "][" << toString(level) << "] " << message;
    std::string path = buildLogPath(category);
    writeToFile(path, oss.str());
}

void LogManager::log(const std::string& category, const std::string& level, const std::string& message) {
    log(category, fromString(level), message);
}

void LogManager::logDriver(const std::string& driverName, const std::string& message) {
    log("driver_" + driverName, LogLevel::INFO, message);
}

void LogManager::logError(const std::string& message) {
    log("error", LogLevel::ERROR, message);
}

void LogManager::logPacket(const std::string& driver, const std::string& device,
                           const std::string& rawPacket, const std::string& decoded) {
    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "]\n[RAW] " << rawPacket << "\n[DECODED] " << decoded;
    std::string category = "packet_" + driver + "/" + device;
    std::string path = buildLogPath(category);
    writeToFile(path, oss.str());
}

void LogManager::flushAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : logFiles_) {
        if (kv.second.is_open()) {
            kv.second.flush();
            kv.second.close();
        }
    }
    logFiles_.clear();
}

} // namespace PulseOne