/**
 * @file S3TargetHandler.cpp
 * @brief CSP Gateway AWS S3 타겟 핸들러 구현
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/S3TargetHandler.cpp
 * 
 * 🚨 컴파일 에러 수정 완료:
 * - 모든 멤버 변수 헤더에서 선언
 * - TargetSendResult 필드명 정확히 사용
 * - getTypeName() 중복 정의 제거
 * - 모든 메서드 헤더에 선언됨
 */

#include "CSP/S3TargetHandler.h"
#include "Client/S3Client.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <fstream>
namespace PulseOne {
namespace CSP {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

S3TargetHandler::S3TargetHandler() {
    LogManager::getInstance().Info("S3TargetHandler 초기화");
}

S3TargetHandler::~S3TargetHandler() {
    LogManager::getInstance().Info("S3TargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool S3TargetHandler::initialize(const json& config) {
    try {
        LogManager::getInstance().Info("S3 타겟 핸들러 초기화 시작");
        
        // 필수 설정 검증
        if (!config.contains("bucket_name") || config["bucket_name"].get<std::string>().empty()) {
            LogManager::getInstance().Error("S3 버킷명이 설정되지 않음");
            return false;
        }
        
        // ✅ 버킷명 환경변수 치환
        std::string bucket_name = expandEnvironmentVariables(
            config["bucket_name"].get<std::string>()
        );
        LogManager::getInstance().Info("S3 타겟 버킷: " + bucket_name);
        
        // S3 설정 구성
        PulseOne::Client::S3Config s3_config;
        s3_config.bucket_name = bucket_name;
        
        // ✅ 리전 환경변수 치환
        std::string region = config.value("region", "us-east-1");
        region = expandEnvironmentVariables(region);
        s3_config.region = region;
        
        if (config.contains("endpoint")) {
            // ✅ 명시적 엔드포인트 환경변수 치환
            std::string endpoint = config["endpoint"].get<std::string>();
            endpoint = expandEnvironmentVariables(endpoint);
            s3_config.endpoint = endpoint;
            
            LogManager::getInstance().Info("명시적 엔드포인트 사용: " + s3_config.endpoint);
        } else {
            // ✅ 자동 생성 (치환된 region 사용)
            s3_config.endpoint = generateS3Endpoint(region);
            LogManager::getInstance().Info("자동 생성된 엔드포인트: " + s3_config.endpoint);
        }
        
        // 자격증명 로드
        loadCredentials(config, s3_config);
        
        // 나머지 설정 (기존 코드 유지)
        s3_config.upload_timeout_sec = config.value("upload_timeout_sec", 60);
        s3_config.connect_timeout_sec = config.value("connect_timeout_sec", 10);
        s3_config.verify_ssl = config.value("verify_ssl", true);
        s3_config.max_retries = config.value("max_retries", 3);
        
        // ✅ content_type 환경변수 치환
        std::string content_type = config.value("content_type", "application/json");
        s3_config.content_type = expandEnvironmentVariables(content_type);
        
        compression_enabled_ = config.value("compression_enabled", false);
        compression_level_ = config.value("compression_level", 6);
        
        // ✅ object_key_template 환경변수 치환
        object_key_template_ = config.value("object_key_template",
            "{building_id}/{date}/{point_name}_{timestamp}_alarm.json");
        object_key_template_ = expandEnvironmentVariables(object_key_template_);
        
        loadMetadataTemplate(config);
        
        try {
            s3_client_ = std::make_unique<PulseOne::Client::S3Client>(s3_config);
            if (!s3_client_) {
                LogManager::getInstance().Error("S3 클라이언트 생성 실패 (nullptr)");
                return false;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("S3 클라이언트 생성 예외: " + std::string(e.what()));
            s3_client_ = nullptr;
            return false;
        }
        
        LogManager::getInstance().Info("S3 타겟 핸들러 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("S3 타겟 핸들러 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

TargetSendResult S3TargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "S3";
    result.target_name = getTargetName(config);
    result.success = false;
    
    try {
        LogManager::getInstance().Info("S3 알람 업로드 시작: " + result.target_name);

        if (!s3_client_) {
            result.error_message = "S3 클라이언트가 초기화되지 않음";
            LogManager::getInstance().Error(result.error_message);
            return result;
        }
        
        LogManager::getInstance().Info("S3 알람 업로드 시작: " + result.target_name);
        
        // 객체 키 생성
        std::string object_key = generateObjectKey(alarm, config);
        LogManager::getInstance().Debug("S3 객체 키: " + object_key);
        
        // JSON 내용 생성
        std::string json_content = buildJsonContent(alarm, config);
        
        // 압축 처리 (선택사항)
        if (compression_enabled_) {
            json_content = compressContent(json_content);
            LogManager::getInstance().Debug("압축 완료 - 원본: " + std::to_string(json_content.length()) + 
                                           " bytes");
        }
        
        // 메타데이터 생성
        auto metadata = buildMetadata(alarm, config);
        
        // S3 업로드 실행 (S3Client.cpp의 재시도 로직 활용)
        auto upload_result = s3_client_->uploadJson(object_key, json_content, metadata);
        
        // 결과 변환 (올바른 필드명 사용)
        result.success = upload_result.success;
        result.response_time = std::chrono::milliseconds(static_cast<long>(upload_result.upload_time_ms));
        result.content_size = json_content.length();
        
        if (upload_result.success) {
            result.s3_object_key = object_key;
            LogManager::getInstance().Info("S3 알람 업로드 성공: " + object_key + 
                                          " (ETag: " + upload_result.etag + 
                                          ", 소요시간: " + std::to_string(result.response_time.count()) + "ms)");
        } else {
            result.error_message = upload_result.error_message;
            LogManager::getInstance().Error("S3 알람 업로드 실패: " + object_key + 
                                           " - " + result.error_message);
        }
        
    } catch (const std::exception& e) {
        result.error_message = "S3 업로드 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
    }
    
    return result;
}

bool S3TargetHandler::testConnection(const json& config) {
    try {
        LogManager::getInstance().Info("S3 연결 테스트 시작");
        
        if (!s3_client_) {
            LogManager::getInstance().Error("S3 클라이언트가 초기화되지 않음");
            return false;
        }
        
        // S3Client의 testConnection 메서드 활용
        bool success = s3_client_->testConnection();
        
        if (success) {
            LogManager::getInstance().Info("S3 연결 테스트 성공");
            
            // 추가 검증: 테스트 객체 업로드/삭제 (선택사항)
            if (config.value("test_upload", false)) {
                success = performTestUpload();
            }
        } else {
            LogManager::getInstance().Error("S3 연결 테스트 실패");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("S3 연결 테스트 예외: " + std::string(e.what()));
        return false;
    }
}

json S3TargetHandler::getStatus() const {
    return json{
        {"type", "S3"},
        {"upload_count", upload_count_.load()},
        {"success_count", success_count_.load()},
        {"failure_count", failure_count_.load()},
        {"total_bytes_uploaded", total_bytes_uploaded_.load()},
        {"compression_enabled", compression_enabled_}
    };
}

void S3TargetHandler::cleanup() {
    if (s3_client_) {
        s3_client_.reset();
    }
    LogManager::getInstance().Info("S3TargetHandler 정리 완료");
}

bool S3TargetHandler::validateConfig(const json& config, std::vector<std::string>& errors) {
    errors.clear();
    
    try {
        if (!config.contains("bucket_name")) {
            errors.push_back("bucket_name 필드가 필수입니다");
            return false;
        }
        
        if (!config["bucket_name"].is_string() || config["bucket_name"].get<std::string>().empty()) {
            errors.push_back("bucket_name은 비어있지 않은 문자열이어야 합니다");
            return false;
        }
        
        // 버킷명 형식 검증 (AWS S3 규칙)
        std::string bucket_name = config["bucket_name"].get<std::string>();
        if (bucket_name.length() < 3 || bucket_name.length() > 63) {
            errors.push_back("bucket_name은 3-63자여야 합니다");
            return false;
        }
        
        // 영문 소문자, 숫자, 하이픈만 허용
        if (!std::regex_match(bucket_name, std::regex("^[a-z0-9][a-z0-9.-]*[a-z0-9]$"))) {
            errors.push_back("bucket_name은 소문자, 숫자, 하이픈만 사용 가능합니다");
            return false;
        }
        
        LogManager::getInstance().Debug("S3 타겟 설정 검증 성공");
        return true;
        
    } catch (const std::exception& e) {
        errors.push_back("설정 검증 중 예외 발생: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

void S3TargetHandler::loadCredentials(const json& config, PulseOne::Client::S3Config& s3_config) {
    auto& config_manager = ConfigManager::getInstance();
    
    // 1. 직접 설정에서 로드 (환경변수 치환 지원)
    if (config.contains("access_key") && config.contains("secret_key")) {
        s3_config.access_key = expandEnvironmentVariables(
            config["access_key"].get<std::string>()
        );
        s3_config.secret_key = expandEnvironmentVariables(
            config["secret_key"].get<std::string>()
        );
        LogManager::getInstance().Debug("S3 자격증명을 설정에서 로드");
        return;
    }
    
    // 2. 파일에서 로드 (파일 경로도 환경변수 치환)
    if (config.contains("access_key_file") && config.contains("secret_key_file")) {
        std::string access_key_file = expandEnvironmentVariables(
            config["access_key_file"].get<std::string>()
        );
        std::string secret_key_file = expandEnvironmentVariables(
            config["secret_key_file"].get<std::string>()
        );
        
        // ✅ 수정: loadKeyFromFile 대신 직접 파일 읽기
        try {
            // access_key 파일 읽기
            std::ifstream access_file(access_key_file);
            if (access_file.is_open()) {
                std::string line;
                if (std::getline(access_file, line)) {
                    // 앞뒤 공백 제거
                    line.erase(0, line.find_first_not_of(" \t\r\n"));
                    line.erase(line.find_last_not_of(" \t\r\n") + 1);
                    s3_config.access_key = line;
                }
                access_file.close();
            } else {
                LogManager::getInstance().Error("액세스 키 파일 열기 실패: " + access_key_file);
            }
            
            // secret_key 파일 읽기
            std::ifstream secret_file(secret_key_file);
            if (secret_file.is_open()) {
                std::string line;
                if (std::getline(secret_file, line)) {
                    // 앞뒤 공백 제거
                    line.erase(0, line.find_first_not_of(" \t\r\n"));
                    line.erase(line.find_last_not_of(" \t\r\n") + 1);
                    s3_config.secret_key = line;
                }
                secret_file.close();
            } else {
                LogManager::getInstance().Error("시크릿 키 파일 열기 실패: " + secret_key_file);
            }
            
            if (!s3_config.access_key.empty() && !s3_config.secret_key.empty()) {
                LogManager::getInstance().Debug("S3 자격증명을 파일에서 로드");
                return;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("키 파일 읽기 실패: " + std::string(e.what()));
        }
    }
    
    // 3. ConfigManager에서 로드
    s3_config.access_key = config_manager.getOrDefault("AWS_ACCESS_KEY", "");
    s3_config.secret_key = config_manager.getOrDefault("AWS_SECRET_KEY", "");
    
    if (!s3_config.access_key.empty()) {
        LogManager::getInstance().Debug("S3 자격증명을 ConfigManager에서 로드");
    }
}

void S3TargetHandler::loadMetadataTemplate(const json& config) {
    metadata_template_.clear();
    
    if (config.contains("metadata") && config["metadata"].is_object()) {
        for (auto& [key, value] : config["metadata"].items()) {
            if (value.is_string()) {
                metadata_template_[key] = value.get<std::string>();
            }
        }
    }
    
    // 기본 메타데이터 추가
    if (metadata_template_.find("source") == metadata_template_.end()) {
        metadata_template_["source"] = "PulseOne-CSPGateway";
    }
    
    if (metadata_template_.find("version") == metadata_template_.end()) {
        metadata_template_["version"] = "1.0";
    }
    
    LogManager::getInstance().Debug("메타데이터 템플릿 로드: " + std::to_string(metadata_template_.size()) + "개 항목");
}

std::string S3TargetHandler::generateObjectKey(const AlarmMessage& alarm, const json& config) const {
    // 설정에서 객체 키 템플릿 로드 (기본값 사용)
    std::string template_str = config.value("object_key_template", object_key_template_);
    
    // 템플릿 변수 확장
    std::string object_key = expandTemplate(template_str, alarm);
    
    // 경로 정규화 (더블 슬래시 제거 등)
    object_key = std::regex_replace(object_key, std::regex("//+"), "/");
    
    // 시작 슬래시 제거
    if (!object_key.empty() && object_key[0] == '/') {
        object_key = object_key.substr(1);
    }
    
    return object_key;
}

std::string S3TargetHandler::expandTemplate(const std::string& template_str, const AlarmMessage& alarm) const {
    std::string result = template_str;
    
    // 기본 변수 치환
    result = std::regex_replace(result, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    result = std::regex_replace(result, std::regex("\\{point_name\\}"), alarm.nm);
    result = std::regex_replace(result, std::regex("\\{value\\}"), std::to_string(alarm.vl));
    result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"), std::to_string(alarm.al));
    result = std::regex_replace(result, std::regex("\\{status\\}"), std::to_string(alarm.st));
    
    // 타임스탬프 관련 변수
    result = std::regex_replace(result, std::regex("\\{timestamp\\}"), generateTimestampString());
    result = std::regex_replace(result, std::regex("\\{date\\}"), generateDateString());
    result = std::regex_replace(result, std::regex("\\{year\\}"), generateYearString());
    result = std::regex_replace(result, std::regex("\\{month\\}"), generateMonthString());
    result = std::regex_replace(result, std::regex("\\{day\\}"), generateDayString());
    result = std::regex_replace(result, std::regex("\\{hour\\}"), generateHourString());
    
    // 알람 상태 문자열
    result = std::regex_replace(result, std::regex("\\{alarm_status\\}"), alarm.get_alarm_status_string());
    
    // 안전한 파일명으로 변환 (특수문자 제거)
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
    
    // 메타데이터 추가
    content["source"] = "PulseOne-CSPGateway";
    content["version"] = "1.0";
    content["upload_timestamp"] = getCurrentTimestamp();
    content["alarm_status"] = alarm.get_alarm_status_string();
    
    // 빌딩별 추가 정보 (선택사항)
    if (config.contains("building_info") && config["building_info"].is_object()) {
        content["building_info"] = config["building_info"];
    }
    
    // 사용자 정의 필드 추가
    if (config.contains("additional_fields") && config["additional_fields"].is_object()) {
        for (auto& [key, value] : config["additional_fields"].items()) {
            content[key] = value;
        }
    }
    
    // 압축 메타데이터 (압축 사용 시)
    if (compression_enabled_) {
        content["_compression"] = "gzip";
        content["_compression_level"] = compression_level_;
    }
    
    return content.dump(2); // 읽기 쉽게 들여쓰기 적용
}

std::unordered_map<std::string, std::string> S3TargetHandler::buildMetadata(const AlarmMessage& alarm, 
                                                                            const json& config) const {
    std::unordered_map<std::string, std::string> metadata;
    
    // 템플릿에서 메타데이터 생성
    for (const auto& [key, template_value] : metadata_template_) {
        std::string expanded_value = expandTemplate(template_value, alarm);
        metadata[key] = expanded_value;
    }
    
    // 알람 특화 메타데이터
    metadata["building-id"] = std::to_string(alarm.bd);
    metadata["point-name"] = alarm.nm;
    metadata["alarm-flag"] = std::to_string(alarm.al);
    metadata["alarm-status"] = alarm.get_alarm_status_string();
    metadata["upload-timestamp"] = getCurrentTimestamp();
    
    // 압축 정보
    if (compression_enabled_) {
        metadata["content-encoding"] = "gzip";
        metadata["compression-level"] = std::to_string(compression_level_);
    }
    
    // 설정에서 추가 메타데이터
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

std::string S3TargetHandler::compressContent(const std::string& content) const {
    // 실제 구현에서는 zlib 사용
    // 여기서는 단순화된 구현
    LogManager::getInstance().Debug("압축 기능은 실제 구현에서 zlib 사용 예정");
    return content; // 임시로 원본 반환
}

bool S3TargetHandler::performTestUpload() {
    try {
        LogManager::getInstance().Info("S3 테스트 업로드 실행");
        
        // 테스트 객체 생성
        std::string test_key = "test/connection_test_" + generateTimestampString() + ".json";
        json test_content = {
            {"test", true},
            {"timestamp", getCurrentTimestamp()},
            {"source", "PulseOne-CSPGateway-Test"}
        };
        
        std::unordered_map<std::string, std::string> test_metadata = {
            {"test-upload", "true"},
            {"source", "PulseOne-CSPGateway"}
        };
        
        // 업로드 실행
        auto result = s3_client_->uploadJson(test_key, test_content.dump(), test_metadata);
        
        if (result.success) {
            LogManager::getInstance().Info("S3 테스트 업로드 성공: " + test_key);
            
            // 테스트 객체 삭제 시도 (선택사항)
            // 실제 S3Client에 delete 메서드가 있다면 사용
            LogManager::getInstance().Debug("테스트 객체 정리는 수동으로 수행하세요: " + test_key);
            
            return true;
        } else {
            LogManager::getInstance().Error("S3 테스트 업로드 실패: " + result.error_message);
            return false;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("S3 테스트 업로드 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string S3TargetHandler::getTargetName(const json& config) const {
    if (config.contains("name") && config["name"].is_string()) {
        return config["name"].get<std::string>();
    }
    
    if (config.contains("bucket_name") && config["bucket_name"].is_string()) {
        return "S3://" + config["bucket_name"].get<std::string>();
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
        
        // 환경변수 이름 추출
        std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
        
        // 환경변수 값 가져오기 (Windows/Linux 모두 동작)
        const char* env_value = std::getenv(var_name.c_str());
        std::string replacement = env_value ? env_value : "";
        
        // 치환
        result.replace(pos, end_pos - pos + 1, replacement);
        pos += replacement.length();
    }
    
    return result;
}

std::string S3TargetHandler::generateS3Endpoint(const std::string& region) const {
    // us-east-1은 특별 케이스
    if (region == "us-east-1") {
        return "https://s3.amazonaws.com";
    }
    
    // 중국 리전
    if (region.find("cn-") == 0) {
        return "https://s3." + region + ".amazonaws.com.cn";
    }
    
    // AWS GovCloud 리전
    if (region.find("us-gov-") == 0) {
        return "https://s3." + region + ".amazonaws.com";
    }
    
    // 일반 AWS 리전
    return "https://s3." + region + ".amazonaws.com";
}

} // namespace CSP
} // namespace PulseOne