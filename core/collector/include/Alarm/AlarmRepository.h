// =============================================================================
// collector/include/Alarm/AlarmRepository.h
// 알람 데이터 저장소 (분리)
// =============================================================================

#ifndef ALARM_REPOSITORY_H
#define ALARM_REPOSITORY_H

#include "Alarm/AlarmTypes.h"
#include <unordered_map>
#include <shared_mutex>

namespace PulseOne {
namespace Database { class DatabaseManager; }

namespace Alarm {

class AlarmRepository {
public:
    AlarmRepository();
    ~AlarmRepository();
    
    // 초기화
    bool initialize(std::shared_ptr<Database::DatabaseManager> db_manager);
    
    // 규칙 관리
    AlarmErrorCode loadRules(int tenant_id);
    AlarmErrorCode saveRule(const AlarmRule& rule);
    AlarmErrorCode updateRule(int rule_id, const AlarmRule& rule);
    AlarmErrorCode deleteRule(int rule_id);
    
    // 규칙 조회
    std::optional<AlarmRule> getRule(int rule_id) const;
    std::vector<AlarmRule> getRules(int tenant_id) const;
    std::vector<AlarmRule> getRulesForPoint(int point_id, const std::string& point_type) const;
    
    // 발생 정보 관리
    int64_t saveOccurrence(const AlarmOccurrence& occurrence);
    AlarmErrorCode updateOccurrence(const AlarmOccurrence& occurrence);
    AlarmErrorCode deleteOccurrence(int64_t occurrence_id);
    
    // 발생 정보 조회
    std::optional<AlarmOccurrence> getOccurrence(int64_t occurrence_id) const;
    std::vector<AlarmOccurrence> getActiveOccurrences(int tenant_id) const;
    std::vector<AlarmOccurrence> queryOccurrences(const AlarmFilter& filter) const;
    
    // 인덱스 관리
    void rebuildIndexes();
    void addToPointIndex(int point_id, int rule_id);
    void removeFromPointIndex(int point_id, int rule_id);
    
private:
    // 규칙 저장소
    mutable std::shared_mutex rules_mutex_;
    std::unordered_map<int, AlarmRule> rules_;
    std::unordered_map<int, std::vector<int>> point_index_;  // point_id -> rule_ids
    
    // 발생 정보 저장소
    mutable std::shared_mutex occurrences_mutex_;
    std::unordered_map<int64_t, AlarmOccurrence> active_occurrences_;
    
    // 데이터베이스
    std::shared_ptr<Database::DatabaseManager> db_manager_;
    
    // ID 생성
    std::atomic<int64_t> next_occurrence_id_{1};
};

} // namespace Alarm
} // namespace PulseOne

#endif // ALARM_REPOSITORY_H
