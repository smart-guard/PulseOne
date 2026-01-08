/**
 * @file AlarmMessage.cpp
 * @brief CSP Gateway AlarmMessage 구현 - C# CSPGateway 완전 포팅
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.2 - LOG 매크로 문제 해결
 */

#include "CSP/AlarmMessage.h"
#include <sstream>
#include <iomanip>

#ifdef HAS_SHARED_LIBS
    // Shared 라이브러리가 있을 때만 Entity 변환 구현
    #include "Database/Entities/AlarmOccurrenceEntity.h"
    #include "Database/Entities/AlarmRuleEntity.h"
    #include "Database/Entities/DataPointEntity.h"
    #include "Database/Entities/DeviceEntity.h"
    #include "Logging/LogManager.h"  // LogManager 직접 사용
#endif

namespace PulseOne {
namespace CSP {

#ifdef HAS_SHARED_LIBS
/**
 * @brief PulseOne AlarmOccurrence에서 AlarmMessage 생성
 */
AlarmMessage AlarmMessage::from_alarm_occurrence(
    const Database::Entities::AlarmOccurrenceEntity& occurrence,
    int building_id) {
    
    AlarmMessage msg;
    
    try {
        // =======================================================================
        // 1. Building ID 설정 (C# CSPGateway와 동일)
        // =======================================================================
        
        if (building_id > 0) {
            msg.bd = building_id;
        } else {
            // 기본값: tenant_id 사용 (C# config.BuildingID와 유사)
            msg.bd = occurrence.getTenantId();
        }
        
        // =======================================================================
        // 2. Point Name 설정 (C# onlineAlarm.VariableName와 유사)
        // =======================================================================
        
        // source_name이 있으면 사용, 없으면 기본 이름 생성
        if (!occurrence.getSourceName().empty()) {
            msg.nm = occurrence.getSourceName();
        } else {
            // rule_id와 point 정보로 이름 생성
            msg.nm = "Alarm_" + std::to_string(occurrence.getRuleId());
            
            // device_id나 point_id가 있으면 추가
            if (occurrence.getDeviceId().has_value()) {
                msg.nm += "_Dev" + std::to_string(occurrence.getDeviceId().value());
            }
            if (occurrence.getPointId().has_value()) {
                msg.nm += "_Point" + std::to_string(occurrence.getPointId().value());
            }
        }
        
        // =======================================================================
        // 3. Trigger Value 설정 (C# onlineAlarm.Value)
        // =======================================================================
        
        if (!occurrence.getTriggerValue().empty()) {
            try {
                msg.vl = std::stod(occurrence.getTriggerValue());
            } catch (const std::exception&) {
                // 숫자 변환 실패 시 C# double.MaxValue 사용
                msg.vl = std::numeric_limits<double>::max();
                LogManager::getInstance().Debug("Failed to parse trigger_value: " + occurrence.getTriggerValue());
            }
        } else {
            msg.vl = 0.0;
        }
        
        // =======================================================================
        // 4. Timestamp 설정 (C# 시간 형식 완전 호환)
        // =======================================================================
        
        // occurrence_time을 C# 형식으로 변환
        msg.tm = time_to_csharp_format(occurrence.getOccurrenceTime(), true); // 로컬시간 사용
        
        // =======================================================================
        // 5. Alarm Status 설정 (C# ReasonType 로직)
        // =======================================================================
        
        // C# switch 로직 포팅:
        // case OnlineAlarmReasonType.Received: al = 1 (발생)
        // case OnlineAlarmReasonType.Cleared: al = 0 (해제)
        
        auto state = occurrence.getState();
        if (state == PulseOne::Alarm::AlarmState::ACTIVE) {
            msg.al = 1; // 발생
        } else if (state == PulseOne::Alarm::AlarmState::CLEARED) {
            msg.al = 0; // 해제
        } else {
            // 기타 상태 (ACKNOWLEDGED 등)는 발생으로 처리
            msg.al = 1;
        }
        
        // =======================================================================
        // 6. Alarm State 설정 (C# Status & 0x40000 로직 근사)
        // =======================================================================
        
        // C# 로직: (onlineAlarm.Status & 0x40000) == 0 ? 1 : 0
        // PulseOne에서는 acknowledged 상태로 판단
        if (occurrence.getAcknowledgedTime().has_value()) {
            msg.st = 0; // acknowledged
        } else {
            msg.st = 1; // not acknowledged
        }
        
        // =======================================================================
        // 7. Description 설정 (C# LimitText)
        // =======================================================================
        
        if (!occurrence.getAlarmMessage().empty()) {
            msg.des = occurrence.getAlarmMessage();
        } else {
            // 기본 설명 생성
            std::string severity_str = (occurrence.getSeverity() == PulseOne::Alarm::AlarmSeverity::CRITICAL) ? "CRITICAL" :
                                     (occurrence.getSeverity() == PulseOne::Alarm::AlarmSeverity::HIGH) ? "HIGH" :
                                     (occurrence.getSeverity() == PulseOne::Alarm::AlarmSeverity::MEDIUM) ? "MEDIUM" : "LOW";
            
            msg.des = severity_str + " 알람 " + msg.get_alarm_status_string();
        }
        
        LogManager::getInstance().Debug("AlarmMessage created: " + msg.to_string());
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception in from_alarm_occurrence: " + std::string(e.what()));
        
        // 예외 발생 시 최소한의 유효한 메시지 생성
        msg.bd = building_id > 0 ? building_id : 1001;
        msg.nm = "Error_Alarm";
        msg.vl = 0.0;
        msg.tm = current_time_to_csharp_format(true);
        msg.al = 1;
        msg.st = 1;
        msg.des = "알람 변환 오류";
    }
    
    return msg;
}

#endif // HAS_SHARED_LIBS

// =============================================================================
// 테스트 및 유틸리티 메서드들 (Shared 라이브러리 없이도 동작)
// =============================================================================

/**
 * @brief C# CSPGateway 스타일 테스트 데이터 생성
 */
std::vector<AlarmMessage> AlarmMessage::create_test_data() {
    std::vector<AlarmMessage> test_messages;
    
    // 테스트 케이스 1: Tank Level High 알람 (발생)
    auto msg1 = create_sample(1001, "Tank001.Level", 85.5, true);
    msg1.des = "Tank Level High - 85.5% (한계: 80%)";
    test_messages.push_back(msg1);
    
    // 테스트 케이스 2: Pump Status 알람 (해제)
    auto msg2 = create_sample(1001, "Pump001.Status", 0.0, false);
    msg2.des = "Pump Status Normal";
    test_messages.push_back(msg2);
    
    // 테스트 케이스 3: Temperature High 알람 (Critical)
    auto msg3 = create_sample(1002, "Reactor.Temperature", 120.0, true);
    msg3.des = "CRITICAL Temperature High - 120°C (한계: 100°C)";
    test_messages.push_back(msg3);
    
    // 테스트 케이스 4: 멀티 빌딩 테스트 (Building 1003)
    auto msg4 = create_sample(1003, "HVAC.Airflow", 45.2, true);
    msg4.des = "HVAC Airflow Low - 45.2 m³/h";
    test_messages.push_back(msg4);
    
    return test_messages;
}

/**
 * @brief JSON 배열로 테스트 데이터 출력 (C# API 호출 형식)
 */
std::string AlarmMessage::create_test_json_array() {
    auto test_data = create_test_data();
    json j_array = json::array();
    
    for (const auto& msg : test_data) {
        j_array.push_back(msg.to_json());
    }
    
    return j_array.dump(2); // 보기 좋게 포맷팅
}

/**
 * @brief C# CSPGateway API 호출 시뮬레이션
 */
bool AlarmMessage::simulate_csp_api_call(const AlarmMessage& msg, std::string& response) {
    try {
        // C# JSON 포맷으로 시뮬레이션
        json request = msg.to_json();
        
        // API 응답 시뮬레이션 (C# ReturnMessage 형식)
        json api_response = {
            {"statusCode", 200},
            {"Status", "OK"},
            {"Message", "Alarm processed successfully"},
            {"Data", request}
        };
        
        response = api_response.dump();
        
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Debug("API Call simulated for: " + msg.nm + "/" + msg.get_alarm_status_string());
#endif
        return true;
        
    } catch (const std::exception& e) {
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Error("API simulation failed: " + std::string(e.what()));
#endif
        
        // 실패 응답 생성
        json error_response = {
            {"statusCode", 500},
            {"Status", "ERROR"}, 
            {"Message", "API call failed"},
            {"Error", e.what()}
        };
        
        response = error_response.dump();
        return false;
    }
}

/**
 * @brief AlarmMessage 검증 (C# 유효성 검사 로직)
 */
bool AlarmMessage::validate_for_csp_api() const {
    // C# 필수 필드 검증
    if (bd <= 0) {
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Error("Invalid building_id: " + std::to_string(bd));
#endif
        return false;
    }
    
    if (nm.empty()) {
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Error("Point name is empty");
#endif
        return false;
    }
    
    if (tm.empty()) {
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Error("Timestamp is empty");
#endif
        return false;
    }
    
    // 알람 상태 유효성 (0 또는 1만 허용)
    if (al != 0 && al != 1) {
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Error("Invalid alarm status: " + std::to_string(al));
#endif
        return false;
    }
    
    // C# DateTime 형식 검증 (간단한 정규식 체크)
    if (tm.length() < 23) { // yyyy-MM-dd HH:mm:ss.fff 최소 길이
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Error("Invalid timestamp format: " + tm);
#endif
        return false;
    }
    
    return true;
}

/**
 * @brief 성능 테스트용 대량 데이터 생성
 */
std::vector<AlarmMessage> AlarmMessage::create_bulk_test_data(size_t count) {
    std::vector<AlarmMessage> bulk_data;
    bulk_data.reserve(count);
    
    const std::vector<std::string> point_names = {
        "Tank.Level", "Pump.Status", "Valve.Position", "Sensor.Temperature", 
        "Motor.Speed", "Flow.Rate", "Pressure.Reading", "HVAC.Status"
    };
    
    const std::vector<int> building_ids = {1001, 1002, 1003, 1004, 1005};
    
    for (size_t i = 0; i < count; ++i) {
        int building_id = building_ids[i % building_ids.size()];
        std::string point_name = point_names[i % point_names.size()] + 
                                std::to_string((i / point_names.size()) + 1);
        double trigger_value = 50.0 + (i % 100); // 50-149 범위
        bool is_active = (i % 3) != 0; // 약 67% 활성 알람
        
        auto msg = create_sample(building_id, point_name, trigger_value, is_active);
        msg.des = "Bulk test alarm #" + std::to_string(i + 1);
        
        bulk_data.push_back(msg);
    }
    
    return bulk_data;
}

/**
 * @brief 현재 시간을 C# 형식으로 변환
 */
std::string AlarmMessage::current_time_to_csharp_format(bool use_local_time) {
    auto now = std::chrono::system_clock::now();
    return time_to_csharp_format(now, use_local_time);
}

/**
 * @brief 시간을 C# 형식으로 변환 (yyyy-MM-dd HH:mm:ss.fff)
 */
std::string AlarmMessage::time_to_csharp_format(const std::chrono::system_clock::time_point& time_point, 
                                              bool use_local_time) {
    // 시간을 time_t와 밀리초로 분리
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch()) % 1000;
    
    // tm 구조체로 변환
    std::tm tm_buf;
    if (use_local_time) {
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif
    } else {
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t);
#else
        gmtime_r(&time_t, &tm_buf);
#endif
    }
    
    // C# DateTime 형식으로 포맷팅: yyyy-MM-dd HH:mm:ss.fff
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

/**
 * @brief C# CSPGateway 스타일 샘플 생성
 */
AlarmMessage AlarmMessage::create_sample(int building_id, const std::string& point_name, 
                                       double trigger_value, bool is_alarm_active) {
    AlarmMessage msg;
    
    // C# 필드 직접 매핑
    msg.bd = building_id;
    msg.nm = point_name;
    msg.vl = trigger_value;
    msg.tm = current_time_to_csharp_format(true); // 로컬 시간 사용
    msg.al = is_alarm_active ? 1 : 0;
    msg.st = is_alarm_active ? 1 : 0; // 간단한 상태 매핑
    
    // 기본 설명 생성
    if (is_alarm_active) {
        msg.des = point_name + " 알람 발생 (값: " + std::to_string(trigger_value) + ")";
    } else {
        msg.des = point_name + " 알람 해제 (값: " + std::to_string(trigger_value) + ")";
    }
    
    return msg;
}

/**
 * @brief 디버깅용 문자열 표현
 */
std::string AlarmMessage::to_string() const {
    std::ostringstream oss;
    oss << "AlarmMessage{";
    oss << "bd=" << bd;
    oss << ", nm='" << nm << "'";
    oss << ", vl=" << vl;
    oss << ", tm='" << tm << "'";
    oss << ", al=" << al;
    oss << ", st=" << st;
    oss << ", des='" << des << "'";
    oss << "}";
    return oss.str();
}

} // namespace CSP
} // namespace PulseOne