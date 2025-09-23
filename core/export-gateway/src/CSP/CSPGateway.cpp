/**
 * @file CSPGateway.cpp - 완전한 구현 파일 (멀티빌딩, 시간필터, 배치관리 추가)
 * @brief CSP Gateway 메인 구현 - C# CSPGateway 완전 포팅
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/CSPGateway.cpp
 */

#include "CSP/CSPGateway.h"
#include "Client/HttpClient.h"
#include "Client/S3Client.h"
#include "Utils/RetryManager.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <future>
#include <regex>
#include <iomanip>

namespace PulseOne {
namespace CSP {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

CSPGateway::CSPGateway(const CSPGatewayConfig& config) : config_(config) {
    LogManager::getInstance().Info("CSPGateway initializing...");
    
    // ConfigManager에서 암호화된 설정 로드
    loadConfigFromConfigManager();
    
    // HTTP 클라이언트 초기화
    initializeHttpClient();
    
    // S3 클라이언트 초기화
    initializeS3Client();
    
    // 알람 디렉토리 생성
    try {
        std::filesystem::create_directories(config_.alarm_dir_path);
        std::filesystem::create_directories(config_.failed_file_path);
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("Failed to create directories: " + std::string(e.what()));
    }
    
    LogManager::getInstance().Info("CSPGateway initialized successfully");
}

CSPGateway::~CSPGateway() {
    stop();
}

// =============================================================================
// 기본 C# CSPGateway 메서드들 (기존 유지)
// =============================================================================

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
        
        // HTTP 요청 전송
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
            
            // 평균 응답시간 업데이트
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
        
        // S3 업로드
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

#ifdef HAS_SHARED_LIBS
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
#endif

// =============================================================================
// 1. 멀티빌딩 지원 구현 (C# Dictionary<int, List<AlarmMessage>> 포팅)
// =============================================================================

std::unordered_map<int, BatchAlarmResult> CSPGateway::processMultiBuildingAlarms(
    const MultiBuildingAlarms& building_alarms) {
    
    LogManager::getInstance().Info("Processing multi-building alarms for " + 
        std::to_string(building_alarms.size()) + " buildings");
    
    std::unordered_map<int, BatchAlarmResult> results;
    
    for (const auto& [building_id, alarms] : building_alarms) {
        BatchAlarmResult batch_result;
        batch_result.total_alarms = static_cast<int>(alarms.size());
        batch_result.processed_time = std::chrono::system_clock::now();
        
        // 알람 무시 시간 필터링 적용
        auto filtered_alarms = filterIgnoredAlarms(alarms);
        
        // 무시된 알람 수 기록
        size_t ignored_count = alarms.size() - filtered_alarms.size();
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.ignored_alarms += ignored_count;
        }
        
        if (filtered_alarms.empty()) {
            LogManager::getInstance().Debug("All alarms ignored for building " + std::to_string(building_id));
            batch_result.batch_file_path = "";
            results[building_id] = batch_result;
            continue;
        }
        
        // 배치 파일 저장 (C# 스타일)
        batch_result.batch_file_path = saveBatchAlarmFile(building_id, filtered_alarms);
        
        // API 호출 처리
        int successful_calls = 0;
        int failed_calls = 0;
        
        for (const auto& alarm : filtered_alarms) {
            auto result = callAPIAlarm(alarm);
            if (result.success) {
                successful_calls++;
            } else {
                failed_calls++;
            }
        }
        
        batch_result.successful_api_calls = successful_calls;
        batch_result.failed_api_calls = failed_calls;
        
        // S3 업로드 (전체 배치 파일)
        if (!batch_result.batch_file_path.empty()) {
            // 배치 파일을 S3에 업로드
            try {
                std::ifstream file(batch_result.batch_file_path);
                std::string file_content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                
                std::string s3_key = "batch_alarms/" + std::to_string(building_id) + "/" + 
                                   std::filesystem::path(batch_result.batch_file_path).filename().string();
                
                batch_result.s3_success = uploadToS3(s3_key, file_content);
                
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("Failed to upload batch file to S3: " + std::string(e.what()));
                batch_result.s3_success = false;
            }
        }
        
        // C# 로직: 성공 시 파일 삭제
        if (batch_result.isCompleteSuccess() && config_.auto_cleanup_success_files) {
            cleanupSuccessfulBatchFile(batch_result.batch_file_path);
            LogManager::getInstance().Info("Success AlarmMessage Upload[Building(" + 
                std::to_string(building_id) + ")] : " + std::to_string(filtered_alarms.size()) + " Alarms");
        } else {
            LogManager::getInstance().Warn("Failure AlarmMessage Upload[Building(" + 
                std::to_string(building_id) + ")] : [" + std::to_string(failed_calls) + 
                "] API Failure, [" + (batch_result.s3_success ? "true" : "false") + "] S3 result");
        }
        
        results[building_id] = batch_result;
        updateBatchStats(batch_result);
    }
    
    return results;
}

MultiBuildingAlarms CSPGateway::groupAlarmsByBuilding(const std::vector<AlarmMessage>& alarms) {
    MultiBuildingAlarms grouped_alarms;
    
    for (const auto& alarm : alarms) {
        // 지원하는 빌딩 ID 확인
        if (config_.multi_building_enabled) {
            auto& supported_ids = config_.supported_building_ids;
            if (std::find(supported_ids.begin(), supported_ids.end(), alarm.bd) == supported_ids.end()) {
                LogManager::getInstance().Debug("Skipping alarm for unsupported building: " + std::to_string(alarm.bd));
                continue;
            }
        }
        
        grouped_alarms[alarm.bd].push_back(alarm);
    }
    
    return grouped_alarms;
}

void CSPGateway::setSupportedBuildingIds(const std::vector<int>& building_ids) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.supported_building_ids = building_ids;
    LogManager::getInstance().Info("Updated supported building IDs: " + std::to_string(building_ids.size()) + " buildings");
}

void CSPGateway::setMultiBuildingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.multi_building_enabled = enabled;
    LogManager::getInstance().Info("Multi-building mode: " + std::string(enabled ? "enabled" : "disabled"));
}

// =============================================================================
// 2. 알람 무시 시간 필터링 구현 (C# AlarmIgnoreMinutes 포팅)
// =============================================================================

std::vector<AlarmMessage> CSPGateway::filterIgnoredAlarms(const std::vector<AlarmMessage>& alarms) {
    if (config_.alarm_ignore_minutes <= 0) {
        return alarms; // 필터링 비활성화
    }
    
    std::vector<AlarmMessage> filtered_alarms;
    size_t ignored_count = 0;
    
    for (const auto& alarm : alarms) {
        if (!shouldIgnoreAlarm(alarm.tm)) {
            filtered_alarms.push_back(alarm);
        } else {
            ignored_count++;
            LogManager::getInstance().Debug("Ignoring alarm due to time filter: " + alarm.nm + " at " + alarm.tm);
        }
    }
    
    LogManager::getInstance().Debug("Filtered " + std::to_string(ignored_count) + 
        " alarms out of " + std::to_string(alarms.size()));
    
    return filtered_alarms;
}

bool CSPGateway::shouldIgnoreAlarm(const std::string& alarm_time) const {
    try {
        // C# DateTime 문자열을 파싱
        auto alarm_tp = parseCSTimeString(alarm_time);
        
        // 현재 시간에서 무시 시간을 뺀 시점 계산 (C# ignoreTime 로직)
        auto now = std::chrono::system_clock::now();
        auto ignore_duration = std::chrono::minutes(config_.alarm_ignore_minutes);
        auto ignore_time = now - ignore_duration;
        
        // C# 로직: if (onlineAlarm.ReceivedTime < ignoreTime) continue;
        return alarm_tp < ignore_time;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Debug("Failed to parse alarm time, not ignoring: " + std::string(e.what()));
        return false; // 파싱 실패 시 무시하지 않음
    }
}

void CSPGateway::setAlarmIgnoreMinutes(int minutes) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.alarm_ignore_minutes = minutes;
    LogManager::getInstance().Info("Alarm ignore minutes set to: " + std::to_string(minutes));
}

void CSPGateway::setUseLocalTime(bool use_local) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.use_local_time = use_local;
    LogManager::getInstance().Info("Use local time: " + std::string(use_local ? "enabled" : "disabled"));
}

// =============================================================================
// 3. 배치 파일 관리 및 자동 정리 구현 (C# 파일 관리 로직 포팅)
// =============================================================================

std::string CSPGateway::saveBatchAlarmFile(int building_id, const std::vector<AlarmMessage>& alarms) {
    try {
        // C# 스타일 디렉토리 구조 생성
        std::string batch_dir = createBatchDirectory(building_id);
        std::filesystem::create_directories(batch_dir);
        
        // C# 스타일 파일명 생성
        std::string filename = generateBatchFileName();
        std::string full_path = batch_dir + "/" + filename;
        
        // JSON 배열로 저장 (C# Json.SaveFile 로직)
        nlohmann::json alarm_array = nlohmann::json::array();
        for (const auto& alarm : alarms) {
            alarm_array.push_back(alarm.to_json());
        }
        
        std::ofstream file(full_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + full_path);
        }
        
        file << alarm_array.dump(2);
        file.close();
        
        LogManager::getInstance().Debug("Batch alarm file saved: " + full_path + 
            " (" + std::to_string(alarms.size()) + " alarms)");
        
        return full_path;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to save batch alarm file: " + std::string(e.what()));
        return "";
    }
}

bool CSPGateway::cleanupSuccessfulBatchFile(const std::string& file_path) {
    try {
        if (file_path.empty() || !std::filesystem::exists(file_path)) {
            return false;
        }
        
        // C# File.Delete 로직
        std::filesystem::remove(file_path);
        LogManager::getInstance().Debug("Cleaned up successful batch file: " + file_path);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to cleanup batch file: " + std::string(e.what()));
        return false;
    }
}

size_t CSPGateway::cleanupOldFailedFiles(int days_to_keep) {
    if (days_to_keep < 0) {
        days_to_keep = config_.keep_failed_files_days;
    }
    
    size_t deleted_count = 0;
    
    try {
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days_to_keep);
        
        // 실패 파일 디렉토리와 알람 디렉토리 모두 정리
        std::vector<std::string> dirs_to_clean = {config_.failed_file_path, config_.alarm_dir_path};
        
        for (const auto& dir : dirs_to_clean) {
            if (!std::filesystem::exists(dir)) continue;
            
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    auto file_time = std::filesystem::last_write_time(entry);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                    
                    if (sctp < cutoff_time) {
                        std::filesystem::remove(entry);
                        deleted_count++;
                        LogManager::getInstance().Debug("Cleaned up old file: " + entry.path().string());
                    }
                }
            }
        }
        
        LogManager::getInstance().Info("Cleaned up " + std::to_string(deleted_count) + " old files");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to cleanup old files: " + std::string(e.what()));
    }
    
    return deleted_count;
}

void CSPGateway::setAlarmDirectoryPath(const std::string& dir_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.alarm_dir_path = dir_path;
    
    try {
        std::filesystem::create_directories(dir_path);
        LogManager::getInstance().Info("Alarm directory path set to: " + dir_path);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to create alarm directory: " + std::string(e.what()));
    }
}

void CSPGateway::setAutoCleanupEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.auto_cleanup_success_files = enabled;
    LogManager::getInstance().Info("Auto cleanup: " + std::string(enabled ? "enabled" : "disabled"));
}

// =============================================================================
// 기존 메서드들 (설정 및 상태 관리)
// =============================================================================

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
    stats_.total_batches.store(0);
    stats_.successful_batches.store(0);
    stats_.ignored_alarms.store(0);
    stats_.avg_response_time_ms = 0.0;
    
    LogManager::getInstance().Info("CSPGateway statistics reset");
}

// =============================================================================
// 테스트 메서드들
// =============================================================================

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
    
    return api_ok || s3_ok;
}

bool CSPGateway::testS3Connection() {
    if (!s3_client_) {
        return false;
    }
    return s3_client_->testConnection();
}

AlarmSendResult CSPGateway::sendTestAlarm() {
    int building_id_int = 1001;
    try {
        building_id_int = std::stoi(config_.building_id);
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("Failed to convert building_id to int, using default 1001: " + std::string(e.what()));
    }
    
    auto test_alarm = AlarmMessage::create_sample(building_id_int, "TEST_POINT", 99.9, true);
    test_alarm.des = "CSPGateway 연결 테스트 알람";
    
    LogManager::getInstance().Info("Sending test alarm");
    return taskAlarmSingle(test_alarm);
}

std::unordered_map<int, BatchAlarmResult> CSPGateway::testMultiBuildingAlarms() {
    LogManager::getInstance().Info("Testing multi-building alarm processing");
    
    // 테스트 알람 생성
    std::vector<AlarmMessage> test_alarms;
    for (int building_id : config_.supported_building_ids) {
        auto alarm = AlarmMessage::create_sample(building_id, "TEST_MULTI_" + std::to_string(building_id), 50.0, true);
        alarm.des = "Multi-building test alarm for building " + std::to_string(building_id);
        test_alarms.push_back(alarm);
    }
    
    // 빌딩별로 그룹화
    auto grouped_alarms = groupAlarmsByBuilding(test_alarms);
    
    // 처리 실행
    return processMultiBuildingAlarms(grouped_alarms);
}

// =============================================================================
// Private 헬퍼 메서드들
// =============================================================================

std::string CSPGateway::generateBatchFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d%H%M%S") << ".json";
    return ss.str();
}

std::string CSPGateway::createBatchDirectory(int building_id) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << config_.alarm_dir_path << "/" << building_id << "/"
       << std::put_time(std::localtime(&time_t), "%Y%m%d");
    return ss.str();
}

std::chrono::system_clock::time_point CSPGateway::parseCSTimeString(const std::string& time_str) const {
    // C# DateTime 형식 파싱: "yyyy-MM-dd HH:mm:ss.fff"
    std::regex time_regex(R"((\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})\.(\d{3}))");
    std::smatch matches;
    
    if (!std::regex_match(time_str, matches, time_regex)) {
        throw std::invalid_argument("Invalid time format: " + time_str);
    }
    
    std::tm tm = {};
    tm.tm_year = std::stoi(matches[1]) - 1900;
    tm.tm_mon = std::stoi(matches[2]) - 1;
    tm.tm_mday = std::stoi(matches[3]);
    tm.tm_hour = std::stoi(matches[4]);
    tm.tm_min = std::stoi(matches[5]);
    tm.tm_sec = std::stoi(matches[6]);
    
    auto time_t = std::mktime(&tm);
    auto tp = std::chrono::system_clock::from_time_t(time_t);
    
    // 밀리초 추가
    int milliseconds = std::stoi(matches[7]);
    tp += std::chrono::milliseconds(milliseconds);
    
    return tp;
}

void CSPGateway::updateBatchStats(const BatchAlarmResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_batches++;
    if (result.isCompleteSuccess()) {
        stats_.successful_batches++;
    }
}

void CSPGateway::loadConfigFromConfigManager() {
    ConfigManager& config_mgr = ConfigManager::getInstance();
    
    try {
        // API 키 로드
        if (config_.api_key.empty()) {
            std::string api_key_file = config_mgr.getOrDefault("CSP_API_KEY_FILE", 
                config_mgr.getSecretsDirectory() + "/csp_api.key");
            if (!api_key_file.empty()) {
                config_.api_key = config_mgr.loadPasswordFromFile("CSP_API_KEY_FILE");
            }
        }
        
        // S3 키들 로드
        if (config_.s3_access_key.empty()) {
            config_.s3_access_key = config_mgr.loadPasswordFromFile("CSP_S3_ACCESS_KEY_FILE");
        }
        if (config_.s3_secret_key.empty()) {
            config_.s3_secret_key = config_mgr.loadPasswordFromFile("CSP_S3_SECRET_KEY_FILE");
        }
        
        // 다른 설정들 로드
        if (config_.building_id.empty()) {
            config_.building_id = config_mgr.getOrDefault("CSP_BUILDING_ID", "1001");
        }
        if (config_.api_endpoint.empty()) {
            config_.api_endpoint = config_mgr.getOrDefault("CSP_API_ENDPOINT", "");
        }
        if (config_.s3_bucket_name.empty()) {
            config_.s3_bucket_name = config_mgr.getOrDefault("CSP_S3_BUCKET_NAME", "");
        }
        
        LogManager::getInstance().Debug("CSPGateway configuration loaded from ConfigManager");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("Failed to load config from ConfigManager: " + std::string(e.what()));
    }
}

// =============================================================================
// 기존 Private 메서드들 (워커 스레드, HTTP/S3 등)
// =============================================================================

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
        
        queue_cv_.wait(lock, [this] { 
            return !alarm_queue_.empty() || should_stop_.load(); 
        });
        
        if (should_stop_.load()) {
            break;
        }
        
        if (!alarm_queue_.empty()) {
            AlarmMessage alarm = alarm_queue_.front();
            alarm_queue_.pop();
            lock.unlock();
            
            taskAlarmSingle(alarm);
        }
    }
    
    LogManager::getInstance().Info("CSPGateway worker thread stopped");
}

void CSPGateway::retryThread() {
    LogManager::getInstance().Info("CSPGateway retry thread started");
    
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait_for(lock, std::chrono::seconds(30), [this] { 
            return !retry_queue_.empty() || should_stop_.load(); 
        });
        
        if (should_stop_.load()) {
            break;
        }
        
        if (!retry_queue_.empty()) {
            auto retry_item = retry_queue_.front();
            retry_queue_.pop();
            lock.unlock();
            
            retryFailedAlarm(retry_item.first, retry_item.second);
        }
    }
    
    LogManager::getInstance().Info("CSPGateway retry thread stopped");
}

AlarmSendResult CSPGateway::retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count) {
    LogManager::getInstance().Debug("Retrying failed alarm (attempt " + std::to_string(attempt_count) + "): " + alarm_message.nm);
    
    int delay_ms = config_.initial_delay_ms * (1 << (attempt_count - 1));
    delay_ms = std::min(delay_ms, 60000);
    
    if (delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.retry_attempts++;
    }
    
    return taskAlarmSingle(alarm_message);
}

bool CSPGateway::saveFailedAlarmToFile(const AlarmMessage& alarm_message, const std::string& error_message) {
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream filename;
        filename << config_.failed_file_path << "/failed_alarm_"
                 << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S")
                 << "_" << alarm_message.bd << "_" << alarm_message.nm << ".json";
        
        auto alarm_json = alarm_message.to_json();
        alarm_json["error_message"] = error_message;
        alarm_json["failed_timestamp"] = AlarmMessage::current_time_to_csharp_format(true);
        alarm_json["gateway_info"] = {
            {"building_id", config_.building_id},
            {"api_endpoint", config_.api_endpoint},
            {"s3_bucket", config_.s3_bucket_name}
        };
        
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

bool CSPGateway::uploadToS3(const std::string& object_key, const std::string& content) {
    if (!s3_client_) {
        LogManager::getInstance().Error("S3 client not initialized");
        return false;
    }
    
    try {
        auto result = s3_client_->upload(object_key, content, "application/json; charset=utf-8");
        return result.success;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("S3 upload exception: " + std::string(e.what()));
        return false;
    }
}

} // namespace CSP
} // namespace PulseOne