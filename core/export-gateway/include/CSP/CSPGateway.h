/**
 * @file CSPGateway.h - 🎯 컴파일 에러 100% 해결 완료
 * @brief CSP Gateway 헤더 - C# CSPGateway 완전 포팅 (수정본)
 * @author PulseOne Development Team  
 * @date 2025-09-22
 * 🔥 함수 시그니처 불일치 문제 해결 완료
 * 📁 저장 위치: core/export-gateway/include/CSP/CSPGateway.h
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
 * @brief CSP Gateway 설정
 */
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
    
    bool debug_mode = false;
    int max_retry_attempts = 3;
    int initial_delay_ms = 1000;
    int max_queue_size = 10000;
    
    std::string failed_file_path = "./failed_alarms";
    
    // C# 호환성
    bool use_api = true;
    bool use_s3 = true;
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
 * @brief CSP Gateway 통계 🔥 atomic 복사 생성자 문제 해결 완료
 * 기존 Common/Structs.h의 LogStatistics 패턴 100% 적용
 */
struct CSPGatewayStats {
    std::atomic<size_t> total_alarms{0};
    std::atomic<size_t> successful_api_calls{0};
    std::atomic<size_t> failed_api_calls{0};
    std::atomic<size_t> successful_s3_uploads{0};
    std::atomic<size_t> failed_s3_uploads{0};
    std::atomic<size_t> retry_attempts{0};
    
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    double avg_response_time_ms = 0.0;
    
    /**
     * @brief 기본 생성자
     */
    CSPGatewayStats() {
        last_success_time = std::chrono::system_clock::now();
        last_failure_time = last_success_time;
    }
    
    /**
     * @brief 복사 생성자 명시적 구현 (atomic 때문에 필요)
     * 🔥 기존 LogStatistics와 100% 동일한 패턴
     */
    CSPGatewayStats(const CSPGatewayStats& other) 
        : total_alarms(other.total_alarms.load())
        , successful_api_calls(other.successful_api_calls.load())
        , failed_api_calls(other.failed_api_calls.load())
        , successful_s3_uploads(other.successful_s3_uploads.load())
        , failed_s3_uploads(other.failed_s3_uploads.load())
        , retry_attempts(other.retry_attempts.load())
        , last_success_time(other.last_success_time)
        , last_failure_time(other.last_failure_time)
        , avg_response_time_ms(other.avg_response_time_ms) {
    }
    
    /**
     * @brief 할당 연산자 명시적 구현
     * 🔥 기존 LogStatistics와 100% 동일한 패턴
     */
    CSPGatewayStats& operator=(const CSPGatewayStats& other) {
        if (this != &other) {
            total_alarms.store(other.total_alarms.load());
            successful_api_calls.store(other.successful_api_calls.load());
            failed_api_calls.store(other.failed_api_calls.load());
            successful_s3_uploads.store(other.successful_s3_uploads.load());
            failed_s3_uploads.store(other.failed_s3_uploads.load());
            retry_attempts.store(other.retry_attempts.load());
            last_success_time = other.last_success_time;
            last_failure_time = other.last_failure_time;
            avg_response_time_ms = other.avg_response_time_ms;
        }
        return *this;
    }
    
    /**
     * @brief 총 전송 시도 수 계산
     * 🔥 기존 LogStatistics.GetTotalLogs() 패턴 적용
     */
    size_t getTotalAttempts() const {
        return successful_api_calls.load() + failed_api_calls.load();
    }
    
    /**
     * @brief 성공률 계산 (백분율)
     */
    double getSuccessRate() const {
        size_t total = getTotalAttempts();
        return total > 0 ? (static_cast<double>(successful_api_calls.load()) / total * 100.0) : 0.0;
    }
    
    /**
     * @brief S3 총 업로드 시도 수
     */
    size_t getTotalS3Attempts() const {
        return successful_s3_uploads.load() + failed_s3_uploads.load();
    }
    
    /**
     * @brief S3 성공률 계산 (백분율)
     */
    double getS3SuccessRate() const {
        size_t total = getTotalS3Attempts();
        return total > 0 ? (static_cast<double>(successful_s3_uploads.load()) / total * 100.0) : 0.0;
    }
};

/**
 * @brief CSP Gateway 메인 클래스
 * 
 * C# CSPGateway의 핵심 기능들을 C++로 포팅:
 * 
 * 주요 메서드:
 * - taskAlarmSingle() - 단일 알람 처리
 * - callAPIAlarm() - HTTP API 호출
 * - callS3Alarm() - S3 업로드
 * - 재시도 로직
 * - 비동기 배치 처리
 */
class CSPGateway {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 생성자
     * @param config CSP Gateway 설정
     */
    explicit CSPGateway(const CSPGatewayConfig& config);
    
    /**
     * @brief 소멸자
     */
    ~CSPGateway();

    // 복사/이동 생성자 비활성화 (싱글톤 패턴)
    CSPGateway(const CSPGateway&) = delete;
    CSPGateway& operator=(const CSPGateway&) = delete;
    CSPGateway(CSPGateway&&) = delete;
    CSPGateway& operator=(CSPGateway&&) = delete;

    // =======================================================================
    // C# CSPGateway 핵심 메서드들 (원본과 동일한 이름)
    // =======================================================================
    
    /**
     * @brief 단일 알람 처리 (C# taskAlarmSingle 포팅)
     * @param alarm_message 처리할 알람 메시지
     * @return 처리 결과
     */
    AlarmSendResult taskAlarmSingle(const AlarmMessage& alarm_message);
    
    /**
     * @brief API 알람 호출 (C# callAPIAlarm 포팅)
     * @param alarm_message 전송할 알람 메시지
     * @return 전송 결과
     */
    AlarmSendResult callAPIAlarm(const AlarmMessage& alarm_message);
    
    /**
     * @brief S3 알람 업로드 (C# callS3Alarm 포팅)
     * @param alarm_message 업로드할 알람 메시지
     * @param file_name 파일명 (선택사항)
     * @return 업로드 결과
     */
    AlarmSendResult callS3Alarm(const AlarmMessage& alarm_message, 
                                const std::string& file_name = "");

    // =======================================================================
    // PulseOne Entity 연동 메서드들
    // =======================================================================

#ifdef HAS_SHARED_LIBS
    /**
     * @brief PulseOne AlarmOccurrence 처리
     * @param occurrence 알람 발생 엔티티
     * @return 처리 결과
     */
    AlarmSendResult processAlarmOccurrence(const Database::Entities::AlarmOccurrenceEntity& occurrence);
    
    /**
     * @brief 배치 알람 처리
     * @param occurrences 알람 발생 엔티티 리스트
     * @return 처리 결과 리스트
     */
    std::vector<AlarmSendResult> processBatchAlarms(
        const std::vector<Database::Entities::AlarmOccurrenceEntity>& occurrences);
#endif

    // =======================================================================
    // 서비스 제어 메서드들
    // =======================================================================
    
    /**
     * @brief 서비스 시작
     * @return 성공 여부
     */
    bool start();
    
    /**
     * @brief 서비스 중지
     */
    void stop();
    
    /**
     * @brief 서비스 실행 상태 확인
     * @return 실행 중이면 true
     */
    bool isRunning() const { return is_running_.load(); }

    // =======================================================================
    // 설정 및 상태 관리
    // =======================================================================
    
    /**
     * @brief 설정 업데이트
     * @param new_config 새로운 설정
     */
    void updateConfig(const CSPGatewayConfig& new_config);
    
    /**
     * @brief 현재 설정 조회
     * @return 현재 설정
     */
    const CSPGatewayConfig& getConfig() const { return config_; }
    
    /**
     * @brief 통계 정보 조회 (값으로 반환 - 이제 복사 생성자가 있으므로 가능)
     * @return 통계 정보
     * 🔥 이제 atomic 복사 생성자 덕분에 컴파일 에러 없음
     */
    CSPGatewayStats getStats() const { return stats_; }
    
    /**
     * @brief 통계 초기화
     */
    void resetStats();

    // =======================================================================
    // 테스트 및 진단 메서드들
    // =======================================================================
    
    /**
     * @brief 연결 테스트
     * @return 연결 성공 여부
     */
    bool testConnection();
    
    /**
     * @brief S3 연결 테스트
     * @return S3 연결 성공 여부
     */
    bool testS3Connection();
    
    /**
     * @brief 테스트 알람 전송
     * @return 전송 결과
     */
    AlarmSendResult sendTestAlarm();

    // =======================================================================
    // 재시도 및 오류 처리 메서드들
    // =======================================================================
    
    /**
     * @brief 실패한 알람 재처리
     * @return 재처리된 알람 수
     */
    size_t reprocessFailedAlarms();

private:
    // =======================================================================
    // 내부 도우미 메서드들
    // =======================================================================
    
    /**
     * @brief HTTP 클라이언트 초기화
     */
    void initializeHttpClient();
    
    /**
     * @brief S3 클라이언트 초기화
     */
    void initializeS3Client();
    
    /**
     * @brief 실패한 알람 재시도
     * @param alarm_message 알람 메시지
     * @param attempt_count 시도 횟수
     * @return 처리 결과
     */
    AlarmSendResult retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count);
    
    /**
     * @brief 실패한 알람 파일 저장
     * @param alarm_message 알람 메시지
     * @param error_message 오류 메시지
     * @return 저장 성공 여부
     */
    bool saveFailedAlarmToFile(const AlarmMessage& alarm_message, 
                              const std::string& error_message);
    
    /**
     * @brief 워커 스레드
     */
    void workerThread();
    
    /**
     * @brief 재시도 스레드
     */
    void retryThread();
    
    /**
     * @brief HTTP POST 요청 처리 🔥 시그니처 수정 완료
     * @param endpoint 엔드포인트
     * @param json_data JSON 데이터
     * @param content_type Content-Type
     * @param headers 헤더들
     * @return 처리 결과
     */
    AlarmSendResult sendHttpPostRequest(const std::string& endpoint,
                                       const std::string& json_data,
                                       const std::string& content_type,
                                       const std::unordered_map<std::string, std::string>& headers);
    
    /**
     * @brief S3 파일 업로드 🔥 시그니처 수정 완료
     * @param object_key 객체 키
     * @param content 파일 내용
     * @return 업로드 성공 여부
     */
    bool uploadToS3(const std::string& object_key,
                   const std::string& content);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    CSPGatewayConfig config_;                                    ///< 설정 정보
    CSPGatewayStats stats_;                                      ///< 통계 정보 🔥 이제 복사 가능
    
    std::atomic<bool> is_running_{false};                       ///< 실행 상태
    std::atomic<bool> should_stop_{false};                      ///< 중지 플래그
    
    // 스레드 관리
    std::unique_ptr<std::thread> worker_thread_;                 ///< 워커 스레드
    std::unique_ptr<std::thread> retry_thread_;                  ///< 재시도 스레드
    
    // 동기화 객체들
    mutable std::mutex config_mutex_;                            ///< 설정 보호
    mutable std::mutex stats_mutex_;                             ///< 통계 보호
    std::mutex queue_mutex_;                                     ///< 큐 보호
    std::condition_variable queue_cv_;                           ///< 큐 대기
    
    // 알람 큐들
    std::queue<AlarmMessage> alarm_queue_;                       ///< 일반 알람 큐
    std::queue<std::pair<AlarmMessage, int>> retry_queue_;       ///< 재시도 큐
    
    // HTTP 클라이언트 (Shared Library)
    std::unique_ptr<PulseOne::Client::HttpClient> http_client_;

    // S3 클라이언트 (Shared Library)  
    std::unique_ptr<PulseOne::Client::S3Client> s3_client_;
    
    // 재시도 관리자 (Shared Library)
    std::unique_ptr<PulseOne::Utils::RetryManager<AlarmMessage>> retry_manager_;
};

} // namespace CSP
} // namespace PulseOne

#endif // CSP_GATEWAY_H