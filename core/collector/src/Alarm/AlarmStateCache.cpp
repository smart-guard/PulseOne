#include "Alarm/AlarmStateCache.h"
#include <variant>

namespace PulseOne {
namespace Alarm {

void AlarmStateCache::updatePointState(int point_id, const PulseOne::Structs::DataValue& value) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    auto& state = point_states_[point_id];
    state.last_value = value;
    
    // std::visit을 사용하여 값 추출
    std::visit([&state](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            state.last_digital_state = arg;
        } else if constexpr (std::is_arithmetic_v<T>) {
            state.last_digital_state = (static_cast<double>(arg) != 0.0);
        } else {
            state.last_digital_state = false;
        }
    }, value);
    
    state.last_check_time = std::chrono::system_clock::now();
}

AlarmStateCache::PointState AlarmStateCache::getPointState(int point_id) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    auto it = point_states_.find(point_id);
    if (it != point_states_.end()) return it->second;
    return PointState();
}

void AlarmStateCache::setAlarmStatus(int rule_id, bool active, int64_t occurrence_id) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    auto& status = alarm_statuses_[rule_id];
    status.is_active = active;
    status.occurrence_id = occurrence_id;
}

AlarmStateCache::AlarmStatus AlarmStateCache::getAlarmStatus(int rule_id) const {
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    auto it = alarm_statuses_.find(rule_id);
    if (it != alarm_statuses_.end()) return it->second;
    return AlarmStatus();
}

} // namespace Alarm
} // namespace PulseOne
