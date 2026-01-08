/**
 * @file S3TargetHandler.cpp
 * @brief S3 타겟 핸들러 - Stateless 패턴 (v2.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0 - Production-Ready with ClientCacheManager
 * 저장 위치: core/export-gateway/src/CSP/S3TargetHandler.cpp
 * 
 * 🚀 v2.0 주요 변경:
 * - Stateless 핸들러 패턴 적용
 * - ClientCacheManager 기반 클라이언트 캐싱
 * - initialize() 선택적 (없어도 동작)
 * - Thread-safe 보장
 */

#include "CSP/S3TargetHandler.h"
#include "Client/S3Client.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/ClientCacheManager.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

namespace PulseOne {
namespace CSP {

// =============================================================================
// Static S3Client Cache (모든 인스턴스 공유)
// =============================================================================
static Utils::ClientCacheManager<Client::S3Client, Client::S3Config>& getS3ClientCache() {
    static Utils::ClientCacheManager<Client::S3Client, Client::S3Config> cache(
        [](const Client::S3Config& config) {
            // 팩토리: S3Client 생성
            return std::make_shared<Client::S3Client>(config);
        },
        600  // 10분 유휴 시간 (S3는 재연결 비용이 크므로 길게)
    );
    return cache;
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

S3TargetHandler::S3TargetHandler() {
    LogManager::getInstance().Info("S3TargetHandler 초기화 (Stateless)");
}

S3TargetHandler::~S3TargetHandler() {
    LogManager::getInstance().Info("S3TargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool S3TargetHandler::initialize(const json& config) {
    // ✅ Stateless 패턴: initialize()는 선택적
    // 설정 검증만 수행
    std::vector<std::string> errors;
    bool valid = validateConfig(config, errors);
    
    if (!valid) {
        for (const auto& error : errors) {
            LogManager::getInstance().Error("초기화 검증 실패: " + error);
        }
    }
    
    LogManager::getInstance().Info("S3 타겟 핸들러 초기화 완료 (Stateless)");
    return valid;
}

TargetSendResult S3TargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "S3";
    result.target_name = getTargetName(config);
    result.success = false;
    
    try {
        // ✅ 버킷명 추출
        std::string bucket_name = extractBucketName(config);
        if (bucket_name.empty()) {
            result.error_message = "버킷명이 설정되지 않음";
            LogManager::getInstance().Error(result.error_message);
            return result;
        }
        
        LogManager::getInstance().Info("S3 알람 업로드 시작: " + result.target_name);
        
        // ✅ 클라이언트 획득 (캐시 활용)
        auto client = getOrCreateClient(config, bucket_name);
        if (!client) {
            result.error_message = "S3 클라이언트 생성 실패";
            LogManager::getInstance().Error(result.error_message);
            return result;
        }
        
        // 객체 키 생성
        std::string object_key = generateObjectKey(alarm, config);
        LogManager::getInstance().Debug("S3 객체 키: " + object_key);
        
        // JSON 내용 생성
        std::string json_content = buildJsonContent(alarm, config);
        
        // 압축 처리 (선택사항)
        bool compression_enabled = config.value("compression_enabled", false);
        if (compression_enabled) {
            int compression_level = config.value("compression_level", 6);
            json_content = compressContent(json_content, compression_level);
            LogManager::getInstance().Debug("압축 완료 - 크기: " + 
                std::to_string(json_content.length()) + " bytes");
        }
        
        // 메타데이터 생성
        auto metadata = buildMetadata(alarm, config);
        
        // ✅ S3 업로드 실행
        auto upload_result = client->uploadJson(object_key, json_content, metadata);
        
        // 결과 처리
        result.success = upload_result.success;
        result.response_time = std::chrono::milliseconds(
            static_cast<long>(upload_result.upload_time_ms));
        result.content_size = json_content.length();
        
        if (upload_result.success) {
            result.s3_object_key = object_key;
            success_count_++;
            total_bytes_uploaded_ += json_content.length();
            
            LogManager::getInstance().Info("✅ S3 업로드 성공: " + object_key + 
                " (ETag: " + upload_result.etag + 
                ", 소요시간: " + std::to_string(result.response_time.count()) + "ms)");
        } else {
            result.error_message = upload_result.error_message;
            failure_count_++;
            
            LogManager::getInstance().Error("❌ S3 업로드 실패: " + object_key + 
                " - " + result.error_message);
        }
        
        upload_count_++;
        
    } catch (const std::exception& e) {
        result.error_message = "S3 업로드 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
        failure_count_++;
    }
    
    return result;
}

bool S3TargetHandler::testConnection(const json& config) {
    try {
        LogManager::getInstance().Info("S3 연결 테스트 시작");
        
        // ✅ 설정 검증 먼저 (크래시 방지)
        std::vector<std::string> errors;
        if (!validateConfig(config, errors)) {
            for (const auto& err : errors) {
                LogManager::getInstance().Error("설정 검증 실패: " + err);
            }
            LogManager::getInstance().Error("❌ S3 연결 테스트 실패: 설정 오류");
            return false;
        }
        
        std::string bucket_name = extractBucketName(config);
        if (bucket_name.empty()) {
            LogManager::getInstance().Error("❌ S3 연결 테스트 실패: 버킷명이 없음");
            return false;
        }
        
        // ✅ 클라이언트 생성 (검증 포함)
        auto client = getOrCreateClient(config, bucket_name);
        if (!client) {
            LogManager::getInstance().Error("❌ S3 연결 테스트 실패: 클라이언트 생성 실패 (설정 확인 필요)");
            return false;
        }
        
        // ✅ 안전한 testConnection 호출
        bool success = false;
        try {
            success = client->testConnection();
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("❌ S3 연결 테스트 실패: " + std::string(e.what()));
            return false;
        } catch (...) {
            LogManager::getInstance().Error("❌ S3 연결 테스트 실패: 알 수 없는 예외");
            return false;
        }
        
        if (success) {
            LogManager::getInstance().Info("✅ S3 연결 테스트 성공");
            
            // 추가 검증: 테스트 업로드/삭제 (선택사항)
            if (config.value("test_upload", false)) {
                try {
                    std::string test_key = "test/connection_test_" + 
                        generateTimestampString() + ".json";
                    json test_content = {
                        {"test", true},
                        {"timestamp", getCurrentTimestamp()},
                        {"source", "PulseOne-CSPGateway-Test"}
                    };
                    
                    auto result = client->uploadJson(test_key, test_content.dump(), {});
                    if (result.success) {
                        LogManager::getInstance().Info("✅ 테스트 업로드 성공: " + test_key);
                    } else {
                        LogManager::getInstance().Warn("⚠️  테스트 업로드 실패 (연결은 성공)");
                        // 연결은 성공했으므로 true 유지
                    }
                } catch (const std::exception& e) {
                    LogManager::getInstance().Warn("⚠️  테스트 업로드 예외: " + std::string(e.what()));
                    // 연결은 성공했으므로 true 유지
                }
            }
        } else {
            LogManager::getInstance().Error("❌ S3 연결 테스트 실패");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ S3 연결 테스트 예외: " + std::string(e.what()));
        return false;
    } catch (...) {
        // ✅ 모든 예외 처리
        LogManager::getInstance().Error("❌ S3 연결 테스트 알 수 없는 예외 발생");
        return false;
    }
}

bool S3TargetHandler::validateConfig(const json& config, std::vector<std::string>& errors) {
    errors.clear();
    
    // 버킷명 검증
    if (!config.contains("bucket_name")) {
        errors.push_back("bucket_name 필드가 필수입니다");
        return false;
    }
    
    std::string bucket_name = extractBucketName(config);
    if (bucket_name.empty()) {
        errors.push_back("bucket_name이 비어있습니다");
        return false;
    }
    
    // 버킷명 형식 검증 (AWS S3 규칙)
    if (bucket_name.length() < 3 || bucket_name.length() > 63) {
        errors.push_back("bucket_name은 3-63자여야 합니다");
        return false;
    }
    
    // 영문 소문자, 숫자, 하이픈만 허용
    if (!std::regex_match(bucket_name, std::regex("^[a-z0-9][a-z0-9.-]*[a-z0-9]$"))) {
        errors.push_back("bucket_name은 소문자, 숫자, 하이픈만 사용 가능합니다");
        return false;
    }
    
    return true;
}

json S3TargetHandler::getStatus() const {
    auto cache_stats = getS3ClientCache().getStats();
    
    return json{
        {"type", "S3"},
        {"upload_count", upload_count_.load()},
        {"success_count", success_count_.load()},
        {"failure_count", failure_count_.load()},
        {"total_bytes_uploaded", total_bytes_uploaded_.load()},
        {"cache_stats", {
            {"active_clients", cache_stats.active_clients},
            {"total_entries", cache_stats.total_entries}
        }}
    };
}

void S3TargetHandler::cleanup() {
    getS3ClientCache().clear();
    LogManager::getInstance().Info("S3TargetHandler 정리 완료");
}

// =============================================================================
// Private 핵심 메서드
// =============================================================================

std::shared_ptr<Client::S3Client> S3TargetHandler::getOrCreateClient(
    const json& config,
    const std::string& bucket_name) {
    
    // ✅ 캐시 키: bucket_name (버킷별로 클라이언트 재사용)
    std::string cache_key = bucket_name;
    
    // S3 설정 구성
    Client::S3Config s3_config = buildS3Config(config);
    
    // ✅ 설정 유효성 검증 (크래시 방지)
    if (!s3_config.isValid()) {
        std::string error_detail = "Invalid S3 configuration: ";
        if (s3_config.access_key.empty()) error_detail += "access_key is empty; ";
        if (s3_config.secret_key.empty()) error_detail += "secret_key is empty; ";
        if (s3_config.bucket_name.empty()) error_detail += "bucket_name is empty; ";
        
        LogManager::getInstance().Error(error_detail);
        return nullptr;  // ✅ nullptr 반환 (크래시 방지)
    }
    
    // ✅ 캐시에서 가져오거나 생성
    try {
        auto client = getS3ClientCache().getOrCreate(cache_key, s3_config);
        
        // ✅ 생성 실패 시 nullptr 체크
        if (!client) {
            LogManager::getInstance().Error("S3Client 생성 실패: " + bucket_name);
            return nullptr;
        }
        
        return client;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("S3Client 생성 예외: " + std::string(e.what()));
        return nullptr;
    } catch (...) {
        LogManager::getInstance().Error("S3Client 생성 알 수 없는 예외");
        return nullptr;
    }
}

std::string S3TargetHandler::extractBucketName(const json& config) const {
    if (config.contains("bucket_name") && !config["bucket_name"].get<std::string>().empty()) {
        std::string bucket_name = config["bucket_name"].get<std::string>();
        return expandEnvironmentVariables(bucket_name);
    }
    return "";
}

Client::S3Config S3TargetHandler::buildS3Config(const json& config) const {
    Client::S3Config s3_config;
    
    // 버킷명
    s3_config.bucket_name = extractBucketName(config);
    
    // 리전
    std::string region = config.value("region", "us-east-1");
    s3_config.region = expandEnvironmentVariables(region);
    
    // 엔드포인트
    if (config.contains("endpoint")) {
        std::string endpoint = config["endpoint"].get<std::string>();
        s3_config.endpoint = expandEnvironmentVariables(endpoint);
    } else {
        s3_config.endpoint = generateS3Endpoint(s3_config.region);
    }
    
    // 자격증명 로드
    loadCredentials(config, s3_config);
    
    // 타임아웃 설정
    s3_config.upload_timeout_sec = config.value("upload_timeout_sec", 60);
    s3_config.connect_timeout_sec = config.value("connect_timeout_sec", 10);
    s3_config.verify_ssl = config.value("verify_ssl", true);
    s3_config.max_retries = config.value("max_retries", 3);
    
    // 컨텐츠 타입
    std::string content_type = config.value("content_type", "application/json");
    s3_config.content_type = expandEnvironmentVariables(content_type);
    
    return s3_config;
}

void S3TargetHandler::loadCredentials(const json& config, Client::S3Config& s3_config) const {
    auto& config_manager = ConfigManager::getInstance();
    
    // 1. 파일에서 자격증명 로드 (최우선)
    if (config.contains("access_key_file") && config.contains("secret_key_file")) {
        try {
            std::string access_key_config = config["access_key_file"].get<std::string>();
            std::string secret_key_config = config["secret_key_file"].get<std::string>();
            
            std::string access_key = config_manager.getSecret(access_key_config);
            std::string secret_key = config_manager.getSecret(secret_key_config);
            
            if (!access_key.empty() && !secret_key.empty()) {
                s3_config.access_key = access_key;
                s3_config.secret_key = secret_key;
                LogManager::getInstance().Info("✅ S3 자격증명 파일에서 로드");
                return;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("자격증명 파일 로드 실패: " + std::string(e.what()));
        }
    }
    
    // 2. 직접 설정에서 로드
    if (config.contains("access_key") && config.contains("secret_key")) {
        s3_config.access_key = config_manager.expandVariables(
            config["access_key"].get<std::string>());
        s3_config.secret_key = config_manager.expandVariables(
            config["secret_key"].get<std::string>());
        LogManager::getInstance().Info("✅ S3 자격증명 설정에서 로드");
        return;
    }
    
    // 3. 환경변수에서 로드
    std::string credential_prefix = config.value("credential_prefix", "S3_");
    
    std::string access_key = config_manager.getSecret(credential_prefix + "ACCESS_KEY");
    if (!access_key.empty()) {
        s3_config.access_key = access_key;
    }
    
    std::string secret_key = config_manager.getSecret(credential_prefix + "SECRET_KEY");
    if (!secret_key.empty()) {
        s3_config.secret_key = secret_key;
    }
    
    if (s3_config.access_key.empty() || s3_config.secret_key.empty()) {
        LogManager::getInstance().Warn("⚠️  S3 자격증명이 설정되지 않음 (IAM Role 사용 가능)");
    } else {
        LogManager::getInstance().Info("✅ S3 자격증명 환경변수에서 로드");
    }
}

std::string S3TargetHandler::generateObjectKey(const AlarmMessage& alarm, const json& config) const {
    // 객체 키 템플릿
    std::string template_str = config.value("object_key_template",
        "{building_id}/{date}/{point_name}_{timestamp}_alarm.json");
    template_str = expandEnvironmentVariables(template_str);
    
    // 템플릿 확장
    std::string object_key = expandTemplate(template_str, alarm);
    
    // 경로 정규화
    object_key = std::regex_replace(object_key, std::regex("//+"), "/");
    
    // 시작 슬래시 제거
    if (!object_key.empty() && object_key[0] == '/') {
        object_key = object_key.substr(1);
    }
    
    return object_key;
}

std::string S3TargetHandler::expandTemplate(const std::string& template_str, 
                                            const AlarmMessage& alarm) const {
    std::string result = template_str;
    
    // 알람 변수 치환
    result = std::regex_replace(result, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    result = std::regex_replace(result, std::regex("\\{point_name\\}"), alarm.nm);
    result = std::regex_replace(result, std::regex("\\{value\\}"), std::to_string(alarm.vl));
    result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"), std::to_string(alarm.al));
    result = std::regex_replace(result, std::regex("\\{status\\}"), std::to_string(alarm.st));
    
    // 타임스탬프 변수
    result = std::regex_replace(result, std::regex("\\{timestamp\\}"), generateTimestampString());
    result = std::regex_replace(result, std::regex("\\{date\\}"), generateDateString());
    result = std::regex_replace(result, std::regex("\\{year\\}"), generateYearString());
    result = std::regex_replace(result, std::regex("\\{month\\}"), generateMonthString());
    result = std::regex_replace(result, std::regex("\\{day\\}"), generateDayString());
    result = std::regex_replace(result, std::regex("\\{hour\\}"), generateHourString());
    
    // 알람 상태
    result = std::regex_replace(result, std::regex("\\{alarm_status\\}"), 
        alarm.get_alarm_status_string());
    
    // 안전한 파일명으로 변환
    result = std::regex_replace(result, std::regex("[^a-zA-Z0-9/_.-]"), "_");
    
    return result;
}

std::string S3TargetHandler::buildJsonContent(const AlarmMessage& alarm, const json& config) const {
    json content;
    
    // 기본 알람 데이터
    content["building_id"] = alarm.bd;
    content["point_name"] = alarm.nm;
    content["value"] = alarm.vl;
    content["timestamp"] = alarm.tm;
    content["alarm_flag"] = alarm.al;
    content["status"] = alarm.st;
    content["description"] = alarm.des;
    
    // 메타데이터
    content["source"] = "PulseOne-CSPGateway";
    content["version"] = "2.0";
    content["upload_timestamp"] = getCurrentTimestamp();
    content["alarm_status"] = alarm.get_alarm_status_string();
    
    // 사용자 정의 필드
    if (config.contains("additional_fields") && config["additional_fields"].is_object()) {
        for (auto& [key, value] : config["additional_fields"].items()) {
            content[key] = value;
        }
    }
    
    // 압축 메타데이터
    if (config.value("compression_enabled", false)) {
        content["_compression"] = "gzip";
        content["_compression_level"] = config.value("compression_level", 6);
    }
    
    return content.dump(2);
}

std::unordered_map<std::string, std::string> S3TargetHandler::buildMetadata(
    const AlarmMessage& alarm,
    const json& config) const {
    
    std::unordered_map<std::string, std::string> metadata;
    
    // 기본 메타데이터
    metadata["building-id"] = std::to_string(alarm.bd);
    metadata["point-name"] = alarm.nm;
    metadata["alarm-flag"] = std::to_string(alarm.al);
    metadata["alarm-status"] = alarm.get_alarm_status_string();
    metadata["upload-timestamp"] = getCurrentTimestamp();
    metadata["source"] = "PulseOne-CSPGateway";
    metadata["version"] = "2.0";
    
    // 압축 정보
    if (config.value("compression_enabled", false)) {
        metadata["content-encoding"] = "gzip";
        metadata["compression-level"] = std::to_string(config.value("compression_level", 6));
    }
    
    // 사용자 정의 메타데이터
    if (config.contains("custom_metadata") && config["custom_metadata"].is_object()) {
        for (auto& [key, value] : config["custom_metadata"].items()) {
            if (value.is_string()) {
                std::string expanded_value = expandTemplate(value.get<std::string>(), alarm);
                metadata[key] = expanded_value;
            }
        }
    }
    
    return metadata;
}

std::string S3TargetHandler::compressContent(const std::string& content, int level) const {
    // 실제 구현: zlib 사용
    // 여기서는 단순화된 구현
    LogManager::getInstance().Debug("압축 (레벨: " + std::to_string(level) + 
        ") - 실제 구현 시 zlib 사용");
    return content;
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string S3TargetHandler::getTargetName(const json& config) const {
    if (config.contains("name") && config["name"].is_string()) {
        return config["name"].get<std::string>();
    }
    
    std::string bucket_name = extractBucketName(config);
    if (!bucket_name.empty()) {
        return "S3://" + bucket_name;
    }
    
    return "S3-Target";
}

std::string S3TargetHandler::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return oss.str();
}

std::string S3TargetHandler::generateTimestampString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string S3TargetHandler::generateDateString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    return oss.str();
}

std::string S3TargetHandler::generateYearString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y");
    return oss.str();
}

std::string S3TargetHandler::generateMonthString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%m");
    return oss.str();
}

std::string S3TargetHandler::generateDayString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%d");
    return oss.str();
}

std::string S3TargetHandler::generateHourString() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%H");
    return oss.str();
}

std::string S3TargetHandler::expandEnvironmentVariables(const std::string& str) const {
    std::string result = str;
    size_t pos = 0;
    
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find("}", pos + 2);
        if (end_pos == std::string::npos) break;
        
        std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
        const char* env_value = std::getenv(var_name.c_str());
        std::string replacement = env_value ? env_value : "";
        
        result.replace(pos, end_pos - pos + 1, replacement);
        pos += replacement.length();
    }
    
    return result;
}

std::string S3TargetHandler::generateS3Endpoint(const std::string& region) const {
    if (region == "us-east-1") {
        return "https://s3.amazonaws.com";
    }
    
    if (region.find("cn-") == 0) {
        return "https://s3." + region + ".amazonaws.com.cn";
    }
    
    if (region.find("us-gov-") == 0) {
        return "https://s3." + region + ".amazonaws.com";
    }
    
    return "https://s3." + region + ".amazonaws.com";
}

} // namespace CSP
} // namespace PulseOne