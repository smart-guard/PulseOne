/**
 * @file DeviceEntity.cpp
 * @brief PulseOne DeviceEntity 구현 - 현재 DB 스키마 v2.1.0 완전 대응
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 🔥 현재 DB 스키마 완전 적용:
 * - protocol_type → protocol_id (외래키)
 * - 새로운 컬럼들: polling_interval, timeout, retry_count
 * - Repository 패턴 유지 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현
 */

#include "Database/Entities/DeviceEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 - 새로운 필드들 초기화
// =============================================================================

DeviceEntity::DeviceEntity() 
    : BaseEntity<DeviceEntity>()
    , tenant_id_(0)
    , site_id_(0)
    , device_group_id_(std::nullopt)
    , edge_server_id_(std::nullopt)
    , name_("")
    , description_("")
    , device_type_("")
    , manufacturer_("")
    , model_("")
    , serial_number_("")
    , protocol_id_(0)              // 🔥 변경: protocol_type_ → protocol_id_
    , endpoint_("")
    , config_("{}")
    , polling_interval_(1000)      // 🔥 새로 추가: 기본 1초
    , timeout_(3000)               // 🔥 새로 추가: 기본 3초
    , retry_count_(3)              // 🔥 새로 추가: 기본 3회
    , is_enabled_(true)
    , installation_date_(std::nullopt)
    , last_maintenance_(std::nullopt)
    , created_by_(std::nullopt)
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

DeviceEntity::DeviceEntity(int device_id) 
    : DeviceEntity() {  // 위임 생성자 사용
    setId(device_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 - Repository 활용
// =============================================================================

bool DeviceEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("DeviceEntity::loadFromDatabase - Invalid device ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceEntity - Loaded device: " + name_ + 
                                " (protocol_id: " + std::to_string(protocol_id_) + 
                                ", polling: " + std::to_string(polling_interval_) + "ms)");
                }
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("DeviceEntity::saveToDatabase - Invalid device data. "
                         "Required fields: tenant_id=" + std::to_string(tenant_id_) + 
                         ", site_id=" + std::to_string(site_id_) + 
                         ", name=" + name_ + 
                         ", protocol_id=" + std::to_string(protocol_id_) +
                         ", endpoint=" + endpoint_);
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            // 저장 전 타임스탬프 업데이트
            if (getId() <= 0) {
                // 새 레코드인 경우
                created_at_ = std::chrono::system_clock::now();
            }
            updated_at_ = std::chrono::system_clock::now();
            
            bool success = repo->save(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceEntity - Saved device: " + name_ + 
                                " (ID: " + std::to_string(getId()) + 
                                ", protocol_id: " + std::to_string(protocol_id_) + ")");
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("DeviceEntity::deleteFromDatabase - Invalid device ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("DeviceEntity - Deleted device: " + name_ + 
                                " (ID: " + std::to_string(getId()) + ")");
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("DeviceEntity::updateToDatabase - Invalid device data or ID. "
                         "ID=" + std::to_string(getId()) + ", Valid=" + (isValid() ? "true" : "false"));
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            // 업데이트 전 타임스탬프 갱신
            updated_at_ = std::chrono::system_clock::now();
            
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceEntity - Updated device: " + name_ + 
                                " (polling: " + std::to_string(polling_interval_) + "ms, " +
                                "timeout: " + std::to_string(timeout_) + "ms)");
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드 구현
// =============================================================================

std::string DeviceEntity::dateToString(const std::chrono::system_clock::time_point& date) const {
    auto time_t = std::chrono::system_clock::to_time_t(date);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
    return ss.str();
}

std::string DeviceEntity::timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::shared_ptr<Repositories::DeviceRepository> DeviceEntity::getRepository() const {
    auto& factory = RepositoryFactory::getInstance();
    return factory.getDeviceRepository();
}

// =============================================================================
// 새로운 비즈니스 로직 메서드 구현
// =============================================================================

void DeviceEntity::setOptimalPollingForProtocol() {
    try {
        // 현재 RepositoryFactory에 ProtocolRepository가 없으므로 임시 구현
        // 실제로는 protocol_id로 프로토콜 정보를 조회해야 함
        
        // 임시로 protocol_id 기반 기본값 설정
        if (protocol_id_ == 1) { // Modbus 계열
            polling_interval_ = 500;  
        } else if (protocol_id_ == 2) { // MQTT 계열
            polling_interval_ = 2000; 
        } else if (protocol_id_ == 3) { // BACnet 계열
            polling_interval_ = 1000; 
        } else if (protocol_id_ == 4) { // OPC-UA 계열
            polling_interval_ = 1000; 
        } else {
            polling_interval_ = 1000; // 기본값
        }
        
        markModified();
        
        if (logger_) {
            logger_->Info("DeviceEntity - Set optimal polling for protocol_id " + 
                        std::to_string(protocol_id_) + ": " + std::to_string(polling_interval_) + "ms");
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::setOptimalPollingForProtocol failed: " + std::string(e.what()));
        }
        // 실패 시 기본값 사용
        polling_interval_ = 1000;
        markModified();
    }
}

void DeviceEntity::setOptimalTimeoutForEndpoint() {
    try {
        if (endpoint_.find("127.0.0.1") != std::string::npos || 
            endpoint_.find("localhost") != std::string::npos) {
            timeout_ = 1000; // 로컬은 1초
        } else if (endpoint_.find("192.168.") != std::string::npos ||
                   endpoint_.find("10.") != std::string::npos ||
                   endpoint_.find("172.") != std::string::npos) {
            timeout_ = 3000; // LAN은 3초
        } else if (endpoint_.find("://") != std::string::npos) {
            // URL 형태인 경우 (HTTP, HTTPS 등)
            timeout_ = 10000; // 웹 서비스는 10초
        } else {
            timeout_ = 5000; // 외부 네트워크는 5초
        }
        
        markModified();
        
        if (logger_) {
            logger_->Info("DeviceEntity - Set optimal timeout for endpoint " + endpoint_ + 
                        ": " + std::to_string(timeout_) + "ms");
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::setOptimalTimeoutForEndpoint failed: " + std::string(e.what()));
        }
        // 실패 시 기본값 사용
        timeout_ = 3000;
        markModified();
    }
}

void DeviceEntity::applyDeviceTypeDefaults() {
    std::string type_lower = device_type_;
    std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);
    
    // 디바이스 타입별 기본 설정
    if (type_lower == "plc") {
        if (manufacturer_.empty()) manufacturer_ = "Generic";
        if (polling_interval_ == 1000) polling_interval_ = 500; // PLC는 빠른 폴링
        if (retry_count_ == 3) retry_count_ = 5; // PLC는 재시도 많이
    } else if (type_lower == "sensor") {
        if (manufacturer_.empty()) manufacturer_ = "Generic";
        if (polling_interval_ == 1000) polling_interval_ = 2000; // 센서는 느린 폴링
        if (timeout_ == 3000) timeout_ = 5000; // 센서는 긴 타임아웃
    } else if (type_lower == "gateway") {
        if (manufacturer_.empty()) manufacturer_ = "Generic";
        if (polling_interval_ == 1000) polling_interval_ = 1000; // 게이트웨이는 표준 폴링
        if (retry_count_ == 3) retry_count_ = 2; // 게이트웨이는 재시도 적게
    } else if (type_lower == "hmi") {
        if (manufacturer_.empty()) manufacturer_ = "Generic";
        if (polling_interval_ == 1000) polling_interval_ = 1500; // HMI는 중간 폴링
        if (timeout_ == 3000) timeout_ = 2000; // HMI는 빠른 응답 기대
    } else if (type_lower == "meter") {
        if (manufacturer_.empty()) manufacturer_ = "Generic";
        if (polling_interval_ == 1000) polling_interval_ = 5000; // 미터는 매우 느린 폴링
        if (timeout_ == 3000) timeout_ = 10000; // 미터는 긴 타임아웃
    }
    
    markModified();
    
    if (logger_) {
        logger_->Info("DeviceEntity - Applied defaults for device type: " + device_type_ +
                    " (polling: " + std::to_string(polling_interval_) + "ms, " +
                    "timeout: " + std::to_string(timeout_) + "ms, " +
                    "retries: " + std::to_string(retry_count_) + ")");
    }
}

void DeviceEntity::applyProtocolDefaults() {
    json config = getConfigAsJson();
    
    // 기본 연결 설정 추가
    if (!config.contains("connection_retries")) {
        config["connection_retries"] = retry_count_;
    }
    if (!config.contains("response_timeout")) {
        config["response_timeout"] = timeout_;
    }
    if (!config.contains("polling_interval")) {
        config["polling_interval"] = polling_interval_;
    }
    
    // 공통 설정
    if (!config.contains("auto_reconnect")) {
        config["auto_reconnect"] = true;
    }
    if (!config.contains("keep_alive")) {
        config["keep_alive"] = true;
    }
    
    setConfigAsJson(config);
    
    if (logger_) {
        logger_->Info("DeviceEntity - Applied protocol defaults for protocol_id: " + 
                    std::to_string(protocol_id_));
    }
}

// =============================================================================
// 이전 버전 호환성 메서드 구현 (deprecated)
// =============================================================================

std::string DeviceEntity::getProtocolType() const {
    // 임시 구현 - 실제로는 protocol_id로 protocols 테이블에서 조회해야 함
    switch (protocol_id_) {
        case 1: return "MODBUS_TCP";
        case 2: return "MQTT";
        case 3: return "BACNET";
        case 4: return "OPCUA";
        default: return "UNKNOWN";
    }
}

void DeviceEntity::setProtocolType(const std::string& protocol_type) {
    // 임시 구현 - 실제로는 protocol_type으로 protocols 테이블에서 ID 조회해야 함
    std::string type_upper = protocol_type;
    std::transform(type_upper.begin(), type_upper.end(), type_upper.begin(), ::toupper);
    
    if (type_upper.find("MODBUS") != std::string::npos) {
        protocol_id_ = 1;
    } else if (type_upper.find("MQTT") != std::string::npos) {
        protocol_id_ = 2;
    } else if (type_upper.find("BACNET") != std::string::npos) {
        protocol_id_ = 3;
    } else if (type_upper.find("OPCUA") != std::string::npos || type_upper.find("OPC") != std::string::npos) {
        protocol_id_ = 4;
    } else {
        protocol_id_ = 1; // 기본값: Modbus
    }
    
    markModified();
    
    if (logger_) {
        logger_->Info("DeviceEntity - Set protocol type: " + protocol_type + 
                    " -> protocol_id: " + std::to_string(protocol_id_));
    }
}

// 헬퍼 메서드 구현
bool DeviceEntity::isLocalEndpoint() const {
    return endpoint_.find("127.0.0.1") != std::string::npos ||
           endpoint_.find("localhost") != std::string::npos;
}

bool DeviceEntity::isLANEndpoint() const {
    return endpoint_.find("192.168.") != std::string::npos ||
           endpoint_.find("10.") != std::string::npos ||
           endpoint_.find("172.") != std::string::npos;
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne