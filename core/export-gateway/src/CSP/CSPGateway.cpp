/**
 * @file CSPGateway.cpp - v2.0 스키마 완전 적응 버전
 * @brief CSP Gateway 구현부 - 완전한 파일
 * @author PulseOne Development Team
 * @date 2025-10-21
 * @version 2.0.1
 */

#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include "CSP/DynamicTargetManager.h"
#include "Client/HttpClient.h"
#include "Client/S3Client.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportLogEntity.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <future>
#include <regex>
#include <iomanip>

namespace PulseOne {
namespace CSP {

CSPGateway::CSPGateway(const CSPGatewayConfig& config) : config_(config) {
    LogManager::getInstance().Info("CSPGateway initializing...");
    loadConfigFromConfigManager();
    initializeHttpClient();
    initializeS3Client();
    initializeDynamicTargetSystem();
    try {
        std::filesystem::create_directories(config_.alarm_dir_path);
        std::filesystem::create_directories(config_.failed_file_path);
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("Failed to create directories: " + std::string(e.what()));
    }
    LogManager::getInstance().Info("CSPGateway initialized successfully");
}

CSPGateway::~CSPGateway() {
    stop();
}

void CSPGateway::initializeDynamicTargetSystem() {
    if (!config_.use_dynamic_targets) {
        LogManager::getInstance().Info("Dynamic Target System disabled");
        return;
    }
    try {
        LogManager::getInstance().Info("Initializing Dynamic Target System (Database Mode)...");
        std::string db_path = getDatabasePath();
        if (db_path.empty()) {
            LogManager::getInstance().Error("Database path not configured");
            return;
        }
        LogManager::getInstance().Debug("Database path: " + db_path);
        dynamic_target_manager_ = std::make_unique<DynamicTargetManager>("");
        LogManager::getInstance().Info("Skipping JSON config file - using database mode");
        if (loadTargetsFromDatabase()) {
            use_dynamic_targets_ = true;
            LogManager::getInstance().Info("Dynamic Target System initialized successfully from database");
        } else {
            LogManager::getInstance().Warn("No targets loaded from database - system will start with empty target list");
            use_dynamic_targets_ = true;
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception initializing Dynamic Target System: " + std::string(e.what()));
        dynamic_target_manager_.reset();
    }
}

bool CSPGateway::loadTargetsFromDatabase() {
    try {
        LogManager::getInstance().Info("Loading targets from database...");
        using namespace PulseOne::Database::Repositories;
        using namespace PulseOne::Database::Entities;
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
                try {
                    target.config = json::parse(entity.getConfig());
                } catch (const std::exception& e) {
                    LogManager::getInstance().Error("Failed to parse config JSON for target: " + target.name + ", error: " + e.what());
                    continue;
                }
                target.config["id"] = entity.getId();
                if (!entity.getDescription().empty()) {
                    target.config["description"] = entity.getDescription();
                }
                auto stats = getTargetStatisticsFromLogs(entity.getId());
                if (stats.has_value()) {
                    target.config["total_exports"] = stats->total_count;
                    target.config["successful_exports"] = stats->success_count;
                    target.config["failed_exports"] = stats->failure_count;
                    target.config["avg_response_time_ms"] = stats->avg_response_time_ms;
                }
                if (dynamic_target_manager_ && dynamic_target_manager_->addTarget(target)) {
                    loaded_count++;
                    LogManager::getInstance().Debug("Loaded target: " + target.name + " (" + target.type + ")");
                } else {
                    LogManager::getInstance().Warn("Failed to add target to manager: " + target.name);
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("Error processing target entity: " + std::string(e.what()));
                continue;
            }
        }
        LogManager::getInstance().Info("Loaded " + std::to_string(loaded_count) + " targets from database");
        return (loaded_count > 0);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception loading targets from database: " + std::string(e.what()));
        return false;
    }
}

std::optional<TargetStatistics> CSPGateway::getTargetStatisticsFromLogs(int target_id, int hours) {
    try {
        using namespace PulseOne::Database::Repositories;
        ExportLogRepository log_repo;
        auto stats_map = log_repo.getTargetStatistics(target_id, hours);
        if (stats_map.empty()) {
            return std::nullopt;
        }
        TargetStatistics stats;
        int success = 0;
        int failed = 0;
        for (const auto& [status, count] : stats_map) {
            if (status == "success") {
                success = count;
            } else if (status == "failed") {
                failed = count;
            }
        }
        stats.success_count = success;
        stats.failure_count = failed;
        stats.total_count = success + failed;
        stats.avg_response_time_ms = log_repo.getAverageProcessingTime(target_id, hours);
        return stats;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to get target statistics: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool CSPGateway::addDynamicTarget(const std::string& name, const std::string& type, const json& config, bool enabled, int priority) {
    try {
        if (!dynamic_target_manager_) {
            LogManager::getInstance().Error("Dynamic Target Manager not initialized");
            return false;
        }
        DynamicTarget target;
        target.name = name;
        target.type = type;
        target.config = config;
        target.enabled = enabled;
        target.priority = priority;
        bool manager_success = dynamic_target_manager_->addTarget(target);
        if (!manager_success) {
            LogManager::getInstance().Error("Failed to add target to manager: " + name);
            return false;
        }
        bool db_success = saveTargetToDatabase(target);
        if (db_success) {
            LogManager::getInstance().Info("Dynamic target added and saved to DB: " + name);
        } else {
            LogManager::getInstance().Warn("Target added to manager but failed to save to DB: " + name);
        }
        return manager_success;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception adding dynamic target: " + std::string(e.what()));
        return false;
    }
}

bool CSPGateway::removeDynamicTarget(const std::string& name) {
    if (!use_dynamic_targets_ || !dynamic_target_manager_) {
        LogManager::getInstance().Error("Dynamic Target System not available");
        return false;
    }
    bool manager_success = dynamic_target_manager_->removeTarget(name);
    if (manager_success) {
        bool db_success = deleteTargetFromDatabase(name);
        if (db_success) {
            LogManager::getInstance().Info("Target removed from both manager and DB: " + name);
        } else {
            LogManager::getInstance().Warn("Target removed from manager but failed to delete from DB: " + name);
        }
    }
    return manager_success;
}

bool CSPGateway::enableDynamicTarget(const std::string& name, bool enabled) {
    if (!use_dynamic_targets_ || !dynamic_target_manager_) {
        LogManager::getInstance().Error("Dynamic Target System not available");
        return false;
    }
    bool manager_success = dynamic_target_manager_->enableTarget(name, enabled);
    if (manager_success) {
        bool db_success = updateTargetEnabledStatus(name, enabled);
        if (db_success) {
            LogManager::getInstance().Info("Target " + name + " " + (enabled ? "enabled" : "disabled") + " in both manager and DB");
        } else {
            LogManager::getInstance().Warn("Target status updated in manager but failed in DB: " + name);
        }
    }
    return manager_success;
}

bool CSPGateway::reloadDynamicTargets() {
    if (!use_dynamic_targets_ || !dynamic_target_manager_) {
        LogManager::getInstance().Error("Dynamic Target System not available");
        return false;
    }
    try {
        LogManager::getInstance().Info("Reloading dynamic targets from database...");
        auto existing_targets = getDynamicTargetStats();
        for (const auto& target_stat : existing_targets) {
            dynamic_target_manager_->removeTarget(target_stat.name);
        }
        return loadTargetsFromDatabase();
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to reload dynamic targets: " + std::string(e.what()));
        return false;
    }
}

std::string CSPGateway::getDatabasePath() const {
    ConfigManager& config_mgr = ConfigManager::getInstance();
    std::string db_path = config_mgr.getOrDefault("CSP_DATABASE_PATH", "");
    if (db_path.empty()) {
        db_path = config_mgr.getOrDefault("DATABASE_PATH", "/app/data/pulseone.db");
    }
    LogManager::getInstance().Debug("Database path: " + db_path);
    return db_path;
}

bool CSPGateway::saveTargetToDatabase(const DynamicTarget& target) {
    try {
        using namespace PulseOne::Database::Repositories;
        using namespace PulseOne::Database::Entities;
        ExportTargetRepository repo;
        auto existing = repo.findByName(target.name);
        ExportTargetEntity entity;
        if (existing.has_value()) {
            entity = existing.value();
        }
        entity.setName(target.name);
        entity.setTargetType(target.type);
        entity.setConfig(target.config.dump());
        entity.setEnabled(target.enabled);
        if (target.config.contains("description")) {
            entity.setDescription(target.config["description"].get<std::string>());
        }
        bool success = existing.has_value() ? repo.update(entity) : repo.save(entity);
        if (success) {
            LogManager::getInstance().Debug("Target saved to DB: " + target.name);
        } else {
            LogManager::getInstance().Error("Failed to save target to DB: " + target.name);
        }
        return success;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception saving target to DB: " + std::string(e.what()));
        return false;
    }
}

bool CSPGateway::deleteTargetFromDatabase(const std::string& name) {
    try {
        using namespace PulseOne::Database::Repositories;
        ExportTargetRepository repo;
        auto existing = repo.findByName(name);
        if (!existing.has_value()) {
            LogManager::getInstance().Warn("Target not found in DB: " + name);
            return false;
        }
        bool success = repo.deleteById(existing->getId());
        if (success) {
            LogManager::getInstance().Debug("Target deleted from DB: " + name);
        } else {
            LogManager::getInstance().Error("Failed to delete target from DB: " + name);
        }
        return success;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception deleting target from DB: " + std::string(e.what()));
        return false;
    }
}

bool CSPGateway::updateTargetEnabledStatus(const std::string& name, bool enabled) {
    try {
        using namespace PulseOne::Database::Repositories;
        ExportTargetRepository repo;
        auto existing = repo.findByName(name);
        if (!existing.has_value()) {
            LogManager::getInstance().Warn("Target not found in DB: " + name);
            return false;
        }
        auto entity = existing.value();
        entity.setEnabled(enabled);
        bool success = repo.update(entity);
        if (success) {
            LogManager::getInstance().Debug("Target status updated in DB: " + name + " = " + (enabled ? "enabled" : "disabled"));
        } else {
            LogManager::getInstance().Error("Failed to update target status in DB: " + name);
        }
        return success;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception updating target status in DB: " + std::string(e.what()));
        return false;
    }
}

/**
 * @brief Export 타겟 통계 업데이트 및 로그 저장
 * @param target_name 타겟 이름
 * @param target_type 타겟 타입 (HTTP, HTTPS, S3, FILE 등)
 * @param success 성공 여부
 * @param response_time_ms 응답 시간 (밀리초)
 */
void CSPGateway::updateExportTargetStats(
    const std::string& target_name,
    const std::string& target_type,  // ✅ 추가된 파라미터
    bool success,
    double response_time_ms
) {
    try {
        using namespace PulseOne::Database::Repositories;
        using namespace PulseOne::Database::Entities;
        
        // 1. 타겟 조회
        ExportTargetRepository target_repo;
        auto target = target_repo.findByName(target_name);
        if (!target.has_value()) {
            LogManager::getInstance().Warn("Target not found for stats update: " + target_name);
            return;
        }
        
        // 2. 로그 엔티티 생성
        ExportLogRepository log_repo;
        ExportLogEntity log_entity;
        
        // ✅ 핵심 수정: target_type을 소문자로 정규화하여 log_type에 설정
        std::string normalized_type = normalizeTargetType(target_type);
        log_entity.setLogType(normalized_type);
        
        log_entity.setTargetId(target->getId());
        log_entity.setStatus(success ? "success" : "failed");
        log_entity.setProcessingTimeMs(static_cast<int>(response_time_ms));
        
        if (!success) {
            log_entity.setErrorMessage("Export failed");
        }
        
        // 3. 로그 저장
        if (!log_repo.save(log_entity)) {
            LogManager::getInstance().Warn("Failed to save export log for target: " + target_name);
        } else {
            LogManager::getInstance().Debug(
                "Export log saved - target: " + target_name + 
                ", type: " + normalized_type + 
                ", status: " + (success ? "success" : "failed")
            );
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception updating target stats: " + std::string(e.what()));
    }
}

/**
 * @brief 타겟 타입을 log_type 형식으로 정규화
 * @param target_type 원본 타겟 타입 (HTTP, HTTPS, S3, FILE 등)
 * @return 정규화된 타입 (http, s3, file)
 */
std::string CSPGateway::normalizeTargetType(const std::string& target_type) {
    // 소문자로 변환
    std::string normalized = target_type;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    // HTTPS는 http로 통일
    if (normalized == "https") {
        normalized = "http";
    }
    
    LogManager::getInstance().Debug("Normalized target type: " + target_type + " -> " + normalized);
    
    return normalized;
}

AlarmSendResult CSPGateway::taskAlarmSingleDynamic(const AlarmMessage& alarm_message) {
    try {
        LogManager::getInstance().Debug("Using Dynamic Target System for: " + alarm_message.nm);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        auto target_results = dynamic_target_manager_->sendAlarmToAllTargets(alarm_message);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        AlarmSendResult result;
        bool has_success = false;
        size_t successful_targets = 0;
        
        // 각 타겟 결과 처리
        for (const auto& target_result : target_results) {
            double response_time_ms = std::chrono::duration<double, std::milli>(target_result.response_time).count();
            
            // ✅ 핵심 수정: target_type 파라미터 추가
            updateExportTargetStats(
                target_result.target_name,
                target_result.target_type,  // ✅ 추가 - 타겟 타입 전달
                target_result.success,
                response_time_ms
            );
            
            if (target_result.success) {
                successful_targets++;
                has_success = true;
                
                // HTTP/HTTPS 결과 처리
                if (target_result.target_type == "HTTP" || target_result.target_type == "HTTPS") {
                    result.status_code = target_result.status_code;
                    result.response_body = target_result.response_body;
                }
                
                // S3 결과 처리
                if (target_result.target_type == "S3") {
                    result.s3_success = true;
                    result.s3_file_path = target_result.s3_object_key;
                }
                
                // FILE 결과 처리
                if (target_result.target_type == "FILE") {
                    result.s3_file_path = target_result.file_path;
                }
            }
        }
        
        result.success = has_success;
        result.response_time = duration;
        
        // 에러 메시지 수집
        if (!has_success || successful_targets < target_results.size()) {
            std::string error_msg = "Dynamic targets errors: ";
            bool first = true;
            for (const auto& target_result : target_results) {
                if (!target_result.success && !target_result.error_message.empty()) {
                    if (!first) error_msg += "; ";
                    error_msg += target_result.target_name + ": " + target_result.error_message;
                    first = false;
                }
            }
            if (!error_msg.empty() && error_msg != "Dynamic targets errors: ") {
                result.error_message = error_msg;
            }
        }
        
        updateStatsFromDynamicResults(target_results, static_cast<double>(duration.count()));
        
        LogManager::getInstance().Debug(
            "Dynamic target system processed " + std::to_string(target_results.size()) + 
            " targets, " + std::to_string(successful_targets) + " successful"
        );
        
        return result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception in taskAlarmSingleDynamic: " + std::string(e.what()));
        AlarmSendResult result;
        result.success = false;
        result.error_message = "Dynamic target system error: " + std::string(e.what());
        return result;
    }
}

bool CSPGateway::start() {
    if (is_running_.load()) {
        LogManager::getInstance().Error("CSPGateway is already running");
        return false;
    }
    LogManager::getInstance().Info("Starting CSPGateway service...");
    should_stop_.store(false);
    is_running_.store(true);
    worker_thread_ = std::make_unique<std::thread>(&CSPGateway::workerThread, this);
    retry_thread_ = std::make_unique<std::thread>(&CSPGateway::retryThread, this);
    LogManager::getInstance().Info("CSPGateway service started");
    return true;
}

void CSPGateway::stop() {
    LogManager::getInstance().Info("Stopping CSPGateway service...");
    should_stop_ = true;
    if (worker_thread_ && worker_thread_->joinable()) {
        queue_cv_.notify_all();
        worker_thread_->join();
    }
    if (retry_thread_ && retry_thread_->joinable()) {
        retry_thread_->join();
    }
    if (dynamic_target_manager_) {
        LogManager::getInstance().Info("Stopping Dynamic Target System...");
        dynamic_target_manager_->stop();
    }
    is_running_.store(false);
    LogManager::getInstance().Info("CSPGateway service stopped");
}

AlarmSendResult CSPGateway::taskAlarmSingle(const AlarmMessage& alarm_message) {
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_alarms++;
    }
    LogManager::getInstance().Debug("Processing alarm: " + alarm_message.nm);
    if (use_dynamic_targets_ && dynamic_target_manager_) {
        return taskAlarmSingleDynamic(alarm_message);
    }
    return taskAlarmSingleLegacy(alarm_message);
}

AlarmSendResult CSPGateway::taskAlarmSingleLegacy(const AlarmMessage& alarm) {
    LogManager::getInstance().Debug("레거시 알람 처리: " + alarm.nm);
    AlarmSendResult result;
    result.success = false;
    result.error_message = "Legacy method not implemented - use Dynamic Targets";
    LogManager::getInstance().Warn("Legacy alarm processing not implemented yet");
    return result;
}

AlarmSendResult CSPGateway::callAPIAlarm(const AlarmMessage& alarm_message) {
    if (config_.api_endpoint.empty()) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "API endpoint not configured";
        return result;
    }
    try {
        std::string json_data = alarm_message.to_json().dump();
        std::unordered_map<std::string, std::string> headers;
        if (!config_.api_key.empty()) {
            headers["Authorization"] = "Bearer " + config_.api_key;
        }
        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = sendHttpPostRequest(config_.api_endpoint, json_data, "application/json", headers);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double response_time_ms = static_cast<double>(duration.count());
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (result.success) {
                stats_.successful_api_calls++;
                stats_.last_success_time = std::chrono::system_clock::now();
            } else {
                stats_.failed_api_calls++;
                stats_.last_failure_time = std::chrono::system_clock::now();
            }
            stats_.avg_response_time_ms = (stats_.avg_response_time_ms * 0.9) + (response_time_ms * 0.1);
        }
        return result;
    } catch (const std::exception& e) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "Exception in callAPIAlarm: " + std::string(e.what());
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.failed_api_calls++;
            stats_.last_failure_time = std::chrono::system_clock::now();
        }
        return result;
    }
}

AlarmSendResult CSPGateway::callS3Alarm(const AlarmMessage& alarm_message, const std::string& file_name) {
    if (config_.s3_bucket_name.empty()) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "S3 bucket not configured";
        return result;
    }
    try {
        std::string object_key = file_name;
        if (object_key.empty()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::ostringstream ss;
            ss << "alarms/" << alarm_message.bd << "/" << std::put_time(std::gmtime(&time_t), "%Y/%m/%d/") << "alarm_" << alarm_message.bd << "_" << alarm_message.nm << "_" << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") << ".json";
            object_key = ss.str();
        }
        std::string json_content = alarm_message.to_json().dump(2);
        bool upload_success = uploadToS3(object_key, json_content);
        AlarmSendResult result;
        result.success = upload_success;
        result.s3_success = upload_success;
        result.s3_file_path = object_key;
        if (!upload_success) {
            result.error_message = "S3 upload failed";
            result.s3_error_message = result.error_message;
        }
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (upload_success) {
                stats_.successful_s3_uploads++;
            } else {
                stats_.failed_s3_uploads++;
            }
        }
        return result;
    } catch (const std::exception& e) {
        AlarmSendResult result;
        result.success = false;
        result.error_message = "Exception in callS3Alarm: " + std::string(e.what());
        result.s3_success = false;
        result.s3_error_message = result.error_message;
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.failed_s3_uploads++;
        }
        return result;
    }
}

#ifdef HAS_SHARED_LIBS
AlarmSendResult CSPGateway::processAlarmOccurrence(const Database::Entities::AlarmOccurrenceEntity& occurrence) {
    AlarmMessage alarm_msg = AlarmMessage::from_alarm_occurrence(occurrence);
    return taskAlarmSingle(alarm_msg);
}

std::vector<AlarmSendResult> CSPGateway::processBatchAlarms(const std::vector<Database::Entities::AlarmOccurrenceEntity>& occurrences) {
    LogManager::getInstance().Info("Processing batch alarms: " + std::to_string(occurrences.size()) + " items");
    std::vector<AlarmSendResult> results;
    results.reserve(occurrences.size());
    for (const auto& occurrence : occurrences) {
        auto result = processAlarmOccurrence(occurrence);
        results.push_back(result);
    }
    return results;
}
#endif

std::unordered_map<int, BatchAlarmResult> CSPGateway::processMultiBuildingAlarms(const MultiBuildingAlarms& building_alarms) {
    LogManager::getInstance().Info("Processing multi-building alarms for " + std::to_string(building_alarms.size()) + " buildings");
    std::unordered_map<int, BatchAlarmResult> results;
    for (const auto& [building_id, alarms] : building_alarms) {
        BatchAlarmResult batch_result;
        batch_result.total_alarms = static_cast<int>(alarms.size());
        batch_result.processed_time = std::chrono::system_clock::now();
        auto filtered_alarms = filterIgnoredAlarms(alarms);
        size_t ignored_count = alarms.size() - filtered_alarms.size();
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.ignored_alarms += ignored_count;
        }
        if (filtered_alarms.empty()) {
            LogManager::getInstance().Debug("All alarms ignored for building " + std::to_string(building_id));
            batch_result.batch_file_path = "";
            results[building_id] = batch_result;
            continue;
        }
        batch_result.batch_file_path = saveBatchAlarmFile(building_id, filtered_alarms);
        int successful_calls = 0;
        int failed_calls = 0;
        for (const auto& alarm : filtered_alarms) {
            auto result = callAPIAlarm(alarm);
            if (result.success) {
                successful_calls++;
            } else {
                failed_calls++;
            }
        }
        batch_result.successful_api_calls = successful_calls;
        batch_result.failed_api_calls = failed_calls;
        if (!batch_result.batch_file_path.empty()) {
            try {
                std::ifstream file(batch_result.batch_file_path);
                std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                std::string s3_key = "batch_alarms/" + std::to_string(building_id) + "/" + std::filesystem::path(batch_result.batch_file_path).filename().string();
                batch_result.s3_success = uploadToS3(s3_key, file_content);
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("Failed to upload batch file to S3: " + std::string(e.what()));
                batch_result.s3_success = false;
            }
        }
        if (batch_result.isCompleteSuccess() && config_.auto_cleanup_success_files) {
            cleanupSuccessfulBatchFile(batch_result.batch_file_path);
        }
        results[building_id] = batch_result;
        updateBatchStats(batch_result);
    }
    return results;
}

MultiBuildingAlarms CSPGateway::groupAlarmsByBuilding(const std::vector<AlarmMessage>& alarms) {
    MultiBuildingAlarms grouped_alarms;
    for (const auto& alarm : alarms) {
        if (config_.multi_building_enabled) {
            auto& supported_ids = config_.supported_building_ids;
            if (std::find(supported_ids.begin(), supported_ids.end(), alarm.bd) == supported_ids.end()) {
                LogManager::getInstance().Debug("Skipping alarm for unsupported building: " + std::to_string(alarm.bd));
                continue;
            }
        }
        grouped_alarms[alarm.bd].push_back(alarm);
    }
    return grouped_alarms;
}

void CSPGateway::setSupportedBuildingIds(const std::vector<int>& building_ids) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.supported_building_ids = building_ids;
    LogManager::getInstance().Info("Updated supported building IDs: " + std::to_string(building_ids.size()) + " buildings");
}

void CSPGateway::setMultiBuildingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.multi_building_enabled = enabled;
    LogManager::getInstance().Info("Multi-building mode: " + std::string(enabled ? "enabled" : "disabled"));
}

std::vector<AlarmMessage> CSPGateway::filterIgnoredAlarms(const std::vector<AlarmMessage>& alarms) {
    if (config_.alarm_ignore_minutes <= 0) {
        return alarms;
    }
    std::vector<AlarmMessage> filtered_alarms;
    size_t ignored_count = 0;
    for (const auto& alarm : alarms) {
        if (!shouldIgnoreAlarm(alarm.tm)) {
            filtered_alarms.push_back(alarm);
        } else {
            ignored_count++;
            LogManager::getInstance().Debug("Ignoring alarm due to time filter: " + alarm.nm + " at " + alarm.tm);
        }
    }
    LogManager::getInstance().Debug("Filtered " + std::to_string(ignored_count) + " alarms out of " + std::to_string(alarms.size()));
    return filtered_alarms;
}

bool CSPGateway::shouldIgnoreAlarm(const std::string& alarm_time) const {
    try {
        auto alarm_tp = parseCSTimeString(alarm_time);
        auto now = std::chrono::system_clock::now();
        auto ignore_duration = std::chrono::minutes(config_.alarm_ignore_minutes);
        auto ignore_time = now - ignore_duration;
        return alarm_tp < ignore_time;
    } catch (const std::exception& e) {
        LogManager::getInstance().Debug("Failed to parse alarm time, not ignoring: " + std::string(e.what()));
        return false;
    }
}

void CSPGateway::setAlarmIgnoreMinutes(int minutes) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.alarm_ignore_minutes = minutes;
    LogManager::getInstance().Info("Alarm ignore minutes set to: " + std::to_string(minutes));
}

void CSPGateway::setUseLocalTime(bool use_local) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.use_local_time = use_local;
    LogManager::getInstance().Info("Use local time: " + std::string(use_local ? "enabled" : "disabled"));
}

std::string CSPGateway::saveBatchAlarmFile(int building_id, const std::vector<AlarmMessage>& alarms) {
    try {
        std::string batch_dir = createBatchDirectory(building_id);
        std::filesystem::create_directories(batch_dir);
        std::string filename = generateBatchFileName();
        std::string full_path = batch_dir + "/" + filename;
        nlohmann::json alarm_array = nlohmann::json::array();
        for (const auto& alarm : alarms) {
            alarm_array.push_back(alarm.to_json());
        }
        std::ofstream file(full_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + full_path);
        }
        file << alarm_array.dump(2);
        file.close();
        LogManager::getInstance().Debug("Batch alarm file saved: " + full_path + " (" + std::to_string(alarms.size()) + " alarms)");
        return full_path;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to save batch alarm file: " + std::string(e.what()));
        return "";
    }
}

bool CSPGateway::cleanupSuccessfulBatchFile(const std::string& file_path) {
    try {
        if (file_path.empty() || !std::filesystem::exists(file_path)) {
            return false;
        }
        std::filesystem::remove(file_path);
        LogManager::getInstance().Debug("Cleaned up successful batch file: " + file_path);
        return true;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to cleanup batch file: " + std::string(e.what()));
        return false;
    }
}

size_t CSPGateway::cleanupOldFailedFiles(int days_to_keep) {
    if (days_to_keep < 0) {
        days_to_keep = config_.keep_failed_files_days;
    }
    size_t deleted_count = 0;
    try {
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days_to_keep);
        std::vector<std::string> dirs_to_clean = {config_.failed_file_path, config_.alarm_dir_path};
        for (const auto& dir : dirs_to_clean) {
            if (!std::filesystem::exists(dir)) continue;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    auto file_time = std::filesystem::last_write_time(entry);
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                    if (sctp < cutoff_time) {
                        std::filesystem::remove(entry);
                        deleted_count++;
                        LogManager::getInstance().Debug("Cleaned up old file: " + entry.path().string());
                    }
                }
            }
        }
        LogManager::getInstance().Info("Cleaned up " + std::to_string(deleted_count) + " old files");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to cleanup old files: " + std::string(e.what()));
    }
    return deleted_count;
}

void CSPGateway::setAlarmDirectoryPath(const std::string& dir_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.alarm_dir_path = dir_path;
    try {
        std::filesystem::create_directories(dir_path);
        LogManager::getInstance().Info("Alarm directory path set to: " + dir_path);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to create alarm directory: " + std::string(e.what()));
    }
}

void CSPGateway::setAutoCleanupEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.auto_cleanup_success_files = enabled;
    LogManager::getInstance().Info("Auto cleanup: " + std::string(enabled ? "enabled" : "disabled"));
}

void CSPGateway::updateConfig(const CSPGatewayConfig& new_config) {
    LogManager::getInstance().Info("Updating CSPGateway configuration...");
    if (dynamic_target_manager_) {
        LogManager::getInstance().Info("Shutting down existing Dynamic Target System...");
        dynamic_target_manager_->stop();
        dynamic_target_manager_.reset();
    }
    config_ = new_config;
    initializeHttpClient();
    initializeS3Client();
    initializeDynamicTargetSystem();
    LogManager::getInstance().Info("Configuration updated successfully");
}

CSPGatewayStats CSPGateway::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    CSPGatewayStats stats = stats_;
    if (use_dynamic_targets_ && dynamic_target_manager_) {
        try {
            auto system_metrics = dynamic_target_manager_->getSystemMetrics();
            stats.dynamic_targets_enabled = true;
            stats.total_dynamic_targets = system_metrics.total_targets;
            stats.active_dynamic_targets = system_metrics.active_targets;
            stats.healthy_dynamic_targets = system_metrics.healthy_targets;
            stats.dynamic_target_success_rate = system_metrics.overall_success_rate;
        } catch (const std::exception& e) {
            LogManager::getInstance().Debug("Failed to get dynamic target metrics: " + std::string(e.what()));
        }
    } else {
        stats.dynamic_targets_enabled = false;
        stats.total_dynamic_targets = 0;
        stats.active_dynamic_targets = 0;
        stats.healthy_dynamic_targets = 0;
        stats.dynamic_target_success_rate = 0.0;
    }
    return stats;
}

void CSPGateway::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_alarms.store(0);
    stats_.successful_api_calls.store(0);
    stats_.failed_api_calls.store(0);
    stats_.successful_s3_uploads.store(0);
    stats_.failed_s3_uploads.store(0);
    stats_.retry_attempts.store(0);
    stats_.total_batches.store(0);
    stats_.successful_batches.store(0);
    stats_.ignored_alarms.store(0);
    stats_.avg_response_time_ms = 0.0;
    LogManager::getInstance().Info("CSPGateway statistics reset");
}

bool CSPGateway::testConnection() {
    LogManager::getInstance().Info("Testing all connections...");
    bool overall_ok = true;
    bool legacy_ok = testConnectionLegacy();
    overall_ok &= legacy_ok;
    if (use_dynamic_targets_ && dynamic_target_manager_) {
        try {
            auto detailed_stats = dynamic_target_manager_->getDetailedStatistics();
            bool dynamic_ok = detailed_stats.contains("healthy_targets") && detailed_stats["healthy_targets"].get<int>() > 0;
            overall_ok &= dynamic_ok;
            LogManager::getInstance().Info("Dynamic Target System connection test: " + std::string(dynamic_ok ? "OK" : "FAILED"));
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Dynamic targets connection test failed: " + std::string(e.what()));
            overall_ok = false;
        }
    }
    LogManager::getInstance().Info("Overall connection test: " + std::string(overall_ok ? "OK" : "FAILED"));
    return overall_ok;
}

bool CSPGateway::testConnectionLegacy() {
    bool api_ok = false;
    bool s3_ok = false;
    if (!config_.api_endpoint.empty() && http_client_) {
        try {
            auto test_result = sendHttpPostRequest(config_.api_endpoint + "/health", "{}", "application/json", {});
            api_ok = (test_result.status_code == 200);
        } catch (const std::exception&) {
            api_ok = false;
        }
        LogManager::getInstance().Info("Legacy API connection test: " + std::string(api_ok ? "OK" : "FAILED"));
    }
    if (!config_.s3_bucket_name.empty() && s3_client_) {
        try {
            s3_ok = uploadToS3("test/connection_test.txt", "test");
        } catch (const std::exception&) {
            s3_ok = false;
        }
        LogManager::getInstance().Info("Legacy S3 connection test: " + std::string(s3_ok ? "OK" : "FAILED"));
    }
    return api_ok || s3_ok;
}

bool CSPGateway::testS3Connection() {
    if (!s3_client_) {
        return false;
    }
    try {
        return uploadToS3("test/s3_connection_test.txt", "test");
    } catch (const std::exception&) {
        return false;
    }
}

AlarmSendResult CSPGateway::sendTestAlarm() {
    int building_id_int = 1001;
    try {
        building_id_int = std::stoi(config_.building_id);
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("Failed to convert building_id to int, using default 1001: " + std::string(e.what()));
    }
    auto test_alarm = AlarmMessage::create_sample(building_id_int, "TEST_POINT", 99.9, true);
    test_alarm.des = "CSPGateway 연결 테스트 알람";
    LogManager::getInstance().Info("Sending test alarm");
    return taskAlarmSingle(test_alarm);
}

std::unordered_map<int, BatchAlarmResult> CSPGateway::testMultiBuildingAlarms() {
    LogManager::getInstance().Info("Testing multi-building alarm processing");
    std::vector<AlarmMessage> test_alarms;
    for (int building_id : config_.supported_building_ids) {
        auto alarm = AlarmMessage::create_sample(building_id, "TEST_MULTI_" + std::to_string(building_id), 50.0, true);
        alarm.des = "Multi-building test alarm for building " + std::to_string(building_id);
        test_alarms.push_back(alarm);
    }
    auto grouped_alarms = groupAlarmsByBuilding(test_alarms);
    return processMultiBuildingAlarms(grouped_alarms);
}

std::vector<std::string> CSPGateway::getSupportedTargetTypes() const {
    if (!use_dynamic_targets_ || !dynamic_target_manager_) {
        return {"HTTP", "S3"};
    }
    return dynamic_target_manager_->getSupportedHandlerTypes();
}

std::vector<DynamicTargetStats> CSPGateway::getDynamicTargetStats() const {
    if (!dynamic_target_manager_) {
        return {};
    }
    try {
        auto stats_json = dynamic_target_manager_->getDetailedStatistics();
        std::vector<DynamicTargetStats> stats_list;
        if (stats_json.contains("targets") && stats_json["targets"].is_array()) {
            for (const auto& target_json : stats_json["targets"]) {
                DynamicTargetStats stats;
                if (target_json.contains("name")) stats.name = target_json["name"];
                if (target_json.contains("type")) stats.type = target_json["type"];
                if (target_json.contains("success_count")) stats.success_count = target_json["success_count"];
                if (target_json.contains("failure_count")) stats.failure_count = target_json["failure_count"];
                if (target_json.contains("enabled")) stats.enabled = target_json["enabled"];
                if (target_json.contains("healthy")) stats.healthy = target_json["healthy"];
                if (target_json.contains("last_error")) stats.last_error_message = target_json["last_error"];
                stats.success_rate = stats.calculateSuccessRate();
                stats_list.push_back(stats);
            }
        }
        return stats_list;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to get dynamic target stats: " + std::string(e.what()));
        return {};
    }
}

void CSPGateway::updateStatsFromDynamicResults(const std::vector<TargetSendResult>& target_results, double response_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    for (const auto& target_result : target_results) {
        if (target_result.target_type == "HTTP" || target_result.target_type == "HTTPS") {
            if (target_result.success) {
                stats_.successful_api_calls++;
                stats_.last_success_time = std::chrono::system_clock::now();
            } else {
                stats_.failed_api_calls++;
                stats_.last_failure_time = std::chrono::system_clock::now();
            }
        } else if (target_result.target_type == "S3") {
            if (target_result.success) {
                stats_.successful_s3_uploads++;
            } else {
                stats_.failed_s3_uploads++;
            }
        }
    }
    stats_.avg_response_time_ms = (stats_.avg_response_time_ms * 0.9) + (response_time_ms * 0.1);
}

void CSPGateway::updateBatchStats(const BatchAlarmResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_batches++;
    if (result.isCompleteSuccess()) {
        stats_.successful_batches++;
    }
}

void CSPGateway::initializeHttpClient() {
    try {
        if (config_.api_endpoint.empty()) {
            http_client_.reset();
            return;
        }
        PulseOne::Client::HttpRequestOptions options;
        options.timeout_sec = config_.api_timeout_sec;
        options.user_agent = "PulseOne-CSPGateway/1.0";
        if (!config_.api_key.empty()) {
            options.bearer_token = config_.api_key;
        }
        http_client_ = std::make_unique<PulseOne::Client::HttpClient>(config_.api_endpoint, options);
        LogManager::getInstance().Debug("HTTP client initialized");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to initialize HTTP client: " + std::string(e.what()));
        http_client_.reset();
    }
}

void CSPGateway::initializeS3Client() {
    try {
        if (config_.s3_bucket_name.empty()) {
            s3_client_.reset();
            return;
        }
        PulseOne::Client::S3Config s3_config;
        s3_config.endpoint = config_.s3_endpoint;
        s3_config.access_key = config_.s3_access_key;
        s3_config.secret_key = config_.s3_secret_key;
        s3_config.bucket_name = config_.s3_bucket_name;
        s3_config.region = config_.s3_region;
        s3_config.upload_timeout_sec = config_.api_timeout_sec;
        s3_client_ = std::make_unique<PulseOne::Client::S3Client>(s3_config);
        LogManager::getInstance().Debug("S3 client initialized");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Failed to initialize S3 client: " + std::string(e.what()));
        s3_client_.reset();
    }
}

void CSPGateway::workerThread() {
    LogManager::getInstance().Info("CSPGateway worker thread started");
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !alarm_queue_.empty() || should_stop_.load(); });
        if (should_stop_.load()) {
            break;
        }
        if (!alarm_queue_.empty()) {
            AlarmMessage alarm = alarm_queue_.front();
            alarm_queue_.pop();
            lock.unlock();
            taskAlarmSingle(alarm);
        }
    }
    LogManager::getInstance().Info("CSPGateway worker thread stopped");
}

void CSPGateway::retryThread() {
    LogManager::getInstance().Info("CSPGateway retry thread started");
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait_for(lock, std::chrono::seconds(30), [this] { return !retry_queue_.empty() || should_stop_.load(); });
        if (should_stop_.load()) {
            break;
        }
        if (!retry_queue_.empty()) {
            auto retry_item = retry_queue_.front();
            retry_queue_.pop();
            lock.unlock();
            retryFailedAlarm(retry_item.first, retry_item.second);
        }
    }
    LogManager::getInstance().Info("CSPGateway retry thread stopped");
}

AlarmSendResult CSPGateway::retryFailedAlarm(const AlarmMessage& alarm_message, int attempt_count) {
    LogManager::getInstance().Debug("Retrying failed alarm (attempt " + std::to_string(attempt_count) + "): " + alarm_message.nm);
    int delay_ms = config_.initial_delay_ms * (1 << (attempt_count - 1));
    delay_ms = std::min(delay_ms, 60000);
    if (delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.retry_attempts++;
    }
    return taskAlarmSingle(alarm_message);
}

bool CSPGateway::saveFailedAlarmToFile(const AlarmMessage& alarm_message, const std::string& error_message) {
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream filename;
        filename << config_.failed_file_path << "/failed_alarm_" << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") << "_" << alarm_message.bd << "_" << alarm_message.nm << ".json";
        auto alarm_json = alarm_message.to_json();
        alarm_json["error_message"] = error_message;
        alarm_json["failed_timestamp"] = AlarmMessage::current_time_to_csharp_format(true);
        alarm_json["gateway_info"] = {{"building_id", config_.building_id}, {"api_endpoint", config_.api_endpoint}, {"s3_bucket", config_.s3_bucket_name}};
        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << alarm_json.dump(2);
            file.close();
            LogManager::getInstance().Debug("Failed alarm saved to: " + filename.str());
            return true;
        } else {
            LogManager::getInstance().Error("Failed to open file for writing: " + filename.str());
            return false;
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception saving failed alarm to file: " + std::string(e.what()));
        return false;
    }
}

size_t CSPGateway::reprocessFailedAlarms() {
    LogManager::getInstance().Info("reprocessFailedAlarms - Not fully implemented yet");
    return 0;
}

AlarmSendResult CSPGateway::sendHttpPostRequest(const std::string& endpoint, const std::string& json_data, const std::string& content_type, const std::unordered_map<std::string, std::string>& headers) {
    AlarmSendResult result;
    if (!http_client_) {
        result.success = false;
        result.error_message = "HTTP client not initialized";
        return result;
    }
    try {
        auto response = http_client_->post(endpoint, json_data, content_type, headers);
        result.success = (response.status_code >= 200 && response.status_code < 300);
        result.status_code = response.status_code;
        result.response_body = response.body;
        if (!result.success) {
            result.error_message = "HTTP " + std::to_string(response.status_code) + ": " + response.body;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "HTTP request exception: " + std::string(e.what());
        result.status_code = 0;
    }
    return result;
}

bool CSPGateway::uploadToS3(const std::string& object_key, const std::string& content) {
    if (!s3_client_) {
        LogManager::getInstance().Error("S3 client not initialized");
        return false;
    }
    try {
        auto result = s3_client_->upload(object_key, content, "application/json; charset=utf-8");
        return result.success;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("S3 upload exception: " + std::string(e.what()));
        return false;
    }
}

std::string CSPGateway::generateBatchFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d%H%M%S") << ".json";
    return ss.str();
}

std::string CSPGateway::createBatchDirectory(int building_id) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << config_.alarm_dir_path << "/" << building_id << "/" << std::put_time(std::localtime(&time_t), "%Y%m%d");
    return ss.str();
}

std::chrono::system_clock::time_point CSPGateway::parseCSTimeString(const std::string& time_str) const {
    std::regex time_regex(R"((\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})\.(\d{3}))");
    std::smatch matches;
    if (!std::regex_match(time_str, matches, time_regex)) {
        throw std::invalid_argument("Invalid time format: " + time_str);
    }
    std::tm tm = {};
    tm.tm_year = std::stoi(matches[1]) - 1900;
    tm.tm_mon = std::stoi(matches[2]) - 1;
    tm.tm_mday = std::stoi(matches[3]);
    tm.tm_hour = std::stoi(matches[4]);
    tm.tm_min = std::stoi(matches[5]);
    tm.tm_sec = std::stoi(matches[6]);
    auto time_t = std::mktime(&tm);
    auto tp = std::chrono::system_clock::from_time_t(time_t);
    int milliseconds = std::stoi(matches[7]);
    tp += std::chrono::milliseconds(milliseconds);
    return tp;
}

void CSPGateway::loadConfigFromConfigManager() {
    ConfigManager& config_mgr = ConfigManager::getInstance();
    try {
        if (config_.api_key.empty()) {
            std::string api_key_file = config_mgr.getOrDefault("CSP_API_KEY_FILE", config_mgr.getSecretsDirectory() + "/csp_api.key");
            if (!api_key_file.empty()) {
                config_.api_key = config_mgr.loadPasswordFromFile("CSP_API_KEY_FILE");
            }
        }
        if (config_.s3_access_key.empty()) {
            config_.s3_access_key = config_mgr.loadPasswordFromFile("CSP_S3_ACCESS_KEY_FILE");
        }
        if (config_.s3_secret_key.empty()) {
            config_.s3_secret_key = config_mgr.loadPasswordFromFile("CSP_S3_SECRET_KEY_FILE");
        }
        if (config_.building_id.empty()) {
            config_.building_id = config_mgr.getOrDefault("CSP_BUILDING_ID", "1001");
        }
        if (config_.api_endpoint.empty()) {
            config_.api_endpoint = config_mgr.getOrDefault("CSP_API_ENDPOINT", "");
        }
        if (config_.s3_bucket_name.empty()) {
            config_.s3_bucket_name = config_mgr.getOrDefault("CSP_S3_BUCKET_NAME", "");
        }
        if (config_.dynamic_targets_config_path.empty()) {
            config_.dynamic_targets_config_path = config_mgr.getOrDefault("CSP_DYNAMIC_TARGETS_CONFIG", "./config/csp-targets.json");
        }
        LogManager::getInstance().Debug("CSPGateway configuration loaded from ConfigManager");
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("Failed to load config from ConfigManager: " + std::string(e.what()));
    }
}

json DynamicTargetStats::toJson() const {
    auto timeToString = [](const std::chrono::system_clock::time_point& tp) -> std::string {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    };
    return json{
        {"name", name}, {"type", type}, {"enabled", enabled}, {"healthy", healthy},
        {"success_count", success_count}, {"failure_count", failure_count},
        {"success_rate", calculateSuccessRate()}, {"avg_response_time_ms", avg_response_time_ms},
        {"last_success_time", timeToString(last_success_time)},
        {"last_failure_time", timeToString(last_failure_time)},
        {"last_error_message", last_error_message},
        {"circuit_breaker_state", circuit_breaker_state},
        {"consecutive_failures", consecutive_failures},
        {"priority", priority}, {"config", config}
    };
}

} // namespace CSP
} // namespace PulseOne