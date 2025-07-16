#include "LogManager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

LogManager::LogManager() {}
LogManager::~LogManager() {
    flushAll();
}

LogManager& LogManager::getInstance() {
    static LogManager instance;
    return instance;
}

// 날짜 문자열: YYYYMMDD
std::string LogManager::getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    localtime_r(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y%m%d");
    return oss.str();
}

// 시간 문자열: HH:MM:SS
std::string LogManager::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    localtime_r(&t, &buf);
    std::ostringstream oss;
    oss << std::put_time(&buf, "%H:%M:%S");
    return oss.str();
}

// 로그 경로 생성: 일반 로그 vs 패킷 로그 자동 분리
std::string LogManager::buildLogPath(const std::string& category) {
    std::string date = getCurrentDate();
    std::string baseDir;

    if (category.rfind("packet_", 0) == 0) {
        baseDir = "logs/packets/" + date + "/" + category.substr(7); // remove 'packet_'
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

// 내부 쓰기 함수 (파일 핸들 캐싱, 스레드 세이프)
void LogManager::writeToFile(const std::string& filePath, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream& stream = logFiles_[filePath];
    if (!stream.is_open()) {
        stream.open(filePath, std::ios::app);
    }
    stream << message << std::endl;
}

// 일반 로그 (레벨 문자열 버전)
void LogManager::log(const std::string& category, const std::string& level, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "][" << level << "] " << message;
    std::string path = buildLogPath(category);
    writeToFile(path, oss.str());
}

// 일반 로그 (LogLevel enum 버전)
void LogManager::log(const std::string& category, LogLevel level, const std::string& message) {
    log(category, toString(level), message);
}

// 드라이버용 로그
void LogManager::logDriver(const std::string& driverName, const std::string& message) {
    log("driver_" + driverName, LOG_LEVEL_INFO, message);
}

// 에러 로그 고정
void LogManager::logError(const std::string& message) {
    log("error", LOG_LEVEL_ERROR, message);
}

// 패킷 로그: driver + device 조합으로 로그 파일 분리
void LogManager::logPacket(const std::string& driver, const std::string& device,
                           const std::string& rawPacket, const std::string& decoded) {
    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "]\n[RAW] " << rawPacket << "\n[DECODED] " << decoded;
    std::string category = "packet_" + driver + "/" + device;
    std::string path = buildLogPath(category);
    writeToFile(path, oss.str());
}

// 모든 열려있는 로그 flush + close
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
