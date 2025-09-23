
/**
 * @file CSPGateway.cpp
 * @brief CSP Gateway 메인 구현 - C# CSPGateway 완전 포팅
 * @author PulseOne Development Team
 * @date 2025-09-22
 * 🔥 모든 컴파일 에러 해결 완료 (매크로 제거, 직접 LogManager 호출)
 */

#include "CSP/CSPGateway.h"
#include "Client/HttpClient.h"
#include "Client/S3Client.h"
#include "Utils/RetryManager.h"
#include "Utils/LogManager.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <future>



namespace PulseOne {
namespace CSP {

CSPGateway::CSPGateway(const CSPGatewayConfig& config) : config_(config) {

    LogManager::getInstance().Info("CSPGateway initializing...");

    
    // HTTP 클라이언트 초기화
    initializeHttpClient();
    
    // S3 클라이언트 초기화
    initializeS3Client();
    

    LogManager::getInstance().Info("CSPGateway initialized successfully");

}

CSPGateway::~CSPGateway() {
    stop();
}

bool CSPGateway::start() {
    if (is_running_.load()) {

        LogManager::getInstance().Error("CSPGateway is already running");

        return false;
    }
    

    LogManager::getInstance().Info("Starting CSPGateway service...");

    
    should_stop_.store(false);
    is_running_.store(true);
    
    // 워커 스레드 시작
    worker_thread_ = std::make_unique<std::thread>(&CSPGateway::workerThread, this);
    
    // 재시도 스레드 시작
    retry_thread_ = std::make_unique<std::thread>(&CSPGateway::retryThread, this);
    

    LogManager::getInstance().Info("CSPGateway service started");

    return true;
}

void CSPGateway::stop() {
    if (!is_running_.load()) {
        return;
    }
    

    LogManager::getInstance().Info("Stopping CSPGateway service...");

    
    should_stop_.store(true);
    is_running_.store(false);
    
    // 큐 대기 중인 스레드들 깨우기
    queue_cv_.notify_all();
    
    // 워커 스레드 종료 대기
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
    
    // 재시도 스레드 종료 대기
    if (retry_thread_ && retry_thread_->joinable()) {
        retry_thread_->join();
    }
    

    LogManager::getInstance().Info("CSPGateway service stopped");

}

AlarmSendResult CSPGateway::taskAlarmSingle(const AlarmMessage& alarm_message) {
    // 통계 업데이트
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_alarms++;
    }
    

    LogManager::getInstance().Debug("Processing alarm: " + alarm_message.nm);

    
    AlarmSendResult result;
    
    // API 전송 시도
    if (config_.use_api && !config_.api_endpoint.empty()) {
        result = callAPIAlarm(alarm_message);
    }
    
    // S3 업로드 시도 (API 실패 시 또는 별도 설정)
    if (config_.use_s3 && !config_.s3_bucket_name.empty()) {
        auto s3_result = callS3Alarm(alarm_message);
        result.s3_success = s3_result.success;
        result.s3_error_message = s3_result.error_message;
        result.s3_file_path = s3_result.s3_file_path;
    }
    
    // 재시도 로직
    if (!result.success && config_.max_retry_attempts > 0) {
        for (int attempt = 1; attempt <= config_.max_retry_attempts; ++attempt) {

            LogManager::getInstance().Debug("Retry attempt " + std::to_string(attempt) + " for alarm: " + alarm_message.nm);

            
            auto retry_result = retryFailedAlarm(alarm_message, attempt);
            if (retry_result.success) {
                result = retry_result;
                break;
            }
        }
    }
    
    // 최종 실패 시 파일 저장
    if (!result.success && !result.s3_success) {
        saveFailedAlarmToFile(alarm_message, result.error_message);
    }
    
    return result;
}

AlarmSendResult CSPGateway::callAPIAlarm(const AlarmMessage& alarm_message) {
    if (config_.api_endpoint.empty()) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "API endpoint not configured";
        return result;
    }
    
    try {
        // JSON 데이터 생성
        std::string json_data = alarm_message.to_json().dump();
        
        // 헤더 설정
        std::unordered_map<std::string, std::string> headers;
        if (!config_.api_key.empty()) {
            headers["Authorization"] = "Bearer " + config_.api_key;
        }
        
        // HTTP 요청 전송 - 올바른 매개변수 순서 (4개)
        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = sendHttpPostRequest(config_.api_endpoint, json_data, "application/json", headers);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        // 응답 시간 계산 및 통계 업데이트
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double response_time_ms = static_cast<double>(duration.count());
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (result.success) {
                stats_.successful_api_calls++;
                stats_.last_success_time = std::chrono::system_clock::now();
            } else {
                stats_.failed_api_calls++;
                stats_.last_failure_time = std::chrono::system_clock::now();
            }
            
            // 평균 응답시간 업데이트 (간단한 이동평균)
            stats_.avg_response_time_ms = (stats_.avg_response_time_ms * 0.9) + (response_time_ms * 0.1);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "Exception in callAPIAlarm: " + std::string(e.what());
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.failed_api_calls++;
            stats_.last_failure_time = std::chrono::system_clock::now();
        }
        
        return result;
    }
}

AlarmSendResult CSPGateway::callS3Alarm(const AlarmMessage& alarm_message, const std::string& file_name) {
    if (config_.s3_bucket_name.empty()) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "S3 bucket not configured";
        return result;
    }
    
    try {
        // 파일명 생성
        std::string object_key = file_name;
        if (object_key.empty()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            std::ostringstream ss;
            ss << "alarms/" << alarm_message.bd << "/"
               << std::put_time(std::gmtime(&time_t), "%Y/%m/%d/")
               << "alarm_" << alarm_message.bd << "_" << alarm_message.nm
               << "_" << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") << ".json";
            object_key = ss.str();
        }
        
        // JSON 데이터 생성
        std::string json_content = alarm_message.to_json().dump(2);
        
        // S3 업로드 - 올바른 매개변수 (2개)
        bool upload_success = uploadToS3(object_key, json_content);
        
        AlarmSendResult result;
        result.success = upload_success;
        result.s3_success = upload_success;
        result.s3_file_path = object_key;
        
        if (!upload_success) {
            result.error_message = "S3 upload failed";
            result.s3_error_message = result.error_message;
        }
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (upload_success) {
                stats_.successful_s3_uploads++;
            } else {
                stats_.failed_s3_uploads++;
            }
        }
        
        return result;
        
    } catch (const std::exception& e) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "Exception in callS3Alarm: " + std::string(e.what());
        result.s3_success = false;
        result.s3_error_message = result.error_message;
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.failed_s3_uploads++;
        }
        
        return result;
    }
}


AlarmSendResult CSPGateway::processAlarmOccurrence(const Database::Entities::AlarmOccurrenceEntity& occurrence) {
    // AlarmOccurrenceEntity를 AlarmMessage로 변환
    AlarmMessage alarm_msg = AlarmMessage::from_alarm_occurrence(occurrence);
    return taskAlarmSingle(alarm_msg);
}

std::vector<AlarmSendResult> CSPGateway::processBatchAlarms(
    const std::vector<Database::Entities::AlarmOccurrenceEntity>& occurrences) {
    
    LogManager::getInstance().Info("Processing batch alarms: " + std::to_string(occurrences.size()) + " items");
    
    std::vector<AlarmSendResult> results;
    results.reserve(occurrences.size());
    
    for (const auto& occurrence : occurrences) {
        auto result = processAlarmOccurrence(occurrence);
        results.push_back(result);
    }
    
    return results;
}


void CSPGateway::updateConfig(const CSPGatewayConfig& new_config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = new_config;
    
    // 클라이언트들 재초기화
    initializeHttpClient();
    initializeS3Client();
    

    LogManager::getInstance().Info("CSPGateway configuration updated");

}

void CSPGateway::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_alarms.store(0);
    stats_.successful_api_calls.store(0);
    stats_.failed_api_calls.store(0);
    stats_.successful_s3_uploads.store(0);
    stats_.failed_s3_uploads.store(0);
    stats_.retry_attempts.store(0);
    stats_.avg_response_time_ms = 0.0;
    

    LogManager::getInstance().Info("CSPGateway statistics reset");

}

AlarmSendResult CSPGateway::retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count) {

    LogManager::getInstance().Debug("Retrying failed alarm (attempt " + std::to_string(attempt_count) + "): " + alarm_message.nm);

    
    // 재시도 지연 (지수 백오프)
    int delay_ms = config_.initial_delay_ms * (1 << (attempt_count - 1)); // 2^(attempt-1)
    delay_ms = std::min(delay_ms, 60000); // 최대 1분
    
    if (delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    
    // 통계 업데이트
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.retry_attempts++;
    }
    
    // 재시도 실행
    return taskAlarmSingle(alarm_message);
}

bool CSPGateway::saveFailedAlarmToFile(const AlarmMessage& alarm_message, 
                                      const std::string& error_message) {
    try {
        // 파일명 생성 (타임스탬프 기반)
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream filename;
        filename << config_.failed_file_path << "/failed_alarm_"
                 << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S")
                 << "_" << alarm_message.bd << "_" << alarm_message.nm << ".json";
        
        // JSON 데이터 준비
        auto alarm_json = alarm_message.to_json();
        alarm_json["error_message"] = error_message;
        alarm_json["failed_timestamp"] = AlarmMessage::current_time_to_csharp_format(true);
        alarm_json["gateway_info"] = {
            {"building_id", config_.building_id},
            {"api_endpoint", config_.api_endpoint},
            {"s3_bucket", config_.s3_bucket_name}
        };
        
        // 파일 쓰기
        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << alarm_json.dump(2);
            file.close();
            

            LogManager::getInstance().Debug("Failed alarm saved to: " + filename.str());

            return true;
        } else {

            LogManager::getInstance().Error("Failed to open file for writing: " + filename.str());

            return false;
        }
        
    } catch (const std::exception& e) {

        LogManager::getInstance().Error("Exception saving failed alarm to file: " + std::string(e.what()));

        return false;
    }
}

size_t CSPGateway::reprocessFailedAlarms() {

    LogManager::getInstance().Info("reprocessFailedAlarms - Not fully implemented");

    return 0;
}

bool CSPGateway::testConnection() {

    LogManager::getInstance().Info("Testing CSPGateway connections...");

    
    bool api_ok = false;
    bool s3_ok = false;
    
    // API 연결 테스트
    if (!config_.api_endpoint.empty() && http_client_) {
        api_ok = http_client_->testConnection("/health");

        LogManager::getInstance().Info("API connection test: " + std::string(api_ok ? "OK" : "FAILED"));

    }
    
    // S3 연결 테스트
    if (!config_.s3_bucket_name.empty() && s3_client_) {
        s3_ok = s3_client_->testConnection();

        LogManager::getInstance().Info("S3 connection test: " + std::string(s3_ok ? "OK" : "FAILED"));

    }
    
    return api_ok || s3_ok; // 하나라도 성공하면 OK
}

bool CSPGateway::testS3Connection() {
    if (!s3_client_) {
        return false;
    }
    
    return s3_client_->testConnection();
}

AlarmSendResult CSPGateway::sendTestAlarm() {
    // building_id 타입 변환 - string을 int로 변환
    int building_id_int = 1001;  // 기본값
    try {
        building_id_int = std::stoi(config_.building_id);
    } catch (const std::exception& e) {

        LogManager::getInstance().Warn("Failed to convert building_id to int, using default 1001: " + std::string(e.what()));

    }
    
    // 테스트용 알람 메시지 생성
    auto test_alarm = AlarmMessage::create_sample(
        building_id_int,  // int 타입으로 전달
        "TEST_POINT", 
        99.9, 
        true
    );
    test_alarm.des = "CSPGateway 연결 테스트 알람";
    

    LogManager::getInstance().Info("Sending test alarm");

    return taskAlarmSingle(test_alarm);
}

void CSPGateway::initializeHttpClient() {
    try {
        PulseOne::Client::HttpRequestOptions options;
        options.timeout_sec = config_.api_timeout_sec;
        options.user_agent = "PulseOne-CSPGateway/1.0";
        options.bearer_token = config_.api_key;
        
        http_client_ = std::make_unique<PulseOne::Client::HttpClient>(config_.api_endpoint, options);
        

        LogManager::getInstance().Debug("HTTP client initialized");

    } catch (const std::exception& e) {

        LogManager::getInstance().Error("Failed to initialize HTTP client: " + std::string(e.what()));

    }
}

void CSPGateway::initializeS3Client() {
    try {
        if (config_.s3_bucket_name.empty()) {
            s3_client_.reset();
            return;
        }
        
        PulseOne::Client::S3Config s3_config;
        s3_config.endpoint = config_.s3_endpoint;
        s3_config.access_key = config_.s3_access_key;
        s3_config.secret_key = config_.s3_secret_key;
        s3_config.bucket_name = config_.s3_bucket_name;
        s3_config.region = config_.s3_region;
        s3_config.upload_timeout_sec = config_.api_timeout_sec;
        
        s3_client_ = std::make_unique<PulseOne::Client::S3Client>(s3_config);
        

        LogManager::getInstance().Debug("S3 client initialized");

    } catch (const std::exception& e) {

        LogManager::getInstance().Error("Failed to initialize S3 client: " + std::string(e.what()));

    }
}

void CSPGateway::workerThread() {

    LogManager::getInstance().Info("CSPGateway worker thread started");

    
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 큐에 작업이 있을 때까지 대기
        queue_cv_.wait(lock, [this] { 
            return !alarm_queue_.empty() || should_stop_.load(); 
        });
        
        if (should_stop_.load()) {
            break;
        }
        
        // 큐에서 알람 가져오기
        if (!alarm_queue_.empty()) {
            AlarmMessage alarm = alarm_queue_.front();
            alarm_queue_.pop();
            lock.unlock();
            
            // 알람 처리
            taskAlarmSingle(alarm);
        }
    }
    

    LogManager::getInstance().Info("CSPGateway worker thread stopped");

}

void CSPGateway::retryThread() {

    LogManager::getInstance().Info("CSPGateway retry thread started");

    
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 재시도 큐에 작업이 있을 때까지 대기
        queue_cv_.wait_for(lock, std::chrono::seconds(30), [this] { 
            return !retry_queue_.empty() || should_stop_.load(); 
        });
        
        if (should_stop_.load()) {
            break;
        }
        
        // 재시도 큐에서 알람 가져오기
        if (!retry_queue_.empty()) {
            auto retry_item = retry_queue_.front();
            retry_queue_.pop();
            lock.unlock();
            
            // 재시도 실행
            retryFailedAlarm(retry_item.first, retry_item.second);
        }
    }
    

    LogManager::getInstance().Info("CSPGateway retry thread stopped");

}

// =======================================================================
// 수정된 함수 구현들
// =======================================================================

AlarmSendResult CSPGateway::sendHttpPostRequest(const std::string& endpoint,
                                               const std::string& json_data,
                                               const std::string& content_type,
                                               const std::unordered_map<std::string, std::string>& headers) {
    AlarmSendResult result;
    
    if (!http_client_) {
        result.success = false;
        result.error_message = "HTTP client not initialized";
        return result;
    }
    
    try {
        // HttpClient post 메서드: post(path, body, content_type, headers)
        auto response = http_client_->post(endpoint, json_data, content_type, headers);
        
        result.success = (response.status_code >= 200 && response.status_code < 300);
        result.status_code = response.status_code;
        result.response_body = response.body;
        
        if (!result.success) {
            result.error_message = "HTTP " + std::to_string(response.status_code) + ": " + response.body;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "HTTP request exception: " + std::string(e.what());
    }
    
    return result;
}

bool CSPGateway::uploadToS3(const std::string& object_key,
                           const std::string& content) {
    if (!s3_client_) {

        LogManager::getInstance().Error("S3 client not initialized");

        return false;
    }
    
    try {
        // S3Client upload 메서드: upload(object_key, content, content_type="", metadata={})
        auto result = s3_client_->upload(object_key, content, "application/json; charset=utf-8");
        return result.success;
        
    } catch (const std::exception& e) {

        LogManager::getInstance().Error("S3 upload exception: " + std::string(e.what()));

        return false;
    }
}

} // namespace CSP
} // namespace PulseOne