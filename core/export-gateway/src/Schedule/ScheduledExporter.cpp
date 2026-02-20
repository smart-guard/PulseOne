/**
 * @file ScheduledExporter.cpp
 * @brief DB 기반 스케줄 관리 및 실행 구현 (리팩토링 버전)
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 2.0.0
 *
 * 주요 변경사항:
 * - ❌ DynamicTargetManager 멤버 소유 제거
 * - ✅ DynamicTargetManager::getInstance() 싱글턴 사용
 * - ✅ getTargetWithTemplate() 메서드 활용
 * - ✅ transformDataWithTemplate() 메서드 추가
 */

#include "Schedule/ScheduledExporter.h"
#include "Client/RedisClientImpl.h"
#include "Gateway/Model/AlarmMessage.h"
#include "Gateway/Model/ValueMessage.h"
#include "Gateway/Service/TargetRunner.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/ccronexpr.h"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp> // full json here; header only has json_fwd.hpp
#include <sstream>

namespace fs = std::filesystem;

namespace PulseOne {
namespace Schedule {

// .cpp-local alias replacing the removed global 'using json' in header
using json = nlohmann::json;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ScheduledExporter &
ScheduledExporter::getInstance(const ScheduledExporterConfig &config) {
  static ScheduledExporter instance(config);
  return instance;
}

ScheduledExporter::ScheduledExporter(const ScheduledExporterConfig &config)
    : config_(config), last_reload_time_(std::chrono::system_clock::now()) {

  LogManager::getInstance().Info("ScheduledExporter v2.0 초기화 시작");
  LogManager::getInstance().Info(
      "스케줄 체크 간격: " + std::to_string(config_.check_interval_seconds) +
      "초");
  LogManager::getInstance().Info(
      "리로드 간격: " + std::to_string(config_.reload_interval_seconds) + "초");
  LogManager::getInstance().Info("✅ DynamicTargetManager 싱글턴 사용");
}

ScheduledExporter::~ScheduledExporter() {
  stop();
  LogManager::getInstance().Info("ScheduledExporter 종료 완료");
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool ScheduledExporter::start() {
  if (is_running_.load()) {
    LogManager::getInstance().Warn("ScheduledExporter가 이미 실행 중입니다");
    return false;
  }

  LogManager::getInstance().Info("ScheduledExporter 시작 중...");

  // 1. TargetRunner 확인
  if (!target_runner_) {
    LogManager::getInstance().Error("TargetRunner가 설정되지 않음");
    return false;
  }

  // 2. Redis 연결
  if (!initializeRedisConnection()) {
    LogManager::getInstance().Error("Redis 연결 실패");
    return false;
  }

  // 3. Repositories 초기화
  if (!initializeRepositories()) {
    LogManager::getInstance().Error("Repositories 초기화 실패");
    return false;
  }

  // ❌ 4. DynamicTargetManager 초기화 제거
  // if (!initializeDynamicTargetManager()) {
  //     LogManager::getInstance().Error("DynamicTargetManager 초기화 실패");
  //     return false;
  // }

  // 4. 초기 스케줄 로드
  int loaded = reloadSchedules();
  LogManager::getInstance().Info("초기 스케줄 로드: " + std::to_string(loaded) +
                                 "개");

  // 5. 워커 스레드 시작
  should_stop_ = false;
  is_running_ = true;
  worker_thread_ = std::make_unique<std::thread>(
      &ScheduledExporter::scheduleCheckLoop, this);

  // 6. 재시도 스레드 시작
  startRetryThread();

  LogManager::getInstance().Info("ScheduledExporter 시작 완료 ✅");
  return true;
}

void ScheduledExporter::stop() {
  if (!is_running_.load()) {
    return;
  }

  LogManager::getInstance().Info("ScheduledExporter 중지 중...");

  should_stop_ = true;

  // 재시도 스레드 중지
  stopRetryThread();

  if (worker_thread_ && worker_thread_->joinable()) {
    worker_thread_->join();
  }

  is_running_ = false;
  LogManager::getInstance().Info("ScheduledExporter 중지 완료");
}

// =============================================================================
// 수동 실행
// =============================================================================

ScheduleExecutionResult ScheduledExporter::executeSchedule(int schedule_id) {
  LogManager::getInstance().Info("스케줄 수동 실행: ID=" +
                                 std::to_string(schedule_id));

  ScheduleExecutionResult result;
  result.schedule_id = schedule_id;

  try {
    // DB에서 스케줄 조회
    auto schedule_opt = schedule_repo_->findById(schedule_id);
    if (!schedule_opt.has_value()) {
      result.success = false;
      result.error_message = "스케줄을 찾을 수 없음";
      LogManager::getInstance().Error(result.error_message);
      return result;
    }

    result = executeScheduleInternal(schedule_opt.value());

  } catch (const std::exception &e) {
    result.success = false;
    result.error_message = e.what();
    LogManager::getInstance().Error("스케줄 실행 실패: " +
                                    std::string(e.what()));
  }

  return result;
}

int ScheduledExporter::executeAllSchedules() {
  LogManager::getInstance().Info("모든 활성 스케줄 실행");

  int executed_count = 0;

  try {
    auto schedules = schedule_repo_->findEnabled();

    for (const auto &schedule : schedules) {
      if (should_stop_.load())
        break;

      auto result = executeScheduleInternal(schedule);
      if (result.success) {
        executed_count++;
      }
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("전체 실행 실패: " + std::string(e.what()));
  }

  return executed_count;
}

// =============================================================================
// 스케줄 관리
// =============================================================================

int ScheduledExporter::reloadSchedules() {
  std::lock_guard<std::mutex> lock(schedule_mutex_);

  try {
    LogManager::getInstance().Info("스케줄 리로드 시작");

    // DB에서 활성 스케줄 조회
    auto schedules = schedule_repo_->findEnabled();

    // 캐시 업데이트
    cached_schedules_.clear();
    for (const auto &schedule : schedules) {
      cached_schedules_[schedule.getId()] = schedule;
    }

    last_reload_time_ = std::chrono::system_clock::now();

    LogManager::getInstance().Info(
        "스케줄 리로드 완료: " + std::to_string(schedules.size()) + "개");

    return schedules.size();

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("스케줄 리로드 실패: " +
                                    std::string(e.what()));
    return 0;
  }
}

std::vector<int> ScheduledExporter::getActiveScheduleIds() const {
  std::lock_guard<std::mutex> lock(schedule_mutex_);

  std::vector<int> ids;
  ids.reserve(cached_schedules_.size());

  for (const auto &pair : cached_schedules_) {
    ids.push_back(pair.first);
  }

  return ids;
}

std::map<int, std::chrono::system_clock::time_point>
ScheduledExporter::getUpcomingSchedules() const {

  std::lock_guard<std::mutex> lock(schedule_mutex_);

  std::map<int, std::chrono::system_clock::time_point> upcoming;

  for (const auto &pair : cached_schedules_) {
    // getNextRunAt()은 optional을 반환
    auto next_run = pair.second.getNextRunAt();
    if (next_run.has_value()) {
      upcoming[pair.first] = next_run.value();
    }
  }

  return upcoming;
}

// =============================================================================
// 통계
// =============================================================================

json ScheduledExporter::getStatistics() const {
  json stats;

  stats["is_running"] = is_running_.load();
  stats["total_executions"] = total_executions_.load();
  stats["successful_executions"] = successful_executions_.load();
  stats["failed_executions"] = failed_executions_.load();
  stats["total_points_exported"] = total_data_points_exported_.load();
  stats["total_execution_time_ms"] = total_execution_time_ms_.load();

  {
    std::lock_guard<std::mutex> lock(schedule_mutex_);
    stats["active_schedules"] = cached_schedules_.size();
  }

  // 평균 계산
  size_t total = total_executions_.load();
  if (total > 0) {
    stats["avg_execution_time_ms"] =
        static_cast<double>(total_execution_time_ms_.load()) / total;
    stats["success_rate"] =
        static_cast<double>(successful_executions_.load()) / total * 100.0;
  } else {
    stats["avg_execution_time_ms"] = 0.0;
    stats["success_rate"] = 0.0;
  }

  return stats;
}

void ScheduledExporter::resetStatistics() {
  total_executions_ = 0;
  successful_executions_ = 0;
  failed_executions_ = 0;
  total_data_points_exported_ = 0;
  total_execution_time_ms_ = 0;

  LogManager::getInstance().Info("통계 초기화 완료");
}

// =============================================================================
// 내부 메서드 - 메인 루프
// =============================================================================

void ScheduledExporter::scheduleCheckLoop() {
  using namespace std::chrono;

  LogManager::getInstance().Info("스케줄 체크 루프 시작");

  while (!should_stop_.load()) {
    try {
      // 1. 주기적 리로드 체크
      auto now = system_clock::now();
      auto elapsed = duration_cast<seconds>(now - last_reload_time_).count();

      if (elapsed >= config_.reload_interval_seconds) {
        reloadSchedules();
      }

      // 2. 실행할 스케줄 찾기
      auto due_schedules = findDueSchedules();

      // 3. 각 스케줄 실행
      for (const auto &schedule : due_schedules) {
        if (should_stop_.load())
          break;

        auto result = executeScheduleInternal(schedule);

        // 결과 로깅
        logExecutionResult(result);

        // 스케줄 상태 업데이트
        updateScheduleStatus(schedule.getId(), result.success,
                             result.next_run_time);
      }

    } catch (const std::exception &e) {
      LogManager::getInstance().Error("스케줄 체크 루프 에러: " +
                                      std::string(e.what()));
    }

    // 대기
    for (int i = 0; i < config_.check_interval_seconds && !should_stop_.load();
         ++i) {
      std::this_thread::sleep_for(seconds(1));
    }
  }

  LogManager::getInstance().Info("스케줄 체크 루프 종료");
}

std::vector<PulseOne::Database::Entities::ExportScheduleEntity>
ScheduledExporter::findDueSchedules() {

  std::vector<PulseOne::Database::Entities::ExportScheduleEntity> due_schedules;

  try {
    // DB에서 실행 대기 중인 스케줄 조회
    due_schedules = schedule_repo_->findPending();

    if (config_.enable_debug_log && !due_schedules.empty()) {
      LogManager::getInstance().Debug(
          "실행 대기 스케줄: " + std::to_string(due_schedules.size()) + "개");
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("실행 대기 스케줄 조회 실패: " +
                                    std::string(e.what()));
  }

  return due_schedules;
}

ScheduleExecutionResult ScheduledExporter::executeScheduleInternal(
    const PulseOne::Database::Entities::ExportScheduleEntity &schedule) {

  ScheduleExecutionResult result;
  result.schedule_id = schedule.getId();
  result.execution_timestamp = std::chrono::system_clock::now();

  auto start_time = std::chrono::high_resolution_clock::now();

  try {
    LogManager::getInstance().Info(
        "스케줄 실행: " + schedule.getScheduleName() +
        " (ID: " + std::to_string(schedule.getId()) + ")");

    // 1. Redis에서 데이터 수집
    auto data_points = collectDataFromRedis(schedule);
    result.data_point_count = data_points.size();

    if (data_points.empty()) {
      result.success = false;
      result.error_message = "수집된 데이터가 없음";
      LogManager::getInstance().Warn(result.error_message);
      return result;
    }

    LogManager::getInstance().Info(
        "데이터 수집 완료: " + std::to_string(data_points.size()) +
        "개 포인트");

    // 2. 타겟 조회
    auto target_id = schedule.getTargetId();
    auto target_entity = target_repo_->findById(target_id);

    if (!target_entity.has_value()) {
      result.success = false;
      result.error_message =
          "타겟을 찾을 수 없음 (ID: " + std::to_string(target_id) + ")";
      LogManager::getInstance().Error(result.error_message);
      return result;
    }

    std::string target_name = target_entity->getName();
    result.target_names.push_back(target_name);

    // 3. 데이터 전송 (템플릿 지원)
    bool send_success = sendDataToTarget(target_name, data_points, schedule);

    if (send_success) {
      result.successful_targets++;
      result.success = true;
    } else {
      result.failed_targets++;
      result.success = false;
      result.error_message = "타겟 전송 실패: " + target_name;
    }

    // 4. 실행 시간 계산
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count();

    // 5. 로그 저장
    saveExecutionLog(schedule, result);

    // 6. 통계 업데이트
    total_executions_.fetch_add(1);
    if (result.success) {
      successful_executions_.fetch_add(1);
    } else {
      failed_executions_.fetch_add(1);
    }
    total_data_points_exported_.fetch_add(result.data_point_count);

    LogManager::getInstance().Info(
        "스케줄 실행 완료: " + schedule.getScheduleName() + " (" +
        std::to_string(result.execution_time_ms) + "ms)");

  } catch (const std::exception &e) {
    result.success = false;
    result.error_message = e.what();

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count();

    LogManager::getInstance().Error("스케줄 실행 실패: " +
                                    std::string(e.what()));
  }

  return result;
}

// =============================================================================
// 내부 메서드 - 데이터 수집
// =============================================================================

std::vector<ExportDataPoint> ScheduledExporter::collectDataForSchedule(
    const PulseOne::Database::Entities::ExportScheduleEntity &schedule,
    const PulseOne::Database::Entities::ExportTargetEntity &target) {

  std::vector<ExportDataPoint> data_points;

  try {
    // 1. 시간 범위 계산
    auto [start_time, end_time] = calculateTimeRange(
        schedule.getDataRange(), schedule.getLookbackPeriods());

    // 2. 타겟의 매핑 조회
    auto mappings = mapping_repo_->findByTargetId(target.getId());

    if (mappings.empty()) {
      LogManager::getInstance().Warn("타겟에 매핑된 포인트 없음: " +
                                     target.getName());
      return data_points;
    }

    // 3. 각 매핑된 포인트의 데이터 조회
    for (const auto &mapping : mappings) {
      // ✅ isEnabled() 사용
      if (!mapping.isEnabled() || !mapping.getPointId().has_value())
        continue;

      auto point_data =
          fetchPointData(mapping.getPointId().value(), start_time, end_time);

      if (point_data.has_value()) {
        ExportDataPoint dp = point_data.value();

        // ✅ 매핑 정보 적용
        if (!mapping.getTargetFieldName().empty()) {
          dp.mapped_name = mapping.getTargetFieldName();
        } else {
          dp.mapped_name = dp.point_name;
        }

        // ✅ 변환 설정 적용 (Scale/Offset)
        if (mapping.hasConversion()) {
          try {
            double val = std::stod(dp.value);
            double scale = mapping.getScale();
            double offset = mapping.getOffset();

            double converted = (val * scale) + offset;

            // 소수점 4자리까지 포맷팅 (정확도 유지)
            std::stringstream oss;
            oss << std::fixed << std::setprecision(4) << converted;
            std::string conv_str = oss.str();

            // 불필요한 0 제거
            size_t dot_pos = conv_str.find('.');
            if (dot_pos != std::string::npos) {
              conv_str.erase(conv_str.find_last_not_of('0') + 1,
                             std::string::npos);
              if (conv_str.back() == '.')
                conv_str.pop_back();
            }

            dp.value = conv_str;
          } catch (...) {
            // 변환 실패 시 원본값 유지
          }
        }

        data_points.push_back(dp);
      }
    }

    if (config_.enable_debug_log) {
      LogManager::getInstance().Debug(
          "데이터 수집 완료: " + std::to_string(data_points.size()) +
          "개 포인트");
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("데이터 수집 실패: " +
                                    std::string(e.what()));
  }

  return data_points;
}

std::pair<std::chrono::system_clock::time_point,
          std::chrono::system_clock::time_point>
ScheduledExporter::calculateTimeRange(const std::string &data_range,
                                      int lookback_periods) {

  using namespace std::chrono;

  auto now = system_clock::now();
  system_clock::time_point start_time;

  if (data_range == "minute") {
    start_time = now - minutes(lookback_periods);
  } else if (data_range == "hour") {
    start_time = now - hours(lookback_periods);
  } else if (data_range == "day") {
    start_time = now - hours(24 * lookback_periods);
  } else if (data_range == "week") {
    start_time = now - hours(24 * 7 * lookback_periods);
  } else {
    // 기본값: 1시간
    start_time = now - hours(1);
  }

  return {start_time, now};
}

std::optional<ExportDataPoint> ScheduledExporter::fetchPointData(
    int point_id, const std::chrono::system_clock::time_point &start_time,
    const std::chrono::system_clock::time_point &end_time) {
  (void)start_time;
  (void)end_time;

  if (!redis_client_ || !redis_client_->isConnected()) {
    return std::nullopt;
  }

  try {
    // Redis에서 현재값 조회 (Collector 표준: :latest)
    std::string key = "point:" + std::to_string(point_id) + ":latest";
    std::string json_str = redis_client_->get(key);

    if (json_str.empty()) {
      return std::nullopt;
    }

    auto data = json::parse(json_str);

    ExportDataPoint point;
    point.point_id = point_id;

    // Use safe type-checking and conversions for all potential type-mismatched
    // fields
    if (data.contains("bd") && data["bd"].is_number()) {
      point.building_id = data["bd"].get<int>();
    } else if (data.contains("bd") && data["bd"].is_string()) {
      try {
        point.building_id = std::stoi(data["bd"].get<std::string>());
      } catch (...) {
        point.building_id = 0;
      }
    } else if (data.contains("building_id") &&
               data["building_id"].is_number()) {
      point.building_id = data["building_id"].get<int>();
    } else if (data.contains("building_id") &&
               data["building_id"].is_string()) {
      try {
        point.building_id = std::stoi(data["building_id"].get<std::string>());
      } catch (...) {
        point.building_id = 0;
      }
    } else {
      point.building_id = 0;
    }

    point.point_name = data.value("nm", data.value("point_name", ""));
    point.value = data.value("vl", data.value("value", ""));

    if (data.contains("tm_ms") && data["tm_ms"].is_number()) {
      point.timestamp = data["tm_ms"].get<long long>();
    } else if (data.contains("timestamp") && data["timestamp"].is_number()) {
      point.timestamp = data["timestamp"].get<long long>();
    } else {
      point.timestamp = 0LL;
    }

    if (data.contains("st") && data["st"].is_number()) {
      point.quality = data["st"].get<int>();
    } else if (data.contains("st") && data["st"].is_string()) {
      try {
        point.quality = (int)data["st"].get<std::string>()[0];
      } catch (...) {
        point.quality = 0;
      }
    } else if (data.contains("quality") && data["quality"].is_number()) {
      point.quality = data["quality"].get<int>();
    } else if (data.contains("quality") && data["quality"].is_string()) {
      try {
        point.quality = (int)data["quality"].get<std::string>()[0];
      } catch (...) {
        point.quality = 0;
      }
    } else {
      point.quality = 0;
    }

    point.unit = data.value("unit", "");

    // [v3.0.0] Zero-Assumption: Store full raw data for token harvesting
    point.extra_info = data;

    return point;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("포인트 데이터 조회 실패 [" +
                                    std::to_string(point_id) +
                                    "]: " + std::string(e.what()));
    return std::nullopt;
  }
}

// =============================================================================
// 내부 메서드 - 데이터 전송
// =============================================================================

bool ScheduledExporter::sendDataToTarget(
    const std::string &target_name,
    const std::vector<ExportDataPoint> &data_points,
    const PulseOne::Database::Entities::ExportScheduleEntity &schedule) {
  (void)schedule;

  try {
    // 1. ValueMessage 벡터로 변환 (C# 호환 포맷)
    std::vector<PulseOne::Gateway::Model::ValueMessage> values;
    for (const auto &point : data_points) {
      PulseOne::Gateway::Model::ValueMessage msg;
      msg.site_id = point.building_id;
      msg.point_id = point.point_id;
      msg.point_name =
          point.mapped_name.empty() ? point.point_name : point.mapped_name;
      msg.measured_value = point.value;

      // Timestamp 변환 (ms -> yyyy-MM-dd HH:mm:ss.fff)
      std::time_t tt = point.timestamp / 1000;
      std::tm *tm_info = std::gmtime(&tt);
      char buf[32];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
      msg.timestamp = buf;

      msg.status_code = point.quality;
      msg.data_type = "dbl";             // Default type
      msg.extra_info = point.extra_info; // [v3.0.0] Propagate raw metadata

      values.push_back(std::move(msg));
    }

    // 2. TargetRunner 사용 (이미 주입됨)
    // 3. 특정 타겟으로 전송
    auto result = target_runner_->sendValueBatch(values, target_name);

    if (result.success_count > 0) {
      LogManager::getInstance().Info(
          "타겟 전송 성공 [" + target_name +
          "]: " + std::to_string(result.success_count) + "개 성공");
      return true;
    } else {
      LogManager::getInstance().Error("타겟 전송 실패 [" + target_name + "]");

      // [FIX] 영구 실패 방지: TargetRegistry에서 target config를 조회해
      // 실패 데이터를 로컬 파일에 저장 (Persistent Retry용)
      try {
        auto target_opt = target_runner_->getRegistry().getTarget(target_name);
        json target_config = target_opt ? target_opt->config : json::object();
        saveFailedBatchToFile(target_name, values, target_config);
      } catch (const std::exception &e) {
        LogManager::getInstance().Error("실패 데이터 로컬 저장 중 예외 [" +
                                        target_name +
                                        "]: " + std::string(e.what()));
      }

      return false;
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("데이터 전송 중 예외 [" + target_name +
                                    "]: " + std::string(e.what()));
    return false;
  }

  return false;
}

// =============================================================================
// 템플릿 기반 데이터 변환 (v2.0 신규)
// =============================================================================

json ScheduledExporter::transformDataWithTemplate(
    const json &template_json, const json &field_mappings,
    const std::vector<ExportDataPoint> &data_points) {

  json result = template_json; // 템플릿 복사

  try {
    LogManager::getInstance().Debug("템플릿 변환 시작 - 데이터 포인트: " +
                                    std::to_string(data_points.size()) + "개");

    // field_mappings 형식:
    // {
    //   "point_name_1": "$.data.temperature",
    //   "point_name_2": "$.data.humidity"
    // }

    for (const auto &[point_name, json_path] : field_mappings.items()) {
      // 해당 포인트 찾기
      auto it = std::find_if(data_points.begin(), data_points.end(),
                             [&point_name](const ExportDataPoint &p) {
                               return p.point_name == point_name;
                             });

      if (it == data_points.end()) {
        LogManager::getInstance().Warn("포인트를 찾을 수 없음: " + point_name);
        continue;
      }

      // JSON Path 파싱 (간단한 구현)
      // 예: "$.data.temperature" → result["data"]["temperature"] = value
      std::string path = json_path.get<std::string>();

      // $ 제거
      if (path[0] == '$') {
        path = path.substr(1);
      }
      // . 제거
      if (path[0] == '.') {
        path = path.substr(1);
      }

      // 경로를 . 기준으로 분리
      std::vector<std::string> keys;
      std::stringstream ss(path);
      std::string key;

      while (std::getline(ss, key, '.')) {
        keys.push_back(key);
      }

      // 중첩된 객체 생성 및 값 설정
      json *current = &result;
      for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (!current->contains(keys[i]) || !(*current)[keys[i]].is_object()) {
          (*current)[keys[i]] = json::object();
        }
        current = &(*current)[keys[i]];
      }

      // 마지막 키에 값 설정
      if (!keys.empty()) {
        // value를 적절한 타입으로 변환
        try {
          // 숫자로 변환 시도
          double num_value = std::stod(it->value);
          (*current)[keys.back()] = num_value;
        } catch (...) {
          // 문자열로 저장
          (*current)[keys.back()] = it->value;
        }
      }

      LogManager::getInstance().Debug("매핑 완료: " + point_name + " → " +
                                      json_path.get<std::string>());
    }

    LogManager::getInstance().Info("템플릿 변환 완료");

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("템플릿 변환 실패: " +
                                    std::string(e.what()));
    // 변환 실패 시 원본 템플릿 반환
  }

  return result;
}

std::vector<std::vector<ExportDataPoint>> ScheduledExporter::createBatches(
    const std::vector<ExportDataPoint> &data_points) {

  std::vector<std::vector<ExportDataPoint>> batches;

  if (data_points.empty()) {
    return batches;
  }

  for (size_t i = 0; i < data_points.size(); i += config_.batch_size) {
    size_t end = std::min(i + config_.batch_size, data_points.size());
    batches.emplace_back(data_points.begin() + i, data_points.begin() + end);
  }

  return batches;
}

// =============================================================================
// 내부 메서드 - Cron 처리
// =============================================================================

std::chrono::system_clock::time_point ScheduledExporter::calculateNextRunTime(
    const std::string &cron_expression, const std::string &timezone,
    const std::chrono::system_clock::time_point &from_time) {

  (void)timezone; // Timezone handling is complex, defaulting to system time
                  // for now

  using namespace std::chrono;

  cron_expr expr;
  const char *err = nullptr;

  // 1. Cron 표현식 파싱
  cron_parse_expr(cron_expression.c_str(), &expr, &err);

  if (err) {
    LogManager::getInstance().Error("❌ Cron 파싱 실패: " + cron_expression +
                                    " (" + std::string(err) + ")");
    // 실패 시 안전책: 1시간 후 리턴
    return from_time + hours(1);
  }

  // 2. 현재 시간(from_time)을 time_t로 변환
  time_t now_t = system_clock::to_time_t(from_time);

  // 3. 다음 실행 시간 계산
  time_t next_t = cron_next(&expr, now_t);

  if (next_t == (time_t)-1) {
    LogManager::getInstance().Error("❌ 다음 실행 시간 계산 실패 (Cron): " +
                                    cron_expression);
    return from_time + hours(1);
  }

  return system_clock::from_time_t(next_t);
}

// =============================================================================
// 내부 메서드 - DB 연동
// =============================================================================

void ScheduledExporter::logExecutionResult(
    const ScheduleExecutionResult &result) {
  try {
    // ExportLogEntity 생성
    PulseOne::Database::Entities::ExportLogEntity log;
    log.setTargetId(result.schedule_id); // schedule_id를 임시로 사용
    log.setTimestamp(std::chrono::system_clock::now());
    log.setStatus(result.success ? "success" : "failed");

    // ✅ ExportLogEntity에 setTotalRecords 등이 없으므로
    // 메시지에 포함하거나 나중에 추가
    std::string details =
        "Points: " + std::to_string(result.total_points) +
        ", Success: " + std::to_string(result.exported_points) +
        ", Failed: " + std::to_string(result.failed_points) +
        ", Duration: " + std::to_string(result.execution_time.count()) + "ms";

    // ErrorMessage 필드를 활용하여 상세 정보 저장
    if (!result.error_message.empty()) {
      log.setErrorMessage(result.error_message + " | " + details);
    } else {
      log.setErrorMessage(details);
    }

    log_repo_->save(log);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("실행 결과 로그 저장 실패: " +
                                    std::string(e.what()));
  }
}

bool ScheduledExporter::updateScheduleStatus(
    int schedule_id, bool success,
    const std::chrono::system_clock::time_point &next_run) {

  try {
    return schedule_repo_->updateRunStatus(schedule_id, success, next_run);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("스케줄 상태 업데이트 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 내부 메서드 - 초기화
// =============================================================================

bool ScheduledExporter::initializeRedisConnection() {
  try {
    LogManager::getInstance().Info("Redis 연결 초기화 중...");

    redis_client_ = std::make_shared<RedisClientImpl>();

    if (!redis_client_->isConnected()) {
      LogManager::getInstance().Error("Redis 연결 실패");
      return false;
    }

    LogManager::getInstance().Info("Redis 연결 성공");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Redis 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

bool ScheduledExporter::initializeRepositories() {
  try {
    LogManager::getInstance().Info("Repositories 초기화 중...");

    schedule_repo_ = std::make_unique<
        PulseOne::Database::Repositories::ExportScheduleRepository>();

    target_repo_ = std::make_unique<
        PulseOne::Database::Repositories::ExportTargetRepository>();

    log_repo_ = std::make_unique<
        PulseOne::Database::Repositories::ExportLogRepository>();

    mapping_repo_ = std::make_unique<
        PulseOne::Database::Repositories::ExportTargetMappingRepository>();

    // ✅ 템플릿 Repository 추가
    template_repo_ = std::make_unique<
        PulseOne::Database::Repositories::PayloadTemplateRepository>();

    LogManager::getInstance().Info("Repositories 초기화 완료");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Repositories 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

std::vector<ExportDataPoint> ScheduledExporter::collectDataFromRedis(
    const PulseOne::Database::Entities::ExportScheduleEntity &schedule) {

  std::vector<ExportDataPoint> data_points;

  try {
    LogManager::getInstance().Info("Redis에서 데이터 수집 시작: " +
                                   schedule.getScheduleName());

    // 1. 타겟 조회
    auto target_id = schedule.getTargetId();
    auto target_entity = target_repo_->findById(target_id);

    if (!target_entity.has_value()) {
      LogManager::getInstance().Error(
          "타겟을 찾을 수 없음 (ID: " + std::to_string(target_id) + ")");
      return data_points;
    }

    // 2. collectDataForSchedule 호출 (이미 구현된 함수 사용)
    data_points = collectDataForSchedule(schedule, target_entity.value());

    LogManager::getInstance().Info(
        "Redis 데이터 수집 완료: " + std::to_string(data_points.size()) +
        "개 포인트");

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Redis 데이터 수집 실패: " +
                                    std::string(e.what()));
  }

  return data_points;
}

void ScheduledExporter::saveExecutionLog(
    const PulseOne::Database::Entities::ExportScheduleEntity &schedule,
    const ScheduleExecutionResult &result) {

  try {
    if (!log_repo_) {
      LogManager::getInstance().Warn("ExportLogRepository가 초기화되지 않음");
      return;
    }

    // ExportLogEntity 생성
    PulseOne::Database::Entities::ExportLogEntity log;

    // ✅ 실제 존재하는 필드만 사용
    log.setLogType("schedule");              // 스케줄 실행 로그
    log.setTargetId(schedule.getTargetId()); // 타겟 ID
    log.setServiceId(schedule.getId()); // ✅ schedule_id를 service_id에 저장
    log.setStatus(result.success ? "success" : "failure");
    log.setProcessingTimeMs(static_cast<int>(result.execution_time_ms));

    // 상세 정보를 JSON으로 구성
    json details = {{"schedule_id", schedule.getId()},
                    {"schedule_name", schedule.getScheduleName()},
                    {"data_point_count", result.data_point_count},
                    {"successful_targets", result.successful_targets},
                    {"failed_targets", result.failed_targets},
                    {"execution_time_ms", result.execution_time_ms}};

    // ✅ 실제 존재하는 setter 사용
    log.setResponseData(details.dump()); // 상세 정보는 JSON으로 저장

    if (!result.error_message.empty()) {
      log.setErrorMessage(result.error_message);
    }

    // 로그 저장
    if (!log_repo_->save(log)) {
      LogManager::getInstance().Error("실행 로그 저장 실패");
    } else {
      LogManager::getInstance().Debug("실행 로그 저장 완료: Schedule ID=" +
                                      std::to_string(schedule.getId()));
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("실행 로그 저장 중 예외: " +
                                    std::string(e.what()));
  }
}

// =============================================================================
// 실패 데이터 처리 (Persistent Retry)
// =============================================================================

void ScheduledExporter::saveFailedBatchToFile(
    const std::string &target_name,
    const std::vector<PulseOne::Gateway::Model::ValueMessage> &values,
    const json &target_config) {
  // [FIX] Signature unified with header: CSP::ValueMessage alias ->
  // Gateway::Model::ValueMessage

  try {
    // 1. 저장 경로 확인 (ConfigManager 사용)
    std::string base_dir = "/app/data/export_failed";

    // 환경 변수 또는 ConfigManager에서 경로 가져오기 시도
    char *env_path = std::getenv("PULSEONE_EXPORT_FAILED_DIR");
    if (env_path) {
      base_dir = env_path;
    }

    if (!fs::exists(base_dir)) {
      fs::create_directories(base_dir);
    }

    // 2. 파일명 생성 (타겟명_타임스탬프_랜덤.json)
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();

    std::string filename = target_name + "_" + std::to_string(ts) + ".json";
    std::string full_path = base_dir + "/" + filename;

    // 3. 데이터 구성
    json failed_data;
    failed_data["target_name"] = target_name;
    failed_data["target_config"] = target_config;
    failed_data["type"] = "value";
    failed_data["retry_count"] = 0;
    failed_data["timestamp"] = ts;

    json values_json = json::array();
    for (const auto &v : values) {
      values_json.push_back(v.to_json());
    }
    failed_data["values"] = values_json;

    // 4. 파일 저장
    std::ofstream ofs(full_path);
    if (ofs.is_open()) {
      ofs << failed_data.dump(2);
      ofs.close();
      LogManager::getInstance().Warn("실패 데이터 로컬 저장 완료: " +
                                     full_path);
    } else {
      LogManager::getInstance().Error(
          "실패 데이터 저장 실패 (파일 열기 오류): " + full_path);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("실패 데이터 저장 중 예외: " +
                                    std::string(e.what()));
  }
}

void ScheduledExporter::startRetryThread() {
  if (retry_thread_running_.load())
    return;

  retry_thread_running_ = true;
  retry_thread_ = std::make_unique<std::thread>(
      &ScheduledExporter::backgroundRetryLoop, this);
  LogManager::getInstance().Info("✅ Export 재시도 스레드 시작");
}

void ScheduledExporter::stopRetryThread() {
  retry_thread_running_ = false;
  if (retry_thread_ && retry_thread_->joinable()) {
    retry_thread_->join();
  }
}

void ScheduledExporter::backgroundRetryLoop() {
  // 초기 지연 (시스템 안정화 대기)
  std::this_thread::sleep_for(std::chrono::seconds(30));

  while (retry_thread_running_.load() && !should_stop_.load()) {
    try {
      processFailedExports();
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("재시도 루프 예외: " +
                                      std::string(e.what()));
    }

    // 주기적 실행 (기본 5분)
    int interval_secs = config_.retry_interval_seconds;

    for (int i = 0; i < interval_secs && retry_thread_running_.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void ScheduledExporter::processFailedExports() {
  std::string base_dir = "/app/data/export_failed";
  char *env_path = std::getenv("PULSEONE_EXPORT_FAILED_DIR");
  if (env_path)
    base_dir = env_path;

  if (!fs::exists(base_dir))
    return;

  // LogManager::getInstance().Debug("실패 데이터 재시도 스캔 시작: " +
  // base_dir);

  for (const auto &entry : fs::directory_iterator(base_dir)) {
    if (!retry_thread_running_.load())
      break;
    if (!entry.is_regular_file())
      continue;
    if (entry.path().extension() != ".json")
      continue;

    std::string file_path = entry.path().string();

    try {
      std::ifstream ifs(file_path);
      if (!ifs.is_open())
        continue;

      json failed_data = json::parse(ifs);
      ifs.close();

      std::string target_name = failed_data.value("target_name", "");
      json target_config = failed_data["target_config"];
      std::string type = failed_data.value("type", "value");
      int retry_count = failed_data.value("retry_count", 0);

      if (target_name.empty()) {
        fs::remove(file_path);
        continue;
      }

      PulseOne::Export::BatchTargetResult result;

      if (!target_runner_) {
        LogManager::getInstance().Warn("target_runner_ 없음 - 재시도 스킵 [" +
                                       target_name + "]");
        continue;
      }

      if (type == "value") {
        json values_json = failed_data["values"];
        if (values_json.empty()) {
          fs::remove(file_path);
          continue;
        }

        std::vector<PulseOne::Gateway::Model::ValueMessage> values;
        for (const auto &v_json : values_json) {
          PulseOne::Gateway::Model::ValueMessage v;
          v.site_id = v_json.value("site_id", v_json.value("bd", 0));
          v.point_id = v_json.value("point_id", 0);
          v.point_name = v_json.value("point_name", v_json.value("nm", ""));
          v.measured_value =
              v_json.value("measured_value", v_json.value("vl", ""));
          v.timestamp = v_json.value("timestamp", v_json.value("tm", ""));
          v.status_code = v_json.value("status_code", v_json.value("st", 0));
          v.data_type = v_json.value("data_type", v_json.value("ty", ""));
          values.push_back(v);
        }

        LogManager::getInstance().Info(
            "실패 데이터 재전송 시도 [" + target_name +
            "]: " + std::to_string(values.size()) + "개 (시도 " +
            std::to_string(retry_count + 1) + ")");

        result = target_runner_->sendValueBatch(values, target_name);
      } else if (type == "alarm") {
        json alarms_json = failed_data["alarms"];
        if (alarms_json.empty()) {
          fs::remove(file_path);
          continue;
        }

        std::vector<PulseOne::Gateway::Model::AlarmMessage> alarms;
        for (const auto &a_json : alarms_json) {
          PulseOne::Gateway::Model::AlarmMessage a;
          a.site_id = a_json.value("bd", 0);
          a.point_name = a_json.value("nm", "");
          a.measured_value = a_json.value("vl", 0.0);
          a.timestamp = a_json.value("tm", "");
          a.alarm_level = a_json.value("al", 0);
          a.status_code = a_json.value("st", 0);
          a.description = a_json.value("des", "");
          alarms.push_back(a);
        }

        LogManager::getInstance().Info("실패 알람 재전송 시도 [" + target_name +
                                       "]: " + std::to_string(alarms.size()) +
                                       "개 (시도 " +
                                       std::to_string(retry_count + 1) + ")");

        result = target_runner_->sendAlarmBatch(alarms, target_name);
      }

      if (result.successful_targets > 0) {
        LogManager::getInstance().Info("✅ 재전송 성공: " + file_path);
        fs::remove(file_path);
      } else {
        // 실패 시 카운트 증가 및 업데이트
        failed_data["retry_count"] = retry_count + 1;

        // 너무 많이 실패한 경우? (옵션: 별도 폴더로 이동하거나 삭제)
        if (retry_count >= 10) {
          LogManager::getInstance().Error(
              "❌ 최대 재시도 횟수 초과, 파일 삭제: " + file_path);
          fs::remove(file_path);
        } else {
          std::ofstream ofs(file_path);
          ofs << failed_data.dump(2);
          ofs.close();
        }
      }

    } catch (const std::exception &e) {
      LogManager::getInstance().Error("실패 파일 처리 중 오류 [" + file_path +
                                      "]: " + std::string(e.what()));
    }

    // 타겟 부하 방지를 위해 파일 간 약간의 지연
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void ScheduledExporter::saveFailedAlarmBatchToFile(
    const std::string &target_name,
    const std::vector<PulseOne::CSP::AlarmMessage> &alarms,
    const json &target_config) {

  try {
    std::string base_dir = "/app/data/export_failed";
    char *env_path = std::getenv("PULSEONE_EXPORT_FAILED_DIR");
    if (env_path)
      base_dir = env_path;

    if (!fs::exists(base_dir)) {
      fs::create_directories(base_dir);
    }

    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();

    std::string filename =
        "alarm_" + target_name + "_" + std::to_string(ts) + ".json";
    std::string full_path = base_dir + "/" + filename;

    json failed_data;
    failed_data["target_name"] = target_name;
    failed_data["target_config"] = target_config;
    failed_data["type"] = "alarm";
    failed_data["retry_count"] = 0;
    failed_data["timestamp"] = ts;

    json alarms_json = json::array();
    for (const auto &a : alarms) {
      alarms_json.push_back(a.to_json());
    }
    failed_data["alarms"] = alarms_json;

    std::ofstream ofs(full_path);
    if (ofs.is_open()) {
      ofs << failed_data.dump(2);
      ofs.close();
      LogManager::getInstance().Info("❌ 실패 알람 배치 로컬 저장 완료: " +
                                     full_path);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("❌ 실패 데이터 저장 중 예외: " +
                                    std::string(e.what()));
  }
}

} // namespace Schedule
} // namespace PulseOne

// =============================================================================
// ExportDataPoint::to_json() / ScheduleExecutionResult::to_json()
// moved from ScheduledExporter.h (structs live in PulseOne::Schedule)
// =============================================================================
namespace PulseOne {
namespace Schedule {

nlohmann::json ExportDataPoint::to_json() const {
  using json = nlohmann::json;
  return json::object({{"point_id", point_id},
                       {"building_id", building_id},
                       {"point_name", point_name},
                       {"mapped_name", mapped_name},
                       {"value", value},
                       {"timestamp", timestamp},
                       {"quality", quality},
                       {"unit", unit},
                       {"extra_info", extra_info}});
}

nlohmann::json ScheduleExecutionResult::to_json() const {
  using json = nlohmann::json;
  return json{{"schedule_id", schedule_id},
              {"success", success},
              {"error_message", error_message},
              {"data_point_count", data_point_count},
              {"execution_time_ms", execution_time_ms},
              {"target_names", target_names},
              {"successful_targets", successful_targets},
              {"failed_targets", failed_targets}};
}

} // namespace Schedule
} // namespace PulseOne
