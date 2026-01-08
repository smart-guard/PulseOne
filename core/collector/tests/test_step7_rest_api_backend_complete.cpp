// =============================================================================
// collector/tests/test_step7_rest_api_backend_complete.cpp
// Step 7: REST API 백엔드 데이터 플로우 검증 테스트 - 수정 완성본
// URL 패턴 수정: /control 및 /set 추가
// =============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <queue>
#include <mutex>

// Core components
#include "Core/Application.h"
#include "Network/RestApiServer.h"
#include "Workers/WorkerManager.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"

// API Callbacks
#include "Api/DeviceApiCallbacks.h"
#include "Api/HardwareApiCallbacks.h"
#include "Api/ConfigApiCallbacks.h"

// Test utilities
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace PulseOne;

// =============================================================================
// 데이터 플로우 검증용 Mock 클래스들
// =============================================================================

/**
 * @brief 명령 추적 및 검증을 위한 Mock WorkerManager
 */
class MockWorkerManagerForDataFlow {
private:
    mutable std::mutex command_mutex_;
    std::queue<json> received_commands_;
    std::map<std::string, bool> worker_states_;
    std::map<std::string, json> last_hardware_commands_;

public:
    // 명령 수신 기록
    void RecordCommand(const std::string& type, const std::string& device_id, const json& data = json::object()) {
        std::lock_guard<std::mutex> lock(command_mutex_);
        json command = {
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"type", type},
            {"device_id", device_id},
            {"data", data}
        };
        received_commands_.push(command);
        LogManager::getInstance().Info("[MOCK] Command recorded: " + type + " for device " + device_id);
    }

    // Worker 제어 Mock 구현
    bool StartWorker(const std::string& device_id) {
        RecordCommand("start_worker", device_id);
        worker_states_[device_id] = true;
        
        // 약간의 지연 시뮬레이션
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    }

    bool StopWorker(const std::string& device_id) {
        RecordCommand("stop_worker", device_id);
        worker_states_[device_id] = false;
        return true;
    }

    bool PauseWorker(const std::string& device_id) {
        RecordCommand("pause_worker", device_id);
        return true;
    }

    bool ResumeWorker(const std::string& device_id) {
        RecordCommand("resume_worker", device_id);
        return true;
    }

    bool RestartWorker(const std::string& device_id) {
        RecordCommand("restart_worker", device_id);
        worker_states_[device_id] = true;
        return true;
    }

    // 하드웨어 제어 Mock 구현
    bool ControlDigitalOutput(const std::string& device_id, const std::string& output_id, bool enable) {
        json hardware_data = {
            {"output_type", "digital"},
            {"output_id", output_id},
            {"enable", enable}
        };
        RecordCommand("digital_control", device_id, hardware_data);
        last_hardware_commands_[device_id + "_" + output_id] = hardware_data;
        return worker_states_[device_id]; // Worker가 실행 중일 때만 성공
    }

    bool ControlAnalogOutput(const std::string& device_id, const std::string& output_id, double value) {
        // 값 검증
        if (value < 0.0 || value > 100.0) {
            LogManager::getInstance().Error("Analog output value out of range (0-100%): " + std::to_string(value));
            return false;
        }
        
        json hardware_data = {
            {"output_type", "analog"},
            {"output_id", output_id},
            {"value", value}
        };
        RecordCommand("analog_control", device_id, hardware_data);
        last_hardware_commands_[device_id + "_" + output_id] = hardware_data;
        return worker_states_[device_id];
    }

    bool ChangeParameter(const std::string& device_id, const std::string& param_id, double value) {
        json param_data = {
            {"parameter_id", param_id},
            {"value", value}
        };
        RecordCommand("parameter_change", device_id, param_data);
        last_hardware_commands_[device_id + "_" + param_id] = param_data;
        return worker_states_[device_id];
    }

    // 상태 조회 Mock
    bool IsWorkerRunning(const std::string& device_id) const {
        auto it = worker_states_.find(device_id);
        return (it != worker_states_.end()) ? it->second : false;
    }

    // 검증 메서드들
    size_t GetReceivedCommandCount() const {
        std::unique_lock<std::mutex> lock(command_mutex_);
        return received_commands_.size();
    }

    std::vector<json> GetAllCommands() const {
        std::unique_lock<std::mutex> lock(command_mutex_);
        std::vector<json> commands;
        auto temp_queue = received_commands_;
        while (!temp_queue.empty()) {
            commands.push_back(temp_queue.front());
            temp_queue.pop();
        }
        return commands;
    }

    json GetLastHardwareCommand(const std::string& device_id, const std::string& output_id) const {
        std::string key = device_id + "_" + output_id;
        auto it = last_hardware_commands_.find(key);
        return (it != last_hardware_commands_.end()) ? it->second : json::object();
    }

    bool VerifyCommandSequence(const std::vector<std::string>& expected_types) const {
        auto commands = GetAllCommands();
        if (commands.size() < expected_types.size()) return false;
        
        for (size_t i = 0; i < expected_types.size(); ++i) {
            if (commands[i]["type"] != expected_types[i]) {
                LogManager::getInstance().Error("Command sequence mismatch at index " + std::to_string(i) + 
                    ": expected '" + expected_types[i] + "', got '" + commands[i]["type"].get<std::string>() + "'");
                return false;
            }
        }
        return true;
    }

    void ClearCommands() {
        std::lock_guard<std::mutex> lock(command_mutex_);
        std::queue<json> empty;
        received_commands_.swap(empty);
        last_hardware_commands_.clear();
    }
};

// =============================================================================
// HTTP 클라이언트 (기존과 동일)
// =============================================================================

class HttpTestClient {
private:
    std::string base_url_;
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

public:
    struct Response {
        long status_code = 0;
        std::string body;
        bool success = false;
        
        json GetJson() const {
            try {
                return json::parse(body);
            } catch (const json::parse_error&) {
                return json::object();
            }
        }
    };

    HttpTestClient(const std::string& base_url) : base_url_(base_url) {}

    Response Get(const std::string& endpoint) {
        Response response;
        CURL* curl = curl_easy_init();
        
        if (!curl) return response;
        
        std::string url = base_url_ + endpoint;
        std::string response_body;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
            response.body = response_body;
            response.success = (response.status_code >= 200 && response.status_code < 300);
        }
        
        curl_easy_cleanup(curl);
        return response;
    }
    
    Response Post(const std::string& endpoint, const json& data = json::object()) {
        Response response;
        CURL* curl = curl_easy_init();
        
        if (!curl) return response;
        
        std::string url = base_url_ + endpoint;
        std::string post_data = data.dump();
        std::string response_body;
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
            response.body = response_body;
            response.success = (response.status_code >= 200 && response.status_code < 300);
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
    }
};

// =============================================================================
// 데이터 플로우 검증 테스트 픽스처
// =============================================================================

class Step7DataFlowTest : public ::testing::Test {
protected:
    std::unique_ptr<Network::RestApiServer> api_server_;
    std::unique_ptr<HttpTestClient> http_client_;
    std::unique_ptr<MockWorkerManagerForDataFlow> mock_worker_mgr_;
    std::atomic<bool> server_ready_{false};
    std::thread server_thread_;
    
    static constexpr int TEST_PORT = 8082;  // 다른 포트 사용

    void SetUp() override {
        // 시스템 초기화
        LogManager::getInstance().Info("=== Step 7: Data Flow Validation Test ===");
        ConfigManager::getInstance().initialize();
        
        DbLib::DatabaseConfig dbConfig;
        dbConfig.type = "SQLITE";
        DbLib::DatabaseManager::getInstance().initialize(dbConfig);
        
        Database::RepositoryFactory::getInstance().initialize();
        
        // Mock WorkerManager 초기화
        mock_worker_mgr_ = std::make_unique<MockWorkerManagerForDataFlow>();
        
        // HTTP 클라이언트 초기화
        http_client_ = std::make_unique<HttpTestClient>("http://localhost:" + std::to_string(TEST_PORT));
        
        // REST API 서버 초기화 및 Mock 콜백 설정
        SetupApiServerWithMockCallbacks();
        
        // 서버 시작
        StartApiServer();
        
        LogManager::getInstance().Info("Data flow test environment setup completed");
    }
    
    void TearDown() override {
        LogManager::getInstance().Info("Tearing down data flow test environment...");
        StopApiServer();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        LogManager::getInstance().Info("Data flow test environment cleanup completed");
    }

private:
    void SetupApiServerWithMockCallbacks() {
        api_server_ = std::make_unique<Network::RestApiServer>(TEST_PORT);
        
        // Mock 콜백들 설정
        api_server_->SetDeviceListCallback([]() -> json {
            json device_list = json::array();
            for (int i = 1; i <= 3; ++i) {
                device_list.push_back({
                    {"device_id", std::to_string(i)},
                    {"name", "Test Device " + std::to_string(i)},
                    {"protocol", "Mock Protocol"},
                    {"enabled", true},
                    {"status", "ready"},
                    {"connection_status", "ready"}
                });
            }
            return device_list;
        });
        
        // Worker 제어 콜백들
        api_server_->SetDeviceStartCallback([this](const std::string& device_id) -> bool {
            return mock_worker_mgr_->StartWorker(device_id);
        });
        
        api_server_->SetDeviceStopCallback([this](const std::string& device_id) -> bool {
            return mock_worker_mgr_->StopWorker(device_id);
        });
        
        api_server_->SetDevicePauseCallback([this](const std::string& device_id) -> bool {
            return mock_worker_mgr_->PauseWorker(device_id);
        });
        
        api_server_->SetDeviceResumeCallback([this](const std::string& device_id) -> bool {
            return mock_worker_mgr_->ResumeWorker(device_id);
        });
        
        api_server_->SetDeviceRestartCallback([this](const std::string& device_id) -> bool {
            return mock_worker_mgr_->RestartWorker(device_id);
        });
        
        // 상태 조회 콜백
        api_server_->SetDeviceStatusCallback([this](const std::string& device_id) -> json {
            return json{
                {"device_id", device_id},
                {"running", mock_worker_mgr_->IsWorkerRunning(device_id)},
                {"connected", mock_worker_mgr_->IsWorkerRunning(device_id)},
                {"uptime", "0d 0h 0m 0s"}
            };
        });
        
        // 하드웨어 제어 콜백들
        api_server_->SetDigitalOutputCallback([this](const std::string& device_id, const std::string& output_id, bool enable) -> bool {
            return mock_worker_mgr_->ControlDigitalOutput(device_id, output_id, enable);
        });
        
        api_server_->SetAnalogOutputCallback([this](const std::string& device_id, const std::string& output_id, double value) -> bool {
            return mock_worker_mgr_->ControlAnalogOutput(device_id, output_id, value);
        });
        
        api_server_->SetParameterChangeCallback([this](const std::string& device_id, const std::string& param_id, double value) -> bool {
            return mock_worker_mgr_->ChangeParameter(device_id, param_id, value);
        });
        
        LogManager::getInstance().Info("API server configured with mock callbacks");
    }
    
    void StartApiServer() {
        LogManager::getInstance().Info("Starting REST API server for data flow test...");
        
        server_thread_ = std::thread([this]() {
            if (api_server_->Start()) {
                server_ready_ = true;
                LogManager::getInstance().Info("Data flow test API server started successfully");
                
                while (api_server_->IsRunning()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });
        
        // 서버 준비 대기
        int wait_count = 0;
        while (!server_ready_ && wait_count < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
    }
    
    void StopApiServer() {
        if (api_server_) {
            api_server_->Stop();
        }
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        server_ready_ = false;
    }

protected:
    bool ValidateJsonResponse(const HttpTestClient::Response& response, 
                             const std::string& expected_field = "") {
        if (!response.success) return false;
        
        json parsed = response.GetJson();
        if (parsed.empty()) return false;
        
        if (!parsed.contains("success") || parsed["success"] != true) return false;
        if (!parsed.contains("timestamp")) return false;
        
        if (!expected_field.empty() && !parsed.contains(expected_field)) return false;
        
        return true;
    }
};

// =============================================================================
// 데이터 플로우 검증 테스트들 - URL 수정됨
// =============================================================================

TEST_F(Step7DataFlowTest, BasicConnectivityDataFlow) {
    LogManager::getInstance().Info("=== Basic Connectivity Data Flow Test ===");
    
    auto health_response = http_client_->Get("/api/health");
    EXPECT_TRUE(ValidateJsonResponse(health_response, "data"));
    LogManager::getInstance().Info("✓ Health check data flow verified");
}

TEST_F(Step7DataFlowTest, WorkerControlDataFlow) {
    LogManager::getInstance().Info("=== Worker Control Data Flow Test ===");
    
    const std::string device_id = "1";
    mock_worker_mgr_->ClearCommands();
    
    // 1. Worker 시작
    auto start_response = http_client_->Post("/api/devices/" + device_id + "/worker/start");
    EXPECT_TRUE(ValidateJsonResponse(start_response, "data"));
    
    // 2. 명령 전달 검증
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_GE(mock_worker_mgr_->GetReceivedCommandCount(), 1);
    
    // 3. 상태 확인
    auto status_response = http_client_->Get("/api/devices/" + device_id + "/status");
    EXPECT_TRUE(ValidateJsonResponse(status_response, "data"));
    
    if (status_response.success) {
        json status_data = status_response.GetJson()["data"];
        EXPECT_EQ(status_data["running"], true);
    }
    
    // 4. Worker 중지
    auto stop_response = http_client_->Post("/api/devices/" + device_id + "/worker/stop");
    EXPECT_TRUE(ValidateJsonResponse(stop_response, "data"));
    
    // 5. 명령 시퀀스 검증
    std::vector<std::string> expected_sequence = {"start_worker", "stop_worker"};
    EXPECT_TRUE(mock_worker_mgr_->VerifyCommandSequence(expected_sequence));
    
    LogManager::getInstance().Info("✓ Worker control data flow verified");
}

TEST_F(Step7DataFlowTest, HardwareControlDataFlow) {
    LogManager::getInstance().Info("=== Hardware Control Data Flow Test ===");
    
    const std::string device_id = "1";
    mock_worker_mgr_->ClearCommands();
    
    // 먼저 Worker 시작
    auto start_response = http_client_->Post("/api/devices/" + device_id + "/worker/start");
    EXPECT_TRUE(ValidateJsonResponse(start_response, "data"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 1. 디지털 출력 제어 - URL 수정: /control 추가
    json digital_data = {{"enable", true}};
    auto digital_response = http_client_->Post("/api/devices/" + device_id + "/digital/pump01/control", digital_data);
    EXPECT_TRUE(ValidateJsonResponse(digital_response, "data"));
    
    // 2. 아날로그 출력 제어 - URL 수정: /control 추가
    json analog_data = {{"value", 75.5}};
    auto analog_response = http_client_->Post("/api/devices/" + device_id + "/analog/valve01/control", analog_data);
    EXPECT_TRUE(ValidateJsonResponse(analog_response, "data"));
    
    // 3. 파라미터 변경 - URL 수정: /set 추가
    json param_data = {{"value", 25.0}};
    auto param_response = http_client_->Post("/api/devices/" + device_id + "/parameters/temp_setpoint/set", param_data);
    EXPECT_TRUE(ValidateJsonResponse(param_response, "data"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 4. 전달된 데이터 검증
    auto digital_command = mock_worker_mgr_->GetLastHardwareCommand(device_id, "pump01");
    EXPECT_EQ(digital_command["output_type"], "digital");
    EXPECT_EQ(digital_command["enable"], true);
    
    auto analog_command = mock_worker_mgr_->GetLastHardwareCommand(device_id, "valve01");
    EXPECT_EQ(analog_command["output_type"], "analog");
    EXPECT_EQ(analog_command["value"], 75.5);
    
    auto param_command = mock_worker_mgr_->GetLastHardwareCommand(device_id, "temp_setpoint");
    EXPECT_EQ(param_command["value"], 25.0);
    
    LogManager::getInstance().Info("✓ Hardware control data flow verified");
}

TEST_F(Step7DataFlowTest, InvalidDataHandling) {
    LogManager::getInstance().Info("=== Invalid Data Handling Test ===");
    
    const std::string device_id = "1";
    
    // Worker 시작
    auto start_response = http_client_->Post("/api/devices/" + device_id + "/worker/start");
    EXPECT_TRUE(ValidateJsonResponse(start_response, "data"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 1. 범위 초과 아날로그 값 테스트 - URL 수정: /control 추가
    json invalid_analog = {{"value", 150.0}};  // 100% 초과
    auto invalid_response = http_client_->Post("/api/devices/" + device_id + "/analog/valve01/control", invalid_analog);
    EXPECT_FALSE(invalid_response.success);  // 실패해야 정상
    
    // 2. Worker가 중지된 상태에서 하드웨어 제어 시도
    auto stop_response = http_client_->Post("/api/devices/" + device_id + "/worker/stop");
    EXPECT_TRUE(ValidateJsonResponse(stop_response, "data"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // URL 수정: /control 추가
    json control_data = {{"enable", true}};
    auto control_response = http_client_->Post("/api/devices/" + device_id + "/digital/pump01/control", control_data);
    EXPECT_FALSE(control_response.success);  // Worker가 중지되어 실패해야 정상
    
    LogManager::getInstance().Info("✓ Invalid data handling verified");
}

TEST_F(Step7DataFlowTest, CommandSequenceValidation) {
    LogManager::getInstance().Info("=== Command Sequence Validation Test ===");
    
    const std::string device_id = "1";
    mock_worker_mgr_->ClearCommands();
    
    // 복잡한 명령 시퀀스 실행
    auto start_cmd = http_client_->Post("/api/devices/" + device_id + "/worker/start");
    EXPECT_TRUE(ValidateJsonResponse(start_cmd, "data"));
    
    auto pause_cmd = http_client_->Post("/api/devices/" + device_id + "/worker/pause");
    EXPECT_TRUE(ValidateJsonResponse(pause_cmd, "data"));
    
    auto resume_cmd = http_client_->Post("/api/devices/" + device_id + "/worker/resume");
    EXPECT_TRUE(ValidateJsonResponse(resume_cmd, "data"));
    
    auto restart_cmd = http_client_->Post("/api/devices/" + device_id + "/worker/restart");
    EXPECT_TRUE(ValidateJsonResponse(restart_cmd, "data"));
    
    auto stop_cmd = http_client_->Post("/api/devices/" + device_id + "/worker/stop");
    EXPECT_TRUE(ValidateJsonResponse(stop_cmd, "data"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 시퀀스 검증
    std::vector<std::string> expected_sequence = {
        "start_worker", "pause_worker", "resume_worker", "restart_worker", "stop_worker"
    };
    EXPECT_TRUE(mock_worker_mgr_->VerifyCommandSequence(expected_sequence));
    
    LogManager::getInstance().Info("✓ Command sequence validation verified");
}

TEST_F(Step7DataFlowTest, EndToEndDataFlow) {
    LogManager::getInstance().Info("=== End-to-End Data Flow Test ===");
    
    const std::string device_id = "1";
    mock_worker_mgr_->ClearCommands();
    
    // 1. 디바이스 목록 조회
    auto devices_response = http_client_->Get("/api/devices");
    EXPECT_TRUE(ValidateJsonResponse(devices_response, "data"));
    
    // 2. Worker 시작
    auto start_response = http_client_->Post("/api/devices/" + device_id + "/worker/start");
    EXPECT_TRUE(ValidateJsonResponse(start_response, "data"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 3. 하드웨어 제어 시퀀스 - 모든 URL 수정됨
    json pump_on = {{"enable", true}};
    auto pump_on_resp = http_client_->Post("/api/devices/" + device_id + "/digital/pump01/control", pump_on);
    EXPECT_TRUE(ValidateJsonResponse(pump_on_resp, "data"));
    
    json valve_control = {{"value", 50.0}};
    auto valve_resp = http_client_->Post("/api/devices/" + device_id + "/analog/valve01/control", valve_control);
    EXPECT_TRUE(ValidateJsonResponse(valve_resp, "data"));
    
    json temp_setting = {{"value", 22.5}};
    auto temp_resp = http_client_->Post("/api/devices/" + device_id + "/parameters/temp_setpoint/set", temp_setting);
    EXPECT_TRUE(ValidateJsonResponse(temp_resp, "data"));
    
    json pump_off = {{"enable", false}};
    auto pump_off_resp = http_client_->Post("/api/devices/" + device_id + "/digital/pump01/control", pump_off);
    EXPECT_TRUE(ValidateJsonResponse(pump_off_resp, "data"));
    
    // 4. Worker 중지
    auto stop_response = http_client_->Post("/api/devices/" + device_id + "/worker/stop");
    EXPECT_TRUE(ValidateJsonResponse(stop_response, "data"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 5. 전체 플로우 데이터 검증
    EXPECT_GE(mock_worker_mgr_->GetReceivedCommandCount(), 6); // start + 4 hardware + stop
    
    auto final_commands = mock_worker_mgr_->GetAllCommands();
    LogManager::getInstance().Info("Total commands processed: " + std::to_string(final_commands.size()));
    
    // 6. 핵심 데이터 도착 검증
    auto pump_command = mock_worker_mgr_->GetLastHardwareCommand(device_id, "pump01");
    EXPECT_EQ(pump_command["enable"], false); // 마지막 펌프 명령은 OFF
    
    auto valve_command = mock_worker_mgr_->GetLastHardwareCommand(device_id, "valve01");
    EXPECT_EQ(valve_command["value"], 50.0);
    
    auto temp_command = mock_worker_mgr_->GetLastHardwareCommand(device_id, "temp_setpoint");
    EXPECT_EQ(temp_command["value"], 22.5);
    
    LogManager::getInstance().Info("✓ End-to-end data flow completely verified!");
}

// =============================================================================
// 메인 함수
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "=== PulseOne Step 7: REST API Data Flow Validation Test ===" << std::endl;
    std::cout << "Testing signal transmission and data arrival verification" << std::endl;
    std::cout << "No actual hardware connection required" << std::endl;
    std::cout << std::endl;
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    LogManager::getInstance().Info("Starting Step 7 data flow validation tests...");
    
    int result = RUN_ALL_TESTS();
    
    LogManager::getInstance().Info("Step 7 data flow validation tests completed");
    curl_global_cleanup();
    
    if (result == 0) {
        std::cout << std::endl;
        std::cout << "🎉 All Step 7 Data Flow Tests PASSED!" << std::endl;
        std::cout << "✅ HTTP → REST API → WorkerManager signal transmission verified" << std::endl;
        std::cout << "✅ Worker control command data flow verified" << std::endl;
        std::cout << "✅ Hardware control command data flow verified" << std::endl;
        std::cout << "✅ Invalid data handling verification passed" << std::endl;
        std::cout << "✅ Command sequence validation passed" << std::endl;
        std::cout << "✅ End-to-end data arrival verification completed" << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "❌ Some Step 7 data flow tests FAILED. Check logs for details." << std::endl;
    }
    
    return result;
}