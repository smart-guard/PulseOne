#ifndef DEVICE_SETTINGS_ENTITY_H
#define DEVICE_SETTINGS_ENTITY_H

/**
 * @file DeviceSettingsEntity.h
 * @brief PulseOne DeviceSettingsEntity - 디바이스 설정 엔티티 (BaseEntity 상속)
 * @author PulseOne Development Team  
 * @date 2025-07-31
 * 
 * 🎯 헤더/구현 분리:
 * - 헤더: 선언만 (순환 참조 방지)
 * - CPP: Repository 호출 구현
 * - device_settings 테이블과 1:1 매핑
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <sstream>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
struct json {
    template<typename T> T get() const { return T{}; }
    bool contains(const std::string&) const { return false; }
    std::string dump() const { return "{}"; }
    static json parse(const std::string&) { return json{}; }
    static json object() { return json{}; }
};
#endif

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 디바이스 설정 엔티티 클래스 (BaseEntity 상속)
 */
class DeviceSettingsEntity : public BaseEntity<DeviceSettingsEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    DeviceSettingsEntity();
    explicit DeviceSettingsEntity(int device_id);
    virtual ~DeviceSettingsEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (인라인 구현)
    // =======================================================================
    
    json toJson() const override {
        json j;
        try {
            j["device_id"] = device_id_;
            j["polling_interval_ms"] = polling_interval_ms_;
            j["connection_timeout_ms"] = connection_timeout_ms_;
            j["max_retry_count"] = max_retry_count_;
            j["retry_interval_ms"] = retry_interval_ms_;
            j["backoff_time_ms"] = backoff_time_ms_;
            j["keep_alive_enabled"] = keep_alive_enabled_;
            j["keep_alive_interval_s"] = keep_alive_interval_s_;
            
            if (scan_rate_override_.has_value()) {
                j["scan_rate_override"] = scan_rate_override_.value();
            }
            
            j["read_timeout_ms"] = read_timeout_ms_;
            j["write_timeout_ms"] = write_timeout_ms_;
            j["backoff_multiplier"] = backoff_multiplier_;
            j["max_backoff_time_ms"] = max_backoff_time_ms_;
            j["keep_alive_timeout_s"] = keep_alive_timeout_s_;
            j["data_validation_enabled"] = data_validation_enabled_;
            j["performance_monitoring_enabled"] = performance_monitoring_enabled_;
            j["diagnostic_mode_enabled"] = diagnostic_mode_enabled_;
        } catch (const std::exception&) {
            // JSON 생성 실패 시 기본 객체 반환
        }
        return j;
    }
    
    bool fromJson(const json& data) override {
        try {
            if (data.contains("device_id")) setDeviceId(data["device_id"].get<int>());
            if (data.contains("polling_interval_ms")) setPollingIntervalMs(data["polling_interval_ms"].get<int>());
            if (data.contains("connection_timeout_ms")) setConnectionTimeoutMs(data["connection_timeout_ms"].get<int>());
            if (data.contains("max_retry_count")) setMaxRetryCount(data["max_retry_count"].get<int>());
            if (data.contains("retry_interval_ms")) setRetryIntervalMs(data["retry_interval_ms"].get<int>());
            if (data.contains("backoff_time_ms")) setBackoffTimeMs(data["backoff_time_ms"].get<int>());
            if (data.contains("keep_alive_enabled")) setKeepAliveEnabled(data["keep_alive_enabled"].get<bool>());
            if (data.contains("keep_alive_interval_s")) setKeepAliveIntervalS(data["keep_alive_interval_s"].get<int>());
            if (data.contains("scan_rate_override")) {
                if (data["scan_rate_override"].is_null()) {
                    setScanRateOverride(std::nullopt);
                } else {
                    setScanRateOverride(data["scan_rate_override"].get<int>());
                }
            }
            if (data.contains("read_timeout_ms")) setReadTimeoutMs(data["read_timeout_ms"].get<int>());
            if (data.contains("write_timeout_ms")) setWriteTimeoutMs(data["write_timeout_ms"].get<int>());
            if (data.contains("backoff_multiplier")) setBackoffMultiplier(data["backoff_multiplier"].get<double>());
            if (data.contains("max_backoff_time_ms")) setMaxBackoffTimeMs(data["max_backoff_time_ms"].get<int>());
            if (data.contains("keep_alive_timeout_s")) setKeepAliveTimeoutS(data["keep_alive_timeout_s"].get<int>());
            if (data.contains("data_validation_enabled")) setDataValidationEnabled(data["data_validation_enabled"].get<bool>());
            if (data.contains("performance_monitoring_enabled")) setPerformanceMonitoringEnabled(data["performance_monitoring_enabled"].get<bool>());
            if (data.contains("diagnostic_mode_enabled")) setDiagnosticModeEnabled(data["diagnostic_mode_enabled"].get<bool>());
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "DeviceSettingsEntity[device_id=" << device_id_ 
            << ", polling=" << polling_interval_ms_ << "ms"
            << ", timeout=" << connection_timeout_ms_ << "ms"
            << ", retry=" << max_retry_count_ << "]";
        return oss.str();
    }
    
    std::string getTableName() const override { return "device_settings"; }

    // =======================================================================
    // Getter/Setter 메서드들 (인라인)
    // =======================================================================
    
    int getDeviceId() const { return device_id_; }
    void setDeviceId(int device_id) { 
        device_id_ = device_id; 
        setId(device_id);
        markModified();
    }
    
    int getPollingIntervalMs() const { return polling_interval_ms_; }
    void setPollingIntervalMs(int interval_ms) { 
        if (interval_ms > 0) {
            polling_interval_ms_ = interval_ms; 
            markModified();
        }
    }
    
    int getConnectionTimeoutMs() const { return connection_timeout_ms_; }
    void setConnectionTimeoutMs(int timeout_ms) { 
        if (timeout_ms > 0) {
            connection_timeout_ms_ = timeout_ms; 
            markModified();
        }
    }
    
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
    
    int getBackoffTimeMs() const { return backoff_time_ms_; }
    void setBackoffTimeMs(int backoff_time_ms) { 
        if (backoff_time_ms > 0) {
            backoff_time_ms_ = backoff_time_ms; 
            markModified();
        }
    }
    
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
    
    std::optional<int> getScanRateOverride() const { return scan_rate_override_; }
    void setScanRateOverride(const std::optional<int>& scan_rate_override) { 
        scan_rate_override_ = scan_rate_override; 
        markModified();
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
    
    double getBackoffMultiplier() const { return backoff_multiplier_; }
    void setBackoffMultiplier(double backoff_multiplier) { 
        if (backoff_multiplier > 0.0) {
            backoff_multiplier_ = backoff_multiplier; 
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
    
    int getKeepAliveTimeoutS() const { return keep_alive_timeout_s_; }
    void setKeepAliveTimeoutS(int keep_alive_timeout_s) { 
        if (keep_alive_timeout_s > 0) {
            keep_alive_timeout_s_ = keep_alive_timeout_s; 
            markModified();
        }
    }
    
    bool isDataValidationEnabled() const { return data_validation_enabled_; }
    void setDataValidationEnabled(bool data_validation_enabled) { 
        data_validation_enabled_ = data_validation_enabled; 
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
    
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }

    // =======================================================================
    // 유효성 검사
    // =======================================================================
    
    bool isValid() const override {
        if (!BaseEntity<DeviceSettingsEntity>::isValid()) return false;
        if (device_id_ <= 0) return false;
        if (polling_interval_ms_ <= 0) return false;
        if (connection_timeout_ms_ <= 0) return false;
        if (max_retry_count_ < 0) return false;
        if (retry_interval_ms_ <= 0) return false;
        if (backoff_time_ms_ <= 0) return false;
        if (keep_alive_interval_s_ <= 0) return false;
        if (read_timeout_ms_ <= 0) return false;
        if (write_timeout_ms_ <= 0) return false;
        if (backoff_multiplier_ <= 0.0) return false;
        if (max_backoff_time_ms_ <= 0) return false;
        if (keep_alive_timeout_s_ <= 0) return false;
        return true;
    }

    // =======================================================================
    // 프리셋 설정 메서드들
    // =======================================================================
    
    void resetToIndustrialDefaults() {
        polling_interval_ms_ = 1000;
        connection_timeout_ms_ = 10000;
        max_retry_count_ = 3;
        retry_interval_ms_ = 5000;
        backoff_time_ms_ = 60000;
        keep_alive_enabled_ = true;
        keep_alive_interval_s_ = 30;
        read_timeout_ms_ = 5000;
        write_timeout_ms_ = 5000;
        backoff_multiplier_ = 1.5;
        max_backoff_time_ms_ = 300000;
        keep_alive_timeout_s_ = 10;
        data_validation_enabled_ = true;
        performance_monitoring_enabled_ = true;
        diagnostic_mode_enabled_ = false;
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
    // 멤버 변수들
    // =======================================================================
    
    int device_id_;
    int polling_interval_ms_;
    int connection_timeout_ms_;
    int max_retry_count_;
    int retry_interval_ms_;
    int backoff_time_ms_;
    bool keep_alive_enabled_;
    int keep_alive_interval_s_;
    std::optional<int> scan_rate_override_;
    int read_timeout_ms_;
    int write_timeout_ms_;
    double backoff_multiplier_;
    int max_backoff_time_ms_;
    int keep_alive_timeout_s_;
    bool data_validation_enabled_;
    bool performance_monitoring_enabled_;
    bool diagnostic_mode_enabled_;
    std::chrono::system_clock::time_point updated_at_;
};

} // namespace Entities
} // namespace Database  
} // namespace PulseOne

#endif // DEVICE_SETTINGS_ENTITY_