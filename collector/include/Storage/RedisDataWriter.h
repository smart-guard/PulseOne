#include "Utils/LogManager.h"
//=============================================================================
// collector/include/Storage/RedisDataWriter.h
// 
// 목적: Backend 완전 호환 Redis 데이터 저장 클래스
// 특징:
//   - Backend realtime.js가 기대하는 정확한 JSON 구조 구현
//   - device:{id}:{point_name} 키 패턴 완벽 지원
//   - Worker 초기화 데이터 저장 지원
//=============================================================================

#ifndef REDIS_DATA_WRITER_H
#define REDIS_DATA_WRITER_H

#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Client/RedisClient.h"
#include "Utils/LogManager.h"
#include "nlohmann/json.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace PulseOne {
namespace Storage {

/**
 * @brief Backend 완전 호환 Redis 데이터 구조체들
 */
namespace BackendFormat {

    /**
     * @brief Device Point 데이터 (Backend realtime.js가 읽는 형식)
     * Redis 키: device:{device_id}:{point_name}
     * 
     * Backend의 processPointKey() 함수가 기대하는 정확한 구조
     */
    struct DevicePointData {
        int point_id;                   // 포인트 ID
        std::string device_id;          // "1", "2", "3" (숫자 문자열)
        std::string device_name;        // "Device 1", "Device 2"
        std::string point_name;         // "temperature_sensor_01", "pressure_sensor_01"
        std::string value;              // "25.4", "true", "150" (항상 문자열)
        int64_t timestamp;              // Unix timestamp (milliseconds)
        std::string quality;            // "good", "bad", "uncertain", "comm_failure", "last_known"
        std::string data_type;          // "boolean", "integer", "number", "string"
        std::string unit;               // "°C", "bar", "L/min", ""
        bool changed = false;           // 값 변경 여부
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["point_id"] = point_id;
            j["device_id"] = device_id;
            j["device_name"] = device_name;
            j["point_name"] = point_name;
            j["value"] = value;
            j["timestamp"] = timestamp;
            j["quality"] = quality;
            j["data_type"] = data_type;
            j["unit"] = unit;
            if (changed) j["changed"] = true;
            return j;
        }
    };

    /**
     * @brief Point Latest 데이터 (legacy 지원)
     * Redis 키: point:{point_id}:latest
     */
    struct PointLatestData {
        std::string device_id;          // 디바이스 ID
        int point_id;                   // 포인트 ID
        std::string value;              // 값 (문자열)
        int64_t timestamp;              // Unix timestamp (milliseconds)
        int quality;                    // 품질 코드 (숫자)
        bool changed = false;           // 값 변경 여부
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["device_id"] = device_id;
            j["point_id"] = point_id;
            j["value"] = value;
            j["timestamp"] = timestamp;
            j["quality"] = quality;
            if (changed) j["changed"] = true;
            return j;
        }
    };

    /**
     * @brief Alarm 이벤트 데이터
     * Redis 채널: alarms:all, tenant:{id}:alarms, device:{id}:alarms
     * Redis 키: alarm:active:{rule_id}
     */
    struct AlarmEventData {
        std::string type = "alarm_event";
        std::string occurrence_id;      // 발생 ID
        int rule_id;                    // 규칙 ID
        int tenant_id;                  // 테넌트 ID
        int point_id;                   // 포인트 ID
        std::string device_id;          // 디바이스 ID
        std::string message;            // 알람 메시지
        int severity;                   // 0=INFO, 1=LOW, 2=MEDIUM, 3=HIGH, 4=CRITICAL
        int state;                      // 0=INACTIVE, 1=ACTIVE, 2=ACKNOWLEDGED, 3=CLEARED
        int64_t timestamp;              // Unix timestamp (milliseconds)
        std::string source_name;        // 소스 이름
        std::string location;           // 위치
        std::string trigger_value;      // 트리거 값 (문자열)
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["type"] = type;
            j["occurrence_id"] = occurrence_id;
            j["rule_id"] = rule_id;
            j["tenant_id"] = tenant_id;
            j["point_id"] = point_id;
            j["device_id"] = device_id;
            j["message"] = message;
            j["severity"] = severity;
            j["state"] = state;
            j["timestamp"] = timestamp;
            j["source_name"] = source_name;
            j["location"] = location;
            j["trigger_value"] = trigger_value;
            return j;
        }
    };

    /**
     * @brief Device Full State 데이터
     * Redis 키: device:full:{device_id}
     */
    struct DeviceFullData {
        std::string device_id;          // 디바이스 ID
        int64_t timestamp;              // Unix timestamp (milliseconds)
        nlohmann::json points;          // 포인트 배열
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["device_id"] = device_id;
            j["timestamp"] = timestamp;
            j["points"] = points;
            return j;
        }
    };

} // namespace BackendFormat

/**
 * @brief Backend 완전 호환 Redis 데이터 저장 클래스
 */
class RedisDataWriter {
public:
    // ==========================================================================
    // 생성자 및 초기화
    // ==========================================================================
    
    explicit RedisDataWriter(std::shared_ptr<RedisClient> redis_client = nullptr);
    ~RedisDataWriter() = default;
    
    void SetRedisClient(std::shared_ptr<RedisClient> redis_client);
    bool IsConnected() const;

    // ==========================================================================
    // Backend 완전 호환 저장 메서드들 (메인 API)
    // ==========================================================================
    
    /**
     * @brief DeviceDataMessage를 Backend 호환 형식으로 저장
     * device:{device_id}:{point_name} + point:{point_id}:latest 동시 저장
     * @param message 디바이스 메시지
     * @return 성공한 포인트 수
     */
    size_t SaveDeviceMessage(const Structs::DeviceDataMessage& message);
    
    /**
     * @brief 개별 포인트를 Backend 호환 형식으로 저장
     * @param point 타임스탬프 값
     * @param device_id 디바이스 ID ("device_001" 형식)
     * @return 성공 여부
     */
    bool SaveSinglePoint(const Structs::TimestampedValue& point, const std::string& device_id);
    
    /**
     * @brief 알람 이벤트를 Redis에 발행 및 저장
     * @param alarm_data 알람 데이터
     * @return 성공 여부
     */
    bool PublishAlarmEvent(const BackendFormat::AlarmEventData& alarm_data);
    bool StoreVirtualPointToRedis(const Structs::TimestampedValue& virtual_point_data);
    // ==========================================================================
    // Worker 초기화 전용 메서드들
    // ==========================================================================
    
    /**
     * @brief Worker 초기화 완료 시 현재 데이터 저장
     * @param device_id 디바이스 ID ("device_001")
     * @param current_values DB에서 읽은 현재 값들
     * @return 성공한 포인트 수
     */
    size_t SaveWorkerInitialData(
        const std::string& device_id, 
        const std::vector<Structs::TimestampedValue>& current_values);
    
    /**
     * @brief Worker 상태 정보 저장
     * @param device_id 디바이스 ID
     * @param status 상태 ("initialized", "running", "stopped", "error")
     * @param metadata 추가 메타데이터
     * @return 성공 여부
     */
    bool SaveWorkerStatus(
        const std::string& device_id,
        const std::string& status,
        const nlohmann::json& metadata = {});

    // ==========================================================================
    // 통계 및 상태
    // ==========================================================================
    
    struct WriteStats {
        std::atomic<uint64_t> total_writes{0};
        std::atomic<uint64_t> successful_writes{0};
        std::atomic<uint64_t> failed_writes{0};
        std::atomic<uint64_t> device_point_writes{0};     // device:{id}:{name} 저장
        std::atomic<uint64_t> point_latest_writes{0};     // point:{id}:latest 저장
        std::atomic<uint64_t> alarm_publishes{0};         // 알람 발행
        std::atomic<uint64_t> worker_init_writes{0};      // Worker 초기화 저장
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["total_writes"] = total_writes.load();
            j["successful_writes"] = successful_writes.load();
            j["failed_writes"] = failed_writes.load();
            j["device_point_writes"] = device_point_writes.load();
            j["point_latest_writes"] = point_latest_writes.load();
            j["alarm_publishes"] = alarm_publishes.load();
            j["worker_init_writes"] = worker_init_writes.load();
            j["success_rate"] = total_writes.load() > 0 ? 
                (double)successful_writes.load() / total_writes.load() * 100.0 : 0.0;
            return j;
        }
    };
    
    WriteStats GetStatistics() const;
    void ResetStatistics();
    nlohmann::json GetStatus() const;

private:
    // ==========================================================================
    // 내부 데이터 변환 메서드들
    // ==========================================================================
    
    /**
     * @brief TimestampedValue를 Backend 호환 DevicePointData로 변환
     */
    BackendFormat::DevicePointData ConvertToDevicePointData(
        const Structs::TimestampedValue& point, 
        const std::string& device_id) const;
    
    /**
     * @brief TimestampedValue를 Backend 호환 PointLatestData로 변환
     */
    BackendFormat::PointLatestData ConvertToPointLatestData(
        const Structs::TimestampedValue& point, 
        const std::string& device_id) const;
    
    /**
     * @brief DataValue를 문자열로 변환
     */
    std::string ConvertValueToString(const Structs::DataValue& value) const;
    
    /**
     * @brief DataQuality를 Backend 호환 문자열로 변환
     */
    std::string ConvertQualityToString(const Structs::DataQuality& quality) const;
    
    /**
     * @brief DataValue 타입을 문자열로 변환
     */
    std::string GetDataTypeString(const Structs::DataValue& value) const;

    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    /**
     * @brief "device_001" -> "1" 변환
     */
    std::string ExtractDeviceNumber(const std::string& device_id) const;
    
    /**
     * @brief 포인트 ID에서 포인트 이름 가져오기
     */
    std::string GetPointName(int point_id) const;
    
    /**
     * @brief 포인트 ID에서 단위 문자열 가져오기
     */
    std::string GetUnit(int point_id) const;
    
    /**
     * @brief 포인트 ID에서 디바이스 ID 추론
     */
    std::string GetDeviceIdForPoint(int point_id) const;
    
    /**
     * @brief 에러 처리 및 통계 업데이트
     */
    void HandleError(const std::string& context, const std::string& error_message);

    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    /// Redis 클라이언트
    std::shared_ptr<RedisClient> redis_client_;
    
    /// 통계
    WriteStats stats_;
    
    /// 스레드 안전성을 위한 뮤텍스
    mutable std::mutex redis_mutex_;
    
    /// 로거 참조
    
    
    /// 포인트 정보 캐시 (성능 최적화)
    mutable std::unordered_map<int, std::string> point_name_cache_;
    mutable std::unordered_map<int, std::string> unit_cache_;
    mutable std::mutex cache_mutex_;
};

} // namespace Storage
} // namespace PulseOne

#endif // REDIS_DATA_WRITER_H