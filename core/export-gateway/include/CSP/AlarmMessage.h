/**
 * @file AlarmMessage.h
 * @brief CSP Gateway AlarmMessage - C# CSPGateway 완전 포팅 (완성본)
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.2 - 모든 메서드 선언 포함
 * 
 * C# AlarmMessage 구조체 완전 포팅:
 * - bd: Building ID (int)
 * - nm: Point Name (string) 
 * - vl: Trigger Value (double)
 * - tm: Timestamp (string, yyyy-MM-dd HH:mm:ss.fff)
 * - al: Alarm Status (int, 1=발생, 0=해제)
 * - st: Alarm State (int)
 * - des: Description (string)
 */

#ifndef CSP_ALARM_MESSAGE_H
#define CSP_ALARM_MESSAGE_H

#include <string>
#include <chrono>
#include <vector>
#include <optional>
#include <limits>
#include <nlohmann/json.hpp>

// PulseOne Shared 라이브러리 사용 (collector 패턴)
#ifdef HAS_SHARED_LIBS
    // Shared 라이브러리 Entity들 사용
    #include "Database/Entities/AlarmOccurrenceEntity.h"
    #include "Database/Entities/AlarmRuleEntity.h"
    #include "Database/Entities/DataPointEntity.h"
    #include "Database/Entities/DeviceEntity.h"
    #include "Alarm/AlarmTypes.h"
    
    // Logger 사용 (DEBUG 매크로 충돌 방지)
    #ifndef PULSEONE_DEBUG
        #define PULSEONE_DEBUG 0
    #endif
    
    // LogManager는 조건부 사용 (오류 방지)
    #ifdef HAVE_LOG_MANAGER
        #include "Utils/LogManager.h"
        #define LOG_ERROR(msg) LogManager::getInstance().Error(msg)
        #define LOG_DEBUG(msg) LogManager::getInstance().Debug(msg)
    #else
        #define LOG_ERROR(msg) // 로깅 비활성화
        #define LOG_DEBUG(msg) // 로깅 비활성화
    #endif
#endif

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

/**
 * @brief CSP Gateway AlarmMessage 구조체
 * 
 * C# 원본과 100% 호환:
 * ```csharp
 * public class AlarmMessage {
 *     public int bd;        // Building ID
 *     public string nm;     // Point Name
 *     public double vl;     // Trigger Value
 *     public string tm;     // Timestamp
 *     public int al;        // Alarm Status (1=발생, 0=해제)
 *     public int st;        // Alarm State
 *     public string des;    // Description
 * }
 * ```
 */
struct AlarmMessage {
    // =======================================================================
    // C# 필드와 정확히 일치
    // =======================================================================
    
    int bd = 0;             ///< Building ID (빌딩 식별자)
    std::string nm;         ///< Point Name (포인트명)
    double vl = 0.0;        ///< Trigger Value (트리거 값)
    std::string tm;         ///< Timestamp (yyyy-MM-dd HH:mm:ss.fff 형식)
    int al = 0;             ///< Alarm Status (1=발생, 0=해제)
    int st = 0;             ///< Alarm State (알람 상태)
    std::string des;        ///< Description (설명/한계 텍스트)

    // =======================================================================
    // 생성자들
    // =======================================================================
    
    /**
     * @brief 기본 생성자
     */
    AlarmMessage() = default;

    /**
     * @brief 전체 필드 생성자
     */
    AlarmMessage(int building_id, const std::string& point_name, double trigger_value,
                const std::string& timestamp, int alarm_status, int alarm_state, 
                const std::string& description)
        : bd(building_id), nm(point_name), vl(trigger_value), tm(timestamp),
          al(alarm_status), st(alarm_state), des(description) {}

    // =======================================================================
    // JSON 직렬화/역직렬화 (C# Json.GetString() 호환)
    // =======================================================================
    
    /**
     * @brief JSON으로 변환 (C# Json.GetString() 동일)
     * @return JSON 객체
     */
    json to_json() const {
        return json{
            {"bd", bd},
            {"nm", nm},
            {"vl", vl},
            {"tm", tm},
            {"al", al},
            {"st", st},
            {"des", des}
        };
    }

    /**
     * @brief JSON 문자열로 변환
     * @return JSON 문자열
     */
    std::string to_json_string() const {
        return to_json().dump();
    }

    /**
     * @brief JSON에서 로드
     * @param j JSON 객체
     * @return 성공 여부
     */
    bool from_json(const json& j) {
        try {
            bd = j.value("bd", 0);
            nm = j.value("nm", "");
            vl = j.value("vl", 0.0);
            tm = j.value("tm", "");
            al = j.value("al", 0);
            st = j.value("st", 0);
            des = j.value("des", "");
            return true;
        } catch (const std::exception&) {
            LOG_ERROR("AlarmMessage::from_json failed");
            return false;
        }
    }

    /**
     * @brief JSON 문자열에서 로드
     * @param json_str JSON 문자열
     * @return 성공 여부
     */
    bool from_json_string(const std::string& json_str) {
        try {
            json j = json::parse(json_str);
            return from_json(j);
        } catch (const std::exception&) {
            LOG_ERROR("AlarmMessage::from_json_string failed");
            return false;
        }
    }

    // =======================================================================
    // PulseOne Entity 변환 메서드들 (Shared 라이브러리 사용 시)
    // =======================================================================
    
#ifdef HAS_SHARED_LIBS
    /**
     * @brief PulseOne AlarmOccurrence에서 AlarmMessage 생성
     * @param occurrence 알람 발생 엔티티
     * @param building_id 빌딩 ID (기본값: tenant_id 사용)
     * @return AlarmMessage 인스턴스
     */
    static AlarmMessage from_alarm_occurrence(
        const Database::Entities::AlarmOccurrenceEntity& occurrence,
        int building_id = -1);
#endif

    // =======================================================================
    // 시간 변환 유틸리티 (C# 형식 호환)
    // =======================================================================
    
    /**
     * @brief 현재 시간을 C# 형식으로 변환
     * @param use_local_time true=로컬시간, false=UTC
     * @return yyyy-MM-dd HH:mm:ss.fff 형식 문자열
     */
    static std::string current_time_to_csharp_format(bool use_local_time = true);

    /**
     * @brief 시간을 C# 형식으로 변환
     * @param time_point 변환할 시간
     * @param use_local_time true=로컬시간, false=UTC
     * @return yyyy-MM-dd HH:mm:ss.fff 형식 문자열
     */
    static std::string time_to_csharp_format(const std::chrono::system_clock::time_point& time_point, 
                                            bool use_local_time = true);

    // =======================================================================
    // 기본 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 유효성 검사
     * @return 유효하면 true
     */
    bool is_valid() const {
        return bd > 0 && !nm.empty() && !tm.empty() && (al == 0 || al == 1);
    }

    /**
     * @brief 알람 상태 문자열 변환
     * @return "발생" 또는 "해제"
     */
    std::string get_alarm_status_string() const {
        return (al == 1) ? "발생" : "해제";
    }

    /**
     * @brief 디버깅용 문자열 표현
     * @return 읽기 쉬운 문자열
     */
    std::string to_string() const;

    /**
     * @brief C# CSPGateway 스타일 샘플 생성 (테스트용)
     * @param building_id 빌딩 ID
     * @param point_name 포인트명
     * @param trigger_value 트리거 값
     * @param is_alarm_active true=발생, false=해제
     * @return 샘플 AlarmMessage
     */
    static AlarmMessage create_sample(int building_id, const std::string& point_name, 
                                    double trigger_value, bool is_alarm_active = true);

    /**
     * @brief 동등성 비교
     */
    bool operator==(const AlarmMessage& other) const {
        return bd == other.bd && nm == other.nm && vl == other.vl && 
               tm == other.tm && al == other.al && st == other.st && des == other.des;
    }

    bool operator!=(const AlarmMessage& other) const {
        return !(*this == other);
    }

    // =======================================================================
    // 테스트 및 시뮬레이션 메서드들 (AlarmMessage.cpp에서 구현됨)
    // =======================================================================
    
    /**
     * @brief C# CSPGateway 스타일 테스트 데이터 생성
     * @return 테스트용 AlarmMessage 벡터
     */
    static std::vector<AlarmMessage> create_test_data();

    /**
     * @brief JSON 배열로 테스트 데이터 출력
     * @return JSON 배열 문자열
     */
    static std::string create_test_json_array();

    /**
     * @brief CSP API 호출 시뮬레이션
     * @param msg 전송할 AlarmMessage
     * @param response API 응답 (출력 매개변수)
     * @return 성공 여부
     */
    static bool simulate_csp_api_call(const AlarmMessage& msg, std::string& response);

    /**
     * @brief CSP API 전송을 위한 유효성 검사
     * @return 유효하면 true
     */
    bool validate_for_csp_api() const;

    /**
     * @brief 성능 테스트용 대량 데이터 생성
     * @param count 생성할 개수
     * @return 대량 테스트 데이터
     */
    static std::vector<AlarmMessage> create_bulk_test_data(size_t count);
};

// =======================================================================
// nlohmann::json 지원 (자동 변환)
// =======================================================================

inline void to_json(json& j, const AlarmMessage& msg) {
    j = msg.to_json();
}

inline void from_json(const json& j, AlarmMessage& msg) {
    msg.from_json(j);
}

} // namespace CSP
} // namespace PulseOne

#endif // CSP_ALARM_MESSAGE_H