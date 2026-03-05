//=============================================================================
// collector/src/Pipeline/DataProcessingService.cpp - 완전한 구현 파일 (컴파일
// 에러 수정)
//
// 🎯 목적: 헤더와 100% 일치하는 모든 함수 구현, 컴파일 에러 완전 수정
// 📋 특징:
//   - 올바른 처리 순서: 가상포인트 → 알람 → Redis 저장
//   - 모든 헤더 선언 함수 구현
//   - 컴파일 에러 완전 방지
//=============================================================================

#include "Pipeline/DataProcessingService.h"
#include "Alarm/AlarmManager.h"
#include "Client/InfluxClientImpl.h"
#include "Client/RedisClientImpl.h"
#include "Common/Enums.h"
#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/RepositoryFactory.h"
#include "Database/RuntimeSQLQueries.h"
using namespace PulseOne::Database::SQL;
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include "Pipeline/PipelineManager.h"
#include "Storage/RedisDataWriter.h"
#include "Utils/ConfigManager.h"
#include "VirtualPoint/VirtualPointBatchWriter.h"
#include "VirtualPoint/VirtualPointEngine.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

// Includes for stages
#include "Pipeline/Stages/AlarmStage.h"
#include "Pipeline/Stages/EnrichmentStage.h"
#include "Pipeline/Stages/PersistenceStage.h"

using LogLevel = PulseOne::Enums::LogLevel;
using json = nlohmann::json;

namespace PulseOne {
namespace Pipeline {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DataProcessingService::DataProcessingService()
    : vp_batch_writer_(
          std::make_unique<VirtualPoint::VirtualPointBatchWriter>(100, 30)),
      should_stop_(false), is_running_(false),
      thread_count_(std::thread::hardware_concurrency()), batch_size_(100) {

  try {
    if (thread_count_ == 0)
      thread_count_ = 1;
    else if (thread_count_ > 16)
      thread_count_ = 16;

    // RedisDataWriter가 자체적으로 Redis 연결 생성
    redis_data_writer_ = std::make_unique<Storage::RedisDataWriter>();

    // InfluxClient 활성화 (ROS 연동 등 시계열 데이터 저장용)
    try {
      auto &db_manager = DbLib::DatabaseManager::getInstance();
      // InfluxDB 설정 확인 및 클라이언트 생성
      auto *influx = new PulseOne::Client::InfluxClientImpl();
      influx_client_ = std::shared_ptr<PulseOne::Client::InfluxClient>(influx);

      // Note: 실제 연결은 ConfigManager가 로드된 후에 수행되어야 함.
      // 여기서는 일단 인스턴스만 생성하고, Start() 등에서 연결 상태를
      // 확인하거나 DatabaseManager의 설정을 따르도록 함.
      LogManager::getInstance().log("processing", LogLevel::INFO,
                                    "InfluxClient 초기화 완료 (In-Process)");
    } catch (const std::exception &e) {
      LogManager::getInstance().log("processing", LogLevel::WARN,
                                    "InfluxClient 초기화 실패: " +
                                        std::string(e.what()));
    }

    LogManager::getInstance().log("processing", LogLevel::INFO,
                                  "DataProcessingService 생성 완료");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "DataProcessingService 생성 중 예외: " +
                                      std::string(e.what()));
  }
}

DataProcessingService::~DataProcessingService() {
  Stop();
  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "DataProcessingService 소멸됨");
}

// =============================================================================
// 서비스 제어
// =============================================================================

bool DataProcessingService::Start() {
  if (is_running_.load()) {
    LogManager::getInstance().log("processing", LogLevel::WARN,
                                  "DataProcessingService가 이미 실행 중입니다");
    return false;
  }

  // PipelineManager 의존성 확인 (필수)
  auto &pipeline_manager = PipelineManager::getInstance();
  if (!pipeline_manager.IsRunning()) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "PipelineManager가 실행되지 않았습니다!");
    return false;
  }

  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "DataProcessingService 시작 중...");

  // VirtualPointBatchWriter 시작 (선택적)
  if (vp_batch_writer_ && !vp_batch_writer_->Start()) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "VirtualPointBatchWriter 시작 실패");
    return false;
  }

  should_stop_ = false;
  is_running_ = true;

  // 스레드 풀 시작
  processing_threads_.reserve(thread_count_);
  for (size_t i = 0; i < thread_count_; ++i) {
    processing_threads_.emplace_back(
        &DataProcessingService::ProcessingThreadLoop, this, i);
  }

  // Persistence 스레드 시작
  persistence_thread_ =
      std::thread(&DataProcessingService::PersistenceThreadLoop, this);

  // 🔥 주기 Redis→SQLite 동기화 스레드 시작 (아날로그 포인트 배치 저장)
  rdb_sync_thread_ =
      std::thread(&DataProcessingService::RdbSyncThreadLoop, this);

  // Manager 초기화 (명시적)
  PulseOne::VirtualPoint::VirtualPointEngine::getInstance().initialize();
  PulseOne::Alarm::AlarmManager::getInstance().initialize();

  // Pipeline 초기화
  InitializePipeline();

  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "DataProcessingService 시작 완료 (Threads: " +
                                    std::to_string(thread_count_) + ")");

  // InfluxDB 연결 시도 (환경 변수 우선)
  if (influx_client_) {
    try {
      auto &config = ConfigManager::getInstance();

      // 환경 변수 우선순위 최상위 (Docker Compose 및 verify_e2e 스크립트 호환)
      const char *env_url = std::getenv("INFLUX_URL");
      const char *env_host = std::getenv("INFLUXDB_HOST");
      const char *env_port = std::getenv("INFLUXDB_PORT");
      const char *env_token = std::getenv("INFLUXDB_TOKEN");
      const char *env_org = std::getenv("INFLUXDB_ORG");
      const char *env_bucket = std::getenv("INFLUXDB_BUCKET");

      // URL 구성 (INFLUX_URL이 있으면 그것을 쓰고, 없으면 HOST:PORT 조합)
      std::string url;
      if (env_url && strlen(env_url) > 0) {
        url = env_url;
      } else {
        std::string host =
            env_host ? env_host
                     : config.getOrDefault("INFLUXDB_HOST", "localhost");
        std::string port =
            env_port ? env_port : config.getOrDefault("INFLUXDB_PORT", "8086");
        url = "http://" + host + ":" + port;
      }

      std::string token =
          env_token ? env_token
                    : config.getOrDefault("INFLUXDB_TOKEN",
                                          "my-super-secret-auth-token");
      std::string org =
          env_org ? env_org : config.getOrDefault("INFLUXDB_ORG", "pulseone");
      std::string bucket =
          env_bucket ? env_bucket
                     : config.getOrDefault("INFLUXDB_BUCKET", "timeseries");

      LogManager::getInstance().log("processing", LogLevel::INFO,
                                    "InfluxDB 연결 시도: " + url + " (Org: " +
                                        org + ", Bucket: " + bucket + ")");

      // 명시적 연결 확인 및 로깅 강화
      if (influx_client_->connect(url, token, org, bucket)) {
        LogManager::getInstance().log(
            "processing", LogLevel::INFO,
            "InfluxDB 연결 성공! (Persistence 활성화됨)");
      } else {
        LogManager::getInstance().log(
            "processing", LogLevel::WARN,
            "InfluxDB 연결 실패! (데이터는 유실될 수 있습니다: " + url + ")");
      }
    } catch (const std::exception &e) {
      LogManager::getInstance().log("processing", LogLevel::WARN,
                                    "InfluxDB 연결 중 예외 발생: " +
                                        std::string(e.what()));
    }
  }

  return true;
}

void DataProcessingService::Stop() {
  if (!is_running_.load()) {
    return;
  }

  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "DataProcessingService 중지 중...");

  // 1. 중지 플래그 설정
  should_stop_ = true;

  // 2. VirtualPointBatchWriter 먼저 중지
  if (vp_batch_writer_) {
    vp_batch_writer_->Stop();
  }

  // 3. PipelineManager 중지 신호를 통해 GetBatch 대기 해제
  PipelineManager::getInstance().Shutdown();

  // 4. 스레드 종료 대기
  for (size_t i = 0; i < processing_threads_.size(); ++i) {
    if (processing_threads_[i].joinable()) {
      processing_threads_[i].join();
    }
  }
  processing_threads_.clear();

  // Persistence 스레드 종료
  if (persistence_thread_.joinable()) {
    persistence_thread_.join();
  }

  // 🔥 종료 전 Redis 모든 current_values → SQLite 일괄 플러시
  FlushCurrentValuesToSQLite();

  // RDB 동기화 스레드 종료
  if (rdb_sync_thread_.joinable()) {
    rdb_sync_thread_.join();
  }

  is_running_.store(false);
  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "DataProcessingService 중지 완료");
}

void DataProcessingService::SetThreadCount(size_t thread_count) {
  if (is_running_.load()) {
    LogManager::getInstance().log(
        "processing", LogLevel::WARN,
        "서비스 실행 중에는 스레드 수를 변경할 수 없습니다");
    return;
  }

  if (thread_count == 0)
    thread_count = 1;
  else if (thread_count > 32)
    thread_count = 32;

  thread_count_ = thread_count;
  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "스레드 수 설정: " +
                                    std::to_string(thread_count_));
}

DataProcessingService::ServiceConfig DataProcessingService::GetConfig() const {
  ServiceConfig config;
  config.thread_count = thread_count_;
  config.batch_size = batch_size_;
  config.alarm_evaluation_enabled = alarm_evaluation_enabled_.load();
  config.virtual_point_calculation_enabled =
      virtual_point_calculation_enabled_.load();
  config.external_notification_enabled = external_notification_enabled_.load();
  return config;
}

// =============================================================================
// 멀티스레드 처리 루프
// =============================================================================

void DataProcessingService::ProcessingThreadLoop(size_t thread_index) {
  LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL,
                                "처리 스레드 " + std::to_string(thread_index) +
                                    " 시작");

  while (!should_stop_.load()) {
    try {
      auto batch = CollectBatchFromPipelineManager();

      if (!batch.empty()) {
        auto start_time = std::chrono::high_resolution_clock::now();
        ProcessBatch(batch, thread_index);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        UpdateStatistics(batch.size(), static_cast<double>(duration.count()));
      } else {
        // [CRITICAL FIX] static은 모든 스레드가 공유 → 데이터 레이스 발생
        // thread_local로 교체: 스레드별 독립 카운터 사용
        thread_local int empty_count = 0;
        if (++empty_count >= 100) { // Every ~1 second
          LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL,
                                        "Thread " +
                                            std::to_string(thread_index) +
                                            " waiting for data...");
          empty_count = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

    } catch (const std::exception &e) {
      processing_errors_.fetch_add(1);
      HandleError("스레드 " + std::to_string(thread_index) + " 처리 중 예외",
                  e.what());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL,
                                "처리 스레드 " + std::to_string(thread_index) +
                                    " 종료");
}

void DataProcessingService::PersistenceThreadLoop() {
  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "Persistence 스레드 시작");

  while (!should_stop_.load() || !persistence_queue_.empty()) {
    try {
      // 배치로 태스크 수집 (최대 100개씩 혹은 100ms 대기)
      auto tasks = persistence_queue_.pop_batch(100, 100);
      if (tasks.empty()) {
        if (should_stop_.load())
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      // RDB와 InfluxDB 태스크 분리
      std::vector<PersistenceTask> rdb_tasks;
      std::vector<PersistenceTask> influx_tasks;
      std::vector<PersistenceTask> comm_stats_tasks;

      for (auto &task : tasks) {
        switch (task.type) {
        case PersistenceTask::Type::RDB_SAVE:
          rdb_tasks.push_back(std::move(task));
          break;
        case PersistenceTask::Type::INFLUX_SAVE:
          influx_tasks.push_back(std::move(task));
          break;
        case PersistenceTask::Type::COMM_STATS_SAVE:
          comm_stats_tasks.push_back(std::move(task));
          break;
        }
      }

      // 개별 처리 메서드 호출 (컴파일러 부담 완화 및 코드 명확화)
      if (!rdb_tasks.empty())
        ProcessRDBTasks(rdb_tasks);
      if (!influx_tasks.empty())
        ProcessInfluxTasks(influx_tasks);
      if (!comm_stats_tasks.empty())
        ProcessCommStatsTasks(comm_stats_tasks);

    } catch (const std::exception &e) {
      LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                    "PersistenceThreadLoop 에러: " +
                                        std::string(e.what()));
    }
  }

  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "Persistence 스레드 종료");
}

void DataProcessingService::ProcessRDBTasks(
    const std::vector<PersistenceTask> &rdb_tasks) {
  auto &factory = PulseOne::Database::RepositoryFactory::getInstance();
  auto current_value_repo = factory.getCurrentValueRepository();

  if (!current_value_repo)
    return;

  // 🔥 모든 task의 포인트를 한 번에 모아 saveBatch() 1회 호출
  // (기존: 포인트당 개별 트랜잭션 → 30K 포인트 = 30K 트랜잭션 → 느림)
  // (개선: 전체 포인트를 1 트랜잭션으로 → 30K 포인트 = 1 트랜잭션 → 빠름)
  std::vector<PulseOne::Database::Entities::CurrentValueEntity> batch_entities;

  size_t total_points = 0;
  for (const auto &task : rdb_tasks) {
    total_points += task.points.size();
  }
  batch_entities.reserve(total_points);

  for (const auto &task : rdb_tasks) {
    for (const auto &point : task.points) {
      try {
        auto entity = ConvertToCurrentValueEntity(point, task.message);
        batch_entities.push_back(std::move(entity));
      } catch (...) {
        // 변환 실패 포인트는 건너뜀
      }
    }
  }

  if (batch_entities.empty())
    return;

  // 🔒 SQLite 직렬화: FlushCurrentValuesToSQLite와 동시 executeBatch() 호출
  // 방지
  std::lock_guard<std::mutex> lock(sqlite_write_mutex_);
  size_t saved = current_value_repo->saveBatch(batch_entities);

  if (saved > 0) {
    LogManager::getInstance().log(
        "processing", LogLevel::DEBUG_LEVEL,
        "🔥 디지털 RDB 즉시 저장: " + std::to_string(saved) + "개 포인트");
  }
}

// =============================================================================
// 🔥 주기 Redis→SQLite 동기화 (아날로그 포인트 배치 저장)
// =============================================================================

void DataProcessingService::RdbSyncThreadLoop() {
  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "RDB Sync 스레드 시작");

  while (!should_stop_.load()) {
    // 설정된 주기만큼 대기 (1초 단위로 깨어나서 중지 여부 확인)
    int interval_s = rdb_sync_interval_s_.load();
    if (interval_s <= 0)
      interval_s = 60;

    for (int i = 0; i < interval_s * 10 && !should_stop_.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (should_stop_.load())
      break;

    // Redis 전체 스캔 → current_values 배치 저장
    FlushCurrentValuesToSQLite();
  }

  LogManager::getInstance().log("processing", LogLevel::INFO,
                                "RDB Sync 스레드 종료");
}

void DataProcessingService::FlushCurrentValuesToSQLite() {
  // Redis keys("point:*:current") + mget() 2회 호출로 모든 포인트 최신값 배치
  // 조회 → 기존: 포인트마다 GET N회, 개선: 2회 왕복으로 30K포인트 처리
  auto &factory = PulseOne::Database::RepositoryFactory::getInstance();
  auto current_value_repo = factory.getCurrentValueRepository();
  if (!current_value_repo)
    return;

  auto &db_manager = DbLib::DatabaseManager::getInstance();
  auto *redis = db_manager.getRedisClient();
  if (!redis) {
    LogManager::getInstance().log(
        "processing", LogLevel::DEBUG_LEVEL,
        "FlushCurrentValuesToSQLite: Redis 없음, 스킵");
    return;
  }

  try {
    // 1) SCAN으로 논블로킹 키 순회 (KEYS 대신 사용 - 30K+ 키 안전)
    auto keys = redis->scan("point:*:current", 200);
    if (keys.empty())
      return;

    // 2) MGET으로 모든 값 한 번에 조회 (1회 왕복)
    auto values = redis->mget(keys);

    std::vector<PulseOne::Database::Entities::CurrentValueEntity> batch;
    batch.reserve(keys.size());

    for (size_t i = 0; i < keys.size() && i < values.size(); ++i) {
      if (values[i].empty())
        continue;

      // 키에서 point_id 추출: "point:12345:current" → 12345
      int point_id = 0;
      try {
        const auto &k = keys[i];
        auto first_colon = k.find(':');
        auto second_colon = k.find(':', first_colon + 1);
        if (first_colon == std::string::npos ||
            second_colon == std::string::npos)
          continue;
        point_id = std::stoi(
            k.substr(first_colon + 1, second_colon - first_colon - 1));
      } catch (...) {
        continue;
      }

      try {
        auto j = nlohmann::json::parse(values[i]);
        PulseOne::Database::Entities::CurrentValueEntity entity;
        entity.setPointId(point_id);
        entity.setCurrentValue(j.value("value", ""));
        entity.setRawValue(j.value("value", ""));
        entity.setValueType("number");
        entity.setQuality(
            static_cast<PulseOne::Enums::DataQuality>(j.value("quality", 0)));
        entity.setValueTimestamp(PulseOne::Utils::GetCurrentTimestamp());
        batch.push_back(std::move(entity));
      } catch (...) {
        // JSON 파싱 실패 시 스킵
      }
    }

    if (batch.empty())
      return;

    // 🔒 SQLite 직렬화: ProcessRDBTasks와 동시 executeBatch() 호출 방지
    std::lock_guard<std::mutex> lock(sqlite_write_mutex_);
    size_t saved = current_value_repo->saveBatch(batch);
    LogManager::getInstance().log(
        "processing", LogLevel::INFO,
        "🔄 Redis→SQLite 주기 동기화: " + std::to_string(saved) + "/" +
            std::to_string(batch.size()) + "개 포인트 저장 (SCAN+MGET)");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::WARN,
                                  "FlushCurrentValuesToSQLite 실패: " +
                                      std::string(e.what()));
  }
}

void DataProcessingService::ProcessInfluxTasks(
    const std::vector<PersistenceTask> &influx_tasks) {
  if (!influx_client_)
    return;

  std::vector<std::string> batch_lines;
  batch_lines.reserve(influx_tasks.size() * 5); // Conservative estimate

  for (const auto &task : influx_tasks) {
    const auto &msg = task.message;
    std::string measurement = "device_telemetry";

    for (const auto &point : task.points) {
      std::map<std::string, std::string> tags;
      tags["device_id"] = msg.device_id;
      tags["protocol"] = msg.protocol;
      tags["tenant_id"] = std::to_string(msg.tenant_id);
      tags["site_id"] = std::to_string(msg.site_id);
      tags["point_id"] = std::to_string(point.point_id);
      tags["point_name"] = getPointName(point.point_id);

      std::map<std::string, double> fields;

      std::visit(
          [&](const auto &val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
              fields["p_" + std::to_string(point.point_id)] =
                  static_cast<double>(val);
            }
          },
          point.value);

      if (!fields.empty()) {
        // [BUG #25 FIX] Use actual measurement timestamp instead of now().
        // Convert point.timestamp (system_clock::time_point) to epoch seconds.
        long long epoch_s = std::chrono::duration_cast<std::chrono::seconds>(
                                point.timestamp.time_since_epoch())
                                .count();

        // Build line protocol manually so we can pass the real timestamp.
        // Tags are already escaped inside formatLineProtocol.
        std::string measurement_escaped = "device_telemetry";
        std::ostringstream line;
        line << measurement_escaped;
        for (const auto &[k, v] : tags) {
          // Inline escape: space→\ , comma→\, , equals→\=
          auto esc = [](const std::string &s) {
            std::string o;
            o.reserve(s.size() + 4);
            for (char c : s) {
              if (c == ' ')
                o += "\\ ";
              else if (c == ',')
                o += "\\,";
              else if (c == '=')
                o += "\\=";
              else
                o += c;
            }
            return o;
          };
          line << "," << esc(k) << "=" << esc(v);
        }
        line << " ";
        bool first_f = true;
        for (const auto &[k, v] : fields) {
          if (!first_f)
            line << ",";
          line << k << "=" << v;
          first_f = false;
        }
        if (epoch_s > 0)
          line << " " << epoch_s;

        batch_lines.push_back(line.str());
      }
    }
  }

  if (!batch_lines.empty()) {
    if (influx_client_->writeBatch(batch_lines)) {
      influx_writes_.fetch_add(batch_lines.size());
    }
  }
}

void DataProcessingService::ProcessCommStatsTasks(
    const std::vector<PersistenceTask> &comm_stats_tasks) {
  for (const auto &task : comm_stats_tasks) {
    const auto &msg = task.message;

    std::map<std::string, std::string> tags;
    tags["device_id"] = msg.device_id;
    tags["protocol"] = msg.protocol;
    tags["tenant_id"] = std::to_string(msg.tenant_id);

    std::map<std::string, double> fields;
    fields["attempts"] = static_cast<double>(msg.total_attempts);
    fields["failures"] = static_cast<double>(msg.total_failures);
    fields["latency_ms"] = static_cast<double>(msg.response_time.count());

    double success_rate =
        (msg.total_attempts > 0)
            ? ((double)(msg.total_attempts - msg.total_failures) /
               msg.total_attempts * 100.0)
            : 0.0;
    fields["success_rate"] = success_rate;

    if (influx_client_) {
      influx_client_->writeRecord("comm_stats", tags, fields);
    }

    // 🔧 E2E 스크립트 및 백엔드 호환성: device_status 테이블 상태 업데이트
    // (UPSERT)
    try {
      auto &db_mgr = DbLib::DatabaseManager::getInstance();

      // 상태 매핑 개선
      std::string status_str;
      switch (msg.device_status) {
      case PulseOne::Enums::DeviceStatus::ONLINE:
        status_str = "connected";
        break;
      case PulseOne::Enums::DeviceStatus::OFFLINE:
        status_str = "disconnected";
        break;
      case PulseOne::Enums::DeviceStatus::DEVICE_ERROR:
        status_str = "error";
        break;
      case PulseOne::Enums::DeviceStatus::WARNING:
        status_str = "connected";
        break;
      default:
        status_str = "disconnected";
        break;
      }

      bool is_online =
          (msg.device_status == PulseOne::Enums::DeviceStatus::ONLINE ||
           msg.device_status == PulseOne::Enums::DeviceStatus::WARNING);

      // [CRITICAL FIX] SQL 인젝션 방어: msg.device_id를 직접 쿼리에 삽입하면
      // 위험 device_id를 숫자만 추출하여 안전한 정수 문자열로 변환
      std::string safe_device_id;
      try {
        // 숫자 부분만 추출 ("device_42" → "42", "42" → "42")
        size_t last_underscore = msg.device_id.find_last_of('_');
        std::string raw_num = (last_underscore != std::string::npos)
                                  ? msg.device_id.substr(last_underscore + 1)
                                  : msg.device_id;
        safe_device_id = std::to_string(std::stoi(raw_num));
      } catch (...) {
        // 파싱 실패 시 해당 레코드 스킵 (0 삽입 대신 건너뜀)
        LogManager::getInstance().log("processing", LogLevel::WARN,
                                      "Invalid device_id for SQL, skipping: " +
                                          msg.device_id);
        continue;
      }

      // ✅ RuntimeSQLQueries::DeviceStatus::UPSERT_STATUS
      db_mgr.executeNonQuery(Runtime::DeviceStatus::UPSERT_STATUS(
          safe_device_id, status_str, msg.response_time.count(), is_online));
    } catch (...) {
    }
  }
}

std::vector<Structs::DeviceDataMessage>
DataProcessingService::CollectBatchFromPipelineManager() {
  auto &pipeline_manager = PipelineManager::getInstance();
  return pipeline_manager.GetBatch(batch_size_, 100);
}

// =============================================================================
// 메인 처리 메서드 - 올바른 순서로 수정
// =============================================================================

void DataProcessingService::InitializePipeline() {
  LogManager::getInstance().Info("Beginning InitializePipeline");

  pipeline_stages_.clear();

  // 1. Enrichment Stage
  pipeline_stages_.push_back(std::make_unique<Stages::EnrichmentStage>());

  // 2. Alarm Stage
  pipeline_stages_.push_back(std::make_unique<Stages::AlarmStage>());

  // DataProxy Wrappers
  auto redis_deleter = [](Storage::RedisDataWriter *p) {};
  std::shared_ptr<Storage::RedisDataWriter> redis_proxy(
      redis_data_writer_.get(), redis_deleter);

  auto queue_deleter = [](IPersistenceQueue *p) {};
  std::shared_ptr<IPersistenceQueue> queue_proxy(this, queue_deleter);

  pipeline_stages_.push_back(
      std::make_unique<Stages::PersistenceStage>(redis_proxy, queue_proxy));

  LogManager::getInstance().Info("Pipeline initialized with " +
                                 std::to_string(pipeline_stages_.size()) +
                                 " stages.");
}

void DataProcessingService::ProcessBatch(
    const std::vector<Structs::DeviceDataMessage> &batch, size_t thread_index) {

  if (batch.empty())
    return;

  auto start_time = std::chrono::high_resolution_clock::now();

  try {
    LogManager::getInstance().log(
        "processing", LogLevel::INFO,
        "ProcessBatch Pipeline Start: " + std::to_string(batch.size()) +
            " messages (Thread " + std::to_string(thread_index) + ")");

    size_t processed_count = 0;

    for (const auto &message : batch) {
      try {

        // Initialize Context
        PipelineContext context(message);
        context.should_evaluate_alarms = alarm_evaluation_enabled_.load();

        // Execute Pipeline
        for (auto &stage : pipeline_stages_) {
          if (!stage->Process(context)) {
            break; // Stop if stage returns false
          }
        }

        // Update Global Counters from Context Stats
        if (context.stats.virtual_points_added > 0)
          virtual_points_calculated_.fetch_add(
              context.stats.virtual_points_added);
        if (context.stats.alarms_triggered > 0)
          alarms_evaluated_.fetch_add(1); // Approximate
        if (context.stats.persisted_to_redis)
          redis_writes_.fetch_add(1); // Per message or point?

        processed_count++;

      } catch (const std::exception &e) {
        LogManager::getInstance().Error(
            "Pipeline Error (device=" + message.device_id +
            "): " + std::string(e.what()));
        processing_errors_.fetch_add(1);
      }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    UpdateStatistics(processed_count, static_cast<double>(duration.count()));

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ProcessBatch Critical Error: " +
                                    std::string(e.what()));
    processing_errors_.fetch_add(batch.size());
  }
}

// IPersistenceQueue Implementation
void DataProcessingService::QueueRDBTask(
    const Structs::DeviceDataMessage &message,
    const std::vector<Structs::TimestampedValue> &points) {

  // 🎯 디지털(bool) 포인트 중 값이 변경된 것만 즉시 SQLite에 저장
  // 아날로그는 RDB 큐에 넣지 않음 → RdbSyncThreadLoop이 주기적으로 처리
  std::vector<Structs::TimestampedValue> digital_changed;
  for (const auto &p : points) {
    if (std::holds_alternative<bool>(p.value) && p.value_changed) {
      digital_changed.push_back(p);
    }
  }

  if (digital_changed.empty())
    return;

  PersistenceTask task;
  task.type = PersistenceTask::Type::RDB_SAVE;
  task.message = message;
  task.points = std::move(digital_changed);

  // Backpressure Protection: 큐가 가득 차면 데이터를 버림 (10,000개 제한)
  if (!persistence_queue_.try_push(std::move(task), 10000)) {
    static std::atomic<int> drop_counter{0};
    int count = ++drop_counter;
    if (count % 100 == 1) {
      LogManager::getInstance().log(
          "processing", LogLevel::WARN,
          "Persistence Queue FULL. Dropping digital RDB data! (Count: " +
              std::to_string(count) + ")");
    }
  }
}

void DataProcessingService::QueueInfluxTask(
    const Structs::DeviceDataMessage &message,
    const std::vector<Structs::TimestampedValue> &points) {

  int interval_ms = influxdb_storage_interval_ms_.load();
  std::vector<Structs::TimestampedValue> filtered_points;

  if (interval_ms <= 0) {
    // 필터링 없이 모두 저장
    filtered_points = points;
  } else {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(influxd_save_mutex_);

    for (const auto &p : points) {
      bool should_save = false;

      // bool이나 string 같은 상태성 데이터는 주기 상관없이 항상 저장
      if (std::holds_alternative<bool>(p.value) ||
          std::holds_alternative<std::string>(p.value)) {
        should_save = true;
      } else {
        // 아날로그 데이터의 경우 주기 체크
        auto it = last_influxd_save_times_.find(p.point_id);
        if (it == last_influxd_save_times_.end()) {
          should_save = true;
        } else {
          auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now - it->second)
                             .count();
          if (elapsed >= interval_ms) {
            should_save = true;
          }
        }
      }

      if (should_save) {
        filtered_points.push_back(p);
        last_influxd_save_times_[p.point_id] = now;
      }
    }
  }

  if (filtered_points.empty()) {
    return; // 저장할 포인트가 없으면 스킵
  }

  PersistenceTask task;
  task.type = PersistenceTask::Type::INFLUX_SAVE;
  task.message = message;
  task.points = filtered_points;
  // Backpressure Protection
  if (!persistence_queue_.try_push(std::move(task), 10000)) {
    // ...logging logic remains...
    static std::atomic<int> drop_counter{0};
    int count = ++drop_counter;
    if (count % 100 == 1) {
      LogManager::getInstance().log("processing", LogLevel::WARN,
                                    "Persistence Queue FULL (10,000 items). "
                                    "Dropping InfluxDB data! (Count: " +
                                        std::to_string(count) + ")");
    }
  }
}

void DataProcessingService::QueueCommStatsTask(
    const Structs::DeviceDataMessage &message) {
  PersistenceTask task;
  task.type = PersistenceTask::Type::COMM_STATS_SAVE;
  task.message = message;

  // Backpressure Protection
  if (!persistence_queue_.try_push(std::move(task), 10000)) {
    // Comm stats are less critical, drop silently or verbose log
  }
}

// =============================================================================
// 가상포인트 처리
// =============================================================================

Structs::DeviceDataMessage
DataProcessingService::CalculateVirtualPointsAndEnrich(
    const Structs::DeviceDataMessage &original_message) {

  try {
    auto &vp_engine = VirtualPoint::VirtualPointEngine::getInstance();

    if (!vp_engine.isInitialized()) {
      return original_message;
    }

    auto vp_results = vp_engine.calculateForMessage(original_message);

    if (vp_results.empty()) {
      return original_message;
    }

    // 메시지 확장
    auto enriched_data = original_message;
    for (const auto &vp : vp_results) {
      enriched_data.points.push_back(vp);
    }

    LogManager::getInstance().log(
        "processing", LogLevel::INFO,
        "메시지 확장 완료: " + std::to_string(enriched_data.points.size()) +
            "개 포인트 (가상포인트 " + std::to_string(vp_results.size()) +
            "개 추가됨)");

    return enriched_data;

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "가상포인트 계산 실패: " +
                                      std::string(e.what()));
    return original_message;
  }
}

std::vector<Structs::TimestampedValue>
DataProcessingService::CalculateVirtualPoints(
    const std::vector<Structs::DeviceDataMessage> &batch) {

  std::vector<Structs::TimestampedValue> enriched_data;

  try {
    LogManager::getInstance().log(
        "processing", LogLevel::DEBUG_LEVEL,
        "가상포인트 계산 시작: " + std::to_string(batch.size()) + "개 메시지");

    // 메시지 내용 상세 분석
    int total_points = 0;
    int virtual_point_count = 0;

    for (const auto &device_msg : batch) {
      total_points += device_msg.points.size();

      // 각 포인트의 is_virtual_point 플래그 확인
      for (const auto &point : device_msg.points) {
        if (point.is_virtual_point) {
          virtual_point_count++;
          LogManager::getInstance().log(
              "processing", LogLevel::INFO,
              "가상포인트 발견: point_id=" + std::to_string(point.point_id) +
                  ", source=" + point.source);
        }
      }
    }

    LogManager::getInstance().log(
        "processing", LogLevel::INFO,
        "포인트 분석: 총 " + std::to_string(total_points) + "개, " +
            "가상포인트 " + std::to_string(virtual_point_count) + "개");

    // 원본 데이터를 enriched_data에 복사
    for (const auto &device_msg : batch) {
      auto converted = ConvertToTimestampedValues(device_msg);
      enriched_data.insert(enriched_data.end(), converted.begin(),
                           converted.end());
    }

    // VirtualPointEngine 초기화 상태 확인
    auto &vp_engine = VirtualPoint::VirtualPointEngine::getInstance();

    if (!vp_engine.isInitialized()) {
      LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                    "VirtualPointEngine이 초기화되지 않음!");

      // 강제 초기화 시도
      try {
        bool init_result = vp_engine.initialize();
        if (init_result) {
          LogManager::getInstance().log("processing", LogLevel::INFO,
                                        "VirtualPointEngine 강제 초기화 성공");
        } else {
          LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                        "VirtualPointEngine 강제 초기화 실패");
          return enriched_data;
        }
      } catch (const std::exception &e) {
        LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                      "VirtualPointEngine 초기화 예외: " +
                                          std::string(e.what()));
        return enriched_data;
      }
    }

    // 가상포인트 계산 실행
    size_t virtual_points_calculated = 0;

    for (const auto &device_msg : batch) {
      try {
        LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL,
                                      "device_id=" + device_msg.device_id +
                                          " 가상포인트 계산 시작");

        auto vp_results = vp_engine.calculateForMessage(device_msg);

        LogManager::getInstance().log(
            "processing", LogLevel::INFO,
            "device_id=" + device_msg.device_id + "에서 " +
                std::to_string(vp_results.size()) + "개 가상포인트 계산됨");

        for (const auto &vp_result : vp_results) {
          // 가상포인트임을 명시적으로 표시
          auto virtual_point_data = vp_result;
          virtual_point_data.is_virtual_point = true;
          virtual_point_data.source = "VirtualPointEngine";

          enriched_data.push_back(virtual_point_data);

          // Redis 저장
          if (redis_data_writer_ && redis_data_writer_->IsConnected()) {
            StoreVirtualPointToRedis(virtual_point_data);
          }

          // 비동기 큐에 추가
          if (vp_batch_writer_) {
            vp_batch_writer_->QueueVirtualPointResult(virtual_point_data);
          }

          LogManager::getInstance().log("processing", LogLevel::INFO,
                                        "가상포인트 저장 완료: point_id=" +
                                            std::to_string(vp_result.point_id));
        }

        virtual_points_calculated += vp_results.size();

      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "processing", LogLevel::LOG_ERROR,
            "가상포인트 계산 실패 (device=" + device_msg.device_id +
                "): " + std::string(e.what()));
      }
    }

    if (virtual_points_calculated > 0) {
      LogManager::getInstance().log(
          "processing", LogLevel::INFO,
          "가상포인트 계산 성공: " + std::to_string(virtual_points_calculated) +
              "개");
    } else {
      LogManager::getInstance().log(
          "processing", LogLevel::WARN,
          "계산된 가상포인트 없음 - 다음 사항을 확인하세요:");
      LogManager::getInstance().log("processing", LogLevel::WARN,
                                    "   1. DB에 활성화된 가상포인트가 있는가?");
      LogManager::getInstance().log(
          "processing", LogLevel::WARN,
          "   2. 입력 데이터포인트가 가상포인트 종속성과 일치하는가?");
      LogManager::getInstance().log(
          "processing", LogLevel::WARN,
          "   3. VirtualPointEngine이 제대로 로드되었는가?");
    }

    LogManager::getInstance().log("processing", LogLevel::INFO,
                                  "가상포인트 처리 완료: 총 " +
                                      std::to_string(enriched_data.size()) +
                                      "개 데이터 (원본 + 가상포인트)");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "가상포인트 계산 전체 실패: " +
                                      std::string(e.what()));
  }

  return enriched_data;
}

// =============================================================================
// 알람 처리
// =============================================================================

void DataProcessingService::EvaluateAlarms(
    const std::vector<Structs::DeviceDataMessage> &batch, size_t thread_index) {

  if (batch.empty()) {
    return;
  }

  try {
    auto &alarm_manager = PulseOne::Alarm::AlarmManager::getInstance();

    if (!alarm_manager.isInitialized()) {
      LogManager::getInstance().log(
          "processing", LogLevel::DEBUG_LEVEL,
          "AlarmManager가 초기화되지 않음 - 알람 평가 건너뜀");
      return;
    }

    LogManager::getInstance().log(
        "processing", LogLevel::DEBUG_LEVEL,
        "알람 평가 시작: " + std::to_string(batch.size()) + "개 메시지");

    size_t total_events = 0;

    for (const auto &device_message : batch) {
      try {
        auto alarm_events = alarm_manager.evaluateForMessage(device_message);
        total_events += alarm_events.size();

        if (!alarm_events.empty()) {
          ProcessAlarmEvents(alarm_events, thread_index);
        }

        total_alarms_evaluated_.fetch_add(device_message.points.size());

      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "processing", LogLevel::WARN,
            "디바이스 알람 평가 실패 (device=" + device_message.device_id +
                "): " + std::string(e.what()));
      }
    }

    if (total_events > 0) {
      total_alarms_triggered_.fetch_add(total_events);
    }

    LogManager::getInstance().log(
        "processing", LogLevel::DEBUG_LEVEL,
        "알람 평가 완료: " + std::to_string(total_events) + "개 이벤트 생성");

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "알람 평가 전체 실패: " +
                                      std::string(e.what()));
  }
}

// ✅ 컴파일 에러 수정: string 타입 처리 추가
Storage::BackendFormat::AlarmEventData
DataProcessingService::ConvertAlarmEventToBackendFormat(
    const PulseOne::Alarm::AlarmEvent &alarm_event) const {

  Storage::BackendFormat::AlarmEventData data;
  data.occurrence_id = std::to_string(alarm_event.occurrence_id);
  data.rule_id = alarm_event.rule_id;
  data.tenant_id = alarm_event.tenant_id;
  data.site_id = alarm_event.site_id;
  data.point_id = alarm_event.point_id;
  data.device_id = alarm_event.device_id;
  data.message = alarm_event.message;
  data.severity = PulseOne::Alarm::severityToString(alarm_event.severity);
  data.state = PulseOne::Alarm::stateToString(alarm_event.state);
  data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                       alarm_event.occurrence_time.time_since_epoch())
                       .count();
  data.source_name = alarm_event.source_name;
  data.location = alarm_event.location;
  data.extra_info = alarm_event.extra_info;

  // ✅ DataValue → string 변환
  std::visit(
      [&data](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
          data.trigger_value = v;
        } else {
          data.trigger_value = std::to_string(v);
        }
      },
      alarm_event.trigger_value);

  return data;
}

void DataProcessingService::ProcessAlarmEvents(
    const std::vector<PulseOne::Alarm::AlarmEvent> &alarm_events,
    size_t thread_index) {

  if (alarm_events.empty()) {
    return;
  }

  try {
    LogManager::getInstance().log(
        "processing", LogLevel::INFO,
        "알람 이벤트 후처리: " + std::to_string(alarm_events.size()) + "개");

    for (const auto &alarm_event : alarm_events) {
      try {
        // DB 저장
        if (alarm_event.state == PulseOne::Alarm::AlarmState::ACTIVE) {
          SaveAlarmToDatabase(alarm_event);
        }

        // ✅ 컴파일 에러 수정: 올바른 타입으로 변환 후 호출
        if (redis_data_writer_) {
          auto backend_alarm = ConvertAlarmEventToBackendFormat(alarm_event);
          redis_data_writer_->PublishAlarmEvent(
              backend_alarm); // 변환된 객체 사용
          LogManager::getInstance().log(
              "processing", LogLevel::DEBUG_LEVEL,
              "RedisDataWriter 알람 발행 완료: rule_id=" +
                  std::to_string(alarm_event.rule_id));
        }

        if (external_notification_enabled_.load()) {
          SendExternalNotifications(alarm_event);
        }

        NotifyWebClients(alarm_event);

        if (alarm_event.severity == PulseOne::Alarm::AlarmSeverity::CRITICAL) {
          critical_alarms_count_.fetch_add(1);
        } else if (alarm_event.severity ==
                   PulseOne::Alarm::AlarmSeverity::HIGH) {
          high_alarms_count_.fetch_add(1);
        }

      } catch (const std::exception &e) {
        HandleError("개별 알람 이벤트 처리 실패", e.what());
      }
    }

  } catch (const std::exception &e) {
    HandleError("알람 이벤트 후처리 전체 실패", e.what());
  }
}

// =============================================================================
// 외부 시스템 연동
// =============================================================================

void DataProcessingService::SendExternalNotifications(
    const PulseOne::Alarm::AlarmEvent &event) {
  try {
    LogManager::getInstance().log(
        "processing", LogLevel::DEBUG_LEVEL,
        "외부 알림 발송 (스텁): rule_id=" + std::to_string(event.rule_id) +
            ", severity=" + std::to_string(static_cast<int>(event.severity)));

    if (event.severity >= PulseOne::Alarm::AlarmSeverity::CRITICAL) {
      LogManager::getInstance().log("processing", LogLevel::INFO,
                                    "긴급 알림 발송: " + event.message);
    } else if (event.severity >= PulseOne::Alarm::AlarmSeverity::HIGH) {
      LogManager::getInstance().log("processing", LogLevel::INFO,
                                    "높은 우선순위 알림 발송: " +
                                        event.message);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::WARN,
                                  "외부 알림 발송 실패: " +
                                      std::string(e.what()));
  }
}

void DataProcessingService::NotifyWebClients(
    const PulseOne::Alarm::AlarmEvent &event) {
  try {
    LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL,
                                  "웹클라이언트 알림 (스텁): rule_id=" +
                                      std::to_string(event.rule_id));

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::WARN,
                                  "웹클라이언트 알림 실패: " +
                                      std::string(e.what()));
  }
}

void DataProcessingService::SaveChangedPointsToRDB(
    const Structs::DeviceDataMessage &message,
    const std::vector<Structs::TimestampedValue> &changed_points) {

  if (changed_points.empty()) {
    return;
  }

  try {
    auto now_steady = std::chrono::steady_clock::now();
    std::vector<Structs::TimestampedValue> points_to_save;

    {
      // 🎯 저장 간격 필터링 (Digital: 상시, Analog: 5분 주기)
      std::lock_guard<std::mutex> lock(rdb_save_mutex_);
      for (const auto &point : changed_points) {
        bool is_digital = std::holds_alternative<bool>(point.value);

        if (is_digital) {
          points_to_save.push_back(point);
        } else {
          auto it = last_rdb_save_times_.find(point.point_id);
          if (it == last_rdb_save_times_.end() ||
              std::chrono::duration_cast<std::chrono::minutes>(now_steady -
                                                               it->second)
                      .count() >= 5) {

            points_to_save.push_back(point);
            last_rdb_save_times_[point.point_id] = now_steady;
          }
        }
      }
    }

    if (points_to_save.empty()) {
      return;
    }

    // 🎯 비동기 큐에 태스크 추가 (서비스가 실행 중일 때만)
    if (is_running_.load()) {
      PersistenceTask task;
      task.type = PersistenceTask::Type::RDB_SAVE;
      task.message = message;
      task.points = std::move(points_to_save);
      persistence_queue_.push(std::move(task));
    } else {
      // 서비스가 실행 중이 아니면 동기식으로 저장 (테스트용)
      auto current_value_repo = Database::RepositoryFactory::getInstance()
                                    .getCurrentValueRepository();
      if (current_value_repo) {
        for (const auto &point : points_to_save) {
          try {
            auto entity = ConvertToCurrentValueEntity(point, message);
            current_value_repo->save(entity);
          } catch (...) {
          }
        }
      }
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "RDB 저장 예약 실패: " +
                                      std::string(e.what()));
  }
}

// =============================================================================
// InfluxDB 저장 메서드들
// =============================================================================

void DataProcessingService::SaveToInfluxDB(
    const std::vector<Structs::TimestampedValue> &batch) {
  if (batch.empty()) {
    return;
  }
  // 🎯 비동기 큐에 태스크 추가 (서비스가 실행 중일 때만)
  if (is_running_.load()) {
    PersistenceTask task;
    task.type = PersistenceTask::Type::INFLUX_SAVE;
    // 'message' is not available in this scope, assuming it's not needed for
    // this specific task type or should be derived. task.message = message;
    task.points = batch; // Use 'batch' as 'changed_points' from the instruction
    persistence_queue_.push(std::move(task));
  } else {
    // 서비스가 실행 중이 아니면 동기식으로 저장 (테스트용)
    // auto influx_client =
    // DbLib::DatabaseManager::getInstance().getInfluxClient();
    /*
    if (influx_client) {
        for (const auto& point : batch) { // Use 'batch' as 'changed_points'
    from the instruction std::visit([&](const auto& val) { using T =
    std::decay_t<decltype(val)>; if constexpr (std::is_arithmetic_v<T>) {
                    influx_client->writePoint("device_data", "field_" +
    std::to_string(point.point_id), static_cast<double>(val));
                }
            }, point.value);
        }
    }
    */
  }
}

void DataProcessingService::BufferForInfluxDB(
    const Structs::DeviceDataMessage &message) {
  try {
    PersistenceTask task;
    task.type = PersistenceTask::Type::INFLUX_SAVE;
    task.message = message;
    task.points = ConvertToTimestampedValues(message);

    persistence_queue_.push(std::move(task));

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "InfluxDB 저장 예약 실패: " +
                                      std::string(e.what()));
  }
}

void DataProcessingService::BufferCommStatsForInfluxDB(
    const Structs::DeviceDataMessage &message) {
  try {
    PersistenceTask task;
    task.type = PersistenceTask::Type::COMM_STATS_SAVE;
    task.message = message;

    persistence_queue_.push(std::move(task));

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "통계 이력 저장 예약 실패: " +
                                      std::string(e.what()));
  }
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

std::vector<Structs::TimestampedValue>
DataProcessingService::ConvertToTimestampedValues(
    const Structs::DeviceDataMessage &device_msg) {

  std::vector<Structs::TimestampedValue> result;
  result.reserve(device_msg.points.size());

  for (const auto &point : device_msg.points) {
    Structs::TimestampedValue tv;
    tv.point_id = point.point_id;
    tv.value = point.value;
    tv.timestamp = point.timestamp;
    tv.quality = point.quality;
    tv.value_changed = point.value_changed;

    result.push_back(tv);
  }

  return result;
}

std::vector<Structs::TimestampedValue> DataProcessingService::GetChangedPoints(
    const Structs::DeviceDataMessage &message) {

  std::vector<Structs::TimestampedValue> changed_points;

  for (const auto &point : message.points) {
    if (point.value_changed) {
      // TimestampedValue로 직접 복사
      Structs::TimestampedValue data = point; // 직접 복사
      changed_points.push_back(data);
    }
  }

  return changed_points;
}

// =============================================================================
// JSON 변환 메서드들
// =============================================================================

std::string DataProcessingService::TimestampedValueToJson(
    const Structs::TimestampedValue &value) {
  try {
    json json_value;
    json_value["point_id"] = value.point_id;

    std::visit([&json_value](const auto &v) { json_value["value"] = v; },
               value.value);

    json_value["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            value.timestamp.time_since_epoch())
            .count();

    json_value["quality"] = static_cast<int>(value.quality);

    if (value.value_changed) {
      json_value["changed"] = true;
    }

    return json_value.dump();

  } catch (const std::exception &e) {
    HandleError("JSON 변환 실패", e.what());
    return R"({"point_id":)" + std::to_string(value.point_id) +
           R"(,"value":null,"error":"conversion_failed"})";
  }
}

std::string DataProcessingService::DeviceDataMessageToJson(
    const Structs::DeviceDataMessage &message) {
  try {
    json j;
    j["device_id"] = message.device_id;
    j["protocol"] = message.protocol;
    j["tenant_id"] = message.tenant_id;
    j["device_status"] = static_cast<int>(message.device_status);
    j["is_connected"] = message.is_connected;
    j["manual_status"] = message.manual_status;
    j["status_message"] = message.status_message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         message.timestamp.time_since_epoch())
                         .count();

    j["points"] = json::array();
    for (const auto &point : message.points) {
      json point_json;
      point_json["point_id"] = point.point_id;
      std::visit([&point_json](const auto &v) { point_json["value"] = v; },
                 point.value);
      point_json["timestamp"] =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              point.timestamp.time_since_epoch())
              .count();
      point_json["quality"] = static_cast<int>(point.quality);
      point_json["changed"] = point.value_changed;
      j["points"].push_back(point_json);
    }

    return j.dump();

  } catch (const std::exception &e) {
    HandleError("DeviceDataMessage JSON 변환 실패", e.what());
    return R"({"device_id":")" + message.device_id +
           R"(","error":"conversion_failed"})";
  }
}

std::string DataProcessingService::ConvertToLightDeviceStatus(
    const Structs::DeviceDataMessage &message) {
  json light_status;
  light_status["id"] = message.device_id;
  light_status["proto"] = message.protocol;
  light_status["status"] = static_cast<int>(message.device_status);
  light_status["connected"] = message.is_connected;
  light_status["manual"] = message.manual_status;
  light_status["msg"] = message.status_message.substr(0, 50);
  light_status["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                           message.timestamp.time_since_epoch())
                           .count();

  light_status["stats"] = {{"fail", message.consecutive_failures},
                           {"total", message.total_points_configured},
                           {"ok", message.successful_points},
                           {"err", message.failed_points},
                           {"rtt", message.response_time.count()}};

  return light_status.dump();
}

// ✅ 컴파일 에러 수정: getPointName, getUnit 함수 호출을 멤버 함수로 변경
std::string DataProcessingService::ConvertToLightPointValue(
    const Structs::TimestampedValue &value, const std::string &device_id) {

  json light_point;

  // 기본 식별 정보
  light_point["point_id"] = value.point_id;
  light_point["device_id"] = device_id;

  // 간단한 네이밍 (DB 조회 없이)
  std::string point_name = getPointName(value.point_id); // 멤버 함수 호출
  std::string device_name = "device_" + device_id;
  light_point["key"] = "device:" + device_id + ":" + point_name;

  light_point["device_name"] = device_name;
  light_point["point_name"] = point_name;

  // 실제 값 처리
  std::visit(
      [&light_point](const auto &v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, bool>) {
          light_point["value"] = v ? "true" : "false";
          light_point["data_type"] = "boolean";
        } else if constexpr (std::is_integral_v<T>) {
          light_point["value"] = std::to_string(v);
          light_point["data_type"] = "integer";
        } else if constexpr (std::is_floating_point_v<T>) {
          // 소수점 2자리까지 문자열로 변환
          std::ostringstream oss;
          oss << std::fixed << std::setprecision(2) << v;
          light_point["value"] = oss.str();
          light_point["data_type"] = "number";
        } else if constexpr (std::is_same_v<T, std::string>) {
          light_point["value"] = v;
          light_point["data_type"] = "string";
        } else {
          light_point["value"] = "unknown";
          light_point["data_type"] = "unknown";
        }
      },
      value.value);

  // 타임스탬프 (ISO 8601 형식)
  auto time_t = std::chrono::system_clock::to_time_t(value.timestamp);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                value.timestamp.time_since_epoch()) %
            1000;

  std::ostringstream timestamp_stream;
  timestamp_stream << std::put_time(std::localtime(&time_t),
                                    "%Y-%m-%dT%H:%M:%S");
  timestamp_stream << "." << std::setfill('0') << std::setw(3) << ms.count()
                   << "Z";
  light_point["timestamp"] = timestamp_stream.str();

  // 품질 상태 (Utils.h의 함수 사용)
  light_point["quality"] =
      PulseOne::Utils::DataQualityToString(value.quality, true);

  // 단위 (멤버 함수 사용)
  light_point["unit"] = getUnit(value.point_id);

  // 값 변경 여부
  if (value.value_changed) {
    light_point["changed"] = true;
  }

  return light_point.dump();
}

std::string DataProcessingService::ConvertToBatchPointData(
    const Structs::DeviceDataMessage &message) {
  json batch_data;
  batch_data["dev"] = message.device_id;
  batch_data["proto"] = message.protocol;
  batch_data["batch_ts"] =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          message.timestamp.time_since_epoch())
          .count();
  batch_data["seq"] = message.batch_sequence;

  batch_data["points"] = json::array();
  for (const auto &point : message.points) {
    json p;
    p["id"] = point.point_id;
    std::visit([&p](const auto &v) { p["val"] = v; }, point.value);
    p["ts"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                  point.timestamp.time_since_epoch())
                  .count();
    p["q"] = static_cast<int>(point.quality);
    if (point.value_changed)
      p["chg"] = true;
    batch_data["points"].push_back(p);
  }

  return batch_data.dump();
}

std::string DataProcessingService::getDeviceIdForPoint(int point_id) {
  return "device_" + std::to_string(point_id / 100);
}

// =============================================================================
// ✅ 컴파일 에러 수정: getPointName, getUnit 멤버 함수 구현
// =============================================================================

std::string DataProcessingService::getPointName(int point_id) const {
  // Backend realtime.js와 일치하는 포인트 이름들
  std::unordered_map<int, std::string> point_names = {
      {1, "temperature_sensor_01"},
      {2, "pressure_sensor_01"},
      {3, "flow_rate"},
      {4, "pump_status"},
      {5, "room_temperature"},
      {6, "humidity_level"},
      {7, "fan_speed"},
      {8, "cooling_mode"},
      {9, "voltage_l1"},
      {10, "current_l1"},
      {11, "power_total"},
      {12, "energy_consumed"}};

  auto it = point_names.find(point_id);
  return (it != point_names.end()) ? it->second
                                   : ("point_" + std::to_string(point_id));
}

std::string DataProcessingService::getUnit(int point_id) const {
  std::unordered_map<int, std::string> units = {
      {1, "°C"},  {2, "bar"}, {3, "L/min"}, {4, ""},   {5, "°C"}, {6, "%"},
      {7, "rpm"}, {8, ""},    {9, "V"},     {10, "A"}, {11, "W"}, {12, "kWh"}};

  auto it = units.find(point_id);
  return (it != units.end()) ? it->second : "";
}

// =============================================================================
// 통계 메서드들
// =============================================================================

void DataProcessingService::UpdateStatistics(size_t processed_count,
                                             double processing_time_ms) {
  total_processing_time_ms_.fetch_add(
      static_cast<uint64_t>(processing_time_ms));
  total_operations_.fetch_add(1);

  if (processed_count > 0) {
    total_messages_processed_.fetch_add(processed_count);
    total_batches_processed_.fetch_add(1);
  }
}

void DataProcessingService::UpdateStatistics(size_t point_count) {
  points_processed_.fetch_add(point_count);
}

// ✅ 컴파일 에러 수정: atomic 멤버를 개별적으로 복사
DataProcessingService::ProcessingStats
DataProcessingService::GetStatistics() const {
  ProcessingStats stats;

  // atomic 값들을 개별적으로 load해서 복사
  stats.total_batches_processed.store(total_batches_processed_.load());
  stats.total_messages_processed.store(total_messages_processed_.load());
  stats.redis_writes.store(redis_writes_.load());
  stats.influx_writes.store(influx_writes_.load());
  stats.processing_errors.store(processing_errors_.load());

  uint64_t total_ops = total_operations_.load();
  if (total_ops > 0) {
    stats.avg_processing_time_ms =
        static_cast<double>(total_processing_time_ms_.load()) / total_ops;
  }

  return stats;
}

PulseOne::Alarm::AlarmProcessingStats
DataProcessingService::GetAlarmStatistics() const {
  PulseOne::Alarm::AlarmProcessingStats stats;
  stats.total_evaluated = total_alarms_evaluated_.load();
  stats.total_triggered = total_alarms_triggered_.load();
  stats.critical_count = critical_alarms_count_.load();
  stats.high_count = high_alarms_count_.load();
  return stats;
}

DataProcessingService::ExtendedProcessingStats
DataProcessingService::GetExtendedStatistics() const {
  ExtendedProcessingStats stats;

  // ✅ 컴파일 에러 수정: 대입 연산자 대신 개별 복사
  auto processing_stats = GetStatistics();
  stats.processing.total_batches_processed.store(
      processing_stats.total_batches_processed.load());
  stats.processing.total_messages_processed.store(
      processing_stats.total_messages_processed.load());
  stats.processing.redis_writes.store(processing_stats.redis_writes.load());
  stats.processing.influx_writes.store(processing_stats.influx_writes.load());
  stats.processing.processing_errors.store(
      processing_stats.processing_errors.load());
  stats.processing.avg_processing_time_ms =
      processing_stats.avg_processing_time_ms;

  stats.alarms = GetAlarmStatistics();
  return stats;
}

void DataProcessingService::HandleError(const std::string &error_message,
                                        const std::string &context) {
  std::string full_message = error_message;
  if (!context.empty()) {
    full_message += ": " + context;
  }

  LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                full_message);
  processing_errors_.fetch_add(1);
}

PulseOne::Database::Entities::CurrentValueEntity
DataProcessingService::ConvertToCurrentValueEntity(
    const Structs::TimestampedValue &point,
    const Structs::DeviceDataMessage &message) {

  using namespace PulseOne::Database::Entities;

  CurrentValueEntity entity;

  try {
    entity.setPointId(point.point_id);
    entity.setValueTimestamp(point.timestamp);
    entity.setQuality(point.quality);

    // DataVariant를 JSON 문자열로 변환
    json value_json;
    std::visit(
        [&value_json](const auto &value) { value_json["value"] = value; },
        point.value);

    entity.setCurrentValue(value_json.dump());
    entity.setRawValue(value_json.dump());

    // 타입 설정 (int64_t 등 모든 DataVariant 타입 자동 지원)
    entity.setValueType(
        PulseOne::BasicTypes::GetDataVariantTypeName(point.value));

    auto now = std::chrono::system_clock::now();
    entity.setLastReadTime(now);
    entity.setUpdatedAt(now);

    return entity;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "processing", LogLevel::LOG_ERROR,
        "Point " + std::to_string(point.point_id) +
            " CurrentValueEntity 변환 실패: " + std::string(e.what()));
    throw;
  }
}

void DataProcessingService::SaveChangedPointsToRDB(
    const Structs::DeviceDataMessage &message) {
  try {
    auto changed_points = GetChangedPoints(message);

    if (changed_points.empty()) {
      LogManager::getInstance().log("processing", LogLevel::DEBUG_LEVEL,
                                    "변화된 포인트가 없음, RDB 저장 건너뜀");
      return;
    }

    SaveChangedPointsToRDB(message, changed_points);

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "SaveChangedPointsToRDB(단일) 실패: " +
                                      std::string(e.what()));
    HandleError("RDB 저장 실패", e.what());
  }
}

void DataProcessingService::SaveAlarmToDatabase(
    const PulseOne::Alarm::AlarmEvent &event) {
  try {
    auto &factory = PulseOne::Database::RepositoryFactory::getInstance();
    auto alarm_occurrence_repo = factory.getAlarmOccurrenceRepository();

    if (!alarm_occurrence_repo) {
      LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                    "AlarmOccurrenceRepository 없음");
      return;
    }

    PulseOne::Database::Entities::AlarmOccurrenceEntity occurrence;
    occurrence.setRuleId(event.rule_id);
    occurrence.setTenantId(event.tenant_id);
    occurrence.setOccurrenceTime(event.occurrence_time);

    std::string trigger_value_str;
    std::visit(
        [&trigger_value_str](auto &&v) {
          if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                       std::string>) {
            trigger_value_str = v;
          } else {
            trigger_value_str = std::to_string(v);
          }
        },
        event.trigger_value);
    occurrence.setTriggerValue(trigger_value_str);

    occurrence.setAlarmMessage(event.message);
    occurrence.setSeverity(event.severity);
    occurrence.setState(PulseOne::Alarm::AlarmState::ACTIVE);
    occurrence.setSourceName(event.source_name);
    occurrence.setLocation(event.location);
    occurrence.setContextData("{}");

    if (alarm_occurrence_repo->save(occurrence)) {
      LogManager::getInstance().log("processing", LogLevel::INFO,
                                    "알람 DB 저장 성공: rule_id=" +
                                        std::to_string(event.rule_id));
    } else {
      LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                    "알람 DB 저장 실패: rule_id=" +
                                        std::to_string(event.rule_id));
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().log("processing", LogLevel::LOG_ERROR,
                                  "알람 DB 저장 예외: " +
                                      std::string(e.what()));
  }
}

void DataProcessingService::StoreVirtualPointToRedis(
    const Structs::TimestampedValue &vp_result) {
  if (redis_data_writer_) {
    redis_data_writer_->StoreVirtualPointToRedis(vp_result);
  }
}

nlohmann::json DataProcessingService::ExtendedProcessingStats::toJson() const {
  nlohmann::json j;
  j["processing"] = processing.toJson();
  j["alarms"] = alarms.toJson();
  return j;
}

} // namespace Pipeline
} // namespace PulseOne