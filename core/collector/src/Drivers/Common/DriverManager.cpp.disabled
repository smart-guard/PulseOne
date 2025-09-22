// collector/src/Drivers/DriverManager.cpp
// PulseOne 드라이버 매니저 구현

#include "Drivers/DriverManager.h"
#include <fstream>
#include <sstream>

namespace PulseOne {
namespace Drivers {

DriverManager::DriverManager(std::shared_ptr<LogManager> logger, 
                           std::shared_ptr<ConfigManager> config)
    : logger_(logger), config_(config), is_initialized_(false), is_running_(false) {
}

DriverManager::~DriverManager() {
    Cleanup();
}

bool DriverManager::Initialize() {
    try {
        logger_->Info("Initializing Driver Manager...");
        
        if (!ValidateConfiguration()) {
            logger_->Error("Driver configuration validation failed");
            return false;
        }

#ifdef HAVE_DRIVER_SYSTEM
        // 드라이버 팩토리 초기화 확인
        auto& factory = DriverFactory::GetInstance();
        auto available_protocols = factory.GetAvailableProtocols();
        
        logger_->Info("Available protocol drivers: " + std::to_string(available_protocols.size()));
        for (const auto& protocol : available_protocols) {
            logger_->Info("  - " + ProtocolTypeToString(protocol));
        }
#else
        logger_->Info("Using legacy driver system (new driver system disabled)");
#endif

        is_initialized_ = true;
        logger_->Info("Driver Manager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Driver Manager initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool DriverManager::LoadDriversFromConfig(const std::string& config_path) {
    if (!is_initialized_) {
        logger_->Error("Driver Manager not initialized");
        return false;
    }

    try {
        logger_->Info("Loading drivers from config: " + config_path);

#ifdef HAVE_DRIVER_SYSTEM
        // 설정 파일에서 드라이버 생성
        active_drivers_ = CreateDriversFromConfig(config_path);
        
        logger_->Info("Loaded " + std::to_string(active_drivers_.size()) + " drivers from config");
        return !active_drivers_.empty();
#else
        logger_->Info("Driver config loading skipped (new driver system disabled)");
        return true;
#endif
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to load drivers from config: " + std::string(e.what()));
        return false;
    }
}

bool DriverManager::StartAllDrivers() {
    if (!is_initialized_) {
        logger_->Error("Driver Manager not initialized");
        return false;
    }

    try {
        logger_->Info("Starting all drivers...");

#ifdef HAVE_DRIVER_SYSTEM
        size_t started_count = 0;
        for (auto& driver : active_drivers_) {
            if (driver && driver->Start()) {
                started_count++;
                logger_->Info("Started driver: " + driver->GetDriverName());
            } else {
                logger_->Warn("Failed to start driver: " + 
                               (driver ? driver->GetDriverName() : "unknown"));
            }
        }
        
        is_running_ = (started_count > 0);
        logger_->Info("Started " + std::to_string(started_count) + "/" + 
                     std::to_string(active_drivers_.size()) + " drivers");
#else
        logger_->Info("Driver startup skipped (new driver system disabled)");
        is_running_ = true;
#endif

        return is_running_;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to start drivers: " + std::string(e.what()));
        return false;
    }
}

void DriverManager::StopAllDrivers() {
    if (!is_running_) {
        return;
    }

    try {
        logger_->Info("Stopping all drivers...");

#ifdef HAVE_DRIVER_SYSTEM
        for (auto& driver : active_drivers_) {
            if (driver) {
                driver->Stop();
                logger_->Info("Stopped driver: " + driver->GetDriverName());
            }
        }
#endif

        is_running_ = false;
        logger_->Info("All drivers stopped");
        
    } catch (const std::exception& e) {
        logger_->Error("Error stopping drivers: " + std::string(e.what()));
    }
}

size_t DriverManager::GetActiveDriverCount() const {
#ifdef HAVE_DRIVER_SYSTEM
    if (!is_running_) return 0;
    
    size_t active_count = 0;
    for (const auto& driver : active_drivers_) {
        if (driver && driver->IsConnected()) {
            active_count++;
        }
    }
    return active_count;
#else
    return is_running_ ? 1 : 0;  // 레거시 시스템에서는 단순히 실행 상태만 반환
#endif
}

std::string DriverManager::GetStatistics() const {
    std::ostringstream stats;
    stats << "Driver Manager Statistics:\n";
    stats << "  Initialized: " << (is_initialized_ ? "Yes" : "No") << "\n";
    stats << "  Running: " << (is_running_ ? "Yes" : "No") << "\n";
    
#ifdef HAVE_DRIVER_SYSTEM
    stats << "  Total Drivers: " << active_drivers_.size() << "\n";
    stats << "  Active Drivers: " << GetActiveDriverCount() << "\n";
    
    // 드라이버별 상태
    for (const auto& driver : active_drivers_) {
        if (driver) {
            stats << "  - " << driver->GetDriverName() 
                  << ": " << (driver->IsConnected() ? "Connected" : "Disconnected") << "\n";
        }
    }
#else
    stats << "  Driver System: Legacy\n";
#endif
    
    return stats.str();
}

void DriverManager::Cleanup() {
    if (is_running_) {
        StopAllDrivers();
    }

#ifdef HAVE_DRIVER_SYSTEM
    active_drivers_.clear();
    driver_statistics_.clear();
#endif

    is_initialized_ = false;
    
    if (logger_) {
        logger_->Info("Driver Manager cleaned up");
    }
}

bool DriverManager::ValidateConfiguration() const {
    try {
        // 기본 설정 확인
        std::string driver_config_path = config_->GetValue("driver_config_path", "");
        if (driver_config_path.empty()) {
            logger_->Warn("No driver config path specified, using defaults");
        }

        // 추가 검증 로직이 필요하면 여기에 추가
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Configuration validation error: " + std::string(e.what()));
        return false;
    }
}

void DriverManager::UpdateStatistics() {
    // 통계 업데이트 로직 (필요시 구현)
}

} // namespace Drivers
} // namespace PulseOne