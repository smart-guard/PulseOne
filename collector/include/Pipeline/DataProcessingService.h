// =============================================================================
// collector/include/Pipeline/DataProcessingService.h - 완성된 버전
// =============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Client/RedisClient.h"
#include "Client/InfluxClient.h"
#include "VirtualPoint/VirtualPointEngine.h"
#include "Alarm/AlarmTypes.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Pipeline {

// 전방 선언
class PipelineManager;

/**
 * @brief DataProcessingService - 데이터 파이프라인 처리 서비스
 * @details 멀티스레드로 DeviceDataMessage를 처리하여 Redis/InfluxDB에 저장
 *          가상포인트 계산, 알람 평가, 경량화 기능 포함
 */
class DataProcessingService {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    explicit DataProcessingService(
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    ~DataProcessingService();
    
    // 복사/이동 생성자 금지 (싱글톤 스타일)
    DataProcessingService(const DataProcessingService&) = delete;
    DataProcessingService& operator=(const DataProcessingService&) = delete;
    DataProcessingService(DataProcessingService&&) = delete;
    DataProcessingService& operator=(DataProcessingService&&) = delete;
    
    // ==========================================================================
    // 공개 인터페이스
    // ==========================================================================
    
    /**
     * @brief 데이터 처리 서비스 시작
     * @return 성공 시 true
     */
    bool Start();
    
    /**
     * @brief 데이터 처리 서비스 중지
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     * @return 실행 중이면 true
     */
    bool IsRunning() const { return is_running_.load(); }
    
    /**
     * @brief 처리 스레드 수 설정
     * @param thread_count 스레드 수 (1-32)
     */
    void SetThreadCount(size_t thread_count);
    
    /**
     * @brief 배치 크기 설정
     * @param batch_size 한 번에 처리할 메시지 수
     */
    void SetBatchSize(size_t batch_size) { batch_size_ = batch_size; }
    
    /**
     * @brief 경량화 모드 설정
     * @param enable true면 경량화 구조체 사용, false면 전체 데이터 사용
     */
    void EnableLightweightMode(bool enable) { use_lightweight_redis_ = enable; }
    
    /**
     * @brief 현재 설정 정보 조회
     */
    struct ServiceConfig {
        size_t thread_count;
        size_t batch_size;
        bool lightweight_mode;
        bool alarm_evaluation_enabled;
        bool virtual_point_calculation_enabled;
    };
    
    ServiceConfig GetConfig() const;
    
    // ==========================================================================
    // 통계 구조체들
    // ==========================================================================
    
    /**
     * @brief 기본 처리 통계
     */
    struct ProcessingStats {
        uint64_t total_batches_processed = 0;
        uint64_t total_messages_processed = 0;
        uint64_t redis_writes = 0;
        uint64_t influx_writes = 0;
        uint64_t processing_errors = 0;
        double avg_processing_time_ms = 0.0;
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["total_batches"] = total_batches_processed;
            j["total_messages"] = total_messages_processed;
            j["redis_writes"] = redis_writes;
            j["influx_writes"] = influx_writes;
            j["errors"] = processing_errors;
            j["avg_time_ms"] = avg_processing_time_ms;
            return j;
        }
    };
    
    /**
     * @brief 확장된 통계 정보 (처리 + 알람)
     */
    struct ExtendedProcessingStats {
        ProcessingStats processing;
        PulseOne::Alarm::AlarmProcessingStats alarms;
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["processing"] = processing.toJson();
            j["alarms"] = alarms.toJson();
            return j;
        }
    };
    
    /**
     * @brief 처리 통계 조회
     */
    ProcessingStats GetStatistics() const;
    
    /**
     * @brief 알람 통계 조회
     */
    PulseOne::Alarm::AlarmProcessingStats GetAlarmStatistics() const;
    
    /**
     * @brief 확장 통계 조회
     */
    ExtendedProcessingStats GetExtendedStatistics() const;
    
    /**
     * @brief Redis 클라이언트 접근
     */
    std::shared_ptr<RedisClient> GetRedisClient() const { 
        return redis_client_; 
    }
    
    /**
     * @brief InfluxDB 클라이언트 접근
     */
    std::shared_ptr<InfluxClient> GetInfluxClient() const { 
        return influx_client_; 
    }

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // 멀티스레드 관리
    std::vector<std::thread> processing_threads_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    size_t thread_count_{4};  // 기본 4개 스레드
    size_t batch_size_{100};  // 기본 100개씩 처리
    
    // 기능 플래그
    std::atomic<bool> use_lightweight_redis_{false};  // 🔥 경량화 모드
    std::atomic<bool> alarm_evaluation_enabled_{true};
    std::atomic<bool> virtual_point_calculation_enabled_{true};
    
    // 기본 통계 (스레드 안전)
    std::atomic<uint64_t> total_batches_processed_{0};
    std::atomic<uint64_t> total_messages_processed_{0};
    std::atomic<uint64_t> redis_writes_{0};
    std::atomic<uint64_t> influx_writes_{0};
    std::atomic<uint64_t> processing_errors_{0};
    
    // 알람 관련 통계
    std::atomic<uint64_t> total_alarms_evaluated_{0};
    std::atomic<uint64_t> total_alarms_triggered_{0};
    std::atomic<uint64_t> critical_alarms_count_{0};
    std::atomic<uint64_t> high_alarms_count_{0};
    
    // 성능 측정
    std::atomic<uint64_t> total_processing_time_ms_{0};
    std::atomic<uint64_t> total_operations_{0};
    
    // ==========================================================================
    // 내부 처리 메서드들
    // ==========================================================================
    
    /**
     * @brief 개별 처리 스레드 메인 루프
     * @param thread_index 스레드 인덱스
     */
    void ProcessingThreadLoop(size_t thread_index);
    
    /**
     * @brief PipelineManager에서 배치 수집
     * @return DeviceDataMessage 배치
     */
    std::vector<Structs::DeviceDataMessage> CollectBatchFromPipelineManager();
    
    /**
     * @brief 배치 처리 실행
     * @param batch 처리할 메시지 배치
     * @param thread_index 처리하는 스레드 인덱스
     */
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, 
                     size_t thread_index);
    
    // ==========================================================================
    // 가상포인트 및 데이터 변환
    // ==========================================================================
    
    /**
     * @brief 가상포인트 계산 및 TimestampedValue 변환
     * @param batch DeviceDataMessage 배치
     * @return 원본 데이터 + 가상포인트를 TimestampedValue로 변환한 결과
     */
    std::vector<Structs::TimestampedValue> CalculateVirtualPoints(
        const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief DeviceDataMessage를 TimestampedValue로 변환
     * @param device_msg 디바이스 메시지
     * @return TimestampedValue 목록
     */
    std::vector<Structs::TimestampedValue> ConvertToTimestampedValues(
        const Structs::DeviceDataMessage& device_msg);
    
    // ==========================================================================
    // 알람 평가 시스템
    // ==========================================================================
    
    /**
     * @brief 알람 평가 실행
     * @param data 평가할 데이터
     * @param thread_index 스레드 인덱스
     */
    void EvaluateAlarms(const std::vector<Structs::TimestampedValue>& data, 
                       size_t thread_index);
    
    /**
     * @brief 개별 메시지에 대한 알람 평가
     * @param message 평가할 메시지
     */
    void evaluateAlarmsForMessage(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief 알람 이벤트 후처리
     * @param alarm_events 발생한 알람 이벤트들
     * @param source_data 원본 데이터
     * @param thread_index 스레드 인덱스
     */
    void ProcessAlarmEvents(const std::vector<PulseOne::Alarm::AlarmEvent>& alarm_events,
                           const Structs::TimestampedValue& source_data,
                           size_t thread_index);
    
    // ==========================================================================
    // 저장 시스템 (경량화 지원)
    // ==========================================================================
    
    /**
     * @brief Redis 저장 (경량화 모드 지원)
     * @param batch 저장할 데이터 배치
     */
    void SaveToRedis(const std::vector<Structs::TimestampedValue>& batch);
    
    /**
     * @brief Redis 저장 - 테스트용 (전체 데이터)
     * @param batch 저장할 메시지 배치
     */
    void SaveToRedisFullData(const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief Redis 저장 - 경량화 버전
     * @param batch 저장할 데이터 배치
     */
    void SaveToRedisLightweight(const std::vector<Structs::TimestampedValue>& batch);
    
    /**
     * @brief InfluxDB 저장
     * @param batch 저장할 데이터 배치
     */
    void SaveToInfluxDB(const std::vector<Structs::TimestampedValue>& batch);
    
    // ==========================================================================
    // 경량화 구조체 변환 메서드들 (미리 준비)
    // ==========================================================================
    
    /**
     * @brief DeviceDataMessage를 LightDeviceStatus로 변환
     * @param message 원본 메시지
     * @return 경량화된 디바이스 상태
     */
    std::string ConvertToLightDeviceStatus(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief TimestampedValue를 LightPointValue로 변환
     * @param value 원본 값
     * @param device_id 디바이스 ID
     * @return 경량화된 포인트 값
     */
    std::string ConvertToLightPointValue(const Structs::TimestampedValue& value, 
                                        const std::string& device_id);
    
    /**
     * @brief DeviceDataMessage를 BatchPointData로 변환
     * @param message 원본 메시지
     * @return 경량화된 배치 데이터
     */
    std::string ConvertToBatchPointData(const Structs::DeviceDataMessage& message);
    
    // ==========================================================================
    // 헬퍼 메서드들
    // ==========================================================================
    std::string getDeviceIdForPoint(int point_id);
    void SaveAlarmEventToRedis(const PulseOne::Alarm::AlarmEvent& alarm_event, size_t thread_index);
    /**
     * @brief TimestampedValue를 Redis에 저장
     * @param value 저장할 값
     */
    void WriteTimestampedValueToRedis(const Structs::TimestampedValue& value);
    
    /**
     * @brief TimestampedValue를 JSON으로 변환
     * @param value 변환할 값
     * @return JSON 문자열
     */
    std::string TimestampedValueToJson(const Structs::TimestampedValue& value);
    
    /**
     * @brief DeviceDataMessage를 JSON으로 변환 (테스트용)
     * @param message 변환할 메시지
     * @return JSON 문자열
     */
    std::string DeviceDataMessageToJson(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief 처리 통계 업데이트
     * @param processed_count 처리된 항목 수
     * @param processing_time_ms 처리 시간 (밀리초)
     */
    void UpdateStatistics(size_t processed_count, double processing_time_ms);
    
    /**
     * @brief 알람 통계 업데이트
     * @param alarms_evaluated 평가된 알람 수
     * @param alarms_triggered 발생한 알람 수
     */
    void UpdateAlarmStatistics(size_t alarms_evaluated, size_t alarms_triggered);
    void UpdateAlarmStatistics(size_t evaluated_count, size_t triggered_count, 
                                                  size_t thread_index);    
    /**
     * @brief 에러 처리 및 로깅
     * @param error_message 에러 메시지
     * @param context 에러 발생 컨텍스트
     */
    void HandleError(const std::string& error_message, const std::string& context);

    // 🔥 알람 통계 구조체
    struct AlarmStatistics {
        std::atomic<size_t> total_evaluations{0};
        std::atomic<size_t> total_triggers{0};
        std::chrono::system_clock::time_point last_evaluation_time;
        double trigger_rate{0.0};
        
        struct ThreadStats {
            std::atomic<size_t> evaluations{0};
            std::atomic<size_t> triggers{0};
        };
        std::map<size_t, ThreadStats> thread_statistics;
    };
    
    // 🔥 멤버 변수
    AlarmStatistics alarm_statistics_;
    std::mutex alarm_stats_mutex_;

};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H