#ifndef PULSEONE_EXPORT_TYPES_H
#define PULSEONE_EXPORT_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>

namespace PulseOne {
namespace Export {

// Export 프로토콜 타입 (C# iCos5CSPGateway 기반)
enum class ExportProtocol {
    AWS_IOT_CORE,
    MODBUS_TCP_SERVER,
    OPCUA_SERVER, 
    REST_API_SERVER,
    MQTT_PUBLISHER
};

// Export 연결 설정
struct ExportConnection {
    int id = 0;
    std::string name;
    ExportProtocol protocol;
    bool enabled = true;
    
    // 연결 정보
    std::string host;
    int port = 0;
    std::string username;
    std::string password;
    std::map<std::string, std::string> additional_config;
    
    // 전송 설정 (C# 기반)
    int periodic_interval_sec = 300;  // 5분마다 전송
    int batch_size = 100;
    int timeout_sec = 30;
    
    // 통계
    std::chrono::system_clock::time_point last_export_time;
    size_t successful_exports = 0;
    size_t failed_exports = 0;
};

// Export 데이터 포인트 (PulseOne 구조 기반)
struct ExportDataPoint {
    int source_point_id;
    std::string target_address;
    std::string target_name;
    
    // 현재값 정보 (Structs.h 호환)
    std::string current_value;
    std::string previous_value;
    std::chrono::system_clock::time_point timestamp;
    std::string quality;
    
    // 변환 설정
    double scaling_factor = 1.0;
    double scaling_offset = 0.0;
    std::string unit_conversion;
    
    bool export_on_change_only = true;
    std::chrono::system_clock::time_point last_exported_time;
};

// 알람 Export 이벤트 (C# AlarmMessage 기반)
struct AlarmExportEvent {
    int alarm_rule_id;
    int point_id;
    std::string alarm_type;
    std::string severity;
    std::string message;
    std::chrono::system_clock::time_point occurrence_time;
    
    // AWS 전송용 추가 필드 (C# 기반)
    std::string building_id;
    std::string device_name;
    std::string point_name;
    std::map<std::string, std::string> additional_properties;
};

// Export 통계
struct ExportStats {
    std::atomic<size_t> total_exports{0};
    std::atomic<size_t> successful_exports{0};
    std::atomic<size_t> failed_exports{0};
    std::atomic<size_t> pending_exports{0};
    
    std::chrono::system_clock::time_point last_export_time;
    double avg_export_time_ms = 0.0;
    
    // 프로토콜별 통계
    std::map<ExportProtocol, size_t> exports_by_protocol;
};

} // namespace Export
} // namespace PulseOne

#endif // PULSEONE_EXPORT_TYPES_H
