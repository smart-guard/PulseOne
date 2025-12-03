/**
 * @file test_mqtt_handler.cpp
 * @brief MqttTargetHandler 완전한 단위 테스트
 * @author PulseOne Development Team
 * @date 2025-12-03
 * @version 1.0.0
 * 
 * 🎯 테스트 목표:
 * [MQTT 브로커 연결] (5개)
 * - TCP 연결
 * - 클라이언트 ID 생성
 * - Username/Password 인증
 * - 연결 실패 처리
 * - 연결 상태 확인
 * 
 * [토픽 발행] (5개)
 * - 단순 토픽
 * - 템플릿 토픽
 * - QoS 0, 1, 2
 * - Retain 플래그
 * - 발행 실패
 * 
 * [메시지 포맷] (3개)
 * - JSON 형식
 * - Text 형식
 * - 커스텀 필드
 * 
 * [재연결 및 큐잉] (4개)
 * - 자동 재연결
 * - 메시지 큐잉
 * - 큐 오버플로우
 * - 재연결 후 처리
 * 
 * [통계 및 기타] (3개)
 * - 통계 정확도
 * - 연결 테스트
 * - 설정 검증
 * 
 * 📋 Mock MQTT 브로커 필요:
 * python3 mock_mqtt_broker.py &
 */

#include "CSP/MqttTargetHandler.h"
#include "Utils/LogManager.h"
#include "Export/ExportTypes.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace PulseOne::CSP;
using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════════
// 테스트 헬퍼
// ═══════════════════════════════════════════════════════════════════════════

static int test_count = 0;
static int passed_count = 0;
static int failed_count = 0;

#define TEST(name) \
    test_count++; \
    std::cout << "\n🧪 TEST_" << std::setfill('0') << std::setw(3) << test_count \
              << ": " << name << "... " << std::flush;

#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cout << "❌ FAIL: " << message << std::endl; \
        failed_count++; \
        return; \
    }

#define PASS() \
    std::cout << "✅" << std::flush; \
    passed_count++;

// Mock MQTT 브로커 설정
const std::string MQTT_BROKER_HOST = "localhost";
const int MQTT_BROKER_PORT = 1883;

// 더미 AlarmMessage 생성
AlarmMessage createTestAlarm(int building_id = 1001, const std::string& point_name = "TEMP_01", double value = 25.5) {
    AlarmMessage alarm;
    alarm.bd = building_id;
    alarm.nm = point_name;
    alarm.vl = value;
    alarm.tm = "2025-12-03T10:30:45.123Z";
    alarm.al = 1;
    alarm.st = 0;
    alarm.des = "Test MQTT alarm message";
    return alarm;
}

// Mock 브로커 헬스체크
bool checkMqttBroker() {
    try {
        MqttTargetHandler handler;
        json config = {
            {"broker_host", MQTT_BROKER_HOST},
            {"broker_port", MQTT_BROKER_PORT},
            {"client_id", "health_check"},
            {"auto_connect", true},
            {"auto_reconnect", false}
        };
        
        bool initialized = handler.initialize(config);
        
        if (initialized) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        return initialized;
        
    } catch (...) {
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 1: MQTT 브로커 연결 (5개)
// ═══════════════════════════════════════════════════════════════════════════

void test_tcp_connection() {
    TEST("TCP 연결");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"client_id", "test_tcp"},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    bool success = handler.initialize(config);
    ASSERT(success, "TCP 연결 실패");
    
    // 연결 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto status = handler.getStatus();
    ASSERT(status["type"] == "MQTT", "타입 불일치");
    ASSERT(status["connected"] == true, "연결 상태가 false");
    
    std::cout << " [" << status["client_id"].get<std::string>() << "]";
    
    PASS();
}

void test_client_id_generation() {
    TEST("클라이언트 ID 생성");
    
    MqttTargetHandler handler1;
    MqttTargetHandler handler2;
    
    json config1 = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"client_id", "custom_client"},
        {"auto_connect", false}
    };
    
    json config2 = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        // client_id 없음 (자동 생성)
        {"auto_connect", false}
    };
    
    handler1.initialize(config1);
    handler2.initialize(config2);
    
    auto status1 = handler1.getStatus();
    auto status2 = handler2.getStatus();
    
    std::string client_id1 = status1["client_id"].get<std::string>();
    std::string client_id2 = status2["client_id"].get<std::string>();
    
    ASSERT(!client_id1.empty(), "클라이언트 ID 1 없음");
    ASSERT(!client_id2.empty(), "클라이언트 ID 2 없음");
    ASSERT(client_id1.find("custom_client") != std::string::npos, "커스텀 ID 불일치");
    ASSERT(client_id2.find("pulseone") != std::string::npos, "자동 생성 ID 형식 불일치");
    
    std::cout << " [" << client_id1 << ", " << client_id2 << "]";
    
    PASS();
}

void test_username_password_auth() {
    TEST("Username/Password 인증");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"client_id", "test_auth"},
        {"username", "testuser"},
        {"password", "testpass"},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    bool success = handler.initialize(config);
    ASSERT(success, "인증 실패");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto status = handler.getStatus();
    ASSERT(status["connected"] == true, "인증 후 연결 실패");
    
    PASS();
}

void test_connection_failure() {
    TEST("연결 실패 처리");
    
    MqttTargetHandler handler;
    
    // 존재하지 않는 브로커
    json config = {
        {"broker_host", "nonexistent.mqtt.broker"},
        {"broker_port", 9999},
        {"client_id", "test_fail"},
        {"auto_connect", true},
        {"auto_reconnect", false},
        {"connect_timeout_sec", 1}
    };
    
    bool success = handler.initialize(config);
    
    // 초기화는 성공할 수 있지만 연결은 실패
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    auto status = handler.getStatus();
    // 연결 실패 상태 확인
    ASSERT(status["connected"] == false || status["connecting"] == true, 
           "연결 실패 상태가 아님");
    
    PASS();
}

void test_connection_status() {
    TEST("연결 상태 확인");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 연결 테스트
    bool test_result = handler.testConnection(config);
    ASSERT(test_result, "연결 테스트 실패");
    
    auto status = handler.getStatus();
    ASSERT(status.contains("connected"), "connected 필드 없음");
    ASSERT(status.contains("publish_count"), "publish_count 필드 없음");
    ASSERT(status.contains("broker_uri"), "broker_uri 필드 없음");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 2: 토픽 발행 (5개)
// ═══════════════════════════════════════════════════════════════════════════

void test_simple_topic() {
    TEST("단순 토픽 발행");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/simple"},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AlarmMessage alarm = createTestAlarm(101, "SENSOR_A", 30.5);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "발행 실패: " + result.error_message);
    ASSERT(result.mqtt_topic == "test/simple", "토픽 불일치");
    ASSERT(result.content_size > 0, "페이로드 크기 0");
    
    std::cout << " [" << result.mqtt_topic << ", " << result.content_size << "bytes]";
    
    PASS();
}

void test_template_topic() {
    TEST("템플릿 토픽 발행");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "building/{building_id}/alarm/{point_name}"},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AlarmMessage alarm = createTestAlarm(202, "TEMP_SENSOR", 28.3);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "템플릿 토픽 발행 실패");
    ASSERT(result.mqtt_topic.find("building/202") != std::string::npos, 
           "building_id 치환 실패");
    ASSERT(result.mqtt_topic.find("TEMP_SENSOR") != std::string::npos, 
           "point_name 치환 실패");
    
    std::cout << " [" << result.mqtt_topic << "]";
    
    PASS();
}

void test_qos_levels() {
    TEST("QoS 레벨 (0, 1, 2)");
    
    MqttTargetHandler handler;
    
    json base_config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/qos"},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(base_config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AlarmMessage alarm = createTestAlarm();
    
    int success_count = 0;
    
    // QoS 0, 1, 2 테스트
    for (int qos = 0; qos <= 2; qos++) {
        json config = base_config;
        config["qos"] = qos;
        
        auto result = handler.sendAlarm(alarm, config);
        if (result.success) {
            success_count++;
        }
    }
    
    ASSERT(success_count == 3, "QoS 레벨 테스트 실패");
    
    std::cout << " [QoS 0/1/2 성공]";
    
    PASS();
}

void test_retain_flag() {
    TEST("Retain 플래그");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/retain"},
        {"retain", true},  // ✅ Retained 메시지
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "Retained 메시지 발행 실패");
    
    // Retain false 테스트
    config["retain"] = false;
    auto result2 = handler.sendAlarm(alarm, config);
    ASSERT(result2.success, "일반 메시지 발행 실패");
    
    PASS();
}

void test_publish_failure() {
    TEST("발행 실패 처리");
    
    MqttTargetHandler handler;
    
    // 연결하지 않은 상태에서 발행 시도
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/fail"},
        {"auto_connect", false}  // ❌ 연결하지 않음
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(!result.success, "연결 없이 발행 성공 (이상함)");
    ASSERT(!result.error_message.empty(), "에러 메시지 없음");
    
    std::cout << " [" << result.error_message << "]";
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 3: 메시지 포맷 (3개)
// ═══════════════════════════════════════════════════════════════════════════

void test_json_format() {
    TEST("JSON 메시지 형식");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/json"},
        {"message_format", "json"},  // ✅ JSON 형식
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AlarmMessage alarm = createTestAlarm(303, "JSON_TEST", 99.9);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "JSON 메시지 발행 실패");
    ASSERT(result.content_size > 50, "JSON 페이로드가 너무 작음");
    
    std::cout << " [" << result.content_size << "bytes]";
    
    PASS();
}

void test_text_format() {
    TEST("Text 메시지 형식");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/text"},
        {"message_format", "text"},  // ✅ Text 형식
        {"text_format", "simple"},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "Text 메시지 발행 실패");
    ASSERT(result.content_size > 0, "Text 페이로드 없음");
    
    std::cout << " [" << result.content_size << "bytes]";
    
    PASS();
}

void test_custom_fields() {
    TEST("커스텀 필드");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/custom"},
        {"message_format", "json"},
        {"include_metadata", true},  // ✅ 메타데이터 포함
        {"additional_fields", {
            {"source", "PulseOne-Test"},
            {"environment", "production"}
        }},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "커스텀 필드 발행 실패");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 4: 재연결 및 큐잉 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_auto_reconnect() {
    TEST("자동 재연결");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"auto_connect", true},
        {"auto_reconnect", true},  // ✅ 자동 재연결 활성화
        {"reconnect_interval_sec", 2},
        {"max_reconnect_attempts", 3}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto status = handler.getStatus();
    ASSERT(status["connected"] == true, "초기 연결 실패");
    
    // 재연결 로직은 연결 끊김 시 동작하므로
    // 여기서는 설정만 검증
    ASSERT(status.contains("connection_attempts"), "connection_attempts 없음");
    
    PASS();
}

void test_message_queueing() {
    TEST("메시지 큐잉");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/queue"},
        {"max_queue_size", 100},
        {"auto_connect", false},  // ❌ 연결하지 않음 (큐잉 테스트)
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    
    // 연결 없이 여러 메시지 전송 (큐에 저장됨)
    for (int i = 0; i < 5; i++) {
        auto result = handler.sendAlarm(alarm, config);
        // 큐에 저장되므로 success는 false
    }
    
    auto status = handler.getStatus();
    ASSERT(status["queue_size"].get<int>() >= 3, "큐에 메시지가 저장되지 않음");
    
    std::cout << " [큐 크기: " << status["queue_size"] << "]";
    
    PASS();
}

void test_queue_overflow() {
    TEST("큐 오버플로우");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/overflow"},
        {"max_queue_size", 10},  // ✅ 작은 큐 크기
        {"auto_connect", false},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    
    // 큐 크기를 초과하도록 메시지 전송
    for (int i = 0; i < 15; i++) {
        handler.sendAlarm(alarm, config);
    }
    
    auto status = handler.getStatus();
    int queue_size = status["queue_size"].get<int>();
    
    ASSERT(queue_size <= 10, "큐 크기가 max_queue_size를 초과");
    
    std::cout << " [최대 큐: " << queue_size << "/10]";
    
    PASS();
}

void test_queue_processing() {
    TEST("재연결 후 큐 처리");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/process"},
        {"max_queue_size", 100},
        {"auto_connect", false},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    
    // 큐에 메시지 저장
    AlarmMessage alarm = createTestAlarm();
    for (int i = 0; i < 3; i++) {
        handler.sendAlarm(alarm, config);
    }
    
    auto status_before = handler.getStatus();
    int queue_before = status_before["queue_size"].get<int>();
    ASSERT(queue_before >= 2, "큐에 메시지가 없음");
    
    // 연결 후 큐 처리 (실제로는 자동으로 처리됨)
    config["auto_connect"] = true;
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    auto status_after = handler.getStatus();
    int queue_after = status_after["queue_size"].get<int>();
    
    // 큐가 처리되어 줄어들어야 함
    ASSERT(queue_after < queue_before, "큐가 처리되지 않음");
    
    std::cout << " [" << queue_before << " → " << queue_after << "]";
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 5: 통계 및 기타 (3개)
// ═══════════════════════════════════════════════════════════════════════════

void test_statistics() {
    TEST("통계 정확도");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"topic_pattern", "test/stats"},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto initial_stats = handler.getStatus();
    
    // 5번 발행
    AlarmMessage alarm = createTestAlarm();
    for (int i = 0; i < 5; i++) {
        handler.sendAlarm(alarm, config);
    }
    
    auto final_stats = handler.getStatus();
    
    int publish_count = final_stats["publish_count"].get<int>();
    int success_count = final_stats["success_count"].get<int>();
    
    ASSERT(publish_count >= 5, "발행 카운트 부족");
    ASSERT(success_count >= 4, "성공 카운트 부족");
    
    std::cout << " [발행:" << publish_count << ", 성공:" << success_count << "]";
    
    PASS();
}

void test_connection_test() {
    TEST("연결 테스트 메서드");
    
    MqttTargetHandler handler;
    
    json config = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT},
        {"auto_connect", true},
        {"auto_reconnect", false}
    };
    
    handler.initialize(config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    bool test_result = handler.testConnection(config);
    ASSERT(test_result, "연결 테스트 실패");
    
    // 연결되지 않은 핸들러 테스트
    MqttTargetHandler handler2;
    json bad_config = {
        {"broker_host", "nonexistent.broker"},
        {"broker_port", 9999},
        {"connect_timeout_sec", 1}
    };
    
    bool test_result2 = handler2.testConnection(bad_config);
    ASSERT(!test_result2, "잘못된 연결인데 성공");
    
    PASS();
}

void test_config_validation() {
    TEST("설정 검증");
    
    MqttTargetHandler handler;
    
    // ❌ broker_host 없음
    json config1 = {
        {"broker_port", 1883}
    };
    std::vector<std::string> errors1;
    bool valid1 = handler.validateConfig(config1, errors1);
    ASSERT(!valid1, "broker_host 없는데 검증 통과");
    ASSERT(!errors1.empty(), "에러 메시지 없음");
    
    // ❌ 빈 broker_host
    json config2 = {
        {"broker_host", ""},
        {"broker_port", 1883}
    };
    std::vector<std::string> errors2;
    bool valid2 = handler.validateConfig(config2, errors2);
    ASSERT(!valid2, "빈 broker_host인데 검증 통과");
    
    // ✅ 정상 설정
    json config3 = {
        {"broker_host", MQTT_BROKER_HOST},
        {"broker_port", MQTT_BROKER_PORT}
    };
    std::vector<std::string> errors3;
    bool valid3 = handler.validateConfig(config3, errors3);
    ASSERT(valid3, "정상 설정인데 검증 실패");
    ASSERT(errors3.empty(), "에러 메시지가 있음");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 메인
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  MqttTargetHandler 완전한 단위 테스트\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    auto& logger = LogManager::getInstance();
    logger.Info("MqttTargetHandler 테스트 시작");
    
    // Mock MQTT 브로커 체크
    std::cout << "\n🔍 Mock MQTT 브로커 확인 중... " << std::flush;
    if (!checkMqttBroker()) {
        std::cout << "❌\n";
        std::cerr << "\n💥 에러: Mock MQTT 브로커가 실행되지 않았습니다!\n";
        std::cerr << "다음 명령어로 서버를 시작하세요:\n";
        std::cerr << "  python3 mock_mqtt_broker.py &\n\n";
        return 1;
    }
    std::cout << "✅\n";
    
    // 파트 1: MQTT 브로커 연결 (5개)
    test_tcp_connection();
    test_client_id_generation();
    test_username_password_auth();
    test_connection_failure();
    test_connection_status();
    
    // 파트 2: 토픽 발행 (5개)
    test_simple_topic();
    test_template_topic();
    test_qos_levels();
    test_retain_flag();
    test_publish_failure();
    
    // 파트 3: 메시지 포맷 (3개)
    test_json_format();
    test_text_format();
    test_custom_fields();
    
    // 파트 4: 재연결 및 큐잉 (4개)
    test_auto_reconnect();
    test_message_queueing();
    test_queue_overflow();
    test_queue_processing();
    
    // 파트 5: 통계 및 기타 (3개)
    test_statistics();
    test_connection_test();
    test_config_validation();
    
    // 최종 결과
    std::cout << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    if (failed_count == 0) {
        std::cout << "  🎉 결과: " << passed_count << "/" << test_count << " passed - PERFECT! 🎉\n";
    } else {
        std::cout << "  ⚠️  결과: " << passed_count << "/" << test_count << " passed";
        std::cout << " (" << failed_count << " failed) ⚠️\n";
    }
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    logger.Info("MqttTargetHandler 테스트 완료");
    
    return (failed_count == 0) ? 0 : 1;
}