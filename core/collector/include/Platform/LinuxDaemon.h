#ifndef PULSEONE_PLATFORM_LINUX_DAEMON_H
#define PULSEONE_PLATFORM_LINUX_DAEMON_H

#include "Platform/ServiceManager.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fstream>

namespace PulseOne {
namespace Platform {

class LinuxDaemon : public LinuxDaemonManager {
public:
    using LinuxDaemonManager::LinuxDaemonManager;

    int Run(ApplicationCallback start_callback) override {
        // systemd 환경에서는 이미 백그라운드이거나 포크가 필요 없으므로 바로 실행
        // 하지만 전통적인 데몬 모드 지원을 위해 PID 파일 관리 등 추가 가능
        
        LogManager::getInstance().Info("Linux Daemon [" + service_name_ + "] started.");
        
        // PID 파일 작성 (옵션)
        std::string pid_file = "/var/run/" + service_name_ + ".pid";
        std::ofstream ofs(pid_file);
        if (ofs.is_open()) {
            ofs << getpid();
            ofs.close();
        }

        int result = start_callback();

        // 종료 시 PID 파일 삭제
        unlink(pid_file.c_str());
        
        return result;
    }

    bool InstallService() override {
        // systemd unit 파일 생성 예시 로깅
        std::string unit_content = 
            "[Unit]\n"
            "Description=" + display_name_ + "\n"
            "After=network.target\n\n"
            "[Service]\n"
            "ExecStart=/usr/local/bin/pulseone-collector\n"
            "Restart=always\n\n"
            "[Install]\n"
            "WantedBy=multi-user.target\n";
            
        LogManager::getInstance().Info("To install, create /etc/systemd/system/" + service_name_ + ".service with content:\n" + unit_content);
        return true;
    }
};

} // namespace Platform
} // namespace PulseOne

#endif // PULSEONE_PLATFORM_LINUX_DAEMON_H
