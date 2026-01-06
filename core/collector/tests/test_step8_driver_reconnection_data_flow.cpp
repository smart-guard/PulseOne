/**
 * @file test_step8_driver_reconnection_data_flow.cpp
 * @brief Step 8: 드라이버 연결/재연결 및 데이터 파이프라인 E2E 검증 테스트
 * 
 * 이 테스트는 다음을 검증합니다:
 * 1. 가상 Modbus TCP 서버와의 초기 연결 및 데이터 수집
 * 2. 수집된 데이터가 Pipeline을 거쳐 Redis에 규격대로 저장되는지 확인
 * 3. 서버 연결 유실 시 워커의 RECONNECTING 상태 전이 및 재시도 로직
 * 4. 서버 복구 시 자동으로 재연결되어 데이터 수집이 재개되는지 확인
 */

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <nlohmann/json.hpp>

// PulseOne Core
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/WorkerManager.h"
#include "Workers/Protocol/ModbusWorker.h"
#include "Pipeline/PipelineManager.h"
#include "Pipeline/DataProcessingService.h"
#include "Storage/RedisDataWriter.h"
#include "Client/RedisClientImpl.h"

using namespace PulseOne;
using namespace PulseOne::Workers;
using namespace PulseOne::Pipeline;
using namespace PulseOne::Database;
using namespace PulseOne::Storage;
using json = nlohmann::json;

// =============================================================================
// 간단한 가상 Modbus TCP 서버 (테스트용 하드웨어 시뮬레이터)
// =============================================================================
class VirtualModbusServer {
public:
    VirtualModbusServer(int port = 1502) : port_(port), running_(false), server_fd_(-1) {}
    ~VirtualModbusServer() { Stop(); }

    void Start() {
        if (running_) return;
        running_ = true;
        server_thread_ = std::thread(&VirtualModbusServer::Run, this);
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); 
    }

    void Stop() {
        if (!running_) return;
        running_ = false;
        
        if (server_fd_ != -1) {
            // shutdown()으로 accept()를 확실히 깨움
            shutdown(server_fd_, SHUT_RDWR);
            close(server_fd_);
            server_fd_ = -1;
        }
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        // 모든 클라이언트 스레드 종료 대기
        std::lock_guard<std::mutex> lock(client_threads_mutex_);
        for (auto& t : client_threads_) {
            if (t.joinable()) t.join();
        }
        client_threads_.clear();
    }

    void SetRegisterValue(uint16_t address, uint16_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        registers_[address] = value;
    }

    int GetConnectionCount() const { return connection_count_.load(); }

private:
    void Run() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) return;

        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(server_fd_);
            server_fd_ = -1;
            return;
        }

        if (listen(server_fd_, 10) < 0) {
            close(server_fd_);
            server_fd_ = -1;
            return;
        }

        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addrlen);
            
            if (client_fd < 0) {
                if (running_) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 비동기로 클라이언트 처리
            std::lock_guard<std::mutex> lock(client_threads_mutex_);
            client_threads_.push_back(std::thread(&VirtualModbusServer::HandleClient, this, client_fd));
            
            // 만료된 스레드 정리
            auto it = client_threads_.begin();
            while (it != client_threads_.end()) {
                if (it->get_id() == std::thread::id()) { // (실제로는 이렇게 체크하면 안됨, 여기선 간단히)
                    // it = client_threads_.erase(it);
                } else {
                    // it++;
                }
                break; // 여기선 생략
            }
        }
    }

    void HandleClient(int client_fd) {
        connection_count_++;
        uint8_t buffer[1024];
        
        // 소켓 타임아웃 설정 (종료 감지용)
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 500000; // 0.5s
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        while (running_) {
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                break;
            }

            if (bytes_read < 12) continue;

            uint8_t func_code = buffer[7];
            uint16_t start_addr = (buffer[8] << 8) | buffer[9];
            uint16_t quantity = (buffer[10] << 8) | buffer[11];

            if (func_code == 0x03) { // Read Holding Registers
                uint8_t response[256];
                memcpy(response, buffer, 4); // Transaction & Protocol ID
                
                uint16_t data_len = 3 + (quantity * 2);
                response[4] = (data_len >> 8) & 0xFF;
                response[5] = data_len & 0xFF;
                response[6] = buffer[6]; // Unit ID
                response[7] = 0x03;      // Func Code
                response[8] = quantity * 2; // Byte Count

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (int i = 0; i < quantity; ++i) {
                        uint16_t val = registers_[start_addr + i];
                        response[9 + i * 2] = (val >> 8) & 0xFF;
                        response[10 + i * 2] = val & 0xFF;
                    }
                }
                send(client_fd, response, 6 + data_len, 0);
            } else if (func_code == 0x06) { // Write Single Register
                uint16_t value = (buffer[10] << 8) | buffer[11];
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    registers_[start_addr] = value;
                }
                send(client_fd, buffer, bytes_read, 0);
            } else if (func_code == 0x10) { // Write Multiple Registers
                uint16_t quantity_write = (buffer[10] << 8) | buffer[11];
                // uint8_t byte_count = buffer[12]; // Not used but present
                
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (int i = 0; i < quantity_write; ++i) {
                        uint16_t val = (buffer[13 + i * 2] << 8) | buffer[14 + i * 2];
                        registers_[start_addr + i] = val;
                    }
                }
                
                uint8_t response[12];
                memcpy(response, buffer, 12);
                response[4] = 0;
                response[5] = 6; 
                send(client_fd, response, 12, 0);
            }
        }
        close(client_fd);
    }

    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::vector<std::thread> client_threads_;
    std::mutex client_threads_mutex_;
    int server_fd_;
    std::map<uint16_t, uint16_t> registers_;
    std::mutex mutex_;
    std::atomic<int> connection_count_{0};
};

// =============================================================================
// Step 8 통합 테스트 클래스
// =============================================================================
class DriverReconnectionDataFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n🚀 === Step 8: 드라이버 연결 및 데이터 파이프라인 테스트 시작 ===" << std::endl;
        
        // 1. 하드웨어 시뮬레이터 시작 (Port 1502)
        virtual_server_ = std::make_unique<VirtualModbusServer>(1502);
        virtual_server_->Start();
        virtual_server_->SetRegisterValue(0, 1234); // 테스트 데이터 설정 (Address 40001 -> Modbus offset 0)

        // 2. 시스템 초기화
        config_manager_ = &ConfigManager::getInstance();
        config_manager_->initialize(); // 핵심: 설정을 먼저 로드해야 함

        db_manager_ = &DatabaseManager::getInstance();
        db_manager_->initialize(); // 설정을 기반으로 DB 초기화

        // DB 초기화 (스키마 적용)
        std::string schema_path = "db/test_schema_complete.sql";
        std::ifstream sql_file(schema_path);
        if (!sql_file.is_open()) {
            // 커런트 디렉토리에 따라 경로가 다를 수 있음
            schema_path = "core/collector/tests/db/test_schema_complete.sql";
            sql_file.open(schema_path);
        }

        if (sql_file.is_open()) {
            std::stringstream buffer;
            buffer << sql_file.rdbuf();
            db_manager_->executeNonQuery(buffer.str());
            std::cout << "✅ Database schema initialized from " << schema_path << std::endl;
        } else {
            std::cout << "⚠️ Failed to open schema file: " << schema_path << std::endl;
        }

        // 테스트용 디바이스 및 포인트 강제 삽입
        db_manager_->executeNonQuery(
            "INSERT OR REPLACE INTO devices (id, name, protocol_id, endpoint, polling_interval, timeout, retry_count, is_enabled) "
            "VALUES (1, 'TestModbusDevice', 1, '127.0.0.1:1502', 1000, 1000, 3, 1);"
        );
        db_manager_->executeNonQuery(
            "INSERT OR REPLACE INTO data_points (id, device_id, name, address, data_type, is_enabled) "
            "VALUES (101, 1, 'Temperature', 1, 'INT16', 1);"
        );

        // 3. 인프라 서비스 시작
        PipelineManager::GetInstance().Start();
        data_processing_service_ = std::make_unique<DataProcessingService>();
        data_processing_service_->Start();

        // 4. Redis 클라이언트 (설정에서 호스트/포트 가져오기)
        std::string redis_host = GetConfig("REDIS_PRIMARY_HOST", "pulseone-redis");
        int redis_port = GetConfigInt("REDIS_PRIMARY_PORT", 6379);
        
        redis_client_ = std::make_shared<RedisClientImpl>();
        std::cout << "Connecting to Redis at " << redis_host << ":" << redis_port << "..." << std::endl;
        fflush(stdout);
        
        if (redis_client_->connect(redis_host, redis_port)) {
            std::cout << "✅ Redis connection successful" << std::endl;
            redis_client_->del("device:1:Temperature");
            redis_client_->del("point:101:latest");
        } else {
            std::cout << "❌ Redis connection failed!" << std::endl;
        }
        fflush(stdout);
    }

    void TearDown() override {
        if (data_processing_service_) data_processing_service_->Stop();
        PipelineManager::GetInstance().Shutdown();
        if (virtual_server_) virtual_server_->Stop();
        std::cout << "🏁 === 테스트 종료 ===" << std::endl;
    }

    std::unique_ptr<VirtualModbusServer> virtual_server_;
    ConfigManager* config_manager_;
    DatabaseManager* db_manager_;
    std::unique_ptr<DataProcessingService> data_processing_service_;
    std::shared_ptr<RedisClientImpl> redis_client_;
};

// -----------------------------------------------------------------------------
// 테스트 케이스 1: 정상 연결 및 데이터 파이프라인 검증
// -----------------------------------------------------------------------------
TEST_F(DriverReconnectionDataFlowTest, Normal_Operation_And_Redis_Pipeline) {
    // 1. 워커 생성 및 시작
    PulseOne::DeviceInfo dev_info;
    dev_info.id = "1";
    dev_info.name = "TestModbusDevice";
    dev_info.endpoint = "127.0.0.1:1502";
    dev_info.polling_interval_ms = 1000;
    dev_info.timeout_ms = 1000;
    dev_info.retry_count = 3;
    dev_info.retry_interval_ms = 1000;
    dev_info.is_enabled = true;

    PulseOne::DataPoint dp;
    dp.id = "101";
    dp.device_id = "1";
    dp.name = "Temperature";
    dp.address = 40001; // 40001 -> Register address 0 (Holding Register)
    dp.data_type = "INT16";
    auto worker = std::make_shared<ModbusWorker>(dev_info);
    worker->AddDataPoint(dp);

    // 가상 서버에 초기값 설정
    virtual_server_->SetRegisterValue(0, 1234);

    std::cout << "Starting ModbusTcpWorker..." << std::endl;
    auto start_future = worker->Start();
    ASSERT_TRUE(start_future.get());

    // 2. 가상 서버 연결 확인
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_GT(virtual_server_->GetConnectionCount(), 0);
    EXPECT_EQ(worker->GetState(), WorkerState::RUNNING);

    // 3. Redis 데이터 검증 (Worker -> Pipeline -> DataProcessing -> Redis)
    std::cout << "Verifying data in Redis (point:101:latest and device:1:Temperature)..." << std::endl;
    fflush(stdout);
    
    bool found = false;
    for (int i = 0; i < 15; ++i) { // 15초 대기
        std::string latest_val = redis_client_->get("point:101:latest");
        std::string dev_val = redis_client_->get("device:1:Temperature");
        
        if (!latest_val.empty() || !dev_val.empty()) {
            if (!latest_val.empty()) std::cout << "Found Redis data (latest): " << latest_val << std::endl;
            if (!dev_val.empty()) std::cout << "Found Redis data (device): " << dev_val << std::endl;
            fflush(stdout);
            
            if (!latest_val.empty()) {
                auto j = json::parse(latest_val);
                // point:101:latest 에는 "val" 필드도 있음
                EXPECT_EQ(j["point_id"], 101);
                found = true;
            }
            if (!dev_val.empty()) {
                auto j = json::parse(dev_val);
                EXPECT_EQ(j["point_id"], 101);
                EXPECT_EQ(j["point_name"], "Temperature");
                found = true;
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Waiting for data in Redis... (" << (i+1) << "/15)" << std::endl;
        fflush(stdout);
    }
    EXPECT_TRUE(found) << "Data did not reach Redis through the pipeline!";

    worker->Stop().get();
}

// -----------------------------------------------------------------------------
// 테스트 케이스 2: 연결 유실 및 자동 재연결 검증
// -----------------------------------------------------------------------------
TEST_F(DriverReconnectionDataFlowTest, Reconnection_Lifecycle_Validation) {
    // 1. 워커 시작
    PulseOne::DeviceInfo dev_info;
    dev_info.id = "1";
    dev_info.name = "TestModbusDevice";
    dev_info.endpoint = "127.0.0.1:1502";
    dev_info.polling_interval_ms = 500;
    dev_info.timeout_ms = 500;
    dev_info.retry_count = 10;
    dev_info.retry_interval_ms = 1000; // 1초마다 재시도
    PulseOne::DataPoint dp;
    dp.id = "101";
    dp.device_id = "1";
    dp.name = "Temperature";
    dp.name = "Temperature";
    dp.address = 40001; // 40001 -> Register address 0
    dp.data_type = "INT16";
    dp.data_type = "INT16";
    dp.is_enabled = true;

    auto worker = std::make_shared<ModbusWorker>(dev_info);
    worker->AddDataPoint(dp);
    worker->Start().get();
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(worker->GetState(), WorkerState::RUNNING);
    ASSERT_GT(virtual_server_->GetConnectionCount(), 0);

    // 2. 가상 서버 중지 (연결 유실 유도)
    std::cout << "⚠️ Stopping Virtual Server (Simulating connection loss)..." << std::endl;
    virtual_server_->Stop();

    // 3. 워커 상태 전이 확인 (RUNNING -> RECONNECTING)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Worker State: " << static_cast<int>(worker->GetState()) << std::endl;
    EXPECT_EQ(worker->GetState(), WorkerState::RECONNECTING);

    // 4. 가상 서버 재시작 (복구)
    std::cout << "✅ Restarting Virtual Server (Simulating recovery)..." << std::endl;
    virtual_server_ = std::make_unique<VirtualModbusServer>(1502);
    virtual_server_->Start();

    // 5. 자동 복구 확인 (RECONNECTING -> RUNNING)
    std::cout << "Waiting for auto-reconnection..." << std::endl;
    bool recovered = false;
    for (int i = 0; i < 10; ++i) {
        if (worker->GetState() == WorkerState::RUNNING && virtual_server_->GetConnectionCount() > 0) {
            recovered = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_TRUE(recovered) << "Worker failed to recover connection automatically!";
    
    // 6. 복구 후 데이터 취득 확인
    std::cout << "🚀 Verifying data acquisition after recovery..." << std::endl;
    virtual_server_->SetRegisterValue(0, 4321); // 새 값 설정 (취득 확인용)
    
    bool found_after_recovery = false;
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string val = redis_client_->get("point:101:latest");
        if (!val.empty() && val.find("4321") != std::string::npos) {
            std::cout << "✅ Found updated data in Redis after recovery: " << val << std::endl;
            found_after_recovery = true;
            break;
        }
        std::cout << "Waiting for data acquisition... (" << (i+1) << "/10)" << std::endl;
    }
    EXPECT_TRUE(found_after_recovery) << "Data acquisition did not resume after reconnection!";

    worker->Stop().get();
}

// -----------------------------------------------------------------------------
// 테스트 케이스 3: 복수 디바이스, 복수 포인트 및 제어(Write) 테스트
// -----------------------------------------------------------------------------
TEST_F(DriverReconnectionDataFlowTest, Complex_MultiDevice_Control_Validation) {
    // 1. 추가 가상 서버 시작 (Port 1503)
    auto virtual_server2 = std::make_unique<VirtualModbusServer>(1503);
    virtual_server2->Start();
    
    // DB에 두 번째 디바이스 및 포인트 추가
    db_manager_->executeNonQuery(
        "INSERT OR REPLACE INTO devices (id, name, protocol_id, endpoint, polling_interval, timeout, retry_count, is_enabled) "
        "VALUES (2, 'SecondModbusDevice', 1, '127.0.0.1:1503', 500, 500, 3, 1);"
    );
    db_manager_->executeNonQuery(
        "INSERT OR REPLACE INTO data_points (id, device_id, name, address, data_type, is_enabled) "
        "VALUES (201, 2, 'Pressure', 40001, 'INT16', 1), (202, 2, 'Flow', 40002, 'INT16', 1);"
    );

    // 2. 워커 1 설정 (기존 디바이스 1)
    PulseOne::DeviceInfo dev_info1;
    dev_info1.id = "1";
    dev_info1.name = "TestModbusDevice";
    dev_info1.endpoint = "127.0.0.1:1502";
    dev_info1.polling_interval_ms = 500;
    dev_info1.is_enabled = true;
    
    auto worker1 = std::make_shared<ModbusWorker>(dev_info1);
    DataPoint dp101; dp101.id = "101"; dp101.device_id = "1"; dp101.address = 40001; dp101.data_type = "INT16";
    worker1->AddDataPoint(dp101);

    // 3. 워커 2 설정 (디바이스 2)
    PulseOne::DeviceInfo dev_info2;
    dev_info2.id = "2";
    dev_info2.name = "SecondModbusDevice";
    dev_info2.endpoint = "127.0.0.1:1503";
    dev_info2.polling_interval_ms = 500;
    dev_info2.is_enabled = true;

    auto worker2 = std::make_shared<ModbusWorker>(dev_info2);
    DataPoint dp201; dp201.id = "201"; dp201.device_id = "2"; dp201.address = 40001; dp201.data_type = "INT16";
    DataPoint dp202; dp202.id = "202"; dp202.device_id = "2"; dp202.address = 40002; dp202.data_type = "INT16";
    DataPoint dp203; dp203.id = "203"; dp203.device_id = "2"; dp203.address = 40010; dp203.data_type = "FLOAT32";
    worker2->AddDataPoint(dp201);
    worker2->AddDataPoint(dp202);
    worker2->AddDataPoint(dp203);

    // 4. 두 워커 시작
    std::cout << "🚀 Starting concurrent workers..." << std::endl;
    worker1->Start().get();
    worker2->Start().get();

    // 초기 데이터 설정 (ParseModbusAddress가 40001->0, 40002->1로 변환하므로 0-based 주소 사용)
    virtual_server_->SetRegisterValue(0, 100);
    virtual_server2->SetRegisterValue(0, 200);
    virtual_server2->SetRegisterValue(1, 300);

    // Redis 데이터 확인 (동시 수집 확인)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(redis_client_->get("point:101:latest").find("100") != std::string::npos);
    EXPECT_TRUE(redis_client_->get("point:201:latest").find("200") != std::string::npos);
    EXPECT_TRUE(redis_client_->get("point:202:latest").find("300") != std::string::npos);
    std::cout << "✅ Concurrent polling verified for multiple devices and points" << std::endl;

    // 5. 제어(Write) 테스트
    std::cout << "🎮 Testing control operation (WriteSingleRegister)..." << std::endl;
    Structs::DataValue write_val = static_cast<int16_t>(999);
    
    // Write 작업을 별도 스레드에서 실행하여 타임아웃 감지
    std::atomic<bool> write_done{false};
    std::atomic<bool> write_result{false};
    std::thread write_thread([&]() {
        write_result = worker2->WriteDataPoint(dp201.id, write_val);
        write_done = true;
    });
    
    // 최대 10초 대기
    for (int i = 0; i < 100 && !write_done; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!write_done) {
        std::cout << "❌ Write operation timed out after 10 seconds!" << std::endl;
        write_thread.detach(); // 스레드를 detach하여 종료 시 문제 방지
    } else {
        write_thread.join();
    }
    
    bool write_success = write_done && write_result;
    EXPECT_TRUE(write_success) << "Write operation failed!";

    // 다음 폴링에서 변경된 값을 가져오는지 확인
    bool verified = false;
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string val = redis_client_->get("point:201:latest");
        if (val.find("999") != std::string::npos) {
            std::cout << "✅ Control verified: Value 999 acquired after write" << std::endl;
            verified = true;
            break;
        }
    }
    EXPECT_TRUE(verified) << "Updated value not found in Redis after write!";

    // 6. 32비트 Float 제어 테스트
    std::cout << "🎮 Testing 32-bit Float Write (12345.67)..." << std::endl;
    Structs::DataValue float_val = 12345.67;
    bool float_write_success = worker2->WriteDataPoint(dp203.id, float_val);
    EXPECT_TRUE(float_write_success) << "Float Write operation failed!";
    
    // Verify Redis
    verified = false;
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string val = redis_client_->get("point:203:latest");
        if (val.find("12345.6") != std::string::npos) {
             std::cout << "✅ Float Control verified: Value 12345.67 acquired" << std::endl;
             verified = true;
             break;
        }
    }
    EXPECT_TRUE(verified) << "Float value not found in Redis!";

    worker1->Stop().get();
    worker2->Stop().get();
    virtual_server2->Stop();
}

// -----------------------------------------------------------------------------
// 테스트 케이스 4: 엔디안 (Word Swap) 검증 테스트
// -----------------------------------------------------------------------------
TEST_F(DriverReconnectionDataFlowTest, Endianness_Swapped_Float_Validation) {
    // 1. 가상 서버 준비 (기존 1502 포트 사용)
    virtual_server_->SetRegisterValue(20, 0xE6B7); // Low Word (CD)
    virtual_server_->SetRegisterValue(21, 0x4640); // High Word (AB) -> 12345.67 (Approx)
    // "swapped" 설정 시 registers[20]=Low, registers[21]=High 이므로 
    // 로직상 combined = (registers[21] << 16) | registers[20] = 0x4640E6B7 이 되어야 함.

    // 2. 워커 설정 (byte_order = "swapped")
    PulseOne::DeviceInfo dev_info;
    dev_info.id = "3";
    dev_info.name = "SwappedDevice";
    dev_info.endpoint = "127.0.0.1:1502";
    dev_info.polling_interval_ms = 500;
    dev_info.is_enabled = true;
    dev_info.driver_config.properties["byte_order"] = "swapped";

    auto worker = std::make_shared<ModbusWorker>(dev_info);
    
    DataPoint dp;
    dp.id = "301";
    dp.device_id = "3";
    dp.address = 40021; // Register 20
    dp.data_type = "FLOAT32";
    dp.is_enabled = true;
    
    worker->AddDataPoint(dp);
    worker->Start().get();

    // 3. Redis 데이터 확인
    std::cout << "🚀 Verifying swapped float data acquisition..." << std::endl;
    bool verified = false;
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string val = redis_client_->get("point:301:latest");
        if (!val.empty() && (val.find("12345.") != std::string::npos)) {
            std::cout << "✅ Swapped Float verified: " << val << std::endl;
            verified = true;
            break;
        }
        std::cout << "Waiting for swapped data... (" << val << ")" << std::endl;
    }
    EXPECT_TRUE(verified) << "Swapped float value not correctly interpreted!";

    worker->Stop().get();
}
