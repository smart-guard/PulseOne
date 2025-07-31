#ifndef DEVICE_ENTITY_H
#define DEVICE_ENTITY_H

/**
 * @file DeviceEntity.h
 * @brief PulseOne 디바이스 엔티티 (BaseEntity 상속)
 * @author PulseOne Development Team
 * @date 2025-07-27
 * 
 * 🔥 BaseEntity 상속 패턴:
 * - BaseEntity<DeviceEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - UnifiedCommonTypes.h의 DeviceInfo 활용
 * - DataPointEntity와 동일한 네임스페이스 구조
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <vector>
#include <optional>
#include <map>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 디바이스 엔티티 클래스 (BaseEntity 상속)
 */
class DeviceEntity : public BaseEntity<DeviceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자  
    // =======================================================================
    
    /**
     * @brief 기본 생성자
     */
    DeviceEntity();
    
    /**
     * @brief ID로 생성하는 생성자
     * @param device_id 디바이스 ID
     */
    explicit DeviceEntity(int device_id);
    
    /**
     * @brief DeviceInfo 구조체로 생성하는 생성자
     * @param device_info UnifiedCommonTypes.h의 DeviceInfo 구조체
     */
    explicit DeviceEntity(const DeviceInfo& device_info);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~DeviceEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    
    /**
     * @brief DB에서 엔티티 로드
     * @return 성공 시 true
     */
    bool loadFromDatabase() override;
    
    /**
     * @brief DB에 엔티티 저장
     * @return 성공 시 true
     */
    bool saveToDatabase() override;
    
    /**
     * @brief DB에서 엔티티 삭제
     * @return 성공 시 true
     */
    bool deleteFromDatabase() override;
    
    /**
     * @brief DB에 엔티티 업데이트
     * @return 성공 시 true
     */
    bool updateToDatabase() override;
    
    /**
     * @brief JSON으로 직렬화
     * @return JSON 객체
     */
    json toJson() const override;
    
    /**
     * @brief JSON에서 역직렬화
     * @param data JSON 데이터
     * @return 성공 시 true
     */
    bool fromJson(const json& data) override;
    
    /**
     * @brief 엔티티 문자열 표현
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override;
    
    /**
     * @brief 테이블명 조회
     * @return 테이블명
     */
    std::string getTableName() const override { return "devices"; }

    // =======================================================================
    // 기본 속성 접근자
    // =======================================================================
    
    /**
     * @brief 테넌트 ID 조회/설정
     */
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified(); 
    }
    
    /**
     * @brief 사이트 ID 조회/설정
     */
    int getSiteId() const { return site_id_; }
    void setSiteId(int site_id) { 
        site_id_ = site_id; 
        markModified(); 
    }
    
    /**
     * @brief 디바이스 정보 조회/설정
     */
    const DeviceInfo& getDeviceInfo() const { return device_info_; }
    void setDeviceInfo(const DeviceInfo& device_info) { 
        device_info_ = device_info; 
        markModified(); 
    }
    
    /**
     * @brief 디바이스 이름 조회/설정
     */
    const std::string& getName() const { return device_info_.name; }
    void setName(const std::string& name) { 
        device_info_.name = name; 
        markModified(); 
    }
    
    /**
     * @brief 프로토콜 타입 조회/설정
     */
    const std::string& getProtocolType() const { return device_info_.protocol_type; }
    void setProtocolType(const std::string& protocol_type) { 
        device_info_.protocol_type = protocol_type; 
        markModified(); 
    }
    
    /**
     * @brief 연결 문자열 조회/설정
     */
    const std::string& getConnectionString() const { return device_info_.connection_string; }
    void setConnectionString(const std::string& connection_string) { 
        device_info_.connection_string = connection_string; 
        markModified(); 
    }
    
    /**
     * @brief 활성화 상태 조회/설정
     */
    bool isEnabled() const { return device_info_.is_enabled; }
    void setEnabled(bool is_enabled) { 
        device_info_.is_enabled = is_enabled; 
        markModified(); 
    }

    const std::string& getDescription() const { return device_info_.description; }
    void setDescription(const std::string& description) { 
        device_info_.description = description; 
        markModified(); 
    }

    // =======================================================================
    // 고급 기능 (프로토콜별 설정, 데이터포인트 관리 등)
    // =======================================================================
    
    /**
     * @brief 프로토콜별 설정 추출
     */
    json extractModbusConfig() const;
    json extractMqttConfig() const;
    json extractBacnetConfig() const;
    json extractProtocolConfig() const;
    
    /**
     * @brief RDB 저장용 컨텍스트 조회
     * @return RDB 컨텍스트
     */
    json getRDBContext() const;
    
    /**
     * @brief 실시간 데이터 RDB 저장
     * @param values 실시간 값들
     * @return 저장된 개수
     */
    //int saveRealtimeDataToRDB(const std::vector<RealtimeValueEntity>& values);
    int saveRealtimeDataToRDB(const std::vector<json>& values);
    /**
     * @brief 디바이스 데이터포인트 조회
     * @param enabled_only 활성화된 것만 조회할지 여부
     * @return 데이터포인트 목록
     */
    std::vector<DataPoint> getDataPoints(bool enabled_only = true);
    
    /**
     * @brief 알람 설정 조회
     * @return 알람 설정 목록
     */
    std::vector<json> getAlarmConfigs();
    
    /**
     * @brief 디바이스 상태 업데이트
     * @param status 새 상태
     * @param error_message 에러 메시지 (옵션)
     * @return 성공 시 true
     */
    bool updateDeviceStatus(const std::string& status, const std::string& error_message = "");
    
    /**
     * @brief 유효성 검사
     */
    bool isValid() const override;

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 관계 정보
    int tenant_id_;                          // 외래키 (tenants.id)
    int site_id_;                            // 외래키 (sites.id)
    
    // 디바이스 정보 (UnifiedCommonTypes.h의 DeviceInfo)
    DeviceInfo device_info_;
    
    // 캐시된 관련 데이터
    std::optional<std::vector<DataPoint>> cached_data_points_;
    std::optional<std::vector<json>> cached_alarm_configs_;
    bool data_points_loaded_;
    bool alarm_configs_loaded_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 DeviceInfo로 매핑
     */
    DeviceInfo mapRowToDeviceInfo(const std::map<std::string, std::string>& row);
    
    /**
     * @brief INSERT SQL 쿼리 생성
     */
    std::string buildInsertSQL() const;
    
    /**
     * @brief UPDATE SQL 쿼리 생성
     */
    std::string buildUpdateSQL() const;
    
    /**
     * @brief 프로토콜 설정 파싱
     */
    json parseProtocolConfig(const std::string& protocol_type, const std::string& config_json) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_ENTITY_H