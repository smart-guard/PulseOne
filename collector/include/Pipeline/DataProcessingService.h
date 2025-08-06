// =============================================================================
// collector/include/Pipeline/DataProcessingService.h - 새로 생성
// 🔥 핵심! 멀티스레드로 실제 데이터 처리 담당
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

/**
 * @brief DataProcessingService - 핵심 데이터 처리 엔진
 * @details WorkerPipelineManager로부터 배치를 받아 멀티스레드로 처리
 *          순차적으로: 가상포인트 → 알람 → Redis → InfluxDB
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
    // 🔥 핵심 인터페이스
    // ==========================================================================
    
    /**
     * @brief WorkerPipelineManager로부터 배치 받아서 처리
     * @param batch 처리할 DeviceDataMessage 배치
     */
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 서비스 시작
     */
    bool Start();
    
    /**
     * @brief 서비스 중지
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     */
    bool IsRunning() const { return is_running_.load(); }

private:
    // ==========================================================================
    // 🔥 멤버 변수들
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // 실행 상태
    std::atomic<bool> is_running_{false};
    
    // 통계
    std::atomic<uint64_t> total_batches_processed_{0};
    std::atomic<uint64_t> total_messages_processed_{0};
    std::atomic<uint64_t> redis_writes_{0};
    std::atomic<uint64_t> influx_writes_{0};
    
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
     * @brief 3단계: Redis 저장 (지금 구현!)
     */
    void SaveToRedis(const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 4단계: InfluxDB 저장 (나중에 구현)
     */
    void SaveToInfluxDB(const std::vector<Structs::DeviceDataMessage>& batch);
    
    // ==========================================================================
    // 🔥 Redis 저장 헬퍼 메서드들
    // ==========================================================================
    
    /**
     * @brief 개별 디바이스 데이터를 Redis에 저장
     */
    void WriteDeviceDataToRedis(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief TimestampedValue를 JSON으로 변환
     */
    std::string TimestampedValueToJson(const Structs::TimestampedValue& value);
};

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H