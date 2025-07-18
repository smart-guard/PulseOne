// =============================================================================
// collector/src/Drivers/DriverFactory.cpp
// 드라이버 팩토리 구현
// =============================================================================

#include "Drivers/DriverFactory.h"
#include "Drivers/CommonTypes.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <stdexcept>

namespace PulseOne {
namespace Drivers {

// 정적 멤버 초기화는 헤더에서 인라인으로 처리되므로 별도 정의 불필요

/**
 * @brief 프로토콜 타입을 문자열로 변환하는 유틸리티 함수
 * @param protocol 프로토콜 타입
 * @return 프로토콜 문자열
 */
std::string ProtocolTypeToString(ProtocolType protocol) {
    switch (protocol) {
        case ProtocolType::MODBUS_TCP:
            return "Modbus TCP";
        case ProtocolType::MODBUS_RTU:
            return "Modbus RTU";
        case ProtocolType::MQTT:
            return "MQTT";
        case ProtocolType::BACNET_IP:
            return "BACnet/IP";
        case ProtocolType::OPC_UA:
            return "OPC UA";
        case ProtocolType::ETHERNET_IP:
            return "EtherNet/IP";
        case ProtocolType::PROFINET:
            return "PROFINET";
        case ProtocolType::DNP3:
            return "DNP3";
        case ProtocolType::IEC61850:
            return "IEC 61850";
        case ProtocolType::CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
}

/**
 * @brief 문자열을 프로토콜 타입으로 변환하는 유틸리티 함수
 * @param protocol_str 프로토콜 문자열
 * @return 프로토콜 타입
 */
ProtocolType StringToProtocolType(const std::string& protocol_str) {
    std::string lower_str = protocol_str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "modbus tcp" || lower_str == "modbus_tcp") {
        return ProtocolType::MODBUS_TCP;
    } else if (lower_str == "modbus rtu" || lower_str == "modbus_rtu") {
        return ProtocolType::MODBUS_RTU;
    } else if (lower_str == "mqtt") {
        return ProtocolType::MQTT;
    } else if (lower_str == "bacnet/ip" || lower_str == "bacnet_ip" || lower_str == "bacnet") {
        return ProtocolType::BACNET_IP;
    } else if (lower_str == "opc ua" || lower_str == "opc_ua" || lower_str == "opcua") {
        return ProtocolType::OPC_UA;
    } else if (lower_str == "ethernet/ip" || lower_str == "ethernet_ip" || lower_str == "ethernetip") {
        return ProtocolType::ETHERNET_IP;
    } else if (lower_str == "profinet") {
        return ProtocolType::PROFINET;
    } else if (lower_str == "dnp3") {
        return ProtocolType::DNP3;
    } else if (lower_str == "iec 61850" || lower_str == "iec61850") {
        return ProtocolType::IEC61850;
    } else if (lower_str == "custom") {
        return ProtocolType::CUSTOM;
    } else {
        throw std::invalid_argument("Unknown protocol type: " + protocol_str);
    }
}

/**
 * @brief 드라이버 팩토리 전역 함수들
 */
namespace FactoryUtils {
    
    /**
     * @brief 모든 등록된 드라이버 정보를 문자열로 반환
     * @return 드라이버 정보 문자열
     */
    std::string GetRegisteredDriversInfo() {
        auto& factory = DriverFactory::GetInstance();
        auto protocols = factory.GetSupportedProtocols();
        
        std::ostringstream oss;
        oss << "Registered Drivers (" << protocols.size() << "):\n";
        
        for (const auto& protocol : protocols) {
            oss << "  - " << ProtocolTypeToString(protocol) << "\n";
        }
        
        return oss.str();
    }
    
    /**
     * @brief 특정 프로토콜의 드라이버 생성 및 초기화
     * @param protocol 프로토콜 타입
     * @param config 드라이버 설정
     * @return 초기화된 드라이버 인스턴스
     */
    std::unique_ptr<IProtocolDriver> CreateAndInitializeDriver(
        ProtocolType protocol, 
        const DriverConfig& config) {
        
        auto& factory = DriverFactory::GetInstance();
        
        // 드라이버 생성
        auto driver = factory.CreateDriver(protocol);
        if (!driver) {
            throw std::runtime_error("Failed to create driver for protocol: " + 
                                   ProtocolTypeToString(protocol));
        }
        
        // 드라이버 초기화
        if (!driver->Initialize(config)) {
            auto error = driver->GetLastError();
            throw std::runtime_error("Failed to initialize driver: " + error.message);
        }
        
        return driver;
    }
    
    /**
     * @brief 설정 문자열에서 프로토콜 타입 추출
     * @param config_str 설정 문자열 (예: "protocol=modbus_tcp")
     * @return 프로토콜 타입
     */
    ProtocolType ExtractProtocolFromConfig(const std::string& config_str) {
        // 간단한 파싱 (실제로는 JSON 파서 사용 권장)
        std::string protocol_key = "protocol=";
        size_t pos = config_str.find(protocol_key);
        
        if (pos != std::string::npos) {
            size_t start = pos + protocol_key.length();
            size_t end = config_str.find_first_of(" \t\n\r;,", start);
            
            if (end == std::string::npos) {
                end = config_str.length();
            }
            
            std::string protocol_str = config_str.substr(start, end - start);
            return StringToProtocolType(protocol_str);
        }
        
        throw std::invalid_argument("Protocol not specified in config string");
    }
    
    /**
     * @brief 드라이버 성능 테스트
     * @param driver 테스트할 드라이버
     * @param test_config 테스트 설정
     * @return 테스트 결과
     */
    struct PerformanceTestResult {
        bool success;
        std::chrono::milliseconds connection_time;
        std::chrono::milliseconds avg_read_time;
        size_t successful_reads;
        size_t failed_reads;
        std::string error_message;
    };
    
    PerformanceTestResult TestDriverPerformance(
        IProtocolDriver* driver, 
        const std::vector<DataPoint>& test_points,
        int test_iterations = 10) {
        
        PerformanceTestResult result = {};
        
        if (!driver) {
            result.error_message = "Driver is null";
            return result;
        }
        
        try {
            // 연결 시간 측정
            auto start_time = std::chrono::steady_clock::now();
            bool connected = driver->Connect();
            auto connection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
            
            result.connection_time = connection_time;
            
            if (!connected) {
                result.error_message = "Failed to connect: " + driver->GetLastError().message;
                return result;
            }
            
            // 읽기 성능 테스트
            std::chrono::milliseconds total_read_time(0);
            
            for (int i = 0; i < test_iterations; ++i) {
                auto read_start = std::chrono::steady_clock::now();
                
                std::vector<TimestampedValue> values;
                bool read_success = driver->ReadValues(test_points, values);
                
                auto read_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - read_start);
                total_read_time += read_time;
                
                if (read_success) {
                    result.successful_reads++;
                } else {
                    result.failed_reads++;
                }
                
                // 테스트 간 짧은 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            result.avg_read_time = total_read_time / test_iterations;
            result.success = true;
            
            driver->Disconnect();
            
        } catch (const std::exception& e) {
            result.error_message = "Test exception: " + std::string(e.what());
        }
        
        return result;
    }
    
    /**
     * @brief 모든 등록된 드라이버의 상태 확인
     * @return 드라이버 상태 정보 맵
     */
    std::map<ProtocolType, std::string> GetAllDriverStatus() {
        std::map<ProtocolType, std::string> status_map;
        auto& factory = DriverFactory::GetInstance();
        auto protocols = factory.GetSupportedProtocols();
        
        for (const auto& protocol : protocols) {
            try {
                auto driver = factory.CreateDriver(protocol);
                if (driver) {
                    status_map[protocol] = "Available";
                } else {
                    status_map[protocol] = "Creation Failed";
                }
            } catch (const std::exception& e) {
                status_map[protocol] = "Error: " + std::string(e.what());
            }
        }
        
        return status_map;
    }
    
} // namespace FactoryUtils

/**
 * @brief 드라이버 팩토리 관리자 클래스
 * 
 * 팩토리의 고급 관리 기능을 제공합니다.
 */
class DriverFactoryManager {
public:
    /**
     * @brief 싱글턴 인스턴스 반환
     */
    static DriverFactoryManager& GetInstance() {
        static DriverFactoryManager instance;
        return instance;
    }
    
    /**
     * @brief 팩토리 초기화 및 기본 드라이버 등록 확인
     * @return 성공 시 true
     */
    bool Initialize() {
        try {
            auto& factory = DriverFactory::GetInstance();
            
            // 기본 드라이버들이 등록되었는지 확인
            std::vector<ProtocolType> required_protocols = {
                ProtocolType::MODBUS_TCP,
                ProtocolType::MQTT,
                ProtocolType::BACNET_IP
            };
            
            bool all_registered = true;
            for (const auto& protocol : required_protocols) {
                if (!factory.IsProtocolSupported(protocol)) {
                    LogError("Required protocol not registered: " + 
                           ProtocolTypeToString(protocol));
                    all_registered = false;
                }
            }
            
            if (all_registered) {
                LogInfo("All required drivers registered successfully");
                LogInfo(FactoryUtils::GetRegisteredDriversInfo());
            }
            
            initialized_ = all_registered;
            return initialized_;
            
        } catch (const std::exception& e) {
            LogError("Factory manager initialization failed: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 드라이버 호환성 확인
     * @param protocol 프로토콜 타입
     * @param config 설정
     * @return 호환성 확인 결과
     */
    struct CompatibilityResult {
        bool compatible;
        std::vector<std::string> issues;
        std::vector<std::string> warnings;
    };
    
    CompatibilityResult CheckDriverCompatibility(ProtocolType protocol, 
                                                const DriverConfig& config) {
        CompatibilityResult result;
        result.compatible = true;
        
        try {
            auto& factory = DriverFactory::GetInstance();
            
            // 드라이버 지원 여부 확인
            if (!factory.IsProtocolSupported(protocol)) {
                result.compatible = false;
                result.issues.push_back("Protocol not supported: " + 
                                      ProtocolTypeToString(protocol));
                return result;
            }
            
            // 프로토콜별 설정 검증
            switch (protocol) {
                case ProtocolType::MODBUS_TCP:
                    CheckModbusConfig(config, result);
                    break;
                    
                case ProtocolType::MQTT:
                    CheckMqttConfig(config, result);
                    break;
                    
                case ProtocolType::BACNET_IP:
                    CheckBacnetConfig(config, result);
                    break;
                    
                default:
                    result.warnings.push_back("No specific validation for protocol: " + 
                                            ProtocolTypeToString(protocol));
                    break;
            }
            
        } catch (const std::exception& e) {
            result.compatible = false;
            result.issues.push_back("Compatibility check failed: " + std::string(e.what()));
        }
        
        return result;
    }
    
    /**
     * @brief 드라이버 팩토리 통계 정보
     */
    struct FactoryStatistics {
        size_t total_protocols;
        size_t successful_creations;
        size_t failed_creations;
        std::chrono::system_clock::time_point last_creation_time;
        std::map<ProtocolType, size_t> creation_count_by_protocol;
    };
    
    /**
     * @brief 팩토리 통계 반환
     */
    FactoryStatistics GetStatistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return statistics_;
    }
    
    /**
     * @brief 통계 리셋
     */
    void ResetStatistics() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_ = FactoryStatistics{};
        statistics_.total_protocols = DriverFactory::GetInstance().GetDriverCount();
    }
    
    /**
     * @brief 드라이버 생성 시 통계 업데이트 (팩토리에서 호출)
     */
    void OnDriverCreated(ProtocolType protocol, bool success) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        if (success) {
            statistics_.successful_creations++;
        } else {
            statistics_.failed_creations++;
        }
        
        statistics_.creation_count_by_protocol[protocol]++;
        statistics_.last_creation_time = std::chrono::system_clock::now();
    }
    
private:
    DriverFactoryManager() : initialized_(false) {}
    ~DriverFactoryManager() = default;
    
    // 복사 및 이동 방지
    DriverFactoryManager(const DriverFactoryManager&) = delete;
    DriverFactoryManager& operator=(const DriverFactoryManager&) = delete;
    
    bool initialized_;
    mutable std::mutex stats_mutex_;
    FactoryStatistics statistics_;
    
    // 설정 검증 메서드들
    void CheckModbusConfig(const DriverConfig& config, CompatibilityResult& result) {
        // Modbus 특화 검증
        if (config.endpoint.empty()) {
            result.compatible = false;
            result.issues.push_back("Modbus endpoint not specified");
        }
        
        // IP:Port 형식 검증
        size_t colon_pos = config.endpoint.find(':');
        if (colon_pos == std::string::npos) {
            result.warnings.push_back("Modbus endpoint should include port (e.g., 192.168.1.100:502)");
        }
        
        // 타임아웃 검증
        if (config.timeout_ms < 1000 || config.timeout_ms > 30000) {
            result.warnings.push_back("Modbus timeout should be between 1-30 seconds");
        }
    }
    
    void CheckMqttConfig(const DriverConfig& config, CompatibilityResult& result) {
        // MQTT 특화 검증
        if (config.endpoint.empty()) {
            result.compatible = false;
            result.issues.push_back("MQTT broker endpoint not specified");
        }
        
        // mqtt:// 프로토콜 확인
        if (config.endpoint.find("mqtt://") != 0 && config.endpoint.find("mqtts://") != 0) {
            result.warnings.push_back("MQTT endpoint should start with mqtt:// or mqtts://");
        }
        
        // Keep-alive 시간 검증
        if (config.retry_count > 10) {
            result.warnings.push_back("MQTT retry count seems too high (>10)");
        }
    }
    
    void CheckBacnetConfig(const DriverConfig& config, CompatibilityResult& result) {
        // BACnet 특화 검증
        if (config.device_id.empty()) {
            result.compatible = false;
            result.issues.push_back("BACnet device ID not specified");
        }
        
        // 디바이스 ID 범위 확인 (실제 구현에 따라 조정)
        try {
            uint32_t device_id = std::stoul(config.device_id);
            if (device_id == 0 || device_id > 4194303) {  // BACnet 디바이스 ID 범위
                result.warnings.push_back("BACnet device ID should be in range 1-4194303");
            }
        } catch (const std::exception&) {
            result.compatible = false;
            result.issues.push_back("Invalid BACnet device ID format");
        }
    }
    
    // 로깅 유틸리티
    void LogInfo(const std::string& message) {
        // 실제 구현에서는 PulseOne 로깅 시스템 사용
        // 여기서는 간단한 구현
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
            << "] [DriverFactory] [INFO] " << message;
        
        // 실제로는 LogManager를 통해 로깅
        std::cout << oss.str() << std::endl;
    }
    
    void LogError(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
            << "] [DriverFactory] [ERROR] " << message;
        
        std::cerr << oss.str() << std::endl;
    }
};

/**
 * @brief 편의를 위한 전역 함수들
 */

/**
 * @brief 간편한 드라이버 생성 함수
 * @param protocol_name 프로토콜 이름 (문자열)
 * @param endpoint 연결 엔드포인트
 * @param device_id 디바이스 ID
 * @return 생성된 드라이버 인스턴스
 */
std::unique_ptr<IProtocolDriver> CreateDriverSimple(
    const std::string& protocol_name,
    const std::string& endpoint,
    const std::string& device_id = "") {
    
    try {
        ProtocolType protocol = StringToProtocolType(protocol_name);
        
        DriverConfig config;
        config.device_id = device_id.empty() ? "default_device" : device_id;
        config.device_name = "Device via " + protocol_name;
        config.protocol_type = protocol;
        config.endpoint = endpoint;
        config.timeout_ms = 5000;  // 기본 5초 타임아웃
        config.retry_count = 3;    // 기본 3회 재시도
        config.is_enabled = true;
        
        return FactoryUtils::CreateAndInitializeDriver(protocol, config);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to create driver: " + std::string(e.what()));
    }
}

/**
 * @brief 설정 파일에서 드라이버 생성
 * @param config_file_path 설정 파일 경로
 * @return 드라이버 인스턴스들
 */
std::vector<std::unique_ptr<IProtocolDriver>> CreateDriversFromConfig(
    const std::string& config_file_path) {
    
    std::vector<std::unique_ptr<IProtocolDriver>> drivers;
    
    try {
        // 설정 파일 읽기 (실제로는 JSON 파서 사용)
        std::ifstream config_file(config_file_path);
        if (!config_file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + config_file_path);
        }
        
        // 간단한 구현 - 실제로는 JSON 라이브러리 사용 권장
        std::string line;
        while (std::getline(config_file, line)) {
            if (line.empty() || line[0] == '#') {
                continue;  // 빈 줄이나 주석 건너뛰기
            }
            
            // 각 라인에서 드라이버 설정 파싱
            // 형식: protocol=modbus_tcp,endpoint=192.168.1.100:502,device_id=device1
            DriverConfig config = ParseConfigLine(line);
            
            auto driver = FactoryUtils::CreateAndInitializeDriver(config.protocol_type, config);
            if (driver) {
                drivers.push_back(std::move(driver));
            }
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load drivers from config: " + std::string(e.what()));
    }
    
    return drivers;
}

/**
 * @brief 드라이버 팩토리 초기화 (프로그램 시작 시 호출)
 * @return 성공 시 true
 */
bool InitializeDriverFactory() {
    return DriverFactoryManager::GetInstance().Initialize();
}

/**
 * @brief 설정 라인 파싱 유틸리티
 */
DriverConfig ParseConfigLine(const std::string& line) {
    DriverConfig config;
    
    // 키=값 쌍을 콤마로 분리하여 파싱
    std::istringstream iss(line);
    std::string pair;
    
    while (std::getline(iss, pair, ',')) {
        size_t equals_pos = pair.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = pair.substr(0, equals_pos);
            std::string value = pair.substr(equals_pos + 1);
            
            // 앞뒤 공백 제거
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // 설정 적용
            if (key == "protocol") {
                config.protocol_type = StringToProtocolType(value);
            } else if (key == "endpoint") {
                config.endpoint = value;
            } else if (key == "device_id") {
                config.device_id = value;
            } else if (key == "device_name") {
                config.device_name = value;
            } else if (key == "timeout") {
                config.timeout_ms = std::stoul(value);
            } else if (key == "retry_count") {
                config.retry_count = std::stoul(value);
            } else if (key == "enabled") {
                config.is_enabled = (value == "true" || value == "1");
            } else if (key == "protocol_config") {
                config.protocol_config = value;
            }
        }
    }
    
    return config;
}

} // namespace Drivers
} // namespace PulseOne