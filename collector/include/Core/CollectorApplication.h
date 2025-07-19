// =============================================================================
// collector/include/Core/CollectorApplication.h
// PulseOne Collector 메인 애플리케이션 클래스
// =============================================================================

#ifndef PULSEONE_COLLECTOR_APPLICATION_H
#define PULSEONE_COLLECTOR_APPLICATION_H

#include <memory>
#include <atomic>
#include <map>
#include <vector>
#include <thread>
#include <functional>
#include <nlohmann/json.hpp>

#include "Config/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/DataAccessManager.h"
#include "Utils/LogManager.h"
#include "Drivers/DriverFactory.h"
#include "Drivers/DriverManager.h"
#include "Network/WebSocketServer.h"
#include "Network/RestApiServer.h"

namespace PulseOne {
namespace Core {

using json = nlohmann::json;

/**
 * @brief PulseOne Collector 메인 애플리케이션 클래스
 * 
 * 시스템의 전체 생명주기를 관리하며, 웹 클라이언트로부터의
 * 동적 제어 명령을 처리합니다.
 */
class CollectorApplication {
public:
    enum class State {
        UNINITIALIZED,
        INITIALIZING,
        INITIALIZED,
        RUNNING,
        STOPPING,
        STOPPED,
        ERROR
    };

    struct SystemStatus {
        State state;
        std::string message;
        int active_drivers;
        int connected_devices;
        int error_devices;
        std::chrono::system_clock::time_point last_update;
        json statistics;
    };

public:
    CollectorApplication();
    ~CollectorApplication();

    // ==========================================================================
    // 메인 생명주기 관리
    // ==========================================================================
    
    /**
     * @brief 애플리케이션 초기화
     * @return 성공 시 true
     */
    bool Initialize();
    
    /**
     * @brief 애플리케이션 시작
     * @return 성공 시 true
     */
    bool Start();
    
    /**
     * @brief 애플리케이션 중지
     */
    void Stop();
    
    /**
     * @brief 메인 실행 루프 (블로킹)
     */
    void Run();
    
    /**
     * @brief 현재 상태 조회
     */
    SystemStatus GetStatus() const;
    
    /**
     * @brief 실행 중인지 확인
     */
    bool IsRunning() const;

    // ==========================================================================
    // 🌐 웹 클라이언트 제어 API (핵심!)
    // ==========================================================================
    
    /**
     * @brief 데이터베이스에서 설정 다시 읽기
     * @return 성공 시 true
     */
    bool ReloadConfiguration();
    
    /**
     * @brief 드라이버들 다시 초기화
     * @return 성공 시 true
     */
    bool ReinitializeDrivers();
    
    /**
     * @brief 특정 디바이스 드라이버 시작
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool StartDevice(const std::string& device_id);
    
    /**
     * @brief 특정 디바이스 드라이버 중지
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool StopDevice(const std::string& device_id);
    
    /**
     * @brief 특정 디바이스 드라이버 재시작
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool RestartDevice(const std::string& device_id);
    
    /**
     * @brief 모든 디바이스 목록 조회
     * @return JSON 형태의 디바이스 목록
     */
    json GetDeviceList() const;
    
    /**
     * @brief 특정 디바이스 상태 조회
     * @param device_id 디바이스 ID
     * @return JSON 형태의 디바이스 상태
     */
    json GetDeviceStatus(const std::string& device_id) const;
    
    /**
     * @brief 진단 기능 활성화/비활성화
     * @param device_id 디바이스 ID
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool SetDiagnostics(const std::string& device_id, bool enabled);
    
    /**
     * @brief 시스템 통계 조회
     * @return JSON 형태의 시스템 통계
     */
    json GetSystemStatistics() const;

    // ==========================================================================
    // 콜백 및 이벤트 핸들러 등록
    // ==========================================================================
    
    /**
     * @brief 상태 변경 콜백 등록
     */
    void SetStatusCallback(std::function<void(const SystemStatus&)> callback);
    
    /**
     * @brief 디바이스 상태 변경 콜백 등록
     */
    void SetDeviceStatusCallback(std::function<void(const std::string&, const json&)> callback);

private:
    // ==========================================================================
    // 내부 초기화 메소드들
    // ==========================================================================
    
    bool InitializeLogSystem();
    bool InitializeConfiguration();
    bool InitializeDatabase();
    bool InitializeDriverSystem();
    bool InitializeWebServices();
    
    bool LoadDevicesFromDatabase();
    bool CreateDeviceDrivers();
    
    // ==========================================================================
    // 내부 관리 메소드들
    // ==========================================================================
    
    void MainLoop();
    void HealthCheckLoop();
    void UpdateSystemStatus();
    void ProcessPendingCommands();
    
    // 상태 변경 알림
    void NotifyStatusChanged();
    void NotifyDeviceStatusChanged(const std::string& device_id, const json& status);
    
    // 정리 메소드들
    void ShutdownWebServices();
    void ShutdownDrivers();
    void ShutdownDatabase();
    void ShutdownLogging();

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 상태 관리
    mutable std::mutex state_mutex_;
    State current_state_;
    std::string status_message_;
    
    // 실행 제어
    std::atomic<bool> running_;
    std::thread main_thread_;
    std::thread health_check_thread_;
    
    // 시스템 컴포넌트들
    std::shared_ptr<LogManager> log_manager_;
    std::shared_ptr<ConfigManager> config_manager_;
    std::shared_ptr<DatabaseManager> database_manager_;
    std::unique_ptr<Drivers::DriverManager> driver_manager_;
    
    // 웹 서비스들
    std::unique_ptr<Network::RestApiServer> api_server_;
    std::unique_ptr<Network::WebSocketServer> websocket_server_;
    
    // 디바이스 관리
    mutable std::mutex devices_mutex_;
    std::map<std::string, json> device_configs_;
    std::map<std::string, std::shared_ptr<Drivers::IProtocolDriver>> active_drivers_;
    
    // 콜백들
    std::function<void(const SystemStatus&)> status_callback_;
    std::function<void(const std::string&, const json&)> device_status_callback_;
    
    // 명령 큐 (웹에서 온 명령들)
    mutable std::mutex command_mutex_;
    std::queue<std::function<void()>> pending_commands_;
    
    // 통계
    mutable std::mutex stats_mutex_;
    json system_statistics_;
    std::chrono::system_clock::time_point last_stats_update_;
};

} // namespace Core
} // namespace PulseOne

#endif // PULSEONE_COLLECTOR_APPLICATION_H