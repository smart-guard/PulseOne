#ifndef DATA_POINT_ENTITY_H
#define DATA_POINT_ENTITY_H

/**
 * @file DataPointEntity.h
 * @brief PulseOne 데이터포인트 엔티티 (BaseEntity 상속)
 * @author PulseOne Development Team
 * @date 2025-07-27
 * 
 * 🔥 DeviceEntity와 동일한 패턴:
 * - BaseEntity<DataPointEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - UnifiedCommonTypes.h의 DataPoint 활용
 * - 같은 네임스페이스 구조 (PulseOne::Database::Entities)
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
 * @brief 데이터포인트 엔티티 클래스 (BaseEntity 상속)
 */
class DataPointEntity : public BaseEntity<DataPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자  
    // =======================================================================
    
    /**
     * @brief 기본 생성자
     */
    DataPointEntity();
    
    /**
     * @brief ID로 생성하는 생성자
     * @param point_id 데이터포인트 ID
     */
    explicit DataPointEntity(int point_id);
    
    /**
     * @brief DataPoint 구조체로 생성하는 생성자
     * @param data_point UnifiedCommonTypes.h의 DataPoint 구조체
     */
    explicit DataPointEntity(const DataPoint& data_point);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~DataPointEntity() = default;

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
     * @param j JSON 객체
     * @return 성공 시 true
     */
    bool fromJson(const json& j) override;
    
    /**
     * @brief 엔티티 문자열 표현
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override;
    
    /**
     * @brief 테이블명 조회
     * @return 테이블명
     */
    std::string getTableName() const override;

    // =======================================================================
    // 기본 속성 접근자 (DeviceEntity와 동일 패턴)
    // =======================================================================
    
    /**
     * @brief 디바이스 ID 조회
     */
    int getDeviceId() const { return device_id_; }
    void setDeviceId(int device_id) { 
        device_id_ = device_id; 
        markModified(); 
    }
    
    /**
     * @brief 이름 조회/설정
     */
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { 
        name_ = name; 
        markModified(); 
    }
    
    /**
     * @brief 설명 조회/설정
     */
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified(); 
    }
    
    /**
     * @brief 주소 조회/설정
     */
    int getAddress() const { return address_; }
    void setAddress(int address) { 
        address_ = address; 
        markModified(); 
    }
    
    /**
     * @brief 데이터 타입 조회/설정
     */
    const std::string& getDataType() const { return data_type_; }
    void setDataType(const std::string& data_type) { 
        data_type_ = data_type; 
        markModified(); 
    }
    
    /**
     * @brief 접근 모드 조회/설정
     */
    const std::string& getAccessMode() const { return access_mode_; }
    void setAccessMode(const std::string& access_mode) { 
        access_mode_ = access_mode; 
        markModified(); 
    }
    
    /**
     * @brief 활성화 상태 조회/설정
     */
    bool isEnabled() const { return is_enabled_; }
    void setEnabled(bool is_enabled) { 
        is_enabled_ = is_enabled; 
        markModified(); 
    }

    // =======================================================================
    // 엔지니어링 정보 접근자
    // =======================================================================
    
    /**
     * @brief 단위 조회/설정
     */
    const std::string& getUnit() const { return unit_; }
    void setUnit(const std::string& unit) { 
        unit_ = unit; 
        markModified(); 
    }
    
    /**
     * @brief 스케일링 팩터 조회/설정
     */
    double getScalingFactor() const { return scaling_factor_; }
    void setScalingFactor(double scaling_factor) { 
        scaling_factor_ = scaling_factor; 
        markModified(); 
    }
    
    /**
     * @brief 스케일링 오프셋 조회/설정
     */
    double getScalingOffset() const { return scaling_offset_; }
    void setScalingOffset(double scaling_offset) { 
        scaling_offset_ = scaling_offset; 
        markModified(); 
    }
    
    /**
     * @brief 최솟값 조회/설정
     */
    double getMinValue() const { return min_value_; }
    void setMinValue(double min_value) { 
        min_value_ = min_value; 
        markModified(); 
    }
    
    /**
     * @brief 최댓값 조회/설정
     */
    double getMaxValue() const { return max_value_; }
    void setMaxValue(double max_value) { 
        max_value_ = max_value; 
        markModified(); 
    }

    // =======================================================================
    // 로깅 설정 접근자
    // =======================================================================
    
    /**
     * @brief 로깅 활성화 상태 조회/설정
     */
    bool isLogEnabled() const { return log_enabled_; }
    void setLogEnabled(bool log_enabled) { 
        log_enabled_ = log_enabled; 
        markModified(); 
    }
    
    /**
     * @brief 로깅 간격 조회/설정
     */
    int getLogInterval() const { return log_interval_ms_; }
    void setLogInterval(int log_interval_ms) { 
        log_interval_ms_ = log_interval_ms; 
        markModified(); 
    }
    
    /**
     * @brief 로깅 데드밴드 조회/설정
     */
    double getLogDeadband() const { return log_deadband_; }
    void setLogDeadband(double log_deadband) { 
        log_deadband_ = log_deadband; 
        markModified(); 
    }

    // =======================================================================
    // 고급 기능 (DeviceEntity와 동일 패턴)
    // =======================================================================
    
    /**
     * @brief UnifiedCommonTypes.h의 DataPoint 구조체로 변환
     * @return DataPoint 구조체
     */
    DataPoint toDataPointStruct() const;
    
    /**
     * @brief 데이터포인트 설정을 JSON으로 추출
     * @return 설정 JSON
     */
    json extractConfiguration() const;
    
    /**
     * @brief Worker용 컨텍스트 조회 (실시간 데이터 수집용)
     * @return Worker 컨텍스트
     */
    json getWorkerContext() const;
    
    /**
     * @brief RDB 저장용 컨텍스트 조회
     * @return RDB 컨텍스트
     */
    json getRDBContext() const;
    
    /**
     * @brief 태그 목록 조회
     */
    const std::vector<std::string>& getTags() const { return tags_; }
    void setTags(const std::vector<std::string>& tags) { 
        tags_ = tags; 
        markModified(); 
    }
    
    /**
     * @brief 메타데이터 조회/설정
     */
    const json& getMetadata() const { return metadata_; }
    void setMetadata(const json& metadata) { 
        metadata_ = metadata; 
        markModified(); 
    }
    
    /**
     * @brief 유효성 검사
     */
    bool isValid() const;

private:
    // =======================================================================
    // 멤버 변수들 (UnifiedCommonTypes.h의 DataPoint 구조 기반)
    // =======================================================================
    
    // 기본 정보
    int device_id_;                          // 외래키 (devices.id)
    std::string name_;                       // 데이터포인트 이름
    std::string description_;                // 설명
    
    // 주소 및 타입 정보
    int address_;                            // 주소 (레지스터, 객체 ID 등)
    std::string data_type_;                  // 데이터 타입 (bool, int16, float 등)
    std::string access_mode_;                // 접근 모드 (read, write, read_write)
    bool is_enabled_;                        // 활성화 상태
    
    // 엔지니어링 정보
    std::string unit_;                       // 단위 (°C, bar, % 등)
    double scaling_factor_;                  // 스케일링 팩터
    double scaling_offset_;                  // 스케일링 오프셋
    double min_value_;                       // 최솟값
    double max_value_;                       // 최댓값
    
    // 로깅 설정
    bool log_enabled_;                       // 로깅 활성화
    int log_interval_ms_;                    // 로깅 간격 (ms)
    double log_deadband_;                    // 로깅 데드밴드
    
    // 메타데이터
    std::vector<std::string> tags_;          // 태그 목록
    json metadata_;                          // 추가 메타데이터
    
    // 통계 정보 (실시간 수집용)
    std::chrono::system_clock::time_point last_read_time_;
    std::chrono::system_clock::time_point last_write_time_;
    uint64_t read_count_;
    uint64_t write_count_;
    uint64_t error_count_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 매핑
     */
    bool mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 엔티티를 데이터베이스 값들로 변환
     */
    std::map<std::string, std::string> mapEntityToRow() const;

    /**
     * @brief INSERT SQL 쿼리 생성
     */
    std::string buildInsertSQL() const;
    
    /**
     * @brief UPDATE SQL 쿼리 생성
     */
    std::string buildUpdateSQL() const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_ENTITY_H