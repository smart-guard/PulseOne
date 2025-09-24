/**
 * @file CSPDynamicTargets.cpp
 * @brief CSP Gateway 동적 전송 대상 시스템 구현 - 중복 정의 완전 제거
 * @author PulseOne Development Team  
 * @date 2025-09-24
 * @version 1.2.0 (FailureProtectorStats 중복 정의 제거)
 */

#include "CSP/CSPDynamicTargets.h"
#include "CSP/FailureProtector.h"  // FailureProtectorStats 정의가 여기에 있음
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <random>

namespace PulseOne {
namespace CSP {

// =============================================================================
// 공통 유틸리티 함수들
// =============================================================================

/**
 * @brief 알람 메시지 유효성 검증
 */
bool isValidAlarmMessage(const AlarmMessage& alarm) {
    if (alarm.nm.empty()) {
        LogManager::getInstance().Error("알람 메시지에 포인트명이 없습니다");
        return false;
    }
    
    if (alarm.bd <= 0) {
        LogManager::getInstance().Error("유효하지 않은 빌딩 ID: " + std::to_string(alarm.bd));
        return false;
    }
    
    return true;
}

/**
 * @brief 설정 유효성 검증
 */
bool isValidTargetConfig(const json& config, const std::string& target_type) {
    if (config.empty() || !config.is_object()) {
        LogManager::getInstance().Error("타겟 설정이 비어있거나 올바른 JSON 객체가 아닙니다");
        return false;
    }
    
    // 타겟 타입별 필수 필드 검증
    if (target_type == "http" || target_type == "HTTP") {
        if (!config.contains("endpoint") || config["endpoint"].empty()) {
            LogManager::getInstance().Error("HTTP 타겟에 endpoint가 설정되지 않음");
            return false;
        }
    } else if (target_type == "s3" || target_type == "S3") {
        if (!config.contains("bucket_name") || config["bucket_name"].empty()) {
            LogManager::getInstance().Error("S3 타겟에 bucket_name이 설정되지 않음");
            return false;
        }
    } else if (target_type == "mqtt" || target_type == "MQTT") {
        if (!config.contains("broker_host") || config["broker_host"].empty()) {
            LogManager::getInstance().Error("MQTT 타겟에 broker_host가 설정되지 않음");
            return false;
        }
    } else if (target_type == "file" || target_type == "FILE") {
        if (!config.contains("base_path") || config["base_path"].empty()) {
            LogManager::getInstance().Error("FILE 타겟에 base_path가 설정되지 않음");
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 문자열에서 변수 치환
 */
std::string replaceVariables(const std::string& template_str, const AlarmMessage& alarm) {
    std::string result = template_str;
    
    // 기본 알람 필드들 치환
    std::regex building_regex(R"(\{building_id\})");
    result = std::regex_replace(result, building_regex, std::to_string(alarm.bd));
    
    std::regex point_regex(R"(\{point_name\})");
    result = std::regex_replace(result, point_regex, alarm.nm);
    
    std::regex value_regex(R"(\{value\})");
    result = std::regex_replace(result, value_regex, std::to_string(alarm.vl));
    
    std::regex timestamp_regex(R"(\{timestamp\})");
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    result = std::regex_replace(result, timestamp_regex, ss.str());
    
    return result;
}

/**
 * @brief JSON 알람 메시지 생성
 */
json createAlarmJson(const AlarmMessage& alarm) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    
    return json{
        {"building_id", alarm.bd},
        {"point_name", alarm.nm},
        {"value", alarm.vl},
        {"timestamp", ss.str()},
        {"alarm_type", "CRITICAL"},
        {"severity", 1},
        {"message", "Alarm occurred on " + alarm.nm},
        {"source", "PulseOne CSP Gateway"},
        {"status", alarm.st}
    };
}

/**
 * @brief 타임스탬프 문자열 생성
 */
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

/**
 * @brief 디렉토리 생성 (재귀적)
 */
bool createDirectoryRecursive(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("디렉토리 생성 실패: " + path + " - " + e.what());
        return false;
    }
}

/**
 * @brief 파일 존재 여부 확인
 */
bool fileExists(const std::string& file_path) {
    return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
}

/**
 * @brief 파일 크기 조회
 */
size_t getFileSize(const std::string& file_path) {
    try {
        if (std::filesystem::exists(file_path)) {
            return std::filesystem::file_size(file_path);
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 크기 조회 실패: " + file_path + " - " + e.what());
    }
    return 0;
}

/**
 * @brief HTTP 상태 코드 설명
 */
std::string getHttpStatusDescription(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 408: return "Request Timeout";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown Status";
    }
}

/**
 * @brief 재시도 지연 시간 계산 (지수 백오프)
 */
std::chrono::milliseconds calculateRetryDelay(int retry_count, int base_delay_ms = 1000) {
    int delay_ms = base_delay_ms * (1 << std::min(retry_count, 6)); // 최대 64초
    
    // 지터 추가 (±20%)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.8, 1.2);
    
    return std::chrono::milliseconds(static_cast<int>(delay_ms * dis(gen)));
}

// =============================================================================
// 유틸리티 함수들 - 중복 정의된 메서드들은 제거 (헤더에서 인라인 정의됨)
// =============================================================================

} // namespace CSP
} // namespace PulseOne