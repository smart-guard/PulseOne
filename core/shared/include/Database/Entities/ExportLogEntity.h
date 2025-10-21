/**
 * @file ExportLogEntity.h
 * @brief Export Log 엔티티 (전송 로그 저장용)
 * @version 3.0.0 - 완전 리팩토링
 * @date 2025-10-21
 * 
 * 저장 위치: core/shared/include/Database/Entities/ExportLogEntity.h
 * 
 * 역할:
 *   - 모든 export 전송 로그를 저장
 *   - 통계 집계의 원천 데이터
 *   - export_targets의 통계 필드를 대체
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

// Forward declarations
namespace Repositories {
    class ExportLogRepository;
}

namespace Entities {

using json = nlohmann::json;

/**
 * @class ExportLogEntity
 * @brief Export 전송 로그 엔티티
 * 
 * 매 전송마다 INSERT되며, 통계 집계의 원천 데이터로 사용됨
 */
class ExportLogEntity : public BaseEntity<ExportLogEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ExportLogEntity();
    explicit ExportLogEntity(int id);
    virtual ~ExportLogEntity();
    
    // =======================================================================
    // Repository 패턴 지원
    // =======================================================================
    
    std::shared_ptr<Repositories::ExportLogRepository> getRepository() const;
    
    // =======================================================================
    // Getter (inline 허용)
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
    // Setter (inline 허용)
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
    // 비즈니스 로직 (선언만, cpp에서 구현)
    // =======================================================================
    
    /**
     * @brief 유효성 검사
     * @return true if valid, false otherwise
     */
    bool validate() const;
    
    /**
     * @brief 성공 여부 확인
     * @return true if status is 'success', false otherwise
     */
    bool isSuccess() const;
    
    /**
     * @brief 실패 여부 확인
     * @return true if status is 'failed', false otherwise
     */
    bool isFailure() const;
    
    /**
     * @brief 엔티티 타입명 반환
     */
    std::string getEntityTypeName() const;
    
    // =======================================================================
    // JSON 직렬화 (선언만, cpp에서 구현)
    // =======================================================================
    
    json toJson() const override;
    bool fromJson(const json& data) override;
    std::string toString() const override;
    
    // =======================================================================
    // DB 연산 (선언만, cpp에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    
    std::string getTableName() const override { 
        return "export_logs"; 
    }

private:
    // =======================================================================
    // 멤버 변수 (DB 스키마와 완전 일치)
    // =======================================================================
    
    std::string log_type_;                                      // 로그 타입
    int service_id_ = 0;                                        // FK (nullable)
    int target_id_ = 0;                                         // FK (nullable)
    int mapping_id_ = 0;                                        // FK (nullable)
    int point_id_ = 0;                                          // FK (nullable)
    std::string source_value_;                                  // 원본 값
    std::string converted_value_;                               // 변환된 값
    std::string status_;                                        // 'success', 'failed'
    std::string error_message_;                                 // 에러 메시지
    std::string error_code_;                                    // 에러 코드
    std::string response_data_;                                 // 응답 데이터
    int http_status_code_ = 0;                                  // HTTP 상태 코드
    int processing_time_ms_ = 0;                                // 처리 시간 (ms)
    std::chrono::system_clock::time_point timestamp_;           // 로그 시각
    std::string client_info_;                                   // 클라이언트 정보
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_LOG_ENTITY_H