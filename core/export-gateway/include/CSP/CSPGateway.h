/**
 * @file CSPGateway.h - 컴파일 에러 완전 수정 버전
 * @brief CSP Gateway 헤더 - C# CSPGateway 완전 포팅 + 동적 타겟 시스템
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 1.1.0 (컴파일 에러 수정)
 * 
 * 주요 수정사항:
 * 1. DynamicTargetManager 메서드명 정정
 * 2. AlarmSendResult 필드 정정
 * 3. DynamicTarget 구조체 필드 정정
 * 4. 중복 정의 제거
 * 5. 누락된 타입 정의 추가
 */

#ifndef CSP_GATEWAY_H
#define CSP_GATEWAY_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <unordered_map>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "AlarmMessage.h"
#include "CSPDynamicTargets.h"

using json = nlohmann::json;

// Forward declarations
namespace PulseOne {
namespace CSP {
    class DynamicTargetManager;
}
namespace Client {
    class HttpClient;
    class S3Client;
}
}

#ifdef HAS_SHARED_LIBS
#include "Database/Entities/AlarmOccurrenceEntity.h"
#endif

namespace PulseOne {
namespace CSP {

// =============================================================================
// 타입 정의들
// =============================================================================

/**
 * @brief 동적 타겟 통계 구조체 (수정됨)
 */
struct DynamicTargetStats {
    std::string name;
    std::string type;
    bool enabled = true;
    bool healthy = false;
    
    // 성능 통계
    uint64_t success_count = 0;
    uint64_t failure_count = 0;
    double success_rate = 0.0;
    double avg_response_time_ms = 0.0;
    
    // 상태 정보
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    std::string last_error_message;
    
    // 실패 방지기 상태
    std::string circuit_breaker_state = "CLOSED";  // CLOSED, OPEN, HALF_OPEN
    int consecutive_failures = 0;
    
    // 설정 정보
    int priority = 100;
    json config;
    
    /**
     * @brief 성공률 계산
     */
    double calculateSuccessRate() const {
        uint64_t total = success_count + failure_count;
        return total > 0 ? (static_cast<double>(success_count) / total * 100.0) : 0.0;
    }
    
    /**
     * @brief 통계를 JSON으로 변환
     */
    json toJson() const;
};

/**
 * @brief CSP Gateway 설정 (수정됨)
 */
struct CSPGatewayConfig {
    // 기본 설정
    std::string building_id = "1001";
    std::string api_endpoint = "";
    std::string api_key = "";
    int api_timeout_sec = 30;
    
    // S3 설정
    std::string s3_endpoint = "";
    std::string s3_access_key = "";
    std::string s3_secret_key = "";
    std::string s3_bucket_name = "";
    std::string s3_region = "us-east-1";
    
    // ✅ 수정: Dynamic Targets 설정 (올바른 필드명)
    std::string dynamic_targets_config_path = "";  // config_file → config_path
    bool use_dynamic_targets = false;
    
    // 기본 옵션
    bool debug_mode = false;
    int max_retry_attempts = 3;
    int initial_delay_ms = 1000;
    int max_queue_size = 10000;
    std::string failed_file_path = "./failed_alarms";
    bool use_api = true;
    bool use_s3 = true;
    
    // 멀티빌딩 지원
    std::vector<int> supported_building_ids = {1001};
    bool multi_building_enabled = false;
    
    // 알람 무시 시간 설정
    int alarm_ignore_minutes = 5;
    bool use_local_time = true;
    
    // 배치 파일 관리
    std::string alarm_dir_path = "./alarm_files";
    bool auto_cleanup_success_files = true;
    int keep_failed_files_days = 7;
    size_t max_batch_size = 1000;
    int batch_timeout_ms = 5000;
};

/**
 * @brief 알람 전송 결과 (수정됨 - 실제 구조체와 일치)
 */
struct AlarmSendResult {
    // ✅ 기본 결과 (실제 필드명과 일치)
    bool success = false;                    // api_success가 아님!
    int status_code = 0;                     // http_status_code가 아님!
    std::string response_body = "";
    std::string error_message = "";
    
    // S3 관련
    bool s3_success = false;
    std::string s3_error_message = "";
    std::string s3_file_path = "";
    
    // 추가 메타데이터
    std::chrono::milliseconds response_time{0};
    int retry_count = 0;
    
    // ❌ alarm_message 필드는 존재하지 않음!
    
    /**
     * @brief 완전한 성공 여부 (API + S3 모두 성공)
     */
    bool isCompleteSuccess() const {
        return success && (s3_file_path.empty() || s3_success);
    }
    
    /**
     * @brief 결과를 JSON으로 변환
     */
    json toJson() const {
        return json{
            {"success", success},
            {"status_code", status_code},
            {"response_body", response_body},
            {"error_message", error_message},
            {"s3_success", s3_success},
            {"s3_error_message", s3_error_message},
            {"s3_file_path", s3_file_path},
            {"response_time_ms", response_time.count()},
            {"retry_count", retry_count}
        };
    }
};

/**
 * @brief 배치 알람 처리 결과
 */
struct BatchAlarmResult {
    int total_alarms = 0;
    int successful_api_calls = 0;
    int failed_api_calls = 0;
    bool s3_success = false;
    std::string batch_file_path;
    std::chrono::system_clock::time_point processed_time;
    
    // C# 스타일 계산 메서드
    int getFailAPIAlarms() const { return failed_api_calls; }
    bool isCompleteSuccess() const { return failed_api_calls == 0 && s3_success; }
};

/**
 * @brief CSP Gateway 통계
 */
struct CSPGatewayStats {
    std::atomic<size_t> total_alarms{0};
    std::atomic<size_t> successful_api_calls{0};
    std::atomic<size_t> failed_api_calls{0};
    std::atomic<size_t> successful_s3_uploads{0};
    std::atomic<size_t> failed_s3_uploads{0};
    std::atomic<size_t> retry_attempts{0};
    
    // 배치 처리 통계
    std::atomic<size_t> total_batches{0};
    std::atomic<size_t> successful_batches{0};
    std::atomic<size_t> ignored_alarms{0};
    
    // 동적 타겟 통계 추가
    bool dynamic_targets_enabled = false;
    size_t total_dynamic_targets = 0;
    size_t active_dynamic_targets = 0;
    size_t healthy_dynamic_targets = 0;
    double dynamic_target_success_rate = 0.0;
    
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    double avg_response_time_ms = 0.0;
    
    // 기본 생성자
    CSPGatewayStats() {
        last_success_time = std::chrono::system_clock::now();
        last_failure_time = last_success_time;
    }
    
    // 복사 생성자 (atomic 때문에 필요)
    CSPGatewayStats(const CSPGatewayStats& other)
        : total_alarms(other.total_alarms.load())
        , successful_api_calls(other.successful_api_calls.load())
        , failed_api_calls(other.failed_api_calls.load())
        , successful_s3_uploads(other.successful_s3_uploads.load())
        , failed_s3_uploads(other.failed_s3_uploads.load())
        , retry_attempts(other.retry_attempts.load())
        , total_batches(other.total_batches.load())
        , successful_batches(other.successful_batches.load())
        , ignored_alarms(other.ignored_alarms.load())
        , dynamic_targets_enabled(other.dynamic_targets_enabled)
        , total_dynamic_targets(other.total_dynamic_targets)
        , active_dynamic_targets(other.active_dynamic_targets)
        , healthy_dynamic_targets(other.healthy_dynamic_targets)
        , dynamic_target_success_rate(other.dynamic_target_success_rate)
        , last_success_time(other.last_success_time)
        , last_failure_time(other.last_failure_time)
        , avg_response_time_ms(other.avg_response_time_ms) {}
    
    // 할당 연산자
    CSPGatewayStats& operator=(const CSPGatewayStats& other) {
        if (this != &other) {
            total_alarms.store(other.total_alarms.load());
            successful_api_calls.store(other.successful_api_calls.load());
            failed_api_calls.store(other.failed_api_calls.load());
            successful_s3_uploads.store(other.successful_s3_uploads.load());
            failed_s3_uploads.store(other.failed_s3_uploads.load());
            retry_attempts.store(other.retry_attempts.load());
            total_batches.store(other.total_batches.load());
            successful_batches.store(other.successful_batches.load());
            ignored_alarms.store(other.ignored_alarms.load());
            dynamic_targets_enabled = other.dynamic_targets_enabled;
            total_dynamic_targets = other.total_dynamic_targets;
            active_dynamic_targets = other.active_dynamic_targets;
            healthy_dynamic_targets = other.healthy_dynamic_targets;
            dynamic_target_success_rate = other.dynamic_target_success_rate;
            last_success_time = other.last_success_time;
            last_failure_time = other.last_failure_time;
            avg_response_time_ms = other.avg_response_time_ms;
        }
        return *this;
    }
    
    // API 성공률 계산
    double getAPISuccessRate() const {
        size_t total = successful_api_calls.load() + failed_api_calls.load();
        return total > 0 ? (static_cast<double>(successful_api_calls.load()) / total * 100.0) : 0.0;
    }
    
    // S3 성공률 계산
    double getS3SuccessRate() const {
        size_t total = successful_s3_uploads.load() + failed_s3_uploads.load();
        return total > 0 ? (static_cast<double>(successful_s3_uploads.load()) / total * 100.0) : 0.0;
    }
};

// 멀티빌딩 알람 타입 정의
using MultiBuildingAlarms = std::unordered_map<int, std::vector<AlarmMessage>>;

// =============================================================================
// CSP Gateway 메인 클래스
// =============================================================================

/**
 * @brief CSP Gateway 메인 클래스 (컴파일 에러 완전 수정)
 */
class CSPGateway {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    explicit CSPGateway(const CSPGatewayConfig& config);
    ~CSPGateway();
    
    // 복사/이동 생성자 비활성화
    CSPGateway(const CSPGateway&) = delete;
    CSPGateway& operator=(const CSPGateway&) = delete;
    CSPGateway(CSPGateway&&) = delete;
    CSPGateway& operator=(CSPGateway&&) = delete;
    
    // =======================================================================
    // 기본 C# CSPGateway 메서드들
    // =======================================================================
    
    AlarmSendResult taskAlarmSingle(const AlarmMessage& alarm_message);
    AlarmSendResult callAPIAlarm(const AlarmMessage& alarm_message);
    AlarmSendResult callS3Alarm(const AlarmMessage& alarm_message, const std::string& file_name = "");
    
#ifdef HAS_SHARED_LIBS
    AlarmSendResult processAlarmOccurrence(const Database::Entities::AlarmOccurrenceEntity& occurrence);
    std::vector<AlarmSendResult> processBatchAlarms(
        const std::vector<Database::Entities::AlarmOccurrenceEntity>& occurrences);
#endif
    
    // =======================================================================
    // 멀티빌딩 지원
    // =======================================================================
    
    std::unordered_map<int, BatchAlarmResult> processMultiBuildingAlarms(
        const MultiBuildingAlarms& building_alarms);
    MultiBuildingAlarms groupAlarmsByBuilding(const std::vector<AlarmMessage>& alarms);
    void setSupportedBuildingIds(const std::vector<int>& building_ids);
    void setMultiBuildingEnabled(bool enabled);
    
    // =======================================================================
    // 알람 무시 시간 필터링
    // =======================================================================
    
    std::vector<AlarmMessage> filterIgnoredAlarms(const std::vector<AlarmMessage>& alarms);
    bool shouldIgnoreAlarm(const std::string& alarm_time) const;
    void setAlarmIgnoreMinutes(int minutes);
    void setUseLocalTime(bool use_local);
    
    // =======================================================================
    // 배치 파일 관리 및 자동 정리
    // =======================================================================
    
    std::string saveBatchAlarmFile(int building_id, const std::vector<AlarmMessage>& alarms);
    bool cleanupSuccessfulBatchFile(const std::string& file_path);
    size_t cleanupOldFailedFiles(int days_to_keep = -1);
    void setAlarmDirectoryPath(const std::string& dir_path);
    void setAutoCleanupEnabled(bool enabled);
    
    // =======================================================================
    // 서비스 제어 메서드들
    // =======================================================================
    
    bool start();
    void stop();
    bool isRunning() const { return is_running_.load(); }
    
    // =======================================================================
    // 설정 및 상태 관리
    // =======================================================================
    
    void updateConfig(const CSPGatewayConfig& new_config);
    const CSPGatewayConfig& getConfig() const { return config_; }
    CSPGatewayStats getStats() const;  // ✅ 인라인 제거 - cpp에서 구현
    void resetStats();
    
    // =======================================================================
    // 테스트 및 진단 메서드들
    // =======================================================================
    
    bool testConnection();
    bool testS3Connection();
    AlarmSendResult sendTestAlarm();
    std::unordered_map<int, BatchAlarmResult> testMultiBuildingAlarms();
    
    // =======================================================================
    // Dynamic Target 관리 메서드들
    // =======================================================================
    
    bool addDynamicTarget(const std::string& name, 
                         const std::string& type,
                         const json& config, 
                         bool enabled = true, 
                         int priority = 100);
    
    bool removeDynamicTarget(const std::string& name);
    bool enableDynamicTarget(const std::string& name, bool enabled);
    std::vector<std::string> getSupportedTargetTypes() const;
    bool reloadDynamicTargets();
    std::vector<DynamicTargetStats> getDynamicTargetStats() const;

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    CSPGatewayConfig config_;
    mutable std::mutex config_mutex_;
    
    // 통계
    mutable CSPGatewayStats stats_;
    mutable std::mutex stats_mutex_;
    
    // 스레드 관리
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    std::unique_ptr<std::thread> worker_thread_;
    std::unique_ptr<std::thread> retry_thread_;
    
    // 큐 관리
    std::queue<AlarmMessage> alarm_queue_;
    std::queue<std::pair<AlarmMessage, int>> retry_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 클라이언트들
    std::unique_ptr<PulseOne::Client::HttpClient> http_client_;
    std::unique_ptr<PulseOne::Client::S3Client> s3_client_;
    
    // Dynamic Target System
    std::unique_ptr<DynamicTargetManager> dynamic_target_manager_;
    std::atomic<bool> use_dynamic_targets_{false};
    
    // =======================================================================
    // Private 메서드들
    // =======================================================================
    
    // 초기화 메서드들
    void initializeHttpClient();
    void initializeS3Client();
    void initializeDynamicTargetSystem();
    
    // 스레드 관련 메서드들
    void workerThread();
    void retryThread();
    
    // 알람 처리 메서드들
    AlarmSendResult taskAlarmSingleDynamic(const AlarmMessage& alarm_message);
    AlarmSendResult taskAlarmSingleLegacy(const AlarmMessage& alarm_message);
    AlarmSendResult retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count);
    bool saveFailedAlarmToFile(const AlarmMessage& alarm_message, const std::string& error_message);
    size_t reprocessFailedAlarms();
    
    // 통계 업데이트 메서드들
    void updateStatsFromDynamicResults(const std::vector<TargetSendResult>& target_results,
                                     double response_time_ms);
    void updateBatchStats(const BatchAlarmResult& result);
    
    // HTTP/S3 헬퍼 메서드들
    AlarmSendResult sendHttpPostRequest(const std::string& endpoint,
                                       const std::string& json_data,
                                       const std::string& content_type,
                                       const std::unordered_map<std::string, std::string>& headers);
    bool uploadToS3(const std::string& object_key, const std::string& content);
    
    // 테스트 메서드들
    bool testConnectionLegacy();
    
    // 유틸리티 메서드들
    std::string generateBatchFileName() const;
    std::string createBatchDirectory(int building_id) const;
    std::chrono::system_clock::time_point parseCSTimeString(const std::string& time_str) const;
    void loadConfigFromConfigManager();
};

} // namespace CSP
} // namespace PulseOne

#endif // CSP_GATEWAY_H