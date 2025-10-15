/**
 * @file ExportLogEntity.h
 * @brief Export Log Entity - BaseEntity 패턴 적용
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/include/Database/Entities/ExportLogEntity.h
 * 
 * 테이블: export_logs
 * 기능: Export 전송 로그 및 결과 기록
 */

#ifndef EXPORT_LOG_ENTITY_H
#define EXPORT_LOG_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>  // 🔧 추가

namespace PulseOne {
namespace Database {
namespace Entities {

using json = nlohmann::json;  // 🔧 추가

/**
 * @brief Export Log Entity
 * 
 * export_logs 테이블과 매핑
 */
class ExportLogEntity : public BaseEntity<ExportLogEntity> {
public:
    // =======================================================================
    // 생성자
    // =======================================================================
    
    ExportLogEntity() = default;
    
    explicit ExportLogEntity(int id) : BaseEntity(id) {}
    
    virtual ~ExportLogEntity() = default;
    
    // =======================================================================
    // Getters
    // =======================================================================
    
    std::string getLogType() const { return log_type_; }
    int getServiceId() const { return service_id_; }
    int getTargetId() const { return target_id_; }
    int getMappingId() const { return mapping_id_; }
    int getPointId() const { return point_id_; }
    std::string getSourceValue() const { return source_value_; }
    std::string getConvertedValue() const { return converted_value_; }
    std::string getStatus() const { return status_; }
    std::string getErrorMessage() const { return error_message_; }
    std::string getErrorCode() const { return error_code_; }
    std::string getResponseData() const { return response_data_; }
    int getHttpStatusCode() const { return http_status_code_; }
    int getProcessingTimeMs() const { return processing_time_ms_; }
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    std::string getClientInfo() const { return client_info_; }
    
    // =======================================================================
    // Setters
    // =======================================================================
    
    void setLogType(const std::string& log_type) { 
        log_type_ = log_type; 
        markModified(); 
    }
    
    void setServiceId(int service_id) { 
        service_id_ = service_id; 
        markModified(); 
    }
    
    void setTargetId(int target_id) { 
        target_id_ = target_id; 
        markModified(); 
    }
    
    void setMappingId(int mapping_id) { 
        mapping_id_ = mapping_id; 
        markModified(); 
    }
    
    void setPointId(int point_id) { 
        point_id_ = point_id; 
        markModified(); 
    }
    
    void setSourceValue(const std::string& value) { 
        source_value_ = value; 
        markModified(); 
    }
    
    void setConvertedValue(const std::string& value) { 
        converted_value_ = value; 
        markModified(); 
    }
    
    void setStatus(const std::string& status) { 
        status_ = status; 
        markModified(); 
    }
    
    void setErrorMessage(const std::string& message) { 
        error_message_ = message; 
        markModified(); 
    }
    
    void setErrorCode(const std::string& code) { 
        error_code_ = code; 
        markModified(); 
    }
    
    void setResponseData(const std::string& data) { 
        response_data_ = data; 
        markModified(); 
    }
    
    void setHttpStatusCode(int code) { 
        http_status_code_ = code; 
        markModified(); 
    }
    
    void setProcessingTimeMs(int time_ms) { 
        processing_time_ms_ = time_ms; 
        markModified(); 
    }
    
    void setTimestamp(const std::chrono::system_clock::time_point& time) { 
        timestamp_ = time; 
        markModified(); 
    }
    
    void setClientInfo(const std::string& info) { 
        client_info_ = info; 
        markModified(); 
    }
    
    // =======================================================================
    // 비즈니스 로직
    // =======================================================================
    
    /**
     * @brief 성공 여부
     */
    bool isSuccess() const {
        return status_ == "success";
    }
    
    /**
     * @brief 유효성 검증
     */
    bool validate() const {  // 🔧 override 제거
        if (log_type_.empty()) return false;
        if (status_.empty()) return false;
        return true;
    }
    
    /**
     * @brief JSON 변환
     */
    json toJson() const override {  // 🔧 반환타입 변경
        json j;
        j["id"] = id_;
        j["log_type"] = log_type_;
        j["service_id"] = service_id_;
        j["target_id"] = target_id_;
        j["mapping_id"] = mapping_id_;
        j["point_id"] = point_id_;
        j["source_value"] = source_value_;
        j["converted_value"] = converted_value_;
        j["status"] = status_;
        j["error_message"] = error_message_;
        j["error_code"] = error_code_;
        j["response_data"] = response_data_;
        j["http_status_code"] = http_status_code_;
        j["processing_time_ms"] = processing_time_ms_;
        j["timestamp"] = std::chrono::system_clock::to_time_t(timestamp_);
        j["client_info"] = client_info_;
        
        return j;  // 🔧 변경
    }
    
    // 🔧 추가: BaseEntity 순수 가상 함수
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    bool fromJson(const json& data) override;
    std::string toString() const override { return toJson().dump(2); }
    
    /**
     * @brief 엔티티 타입 이름
     */
    std::string getEntityTypeName() const {  // 🔧 override 제거
        return "ExportLog";
    }
    std::string getTableName() const override { return "export_logs"; }

private:
    std::string log_type_;  // "export", "protocol", "mapping"
    int service_id_ = 0;
    int target_id_ = 0;
    int mapping_id_ = 0;
    int point_id_ = 0;
    std::string source_value_;
    std::string converted_value_;
    std::string status_;  // "success", "failed", "retry"
    std::string error_message_;
    std::string error_code_;
    std::string response_data_;
    int http_status_code_ = 0;
    int processing_time_ms_ = 0;
    std::chrono::system_clock::time_point timestamp_ = std::chrono::system_clock::now();
    std::string client_info_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_LOG_ENTITY_H