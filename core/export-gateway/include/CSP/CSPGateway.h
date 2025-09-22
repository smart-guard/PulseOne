/**
 * @file CSPGateway.h
 * @brief CSP Gateway 클래스 - C# CSPGateway 완전 포팅
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.0
 * 
 * C# CSPGateway 핵심 기능 포팅:
 * - callAPIAlarm() - HTTP API 호출
 * - callS3Alarm() - S3 업로드
 * - taskAlarmSingle() - 단일 알람 처리
 * - 재시도 로직 및 오류 처리
 */

#ifndef CSP_GATEWAY_H
#define CSP_GATEWAY_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <functional>

#include "CSP/AlarmMessage.h"

// PulseOne Shared 라이브러리
#include "Client/HttpClient.h"
#include "Client/S3Client.h"
#include "Utils/RetryManager.h"

#ifdef HAS_SHARED_LIBS
    #include "Database/Entities/AlarmOccurrenceEntity.h"
    #include "Utils/LogManager.h"
#endif

namespace PulseOne {
namespace CSP {

/**
 * @brief CSP Gateway 설정 구조체
 * 
 * C# 원본의 설정 항목들:
 * - API 엔드포인트
 * - S3 설정
 * - Building ID
 * - 재시도 설정
 */
struct CSPGatewayConfig {
    // API 설정
    std::string api_endpoint = "";
    std::string api_key = "";
    std::string api_secret = "";
    int api_timeout_sec = 30;
    
    // S3 설정
    std::string s3_endpoint = "";
    std::string s3_access_key = "";
    std::string s3_secret_key = "";
    std::string s3_bucket_name = "";
    std::string s3_region = "us-east-1";
    
    // Building 설정
    int building_id = 1001;
    bool use_local_time = true;
    
    // 재시도 설정
    int max_retry_count = 3;
    int retry_interval_ms = 5000;
    int initial_delay_ms = 1000;
    
    // 배치 처리 설정
    int batch_size = 10;
    int batch_timeout_ms = 5000;
    
    // 디버그 설정
    bool debug_mode = false;
    bool save_failed_to_file = true;
    std::string failed_file_path = "./failed_alarms";
};

/**
 * @brief 알람 전송 결과
 */
struct AlarmSendResult {
    bool success = false;
    std::string error_message;
    int http_status_code = 0;
    std::chrono::system_clock::time_point timestamp;
    
    // S3 관련
    bool s3_success = false;
    std::string s3_error_message;
    std::string s3_file_path;
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
    
    std::chrono::system_clock::time_point last_success_time;
    std::chrono::system_clock::time_point last_failure_time;
    double avg_response_time_ms = 0.0;
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
     * @brief 통계 정보 조회 (참조로 반환)
     * @return 통계 정보
     */
    CSPGatewayStats getStats() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStats();

    // =======================================================================
    // 재시도 및 오류 처리
    // =======================================================================
    
    /**
     * @brief 실패한 알람 재시도
     * @param alarm_message 재시도할 알람
     * @param attempt_count 현재 시도 횟수
     * @return 재시도 결과
     */
    AlarmSendResult retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count);
    
    /**
     * @brief 실패한 알람을 파일로 저장
     * @param alarm_message 저장할 알람
     * @param error_message 오류 메시지
     * @return 저장 성공 여부
     */
    bool saveFailedAlarmToFile(const AlarmMessage& alarm_message, 
                              const std::string& error_message);
    
    /**
     * @brief 저장된 실패 알람들 재처리
     * @return 재처리된 알람 개수
     */
    size_t reprocessFailedAlarms();

    // =======================================================================
    // 테스트 및 유틸리티 메서드들
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
     * @return 테스트 결과
     */
    AlarmSendResult sendTestAlarm();

private:
    // =======================================================================
    // 내부 구현 메서드들
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
     * @brief 워커 스레드 실행
     */
    void workerThread();
    
    /**
     * @brief 재시도 스레드 실행
     */
    void retryThread();
    
    /**
     * @brief HTTP POST 요청 실행
     * @param endpoint 엔드포인트
     * @param json_data JSON 데이터
     * @param headers HTTP 헤더들
     * @return HTTP 응답
     */
    std::pair<int, std::string> executeHttpPost(const std::string& endpoint,
                                               const std::string& json_data,
                                               const std::unordered_map<std::string, std::string>& headers);
    
    /**
     * @brief S3 파일 업로드
     * @param bucket_name 버킷명
     * @param object_key 객체 키
     * @param content 파일 내용
     * @return 업로드 성공 여부
     */
    bool uploadToS3(const std::string& bucket_name,
                   const std::string& object_key,
                   const std::string& content);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    CSPGatewayConfig config_;                                    ///< 설정 정보
    CSPGatewayStats stats_;                                      ///< 통계 정보
    
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