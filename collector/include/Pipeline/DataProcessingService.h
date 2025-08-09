// =============================================================================
// collector/include/Pipeline/DataProcessingService.h - 올바른 설계
// 🔥 PipelineManager 싱글톤과 연동, TimestampedValue 필드 올바른 사용
// =============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Client/RedisClient.h"
#include "Client/InfluxClient.h"
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

namespace PulseOne {
namespace Pipeline {

// 전방 선언
class PipelineManager;

/**
 * @brief 올바른 DataProcessingService - PipelineManager 싱글톤과 연동
 * @details PipelineManager::GetInstance()에서 배치를 가져와서 처리
 * WorkerPipelineManager와 완전히 분리됨!
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
    
    // ==========================================================================
    // 🔥 올바른 인터페이스 (PipelineManager 싱글톤 사용)
    // ==========================================================================
    
    /**
     * @brief 서비스 시작 - PipelineManager 싱글톤에서 데이터 가져오기
     * WorkerPipelineManager 의존성 완전 제거!
     */
    bool Start();
    
    /**
     * @brief 서비스 중지 - 모든 스레드 종료
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     */
    bool IsRunning() const { return is_running_.load(); }
    
    /**
     * @brief 처리 스레드 수 설정 (시작 전에만 가능)
     */
    void SetThreadCount(size_t thread_count);
    
    /**
     * @brief 배치 크기 설정
     */
    void SetBatchSize(size_t batch_size) { batch_size_ = batch_size; }
    
    /**
     * @brief 통계 정보 조회
     */
    struct ProcessingStats {
        uint64_t total_batches_processed = 0;
        uint64_t total_messages_processed = 0;
        uint64_t redis_writes = 0;
        uint64_t influx_writes = 0;
        uint64_t processing_errors = 0;
        double avg_processing_time_ms = 0.0;
    };
    
    ProcessingStats GetStatistics() const;
    std::shared_ptr<RedisClient> GetRedisClient() const { 
        return redis_client_; 
    }
private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // 🔥 WorkerPipelineManager 의존성 완전 제거!
    // PipelineManager::GetInstance() 직접 사용
    
    // 멀티스레드 관리
    std::vector<std::thread> processing_threads_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    size_t thread_count_{8};
    size_t batch_size_{500};
    
    // 통계 (스레드 안전)
    std::atomic<uint64_t> total_batches_processed_{0};
    std::atomic<uint64_t> total_messages_processed_{0};
    std::atomic<uint64_t> redis_writes_{0};
    std::atomic<uint64_t> influx_writes_{0};
    std::atomic<uint64_t> processing_errors_{0};
    
    // ==========================================================================
    // 🔥 올바른 멀티스레드 처리 메서드들
    // ==========================================================================
    
    /**
     * @brief 개별 처리 스레드 메인 루프
     * @param thread_index 스레드 인덱스 (0~7)
     */
    void ProcessingThreadLoop(size_t thread_index);
    
    /**
     * @brief PipelineManager 싱글톤에서 배치 수집
     * @return 수집된 배치 (최대 batch_size_개)
     */
    std::vector<Structs::DeviceDataMessage> CollectBatchFromPipelineManager();
    
    /**
     * @brief 수집된 배치를 순차 처리
     * @param batch 처리할 배치
     * @param thread_index 처리하는 스레드 인덱스
     */
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, 
                     size_t thread_index);
    
    // ==========================================================================
    // 🔥 단계별 처리 메서드들 (순차 실행)
    // ==========================================================================
    
    /**
     * @brief 1단계: 가상포인트 계산 (나중에 구현)
     */
    std::vector<Structs::DeviceDataMessage> CalculateVirtualPoints(
        const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 2단계: 알람 체크 (나중에 구현)
     */
    void CheckAlarms(const std::vector<Structs::DeviceDataMessage>& all_data);
    
    /**
     * @brief 3단계: Redis 저장 (올바른 구현!)
     */
    void SaveToRedis(const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 4단계: InfluxDB 저장 (나중에 구현)
     */
    void SaveToInfluxDB(const std::vector<Structs::DeviceDataMessage>& batch);
    
    // ==========================================================================
    // 🔥 Redis 저장 헬퍼 메서드들 (필드 오류 수정!)
    // ==========================================================================
    
    /**
     * @brief 개별 디바이스 데이터를 Redis에 저장
     */
    void WriteDeviceDataToRedis(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief TimestampedValue를 JSON으로 변환 (올바른 필드 사용!)
     */
    std::string TimestampedValueToJson(const Structs::TimestampedValue& value, 
                                      const std::string& point_id);
    
    /**
     * @brief 통계 업데이트 헬퍼
     */
    void UpdateStatistics(size_t processed_count, double processing_time_ms);
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H