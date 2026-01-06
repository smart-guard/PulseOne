/**
 * @file ExportCoordinator.cpp
 * @brief Export 시스템 중앙 조율자 구현
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0
 * 
 * v2.0 변경사항:
 * - AlarmSubscriber → EventSubscriber (범용 이벤트 구독)
 * - 스케줄 이벤트 지원 추가
 * - 하위 호환성 유지
 */

#include "CSP/ExportCoordinator.h"
#include "Database/DatabaseManager.h"
#include <algorithm>
#include <numeric>

namespace PulseOne {
namespace Coordinator {

// =============================================================================
// Forward declarations
// =============================================================================

// ScheduleEventHandler 내부 클래스
class ScheduleEventHandler : public PulseOne::Event::IEventHandler {
private:
    ExportCoordinator* coordinator_;
    
public:
    explicit ScheduleEventHandler(ExportCoordinator* coordinator)
        : coordinator_(coordinator) {}
    
    bool handleEvent(const std::string& channel, const std::string& message) override {
        coordinator_->handleScheduleEvent(channel, message);
        return true;
    }
    
    std::string getName() const override {
        return "ScheduleEventHandler";
    }
};

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
    
    LogManager::getInstance().Info("ExportCoordinator v2.0 초기화 시작");
    LogManager::getInstance().Info("데이터베이스: " + config_.database_path);
    LogManager::getInstance().Info("Redis: " + config_.redis_host + ":" + 
                                  std::to_string(config_.redis_port));
    LogManager::getInstance().Info("✅ EventSubscriber: 범용 이벤트 구독자");
    
    stats_.start_time = std::chrono::system_clock::now();
}

ExportCoordinator::~ExportCoordinator() {
    try {
        stop();
        LogManager::getInstance().Info("ExportCoordinator 소멸 완료");
    } catch (const std::exception& e) {
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
        // 1. 공유 리소스 초기화
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
        
        // 4. EventSubscriber 초기화 및 시작
        if (!initializeEventSubscriber()) {
            LogManager::getInstance().Error("EventSubscriber 초기화 실패");
            return false;
        }
        
        // 5. ScheduledExporter 초기화 및 시작
        if (!initializeScheduledExporter()) {
            LogManager::getInstance().Error("ScheduledExporter 초기화 실패");
            return false;
        }
        
        is_running_ = true;
        LogManager::getInstance().Info("ExportCoordinator v2.0 시작 완료 ✅");
        
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
    
    // 1. EventSubscriber 중지
    if (event_subscriber_) {
        LogManager::getInstance().Info("EventSubscriber 중지 중...");
        event_subscriber_->stop();
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
        
        // 1. DynamicTargetManager 싱글턴
        if (!shared_target_manager_) {
            LogManager::getInstance().Info("DynamicTargetManager 초기화 중...");
            
            shared_target_manager_ = std::shared_ptr<PulseOne::CSP::DynamicTargetManager>(
                &PulseOne::CSP::DynamicTargetManager::getInstance(),
                [](PulseOne::CSP::DynamicTargetManager*){} // no-op 삭제자
            );
            
            if (!shared_target_manager_->start()) {
                LogManager::getInstance().Error("DynamicTargetManager 시작 실패");
                return false;
            }
            
            LogManager::getInstance().Info("DynamicTargetManager 초기화 완료");
        }
        
        // 2. PayloadTransformer 싱글턴
        if (!shared_payload_transformer_) {
            LogManager::getInstance().Info("PayloadTransformer 초기화 중...");
            
            shared_payload_transformer_ = std::shared_ptr<PulseOne::Transform::PayloadTransformer>(
                &PulseOne::Transform::PayloadTransformer::getInstance(),
                [](PulseOne::Transform::PayloadTransformer*){}
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
    
    if (shared_target_manager_) {
        shared_target_manager_->stop();
        shared_target_manager_.reset();
        LogManager::getInstance().Info("DynamicTargetManager 정리 완료");
    }
    
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
        
        auto& db_manager = DatabaseManager::getInstance();
        
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

bool ExportCoordinator::initializeEventSubscriber() {
    try {
        LogManager::getInstance().Info("EventSubscriber 초기화 중...");
        
        // EventSubscriber 설정
        PulseOne::Event::EventSubscriberConfig event_config;
        event_config.redis_host = config_.redis_host;
        event_config.redis_port = config_.redis_port;
        event_config.redis_password = config_.redis_password;
        
        // ✅ 알람 채널 + 스케줄 이벤트 채널
        event_config.subscribe_channels = config_.alarm_channels;
        event_config.subscribe_channels.push_back("schedule:reload");
        event_config.subscribe_channels.push_back("schedule:execute:*");
        
        event_config.subscribe_patterns = config_.alarm_patterns;
        event_config.worker_thread_count = config_.alarm_worker_threads;
        event_config.max_queue_size = config_.alarm_max_queue_size;
        event_config.enable_debug_log = config_.enable_debug_log;
        
        // EventSubscriber 생성
        event_subscriber_ = std::make_unique<PulseOne::Event::EventSubscriber>(
            event_config);
        
        // ✅ 스케줄 이벤트 핸들러 등록
        auto schedule_handler = std::make_shared<ScheduleEventHandler>(this);
        event_subscriber_->registerHandler("schedule:*", schedule_handler);
        
        // EventSubscriber 시작
        if (!event_subscriber_->start()) {
            LogManager::getInstance().Error("EventSubscriber 시작 실패");
            return false;
        }
        
        LogManager::getInstance().Info("EventSubscriber 초기화 완료 ✅");
        LogManager::getInstance().Info("  - 알람 채널: " + 
            std::to_string(config_.alarm_channels.size()) + "개");
        LogManager::getInstance().Info("  - 스케줄 이벤트: 활성화");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("EventSubscriber 초기화 실패: " + 
                                       std::string(e.what()));
        return false;
    }
}

bool ExportCoordinator::initializeScheduledExporter() {
    try {
        LogManager::getInstance().Info("ScheduledExporter 초기화 중...");
        
        PulseOne::Schedule::ScheduledExporterConfig schedule_config;
        schedule_config.redis_host = config_.redis_host;
        schedule_config.redis_port = config_.redis_port;
        schedule_config.redis_password = config_.redis_password;
        schedule_config.check_interval_seconds = config_.schedule_check_interval_seconds;
        schedule_config.reload_interval_seconds = config_.schedule_reload_interval_seconds;
        schedule_config.batch_size = config_.schedule_batch_size;
        schedule_config.enable_debug_log = config_.enable_debug_log;
        
        scheduled_exporter_ = std::make_unique<PulseOne::Schedule::ScheduledExporter>(
            schedule_config);
        
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
// ✅ 이벤트 핸들러 (간소화)
// =============================================================================

void ExportCoordinator::handleScheduleEvent(const std::string& channel, const std::string& message) {
    try {
        LogManager::getInstance().Info("🔄 스케줄 이벤트 수신: " + channel);
        
        if (!scheduled_exporter_) {
            LogManager::getInstance().Warn("ScheduledExporter가 초기화되지 않음");
            return;
        }
        
        // ✅ schedule:reload 처리
        if (channel == "schedule:reload") {
            int loaded = scheduled_exporter_->reloadSchedules();
            LogManager::getInstance().Info(
                "✅ 스케줄 리로드 완료: " + std::to_string(loaded) + "개");
        }
        // ✅ schedule:execute:{id} 처리 (NEW!)
        else if (channel.find("schedule:execute:") == 0) {
            std::string id_str = channel.substr(17); // "schedule:execute:" 이후
            try {
                int schedule_id = std::stoi(id_str);
                LogManager::getInstance().Info(
                    "⚡ 스케줄 실행 요청: ID=" + std::to_string(schedule_id)
                );
                
                auto result = scheduled_exporter_->executeSchedule(schedule_id);
                
                if (result.success) {
                    LogManager::getInstance().Info(
                        "✅ 스케줄 실행 완료: " + std::to_string(result.data_point_count) + "개 데이터 포인트"
                    );
                } else {
                    LogManager::getInstance().Error(
                        "❌ 스케줄 실행 실패: " + result.error_message
                    );
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().Error(
                    "스케줄 ID 파싱 실패: " + std::string(e.what())
                );
            }
        }
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.schedule_events++;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "스케줄 이벤트 처리 실패: " + std::string(e.what()));
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
        
        auto target_results = target_manager->sendAlarmToTargets(alarm);
        
        for (const auto& target_result : target_results) {
            ExportResult result = convertTargetSendResult(target_result);
            results.push_back(result);
            
            logExportResult(result, &alarm);
            updateStats(result);
        }
        
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
        
        auto execution_result = scheduled_exporter_->executeSchedule(schedule_id);
        
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
    
    ExportCoordinatorStats current_stats = stats_;
    
    // EventSubscriber의 통계 합산 (v3.0 통합)
    if (event_subscriber_) {
        auto sub_stats = event_subscriber_->getStats();
        current_stats.total_exports += sub_stats.total_processed;
        current_stats.alarm_events += sub_stats.total_processed;
        
        // 성공/실패 합산 (EventSubscriber는 현재 성공만 카운트하거나 실패는 따로 관리)
        current_stats.successful_exports += sub_stats.total_processed;
        current_stats.failed_exports += sub_stats.total_failed;
    }
    
    return current_stats;
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
        
        if (!target_manager->forceReload()) {
            LogManager::getInstance().Error("타겟 리로드 실패");
            return 0;
        }
        
        auto targets = target_manager->getAllTargets();
        int reloaded_count = targets.size();
        
        LogManager::getInstance().Info("타겟 리로드 완료: " + 
            std::to_string(reloaded_count) + "개");
        
        return reloaded_count;
        
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
        if (!is_running_.load()) {
            return false;
        }
        
        if (event_subscriber_ && !event_subscriber_->isRunning()) {
            return false;
        }
        
        if (scheduled_exporter_ && !scheduled_exporter_->isRunning()) {
            return false;
        }
        
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
        
        status["event_subscriber"] = event_subscriber_ ? 
            event_subscriber_->isRunning() : false;
        
        status["scheduled_exporter"] = scheduled_exporter_ ? 
            scheduled_exporter_->isRunning() : false;
        
        auto target_manager = getTargetManager();
        status["target_manager"] = target_manager ? 
            target_manager->isRunning() : false;
        
        status["shared_resources_initialized"] = 
            shared_resources_initialized_.load();
        
        status["version"] = "2.0";
        status["features"] = json::array({"alarm_events", "schedule_events", "manual_export"});
        
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
    result.http_status_code = target_result.status_code;
    result.processing_time = target_result.response_time;
    result.data_size = target_result.content_size;
    
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
    
    if (stats_.total_exports > 0) {
        double current_avg = stats_.avg_processing_time_ms;
        double new_time = static_cast<double>(result.processing_time.count());
        stats_.avg_processing_time_ms = 
            (current_avg * (stats_.total_exports - 1) + new_time) / stats_.total_exports;
    }
}

std::string ExportCoordinator::getDatabasePath() const {
    auto& config_mgr = ConfigManager::getInstance();
    std::string db_path = config_mgr.getOrDefault("DATABASE_PATH", config_.database_path);
    return db_path;
}

} // namespace Coordinator
} // namespace PulseOne