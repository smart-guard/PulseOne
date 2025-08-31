//=============================================================================
// collector/include/Storage/BackendFormat.h
// 
// 목적: Backend API와 완전히 호환되는 데이터 구조체들 정의
// 특징:
//   - Backend realtime.js, alarmController.js가 기대하는 정확한 구조
//   - AlarmEventData, DevicePointData 등 핵심 구조체들
//   - nlohmann::json 직접 변환 지원
//=============================================================================

#ifndef BACKEND_FORMAT_H
#define BACKEND_FORMAT_H

#include <string>
#include <nlohmann/json.hpp>
#include <cstdint>

namespace PulseOne {
namespace Storage {
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
            j["changed"] = changed;
            return j;
        }
    };

    /**
     * @brief Point Latest 데이터 (Backend가 최신값으로 읽는 형식)
     * Redis 키: point:{point_id}:latest
     */
    struct PointLatestData {
        int point_id;                   // 포인트 ID
        std::string device_id;          // 디바이스 ID
        std::string point_name;         // 포인트명 (필요!)
        std::string value;              // 값 (문자열)
        int64_t timestamp;              // Unix timestamp (milliseconds)
        std::string quality;            // 품질 (string 타입으로 변경!)
        std::string data_type;          // 데이터 타입 (필요!)
        std::string unit;               // 단위 (필요!)
        bool changed = false;           // 값 변경 여부 (필요!)
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["point_id"] = point_id;
            j["device_id"] = device_id;
            j["point_name"] = point_name;       // ← 있어야 함!
            j["value"] = value;
            j["timestamp"] = timestamp;
            j["quality"] = quality;
            j["data_type"] = data_type;         // ← 있어야 함!
            j["unit"] = unit;                   // ← 있어야 함!
            if (changed) j["changed"] = true;   // ← 있어야 함!
            return j;
        }
    };

    /**
     * @brief 알람 이벤트 데이터 (Backend Pub/Sub 및 API 호환)
     * Redis 키: alarm:active:{rule_id}
     * Pub/Sub 채널: alarms:all, tenant:{id}:alarms, alarms:{severity}
     */
    struct AlarmEventData {
        std::string type = "alarm_event";
        std::string occurrence_id;
        int rule_id;
        int tenant_id;  
        int point_id;
        std::string device_id;
        std::string message;
        std::string severity;        // ← int에서 std::string으로 변경
        std::string state;           // ← int에서 std::string으로 변경  
        int64_t timestamp;
        std::string source_name;
        std::string location;
        std::string trigger_value;
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["type"] = type;
            j["occurrence_id"] = occurrence_id;
            j["rule_id"] = rule_id;
            j["tenant_id"] = tenant_id;
            j["point_id"] = point_id;
            j["device_id"] = device_id;
            j["message"] = message;
            j["severity"] = severity;    // 문자열로 직접 할당
            j["state"] = state;          // 문자열로 직접 할당
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

    /**
     * @brief Worker 상태 데이터
     * Redis 키: worker:status:{device_id}
     */
    struct WorkerStatusData {
        std::string device_id;          // 디바이스 ID
        std::string status;             // 상태 ("initialized", "running", "stopped", "error")
        int64_t timestamp;              // Unix timestamp (milliseconds)
        nlohmann::json metadata;        // 추가 메타데이터
        
        nlohmann::json toJson() const {
            nlohmann::json j;
            j["device_id"] = device_id;
            j["status"] = status;
            j["timestamp"] = timestamp;
            j["metadata"] = metadata;
            return j;
        }
    };

} // namespace BackendFormat
} // namespace Storage
} // namespace PulseOne

#endif // BACKEND_FORMAT_H