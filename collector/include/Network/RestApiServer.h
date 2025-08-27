// =============================================================================
// collector/include/Network/RestApiServer.h
// REST API 서버 헤더 - 컴파일 에러 완전 수정
// =============================================================================

#ifndef PULSEONE_REST_API_SERVER_H
#define PULSEONE_REST_API_SERVER_H

#include <memory>
#include <functional>
#include <string>
#include <atomic>
#include <thread>
#include <map>
#include <vector>

// JSON 라이브러리 수정 (nlohmann/json 우선 사용)
#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
namespace PulseOne {
namespace Network {
using json = nlohmann::json;
}
}
#else
// 더미 JSON 클래스 (컴파일만 되도록 최소한 구현)
namespace PulseOne {
namespace Network {
class json {
private:
    std::string data_ = "{}";
    
public:
    // 기본 생성자들 명시
    json() = default;
    json(const json& other) = default;
    json(json&& other) = default;
    json& operator=(const json& other) = default;
    json& operator=(json&& other) = default;
    
    // 팩토리 메서드들
    static json object() { return json{}; }
    static json array() { return json{}; }
    static json parse(const std::string& s) { return json{}; }
    
    // 변환 메서드들
    std::string dump() const { return data_; }
    bool empty() const { return data_ == "{}"; }
    bool contains(const std::string& key) const { return false; }
    
    // 연산자들 (체이닝 가능하도록)
    json& operator[](const std::string& key) { return *this; }
    json& operator=(const std::string& value) { data_ = "\"" + value + "\""; return *this; }
    json& operator=(bool value) { data_ = value ? "true" : "false"; return *this; }
    json& operator=(int value) { data_ = std::to_string(value); return *this; }
    json& operator=(double value) { data_ = std::to_string(value); return *this; }
    json& operator=(long value) { data_ = std::to_string(value); return *this; }  // 추가
    
    // 컨테이너 메서드들
    void push_back(const json& item) {}
    
    // 템플릿 value 메서드
    template<typename T>
    T value(const std::string& key, const T& default_value) const { 
        return default_value; 
    }
};
}
}
#endif

// HTTP 라이브러리 전방 선언
namespace httplib {
    class Request;
    class Response;
    class Server;
}

namespace PulseOne {
namespace Network {

class RestApiServer {
public:
    // 콜백 함수 타입들
    using ReloadConfigCallback = std::function<bool()>;
    using ReinitializeCallback = std::function<bool()>;
    using SystemStatsCallback = std::function<json()>;
    using DeviceControlCallback = std::function<bool(const std::string&)>;
    using DeviceListCallback = std::function<json()>;
    using DeviceStatusCallback = std::function<json(const std::string&)>;
    using DiagnosticsCallback = std::function<bool(const std::string&, bool)>;
    using DeviceStartCallback = std::function<bool(const std::string&)>;
    using DeviceStopCallback = std::function<bool(const std::string&)>;
    using DevicePauseCallback = std::function<bool(const std::string&)>;
    using DeviceResumeCallback = std::function<bool(const std::string&)>;
    using DeviceRestartCallback = std::function<bool(const std::string&)>;
    using PumpControlCallback = std::function<bool(const std::string&, const std::string&, bool)>;
    using ValveControlCallback = std::function<bool(const std::string&, const std::string&, double)>;
    using SetpointChangeCallback = std::function<bool(const std::string&, const std::string&, double)>;
    using DeviceConfigCallback = std::function<bool(const json&)>;
    using DataPointConfigCallback = std::function<bool(const std::string&, const json&)>;
    using AlarmConfigCallback = std::function<bool(const json&)>;
    using VirtualPointConfigCallback = std::function<bool(const json&)>;
    using UserManagementCallback = std::function<json(const std::string&, const json&)>;
    using SystemBackupCallback = std::function<bool(const std::string&)>;
    using LogDownloadCallback = std::function<std::string(const std::string&, const std::string&)>;

public:
    explicit RestApiServer(int port = 8080);
    ~RestApiServer();

    bool Start();
    void Stop();
    bool IsRunning() const;

    // 콜백 설정 메서드들
    void SetReloadConfigCallback(ReloadConfigCallback callback);
    void SetReinitializeCallback(ReinitializeCallback callback);
    void SetDeviceListCallback(DeviceListCallback callback);
    void SetDeviceStatusCallback(DeviceStatusCallback callback);
    void SetSystemStatsCallback(SystemStatsCallback callback);
    void SetDiagnosticsCallback(DiagnosticsCallback callback);
    void SetDeviceStartCallback(DeviceStartCallback callback);
    void SetDeviceStopCallback(DeviceStopCallback callback);
    void SetDevicePauseCallback(DevicePauseCallback callback);
    void SetDeviceResumeCallback(DeviceResumeCallback callback);
    void SetDeviceRestartCallback(DeviceRestartCallback callback);
    void SetPumpControlCallback(PumpControlCallback callback);
    void SetValveControlCallback(ValveControlCallback callback);
    void SetSetpointChangeCallback(SetpointChangeCallback callback);
    void SetDeviceConfigCallback(DeviceConfigCallback callback);
    void SetDataPointConfigCallback(DataPointConfigCallback callback);
    void SetAlarmConfigCallback(AlarmConfigCallback callback);
    void SetVirtualPointConfigCallback(VirtualPointConfigCallback callback);
    void SetUserManagementCallback(UserManagementCallback callback);
    void SetSystemBackupCallback(SystemBackupCallback callback);
    void SetLogDownloadCallback(LogDownloadCallback callback);

private:
    void SetupRoutes();
    
    // 기존 핸들러들
    void HandleGetDevices(const httplib::Request& req, httplib::Response& res);
    void HandleGetDeviceStatus(const httplib::Request& req, httplib::Response& res);
    void HandlePostReloadConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePostReinitialize(const httplib::Request& req, httplib::Response& res);
    void HandleGetSystemStats(const httplib::Request& req, httplib::Response& res);
    void HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res);
    
    // DeviceWorker 제어 핸들러들
    void HandlePostDeviceStart(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceStop(const httplib::Request& req, httplib::Response& res);
    void HandlePostDevicePause(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceResume(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceRestart(const httplib::Request& req, httplib::Response& res);
    
    // 누락된 핸들러들 추가
    void HandlePostDeviceControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostPointControl(const httplib::Request& req, httplib::Response& res);
    
    // 하드웨어 제어 핸들러들
    void HandlePostPumpControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostValveControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostSetpointChange(const httplib::Request& req, httplib::Response& res);
    
    // 설정 관리 핸들러들
    void HandleGetDeviceConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePutDeviceConfig(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteDeviceConfig(const httplib::Request& req, httplib::Response& res);
    void HandleGetDataPoints(const httplib::Request& req, httplib::Response& res);
    void HandlePostDataPoint(const httplib::Request& req, httplib::Response& res);
    void HandlePutDataPoint(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteDataPoint(const httplib::Request& req, httplib::Response& res);
    void HandleGetAlarmRules(const httplib::Request& req, httplib::Response& res);
    void HandlePostAlarmRule(const httplib::Request& req, httplib::Response& res);
    void HandlePutAlarmRule(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteAlarmRule(const httplib::Request& req, httplib::Response& res);
    void HandleGetVirtualPoints(const httplib::Request& req, httplib::Response& res);
    void HandlePostVirtualPoint(const httplib::Request& req, httplib::Response& res);
    void HandlePutVirtualPoint(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteVirtualPoint(const httplib::Request& req, httplib::Response& res);
    void HandleGetUsers(const httplib::Request& req, httplib::Response& res);
    void HandlePostUser(const httplib::Request& req, httplib::Response& res);
    void HandlePutUser(const httplib::Request& req, httplib::Response& res);
    void HandleDeleteUser(const httplib::Request& req, httplib::Response& res);
    void HandlePostUserPermissions(const httplib::Request& req, httplib::Response& res);
    void HandlePostSystemBackup(const httplib::Request& req, httplib::Response& res);
    void HandleGetSystemLogs(const httplib::Request& req, httplib::Response& res);
    void HandleGetSystemConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePutSystemConfig(const httplib::Request& req, httplib::Response& res);
    
    // 유틸리티 메서드들
    void SetCorsHeaders(httplib::Response& res);
    json CreateErrorResponse(const std::string& error);
    json CreateSuccessResponse(const json& data = json::object());
    json CreateMessageResponse(const std::string& message);       // 추가
    json CreateHealthResponse();                                  // 추가  
    json CreateValveResponse(double position);                    // 추가
    json CreateSetpointResponse(double value);                   // 추가
    bool ValidateJsonSchema(const json& data, const std::string& schema_type);
    std::string ExtractDeviceId(const httplib::Request& req, int match_index = 1);

private:
    int port_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    
    // 콜백들
    ReloadConfigCallback reload_config_callback_;
    ReinitializeCallback reinitialize_callback_;
    DeviceListCallback device_list_callback_;
    DeviceStatusCallback device_status_callback_;
    SystemStatsCallback system_stats_callback_;
    DiagnosticsCallback diagnostics_callback_;
    DeviceStartCallback device_start_callback_;
    DeviceStopCallback device_stop_callback_;
    DevicePauseCallback device_pause_callback_;
    DeviceResumeCallback device_resume_callback_;
    DeviceRestartCallback device_restart_callback_;
    PumpControlCallback pump_control_callback_;
    ValveControlCallback valve_control_callback_;
    SetpointChangeCallback setpoint_change_callback_;
    DeviceConfigCallback device_config_callback_;
    DataPointConfigCallback datapoint_config_callback_;
    AlarmConfigCallback alarm_config_callback_;
    VirtualPointConfigCallback virtualpoint_config_callback_;
    UserManagementCallback user_management_callback_;
    SystemBackupCallback system_backup_callback_;
    LogDownloadCallback log_download_callback_;
};

} // namespace Network
} // namespace PulseOne

#endif // PULSEONE_REST_API_SERVER_H