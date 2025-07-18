/**
 * @file DeviceDataAccess.h
 * @brief 디바이스 관련 데이터베이스 액세스 레이어
 * @details PostgreSQL 기반 멀티테넌트 스키마에서 디바이스, 데이터포인트, 알람 데이터 관리
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 2.0.0
 */

#ifndef DATABASE_DEVICE_DATA_ACCESS_H
#define DATABASE_DEVICE_DATA_ACCESS_H

#include "Database/DatabaseManager.h"
#include "Drivers/CommonTypes.h"
#include "Utils/LogManager.h"
#include <vector>
#include <optional>
#include <map>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {

using json = nlohmann::json;
using UUID = Drivers::UUID;
using Timestamp = std::chrono::system_clock::time_point;

// =============================================================================
// 데이터베이스 엔티티 구조체들
// =============================================================================

/**
 * @brief 테넌트 정보
 */
struct TenantInfo {
    UUID tenant_id;
    std::string company_name;
    std::string company_code;
    std::string subscription_status;
    bool is_active;
    Timestamp created_at;
    
    // 메타데이터
    json settings;
    json features;
    
    TenantInfo() : is_active(false), created_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 공장 정보
 */
struct FactoryInfo {
    UUID factory_id;
    std::string name;
    std::string code;
    std::string description;
    std::string location;
    std::string timezone;
    
    // 연락처 정보
    std::string manager_name;
    std::string manager_email;
    std::string manager_phone;
    
    // Edge 서버 정보
    std::optional<UUID> edge_server_id;
    
    bool is_active;
    Timestamp created_at;
    Timestamp updated_at;
    
    FactoryInfo() : is_active(true), created_at(std::chrono::system_clock::now()), updated_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 디바이스 정보
 */
struct DeviceInfo {
    UUID device_id;
    UUID factory_id;
    std::string name;
    std::string code;
    std::string description;
    std::string protocol_type;
    
    // 연결 설정
    std::string connection_string;
    json connection_config;
    
    // 상태 정보
    bool is_enabled;
    std::string status;
    Timestamp last_seen;
    
    // 폴링 설정
    int polling_interval_ms;
    int timeout_ms;
    int retry_count;
    
    // 그룹 및 태그
    std::optional<UUID> device_group_id;
    std::vector<std::string> tags;
    
    // 메타데이터
    json metadata;
    
    Timestamp created_at;
    Timestamp updated_at;
    
    DeviceInfo() 
        : is_enabled(true)
        , status("disconnected")
        , last_seen(std::chrono::system_clock::now())
        , polling_interval_ms(1000)
        , timeout_ms(5000)
        , retry_count(3)
        , created_at(std::chrono::system_clock::now())
        , updated_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 데이터 포인트 정보
 */
struct DataPointInfo {
    UUID point_id;
    UUID device_id;
    std::string name;
    std::string description;
    std::string address;
    std::string data_type;
    
    // 스케일링 설정
    double scaling_factor;
    double scaling_offset;
    double min_value;
    double max_value;
    std::string unit;
    
    // 설정
    bool is_enabled;
    bool is_writable;
    int scan_rate_ms;
    double deadband;
    
    // 로깅 설정
    bool log_enabled;
    int log_interval_ms;
    double log_deadband;
    
    // 태그 및 메타데이터
    std::vector<std::string> tags;
    json metadata;
    
    Timestamp created_at;
    Timestamp updated_at;
    
    DataPointInfo()
        : scaling_factor(1.0)
        , scaling_offset(0.0)
        , min_value(-999999.0)
        , max_value(999999.0)
        , is_enabled(true)
        , is_writable(false)
        , scan_rate_ms(0)
        , deadband(0.0)
        , log_enabled(true)
        , log_interval_ms(0)
        , log_deadband(0.0)
        , created_at(std::chrono::system_clock::now())
        , updated_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 현재 값 정보
 */
struct CurrentValueInfo {
    UUID point_id;
    double value;
    std::string quality;
    Timestamp timestamp;
    
    CurrentValueInfo() 
        : value(0.0)
        , quality("good")
        , timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief 알람 설정 정보
 */
struct AlarmConfigInfo {
    UUID alarm_id;
    UUID factory_id;
    std::string source_type;  // data_point, virtual_point
    UUID source_id;
    std::string alarm_type;   // high, low, deviation, discrete
    
    // 알람 설정
    bool enabled;
    std::string priority;     // low, medium, high, critical
    bool auto_acknowledge;
    
    // 아날로그 알람 설정
    std::optional<double> high_limit;
    std::optional<double> low_limit;
    std::optional<double> deadband;
    
    // 디지털 알람 설정
    std::optional<double> trigger_value;
    
    // 메시지 설정
    std::string message_template;
    
    // 알림 설정
    bool email_enabled;
    std::vector<std::string> email_recipients;
    bool sms_enabled;
    std::vector<std::string> sms_recipients;
    
    UUID created_by;
    Timestamp created_at;
    Timestamp updated_at;
    
    AlarmConfigInfo()
        : enabled(true)
        , priority("medium")
        , auto_acknowledge(false)
        , email_enabled(false)
        , sms_enabled(false)
        , created_at(std::chrono::system_clock::now())
        , updated_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 활성 알람 정보
 */
struct ActiveAlarmInfo {
    UUID alarm_instance_id;
    UUID alarm_config_id;
    double triggered_value;
    std::string message;
    Timestamp triggered_at;
    
    // 응답 정보
    std::optional<Timestamp> acknowledged_at;
    std::optional<UUID> acknowledged_by;
    std::optional<std::string> acknowledgment_comment;
    
    bool is_active;
    
    ActiveAlarmInfo()
        : triggered_value(0.0)
        , triggered_at(std::chrono::system_clock::now())
        , is_active(true) {}
};

/**
 * @brief 가상 포인트 정보
 */
struct VirtualPointInfo {
    UUID virtual_point_id;
    UUID factory_id;
    std::string name;
    std::string description;
    std::string formula;
    std::string unit;
    
    // 계산 설정
    bool is_enabled;
    int calculation_interval_ms;
    
    // 태그 및 메타데이터
    std::vector<std::string> tags;
    json metadata;
    
    Timestamp created_at;
    Timestamp updated_at;
    
    VirtualPointInfo()
        : is_enabled(true)
        , calculation_interval_ms(1000)
        , created_at(std::chrono::system_clock::now())
        , updated_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief 데이터 히스토리 정보
 */
struct DataHistoryInfo {
    UUID point_id;
    double value;
    std::string quality;
    Timestamp timestamp;
    
    DataHistoryInfo()
        : value(0.0)
        , quality("good")
        , timestamp(std::chrono::system_clock::now()) {}
};

// =============================================================================
// DeviceDataAccess 클래스 정의
// =============================================================================

/**
 * @brief 디바이스 데이터 액세스 레이어
 * @details PostgreSQL 멀티테넌트 스키마를 기반으로 디바이스 관련 모든 데이터 액세스 제공
 * 
 * 주요 기능:
 * - 테넌트별 스키마 자동 선택
 * - 디바이스 및 데이터 포인트 CRUD
 * - 실시간 데이터 읽기/쓰기
 * - 알람 설정 및 이력 관리
 * - 가상 포인트 관리
 * - 히스토리 데이터 조회/저장
 * - 트랜잭션 지원
 */
class DeviceDataAccess {
public:
    /**
     * @brief 생성자
     * @param db_manager 데이터베이스 매니저
     * @param tenant_code 테넌트 코드 (스키마 선택용)
     */
    DeviceDataAccess(std::shared_ptr<DatabaseManager> db_manager, 
                     const std::string& tenant_code);
    
    /**
     * @brief 소멸자
     */
    ~DeviceDataAccess();
    
    // 복사/이동 방지
    DeviceDataAccess(const DeviceDataAccess&) = delete;
    DeviceDataAccess& operator=(const DeviceDataAccess&) = delete;
    
    // =================================================================
    // 테넌트 및 공장 관리
    // =================================================================
    
    /**
     * @brief 테넌트 정보 조회
     * @param tenant_code 테넌트 코드
     * @return 테넌트 정보
     */
    std::optional<TenantInfo> GetTenantInfo(const std::string& tenant_code);
    
    /**
     * @brief 공장 목록 조회
     * @param active_only 활성 공장만 조회 여부
     * @return 공장 목록
     */
    std::vector<FactoryInfo> GetFactories(bool active_only = true);
    
    /**
     * @brief 공장 정보 조회
     * @param factory_id 공장 ID
     * @return 공장 정보
     */
    std::optional<FactoryInfo> GetFactory(const UUID& factory_id);
    
    /**
     * @brief 공장 생성
     * @param factory 공장 정보
     * @return 성공 시 true
     */
    bool CreateFactory(const FactoryInfo& factory);
    
    /**
     * @brief 공장 업데이트
     * @param factory 공장 정보
     * @return 성공 시 true
     */
    bool UpdateFactory(const FactoryInfo& factory);
    
    // =================================================================
    // 디바이스 관리
    // =================================================================
    
    /**
     * @brief 디바이스 목록 조회
     * @param factory_id 공장 ID (빈 값이면 전체)
     * @param enabled_only 활성 디바이스만 조회 여부
     * @return 디바이스 목록
     */
    std::vector<DeviceInfo> GetDevices(const UUID& factory_id = UUID{}, bool enabled_only = true);
    
    /**
     * @brief 디바이스 정보 조회
     * @param device_id 디바이스 ID
     * @return 디바이스 정보
     */
    std::optional<DeviceInfo> GetDevice(const UUID& device_id);
    
    /**
     * @brief 디바이스 생성
     * @param device 디바이스 정보
     * @return 성공 시 true
     */
    bool CreateDevice(const DeviceInfo& device);
    
    /**
     * @brief 디바이스 업데이트
     * @param device 디바이스 정보
     * @return 성공 시 true
     */
    bool UpdateDevice(const DeviceInfo& device);
    
    /**
     * @brief 디바이스 삭제
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool DeleteDevice(const UUID& device_id);
    
    /**
     * @brief 디바이스 상태 업데이트
     * @param device_id 디바이스 ID
     * @param status 상태
     * @param last_seen 마지막 확인 시간
     * @return 성공 시 true
     */
    bool UpdateDeviceStatus(const UUID& device_id, const std::string& status, const Timestamp& last_seen);
    
    // =================================================================
    // 데이터 포인트 관리
    // =================================================================
    
    /**
     * @brief 데이터 포인트 목록 조회
     * @param device_id 디바이스 ID
     * @param enabled_only 활성 포인트만 조회 여부
     * @return 데이터 포인트 목록
     */
    std::vector<DataPointInfo> GetDataPoints(const UUID& device_id, bool enabled_only = true);
    
    /**
     * @brief 데이터 포인트 정보 조회
     * @param point_id 포인트 ID
     * @return 데이터 포인트 정보
     */
    std::optional<DataPointInfo> GetDataPoint(const UUID& point_id);
    
    /**
     * @brief 데이터 포인트 생성
     * @param point 데이터 포인트 정보
     * @return 성공 시 true
     */
    bool CreateDataPoint(const DataPointInfo& point);
    
    /**
     * @brief 데이터 포인트 업데이트
     * @param point 데이터 포인트 정보
     * @return 성공 시 true
     */
    bool UpdateDataPoint(const DataPointInfo& point);
    
    /**
     * @brief 데이터 포인트 삭제
     * @param point_id 포인트 ID
     * @return 성공 시 true
     */
    bool DeleteDataPoint(const UUID& point_id);
    
    // =================================================================
    // 현재 값 관리
    // =================================================================
    
    /**
     * @brief 현재 값 조회
     * @param point_id 포인트 ID
     * @return 현재 값 정보
     */
    std::optional<CurrentValueInfo> GetCurrentValue(const UUID& point_id);
    
    /**
     * @brief 현재 값 목록 조회
     * @param point_ids 포인트 ID 목록
     * @return 현재 값 목록
     */
    std::vector<CurrentValueInfo> GetCurrentValues(const std::vector<UUID>& point_ids);
    
    /**
     * @brief 현재 값 업데이트
     * @param point_id 포인트 ID
     * @param value 값
     * @param quality 품질
     * @param timestamp 타임스탬프
     * @return 성공 시 true
     */
    bool UpdateCurrentValue(const UUID& point_id, double value, const std::string& quality, const Timestamp& timestamp);
    
    /**
     * @brief 현재 값 일괄 업데이트
     * @param values 현재 값 목록
     * @return 성공 시 true
     */
    bool UpdateCurrentValues(const std::vector<CurrentValueInfo>& values);
    
    // =================================================================
    // 알람 관리
    // =================================================================
    
    /**
     * @brief 알람 설정 목록 조회
     * @param factory_id 공장 ID (빈 값이면 전체)
     * @param enabled_only 활성 알람만 조회 여부
     * @return 알람 설정 목록
     */
    std::vector<AlarmConfigInfo> GetAlarmConfigs(const UUID& factory_id = UUID{}, bool enabled_only = true);
    
    /**
     * @brief 활성 알람 목록 조회
     * @param factory_id 공장 ID (빈 값이면 전체)
     * @return 활성 알람 목록
     */
    std::vector<ActiveAlarmInfo> GetActiveAlarms(const UUID& factory_id = UUID{});
    
    /**
     * @brief 알람 설정 생성
     * @param alarm_config 알람 설정 정보
     * @return 성공 시 true
     */
    bool CreateAlarmConfig(const AlarmConfigInfo& alarm_config);
    
    /**
     * @brief 알람 발생 기록
     * @param alarm_config_id 알람 설정 ID
     * @param triggered_value 트리거 값
     * @param message 메시지
     * @return 성공 시 알람 인스턴스 ID
     */
    std::optional<UUID> TriggerAlarm(const UUID& alarm_config_id, double triggered_value, const std::string& message);
    
    /**
     * @brief 알람 확인 처리
     * @param alarm_instance_id 알람 인스턴스 ID
     * @param acknowledged_by 확인자
     * @param comment 확인 코멘트
     * @return 성공 시 true
     */
    bool AcknowledgeAlarm(const UUID& alarm_instance_id, const UUID& acknowledged_by, const std::string& comment = "");
    
    /**
     * @brief 알람 해제 처리
     * @param alarm_instance_id 알람 인스턴스 ID
     * @return 성공 시 true
     */
    bool ClearAlarm(const UUID& alarm_instance_id);
    
    // =================================================================
    // 가상 포인트 관리
    // =================================================================
    
    /**
     * @brief 가상 포인트 목록 조회
     * @param factory_id 공장 ID
     * @param enabled_only 활성 포인트만 조회 여부
     * @return 가상 포인트 목록
     */
    std::vector<VirtualPointInfo> GetVirtualPoints(const UUID& factory_id, bool enabled_only = true);
    
    /**
     * @brief 가상 포인트 생성
     * @param virtual_point 가상 포인트 정보
     * @return 성공 시 true
     */
    bool CreateVirtualPoint(const VirtualPointInfo& virtual_point);
    
    // =================================================================
    // 히스토리 데이터 관리
    // =================================================================
    
    /**
     * @brief 히스토리 데이터 저장
     * @param point_id 포인트 ID
     * @param value 값
     * @param quality 품질
     * @param timestamp 타임스탬프
     * @return 성공 시 true
     */
    bool SaveHistoryData(const UUID& point_id, double value, const std::string& quality, const Timestamp& timestamp);
    
    /**
     * @brief 히스토리 데이터 일괄 저장
     * @param history_data 히스토리 데이터 목록
     * @return 성공 시 true
     */
    bool SaveHistoryDataBatch(const std::vector<DataHistoryInfo>& history_data);
    
    /**
     * @brief 히스토리 데이터 조회
     * @param point_id 포인트 ID
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @param limit 최대 레코드 수
     * @return 히스토리 데이터 목록
     */
    std::vector<DataHistoryInfo> GetHistoryData(const UUID& point_id, 
                                               const Timestamp& start_time, 
                                               const Timestamp& end_time, 
                                               int limit = 1000);
    
    // =================================================================
    // 통계 및 집계
    // =================================================================
    
    /**
     * @brief 디바이스 통계 조회
     * @param device_id 디바이스 ID
     * @return 통계 정보 JSON
     */
    json GetDeviceStatistics(const UUID& device_id);
    
    /**
     * @brief 공장 알람 통계 조회
     * @param factory_id 공장 ID
     * @return 알람 통계 JSON
     */
    json GetAlarmStatistics(const UUID& factory_id);
    
    // =================================================================
    // 트랜잭션 지원
    // =================================================================
    
    /**
     * @brief 트랜잭션 시작
     * @return 성공 시 true
     */
    bool BeginTransaction();
    
    /**
     * @brief 트랜잭션 커밋
     * @return 성공 시 true
     */
    bool CommitTransaction();
    
    /**
     * @brief 트랜잭션 롤백
     * @return 성공 시 true
     */
    bool RollbackTransaction();

private:
    // =============================================================================
    // 내부 데이터 멤버
    // =============================================================================
    
    std::shared_ptr<DatabaseManager> db_manager_;   ///< 데이터베이스 매니저
    std::string tenant_code_;                       ///< 테넌트 코드
    std::string schema_name_;                       ///< 스키마 이름 (tenant_xxx)
    LogManager& logger_;                            ///< 로거
    
    // =============================================================================
    // 내부 유틸리티 함수들
    // =============================================================================
    
    /**
     * @brief 스키마명 생성
     * @param tenant_code 테넌트 코드
     * @return 스키마명
     */
    std::string GetSchemaName(const std::string& tenant_code);
    
    /**
     * @brief 테이블명 생성 (스키마 포함)
     * @param table_name 테이블명
     * @return 스키마.테이블명
     */
    std::string GetTableName(const std::string& table_name);
    
    /**
     * @brief UUID를 문자열로 변환
     * @param uuid UUID
     * @return UUID 문자열
     */
    std::string UUIDToString(const UUID& uuid);
    
    /**
     * @brief 문자열을 UUID로 변환
     * @param uuid_str UUID 문자열
     * @return UUID
     */
    UUID StringToUUID(const std::string& uuid_str);
    
    /**
     * @brief Timestamp를 문자열로 변환
     * @param timestamp 타임스탬프
     * @return ISO 8601 형식 문자열
     */
    std::string TimestampToString(const Timestamp& timestamp);
    
    /**
     * @brief 문자열을 Timestamp로 변환
     * @param timestamp_str 타임스탬프 문자열
     * @return 타임스탬프
     */
    Timestamp StringToTimestamp(const std::string& timestamp_str);
    
    /**
     * @brief JSON 배열을 문자열 벡터로 변환
     * @param json_array JSON 배열
     * @return 문자열 벡터
     */
    std::vector<std::string> JsonArrayToStringVector(const json& json_array);
    
    /**
     * @brief 문자열 벡터를 JSON 배열로 변환
     * @param string_vector 문자열 벡터
     * @return JSON 배열
     */
    json StringVectorToJsonArray(const std::vector<std::string>& string_vector);
};

} // namespace Database
} // namespace PulseOne

#endif // DATABASE_DEVICE_DATA_ACCESS_H