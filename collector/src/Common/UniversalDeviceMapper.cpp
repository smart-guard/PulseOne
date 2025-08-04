//=============================================================================
// collector/src/Common/UniversalDeviceMapper.cpp
// 🔥 Universal Device Mapper 구현 - 모든 프로토콜 디바이스 정보 통합 변환
//=============================================================================

#include "Common/UniversalDeviceMapper.h"
#include "Common/Utils.h"  // UUID 생성 유틸리티

#include <regex>
#include <random>
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Common {

// =============================================================================
// 🔥 정적 멤버 변수들
// =============================================================================
UniversalDeviceMapper::MapperStatistics UniversalDeviceMapper::statistics_;
std::mutex UniversalDeviceMapper::statistics_mutex_;

// =============================================================================
// 🔥 BACnet → DeviceInfo 변환 (실제 구조체 사용)
// =============================================================================

UniversalDeviceMapper::MapperResult<DeviceInfo> 
UniversalDeviceMapper::FromBACnet(const BACnetDeviceInfo& bacnet_info) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    MapperResult<DeviceInfo> result;
    
    try {
        DeviceInfo device_info;
        
        // =====================================================================
        // 🔥 기본 정보 매핑 (실제 필드들)
        // =====================================================================
        
        // ID 설정 (BACnet Device ID를 문자열로)
        device_info.id = std::to_string(bacnet_info.device_id);
        
        // 이름 설정
        device_info.name = bacnet_info.device_name.empty() ? 
            ("BACnet Device " + std::to_string(bacnet_info.device_id)) : 
            bacnet_info.device_name;
        
        // 엔드포인트 설정 (IP:PORT 형식)
        std::string ip = bacnet_info.GetIpAddress();
        uint16_t port = bacnet_info.GetPort();
        
        if (ip.empty()) {
            result.AddError(MapperError("INVALID_IP", 
                "BACnet device has no valid IP address", 
                "ip_address", "Set valid IP address in BACnetDeviceInfo"));
            return result;
        }
        
        device_info.endpoint = ip + ":" + std::to_string(port);
        
        // 프로토콜 타입 설정
        device_info.protocol = ProtocolType::BACNET_IP;
        
        // =====================================================================
        // 🔥 BACnet 특화 속성들을 properties에 저장
        // =====================================================================
        
        device_info.properties["device_id"] = std::to_string(bacnet_info.device_id);
        device_info.properties["vendor_id"] = std::to_string(bacnet_info.vendor_id);
        device_info.properties["max_apdu_length"] = std::to_string(bacnet_info.max_apdu_length);
        device_info.properties["segmentation_support"] = std::to_string(bacnet_info.segmentation_support);
        device_info.properties["protocol_version"] = std::to_string(bacnet_info.protocol_version);
        device_info.properties["protocol_revision"] = std::to_string(bacnet_info.protocol_revision);
        
        // 선택적 정보들
        if (!bacnet_info.model_name.empty()) {
            device_info.properties["model_name"] = bacnet_info.model_name;
        }
        if (!bacnet_info.firmware_revision.empty()) {
            device_info.properties["firmware_revision"] = bacnet_info.firmware_revision;
        }
        if (!bacnet_info.application_software_version.empty()) {
            device_info.properties["application_software_version"] = bacnet_info.application_software_version;
        }
        if (!bacnet_info.location.empty()) {
            device_info.properties["location"] = bacnet_info.location;
        }
        if (!bacnet_info.description.empty()) {
            device_info.properties["description"] = bacnet_info.description;
        }
        
        // 네트워크 주소 정보 (BACNET_ADDRESS)
        device_info.properties["bacnet_network"] = std::to_string(bacnet_info.address.net);
        device_info.properties["bacnet_address_length"] = std::to_string(bacnet_info.address.len);
        
        // 객체 개수 정보
        device_info.properties["total_objects"] = std::to_string(bacnet_info.objects.size());
        
        // 각 객체 타입별 카운트
        std::unordered_map<uint32_t, size_t> object_type_counts;
        for (const auto& obj : bacnet_info.objects) {
            object_type_counts[static_cast<uint32_t>(obj.object_type)]++;
        }
        
        for (const auto& [type, count] : object_type_counts) {
            device_info.properties["objects_type_" + std::to_string(type)] = std::to_string(count);
        }
        
        // =====================================================================
        // 🔥 기본 설정값들 (DeviceInfo 구조체 필드들)
        // =====================================================================
        
        // 타임아웃 설정
        device_info.timeout = std::chrono::milliseconds(5000);
        
        // 폴링 간격 설정
        device_info.polling_interval = std::chrono::seconds(30);
        
        // 기본값으로 활성화
        device_info.enabled = true;
        
        // 마지막 업데이트 시간
        device_info.last_updated = bacnet_info.last_seen;
        
        // 온라인 상태
        device_info.properties["is_online"] = bacnet_info.is_online ? "true" : "false";
        
        // UUID 생성
        device_info.uuid = GenerateDeviceUUID("bacnet", std::to_string(bacnet_info.device_id));
        
        // =====================================================================
        // 🔥 성공 처리
        // =====================================================================
        
        result.data = device_info;
        result.success = true;
        
        // 경고 확인
        if (bacnet_info.objects.empty()) {
            result.AddWarning("BACnet device has no discovered objects");
        }
        
        if (!bacnet_info.IsRecentlyActive()) {
            result.AddWarning("BACnet device was not recently active");
        }
        
        // 통계 업데이트
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(true, "bacnet", duration);
        
        LogManager::getInstance().Debug(
            "Successfully converted BACnet device " + std::to_string(bacnet_info.device_id) + 
            " to DeviceInfo with " + std::to_string(bacnet_info.objects.size()) + " objects");
        
    } catch (const std::exception& e) {
        result.AddError(MapperError("CONVERSION_EXCEPTION", 
            std::string("Exception during BACnet conversion: ") + e.what(),
            "bacnet_info", "Check BACnetDeviceInfo data integrity"));
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(false, "bacnet", duration);
    }
    
    return result;
}

// =============================================================================
// 🔥 Modbus RTU 변환 (ModbusRtuSlaveInfo 사용)
// =============================================================================

UniversalDeviceMapper::MapperResult<DeviceInfo> 
UniversalDeviceMapper::FromModbusRtu(const ModbusRtuSlaveInfo& modbus_info) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    MapperResult<DeviceInfo> result;
    
    try {
        DeviceInfo device_info;
        
        // =====================================================================
        // 🔥 기본 정보 매핑
        // =====================================================================
        
        device_info.id = "modbus_rtu_" + std::to_string(modbus_info.slave_id);
        device_info.name = modbus_info.device_name.empty() ? 
            ("Modbus RTU Slave " + std::to_string(modbus_info.slave_id)) : 
            modbus_info.device_name;
        
        device_info.endpoint = modbus_info.endpoint; // 시리얼 포트 (예: "/dev/ttyUSB0")
        device_info.protocol = ProtocolType::MODBUS_RTU;
        
        // =====================================================================
        // 🔥 Modbus RTU 특화 속성들
        // =====================================================================
        
        device_info.properties["slave_id"] = std::to_string(modbus_info.slave_id);
        device_info.properties["baud_rate"] = std::to_string(modbus_info.baud_rate);
        device_info.properties["parity"] = std::string(1, modbus_info.parity);
        device_info.properties["data_bits"] = std::to_string(modbus_info.data_bits);
        device_info.properties["stop_bits"] = std::to_string(modbus_info.stop_bits);
        device_info.properties["is_online"] = modbus_info.is_online ? "true" : "false";
        device_info.properties["response_time_ms"] = std::to_string(modbus_info.response_time_ms);
        
        // 통계 정보
        device_info.properties["total_requests"] = std::to_string(modbus_info.total_requests);
        device_info.properties["successful_requests"] = std::to_string(modbus_info.successful_requests);
        device_info.properties["timeout_errors"] = std::to_string(modbus_info.timeout_errors);
        device_info.properties["crc_errors"] = std::to_string(modbus_info.crc_errors);
        
        if (!modbus_info.last_error.empty()) {
            device_info.properties["last_error"] = modbus_info.last_error;
        }
        
        // =====================================================================
        // 🔥 기본 설정값들
        // =====================================================================
        
        device_info.timeout = std::chrono::milliseconds(3000);
        device_info.polling_interval = std::chrono::seconds(10);
        device_info.enabled = modbus_info.is_online;
        device_info.last_updated = modbus_info.last_response;
        device_info.uuid = GenerateDeviceUUID("modbus_rtu", std::to_string(modbus_info.slave_id));
        
        result.data = device_info;
        result.success = true;
        
        // 통계 업데이트
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(true, "modbus_rtu", duration);
        
        LogManager::getInstance().Debug(
            "Successfully converted Modbus RTU slave " + std::to_string(modbus_info.slave_id) + 
            " to DeviceInfo");
        
    } catch (const std::exception& e) {
        result.AddError(MapperError("CONVERSION_EXCEPTION", 
            std::string("Exception during Modbus RTU conversion: ") + e.what(),
            "modbus_rtu_info", "Check ModbusRtuSlaveInfo data integrity"));
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(false, "modbus_rtu", duration);
    }
    
    return result;
}

// =============================================================================
// 🔥 Modbus TCP 변환 (ModbusTcpDeviceInfo 사용)
// =============================================================================

UniversalDeviceMapper::MapperResult<DeviceInfo> 
UniversalDeviceMapper::FromModbusTcp(const ModbusTcpDeviceInfo& modbus_info) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    MapperResult<DeviceInfo> result;
    
    try {
        DeviceInfo device_info;
        
        // =====================================================================
        // 🔥 기본 정보 매핑
        // =====================================================================
        
        device_info.id = "modbus_tcp_" + modbus_info.ip_address + "_" + std::to_string(modbus_info.slave_id);
        device_info.name = modbus_info.device_name.empty() ? 
            ("Modbus TCP Device " + modbus_info.ip_address + ":" + std::to_string(modbus_info.port)) : 
            modbus_info.device_name;
        
        device_info.endpoint = modbus_info.ip_address + ":" + std::to_string(modbus_info.port);
        device_info.protocol = ProtocolType::MODBUS_TCP;
        
        // =====================================================================
        // 🔥 Modbus TCP 특화 속성들
        // =====================================================================
        
        device_info.properties["slave_id"] = std::to_string(modbus_info.slave_id);
        device_info.properties["ip_address"] = modbus_info.ip_address;
        device_info.properties["port"] = std::to_string(modbus_info.port);
        device_info.properties["timeout_ms"] = std::to_string(modbus_info.timeout_ms);
        device_info.properties["max_retries"] = std::to_string(modbus_info.max_retries);
        device_info.properties["is_online"] = modbus_info.is_online ? "true" : "false";
        device_info.properties["response_time_ms"] = std::to_string(modbus_info.response_time_ms);
        
        // 통계 정보
        device_info.properties["total_requests"] = std::to_string(modbus_info.total_requests);
        device_info.properties["successful_requests"] = std::to_string(modbus_info.successful_requests);
        device_info.properties["timeout_errors"] = std::to_string(modbus_info.timeout_errors);
        
        if (!modbus_info.last_error.empty()) {
            device_info.properties["last_error"] = modbus_info.last_error;
        }
        
        // =====================================================================
        // 🔥 기본 설정값들
        // =====================================================================
        
        device_info.timeout = std::chrono::milliseconds(modbus_info.timeout_ms);
        device_info.polling_interval = std::chrono::seconds(10);
        device_info.enabled = modbus_info.is_online;
        device_info.last_updated = modbus_info.last_response;
        device_info.uuid = GenerateDeviceUUID("modbus_tcp", 
            modbus_info.ip_address + "_" + std::to_string(modbus_info.slave_id));
        
        result.data = device_info;
        result.success = true;
        
        // 통계 업데이트
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(true, "modbus_tcp", duration);
        
        LogManager::getInstance().Debug(
            "Successfully converted Modbus TCP device " + modbus_info.ip_address + 
            " (slave " + std::to_string(modbus_info.slave_id) + ") to DeviceInfo");
        
    } catch (const std::exception& e) {
        result.AddError(MapperError("CONVERSION_EXCEPTION", 
            std::string("Exception during Modbus TCP conversion: ") + e.what(),
            "modbus_tcp_info", "Check ModbusTcpDeviceInfo data integrity"));
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(false, "modbus_tcp", duration);
    }
    
    return result;
}

// =============================================================================
// 🔥 MQTT 변환 (MQTTDeviceInfo 사용)
// =============================================================================

UniversalDeviceMapper::MapperResult<DeviceInfo> 
UniversalDeviceMapper::FromMQTT(const MQTTDeviceInfo& mqtt_info) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    MapperResult<DeviceInfo> result;
    
    try {
        DeviceInfo device_info;
        
        // =====================================================================
        // 🔥 기본 정보 매핑
        // =====================================================================
        
        device_info.id = "mqtt_" + mqtt_info.client_id;
        device_info.name = "MQTT Device (" + mqtt_info.client_id + ")";
        device_info.endpoint = mqtt_info.broker_host + ":" + std::to_string(mqtt_info.broker_port);
        device_info.protocol = ProtocolType::MQTT;
        
        // =====================================================================
        // 🔥 MQTT 특화 속성들
        // =====================================================================
        
        device_info.properties["client_id"] = mqtt_info.client_id;
        device_info.properties["broker_host"] = mqtt_info.broker_host;
        device_info.properties["broker_port"] = std::to_string(mqtt_info.broker_port);
        device_info.properties["username"] = mqtt_info.username;
        device_info.properties["use_ssl"] = mqtt_info.use_ssl ? "true" : "false";
        device_info.properties["clean_session"] = mqtt_info.clean_session ? "true" : "false";
        device_info.properties["keep_alive_interval"] = std::to_string(mqtt_info.keep_alive_interval);
        device_info.properties["default_qos"] = std::to_string(mqtt_info.default_qos);
        device_info.properties["is_connected"] = mqtt_info.is_connected ? "true" : "false";
        
        // 구독 토픽들을 쉼표로 구분된 문자열로 저장
        if (!mqtt_info.subscribed_topics.empty()) {
            std::string topics_str;
            for (size_t i = 0; i < mqtt_info.subscribed_topics.size(); ++i) {
                if (i > 0) topics_str += ",";
                topics_str += mqtt_info.subscribed_topics[i];
            }
            device_info.properties["subscribed_topics"] = topics_str;
        }
        
        // 통계 정보
        device_info.properties["messages_published"] = std::to_string(mqtt_info.messages_published);
        device_info.properties["messages_received"] = std::to_string(mqtt_info.messages_received);
        device_info.properties["connection_failures"] = std::to_string(mqtt_info.connection_failures);
        
        if (!mqtt_info.last_error.empty()) {
            device_info.properties["last_error"] = mqtt_info.last_error;
        }
        
        // 비밀번호는 보안상 저장하지 않음
        
        // =====================================================================
        // 🔥 기본 설정값들
        // =====================================================================
        
        device_info.timeout = std::chrono::milliseconds(10000); // MQTT는 타임아웃이 길어야 함
        device_info.polling_interval = std::chrono::seconds(5);  // MQTT는 실시간이므로 짧게
        device_info.enabled = mqtt_info.is_connected;
        device_info.last_updated = mqtt_info.last_connect_time;
        device_info.uuid = GenerateDeviceUUID("mqtt", mqtt_info.client_id);
        
        result.data = device_info;
        result.success = true;
        
        // 경고 확인
        if (mqtt_info.subscribed_topics.empty()) {
            result.AddWarning("MQTT device has no subscribed topics");
        }
        
        if (mqtt_info.connection_failures > 10) {
            result.AddWarning("MQTT device has high connection failure count: " + 
                            std::to_string(mqtt_info.connection_failures));
        }
        
        // 통계 업데이트
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(true, "mqtt", duration);
        
        LogManager::getInstance().Debug(
            "Successfully converted MQTT device " + mqtt_info.client_id + " to DeviceInfo");
        
    } catch (const std::exception& e) {
        result.AddError(MapperError("CONVERSION_EXCEPTION", 
            std::string("Exception during MQTT conversion: ") + e.what(),
            "mqtt_info", "Check MQTTDeviceInfo data integrity"));
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(false, "mqtt", duration);
    }
    
    return result;
}

UniversalDeviceMapper::MapperResult<BACnetDeviceInfo> 
UniversalDeviceMapper::ToBACnet(const DeviceInfo& device_info) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    MapperResult<BACnetDeviceInfo> result;
    
    try {
        // 프로토콜 타입 검증
        if (device_info.protocol != ProtocolType::BACNET_IP && 
            device_info.protocol != ProtocolType::BACNET_MSTP) {
            result.AddError(MapperError("INVALID_PROTOCOL", 
                "DeviceInfo protocol is not BACnet",
                "protocol", "Set protocol to BACNET_IP or BACNET_MSTP"));
            return result;
        }
        
        BACnetDeviceInfo bacnet_info;
        
        // =====================================================================
        // 🔥 기본 정보 역매핑
        // =====================================================================
        
        // Device ID 변환
        try {
            bacnet_info.device_id = std::stoul(device_info.id);
            if (bacnet_info.device_id > 4194303U) {  // BACNET_MAX_INSTANCE
                result.AddError(MapperError("INVALID_DEVICE_ID", 
                    "BACnet device ID exceeds maximum (4194303)",
                    "id", "Use device ID within BACnet range"));
                return result;
            }
        } catch (const std::exception&) {
            result.AddError(MapperError("INVALID_DEVICE_ID_FORMAT", 
                "Cannot convert device ID to numeric format",
                "id", "Use numeric device ID for BACnet"));
            return result;
        }
        
        // 디바이스 이름
        bacnet_info.device_name = device_info.name;
        
        // 엔드포인트 파싱 (IP:PORT)
        auto [ip, port] = ParseEndpoint(device_info.endpoint);
        if (ip.empty()) {
            result.AddError(MapperError("INVALID_ENDPOINT", 
                "Cannot parse IP address from endpoint",
                "endpoint", "Use 'IP:PORT' format (e.g., '192.168.1.100:47808')"));
            return result;
        }
        
        bacnet_info.SetIpAddress(ip, port);
        
        // =====================================================================
        // 🔥 BACnet 특화 속성들 복원
        // =====================================================================
        
        bacnet_info.vendor_id = SafeGetNumericProperty<uint16_t>(
            device_info.properties, "vendor_id", 0);
        
        bacnet_info.max_apdu_length = SafeGetNumericProperty<uint16_t>(
            device_info.properties, "max_apdu_length", 1476);
        
        bacnet_info.segmentation_support = SafeGetNumericProperty<uint8_t>(
            device_info.properties, "segmentation_support", 1);
        
        bacnet_info.protocol_version = SafeGetNumericProperty<uint8_t>(
            device_info.properties, "protocol_version", 1);
        
        bacnet_info.protocol_revision = SafeGetNumericProperty<uint8_t>(
            device_info.properties, "protocol_revision", 14);
        
        // 선택적 정보들
        bacnet_info.model_name = SafeGetProperty(device_info.properties, "model_name");
        bacnet_info.firmware_revision = SafeGetProperty(device_info.properties, "firmware_revision");
        bacnet_info.application_software_version = SafeGetProperty(device_info.properties, "application_software_version");
        bacnet_info.location = SafeGetProperty(device_info.properties, "location");
        bacnet_info.description = SafeGetProperty(device_info.properties, "description");
        
        // 상태 정보
        bacnet_info.last_seen = device_info.last_updated;
        bacnet_info.is_online = SafeGetProperty(device_info.properties, "is_online") == "true";
        
        // =====================================================================
        // 🔥 성공 처리
        // =====================================================================
        
        result.data = bacnet_info;
        result.success = true;
        
        // 통계 업데이트
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(true, "bacnet_reverse", duration);
        
        LogManager::getInstance().Debug(
            "Successfully converted DeviceInfo back to BACnet device " + 
            std::to_string(bacnet_info.device_id));
        
    } catch (const std::exception& e) {
        result.AddError(MapperError("REVERSE_CONVERSION_EXCEPTION", 
            std::string("Exception during reverse BACnet conversion: ") + e.what(),
            "device_info", "Check DeviceInfo data integrity"));
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        UpdateStatistics(false, "bacnet_reverse", duration);
    }
    
    return result;
}

// =============================================================================
// 🔥 BACnet 배치 변환
// =============================================================================

UniversalDeviceMapper::BatchMapperResult<DeviceInfo> 
UniversalDeviceMapper::FromBACnetBatch(const std::vector<BACnetDeviceInfo>& bacnet_devices) {
    
    BatchMapperResult<DeviceInfo> batch_result;
    batch_result.total_processed = bacnet_devices.size();
    
    LogManager::getInstance().Info(
        "Starting batch conversion of " + std::to_string(bacnet_devices.size()) + " BACnet devices");
    
    for (size_t i = 0; i < bacnet_devices.size(); ++i) {
        const auto& bacnet_device = bacnet_devices[i];
        
        auto conversion_result = FromBACnet(bacnet_device);
        
        if (conversion_result.success) {
            batch_result.successful_results.push_back(conversion_result.data);
            
            // 경고가 있으면 로그에 기록
            for (const auto& warning : conversion_result.warnings) {
                LogManager::getInstance().Warn(
                    "Device " + std::to_string(bacnet_device.device_id) + ": " + warning);
            }
        } else {
            // 실패한 항목들을 기록
            for (const auto& error : conversion_result.errors) {
                MapperError batch_error = error;
                batch_error.description = "Device " + std::to_string(bacnet_device.device_id) + 
                                        " (index " + std::to_string(i) + "): " + error.description;
                batch_result.failed_items.push_back(batch_error);
            }
        }
    }
    
    LogManager::getInstance().Info(
        "Batch conversion completed: " + std::to_string(batch_result.successful_results.size()) + 
        "/" + std::to_string(batch_result.total_processed) + " successful (" +
        std::to_string(batch_result.GetSuccessRate()) + "%)");
    
    return batch_result;
}

// =============================================================================
// 🔥 프로토콜 감지 및 유틸리티 함수들
// =============================================================================

ProtocolType UniversalDeviceMapper::DetectProtocolType(
    const std::string& endpoint,
    const std::unordered_map<std::string, std::string>& additional_info) {
    
    // 포트 기반 감지
    auto [ip, port] = ParseEndpoint(endpoint);
    
    switch (port) {
        case 47808:  // BACnet/IP 표준 포트
            return ProtocolType::BACNET_IP;
        case 502:    // Modbus TCP 표준 포트
            return ProtocolType::MODBUS_TCP;
        case 1883:   // MQTT 표준 포트
        case 8883:   // MQTT SSL 포트
            return ProtocolType::MQTT;
        default:
            break;
    }
    
    // 엔드포인트 패턴 기반 감지
    if (endpoint.find("/dev/tty") == 0 || endpoint.find("COM") == 0) {
        // 시리얼 포트 패턴 = Modbus RTU
        return ProtocolType::MODBUS_RTU;
    }
    
    if (endpoint.find("mqtt://") == 0 || endpoint.find("mqtts://") == 0) {
        return ProtocolType::MQTT;
    }
    
    // 추가 정보 기반 감지
    auto protocol_hint = additional_info.find("protocol");
    if (protocol_hint != additional_info.end()) {
        std::string hint = protocol_hint->second;
        std::transform(hint.begin(), hint.end(), hint.begin(), ::tolower);
        
        if (hint.find("bacnet") != std::string::npos) {
            return ProtocolType::BACNET_IP;
        } else if (hint.find("modbus") != std::string::npos) {
            if (hint.find("rtu") != std::string::npos) {
                return ProtocolType::MODBUS_RTU;
            } else {
                return ProtocolType::MODBUS_TCP;
            }
        } else if (hint.find("mqtt") != std::string::npos) {
            return ProtocolType::MQTT;
        }
    }
    
    // slave_id가 있으면 Modbus
    auto slave_id_hint = additional_info.find("slave_id");
    if (slave_id_hint != additional_info.end()) {
        if (endpoint.find("/dev/") == 0 || endpoint.find("COM") == 0) {
            return ProtocolType::MODBUS_RTU;
        } else {
            return ProtocolType::MODBUS_TCP;
        }
    }
    
    // client_id가 있으면 MQTT
    auto client_id_hint = additional_info.find("client_id");
    if (client_id_hint != additional_info.end()) {
        return ProtocolType::MQTT;
    }
    
    // device_id가 숫자이고 IP 엔드포인트면 BACnet
    auto device_id_hint = additional_info.find("device_id");
    if (device_id_hint != additional_info.end() && !ip.empty()) {
        try {
            uint32_t device_id = std::stoul(device_id_hint->second);
            if (device_id <= 4194303U) { // BACNET_MAX_INSTANCE
                return ProtocolType::BACNET_IP;
            }
        } catch (...) {
            // 숫자가 아니면 무시
        }
    }
    
    // 기본값: IP 엔드포인트면 BACnet, 그 외는 MODBUS_TCP
    if (!ip.empty()) {
        return ProtocolType::BACNET_IP;
    } else {
        return ProtocolType::MODBUS_TCP;
    }
}

UniversalDeviceMapper::MapperResult<bool> 
UniversalDeviceMapper::ValidateDeviceInfo(const DeviceInfo& device_info) {
    
    MapperResult<bool> result;
    result.data = true;  // 기본적으로 유효하다고 가정
    
    // 필수 필드 검증
    if (device_info.id.empty()) {
        result.AddError(MapperError("MISSING_ID", "Device ID is required", "id"));
    }
    
    if (device_info.name.empty()) {
        result.AddError(MapperError("MISSING_NAME", "Device name is required", "name"));
    }
    
    if (device_info.endpoint.empty()) {
        result.AddError(MapperError("MISSING_ENDPOINT", "Device endpoint is required", "endpoint"));
    } else {
        // 엔드포인트 형식 검증
        auto [ip, port] = ParseEndpoint(device_info.endpoint);
        if (ip.empty()) {
            result.AddError(MapperError("INVALID_ENDPOINT_FORMAT", 
                "Endpoint must be in 'IP:PORT' format", "endpoint"));
        }
    }
    
    // 프로토콜별 특정 검증
    auto missing_props = GetMissingRequiredProperties(device_info);
    for (const auto& prop : missing_props) {
        result.AddError(MapperError("MISSING_REQUIRED_PROPERTY", 
            "Missing required property: " + prop, "properties." + prop));
    }
    
    // 에러가 있으면 실패
    if (result.HasErrors()) {
        result.data = false;
        result.success = false;
    }
    
    return result;
}

std::vector<std::string> UniversalDeviceMapper::GetMissingRequiredProperties(
    const DeviceInfo& device_info) {
    
    std::vector<std::string> missing;
    
    switch (device_info.protocol) {
        case ProtocolType::BACNET_IP:
        case ProtocolType::BACNET_MSTP:
            if (device_info.properties.find("device_id") == device_info.properties.end()) {
                missing.push_back("device_id");
            }
            break;
            
        case ProtocolType::MODBUS_TCP:
        case ProtocolType::MODBUS_RTU:
            if (device_info.properties.find("slave_id") == device_info.properties.end()) {
                missing.push_back("slave_id");
            }
            // Modbus RTU 추가 속성들
            if (device_info.protocol == ProtocolType::MODBUS_RTU) {
                if (device_info.properties.find("baud_rate") == device_info.properties.end()) {
                    missing.push_back("baud_rate");
                }
                if (device_info.properties.find("parity") == device_info.properties.end()) {
                    missing.push_back("parity");
                }
                if (device_info.properties.find("data_bits") == device_info.properties.end()) {
                    missing.push_back("data_bits");
                }
                if (device_info.properties.find("stop_bits") == device_info.properties.end()) {
                    missing.push_back("stop_bits");
                }
            }
            break;
            
        case ProtocolType::MQTT:
            if (device_info.properties.find("client_id") == device_info.properties.end()) {
                missing.push_back("client_id");
            }
            if (device_info.properties.find("broker_host") == device_info.properties.end()) {
                missing.push_back("broker_host");
            }
            break;
            
        default:
            // 기타 프로토콜은 특별한 요구사항 없음
            break;
    }
    
    return missing;
}

// =============================================================================
// 🔥 UUID 생성 및 기본 설정
// =============================================================================

UUID UniversalDeviceMapper::GenerateDeviceUUID(
    const std::string& protocol_prefix, 
    const std::string& device_identifier) {
    
    // UUID 네임스페이스: PulseOne + Protocol + Device
    std::string uuid_source = "pulseone-" + protocol_prefix + "-" + device_identifier;
    
    // SHA-1 해시 기반 UUID 생성 (UUID v5 스타일)
    std::hash<std::string> hasher;
    size_t hash_value = hasher(uuid_source);
    
    // UUID 형식으로 포맷팅
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (hash_value & 0xFFFFFFFF) << "-";
    oss << std::setw(4) << ((hash_value >> 32) & 0xFFFF) << "-";
    oss << std::setw(4) << ((hash_value >> 48) & 0x0FFF | 0x5000) << "-";  // Version 5
    oss << std::setw(4) << (hash_value & 0x3FFF | 0x8000) << "-";         // Variant
    oss << std::setw(12) << (hash_value ^ (hash_value >> 16));
    
    return oss.str();
}

void UniversalDeviceMapper::ApplyProtocolDefaults(DeviceInfo& device_info, ProtocolType protocol) {
    switch (protocol) {
        case ProtocolType::BACNET_IP:
        case ProtocolType::BACNET_MSTP:
            device_info.timeout = std::chrono::milliseconds(5000);
            device_info.polling_interval = std::chrono::seconds(30);
            if (device_info.properties.find("max_apdu_length") == device_info.properties.end()) {
                device_info.properties["max_apdu_length"] = "1476";
            }
            break;
            
        case ProtocolType::MODBUS_TCP:
        case ProtocolType::MODBUS_RTU:
            device_info.timeout = std::chrono::milliseconds(3000);
            device_info.polling_interval = std::chrono::seconds(10);
            if (device_info.properties.find("byte_timeout") == device_info.properties.end()) {
                device_info.properties["byte_timeout"] = "500";
            }
            break;
            
        case ProtocolType::MQTT:
            device_info.timeout = std::chrono::milliseconds(10000);
            device_info.polling_interval = std::chrono::seconds(60);
            if (device_info.properties.find("keep_alive") == device_info.properties.end()) {
                device_info.properties["keep_alive"] = "60";
            }
            break;
            
        default:
            device_info.timeout = std::chrono::milliseconds(5000);
            device_info.polling_interval = std::chrono::seconds(30);
            break;
    }
}

// =============================================================================
// 🔥 통계 및 내부 헬퍼 함수들
// =============================================================================

UniversalDeviceMapper::MapperStatistics UniversalDeviceMapper::GetStatistics() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return statistics_;
}

void UniversalDeviceMapper::ResetStatistics() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    statistics_ = MapperStatistics{};
}

std::pair<std::string, uint16_t> UniversalDeviceMapper::ParseEndpoint(const std::string& endpoint) {
    std::regex endpoint_regex(R"(^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}):(\d{1,5})$)");
    std::smatch matches;
    
    if (std::regex_match(endpoint, matches, endpoint_regex)) {
        std::string ip = matches[1].str();
        uint16_t port = static_cast<uint16_t>(std::stoul(matches[2].str()));
        return {ip, port};
    }
    
    return {"", 0};
}

std::string UniversalDeviceMapper::SafeGetProperty(
    const std::unordered_map<std::string, std::string>& properties,
    const std::string& key, 
    const std::string& default_value) {
    
    auto it = properties.find(key);
    return (it != properties.end()) ? it->second : default_value;
}

void UniversalDeviceMapper::UpdateStatistics(
    bool success, 
    const std::string& protocol,
    std::chrono::milliseconds processing_time) {
    
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    
    statistics_.total_conversions++;
    if (success) {
        statistics_.successful_conversions++;
    } else {
        statistics_.failed_conversions++;
    }
    
    statistics_.total_processing_time += processing_time;
    statistics_.protocol_counts[protocol]++;
}

} // namespace Common
} // namespace PulseOne