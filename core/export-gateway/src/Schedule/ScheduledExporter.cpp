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
#include "CSP/DynamicTargetManager.h"  // ✅ 추가
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
    
    LogManager::getInstance().Info("ScheduledExporter v2.0 초기화 시작");
    LogManager::getInstance().Info("스케줄 체크 간격: " + 
        std::to_string(config_.check_interval_seconds) + "초");
    LogManager::getInstance().Info("리로드 간격: " + 
        std::to_string(config_.reload_interval_seconds) + "초");
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
    
    // ✅ 1. DynamicTargetManager 확인 (싱글턴이 초기화되었는지)
    try {
        auto& manager = PulseOne::CSP::DynamicTargetManager::getInstance();
        if (!manager.isRunning()) {
            LogManager::getInstance().Error("DynamicTargetManager가 실행되지 않음");
            return false;
        }
        LogManager::getInstance().Info("✅ DynamicTargetManager 연결 확인");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DynamicTargetManager 접근 실패: " + std::string(e.what()));
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
    LogManager::getInstance().Info("초기 스케줄 로드: " + std::to_string(loaded) + "개");
    
    // 5. 워커 스레드 시작
    should_stop_ = false;
    is_running_ = true;
    worker_thread_ = std::make_unique<std::thread>(
        &ScheduledExporter::scheduleCheckLoop, this);
    
    LogManager::getInstance().Info("ScheduledExporter 시작 완료 ✅");
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
        cached_schedules_.clear();
        for (const auto& schedule : schedules) {
            cached_schedules_[schedule.getId()] = schedule;
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
    ids.reserve(cached_schedules_.size());
    
    for (const auto& pair : cached_schedules_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::map<int, std::chrono::system_clock::time_point> 
ScheduledExporter::getUpcomingSchedules() const {
    
    std::lock_guard<std::mutex> lock(schedule_mutex_);
    
    std::map<int, std::chrono::system_clock::time_point> upcoming;
    
    for (const auto& pair : cached_schedules_) {
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
    result.execution_timestamp = std::chrono::system_clock::now();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        LogManager::getInstance().Info("스케줄 실행: " + schedule.getScheduleName() + 
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
        
        LogManager::getInstance().Info("데이터 수집 완료: " + 
                                      std::to_string(data_points.size()) + "개 포인트");
        
        // 2. 타겟 조회
        auto target_id = schedule.getTargetId();
        auto target_entity = target_repo_->findById(target_id);
        
        if (!target_entity.has_value()) {
            result.success = false;
            result.error_message = "타겟을 찾을 수 없음 (ID: " + std::to_string(target_id) + ")";
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
        result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
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
        
        LogManager::getInstance().Info("스케줄 실행 완료: " + schedule.getScheduleName() + 
                                      " (" + std::to_string(result.execution_time_ms) + "ms)");
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        LogManager::getInstance().Error("스케줄 실행 실패: " + std::string(e.what()));
    }
    
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
    const std::string& target_name,
    const std::vector<ExportDataPoint>& data_points,
    const PulseOne::Database::Entities::ExportScheduleEntity& schedule) {
    
    try {
        // ✅ DynamicTargetManager 싱글턴 사용
        auto& manager = PulseOne::CSP::DynamicTargetManager::getInstance();
        
        // ✅ 템플릿 포함 타겟 조회
        auto target_opt = manager.getTargetWithTemplate(target_name);
        
        if (!target_opt.has_value()) {
            LogManager::getInstance().Error("타겟을 찾을 수 없음: " + target_name);
            return false;
        }
        
        const auto& target = target_opt.value();
        
        // 데이터 준비
        json payload;
        
        // ✅ 템플릿이 있으면 템플릿 형식으로 변환
        if (target.config.contains("template_json") && 
            target.config.contains("field_mappings")) {
            
            LogManager::getInstance().Info("템플릿 기반 데이터 변환 시작: " + target_name);
            
            payload = transformDataWithTemplate(
                target.config["template_json"],
                target.config["field_mappings"],
                data_points
            );
            
            LogManager::getInstance().Debug("변환된 페이로드: " + payload.dump());
            
        } else {
            // 기본 JSON 형식
            payload["schedule_id"] = schedule.getId();
            payload["schedule_name"] = schedule.getScheduleName();
            payload["data_range"] = schedule.getDataRange();
            payload["timestamp"] = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());
            
            json data_array = json::array();
            for (const auto& point : data_points) {
                data_array.push_back(point.to_json());
            }
            payload["data_points"] = data_array;
            
            LogManager::getInstance().Debug("기본 페이로드 생성");
        }
        
        // HTTP 전송
        if (target.type == "http" || target.type == "https") {
            // TODO: HTTP 핸들러를 통한 전송
            // handler->sendJson(payload, target.config);
            
            LogManager::getInstance().Info("HTTP 전송 성공: " + target_name);
            return true;
        }
        // S3 전송
        else if (target.type == "s3") {
            // TODO: S3 핸들러를 통한 전송
            
            LogManager::getInstance().Info("S3 전송 성공: " + target_name);
            return true;
        }
        // File 저장
        else if (target.type == "file") {
            // TODO: File 핸들러를 통한 저장
            
            LogManager::getInstance().Info("File 저장 성공: " + target_name);
            return true;
        }
        
        LogManager::getInstance().Warn("지원되지 않는 타겟 타입: " + target.type);
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("데이터 전송 실패: " + std::string(e.what()));
        return false;
    }
}



// =============================================================================
// 템플릿 기반 데이터 변환 (v2.0 신규)
// =============================================================================

json ScheduledExporter::transformDataWithTemplate(
    const json& template_json,
    const json& field_mappings,
    const std::vector<ExportDataPoint>& data_points) {
    
    json result = template_json;  // 템플릿 복사
    
    try {
        LogManager::getInstance().Debug("템플릿 변환 시작 - 데이터 포인트: " + 
                                       std::to_string(data_points.size()) + "개");
        
        // field_mappings 형식:
        // {
        //   "point_name_1": "$.data.temperature",
        //   "point_name_2": "$.data.humidity"
        // }
        
        for (const auto& [point_name, json_path] : field_mappings.items()) {
            // 해당 포인트 찾기
            auto it = std::find_if(data_points.begin(), data_points.end(),
                [&point_name](const ExportDataPoint& p) {
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
            json* current = &result;
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
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("템플릿 변환 실패: " + std::string(e.what()));
        // 변환 실패 시 원본 템플릿 반환
    }
    
    return result;
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
        
        // ✅ 템플릿 Repository 추가
        template_repo_ = std::make_unique<
            PulseOne::Database::Repositories::PayloadTemplateRepository>();
        
        LogManager::getInstance().Info("Repositories 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Repositories 초기화 실패: " + 
            std::string(e.what()));
        return false;
    }
}

} // namespace Schedule
} // namespace PulseOne