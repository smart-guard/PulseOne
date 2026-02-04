/**
 * @file AlarmMessage.h
 * @brief CSP Gateway AlarmMessage - C# CSPGateway 완전 포팅 (최종 완성본)
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.3 - 불필요한 부분 제거, 핵심 기능만 유지
 */

#ifndef CSP_ALARM_MESSAGE_H
#define CSP_ALARM_MESSAGE_H

#include <chrono>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

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
  int bd = 101;                ///< Building ID (default 101)
  std::string ty = "num";      ///< Type (num, bit 등)
  std::string nm;              ///< Point Name
  double vl = 0.0;             ///< Value
  std::string il = "";         ///< Info Limit (empty)
  std::string xl = "";         ///< Extra Limit (empty)
  std::vector<double> mi = {}; ///< Min (empty array)
  std::vector<double> mx = {}; ///< Max (empty array)
  std::string tm;              ///< Timestamp (yyyy-MM-dd HH:mm:ss.fff)
  int st = 1;                  ///< Comm Status (1: Normal)
  int al = 0;                  ///< Alarm Status (1: Occur, 0: Clear)
  std::string des;             ///< Description

  // Internal Logic Fields (Not exported to final JSON)
  int point_id = 0;
  int site_id = 0;
  json extra_info = json::object(); ///< Any other fields from input

  // =======================================================================
  // 핵심 메서드들만 유지
  // =======================================================================

  /**
   * @brief JSON으로 변환
   */
  json to_json() const {
    // User requested format:
    // [{"bd":9,"ty":"num","nm":"...","vl":1,"il":"-","xl":"1","mi":[0],"mx":[1],"tm":"...","st":1,"al":1,"des":"..."}]
    return json{{"bd", bd}, {"ty", ty}, {"nm", nm}, {"vl", vl},
                {"il", il}, {"xl", xl}, {"mi", mi}, {"mx", mx},
                {"tm", tm}, {"st", st}, {"al", al}, {"des", des}};
  }

  /**
   * @brief JSON에서 로드
   */
  bool from_json(const json &j) {
    try {
      // 1. Building/Tenant ID
      if (j.contains("bd"))
        bd = j["bd"].is_number() ? j["bd"].get<int>()
                                 : std::stoi(j["bd"].get<std::string>());
      else if (j.contains("site_id"))
        bd = j["site_id"].is_number()
                 ? j["site_id"].get<int>()
                 : std::stoi(j["site_id"].get<std::string>());
      else if (j.contains("tenant_id"))
        bd = j["tenant_id"].is_number()
                 ? j["tenant_id"].get<int>()
                 : std::stoi(j["tenant_id"].get<std::string>());

      // 2. Name
      if (j.contains("nm"))
        nm = j["nm"].get<std::string>();
      else if (j.contains("source_name"))
        nm = j["source_name"].get<std::string>();

      // 3. Value
      if (j.contains("vl")) {
        if (j["vl"].is_number())
          vl = j["vl"].get<double>();
      } else if (j.contains("val")) {
        if (j["val"].is_number())
          vl = j["val"].get<double>();
      } else if (j.contains("trigger_value")) {
        if (j["trigger_value"].is_number())
          vl = j["trigger_value"].get<double>();
        else if (j["trigger_value"].is_string()) {
          try {
            vl = std::stod(j["trigger_value"].get<std::string>());
          } catch (...) {
            vl = 0.0;
          }
        }
      }

      // 4. Timestamp
      if (j.contains("tm"))
        tm = j["tm"].get<std::string>();
      else if (j.contains("timestamp")) {
        if (j["timestamp"].is_string()) {
          tm = j["timestamp"].get<std::string>();
        } else if (j["timestamp"].is_number()) {
          // MS timestamp (int64) -> yyyy-MM-dd HH:mm:ss.fff
          int64_t ms = j["timestamp"].get<int64_t>();
          auto tp = std::chrono::system_clock::time_point(
              std::chrono::milliseconds(ms));
          tm = time_to_csharp_format(tp, true);
        }
      }

      // 5. Status/State
      if (j.contains("al")) {
        if (j["al"].is_number())
          al = j["al"].get<int>();
        else {
          std::string s = j["al"].get<std::string>();
          if (s == "ALARM" || s == "1" || s == "active" || s == "ACTIVE")
            al = 1;
          else
            al = 0;
        }
      } else if (j.contains("state")) {
        std::string s = j["state"].get<std::string>();
        if (s == "ALARM" || s == "1" || s == "active" || s == "ACTIVE")
          al = 1;
        else
          al = 0;
      }

      if (j.contains("st")) {
        if (j["st"].is_number())
          st = j["st"].get<int>();
      }

      // 6. Description
      if (j.contains("des"))
        des = j["des"].get<std::string>();
      else if (j.contains("message"))
        des = j["message"].get<std::string>();

      // 7. Internal Mapping Inputs
      if (j.contains("site_id"))
        site_id = j["site_id"].get<int>();
      if (j.contains("point_id"))
        point_id = j["point_id"].get<int>();

      // Optional: Parse extra fields if they exist in input, otherwise keep
      // defaults
      if (j.contains("ty"))
        ty = j["ty"].get<std::string>();
      if (j.contains("il"))
        il = j["il"].get<std::string>();
      if (j.contains("xl"))
        xl = j["xl"].get<std::string>();

      // 8. Capture ALL other fields for dynamic pass-through
      if (j.contains("extra_info") && j["extra_info"].is_object()) {
        for (auto &el : j["extra_info"].items()) {
          extra_info[el.key()] = el.value();
        }
      }

      for (auto it = j.begin(); it != j.end(); ++it) {
        // Skip already mapped fields
        static const std::set<std::string> known_fields = {
            "bd", "site_id",   "nm", "source_name", "vl",  "val",
            "tm", "timestamp", "al", "st",          "des", "point_id",
            "ty", "il",        "xl", "extra_info"};

        if (known_fields.find(it.key()) == known_fields.end()) {
          if (it.value().is_primitive()) {
            extra_info[it.key()] = it.value();
          } else {
            extra_info[it.key()] = it.value().dump(); // Flatten complex objects
          }
        }
      }

      return true;
    } catch (const std::exception &e) {
      return false;
    }
  }

#ifdef HAS_SHARED_LIBS
  /**
   * @brief PulseOne AlarmOccurrence에서 변환
   */
  static AlarmMessage from_alarm_occurrence(
      const Database::Entities::AlarmOccurrenceEntity &occurrence,
      int building_id = -1);
#endif

  /**
   * @brief 현재 시간을 C# 형식으로 변환
   */
  static std::string current_time_to_csharp_format(bool use_local_time = true);

  /**
   * @brief 시간을 C# 형식으로 변환
   */
  static std::string
  time_to_csharp_format(const std::chrono::system_clock::time_point &time_point,
                        bool use_local_time = true);

  /**
   * @brief 테스트용 샘플 생성
   */
  static AlarmMessage create_sample(int building_id,
                                    const std::string &point_name,
                                    double trigger_value,
                                    bool is_alarm_active = true);

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
  bool operator==(const AlarmMessage &other) const {
    return bd == other.bd && nm == other.nm && vl == other.vl &&
           tm == other.tm && al == other.al && st == other.st &&
           des == other.des;
  }

  bool operator!=(const AlarmMessage &other) const { return !(*this == other); }

  // =======================================================================
  // 테스트용 메서드들 (필요시에만 사용)
  // =======================================================================

  static std::vector<AlarmMessage> create_test_data();
  static std::string create_test_json_array();
  static bool simulate_csp_api_call(const AlarmMessage &msg,
                                    std::string &response);
  bool validate_for_csp_api() const;
  static std::vector<AlarmMessage> create_bulk_test_data(size_t count);
};

// nlohmann::json 자동 변환 지원
inline void to_json(json &j, const AlarmMessage &msg) { j = msg.to_json(); }

inline void from_json(const json &j, AlarmMessage &msg) { msg.from_json(j); }

} // namespace CSP
} // namespace PulseOne

#endif // CSP_ALARM_MESSAGE_H