/**
 * @file ExportService.cpp
 * @brief Export Gateway 메인 서비스 - v2.0 스키마 완전 적용
 * @author PulseOne Development Team
 * @date 2025-10-21
 * @version 2.1.1 (v2.0 스키마 - 통계 필드 제거)
 *
 * v2.0 변경사항:
 * - export_targets: 통계 필드 제거 (설정만)
 * - export_logs: 로그만 기록 (INSERT만)
 * - 통계 조회: export_logs 집계 (SELECT만)
 */

#include "Export/ExportService.h"
#include "CSP/FileTargetHandler.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/S3TargetHandler.h"
#include "Client/RedisClientImpl.h"
#include "Database/Entities/ExportLogEntity.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportTargetMappingEntity.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace PulseOne {
namespace Export {

ExportService::ExportService() {
  LogManager::getInstance().Info("ExportService 생성");
}

ExportService::~ExportService() {
  Stop();
  LogManager::getInstance().Info("ExportService 소멸");
}

bool ExportService::Initialize() {
  try {
    LogManager::getInstance().Info("ExportService 초기화 시작");

    redis_client_ = std::make_shared<RedisClientImpl>();

    if (!redis_client_->isConnected()) {
      LogManager::getInstance().Warn(
          "ExportService: Redis 초기 연결 실패, 백그라운드 재연결 진행 중");
    } else {
      LogManager::getInstance().Info("ExportService: Redis 연결 성공");
    }

    data_reader_ = std::make_unique<Shared::Data::DataReader>(redis_client_);
    data_writer_ = std::make_unique<Shared::Data::DataWriter>(redis_client_);

    http_handler_ = std::make_unique<CSP::HttpTargetHandler>();
    s3_handler_ = std::make_unique<CSP::S3TargetHandler>();
    file_handler_ = std::make_unique<CSP::FileTargetHandler>();

    export_interval_ms_ =
        ConfigManager::getInstance().getInt("EXPORT_INTERVAL_MS", 1000);
    batch_size_ = ConfigManager::getInstance().getInt("EXPORT_BATCH_SIZE", 100);
    max_retry_count_ =
        ConfigManager::getInstance().getInt("EXPORT_MAX_RETRY", 3);
    retry_delay_ms_ =
        ConfigManager::getInstance().getInt("EXPORT_RETRY_DELAY_MS", 1000);

    failed_data_dir_ = ConfigManager::getInstance().getOrDefault(
        "EXPORT_FAILED_DIR", "/app/data/export_failed");
    if (!fs::exists(failed_data_dir_)) {
      fs::create_directories(failed_data_dir_);
    }

    LogManager::getInstance().Info("ExportService: 초기화 완료");
    LogManager::getInstance().Info(
        "  - Export 간격: " + std::to_string(export_interval_ms_) + "ms");
    LogManager::getInstance().Info("  - 배치 크기: " +
                                   std::to_string(batch_size_));
    LogManager::getInstance().Info("  - 실패 데이터 저장 경로: " +
                                   failed_data_dir_);
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportService::Initialize 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

bool ExportService::Start() {
  if (is_running_.load()) {
    LogManager::getInstance().Warn("ExportService: 이미 실행 중");
    return false;
  }

  int target_count = LoadActiveTargets();
  LogManager::getInstance().Info(
      "ExportService: " + std::to_string(target_count) + "개 타겟 로드됨");

  if (target_count == 0) {
    LogManager::getInstance().Warn("ExportService: 활성화된 타겟이 없습니다");
  }

  is_running_.store(true);
  stats_.start_time = std::chrono::steady_clock::now();

  worker_thread_ =
      std::make_unique<std::thread>(&ExportService::workerThread, this);

  LogManager::getInstance().Info("ExportService: 시작됨 (간격: " +
                                 std::to_string(export_interval_ms_) + "ms)");
  return true;
}

void ExportService::Stop() {
  if (!is_running_.load()) {
    return;
  }

  LogManager::getInstance().Info("ExportService: 중지 중...");

  is_running_.store(false);

  if (worker_thread_ && worker_thread_->joinable()) {
    worker_thread_->join();
  }

  LogManager::getInstance().Info("ExportService: 중지 완료");
}

int ExportService::LoadActiveTargets() {
  std::lock_guard<std::mutex> lock(targets_mutex_);
  active_targets_.clear();

  try {
    auto &factory = Database::RepositoryFactory::getInstance();
    auto export_repo = factory.getExportTargetRepository();

    if (!export_repo) {
      LogManager::getInstance().Error(
          "ExportService: ExportTargetRepository 없음");
      return 0;
    }

    auto db_targets = export_repo->findByEnabled(true);

    for (const auto &db_target : db_targets) {
      ExportTargetConfig target;

      target.id = db_target.getId();
      target.name = db_target.getName();
      target.type = db_target.getTargetType();
      target.enabled = db_target.isEnabled();
      target.execution_order = db_target.getExecutionOrder(); // 🆕 추가
      target.endpoint = "";

      try {
        std::string config_str = db_target.getConfig();
        target.config = json::parse(config_str);

        if (target.config.contains("endpoint")) {
          target.endpoint = target.config["endpoint"].get<std::string>();
        } else if (target.config.contains("bucket_name")) {
          target.endpoint = target.config["bucket_name"].get<std::string>();
        }

      } catch (const json::exception &e) {
        LogManager::getInstance().Error("ExportService: 타겟 " + target.name +
                                        " config 파싱 실패: " + e.what());
        continue;
      }

      // ✅ v2.0: 통계 필드 제거 (export_logs에서 조회해야 함)
      target.success_count = 0;
      target.failure_count = 0;

      active_targets_.push_back(target);

      LogManager::getInstance().Info(
          "ExportService: 타겟 로드됨 - " + target.name + " (" + target.type +
          ", Order: " + std::to_string(target.execution_order) + ")");
    }

    // ✅ v3.1.2: execution_order 기준 정렬 (낮은 숫자가 높은 우선순위)
    std::sort(active_targets_.begin(), active_targets_.end(),
              [](const ExportTargetConfig &a, const ExportTargetConfig &b) {
                return a.execution_order < b.execution_order;
              });

    // ✅ v3.2.0: Payload Template 주입 로직
    // (active_targets_ 순회하며 DB에서 템플릿 로드)
    auto payload_repo = factory.getPayloadTemplateRepository();
    if (payload_repo) {
      for (auto &target : active_targets_) {
        // active_targets_에는 template_id가 없으므로 db_targets에서 찾아야 함
        // (ID로 매칭)
        auto it = std::find_if(
            db_targets.begin(), db_targets.end(),
            [&target](const auto &dbt) { return dbt.getId() == target.id; });

        if (it != db_targets.end()) {
          auto template_id_opt = it->getTemplateId();
          if (template_id_opt.has_value()) {
            int tid = template_id_opt.value();
            auto template_opt = payload_repo->findById(tid);

            if (template_opt.has_value()) {
              try {
                // 템플릿 JSON 파싱
                std::string body_json = template_opt->getTemplateJson();
                json body_template = json::parse(body_json);

                // config에 주입 (S3TargetHandler 등이 사용)
                target.config["body_template"] = body_template;

                LogManager::getInstance().Info(
                    "ExportService: Payload Template 주입 완료 - 타겟: " +
                    target.name + ", 템플릿ID: " + std::to_string(tid));
              } catch (const std::exception &e) {
                LogManager::getInstance().Error(
                    "ExportService: Payload Template JSON 파싱 실패 - " +
                    std::string(e.what()));
              }
            }
          }
        }
      }
    }

    LogManager::getInstance().Info(
        "ExportService: " + std::to_string(active_targets_.size()) +
        "개 타겟 로드 완료");
    return active_targets_.size();

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportService::LoadActiveTargets 실패: " +
                                    std::string(e.what()));
    return 0;
  }
}

bool ExportService::EnableTarget(int target_id, bool enable) {
  std::lock_guard<std::mutex> lock(targets_mutex_);

  auto it = std::find_if(
      active_targets_.begin(), active_targets_.end(),
      [target_id](const ExportTargetConfig &t) { return t.id == target_id; });

  if (it != active_targets_.end()) {
    it->enabled = enable;
    LogManager::getInstance().Info("ExportService: 타겟 " +
                                   std::to_string(target_id) +
                                   (enable ? " 활성화" : " 비활성화"));
    return true;
  }

  return false;
}

void ExportService::UpdateTargetStatus(int target_id,
                                       const std::string &status) {
  std::lock_guard<std::mutex> lock(targets_mutex_);

  auto it = std::find_if(
      active_targets_.begin(), active_targets_.end(),
      [target_id](const ExportTargetConfig &t) { return t.id == target_id; });

  if (it != active_targets_.end()) {
    it->status = status;
    it->last_update_time = std::chrono::system_clock::now();
  }
}

void ExportService::workerThread() {
  LogManager::getInstance().Info("ExportService: 워커 스레드 시작");

  while (is_running_.load()) {
    try {
      std::vector<ExportTargetConfig> targets;
      {
        std::lock_guard<std::mutex> lock(targets_mutex_);
        targets = active_targets_;
      }

      if (targets.empty()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(export_interval_ms_));
        continue;
      }

      std::vector<int> point_ids = collectPointIdsForExport(targets);

      if (point_ids.empty()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(export_interval_ms_));
        continue;
      }

      auto batch_result = data_reader_->ReadPointsBatch(point_ids);

      if (batch_result.values.empty()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(export_interval_ms_));
        continue;
      }

      LogManager::getInstance().Debug(
          "ExportService: " + std::to_string(batch_result.values.size()) +
          "개 데이터 포인트 읽음");

      for (const auto &target : targets) {
        if (!target.enabled) {
          continue;
        }

        bool success = exportToTargetWithRetry(target, batch_result.values);

        {
          std::lock_guard<std::mutex> lock(stats_mutex_);
          stats_.total_exports++;

          if (success) {
            stats_.successful_exports++;
            stats_.total_data_points += batch_result.values.size();
          } else {
            stats_.failed_exports++;
          }
        }
      }

    } catch (const std::exception &e) {
      LogManager::getInstance().Error("ExportService::workerThread 에러: " +
                                      std::string(e.what()));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
  }

  LogManager::getInstance().Info("ExportService: 워커 스레드 종료");
}

bool ExportService::exportToTargetWithRetry(
    const ExportTargetConfig &target,
    const std::vector<Shared::Data::CurrentValue> &data) {
  int retry_count = 0;
  bool success = false;
  std::string last_error;

  while (retry_count <= max_retry_count_ && !success) {
    try {
      if (retry_count > 0) {
        LogManager::getInstance().Warn(
            "ExportService: 재시도 " + std::to_string(retry_count) + "/" +
            std::to_string(max_retry_count_) + " - " + target.name);
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_));
      }

      success = exportToTarget(target, data);

      if (success) {
        logExportResult(target.id, "success", data.size(), "");

        if (retry_count > 0) {
          LogManager::getInstance().Info("ExportService: 재시도 성공 - " +
                                         target.name);
        }

        break;
      }

    } catch (const std::exception &e) {
      last_error = e.what();
      LogManager::getInstance().Error("ExportService: 전송 예외 - " +
                                      target.name + ": " + last_error);
    }

    retry_count++;
  }

  if (!success) {
    logExportResult(target.id, "failed", data.size(), last_error);
    saveFailedData(target, data, last_error);
    LogManager::getInstance().Error("ExportService: 전송 최종 실패 (" +
                                    std::to_string(max_retry_count_ + 1) +
                                    "회 시도) - " + target.name);
  }

  return success;
}

bool ExportService::exportToTarget(
    const ExportTargetConfig &target,
    const std::vector<Shared::Data::CurrentValue> &data) {
  try {
    json payload = json::array();
    for (const auto &value : data) {
      payload.push_back(value.toJson());
    }

    std::string json_str = payload.dump();

    bool success = false;

    if (target.type == "HTTP" || target.type == "HTTPS") {
      CSP::AlarmMessage dummy_alarm;
      auto result = http_handler_->sendAlarm(dummy_alarm, target.config);
      success = result.success;

    } else if (target.type == "S3") {
      CSP::AlarmMessage dummy_alarm;
      auto result = s3_handler_->sendAlarm(dummy_alarm, target.config);
      success = result.success;

    } else if (target.type == "FILE") {
      CSP::AlarmMessage dummy_alarm;
      auto result = file_handler_->sendAlarm(dummy_alarm, target.config);
      success = result.success;

    } else {
      LogManager::getInstance().Warn("ExportService: 알 수 없는 타겟 타입: " +
                                     target.type);
      return false;
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportService::exportToTarget 예외: " +
                                    std::string(e.what()));
    return false;
  }
}

void ExportService::saveFailedData(
    const ExportTargetConfig &target,
    const std::vector<Shared::Data::CurrentValue> &data,
    const std::string &error_message) {
  try {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);

    std::ostringstream filename;
    filename << "failed_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_"
             << "target" << target.id << "_" << data.size() << "points.json";

    std::string file_path = failed_data_dir_ + "/" + filename.str();

    json failed_record;
    failed_record["timestamp"] = std::time(nullptr);
    failed_record["target_id"] = target.id;
    failed_record["target_name"] = target.name;
    failed_record["target_type"] = target.type;
    failed_record["error_message"] = error_message;
    failed_record["retry_count"] = max_retry_count_ + 1;

    json data_array = json::array();
    for (const auto &value : data) {
      data_array.push_back(value.toJson());
    }
    failed_record["data"] = data_array;

    std::ofstream file(file_path);
    if (file.is_open()) {
      file << failed_record.dump(2);
      file.close();

      LogManager::getInstance().Info("ExportService: 실패 데이터 저장됨 - " +
                                     file_path);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportService::saveFailedData 예외: " +
                                    std::string(e.what()));
  }
}

std::vector<int> ExportService::collectPointIdsForExport(
    const std::vector<ExportTargetConfig> &targets) {
  std::set<int> point_ids_set;

  try {
    auto &factory = Database::RepositoryFactory::getInstance();
    auto mapping_repo = factory.getExportTargetMappingRepository();

    if (!mapping_repo) {
      LogManager::getInstance().Error("ExportService: MappingRepository 없음");
      return {};
    }

    for (const auto &target : targets) {
      if (!target.enabled) {
        continue;
      }

      auto mappings = mapping_repo->findByTargetId(target.id);

      for (const auto &mapping : mappings) {
        if (mapping.isEnabled()) {
          auto point_id = mapping.getPointId();
          if (point_id.has_value()) {
            point_ids_set.insert(point_id.value());
          }
        }
      }
    }

    std::vector<int> point_ids(point_ids_set.begin(), point_ids_set.end());

    if (!point_ids.empty()) {
      LogManager::getInstance().Debug(
          "ExportService: " + std::to_string(point_ids.size()) +
          "개 포인트 수집됨");
    }

    return point_ids;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error(
        "ExportService::collectPointIdsForExport 실패: " +
        std::string(e.what()));
    return {};
  }
}

void ExportService::logExportResult(int target_id, const std::string &status,
                                    int data_count,
                                    const std::string &error_message) {
  try {
    // ✅ v2.0: export_logs 테이블에 로그만 기록
    auto &factory = Database::RepositoryFactory::getInstance();
    auto log_repo = factory.getExportLogRepository();

    if (log_repo) {
      Database::Entities::ExportLogEntity log;
      log.setLogType("export");
      log.setTargetId(target_id);
      log.setStatus(status);
      log.setErrorMessage(error_message);

      log_repo->save(log);
    }

    // ✅ v2.0: export_targets 테이블 업데이트 제거
    //          (통계는 export_logs 집계로 조회)

    if (status == "success") {
      LogManager::getInstance().Info("ExportService: 전송 성공 - 타겟 " +
                                     std::to_string(target_id) + ", " +
                                     std::to_string(data_count) + "개 포인트");
    } else {
      LogManager::getInstance().Error("ExportService: 전송 실패 - 타겟 " +
                                      std::to_string(target_id) + ": " +
                                      error_message);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ExportService::logExportResult 예외: " +
                                    std::string(e.what()));
  }
}

ExportService::Statistics ExportService::GetStatistics() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void ExportService::ResetStatistics() {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  stats_.total_exports = 0;
  stats_.successful_exports = 0;
  stats_.failed_exports = 0;
  stats_.total_data_points = 0;
  stats_.start_time = std::chrono::steady_clock::now();

  LogManager::getInstance().Info("ExportService: 통계 초기화됨");
}

} // namespace Export
} // namespace PulseOne