/**
 * @file AlarmMessage.h
 * @brief CSP Gateway AlarmMessage - C# CSPGateway 완전 포팅 (최종 완성본)
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.3 - 불필요한 부분 제거, 핵심 기능만 유지
 */

#ifndef CSP_ALARM_MESSAGE_H
#define CSP_ALARM_MESSAGE_H

#include <string>
#include <chrono>
#include <vector>
#include <nlohmann/json.hpp>

// PulseOne Shared 라이브러리 조건부 사용
#ifdef HAS_SHARED_LIBS
    #include "Database/Entities/AlarmOccurrenceEntity.h"
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
    // C# 필드 직접 매핑
    int bd = 0;             ///< Building ID
    std::string nm;         ///< Point Name
    double vl = 0.0;        ///< Trigger Value
    std::string tm;         ///< Timestamp (yyyy-MM-dd HH:mm:ss.fff)
    int al = 0;             ///< Alarm Status (1=발생, 0=해제)
    int st = 0;             ///< Alarm State
    std::string des;        ///< Description

    // =======================================================================
    // 핵심 메서드들만 유지
    // =======================================================================
    
    /**
     * @brief JSON으로 변환
     */
    json to_json() const {
        return json{
            {"bd", bd}, {"nm", nm}, {"vl", vl}, 
            {"tm", tm}, {"al", al}, {"st", st}, {"des", des}
        };
    }

    /**
     * @brief JSON에서 로드
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
            return false;
        }
    }

#ifdef HAS_SHARED_LIBS
    /**
     * @brief PulseOne AlarmOccurrence에서 변환
     */
    static AlarmMessage from_alarm_occurrence(
        const Database::Entities::AlarmOccurrenceEntity& occurrence,
        int building_id = -1);
#endif

    /**
     * @brief 현재 시간을 C# 형식으로 변환
     */
    static std::string current_time_to_csharp_format(bool use_local_time = true);

    /**
     * @brief 시간을 C# 형식으로 변환
     */
    static std::string time_to_csharp_format(
        const std::chrono::system_clock::time_point& time_point, 
        bool use_local_time = true);

    /**
     * @brief 테스트용 샘플 생성
     */
    static AlarmMessage create_sample(int building_id, const std::string& point_name, 
                                    double trigger_value, bool is_alarm_active = true);

    /**
     * @brief 유효성 검사
     */
    bool is_valid() const {
        return bd > 0 && !nm.empty() && !tm.empty() && (al == 0 || al == 1);
    }

    /**
     * @brief 알람 상태 문자열
     */
    std::string get_alarm_status_string() const {
        return (al == 1) ? "발생" : "해제";
    }

    /**
     * @brief 디버깅용 문자열
     */
    std::string to_string() const;

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
    // 테스트용 메서드들 (필요시에만 사용)
    // =======================================================================
    
    static std::vector<AlarmMessage> create_test_data();
    static std::string create_test_json_array();
    static bool simulate_csp_api_call(const AlarmMessage& msg, std::string& response);
    bool validate_for_csp_api() const;
    static std::vector<AlarmMessage> create_bulk_test_data(size_t count);
};

// nlohmann::json 자동 변환 지원
inline void to_json(json& j, const AlarmMessage& msg) {
    j = msg.to_json();
}

inline void from_json(const json& j, AlarmMessage& msg) {
    msg.from_json(j);
}

} // namespace CSP
} // namespace PulseOne

#endif // CSP_ALARM_MESSAGE_H