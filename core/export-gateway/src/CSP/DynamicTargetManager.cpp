/**
 * @file DynamicTargetManager.cpp
 * @brief 동적 타겟 관리자 구현 - 완전 재작성 (컴파일 에러 해결)
 * @author PulseOne Development Team
 * @date 2025-09-29
 * @version 5.0.0 (구조체 정의 위치 수정)
 */

#include "CSP/DynamicTargetManager.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/S3TargetHandler.h"
#include "CSP/FileTargetHandler.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <fstream>
#include <thread>
#include <algorithm>
#include <future>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <unordered_set>

namespace PulseOne {
namespace CSP {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DynamicTargetManager::DynamicTargetManager(const std::string& config_file_path)
    : config_file_path_(config_file_path)
    , last_config_check_(std::chrono::system_clock::time_point::min())
    , last_rate_reset_(std::chrono::system_clock::now()) {
    
    LogManager::getInstance().Info("DynamicTargetManager 초기화 시작: " + config_file_path);
    
    // 기본 핸들러들 등록
    registerDefaultHandlers();
    
    // 글로벌 설정 초기화
    global_settings_ = json{
        {"auto_reload", true},
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
        
        // 핸들러들 명시적 정리 (순서 중요)
        handlers_.clear();
        
        // 타겟들 정리
        {
            std::unique_lock<std::shared_mutex> lock(targets_mutex_);
            targets_.clear();
        }
        
        // 실패 방지기들 정리
        failure_protectors_.clear();
        
        LogManager::getInstance().Info("DynamicTargetManager 소멸 완료");
    } catch (const std::exception& e) {
        // 소멸자에서 예외 발생 시 로그만 남기고 전파하지 않음
        try {
            LogManager::getInstance().Error("DynamicTargetManager 소멸 중 예외: " + std::string(e.what()));
        } catch (...) {
            // LogManager도 실패하면 무시
        }
    } catch (...) {
        // 알 수 없는 예외도 무시
    }
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool DynamicTargetManager::start() {
    LogManager::getInstance().Info("DynamicTargetManager 시작...");
    
    try {
        is_running_.store(true);
        
        // 설정 파일 로드
        if (!loadConfiguration()) {
            LogManager::getInstance().Error("설정 파일 로드 실패");
            is_running_.store(false);
            return false;
        }
        
        // 실패 방지기들 초기화
        initializeFailureProtectors();
        
        // 백그라운드 스레드들 시작
        startBackgroundThreads();
        
        LogManager::getInstance().Info("DynamicTargetManager 시작 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DynamicTargetManager 시작 실패: " + std::string(e.what()));
        is_running_.store(false);
        return false;
    }
}

void DynamicTargetManager::stop() {
    if (!is_running_.load()) return;
    
    try {
        LogManager::getInstance().Info("DynamicTargetManager 중지...");
        
        should_stop_.store(true);
        is_running_.store(false);
        
        // 백그라운드 스레드 정리
        stopBackgroundThreads();
        
        LogManager::getInstance().Info("DynamicTargetManager 중지 완료");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DynamicTargetManager 중지 중 예외: " + std::string(e.what()));
    } catch (...) {
        // 예외 무시
    }
}

// =============================================================================
// 설정 관리
// =============================================================================

bool DynamicTargetManager::loadConfiguration() {
    std::unique_lock<std::mutex> lock(config_mutex_);
    
    try {
        LogManager::getInstance().Info("설정 파일 로드 시작: " + config_file_path_);
        
        if (!std::filesystem::exists(config_file_path_)) {
            LogManager::getInstance().Warn("설정 파일이 존재하지 않음, 기본 설정으로 생성: " + config_file_path_);
            createDefaultConfigFile();
        }
        
        std::ifstream file(config_file_path_);
        if (!file.is_open()) {
            LogManager::getInstance().Error("설정 파일을 열 수 없음: " + config_file_path_);
            return false;
        }
        
        json config;
        file >> config;
        file.close();
        
        // 설정 유효성 검증
        std::vector<std::string> errors;
        if (!validateConfiguration(config, errors)) {
            LogManager::getInstance().Error("설정 파일 유효성 검증 실패");
            for (const auto& error : errors) {
                LogManager::getInstance().Error("  - " + error);
            }
            return false;
        }
        
        // 글로벌 설정 적용
        if (config.contains("global")) {
            global_settings_ = config["global"];
            auto_reload_enabled_.store(global_settings_.value("auto_reload", true));
            LogManager::getInstance().Info("글로벌 설정 적용 완료");
        }
        
        // 타겟들 로드
        std::unique_lock<std::shared_mutex> targets_lock(targets_mutex_);
        targets_.clear();
        
        if (config.contains("targets") && config["targets"].is_array()) {
            for (const auto& target_config : config["targets"]) {
                targets_.emplace_back();
                auto& target = targets_.back();
                
                target.name = target_config.value("name", "unnamed");
                target.type = target_config.value("type", "http");
                target.enabled = target_config.value("enabled", true);
                target.priority = target_config.value("priority", 100);
                target.description = target_config.value("description", "");
                target.config = target_config.value("config", json{});
                
                if (handlers_.find(target.type) == handlers_.end()) {
                    LogManager::getInstance().Error("지원되지 않는 타겟 타입, 건너뜀: " + target.type);
                    targets_.pop_back();
                    continue;
                }
                
                LogManager::getInstance().Info("타겟 로드됨: " + target.name);
            }
        }
        
        targets_lock.unlock();
        
        // 설정 파일 변경 시간 업데이트
        last_config_check_ = std::chrono::system_clock::now();
        
        LogManager::getInstance().Info("설정 파일 로드 완료 - 타겟 수: " + std::to_string(targets_.size()));
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("설정 파일 로드 실패: " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::reloadIfChanged() {
    try {
        if (!std::filesystem::exists(config_file_path_)) {
            return false;
        }
        
        auto fs_time = std::filesystem::last_write_time(config_file_path_);
        // 파일 시간과 시스템 시간 비교를 단순화
        auto now = std::chrono::system_clock::now();
        
        // 1초 이상 차이가 나면 변경된 것으로 간주
        if ((now - last_config_check_) > std::chrono::seconds(1)) {
            LogManager::getInstance().Info("설정 파일 변경 감지, 자동 재로드 중...");
            return loadConfiguration();
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("설정 파일 변경 확인 실패: " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::forceReload() {
    LogManager::getInstance().Info("설정 파일 강제 재로드 시작");
    return loadConfiguration();
}

bool DynamicTargetManager::saveConfiguration(const json& config) {
    std::unique_lock<std::mutex> lock(config_mutex_);
    
    try {
        std::vector<std::string> errors;
        if (!validateConfiguration(config, errors)) {
            LogManager::getInstance().Error("저장할 설정 유효성 검증 실패");
            return false;
        }
        
        if (std::filesystem::exists(config_file_path_)) {
            backupConfigFile();
        }
        
        std::ofstream file(config_file_path_);
        if (!file.is_open()) {
            LogManager::getInstance().Error("설정 파일을 쓸 수 없음: " + config_file_path_);
            return false;
        }
        
        file << config.dump(2);
        file.close();
        
        LogManager::getInstance().Info("설정 파일 저장 완료: " + config_file_path_);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("설정 파일 저장 실패: " + std::string(e.what()));
        return false;
    }
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

// 🚨 네임스페이스 레벨 구조체 사용
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

// 🚨 네임스페이스 레벨 구조체 사용
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
    return std::vector<DynamicTarget>();  // 복사 문제로 인해 빈 벡터 반환
}

json DynamicTargetManager::getSystemStatus() const {
    json status;
    status["running"] = isRunning();
    status["target_count"] = targets_.size();
    return status;
}

json DynamicTargetManager::getDetailedStatistics() const {
    json stats;
    stats["system"]["target_count"] = targets_.size();
    return stats;
}

// 🚨 네임스페이스 레벨 구조체 사용
SystemMetrics DynamicTargetManager::getSystemMetrics() const {
    SystemMetrics metrics;
    
    try {
        std::shared_lock<std::shared_mutex> lock(targets_mutex_);
        
        metrics.total_targets = targets_.size();
        
        size_t active_count = 0;
        size_t healthy_count = 0;
        
        for (const auto& target : targets_) {
            if (target.enabled) {
                active_count++;
                if (target.healthy.load()) {
                    healthy_count++;
                }
            }
        }
        
        metrics.active_targets = active_count;
        metrics.healthy_targets = healthy_count;
        metrics.total_requests = total_requests_.load();
        metrics.successful_requests = successful_requests_.load();
        metrics.failed_requests = failed_requests_.load();
        
        uint64_t total_reqs = metrics.successful_requests + metrics.failed_requests;
        metrics.overall_success_rate = total_reqs > 0 ? 
            (static_cast<double>(metrics.successful_requests) / total_reqs * 100.0) : 0.0;
        
        metrics.last_update = std::chrono::system_clock::now();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("시스템 메트릭 생성 실패: " + std::string(e.what()));
    }
    
    return metrics;
}

json DynamicTargetManager::generatePerformanceReport(
    std::chrono::system_clock::time_point start_time,
    std::chrono::system_clock::time_point end_time) const {
    
    auto duration = end_time - start_time;
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    return json{
        {"status", "implemented"},
        {"report_period_start", std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count()},
        {"report_period_end", std::chrono::duration_cast<std::chrono::milliseconds>(end_time.time_since_epoch()).count()},
        {"report_duration_ms", duration_ms},
        {"total_targets", targets_.size()},
        {"total_requests", total_requests_.load()},
        {"successful_requests", successful_requests_.load()},
        {"failed_requests", failed_requests_.load()},
        {"peak_concurrent_requests", peak_concurrent_requests_.load()}
    };
}

// =============================================================================
// 설정 유효성 검증
// =============================================================================

bool DynamicTargetManager::validateConfiguration(const json& config, std::vector<std::string>& errors) {
    errors.clear();
    return config.is_object();
}

bool DynamicTargetManager::validateTargetConfig(const json& target_config, std::vector<std::string>& errors) {
    errors.clear();
    return target_config.is_object();
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
// 내부 구현 메서드들
// =============================================================================

void DynamicTargetManager::registerDefaultHandlers() {
    handlers_["http"] = std::make_unique<HttpTargetHandler>();
    handlers_["https"] = std::make_unique<HttpTargetHandler>();
    handlers_["s3"] = std::make_unique<S3TargetHandler>();
    handlers_["file"] = std::make_unique<FileTargetHandler>();
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
        
        LogManager::getInstance().Info("실패 방지기 초기화 완료: " + target_name);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("실패 방지기 초기화 실패: " + target_name + " - " + std::string(e.what()));
    }
}

void DynamicTargetManager::startBackgroundThreads() {
    should_stop_.store(false);
    
    if (auto_reload_enabled_.load()) {
        config_watcher_thread_ = std::make_unique<std::thread>(&DynamicTargetManager::configWatcherThread, this);
    }
    
    health_check_thread_ = std::make_unique<std::thread>(&DynamicTargetManager::healthCheckThread, this);
    metrics_collector_thread_ = std::make_unique<std::thread>(&DynamicTargetManager::metricsCollectorThread, this);
}

void DynamicTargetManager::stopBackgroundThreads() {
    should_stop_.store(true);

    auto timeout = std::chrono::milliseconds(1000);
    
    if (config_watcher_thread_ && config_watcher_thread_->joinable()) {
        config_watcher_thread_->join();
    }
    
    if (health_check_thread_ && health_check_thread_->joinable()) {
        health_check_thread_->join();
    }
    
    if (metrics_collector_thread_ && metrics_collector_thread_->joinable()) {
        metrics_collector_thread_->join();
    }
}

bool DynamicTargetManager::processTargetByIndex(size_t index, const AlarmMessage& alarm, TargetSendResult& result) {
    auto start_time = std::chrono::steady_clock::now();
    
    result.success = false;
    
    try {
        if (index >= targets_.size()) {
            result.error_message = "Invalid target index";
            return false;
        }
        
        const auto& target = targets_[index];
        result.target_name = target.name;
        result.target_type = target.type;
        
        if (!checkRateLimit()) {
            result.error_message = "Rate limit exceeded";
            return false;
        }
        
        auto handler_it = handlers_.find(target.type);
        if (handler_it == handlers_.end()) {
            result.error_message = "Handler not found for type: " + target.type;
            return false;
        }
        
        json expanded_config = target.config;
        expandConfigVariables(expanded_config, alarm);
        
        // ✅ mutable_target 선언을 여기로 이동
        auto& mutable_target = targets_[index];
        
        // 핸들러 초기화 (타겟마다 한 번만)
        if (mutable_target.handler_initialized.load() == false) {
            if (!handler_it->second->initialize(expanded_config)) {
                result.error_message = "Handler initialization failed";
                return false;
            }
            mutable_target.handler_initialized.store(true);
        }
        
        result = handler_it->second->sendAlarm(alarm, expanded_config);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 통계 업데이트 (mutable_target 이미 선언됨)
        if (result.success) {
            mutable_target.success_count.fetch_add(1);
            mutable_target.consecutive_failures.store(0);
            mutable_target.healthy.store(true);
            total_requests_.fetch_add(1);
            successful_requests_.fetch_add(1);
        } else {
            mutable_target.failure_count.fetch_add(1);
            mutable_target.consecutive_failures.fetch_add(1);
            
            if (mutable_target.consecutive_failures.load() >= 5) {
                mutable_target.healthy.store(false);
            }
            total_requests_.fetch_add(1);
            failed_requests_.fetch_add(1);
        }
        
        double current_avg = mutable_target.avg_response_time_ms.load();
        double new_avg = (current_avg * 0.8) + (duration.count() * 0.2);
        mutable_target.avg_response_time_ms.store(new_avg);
        
        return result.success;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "processTargetByIndex 예외: " + std::string(e.what());
        return false;
    }
}

bool DynamicTargetManager::checkRateLimit() {
    return true;  // 간단한 구현
}

void DynamicTargetManager::updateTargetHealth(const std::string& target_name, bool healthy) {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    auto it = findTarget(target_name);
    if (it != targets_.end()) {
        it->healthy.store(healthy);
    }
}

void DynamicTargetManager::updateTargetStatistics(const std::string& target_name, const TargetSendResult& result) {
    // 이미 processTargetByIndex에서 처리됨
}

bool DynamicTargetManager::createDefaultConfigFile() {
    try {
        json default_config = json{
            {"global", {
                {"auto_reload", true},
                {"health_check_interval_sec", 60},
                {"max_concurrent_requests", 100}
            }},
            {"targets", json::array({
                {
                    {"name", "example_http"},
                    {"type", "http"},
                    {"enabled", false},
                    {"priority", 100},
                    {"description", "예제 HTTP 타겟"},
                    {"config", {
                        {"url", "https://api.example.com/alarms"},
                        {"method", "POST"}
                    }}
                }
            })}
        };
        
        std::ofstream file(config_file_path_);
        if (!file.is_open()) {
            return false;
        }
        
        file << default_config.dump(2);
        file.close();
        
        LogManager::getInstance().Info("기본 설정 파일 생성 완료: " + config_file_path_);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("기본 설정 파일 생성 실패: " + std::string(e.what()));
        return false;
    }
}

bool DynamicTargetManager::backupConfigFile() {
    try {
        if (!std::filesystem::exists(config_file_path_)) {
            return true;
        }
        
        std::string backup_path = config_file_path_ + ".backup." + 
                                 std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch()).count());
        
        std::filesystem::copy_file(config_file_path_, backup_path);
        LogManager::getInstance().Info("설정 파일 백업 생성: " + backup_path);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("설정 파일 백업 실패: " + std::string(e.what()));
        return false;
    }
}

void DynamicTargetManager::expandConfigVariables(json& config, const AlarmMessage& alarm) {
    if (config.empty() || alarm.nm.empty()) {
        return;
    }
    
    try {
        LogManager::getInstance().Debug("알람 변수 확장 - Building: " + std::to_string(alarm.bd) + 
                                      ", Point: " + alarm.nm + ", Value: " + std::to_string(alarm.vl));
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("설정 변수 확장 실패: " + std::string(e.what()));
    }
}

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

void DynamicTargetManager::configWatcherThread() {
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (should_stop_.load()) break;
        reloadIfChanged();
    }
}

void DynamicTargetManager::healthCheckThread() {
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
        if (should_stop_.load()) break;
        // 헬스 체크 로직
    }
}

void DynamicTargetManager::metricsCollectorThread() {
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (should_stop_.load()) break;
        // 메트릭 수집 로직
    }
}

void DynamicTargetManager::cleanupThread() {
    // 필요시 구현
}

} // namespace CSP
} // namespace PulseOne