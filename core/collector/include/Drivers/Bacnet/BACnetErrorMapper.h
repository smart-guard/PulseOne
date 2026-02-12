// =============================================================================
// collector/include/Drivers/Bacnet/BACnetErrorMapper.h
// 🔥 BACnet 에러 매핑 - 기존 DriverError 시스템 활용
// =============================================================================

#ifndef BACNET_ERROR_MAPPER_H
#define BACNET_ERROR_MAPPER_H

#include "Common/DriverError.h"
#include "Common/Enums.h"
#include "Drivers/Bacnet/BACnetTypes.h"
#include <string>
#include <map>

#ifdef HAVE_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacerror.h>
}

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif


#endif

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 에러 매핑 클래스
 * 
 * 기존 DriverError 시스템을 활용하여 BACnet 특화 에러를 
 * PulseOne 표준 에러 코드로 변환합니다.
 */
class BACnetErrorMapper {
public:
    // ==========================================================================
    // 싱글톤 패턴 (기존 스타일 유지)
    // ==========================================================================
    static BACnetErrorMapper& getInstance() {
        static BACnetErrorMapper instance;
        return instance;
    }
    
    // 복사/이동 방지
    BACnetErrorMapper(const BACnetErrorMapper&) = delete;
    BACnetErrorMapper& operator=(const BACnetErrorMapper&) = delete;
    BACnetErrorMapper(BACnetErrorMapper&&) = delete;
    BACnetErrorMapper& operator=(BACnetErrorMapper&&) = delete;
    
    // ==========================================================================
    // 🔥 BACnet 에러 변환 메서드들
    // ==========================================================================
    
    /**
     * @brief BACnet 에러 클래스/코드를 PulseOne 에러로 변환
     */
    Enums::ErrorCode MapBACnetError(uint8_t error_class, uint8_t error_code) const {
#ifdef HAVE_BACNET_STACK
        // BACnet Error Class 기반 매핑
        switch (error_class) {
            case ERROR_CLASS_DEVICE:
                return Enums::ErrorCode::DEVICE_ERROR;
                
            case ERROR_CLASS_OBJECT:
                return Enums::ErrorCode::DEVICE_NOT_FOUND; // 객체를 찾을 수 없음
                
            case ERROR_CLASS_PROPERTY:
                return Enums::ErrorCode::INVALID_PARAMETER;
                
            case ERROR_CLASS_RESOURCES:
                return Enums::ErrorCode::RESOURCE_EXHAUSTED;
                
            case ERROR_CLASS_SECURITY:
                return Enums::ErrorCode::AUTHENTICATION_FAILED;
                
            case ERROR_CLASS_SERVICES:
                return Enums::ErrorCode::UNSUPPORTED_FUNCTION;
                
            case ERROR_CLASS_VT:
                return Enums::ErrorCode::UNSUPPORTED_FUNCTION;
                
            case ERROR_CLASS_COMMUNICATION:
                return Enums::ErrorCode::INVALID_ENDPOINT;
                
            default:
                // Error Code 기반 세부 매핑
                switch (error_code) {
                    case ERROR_CODE_UNKNOWN_OBJECT:
                        return Enums::ErrorCode::DEVICE_NOT_FOUND;
                    case ERROR_CODE_UNKNOWN_PROPERTY:
                        return Enums::ErrorCode::CONFIGURATION_ERROR;
                    case ERROR_CODE_UNSUPPORTED_OBJECT_TYPE:
                        return Enums::ErrorCode::PROTOCOL_ERROR;
                    case ERROR_CODE_VALUE_OUT_OF_RANGE:
                        return Enums::ErrorCode::DATA_OUT_OF_RANGE;
                    case ERROR_CODE_WRITE_ACCESS_DENIED:
                        return Enums::ErrorCode::INSUFFICIENT_PERMISSION;
                    case ERROR_CODE_DEVICE_BUSY:
                        return Enums::ErrorCode::MAINTENANCE_ACTIVE;
                    case ERROR_CODE_COMMUNICATION_DISABLED:
                        return Enums::ErrorCode::CONNECTION_FAILED;
                    case ERROR_CODE_TIMEOUT:
                        return Enums::ErrorCode::CONNECTION_TIMEOUT;
                    default:
                        return Enums::ErrorCode::PROTOCOL_ERROR;
                }
        }
#else
        (void)error_class; (void)error_code;
        return Enums::ErrorCode::PROTOCOL_ERROR;
#endif
    }
    
    /**
     * @brief BACnet Reject Reason 매핑
     */
    Enums::ErrorCode MapRejectReason(uint8_t reject_reason) const {
#ifdef HAVE_BACNET_STACK
        switch (reject_reason) {
            case REJECT_REASON_OTHER:
                return Enums::ErrorCode::PROTOCOL_ERROR;
            case REJECT_REASON_BUFFER_OVERFLOW:
                return Enums::ErrorCode::RESOURCE_EXHAUSTED;
            case REJECT_REASON_INCONSISTENT_PARAMETERS:
                return Enums::ErrorCode::CONFIGURATION_ERROR;
            case REJECT_REASON_INVALID_PARAMETER_DATA_TYPE:
                return Enums::ErrorCode::DATA_TYPE_MISMATCH;
            case REJECT_REASON_INVALID_TAG:
                return Enums::ErrorCode::DATA_FORMAT_ERROR;
            case REJECT_REASON_MISSING_REQUIRED_PARAMETER:
                return Enums::ErrorCode::CONFIGURATION_ERROR;
            case REJECT_REASON_PARAMETER_OUT_OF_RANGE:
                return Enums::ErrorCode::DATA_OUT_OF_RANGE;
            case REJECT_REASON_TOO_MANY_ARGUMENTS:
                return Enums::ErrorCode::CONFIGURATION_ERROR;
            case REJECT_REASON_UNDEFINED_ENUMERATION:
                return Enums::ErrorCode::DATA_FORMAT_ERROR;
            case REJECT_REASON_UNRECOGNIZED_SERVICE:
                return Enums::ErrorCode::PROTOCOL_ERROR;
            default:
                return Enums::ErrorCode::PROTOCOL_ERROR;
        }
#else
        (void)reject_reason;
        return Enums::ErrorCode::PROTOCOL_ERROR;
#endif
    }
    
    /**
     * @brief BACnet Abort Reason 매핑
     */
    Enums::ErrorCode MapAbortReason(uint8_t abort_reason) const {
#ifdef HAVE_BACNET_STACK
        switch (abort_reason) {
            case ABORT_REASON_OTHER:
                return Enums::ErrorCode::PROTOCOL_ERROR;
            case ABORT_REASON_BUFFER_OVERFLOW:
                return Enums::ErrorCode::RESOURCE_EXHAUSTED;
            case ABORT_REASON_INVALID_APDU_IN_THIS_STATE:
                return Enums::ErrorCode::PROTOCOL_ERROR;
            case ABORT_REASON_PREEMPTED_BY_HIGHER_PRIORITY_TASK:
                return Enums::ErrorCode::MAINTENANCE_ACTIVE;
            case ABORT_REASON_SEGMENTATION_NOT_SUPPORTED:
                return Enums::ErrorCode::PROTOCOL_ERROR;
            default:
                return Enums::ErrorCode::PROTOCOL_ERROR;
        }
#else
        (void)abort_reason;
        return Enums::ErrorCode::PROTOCOL_ERROR;
#endif
    }
    
    /**
     * @brief 소켓 에러 매핑
     */
    Enums::ErrorCode MapSocketError(int socket_error) const {
        switch (socket_error) {
            case 0:
                return Enums::ErrorCode::SUCCESS;
            case ECONNREFUSED:
                return Enums::ErrorCode::CONNECTION_FAILED;
            case ETIMEDOUT:
                return Enums::ErrorCode::CONNECTION_TIMEOUT;
            case ECONNRESET:
                return Enums::ErrorCode::CONNECTION_LOST;
            case EHOSTUNREACH:
                return Enums::ErrorCode::CONNECTION_FAILED;
            case ENETUNREACH:
                return Enums::ErrorCode::CONNECTION_FAILED;
            case ENOBUFS:
                return Enums::ErrorCode::RESOURCE_EXHAUSTED;
            default:
                return Enums::ErrorCode::CONNECTION_FAILED;
        }
    }
    
    /**
     * @brief 에러 코드 이름 반환
     */
    std::string GetErrorCodeName(uint8_t error_class, uint8_t error_code) const {
#ifdef HAVE_BACNET_STACK
        std::string class_name = "Class_" + std::to_string(error_class);
        std::string code_name = "Code_" + std::to_string(error_code);
        
        // 알려진 에러 클래스 이름 매핑
        switch (error_class) {
            case ERROR_CLASS_DEVICE: class_name = "DEVICE"; break;
            case ERROR_CLASS_OBJECT: class_name = "OBJECT"; break;
            case ERROR_CLASS_PROPERTY: class_name = "PROPERTY"; break;
            case ERROR_CLASS_RESOURCES: class_name = "RESOURCES"; break;
            case ERROR_CLASS_SECURITY: class_name = "SECURITY"; break;
            case ERROR_CLASS_SERVICES: class_name = "SERVICES"; break;
            case ERROR_CLASS_VT: class_name = "VT"; break;
            case ERROR_CLASS_COMMUNICATION: class_name = "COMMUNICATION"; break;
        }
        
        // 알려진 에러 코드 이름 매핑
        switch (error_code) {
            case ERROR_CODE_UNKNOWN_OBJECT: code_name = "UNKNOWN_OBJECT"; break;
            case ERROR_CODE_UNKNOWN_PROPERTY: code_name = "UNKNOWN_PROPERTY"; break;
            case ERROR_CODE_UNSUPPORTED_OBJECT_TYPE: code_name = "UNSUPPORTED_OBJECT_TYPE"; break;
            case ERROR_CODE_VALUE_OUT_OF_RANGE: code_name = "VALUE_OUT_OF_RANGE"; break;
            case ERROR_CODE_WRITE_ACCESS_DENIED: code_name = "WRITE_ACCESS_DENIED"; break;
            case ERROR_CODE_DEVICE_BUSY: code_name = "DEVICE_BUSY"; break;
            case ERROR_CODE_COMMUNICATION_DISABLED: code_name = "COMMUNICATION_DISABLED"; break;
            case ERROR_CODE_TIMEOUT: code_name = "TIMEOUT"; break;
        }
        
        return class_name + "::" + code_name;
#else
        return "Class_" + std::to_string(error_class) + "::Code_" + std::to_string(error_code);
#endif
    }
    
    /**
     * @brief 통합 에러 정보 생성 (기존 ErrorInfo 활용)
     */
    Structs::ErrorInfo CreateBACnetError(const std::string& message, 
                                        uint8_t error_class = 0, 
                                        uint8_t error_code = 0,
                                        const std::string& context = "") const {
        Enums::ErrorCode mapped_code = MapBACnetError(error_class, error_code);
        
        std::string full_message = message;
        if (error_class != 0 || error_code != 0) {
            full_message += " [" + GetErrorCodeName(error_class, error_code) + "]";
        }
        
        Structs::ErrorInfo error_info(static_cast<Structs::ErrorCode>(mapped_code), full_message);
        
        // BACnet 특화 정보 추가 (DriverError 시스템 활용)
        if (error_class != 0) {
            error_info.additional_info["bacnet_error_class"] = std::to_string(error_class);
        }
        if (error_code != 0) {
            error_info.additional_info["bacnet_error_code"] = std::to_string(error_code);
        }
        if (!context.empty()) {
            error_info.context = context;
        }
        error_info.protocol = "BACnet";
        
        return error_info;
    }

    friend class std::default_delete<BACnetErrorMapper>;
    ~BACnetErrorMapper() = default;
private:
    BACnetErrorMapper() = default;
    
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_ERROR_MAPPER_H