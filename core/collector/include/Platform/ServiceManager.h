#ifndef PULSEONE_PLATFORM_SERVICE_MANAGER_H
#define PULSEONE_PLATFORM_SERVICE_MANAGER_H

#include "Platform/PlatformCompat.h"
#include "Utils/LogManager.h"
#include <string>
#include <functional>

namespace PulseOne {
namespace Platform {

/**
 * @brief 서비스 관리 인터페이스
 * 
 * 시스템 서비스(Windows) 또는 데몬(Linux)으로 동작하기 위한 추상화 레이어입니다.
 */
class ServiceManager {
public:
    using ApplicationCallback = std::function<int()>;

    ServiceManager(const std::string& service_name, const std::string& display_name)
        : service_name_(service_name), display_name_(display_name) {}

    virtual ~ServiceManager() = default;

    /**
     * @brief 서비스를 실행합니다. (블로킹 호출)
     */
    virtual int Run(ApplicationCallback start_callback) = 0;

    /**
     * @brief 서비스 설치 (관리자 권한 필요)
     */
    virtual bool InstallService() = 0;

    /**
     * @brief 서비스 제거 (관리자 권한 필요)
     */
    virtual bool UninstallService() = 0;

protected:
    std::string service_name_;
    std::string display_name_;
};

// 플랫폼별 구현 클래스 선언은 여기에 포함하거나 별도 파일로 분리 가능
#if PULSEONE_WINDOWS
class WindowsServiceManager : public ServiceManager {
public:
    using ServiceManager::ServiceManager;
    
    int Run(ApplicationCallback start_callback) override {
        LogManager::getInstance().Info("Starting as Windows Service: " + service_name_);
        // TODO: ServiceMain, HandlerEx 등록 로직 구현
        return start_callback(); 
    }

    bool InstallService() override {
        // SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
        return true;
    }

    bool UninstallService() override {
        return true;
    }
};
#else
class LinuxDaemonManager : public ServiceManager {
public:
    using ServiceManager::ServiceManager;

    int Run(ApplicationCallback start_callback) override {
        LogManager::getInstance().Info("Starting as Linux Daemon: " + service_name_);
        // systemd는 포크가 필요 없으므로 바로 실행하거나 fork() 사용
        return start_callback();
    }

    bool InstallService() override {
        LogManager::getInstance().Info("Suggested: Create systemd unit file for " + service_name_);
        return true;
    }

    bool UninstallService() override {
        return true;
    }
};
#endif

/**
 * @brief 플랫폼에 맞는 ServiceManager 인스턴스를 생성합니다.
 */
inline std::unique_ptr<ServiceManager> CreateServiceManager(const std::string& name, const std::string& display) {
#if PULSEONE_WINDOWS
    return std::make_unique<WindowsServiceManager>(name, display);
#else
    return std::make_unique<LinuxDaemonManager>(name, display);
#endif
}

} // namespace Platform
} // namespace PulseOne

#endif // PULSEONE_PLATFORM_SERVICE_MANAGER_H
