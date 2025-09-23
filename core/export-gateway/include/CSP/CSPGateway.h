/**
 * @file CSPGateway.h - 완전한 헤더 파일 (멀티빌딩, 시간필터, 배치관리 추가)
 * @brief CSP Gateway 헤더 - C# CSPGateway 완전 포팅
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/include/CSP/CSPGateway.h
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

#include "AlarmMessage.h"

// PulseOne 네임스페이스가 있는 헤더들 (조건부 include)
#ifdef HAS_SHARED_LIBS
    #include "Database/Entities/AlarmOccurrenceEntity.h"
    namespace PulseOne {
        namespace Client {
            class HttpClient;
            class S3Client;
        }
        namespace Utils {
            template<typename T> class RetryManager;
        }
    }
#endif

namespace PulseOne {
namespace CSP {

/**
 * @brief CSP Gateway 설정 (확장된 버전)
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
    
    // 기본 옵션
    bool debug_mode = false;
    int max_retry_attempts = 3;
    int initial_delay_ms = 1000;
    int max_queue_size = 10000;
    std::string failed_file_path = "./failed_alarms";
    bool use_api = true;
    bool use_s3 = true;
    
    // 1. 멀티빌딩 지원 (C# Dictionary<int, List<AlarmMessage>> 지원)
    std::vector<int> supported_building_ids = {1001};
    bool multi_building_enabled = false;
    
    // 2. 알람 무시 시간 설정 (C# AlarmIgnoreMinutes 포팅)
    int alarm_ignore_minutes = 5;
    bool use_local_time = true;
    
    // 3. 배치 파일 관리 (C# _alarmDirPath 포팅)
    std::string alarm_dir_path = "./alarm_files";
    bool auto_cleanup_success_files = true;
    int keep_failed_files_days = 7;
    size_t max_batch_size = 1000;
    int batch_timeout_ms = 5000;
};

/**
 * @brief 알람 전송 결과
 */
struct AlarmSendResult {
    bool success = false;
    int status_code = 0;
    std::string response_body = "";
    std::string error_message = "";
    
    // S3 관련
    bool s3_success = false;
    std::string s3_error_message;
    std::string s3_file_path;
};

/**
 * @brief 배치 알람 처리 결과 (C# taskAlarmMultiple 결과 포팅)
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
    
    // 배치 처리 통계 추가
    std::atomic<size_t> total_batches{0};
    std::atomic<size_t> successful_batches{0};
    std::atomic<size_t> ignored_alarms{0}; // 시간 필터링으로 무시된 알람
    
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

// 멀티빌딩 알람 타입 정의 (C# Dictionary<int, List<AlarmMessage>> 포팅)
using MultiBuildingAlarms = std::unordered_map<int, std::vector<AlarmMessage>>;

/**
 * @brief CSP Gateway 메인 클래스 (완전한 C# 포팅)
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
    // 1. 멀티빌딩 지원 (C# Dictionary<int, List<AlarmMessage>> 포팅)
    // =======================================================================
    
    /**
     * @brief 멀티빌딩 알람 배치 처리 (C# taskAlarmMultiple 포팅)
     * @param building_alarms 빌딩별로 그룹화된 알람들
     * @return 빌딩별 처리 결과
     */
    std::unordered_map<int, BatchAlarmResult> processMultiBuildingAlarms(
        const MultiBuildingAlarms& building_alarms);
    
    /**
     * @brief 알람을 빌딩별로 그룹화
     * @param alarms 알람 메시지들
     * @return 빌딩별로 그룹화된 알람
     */
    MultiBuildingAlarms groupAlarmsByBuilding(const std::vector<AlarmMessage>& alarms);
    
    /**
     * @brief 지원하는 빌딩 ID 설정
     * @param building_ids 지원할 빌딩 ID 목록
     */
    void setSupportedBuildingIds(const std::vector<int>& building_ids);
    
    /**
     * @brief 멀티빌딩 모드 활성화/비활성화
     * @param enabled true=활성화, false=비활성화
     */
    void setMultiBuildingEnabled(bool enabled);

    // =======================================================================
    // 2. 알람 무시 시간 필터링 (C# AlarmIgnoreMinutes 포팅)
    // =======================================================================
    
    /**
     * @brief 알람 무시 시간 필터링 (C# ignoreTime 로직 포팅)
     * @param alarms 필터링할 알람들
     * @return 필터링된 알람들 (무시된 알람 제외)
     */
    std::vector<AlarmMessage> filterIgnoredAlarms(const std::vector<AlarmMessage>& alarms);
    
    /**
     * @brief 알람이 무시 시간 내인지 확인
     * @param alarm_time 알람 시간 (C# 형식 문자열)
     * @return 무시해야 하면 true
     */
    bool shouldIgnoreAlarm(const std::string& alarm_time) const;
    
    /**
     * @brief 알람 무시 분수 설정 (C# AlarmIgnoreMinutes)
     * @param minutes 무시할 분수
     */
    void setAlarmIgnoreMinutes(int minutes);
    
    /**
     * @brief 로컬시간 사용 여부 설정 (C# IsLocalTime)
     * @param use_local true=로컬시간, false=UTC
     */
    void setUseLocalTime(bool use_local);

    // =======================================================================
    // 3. 배치 파일 관리 및 자동 정리 (C# 파일 관리 로직 포팅)
    // =======================================================================
    
    /**
     * @brief 배치 알람 파일 저장 (C# 스타일 경로)
     * @param building_id 빌딩 ID
     * @param alarms 저장할 알람들
     * @return 저장된 파일 경로
     */
    std::string saveBatchAlarmFile(int building_id, const std::vector<AlarmMessage>& alarms);
    
    /**
     * @brief 성공한 배치 파일 자동 삭제 (C# File.Delete 로직)
     * @param file_path 삭제할 파일 경로
     * @return 성공 여부
     */
    bool cleanupSuccessfulBatchFile(const std::string& file_path);
    
    /**
     * @brief 오래된 실패 파일들 정리
     * @param days_to_keep 보관할 일수
     * @return 삭제된 파일 수
     */
    size_t cleanupOldFailedFiles(int days_to_keep = -1);
    
    /**
     * @brief 알람 디렉토리 경로 설정 (C# _alarmDirPath)
     * @param dir_path 알람 파일 저장 디렉토리
     */
    void setAlarmDirectoryPath(const std::string& dir_path);
    
    /**
     * @brief 자동 정리 활성화/비활성화
     * @param enabled true=활성화, false=비활성화
     */
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
    CSPGatewayStats getStats() const { return stats_; }
    void resetStats();

    // =======================================================================
    // 테스트 및 진단 메서드들
    // =======================================================================
    
    bool testConnection();
    bool testS3Connection();
    AlarmSendResult sendTestAlarm();
    
    /**
     * @brief 멀티빌딩 테스트 (새로 추가)
     * @return 빌딩별 테스트 결과
     */
    std::unordered_map<int, BatchAlarmResult> testMultiBuildingAlarms();

private:
    // =======================================================================
    // 기존 private 멤버들
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

    // =======================================================================
    // 기존 private 메서드들
    // =======================================================================
    
    void initializeHttpClient();
    void initializeS3Client();
    void workerThread();
    void retryThread();
    
    AlarmSendResult retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count);
    bool saveFailedAlarmToFile(const AlarmMessage& alarm_message, const std::string& error_message);
    size_t reprocessFailedAlarms();
    
    // HTTP/S3 헬퍼 메서드들
    AlarmSendResult sendHttpPostRequest(const std::string& endpoint,
                                       const std::string& json_data,
                                       const std::string& content_type,
                                       const std::unordered_map<std::string, std::string>& headers);
    bool uploadToS3(const std::string& object_key, const std::string& content);

    // =======================================================================
    // 새로 추가할 private 메서드들
    // =======================================================================
    
    /**
     * @brief C# 스타일 파일명 생성 (yyyyMMddHHmmss.json)
     */
    std::string generateBatchFileName() const;
    
    /**
     * @brief C# 스타일 디렉토리 구조 생성 (BuildingID/yyyyMMdd/)
     */
    std::string createBatchDirectory(int building_id) const;
    
    /**
     * @brief C# DateTime 문자열을 chrono로 변환
     */
    std::chrono::system_clock::time_point parseCSTimeString(const std::string& time_str) const;
    
    /**
     * @brief 배치 처리 통계 업데이트
     */
    void updateBatchStats(const BatchAlarmResult& result);
    
    /**
     * @brief ConfigManager에서 설정 로드 (기존)
     */
    void loadConfigFromConfigManager();
};

} // namespace CSP
} // namespace PulseOne

#endif // CSP_GATEWAY_H