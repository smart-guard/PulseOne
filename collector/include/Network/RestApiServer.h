// =============================================================================
// collector/include/Network/RestApiServer.h
// 웹 클라이언트용 REST API 서버 (선언부만)
// =============================================================================

#ifndef PULSEONE_REST_API_SERVER_H
#define PULSEONE_REST_API_SERVER_H

#include <memory>
#include <functional>
#include <string>
#include <atomic>
#include <thread>
#include <map>

// JSON 라이브러리 (조건부 include)
#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
namespace PulseOne {
namespace Network {
using json = nlohmann::json;
}
}
#else
// JSON 라이브러리가 없는 경우 기본 구현 사용
namespace PulseOne {
namespace Network {
class json {
public:
    static json object() { return json{}; }
    static json array() { return json{}; }
    std::string dump() const { return "{}"; }
    json& operator[](const std::string& key) { return *this; }
    json& operator=(const std::string& value) { return *this; }
    json& operator=(bool value) { return *this; }
    json& operator=(int value) { return *this; }
    json& operator=(double value) { return *this; }
    json& operator=(const json& other) { return *this; }
    bool empty() const { return true; }
    void push_back(const json& item) {}
};
}
}
#endif

// HTTP 라이브러리 전방 선언
#ifdef HAVE_HTTPLIB
namespace httplib {
    class Request;
    class Response;
    class Server;
}
#else
// httplib가 없는 경우 더미 전방 선언
namespace httplib {
    class Request {
    public:
        std::string body;
        std::map<std::string, std::string> headers;
        std::vector<std::string> matches;
    };
    
    class Response {
    public:
        int status = 200;
        std::string body;
        std::map<std::string, std::string> headers;
        void set_header(const std::string& key, const std::string& value);
        void set_content(const std::string& content, const std::string& type);
    };
    
    class Server;
}
#endif

namespace PulseOne {
namespace Network {

/**
 * @brief CollectorApplication을 제어하는 REST API 서버
 * 
 * DeviceWorker 제어, 하드웨어 제어, 시스템 관리 등의 
 * 모든 웹 인터페이스를 제공합니다.
 */
class RestApiServer {
public:
    // ==========================================================================
    // 콜백 함수 타입들 (기존 + 확장)
    // ==========================================================================
    
    // 기본 시스템 제어
    using ReloadConfigCallback = std::function<bool()>;
    using ReinitializeCallback = std::function<bool()>;
    using SystemStatsCallback = std::function<json()>;
    
    // 디바이스 제어 (확장됨)
    using DeviceControlCallback = std::function<bool(const std::string&)>;
    using DeviceListCallback = std::function<json()>;
    using DeviceStatusCallback = std::function<json(const std::string&)>;
    using DiagnosticsCallback = std::function<bool(const std::string&, bool)>;
    
    // 🆕 DeviceWorker 스레드 제어
    using DeviceStartCallback = std::function<bool(const std::string&)>;
    using DeviceStopCallback = std::function<bool(const std::string&)>;
    using DevicePauseCallback = std::function<bool(const std::string&)>;
    using DeviceResumeCallback = std::function<bool(const std::string&)>;
    using DeviceRestartCallback = std::function<bool(const std::string&)>;
    
    // 🆕 하드웨어 제어
    using PumpControlCallback = std::function<bool(const std::string&, const std::string&, bool)>;  // device_id, pump_id, enable
    using ValveControlCallback = std::function<bool(const std::string&, const std::string&, double)>; // device_id, valve_id, position
    using SetpointChangeCallback = std::function<bool(const std::string&, const std::string&, double)>; // device_id, setpoint_id, value
    
    // 🆕 설정 관리
    using DeviceConfigCallback = std::function<bool(const json&)>;  // 디바이스 추가/수정/삭제
    using DataPointConfigCallback = std::function<bool(const std::string&, const json&)>; // 데이터포인트 관리
    using AlarmConfigCallback = std::function<bool(const json&)>;   // 알람 설정
    using VirtualPointConfigCallback = std::function<bool(const json&)>; // 가상포인트 설정
    
    // 🆕 사용자 관리
    using UserManagementCallback = std::function<json(const std::string&, const json&)>; // action, data
    
    // 🆕 시스템 관리
    using SystemBackupCallback = std::function<bool(const std::string&)>; // backup_path
    using LogDownloadCallback = std::function<std::string(const std::string&, const std::string&)>; // start_date, end_date

public:
    /**
     * @brief 생성자
     * @param port 서버 포트 번호 (기본값: 8080)
     */
    explicit RestApiServer(int port = 8080);
    
    /**
     * @brief 소멸자
     */
    ~RestApiServer();

    // ==========================================================================
    // 서버 생명주기 관리
    // ==========================================================================
    
    /**
     * @brief 서버 시작
     * @return 성공 시 true
     */
    bool Start();
    
    /**
     * @brief 서버 중지
     */
    void Stop();
    
    /**
     * @brief 서버 실행 상태 확인
     * @return 실행 중이면 true
     */
    bool IsRunning() const;

    // ==========================================================================
    // 기본 콜백 설정 (기존)
    // ==========================================================================
    
    void SetReloadConfigCallback(ReloadConfigCallback callback);
    void SetReinitializeCallback(ReinitializeCallback callback);
    void SetDeviceListCallback(DeviceListCallback callback);
    void SetDeviceStatusCallback(DeviceStatusCallback callback);
    void SetSystemStatsCallback(SystemStatsCallback callback);
    void SetDiagnosticsCallback(DiagnosticsCallback callback);

    // ==========================================================================
    // 🆕 DeviceWorker 스레드 제어 콜백 설정
    // ==========================================================================
    
    void SetDeviceStartCallback(DeviceStartCallback callback);
    void SetDeviceStopCallback(DeviceStopCallback callback);
    void SetDevicePauseCallback(DevicePauseCallback callback);
    void SetDeviceResumeCallback(DeviceResumeCallback callback);
    void SetDeviceRestartCallback(DeviceRestartCallback callback);

    // ==========================================================================
    // 🆕 하드웨어 제어 콜백 설정
    // ==========================================================================
    
    void SetPumpControlCallback(PumpControlCallback callback);
    void SetValveControlCallback(ValveControlCallback callback);
    void SetSetpointChangeCallback(SetpointChangeCallback callback);

    // ==========================================================================
    // 🆕 설정 관리 콜백 설정
    // ==========================================================================
    
    void SetDeviceConfigCallback(DeviceConfigCallback callback);
    void SetDataPointConfigCallback(DataPointConfigCallback callback);
    void SetAlarmConfigCallback(AlarmConfigCallback callback);
    void SetVirtualPointConfigCallback(VirtualPointConfigCallback callback);

    // ==========================================================================
    // 🆕 사용자 및 시스템 관리 콜백 설정
    // ==========================================================================
    
    void SetUserManagementCallback(UserManagementCallback callback);
    void SetSystemBackupCallback(SystemBackupCallback callback);
    void SetLogDownloadCallback(LogDownloadCallback callback);

private:
    // ==========================================================================
    // 내부 메소드들
    // ==========================================================================
    
    void SetupRoutes();
    
    // 기존 API 핸들러들
    void HandleGetDevices(const httplib::Request& req, httplib::Response& res);
    void HandleGetDeviceStatus(const httplib::Request& req, httplib::Response& res);
    void HandlePostReloadConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePostReinitialize(const httplib::Request& req, httplib::Response& res);
    void HandleGetSystemStats(const httplib::Request& req, httplib::Response& res);
    void HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 DeviceWorker 스레드 제어 핸들러들
    void HandlePostDeviceStart(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceStop(const httplib::Request& req, httplib::Response& res);
    void HandlePostDevicePause(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceResume(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceRestart(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 하드웨어 제어 핸들러들
    void HandlePostPumpControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostValveControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostSetpointChange(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 디바이스 설정 핸들러들
    void HandleGetDeviceConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePutDeviceConfig(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteDeviceConfig(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 데이터포인트 설정 핸들러들
    void HandleGetDataPoints(const httplib::Request& req, httplib::Response& res);
    void HandlePostDataPoint(const httplib::Request& req, httplib::Response& res);
    void HandlePutDataPoint(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteDataPoint(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 알람 설정 핸들러들
    void HandleGetAlarmRules(const httplib::Request& req, httplib::Response& res);
    void HandlePostAlarmRule(const httplib::Request& req, httplib::Response& res);
    void HandlePutAlarmRule(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteAlarmRule(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 가상포인트 설정 핸들러들
    void HandleGetVirtualPoints(const httplib::Request& req, httplib::Response& res);
    void HandlePostVirtualPoint(const httplib::Request& req, httplib::Response& res);
    void HandlePutVirtualPoint(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteVirtualPoint(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 사용자 관리 핸들러들
    void HandleGetUsers(const httplib::Request& req, httplib::Response& res);
    void HandlePostUser(const httplib::Request& req, httplib::Response& res);
    void HandlePutUser(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteUser(const httplib::Request& req, httplib::Response& res);
    void HandlePostUserPermissions(const httplib::Request& req, httplib::Response& res);
    
    // 🆕 시스템 관리 핸들러들
    void HandlePostSystemBackup(const httplib::Request& req, httplib::Response& res);
    void HandleGetSystemLogs(const httplib::Request& req, httplib::Response& res);
    void HandleGetSystemConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePutSystemConfig(const httplib::Request& req, httplib::Response& res);
    
    // 유틸리티 메소드들
    void SetCorsHeaders(httplib::Response& res);
    json CreateErrorResponse(const std::string& error);
    json CreateSuccessResponse(const json& data = json::object());
    bool ValidateJsonSchema(const json& data, const std::string& schema_type);
    std::string ExtractDeviceId(const httplib::Request& req, int match_index = 1);

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    int port_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    
    // 기본 콜백들
    ReloadConfigCallback reload_config_callback_;
    ReinitializeCallback reinitialize_callback_;
    DeviceListCallback device_list_callback_;
    DeviceStatusCallback device_status_callback_;
    SystemStatsCallback system_stats_callback_;
    DiagnosticsCallback diagnostics_callback_;
    
    // 🆕 DeviceWorker 스레드 제어 콜백들
    DeviceStartCallback device_start_callback_;
    DeviceStopCallback device_stop_callback_;
    DevicePauseCallback device_pause_callback_;
    DeviceResumeCallback device_resume_callback_;
    DeviceRestartCallback device_restart_callback_;
    
    // 🆕 하드웨어 제어 콜백들
    PumpControlCallback pump_control_callback_;
    ValveControlCallback valve_control_callback_;
    SetpointChangeCallback setpoint_change_callback_;
    
    // 🆕 설정 관리 콜백들
    DeviceConfigCallback device_config_callback_;
    DataPointConfigCallback datapoint_config_callback_;
    AlarmConfigCallback alarm_config_callback_;
    VirtualPointConfigCallback virtualpoint_config_callback_;
    
    // 🆕 사용자 및 시스템 관리 콜백들
    UserManagementCallback user_management_callback_;
    SystemBackupCallback system_backup_callback_;
    LogDownloadCallback log_download_callback_;
};

} // namespace Network
} // namespace PulseOne

#endif // PULSEONE_REST_API_SERVER_H