// =============================================================================
// collector/src/Core/CollectorApplication.cpp
// PulseOne Collector 애플리케이션 구현
// =============================================================================

#include "Core/CollectorApplication.h"
#include "Drivers/ModbusDriver.h"
#include "Drivers/MqttDriver.h"
#include "Drivers/BACnetDriver.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace PulseOne::Core;
using namespace PulseOne::Drivers;
using namespace std::chrono;

// =============================================================================
// 생성자/소멸자
// =============================================================================

CollectorApplication::CollectorApplication()
    : current_state_(State::UNINITIALIZED)
    , running_(false)
    , last_stats_update_(system_clock::now())
{
    system_statistics_ = json::object();
}

CollectorApplication::~CollectorApplication() {
    if (running_) {
        Stop();
    }
}

// =============================================================================
// 메인 생명주기 관리
// =============================================================================

bool CollectorApplication::Initialize() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (current_state_ != State::UNINITIALIZED) {
        return false;
    }
    
    current_state_ = State::INITIALIZING;
    status_message_ = "Initializing system components...";
    
    try {
        std::cout << "🚀 PulseOne Collector 초기화 시작..." << std::endl;
        
        // 1. 로그 시스템 초기화
        if (!InitializeLogSystem()) {
            throw std::runtime_error("로그 시스템 초기화 실패");
        }
        
        // 2. 설정 관리자 초기화
        if (!InitializeConfiguration()) {
            throw std::runtime_error("설정 시스템 초기화 실패");
        }
        
        // 3. 데이터베이스 초기화
        if (!InitializeDatabase()) {
            throw std::runtime_error("데이터베이스 초기화 실패");
        }
        
        // 4. 드라이버 시스템 초기화
        if (!InitializeDriverSystem()) {
            throw std::runtime_error("드라이버 시스템 초기화 실패");
        }
        
        // 5. 웹 서비스 초기화
        if (!InitializeWebServices()) {
            throw std::runtime_error("웹 서비스 초기화 실패");
        }
        
        current_state_ = State::INITIALIZED;
        status_message_ = "초기화 완료";
        
        log_manager_->Info("CollectorApplication 초기화 완료");
        NotifyStatusChanged();
        
        return true;
        
    } catch (const std::exception& e) {
        current_state_ = State::ERROR;
        status_message_ = "초기화 실패: " + std::string(e.what());
        
        std::cerr << "❌ " << status_message_ << std::endl;
        return false;
    }
}

bool CollectorApplication::Start() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (current_state_ != State::INITIALIZED) {
        return false;
    }
    
    try {
        // 1. 데이터베이스에서 디바이스 목록 로드
        if (!LoadDevicesFromDatabase()) {
            throw std::runtime_error("디바이스 설정 로드 실패");
        }
        
        // 2. 드라이버들 생성 및 시작
        if (!CreateDeviceDrivers()) {
            throw std::runtime_error("드라이버 생성 실패");
        }
        
        // 3. 백그라운드 스레드들 시작
        running_ = true;
        main_thread_ = std::thread(&CollectorApplication::MainLoop, this);
        health_check_thread_ = std::thread(&CollectorApplication::HealthCheckLoop, this);
        
        current_state_ = State::RUNNING;
        status_message_ = "실행 중";
        
        log_manager_->Info("CollectorApplication 시작됨");
        NotifyStatusChanged();
        
        return true;
        
    } catch (const std::exception& e) {
        current_state_ = State::ERROR;
        status_message_ = "시작 실패: " + std::string(e.what());
        
        log_manager_->Error(status_message_);
        return false;
    }
}

void CollectorApplication::Stop() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != State::RUNNING) {
            return;
        }
        
        current_state_ = State::STOPPING;
        status_message_ = "중지 중...";
    }
    
    log_manager_->Info("CollectorApplication 중지 시작");
    
    // 1. 실행 플래그 해제
    running_ = false;
    
    // 2. 스레드들 종료 대기
    if (main_thread_.joinable()) {
        main_thread_.join();
    }
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
    
    // 3. 시스템 컴포넌트들 정리
    ShutdownDrivers();
    ShutdownWebServices();
    ShutdownDatabase();
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        current_state_ = State::STOPPED;
        status_message_ = "중지됨";
    }
    
    log_manager_->Info("CollectorApplication 중지 완료");
    NotifyStatusChanged();
}

void CollectorApplication::Run() {
    if (!Initialize()) {
        throw std::runtime_error("초기화 실패");
    }
    
    if (!Start()) {
        throw std::runtime_error("시작 실패");
    }
    
    // 메인 스레드 완료 대기
    if (main_thread_.joinable()) {
        main_thread_.join();
    }
}

CollectorApplication::SystemStatus CollectorApplication::GetStatus() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    SystemStatus status;
    status.state = current_state_;
    status.message = status_message_;
    status.last_update = system_clock::now();
    
    // 드라이버 통계 계산
    {
        std::lock_guard<std::mutex> devices_lock(devices_mutex_);
        status.active_drivers = active_drivers_.size();
        
        int connected = 0, error_count = 0;
        for (const auto& [id, driver] : active_drivers_) {
            if (driver->IsConnected()) {
                connected++;
            } else if (driver->GetStatus() == DriverStatus::ERROR) {
                error_count++;
            }
        }
        status.connected_devices = connected;
        status.error_devices = error_count;
    }
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        status.statistics = system_statistics_;
    }
    
    return status;
}

bool CollectorApplication::IsRunning() const {
    return running_.load();
}

// =============================================================================
// 🌐 웹 클라이언트 제어 API (핵심 구현!)
// =============================================================================

bool CollectorApplication::ReloadConfiguration() {
    std::lock_guard<std::mutex> lock(command_mutex_);
    
    // 명령을 큐에 추가 (메인 스레드에서 안전하게 처리)
    pending_commands_.push([this]() {
        try {
            log_manager_->Info("설정 다시 로드 시작");
            
            // 1. 설정 파일 다시 읽기
            config_manager_->Reload();
            
            // 2. 데이터베이스 연결 갱신
            database_manager_->Reconnect();
            
            // 3. 디바이스 설정 다시 로드
            LoadDevicesFromDatabase();
            
            log_manager_->Info("설정 다시 로드 완료");
            
        } catch (const std::exception& e) {
            log_manager_->Error("설정 다시 로드 실패: " + std::string(e.what()));
        }
    });
    
    return true;
}

bool CollectorApplication::ReinitializeDrivers() {
    std::lock_guard<std::mutex> lock(command_mutex_);
    
    pending_commands_.push([this]() {
        try {
            log_manager_->Info("드라이버 재초기화 시작");
            
            // 1. 기존 드라이버들 정리
            {
                std::lock_guard<std::mutex> devices_lock(devices_mutex_);
                for (auto& [id, driver] : active_drivers_) {
                    driver->Disconnect();
                }
                active_drivers_.clear();
            }
            
            // 2. 새로운 드라이버들 생성
            CreateDeviceDrivers();
            
            log_manager_->Info("드라이버 재초기화 완료");
            
        } catch (const std::exception& e) {
            log_manager_->Error("드라이버 재초기화 실패: " + std::string(e.what()));
        }
    });
    
    return true;
}

bool CollectorApplication::StartDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(command_mutex_);
    
    pending_commands_.push([this, device_id]() {
        try {
            std::lock_guard<std::mutex> devices_lock(devices_mutex_);
            
            auto it = active_drivers_.find(device_id);
            if (it == active_drivers_.end()) {
                // 드라이버가 없으면 새로 생성
                auto device_it = device_configs_.find(device_id);
                if (device_it == device_configs_.end()) {
                    throw std::runtime_error("디바이스 설정을 찾을 수 없습니다: " + device_id);
                }
                
                auto driver = CreateSingleDriver(device_it->second);
                if (!driver) {
                    throw std::runtime_error("드라이버 생성 실패: " + device_id);
                }
                
                active_drivers_[device_id] = driver;
                it = active_drivers_.find(device_id);
            }
            
            // 연결 시도
            if (it->second->Connect()) {
                log_manager_->Info("디바이스 연결 성공: " + device_id);
                NotifyDeviceStatusChanged(device_id, {{"status", "connected"}});
            } else {
                log_manager_->Error("디바이스 연결 실패: " + device_id);
                NotifyDeviceStatusChanged(device_id, {{"status", "error"}});
            }
            
        } catch (const std::exception& e) {
            log_manager_->Error("디바이스 시작 실패 " + device_id + ": " + std::string(e.what()));
            NotifyDeviceStatusChanged(device_id, {{"status", "error"}, {"error", e.what()}});
        }
    });
    
    return true;
}

bool CollectorApplication::StopDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(command_mutex_);
    
    pending_commands_.push([this, device_id]() {
        try {
            std::lock_guard<std::mutex> devices_lock(devices_mutex_);
            
            auto it = active_drivers_.find(device_id);
            if (it != active_drivers_.end()) {
                it->second->Disconnect();
                log_manager_->Info("디바이스 중지됨: " + device_id);
                NotifyDeviceStatusChanged(device_id, {{"status", "disconnected"}});
            }
            
        } catch (const std::exception& e) {
            log_manager_->Error("디바이스 중지 실패 " + device_id + ": " + std::string(e.what()));
        }
    });
    
    return true;
}

bool CollectorApplication::RestartDevice(const std::string& device_id) {
    // 중지 후 시작
    StopDevice(device_id);
    
    // 잠시 대기 후 시작
    std::lock_guard<std::mutex> lock(command_mutex_);
    pending_commands_.push([this, device_id]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        StartDevice(device_id);
    });
    
    return true;
}

json CollectorApplication::GetDeviceList() const {
    std::lock_guard<std::mutex> devices_lock(devices_mutex_);
    
    json device_list = json::array();
    
    for (const auto& [id, config] : device_configs_) {
        json device_info = config;
        
        // 실시간 상태 추가
        auto driver_it = active_drivers_.find(id);
        if (driver_it != active_drivers_.end()) {
            auto driver = driver_it->second;
            device_info["connection_status"] = driver->IsConnected() ? "connected" : "disconnected";
            device_info["driver_status"] = static_cast<int>(driver->GetStatus());
            device_info["last_error"] = driver->GetLastError().message;
            
            auto stats = driver->GetStatistics();
            device_info["statistics"] = {
                {"total_reads", stats.total_reads},
                {"successful_reads", stats.successful_reads},
                {"failed_reads", stats.failed_reads},
                {"success_rate", stats.GetSuccessRate()}
            };
        } else {
            device_info["connection_status"] = "not_loaded";
            device_info["driver_status"] = -1;
        }
        
        device_list.push_back(device_info);
    }
    
    return device_list;
}

json CollectorApplication::GetDeviceStatus(const std::string& device_id) const {
    std::lock_guard<std::mutex> devices_lock(devices_mutex_);
    
    json status = json::object();
    
    auto config_it = device_configs_.find(device_id);
    if (config_it != device_configs_.end()) {
        status = config_it->second;
    }
    
    auto driver_it = active_drivers_.find(device_id);
    if (driver_it != active_drivers_.end()) {
        auto driver = driver_it->second;
        
        status["connection_status"] = driver->IsConnected() ? "connected" : "disconnected";
        status["driver_status"] = static_cast<int>(driver->GetStatus());
        status["last_error"] = driver->GetLastError().message;
        
        auto stats = driver->GetStatistics();
        status["statistics"] = {
            {"total_reads", stats.total_reads},
            {"successful_reads", stats.successful_reads},
            {"failed_reads", stats.failed_reads},
            {"total_writes", stats.total_writes},
            {"successful_writes", stats.successful_writes},
            {"failed_writes", stats.failed_writes},
            {"success_rate", stats.GetSuccessRate()},
            {"average_response_time", stats.average_response_time_ms},
            {"max_response_time", stats.max_response_time_ms}
        };
        
        // 진단 정보 (지원하는 드라이버인 경우)
        auto diagnostics = driver->GetDiagnostics();
        if (!diagnostics.empty()) {
            status["diagnostics"] = diagnostics;
        }
    } else {
        status["connection_status"] = "not_loaded";
        status["driver_status"] = -1;
    }
    
    status["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    return status;
}

bool CollectorApplication::SetDiagnostics(const std::string& device_id, bool enabled) {
    std::lock_guard<std::mutex> lock(command_mutex_);
    
    pending_commands_.push([this, device_id, enabled]() {
        try {
            std::lock_guard<std::mutex> devices_lock(devices_mutex_);
            
            auto it = active_drivers_.find(device_id);
            if (it == active_drivers_.end()) {
                throw std::runtime_error("드라이버를 찾을 수 없습니다: " + device_id);
            }
            
            // 진단 기능을 지원하는 드라이버인지 확인 후 활성화
            // (각 드라이버별로 캐스팅해서 진단 메소드 호출)
            
            auto modbus_driver = std::dynamic_pointer_cast<ModbusDriver>(it->second);
            if (modbus_driver) {
                if (enabled) {
                    modbus_driver->EnableDiagnostics(*database_manager_, true, false);
                } else {
                    modbus_driver->DisableDiagnostics();
                }
                log_manager_->Info("Modbus 진단 " + std::string(enabled ? "활성화" : "비활성화") + 
                                 ": " + device_id);
                return;
            }
            
            auto mqtt_driver = std::dynamic_pointer_cast<MqttDriver>(it->second);
            if (mqtt_driver) {
                if (enabled) {
                    mqtt_driver->EnableDiagnostics(*database_manager_, true, false);
                } else {
                    mqtt_driver->DisableDiagnostics();
                }
                log_manager_->Info("MQTT 진단 " + std::string(enabled ? "활성화" : "비활성화") + 
                                 ": " + device_id);
                return;
            }
            
            auto bacnet_driver = std::dynamic_pointer_cast<BACnetDriver>(it->second);
            if (bacnet_driver) {
                if (enabled) {
                    bacnet_driver->EnableDiagnostics(*database_manager_, true, false);
                } else {
                    bacnet_driver->DisableDiagnostics();
                }
                log_manager_->Info("BACnet 진단 " + std::string(enabled ? "활성화" : "비활성화") + 
                                 ": " + device_id);
                return;
            }
            
            throw std::runtime_error("진단을 지원하지 않는 드라이버입니다: " + device_id);
            
        } catch (const std::exception& e) {
            log_manager_->Error("진단 설정 실패 " + device_id + ": " + std::string(e.what()));
        }
    });
    
    return true;
}

json CollectorApplication::GetSystemStatistics() const {
    json stats;
    
    {
        std::lock_guard<std::mutex> devices_lock(devices_mutex_);
        stats["total_devices"] = device_configs_.size();
        stats["active_drivers"] = active_drivers_.size();
        
        int connected = 0, errors = 0;
        for (const auto& [id, driver] : active_drivers_) {
            if (driver->IsConnected()) {
                connected++;
            } else if (driver->GetStatus() == DriverStatus::ERROR) {
                errors++;
            }
        }
        stats["connected_devices"] = connected;
        stats["error_devices"] = errors;
    }
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        stats["application_state"] = static_cast<int>(current_state_);
        stats["status_message"] = status_message_;
    }
    
    stats["uptime_seconds"] = duration_cast<seconds>(
        system_clock::now() - last_stats_update_).count();
    
    stats["memory_usage"] = GetMemoryUsage();
    stats["cpu_usage"] = GetCpuUsage();
    
    return stats;
}

// =============================================================================
// 내부 헬퍼 메소드들
// =============================================================================

bool CollectorApplication::LoadDevicesFromDatabase() {
    try {
        log_manager_->Info("데이터베이스에서 디바이스 설정 로드 중...");
        
        std::string query = R"(
            SELECT 
                d.id,
                d.name,
                d.description,
                d.protocol_type,
                d.endpoint,
                d.username,
                d.password,
                d.is_enabled,
                d.protocol_settings
            FROM devices d
            WHERE d.is_enabled = true
            ORDER BY d.name
        )";
        
        auto result = database_manager_->ExecuteQuery(query);
        
        std::lock_guard<std::mutex> lock(devices_mutex_);
        device_configs_.clear();
        
        for (const auto& row : result) {
            json device_config;
            device_config["id"] = row["id"].as<std::string>();
            device_config["name"] = row["name"].as<std::string>();
            device_config["description"] = row["description"].as<std::string>("");
            device_config["protocol_type"] = row["protocol_type"].as<std::string>();
            device_config["endpoint"] = row["endpoint"].as<std::string>();
            device_config["username"] = row["username"].as<std::string>("");
            device_config["password"] = row["password"].as<std::string>("");
            device_config["is_enabled"] = row["is_enabled"].as<bool>();
            
            // 프로토콜 설정 파싱 (JSON 문자열인 경우)
            std::string protocol_settings = row["protocol_settings"].as<std::string>("");
            if (!protocol_settings.empty()) {
                try {
                    device_config["protocol_settings"] = json::parse(protocol_settings);
                } catch (const std::exception&) {
                    device_config["protocol_settings"] = json::object();
                }
            } else {
                device_config["protocol_settings"] = json::object();
            }
            
            device_configs_[device_config["id"]] = device_config;
        }
        
        log_manager_->Info("로드된 디바이스 수: " + std::to_string(device_configs_.size()));
        return true;
        
    } catch (const std::exception& e) {
        log_manager_->Error("디바이스 설정 로드 실패: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::CreateDeviceDrivers() {
    try {
        log_manager_->Info("디바이스 드라이버 생성 중...");
        
        std::lock_guard<std::mutex> lock(devices_mutex_);
        
        int created_count = 0;
        for (const auto& [device_id, config] : device_configs_) {
            try {
                auto driver = CreateSingleDriver(config);
                if (driver) {
                    active_drivers_[device_id] = driver;
                    created_count++;
                    
                    log_manager_->Info("드라이버 생성됨: " + config["name"].get<std::string>() + 
                                     " (" + config["protocol_type"].get<std::string>() + ")");
                }
            } catch (const std::exception& e) {
                log_manager_->Error("드라이버 생성 실패 " + device_id + ": " + std::string(e.what()));
            }
        }
        
        log_manager_->Info("생성된 드라이버 수: " + std::to_string(created_count));
        return created_count > 0;
        
    } catch (const std::exception& e) {
        log_manager_->Error("드라이버 생성 실패: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<IProtocolDriver> CollectorApplication::CreateSingleDriver(const json& config) {
    // DriverConfig 구조체 생성
    DriverConfig driver_config;
    driver_config.device_id = config["id"];
    driver_config.device_name = config["name"];
    driver_config.endpoint = config["endpoint"];
    driver_config.username = config["username"];
    driver_config.password = config["password"];
    
    std::string protocol = config["protocol_type"];
    
    if (protocol == "modbus") {
        driver_config.protocol_type = ProtocolType::MODBUS_TCP;
    } else if (protocol == "mqtt") {
        driver_config.protocol_type = ProtocolType::MQTT;
    } else if (protocol == "bacnet") {
        driver_config.protocol_type = ProtocolType::BACNET_IP;
    } else {
        throw std::runtime_error("지원하지 않는 프로토콜: " + protocol);
    }
    
    // 드라이버 생성
    auto& factory = DriverFactory::GetInstance();
    auto driver = factory.CreateDriver(driver_config.protocol_type);
    
    if (!driver) {
        throw std::runtime_error("드라이버 생성 실패: " + protocol);
    }
    
    // 초기화
    if (!driver->Initialize(driver_config)) {
        throw std::runtime_error("드라이버 초기화 실패: " + protocol);
    }
    
    return driver;
}

// 나머지 구현들...
void CollectorApplication::MainLoop() {
    log_manager_->Info("메인 루프 시작");
    
    while (running_) {
        try {
            // 1. 대기 중인 명령들 처리
            ProcessPendingCommands();
            
            // 2. 시스템 상태 업데이트
            UpdateSystemStatus();
            
            // 3. 100ms 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            log_manager_->Error("메인 루프 오류: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    log_manager_->Info("메인 루프 종료");
}

void CollectorApplication::ProcessPendingCommands() {
    std::lock_guard<std::mutex> lock(command_mutex_);
    
    while (!pending_commands_.empty()) {
        auto command = std::move(pending_commands_.front());
        pending_commands_.pop();
        
        try {
            command();
        } catch (const std::exception& e) {
            log_manager_->Error("명령 실행 오류: " + std::string(e.what()));
        }
    }
}