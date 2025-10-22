/**
 * @file CSPGateway.h - 실제 스키마 기반 완전 수정
 * @brief CSP Gateway 헤더 - export_logs 통계 집계
 * @author PulseOne Development Team
 * @date 2025-10-21
 * @version 1.3.0 (export_logs 기반 통계)
 * 
 * 현재 상황:
 * - export_targets: 구버전 스키마 (통계 필드 포함) - 아직 운영 중
 * - export_logs: 확장됨 - 전송 로그 저장
 * - VIEW: v_export_targets_stats_24h, v_export_targets_stats_all
 * 
 * 해결:
 * - TargetStatistics: export_logs 집계 결과 담는 구조체
 * - getTargetStatisticsFromLogs(): export_logs 쿼리 또는 VIEW 사용
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
#include <optional>
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
 * @brief export_logs 테이블 집계 결과 구조체
 * 
 * 이 구조체는 export_logs 테이블을 집계하거나
 * VIEW (v_export_targets_stats_24h, v_export_targets_stats_all)를 
 * 조회한 결과를 담습니다.
 */
struct TargetStatistics {
    int target_id = 0;
    std::string target_name;
    std::string target_type;
    
    // export_logs 집계 결과
    uint64_t total_count = 0;          // COUNT(*)
    uint64_t success_count = 0;        // SUM(CASE WHEN status='success')
    uint64_t failure_count = 0;        // SUM(CASE WHEN status='failure')
    double avg_response_time_ms = 0.0; // AVG(processing_time_ms)
    
    // 시간 정보
    std::chrono::system_clock::time_point last_export_at;
    std::chrono::system_clock::time_point last_success_at;  // MAX(timestamp WHERE status='success')
    std::chrono::system_clock::time_point last_failure_at;  // MAX(timestamp WHERE status='failure')
    std::string last_error_message;  // 최근 에러 메시지
    
    // 성공률 계산
    double getSuccessRate() const {
        return (total_count > 0) ? (static_cast<double>(success_count) / total_count * 100.0) : 0.0;
    }
    
    json toJson() const {
        return json{
            {"target_id", target_id},
            {"target_name", target_name},
            {"target_type", target_type},
            {"total_count", total_count},
            {"success_count", success_count},
            {"failure_count", failure_count},
            {"success_rate", getSuccessRate()},
            {"avg_response_time_ms", avg_response_time_ms},
            {"last_error_message", last_error_message}
        };
    }
};

struct DynamicTargetStats {
    std::string name;
    std::string type;
    bool enabled = true;
    bool healthy = false;
    
    uint64_t success_count = 0;
    uint64_t failure_count = 0;
    double success_rate = 0.0;
    double avg_response_time_ms = 0.0;
    
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    std::string last_error_message;
    
    std::string circuit_breaker_state = "CLOSED";
    int consecutive_failures = 0;
    int priority = 100;
    json config;
    
    double calculateSuccessRate() const {
        uint64_t total = success_count + failure_count;
        return total > 0 ? (static_cast<double>(success_count) / total * 100.0) : 0.0;
    }
    
    json toJson() const;
};

struct CSPGatewayConfig {
    std::string building_id = "1001";
    std::string api_endpoint = "";
    std::string api_key = "";
    int api_timeout_sec = 30;
    
    std::string s3_endpoint = "";
    std::string s3_access_key = "";
    std::string s3_secret_key = "";
    std::string s3_bucket_name = "";
    std::string s3_region = "us-east-1";
    
    std::string dynamic_targets_config_path = "";
    bool use_dynamic_targets = false;
    
    bool debug_mode = false;
    int max_retry_attempts = 3;
    int initial_delay_ms = 1000;
    int max_queue_size = 10000;
    std::string failed_file_path = "./failed_alarms";
    bool use_api = true;
    bool use_s3 = true;
    
    std::vector<int> supported_building_ids = {1001};
    bool multi_building_enabled = false;
    
    int alarm_ignore_minutes = 5;
    bool use_local_time = true;
    
    std::string alarm_dir_path = "./alarm_files";
    bool auto_cleanup_success_files = true;
    int keep_failed_files_days = 7;
    size_t max_batch_size = 1000;
    int batch_timeout_ms = 5000;
};

struct AlarmSendResult {
    bool success = false;
    int status_code = 0;
    std::string response_body = "";
    std::string error_message = "";
    
    bool s3_success = false;
    std::string s3_error_message = "";
    std::string s3_file_path = "";
    
    std::chrono::milliseconds response_time{0};
    int retry_count = 0;
    
    bool isCompleteSuccess() const {
        return success && (s3_file_path.empty() || s3_success);
    }
    
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

struct BatchAlarmResult {
    int total_alarms = 0;
    int successful_api_calls = 0;
    int failed_api_calls = 0;
    bool s3_success = false;
    std::string batch_file_path;
    std::chrono::system_clock::time_point processed_time;
    
    int getFailAPIAlarms() const { return failed_api_calls; }
    bool isCompleteSuccess() const { return failed_api_calls == 0 && s3_success; }
};

struct CSPGatewayStats {
    std::atomic<size_t> total_alarms{0};
    std::atomic<size_t> successful_api_calls{0};
    std::atomic<size_t> failed_api_calls{0};
    std::atomic<size_t> successful_s3_uploads{0};
    std::atomic<size_t> failed_s3_uploads{0};
    std::atomic<size_t> retry_attempts{0};
    std::atomic<size_t> total_batches{0};
    std::atomic<size_t> successful_batches{0};
    std::atomic<size_t> ignored_alarms{0};
    
    bool dynamic_targets_enabled = false;
    size_t total_dynamic_targets = 0;
    size_t active_dynamic_targets = 0;
    size_t healthy_dynamic_targets = 0;
    double dynamic_target_success_rate = 0.0;
    
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    double avg_response_time_ms = 0.0;
    
    CSPGatewayStats() {
        last_success_time = std::chrono::system_clock::now();
        last_failure_time = last_success_time;
    }
    
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
    
    double getAPISuccessRate() const {
        size_t total = successful_api_calls.load() + failed_api_calls.load();
        return total > 0 ? (static_cast<double>(successful_api_calls.load()) / total * 100.0) : 0.0;
    }
    
    double getS3SuccessRate() const {
        size_t total = successful_s3_uploads.load() + failed_s3_uploads.load();
        return total > 0 ? (static_cast<double>(successful_s3_uploads.load()) / total * 100.0) : 0.0;
    }
};

using MultiBuildingAlarms = std::unordered_map<int, std::vector<AlarmMessage>>;

// =============================================================================
// CSP Gateway 메인 클래스
// =============================================================================

class CSPGateway {
public:
    explicit CSPGateway(const CSPGatewayConfig& config);
    ~CSPGateway();
    
    CSPGateway(const CSPGateway&) = delete;
    CSPGateway& operator=(const CSPGateway&) = delete;
    CSPGateway(CSPGateway&&) = delete;
    CSPGateway& operator=(CSPGateway&&) = delete;
    
    // 기본 알람 처리
    AlarmSendResult taskAlarmSingle(const AlarmMessage& alarm_message);
    AlarmSendResult callAPIAlarm(const AlarmMessage& alarm_message);
    AlarmSendResult callS3Alarm(const AlarmMessage& alarm_message, const std::string& file_name = "");
    
#ifdef HAS_SHARED_LIBS
    AlarmSendResult processAlarmOccurrence(const Database::Entities::AlarmOccurrenceEntity& occurrence);
    std::vector<AlarmSendResult> processBatchAlarms(
        const std::vector<Database::Entities::AlarmOccurrenceEntity>& occurrences);
#endif
    
    // 멀티빌딩 지원
    std::unordered_map<int, BatchAlarmResult> processMultiBuildingAlarms(
        const MultiBuildingAlarms& building_alarms);
    MultiBuildingAlarms groupAlarmsByBuilding(const std::vector<AlarmMessage>& alarms);
    void setSupportedBuildingIds(const std::vector<int>& building_ids);
    void setMultiBuildingEnabled(bool enabled);
    
    // 알람 필터링
    std::vector<AlarmMessage> filterIgnoredAlarms(const std::vector<AlarmMessage>& alarms);
    bool shouldIgnoreAlarm(const std::string& alarm_time) const;
    void setAlarmIgnoreMinutes(int minutes);
    void setUseLocalTime(bool use_local);
    
    // 배치 파일 관리
    std::string saveBatchAlarmFile(int building_id, const std::vector<AlarmMessage>& alarms);
    bool cleanupSuccessfulBatchFile(const std::string& file_path);
    size_t cleanupOldFailedFiles(int days_to_keep = -1);
    void setAlarmDirectoryPath(const std::string& dir_path);
    void setAutoCleanupEnabled(bool enabled);
    
    // 서비스 제어
    bool start();
    void stop();
    bool isRunning() const { return is_running_.load(); }
    
    // 설정 및 상태
    void updateConfig(const CSPGatewayConfig& new_config);
    const CSPGatewayConfig& getConfig() const { return config_; }
    CSPGatewayStats getStats() const;
    void resetStats();
    
    // 테스트
    bool testConnection();
    bool testS3Connection();
    AlarmSendResult sendTestAlarm();
    std::unordered_map<int, BatchAlarmResult> testMultiBuildingAlarms();
    
    // Dynamic Target 관리
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
    CSPGatewayConfig config_;
    mutable std::mutex config_mutex_;
    
    mutable CSPGatewayStats stats_;
    mutable std::mutex stats_mutex_;
    
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    std::unique_ptr<std::thread> worker_thread_;
    std::unique_ptr<std::thread> retry_thread_;
    
    std::queue<AlarmMessage> alarm_queue_;
    std::queue<std::pair<AlarmMessage, int>> retry_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    std::unique_ptr<PulseOne::Client::HttpClient> http_client_;
    std::unique_ptr<PulseOne::Client::S3Client> s3_client_;
    
    std::unique_ptr<DynamicTargetManager> dynamic_target_manager_;
    std::atomic<bool> use_dynamic_targets_{false};
    
    // 초기화
    void initializeHttpClient();
    void initializeS3Client();
    void initializeDynamicTargetSystem();
    
    // 스레드
    void workerThread();
    void retryThread();
    
    // 알람 처리
    AlarmSendResult taskAlarmSingleDynamic(const AlarmMessage& alarm_message);
    AlarmSendResult taskAlarmSingleLegacy(const AlarmMessage& alarm_message);
    AlarmSendResult retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count);
    bool saveFailedAlarmToFile(const AlarmMessage& alarm_message, const std::string& error_message);
    size_t reprocessFailedAlarms();
    
    // 통계 업데이트
    void updateStatsFromDynamicResults(const std::vector<TargetSendResult>& target_results,
                                     double response_time_ms);
    void updateBatchStats(const BatchAlarmResult& result);
    
    // HTTP/S3 헬퍼
    AlarmSendResult sendHttpPostRequest(const std::string& endpoint,
                                       const std::string& json_data,
                                       const std::string& content_type,
                                       const std::unordered_map<std::string, std::string>& headers);
    bool uploadToS3(const std::string& object_key, const std::string& content);
    
    // 테스트
    bool testConnectionLegacy();
    
    // 유틸리티
    std::string generateBatchFileName() const;
    std::string createBatchDirectory(int building_id) const;
    std::chrono::system_clock::time_point parseCSTimeString(const std::string& time_str) const;
    void loadConfigFromConfigManager();
    
    // ========================================================================
    // ✅ 데이터베이스 관련 메서드
    // ========================================================================
    
    /**
     * @brief 데이터베이스에서 동적 타겟 로드
     * @return 성공 시 true
     */
    bool loadTargetsFromDatabase();
    
    /**
     * @brief 데이터베이스 경로 가져오기
     * @return 데이터베이스 파일 경로
     */
    std::string getDatabasePath() const;
    
    /**
     * @brief 타겟을 데이터베이스에 저장
     * @param target 저장할 타겟 정보
     * @return 성공 시 true
     */
    bool saveTargetToDatabase(const DynamicTarget& target);
    
    /**
     * @brief 데이터베이스에서 타겟 삭제
     * @param name 타겟 이름
     * @return 성공 시 true
     */
    bool deleteTargetFromDatabase(const std::string& name);
    
    /**
     * @brief 데이터베이스에서 타겟 활성화 상태 업데이트
     * @param name 타겟 이름
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool updateTargetEnabledStatus(const std::string& name, bool enabled);
    
    /**
     * @brief export_targets 테이블의 통계 업데이트
     * @param target_name 타겟 이름
     * @param success 성공 여부
     * @param response_time_ms 응답 시간 (밀리초)
     */
    void updateExportTargetStats(
        const std::string& target_name,
        const std::string& target_type,  // ✅ 추가
        bool success,
        double response_time_ms
    );

    /**
     * @brief export_logs 테이블에서 타겟 통계 집계
     * 
     * 이 메서드는 export_logs 테이블을 집계하여 통계를 반환합니다.
     * 
     * 실행 쿼리 예시:
     * SELECT 
     *   target_id,
     *   COUNT(*) as total_count,
     *   SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as success_count,
     *   SUM(CASE WHEN status = 'failure' THEN 1 ELSE 0 END) as failure_count,
     *   AVG(CASE WHEN status = 'success' THEN processing_time_ms END) as avg_time_ms,
     *   MAX(CASE WHEN status = 'success' THEN timestamp END) as last_success_at,
     *   MAX(CASE WHEN status = 'failure' THEN timestamp END) as last_failure_at
     * FROM export_logs
     * WHERE target_id = ? AND timestamp > datetime('now', '-24 hours')
     * 
     * 또는 VIEW 사용:
     * SELECT * FROM v_export_targets_stats_24h WHERE id = ?
     * 
     * @param target_id 타겟 ID
     * @param hours 집계 시간 범위 (기본 24시간)
     * @return 통계 정보 (조회 실패 시 std::nullopt)
     */
    std::optional<TargetStatistics> getTargetStatisticsFromLogs(int target_id, int hours = 24);    
    std::string normalizeTargetType(const std::string& target_type);                        
};

} // namespace CSP
} // namespace PulseOne

#endif // CSP_GATEWAY_H