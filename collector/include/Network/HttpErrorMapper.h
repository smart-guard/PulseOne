// =============================================================================
// collector/include/Network/HttpErrorMapper.h
// ErrorCode → HTTP Status Code 매핑 및 에러 상세 정보 제공
// =============================================================================

#ifndef PULSEONE_HTTP_ERROR_MAPPER_H
#define PULSEONE_HTTP_ERROR_MAPPER_H

#include "Common/Enums.h"
#include <string>
#include <unordered_map>
#include <chrono>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace PulseOne {
namespace Network {

/**
 * @brief 세분화된 ErrorCode → HTTP Status Code 매핑
 */
class HttpErrorMapper {
public:
    // 에러 상세 정보 구조체
    struct ErrorDetail {
        int http_status;           // HTTP 상태코드
        std::string error_type;    // 에러 타입 (connection, device, protocol 등)
        std::string severity;      // 심각도 (low, medium, high, critical)
        std::string category;      // 카테고리 (network, hardware, software)
        bool recoverable;          // 자동 복구 가능 여부
        bool user_actionable;      // 사용자 조치 필요 여부
        std::string action_hint;   // 사용자 조치 힌트
        std::string tech_details;  // 기술적 상세 정보 (엔지니어용)
    };

    // 싱글톤 패턴
    static HttpErrorMapper& getInstance() {
        static HttpErrorMapper instance;
        return instance;
    }
    
    /**
     * @brief ClassifyHardwareError의 문자열 결과를 ErrorCode로 변환
     * @param error_str ClassifyHardwareError에서 반환된 에러 분류 문자열
     * @return 대응하는 ErrorCode enum 값
     */
    PulseOne::Enums::ErrorCode ParseErrorString(const std::string& error_str) const {
        static const std::unordered_map<std::string, PulseOne::Enums::ErrorCode> stringToEnum = {
            // 연결 관련
            {"HARDWARE_CONNECTION_FAILED", PulseOne::Enums::ErrorCode::CONNECTION_FAILED},
            {"HARDWARE_TIMEOUT", PulseOne::Enums::ErrorCode::CONNECTION_TIMEOUT},
            {"CONNECTION_REFUSED", PulseOne::Enums::ErrorCode::CONNECTION_REFUSED},
            {"CONNECTION_LOST", PulseOne::Enums::ErrorCode::CONNECTION_LOST},
            
            // Modbus 관련
            {"MODBUS_SLAVE_NO_RESPONSE", PulseOne::Enums::ErrorCode::DEVICE_NOT_RESPONDING},
            {"MODBUS_CRC_ERROR", PulseOne::Enums::ErrorCode::CHECKSUM_ERROR},
            {"MODBUS_PROTOCOL_ERROR", PulseOne::Enums::ErrorCode::MODBUS_EXCEPTION},
            
            // MQTT 관련
            {"MQTT_BROKER_UNREACHABLE", PulseOne::Enums::ErrorCode::CONNECTION_FAILED},
            {"MQTT_AUTH_FAILED", PulseOne::Enums::ErrorCode::AUTHENTICATION_FAILED},
            {"MQTT_CONNECTION_ERROR", PulseOne::Enums::ErrorCode::MQTT_PUBLISH_FAILED},
            
            // Worker 관련
            {"WORKER_ALREADY_RUNNING", PulseOne::Enums::ErrorCode::DEVICE_BUSY},
            {"WORKER_NOT_FOUND", PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND},
            {"WORKER_OPERATION_FAILED", PulseOne::Enums::ErrorCode::DEVICE_ERROR},
            
            // 디바이스 관련
            {"DEVICE_NOT_FOUND", PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND},
            
            // 권한 관련
            {"PERMISSION_DENIED", PulseOne::Enums::ErrorCode::INSUFFICIENT_PERMISSION},
            
            // 설정 관련
            {"CONFIGURATION_ERROR", PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR},
            
            // 기본값
            {"COLLECTOR_INTERNAL_ERROR", PulseOne::Enums::ErrorCode::INTERNAL_ERROR}
        };
        
        auto it = stringToEnum.find(error_str);
        return (it != stringToEnum.end()) ? it->second : PulseOne::Enums::ErrorCode::INTERNAL_ERROR;
    }
    
    /**
     * @brief 세분화된 ErrorCode → HTTP Status Code 매핑
     */
    int MapErrorToHttpStatus(PulseOne::Enums::ErrorCode error_code) const {
        switch (error_code) {
            // ===== 200: 성공 =====
            case PulseOne::Enums::ErrorCode::SUCCESS:
                return 200;
            
            // ===== 연결 관련 에러 (420-429) =====
            case PulseOne::Enums::ErrorCode::CONNECTION_FAILED:
                return 420;
            case PulseOne::Enums::ErrorCode::CONNECTION_TIMEOUT:
                return 421;
            case PulseOne::Enums::ErrorCode::CONNECTION_REFUSED:
                return 422;
            case PulseOne::Enums::ErrorCode::CONNECTION_LOST:
                return 423;
            case PulseOne::Enums::ErrorCode::AUTHENTICATION_FAILED:
                return 424;
            case PulseOne::Enums::ErrorCode::AUTHORIZATION_FAILED:
                return 425;
            
            // ===== 통신/프로토콜 관련 에러 (430-449) =====
            case PulseOne::Enums::ErrorCode::TIMEOUT:
                return 430;
            case PulseOne::Enums::ErrorCode::PROTOCOL_ERROR:
                return 431;
            case PulseOne::Enums::ErrorCode::INVALID_REQUEST:
                return 432;
            case PulseOne::Enums::ErrorCode::INVALID_RESPONSE:
                return 433;
            case PulseOne::Enums::ErrorCode::CHECKSUM_ERROR:
                return 434;
            case PulseOne::Enums::ErrorCode::FRAME_ERROR:
                return 435;
            
            // ===== 데이터 관련 에러 (450-469) =====
            case PulseOne::Enums::ErrorCode::INVALID_DATA:
                return 450;
            case PulseOne::Enums::ErrorCode::DATA_TYPE_MISMATCH:
                return 451;
            case PulseOne::Enums::ErrorCode::DATA_OUT_OF_RANGE:
                return 452;
            case PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR:
                return 453;
            case PulseOne::Enums::ErrorCode::DATA_STALE:
                return 454;
            
            // ===== 디바이스 관련 에러 (470-489) =====
            case PulseOne::Enums::ErrorCode::DEVICE_NOT_RESPONDING:
                return 470;
            case PulseOne::Enums::ErrorCode::DEVICE_BUSY:
                return 471;
            case PulseOne::Enums::ErrorCode::DEVICE_ERROR:
                return 472;
            case PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND:
                return 473;
            case PulseOne::Enums::ErrorCode::DEVICE_OFFLINE:
                return 474;
            
            // ===== 설정 관련 에러 (490-499) =====
            case PulseOne::Enums::ErrorCode::INVALID_CONFIGURATION:
                return 490;
            case PulseOne::Enums::ErrorCode::MISSING_CONFIGURATION:
                return 491;
            case PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR:
                return 492;
            case PulseOne::Enums::ErrorCode::INVALID_PARAMETER:
                return 493;
            
            // ===== 점검/권한 관련 에러 (510-519) =====
            case PulseOne::Enums::ErrorCode::MAINTENANCE_ACTIVE:
                return 510;
            case PulseOne::Enums::ErrorCode::MAINTENANCE_PERMISSION_DENIED:
                return 511;
            case PulseOne::Enums::ErrorCode::MAINTENANCE_TIMEOUT:
                return 512;
            case PulseOne::Enums::ErrorCode::REMOTE_CONTROL_BLOCKED:
                return 513;
            case PulseOne::Enums::ErrorCode::INSUFFICIENT_PERMISSION:
                return 514;
            
            // ===== 시스템 관련 에러 (520-529) =====
            case PulseOne::Enums::ErrorCode::MEMORY_ERROR:
                return 520;
            case PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED:
                return 521;
            case PulseOne::Enums::ErrorCode::INTERNAL_ERROR:
                return 522;
            case PulseOne::Enums::ErrorCode::FILE_ERROR:
                return 523;
            
            // ===== 프로토콜별 에러 (530+) =====
            case PulseOne::Enums::ErrorCode::MODBUS_EXCEPTION:
                return 530;
            case PulseOne::Enums::ErrorCode::MQTT_PUBLISH_FAILED:
                return 540;
            case PulseOne::Enums::ErrorCode::BACNET_SERVICE_ERROR:
                return 550;
            
            // ===== 기본값 =====
            case PulseOne::Enums::ErrorCode::UNKNOWN_ERROR:
            default:
                return 500;
        }
    }
    
    /**
     * @brief 에러코드별 상세 정보 제공
     */
    ErrorDetail GetErrorDetail(PulseOne::Enums::ErrorCode error_code) const {
        int http_status = MapErrorToHttpStatus(error_code);
        
        switch (error_code) {
            // 연결 관련
            case PulseOne::Enums::ErrorCode::CONNECTION_FAILED:
                return {http_status, "connection", "high", "network", true, false, 
                       "자동 재연결을 시도합니다", "TCP 연결 실패 - 네트워크 또는 방화벽 확인 필요"};
            
            case PulseOne::Enums::ErrorCode::CONNECTION_TIMEOUT:
                return {http_status, "connection", "medium", "network", true, false,
                       "연결 재시도 중", "응답 타임아웃 - 네트워크 지연 또는 디바이스 부하"};
            
            case PulseOne::Enums::ErrorCode::CONNECTION_REFUSED:
                return {http_status, "connection", "high", "network", false, true,
                       "디바이스 설정 확인 필요", "연결 거부됨 - 포트나 서비스 설정 확인"};
            
            case PulseOne::Enums::ErrorCode::CONNECTION_LOST:
                return {http_status, "connection", "medium", "network", true, false,
                       "자동 재연결 시도 중", "연결 끊어짐 - 네트워크 불안정 또는 디바이스 재시작"};
            
            // 디바이스 관련
            case PulseOne::Enums::ErrorCode::DEVICE_NOT_RESPONDING:
                return {http_status, "device", "high", "hardware", false, true,
                       "디바이스 전원/네트워크 확인", "디바이스 응답 없음 - 전원 OFF 또는 네트워크 문제"};
            
            case PulseOne::Enums::ErrorCode::DEVICE_BUSY:
                return {http_status, "device", "medium", "hardware", true, false,
                       "잠시 후 재시도", "다른 작업 진행 중 - 대기 후 재시도"};
            
            case PulseOne::Enums::ErrorCode::DEVICE_ERROR:
                return {http_status, "device", "high", "hardware", false, true,
                       "디바이스 상태 점검 필요", "디바이스 내부 에러 - 하드웨어 점검 필요"};
            
            case PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND:
                return {http_status, "device", "high", "configuration", false, true,
                       "디바이스 설정 확인", "디바이스 찾을 수 없음 - 설정 또는 주소 확인"};
            
            case PulseOne::Enums::ErrorCode::DEVICE_OFFLINE:
                return {http_status, "device", "high", "hardware", false, true,
                       "디바이스 전원 확인", "디바이스 오프라인 - 전원 및 연결 상태 확인"};
            
            // 데이터 관련
            case PulseOne::Enums::ErrorCode::INVALID_DATA:
                return {http_status, "data", "medium", "software", false, true,
                       "입력값 확인", "잘못된 데이터 형식 또는 값"};
            
            case PulseOne::Enums::ErrorCode::DATA_TYPE_MISMATCH:
                return {http_status, "data", "medium", "software", false, true,
                       "데이터 타입 확인", "예상 타입과 다른 데이터 수신"};
            
            case PulseOne::Enums::ErrorCode::DATA_OUT_OF_RANGE:
                return {http_status, "data", "medium", "software", false, true,
                       "값 범위 확인", "허용 범위를 벗어난 데이터"};
            
            case PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR:
                return {http_status, "data", "medium", "software", false, true,
                       "데이터 포맷 확인", "잘못된 데이터 포맷 또는 인코딩"};
            
            case PulseOne::Enums::ErrorCode::DATA_STALE:
                return {http_status, "data", "low", "software", true, false,
                       "데이터 업데이트 대기", "오래된 데이터 - 새 데이터 수집 중"};
            
            // 프로토콜 관련
            case PulseOne::Enums::ErrorCode::PROTOCOL_ERROR:
                return {http_status, "protocol", "medium", "software", true, false,
                       "프로토콜 설정 재확인", "프로토콜 에러 - 설정 또는 버전 호환성 문제"};
            
            case PulseOne::Enums::ErrorCode::CHECKSUM_ERROR:
                return {http_status, "protocol", "medium", "network", true, false,
                       "네트워크 품질 확인", "체크섬 에러 - 네트워크 노이즈 또는 케이블 문제"};
            
            case PulseOne::Enums::ErrorCode::FRAME_ERROR:
                return {http_status, "protocol", "medium", "network", true, false,
                       "통신 케이블 점검", "프레임 에러 - 통신 라인 품질 문제"};
            
            // 점검/권한 관련
            case PulseOne::Enums::ErrorCode::MAINTENANCE_ACTIVE:
                return {http_status, "maintenance", "low", "operational", true, false,
                       "점검 완료까지 대기", "점검 모드 활성화 - 점검 완료 후 자동 복구"};
            
            case PulseOne::Enums::ErrorCode::REMOTE_CONTROL_BLOCKED:
                return {http_status, "permission", "medium", "security", false, true,
                       "현장 제어 모드 해제 필요", "원격 제어 차단됨 - 현장 우선 모드 또는 안전 잠금"};
            
            case PulseOne::Enums::ErrorCode::INSUFFICIENT_PERMISSION:
                return {http_status, "permission", "medium", "security", false, true,
                       "관리자 권한 요청", "권한 부족 - 해당 기능에 대한 권한 없음"};
            
            // 프로토콜별 상세 에러
            case PulseOne::Enums::ErrorCode::MODBUS_EXCEPTION:
                return {http_status, "modbus", "medium", "protocol", true, false,
                       "Modbus 설정 확인", "Modbus 예외 발생 - 슬레이브 ID 또는 레지스터 주소 확인"};
            
            case PulseOne::Enums::ErrorCode::MQTT_PUBLISH_FAILED:
                return {http_status, "mqtt", "medium", "protocol", true, false,
                       "MQTT 브로커 상태 확인", "MQTT 발행 실패 - 브로커 연결 또는 토픽 권한 확인"};
            
            case PulseOne::Enums::ErrorCode::BACNET_SERVICE_ERROR:
                return {http_status, "bacnet", "medium", "protocol", true, false,
                       "BACnet 서비스 설정 확인", "BACnet 서비스 에러 - 객체 ID 또는 속성 확인"};
            
            // 설정 관련
            case PulseOne::Enums::ErrorCode::INVALID_CONFIGURATION:
                return {http_status, "configuration", "high", "software", false, true,
                       "설정 파일 확인", "잘못된 설정 파일 또는 파라미터"};
            
            case PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR:
                return {http_status, "configuration", "high", "software", false, true,
                       "설정 재검토", "설정 로드 또는 적용 중 에러"};
            
            case PulseOne::Enums::ErrorCode::INVALID_PARAMETER:
                return {http_status, "parameter", "medium", "software", false, true,
                       "매개변수 확인", "API 호출 시 잘못된 매개변수"};
            
            // 시스템 관련
            case PulseOne::Enums::ErrorCode::MEMORY_ERROR:
                return {http_status, "system", "critical", "hardware", false, true,
                       "시스템 재시작 검토", "메모리 부족 또는 할당 실패"};
            
            case PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED:
                return {http_status, "system", "high", "software", false, true,
                       "시스템 리소스 확인", "CPU, 메모리, 파일 핸들 등 리소스 부족"};
            
            case PulseOne::Enums::ErrorCode::INTERNAL_ERROR:
                return {http_status, "system", "high", "software", false, true,
                       "로그 확인 후 재시도", "내부 로직 에러 - 개발팀 확인 필요"};
            
            case PulseOne::Enums::ErrorCode::FILE_ERROR:
                return {http_status, "system", "medium", "software", false, true,
                       "파일 권한 확인", "파일 읽기/쓰기 실패 - 권한 또는 디스크 공간 확인"};
            
            // 기본값
            case PulseOne::Enums::ErrorCode::UNKNOWN_ERROR:
            default:
                return {http_status, "unknown", "medium", "unknown", false, true,
                       "시스템 관리자 문의", "알 수 없는 에러 - 로그 분석 필요"};
        }
    }
    
    /**
     * @brief 에러 상세 정보를 JSON으로 반환 (프런트엔드용)
     */
#ifdef HAS_NLOHMANN_JSON
    nlohmann::json GetErrorInfoJson(PulseOne::Enums::ErrorCode error_code, 
                                   const std::string& device_id = "",
                                   const std::string& additional_context = "") const {
        ErrorDetail detail = GetErrorDetail(error_code);
        
        nlohmann::json error_info;
        error_info["error_code"] = static_cast<int>(error_code);
        error_info["http_status"] = detail.http_status;
        error_info["error_type"] = detail.error_type;
        error_info["severity"] = detail.severity;
        error_info["category"] = detail.category;
        error_info["recoverable"] = detail.recoverable;
        error_info["user_actionable"] = detail.user_actionable;
        error_info["action_hint"] = detail.action_hint;
        error_info["tech_details"] = detail.tech_details;
        error_info["user_message"] = GetUserFriendlyMessage(error_code, device_id);
        
        if (!device_id.empty()) {
            error_info["device_id"] = device_id;
        }
        
        if (!additional_context.empty()) {
            error_info["context"] = additional_context;
        }
        
        error_info["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        return error_info;
    }
#endif
    
    /**
     * @brief HTTP 상태코드별 에러 카테고리 분류
     */
    std::string GetErrorCategoryByHttpStatus(int http_status) const {
        if (http_status >= 420 && http_status <= 429) return "connection";
        if (http_status >= 430 && http_status <= 449) return "protocol";
        if (http_status >= 450 && http_status <= 469) return "data";
        if (http_status >= 470 && http_status <= 489) return "device";
        if (http_status >= 490 && http_status <= 499) return "configuration";
        if (http_status >= 510 && http_status <= 519) return "maintenance";
        if (http_status >= 520 && http_status <= 529) return "system";
        if (http_status >= 530 && http_status <= 539) return "modbus";
        if (http_status >= 540 && http_status <= 549) return "mqtt";
        if (http_status >= 550 && http_status <= 559) return "bacnet";
        return "unknown";
    }
    
    /**
     * @brief 사용자 친화적 메시지 생성
     */
    std::string GetUserFriendlyMessage(PulseOne::Enums::ErrorCode error_code, 
                                      const std::string& device_id = "") const {
        std::string device_info = device_id.empty() ? "디바이스" : "디바이스 '" + device_id + "'";
        
        switch (error_code) {
            // 연결 관련
            case PulseOne::Enums::ErrorCode::CONNECTION_FAILED:
                return device_info + "에 연결할 수 없습니다. (코드: 420)";
            case PulseOne::Enums::ErrorCode::CONNECTION_TIMEOUT:
                return device_info + "가 응답시간을 초과했습니다. (코드: 421)";
            case PulseOne::Enums::ErrorCode::CONNECTION_REFUSED:
                return device_info + "가 연결을 거부했습니다. (코드: 422)";
            case PulseOne::Enums::ErrorCode::CONNECTION_LOST:
                return device_info + "와의 연결이 끊어졌습니다. (코드: 423)";
            
            // 디바이스 관련
            case PulseOne::Enums::ErrorCode::DEVICE_NOT_RESPONDING:
                return device_info + "가 응답하지 않습니다. (코드: 470)";
            case PulseOne::Enums::ErrorCode::DEVICE_BUSY:
                return device_info + "가 사용 중입니다. (코드: 471)";
            case PulseOne::Enums::ErrorCode::DEVICE_ERROR:
                return device_info + "에서 하드웨어 에러가 발생했습니다. (코드: 472)";
            case PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND:
                return device_info + "를 찾을 수 없습니다. (코드: 473)";
            case PulseOne::Enums::ErrorCode::DEVICE_OFFLINE:
                return device_info + "가 오프라인 상태입니다. (코드: 474)";
            
            // 데이터 관련
            case PulseOne::Enums::ErrorCode::INVALID_DATA:
                return device_info + "에서 잘못된 데이터가 수신되었습니다. (코드: 450)";
            case PulseOne::Enums::ErrorCode::DATA_TYPE_MISMATCH:
                return device_info + "의 데이터 타입이 일치하지 않습니다. (코드: 451)";
            case PulseOne::Enums::ErrorCode::DATA_OUT_OF_RANGE:
                return device_info + "의 데이터가 범위를 초과했습니다. (코드: 452)";
            case PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR:
                return device_info + "의 데이터 포맷에 오류가 있습니다. (코드: 453)";
            
            // 프로토콜 관련
            case PulseOne::Enums::ErrorCode::MODBUS_EXCEPTION:
                return device_info + "에서 Modbus 예외가 발생했습니다. (코드: 530)";
            case PulseOne::Enums::ErrorCode::MQTT_PUBLISH_FAILED:
                return device_info + "의 MQTT 발행이 실패했습니다. (코드: 540)";
            case PulseOne::Enums::ErrorCode::BACNET_SERVICE_ERROR:
                return device_info + "의 BACnet 서비스에서 에러가 발생했습니다. (코드: 550)";
            
            // 점검 관련
            case PulseOne::Enums::ErrorCode::MAINTENANCE_ACTIVE:
                return device_info + "가 점검 중입니다. (코드: 510)";
            case PulseOne::Enums::ErrorCode::REMOTE_CONTROL_BLOCKED:
                return device_info + "의 원격 제어가 차단되었습니다. (코드: 513)";
            case PulseOne::Enums::ErrorCode::INSUFFICIENT_PERMISSION:
                return "권한이 부족합니다. (코드: 514)";
            
            // 설정 관련
            case PulseOne::Enums::ErrorCode::INVALID_CONFIGURATION:
                return device_info + "의 설정이 잘못되었습니다. (코드: 490)";
            case PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR:
                return device_info + "의 설정에서 에러가 발생했습니다. (코드: 492)";
            case PulseOne::Enums::ErrorCode::INVALID_PARAMETER:
                return "잘못된 매개변수입니다. (코드: 493)";
            
            // 시스템 관련
            case PulseOne::Enums::ErrorCode::MEMORY_ERROR:
                return "시스템 메모리 에러가 발생했습니다. (코드: 520)";
            case PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED:
                return "시스템 리소스가 부족합니다. (코드: 521)";
            case PulseOne::Enums::ErrorCode::INTERNAL_ERROR:
                return "내부 시스템 에러가 발생했습니다. (코드: 522)";
            case PulseOne::Enums::ErrorCode::FILE_ERROR:
                return "파일 처리 중 에러가 발생했습니다. (코드: 523)";
            
            default:
                return device_info + "에서 알 수 없는 오류가 발생했습니다. (코드: 500)";
        }
    }
    
    /**
     * @brief DeviceStatus → HTTP Status Code 매핑
     */
    int MapDeviceStatusToHttpStatus(PulseOne::Enums::DeviceStatus device_status) const {
        switch (device_status) {
            case PulseOne::Enums::DeviceStatus::ONLINE:
                return 200;  // OK - 정상 동작
            case PulseOne::Enums::DeviceStatus::WARNING:
                return 206;  // Partial Content - 경고 상태
            case PulseOne::Enums::DeviceStatus::MAINTENANCE:
                return 510;  // 점검 중
            case PulseOne::Enums::DeviceStatus::DEVICE_ERROR:
                return 472;  // 디바이스 에러
            case PulseOne::Enums::DeviceStatus::OFFLINE:
            default:
                return 474;  // 디바이스 오프라인
        }
    }
    
    /**
     * @brief ConnectionStatus → HTTP Status Code 매핑
     */
    int MapConnectionStatusToHttpStatus(PulseOne::Enums::ConnectionStatus connection_status) const {
        switch (connection_status) {
            case PulseOne::Enums::ConnectionStatus::CONNECTED:
                return 200;  // OK - 연결됨
            case PulseOne::Enums::ConnectionStatus::CONNECTING:
                return 102;  // Processing - 연결 중
            case PulseOne::Enums::ConnectionStatus::RECONNECTING:
                return 103;  // Early Hints - 재연결 중
            case PulseOne::Enums::ConnectionStatus::MAINTENANCE:
                return 510;  // 점검 모드
            case PulseOne::Enums::ConnectionStatus::TIMEOUT:
                return 421;  // 연결 타임아웃
            case PulseOne::Enums::ConnectionStatus::ERROR:
                return 420;  // 연결 실패
            case PulseOne::Enums::ConnectionStatus::DISCONNECTED:
            case PulseOne::Enums::ConnectionStatus::DISCONNECTING:
            default:
                return 423;  // 연결 끊어짐
        }
    }
    
    /**
     * @brief 복합 상태 분석 후 가장 적절한 HTTP 상태코드 결정
     */
    int DetermineHttpStatus(PulseOne::Enums::DeviceStatus device_status,
                           PulseOne::Enums::ConnectionStatus connection_status,
                           PulseOne::Enums::ErrorCode error_code) const {
        // 에러코드가 있으면 최우선
        if (error_code != PulseOne::Enums::ErrorCode::SUCCESS) {
            return MapErrorToHttpStatus(error_code);
        }
        
        // 디바이스 상태가 더 심각하면 디바이스 상태 우선
        int device_status_code = MapDeviceStatusToHttpStatus(device_status);
        int connection_status_code = MapConnectionStatusToHttpStatus(connection_status);
        
        // 더 높은 상태코드 (더 심각한 문제) 반환
        return std::max(device_status_code, connection_status_code);
    }

private:
    HttpErrorMapper() = default;
    ~HttpErrorMapper() = default;
    HttpErrorMapper(const HttpErrorMapper&) = delete;
    HttpErrorMapper& operator=(const HttpErrorMapper&) = delete;
};

/**
 * @brief 표준 HTTP 상태코드별 설명
 */
inline std::string GetHttpStatusMeaning(int status_code) {
    switch (status_code) {
        case 200: return "정상";
        case 206: return "부분 성공";
        
        // 연결 관련 (420-429)
        case 420: return "연결 실패";
        case 421: return "연결 타임아웃";
        case 422: return "연결 거부";
        case 423: return "연결 끊어짐";
        case 424: return "인증 실패";
        case 425: return "권한 인증 실패";
        
        // 프로토콜 관련 (430-449)
        case 430: return "타임아웃";
        case 431: return "프로토콜 에러";
        case 432: return "잘못된 요청";
        case 433: return "잘못된 응답";
        case 434: return "체크섬 에러";
        case 435: return "프레임 에러";
        case 436: return "잘못된 상태";
        
        // 데이터 관련 (450-469)
        case 450: return "잘못된 데이터";
        case 451: return "데이터 타입 불일치";
        case 452: return "데이터 범위 초과";
        case 453: return "데이터 포맷 에러";
        case 454: return "오래된 데이터";
        
        // 디바이스 관련 (470-489)
        case 470: return "디바이스 응답 없음";
        case 471: return "디바이스 사용 중";
        case 472: return "디바이스 에러";
        case 473: return "디바이스 없음";
        case 474: return "디바이스 오프라인";
        
        // 설정 관련 (490-499)
        case 490: return "잘못된 설정";
        case 491: return "설정 누락";
        case 492: return "설정 에러";
        case 493: return "잘못된 매개변수";
        
        // 점검 관련 (510-519)
        case 510: return "점검 중";
        case 511: return "점검 권한 없음";
        case 512: return "점검 타임아웃";
        case 513: return "원격 제어 차단";
        case 514: return "권한 부족";
        
        // 시스템 관련 (520-529)
        case 520: return "메모리 에러";
        case 521: return "리소스 부족";
        case 522: return "내부 에러";
        case 523: return "파일 에러";
        
        // 프로토콜별 (530+)
        case 530: return "Modbus 예외";
        case 540: return "MQTT 발행 실패";
        case 550: return "BACnet 서비스 에러";
        
        default: return "알 수 없는 에러";
    }
}

} // namespace Network
} // namespace PulseOne

#endif // PULSEONE_HTTP_ERROR_MAPPER_H