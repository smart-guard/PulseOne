/**
 * @file CSPGateway.cpp
 * @brief CSP Gateway 메인 구현 - C# CSPGateway 완전 포팅
 * @author PulseOne Development Team
 * @date 2025-09-22
 */

#include "CSP/CSPGateway.h"
#include "Client/HttpClient.h"
#include "Client/S3Client.h"
#include "Utils/RetryManager.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <future>

#ifdef HAS_SHARED_LIBS
    #include "Utils/LogManager.h"
    #ifndef LOG_ERROR
        #define LOG_ERROR(msg) LogManager::getInstance().Error(msg)
        #define LOG_DEBUG(msg) LogManager::getInstance().Debug(msg)
        #define LOG_INFO(msg) LogManager::getInstance().Info(msg)
    #endif
#else
    #ifndef LOG_ERROR
        #define LOG_ERROR(msg) // 로깅 비활성화
        #define LOG_DEBUG(msg) // 로깅 비활성화
        #define LOG_INFO(msg)  // 로깅 비활성화
    #endif
#endif

namespace PulseOne {
namespace CSP {

CSPGateway::CSPGateway(const CSPGatewayConfig& config) : config_(config) {
    LOG_INFO("CSPGateway initializing...");
    
    // HTTP 클라이언트 초기화
    initializeHttpClient();
    
    // S3 클라이언트 초기화
    initializeS3Client();
    
    LOG_INFO("CSPGateway initialized successfully");
}

CSPGateway::~CSPGateway() {
    stop();
}

bool CSPGateway::start() {
    if (is_running_.load()) {
        LOG_ERROR("CSPGateway is already running");
        return false;
    }
    
    LOG_INFO("Starting CSPGateway service...");
    
    should_stop_.store(false);
    is_running_.store(true);
    
    // 워커 스레드 시작
    worker_thread_ = std::make_unique<std::thread>(&CSPGateway::workerThread, this);
    
    // 재시도 스레드 시작
    retry_thread_ = std::make_unique<std::thread>(&CSPGateway::retryThread, this);
    
    LOG_INFO("CSPGateway service started");
    return true;
}

void CSPGateway::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LOG_INFO("Stopping CSPGateway service...");
    
    should_stop_.store(true);
    is_running_.store(false);
    
    // 큐 대기 중인 스레드들 깨우기
    queue_cv_.notify_all();
    
    // 워커 스레드 종료 대기
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
    
    if (retry_thread_ && retry_thread_->joinable()) {
        retry_thread_->join();
    }
    
    LOG_INFO("CSPGateway service stopped");
}

AlarmSendResult CSPGateway::taskAlarmSingle(const AlarmMessage& alarm_message) {
    LOG_DEBUG("Processing single alarm: " + alarm_message.nm);
    
    AlarmSendResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 입력 검증
        if (!alarm_message.validate_for_csp_api()) {
            result.error_message = "Invalid alarm message";
            LOG_ERROR("Invalid alarm message: " + alarm_message.to_string());
            return result;
        }
        
        // 병렬 처리: API 호출과 S3 업로드를 동시에 실행
        std::future<AlarmSendResult> api_future = std::async(std::launch::async, 
            [this, &alarm_message]() {
                return callAPIAlarm(alarm_message);
            });
        
        std::future<AlarmSendResult> s3_future = std::async(std::launch::async,
            [this, &alarm_message]() {
                return callS3Alarm(alarm_message);
            });
        
        // 결과 대기
        auto api_result = api_future.get();
        auto s3_result = s3_future.get();
        
        // 결과 통합
        result.success = api_result.success || s3_result.success; // 하나라도 성공하면 성공
        result.http_status_code = api_result.http_status_code;
        result.timestamp = std::chrono::system_clock::now();
        
        // 오류 메시지 통합
        if (!api_result.success && !s3_result.success) {
            result.error_message = "API: " + api_result.error_message + 
                                 "; S3: " + s3_result.error_message;
        } else if (!api_result.success) {
            result.error_message = "API failed: " + api_result.error_message;
        } else if (!s3_result.success) {
            result.error_message = "S3 failed: " + s3_result.error_message;
        }
        
        // S3 관련 정보 복사
        result.s3_success = s3_result.s3_success;
        result.s3_error_message = s3_result.s3_error_message;
        result.s3_file_path = s3_result.s3_file_path;
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_alarms++;
            
            if (api_result.success) {
                stats_.successful_api_calls++;
            } else {
                stats_.failed_api_calls++;
            }
            
            if (s3_result.success) {
                stats_.successful_s3_uploads++;
            } else {
                stats_.failed_s3_uploads++;
            }
            
            if (result.success) {
                stats_.last_success_time = result.timestamp;
            } else {
                stats_.last_failure_time = result.timestamp;
                
                // 실패한 알람을 파일로 저장 (설정에 따라)
                if (config_.save_failed_to_file) {
                    saveFailedAlarmToFile(alarm_message, result.error_message);
                }
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 평균 응답 시간 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            double total_calls = stats_.successful_api_calls + stats_.failed_api_calls;
            if (total_calls > 0) {
                stats_.avg_response_time_ms = (stats_.avg_response_time_ms * (total_calls - 1) + duration.count()) / total_calls;
            }
        }
        
        LOG_DEBUG("Alarm processing completed: " + alarm_message.nm + 
                 " (API: " + (api_result.success ? "OK" : "FAIL") + 
                 ", S3: " + (s3_result.success ? "OK" : "FAIL") + 
                 ", " + std::to_string(duration.count()) + "ms)");
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Exception in taskAlarmSingle: " + std::string(e.what());
        result.timestamp = std::chrono::system_clock::now();
        
        LOG_ERROR("Exception in taskAlarmSingle: " + std::string(e.what()));
        
        // 예외 발생 시 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_alarms++;
            stats_.failed_api_calls++;
            stats_.failed_s3_uploads++;
            stats_.last_failure_time = result.timestamp;
        }
    }
    
    return result;
}

AlarmSendResult CSPGateway::callAPIAlarm(const AlarmMessage& alarm_message) {
    AlarmSendResult result;
    
    try {
        if (config_.api_endpoint.empty()) {
            result.error_message = "API endpoint not configured";
            return result;
        }
        
        // JSON 변환
        std::string json_data = alarm_message.to_json_string();
        
        // HTTP 헤더 설정
        std::unordered_map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json; charset=utf-8";
        headers["Accept"] = "application/json";
        
        // 인증 헤더 추가
        if (!config_.api_key.empty()) {
            headers["Authorization"] = "Bearer " + config_.api_key;
        }
        
        if (!config_.api_secret.empty()) {
            headers["X-API-Secret"] = config_.api_secret;
        }
        
        // 추가 헤더 (C# 호환)
        headers["X-Building-ID"] = std::to_string(alarm_message.bd);
        headers["X-Alarm-Type"] = alarm_message.get_alarm_status_string();
        headers["X-Client"] = "PulseOne-CSPGateway";
        
        LOG_DEBUG("Sending API request to: " + config_.api_endpoint);
        LOG_DEBUG("JSON payload: " + json_data);
        
        // HTTP POST 요청
        auto http_response = executeHttpPost(config_.api_endpoint, json_data, headers);
        
        result.http_status_code = http_response.first;
        result.timestamp = std::chrono::system_clock::now();
        
        if (http_response.first >= 200 && http_response.first < 300) {
            result.success = true;
            LOG_DEBUG("API call successful: " + std::to_string(http_response.first));
        } else {
            result.success = false;
            result.error_message = "HTTP " + std::to_string(http_response.first) + ": " + http_response.second;
            LOG_ERROR("API call failed: " + result.error_message);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "API call exception: " + std::string(e.what());
        result.timestamp = std::chrono::system_clock::now();
        
        LOG_ERROR("Exception in callAPIAlarm: " + std::string(e.what()));
    }
    
    return result;
}

AlarmSendResult CSPGateway::callS3Alarm(const AlarmMessage& alarm_message, 
                                       const std::string& file_name) {
    AlarmSendResult result;
    
    try {
        if (config_.s3_bucket_name.empty()) {
            result.error_message = "S3 bucket not configured";
            return result;
        }
        
        // S3Client가 초기화되지 않은 경우
        if (!s3_client_) {
            result.error_message = "S3 client not initialized";
            return result;
        }
        
        // JSON 변환
        std::string json_data = alarm_message.to_json_string();
        
        LOG_DEBUG("Uploading alarm to S3: " + alarm_message.nm);
        
        // S3 업로드
        auto s3_result = s3_client_->uploadJson(file_name.empty() ? 
                                               s3_client_->generateTimestampFileName("alarm", "json") : file_name, 
                                               json_data);
        
        result.timestamp = std::chrono::system_clock::now();
        result.s3_success = s3_result.success;
        result.s3_file_path = s3_result.file_url;
        
        if (s3_result.success) {
            result.success = true;
            LOG_DEBUG("S3 upload successful: " + s3_result.file_url);
        } else {
            result.success = false;
            result.s3_error_message = s3_result.error_message;
            result.error_message = result.s3_error_message;
            LOG_ERROR("S3 upload failed: " + result.error_message);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.s3_success = false;
        result.error_message = "S3 upload exception: " + std::string(e.what());
        result.s3_error_message = result.error_message;
        result.timestamp = std::chrono::system_clock::now();
        
        LOG_ERROR("Exception in callS3Alarm: " + std::string(e.what()));
    }
    
    return result;
}

#ifdef HAS_SHARED_LIBS
AlarmSendResult CSPGateway::processAlarmOccurrence(const Database::Entities::AlarmOccurrenceEntity& occurrence) {
    // PulseOne Entity를 AlarmMessage로 변환
    AlarmMessage alarm_message = AlarmMessage::from_alarm_occurrence(occurrence, config_.building_id);
    
    return taskAlarmSingle(alarm_message);
}

std::vector<AlarmSendResult> CSPGateway::processBatchAlarms(
    const std::vector<Database::Entities::AlarmOccurrenceEntity>& occurrences) {
    
    std::vector<AlarmSendResult> results;
    results.reserve(occurrences.size());
    
    LOG_INFO("Processing batch alarms: " + std::to_string(occurrences.size()) + " items");
    
    // 배치 크기에 따라 처리
    size_t batch_size = static_cast<size_t>(config_.batch_size);
    
    for (size_t i = 0; i < occurrences.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, occurrences.size());
        
        // 현재 배치를 병렬 처리
        std::vector<std::future<AlarmSendResult>> futures;
        futures.reserve(end - i);
        
        for (size_t j = i; j < end; ++j) {
            futures.push_back(std::async(std::launch::async, 
                [this, &occurrences, j]() {
                    return processAlarmOccurrence(occurrences[j]);
                }));
        }
        
        // 결과 수집
        for (auto& future : futures) {
            results.push_back(future.get());
        }
        
        // 배치 간 지연 (설정에 따라)
        if (end < occurrences.size() && config_.batch_timeout_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.batch_timeout_ms));
        }
    }
    
    LOG_INFO("Batch processing completed: " + std::to_string(results.size()) + " results");
    return results;
}
#endif

void CSPGateway::updateConfig(const CSPGatewayConfig& new_config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = new_config;
    
    // HTTP 클라이언트 재초기화
    initializeHttpClient();
    
    // S3 클라이언트 재초기화
    initializeS3Client();
    
    LOG_INFO("CSPGateway configuration updated");
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
    
    LOG_INFO("CSPGateway statistics reset");
}

AlarmSendResult CSPGateway::retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count) {
    LOG_DEBUG("Retrying failed alarm (attempt " + std::to_string(attempt_count) + "): " + alarm_message.nm);
    
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
        json alarm_json = alarm_message.to_json();
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
            
            LOG_DEBUG("Failed alarm saved to: " + filename.str());
            return true;
        } else {
            LOG_ERROR("Failed to open file for writing: " + filename.str());
            return false;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception saving failed alarm to file: " + std::string(e.what()));
        return false;
    }
}

size_t CSPGateway::reprocessFailedAlarms() {
    // 구현 간소화 - 실제로는 디렉토리 스캔 후 파일 처리
    LOG_INFO("reprocessFailedAlarms - Not fully implemented");
    return 0;
}

bool CSPGateway::testConnection() {
    LOG_INFO("Testing CSPGateway connections...");
    
    bool api_ok = false;
    bool s3_ok = false;
    
    // API 연결 테스트
    if (!config_.api_endpoint.empty() && http_client_) {
        api_ok = http_client_->testConnection("/health");
        LOG_INFO("API connection test: " + std::string(api_ok ? "OK" : "FAILED"));
    }
    
    // S3 연결 테스트
    if (!config_.s3_bucket_name.empty() && s3_client_) {
        s3_ok = s3_client_->testConnection();
        LOG_INFO("S3 connection test: " + std::string(s3_ok ? "OK" : "FAILED"));
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
    // 테스트용 알람 메시지 생성
    auto test_alarm = AlarmMessage::create_sample(
        config_.building_id, 
        "TEST_POINT", 
        99.9, 
        true
    );
    test_alarm.des = "CSPGateway 연결 테스트 알람";
    
    LOG_INFO("Sending test alarm");
    return taskAlarmSingle(test_alarm);
}

void CSPGateway::initializeHttpClient() {
    try {
        PulseOne::Client::HttpRequestOptions options;
        options.timeout_sec = config_.api_timeout_sec;
        options.user_agent = "PulseOne-CSPGateway/1.0";
        options.bearer_token = config_.api_key;
        
        http_client_ = std::make_unique<PulseOne::Client::HttpClient>(config_.api_endpoint, options);
        
        LOG_DEBUG("HTTP client initialized");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize HTTP client: " + std::string(e.what()));
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
        
        LOG_DEBUG("S3 client initialized");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize S3 client: " + std::string(e.what()));
    }
}

void CSPGateway::workerThread() {
    LOG_INFO("CSPGateway worker thread started");
    
    while (!should_stop_.load()) {
        try {
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
                auto result = taskAlarmSingle(alarm);
                
                // 실패 시 재시도 큐에 추가
                if (!result.success && config_.max_retry_count > 0) {
                    std::lock_guard<std::mutex> retry_lock(queue_mutex_);
                    retry_queue_.emplace(alarm, 1);
                    queue_cv_.notify_one();
                }
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in worker thread: " + std::string(e.what()));
        }
    }
    
    LOG_INFO("CSPGateway worker thread stopped");
}

void CSPGateway::retryThread() {
    LOG_INFO("CSPGateway retry thread started");
    
    while (!should_stop_.load()) {
        try {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // 재시도 큐에 작업이 있을 때까지 대기
            queue_cv_.wait_for(lock, std::chrono::seconds(1), [this] { 
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
                
                // 최대 재시도 횟수 확인
                if (retry_item.second <= config_.max_retry_count) {
                    auto result = retryFailedAlarm(retry_item.first, retry_item.second);
                    
                    // 여전히 실패하면 다시 큐에 추가
                    if (!result.success && retry_item.second < config_.max_retry_count) {
                        std::lock_guard<std::mutex> retry_lock(queue_mutex_);
                        retry_queue_.emplace(retry_item.first, retry_item.second + 1);
                    }
                }
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in retry thread: " + std::string(e.what()));
        }
    }
    
    LOG_INFO("CSPGateway retry thread stopped");
}

std::pair<int, std::string> CSPGateway::executeHttpPost(const std::string& endpoint,
                                                       const std::string& json_data,
                                                       const std::unordered_map<std::string, std::string>& headers) {
    
    if (!http_client_) {
        return {0, "HTTP client not initialized"};
    }
    
    try {
        auto response = http_client_->post(endpoint, json_data, "application/json", headers);
        return {response.status_code, response.body};
        
    } catch (const std::exception& e) {
        return {0, "HTTP exception: " + std::string(e.what())};
    }
}

bool CSPGateway::uploadToS3(const std::string& bucket_name,
                           const std::string& object_key,
                           const std::string& content) {
    
    // unused parameter 경고 방지
    (void)bucket_name;
    
    if (!s3_client_) {
        return false;
    }
    
    try {
        auto result = s3_client_->upload(object_key, content);
        return result.success;
        
    } catch (const std::exception& e) {
        LOG_ERROR("S3 upload exception: " + std::string(e.what()));
        return false;
    }
}

} // namespace CSP
} // namespace PulseOne