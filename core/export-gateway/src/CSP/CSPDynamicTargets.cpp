/**
 * @file CSPDynamicTargets.cpp
 * @brief CSP Gateway 동적 전송 대상 시스템 구현
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/CSPDynamicTargets.cpp
 */

#include "CSP/CSPDynamicTargets.h"
#include "Client/HttpClient.h"
#include "Client/S3Client.h"
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
// HTTP Target Handler 구현
// =============================================================================

bool HttpTargetHandler::initialize(const json& config) {
    try {
        // 필수 필드 검증
        if (!config.contains("endpoint") || config["endpoint"].empty()) {
            LogManager::getInstance().Error("HTTP target missing endpoint");
            return false;
        }
        
        LogManager::getInstance().Info("HTTP target handler initialized: " + 
            config["endpoint"].get<std::string>());
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("HTTP handler initialization failed: " + std::string(e.what()));
        return false;
    }
}

TargetSendResult HttpTargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "http";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 설정 추출
        std::string endpoint = config["endpoint"];
        std::string method = config.value("method", "POST");
        std::string content_type = config.value("content_type", "application/json");
        int timeout_ms = config.value("timeout_ms", 10000);
        
        // HTTP 클라이언트 옵션 설정
        PulseOne::Client::HttpRequestOptions options;
        options.timeout_sec = timeout_ms / 1000;
        options.connect_timeout_sec = 5;
        options.user_agent = "PulseOne-CSPGateway/1.8";
        
        // 인증 키 로드
        if (config.contains("auth_key_file")) {
            std::string auth_key = loadAuthKey(config["auth_key_file"]);
            if (!auth_key.empty()) {
                options.bearer_token = auth_key;
            }
        }
        
        // HTTP 클라이언트 생성
        PulseOne::Client::HttpClient client(endpoint, options);
        
        // 요청 데이터 준비
        std::string json_data = alarm.to_json().dump();
        
        // 헤더 준비
        auto headers = prepareHeaders(config);
        
        // HTTP 요청 전송
        PulseOne::Client::HttpResponse response;
        if (method == "POST") {
            response = client.post("", json_data, content_type, headers);
        } else if (method == "PUT") {
            response = client.put("", json_data, content_type, headers);
        } else {
            result.error_message = "Unsupported HTTP method: " + method;
            return result;
        }
        
        // 결과 처리
        result.success = response.isSuccess();
        result.status_code = response.status_code;
        result.response_body = response.body;
        
        if (!result.success) {
            result.error_message = "HTTP " + std::to_string(response.status_code) + ": " + response.body;
        }
        
    } catch (const std::exception& e) {
        result.error_message = "HTTP request exception: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

bool HttpTargetHandler::testConnection(const json& config) {
    try {
        std::string endpoint = config["endpoint"];
        
        PulseOne::Client::HttpRequestOptions options;
        options.timeout_sec = 5;
        options.connect_timeout_sec = 3;
        
        PulseOne::Client::HttpClient client(endpoint, options);
        
        // HEAD 요청으로 연결 테스트
        auto response = client.get("/health", {});
        
        // 2xx, 3xx, 심지어 404도 연결은 성공으로 간주
        return response.status_code > 0 && response.status_code < 500;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Debug("HTTP connection test failed: " + std::string(e.what()));
        return false;
    }
}

std::string HttpTargetHandler::loadAuthKey(const std::string& key_file) {
    try {
        ConfigManager& config_mgr = ConfigManager::getInstance();
        return config_mgr.getSecret(key_file);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to load auth key from " + key_file + ": " + std::string(e.what()));
        return "";
    }
}

std::unordered_map<std::string, std::string> HttpTargetHandler::prepareHeaders(const json& config) {
    std::unordered_map<std::string, std::string> headers;
    
    // 기본 헤더
    headers["Content-Type"] = config.value("content_type", "application/json");
    headers["Accept"] = "application/json";
    headers["User-Agent"] = "PulseOne-CSPGateway/1.8";
    
    // 설정에서 추가 헤더 로드
    if (config.contains("headers") && config["headers"].is_object()) {
        for (const auto& [key, value] : config["headers"].items()) {
            if (value.is_string()) {
                headers[key] = value.get<std::string>();
            }
        }
    }
    
    return headers;
}

// =============================================================================
// S3 Target Handler 구현
// =============================================================================

bool S3TargetHandler::initialize(const json& config) {
    try {
        // 필수 필드 검증
        if (!config.contains("bucket_name") || config["bucket_name"].empty()) {
            LogManager::getInstance().Error("S3 target missing bucket_name");
            return false;
        }
        
        LogManager::getInstance().Info("S3 target handler initialized: " + 
            config["bucket_name"].get<std::string>());
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("S3 handler initialization failed: " + std::string(e.what()));
        return false;
    }
}

TargetSendResult S3TargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "s3";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // S3 설정 구성
        PulseOne::Client::S3Config s3_config;
        s3_config.bucket_name = config["bucket_name"];
        s3_config.region = config.value("region", "ap-northeast-2");
        s3_config.endpoint = config.value("endpoint", "https://s3.amazonaws.com");
        s3_config.upload_timeout_sec = config.value("timeout_ms", 10000) / 1000;
        
        // 자격증명 로드
        s3_config.access_key = loadCredentials(config.value("access_key_file", ""));
        s3_config.secret_key = loadCredentials(config.value("secret_key_file", ""));
        
        if (s3_config.access_key.empty() || s3_config.secret_key.empty()) {
            result.error_message = "S3 credentials not available";
            return result;
        }
        
        // S3 클라이언트 생성
        PulseOne::Client::S3Client s3_client(s3_config);
        
        // 객체 키 생성
        std::string object_key = generateObjectKey(alarm, config);
        
        // JSON 데이터 업로드
        std::string json_data = alarm.to_json().dump(2);
        auto s3_result = s3_client.uploadJson(object_key, json_data);
        
        result.success = s3_result.success;
        result.response_body = object_key;
        
        if (!result.success) {
            result.error_message = s3_result.error_message;
        }
        
    } catch (const std::exception& e) {
        result.error_message = "S3 upload exception: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

bool S3TargetHandler::testConnection(const json& config) {
    try {
        PulseOne::Client::S3Config s3_config;
        s3_config.bucket_name = config["bucket_name"];
        s3_config.region = config.value("region", "ap-northeast-2");
        s3_config.access_key = loadCredentials(config.value("access_key_file", ""));
        s3_config.secret_key = loadCredentials(config.value("secret_key_file", ""));
        
        if (s3_config.access_key.empty() || s3_config.secret_key.empty()) {
            return false;
        }
        
        PulseOne::Client::S3Client s3_client(s3_config);
        return s3_client.testConnection();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Debug("S3 connection test failed: " + std::string(e.what()));
        return false;
    }
}

std::string S3TargetHandler::generateObjectKey(const AlarmMessage& alarm, const json& config) {
    std::string pattern = config.value("file_name_pattern", "{building_id}_{timestamp}_alarm.json");
    std::string prefix = config.value("object_prefix", "alarms/");
    
    // 변수 치환
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
    std::string timestamp = oss.str();
    
    // 패턴 치환
    std::string object_key = pattern;
    object_key = std::regex_replace(object_key, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    object_key = std::regex_replace(object_key, std::regex("\\{timestamp\\}"), timestamp);
    object_key = std::regex_replace(object_key, std::regex("\\{nm\\}"), alarm.nm);
    
    return prefix + object_key;
}

std::string S3TargetHandler::loadCredentials(const std::string& key_file) {
    if (key_file.empty()) {
        return "";
    }
    
    try {
        ConfigManager& config_mgr = ConfigManager::getInstance();
        return config_mgr.getSecret(key_file);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to load S3 credentials from " + key_file + ": " + std::string(e.what()));
        return "";
    }
}

// =============================================================================
// MQTT Target Handler 구현 (기본 구조)
// =============================================================================

bool MqttTargetHandler::initialize(const json& config) {
    try {
        // 필수 필드 검증
        if (!config.contains("broker_host") || config["broker_host"].empty()) {
            LogManager::getInstance().Error("MQTT target missing broker_host");
            return false;
        }
        
        LogManager::getInstance().Info("MQTT target handler initialized: " + 
            config["broker_host"].get<std::string>());
        
        // TODO: Paho MQTT 클라이언트 초기화
        LogManager::getInstance().Warn("MQTT handler not fully implemented yet");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("MQTT handler initialization failed: " + std::string(e.what()));
        return false;
    }
}

TargetSendResult MqttTargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "mqtt";
    
    // TODO: MQTT 메시지 발행 구현
    result.success = false;
    result.error_message = "MQTT handler not implemented";
    
    LogManager::getInstance().Debug("MQTT sendAlarm called for alarm: " + alarm.nm);
    
    return result;
}

bool MqttTargetHandler::testConnection(const json& config) {
    // TODO: MQTT 브로커 연결 테스트
    LogManager::getInstance().Debug("MQTT connection test not implemented");
    return false;
}

std::string MqttTargetHandler::generateTopic(const AlarmMessage& alarm, const json& config) {
    std::string pattern = config.value("topic_pattern", "alarms/{building_id}/{alarm_type}");
    
    // 패턴 치환
    std::string topic = pattern;
    topic = std::regex_replace(topic, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    topic = std::regex_replace(topic, std::regex("\\{alarm_type\\}"), alarm.lvl);
    topic = std::regex_replace(topic, std::regex("\\{nm\\}"), alarm.nm);
    
    return topic;
}

// =============================================================================
// File Target Handler 구현 (로컬 백업)
// =============================================================================

bool FileTargetHandler::initialize(const json& config) {
    try {
        // 필수 필드 검증
        if (!config.contains("base_path") || config["base_path"].empty()) {
            LogManager::getInstance().Error("File target missing base_path");
            return false;
        }
        
        // 디렉토리 생성
        std::string base_path = config["base_path"];
        std::filesystem::create_directories(base_path);
        
        LogManager::getInstance().Info("File target handler initialized: " + base_path);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("File handler initialization failed: " + std::string(e.what()));
        return false;
    }
}

TargetSendResult FileTargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "file";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 파일 경로 생성
        std::string file_path = generateFilePath(alarm, config);
        
        // 디렉토리 생성 (필요시)
        std::filesystem::create_directories(std::filesystem::path(file_path).parent_path());
        
        // JSON 데이터 저장
        std::string json_data = alarm.to_json().dump(2);
        
        std::ofstream file(file_path);
        if (!file.is_open()) {
            result.error_message = "Failed to open file: " + file_path;
            return result;
        }
        
        file << json_data;
        file.close();
        
        // 압축 처리 (설정시)
        std::string compression = config.value("compression", "none");
        if (compression != "none") {
            bool compressed = compressFile(file_path, compression);
            if (!compressed) {
                LogManager::getInstance().Warn("File compression failed: " + file_path);
            }
        }
        
        result.success = true;
        result.response_body = file_path;
        
        // 자동 정리 (설정시)
        if (config.value("rotate_daily", false)) {
            cleanupOldFiles(config);
        }
        
    } catch (const std::exception& e) {
        result.error_message = "File save exception: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    return result;
}

bool FileTargetHandler::testConnection(const json& config) {
    try {
        std::string base_path = config["base_path"];
        
        // 디렉토리 존재 확인 및 쓰기 권한 테스트
        std::filesystem::create_directories(base_path);
        
        // 테스트 파일 작성
        std::string test_file = base_path + "/test_write_permission.tmp";
        std::ofstream test(test_file);
        if (!test.is_open()) {
            return false;
        }
        
        test << "test";
        test.close();
        
        // 테스트 파일 삭제
        std::filesystem::remove(test_file);
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Debug("File connection test failed: " + std::string(e.what()));
        return false;
    }
}

std::string FileTargetHandler::generateFilePath(const AlarmMessage& alarm, const json& config) {
    std::string base_path = config["base_path"];
    std::string pattern = config.value("file_pattern", "{building_id}/{date}/alarms_{timestamp}.json");
    
    // 현재 시간 기반 변수들
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream date_oss;
    date_oss << std::put_time(std::localtime(&time_t), "%Y%m%d");
    std::string date = date_oss.str();
    
    std::ostringstream timestamp_oss;
    timestamp_oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    std::string timestamp = timestamp_oss.str();
    
    // 패턴 치환
    std::string file_path = pattern;
    file_path = std::regex_replace(file_path, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    file_path = std::regex_replace(file_path, std::regex("\\{date\\}"), date);
    file_path = std::regex_replace(file_path, std::regex("\\{timestamp\\}"), timestamp);
    file_path = std::regex_replace(file_path, std::regex("\\{nm\\}"), alarm.nm);
    
    return base_path + "/" + file_path;
}

bool FileTargetHandler::compressFile(const std::string& file_path, const std::string& compression_type) {
    // TODO: 압축 구현 (gzip, zip 등)
    LogManager::getInstance().Debug("File compression not implemented: " + compression_type + " for " + file_path);
    return false;
}

void FileTargetHandler::cleanupOldFiles(const json& config) {
    try {
        int cleanup_after_days = config.value("cleanup_after_days", 30);
        std::string base_path = config["base_path"];
        
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * cleanup_after_days);
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(base_path)) {
            if (entry.is_regular_file()) {
                auto file_time = std::filesystem::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                
                if (sctp < cutoff_time) {
                    std::filesystem::remove(entry);
                    LogManager::getInstance().Debug("Cleaned up old file: " + entry.path().string());
                }
            }
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("File cleanup failed: " + std::string(e.what()));
    }
}

// =============================================================================
// Circuit Breaker 구현
// =============================================================================

CircuitBreaker::CircuitBreaker(size_t failure_threshold, 
                               std::chrono::milliseconds recovery_timeout,
                               size_t half_open_requests)
    : failure_threshold_(failure_threshold)
    , recovery_timeout_(recovery_timeout)
    , half_open_requests_(half_open_requests) {
}

bool CircuitBreaker::canExecute() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    auto now = std::chrono::system_clock::now();
    
    switch (state_.load()) {
        case State::CLOSED:
            return true;
            
        case State::OPEN:
            // 복구 시간이 지났는지 확인
            if (now - last_failure_time_ >= recovery_timeout_) {
                state_.store(State::HALF_OPEN);
                success_count_.store(0);
                return true;
            }
            return false;
            
        case State::HALF_OPEN:
            // 제한된 요청만 허용
            return success_count_.load() < half_open_requests_;
    }
    
    return false;
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    success_count_++;
    failure_count_.store(0);
    
    if (state_.load() == State::HALF_OPEN && 
        success_count_.load() >= half_open_requests_) {
        state_.store(State::CLOSED);
    }
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    failure_count_++;
    last_failure_time_ = std::chrono::system_clock::now();
    
    if (failure_count_.load() >= failure_threshold_) {
        state_.store(State::OPEN);
    }
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    state_.store(State::CLOSED);
    failure_count_.store(0);
    success_count_.store(0);
}

} // namespace CSP
} // namespace PulseOne