
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "Workers/Protocol/BACnetWorker.h"
#include "Drivers/Bacnet/BACnetDriver.h"
#include "Logging/LogManager.h"
#include "DatabaseManager.hpp"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Drivers/Common/DriverFactory.h"

using namespace PulseOne;
using namespace PulseOne::Workers;
using namespace PulseOne::Drivers;
using namespace PulseOne::Database::Repositories;

class BACnetReadWriteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 로거 초기화 (자동)
        LogManager::getInstance();
        
        // DB 초기화
        DbLib::DatabaseConfig db_config;
        db_config.type = "SQLITE";
        db_config.sqlite_path = ":memory:";
        DbLib::DatabaseManager::getInstance().initialize(db_config); 

        // 드라이버 수동 등록
        DriverFactory::GetInstance().RegisterDriver("BACNET", []() {
            return std::make_unique<BACnetDriver>();
        });
        DriverFactory::GetInstance().RegisterDriver("BACNET_IP", []() {
            return std::make_unique<BACnetDriver>();
        });
    }

    void TearDown() override {
        // 정리
    }
    
    // 테스트용 DeviceInfo 생성
    DeviceInfo CreateTestDeviceInfo() {
        DeviceInfo info;
        info.id = "99002"; // 테스트용 ID
        info.name = "Test BACnet RW Device";
        info.protocol_type = "BACNET_IP";
        info.endpoint = "192.168.1.100:47808";
        info.polling_interval_ms = 1000;
        info.driver_config.protocol = Enums::ProtocolType::BACNET_IP;
        
        // BACnet 특화 속성
        info.properties["device_id"] = "12345";
        info.properties["local_device_id"] = "1001";
        
        return info;
    }
    
    // 테스트용 DataPoint 생성
    DataPoint CreateTestDataPoint(const std::string& object_id) {
        DataPoint dp;
        dp.id = "1";
        dp.device_id = "99002";
        dp.name = "Test Analog Input";
        dp.address = 1; // Analog Input 1
        dp.protocol_params["object_id"] = object_id;
        dp.scaling_factor = 1.0;
        dp.scaling_offset = 0.0;
        return dp;
    }
};

// 1. BACnetWorker 초기화 및 연결 테스트
TEST_F(BACnetReadWriteTest, InitializationAndConnection) {
    auto device_info = CreateTestDeviceInfo();
    auto worker = std::make_shared<BACnetWorker>(device_info);
    
    // 연결 시도 (비동기 Start 대신 직접 함수 호출하여 테스트)
    // EstablishProtocolConnection은 protected이므로 Start()를 통해 간접 호출하거나
    // Friend 클래스/테스트용 서브클래스 사용 필요. 
    // 여기서는 Start() 호출 후 잠시 대기
    
    auto future = worker->Start();
    std::future_status status = future.wait_for(std::chrono::seconds(2));
    
    // 시뮬레이션 모드 또는 실제 스택이라도 로컬에서는 연결 성공(소켓 생성 성공)으로 간주될 수 있음
    // 로그 확인 필요하지만, 반환값으로 체크
    
    // Start는 비동기이므로 true 반환은 스레드 시작 성공을 의미
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(future.get());
    
    worker->Stop().wait();
}

// 2. Data Read 테스트 (Polling Loop 검증)
TEST_F(BACnetReadWriteTest, ReadDataFlow) {
    auto device_info = CreateTestDeviceInfo();
    auto worker = std::make_shared<BACnetWorker>(device_info);
    
    // 데이터 포인트 설정
    std::vector<DataPoint> points;
    points.push_back(CreateTestDataPoint("analog-input:1"));
    worker->LoadDataPointsFromConfiguration(points);
    
    // 데이터 수신 확인을 위한 콜백 등록
    bool value_received = false;
    double received_value = 0.0;
    
    worker->SetValueChangedCallback([&](const std::string& id, const TimestampedValue& val) {
        if (id == "1") { // point.id가 "1"로 설정됨
            value_received = true;
            received_value = std::get<double>(val.value);
            std::cout << "Received value: " << received_value << " from " << id << std::endl;
        }
    });
    
    worker->Start();
    
    // 폴링 주기(1초)보다 조금 더 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    worker->Stop().wait();
    
    // 검증: 시뮬레이션 드라이버라면 23.5가 수신되어야 함
    // 실제 드라이버라면 장비가 없으므로 실패하거나 타임아웃 뜰 수 있음
    // 현재 코드베이스 상태 확인 결과: BACnetDriver.cpp의 ReadSingleProperty가 23.5를 반환하도록 되어 있음 (Mock)
    // 따라서 실제 스택이 활성화되어도 ReadSingleProperty가 호출된다면 값을 받을 것임.
    
    EXPECT_TRUE(value_received) << "Data should be received via polling loop";
    if (value_received) {
        EXPECT_NEAR(received_value, 23.5, 0.001) << "Expected simulated value 23.5";
    }
}

// 3. Write Property 테스트
TEST_F(BACnetReadWriteTest, WritePropertyFlow) {
    auto device_info = CreateTestDeviceInfo();
    auto worker = std::make_shared<BACnetWorker>(device_info);
    
    worker->Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 연결 대기
    
    // Write 요청
    DataValue value_to_write = 50.5;
    bool success = worker->WriteProperty(
        12345, // device_id
        OBJECT_ANALOG_VALUE,
        1, // instance
        PROP_PRESENT_VALUE,
        value_to_write,
        16 // priority
    );
    
    EXPECT_TRUE(success) << "Write operation should succeed (simulated)";
    
    worker->Stop().wait();
}
