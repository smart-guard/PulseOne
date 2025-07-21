// collector/include/Core/CollectorApplication.h
#pragma once

#include <atomic>
#include <chrono>
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

namespace PulseOne {
namespace Core {

class CollectorApplication {
private:
    LogManager& logger_;
    ConfigManager* config_;
    std::atomic<bool> running_{false};
    std::string version_ = "2.0.0-simple";
    std::chrono::system_clock::time_point start_time_;

public:
    CollectorApplication();
    ~CollectorApplication();
    
    CollectorApplication(const CollectorApplication&) = delete;
    CollectorApplication& operator=(const CollectorApplication&) = delete;
    
    void Run();
    void Stop();
    bool IsRunning() const { return running_.load(); }
    std::string GetVersion() const { return version_; }

private:
    bool Initialize();
    void MainLoop();
    void Cleanup();
    void PrintStatus();
};

} // namespace Core
} // namespace PulseOne
