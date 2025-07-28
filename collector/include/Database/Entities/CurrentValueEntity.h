#ifndef CURRENT_VALUE_ENTITY_H
#define CURRENT_VALUE_ENTITY_H

/**
 * @file CurrentValueEntity.h
 * @brief PulseOne Current Value Entity - 실시간 데이터 현재값 엔티티 (기존 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 기존 패턴 100% 준수:
 * - BaseEntity<CurrentValueEntity> 상속 (CRTP)
 * - INTEGER ID 기반
 * - markModified() 패턴 통일
 * - JSON 직렬화/역직렬화
 * - DataPointEntity/DeviceEntity와 동일한 구조/네이밍
 * 
 * 핵심 기능:
 * - Redis ↔ RDB 양방향 저장
 * - 실시간 데이터 버퍼링
 * - 주기적 저장 스케줄링
 * - 데이터 품질 관리
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <chrono>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 현재값 엔티티 클래스 (BaseEntity 템플릿 상속)
 */
class CurrentValueEntity : public BaseEntity<CurrentValueEntity> {
public:
    // =======================================================================
    // 데이터 품질 열거형 (산업 표준)
    // =======================================================================
    
    enum class DataQuality {
        GOOD,               // 정상 데이터
        BAD,                // 불량 데이터 (통신 오류)
        UNCERTAIN,          // 불확실한 데이터 (센서 문제)
        TIMEOUT,            // 타임아웃
        INVALID,            // 유효하지 않은 데이터
        OVERRANGE,          // 범위 초과
        UNDERRANGE,         // 범위 미만
        OFFLINE             // 오프라인 상태
    };
    
    // =======================================================================
    // 저장 타입 열거형
    // =======================================================================
    
    enum class StorageType {
        IMMEDIATE,          // 즉시 저장 (Critical points)
        ON_CHANGE,          // 값 변경 시 저장 (Binary/Digital)
        PERIODIC,           // 주기적 저장 (Analog)
        BUFFERED            // 버퍼링 후 배치 저장
    };

    // =======================================================================
    // 생성자 및 소멸자 (기존 패턴)
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (새 현재값)
     */
    CurrentValueEntity();
    
    /**
     * @brief ID로 생성자 (기존 현재값 로드)
     * @param id 현재값 ID
     */
    explicit CurrentValueEntity(int id);
    
    /**
     * @brief DataPoint ID와 값으로 생성자
     * @param data_point_id 데이터포인트 ID
     * @param value 현재값
     */
    CurrentValueEntity(int data_point_id, double value);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~CurrentValueEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (기존 패턴)
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
     * @brief DB에 엔티티 업데이트
     * @return 성공 시 true
     */
    bool updateToDatabase() override;
    
    /**
     * @brief DB에서 엔티티 삭제
     * @return 성공 시 true
     */
    bool deleteFromDatabase() override;
    
    /**
     * @brief 테이블명 반환
     * @return 테이블명
     */
    std::string getTableName() const override { return "current_values"; }
    
    /**
     * @brief 유효성 검사
     * @return 유효하면 true
     */
    bool isValid() const override;
    
    /**
     * @brief JSON으로 변환
     * @return JSON 객체
     */
    json toJson() const override;
    
    /**
     * @brief JSON에서 로드
     * @param data JSON 데이터
     * @return 성공 시 true
     */
    bool fromJson(const json& data) override;
    
    /**
     * @brief 문자열 표현
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override;

    // =======================================================================
    // 접근자 (getter/setter) - 기존 패턴 100% 준수
    // =======================================================================
    
    /**
     * @brief 데이터포인트 ID 조회/설정
     */
    int getDataPointId() const { return data_point_id_; }
    void setDataPointId(int data_point_id) { 
        data_point_id_ = data_point_id; 
        markModified(); 
    }
    
    /**
     * @brief 가상포인트 ID 조회/설정 (선택적)
     */
    int getVirtualPointId() const { return virtual_point_id_; }
    void setVirtualPointId(int virtual_point_id) { 
        virtual_point_id_ = virtual_point_id; 
        markModified(); 
    }
    
    /**
     * @brief 현재값 조회/설정
     */
    double getValue() const { return value_; }
    void setValue(double value) { 
        value_ = value; 
        markModified(); 
        updateTimestamp();
    }
    
    /**
     * @brief 원시값 조회/설정 (스케일링 전)
     */
    double getRawValue() const { return raw_value_; }
    void setRawValue(double raw_value) { 
        raw_value_ = raw_value; 
        markModified(); 
    }
    
    /**
     * @brief 데이터 품질 문자열 변환 (public으로 변경)
     */
    std::string getQualityString() const;
    void setQualityFromString(const std::string& quality_str);
    
    /**
     * @brief 정적 품질 변환 메서드들 (public으로 변경)
     */
    static std::string qualityToString(DataQuality quality);
    static DataQuality stringToQuality(const std::string& quality_str);
    static std::string storageTypeToString(StorageType type);
    static StorageType stringToStorageType(const std::string& type_str);
    
    /**
     * @brief 데이터 품질 조회/설정
     */
    DataQuality getQuality() const { return quality_; }
    void setQuality(DataQuality quality) { 
        quality_ = quality; 
        markModified(); 
    }
    
    /**
     * @brief 타임스탬프 조회/설정
     */
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    void setTimestamp(const std::chrono::system_clock::time_point& timestamp) { 
        timestamp_ = timestamp; 
        markModified(); 
    }
    
    /**
     * @brief Redis 키 조회/설정
     */
    const std::string& getRedisKey() const { return redis_key_; }
    void setRedisKey(const std::string& redis_key) { 
        redis_key_ = redis_key; 
        markModified(); 
    }
    
    /**
     * @brief Redis 로드 상태 조회/설정
     */
    bool isFromRedis() const { return is_from_redis_; }
    void setFromRedis(bool is_from_redis) { 
        is_from_redis_ = is_from_redis; 
    }
    
    /**
     * @brief 저장 타입 조회/설정
     */
    StorageType getStorageType() const { return storage_type_; }
    void setStorageType(StorageType storage_type) { 
        storage_type_ = storage_type; 
        markModified(); 
    }

    // =======================================================================
    // 🔥 Redis 연동 전용 메서드들
    // =======================================================================
    
    /**
     * @brief Redis에서 값 로드
     * @param redis_key Redis 키 (선택적, 없으면 자동 생성)
     * @return 성공 시 true
     */
    bool loadFromRedis(const std::string& redis_key = "");
    
    /**
     * @brief Redis에 값 저장
     * @param ttl_seconds TTL 설정 (초, 0이면 무제한)
     * @return 성공 시 true
     */
    bool saveToRedis(int ttl_seconds = 300);
    
    /**
     * @brief Redis 키 자동 생성
     * @return 생성된 Redis 키
     */
    std::string generateRedisKey() const;
    
    /**
     * @brief Redis JSON 포맷으로 변환
     * @return Redis 저장용 JSON
     */
    json toRedisJson() const;
    
    /**
     * @brief Redis JSON에서 로드
     * @param redis_data Redis JSON 데이터
     * @return 성공 시 true
     */
    bool fromRedisJson(const json& redis_data);

    // =======================================================================
    // 🔥 RDB 저장 최적화 메서드들
    // =======================================================================
    
    /**
     * @brief 주기적 저장이 필요한지 확인
     * @param interval_seconds 저장 주기 (초)
     * @return 저장이 필요하면 true
     */
    bool needsPeriodicSave(int interval_seconds) const;
    
    /**
     * @brief 변경 감지 저장이 필요한지 확인
     * @param deadband 데드밴드 값
     * @return 저장이 필요하면 true
     */
    bool needsOnChangeSave(double deadband = 0.0) const;
    
    /**
     * @brief UPSERT SQL 생성 (성능 최적화)
     * @return UPSERT SQL 문자열
     */
    std::string generateUpsertSql() const;
    
    /**
     * @brief 배치 저장용 값 배열 반환
     * @return SQL 값 배열
     */
    std::string getValuesForBatchInsert() const;

    // =======================================================================
    // 고급 기능 (기존 패턴)
    // =======================================================================
    
    /**
     * @brief Worker용 컨텍스트 조회
     * @return Worker 컨텍스트
     */
    json getWorkerContext() const;
    
    /**
     * @brief 알람 검사용 컨텍스트 조회
     * @return 알람 컨텍스트
     */
    json getAlarmContext() const;
    
    /**
     * @brief 이전 값과 비교
     * @param other 비교할 다른 현재값
     * @return 변경 여부
     */
    bool hasChangedFrom(const CurrentValueEntity& other) const;
    
    /**
     * @brief 데이터 품질 상태 확인
     * @return 정상 데이터인지 여부
     */
    bool isGoodQuality() const { return quality_ == DataQuality::GOOD; }

private:
    // =======================================================================
    // 멤버 변수들 (DB 스키마와 일치)
    // =======================================================================
    
    // 기본 정보
    int data_point_id_;                           // 외래키 (data_points.id)
    int virtual_point_id_;                        // 외래키 (virtual_points.id, 선택적)
    double value_;                                // 현재값
    double raw_value_;                            // 원시값 (스케일링 전)
    DataQuality quality_;                         // 데이터 품질
    std::chrono::system_clock::time_point timestamp_;  // 타임스탬프
    
    // Redis 연동
    std::string redis_key_;                       // Redis 키
    bool is_from_redis_;                          // Redis에서 로드된 데이터인지
    StorageType storage_type_;                    // 저장 타입
    
    // 성능 최적화용
    mutable std::chrono::system_clock::time_point last_save_time_;  // 마지막 저장 시간
    mutable double last_saved_value_;             // 마지막 저장된 값 (변경 감지용)
    
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 내부 헬퍼 메서드들
     */
    void updateTimestamp() {
        timestamp_ = std::chrono::system_clock::now();
    }
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // CURRENT_VALUE_ENTITY_H