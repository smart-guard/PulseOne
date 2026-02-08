/**
 * @file ExportScheduleEntity.cpp
 * @brief Export Schedule Entity 구현부 (컴파일 에러 수정)
 * @version 1.0.1
 */

#include "Database/Entities/ExportScheduleEntity.h"
#include "Database/Repositories/ExportScheduleRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Entities {

// 생성자
ExportScheduleEntity::ExportScheduleEntity()
    : BaseEntity<ExportScheduleEntity>(), profile_id_(0), target_id_(0),
      timezone_("UTC"), data_range_("day"), lookback_periods_(1),
      is_enabled_(true), total_runs_(0), successful_runs_(0), failed_runs_(0) {}

ExportScheduleEntity::ExportScheduleEntity(int id) : ExportScheduleEntity() {
  setId(id);
}

ExportScheduleEntity::~ExportScheduleEntity() {}

// Repository 패턴
std::shared_ptr<Repositories::ExportScheduleRepository>
ExportScheduleEntity::getRepository() const {
  return RepositoryFactory::getInstance().getExportScheduleRepository();
}

// BaseEntity 순수 가상 함수 구현
bool ExportScheduleEntity::fromJson(const json &data) {
  try {
    if (data.contains("profile_id"))
      setProfileId(data["profile_id"]);
    if (data.contains("target_id"))
      setTargetId(data["target_id"]);
    if (data.contains("schedule_name"))
      setScheduleName(data["schedule_name"]);
    if (data.contains("description"))
      setDescription(data["description"]);
    if (data.contains("cron_expression"))
      setCronExpression(data["cron_expression"]);
    if (data.contains("timezone"))
      setTimezone(data["timezone"]);
    if (data.contains("data_range"))
      setDataRange(data["data_range"]);
    if (data.contains("lookback_periods"))
      setLookbackPeriods(data["lookback_periods"]);
    if (data.contains("is_enabled"))
      setEnabled(data["is_enabled"]);
    if (data.contains("last_status"))
      setLastStatus(data["last_status"]);
    if (data.contains("total_runs"))
      setTotalRuns(data["total_runs"]);
    if (data.contains("successful_runs"))
      setSuccessfulRuns(data["successful_runs"]);
    if (data.contains("failed_runs"))
      setFailedRuns(data["failed_runs"]);
    return true;
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportScheduleEntity::fromJson failed: " +
                                    std::string(e.what()));
    return false;
  }
}

// 비즈니스 로직
void ExportScheduleEntity::recordRun(
    bool success, const std::chrono::system_clock::time_point &next_run) {
  last_run_at_ = std::chrono::system_clock::now();
  last_status_ = success ? "success" : "failed";
  next_run_at_ = next_run;
  total_runs_++;
  if (success) {
    successful_runs_++;
  } else {
    failed_runs_++;
  }
  markModified();
}

double ExportScheduleEntity::getSuccessRate() const {
  if (total_runs_ == 0)
    return 0.0;
  return (static_cast<double>(successful_runs_) / total_runs_) * 100.0;
}

// DB 연산
bool ExportScheduleEntity::loadFromDatabase() {
  if (getId() <= 0) {
    return false;
  }

  try {
    auto repo = getRepository();
    if (!repo) {
      return false;
    }

    auto loaded = repo->findById(getId());
    if (loaded.has_value()) {
      *this = loaded.value();
      markSaved();
      return true;
    }

    return false;
  } catch (const std::exception &e) {
    markError();
    return false;
  }
}

bool ExportScheduleEntity::saveToDatabase() {
  try {
    auto repo = getRepository();
    if (!repo) {
      return false;
    }

    bool success = false;

    if (getId() <= 0) {
      success = repo->save(*this);
    } else {
      success = repo->update(*this);
    }

    if (success) {
      markSaved();
    } else {
      markError();
    }

    return success;
  } catch (const std::exception &e) {
    markError();
    return false;
  }
}

bool ExportScheduleEntity::deleteFromDatabase() {
  if (getId() <= 0) {
    return false;
  }

  try {
    auto repo = getRepository();
    if (!repo) {
      return false;
    }

    bool success = repo->deleteById(getId());

    if (success) {
      markDeleted();
    }

    return success;
  } catch (const std::exception &e) {
    return false;
  }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne