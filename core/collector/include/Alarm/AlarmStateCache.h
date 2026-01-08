#ifndef ALARM_STATE_CACHE_H
#define ALARM_STATE_CACHE_H

#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <optional>
#include "Common/Structs.h"

namespace PulseOne {
namespace Alarm {

class AlarmStateCache {
public:
    struct PointState {
        PulseOne::Structs::DataValue last_value;
        bool last_digital_state = false;
        std::chrono::system_clock::time_point last_check_time;
    };

    struct AlarmStatus {
        bool is_active = false;
        int64_t occurrence_id = 0;
    };

    void updatePointState(int point_id, const PulseOne::Structs::DataValue& value);
    PointState getPointState(int point_id) const;

    void setAlarmStatus(int rule_id, bool active, int64_t occurrence_id = 0);
    AlarmStatus getAlarmStatus(int rule_id) const;

private:
    mutable std::shared_mutex state_mutex_;
    std::unordered_map<int, PointState> point_states_;
    std::unordered_map<int, AlarmStatus> alarm_statuses_;
};

} // namespace Alarm
} // namespace PulseOne

#endif
