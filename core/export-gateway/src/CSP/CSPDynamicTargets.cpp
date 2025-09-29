/**
 * @file CSPDynamicTargets.cpp
 * @brief CSP Gateway 동적 전송 대상 시스템 구현 - 완성본
 * @author PulseOne Development Team  
 * @date 2025-09-29
 * @version 3.0.0 (헤더 통합 완료, include 정리)
 */

#include "CSP/CSPDynamicTargets.h"
// #include "CSP/FailureProtector.h"  // 제거됨 - 모든 타입이 CSPDynamicTargets.h에 정의됨
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
// 유틸리티 함수들 구현 (헤더에서 inline으로 정의되지 않은 것들)
// =============================================================================

/**
 * @brief 확장된 알람 메시지 유효성 검증 (헤더의 간단한 버전 확장)
 */
bool isValidAlarmMessageExtended(const AlarmMessage& alarm) {
    // 기본 검증 (헤더의 inline 함수 호출)
    if (!isValidAlarmMessage(alarm)) {
        return false;
    }
    
    // 확장 검증
    if (alarm.nm.empty()) {
        LogManager::getInstance().Error("알람 메시지에 포인트명이 없습니다");
        return false;
    }
    
    if (alarm.bd <= 0) {
        LogManager::getInstance().Error("유효하지 않은 빌딩 ID: " + std::to_string(alarm.bd));
        return false;
    }
    
    // 추가 검증 로직...
    return true;
}

/**
 * @brief 확장된 타겟 설정 유효성 검증 (헤더의 간단한 버전 확장)
 */
bool isValidTargetConfigExtended(const json& config, const std::string& target_type) {
    // 기본 검증 (헤더의 inline 함수 호출)
    if (!isValidTargetConfig(config, target_type)) {
        return false;
    }
    
    if (config.empty() || !config.is_object()) {
        LogManager::getInstance().Error("타겟 설정이 비어있거나 올바른 JSON 객체가 아닙니다");
        return false;
    }
    
    // 타겟 타입별 상세 필수 필드 검증
    if (target_type == "http" || target_type == "HTTP") {
        if (!config.contains("endpoint") || config["endpoint"].empty()) {
            LogManager::getInstance().Error("HTTP 타겟에 endpoint가 설정되지 않음");
            return false;
        }
        
        // URL 형식 검증
        std::string url = config["endpoint"];
        if (url.find("http://") != 0 && url.find("https://") != 0) {
            LogManager::getInstance().Error("HTTP endpoint는 http:// 또는 https://로 시작해야 합니다");
            return false;
        }
        
    } else if (target_type == "s3" || target_type == "S3") {
        if (!config.contains("bucket_name") || config["bucket_name"].empty()) {
            LogManager::getInstance().Error("S3 타겟에 bucket_name이 설정되지 않음");
            return false;
        }
        
        if (!config.contains("access_key") || config["access_key"].empty()) {
            LogManager::getInstance().Error("S3 타겟에 access_key가 설정되지 않음");
            return false;
        }
        
        if (!config.contains("secret_key") || config["secret_key"].empty()) {
            LogManager::getInstance().Error("S3 타겟에 secret_key가 설정되지 않음");
            return false;
        }
        
    } else if (target_type == "mqtt" || target_type == "MQTT") {
        if (!config.contains("broker_host") || config["broker_host"].empty()) {
            LogManager::getInstance().Error("MQTT 타겟에 broker_host가 설정되지 않음");
            return false;
        }
        
        if (!config.contains("topic") || config["topic"].empty()) {
            LogManager::getInstance().Error("MQTT 타겟에 topic이 설정되지 않음");
            return false;
        }
        
    } else if (target_type == "file" || target_type == "FILE") {
        if (!config.contains("base_path") || config["base_path"].empty()) {
            LogManager::getInstance().Error("FILE 타겟에 base_path가 설정되지 않음");
            return false;
        }
        
        // 경로 유효성 검증
        std::string base_path = config["base_path"];
        if (!std::filesystem::exists(std::filesystem::path(base_path).parent_path())) {
            LogManager::getInstance().Error("FILE 타겟의 base_path 상위 디렉토리가 존재하지 않음: " + base_path);
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
    
    std::regex status_regex(R"(\{status\})");
    result = std::regex_replace(result, status_regex, std::to_string(alarm.st));
    
    // 타임스탬프 치환
    std::regex timestamp_regex(R"(\{timestamp\})");
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    result = std::regex_replace(result, timestamp_regex, ss.str());
    
    // 날짜 관련 치환
    std::regex date_regex(R"(\{date\})");
    std::stringstream date_ss;
    date_ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    result = std::regex_replace(result, date_regex, date_ss.str());
    
    std::regex time_regex(R"(\{time\})");
    std::stringstream time_ss;
    time_ss << std::put_time(std::gmtime(&time_t), "%H:%M:%S");
    result = std::regex_replace(result, time_regex, time_ss.str());
    
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
        {"status", alarm.st},
        {"timestamp", ss.str()},
        {"alarm_type", "CRITICAL"},
        {"severity", 1},
        {"message", "Alarm occurred on " + alarm.nm + " with value " + std::to_string(alarm.vl)},
        {"source", "PulseOne CSP Gateway"},
        {"device_name", alarm.device_name.empty() ? alarm.nm : alarm.device_name},
        {"unit", alarm.unit.empty() ? "" : alarm.unit}
    };
}

/**
 * @brief 알람 메시지를 CSV 형식으로 변환
 */
std::string createAlarmCsv(const AlarmMessage& alarm) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    std::stringstream csv;
    csv << alarm.bd << ","
        << "\"" << alarm.nm << "\","
        << alarm.vl << ","
        << alarm.st << ","
        << "\"" << ss.str() << "\","
        << "\"CRITICAL\","
        << "1,"
        << "\"Alarm occurred on " << alarm.nm << "\"";
    
    return csv.str();
}

/**
 * @brief 알람 메시지를 XML 형식으로 변환
 */
std::string createAlarmXml(const AlarmMessage& alarm) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    
    std::stringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<alarm>\n"
        << "  <building_id>" << alarm.bd << "</building_id>\n"
        << "  <point_name>" << alarm.nm << "</point_name>\n"
        << "  <value>" << alarm.vl << "</value>\n"
        << "  <status>" << alarm.st << "</status>\n"
        << "  <timestamp>" << ss.str() << "</timestamp>\n"
        << "  <alarm_type>CRITICAL</alarm_type>\n"
        << "  <severity>1</severity>\n"
        << "  <message>Alarm occurred on " << alarm.nm << "</message>\n"
        << "  <source>PulseOne CSP Gateway</source>\n"
        << "</alarm>";
    
    return xml.str();
}

/**
 * @brief 타임스탬프 문자열 생성 (다양한 형식 지원)
 */
std::string getCurrentTimestamp(const std::string& format) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    if (format == "iso8601") {
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    } else if (format == "filename") {
        ss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
    } else if (format == "date") {
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    } else if (format == "time") {
        ss << std::put_time(std::gmtime(&time_t), "%H:%M:%S");
    } else {
        // 기본 형식
        ss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
    }
    
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
    try {
        return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 존재 확인 실패: " + file_path + " - " + e.what());
        return false;
    }
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
 * @brief 파일의 마지막 수정 시간 조회
 */
std::chrono::system_clock::time_point getFileModificationTime(const std::string& file_path) {
    try {
        if (std::filesystem::exists(file_path)) {
            auto ftime = std::filesystem::last_write_time(file_path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            return sctp;
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 수정 시간 조회 실패: " + file_path + " - " + e.what());
    }
    return std::chrono::system_clock::now();
}

/**
 * @brief HTTP 상태 코드 설명
 */
std::string getHttpStatusDescription(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 422: return "Unprocessable Entity";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: 
            if (status_code >= 200 && status_code < 300) return "Success";
            else if (status_code >= 300 && status_code < 400) return "Redirection";
            else if (status_code >= 400 && status_code < 500) return "Client Error";
            else if (status_code >= 500 && status_code < 600) return "Server Error";
            else return "Unknown Status";
    }
}

/**
 * @brief 재시도 지연 시간 계산 (지수 백오프)
 */
std::chrono::milliseconds calculateRetryDelay(int retry_count, int base_delay_ms) {
    if (base_delay_ms <= 0) base_delay_ms = 1000;
    
    // 지수 백오프: base_delay * 2^retry_count (최대 60초)
    int max_delay_ms = 60000; // 60초
    int delay_ms = base_delay_ms * (1 << std::min(retry_count, 6)); // 최대 64배
    delay_ms = std::min(delay_ms, max_delay_ms);
    
    // 지터 추가 (±20%)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.8, 1.2);
    
    return std::chrono::milliseconds(static_cast<int>(delay_ms * dis(gen)));
}

/**
 * @brief 문자열을 안전하게 이스케이프
 */
std::string escapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.length() + 16);
    
    for (char c : input) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c >= 0 && c < 32) {
                    output += "\\u" + std::to_string(static_cast<unsigned char>(c));
                } else {
                    output += c;
                }
                break;
        }
    }
    
    return output;
}

/**
 * @brief CSV 필드 이스케이프
 */
std::string escapeCsvField(const std::string& field) {
    if (field.find(',') != std::string::npos || 
        field.find('"') != std::string::npos || 
        field.find('\n') != std::string::npos ||
        field.find('\r') != std::string::npos) {
        
        std::string escaped = "\"";
        for (char c : field) {
            if (c == '"') {
                escaped += "\"\"";  // 큰따옴표 이스케이프
            } else {
                escaped += c;
            }
        }
        escaped += "\"";
        return escaped;
    }
    return field;
}

/**
 * @brief XML 문자 이스케이프
 */
std::string escapeXmlText(const std::string& text) {
    std::string result;
    result.reserve(text.length() + 16);
    
    for (char c : text) {
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += c; break;
        }
    }
    
    return result;
}

/**
 * @brief 파일 안전하게 읽기
 */
std::string readFileContents(const std::string& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            LogManager::getInstance().Error("파일 열기 실패: " + file_path);
            return "";
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), 
                           std::istreambuf_iterator<char>());
        return content;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 읽기 예외: " + file_path + " - " + std::string(e.what()));
        return "";
    }
}

/**
 * @brief 파일 안전하게 쓰기
 */
bool writeFileContents(const std::string& file_path, const std::string& content, bool create_dirs) {
    try {
        if (create_dirs) {
            // 디렉토리 생성
            std::filesystem::path path(file_path);
            std::filesystem::create_directories(path.parent_path());
        }
        
        std::ofstream file(file_path);
        if (!file.is_open()) {
            LogManager::getInstance().Error("파일 생성 실패: " + file_path);
            return false;
        }
        
        file << content;
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 쓰기 예외: " + file_path + " - " + std::string(e.what()));
        return false;
    }
}

/**
 * @brief URL 인코딩
 */
std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // RFC 3986에 따른 안전한 문자들
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

/**
 * @brief Base64 인코딩 (간단한 구현)
 */
std::string base64Encode(const std::string& input) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}

/**
 * @brief 알람 심각도를 문자열로 변환
 */
std::string severityToString(int severity) {
    switch (severity) {
        case 0: return "INFO";
        case 1: return "WARNING";
        case 2: return "MINOR";
        case 3: return "MAJOR";
        case 4: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 안전한 파일명 생성 (특수 문자 제거)
 */
std::string sanitizeFileName(const std::string& filename) {
    std::string result;
    for (char c : filename) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.') {
            result += c;
        } else {
            result += '_';
        }
    }
    return result;
}

// =============================================================================
// 레거시 호환성 함수들 (기존 코드와의 호환성 유지)
// =============================================================================

/**
 * @brief 기존 함수명 호환성 유지
 */
std::string getCurrentTimestamp() {
    return getCurrentTimestamp("filename");
}

} // namespace CSP
} // namespace PulseOne