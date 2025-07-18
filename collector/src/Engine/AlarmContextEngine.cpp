/**
 * @file AlarmContextEngine.cpp
 * @brief 알람 컨텍스트 추적 및 분석 엔진 구현
 * @details 사용자 제어 명령과 알람 발생 간의 상관관계를 분석하여 컨텍스트 정보 제공
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 2.0.0
 */

#include "Engine/AlarmContextEngine.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Engine {

using json = nlohmann::json;
using namespace std::chrono;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

AlarmContextEngine::AlarmContextEngine(std::shared_ptr<DatabaseManager> db_manager,
                                       std::shared_ptr<RedisClient> redis_client)
    : db_manager_(db_manager)
    , redis_client_(redis_client)
    , logger_(LogManager::Instance())
    , max_command_history_size_(1000)
    , max_alarm_history_size_(500)
    , default_correlation_window_ms_(30000)  // 30초
    , cleanup_interval_seconds_(300)         // 5분
    , min_correlation_confidence_(0.3)       // 30%
{
    logger_.Info("AlarmContextEngine created");
}

AlarmContextEngine::~AlarmContextEngine() {
    Stop();
}

// =============================================================================
// 생명주기 관리
// =============================================================================

bool AlarmContextEngine::Initialize() {
    try {
        logger_.Info("Initializing AlarmContextEngine...");
        
        // 데이터베이스에서 의존성 규칙 로드
        if (!LoadDependencyRulesFromDB()) {
            logger_.Warning("Failed to load dependency rules from database, continuing with empty rules");
        }
        
        logger_.Info("AlarmContextEngine initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to initialize AlarmContextEngine: " + std::string(e.what()));
        return false;
    }
}

bool AlarmContextEngine::Start() {
    if (running_.load()) {
        logger_.Warning("AlarmContextEngine is already running");
        return false;
    }
    
    try {
        running_ = true;
        
        // 정리 작업 스레드 시작
        cleanup_thread_ = std::thread(&AlarmContextEngine::CleanupThreadMain, this);
        
        // 분석 스레드 시작
        analysis_thread_ = std::thread(&AlarmContextEngine::AnalysisThreadMain, this);
        
        logger_.Info("AlarmContextEngine started successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to start AlarmContextEngine: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

void AlarmContextEngine::Stop() {
    if (!running_.load()) {
        return;
    }
    
    logger_.Info("Stopping AlarmContextEngine...");
    
    running_ = false;
    
    // 스레드 종료 대기
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    if (analysis_thread_.joinable()) {
        analysis_thread_.join();
    }
    
    logger_.Info("AlarmContextEngine stopped");
}

// =============================================================================
// 사용자 명령 추적
// =============================================================================

void AlarmContextEngine::RegisterUserCommand(const UserCommandContext& command_context) {
    std::unique_lock<std::shared_mutex> lock(commands_mutex_);
    
    try {
        recent_commands_.push_back(command_context);
        
        // 히스토리 크기 제한
        while (recent_commands_.size() > static_cast<size_t>(max_command_history_size_)) {
            recent_commands_.pop_front();
        }
        
        logger_.Debug("User command registered: " + command_context.command_id + 
                     " by " + command_context.user_name);
        
        // Redis에 명령 이벤트 발행
        PublishCommandEvent(command_context);
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to register user command: " + std::string(e.what()));
    }
}

void AlarmContextEngine::UpdateCommandCompletion(const std::string& command_id, 
                                                 bool was_successful,
                                                 const Timestamp& completion_time) {
    std::unique_lock<std::shared_mutex> lock(commands_mutex_);
    
    try {
        // 명령 ID로 검색하여 업데이트
        auto it = std::find_if(recent_commands_.rbegin(), recent_commands_.rend(),
            [&command_id](const UserCommandContext& cmd) {
                return cmd.command_id == command_id;
            });
        
        if (it != recent_commands_.rend()) {
            it->was_successful = was_successful;
            it->completed_at = completion_time;
            
            logger_.Debug("Command completion updated: " + command_id + 
                         ", Success: " + (was_successful ? "true" : "false"));
        } else {
            logger_.Warning("Command not found for completion update: " + command_id);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to update command completion: " + std::string(e.what()));
    }
}

std::vector<UserCommandContext> AlarmContextEngine::GetRecentCommands(const UUID& device_id, int limit) const {
    std::shared_lock<std::shared_mutex> lock(commands_mutex_);
    
    std::vector<UserCommandContext> result;
    result.reserve(limit);
    
    try {
        int count = 0;
        for (auto it = recent_commands_.rbegin(); it != recent_commands_.rend() && count < limit; ++it) {
            if (device_id.empty() || it->device_id == device_id) {
                result.push_back(*it);
                count++;
            }
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to get recent commands: " + std::string(e.what()));
    }
    
    return result;
}

// =============================================================================
// 알람 컨텍스트 분석 (메인 기능)
// =============================================================================

EnhancedAlarmEvent AlarmContextEngine::AnalyzeAlarmContext(const AlarmEvent& alarm_event) {
    total_alarms_analyzed_++;
    
    EnhancedAlarmEvent enhanced_alarm;
    
    try {
        // 기본 알람 정보 복사
        CopyBaseAlarmInfo(alarm_event, enhanced_alarm);
        
        // 컨텍스트 분석
        enhanced_alarm.context = AnalyzeCorrelation(alarm_event, default_correlation_window_ms_);
        
        // 사용자 트리거 여부 판단
        enhanced_alarm.context.is_user_triggered = IsUserTriggeredAlarm(alarm_event);
        
        // 통계 업데이트
        if (enhanced_alarm.context.is_user_triggered) {
            user_triggered_alarms_++;
            enhanced_alarm.context.trigger_type = AlarmTriggerType::USER_ACTION;
        } else {
            automatic_alarms_++;
            enhanced_alarm.context.trigger_type = AlarmTriggerType::AUTOMATIC;
        }
        
        // 알람 히스토리에 추가
        AddToAlarmHistory(enhanced_alarm);
        
        // Redis에 확장된 알람 정보 발행
        PublishEnhancedAlarm(enhanced_alarm);
        
        logger_.Debug("Alarm context analyzed: " + enhanced_alarm.alarm_id + 
                     ", User triggered: " + (enhanced_alarm.context.is_user_triggered ? "yes" : "no") +
                     ", Confidence: " + std::to_string(enhanced_alarm.context.correlation_confidence));
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to analyze alarm context: " + std::string(e.what()));
        enhanced_alarm.context.analysis_notes.push_back("Analysis failed: " + std::string(e.what()));
    }
    
    return enhanced_alarm;
}

bool AlarmContextEngine::IsUserTriggeredAlarm(const AlarmEvent& alarm_event) {
    std::shared_lock<std::shared_mutex> lock(commands_mutex_);
    
    try {
        auto alarm_time = alarm_event.triggered_at;
        auto cutoff_time = alarm_time - milliseconds(default_correlation_window_ms_);
        
        // 최근 명령들 중에서 관련 명령 검색
        for (auto it = recent_commands_.rbegin(); it != recent_commands_.rend(); ++it) {
            if (it->executed_at < cutoff_time) {
                break; // 시간 윈도우를 벗어남
            }
            
            // 디바이스 일치 확인
            if (it->device_id == alarm_event.device_id) {
                // 제어 포인트 관련성 확인
                if (IsPointRelated(it->control_point_id, alarm_event.point_id)) {
                    return true;
                }
            }
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to check user triggered alarm: " + std::string(e.what()));
    }
    
    return false;
}

AlarmContext AlarmContextEngine::AnalyzeCorrelation(const AlarmEvent& alarm_event, 
                                                   int correlation_window_ms) {
    AlarmContext context;
    
    try {
        if (correlation_window_ms <= 0) {
            correlation_window_ms = default_correlation_window_ms_;
        }
        
        context.correlation_window_ms = correlation_window_ms;
        context.analysis_time = system_clock::now();
        
        auto alarm_time = alarm_event.triggered_at;
        auto cutoff_time = alarm_time - milliseconds(correlation_window_ms);
        
        // 관련 명령들 찾기
        std::vector<UserCommandContext> related_commands = FindRelatedCommands(alarm_event, cutoff_time);
        
        if (!related_commands.empty()) {
            // 가장 관련성이 높은 명령 선택
            auto best_command = SelectBestRelatedCommand(related_commands, alarm_event);
            if (best_command) {
                context.related_command = *best_command;
                context.is_user_triggered = true;
                context.trigger_type = AlarmTriggerType::USER_ACTION;
            }
            
            // 모든 관련 명령을 잠재적 원인으로 추가
            context.potential_causes = related_commands;
        }
        
        // 상관관계 신뢰도 계산
        context.correlation_confidence = CalculateCorrelationConfidence(context, alarm_event);
        
        // 분석 노트 생성
        GenerateAnalysisNotes(context, alarm_event);
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to analyze correlation: " + std::string(e.what()));
        context.analysis_notes.push_back("Correlation analysis failed: " + std::string(e.what()));
    }
    
    return context;
}

std::vector<EnhancedAlarmEvent> AlarmContextEngine::AnalyzeBatchAlarms(const std::vector<AlarmEvent>& alarm_events) {
    std::vector<EnhancedAlarmEvent> enhanced_alarms;
    enhanced_alarms.reserve(alarm_events.size());
    
    try {
        for (const auto& alarm_event : alarm_events) {
            enhanced_alarms.push_back(AnalyzeAlarmContext(alarm_event));
        }
        
        // 배치 알람들 간의 상관관계 분석 (알람 폭풍 감지)
        AnalyzeBatchCorrelations(enhanced_alarms);
        
        logger_.Info("Batch alarm analysis completed: " + std::to_string(enhanced_alarms.size()) + " alarms");
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to analyze batch alarms: " + std::string(e.what()));
    }
    
    return enhanced_alarms;
}

// =============================================================================
// 의존성 규칙 관리
// =============================================================================

bool AlarmContextEngine::LoadDependencyRulesFromDB() {
    if (!db_manager_) {
        logger_.Warning("Database manager not available for loading dependency rules");
        return false;
    }
    
    try {
        std::unique_lock<std::shared_mutex> lock(dependencies_mutex_);
        
        // 데이터베이스에서 의존성 규칙 조회
        std::string query = R"(
            SELECT rule_id, source_point_id, target_point_id, dependency_type,
                   delay_ms, influence_factor, description, is_enabled
            FROM alarm_dependency_rules 
            WHERE is_enabled = true
            ORDER BY influence_factor DESC
        )";
        
        // 실제 구현에서는 db_manager_->ExecuteQuery() 사용
        // 여기서는 예시 데이터로 구현
        LoadDefaultDependencyRules();
        
        // 의존성 맵 재구성
        RebuildDependencyMap();
        
        logger_.Info("Dependency rules loaded successfully: " + std::to_string(dependency_rules_.size()) + " rules");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to load dependency rules: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 내부 유틸리티 함수들
// =============================================================================

void AlarmContextEngine::CopyBaseAlarmInfo(const AlarmEvent& source, EnhancedAlarmEvent& target) {
    target.alarm_id = source.alarm_id;
    target.device_id = source.device_id;
    target.device_name = source.device_name;
    target.point_id = source.point_id;
    target.point_name = source.point_name;
    target.message = source.message;
    target.severity = static_cast<AlarmSeverity>(source.severity);
    target.trigger_value = source.trigger_value;
    target.threshold_value = source.threshold_value;
    target.unit = source.unit;
    target.triggered_at = source.triggered_at;
    target.is_active = source.is_active;
    target.alarm_rule_id = source.alarm_rule_id;
    target.alarm_type = source.alarm_type;
}

std::vector<UserCommandContext> AlarmContextEngine::FindRelatedCommands(
    const AlarmEvent& alarm_event, 
    const Timestamp& cutoff_time) {
    
    std::vector<UserCommandContext> related_commands;
    
    std::shared_lock<std::shared_mutex> lock(commands_mutex_);
    
    for (auto it = recent_commands_.rbegin(); it != recent_commands_.rend(); ++it) {
        if (it->executed_at < cutoff_time) {
            break; // 시간 윈도우를 벗어남
        }
        
        // 디바이스 관련성 확인
        if (it->device_id == alarm_event.device_id || 
            IsDeviceDependent(it->device_id, alarm_event.device_id)) {
            
            // 포인트 관련성 확인
            if (IsPointRelated(it->control_point_id, alarm_event.point_id)) {
                related_commands.push_back(*it);
            }
        }
    }
    
    return related_commands;
}

std::optional<UserCommandContext> AlarmContextEngine::SelectBestRelatedCommand(
    const std::vector<UserCommandContext>& related_commands,
    const AlarmEvent& alarm_event) {
    
    if (related_commands.empty()) {
        return std::nullopt;
    }
    
    // 가장 최근이면서 관련성이 높은 명령 선택
    auto best_command = std::max_element(related_commands.begin(), related_commands.end(),
        [&alarm_event](const UserCommandContext& a, const UserCommandContext& b) {
            // 시간 점수 (최근일수록 높음)
            auto time_diff_a = duration_cast<milliseconds>(alarm_event.triggered_at - a.executed_at).count();
            auto time_diff_b = duration_cast<milliseconds>(alarm_event.triggered_at - b.executed_at).count();
            double time_score_a = 1.0 / (1.0 + time_diff_a / 1000.0);
            double time_score_b = 1.0 / (1.0 + time_diff_b / 1000.0);
            
            // 관련성 점수 (직접 제어한 포인트일수록 높음)
            double relation_score_a = (a.control_point_id == alarm_event.point_id) ? 1.0 : 0.5;
            double relation_score_b = (b.control_point_id == alarm_event.point_id) ? 1.0 : 0.5;
            
            double total_score_a = time_score_a * relation_score_a;
            double total_score_b = time_score_b * relation_score_b;
            
            return total_score_a < total_score_b;
        });
    
    return *best_command;
}

double AlarmContextEngine::CalculateCorrelationConfidence(const AlarmContext& context, 
                                                         const AlarmEvent& alarm_event) {
    double confidence = 0.0;
    
    try {
        if (!context.related_command) {
            return 0.0;
        }
        
        const auto& command = *context.related_command;
        
        // 시간 기반 신뢰도 (최근일수록 높음, 최대 30초)
        auto time_diff = duration_cast<milliseconds>(alarm_event.triggered_at - command.executed_at).count();
        double time_factor = std::max(0.0, 1.0 - (time_diff / 30000.0));
        
        // 포인트 관련성 기반 신뢰도
        double relation_factor = 0.5; // 기본값
        if (command.control_point_id == alarm_event.point_id) {
            relation_factor = 1.0; // 직접 제어한 포인트
        } else if (IsDirectlyDependent(command.control_point_id, alarm_event.point_id)) {
            relation_factor = 0.8; // 직접 의존성
        } else if (IsIndirectlyDependent(command.control_point_id, alarm_event.point_id)) {
            relation_factor = 0.6; // 간접 의존성
        }
        
        // 명령 성공 여부 기반 신뢰도
        double success_factor = command.was_successful ? 1.0 : 0.7;
        
        // 종합 신뢰도 계산
        confidence = time_factor * relation_factor * success_factor;
        
        // 의존성 규칙 기반 추가 보정
        confidence *= GetDependencyInfluenceFactor(command.control_point_id, alarm_event.point_id);
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to calculate correlation confidence: " + std::string(e.what()));
    }
    
    return std::min(1.0, std::max(0.0, confidence));
}

void AlarmContextEngine::GenerateAnalysisNotes(AlarmContext& context, const AlarmEvent& alarm_event) {
    try {
        if (context.related_command) {
            const auto& command = *context.related_command;
            
            std::stringstream note;
            note << "Potential correlation with user command '" << command.command_type 
                 << "' executed by " << command.user_name;
            
            auto time_diff = duration_cast<seconds>(alarm_event.triggered_at - command.executed_at).count();
            note << " " << time_diff << " seconds before alarm";
            
            if (!command.reason.empty()) {
                note << " (Reason: " << command.reason << ")";
            }
            
            context.analysis_notes.push_back(note.str());
        }
        
        if (context.correlation_confidence > 0.7) {
            context.analysis_notes.push_back("High correlation confidence - likely user-triggered");
        } else if (context.correlation_confidence > 0.3) {
            context.analysis_notes.push_back("Medium correlation confidence - possibly user-triggered");
        } else {
            context.analysis_notes.push_back("Low correlation confidence - likely automatic trigger");
        }
        
        // 의존성 정보 추가
        if (context.related_command) {
            auto dependent_points = GetDependentPoints(context.related_command->control_point_id);
            if (!dependent_points.empty()) {
                context.analysis_notes.push_back("Control point affects " + 
                    std::to_string(dependent_points.size()) + " dependent points");
            }
        }
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to generate analysis notes: " + std::string(e.what()));
    }
}

void AlarmContextEngine::AddToAlarmHistory(const EnhancedAlarmEvent& alarm) {
    std::unique_lock<std::shared_mutex> lock(alarms_mutex_);
    
    recent_alarms_.push_back(alarm);
    
    // 히스토리 크기 제한
    while (recent_alarms_.size() > static_cast<size_t>(max_alarm_history_size_)) {
        recent_alarms_.pop_front();
    }
}

void AlarmContextEngine::PublishCommandEvent(const UserCommandContext& command) {
    if (!redis_client_) return;
    
    try {
        json command_json;
        command_json["event_type"] = "user_command";
        command_json["command_id"] = command.command_id;
        command_json["user_id"] = command.user_id;
        command_json["user_name"] = command.user_name;
        command_json["device_id"] = command.device_id.to_string();
        command_json["control_point_id"] = command.control_point_id;
        command_json["command_type"] = command.command_type;
        command_json["target_value"] = command.target_value;
        command_json["previous_value"] = command.previous_value;
        command_json["reason"] = command.reason;
        command_json["executed_at"] = duration_cast<milliseconds>(command.executed_at.time_since_epoch()).count();
        command_json["was_successful"] = command.was_successful;
        
        std::string channel = "alarm_context:commands";
        redis_client_->Publish(channel, command_json.dump());
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to publish command event: " + std::string(e.what()));
    }
}

void AlarmContextEngine::PublishEnhancedAlarm(const EnhancedAlarmEvent& alarm) {
    if (!redis_client_) return;
    
    try {
        json alarm_json;
        alarm_json["event_type"] = "enhanced_alarm";
        alarm_json["alarm_id"] = alarm.alarm_id;
        alarm_json["device_id"] = alarm.device_id.to_string();
        alarm_json["device_name"] = alarm.device_name;
        alarm_json["point_id"] = alarm.point_id;
        alarm_json["point_name"] = alarm.point_name;
        alarm_json["message"] = alarm.message;
        alarm_json["severity"] = static_cast<int>(alarm.severity);
        alarm_json["trigger_value"] = alarm.trigger_value;
        alarm_json["threshold_value"] = alarm.threshold_value;
        alarm_json["unit"] = alarm.unit;
        alarm_json["triggered_at"] = duration_cast<milliseconds>(alarm.triggered_at.time_since_epoch()).count();
        alarm_json["is_active"] = alarm.is_active;
        
        // 컨텍스트 정보
        alarm_json["context"]["trigger_type"] = static_cast<int>(alarm.context.trigger_type);
        alarm_json["context"]["is_user_triggered"] = alarm.context.is_user_triggered;
        alarm_json["context"]["correlation_confidence"] = alarm.context.correlation_confidence;
        alarm_json["context"]["correlation_window_ms"] = alarm.context.correlation_window_ms;
        alarm_json["context"]["analysis_notes"] = alarm.context.analysis_notes;
        
        if (alarm.context.related_command) {
            const auto& cmd = *alarm.context.related_command;
            alarm_json["context"]["related_command"]["command_id"] = cmd.command_id;
            alarm_json["context"]["related_command"]["user_name"] = cmd.user_name;
            alarm_json["context"]["related_command"]["command_type"] = cmd.command_type;
            alarm_json["context"]["related_command"]["executed_at"] = 
                duration_cast<milliseconds>(cmd.executed_at.time_since_epoch()).count();
        }
        
        std::string channel = "alarm_context:enhanced_alarms";
        redis_client_->Publish(channel, alarm_json.dump());
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to publish enhanced alarm: " + std::string(e.what()));
    }
}

// =============================================================================
// 스레드 함수들
// =============================================================================

void AlarmContextEngine::CleanupThreadMain() {
    logger_.Info("Cleanup thread started");
    
    while (running_.load()) {
        try {
            std::this_thread::sleep_for(seconds(cleanup_interval_seconds_));
            
            if (!running_.load()) break;
            
            PerformCleanup();
            
        } catch (const std::exception& e) {
            logger_.Error("Exception in cleanup thread: " + std::string(e.what()));
        }
    }
    
    logger_.Info("Cleanup thread stopped");
}

void AlarmContextEngine::AnalysisThreadMain() {
    logger_.Info("Analysis thread started");
    
    while (running_.load()) {
        try {
            std::this_thread::sleep_for(seconds(60)); // 1분마다 실행
            
            if (!running_.load()) break;
            
            PerformPeriodicAnalysis();
            
        } catch (const std::exception& e) {
            logger_.Error("Exception in analysis thread: " + std::string(e.what()));
        }
    }
    
    logger_.Info("Analysis thread stopped");
}

void AlarmContextEngine::PerformCleanup() {
    auto now = system_clock::now();
    auto cutoff_time = now - hours(24); // 24시간 이전 데이터 정리
    
    // 오래된 명령 정리
    {
        std::unique_lock<std::shared_mutex> lock(commands_mutex_);
        auto it = std::remove_if(recent_commands_.begin(), recent_commands_.end(),
            [cutoff_time](const UserCommandContext& cmd) {
                return cmd.executed_at < cutoff_time;
            });
        recent_commands_.erase(it, recent_commands_.end());
    }
    
    // 오래된 알람 정리
    {
        std::unique_lock<std::shared_mutex> lock(alarms_mutex_);
        auto it = std::remove_if(recent_alarms_.begin(), recent_alarms_.end(),
            [cutoff_time](const EnhancedAlarmEvent& alarm) {
                return alarm.triggered_at < cutoff_time;
            });
        recent_alarms_.erase(it, recent_alarms_.end());
    }
    
    logger_.Debug("Cleanup completed - Commands: " + std::to_string(recent_commands_.size()) + 
                 ", Alarms: " + std::to_string(recent_alarms_.size()));
}

void AlarmContextEngine::PerformPeriodicAnalysis() {
    // 주기적 분석 작업 (예: 패턴 분석, 통계 업데이트 등)
    logger_.Debug("Periodic analysis - Total alarms: " + std::to_string(total_alarms_analyzed_.load()) +
                 ", User triggered: " + std::to_string(user_triggered_alarms_.load()) +
                 ", Automatic: " + std::to_string(automatic_alarms_.load()));
}

// =============================================================================
// 의존성 관련 유틸리티 함수들
// =============================================================================

void AlarmContextEngine::LoadDefaultDependencyRules() {
    // 예시 의존성 규칙들
    dependency_rules_ = {
        {"rule_1", "pump_control", "flow_rate", "direct", 1000, 0.9, "Pump affects flow rate", true},
        {"rule_2", "valve_position", "pressure", "direct", 2000, 0.8, "Valve affects pressure", true},
        {"rule_3", "setpoint_temp", "actual_temp", "delayed", 5000, 0.7, "Temperature setpoint affects actual", true},
        {"rule_4", "flow_rate", "level", "indirect", 3000, 0.6, "Flow rate affects tank level", true}
    };
}

void AlarmContextEngine::RebuildDependencyMap() {
    point_dependencies_.clear();
    
    for (const auto& rule : dependency_rules_) {
        if (rule.is_enabled) {
            point_dependencies_[rule.source_point_id].push_back(rule.target_point_id);
        }
    }
}

bool AlarmContextEngine::IsPointRelated(const std::string& control_point, const std::string& alarm_point) {
    if (control_point == alarm_point) {
        return true; // 직접 관련
    }
    
    return IsDirectlyDependent(control_point, alarm_point) || 
           IsIndirectlyDependent(control_point, alarm_point);
}

bool AlarmContextEngine::IsDirectlyDependent(const std::string& source_point, const std::string& target_point) {
    std::shared_lock<std::shared_mutex> lock(dependencies_mutex_);
    
    auto it = point_dependencies_.find(source_point);
    if (it != point_dependencies_.end()) {
        return std::find(it->second.begin(), it->second.end(), target_point) != it->second.end();
    }
    
    return false;
}

bool AlarmContextEngine::IsIndirectlyDependent(const std::string& source_point, const std::string& target_point) {
    // 간접 의존성 확인 (최대 2단계까지)
    std::shared_lock<std::shared_mutex> lock(dependencies_mutex_);
    
    auto it = point_dependencies_.find(source_point);
    if (it != point_dependencies_.end()) {
        for (const auto& intermediate_point : it->second) {
            if (IsDirectlyDependent(intermediate_point, target_point)) {
                return true;
            }
        }
    }
    
    return false;
}

std::vector<std::string> AlarmContextEngine::GetDependentPoints(const std::string& source_point) {
    std::shared_lock<std::shared_mutex> lock(dependencies_mutex_);
    
    auto it = point_dependencies_.find(source_point);
    if (it != point_dependencies_.end()) {
        return it->second;
    }
    
    return {};
}

double AlarmContextEngine::GetDependencyInfluenceFactor(const std::string& source_point, const std::string& target_point) {
    std::shared_lock<std::shared_mutex> lock(dependencies_mutex_);
    
    auto it = std::find_if(dependency_rules_.begin(), dependency_rules_.end(),
        [&](const DependencyRule& rule) {
            return rule.source_point_id == source_point && rule.target_point_id == target_point;
        });
    
    if (it != dependency_rules_.end()) {
        return it->influence_factor;
    }
    
    return 1.0; // 기본값
}

bool AlarmContextEngine::IsDeviceDependent(const UUID& source_device, const UUID& target_device) {
    // 디바이스 간 의존성 확인 (실제 구현에서는 디바이스 토폴로지 정보 필요)
    return false; // 간단 구현
}

void AlarmContextEngine::AnalyzeBatchCorrelations(std::vector<EnhancedAlarmEvent>& enhanced_alarms) {
    // 배치 내 알람들 간의 상관관계 분석 (알람 폭풍 감지 등)
    if (enhanced_alarms.size() < 2) return;
    
    // 시간순 정렬
    std::sort(enhanced_alarms.begin(), enhanced_alarms.end(),
        [](const EnhancedAlarmEvent& a, const EnhancedAlarmEvent& b) {
            return a.triggered_at < b.triggered_at;
        });
    
    // 연속된 알람들 간의 관계 분석
    for (size_t i = 1; i < enhanced_alarms.size(); ++i) {
        auto time_diff = duration_cast<milliseconds>(
            enhanced_alarms[i].triggered_at - enhanced_alarms[i-1].triggered_at).count();
        
        if (time_diff < 5000) { // 5초 이내
            enhanced_alarms[i].context.analysis_notes.push_back(
                "Part of potential alarm storm - " + std::to_string(time_diff) + "ms after previous alarm");
        }
    }
}

} // namespace Engine
} // namespace PulseOne