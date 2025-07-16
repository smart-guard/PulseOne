#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>

#include "LogLevels.h"  // 로그 레벨 상수 및 enum 사용

/**
 * LogManager: Collector 전역 로그 관리 클래스 (싱글턴 + 스레드 세이프)
 * - 일반 로그: logs/YYYYMMDD/
 * - 패킷 로그: logs/packets/YYYYMMDD/<driver>/<device>.log
 * 
 * 코드 샘플
 * // 일반 로그
LogManager::getInstance().log("system", LOG_LEVEL_INFO, "Collector 시작됨");
// enum 기반 로그
LogManager::getInstance().log("database", LogLevel::ERROR, "DB 연결 실패");
// 드라이버 상태 로그
LogManager::getInstance().logDriver("Modbus", "Polling 시작");
// 통신 패킷 로그
LogManager::getInstance().logPacket("Modbus", "device01",
    "0x01 0x03 0x00 0x00 0x00 0x02 0xC4 0x0B",
    "Function: 0x03, Addr: 0x0000, Value1: 100, Value2: 230");ㄴ
// 에러 로그
LogManager::getInstance().logError("Plugin 로딩 실패");
 */
class LogManager {
public:
    static LogManager& getInstance();

    // 일반 로그 기록
    void log(const std::string& category, const std::string& level, const std::string& message);
    void log(const std::string& category, LogLevel level, const std::string& message);

    // 드라이버 로그 전용
    void logDriver(const std::string& driverName, const std::string& message);

    // 에러 로그 고정
    void logError(const std::string& message);

    // 통신 패킷 로그 (프로토콜 + 장치 단위)
    void logPacket(const std::string& driver, const std::string& device,
                   const std::string& rawPacket, const std::string& decoded);

    // 모든 로그 스트림 종료 시 flush
    void flushAll();

private:
    LogManager();
    ~LogManager();
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    std::string getCurrentDate();
    std::string getCurrentTime();
    std::string buildLogPath(const std::string& category);
    void writeToFile(const std::string& filePath, const std::string& message);

    std::mutex mutex_;
    std::map<std::string, std::ofstream> logFiles_;
};

#endif // LOG_MANAGER_H
