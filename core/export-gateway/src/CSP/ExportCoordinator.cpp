/**
 * @file ExportCoordinator.cpp
 * @brief Export 시스템 중앙 조율자 구현
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 1.0.0
 */

#include "CSP/ExportCoordinator.h"
#include "Database/DatabaseManager.h"
#include <algorithm>
#include <numeric>

namespace PulseOne {
namespace Coordinator {

// =============================================================================
// 정적 멤버 초기화
// =============================================================================

std::shared_ptr<PulseOne::CSP::DynamicTargetManager> 
    ExportCoordinator::shared_target_manager_ = nullptr;

std::shared_ptr<PulseOne::Transform::PayloadTransformer> 
    ExportCoordinator::shared_payload_transformer_ = nullptr;

std::mutex ExportCoordinator::init_mutex_;
std::atomic<bool> ExportCoordinator::shared_resources_initialized_{false};

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ExportCoordinator::ExportCoordinator(const ExportCoordinatorConfig& config)
    : config_(config) {
    
    LogManager::getInstance().Info("ExportCoordinator 초기화 시작");
    LogManager::getInstance().Info("데이터베이스: " + config_.database_path);
    LogManager::getInstance().Info("Redis: " + config_.redis_host + ":" + 
                                  std::to_string(config_.redis_port));
    
    // 통계 시작 시간 기록
    stats_.start_time = std::chrono::system_clock::now();
}

ExportCoordinator::~ExportCoordinator() {
    try {
        stop();
        LogManager::getInstance().Info("ExportCoordinator 소멸 완료");
    } catch (const std::exception& e) {
        // 소멸자에서 예외 발생 시 로그만 남김
        LogManager::getInstance().Error("ExportCoordinator 소멸 중 예외: " + 
                                       std::string(e.what()));
    }
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool ExportCoordinator::start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("ExportCoordinator가 이미 실행 중입니다");
        return false;
    }
    
    LogManager::getInstance().Info("ExportCoordinator 시작 중...");
    
    try {
        // 1. 공유 리소스 초기화 (최우선)
        if (!initializeSharedResources()) {
            LogManager::getInstance().Error("공유 리소스 초기화 실패");
            return false;
        }
        
        // 2. 데이터베이스 초기화
        if (!initializeDatabase()) {
            LogManager::getInstance().Error("데이터베이스 초기화 실패");
            return false;
        }
        
        // 3. Repositories 초기화
        if (!initializeRepositories()) {
            LogManager::getInstance().Error("Repositories 초기화 실패");
            return false;
        }
        
        // 4. AlarmSubscriber 초기화 및 시작
        if (!initializeAlarmSubscriber()) {
            LogManager::getInstance().Error("AlarmSubscriber 초기화 실패");
            return false;
        }
        
        // 5. ScheduledExporter 초기화 및 시작
        if (!initializeScheduledExporter()) {
            LogManager::getInstance().Error("ScheduledExporter 초기화 실패");
            return false;
        }
        
        is_running_ = true;
        LogManager::getInstance().Info("ExportCoordinator 시작 완료 ✅");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportCoordinator 시작 실패: " + 
                                       std::string(e.what()));
        stop();
        return false;
    }
}

void ExportCoordinator::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("ExportCoordinator 중지 중...");
    
    // 1. AlarmSubscriber 중지
    if (alarm_subscriber_) {
        LogManager::getInstance().Info("AlarmSubscriber 중지 중...");
        alarm_subscriber_->stop();
    }
    
    // 2. ScheduledExporter 중지
    if (scheduled_exporter_) {
        LogManager::getInstance().Info("ScheduledExporter 중지 중...");
        scheduled_exporter_->stop();
    }
    
    // 3. 공유 리소스 정리
    cleanupSharedResources();
    
    is_running_ = false;
    LogManager::getInstance().Info("ExportCoordinator 중지 완료");
}

// =============================================================================
// 공유 리소스 관리
// =============================================================================

bool ExportCoordinator::initializeSharedResources() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    
    if (shared_resources_initialized_.load()) {
        LogManager::getInstance().Info("공유 리소스가 이미 초기화되었습니다");
        return true;
    }
    
    try {
        LogManager::getInstance().Info("공유 리소스 초기화 시작...");
        
        // 1. DynamicTargetManager 싱글턴 가져오기 및 시작
        if (!shared_target_manager_) {
            LogManager::getInstance().Info("DynamicTargetManager 초기화 중...");
            
            // 싱글턴 인스턴스 가져오기
            shared_target_manager_ = std::shared_ptr<PulseOne::CSP::DynamicTargetManager>(
                &PulseOne::CSP::DynamicTargetManager::getInstance(),
                [](PulseOne::CSP::DynamicTargetManager*){} // no-op 삭제자 (싱글턴이므로)
            );
            
            // DB에서 타겟 로드 및 시작
            if (!shared_target_manager_->start()) {
                LogManager::getInstance().Error("DynamicTargetManager 시작 실패");
                return false;
            }
            
            LogManager::getInstance().Info("DynamicTargetManager 초기화 완료");
        }
        
        // 2. PayloadTransformer 싱글턴 가져오기
        if (!shared_payload_transformer_) {
            LogManager::getInstance().Info("PayloadTransformer 초기화 중...");
            
            // 싱글턴 인스턴스 가져오기
            shared_payload_transformer_ = std::shared_ptr<PulseOne::Transform::PayloadTransformer>(
                &PulseOne::Transform::PayloadTransformer::getInstance(),
                [](PulseOne::Transform::PayloadTransformer*){} // no-op 삭제자 (싱글턴이므로)
            );
            
            LogManager::getInstance().Info("PayloadTransformer 초기화 완료");
        }
        
        shared_resources_initialized_ = true;
        LogManager::getInstance().Info("공유 리소스 초기화 완료 ✅");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("공유 리소스 초기화 실패: " + 
                                       std::string(e.what()));
        shared_target_manager_.reset();
        shared_payload_transformer_.reset();
        return false;
    }
}


void ExportCoordinator::cleanupSharedResources() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    
    if (!shared_resources_initialized_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("공유 리소스 정리 중...");
    
    // DynamicTargetManager 정리
    if (shared_target_manager_) {
        shared_target_manager_->stop();
        shared_target_manager_.reset();
        LogManager::getInstance().Info("DynamicTargetManager 정리 완료");
    }
    
    // PayloadTransformer 정리
    if (shared_payload_transformer_) {
        shared_payload_transformer_.reset();
        LogManager::getInstance().Info("PayloadTransformer 정리 완료");
    }
    
    shared_resources_initialized_ = false;
    LogManager::getInstance().Info("공유 리소스 정리 완료");
}

std::shared_ptr<PulseOne::CSP::DynamicTargetManager> 
ExportCoordinator::getTargetManager() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    return shared_target_manager_;
}

std::shared_ptr<PulseOne::Transform::PayloadTransformer> 
ExportCoordinator::getPayloadTransformer() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    return shared_payload_transformer_;
}

// =============================================================================
// 내부 초기화 메서드
// =============================================================================

bool ExportCoordinator::initializeDatabase() {
    try {
        LogManager::getInstance().Info("데이터베이스 초기화 중...");
        
        std::string db_path = getDatabasePath();
        
        // DatabaseManager를 통한 연결 확인
        auto& db_manager = DatabaseManager::getInstance();
        
        // 연결 테스트
        std::vector<std::vector<std::string>> test_result;
        if (!db_manager.executeQuery("SELECT 1", test_result)) {
            LogManager::getInstance().Error("데이터베이스 연결 실패");
            return false;
        }
        
        LogManager::getInstance().Info("데이터베이스 초기화 완료: " + db_path);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("데이터베이스 초기화 실패: " + 
                                       std::string(e.what()));
        return false;
    }
}

bool ExportCoordinator::initializeRepositories() {
    try {
        LogManager::getInstance().Info("Repositories 초기화 중...");
        
        log_repo_ = std::make_unique<
            PulseOne::Database::Repositories::ExportLogRepository>();
        
        target_repo_ = std::make_unique<
            PulseOne::Database::Repositories::ExportTargetRepository>();
        
        LogManager::getInstance().Info("Repositories 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Repositories 초기화 실패: " + 
                                       std::string(e.what()));
        return false;
    }
}

bool ExportCoordinator::initializeAlarmSubscriber() {
    try {
        LogManager::getInstance().Info("AlarmSubscriber 초기화 중...");
        
        // AlarmSubscriber 설정
        PulseOne::Alarm::AlarmSubscriberConfig alarm_config;
        alarm_config.redis_host = config_.redis_host;
        alarm_config.redis_port = config_.redis_port;
        alarm_config.redis_password = config_.redis_password;
        alarm_config.subscribe_channels = config_.alarm_channels;
        alarm_config.subscribe_patterns = config_.alarm_patterns;
        alarm_config.worker_thread_count = config_.alarm_worker_threads;
        alarm_config.max_queue_size = config_.alarm_max_queue_size;
        alarm_config.enable_debug_log = config_.enable_debug_log;
        
        // AlarmSubscriber 생성
        alarm_subscriber_ = std::make_unique<PulseOne::Alarm::AlarmSubscriber>(alarm_config);
        
        // AlarmSubscriber 시작
        if (!alarm_subscriber_->start()) {
            LogManager::getInstance().Error("AlarmSubscriber 시작 실패");
            return false;
        }
        
        LogManager::getInstance().Info("AlarmSubscriber 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmSubscriber 초기화 실패: " + 
                                       std::string(e.what()));
        return false;
    }
}

bool ExportCoordinator::initializeScheduledExporter() {
    try {
        LogManager::getInstance().Info("ScheduledExporter 초기화 중...");
        
        // ScheduledExporter 설정
        PulseOne::Schedule::ScheduledExporterConfig schedule_config;
        schedule_config.redis_host = config_.redis_host;
        schedule_config.redis_port = config_.redis_port;
        schedule_config.redis_password = config_.redis_password;
        schedule_config.check_interval_seconds = config_.schedule_check_interval_seconds;
        schedule_config.reload_interval_seconds = config_.schedule_reload_interval_seconds;
        schedule_config.batch_size = config_.schedule_batch_size;
        schedule_config.enable_debug_log = config_.enable_debug_log;
        
        // ScheduledExporter 생성
        scheduled_exporter_ = std::make_unique<PulseOne::Schedule::ScheduledExporter>(
            schedule_config);
        
        // ScheduledExporter 시작
        if (!scheduled_exporter_->start()) {
            LogManager::getInstance().Error("ScheduledExporter 시작 실패");
            return false;
        }
        
        LogManager::getInstance().Info("ScheduledExporter 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ScheduledExporter 초기화 실패: " + 
                                       std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 알람 전송 조율
// =============================================================================

std::vector<ExportResult> ExportCoordinator::handleAlarmEvent(
    const PulseOne::CSP::AlarmMessage& alarm) {
    
    std::vector<ExportResult> results;
    
    try {
        LogManager::getInstance().Info("알람 이벤트 처리: " + alarm.nm);
        
        auto target_manager = getTargetManager();
        if (!target_manager) {
            LogManager::getInstance().Error("TargetManager가 초기화되지 않았습니다");
            return results;
        }
        
        // DynamicTargetManager를 통한 전송
        auto target_results = target_manager->sendAlarmToAllTargets(alarm);
        
        // 결과 변환 및 로깅
        for (const auto& target_result : target_results) {
            ExportResult result = convertTargetSendResult(target_result);
            results.push_back(result);
            
            logExportResult(result, &alarm);
            updateStats(result);
        }
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.alarm_events++;
            stats_.alarm_exports += results.size();
        }
        
        LogManager::getInstance().Info("알람 전송 완료: " + 
            std::to_string(results.size()) + "개 타겟");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("알람 이벤트 처리 실패: " + 
                                       std::string(e.what()));
    }
    
    return results;
}

std::vector<ExportResult> ExportCoordinator::handleAlarmBatch(
    const std::vector<PulseOne::CSP::AlarmMessage>& alarms) {
    
    std::vector<ExportResult> all_results;
    
    try {
        LogManager::getInstance().Info("알람 배치 처리: " + 
                                      std::to_string(alarms.size()) + "개");
        
        for (const auto& alarm : alarms) {
            auto results = handleAlarmEvent(alarm);
            all_results.insert(all_results.end(), results.begin(), results.end());
        }
        
        LogManager::getInstance().Info("알람 배치 처리 완료: " + 
                                      std::to_string(all_results.size()) + "개 전송");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("알람 배치 처리 실패: " + 
                                       std::string(e.what()));
    }
    
    return all_results;
}

// =============================================================================
// 스케줄 전송 조율
// =============================================================================

std::vector<ExportResult> ExportCoordinator::handleScheduledExport(int schedule_id) {
    std::vector<ExportResult> results;
    
    try {
        LogManager::getInstance().Info("스케줄 전송: ID=" + std::to_string(schedule_id));
        
        if (!scheduled_exporter_) {
            LogManager::getInstance().Error("ScheduledExporter가 초기화되지 않았습니다");
            return results;
        }
        
        // ScheduledExporter를 통한 실행
        auto execution_result = scheduled_exporter_->executeSchedule(schedule_id);
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.schedule_executions++;
            stats_.schedule_exports += execution_result.exported_points;
        }
        
        LogManager::getInstance().Info("스케줄 전송 완료: " + 
            std::to_string(execution_result.exported_points) + "개 포인트 전송");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("스케줄 전송 실패: " + 
                                       std::string(e.what()));
    }
    
    return results;
}

ExportResult ExportCoordinator::handleManualExport(
    const std::string& target_name, 
    const nlohmann::json& data) {
    
    ExportResult result;
    result.target_name = target_name;
    
    try {
        LogManager::getInstance().Info("수동 전송: " + target_name);
        
        auto target_manager = getTargetManager();
        if (!target_manager) {
            result.error_message = "TargetManager 초기화 안 됨";
            return result;
        }
        
        // 수동 전송 로직 (구현 필요)
        // TODO: DynamicTargetManager에 sendDataToTarget 메서드 추가 필요
        
        LogManager::getInstance().Info("수동 전송 완료: " + target_name);
        
    } catch (const std::exception& e) {
        result.error_message = "수동 전송 실패: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
    }
    
    return result;
}

// =============================================================================
// 로깅 및 통계
// =============================================================================

void ExportCoordinator::logExportResult(
    const ExportResult& result, 
    const PulseOne::CSP::AlarmMessage* alarm_message) {
    
    if (!log_repo_) {
        return;
    }
    
    try {
        using namespace PulseOne::Database::Entities;
        
        ExportLogEntity log_entity;
        log_entity.setLogType("alarm_export");
        log_entity.setTargetId(result.target_id);
        log_entity.setStatus(result.success ? "success" : "failure");
        log_entity.setHttpStatusCode(result.http_status_code);
        log_entity.setErrorMessage(result.error_message);
        log_entity.setProcessingTimeMs(
            static_cast<int>(result.processing_time.count()));
        
        if (alarm_message) {
            log_entity.setSourceValue(alarm_message->to_json().dump());
        }
        
        log_repo_->save(log_entity);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("로그 저장 실패: " + std::string(e.what()));
    }
}

void ExportCoordinator::logExportResults(const std::vector<ExportResult>& results) {
    for (const auto& result : results) {
        logExportResult(result);
    }
}

ExportCoordinatorStats ExportCoordinator::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void ExportCoordinator::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ExportCoordinatorStats();
    stats_.start_time = std::chrono::system_clock::now();
    LogManager::getInstance().Info("통계 초기화 완료");
}

nlohmann::json ExportCoordinator::getTargetStats(const std::string& target_name) const {
    nlohmann::json stats = nlohmann::json::object();
    
    try {
        if (!log_repo_) {
            return stats;
        }
        
        // TODO: ExportLogRepository에 getTargetStats 메서드 추가 필요
        // 임시로 빈 객체 반환
        stats["target_name"] = target_name;
        stats["total"] = 0;
        stats["success"] = 0;
        stats["failure"] = 0;
        stats["avg_time_ms"] = 0.0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("타겟 통계 조회 실패: " + 
                                       std::string(e.what()));
    }
    
    return stats;
}

// =============================================================================
// 설정 관리
// =============================================================================

int ExportCoordinator::reloadTargets() {
    try {
        LogManager::getInstance().Info("타겟 리로드 중...");
        
        auto target_manager = getTargetManager();
        if (!target_manager) {
            LogManager::getInstance().Error("TargetManager 초기화 안 됨");
            return 0;
        }
        
        // DynamicTargetManager 리로드
        if (!target_manager->forceReload()) {
            LogManager::getInstance().Error("타겟 리로드 실패");
            return 0;
        }
        
        auto target_names = target_manager->getTargetNames(false);
        int count = static_cast<int>(target_names.size());
        
        LogManager::getInstance().Info("타겟 리로드 완료: " + 
                                      std::to_string(count) + "개");
        return count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("타겟 리로드 실패: " + 
                                       std::string(e.what()));
        return 0;
    }
}

int ExportCoordinator::reloadTemplates() {
    try {
        LogManager::getInstance().Info("템플릿 리로드 중...");
        
        auto transformer = getPayloadTransformer();
        if (!transformer) {
            LogManager::getInstance().Error("PayloadTransformer 초기화 안 됨");
            return 0;
        }
        
        // TODO: PayloadTransformer에 reloadTemplates 메서드 추가 필요
        
        LogManager::getInstance().Info("템플릿 리로드 완료");
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("템플릿 리로드 실패: " + 
                                       std::string(e.what()));
        return 0;
    }
}

void ExportCoordinator::updateConfig(const ExportCoordinatorConfig& new_config) {
    std::lock_guard<std::mutex> lock(export_mutex_);
    config_ = new_config;
    LogManager::getInstance().Info("설정 업데이트 완료");
}

// =============================================================================
// 헬스 체크
// =============================================================================

bool ExportCoordinator::healthCheck() const {
    try {
        // 1. 실행 중 여부
        if (!is_running_.load()) {
            return false;
        }
        
        // 2. AlarmSubscriber 상태
        if (alarm_subscriber_ && !alarm_subscriber_->isRunning()) {
            return false;
        }
        
        // 3. ScheduledExporter 상태
        if (scheduled_exporter_ && !scheduled_exporter_->isRunning()) {
            return false;
        }
        
        // 4. DynamicTargetManager 상태
        auto target_manager = getTargetManager();
        if (!target_manager || !target_manager->isRunning()) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("헬스 체크 실패: " + 
                                       std::string(e.what()));
        return false;
    }
}

nlohmann::json ExportCoordinator::getComponentStatus() const {
    nlohmann::json status = nlohmann::json::object();
    
    try {
        status["coordinator_running"] = is_running_.load();
        
        status["alarm_subscriber"] = alarm_subscriber_ ? 
            alarm_subscriber_->isRunning() : false;
        
        status["scheduled_exporter"] = scheduled_exporter_ ? 
            scheduled_exporter_->isRunning() : false;
        
        auto target_manager = getTargetManager();
        status["target_manager"] = target_manager ? 
            target_manager->isRunning() : false;
        
        status["shared_resources_initialized"] = 
            shared_resources_initialized_.load();
        
    } catch (const std::exception& e) {
        status["error"] = e.what();
    }
    
    return status;
}

// =============================================================================
// 내부 헬퍼 메서드
// =============================================================================

ExportResult ExportCoordinator::convertTargetSendResult(
    const PulseOne::CSP::TargetSendResult& target_result) const {
    
    ExportResult result;
    result.success = target_result.success;
    result.target_name = target_result.target_name;
    result.error_message = target_result.error_message;
    result.http_status_code = target_result.status_code;  // ✅ 수정: status_code 사용
    result.processing_time = target_result.response_time;
    result.data_size = target_result.content_size;  // ✅ response_body.size() 대신 content_size 사용
    
    // target_id 조회 (target_name으로 DB 검색)
    try {
        if (target_repo_) {
            auto target_entity = target_repo_->findByName(result.target_name);
            if (target_entity.has_value()) {
                result.target_id = target_entity->getId();
            }
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("타겟 ID 조회 실패: " + 
                                      std::string(e.what()));
    }
    
    return result;
}


void ExportCoordinator::updateStats(const ExportResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_exports++;
    
    if (result.success) {
        stats_.successful_exports++;
    } else {
        stats_.failed_exports++;
    }
    
    stats_.last_export_time = std::chrono::system_clock::now();
    
    // 평균 처리 시간 계산
    if (stats_.total_exports > 0) {
        double current_avg = stats_.avg_processing_time_ms;
        double new_time = static_cast<double>(result.processing_time.count());
        stats_.avg_processing_time_ms = 
            (current_avg * (stats_.total_exports - 1) + new_time) / stats_.total_exports;
    }
}

std::string ExportCoordinator::getDatabasePath() const {
    // ConfigManager에서 DB 경로 가져오기
    auto& config_mgr = ConfigManager::getInstance();
    std::string db_path = config_mgr.getOrDefault("DATABASE_PATH", config_.database_path);
    return db_path;
}

} // namespace Coordinator
} // namespace PulseOne