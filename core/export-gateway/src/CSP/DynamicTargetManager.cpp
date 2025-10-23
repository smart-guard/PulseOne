/**
 * @file DynamicTargetManager.cpp (싱글턴 완전 버전)
 * @brief 동적 타겟 관리자 - 싱글턴 + 원본 기능 모두 포함
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 6.0.0
 */

#include "CSP/DynamicTargetManager.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/S3TargetHandler.h"
#include "CSP/FileTargetHandler.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/PayloadTemplateEntity.h"
#include "Database/Entities/ExportLogEntity.h"
#include <fstream>
#include <thread>
#include <algorithm>
#include <future>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <regex>

namespace PulseOne {
namespace CSP {
using PulseOne::Database::Repositories::ExportLogRepository;
using PulseOne::Database::Entities::ExportLogEntity;

// =============================================================================
// 싱글턴 구현
// =============================================================================

DynamicTargetManager& DynamicTargetManager::getInstance() {
    static DynamicTargetManager instance;
    return instance;
}

// =============================================================================
// 생성자 및 소멸자 (private)
// =============================================================================

DynamicTargetManager::DynamicTargetManager() {
    LogManager::getInstance().Info("DynamicTargetManager 싱글턴 생성");
    
    // 기본 핸들러 등록
    registerDefaultHandlers();
    
    // 글로벌 설정 초기화
    global_settings_ = json{
        {"health_check_interval_sec", 60},
        {"metrics_collection_interval_sec", 30},
        {"max_concurrent_requests", 100},
        {"request_timeout_ms", 10000},
        {"retry_attempts", 3},
        {"retry_delay_ms", 1000},
        {"rate_limit_requests_per_minute", 1000}
    };
    
    LogManager::getInstance().Info("DynamicTargetManager 초기화 완료");
}

DynamicTargetManager::~DynamicTargetManager() {
    try {
        stop();
        handlers_.clear();
        {
            std::unique_lock<std::shared_mutex> lock(targets_mutex_);
            targets_.clear();
        }
        failure_protectors_.clear();
        LogManager::getInstance().Info("DynamicTargetManager 소멸 완료");
    } catch (const std::exception& e) {
        try {
            LogManager::getInstance().Error("DynamicTargetManager 소멸 중 예외: " + std::string(e.what()));
        } catch (...) {}
    } catch (...) {}
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool DynamicTargetManager::start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("DynamicTargetManager가 이미 실행 중입니다");
        return false;
    }
    
    LogManager::getInstance().Info("DynamicTargetManager 시작...");
    
    try {
        is_running_ = true;
        
        if (!loadFromDatabase()) {
            LogManager::getInstance().Error("DB에서 타겟 로드 실패");
            is_running_ = false;
            return false;
        }
        
        initializeFailureProtectors();
        startBackgroundThreads();
        
        LogManager::getInstance().Info("DynamicTargetManager 시작 완료 ✅");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DynamicTargetManager 시작 실패: " + std::string(e.what()));
        is_running_ = false;
        return false;
    }
}

void DynamicTargetManager::stop() {
    if (!is_running_.load()) return;
    
    try {
        LogManager::getInstance().Info("DynamicTargetManager 중지...");
        should_stop_ = true;
        is_running_ = false;
        stopBackgroundThreads();
        LogManager::getInstance().Info("DynamicTargetManager 중지 완료");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DynamicTargetManager 중지 중 예외: " + std::string(e.what()));
    }
}

// =============================================================================
// DB 기반 설정 관리
// =============================================================================

bool DynamicTargetManager::loadFromDatabase() {
    using namespace PulseOne::Database::Repositories;
    using namespace PulseOne::Database::Entities;
    
    try {
        LogManager::getInstance().Info("DB에서 타겟 로드 시작...");
        
        ExportTargetRepository target_repo;
        auto entities = target_repo.findByEnabled(true);
        
        if (entities.empty()) {
            LogManager::getInstance().Warn("활성화된 타겟이 없습니다");
            return true;
        }
        
        std::unique_lock<std::shared_mutex> lock(targets_mutex_);
        targets_.clear();
        
        int loaded_count = 0;
        for (const auto& entity : entities) {
            try {
                DynamicTarget target;
                target.name = entity.getName();
                target.type = entity.getTargetType();
                target.enabled = entity.isEnabled();
                target.priority = 100;
                target.description = entity.getDescription();
                
                try {
                    target.config = json::parse(entity.getConfig());
                } catch (const std::exception& e) {
                    LogManager::getInstance().Error("Config JSON 파싱 실패: " + 
                        entity.getName() + " - " + std::string(e.what()));
                    continue;
                }
                
                if (entity.getTemplateId() > 0) {
                    target.config["template_id"] = entity.getTemplateId();
                }
                
                targets_.push_back(target);
                loaded_count++;
                
                LogManager::getInstance().Debug("타겟 로드: " + target.name + " (" + target.type + ")");
                
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("타겟 엔티티 처리 실패: " + std::string(e.what()));
                continue;
            }
        }
        
        LogManager::getInstance().Info("DB에서 " + std::to_string(loaded_count) + "개 타겟 로드 완료");
        return (loaded_count > 0);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DB 로드 실패: " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::forceReload() {
    LogManager::getInstance().Info("타겟 강제 리로드...");
    return loadFromDatabase();
}

std::optional<DynamicTarget> DynamicTargetManager::getTargetWithTemplate(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(name);
    if (it == targets_.end()) {
        return std::nullopt;
    }
    
    try {
        using namespace PulseOne::Database::Repositories;
        
        ExportTargetRepository target_repo;
        auto target_entity = target_repo.findByName(name);
        
        if (target_entity.has_value() && target_entity->getTemplateId() > 0) {
            PayloadTemplateRepository template_repo;
            auto template_entity = template_repo.findById(target_entity->getTemplateId());
            
            if (template_entity.has_value()) {
                DynamicTarget target = *it;
                target.config["template_json"] = template_entity->getTemplateJson();
                target.config["field_mappings"] = template_entity->getFieldMappings();
                
                LogManager::getInstance().Debug("템플릿 포함 타겟 반환: " + name);
                return target;
            }
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("템플릿 로드 실패: " + std::string(e.what()));
    }
    
    return *it;
}

// =============================================================================
// 알람 전송 (핵심 기능)
// =============================================================================

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmToAllTargets(const AlarmMessage& alarm) {
    std::vector<TargetSendResult> results;
    
    concurrent_requests_.fetch_add(1);
    auto current = concurrent_requests_.load();
    if (current > peak_concurrent_requests_.load()) {
        peak_concurrent_requests_.store(current);
    }
    
    try {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        if (targets_.empty()) {
            LogManager::getInstance().Warn("sendAlarmToAllTargets: 사용 가능한 타겟이 없음");
            concurrent_requests_.fetch_sub(1);
            return results;
        }
        
        std::vector<size_t> active_indices;
        for (size_t i = 0; i < targets_.size(); ++i) {
            if (targets_[i].enabled && targets_[i].healthy.load()) {
                active_indices.push_back(i);
            }
        }
        
        if (active_indices.empty()) {
            LogManager::getInstance().Warn("sendAlarmToAllTargets: 활성화된 타겟이 없음");
            concurrent_requests_.fetch_sub(1);
            return results;
        }
        
        std::sort(active_indices.begin(), active_indices.end(),
                  [this](size_t a, size_t b) {
                      return targets_[a].priority < targets_[b].priority;
                  });
        
        LogManager::getInstance().Info("알람을 " + std::to_string(active_indices.size()) + "개 타겟에 순차 전송 시작");
        
        for (size_t idx : active_indices) {
            TargetSendResult result;
            if (processTargetByIndex(idx, alarm, result)) {
                results.push_back(result);
            }
        }
        
        LogManager::getInstance().Info("알람 순차 전송 완료 - 처리된 타겟 수: " + std::to_string(results.size()));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("sendAlarmToAllTargets 예외: " + std::string(e.what()));
    }
    
    concurrent_requests_.fetch_sub(1);
    return results;
}

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmToAllTargetsParallel(const AlarmMessage& alarm) {
    std::vector<TargetSendResult> results;
    
    concurrent_requests_.fetch_add(1);
    auto current = concurrent_requests_.load();
    if (current > peak_concurrent_requests_.load()) {
        peak_concurrent_requests_.store(current);
    }
    
    try {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        if (targets_.empty()) {
            LogManager::getInstance().Warn("sendAlarmToAllTargetsParallel: 사용 가능한 타겟이 없음");
            concurrent_requests_.fetch_sub(1);
            return results;
        }
        
        std::vector<size_t> active_indices;
        for (size_t i = 0; i < targets_.size(); ++i) {
            if (targets_[i].enabled && targets_[i].healthy.load()) {
                active_indices.push_back(i);
            }
        }
        
        if (active_indices.empty()) {
            LogManager::getInstance().Warn("sendAlarmToAllTargetsParallel: 활성화된 타겟이 없음");
            concurrent_requests_.fetch_sub(1);
            return results;
        }
        
        LogManager::getInstance().Info("알람을 " + std::to_string(active_indices.size()) + "개 타겟에 병렬 전송 시작");
        
        std::vector<std::future<TargetSendResult>> futures;
        for (size_t idx : active_indices) {
            futures.push_back(std::async(std::launch::async, [this, idx, &alarm]() -> TargetSendResult {
                TargetSendResult result;
                processTargetByIndex(idx, alarm, result);
                return result;
            }));
        }
        
        for (auto& future : futures) {
            try {
                TargetSendResult result = future.get();
                results.push_back(result);
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("병렬 전송 결과 수집 실패: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("알람 병렬 전송 완료 - 처리된 타겟 수: " + std::to_string(results.size()));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("sendAlarmToAllTargetsParallel 예외: " + std::string(e.what()));
    }
    
    concurrent_requests_.fetch_sub(1);
    return results;
}

TargetSendResult DynamicTargetManager::sendAlarmToTarget(const AlarmMessage& alarm, const std::string& target_name) {
    TargetSendResult result;
    result.target_name = target_name;
    result.success = false;
    
    try {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        auto it = findTarget(target_name);
        if (it == targets_.end()) {
            result.error_message = "타겟을 찾을 수 없음: " + target_name;
            LogManager::getInstance().Error(result.error_message);
            return result;
        }
        
        if (!it->enabled) {
            result.error_message = "타겟이 비활성화됨: " + target_name;
            LogManager::getInstance().Warn(result.error_message);
            return result;
        }
        
        size_t index = std::distance(targets_.begin(), it);
        processTargetByIndex(index, alarm, result);
        
    } catch (const std::exception& e) {
        result.error_message = "타겟 전송 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
    }
    
    return result;
}

std::future<std::vector<TargetSendResult>> DynamicTargetManager::sendAlarmAsync(const AlarmMessage& alarm) {
    return std::async(std::launch::async, [this, alarm]() -> std::vector<TargetSendResult> {
        return sendAlarmToAllTargetsParallel(alarm);
    });
}

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmByPriority(const AlarmMessage& alarm, int max_priority) {
    std::vector<TargetSendResult> results;
    
    try {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        std::vector<size_t> filtered_indices;
        for (size_t i = 0; i < targets_.size(); ++i) {
            if (targets_[i].enabled && 
                targets_[i].healthy.load() && 
                targets_[i].priority <= max_priority) {
                filtered_indices.push_back(i);
            }
        }
        
        if (filtered_indices.empty()) {
            LogManager::getInstance().Warn("sendAlarmByPriority: 조건에 맞는 타겟이 없음 (max_priority: " + std::to_string(max_priority) + ")");
            return results;
        }
        
        std::sort(filtered_indices.begin(), filtered_indices.end(),
                  [this](size_t a, size_t b) {
                      return targets_[a].priority < targets_[b].priority;
                  });
        
        for (size_t idx : filtered_indices) {
            TargetSendResult result;
            if (processTargetByIndex(idx, alarm, result)) {
                results.push_back(result);
            }
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("sendAlarmByPriority 예외: " + std::string(e.what()));
    }
    
    return results;
}

BatchProcessingResult DynamicTargetManager::processBuildingAlarms(
    const std::unordered_map<int, std::vector<AlarmMessage>>& building_alarms) {
    
    BatchProcessingResult batch_result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        LogManager::getInstance().Info("빌딩별 알람 배치 처리 시작 - 빌딩 수: " + std::to_string(building_alarms.size()));
        
        for (const auto& [building_id, alarms] : building_alarms) {
            for (const auto& alarm : alarms) {
                auto target_results = sendAlarmToAllTargets(alarm);
                
                for (const auto& result : target_results) {
                    batch_result.results.push_back(result);
                    if (result.success) {
                        batch_result.successful_count++;
                    } else {
                        batch_result.failed_count++;
                    }
                }
                
                batch_result.total_processed++;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        batch_result.total_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("processBuildingAlarms 예외: " + std::string(e.what()));
    }
    
    return batch_result;
}

// =============================================================================
// 타겟 관리
// =============================================================================

bool DynamicTargetManager::addTarget(const DynamicTarget& target) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    try {
        auto it = findTarget(target.name);
        if (it != targets_.end()) {
            LogManager::getInstance().Error("중복된 타겟 이름: " + target.name);
            return false;
        }
        
        if (handlers_.find(target.type) == handlers_.end()) {
            LogManager::getInstance().Error("지원되지 않는 타겟 타입: " + target.type);
            return false;
        }
        
        targets_.emplace_back();
        auto& new_target = targets_.back();
        
        new_target.name = target.name;
        new_target.type = target.type;
        new_target.enabled = target.enabled;
        new_target.priority = target.priority;
        new_target.description = target.description;
        new_target.config = target.config;
        
        initializeFailureProtectorForTarget(new_target.name);
        
        LogManager::getInstance().Info("타겟 추가됨: " + target.name + " (타입: " + target.type + ")");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("타겟 추가 실패: " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::removeTarget(const std::string& target_name) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        failure_protectors_.erase(target_name);
        targets_.erase(it);
        LogManager::getInstance().Info("타겟 제거됨: " + target_name);
        return true;
    }
    
    LogManager::getInstance().Warn("제거할 타겟을 찾을 수 없음: " + target_name);
    return false;
}

std::unordered_map<std::string, bool> DynamicTargetManager::testAllConnections() {
    std::unordered_map<std::string, bool> results;
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    for (const auto& target : targets_) {
        results[target.name] = testTargetConnection(target.name);
    }
    
    return results;
}

bool DynamicTargetManager::testTargetConnection(const std::string& target_name) {
    try {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        auto it = findTarget(target_name);
        if (it == targets_.end()) {
            LogManager::getInstance().Error("테스트할 타겟을 찾을 수 없음: " + target_name);
            return false;
        }
        
        auto handler_it = handlers_.find(it->type);
        if (handler_it == handlers_.end()) {
            LogManager::getInstance().Error("타겟 타입에 대한 핸들러를 찾을 수 없음: " + it->type);
            return false;
        }
        
        bool test_result = handler_it->second->testConnection(it->config);
        LogManager::getInstance().Info("타겟 연결 테스트 " + target_name + ": " + (test_result ? "성공" : "실패"));
        return test_result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("타겟 연결 테스트 예외: " + target_name + " - " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::enableTarget(const std::string& target_name, bool enabled) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->enabled = enabled;
        return true;
    }
    return false;
}

bool DynamicTargetManager::changeTargetPriority(const std::string& target_name, int new_priority) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->priority = new_priority;
        return true;
    }
    return false;
}

bool DynamicTargetManager::updateTargetConfig(const std::string& target_name, const json& new_config) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->config = new_config;
        return true;
    }
    return false;
}

std::vector<std::string> DynamicTargetManager::getTargetNames(bool include_disabled) const {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    
    std::vector<std::string> names;
    for (const auto& target : targets_) {
        if (include_disabled || target.enabled) {
            names.push_back(target.name);
        }
    }
    return names;
}

// =============================================================================
// 실패 방지기 관리
// =============================================================================

void DynamicTargetManager::resetFailureProtector(const std::string& target_name) {
    auto it = failure_protectors_.find(target_name);
    if (it != failure_protectors_.end()) {
        it->second->reset();
    }
}

void DynamicTargetManager::resetAllFailureProtectors() {
    for (auto& [target_name, protector] : failure_protectors_) {
        protector->reset();
    }
}

void DynamicTargetManager::forceOpenFailureProtector(const std::string& target_name) {
    auto it = failure_protectors_.find(target_name);
    if (it != failure_protectors_.end()) {
        LogManager::getInstance().Info("실패 방지기 강제 열림 요청: " + target_name);
    }
}

std::unordered_map<std::string, FailureProtectorStats> DynamicTargetManager::getFailureProtectorStats() const {
    std::unordered_map<std::string, FailureProtectorStats> stats;
    for (const auto& [target_name, protector] : failure_protectors_) {
        stats[target_name] = protector->getStats();
    }
    return stats;
}

// =============================================================================
// 통계 및 모니터링
// =============================================================================

std::vector<DynamicTarget> DynamicTargetManager::getTargetStatistics() const {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    return targets_;
}

// =============================================================================
// 핸들러 관리
// =============================================================================

bool DynamicTargetManager::registerHandler(const std::string& type_name, std::unique_ptr<ITargetHandler> handler) {
    if (!handler) return false;
    handlers_[type_name] = std::move(handler);
    return true;
}

bool DynamicTargetManager::unregisterHandler(const std::string& type_name) {
    return handlers_.erase(type_name) > 0;
}

std::vector<std::string> DynamicTargetManager::getSupportedHandlerTypes() const {
    std::vector<std::string> types;
    for (const auto& [type, _] : handlers_) {
        types.push_back(type);
    }
    return types;
}

// =============================================================================
// 내부 초기화 메서드
// =============================================================================

void DynamicTargetManager::registerDefaultHandlers() {
    handlers_["http"] = std::make_unique<HttpTargetHandler>();
    handlers_["https"] = std::make_unique<HttpTargetHandler>();
    handlers_["s3"] = std::make_unique<S3TargetHandler>();
    handlers_["file"] = std::make_unique<FileTargetHandler>();
    
    LogManager::getInstance().Info("기본 핸들러 등록 완료");
}

void DynamicTargetManager::initializeFailureProtectors() {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    for (const auto& target : targets_) {
        initializeFailureProtectorForTarget(target.name);
    }
}

void DynamicTargetManager::initializeFailureProtectorForTarget(const std::string& target_name) {
    try {
        FailureProtectorConfig fp_config;
        fp_config.failure_threshold = 5;
        fp_config.recovery_timeout_ms = 60000;
        fp_config.half_open_max_attempts = 3;
        
        failure_protectors_[target_name] = std::make_shared<FailureProtector>(target_name, fp_config);
        
        LogManager::getInstance().Debug("실패 방지기 초기화: " + target_name);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("실패 방지기 초기화 실패: " + target_name + " - " + std::string(e.what()));
    }
}

// =============================================================================
// 백그라운드 스레드
// =============================================================================

void DynamicTargetManager::startBackgroundThreads() {
    should_stop_ = false;
    
    health_check_thread_ = std::make_unique<std::thread>(&DynamicTargetManager::healthCheckThread, this);
    metrics_collector_thread_ = std::make_unique<std::thread>(&DynamicTargetManager::metricsCollectorThread, this);
    cleanup_thread_ = std::make_unique<std::thread>(&DynamicTargetManager::cleanupThread, this);
    
    LogManager::getInstance().Info("백그라운드 스레드 시작 완료");
}

void DynamicTargetManager::stopBackgroundThreads() {
    should_stop_ = true;
    
    health_check_cv_.notify_all();
    metrics_collector_cv_.notify_all();
    
    if (health_check_thread_ && health_check_thread_->joinable()) {
        health_check_thread_->join();
    }
    
    if (metrics_collector_thread_ && metrics_collector_thread_->joinable()) {
        metrics_collector_thread_->join();
    }
    
    if (cleanup_thread_ && cleanup_thread_->joinable()) {
        cleanup_thread_->join();
    }
    
    LogManager::getInstance().Info("백그라운드 스레드 종료 완료");
}

void DynamicTargetManager::healthCheckThread() {
    LogManager::getInstance().Info("Health Check Thread 시작");
    
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(cv_mutex_);
        health_check_cv_.wait_for(lock, std::chrono::seconds(60), [this]() {
            return should_stop_.load();
        });
        
        if (should_stop_.load()) break;
        
        try {
            std::shared_lock<std::shared_mutex> target_lock(targets_mutex_);
            for (const auto& target : targets_) {
                if (!target.enabled) continue;
                
                bool is_healthy = (target.success_count.load() > 0) || (target.failure_count.load() < 5);
                
                if (target.healthy.load() != is_healthy) {
                    LogManager::getInstance().Info("타겟 헬스 상태 변경: " + 
                        target.name + " -> " + (is_healthy ? "healthy" : "unhealthy"));
                }
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("헬스 체크 실패: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().Info("Health Check Thread 종료");
}

void DynamicTargetManager::metricsCollectorThread() {
    LogManager::getInstance().Info("Metrics Collector Thread 시작");
    
    while (!should_stop_.load()) {
        std::unique_lock<std::mutex> lock(cv_mutex_);
        metrics_collector_cv_.wait_for(lock, std::chrono::seconds(30), [this]() {
            return should_stop_.load();
        });
        
        if (should_stop_.load()) break;
        
        try {
            std::shared_lock<std::shared_mutex> target_lock(targets_mutex_);
            uint64_t total_requests = 0;
            uint64_t total_success = 0;
            uint64_t total_failure = 0;
            
            for (const auto& target : targets_) {
                total_requests += target.request_count.load();
                total_success += target.success_count.load();
                total_failure += target.failure_count.load();
            }
            
            LogManager::getInstance().Debug("전체 요청: " + std::to_string(total_requests) + 
                ", 성공: " + std::to_string(total_success) + 
                ", 실패: " + std::to_string(total_failure));
                
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("메트릭 수집 실패: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().Info("Metrics Collector Thread 종료");
}

void DynamicTargetManager::cleanupThread() {
    LogManager::getInstance().Info("Cleanup Thread 시작");
    
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(5));
        if (should_stop_.load()) break;
    }
    
    LogManager::getInstance().Info("Cleanup Thread 종료");
}

// =============================================================================
// 유틸리티 메서드
// =============================================================================

std::vector<DynamicTarget>::iterator DynamicTargetManager::findTarget(const std::string& target_name) {
    return std::find_if(targets_.begin(), targets_.end(),
                       [&target_name](const DynamicTarget& target) {
                           return target.name == target_name;
                       });
}

std::vector<DynamicTarget>::const_iterator DynamicTargetManager::findTarget(const std::string& target_name) const {
    return std::find_if(targets_.begin(), targets_.end(),
                       [&target_name](const DynamicTarget& target) {
                           return target.name == target_name;
                       });
}

bool DynamicTargetManager::checkRateLimit() {
    // Rate limit 체크 로직
    return true;
}

void DynamicTargetManager::updateTargetHealth(const std::string& target_name, bool healthy) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->healthy.store(healthy);
    }
}

void DynamicTargetManager::updateTargetStatistics(const std::string& target_name, const TargetSendResult& result) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->request_count.fetch_add(1);
        if (result.success) {
            it->success_count.fetch_add(1);
        } else {
            it->failure_count.fetch_add(1);
        }
    }
}

void DynamicTargetManager::expandConfigVariables(json& config, const AlarmMessage& alarm) {
    // Config 변수 확장 로직 (원본 파일 참조)
    try {
        std::function<void(json&)> expand = [&](json& obj) {
            if (obj.is_object()) {
                for (auto& [key, value] : obj.items()) {
                    expand(value);
                }
            } else if (obj.is_string()) {
                std::string str = obj.get<std::string>();
                
                // {{building_id}} 치환
                size_t pos = str.find("{{building_id}}");
                if (pos != std::string::npos) {
                    str.replace(pos, 16, std::to_string(alarm.bd));
                    obj = str;
                }
                
                // {{point_name}} 치환
                pos = str.find("{{point_name}}");
                if (pos != std::string::npos) {
                    str.replace(pos, 14, alarm.nm);
                    obj = str;
                }
            } else if (obj.is_array()) {
                for (auto& item : obj) {
                    expand(item);
                }
            }
        };
        
        expand(config);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Config 변수 확장 실패: " + std::string(e.what()));
    }
}

bool DynamicTargetManager::processTargetByIndex(
    size_t index, 
    const AlarmMessage& alarm, 
    TargetSendResult& result) {
    
    // targets_mutex_는 이미 caller에서 잠금 상태
    
    // =======================================================================
    // 1. 타겟 유효성 검증
    // =======================================================================
    
    if (index >= targets_.size()) {
        result.success = false;
        result.error_message = "잘못된 타겟 인덱스: " + std::to_string(index);
        LogManager::getInstance().Error(result.error_message);
        return false;
    }
    
    const auto& target = targets_[index];
    result.target_name = target.name;
    
    if (!target.enabled) {
        result.success = false;
        result.error_message = "타겟이 비활성화됨";
        LogManager::getInstance().Warn("타겟 비활성화됨: " + target.name);
        return false;
    }
    
    // =======================================================================
    // 2. Circuit Breaker 체크 (실패 방지)
    // =======================================================================
    
    auto protector_it = failure_protectors_.find(target.name);
    if (protector_it != failure_protectors_.end()) {
        if (!protector_it->second->canExecute()) {
            result.success = false;
            result.error_message = "Circuit Breaker OPEN - 타겟이 일시적으로 차단됨";
            LogManager::getInstance().Warn("Circuit Breaker OPEN: " + target.name);
            return false;
        }
    }
    
    // =======================================================================
    // 3. 핸들러 찾기
    // =======================================================================
    
    auto handler_it = handlers_.find(target.type);
    if (handler_it == handlers_.end()) {
        result.success = false;
        result.error_message = "지원되지 않는 타겟 타입: " + target.type;
        LogManager::getInstance().Error(result.error_message);
        return false;
    }
    
    // =======================================================================
    // 4. Config 변수 확장 (Initialize 전에 수행!)
    // =======================================================================
    
    json expanded_config = target.config;
    expandConfigVariables(expanded_config, alarm);  // alarm 데이터로 {{variable}} 치환
    
    LogManager::getInstance().Debug("Expanded config: " + expanded_config.dump());
    
    // =======================================================================
    // 5. 핸들러 초기화 (최초 1회만, expanded_config 사용!)
    // =======================================================================
    
    auto& mutable_target = targets_[index];
    
    if (mutable_target.handler_initialized.load() == false) {
        LogManager::getInstance().Info("핸들러 초기화 시작 - 타겟: " + target.name + ", 타입: " + target.type);
        LogManager::getInstance().Debug("초기화 config: " + expanded_config.dump());
        
        if (!handler_it->second->initialize(expanded_config)) {
            result.success = false;
            result.error_message = "핸들러 초기화 실패";
            LogManager::getInstance().Error("핸들러 초기화 실패 - 타겟: " + target.name);
            return false;
        }
        
        mutable_target.handler_initialized.store(true);
        LogManager::getInstance().Info("핸들러 초기화 완료 - 타겟: " + target.name);
    }
    
    // =======================================================================
    // 6. 알람 전송 시작
    // =======================================================================
    
    LogManager::getInstance().Info("알람 전송 시작 - 타겟: " + target.name + 
                                  ", 타입: " + target.type + 
                                  ", 알람: " + alarm.nm);
    
    // =======================================================================
    // 7. 실제 전송 수행 (시간 측정)
    // =======================================================================
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // 핸들러를 통한 전송
        result = handler_it->second->sendAlarm(alarm, expanded_config);
        result.target_name = target.name;
        
        auto end_time = std::chrono::steady_clock::now();
        result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // 전송 성공/실패 로그
        if (result.success) {
            LogManager::getInstance().Info(
                "알람 전송 성공 - 타겟: " + target.name + 
                ", 응답시간: " + std::to_string(result.response_time.count()) + "ms");
        } else {
            LogManager::getInstance().Error(
                "알람 전송 실패 - 타겟: " + target.name + 
                ", 오류: " + result.error_message);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "전송 예외: " + std::string(e.what());
        
        auto end_time = std::chrono::steady_clock::now();
        result.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        LogManager::getInstance().Error(
            "알람 전송 예외 - 타겟: " + target.name + ", 예외: " + result.error_message);
    }
    
    // =======================================================================
    // 8. Circuit Breaker 업데이트
    // =======================================================================
    
    if (protector_it != failure_protectors_.end()) {
        if (result.success) {
            protector_it->second->recordSuccess();
        } else {
            protector_it->second->recordFailure();
        }
    }
    
    // =======================================================================
    // 9. 타겟 통계 업데이트
    // =======================================================================
    
    updateTargetStatistics(target.name, result);
    
    // =======================================================================
    // 10. export_logs 테이블에 로그 기록 (선택적)
    // =======================================================================
    
    try {
        using PulseOne::Database::Repositories::ExportLogRepository;
        using PulseOne::Database::Entities::ExportLogEntity;
        
        #ifdef HAS_EXPORT_LOG_REPOSITORY
        ExportLogRepository log_repo;
        
        if (log_repo.initialize()) {
            ExportLogEntity log_entity;
            
            int target_id = target.config.value("target_id", 0);
            log_entity.setTargetId(target_id);
            log_entity.setPointId(alarm.bd);
            
            json alarm_json = alarm.to_json();
            log_entity.setSourceValue(alarm_json.dump());
            
            log_entity.setStatus(result.success ? "SUCCESS" : "FAILED");
            log_entity.setProcessingTimeMs(
                static_cast<int>(result.response_time.count()));
            
            if (!result.success) {
                log_entity.setErrorMessage(result.error_message);
                log_entity.setErrorCode("SEND_FAILED");
            }
            
            if (result.status_code > 0) {
                log_entity.setHttpStatusCode(result.status_code);
            }
            
            json metadata = {
                {"target_name", target.name},
                {"target_type", target.type},
                {"handler_version", "v5.3"},
                {"alarm_flag", alarm.al},
                {"point_name", alarm.nm}
            };
            
            if (!log_repo.save(log_entity)) {
                LogManager::getInstance().Warn(
                    "Export log 저장 실패 - 타겟: " + target.name);
            }
        }
        #endif
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn(
            "Export log 기록 실패: " + std::string(e.what()));
    }
    
    return result.success;
}



} // namespace CSP
} // namespace PulseOne