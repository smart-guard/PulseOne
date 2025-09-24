/**
 * @file TargetTypes.h
 * @brief CSP Gateway 통합 타겟 전송 결과 타입 정의
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 1.0.0
 * 저장 위치: core/export-gateway/include/CSP/TargetTypes.h
 * 
 * 목적: 기존 CSPDynamicTargets.h의 TargetSendResult 중복 정의 문제 해결
 * 기반: 현재 실제 사용 중인 필드명 (컴파일 에러 분석 결과)
 */

#ifndef CSP_TARGET_TYPES_H
#define CSP_TARGET_TYPES_H

#include <string>
#include <chrono>
#include <vector>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace CSP {

using json = nlohmann::json;

/**
 * @brief 타겟 전송 결과 - 모든 Handler에서 공통 사용
 * 
 * 실제 사용 중인 필드명 기준으로 정의:
 * - status_code (http_status_code 아님)
 * - response_body (response_data 아님) 
 * - s3_object_key (존재함)
 * - file_path (존재함)
 */
struct TargetSendResult {
    // 기본 결과 필드들
    bool success = false;
    std::string error_message = "";
    std::chrono::milliseconds response_time{0};
    size_t content_size = 0;
    int retry_count = 0;
    
    // 타겟 정보
    std::string target_name = "";
    std::string target_type = "";
    
    // HTTP 관련 필드들 (기존 프로젝트 실제 필드명)
    int status_code = 0;                    // ✅ 실제 필드명
    std::string response_body = "";         // ✅ 실제 필드명
    
    // 파일/경로 관련 필드들 (기존 프로젝트 실제 필드명)
    std::string file_path = "";             // ✅ 파일 타겟용
    std::string s3_object_key = "";         // ✅ S3 타겟용  
    std::string mqtt_topic = "";            // ✅ MQTT 타겟용
    
    /**
     * @brief HTTP 성공 여부 확인
     */
    bool isHttpSuccess() const {
        return status_code >= 200 && status_code < 300;
    }
    
    /**
     * @brief 클라이언트 에러 여부 확인
     */
    bool isClientError() const {
        return status_code >= 400 && status_code < 500;
    }
    
    /**
     * @brief 서버 에러 여부 확인
     */
    bool isServerError() const {
        return status_code >= 500 && status_code < 600;
    }
    
    /**
     * @brief JSON 변환
     */
    json toJson() const {
        return json{
            {"success", success},
            {"error_message", error_message},
            {"response_time_ms", response_time.count()},
            {"content_size", content_size},
            {"retry_count", retry_count},
            {"target_name", target_name},
            {"target_type", target_type},
            {"status_code", status_code},
            {"response_body", response_body},
            {"file_path", file_path},
            {"s3_object_key", s3_object_key},
            {"mqtt_topic", mqtt_topic}
        };
    }
};

/**
 * @brief 배치 타겟 결과
 */
struct BatchTargetResult {
    int building_id = 0;
    size_t total_targets = 0;
    size_t successful_targets = 0;
    size_t failed_targets = 0;
    std::chrono::milliseconds total_time{0};
    std::vector<TargetSendResult> target_results;
    
    double getSuccessRate() const {
        return total_targets > 0 ? 
            (static_cast<double>(successful_targets) / total_targets * 100.0) : 0.0;
    }
    
    json toJson() const {
        json result_array = json::array();
        for (const auto& target_result : target_results) {
            result_array.push_back(target_result.toJson());
        }
        
        return json{
            {"building_id", building_id},
            {"total_targets", total_targets},
            {"successful_targets", successful_targets},
            {"failed_targets", failed_targets},
            {"success_rate", getSuccessRate()},
            {"total_time_ms", total_time.count()},
            {"target_results", result_array}
        };
    }
};

} // namespace CSP
} // namespace PulseOne

#endif // CSP_TARGET_TYPES_H