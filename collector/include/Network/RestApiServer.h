// =============================================================================
// collector/include/Network/RestApiServer.h
// REST API 서버 헤더 - 기존 콜백 유지 + 디바이스 그룹 기능 추가
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

// nlohmann/json 직접 사용 (기존 프로젝트 패턴 따름)
#include <nlohmann/json.hpp>

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
    // 기본 콜백 함수 타입들
    using ReloadConfigCallback = std::function<bool()>;
    using ReinitializeCallback = std::function<bool()>;
    using SystemStatsCallback = std::function<nlohmann::json()>;
    using DeviceControlCallback = std::function<bool(const std::string&)>;
    using DeviceListCallback = std::function<nlohmann::json()>;
    using DeviceStatusCallback = std::function<nlohmann::json(const std::string&)>;
    using DiagnosticsCallback = std::function<bool(const std::string&, bool)>;
    
    // DeviceWorker 제어 콜백들
    using DeviceStartCallback = std::function<bool(const std::string&)>;
    using DeviceStopCallback = std::function<bool(const std::string&)>;
    using DevicePauseCallback = std::function<bool(const std::string&)>;
    using DeviceResumeCallback = std::function<bool(const std::string&)>;
    using DeviceRestartCallback = std::function<bool(const std::string&)>;
    
    // 기존 하드웨어 제어 콜백들 (유지)
    using PumpControlCallback = std::function<bool(const std::string&, const std::string&, bool)>;
    using ValveControlCallback = std::function<bool(const std::string&, const std::string&, bool)>;
    using SetpointChangeCallback = std::function<bool(const std::string&, const std::string&, double)>;
    
    // 새로운 범용 하드웨어 제어 콜백들
    using DigitalOutputCallback = std::function<bool(const std::string&, const std::string&, bool)>;
    using AnalogOutputCallback = std::function<bool(const std::string&, const std::string&, double)>;
    using ParameterChangeCallback = std::function<bool(const std::string&, const std::string&, double)>;
    
    // 디바이스 그룹 제어 콜백들 (새로 추가)
    using DeviceGroupListCallback = std::function<nlohmann::json()>;
    using DeviceGroupStatusCallback = std::function<nlohmann::json(const std::string&)>;
    using DeviceGroupControlCallback = std::function<bool(const std::string&, const std::string&)>;
    
    // 설정 관리 콜백들
    using DeviceConfigCallback = std::function<bool(const nlohmann::json&)>;
    using DataPointConfigCallback = std::function<bool(const std::string&, const nlohmann::json&)>;
    using AlarmConfigCallback = std::function<bool(const nlohmann::json&)>;
    using VirtualPointConfigCallback = std::function<bool(const nlohmann::json&)>;
    using UserManagementCallback = std::function<nlohmann::json(const std::string&, const nlohmann::json&)>;
    using SystemBackupCallback = std::function<bool(const std::string&)>;
    using LogDownloadCallback = std::function<std::string(const std::string&, const std::string&)>;

public:
    explicit RestApiServer(int port = 8080);
    ~RestApiServer();

    bool Start();
    void Stop();
    bool IsRunning() const;

    // 기본 콜백 설정 메서드들
    void SetReloadConfigCallback(ReloadConfigCallback callback);
    void SetReinitializeCallback(ReinitializeCallback callback);
    void SetDeviceListCallback(DeviceListCallback callback);
    void SetDeviceStatusCallback(DeviceStatusCallback callback);
    void SetSystemStatsCallback(SystemStatsCallback callback);
    void SetDiagnosticsCallback(DiagnosticsCallback callback);
    
    // DeviceWorker 제어 콜백 설정
    void SetDeviceStartCallback(DeviceStartCallback callback);
    void SetDeviceStopCallback(DeviceStopCallback callback);
    void SetDevicePauseCallback(DevicePauseCallback callback);
    void SetDeviceResumeCallback(DeviceResumeCallback callback);
    void SetDeviceRestartCallback(DeviceRestartCallback callback);
    
    // 기존 하드웨어 제어 콜백 설정 (유지)
    void SetPumpControlCallback(PumpControlCallback callback);
    void SetValveControlCallback(ValveControlCallback callback);
    void SetSetpointChangeCallback(SetpointChangeCallback callback);
    
    // 새로운 범용 하드웨어 제어 콜백 설정
    void SetDigitalOutputCallback(DigitalOutputCallback callback);
    void SetAnalogOutputCallback(AnalogOutputCallback callback);
    void SetParameterChangeCallback(ParameterChangeCallback callback);
    
    // 디바이스 그룹 콜백 설정 (새로 추가)
    void SetDeviceGroupListCallback(DeviceGroupListCallback callback);
    void SetDeviceGroupStatusCallback(DeviceGroupStatusCallback callback);
    void SetDeviceGroupControlCallback(DeviceGroupControlCallback callback);
    
    // 설정 관리 콜백 설정
    void SetDeviceConfigCallback(DeviceConfigCallback callback);
    void SetDataPointConfigCallback(DataPointConfigCallback callback);
    void SetAlarmConfigCallback(AlarmConfigCallback callback);
    void SetVirtualPointConfigCallback(VirtualPointConfigCallback callback);
    void SetUserManagementCallback(UserManagementCallback callback);
    void SetSystemBackupCallback(SystemBackupCallback callback);
    void SetLogDownloadCallback(LogDownloadCallback callback);

private:
    void SetupRoutes();
    
    // 기본 핸들러들
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
    
    // 일반 제어 핸들러들
    void HandlePostDeviceControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostPointControl(const httplib::Request& req, httplib::Response& res);
    
    // 기존 하드웨어 제어 핸들러들 (유지)
    void HandlePostPumpControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostValveControl(const httplib::Request& req, httplib::Response& res);
    void HandlePostSetpointChange(const httplib::Request& req, httplib::Response& res);
    
    // 새로운 범용 하드웨어 제어 핸들러들
    void HandlePostDigitalOutput(const httplib::Request& req, httplib::Response& res);
    void HandlePostAnalogOutput(const httplib::Request& req, httplib::Response& res);
    void HandlePostParameterChange(const httplib::Request& req, httplib::Response& res);
    
    // 디바이스 그룹 핸들러들 (새로 추가)
    void HandleGetDeviceGroups(const httplib::Request& req, httplib::Response& res);
    void HandleGetDeviceGroupStatus(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceGroupStart(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceGroupStop(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceGroupPause(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceGroupResume(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceGroupRestart(const httplib::Request& req, httplib::Response& res);
    
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
    nlohmann::json CreateErrorResponse(const std::string& error);
    nlohmann::json CreateSuccessResponse(const nlohmann::json& data = nlohmann::json::object());
    nlohmann::json CreateMessageResponse(const std::string& message);       
    nlohmann::json CreateHealthResponse();                                  
    nlohmann::json CreateOutputResponse(double value, const std::string& type);
    nlohmann::json CreateGroupActionResponse(const std::string& group_id, const std::string& action, bool success); // 새로 추가
    bool ValidateJsonSchema(const nlohmann::json& data, const std::string& schema_type);
    std::string ExtractDeviceId(const httplib::Request& req, int match_index = 1);
    std::string ExtractGroupId(const httplib::Request& req, int match_index = 1); // 새로 추가

private:
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
    
    // DeviceWorker 제어 콜백들
    DeviceStartCallback device_start_callback_;
    DeviceStopCallback device_stop_callback_;
    DevicePauseCallback device_pause_callback_;
    DeviceResumeCallback device_resume_callback_;
    DeviceRestartCallback device_restart_callback_;
    
    // 기존 하드웨어 제어 콜백들 (유지)
    PumpControlCallback pump_control_callback_;
    ValveControlCallback valve_control_callback_;
    SetpointChangeCallback setpoint_change_callback_;
    
    // 새로운 범용 하드웨어 제어 콜백들
    DigitalOutputCallback digital_output_callback_;
    AnalogOutputCallback analog_output_callback_;
    ParameterChangeCallback parameter_change_callback_;
    
    // 디바이스 그룹 콜백들 (새로 추가)
    DeviceGroupListCallback device_group_list_callback_;
    DeviceGroupStatusCallback device_group_status_callback_;
    DeviceGroupControlCallback device_group_control_callback_;
    
    // 설정 관리 콜백들
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