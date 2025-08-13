//=============================================================================
// collector/include/Pipeline/DataProcessingService.h (수정본)
// 
// 🎯 목적: 알람과 가상포인트 Redis 저장 기능 추가를 위한 헤더 확장
// 📋 추가사항:
//   - 알람 평가 및 Redis 저장 메서드 선언
//   - 가상포인트 계산 및 Redis 저장 메서드 선언  
//   - 새로운 통계 카운터 (alarms_evaluated_, virtual_points_calculated_)
//   - 기존 구조 100% 준수, 네임스페이스 및 의존성 유지
//=============================================================================

#ifndef PULSEONE_DATA_PROCESSING_SERVICE_H
#define PULSEONE_DATA_PROCESSING_SERVICE_H

#include "Common/Structs.h"
#include "Client/RedisClientImpl.h"
#include "Client/InfluxClient.h"
#include "Utils/LogManager.h"
#include "Alarm/AlarmTypes.h"      // 🔥 NEW: 알람 타입 정의
#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

// 🔥 전방 선언 추가
namespace PulseOne {
namespace Alarm {
    struct AlarmEvaluation;
}
namespace VirtualPoint {
    class VirtualPointEngine;
}
}

namespace PulseOne {
namespace Pipeline {

/**
 * @brief 데이터 처리 서비스 - 파이프라인의 핵심 처리 엔진
 * 
 * 기능:
 * - 배치 데이터 처리
 * - Redis/RDB/InfluxDB 저장
 * - 🔥 NEW: 알람 평가 및 Redis 저장
 * - 🔥 NEW: 가상포인트 계산 및 Redis 저장
 * - 성능 통계 수집
 */
class DataProcessingService {
public:
    // ==========================================================================
    // 생성자/소멸자
    // ==========================================================================
    
    /**
     * @brief 생성자
     * @param redis_client Redis 클라이언트 (nullable)
     * @param influx_client InfluxDB 클라이언트 (nullable)
     */
    DataProcessingService(std::shared_ptr<RedisClientImpl> redis_client = nullptr,
                         std::shared_ptr<InfluxClient> influx_client = nullptr);
    
    /**
     * @brief 소멸자
     */
    ~DataProcessingService();

    // ==========================================================================
    // 메인 처리 메서드들
    // ==========================================================================
    
    /**
     * @brief 배치 데이터 처리 (메인 진입점)
     * @param batch 처리할 메시지 배치
     * @param thread_index 처리 스레드 인덱스
     * 
     * 🔥 수정사항: 알람 평가 및 가상포인트 계산 추가
     */
    void ProcessBatch(const std::vector<Structs::DeviceDataMessage>& batch, size_t thread_index);
    
    /**
     * @brief 클라이언트 설정
     */
    void SetRedisClient(std::shared_ptr<RedisClientImpl> client);
    void SetInfluxClient(std::shared_ptr<InfluxClient> client);

    // ==========================================================================
    // 🔥 NEW: 알람 관련 메서드들
    // ==========================================================================
    
    /**
     * @brief 메시지에 대해 알람을 평가하고 Redis에 저장
     * @param message 디바이스 데이터 메시지
     */
    void EvaluateAlarmsAndStoreToRedis(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief 개별 알람 결과를 Redis에 저장
     * @param alarm_result 알람 평가 결과
     */
    void StoreAlarmToRedis(const Alarm::AlarmEvaluation& alarm_result);

    // ==========================================================================
    // 🔥 NEW: 가상포인트 관련 메서드들  
    // ==========================================================================
    
    // ==========================================================================
    // 🔥 NEW: 기존 메서드를 래핑한 스마트 처리 메서드들
    // ==========================================================================
    
    /**
     * @brief 기존 알람 평가에 스마트 중복 체크 추가
     * @param batch 메시지 배치
     * @param thread_index 스레드 인덱스
     * 
     * 기존 EvaluateAlarms() 메서드를 래핑하여 중복 처리 방지
     */
    void EvaluateAlarmsWithSmartCheck(const std::vector<Structs::DeviceDataMessage>& batch, size_t thread_index);
    
    /**
     * @brief 기존 가상포인트 계산에 스마트 중복 체크 추가
     * @param batch 메시지 배치
     * @return 계산된 TimestampedValue 배열
     * 
     * 기존 CalculateVirtualPoints() 메서드를 래핑하여 중복 처리 방지
     */
    std::vector<Structs::TimestampedValue> CalculateVirtualPointsWithSmartCheck(
        const std::vector<Structs::DeviceDataMessage>& batch);

    // ==========================================================================
    // 🔥 NEW: Redis 저장 보완 메서드들
    // ==========================================================================
    
    /**
     * @brief 알람 Redis 키 누락 확인 및 복구
     * @param batch 처리된 메시지 배치
     */
    void EnsureAlarmResultsInRedis(const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 가상포인트 Redis 키 누락 확인 및 복구  
     * @param batch 처리된 메시지 배치
     */
    void EnsureVirtualPointResultsInRedis(const std::vector<Structs::DeviceDataMessage>& batch);
    
    /**
     * @brief 개별 가상포인트 결과를 Redis에 저장
     * @param vp_result 가상포인트 계산 결과
     */
    void StoreVirtualPointToRedis(const Structs::TimestampedValue& vp_result);

    // ==========================================================================
    // 기존 저장 메서드들 (기존 구조 유지)
    // ==========================================================================
    
    /**
     * @brief 포인트 데이터를 Redis에 저장 (기존)
     */
    void SavePointDataToRedis(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief 변화된 포인트들을 RDB에 저장 (기존)
     */
    void SaveChangedPointsToRDB(const Structs::DeviceDataMessage& message, 
                               const std::vector<Structs::TimestampedValueData>& changed_points);
    
    /**
     * @brief InfluxDB 버퍼링 (기존)
     */
    void BufferForInfluxDB(const Structs::DeviceDataMessage& message);

    // ==========================================================================
    // 유틸리티 메서드들 (기존 + 신규)
    // ==========================================================================
    
    /**
     * @brief 변화된 포인트만 필터링 (기존)
     */
    std::vector<Structs::TimestampedValueData> GetChangedPoints(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief JSON 직렬화 (기존)
     */
    std::string DeviceDataMessageToJson(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief 통계 업데이트 (기존)
     */
    void UpdateStatistics(size_t point_count);

    // ==========================================================================
    // 🔥 NEW: 확장된 통계 메서드들
    // ==========================================================================
    
    /**
     * @brief 기존 통계 (기존 구조 유지)
     */
    size_t GetPointsProcessed() const { return points_processed_.load(); }
    size_t GetRedisWrites() const { return redis_writes_.load(); }
    size_t GetInfluxWrites() const { return influx_writes_.load(); }
    size_t GetProcessingErrors() const { return processing_errors_.load(); }
    
    /**
     * @brief 🔥 새로운 통계
     */
    size_t GetAlarmsEvaluated() const;
    size_t GetVirtualPointsCalculated() const;
    
    /**
     * @brief 전체 통계 리셋
     */
    void ResetStatistics() {
        points_processed_.store(0);
        redis_writes_.store(0);
        influx_writes_.store(0);
        processing_errors_.store(0);
        alarms_evaluated_.store(0);              // 🔥 NEW
        virtual_points_calculated_.store(0);     // 🔥 NEW
    }

    // ==========================================================================
    // 진단 및 상태 메서드들
    // ==========================================================================
    
    /**
     * @brief 서비스 상태 확인
     */
    bool IsHealthy() const {
        return (redis_client_ && redis_client_->isConnected()) || influx_client_;
    }
    
    /**
     * @brief 상세 상태 정보
     */
    nlohmann::json GetDetailedStatus() const {
        nlohmann::json status;
        status["points_processed"] = GetPointsProcessed();
        status["redis_writes"] = GetRedisWrites();
        status["influx_writes"] = GetInfluxWrites();
        status["processing_errors"] = GetProcessingErrors();
        status["alarms_evaluated"] = GetAlarmsEvaluated();           // 🔥 NEW
        status["virtual_points_calculated"] = GetVirtualPointsCalculated(); // 🔥 NEW
        status["redis_connected"] = redis_client_ && redis_client_->isConnected();
        status["influx_connected"] = influx_client_ != nullptr;
        return status;
    }

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // ==========================================================================
    // 기존 멤버 변수들 (모두 유지)
    // ==========================================================================
    
    // 클라이언트들
    std::shared_ptr<RedisClientImpl> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // 🔥 기존 설정 플래그들 (완전 유지)
    std::atomic<bool> alarm_evaluation_enabled_{true};
    std::atomic<bool> virtual_point_calculation_enabled_{true};
    std::atomic<bool> use_lightweight_redis_{false};
    
    // 기존 통계 카운터들
    std::atomic<size_t> points_processed_{0};
    std::atomic<size_t> redis_writes_{0};
    std::atomic<size_t> influx_writes_{0};
    std::atomic<size_t> processing_errors_{0};
    
    // 🔥 NEW: 확장된 통계 카운터들
    std::atomic<size_t> alarms_evaluated_{0};
    std::atomic<size_t> virtual_points_calculated_{0};
    
    // 추가 멤버 변수들
    mutable std::mutex processing_mutex_;
    std::chrono::steady_clock::time_point start_time_;
    
    // 🔥 기존 메서드들 (private에 선언되어 있다면 여기 포함)
    // EvaluateAlarms(), CalculateVirtualPoints(), SaveToRedisLightweight(), SaveToRedisFullData() 등
};

// =============================================================================
// 🔥 인라인 메서드 구현들
// =============================================================================

inline void DataProcessingService::SetRedisClient(std::shared_ptr<RedisClientImpl> client) {
    redis_client_ = client;
}

inline void DataProcessingService::SetInfluxClient(std::shared_ptr<InfluxClient> client) {
    influx_client_ = client;
}

inline size_t DataProcessingService::GetAlarmsEvaluated() const {
    return alarms_evaluated_.load();
}

inline size_t DataProcessingService::GetVirtualPointsCalculated() const {
    return virtual_points_calculated_.load();
}

} // namespace Pipeline
} // namespace PulseOne

#endif // PULSEONE_DATA_PROCESSING_SERVICE_H