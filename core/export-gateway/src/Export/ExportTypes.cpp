/**
 * @file ExportTypes.cpp
 * @brief CSP Gateway 동적 전송 대상 시스템 구현 - icos 포맷 완성본
 * @author PulseOne Development Team  
 * @date 2025-11-04
 * @version 5.0.0 (icos C# 호환 포맷)
 * 
 * 🔄 주요 변경사항:
 * - ❌ building_id → ✅ bd
 * - ❌ point_name → ✅ nm
 * - ❌ value → ✅ vl
 * - ❌ timestamp → ✅ tm
 * - ❌ alarm_flag → ✅ al
 * - ✅ status → ✅ st (유지)
 * - ❌ description → ✅ des
 * 
 * icos C# AlarmMessage 구조와 100% 호환
 */

#include "Export/ExportTypes.h"
#include "Logging/LogManager.h"
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
// Export 네임스페이스 타입 사용 (호환성)
// =============================================================================

using namespace PulseOne::Export;

// =============================================================================
// 헬퍼 함수들 (사용되는 함수보다 먼저 정의)
// =============================================================================

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

// =============================================================================
// 유틸리티 함수들 구현
// =============================================================================

/**
 * @brief 확장된 알람 메시지 유효성 검증
 */
bool isValidAlarmMessageExtended(const AlarmMessage& alarm) {
    // 기본 검증
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
    
    // 알람 플래그 검증 (0=해제, 1=발생)
    if (alarm.al < 0 || alarm.al > 1) {
        LogManager::getInstance().Error("유효하지 않은 알람 플래그: " + std::to_string(alarm.al));
        return false;
    }
    
    return true;
}

/**
 * @brief 확장된 타겟 설정 유효성 검증
 */
bool isValidTargetConfigExtended(const json& config, const std::string& target_type) {
    // 기본 검증
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
 * @brief 문자열에서 변수 치환 (icos 포맷)
 */
std::string replaceVariables(const std::string& template_str, const AlarmMessage& alarm) {
    std::string result = template_str;
    
    // ✅ icos C# AlarmMessage 필드 치환: bd, nm, vl, tm, al, st, des
    std::regex building_regex(R"(\{building_id\}|\{bd\})");
    result = std::regex_replace(result, building_regex, std::to_string(alarm.bd));
    
    std::regex point_regex(R"(\{point_name\}|\{nm\})");
    result = std::regex_replace(result, point_regex, alarm.nm);
    
    std::regex value_regex(R"(\{value\}|\{vl\})");
    result = std::regex_replace(result, value_regex, std::to_string(alarm.vl));
    
    std::regex status_regex(R"(\{status\}|\{st\})");
    result = std::regex_replace(result, status_regex, std::to_string(alarm.st));
    
    std::regex alarm_flag_regex(R"(\{alarm_flag\}|\{al\})");
    result = std::regex_replace(result, alarm_flag_regex, std::to_string(alarm.al));
    
    std::regex description_regex(R"(\{description\}|\{des\})");
    result = std::regex_replace(result, description_regex, alarm.des);
    
    // 타임스탬프 처리
    std::regex timestamp_regex(R"(\{timestamp\}|\{tm\})");
    std::string timestamp_str;
    if (alarm.tm.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        timestamp_str = ss.str();
    } else {
        timestamp_str = alarm.tm;
    }
    result = std::regex_replace(result, timestamp_regex, timestamp_str);
    
    return result;
}

/**
 * @brief JSON 알람 메시지 생성 (icos C# 호환 포맷)
 */
json createAlarmJson(const AlarmMessage& alarm) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    
    // ✅ icos C# AlarmMessage 포맷 그대로 사용
    json j;
    j["bd"] = alarm.bd;        // Building ID
    j["nm"] = alarm.nm;        // Point Name
    j["vl"] = alarm.vl;        // Value
    j["tm"] = alarm.tm.empty() ? ss.str() : alarm.tm;  // Timestamp
    j["al"] = alarm.al;        // Alarm Status (1=발생, 0=해제)
    j["st"] = alarm.st;        // Communication Status
    j["des"] = alarm.des;      // Description
    
    return j;
}

/**
 * @brief 알람 메시지를 CSV 형식으로 변환 (icos 포맷)
 */
std::string createAlarmCsv(const AlarmMessage& alarm) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    std::stringstream csv;
    // ✅ icos 필드 순서: bd, nm, vl, tm, al, st, des
    csv << alarm.bd << ","
        << "\"" << alarm.nm << "\","
        << alarm.vl << ","
        << "\"" << (alarm.tm.empty() ? ss.str() : alarm.tm) << "\","
        << alarm.al << ","
        << alarm.st << ","
        << "\"" << alarm.des << "\"";
    
    return csv.str();
}

/**
 * @brief 알람 메시지를 XML 형식으로 변환 (icos 포맷)
 */
std::string createAlarmXml(const AlarmMessage& alarm) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    
    std::stringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<alarm>\n"
        // ✅ icos XML 태그: bd, nm, vl, tm, al, st, des
        << "  <bd>" << alarm.bd << "</bd>\n"
        << "  <nm>" << escapeXmlText(alarm.nm) << "</nm>\n"
        << "  <vl>" << alarm.vl << "</vl>\n"
        << "  <tm>" << (alarm.tm.empty() ? ss.str() : alarm.tm) << "</tm>\n"
        << "  <al>" << alarm.al << "</al>\n"
        << "  <st>" << alarm.st << "</st>\n"
        << "  <des>" << escapeXmlText(alarm.des) << "</des>\n"
        << "</alarm>";
    
    return xml.str();
}

/**
 * @brief 현재 타임스탬프 문자열 생성 (용도별 포맷)
 */
std::string getCurrentTimestamp(const std::string& format_type) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    if (format_type == "iso8601") {
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    } else if (format_type == "filename") {
        ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    } else if (format_type == "readable") {
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    } else {
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    }
    
    return ss.str();
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
 * @brief 파일 수정 시간 조회
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
    
    for (unsigned char c : input) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 32) {
                    char hex_buf[8];
                    snprintf(hex_buf, sizeof(hex_buf), "\\u%04x", c);
                    output += hex_buf;
                } else {
                    output += static_cast<char>(c);
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
                escaped += "\"\"";
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
 * @brief Base64 인코딩
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
 * @brief 안전한 파일명 생성
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
// 레거시 호환성 함수들
// =============================================================================

/**
 * @brief 기존 함수명 호환성 유지
 */
std::string getCurrentTimestamp() {
    return getCurrentTimestamp("filename");
}

} // namespace CSP
} // namespace PulseOne