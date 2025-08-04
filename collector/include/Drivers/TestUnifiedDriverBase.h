/**
 * @file TestUnifiedDriverBase.h
 * @brief PulseOne 테스트용 통합 드라이버 베이스 클래스
 */

#ifndef PULSEONE_TEST_UNIFIED_DRIVER_BASE_H
#define PULSEONE_TEST_UNIFIED_DRIVER_BASE_H

#include "../Common/NewTypes.h"
#include "../Common/CompatibilityBridge.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>
#include <queue>
#include <cmath>
#include <cstdlib>

namespace PulseOne {
namespace Drivers {
namespace Test {

// 통합 드라이버 통계 구조체
struct DriverStatistics {
    std::atomic<uint64_t> total_reads{0};
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> successful_reads{0};
    std::atomic<uint64_t> successful_writes{0};
    std::atomic<uint64_t> failed_reads{0};
    std::atomic<uint64_t> failed_writes{0};
    
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point last_activity;
    
    DriverStatistics() {
        started_at = std::chrono::system_clock::now();
        last_activity = started_at;
    }
    
    double GetSuccessRate() const {
        uint64_t total = total_reads.load() + total_writes.load();
        if (total == 0) return 0.0;
        
        uint64_t successful = successful_reads.load() + successful_writes.load();
        return (static_cast<double>(successful) / total) * 100.0;
    }
};

// 데이터 콜백 타입 정의
using DataCallback = std::function<void(const New::UnifiedDeviceInfo&, 
                                       const std::vector<New::UnifiedDataPoint>&)>;

using StatusCallback = std::function<void(const New::UnifiedDeviceInfo&, 
                                         New::ConnectionStatus,
                                         const std::string& message)>;

// 통합 드라이버 베이스 클래스
class UnifiedDriverBase {
protected:
    New::UnifiedDeviceInfo device_info_;
    std::vector<New::UnifiedDataPoint> data_points_;
    
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_initialized_{false};
    std::atomic<New::ConnectionStatus> connection_status_{New::ConnectionStatus::DISCONNECTED};
    
    std::unique_ptr<std::thread> worker_thread_;
    mutable std::mutex data_mutex_;
    mutable std::mutex callback_mutex_;
    
    DriverStatistics statistics_;
    DataCallback data_callback_;
    StatusCallback status_callback_;
    
public:
    explicit UnifiedDriverBase(const New::UnifiedDeviceInfo& device_info = {}) 
        : device_info_(device_info) {
        device_info_.status.store(New::ConnectionStatus::DISCONNECTED);
    }
    
    virtual ~UnifiedDriverBase() {
        Stop();
        if (worker_thread_ && worker_thread_->joinable()) {
            worker_thread_->join();
        }
    }
    
    // 핵심 인터페이스
    virtual bool Initialize(const New::UnifiedDeviceInfo& device_info) {
        device_info_ = device_info;
        is_initialized_.store(true);
        NotifyStatusChange(New::ConnectionStatus::DISCONNECTED, "Driver initialized");
        return InitializeProtocol();
    }
    
    virtual bool Start() {
        if (!is_initialized_.load()) {
            return false;
        }
        
        if (is_running_.load()) {
            return true;
        }
        
        is_running_.store(true);
        
        worker_thread_ = std::make_unique<std::thread>([this]() {
            WorkerLoop();
        });
        
        NotifyStatusChange(New::ConnectionStatus::CONNECTING, "Driver starting");
        return true;
    }
    
    virtual void Stop() {
        if (!is_running_.load()) {
            return;
        }
        
        is_running_.store(false);
        CleanupProtocol();
        
        if (worker_thread_ && worker_thread_->joinable()) {
            worker_thread_->join();
        }
        
        connection_status_.store(New::ConnectionStatus::DISCONNECTED);
        NotifyStatusChange(New::ConnectionStatus::DISCONNECTED, "Driver stopped");
    }
    
    // 콜백 설정
    void SetDataCallback(DataCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        data_callback_ = callback;
    }
    
    void SetStatusCallback(StatusCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        status_callback_ = callback;
    }
    
    // 상태 조회
    const DriverStatistics& GetStatistics() const { return statistics_; }
    New::ConnectionStatus GetConnectionStatus() const { return connection_status_.load(); }
    bool IsRunning() const { return is_running_.load(); }
    const New::UnifiedDeviceInfo& GetDeviceInfo() const { return device_info_; }
    
protected:
    // 프로토콜별 구현 필요
    virtual bool InitializeProtocol() = 0;
    virtual bool ConnectProtocol() = 0;
    virtual void DisconnectProtocol() = 0;
    virtual std::vector<New::UnifiedDataPoint> ReadDataProtocol() = 0;
    virtual bool WriteValueProtocol(const std::string& data_point_id, 
                                   const std::variant<bool, int32_t, uint32_t, float, double, std::string>& value) = 0;
    virtual void CleanupProtocol() = 0;
    
    // 공통 헬퍼 메서드들
    void WorkerLoop() {
        NotifyStatusChange(New::ConnectionStatus::CONNECTING, "Attempting connection");
        
        if (!ConnectProtocol()) {
            NotifyStatusChange(New::ConnectionStatus::ERROR, "Connection failed");
            return;
        }
        
        connection_status_.store(New::ConnectionStatus::CONNECTED);
        device_info_.status.store(New::ConnectionStatus::CONNECTED);
        NotifyStatusChange(New::ConnectionStatus::CONNECTED, "Connected successfully");
        
        while (is_running_.load()) {
            try {
                auto data_points = ReadDataProtocol();
                
                if (!data_points.empty()) {
                    statistics_.successful_reads.fetch_add(1);
                    statistics_.last_activity = std::chrono::system_clock::now();
                    NotifyDataReceived(data_points);
                } else {
                    statistics_.failed_reads.fetch_add(1);
                }
                
                statistics_.total_reads.fetch_add(1);
                
                // 🔧 수정: poll_interval_ms 사용
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(device_info_.poll_interval_ms));
                
            } catch (const std::exception& e) {
                statistics_.failed_reads.fetch_add(1);
                NotifyStatusChange(New::ConnectionStatus::ERROR, 
                                 "Read error: " + std::string(e.what()));
            }
        }
        
        DisconnectProtocol();
        connection_status_.store(New::ConnectionStatus::DISCONNECTED);
        device_info_.status.store(New::ConnectionStatus::DISCONNECTED);
    }
    
    void NotifyDataReceived(const std::vector<New::UnifiedDataPoint>& data_points) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (data_callback_) {
            data_callback_(device_info_, data_points);
        }
    }
    
    void NotifyStatusChange(New::ConnectionStatus status, const std::string& message) {
        connection_status_.store(status);
        device_info_.status.store(status);
        
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (status_callback_) {
            status_callback_(device_info_, status, message);
        }
    }
};

// 테스트용 Modbus 드라이버
class TestModbusDriver : public UnifiedDriverBase {
private:
    std::atomic<int> simulation_counter_{0};
    
public:
    TestModbusDriver() : UnifiedDriverBase() {
        device_info_.id = New::GenerateUUID();
        device_info_.name = "Test Modbus Driver";
        device_info_.protocol = New::ProtocolType::MODBUS_TCP;
        device_info_.endpoint = "127.0.0.1:502";
        device_info_.modbus_config.slave_id = 1;
    }
    
protected:
    bool InitializeProtocol() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    }
    
    bool ConnectProtocol() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return true;
    }
    
    void DisconnectProtocol() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::vector<New::UnifiedDataPoint> ReadDataProtocol() override {
        std::vector<New::UnifiedDataPoint> points;
        
        for (int i = 0; i < 3; ++i) {
            New::UnifiedDataPoint point;
            point.id = "register_" + std::to_string(i);
            point.device_id = device_info_.id;
            point.name = "Test Register " + std::to_string(i);
            
            double angle = (simulation_counter_.load() + i) * 0.1;
            point.current_value = 50.0 + 30.0 * std::sin(angle);
            point.quality = New::DataQuality::GOOD;
            // 🔧 수정: GetCurrentTime() 사용
            point.last_update = New::GetCurrentTime();
            
            points.push_back(point);
        }
        
        simulation_counter_.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        return points;
    }
    
    bool WriteValueProtocol(const std::string& data_point_id, 
                           const std::variant<bool, int32_t, uint32_t, float, double, std::string>& value) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return true;
    }
    
    void CleanupProtocol() override {
        simulation_counter_.store(0);
    }
};

} // namespace Test
} // namespace Drivers  
} // namespace PulseOne

#endif // PULSEONE_TEST_UNIFIED_DRIVER_BASE_H
