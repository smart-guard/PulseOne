/**
 * @file ScheduledExporter.cpp
 * @brief DB 기반 스케줄 관리 및 실행 구현
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
 */

#include "Schedule/ScheduledExporter.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace PulseOne {
namespace Schedule {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ScheduledExporter::ScheduledExporter(const ScheduledExporterConfig& config)
    : config_(config)
    , last_reload_time_(std::chrono::system_clock::now()) {
    
    LogManager::getInstance().Info("ScheduledExporter 초기화 시작");
    LogManager::getInstance().Info("스케줄 체크 간격: " + 
        std::to_string(config_.check_interval_seconds) + "초");
    LogManager::getInstance().Info("리로드 간격: " + 
        std::to_string(config_.reload_interval_seconds) + "초");
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
    
    // 1. Redis 연결
    if (!initializeRedisConnection()) {
        LogManager::getInstance().Error("Redis 연결 실패");
        return false;
    }
    
    // 2. Repositories 초기화
    if (!initializeRepositories()) {
        LogManager::getInstance().Error("Repositories 초기화 실패");
        return false;
    }
    
    // 3. DynamicTargetManager 초기화
    if (!initializeDynamicTargetManager()) {
        LogManager::getInstance().Error("DynamicTargetManager 초기화 실패");
        return false;
    }
    
    // 4. 초기 스케줄 로드
    int loaded = reloadSchedules();
    LogManager::getInstance().Info("초기 스케줄 로드: " + std::to_string(loaded) + "개");
    
    // 5. 워커 스레드 시작
    should_stop_ = false;
    is_running_ = true;
    worker_thread_ = std::make_unique<std::thread>(
        &ScheduledExporter::scheduleCheckLoop, this);
    
    LogManager::getInstance().Info("ScheduledExporter 시작 완료");
    return true;
}

void ScheduledExporter::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("ScheduledExporter 중지 중...");
    
    should_stop_ = true;
    
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
    LogManager::getInstance().Info("스케줄 수동 실행: ID=" + std::to_string(schedule_id));
    
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
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        LogManager::getInstance().Error("스케줄 실행 실패: " + std::string(e.what()));
    }
    
    return result;
}

int ScheduledExporter::executeAllSchedules() {
    LogManager::getInstance().Info("모든 활성 스케줄 실행");
    
    int executed_count = 0;
    
    try {
        auto schedules = schedule_repo_->findEnabled();
        
        for (const auto& schedule : schedules) {
            if (should_stop_.load()) break;
            
            auto result = executeScheduleInternal(schedule);
            if (result.success) {
                executed_count++;
            }
        }
        
    } catch (const std::exception& e) {
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
        active_schedules_.clear();
        for (const auto& schedule : schedules) {
            active_schedules_[schedule.getId()] = schedule;
        }
        
        last_reload_time_ = std::chrono::system_clock::now();
        
        LogManager::getInstance().Info("스케줄 리로드 완료: " + 
            std::to_string(schedules.size()) + "개");
        
        return schedules.size();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("스케줄 리로드 실패: " + std::string(e.what()));
        return 0;
    }
}

std::vector<int> ScheduledExporter::getActiveScheduleIds() const {
    std::lock_guard<std::mutex> lock(schedule_mutex_);
    
    std::vector<int> ids;
    ids.reserve(active_schedules_.size());
    
    for (const auto& pair : active_schedules_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::map<int, std::chrono::system_clock::time_point> 
ScheduledExporter::getUpcomingSchedules() const {
    
    std::lock_guard<std::mutex> lock(schedule_mutex_);
    
    std::map<int, std::chrono::system_clock::time_point> upcoming;
    
    for (const auto& pair : active_schedules_) {
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
    stats["total_points_exported"] = total_points_exported_.load();
    stats["total_execution_time_ms"] = total_execution_time_ms_.load();
    
    {
        std::lock_guard<std::mutex> lock(schedule_mutex_);
        stats["active_schedules"] = active_schedules_.size();
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
    total_points_exported_ = 0;
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
            for (const auto& schedule : due_schedules) {
                if (should_stop_.load()) break;
                
                auto result = executeScheduleInternal(schedule);
                
                // 결과 로깅
                logExecutionResult(result);
                
                // 스케줄 상태 업데이트
                updateScheduleStatus(
                    schedule.getId(),
                    result.success,
                    result.next_run_time);
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("스케줄 체크 루프 에러: " + 
                std::string(e.what()));
        }
        
        // 대기
        for (int i = 0; i < config_.check_interval_seconds && !should_stop_.load(); ++i) {
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
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("실행 대기 스케줄 조회 실패: " + 
            std::string(e.what()));
    }
    
    return due_schedules;
}

ScheduleExecutionResult ScheduledExporter::executeScheduleInternal(
    const PulseOne::Database::Entities::ExportScheduleEntity& schedule) {
    
    ScheduleExecutionResult result;
    result.schedule_id = schedule.getId();
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        LogManager::getInstance().Info(
            "스케줄 실행 시작: [" + std::to_string(schedule.getId()) + "] " + 
            schedule.getScheduleName());
        
        // 1. 타겟 조회
        auto target_opt = target_repo_->findById(schedule.getTargetId());
        if (!target_opt.has_value()) {
            result.success = false;
            result.error_message = "타겟을 찾을 수 없음";
            LogManager::getInstance().Error(result.error_message);
            return result;
        }
        
        auto target = target_opt.value();
        
        // 2. 데이터 수집
        auto data_points = collectDataForSchedule(schedule, target);
        result.total_points = data_points.size();
        
        if (data_points.empty()) {
            LogManager::getInstance().Warn("수집된 데이터 없음");
            result.success = true; // 데이터 없는건 에러 아님
            result.next_run_time = calculateNextRunTime(
                schedule.getCronExpression(),
                schedule.getTimezone());
            return result;
        }
        
        // 3. 데이터 전송
        bool send_success = sendDataToTarget(target, data_points);
        
        if (send_success) {
            result.exported_points = data_points.size();
            result.success = true;
        } else {
            result.failed_points = data_points.size();
            result.success = false;
            result.error_message = "데이터 전송 실패";
        }
        
        // 4. 다음 실행 시각 계산
        result.next_run_time = calculateNextRunTime(
            schedule.getCronExpression(),
            schedule.getTimezone());
        
        // 통계 업데이트
        total_executions_.fetch_add(1);
        if (result.success) {
            successful_executions_.fetch_add(1);
        } else {
            failed_executions_.fetch_add(1);
        }
        total_points_exported_.fetch_add(result.exported_points);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        failed_executions_.fetch_add(1);
        LogManager::getInstance().Error("스케줄 실행 실패: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    total_execution_time_ms_.fetch_add(result.execution_time.count());
    
    LogManager::getInstance().Info(
        "스케줄 실행 완료: [" + std::to_string(schedule.getId()) + "] " +
        (result.success ? "성공" : "실패") +
        ", 포인트: " + std::to_string(result.exported_points) + "/" + 
        std::to_string(result.total_points) +
        ", 소요: " + std::to_string(result.execution_time.count()) + "ms");
    
    return result;
}

// =============================================================================
// 내부 메서드 - 데이터 수집
// =============================================================================

std::vector<ExportDataPoint> ScheduledExporter::collectDataForSchedule(
    const PulseOne::Database::Entities::ExportScheduleEntity& schedule,
    const PulseOne::Database::Entities::ExportTargetEntity& target) {
    
    std::vector<ExportDataPoint> data_points;
    
    try {
        // 1. 시간 범위 계산
        auto [start_time, end_time] = calculateTimeRange(
            schedule.getDataRange(),
            schedule.getLookbackPeriods());
        
        // 2. 타겟의 매핑 조회
        auto mappings = mapping_repo_->findByTargetId(target.getId());
        
        if (mappings.empty()) {
            LogManager::getInstance().Warn(
                "타겟에 매핑된 포인트 없음: " + target.getName());
            return data_points;
        }
        
        // 3. 각 매핑된 포인트의 데이터 조회
        for (const auto& mapping : mappings) {
            // ✅ isEnabled() 사용
            if (!mapping.isEnabled()) continue;
            
            auto point_data = fetchPointData(
                mapping.getPointId(),
                start_time,
                end_time);
            
            if (point_data.has_value()) {
                data_points.push_back(point_data.value());
            }
        }
        
        if (config_.enable_debug_log) {
            LogManager::getInstance().Debug(
                "데이터 수집 완료: " + std::to_string(data_points.size()) + "개 포인트");
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("데이터 수집 실패: " + std::string(e.what()));
    }
    
    return data_points;
}

std::pair<std::chrono::system_clock::time_point, 
          std::chrono::system_clock::time_point>
ScheduledExporter::calculateTimeRange(
    const std::string& data_range, 
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
    int point_id,
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {
    
    if (!redis_client_ || !redis_client_->isConnected()) {
        return std::nullopt;
    }
    
    try {
        // Redis에서 현재값 조회 (간단한 버전)
        std::string key = "point:" + std::to_string(point_id) + ":current";
        std::string json_str = redis_client_->get(key);
        
        if (json_str.empty()) {
            return std::nullopt;
        }
        
        auto data = json::parse(json_str);
        
        ExportDataPoint point;
        point.point_id = point_id;
        point.point_name = data.value("point_name", "");
        point.value = data.value("value", "");
        point.timestamp = data.value("timestamp", 0LL);
        point.quality = data.value("quality", 0);
        point.unit = data.value("unit", "");
        
        return point;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "포인트 데이터 조회 실패 [" + std::to_string(point_id) + "]: " + 
            std::string(e.what()));
        return std::nullopt;
    }
}

// =============================================================================
// 내부 메서드 - 데이터 전송
// =============================================================================

bool ScheduledExporter::sendDataToTarget(
    const PulseOne::Database::Entities::ExportTargetEntity& target,
    const std::vector<ExportDataPoint>& data_points) {
    
    if (!dynamic_target_manager_) {
        LogManager::getInstance().Error("DynamicTargetManager 미초기화");
        return false;
    }
    
    try {
        // 배치 생성
        auto batches = createBatches(data_points);
        
        bool all_success = true;
        
        for (const auto& batch : batches) {
            // JSON 형식으로 변환
            json batch_json = json::array();
            for (const auto& point : batch) {
                batch_json.push_back(point.to_json());
            }
            
            // 여기서는 DynamicTargetManager를 사용하지만
            // 실제로는 target 타입에 따라 적절한 핸들러 호출
            // (FILE, HTTP, MQTT, S3 등)
            
            // 임시: 로그만 출력
            LogManager::getInstance().Info(
                "타겟 [" + target.getName() + "]로 " + 
                std::to_string(batch.size()) + "개 포인트 전송");
            
            if (config_.enable_debug_log) {
                LogManager::getInstance().Debug("배치 데이터: " + batch_json.dump());
            }
        }
        
        return all_success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("데이터 전송 실패: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::vector<ExportDataPoint>> ScheduledExporter::createBatches(
    const std::vector<ExportDataPoint>& data_points) {
    
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
    const std::string& cron_expression,
    const std::string& timezone,
    const std::chrono::system_clock::time_point& from_time) {
    
    // 간단한 구현: 현재 + 1시간 (실제로는 Cron 파서 필요)
    // TODO: 실제 Cron 표현식 파싱 라이브러리 사용 (ccronexpr 등)
    
    using namespace std::chrono;
    
    // 임시: 1시간 후
    return from_time + hours(1);
}

// =============================================================================
// 내부 메서드 - DB 연동
// =============================================================================

void ScheduledExporter::logExecutionResult(const ScheduleExecutionResult& result) {
    try {
        // ExportLogEntity 생성
        PulseOne::Database::Entities::ExportLogEntity log;
        log.setTargetId(result.schedule_id); // schedule_id를 임시로 사용
        log.setTimestamp(std::chrono::system_clock::now());
        log.setStatus(result.success ? "success" : "failed");
        
        // ✅ ExportLogEntity에 setTotalRecords 등이 없으므로
        // 메시지에 포함하거나 나중에 추가
        std::string details = "Points: " + std::to_string(result.total_points) +
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
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("실행 결과 로그 저장 실패: " + 
            std::string(e.what()));
    }
}

bool ScheduledExporter::updateScheduleStatus(
    int schedule_id,
    bool success,
    const std::chrono::system_clock::time_point& next_run) {
    
    try {
        return schedule_repo_->updateRunStatus(schedule_id, success, next_run);
        
    } catch (const std::exception& e) {
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
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Redis 초기화 실패: " + std::string(e.what()));
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
        
        LogManager::getInstance().Info("Repositories 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Repositories 초기화 실패: " + 
            std::string(e.what()));
        return false;
    }
}

bool ScheduledExporter::initializeDynamicTargetManager() {
    try {
        LogManager::getInstance().Info("DynamicTargetManager 초기화 중...");
        
        // ✅ 수정: 빈 문자열로 초기화 (DB 모드 사용)
        // CSPGateway와 동일한 방식
        dynamic_target_manager_ = std::make_unique<
            PulseOne::CSP::DynamicTargetManager>("");
        
        LogManager::getInstance().Info("JSON 설정 파일 건너뜀 - DB 모드 사용");
        
        // ✅ DB에서 타겟 로드
        if (loadTargetsFromDatabase()) {
            LogManager::getInstance().Info(
                "Dynamic Target System 초기화 성공 (DB 모드)");
        } else {
            LogManager::getInstance().Warn(
                "DB에서 타겟 로드 실패 - 빈 타겟 리스트로 시작");
        }
        
        // DynamicTargetManager 시작
        if (!dynamic_target_manager_->start()) {
            LogManager::getInstance().Error("DynamicTargetManager 시작 실패");
            return false;
        }
        
        LogManager::getInstance().Info("DynamicTargetManager 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DynamicTargetManager 초기화 실패: " + std::string(e.what()));
        dynamic_target_manager_.reset();
        return false;
    }
}

bool ScheduledExporter::loadTargetsFromDatabase() {
    try {
        LogManager::getInstance().Info("Loading targets from database...");
        
        using namespace PulseOne::Database::Repositories;
        using namespace PulseOne::Database::Entities;
        using namespace PulseOne::CSP;
        
        ExportTargetRepository target_repo;
        auto entities = target_repo.findByEnabled(true);
        
        if (entities.empty()) {
            LogManager::getInstance().Warn("No targets loaded from database");
            return false;
        }
        
        int loaded_count = 0;
        for (const auto& entity : entities) {
            try {
                DynamicTarget target;
                target.name = entity.getName();
                target.type = entity.getTargetType();
                target.enabled = entity.isEnabled();
                target.priority = 0;
                
                // ✅ getConfig() 사용!
                try {
                    target.config = json::parse(entity.getConfig());
                } catch (const std::exception& e) {
                    LogManager::getInstance().Error(
                        "Failed to parse config JSON for target: " + 
                        target.name + ", error: " + e.what());
                    continue;
                }
                
                target.config["id"] = entity.getId();
                if (!entity.getDescription().empty()) {
                    target.config["description"] = entity.getDescription();
                }
                
                if (dynamic_target_manager_ && dynamic_target_manager_->addTarget(target)) {
                    loaded_count++;
                    LogManager::getInstance().Debug(
                        "Loaded target: " + target.name + " (" + target.type + ")");
                } else {
                    LogManager::getInstance().Warn(
                        "Failed to add target to manager: " + target.name);
                }
                
            } catch (const std::exception& e) {
                LogManager::getInstance().Error(
                    "Error processing target entity: " + std::string(e.what()));
                continue;
            }
        }
        
        LogManager::getInstance().Info(
            "Loaded " + std::to_string(loaded_count) + " targets from database");
        
        return (loaded_count > 0);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "Exception loading targets from database: " + std::string(e.what()));
        return false;
    }
}



} // namespace Schedule
} // namespace PulseOne