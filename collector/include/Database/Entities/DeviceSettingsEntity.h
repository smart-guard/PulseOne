#ifndef DEVICE_SETTINGS_ENTITY_H
#define DEVICE_SETTINGS_ENTITY_H

/**
 * @file DeviceSettingsEntity.h
 * @brief PulseOne DeviceSettingsEntity - 디바이스 설정 엔티티 (BaseEntity 상속)
 * @author PulseOne Development Team  
 * @date 2025-07-31
 * 
 * 🎯 BaseEntity 상속 패턴 적용:
 * - BaseEntity<DeviceSettingsEntity> 상속 (CRTP)
 * - device_id를 PRIMARY KEY로 사용 (BaseEntity ID와 동기화)
 * - DeviceEntity, DataPointEntity와 동일한 패턴
 * - device_settings 테이블과 1:1 매핑
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <memory>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 디바이스 설정 엔티티 클래스 (BaseEntity 상속)
 * 
 * 🎯 DB 스키마 완전 매핑:
 * CREATE TABLE device_settings (
 *     device_id INTEGER PRIMARY KEY,
 *     polling_interval_ms INTEGER DEFAULT 1000,
 *     scan_rate_override INTEGER,
 *     connection_timeout_ms INTEGER DEFAULT 10000,
 *     read_timeout_ms INTEGER DEFAULT 5000,
 *     write_timeout_ms INTEGER DEFAULT 5000,
 *     max_retry_count INTEGER DEFAULT 3,
 *     retry_interval_ms INTEGER DEFAULT 5000,
 *     backoff_multiplier DECIMAL(3,2) DEFAULT 1.5,
 *     backoff_time_ms INTEGER DEFAULT 60000,
 *     max_backoff_time_ms INTEGER DEFAULT 300000,
 *     keep_alive_enabled INTEGER DEFAULT 1,
 *     keep_alive_interval_s INTEGER DEFAULT 30,
 *     keep_alive_timeout_s INTEGER DEFAULT 10,
 *     data_validation_enabled INTEGER DEFAULT 1,
 *     outlier_detection_enabled INTEGER DEFAULT 0,
 *     deadband_enabled INTEGER DEFAULT 1,
 *     detailed_logging_enabled INTEGER DEFAULT 0,
 *     performance_monitoring_enabled INTEGER DEFAULT 1,
 *     diagnostic_mode_enabled INTEGER DEFAULT 0,
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_by INTEGER
 * );
 */
class DeviceSettingsEntity : public BaseEntity<DeviceSettingsEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (기본값으로 초기화)
     */
    DeviceSettingsEntity() 
        : BaseEntity<DeviceSettingsEntity>()
        , device_id_(0)
        , polling_interval_ms_(1000)        // 1초
        , connection_timeout_ms_(10000)     // 10초
        , max_retry_count_(3)
        , retry_interval_ms_(5000)          // 5초
        , backoff_time_ms_(60000)           // 1분
        , keep_alive_enabled_(true)
        , keep_alive_interval_s_(30)        // 30초
        , read_timeout_ms_(5000)            // 5초
        , write_timeout_ms_(5000)           // 5초
        , backoff_multiplier_(1.5)
        , max_backoff_time_ms_(300000)      // 5분
        , keep_alive_timeout_s_(10)         // 10초
        , data_validation_enabled_(true)
        , performance_monitoring_enabled_(true)
        , diagnostic_mode_enabled_(false)
        , outlier_detection_enabled_(false)
        , deadband_enabled_(true)
        , detailed_logging_enabled_(false)
        , created_at_(std::chrono::system_clock::now())
        , updated_at_(std::chrono::system_clock::now()) {
    }
    
    /**
     * @brief 디바이스 ID로 초기화하는 생성자
     * @param device_id 디바이스 ID (PRIMARY KEY)
     */
    explicit DeviceSettingsEntity(int device_id) : DeviceSettingsEntity() {
        device_id_ = device_id;
        setId(device_id);  // BaseEntity의 ID와 동기화
    }
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~DeviceSettingsEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
    // =======================================================================
    
    /**
     * @brief DB에서 엔티티 로드 (Repository에 위임)
     * @return 성공 시 true
     */
    bool loadFromDatabase() override {
        // 🎯 Repository 패턴: Repository에 위임
        auto repo = getRepository();
        if (repo) {
            auto loaded = repo->findById(device_id_);
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief DB에 엔티티 저장 (Repository에 위임)
     * @return 성공 시 true
     */
    bool saveToDatabase() override {
        auto repo = getRepository();
        if (repo) {
            return repo->save(*this);
        }
        return false;
    }
    
    /**
     * @brief DB에서 엔티티 삭제 (Repository에 위임)
     * @return 성공 시 true
     */
    bool deleteFromDatabase() override {
        auto repo = getRepository();
        if (repo && device_id_ > 0) {
            return repo->deleteById(device_id_);
        }
        return false;
    }
    
    /**
     * @brief DB에 엔티티 업데이트 (Repository에 위임)
     * @return 성공 시 true
     */
    bool updateToDatabase() override {
        auto repo = getRepository();
        if (repo) {
            return repo->update(*this);
        }
        return false;
    }
    
    /**
     * @brief JSON으로 직렬화
     * @return JSON 객체
     */
    json toJson() const override {
        json j;
        
        try {
            // 기본 식별자
            j["device_id"] = device_id_;
            
            // 폴링 및 타이밍 설정
            j["polling_interval_ms"] = polling_interval_ms_;
            if (scan_rate_override_.has_value()) {
                j["scan_rate_override"] = scan_rate_override_.value();
            }
            
            // 연결 및 통신 설정
            j["connection_timeout_ms"] = connection_timeout_ms_;
            j["read_timeout_ms"] = read_timeout_ms_;
            j["write_timeout_ms"] = write_timeout_ms_;
            
            // 재시도 정책
            j["max_retry_count"] = max_retry_count_;
            j["retry_interval_ms"] = retry_interval_ms_;
            j["backoff_multiplier"] = backoff_multiplier_;
            j["backoff_time_ms"] = backoff_time_ms_;
            j["max_backoff_time_ms"] = max_backoff_time_ms_;
            
            // Keep-alive 설정
            j["keep_alive_enabled"] = keep_alive_enabled_;
            j["keep_alive_interval_s"] = keep_alive_interval_s_;
            j["keep_alive_timeout_s"] = keep_alive_timeout_s_;
            
            // 데이터 품질 관리
            j["data_validation_enabled"] = data_validation_enabled_;
            j["outlier_detection_enabled"] = outlier_detection_enabled_;
            j["deadband_enabled"] = deadband_enabled_;
            
            // 로깅 및 진단
            j["detailed_logging_enabled"] = detailed_logging_enabled_;
            j["performance_monitoring_enabled"] = performance_monitoring_enabled_;
            j["diagnostic_mode_enabled"] = diagnostic_mode_enabled_;
            
            // 메타데이터
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            if (updated_by_.has_value()) {
                j["updated_by"] = updated_by_.value();
            }
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceSettingsEntity::toJson failed: " + std::string(e.what()));
        }
        
        return j;
    }
    
    /**
     * @brief JSON에서 역직렬화
     * @param data JSON 데이터
     * @return 성공 시 true
     */
    bool fromJson(const json& data) override {
        try {
            if (data.contains("device_id")) {
                setDeviceId(data["device_id"].get<int>());
            }
            
            // 폴링 및 타이밍 설정
            if (data.contains("polling_interval_ms")) {
                setPollingIntervalMs(data["polling_interval_ms"].get<int>());
            }
            if (data.contains("scan_rate_override")) {
                setScanRateOverride(data["scan_rate_override"].get<int>());
            }
            
            // 연결 및 통신 설정
            if (data.contains("connection_timeout_ms")) {
                setConnectionTimeoutMs(data["connection_timeout_ms"].get<int>());
            }
            if (data.contains("read_timeout_ms")) {
                setReadTimeoutMs(data["read_timeout_ms"].get<int>());
            }
            if (data.contains("write_timeout_ms")) {
                setWriteTimeoutMs(data["write_timeout_ms"].get<int>());
            }
            
            // 재시도 정책
            if (data.contains("max_retry_count")) {
                setMaxRetryCount(data["max_retry_count"].get<int>());
            }
            if (data.contains("retry_interval_ms")) {
                setRetryIntervalMs(data["retry_interval_ms"].get<int>());
            }
            if (data.contains("backoff_multiplier")) {
                setBackoffMultiplier(data["backoff_multiplier"].get<double>());
            }
            if (data.contains("backoff_time_ms")) {
                setBackoffTimeMs(data["backoff_time_ms"].get<int>());
            }
            if (data.contains("max_backoff_time_ms")) {
                setMaxBackoffTimeMs(data["max_backoff_time_ms"].get<int>());
            }
            
            // Keep-alive 설정
            if (data.contains("keep_alive_enabled")) {
                setKeepAliveEnabled(data["keep_alive_enabled"].get<bool>());
            }
            if (data.contains("keep_alive_interval_s")) {
                setKeepAliveIntervalS(data["keep_alive_interval_s"].get<int>());
            }
            if (data.contains("keep_alive_timeout_s")) {
                setKeepAliveTimeoutS(data["keep_alive_timeout_s"].get<int>());
            }
            
            // 데이터 품질 관리
            if (data.contains("data_validation_enabled")) {
                setDataValidationEnabled(data["data_validation_enabled"].get<bool>());
            }
            if (data.contains("outlier_detection_enabled")) {
                setOutlierDetectionEnabled(data["outlier_detection_enabled"].get<bool>());
            }
            if (data.contains("deadband_enabled")) {
                setDeadbandEnabled(data["deadband_enabled"].get<bool>());
            }
            
            // 로깅 및 진단
            if (data.contains("detailed_logging_enabled")) {
                setDetailedLoggingEnabled(data["detailed_logging_enabled"].get<bool>());
            }
            if (data.contains("performance_monitoring_enabled")) {
                setPerformanceMonitoringEnabled(data["performance_monitoring_enabled"].get<bool>());
            }
            if (data.contains("diagnostic_mode_enabled")) {
                setDiagnosticModeEnabled(data["diagnostic_mode_enabled"].get<bool>());
            }
            
            markModified();
            return true;
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceSettingsEntity::fromJson failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    /**
     * @brief 엔티티 문자열 표현
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override {
        std::ostringstream oss;
        oss << "DeviceSettingsEntity[";
        oss << "device_id=" << device_id_;
        oss << ", polling_interval=" << polling_interval_ms_ << "ms";
        oss << ", connection_timeout=" << connection_timeout_ms_ << "ms";
        oss << ", max_retry=" << max_retry_count_;
        oss << ", keep_alive=" << (keep_alive_enabled_ ? "enabled" : "disabled");
        oss << "]";
        return oss.str();
    }
    
    /**
     * @brief 테이블명 조회
     * @return 테이블명
     */
    std::string getTableName() const override { 
        return "device_settings"; 
    }

    // =======================================================================
    // 기본 속성 접근자 (스키마와 1:1 매핑)
    // =======================================================================
    
    /**
     * @brief 디바이스 ID 조회/설정 (PRIMARY KEY)
     */
    int getDeviceId() const { return device_id_; }
    void setDeviceId(int device_id) { 
        device_id_ = device_id;
        setId(device_id); // BaseEntity의 ID와 동기화
        markModified(); 
    }

    // =======================================================================
    // 폴링 및 타이밍 설정
    // =======================================================================
    
    int getPollingIntervalMs() const { return polling_interval_ms_; }
    void setPollingIntervalMs(int polling_interval_ms) { 
        if (polling_interval_ms > 0) {
            polling_interval_ms_ = polling_interval_ms; 
            markModified();
        }
    }
    
    std::optional<int> getScanRateOverride() const { return scan_rate_override_; }
    void setScanRateOverride(const std::optional<int>& scan_rate_override) { 
        scan_rate_override_ = scan_rate_override; 
        markModified(); 
    }
    void setScanRateOverride(int scan_rate_override) { 
        scan_rate_override_ = scan_rate_override; 
        markModified(); 
    }

    // =======================================================================
    // 연결 및 통신 설정
    // =======================================================================
    
    int getConnectionTimeoutMs() const { return connection_timeout_ms_; }
    void setConnectionTimeoutMs(int connection_timeout_ms) { 
        if (connection_timeout_ms > 0) {
            connection_timeout_ms_ = connection_timeout_ms; 
            markModified();
        }
    }
    
    int getReadTimeoutMs() const { return read_timeout_ms_; }
    void setReadTimeoutMs(int read_timeout_ms) { 
        if (read_timeout_ms > 0) {
            read_timeout_ms_ = read_timeout_ms; 
            markModified();
        }
    }
    
    int getWriteTimeoutMs() const { return write_timeout_ms_; }
    void setWriteTimeoutMs(int write_timeout_ms) { 
        if (write_timeout_ms > 0) {
            write_timeout_ms_ = write_timeout_ms; 
            markModified();
        }
    }

    // =======================================================================
    // 재시도 정책 설정
    // =======================================================================
    
    int getMaxRetryCount() const { return max_retry_count_; }
    void setMaxRetryCount(int max_retry_count) { 
        if (max_retry_count >= 0) {
            max_retry_count_ = max_retry_count; 
            markModified();
        }
    }
    
    int getRetryIntervalMs() const { return retry_interval_ms_; }
    void setRetryIntervalMs(int retry_interval_ms) { 
        if (retry_interval_ms > 0) {
            retry_interval_ms_ = retry_interval_ms; 
            markModified();
        }
    }
    
    double getBackoffMultiplier() const { return backoff_multiplier_; }
    void setBackoffMultiplier(double backoff_multiplier) { 
        if (backoff_multiplier > 1.0) {
            backoff_multiplier_ = backoff_multiplier; 
            markModified();
        }
    }
    
    int getBackoffTimeMs() const { return backoff_time_ms_; }
    void setBackoffTimeMs(int backoff_time_ms) { 
        if (backoff_time_ms > 0) {
            backoff_time_ms_ = backoff_time_ms; 
            markModified();
        }
    }
    
    int getMaxBackoffTimeMs() const { return max_backoff_time_ms_; }
    void setMaxBackoffTimeMs(int max_backoff_time_ms) { 
        if (max_backoff_time_ms > 0) {
            max_backoff_time_ms_ = max_backoff_time_ms; 
            markModified();
        }
    }

    // =======================================================================
    // Keep-alive 설정
    // =======================================================================
    
    bool isKeepAliveEnabled() const { return keep_alive_enabled_; }
    void setKeepAliveEnabled(bool keep_alive_enabled) { 
        keep_alive_enabled_ = keep_alive_enabled; 
        markModified();
    }
    
    int getKeepAliveIntervalS() const { return keep_alive_interval_s_; }
    void setKeepAliveIntervalS(int keep_alive_interval_s) { 
        if (keep_alive_interval_s > 0) {
            keep_alive_interval_s_ = keep_alive_interval_s; 
            markModified();
        }
    }
    
    int getKeepAliveTimeoutS() const { return keep_alive_timeout_s_; }
    void setKeepAliveTimeoutS(int keep_alive_timeout_s) { 
        if (keep_alive_timeout_s > 0) {
            keep_alive_timeout_s_ = keep_alive_timeout_s; 
            markModified();
        }
    }

    // =======================================================================
    // 데이터 품질 관리 설정
    // =======================================================================
    
    bool isDataValidationEnabled() const { return data_validation_enabled_; }
    void setDataValidationEnabled(bool data_validation_enabled) { 
        data_validation_enabled_ = data_validation_enabled; 
        markModified();
    }
    
    bool isOutlierDetectionEnabled() const { return outlier_detection_enabled_; }
    void setOutlierDetectionEnabled(bool outlier_detection_enabled) { 
        outlier_detection_enabled_ = outlier_detection_enabled; 
        markModified();
    }
    
    bool isDeadbandEnabled() const { return deadband_enabled_; }
    void setDeadbandEnabled(bool deadband_enabled) { 
        deadband_enabled_ = deadband_enabled; 
        markModified();
    }

    // =======================================================================
    // 로깅 및 진단 설정
    // =======================================================================
    
    bool isDetailedLoggingEnabled() const { return detailed_logging_enabled_; }
    void setDetailedLoggingEnabled(bool detailed_logging_enabled) { 
        detailed_logging_enabled_ = detailed_logging_enabled; 
        markModified();
    }
    
    bool isPerformanceMonitoringEnabled() const { return performance_monitoring_enabled_; }
    void setPerformanceMonitoringEnabled(bool performance_monitoring_enabled) { 
        performance_monitoring_enabled_ = performance_monitoring_enabled; 
        markModified();
    }
    
    bool isDiagnosticModeEnabled() const { return diagnostic_mode_enabled_; }
    void setDiagnosticModeEnabled(bool diagnostic_mode_enabled) { 
        diagnostic_mode_enabled_ = diagnostic_mode_enabled; 
        markModified();
    }

    // =======================================================================
    // 메타데이터 (스키마의 타임스탬프 필드들)
    // =======================================================================
    
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    void setCreatedAt(const std::chrono::system_clock::time_point& created_at) { 
        created_at_ = created_at; 
        markModified(); 
    }
    
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    void setUpdatedAt(const std::chrono::system_clock::time_point& updated_at) { 
        updated_at_ = updated_at; 
        markModified(); 
    }
    
    std::optional<int> getUpdatedBy() const { return updated_by_; }
    void setUpdatedBy(const std::optional<int>& updated_by) { 
        updated_by_ = updated_by; 
        markModified(); 
    }
    void setUpdatedBy(int updated_by) { 
        updated_by_ = updated_by; 
        markModified(); 
    }

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 설정이 유효한지 검증
     * @return 유효하면 true
     */
    bool isValid() const override {
        return device_id_ > 0 
            && polling_interval_ms_ > 0 
            && connection_timeout_ms_ > 0 
            && max_retry_count_ >= 0
            && retry_interval_ms_ > 0
            && backoff_time_ms_ > 0
            && keep_alive_interval_s_ > 0
            && read_timeout_ms_ > 0
            && write_timeout_ms_ > 0
            && backoff_multiplier_ > 1.0
            && max_backoff_time_ms_ > 0
            && keep_alive_timeout_s_ > 0;
    }
    
    /**
     * @brief 산업용 기본 설정으로 리셋
     */
    void resetToIndustrialDefaults() {
        polling_interval_ms_ = 1000;         // 1초 (산업용 표준)
        connection_timeout_ms_ = 10000;      // 10초
        max_retry_count_ = 3;                // 3회 재시도
        retry_interval_ms_ = 5000;           // 5초 간격
        backoff_time_ms_ = 60000;            // 1분 백오프
        keep_alive_enabled_ = true;          // Keep-alive 활성화
        keep_alive_interval_s_ = 30;         // 30초 주기
        
        // 고급 설정들
        read_timeout_ms_ = 5000;             // 5초
        write_timeout_ms_ = 5000;            // 5초  
        backoff_multiplier_ = 1.5;           // 1.5배 증가
        max_backoff_time_ms_ = 300000;       // 최대 5분
        keep_alive_timeout_s_ = 10;          // 10초 타임아웃
        data_validation_enabled_ = true;     // 데이터 검증 활성화
        performance_monitoring_enabled_ = true; // 성능 모니터링 활성화
        diagnostic_mode_enabled_ = false;    // 진단 모드 비활성화
        
        markModified();
    }
    
    /**
     * @brief 고속 모드 설정 (짧은 주기, 빠른 응답)
     */
    void setHighSpeedMode() {
        polling_interval_ms_ = 500;          // 500ms
        connection_timeout_ms_ = 3000;       // 3초
        read_timeout_ms_ = 2000;             // 2초
        write_timeout_ms_ = 2000;            // 2초
        retry_interval_ms_ = 2000;           // 2초 간격
        keep_alive_interval_s_ = 10;         // 10초 주기
        
        markModified();
    }
    
    /**
     * @brief 안정성 모드 설정 (긴 주기, 안정적 연결)
     */
    void setStabilityMode() {
        polling_interval_ms_ = 5000;         // 5초
        connection_timeout_ms_ = 30000;      // 30초
        max_retry_count_ = 5;                // 5회 재시도
        retry_interval_ms_ = 10000;          // 10초 간격
        backoff_time_ms_ = 120000;           // 2분 백오프
        keep_alive_interval_s_ = 60;         // 1분 주기
        
        markModified();
    }

private:
    // =======================================================================
    // 멤버 변수들 (스키마와 1:1 매핑)
    // =======================================================================
    
    int device_id_;                     // PRIMARY KEY, FOREIGN KEY to devices.id
    
    // 폴링 및 타이밍 설정
    int polling_interval_ms_;           // 폴링 주기 (ms)
    std::optional<int> scan_rate_override_;  // 개별 스캔 주기 오버라이드
    
    // 연결 및 통신 설정
    int connection_timeout_ms_;         // 연결 타임아웃 (ms)
    int read_timeout_ms_;               // 읽기 타임아웃
    int write_timeout_ms_;              // 쓰기 타임아웃
    
    // 재시도 정책
    int max_retry_count_;               // 최대 재시도 횟수
    int retry_interval_ms_;             // 재시도 간격 (ms)
    double backoff_multiplier_;         // 백오프 배수 (1.5배씩 증가)
    int backoff_time_ms_;               // 백오프 시간 (ms)
    int max_backoff_time_ms_;           // 최대 백오프 시간
    
    // Keep-alive 설정
    bool keep_alive_enabled_;           // Keep-alive 활성화
    int keep_alive_interval_s_;         // Keep-alive 주기 (초)
    int keep_alive_timeout_s_;          // Keep-alive 타임아웃
    
    // 데이터 품질 관리
    bool data_validation_enabled_;      // 데이터 검증 활성화
    bool outlier_detection_enabled_;    // 이상값 감지 활성화
    bool deadband_enabled_;             // 데드밴드 활성화
    
    // 로깅 및 진단
    bool detailed_logging_enabled_;     // 상세 로깅 활성화
    bool performance_monitoring_enabled_; // 성능 모니터링 활성화
    bool diagnostic_mode_enabled_;      // 진단 모드 활성화
    
    // 메타데이터
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    std::optional<int> updated_by_;     // 설정을 변경한 사용자

    // =======================================================================
    // Repository 접근 헬퍼 메서드
    // =======================================================================
    
    /**
     * @brief Repository 인스턴스 얻기
     * @return DeviceSettingsRepository 포인터 (없으면 nullptr)
     */
    std::shared_ptr<Repositories::DeviceSettingsRepository> getRepository() const {
        try {
            // 🎯 Repository 패턴: RepositoryFactory 사용
            return RepositoryFactory::getInstance().getDeviceSettingsRepository();
        } catch (const std::exception& e) {
            logger_->Error("DeviceSettingsEntity::getRepository failed: " + std::string(e.what()));
            return nullptr;
        }
    }
};

} // namespace Entities
} // namespace Database  
} // namespace PulseOne

#endif // DEVICE_SETTINGS_ENTITY_H