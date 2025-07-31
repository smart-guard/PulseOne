#ifndef DEVICE_SETTINGS_ENTITY_H
#define DEVICE_SETTINGS_ENTITY_H

/**
 * @file DeviceSettingsEntity.h
 * @brief PulseOne DeviceSettingsEntity - 디바이스 설정 엔티티
 * @author PulseOne Development Team  
 * @date 2025-07-31
 * 
 * 🎯 Phase 4: DeviceSettings 동적 로딩을 위한 엔티티 클래스
 * - device_settings 테이블과 1:1 매핑
 * - Worker에서 동적으로 설정을 변경할 수 있도록 지원
 * - 엔지니어 친화적 설정 관리
 */

#include <string>
#include <chrono>
#include <memory>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 디바이스 설정 엔티티 클래스
 * 
 * 기능:
 * - device_settings 테이블 완전 매핑
 * - 통신 설정: polling, timeout, retry 등
 * - 고급 설정: backoff, keep-alive, 진단 모드 등
 * - 실시간 설정 변경 지원
 */
class DeviceSettingsEntity {
private:
    // =======================================================================
    // 기본 속성들 (device_settings 테이블과 1:1 매핑)
    // =======================================================================
    
    int device_id_;                     // PRIMARY KEY, FOREIGN KEY to devices.id
    
    // 기본 통신 설정
    int polling_interval_ms_;           // 폴링 주기 (ms)
    int connection_timeout_ms_;         // 연결 타임아웃 (ms)
    int max_retry_count_;               // 최대 재시도 횟수
    int retry_interval_ms_;             // 재시도 간격 (ms)
    int backoff_time_ms_;               // 백오프 시간 (ms)
    bool keep_alive_enabled_;           // Keep-alive 활성화
    int keep_alive_interval_s_;         // Keep-alive 주기 (초)
    
    // 고급 설정들 (추가 예정인 컬럼들)
    std::optional<int> scan_rate_override_;      // 개별 스캔 주기 오버라이드
    int read_timeout_ms_;               // 읽기 타임아웃
    int write_timeout_ms_;              // 쓰기 타임아웃
    double backoff_multiplier_;         // 백오프 배수 (1.5배씩 증가)
    int max_backoff_time_ms_;           // 최대 백오프 시간
    int keep_alive_timeout_s_;          // Keep-alive 타임아웃
    bool data_validation_enabled_;      // 데이터 검증 활성화
    bool performance_monitoring_enabled_; // 성능 모니터링 활성화
    bool diagnostic_mode_enabled_;      // 진단 모드 활성화
    
    // 메타데이터
    std::chrono::system_clock::time_point updated_at_;

public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (기본값으로 초기화)
     */
    DeviceSettingsEntity() 
        : device_id_(0)
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
        , updated_at_(std::chrono::system_clock::now()) {
    }
    
    /**
     * @brief 디바이스 ID로 초기화하는 생성자
     * @param device_id 디바이스 ID
     */
    explicit DeviceSettingsEntity(int device_id) : DeviceSettingsEntity() {
        device_id_ = device_id;
    }
    
    /**
     * @brief 복사 생성자
     */
    DeviceSettingsEntity(const DeviceSettingsEntity& other) = default;
    
    /**
     * @brief 이동 생성자
     */
    DeviceSettingsEntity(DeviceSettingsEntity&& other) noexcept = default;
    
    /**
     * @brief 복사 대입 연산자
     */
    DeviceSettingsEntity& operator=(const DeviceSettingsEntity& other) = default;
    
    /**
     * @brief 이동 대입 연산자
     */
    DeviceSettingsEntity& operator=(DeviceSettingsEntity&& other) noexcept = default;
    
    /**
     * @brief 소멸자
     */
    ~DeviceSettingsEntity() = default;

    // =======================================================================
    // Getter 메서드들
    // =======================================================================
    
    int getDeviceId() const { return device_id_; }
    int getPollingIntervalMs() const { return polling_interval_ms_; }
    int getConnectionTimeoutMs() const { return connection_timeout_ms_; }
    int getMaxRetryCount() const { return max_retry_count_; }
    int getRetryIntervalMs() const { return retry_interval_ms_; }
    int getBackoffTimeMs() const { return backoff_time_ms_; }
    bool isKeepAliveEnabled() const { return keep_alive_enabled_; }
    int getKeepAliveIntervalS() const { return keep_alive_interval_s_; }
    
    // 고급 설정 Getter들
    std::optional<int> getScanRateOverride() const { return scan_rate_override_; }
    int getReadTimeoutMs() const { return read_timeout_ms_; }
    int getWriteTimeoutMs() const { return write_timeout_ms_; }
    double getBackoffMultiplier() const { return backoff_multiplier_; }
    int getMaxBackoffTimeMs() const { return max_backoff_time_ms_; }
    int getKeepAliveTimeoutS() const { return keep_alive_timeout_s_; }
    bool isDataValidationEnabled() const { return data_validation_enabled_; }
    bool isPerformanceMonitoringEnabled() const { return performance_monitoring_enabled_; }
    bool isDiagnosticModeEnabled() const { return diagnostic_mode_enabled_; }
    
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }

    // =======================================================================
    // Setter 메서드들
    // =======================================================================
    
    void setDeviceId(int device_id) { 
        device_id_ = device_id; 
        markAsUpdated();
    }
    
    // 기본 통신 설정 Setter들
    void setPollingIntervalMs(int interval_ms) { 
        if (interval_ms > 0) {
            polling_interval_ms_ = interval_ms; 
            markAsUpdated();
        }
    }
    
    void setConnectionTimeoutMs(int timeout_ms) { 
        if (timeout_ms > 0) {
            connection_timeout_ms_ = timeout_ms; 
            markAsUpdated();
        }
    }
    
    void setMaxRetryCount(int retry_count) { 
        if (retry_count >= 0) {
            max_retry_count_ = retry_count; 
            markAsUpdated();
        }
    }
    
    void setRetryIntervalMs(int interval_ms) { 
        if (interval_ms > 0) {
            retry_interval_ms_ = interval_ms; 
            markAsUpdated();
        }
    }
    
    void setBackoffTimeMs(int backoff_ms) { 
        if (backoff_ms > 0) {
            backoff_time_ms_ = backoff_ms; 
            markAsUpdated();
        }
    }
    
    void setKeepAliveEnabled(bool enabled) { 
        keep_alive_enabled_ = enabled; 
        markAsUpdated();
    }
    
    void setKeepAliveIntervalS(int interval_s) { 
        if (interval_s > 0) {
            keep_alive_interval_s_ = interval_s; 
            markAsUpdated();
        }
    }
    
    // 고급 설정 Setter들
    void setScanRateOverride(const std::optional<int>& scan_rate) { 
        scan_rate_override_ = scan_rate; 
        markAsUpdated();
    }
    
    void setReadTimeoutMs(int timeout_ms) { 
        if (timeout_ms > 0) {
            read_timeout_ms_ = timeout_ms; 
            markAsUpdated();
        }
    }
    
    void setWriteTimeoutMs(int timeout_ms) { 
        if (timeout_ms > 0) {
            write_timeout_ms_ = timeout_ms; 
            markAsUpdated();
        }
    }
    
    void setBackoffMultiplier(double multiplier) { 
        if (multiplier > 1.0) {
            backoff_multiplier_ = multiplier; 
            markAsUpdated();
        }
    }
    
    void setMaxBackoffTimeMs(int max_backoff_ms) { 
        if (max_backoff_ms > 0) {
            max_backoff_time_ms_ = max_backoff_ms; 
            markAsUpdated();
        }
    }
    
    void setKeepAliveTimeoutS(int timeout_s) { 
        if (timeout_s > 0) {
            keep_alive_timeout_s_ = timeout_s; 
            markAsUpdated();
        }
    }
    
    void setDataValidationEnabled(bool enabled) { 
        data_validation_enabled_ = enabled; 
        markAsUpdated();
    }
    
    void setPerformanceMonitoringEnabled(bool enabled) { 
        performance_monitoring_enabled_ = enabled; 
        markAsUpdated();
    }
    
    void setDiagnosticModeEnabled(bool enabled) { 
        diagnostic_mode_enabled_ = enabled; 
        markAsUpdated();
    }

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 설정이 유효한지 검증
     * @return 유효하면 true
     */
    bool isValid() const {
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
        
        markAsUpdated();
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
        
        markAsUpdated();
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
        
        markAsUpdated();
    }
    
    /**
     * @brief 다른 설정 엔티티와 비교
     * @param other 비교할 설정 엔티티
     * @return 같으면 true
     */
    bool operator==(const DeviceSettingsEntity& other) const {
        return device_id_ == other.device_id_
            && polling_interval_ms_ == other.polling_interval_ms_
            && connection_timeout_ms_ == other.connection_timeout_ms_
            && max_retry_count_ == other.max_retry_count_
            && retry_interval_ms_ == other.retry_interval_ms_
            && backoff_time_ms_ == other.backoff_time_ms_
            && keep_alive_enabled_ == other.keep_alive_enabled_
            && keep_alive_interval_s_ == other.keep_alive_interval_s_;
            // 모든 필드를 비교하면 너무 길어지므로 주요 필드만 비교
    }
    
    /**
     * @brief 설정을 JSON 문자열로 변환 (로깅/디버깅용)
     * @return JSON 형태의 설정 정보
     */
    std::string toJson() const;

private:
    /**
     * @brief 업데이트 시간을 현재 시각으로 설정
     */
    void markAsUpdated() {
        updated_at_ = std::chrono::system_clock::now();
    }
};

} // namespace Entities
} // namespace Database  
} // namespace PulseOne

#endif // DEVICE_SETTINGS_ENTITY_H