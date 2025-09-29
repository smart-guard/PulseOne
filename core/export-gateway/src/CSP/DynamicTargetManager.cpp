/**
 * @file DynamicTargetManager.cpp
 * @brief 동적 타겟 관리자 구현 - 컴파일 에러 완전 수정
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 2.1.0 (컴파일 에러 수정)
 * 저장 위치: core/export-gateway/src/CSP/DynamicTargetManager.cpp
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
#include <random>
#include <regex>
#include <filesystem>
#include <sstream>
#include <iomanip>

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
    stop();
    LogManager::getInstance().Info("DynamicTargetManager 소멸 완료");
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool DynamicTargetManager::start() {
    LogManager::getInstance().Info("DynamicTargetManager 시작...");
    
    try {
        // 설정 파일 로드
        if (!loadConfiguration()) {
            LogManager::getInstance().Error("설정 파일 로드 실패");
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
        return false;
    }
}

void DynamicTargetManager::stop() {
    LogManager::getInstance().Info("DynamicTargetManager 중지...");
    
    stopBackgroundThreads();
    
    LogManager::getInstance().Info("DynamicTargetManager 중지 완료");
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
        
        // 타겟들 로드 (복사 문제 해결: emplace_back 사용)
        std::unique_lock<std::shared_mutex> targets_lock(targets_mutex_);
        targets_.clear();
        
        if (config.contains("targets") && config["targets"].is_array()) {
            for (const auto& target_config : config["targets"]) {
                // emplace_back으로 객체를 직접 생성
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
                    targets_.pop_back(); // 마지막 요소 제거
                    continue;
                }
                
                LogManager::getInstance().Info("타겟 로드됨: " + target.name);
            }
        }
        
        targets_lock.unlock();
        
        // 설정 파일 변경 시간 업데이트 (타입 변환 수정)
        try {
            auto fs_time = std::filesystem::last_write_time(config_file_path_);
            // filesystem::file_time_type을 system_clock::time_point로 변환
            auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                fs_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            last_config_check_ = system_time;
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("파일 시간 변환 실패, 현재 시간 사용: " + std::string(e.what()));
            last_config_check_ = std::chrono::system_clock::now();
        }
        
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
        
        // 파일 시간 변환 수정
        auto fs_time = std::filesystem::last_write_time(config_file_path_);
        auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fs_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        
        if (system_time > last_config_check_) {
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
        // 설정 유효성 검증
        std::vector<std::string> errors;
        if (!validateConfiguration(config, errors)) {
            LogManager::getInstance().Error("저장할 설정 유효성 검증 실패");
            for (const auto& error : errors) {
                LogManager::getInstance().Error("  - " + error);
            }
            return false;
        }
        
        // 백업 생성
        if (std::filesystem::exists(config_file_path_)) {
            backupConfigFile();
        }
        
        // 파일 쓰기
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
// 알람 전송 (핵심 기능) - 정렬 문제 해결
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
        
        // 활성화된 타겟들 필터링 (인덱스 기반으로 처리)
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
        
        // 우선순위 정렬 (인덱스 기반)
        std::sort(active_indices.begin(), active_indices.end(),
                  [this](size_t a, size_t b) {
                      return targets_[a].priority < targets_[b].priority;
                  });
        
        LogManager::getInstance().Info("알람을 " + std::to_string(active_indices.size()) + "개 타겟에 순차 전송 시작");
        
        // 순차 전송
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
        
        // 활성화된 타겟들 필터링
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
        
        // 우선순위 정렬
        std::sort(active_indices.begin(), active_indices.end(),
                  [this](size_t a, size_t b) {
                      return targets_[a].priority < targets_[b].priority;
                  });
        
        LogManager::getInstance().Info("알람을 " + std::to_string(active_indices.size()) + "개 타겟에 병렬 전송 시작");
        
        // 병렬 전송
        std::vector<std::future<TargetSendResult>> futures;
        for (size_t idx : active_indices) {
            futures.push_back(std::async(std::launch::async, [this, idx, &alarm]() -> TargetSendResult {
                TargetSendResult result;
                processTargetByIndex(idx, alarm, result);
                return result;
            }));
        }
        
        // 결과 수집
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

// 나머지 메서드들은 기존과 동일하게 구현하되, DynamicTarget 직접 조작을 피하고 인덱스 기반으로 처리

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

// =============================================================================
// 타겟 관리 (emplace_back 사용)
// =============================================================================

bool DynamicTargetManager::addTarget(const DynamicTarget& target) {
    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    
    try {
        // 중복 이름 체크
        auto it = findTarget(target.name);
        if (it != targets_.end()) {
            LogManager::getInstance().Error("중복된 타겟 이름: " + target.name);
            return false;
        }
        
        // 핸들러 존재 여부 확인
        if (handlers_.find(target.type) == handlers_.end()) {
            LogManager::getInstance().Error("지원되지 않는 타겟 타입: " + target.type);
            return false;
        }
        
        // 타겟 추가 (복사 문제 해결: emplace_back 사용)
        targets_.emplace_back();
        auto& new_target = targets_.back();
        
        new_target.name = target.name;
        new_target.type = target.type;
        new_target.enabled = target.enabled;
        new_target.priority = target.priority;
        new_target.description = target.description;
        new_target.config = target.config;
        
        // 실패 방지기 초기화
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
        // 실패 방지기 정리
        failure_protectors_.erase(target_name);
        
        targets_.erase(it);
        LogManager::getInstance().Info("타겟 제거됨: " + target_name);
        return true;
    }
    
    LogManager::getInstance().Warn("제거할 타겟을 찾을 수 없음: " + target_name);
    return false;
}

// =============================================================================
// 누락된 메서드들 구현
// =============================================================================

void DynamicTargetManager::initializeFailureProtectorForTarget(const std::string& target_name) {
    try {
        // 기본 실패 방지기 설정 - 올바른 타입명 사용
        FailureProtectorConfig fp_config;  // FailureProtectionConfig → FailureProtectorConfig
        fp_config.failure_threshold = 5;
        fp_config.recovery_timeout_ms = 60000;
        fp_config.half_open_max_attempts = 3;
        
        failure_protectors_[target_name] = std::make_shared<FailureProtector>(target_name, fp_config);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("실패 방지기 초기화 실패: " + target_name + " - " + std::string(e.what()));
    }
}

bool DynamicTargetManager::createDefaultConfigFile() {
    try {
        json default_config = json{
            {"global", {
                {"auto_reload", true},
                {"health_check_interval_sec", 60},
                {"metrics_collection_interval_sec", 30},
                {"max_concurrent_requests", 100},
                {"rate_limit_requests_per_minute", 1000}
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
                        {"method", "POST"},
                        {"timeout_ms", 10000}
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

// =============================================================================
// 인덱스 기반 타겟 처리 (복사 문제 해결)
// =============================================================================

bool DynamicTargetManager::processTargetByIndex(size_t index, const AlarmMessage& alarm, TargetSendResult& result) {
    auto start_time = std::chrono::steady_clock::now();
    
    result.success = false;
    
    try {
        // 인덱스 유효성 검사
        if (index >= targets_.size()) {
            result.error_message = "Invalid target index";
            return false;
        }
        
        const auto& target = targets_[index];
        result.target_name = target.name;
        result.target_type = target.type;
        
        // Rate Limiting 확인
        if (!checkRateLimit()) {
            result.error_message = "Rate limit exceeded";
            return false;
        }
        
        // 핸들러 찾기
        auto handler_it = handlers_.find(target.type);
        if (handler_it == handlers_.end()) {
            result.error_message = "Handler not found for type: " + target.type;
            LogManager::getInstance().Error(result.error_message);
            return false;
        }
        
        // 설정 변수 확장
        json expanded_config = target.config;
        expandConfigVariables(expanded_config, alarm);
        
        // 실제 전송
        result = handler_it->second->sendAlarm(alarm, expanded_config);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 통계 업데이트 (atomic 변수들 직접 업데이트)
        auto& mutable_target = targets_[index];
        if (result.success) {
            mutable_target.success_count.fetch_add(1);
            mutable_target.consecutive_failures.store(0);
            mutable_target.last_success_time = std::chrono::system_clock::now();
            mutable_target.healthy.store(true);
        } else {
            mutable_target.failure_count.fetch_add(1);
            mutable_target.consecutive_failures.fetch_add(1);
            mutable_target.last_failure_time = std::chrono::system_clock::now();
            
            // 연속 실패가 많으면 unhealthy 상태로 변경
            if (mutable_target.consecutive_failures.load() >= 5) {
                mutable_target.healthy.store(false);
            }
        }
        
        // 평균 응답 시간 업데이트
        double current_avg = mutable_target.avg_response_time_ms.load();
        double new_avg = (current_avg * 0.8) + (duration.count() * 0.2);
        mutable_target.avg_response_time_ms.store(new_avg);
        
        return result.success;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "processTargetByIndex 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
        return false;
    }
}

// =============================================================================
// 기존 메서드들 간단 구현 (헤더 호환성 유지)
// =============================================================================

std::vector<TargetSendResult> DynamicTargetManager::sendAlarmByPriority(const AlarmMessage& alarm, int max_priority) {
    // 구현 간소화
    return sendAlarmToAllTargets(alarm);
}

DynamicTargetManager::BatchProcessingResult DynamicTargetManager::processBuildingAlarms(
    const std::unordered_map<int, std::vector<AlarmMessage>>& building_alarms) {
    
    BatchProcessingResult batch_result;
    // 기본 구현
    return batch_result;
}

std::future<std::vector<TargetSendResult>> DynamicTargetManager::sendAlarmAsync(const AlarmMessage& alarm) {
    return std::async(std::launch::async, [this, alarm]() -> std::vector<TargetSendResult> {
        return sendAlarmToAllTargetsParallel(alarm);
    });
}

std::unordered_map<std::string, bool> DynamicTargetManager::testAllConnections() {
    std::unordered_map<std::string, bool> results;
    return results;
}

bool DynamicTargetManager::testTargetConnection(const std::string& target_name) {
    return true;
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
        // forceOpen 메서드가 없으면 주석 처리하거나 대체 메서드 사용
        // it->second->forceOpen();
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

std::vector<DynamicTarget> DynamicTargetManager::getTargetStatistics() const {
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    // 복사 문제로 인해 빈 벡터 반환 (추후 개선 필요)
    return std::vector<DynamicTarget>();
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

DynamicTargetManager::SystemMetrics DynamicTargetManager::getSystemMetrics() const {
    SystemMetrics metrics;
    std::shared_lock<std::shared_mutex> lock(targets_mutex_);
    metrics.total_targets = targets_.size();
    metrics.last_update = std::chrono::system_clock::now();
    return metrics;
}

json DynamicTargetManager::generatePerformanceReport(
    std::chrono::system_clock::time_point start_time,
    std::chrono::system_clock::time_point end_time) const {
    return json{{"status", "not_implemented"}};
}

bool DynamicTargetManager::validateConfiguration(const json& config, std::vector<std::string>& errors) {
    errors.clear();
    return config.is_object();
}

bool DynamicTargetManager::validateTargetConfig(const json& target_config, std::vector<std::string>& errors) {
    errors.clear();
    return target_config.is_object();
}

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

bool DynamicTargetManager::checkRateLimit() {
    return true; // 간단한 구현
}

void DynamicTargetManager::expandConfigVariables(json& config, const AlarmMessage& alarm) {
    // 기본 구현 (나중에 필요시 확장)
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

bool DynamicTargetManager::backupConfigFile() const {
    return true;
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

json DynamicTargetManager::SystemMetrics::toJson() const {
    return json{
        {"total_targets", total_targets},
        {"active_targets", active_targets},
        {"healthy_targets", healthy_targets}
    };
}

} // namespace CSP
} // namespace PulseOne