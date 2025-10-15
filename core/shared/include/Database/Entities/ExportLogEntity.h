/**
 * @file ExportLogEntity.h
 * @version 2.0.0 - 원본패턴완전유지
 */

#ifndef EXPORT_LOG_ENTITY_H
#define EXPORT_LOG_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Entities {

using json = nlohmann::json;

class ExportLogEntity : public BaseEntity<ExportLogEntity> {
public:
    // ✅ 선언만 (cpp로 이동)
    ExportLogEntity();
    explicit ExportLogEntity(int id);
    virtual ~ExportLogEntity();
    
    // ✅ Repository 패턴 (원본대로)
    std::shared_ptr<Repositories::ExportLogRepository> getRepository() const;
    
    // Getters (inline 허용)
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
    
    // Setters (inline 허용)
    void setLogType(const std::string& log_type) { log_type_ = log_type; markModified(); }
    void setServiceId(int service_id) { service_id_ = service_id; markModified(); }
    void setTargetId(int target_id) { target_id_ = target_id; markModified(); }
    void setMappingId(int mapping_id) { mapping_id_ = mapping_id; markModified(); }
    void setPointId(int point_id) { point_id_ = point_id; markModified(); }
    void setSourceValue(const std::string& value) { source_value_ = value; markModified(); }
    void setConvertedValue(const std::string& value) { converted_value_ = value; markModified(); }
    void setStatus(const std::string& status) { status_ = status; markModified(); }
    void setErrorMessage(const std::string& message) { error_message_ = message; markModified(); }
    void setErrorCode(const std::string& code) { error_code_ = code; markModified(); }
    void setResponseData(const std::string& data) { response_data_ = data; markModified(); }
    void setHttpStatusCode(int code) { http_status_code_ = code; markModified(); }
    void setProcessingTimeMs(int time_ms) { processing_time_ms_ = time_ms; markModified(); }
    void setTimestamp(const std::chrono::system_clock::time_point& time) { timestamp_ = time; markModified(); }
    void setClientInfo(const std::string& info) { client_info_ = info; markModified(); }
    
    // ✅ 비즈니스 로직 선언만 (cpp로 이동)
    bool validate() const;
    bool isSuccess() const;
    std::string getEntityTypeName() const;
    
    // ✅ JSON 직렬화 선언만 (cpp에 이미 구현 있음)
    json toJson() const override;
    bool fromJson(const json& data) override;
    std::string toString() const override;
    
    // ✅ DB 연산 선언만 (cpp에 이미 구현 있음)
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    
    std::string getTableName() const override { return "export_logs"; }

private:
    std::string log_type_;
    int service_id_ = 0;
    int target_id_ = 0;
    int mapping_id_ = 0;
    int point_id_ = 0;
    std::string source_value_;
    std::string converted_value_;
    std::string status_;
    std::string error_message_;
    std::string error_code_;
    std::string response_data_;
    int http_status_code_ = 0;
    int processing_time_ms_ = 0;
    std::chrono::system_clock::time_point timestamp_ = std::chrono::system_clock::now();
    std::string client_info_;
};

}
}
}

#endif