/**
 * @file test_common_types.cpp
 * @brief CommonTypes.h의 단위 테스트
 * @details 공통 타입들의 기능과 유효성을 검증하는 테스트 코드
 * @author PulseOne Development Team
 * @date 2025-01-17
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Drivers/CommonTypes.h"
#include <thread>
#include <chrono>
#include <vector>
#include <set>

using namespace PulseOne::Drivers;
using ::testing::_;
using ::testing::Return;

// =============================================================================
// 기본 타입 변환 테스트
// =============================================================================

class CommonTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 DeviceInfo 초기화
        device_info_.id = GenerateUUID();
        device_info_.name = "Test Device";
        device_info_.description = "Test device for unit testing";
        device_info_.protocol = ProtocolType::MODBUS_TCP;
        device_info_.endpoint = "192.168.1.100:502";
        device_info_.polling_interval_ms = 1000;
        device_info_.timeout_ms = 5000;
        device_info_.retry_count = 3;
        
        // 테스트용 DataPoint 초기화
        data_point_.id = GenerateUUID();
        data_point_.device_id = device_info_.id;
        data_point_.name = "Temperature";
        data_point_.description = "Temperature sensor reading";
        data_point_.address = 100;
        data_point_.data_type = DataType::FLOAT32;
        data_point_.unit = "°C";
        data_point_.scaling_factor = 0.1;
        data_point_.scaling_offset = 0.0;
        data_point_.min_value = -50.0;
        data_point_.max_value = 150.0;
    }
    
    DeviceInfo device_info_;
    DataPoint data_point_;
};

// =============================================================================
// 프로토콜 타입 변환 테스트
// =============================================================================

TEST_F(CommonTypesTest, ProtocolTypeConversion) {
    // 정상적인 변환 테스트
    EXPECT_EQ("modbus_tcp", ProtocolTypeToString(ProtocolType::MODBUS_TCP));
    EXPECT_EQ("modbus_rtu", ProtocolTypeToString(ProtocolType::MODBUS_RTU));
    EXPECT_EQ("mqtt", ProtocolTypeToString(ProtocolType::MQTT));
    EXPECT_EQ("bacnet_ip", ProtocolTypeToString(ProtocolType::BACNET_IP));
    EXPECT_EQ("opc_ua", ProtocolTypeToString(ProtocolType::OPC_UA));
    EXPECT_EQ("unknown", ProtocolTypeToString(ProtocolType::UNKNOWN));
    
    // 역변환 테스트
    EXPECT_EQ(ProtocolType::MODBUS_TCP, StringToProtocolType("modbus_tcp"));
    EXPECT_EQ(ProtocolType::MODBUS_RTU, StringToProtocolType("modbus_rtu"));
    EXPECT_EQ(ProtocolType::MQTT, StringToProtocolType("mqtt"));
    EXPECT_EQ(ProtocolType::BACNET_IP, StringToProtocolType("bacnet_ip"));
    EXPECT_EQ(ProtocolType::OPC_UA, StringToProtocolType("opc_ua"));
    EXPECT_EQ(ProtocolType::UNKNOWN, StringToProtocolType("invalid_protocol"));
    
    // 대소문자 및 잘못된 입력 테스트
    EXPECT_EQ(ProtocolType::UNKNOWN, StringToProtocolType("MODBUS_TCP"));
    EXPECT_EQ(ProtocolType::UNKNOWN, StringToProtocolType(""));
    EXPECT_EQ(ProtocolType::UNKNOWN, StringToProtocolType("xyz"));
}

// 매개변수화 테스트: 모든 프로토콜 타입에 대한 변환 검증
class ProtocolTypeParameterizedTest : public ::testing::TestWithParam<ProtocolType> {};

TEST_P(ProtocolTypeParameterizedTest, RoundTripConversion) {
    ProtocolType original_type = GetParam();
    std::string type_string = ProtocolTypeToString(original_type);
    ProtocolType converted_type = StringToProtocolType(type_string);
    
    // UNKNOWN 타입은 "unknown" 문자열로 변환되므로 예외 처리
    if (original_type == ProtocolType::UNKNOWN) {
        EXPECT_EQ(ProtocolType::UNKNOWN, converted_type);
    } else {
        EXPECT_EQ(original_type, converted_type);
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllProtocolTypes,
    ProtocolTypeParameterizedTest,
    ::testing::Values(
        ProtocolType::UNKNOWN,
        ProtocolType::MODBUS_TCP,
        ProtocolType::MODBUS_RTU,
        ProtocolType::MQTT,
        ProtocolType::BACNET_IP,
        ProtocolType::OPC_UA,
        ProtocolType::ETHERNET_IP,
        ProtocolType::PROFINET,
        ProtocolType::CUSTOM
    )
);

// =============================================================================
// 데이터 타입 변환 테스트
// =============================================================================

TEST_F(CommonTypesTest, DataTypeConversion) {
    // 기본 데이터 타입 변환 테스트
    EXPECT_EQ("bool", DataTypeToString(DataType::BOOL));
    EXPECT_EQ("int16", DataTypeToString(DataType::INT16));
    EXPECT_EQ("uint16", DataTypeToString(DataType::UINT16));
    EXPECT_EQ("int32", DataTypeToString(DataType::INT32));
    EXPECT_EQ("uint32", DataTypeToString(DataType::UINT32));
    EXPECT_EQ("float32", DataTypeToString(DataType::FLOAT32));
    EXPECT_EQ("float64", DataTypeToString(DataType::FLOAT64));
    EXPECT_EQ("string", DataTypeToString(DataType::STRING));
    EXPECT_EQ("binary", DataTypeToString(DataType::BINARY));
    EXPECT_EQ("unknown", DataTypeToString(DataType::UNKNOWN));
}

// =============================================================================
// 연결 상태 변환 테스트
// =============================================================================

TEST_F(CommonTypesTest, ConnectionStatusConversion) {
    EXPECT_EQ("disconnected", ConnectionStatusToString(ConnectionStatus::DISCONNECTED));
    EXPECT_EQ("connecting", ConnectionStatusToString(ConnectionStatus::CONNECTING));
    EXPECT_EQ("connected", ConnectionStatusToString(ConnectionStatus::CONNECTED));
    EXPECT_EQ("reconnecting", ConnectionStatusToString(ConnectionStatus::RECONNECTING));
    EXPECT_EQ("error", ConnectionStatusToString(ConnectionStatus::ERROR));
    EXPECT_EQ("maintenance", ConnectionStatusToString(ConnectionStatus::MAINTENANCE));
    EXPECT_EQ("timeout", ConnectionStatusToString(ConnectionStatus::TIMEOUT));
    EXPECT_EQ("unauthorized", ConnectionStatusToString(ConnectionStatus::UNAUTHORIZED));
}

// =============================================================================
// UUID 생성 테스트
// =============================================================================

TEST_F(CommonTypesTest, UUIDGeneration) {
    // UUID 생성 기본 테스트
    UUID uuid1 = GenerateUUID();
    UUID uuid2 = GenerateUUID();
    
    EXPECT_FALSE(uuid1.empty());
    EXPECT_FALSE(uuid2.empty());
    EXPECT_NE(uuid1, uuid2);  // 서로 다른 UUID가 생성되어야 함
    
    // UUID 형식 검증 (간단한 패턴 매칭)
    // 형식: xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx
    EXPECT_EQ(36, uuid1.length());
    EXPECT_EQ('-', uuid1[8]);
    EXPECT_EQ('-', uuid1[13]);
    EXPECT_EQ('-', uuid1[18]);
    EXPECT_EQ('-', uuid1[23]);
    EXPECT_EQ('4', uuid1[14]);  // UUID v4 표시
    
    // 대량 생성 시 중복 검사
    std::set<UUID> uuid_set;
    constexpr int NUM_UUIDS = 1000;
    
    for (int i = 0; i < NUM_UUIDS; ++i) {
        UUID uuid = GenerateUUID();
        EXPECT_TRUE(uuid_set.insert(uuid).second) 
            << "Duplicate UUID generated: " << uuid;
    }
    
    EXPECT_EQ(NUM_UUIDS, uuid_set.size());
}

// =============================================================================
// 타임스탬프 관련 테스트
// =============================================================================

TEST_F(CommonTypesTest, TimestampHandling) {
    auto now = std::chrono::system_clock::now();
    std::string iso_string = TimestampToISOString(now);
    
    // ISO 8601 형식 기본 검증
    EXPECT_FALSE(iso_string.empty());
    EXPECT_GT(iso_string.length(), 20);  // 최소 길이 확인
    EXPECT_NE(std::string::npos, iso_string.find('T'));  // 날짜/시간 구분자
    EXPECT_NE(std::string::npos, iso_string.find('Z'));  // UTC 표시
    
    // 여러 시간대에서 일관성 확인
    auto past = now - std::chrono::hours(1);
    auto future = now + std::chrono::hours(1);
    
    std::string past_string = TimestampToISOString(past);
    std::string future_string = TimestampToISOString(future);
    
    EXPECT_LT(past_string, iso_string);    // 사전식 정렬로 시간 순서 확인
    EXPECT_LT(iso_string, future_string);
}

// =============================================================================
// DeviceInfo 구조체 테스트
// =============================================================================

TEST_F(CommonTypesTest, DeviceInfoValidation) {
    // 유효한 DeviceInfo 테스트
    EXPECT_TRUE(device_info_.IsValid());
    
    // 잘못된 경우들 테스트
    DeviceInfo invalid_device = device_info_;
    
    invalid_device.id = "";
    EXPECT_FALSE(invalid_device.IsValid()) << "Empty ID should be invalid";
    
    invalid_device = device_info_;
    invalid_device.name = "";
    EXPECT_FALSE(invalid_device.IsValid()) << "Empty name should be invalid";
    
    invalid_device = device_info_;
    invalid_device.protocol = ProtocolType::UNKNOWN;
    EXPECT_FALSE(invalid_device.IsValid()) << "Unknown protocol should be invalid";
    
    invalid_device = device_info_;
    invalid_device.endpoint = "";
    EXPECT_FALSE(invalid_device.IsValid()) << "Empty endpoint should be invalid";
    
    // 경계값 테스트
    invalid_device = device_info_;
    invalid_device.polling_interval_ms = 0;
    EXPECT_FALSE(invalid_device.IsValid()) << "Zero polling interval should be invalid";
    
    invalid_device = device_info_;
    invalid_device.timeout_ms = 0;
    EXPECT_FALSE(invalid_device.IsValid()) << "Zero timeout should be invalid";
    
    invalid_device = device_info_;
    invalid_device.retry_count = -1;
    EXPECT_FALSE(invalid_device.IsValid()) << "Negative retry count should be invalid";
}

// =============================================================================
// DataPoint 구조체 테스트
// =============================================================================

TEST_F(CommonTypesTest, DataPointOperators) {
    DataPoint point1 = data_point_;
    DataPoint point2 = data_point_;
    DataPoint point3 = data_point_;
    point3.id = GenerateUUID();  // 다른 ID
    
    // 등등 연산자 테스트
    EXPECT_EQ(point1, point2);
    EXPECT_NE(point1, point3);
    
    // 비교 연산자 테스트 (STL 컨테이너 사용을 위해)
    std::set<DataPoint> point_set;
    point_set.insert(point1);
    point_set.insert(point2);  // 동일한 ID이므로 중복 삽입 실패
    point_set.insert(point3);
    
    EXPECT_EQ(2, point_set.size());  // point1과 point3만 삽입됨
}

// =============================================================================
// TimestampedValue 구조체 테스트
// =============================================================================

TEST_F(CommonTypesTest, TimestampedValueCreation) {
    // 다양한 데이터 타입으로 TimestampedValue 생성
    TimestampedValue bool_value(DataValue(true), DataQuality::GOOD);
    TimestampedValue int_value(DataValue(42), DataQuality::GOOD);
    TimestampedValue float_value(DataValue(3.14f), DataQuality::UNCERTAIN);
    TimestampedValue string_value(DataValue(std::string("test")), DataQuality::BAD);
    
    // 유효성 검사
    EXPECT_TRUE(bool_value.IsValid());
    EXPECT_TRUE(int_value.IsValid());
    EXPECT_TRUE(float_value.IsValid());   // UNCERTAIN도 유효함
    EXPECT_FALSE(string_value.IsValid());  // BAD는 무효함
    
    // 데이터 타입 확인
    EXPECT_TRUE(std::holds_alternative<bool>(bool_value.value));
    EXPECT_TRUE(std::holds_alternative<int>(int_value.value));
    EXPECT_TRUE(std::holds_alternative<float>(float_value.value));
    EXPECT_TRUE(std::holds_alternative<std::string>(string_value.value));
    
    // 품질 확인
    EXPECT_EQ(DataQuality::GOOD, bool_value.quality);
    EXPECT_EQ(DataQuality::UNCERTAIN, float_value.quality);
    EXPECT_EQ(DataQuality::BAD, string_value.quality);
}

// =============================================================================
// ErrorInfo 구조체 테스트
// =============================================================================

TEST_F(CommonTypesTest, ErrorInfoHandling) {
    // 성공 케이스
    ErrorInfo success_error;
    EXPECT_TRUE(success_error.IsSuccess());
    EXPECT_EQ(ErrorCode::SUCCESS, success_error.code);
    
    // 실패 케이스들
    ErrorInfo connection_error(ErrorCode::CONNECTION_FAILED, "Connection timeout");
    EXPECT_FALSE(connection_error.IsSuccess());
    EXPECT_EQ(ErrorCode::CONNECTION_FAILED, connection_error.code);
    EXPECT_EQ("Connection timeout", connection_error.message);
    
    ErrorInfo protocol_error(ErrorCode::PROTOCOL_ERROR, "Invalid message format", "ReadHoldingRegisters");
    EXPECT_FALSE(protocol_error.IsSuccess());
    EXPECT_EQ("ReadHoldingRegisters", protocol_error.context);
    
    // 타임스탬프 확인
    auto error_time = connection_error.occurred_at;
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - error_time);
    EXPECT_LT(diff.count(), 1);  // 1초 이내에 생성되었는지 확인
}

// =============================================================================
// DataValue variant 테스트
// =============================================================================

class DataValueTest : public ::testing::Test {
protected:
    void SetUp() override {
        bool_value_ = DataValue(true);
        int8_value_ = DataValue(static_cast<int8_t>(-128));
        uint8_value_ = DataValue(static_cast<uint8_t>(255));
        int16_value_ = DataValue(static_cast<int16_t>(-32768));
        uint16_value_ = DataValue(static_cast<uint16_t>(65535));
        int32_value_ = DataValue(static_cast<int32_t>(-2147483648));
        uint32_value_ = DataValue(static_cast<uint32_t>(4294967295));
        float_value_ = DataValue(3.14159f);
        double_value_ = DataValue(2.718281828);
        string_value_ = DataValue(std::string("Hello, World!"));
        binary_value_ = DataValue(std::vector<uint8_t>{0x01, 0x02, 0x03, 0xFF});
        empty_value_ = DataValue{};  // std::monostate
    }
    
    DataValue bool_value_;
    DataValue int8_value_;
    DataValue uint8_value_;
    DataValue int16_value_;
    DataValue uint16_value_;
    DataValue int32_value_;
    DataValue uint32_value_;
    DataValue float_value_;
    DataValue double_value_;
    DataValue string_value_;
    DataValue binary_value_;
    DataValue empty_value_;
};

TEST_F(DataValueTest, TypeChecking) {
    EXPECT_TRUE(std::holds_alternative<bool>(bool_value_));
    EXPECT_TRUE(std::holds_alternative<int8_t>(int8_value_));
    EXPECT_TRUE(std::holds_alternative<uint8_t>(uint8_value_));
    EXPECT_TRUE(std::holds_alternative<int16_t>(int16_value_));
    EXPECT_TRUE(std::holds_alternative<uint16_t>(uint16_value_));
    EXPECT_TRUE(std::holds_alternative<int32_t>(int32_value_));
    EXPECT_TRUE(std::holds_alternative<uint32_t>(uint32_value_));
    EXPECT_TRUE(std::holds_alternative<float>(float_value_));
    EXPECT_TRUE(std::holds_alternative<double>(double_value_));
    EXPECT_TRUE(std::holds_alternative<std::string>(string_value_));
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(binary_value_));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(empty_value_));
}

TEST_F(DataValueTest, ValueRetrieval) {
    // 타입 안전한 값 추출 테스트
    EXPECT_EQ(true, std::get<bool>(bool_value_));
    EXPECT_EQ(-128, std::get<int8_t>(int8_value_));
    EXPECT_EQ(255, std::get<uint8_t>(uint8_value_));
    EXPECT_EQ(-32768, std::get<int16_t>(int16_value_));
    EXPECT_EQ(65535, std::get<uint16_t>(uint16_value_));
    EXPECT_EQ(-2147483648, std::get<int32_t>(int32_value_));
    EXPECT_EQ(4294967295U, std::get<uint32_t>(uint32_value_));
    EXPECT_FLOAT_EQ(3.14159f, std::get<float>(float_value_));
    EXPECT_DOUBLE_EQ(2.718281828, std::get<double>(double_value_));
    EXPECT_EQ("Hello, World!", std::get<std::string>(string_value_));
    
    auto binary_data = std::get<std::vector<uint8_t>>(binary_value_);
    EXPECT_EQ(4, binary_data.size());
    EXPECT_EQ(0x01, binary_data[0]);
    EXPECT_EQ(0xFF, binary_data[3]);
}

// =============================================================================
// 상수 값 검증 테스트
// =============================================================================

TEST_F(CommonTypesTest, ConstantsValidation) {
    // 기본 설정 상수들이 합리적인 값인지 확인
    EXPECT_GT(Constants::DEFAULT_POLLING_INTERVAL_MS, 0);
    EXPECT_LT(Constants::DEFAULT_POLLING_INTERVAL_MS, 60000);  // 1분 이내
    
    EXPECT_GT(Constants::DEFAULT_TIMEOUT_MS, 0);
    EXPECT_LT(Constants::DEFAULT_TIMEOUT_MS, 30000);  // 30초 이내
    
    EXPECT_GE(Constants::DEFAULT_RETRY_COUNT, 0);
    EXPECT_LE(Constants::DEFAULT_RETRY_COUNT, 10);  // 합리적인 재시도 횟수
    
    // 포트 번호들이 유효한 범위인지 확인
    EXPECT_GT(Constants::DEFAULT_MODBUS_TCP_PORT, 0);
    EXPECT_LT(Constants::DEFAULT_MODBUS_TCP_PORT, 65536);
    
    EXPECT_GT(Constants::DEFAULT_MQTT_PORT, 0);
    EXPECT_LT(Constants::DEFAULT_MQTT_PORT, 65536);
    
    // Modbus 프로토콜 제한값들 확인
    EXPECT_GT(Constants::MODBUS_MAX_READ_REGISTERS, 0);
    EXPECT_LE(Constants::MODBUS_MAX_READ_REGISTERS, 125);  // Modbus 표준 제한
    
    EXPECT_GT(Constants::MODBUS_MAX_READ_COILS, 0);
    EXPECT_LE(Constants::MODBUS_MAX_READ_COILS, 2000);  // Modbus 표준 제한
}

// =============================================================================
// 멀티스레드 안전성 테스트
// =============================================================================

TEST_F(CommonTypesTest, ThreadSafetyTest) {
    constexpr int NUM_THREADS = 10;
    constexpr int NUM_OPERATIONS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<UUID>> generated_uuids(NUM_THREADS);
    
    // 여러 스레드에서 동시에 UUID 생성
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&generated_uuids, i]() {
            for (int j = 0; j < NUM_OPERATIONS_PER_THREAD; ++j) {
                generated_uuids[i].push_back(GenerateUUID());
            }
        });
    }
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 전체 UUID 수집 및 중복 검사
    std::set<UUID> all_uuids;
    for (const auto& thread_uuids : generated_uuids) {
        for (const auto& uuid : thread_uuids) {
            EXPECT_TRUE(all_uuids.insert(uuid).second) 
                << "Duplicate UUID found in multithreaded test: " << uuid;
        }
    }
    
    EXPECT_EQ(NUM_THREADS * NUM_OPERATIONS_PER_THREAD, all_uuids.size());
}

// =============================================================================
// 메모리 효율성 테스트
// =============================================================================

TEST_F(CommonTypesTest, MemoryEfficiency) {
    // DataValue의 메모리 효율성 확인
    // std::variant는 가장 큰 타입의 크기 + 타입 태그 크기
    size_t data_value_size = sizeof(DataValue);
    size_t string_size = sizeof(std::string);
    size_t vector_size = sizeof(std::vector<uint8_t>);
    
    // DataValue는 가장 큰 멤버 타입보다 크지만 모든 타입의 합보다는 작아야 함
    EXPECT_GE(data_value_size, std::max(string_size, vector_size));
    EXPECT_LT(data_value_size, string_size + vector_size + sizeof(double) + sizeof(uint64_t));
    
    std::cout << "DataValue size: " << data_value_size << " bytes" << std::endl;
    std::cout << "std::string size: " << string_size << " bytes" << std::endl;
    std::cout << "std::vector<uint8_t> size: " << vector_size << " bytes" << std::endl;
}