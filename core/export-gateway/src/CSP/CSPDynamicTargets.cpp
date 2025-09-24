/**
 * @file CSPDynamicTargets.cpp
 * @brief CSP Gateway 동적 전송 대상 시스템 구현 - 에러 수정 완성본
 * @author PulseOne Development Team  
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/CSPDynamicTargets.cpp
 * 
 * 수정사항:
 * - AlarmMessage 필드명 수정 (lvl → 실제 사용되는 필드명)
 * - FailureProtectorConfig → 직접 생성자 사용  
 * - ConfigManager API 실제 메서드명 사용
 */

#include "CSP/CSPDynamicTargets.h"
#include "CSP/FailureProtector.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <chrono>
#include <thread>
#include <algorithm>

namespace PulseOne {
namespace CSP {

// =============================================================================
// Handler 클래스들의 메서드는 각각의 .cpp 파일에서 구현됩니다:
// - HttpTargetHandler.cpp
// - S3TargetHandler.cpp  
// - MqttTargetHandler.cpp
// - FileTargetHandler.cpp
// =============================================================================

// =============================================================================
// 공통 유틸리티 함수들 (이 파일에서만 구현)
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
            LogManager::getInstance().Error("파일 타겟에 base_path가 설정되지 않음");
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 전송 결과 로깅
 */
void logSendResult(const TargetSendResult& result, const std::string& alarm_name) {
    if (result.success) {
        LogManager::getInstance().Info("알람 전송 성공: " + alarm_name + 
                                      " -> " + result.target_type + 
                                      " (응답시간: " + std::to_string(result.response_time.count()) + "ms)");
    } else {
        LogManager::getInstance().Error("알람 전송 실패: " + alarm_name + 
                                       " -> " + result.target_type + 
                                       " - " + result.error_message);
    }
}

/**
 * @brief 현재 타임스탬프 문자열 생성 (ISO 8601 형식)
 */
std::string getCurrentISOTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return oss.str();
}

/**
 * @brief 템플릿 변수 확장 (공통 유틸리티) - 수정된 필드명 사용
 */
std::string expandTemplateString(const std::string& template_str, const AlarmMessage& alarm) {
    std::string result = template_str;
    
    // 기본 변수들 치환 (실제 AlarmMessage 필드 사용)
    result = std::regex_replace(result, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    result = std::regex_replace(result, std::regex("\\{point_name\\}"), alarm.nm);
    result = std::regex_replace(result, std::regex("\\{nm\\}"), alarm.nm);  // 짧은 형태
    result = std::regex_replace(result, std::regex("\\{value\\}"), std::to_string(alarm.vl));
    result = std::regex_replace(result, std::regex("\\{vl\\}"), std::to_string(alarm.vl));  // 짧은 형태
    result = std::regex_replace(result, std::regex("\\{timestamp\\}"), alarm.tm);
    result = std::regex_replace(result, std::regex("\\{tm\\}"), alarm.tm);  // 짧은 형태
    
    // 🔥 수정: alarm.lvl 대신 실제 필드 사용 (des 필드로 대체 또는 제거)
    result = std::regex_replace(result, std::regex("\\{description\\}"), alarm.des);
    result = std::regex_replace(result, std::regex("\\{des\\}"), alarm.des);  // 짧은 형태
    result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"), std::to_string(alarm.al));
    result = std::regex_replace(result, std::regex("\\{al\\}"), std::to_string(alarm.al));  // 짧은 형태
    result = std::regex_replace(result, std::regex("\\{status\\}"), std::to_string(alarm.st));
    result = std::regex_replace(result, std::regex("\\{st\\}"), std::to_string(alarm.st));  // 짧은 형태
    
    // level/lvl 필드 제거 또는 다른 필드로 대체
    result = std::regex_replace(result, std::regex("\\{level\\}"), alarm.des);  // description으로 대체
    result = std::regex_replace(result, std::regex("\\{lvl\\}"), alarm.des);    // description으로 대체
    
    // 시간 관련 변수들
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream date_oss, time_oss, datetime_oss;
    date_oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    time_oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    datetime_oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H:%M:%S");
    
    result = std::regex_replace(result, std::regex("\\{date\\}"), date_oss.str());
    result = std::regex_replace(result, std::regex("\\{time\\}"), time_oss.str());
    result = std::regex_replace(result, std::regex("\\{datetime\\}"), datetime_oss.str());
    result = std::regex_replace(result, std::regex("\\{iso_timestamp\\}"), getCurrentISOTimestamp());
    
    return result;
}

/**
 * @brief JSON 설정에서 문자열 템플릿 확장
 */
void expandConfigTemplates(json& config, const AlarmMessage& alarm) {
    std::function<void(json&)> expand_recursive = [&](json& obj) {
        if (obj.is_string()) {
            std::string str = obj.get<std::string>();
            obj = expandTemplateString(str, alarm);
        } else if (obj.is_object()) {
            for (auto& [key, value] : obj.items()) {
                expand_recursive(value);
            }
        } else if (obj.is_array()) {
            for (auto& item : obj) {
                expand_recursive(item);
            }
        }
    };
    
    expand_recursive(config);
}

/**
 * @brief 파일 크기를 사람이 읽기 쉬운 형태로 변환
 */
std::string formatFileSize(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}

/**
 * @brief URL 유효성 검증
 */
bool isValidUrl(const std::string& url) {
    // 간단한 URL 형식 검증
    std::regex url_pattern(R"(^(https?|ftp)://[^\s/$.?#].[^\s]*$)", std::regex::icase);
    return std::regex_match(url, url_pattern);
}

/**
 * @brief 파일 경로 유효성 검증
 */
bool isValidFilePath(const std::string& path) {
    if (path.empty()) return false;
    
    // 경로에 금지된 문자가 있는지 확인
    std::regex forbidden_chars(R"([<>:"|?*])");
    if (std::regex_search(path, forbidden_chars)) {
        return false;
    }
    
    // 상대 경로 공격 방지 (.., ./)
    if (path.find("..") != std::string::npos) {
        return false;
    }
    
    return true;
}

/**
 * @brief 디렉토리 생성 (재귀적, 안전)
 */
bool createDirectorySafe(const std::string& dir_path) {
    try {
        if (!isValidFilePath(dir_path)) {
            LogManager::getInstance().Error("유효하지 않은 디렉토리 경로: " + dir_path);
            return false;
        }
        
        std::filesystem::create_directories(dir_path);
        LogManager::getInstance().Debug("디렉토리 생성/확인: " + dir_path);
        return true;
        
    } catch (const std::filesystem::filesystem_error& e) {
        LogManager::getInstance().Error("디렉토리 생성 실패: " + dir_path + " - " + e.what());
        return false;
    }
}

/**
 * @brief 환경 변수에서 설정값 로드
 */
std::string getEnvironmentVariable(const std::string& var_name, const std::string& default_value = "") {
    const char* env_value = std::getenv(var_name.c_str());
    if (env_value == nullptr) {
        if (!default_value.empty()) {
            LogManager::getInstance().Debug("환경변수 " + var_name + " 없음, 기본값 사용: " + default_value);
        }
        return default_value;
    }
    
    LogManager::getInstance().Debug("환경변수 " + var_name + " 로드됨");
    return std::string(env_value);
}

// =============================================================================
// FailureProtector 별칭 (기존 CircuitBreaker 대신)
// =============================================================================

/**
 * @brief CircuitBreaker의 별칭으로 FailureProtector 사용
 * 기존 코드와의 호환성을 위한 타입 별칭
 */
using CircuitBreaker = FailureProtector;

/**
 * @brief 실패 방지기 생성 도우미 함수 - 수정된 생성자 사용
 */
std::unique_ptr<FailureProtector> createFailureProtector(const std::string& target_name, const json& config) {
    // 🔥 수정: FailureProtectorConfig 대신 직접 생성자 매개변수 사용
    size_t failure_threshold = config.value("failure_threshold", 5);
    std::chrono::milliseconds recovery_timeout(config.value("recovery_timeout_ms", 60000));  // 1분
    size_t half_open_requests = config.value("half_open_requests", 3);
    
    // FailureProtector 생성자 직접 호출
    return std::make_unique<FailureProtector>(failure_threshold, recovery_timeout, half_open_requests);
}

/**
 * @brief 글로벌 설정 적용 - 수정된 ConfigManager API 사용
 */
void applyGlobalSettings() {
    auto& config_mgr = ConfigManager::getInstance();
    
    // 🔥 수정: getValue → getOrDefault, getIntValue → getInt 사용
    std::string log_level = config_mgr.getOrDefault("CSP.log_level", "INFO");
    LogManager::getInstance().Info("CSP Gateway 로그 레벨: " + log_level);
    
    // 글로벌 타임아웃 설정
    int global_timeout = config_mgr.getInt("CSP.global_timeout_ms", 30000);
    LogManager::getInstance().Debug("글로벌 타임아웃: " + std::to_string(global_timeout) + "ms");
    
    // 시스템 상태 로깅
    LogManager::getInstance().Info("CSP Dynamic Targets 시스템 초기화 완료");
}

/**
 * @brief 설정에서 암호화된 값 로드
 */
std::string loadEncryptedConfig(const std::string& config_key, const std::string& default_value = "") {
    try {
        auto& config_mgr = ConfigManager::getInstance();
        
        // 먼저 암호화된 시크릿으로 시도
        std::string secret_value = config_mgr.getSecret(config_key);
        if (!secret_value.empty()) {
            return secret_value;
        }
        
        // 일반 설정값으로 폴백
        return config_mgr.getOrDefault(config_key, default_value);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("암호화된 설정 로드 실패: " + config_key + " - " + e.what());
        return default_value;
    }
}

/**
 * @brief 성능 메트릭 로깅
 */
void logPerformanceMetrics(const std::string& operation, 
                          std::chrono::milliseconds duration,
                          bool success,
                          const std::string& target_type = "") {
    std::ostringstream oss;
    oss << "성능 메트릭 [" << operation << "]";
    
    if (!target_type.empty()) {
        oss << " (" << target_type << ")";
    }
    
    oss << " - 소요시간: " << duration.count() << "ms";
    oss << ", 결과: " << (success ? "성공" : "실패");
    
    if (duration.count() > 5000) {  // 5초 이상이면 경고
        LogManager::getInstance().Warn(oss.str());
    } else {
        LogManager::getInstance().Debug(oss.str());
    }
}

} // namespace CSP
} // namespace PulseOne