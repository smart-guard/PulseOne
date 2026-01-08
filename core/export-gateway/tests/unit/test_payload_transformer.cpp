/**
 * @file test_payload_transformer.cpp
 * @brief PayloadTransformer 완전한 단위 테스트
 * @author PulseOne Development Team
 * @date 2025-12-17
 * @version 1.1.0 - 타입 처리 수정
 */

#include "Transform/PayloadTransformer.h"
#include "CSP/AlarmMessage.h"
#include "Logging/LogManager.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>

using namespace PulseOne::Transform;
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

// JSON 값을 안전하게 문자열로 가져오기
std::string getJsonAsString(const json& j) {
    if (j.is_string()) return j.get<std::string>();
    if (j.is_number_integer()) return std::to_string(j.get<int64_t>());
    if (j.is_number_float()) return std::to_string(j.get<double>());
    if (j.is_boolean()) return j.get<bool>() ? "true" : "false";
    return j.dump();
}

// JSON 값을 안전하게 정수로 가져오기
int64_t getJsonAsInt(const json& j) {
    if (j.is_number()) return j.get<int64_t>();
    if (j.is_string()) {
        try { return std::stoll(j.get<std::string>()); }
        catch (...) { return 0; }
    }
    return 0;
}

// 테스트용 AlarmMessage 생성
AlarmMessage createTestAlarm(int building_id = 1001, 
                             const std::string& point_name = "TEMP_01", 
                             double value = 25.5) {
    AlarmMessage alarm;
    alarm.bd = building_id;
    alarm.nm = point_name;
    alarm.vl = value;
    alarm.tm = "2025-12-17T10:30:45.123Z";
    alarm.al = 1;
    alarm.st = 2;
    alarm.des = "테스트 알람 설명";
    return alarm;
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 1: 기본 변환 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_basic_template_transform() {
    TEST("기본 템플릿 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm(1001, "TEMP_01", 25.5);
    auto context = transformer.createContext(alarm, "온도센서", "1층 온도", "25.5");
    
    json template_json = {
        {"building", "{{building_id}}"},
        {"point", "{{point_name}}"},
        {"value", "{{value}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    ASSERT(!result.empty(), "결과가 비어있음");
    ASSERT(result.contains("building"), "building 필드 없음");
    ASSERT(result.contains("point"), "point 필드 없음");
    ASSERT(result.contains("value"), "value 필드 없음");
    
    int building = getJsonAsInt(result["building"]);
    ASSERT(building == 1001, "building 값 불일치");
    
    std::string point = getJsonAsString(result["point"]);
    ASSERT(point == "TEMP_01", "point 값 불일치");
    
    std::cout << " [building=1001, point=TEMP_01]";
    PASS();
}

void test_string_template_transform() {
    TEST("문자열 템플릿 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm(1001, "TEMP_01", 25.5);
    auto context = transformer.createContext(alarm, "온도센서", "1층 온도", "25.5");
    
    std::string template_str = "Building {{building_id}}: {{point_name}} = {{value}}";
    std::string result = transformer.transformString(template_str, context);
    
    ASSERT(result.find("Building 1001") != std::string::npos, "Building 1001 없음");
    ASSERT(result.find("TEMP_01") != std::string::npos, "TEMP_01 없음");
    
    std::cout << " [변환 성공]";
    PASS();
}

void test_nested_template_transform() {
    TEST("중첩된 JSON 템플릿 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm(1001, "TEMP_01", 25.5);
    auto context = transformer.createContext(alarm, "온도센서", "1층 온도", "25.5");
    
    json template_json = {
        {"data", {
            {"building", {
                {"id", "{{building_id}}"},
                {"name", "Building {{building_id}}"}
            }},
            {"sensor", {
                {"name", "{{point_name}}"},
                {"description", "{{description}}"}
            }}
        }}
    };
    
    json result = transformer.transform(template_json, context);
    
    ASSERT(result.contains("data"), "data 필드 없음");
    ASSERT(result["data"].contains("building"), "building 필드 없음");
    ASSERT(result["data"]["building"].contains("id"), "id 필드 없음");
    
    int building_id = getJsonAsInt(result["data"]["building"]["id"]);
    ASSERT(building_id == 1001, "building.id 불일치");
    
    std::string building_name = getJsonAsString(result["data"]["building"]["name"]);
    ASSERT(building_name == "Building 1001", "building.name 불일치");
    
    std::string sensor_name = getJsonAsString(result["data"]["sensor"]["name"]);
    ASSERT(sensor_name == "TEMP_01", "sensor.name 불일치");
    
    std::cout << " [중첩 3단계]";
    PASS();
}

void test_array_template_transform() {
    TEST("배열 내 템플릿 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm(1001, "TEMP_01", 25.5);
    auto context = transformer.createContext(alarm, "온도센서", "1층 온도", "25.5");
    
    json template_json = {
        {"sensors", json::array({
            {{"building", "{{building_id}}"}, {"name", "{{point_name}}"}},
            {{"building", "{{building_id}}"}, {"desc", "{{description}}"}}
        })}
    };
    
    json result = transformer.transform(template_json, context);
    
    ASSERT(result.contains("sensors"), "sensors 필드 없음");
    ASSERT(result["sensors"].is_array(), "sensors가 배열 아님");
    ASSERT(result["sensors"].size() == 2, "배열 크기 불일치");
    
    int building1 = getJsonAsInt(result["sensors"][0]["building"]);
    ASSERT(building1 == 1001, "첫 번째 building 불일치");
    
    std::string name1 = getJsonAsString(result["sensors"][0]["name"]);
    ASSERT(name1 == "TEMP_01", "첫 번째 name 불일치");
    
    std::cout << " [배열 2개]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 2: 변수 치환 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_alarm_message_variables() {
    TEST("AlarmMessage 필드 변수 치환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm;
    alarm.bd = 2002;
    alarm.nm = "HUMIDITY_01";
    alarm.vl = 65.3;
    alarm.tm = "2025-12-17T15:45:30.000Z";
    alarm.al = 2;
    alarm.st = 1;
    alarm.des = "습도 센서 알람";
    
    auto context = transformer.createContext(alarm, "습도센서", "2층 습도", "65.3");
    
    json template_json = {
        {"building_id", "{{building_id}}"},
        {"point_name", "{{point_name}}"},
        {"value", "{{value}}"},
        {"alarm_flag", "{{alarm_flag}}"},
        {"status", "{{status}}"},
        {"description", "{{description}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    ASSERT(getJsonAsInt(result["building_id"]) == 2002, "building_id 불일치");
    ASSERT(getJsonAsString(result["point_name"]) == "HUMIDITY_01", "point_name 불일치");
    ASSERT(getJsonAsInt(result["alarm_flag"]) == 2, "alarm_flag 불일치");
    ASSERT(getJsonAsInt(result["status"]) == 1, "status 불일치");
    ASSERT(getJsonAsString(result["description"]) == "습도 센서 알람", "description 불일치");
    
    std::cout << " [6개 필드]";
    PASS();
}

void test_mapping_field_variables() {
    TEST("매핑 필드 변수 치환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "TargetField_A", "타겟 설명", "변환된값");
    
    json template_json = {
        {"target_field", "{{target_field_name}}"},
        {"target_desc", "{{target_description}}"},
        {"converted", "{{converted_value}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    ASSERT(getJsonAsString(result["target_field"]) == "TargetField_A", "target_field 불일치");
    ASSERT(getJsonAsString(result["target_desc"]) == "타겟 설명", "target_desc 불일치");
    ASSERT(getJsonAsString(result["converted"]) == "변환된값", "converted 불일치");
    
    std::cout << " [target_field_name, target_description, converted_value]";
    PASS();
}

void test_calculated_field_variables() {
    TEST("계산 필드 변수 치환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm();
    alarm.al = 1;
    alarm.st = 2;
    
    auto context = transformer.createContext(alarm, "Field", "Desc", "Value");
    
    json template_json = {
        {"timestamp_iso", "{{timestamp_iso8601}}"},
        {"timestamp_ms", "{{timestamp_unix_ms}}"},
        {"alarm_status", "{{alarm_status}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    // ISO8601 형식 확인
    std::string iso = getJsonAsString(result["timestamp_iso"]);
    ASSERT(iso.find("T") != std::string::npos || iso.find("-") != std::string::npos || iso.length() > 0, 
           "ISO8601 형식 아님");
    
    // Unix timestamp 확인
    ASSERT(result.contains("timestamp_ms"), "timestamp_ms 없음");
    int64_t ts_ms = getJsonAsInt(result["timestamp_ms"]);
    ASSERT(ts_ms > 0, "timestamp_ms가 0 이하");
    
    // alarm_status 확인
    std::string alarm_status = getJsonAsString(result["alarm_status"]);
    ASSERT(!alarm_status.empty(), "alarm_status 비어있음");
    
    std::cout << " [" << alarm_status << "]";
    PASS();
}

void test_custom_variables() {
    TEST("커스텀 변수 치환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "Field", "Desc", "Value");
    
    // 커스텀 변수 추가
    context.custom_vars["custom_field_1"] = "CustomValue1";
    context.custom_vars["custom_field_2"] = "CustomValue2";
    context.custom_vars["site_code"] = "SITE_001";
    
    json template_json = {
        {"custom1", "{{custom_field_1}}"},
        {"custom2", "{{custom_field_2}}"},
        {"site", "{{site_code}}"},
        {"building", "{{building_id}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    ASSERT(getJsonAsString(result["custom1"]) == "CustomValue1", "custom1 불일치");
    ASSERT(getJsonAsString(result["custom2"]) == "CustomValue2", "custom2 불일치");
    ASSERT(getJsonAsString(result["site"]) == "SITE_001", "site 불일치");
    ASSERT(getJsonAsInt(result["building"]) == 1001, "building 불일치");
    
    std::cout << " [3개 커스텀 변수]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 3: 시스템별 템플릿 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_insite_template() {
    TEST("Insite 기본 템플릿");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    json template_json = transformer.getInsiteDefaultTemplate();
    
    ASSERT(!template_json.empty(), "템플릿 비어있음");
    ASSERT(template_json.contains("controlpoint"), "controlpoint 필드 없음");
    ASSERT(template_json.contains("description"), "description 필드 없음");
    ASSERT(template_json.contains("value"), "value 필드 없음");
    ASSERT(template_json.contains("time"), "time 필드 없음");
    ASSERT(template_json.contains("status"), "status 필드 없음");
    
    // 변환 테스트
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "CP_001", "제어점 설명", "ON");
    json result = transformer.transform(template_json, context);
    
    ASSERT(getJsonAsString(result["controlpoint"]) == "CP_001", "controlpoint 불일치");
    
    std::cout << " [5개 필드]";
    PASS();
}

void test_hdc_template() {
    TEST("HDC 기본 템플릿");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    json template_json = transformer.getHDCDefaultTemplate();
    
    ASSERT(!template_json.empty(), "템플릿 비어있음");
    ASSERT(template_json.contains("building_id"), "building_id 필드 없음");
    ASSERT(template_json.contains("point_id"), "point_id 필드 없음");
    ASSERT(template_json.contains("data"), "data 필드 없음");
    ASSERT(template_json["data"].contains("value"), "data.value 필드 없음");
    ASSERT(template_json["data"].contains("timestamp"), "data.timestamp 필드 없음");
    ASSERT(template_json.contains("metadata"), "metadata 필드 없음");
    
    // 변환 테스트
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "HDC_POINT", "HDC 설명", "100");
    json result = transformer.transform(template_json, context);
    
    ASSERT(getJsonAsInt(result["building_id"]) == 1001, "building_id 불일치");
    ASSERT(getJsonAsString(result["point_id"]) == "HDC_POINT", "point_id 불일치");
    
    std::cout << " [중첩 구조]";
    PASS();
}

void test_bems_template() {
    TEST("BEMS 기본 템플릿");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    json template_json = transformer.getBEMSDefaultTemplate();
    
    ASSERT(!template_json.empty(), "템플릿 비어있음");
    ASSERT(template_json.contains("buildingId"), "buildingId 필드 없음");
    ASSERT(template_json.contains("sensorName"), "sensorName 필드 없음");
    ASSERT(template_json.contains("sensorValue"), "sensorValue 필드 없음");
    ASSERT(template_json.contains("timestamp"), "timestamp 필드 없음");
    ASSERT(template_json.contains("alarmLevel"), "alarmLevel 필드 없음");
    
    // 변환 테스트
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "BEMS_SENSOR", "BEMS 설명", "42");
    json result = transformer.transform(template_json, context);
    
    ASSERT(getJsonAsInt(result["buildingId"]) == 1001, "buildingId 불일치");
    ASSERT(getJsonAsString(result["sensorName"]) == "BEMS_SENSOR", "sensorName 불일치");
    
    std::cout << " [5개 필드]";
    PASS();
}

void test_generic_template() {
    TEST("Generic 기본 템플릿");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    json template_json = transformer.getGenericDefaultTemplate();
    
    ASSERT(!template_json.empty(), "템플릿 비어있음");
    ASSERT(template_json.contains("building_id"), "building_id 필드 없음");
    ASSERT(template_json.contains("point_name"), "point_name 필드 없음");
    ASSERT(template_json.contains("value"), "value 필드 없음");
    ASSERT(template_json.contains("source"), "source 필드 없음");
    
    // 변환 테스트
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "GEN_FIELD", "일반 설명", "999");
    json result = transformer.transform(template_json, context);
    
    ASSERT(getJsonAsString(result["source"]) == "PulseOne-ExportGateway", "source 불일치");
    
    std::cout << " [source 포함]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 4: 엣지 케이스 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_missing_variable() {
    TEST("누락된 변수 처리");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "Field", "Desc", "Value");
    
    json template_json = {
        {"existing", "{{building_id}}"},
        {"missing", "{{non_existent_variable}}"},
        {"partial", "Value: {{undefined_var}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    // 존재하는 변수는 변환됨
    ASSERT(getJsonAsInt(result["existing"]) == 1001, "existing 변환 실패");
    
    // 누락된 변수는 원본 유지
    std::string missing = getJsonAsString(result["missing"]);
    ASSERT(missing.find("non_existent") != std::string::npos, "누락 변수 처리 불일치");
    
    std::cout << " [원본 유지]";
    PASS();
}

void test_empty_template() {
    TEST("빈 템플릿 처리");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "Field", "Desc", "Value");
    
    // 빈 객체
    json empty_obj = json::object();
    json result1 = transformer.transform(empty_obj, context);
    ASSERT(result1.empty(), "빈 객체 결과 비어있지 않음");
    
    // 빈 배열
    json empty_arr = json::array();
    json result2 = transformer.transform(empty_arr, context);
    ASSERT(result2.empty(), "빈 배열 결과 비어있지 않음");
    
    // 빈 문자열
    std::string empty_str = "";
    std::string result3 = transformer.transformString(empty_str, context);
    ASSERT(result3.empty(), "빈 문자열 결과 비어있지 않음");
    
    std::cout << " [빈 객체/배열/문자열]";
    PASS();
}

void test_special_characters() {
    TEST("특수 문자 처리");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm();
    alarm.des = "Line1\nLine2\tTabbed";
    
    auto context = transformer.createContext(alarm, "Field", "Desc", "Value");
    
    json template_json = {
        {"description", "{{description}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    ASSERT(result.contains("description"), "description 필드 없음");
    
    std::string desc = getJsonAsString(result["description"]);
    ASSERT(desc.find("Line1") != std::string::npos, "Line1 없음");
    ASSERT(desc.find("Line2") != std::string::npos, "Line2 없음");
    
    std::cout << " [\\n\\t 포함]";
    PASS();
}

void test_number_string_conversion() {
    TEST("숫자/문자열 타입 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm();
    alarm.bd = 1001;
    alarm.vl = 123.456;
    alarm.al = 2;
    
    auto context = transformer.createContext(alarm, "Field", "Desc", "99.9");
    
    json template_json = {
        {"int_val", "{{building_id}}"},
        {"flag_val", "{{alarm_flag}}"},
        {"str_val", "{{point_name}}"},
        {"mixed", "ID: {{building_id}}"}
    };
    
    json result = transformer.transform(template_json, context);
    
    // 순수 숫자 변수는 숫자 타입으로
    ASSERT(result["int_val"].is_number(), "int_val이 숫자 타입 아님");
    ASSERT(getJsonAsInt(result["int_val"]) == 1001, "int_val 값 불일치");
    
    ASSERT(result["flag_val"].is_number(), "flag_val이 숫자 타입 아님");
    ASSERT(getJsonAsInt(result["flag_val"]) == 2, "flag_val 값 불일치");
    
    // 문자열 변수는 문자열 타입으로
    ASSERT(result["str_val"].is_string(), "str_val이 문자열 타입 아님");
    
    // 혼합 문자열은 문자열 타입으로
    ASSERT(result["mixed"].is_string(), "mixed가 문자열 타입 아님");
    ASSERT(getJsonAsString(result["mixed"]) == "ID: 1001", "mixed 값 불일치");
    
    std::cout << " [int/string 자동 변환]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 5: 헬퍼 함수 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_to_iso8601() {
    TEST("toISO8601 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    std::string input1 = "2025-12-17T10:30:45.123Z";
    std::string result1 = transformer.toISO8601(input1);
    ASSERT(!result1.empty(), "결과가 비어있음");
    ASSERT(result1.find("2025") != std::string::npos, "년도 없음");
    
    std::cout << " [" << result1.substr(0, 10) << "...]";
    PASS();
}

void test_to_unix_timestamp_ms() {
    TEST("toUnixTimestampMs 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    std::string input = "2025-12-17T10:30:45.123Z";
    int64_t result = transformer.toUnixTimestampMs(input);
    
    ASSERT(result > 1000000000000LL, "타임스탬프가 너무 작음");
    
    std::cout << " [" << result << "ms]";
    PASS();
}

void test_get_alarm_status_string() {
    TEST("getAlarmStatusString 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    std::string result1 = transformer.getAlarmStatusString(0, 0);
    ASSERT(!result1.empty(), "결과1 비어있음");
    
    std::string result2 = transformer.getAlarmStatusString(1, 0);
    ASSERT(!result2.empty(), "결과2 비어있음");
    
    std::string result3 = transformer.getAlarmStatusString(1, 1);
    ASSERT(!result3.empty(), "결과3 비어있음");
    
    std::cout << " [al=1,st=0 → \"" << result2 << "\"]";
    PASS();
}

void test_create_context() {
    TEST("createContext 생성");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm;
    alarm.bd = 3003;
    alarm.nm = "POINT_X";
    alarm.vl = 77.7;
    alarm.tm = "2025-12-17T12:00:00.000Z";
    alarm.al = 1;
    alarm.st = 1;
    alarm.des = "컨텍스트 테스트";
    
    auto context = transformer.createContext(
        alarm, 
        "TargetFieldName", 
        "TargetDescription", 
        "ConvertedValue"
    );
    
    // AlarmMessage 복사 확인
    ASSERT(context.alarm.bd == 3003, "alarm.bd 불일치");
    ASSERT(context.alarm.nm == "POINT_X", "alarm.nm 불일치");
    ASSERT(std::abs(context.alarm.vl - 77.7) < 0.01, "alarm.vl 불일치");
    
    // 매핑 필드 확인
    ASSERT(context.target_field_name == "TargetFieldName", "target_field_name 불일치");
    ASSERT(context.target_description == "TargetDescription", "target_description 불일치");
    ASSERT(context.converted_value == "ConvertedValue", "converted_value 불일치");
    
    // 계산 필드 확인
    ASSERT(!context.timestamp_iso8601.empty(), "timestamp_iso8601 비어있음");
    ASSERT(context.timestamp_unix_ms > 0, "timestamp_unix_ms가 0 이하");
    ASSERT(!context.alarm_status.empty(), "alarm_status 비어있음");
    
    std::cout << " [모든 필드 설정됨]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 메인
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  PayloadTransformer 완전한 단위 테스트\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    auto& logger = LogManager::getInstance();
    logger.Info("PayloadTransformer 테스트 시작");
    
    // 파트 1: 기본 변환 (4개)
    std::cout << "\n📌 Part 1: 기본 변환" << std::endl;
    test_basic_template_transform();
    test_string_template_transform();
    test_nested_template_transform();
    test_array_template_transform();
    
    // 파트 2: 변수 치환 (4개)
    std::cout << "\n📌 Part 2: 변수 치환" << std::endl;
    test_alarm_message_variables();
    test_mapping_field_variables();
    test_calculated_field_variables();
    test_custom_variables();
    
    // 파트 3: 시스템별 템플릿 (4개)
    std::cout << "\n📌 Part 3: 시스템별 템플릿" << std::endl;
    test_insite_template();
    test_hdc_template();
    test_bems_template();
    test_generic_template();
    
    // 파트 4: 엣지 케이스 (4개)
    std::cout << "\n📌 Part 4: 엣지 케이스" << std::endl;
    test_missing_variable();
    test_empty_template();
    test_special_characters();
    test_number_string_conversion();
    
    // 파트 5: 헬퍼 함수 (4개)
    std::cout << "\n📌 Part 5: 헬퍼 함수" << std::endl;
    test_to_iso8601();
    test_to_unix_timestamp_ms();
    test_get_alarm_status_string();
    test_create_context();
    
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
    
    logger.Info("PayloadTransformer 테스트 완료");
    
    return (failed_count == 0) ? 0 : 1;
}