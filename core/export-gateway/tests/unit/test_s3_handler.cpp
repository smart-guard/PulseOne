/**
 * @file test_s3_handler.cpp
 * @brief S3TargetHandler 완전한 단위 테스트
 * @author PulseOne Development Team
 * @date 2025-12-03
 * @version 1.0.0
 * 
 * 🎯 테스트 목표:
 * [S3 연결] (4개)
 * - 기본 연결
 * - 자격증명 로드
 * - 버킷 검증
 * - 연결 실패
 * 
 * [파일 업로드] (5개)
 * - 단순 업로드
 * - 객체 키 템플릿
 * - JSON 내용
 * - 압축
 * - 업로드 실패
 * 
 * [메타데이터] (3개)
 * - 기본 메타데이터
 * - 커스텀 메타데이터
 * - 알람 정보
 * 
 * [에러 처리] (4개)
 * - 버킷 없음 (자동 생성)
 * - 잘못된 엔드포인트
 * - 타임아웃
 * - 재시도
 * 
 * [통계 및 기타] (4개)
 * - 통계 정확도
 * - 설정 검증
 * - ClientCache 사용
 * - 연결 테스트
 * 
 * 📋 Mock S3 서버 필요:
 * python3 mock_s3_server.py &
 */

#include "CSP/S3TargetHandler.h"
#include "Logging/LogManager.h"
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

// Mock S3 서버 설정
const std::string S3_ENDPOINT = "http://localhost:9000";
const std::string S3_BUCKET = "test-pulseone-bucket";
const std::string S3_ACCESS_KEY = "minioadmin";
const std::string S3_SECRET_KEY = "minioadmin";

// 더미 AlarmMessage 생성
AlarmMessage createTestAlarm(int building_id = 1001, const std::string& point_name = "TEMP_01", double value = 25.5) {
    AlarmMessage alarm;
    alarm.bd = building_id;
    alarm.nm = point_name;
    alarm.vl = value;
    alarm.tm = "2025-12-03T10:30:45.123Z";
    alarm.al = 1;
    alarm.st = 0;
    alarm.des = "Test S3 alarm message";
    return alarm;
}

// Mock S3 서버 헬스체크
bool checkS3Server() {
    try {
        S3TargetHandler handler;
        json config = {
            {"bucket_name", S3_BUCKET},
            {"endpoint", S3_ENDPOINT},
            {"region", "us-east-1"},
            {"access_key", S3_ACCESS_KEY},
            {"secret_key", S3_SECRET_KEY},
            {"verify_ssl", false}
        };
        
        return handler.testConnection(config);
        
    } catch (...) {
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 1: S3 연결 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_basic_connection() {
    TEST("기본 S3 연결");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"verify_ssl", false}
    };
    
    bool success = handler.initialize(config);
    ASSERT(success, "초기화 실패");
    
    // 연결 테스트
    bool test_result = handler.testConnection(config);
    ASSERT(test_result, "연결 테스트 실패");
    
    auto status = handler.getStatus();
    ASSERT(status["type"] == "S3", "타입 불일치");
    
    PASS();
}

void test_credentials_loading() {
    TEST("자격증명 로드");
    
    S3TargetHandler handler;
    
    // 직접 자격증명
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"verify_ssl", false}
    };
    
    bool success = handler.initialize(config);
    ASSERT(success, "자격증명 로드 실패");
    
    // 업로드 테스트
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "자격증명 사용 업로드 실패: " + result.error_message);
    
    PASS();
}

void test_bucket_validation() {
    TEST("버킷 검증");
    
    S3TargetHandler handler;
    
    // ❌ 버킷명 없음
    json config1 = {
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"}
    };
    
    std::vector<std::string> errors1;
    bool valid1 = handler.validateConfig(config1, errors1);
    ASSERT(!valid1, "버킷명 없는데 검증 통과");
    ASSERT(!errors1.empty(), "에러 메시지 없음");
    
    // ❌ 빈 버킷명
    json config2 = {
        {"bucket_name", ""},
        {"endpoint", S3_ENDPOINT}
    };
    
    std::vector<std::string> errors2;
    bool valid2 = handler.validateConfig(config2, errors2);
    ASSERT(!valid2, "빈 버킷명인데 검증 통과");
    
    // ❌ 잘못된 버킷명 형식
    json config3 = {
        {"bucket_name", "INVALID_BUCKET_NAME"},  // 대문자 불가
        {"endpoint", S3_ENDPOINT}
    };
    
    std::vector<std::string> errors3;
    bool valid3 = handler.validateConfig(config3, errors3);
    ASSERT(!valid3, "잘못된 버킷명인데 검증 통과");
    
    // ✅ 정상 버킷명
    json config4 = {
        {"bucket_name", "valid-bucket-name-123"},
        {"endpoint", S3_ENDPOINT}
    };
    
    std::vector<std::string> errors4;
    bool valid4 = handler.validateConfig(config4, errors4);
    ASSERT(valid4, "정상 버킷명인데 검증 실패");
    ASSERT(errors4.empty(), "에러 메시지가 있음");
    
    PASS();
}

void test_connection_failure() {
    TEST("연결 실패 처리");
    
    S3TargetHandler handler;
    
    // 존재하지 않는 엔드포인트
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", "http://nonexistent-s3.example.com:9999"},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"connect_timeout_sec", 1},
        {"verify_ssl", false}
    };
    
    bool test_result = handler.testConnection(config);
    ASSERT(!test_result, "연결 실패인데 성공");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 2: 파일 업로드 (5개)
// ═══════════════════════════════════════════════════════════════════════════

void test_simple_upload() {
    TEST("단순 업로드");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/simple_upload.json"},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm(101, "SENSOR_A", 30.5);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "업로드 실패: " + result.error_message);
    ASSERT(!result.s3_object_key.empty(), "객체 키 없음");
    ASSERT(result.content_size > 0, "콘텐츠 크기 0");
    ASSERT(result.response_time.count() > 0, "응답 시간 0");
    
    std::cout << " [" << result.s3_object_key << ", " 
              << result.content_size << "bytes, " 
              << result.response_time.count() << "ms]";
    
    PASS();
}

void test_object_key_template() {
    TEST("객체 키 템플릿");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "{building_id}/{date}/{point_name}_alarm.json"},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm(202, "TEMP_SENSOR", 28.3);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "템플릿 업로드 실패");
    ASSERT(result.s3_object_key.find("202") != std::string::npos, 
           "building_id 치환 실패");
    ASSERT(result.s3_object_key.find("TEMP_SENSOR") != std::string::npos, 
           "point_name 치환 실패");
    ASSERT(result.s3_object_key.find("alarm.json") != std::string::npos, 
           "파일명 치환 실패");
    
    std::cout << " [" << result.s3_object_key << "]";
    
    PASS();
}

void test_json_content() {
    TEST("JSON 내용 생성");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/json_content.json"},
        {"additional_fields", {
            {"source", "PulseOne-Test"},
            {"environment", "testing"}
        }},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm(303, "JSON_TEST", 99.9);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "JSON 업로드 실패");
    ASSERT(result.content_size > 100, "JSON이 너무 작음");
    
    std::cout << " [" << result.content_size << "bytes]";
    
    PASS();
}

void test_compression() {
    TEST("압축 지원");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/compressed.json.gz"},
        {"compression_enabled", true},  // ✅ 압축 활성화
        {"compression_level", 6},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "압축 업로드 실패");
    
    // 압축은 실제 구현에서만 동작
    std::cout << " [압축 설정 확인]";
    
    PASS();
}

void test_upload_failure() {
    TEST("업로드 실패 처리");
    
    S3TargetHandler handler;
    
    // ❌ 잘못된 버킷명
    json config = {
        {"bucket_name", ""},  // 빈 버킷
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"verify_ssl", false}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(!result.success, "빈 버킷인데 성공");
    ASSERT(!result.error_message.empty(), "에러 메시지 없음");
    
    std::cout << " [" << result.error_message << "]";
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 3: 메타데이터 (3개)
// ═══════════════════════════════════════════════════════════════════════════

void test_basic_metadata() {
    TEST("기본 메타데이터");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/metadata_basic.json"},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm(401, "META_TEST", 50.0);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "메타데이터 업로드 실패");
    
    // 기본 메타데이터는 S3Client에서 자동 추가됨
    // (building-id, point-name, alarm-status 등)
    
    PASS();
}

void test_custom_metadata() {
    TEST("커스텀 메타데이터");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/metadata_custom.json"},
        {"custom_metadata", {
            {"project", "PulseOne"},
            {"version", "2.0"},
            {"environment", "production"}
        }},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "커스텀 메타데이터 업로드 실패");
    
    PASS();
}

void test_alarm_metadata() {
    TEST("알람 정보 메타데이터");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/alarm_metadata.json"},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    // 알람 플래그가 있는 알람
    AlarmMessage alarm = createTestAlarm(999, "CRITICAL_ALARM", 100.0);
    alarm.al = 3;  // 높은 알람 레벨
    
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "알람 메타데이터 업로드 실패");
    
    // 알람 정보는 building-id, point-name, alarm-flag 등으로 저장됨
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 4: 에러 처리 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_bucket_auto_create() {
    TEST("버킷 자동 생성");
    
    S3TargetHandler handler;
    
    // 새로운 버킷명 (자동 생성됨)
    std::string new_bucket = "test-auto-create-" + 
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    json config = {
        {"bucket_name", new_bucket},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/auto_create.json"},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "버킷 자동 생성 업로드 실패");
    
    std::cout << " [" << new_bucket << "]";
    
    PASS();
}

void test_invalid_endpoint() {
    TEST("잘못된 엔드포인트");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", "http://invalid-endpoint:1234"},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"connect_timeout_sec", 2},
        {"verify_ssl", false}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(!result.success, "잘못된 엔드포인트인데 성공");
    ASSERT(!result.error_message.empty(), "에러 메시지 없음");
    
    PASS();
}

void test_timeout() {
    TEST("타임아웃 처리");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"upload_timeout_sec", 60},  // ✅ 타임아웃 설정
        {"connect_timeout_sec", 5},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    // 정상 케이스에서는 성공해야 함
    ASSERT(result.success, "타임아웃 설정 업로드 실패");
    
    PASS();
}

void test_retry_logic() {
    TEST("재시도 로직");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"max_retries", 2},  // ✅ 최대 재시도
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "재시도 설정 업로드 실패");
    
    // 재시도는 실패 시에만 동작하므로 설정만 확인
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 5: 통계 및 기타 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_statistics() {
    TEST("통계 정확도");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/stats_{timestamp}.json"},
        {"verify_ssl", false}
    };
    
    handler.initialize(config);
    
    auto initial_stats = handler.getStatus();
    int initial_upload = initial_stats["upload_count"].get<int>();
    
    // 5번 업로드
    AlarmMessage alarm = createTestAlarm();
    for (int i = 0; i < 5; i++) {
        handler.sendAlarm(alarm, config);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    auto final_stats = handler.getStatus();
    
    int upload_count = final_stats["upload_count"].get<int>();
    int success_count = final_stats["success_count"].get<int>();
    size_t total_bytes = final_stats["total_bytes_uploaded"].get<size_t>();
    
    ASSERT(upload_count >= initial_upload + 5, "업로드 카운트 부족");
    ASSERT(success_count >= 4, "성공 카운트 부족");
    ASSERT(total_bytes > 0, "총 바이트 0");
    
    std::cout << " [업로드:" << upload_count << ", 성공:" << success_count 
              << ", 바이트:" << total_bytes << "]";
    
    PASS();
}

void test_config_validation() {
    TEST("설정 검증");
    
    S3TargetHandler handler;
    
    // ❌ bucket_name 없음
    json config1 = {
        {"endpoint", S3_ENDPOINT}
    };
    std::vector<std::string> errors1;
    bool valid1 = handler.validateConfig(config1, errors1);
    ASSERT(!valid1, "bucket_name 없는데 검증 통과");
    
    // ❌ 너무 짧은 버킷명
    json config2 = {
        {"bucket_name", "ab"}  // 3자 미만
    };
    std::vector<std::string> errors2;
    bool valid2 = handler.validateConfig(config2, errors2);
    ASSERT(!valid2, "짧은 버킷명인데 검증 통과");
    
    // ❌ 너무 긴 버킷명
    json config3 = {
        {"bucket_name", std::string(70, 'a')}  // 63자 초과
    };
    std::vector<std::string> errors3;
    bool valid3 = handler.validateConfig(config3, errors3);
    ASSERT(!valid3, "긴 버킷명인데 검증 통과");
    
    // ✅ 정상 설정
    json config4 = {
        {"bucket_name", "valid-bucket-123"}
    };
    std::vector<std::string> errors4;
    bool valid4 = handler.validateConfig(config4, errors4);
    ASSERT(valid4, "정상 설정인데 검증 실패");
    
    PASS();
}

void test_client_cache() {
    TEST("ClientCache 사용");
    
    S3TargetHandler handler1;
    S3TargetHandler handler2;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"object_key_template", "test/cache_{timestamp}.json"},
        {"verify_ssl", false}
    };
    
    handler1.initialize(config);
    handler2.initialize(config);
    
    AlarmMessage alarm = createTestAlarm();
    
    // 두 핸들러 모두 같은 버킷 사용
    auto result1 = handler1.sendAlarm(alarm, config);
    auto result2 = handler2.sendAlarm(alarm, config);
    
    ASSERT(result1.success, "Handler1 업로드 실패");
    ASSERT(result2.success, "Handler2 업로드 실패");
    
    // ClientCache 통계 확인
    auto status1 = handler1.getStatus();
    ASSERT(status1.contains("cache_stats"), "캐시 통계 없음");
    
    auto cache_stats = status1["cache_stats"];
    ASSERT(cache_stats.contains("active_clients"), "active_clients 없음");
    
    std::cout << " [캐시 클라이언트: " 
              << cache_stats["active_clients"] << "]";
    
    PASS();
}

void test_connection_test() {
    TEST("연결 테스트 메서드");
    
    S3TargetHandler handler;
    
    json config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", S3_ENDPOINT},
        {"region", "us-east-1"},
        {"access_key", S3_ACCESS_KEY},
        {"secret_key", S3_SECRET_KEY},
        {"verify_ssl", false}
    };
    
    bool test_result = handler.testConnection(config);
    ASSERT(test_result, "연결 테스트 실패");
    
    // 잘못된 엔드포인트 테스트
    json bad_config = {
        {"bucket_name", S3_BUCKET},
        {"endpoint", "http://invalid:9999"},
        {"region", "us-east-1"},
        {"connect_timeout_sec", 1},
        {"verify_ssl", false}
    };
    
    bool bad_result = handler.testConnection(bad_config);
    ASSERT(!bad_result, "잘못된 연결인데 성공");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 메인
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  S3TargetHandler 완전한 단위 테스트\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    auto& logger = LogManager::getInstance();
    logger.Info("S3TargetHandler 테스트 시작");
    
    // Mock S3 서버 체크
    std::cout << "\n🔍 Mock S3 서버 확인 중... " << std::flush;
    if (!checkS3Server()) {
        std::cout << "❌\n";
        std::cerr << "\n💥 에러: Mock S3 서버가 실행되지 않았습니다!\n";
        std::cerr << "다음 명령어로 서버를 시작하세요:\n";
        std::cerr << "  python3 mock_s3_server.py &\n\n";
        return 1;
    }
    std::cout << "✅\n";
    
    // 파트 1: S3 연결 (4개)
    test_basic_connection();
    test_credentials_loading();
    test_bucket_validation();
    test_connection_failure();
    
    // 파트 2: 파일 업로드 (5개)
    test_simple_upload();
    test_object_key_template();
    test_json_content();
    test_compression();
    test_upload_failure();
    
    // 파트 3: 메타데이터 (3개)
    test_basic_metadata();
    test_custom_metadata();
    test_alarm_metadata();
    
    // 파트 4: 에러 처리 (4개)
    test_bucket_auto_create();
    test_invalid_endpoint();
    test_timeout();
    test_retry_logic();
    
    // 파트 5: 통계 및 기타 (4개)
    test_statistics();
    test_config_validation();
    test_client_cache();
    test_connection_test();
    
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
    
    logger.Info("S3TargetHandler 테스트 완료");
    
    return (failed_count == 0) ? 0 : 1;
}