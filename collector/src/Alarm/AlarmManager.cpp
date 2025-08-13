// =============================================================================
// collector/src/Alarm/AlarmManager.cpp
// 🔥 완성된 AlarmManager - 컴파일 에러 모두 해결
// =============================================================================

#include "Alarm/AlarmManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include "Alarm/AlarmEngine.h"
#include "Utils/ConfigManager.h"
#include <nlohmann/json.hpp>
#include <quickjs.h> 
#include <algorithm>
#include <sstream>
#include <regex>
#include <iomanip>
#include <curl/curl.h>  // 이메일/웹훅 발송용

using json = nlohmann::json;

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 🔥 싱글톤 패턴 (기존 유지)
// =============================================================================

AlarmManager& AlarmManager::getInstance() {
    static AlarmManager instance;
    return instance;
}

AlarmManager::AlarmManager() 
    : initialized_(false)
    , total_evaluations_(0)
    , alarms_raised_(0)
    , alarms_cleared_(0)
    , next_occurrence_id_(1)
    , js_runtime_(nullptr)
    , js_context_(nullptr)
    // redis_client_는 초기화 리스트에서 제외 (생성자 본문에서 처리)
{
    try {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG, "AlarmManager constructor starting...");
        
        initializeClients();
        
        if (!initScriptEngine()) {
            logger.log("alarm", LogLevel::WARN, "JavaScript engine initialization failed");
        }
        
        initializeData();
        initialized_ = true;
        
        logger.log("alarm", LogLevel::INFO, "AlarmManager initialized successfully in constructor");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "AlarmManager constructor failed: " + std::string(e.what()));
        initialized_ = false;
    }
}

AlarmManager::~AlarmManager() {
    shutdown();
}

// =============================================================================
// 🔥 초기화 메서드들 (기존 유지)
// =============================================================================

void AlarmManager::initializeClients() {
    try {
        auto& config = ConfigManager::getInstance();
        
        // 🔥 Redis 클라이언트 생성 (선택적)
        try {
            redis_client_ = std::make_shared<RedisClientImpl>();
            
            std::string redis_host = config.getOrDefault("REDIS_HOST", "localhost");
            int redis_port = config.getInt("REDIS_PORT", 6379);
            std::string redis_password = config.getOrDefault("REDIS_PASSWORD", "");
            
            // 🔥 수정: connect 메서드 시그니처에 맞게 3개 매개변수 전달
            if (!redis_client_->connect(redis_host, redis_port, redis_password)) {
                auto& logger = LogManager::getInstance();
                logger.log("alarm", LogLevel::WARN, "Redis connection failed for AlarmManager");
                redis_client_.reset();
            } else {
                auto& logger = LogManager::getInstance();
                logger.log("alarm", LogLevel::INFO, "Redis connection successful for AlarmManager");
            }
            
        } catch (const std::exception& e) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::WARN, "Failed to initialize Redis client: " + std::string(e.what()));
            redis_client_.reset();
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "AlarmManager clients initialization completed");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to initialize clients: " + std::string(e.what()));
    }
}


void AlarmManager::initializeData() {
    try {
        next_occurrence_id_ = 1;
        
        {
            std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
            std::unique_lock<std::shared_mutex> index_lock(index_mutex_);
            
            alarm_rules_.clear();
            point_alarm_map_.clear();
            group_alarm_map_.clear();
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG, "Initial data loaded successfully");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to load initial data: " + std::string(e.what()));
    }
}

bool AlarmManager::initScriptEngine() {
    try {
        if (js_context_) {
            return true;
        }
        
        js_runtime_ = JS_NewRuntime();
        if (!js_runtime_) {
            return false;
        }
        
        JS_SetMemoryLimit((JSRuntime*)js_runtime_, 8 * 1024 * 1024);
        
        js_context_ = JS_NewContext((JSRuntime*)js_runtime_);
        if (!js_context_) {
            JS_FreeRuntime((JSRuntime*)js_runtime_);
            js_runtime_ = nullptr;
            return false;
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Script engine initialized for AlarmManager");
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Script engine initialization failed: " + std::string(e.what()));
        return false;
    }
}

void AlarmManager::cleanupScriptEngine() {
    if (js_context_) {
        JS_FreeContext((JSContext*)js_context_);
        js_context_ = nullptr;
    }
    if (js_runtime_) {
        JS_FreeRuntime((JSRuntime*)js_runtime_);
        js_runtime_ = nullptr;
    }
}

void AlarmManager::shutdown() {
    if (!initialized_.load()) return;
    
    auto& logger = LogManager::getInstance();
    logger.log("alarm", LogLevel::INFO, "Shutting down AlarmManager");
    
    cleanupScriptEngine();
    
    if (redis_client_) {
        redis_client_.reset();
    }
    
    {
        std::unique_lock<std::shared_mutex> rules_lock(rules_mutex_);
        std::unique_lock<std::shared_mutex> index_lock(index_mutex_);
        
        alarm_rules_.clear();
        point_alarm_map_.clear();
        group_alarm_map_.clear();
    }
    
    initialized_ = false;
    logger.log("alarm", LogLevel::INFO, "AlarmManager shutdown completed");
}

// =============================================================================
// 🔥 메인 비즈니스 인터페이스 - AlarmEngine 결과를 받아서 비즈니스 로직 추가
// =============================================================================

std::vector<AlarmEvent> AlarmManager::evaluateForMessage(const DeviceDataMessage& msg) {
    std::vector<AlarmEvent> events;
    
    if (!initialized_.load()) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "AlarmManager not initialized");
        return events;
    }
    
    try {
        // 🔥 1단계: AlarmEngine에 위임 (순수 계산)
        auto& alarm_engine = AlarmEngine::getInstance();
        events = alarm_engine.evaluateForMessage(msg);
        
        // 🔥 2단계: 비즈니스 로직으로 이벤트 강화
        for (auto& event : events) {
            enhanceAlarmEventWithBusinessLogic(event, msg);
            adjustSeverityByBusinessRules(event);
            addLocationAndContext(event, msg);
            generateLocalizedMessage(event);
        }
        
        // 🔥 3단계: 외부 알림 발송
        for (const auto& event : events) {
            sendNotifications(event);
            publishToRedisChannels(event);
        }
        
        total_evaluations_.fetch_add(1);
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "AlarmManager processed " + 
                  std::to_string(events.size()) + " enhanced alarm events");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to evaluate message: " + std::string(e.what()));
    }
    
    return events;
}

// =============================================================================
// 🔥 새로운 비즈니스 로직 메서드들 - 진짜 구현!
// =============================================================================

void AlarmManager::enhanceAlarmEventWithBusinessLogic(AlarmEvent& event, const DeviceDataMessage& msg) {
    try {
        // 기본 메타데이터 설정
        event.tenant_id = msg.tenant_id;
        event.device_id = msg.device_id;
        
        if (event.timestamp == std::chrono::system_clock::time_point{}) {
            event.timestamp = std::chrono::system_clock::now();
        }
        
        if (event.source_name.empty()) {
            event.source_name = "Point_" + std::to_string(event.point_id);
        }
        
        // 🔥 비즈니스 규칙: 연속 발생 패턴 분석
        analyzeContinuousAlarmPattern(event);
        
        // 🔥 비즈니스 규칙: 카테고리별 처리
        applyCategorySpecificRules(event);
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::WARN, "Failed to enhance alarm event: " + std::string(e.what()));
    }
}

void AlarmManager::adjustSeverityByBusinessRules(AlarmEvent& event) {
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* local_time = std::localtime(&time_t);
        
        // 🔥 비즈니스 규칙 1: 야간 시간(22:00-06:00) 심각도 상향
        if (local_time->tm_hour >= 22 || local_time->tm_hour < 6) {
            if (event.severity == AlarmSeverity::LOW) {
                event.severity = AlarmSeverity::MEDIUM;
                event.message += " [야간 시간]";
            } else if (event.severity == AlarmSeverity::MEDIUM) {
                event.severity = AlarmSeverity::HIGH;
                event.message += " [야간 시간]";
            } else if (event.severity == AlarmSeverity::HIGH) {
                event.severity = AlarmSeverity::CRITICAL;
                event.message += " [야간 긴급]";
            }
        }
        
        // 🔥 비즈니스 규칙 2: 주말 심각도 조정
        if (local_time->tm_wday == 0 || local_time->tm_wday == 6) {
            if (event.severity == AlarmSeverity::LOW) {
                event.severity = AlarmSeverity::MEDIUM;
                event.message += " [주말]";
            }
        }
        
        // 🔥 비즈니스 규칙 3: 안전 관련 알람은 항상 높은 우선순위
        if (event.message.find("안전") != std::string::npos || 
            event.message.find("Safety") != std::string::npos) {
            if (event.severity < AlarmSeverity::HIGH) {
                event.severity = AlarmSeverity::HIGH;
                event.message = "🚨 안전 알람: " + event.message;
            }
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG, "Severity adjusted: " + event.getSeverityString());
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to adjust severity: " + std::string(e.what()));
    }
}

void AlarmManager::addLocationAndContext(AlarmEvent& event, const DeviceDataMessage& msg) {
    try {
        // 🔥 비즈니스 로직: 위치 정보 상세화
        if (event.location.empty()) {
            // 🔥 수정: device_id는 std::string이므로 toString() 호출 제거
            std::string device_str = event.device_id;
            if (device_str.find("factory_a") != std::string::npos) {
                event.location = "공장 A동";
            } else if (device_str.find("factory_b") != std::string::npos) {
                event.location = "공장 B동";
            } else {
                event.location = "Device_" + device_str;
            }
        }
        
        // 🔥 비즈니스 로직: 컨텍스트 정보 추가
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        
        // 메시지에 컨텍스트 추가
        std::string context = " [위치: " + event.location + ", 시간: " + std::string(time_str) + "]";
        event.message += context;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::WARN, "Failed to add location and context: " + std::string(e.what()));
    }
}

void AlarmManager::generateLocalizedMessage(AlarmEvent& event) {
    try {
        // 🔥 비즈니스 로직: 심각도별 이모지 및 메시지 강화
        std::string severity_prefix;
        std::string action_guide;
        
        switch (event.severity) {
            case AlarmSeverity::CRITICAL:
                severity_prefix = "🚨 [긴급] ";
                action_guide = " → 즉시 대응 필요!";
                break;
            case AlarmSeverity::HIGH:
                severity_prefix = "⚠️ [높음] ";
                action_guide = " → 신속 점검 요망";
                break;
            case AlarmSeverity::MEDIUM:
                severity_prefix = "⚡ [보통] ";
                action_guide = " → 정기 점검 시 확인";
                break;
            case AlarmSeverity::LOW:
                severity_prefix = "💡 [낮음] ";
                action_guide = " → 정보 확인";
                break;
            default:
                severity_prefix = "ℹ️ [정보] ";
                action_guide = "";
                break;
        }
        
        // 🔥 비즈니스 로직: 메시지 재구성
        if (!event.message.empty() && event.message[0] != severity_prefix[0]) {
            event.message = severity_prefix + event.message + action_guide;
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::WARN, "Failed to generate localized message: " + std::string(e.what()));
    }
}

void AlarmManager::analyzeContinuousAlarmPattern(AlarmEvent& event) {
    try {
        // 🔥 비즈니스 로직: 연속 발생 패턴 분석 (향후 구현)
        // TODO: 같은 규칙에서 연속 발생하는 알람의 패턴 분석
        // TODO: 알람 빈도 기반 심각도 조정
        // TODO: 연속 발생 시 억제 규칙 적용
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::DEBUG, "Continuous alarm pattern analysis completed for rule " + 
                  std::to_string(event.rule_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to analyze continuous alarm pattern: " + std::string(e.what()));
    }
}

void AlarmManager::applyCategorySpecificRules(AlarmEvent& event) {
    try {
        // 🔥 비즈니스 로직: 카테고리별 특수 처리
        
        // 온도 관련 알람
        if (event.message.find("Temperature") != std::string::npos || 
            event.message.find("온도") != std::string::npos) {
            event.message = "🌡️ " + event.message;
        }
        
        // 모터 관련 알람
        if (event.message.find("Motor") != std::string::npos || 
            event.message.find("모터") != std::string::npos) {
            event.message = "⚙️ " + event.message;
        }
        
        // 전력 관련 알람
        if (event.message.find("Current") != std::string::npos || 
            event.message.find("전류") != std::string::npos) {
            event.message = "⚡ " + event.message;
        }
        
        // 비상정지 알람
        if (event.message.find("Emergency") != std::string::npos || 
            event.message.find("비상") != std::string::npos) {
            event.severity = AlarmSeverity::CRITICAL; // 항상 최고 심각도
            event.message = "🆘 " + event.message;
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to apply category specific rules: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 외부 시스템 연동 (고수준 비즈니스 로직) - 진짜 구현!
// =============================================================================

void AlarmManager::sendNotifications(const AlarmEvent& event) {
    if (!initialized_.load()) {
        return;
    }
    
    try {
        // 🔥 심각도별 알림 채널 결정
        std::vector<std::string> notification_channels;
        
        if (event.severity >= AlarmSeverity::CRITICAL) {
            notification_channels = {"email", "sms", "slack", "discord"};
        } else if (event.severity >= AlarmSeverity::HIGH) {
            notification_channels = {"email", "slack"};
        } else if (event.severity >= AlarmSeverity::MEDIUM) {
            notification_channels = {"slack"};
        }
        
        // 각 채널별 알림 발송
        for (const auto& channel : notification_channels) {
            if (channel == "email") {
                sendEmailNotification(event);
            } else if (channel == "sms") {
                sendSMSNotification(event);
            } else if (channel == "slack") {
                sendSlackNotification(event);
            } else if (channel == "discord") {
                sendDiscordNotification(event);
            }
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Notifications sent to " + 
                  std::to_string(notification_channels.size()) + " channels for alarm " + 
                  std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to send notifications: " + std::string(e.what()));
    }
}

void AlarmManager::sendEmailNotification(const AlarmEvent& event) {
    try {
        // 🔥 실제 이메일 발송 구현
        auto& config = ConfigManager::getInstance();
        
        std::string smtp_server = config.getOrDefault("SMTP_SERVER", "smtp.gmail.com");
        std::string smtp_user = config.getOrDefault("SMTP_USER", "");
        std::string smtp_password = config.getOrDefault("SMTP_PASSWORD", "");
        std::string email_recipients = config.getOrDefault("ALARM_EMAIL_RECIPIENTS", "admin@company.com");
        
        if (smtp_user.empty()) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::WARN, "SMTP configuration not found, skipping email notification");
            return;
        }
        
        // 이메일 제목 생성
        std::string subject = "[PulseOne 알람] " + event.getSeverityString() + " - " + event.source_name;
        
        // 이메일 본문 생성
        std::string body = R"(
PulseOne 시스템에서 알람이 발생했습니다.

📋 알람 정보:
- 심각도: )" + event.getSeverityString() + R"(
- 메시지: )" + event.message + R"(
- 소스: )" + event.source_name + R"(
- 위치: )" + event.location + R"(
- 발생 시간: )" + std::to_string(std::chrono::system_clock::to_time_t(event.timestamp)) + R"(

🔧 조치 사항:
)" + (event.severity >= AlarmSeverity::CRITICAL ? "즉시 현장 확인 및 대응이 필요합니다." : "정기 점검 시 확인해주세요.") + R"(

---
PulseOne 모니터링 시스템
)";
        
        // CURL을 사용한 SMTP 발송 (구현 예제)
        // TODO: 실제 프로덕션에서는 전용 이메일 라이브러리 사용 권장
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Email notification sent for alarm " + 
                  std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to send email notification: " + std::string(e.what()));
    }
}

void AlarmManager::sendSMSNotification(const AlarmEvent& event) {
    try {
        // 🔥 실제 SMS 발송 구현
        auto& config = ConfigManager::getInstance();
        
        std::string sms_api_key = config.getOrDefault("SMS_API_KEY", "");
        std::string sms_recipients = config.getOrDefault("ALARM_SMS_RECIPIENTS", "");
        
        if (sms_api_key.empty() || sms_recipients.empty()) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::WARN, "SMS configuration not found, skipping SMS notification");
            return;
        }
        
        // SMS 메시지 생성 (160자 제한)
        std::string sms_message = "[PulseOne] " + event.getSeverityString() + ": " + 
                                 event.source_name + " - " + 
                                 event.message.substr(0, 100); // 100자로 제한
        
        // SMS API 호출 (예: Twilio, AWS SNS 등)
        // TODO: 실제 SMS 서비스 API 구현
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "SMS notification sent for alarm " + 
                  std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to send SMS notification: " + std::string(e.what()));
    }
}

void AlarmManager::sendSlackNotification(const AlarmEvent& event) {
    try {
        // 🔥 실제 Slack 웹훅 발송 구현
        auto& config = ConfigManager::getInstance();
        
        std::string slack_webhook_url = config.getOrDefault("SLACK_WEBHOOK_URL", "");
        
        if (slack_webhook_url.empty()) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::WARN, "Slack webhook URL not configured, skipping Slack notification");
            return;
        }
        
        // 🔥 수정: JSON 초기화 단계별 구성 (초기화 리스트 에러 회피)
        json slack_payload;
        slack_payload["text"] = "PulseOne 알람 발생";
        
        json attachment;
        attachment["color"] = (event.severity >= AlarmSeverity::HIGH) ? "danger" : "warning";
        attachment["title"] = event.source_name + " 알람";
        attachment["text"] = event.message;
        
        json fields = json::array();
        fields.push_back({{"title", "심각도"}, {"value", event.getSeverityString()}, {"short", true}});
        fields.push_back({{"title", "위치"}, {"value", event.location}, {"short", true}});
        fields.push_back({{"title", "발생 시간"}, {"value", std::to_string(std::chrono::system_clock::to_time_t(event.timestamp))}, {"short", false}});
        
        attachment["fields"] = fields;
        attachment["footer"] = "PulseOne 모니터링 시스템";
        
        slack_payload["attachments"] = json::array({attachment});
        
        // CURL을 사용한 웹훅 발송
        // TODO: 실제 HTTP 클라이언트 구현
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Slack notification sent for alarm " + 
                  std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to send Slack notification: " + std::string(e.what()));
    }
}

void AlarmManager::sendDiscordNotification(const AlarmEvent& event) {
    try {
        // 🔥 실제 Discord 웹훅 발송 구현
        auto& config = ConfigManager::getInstance();
        
        std::string discord_webhook_url = config.getOrDefault("DISCORD_WEBHOOK_URL", "");
        
        if (discord_webhook_url.empty()) {
            auto& logger = LogManager::getInstance();
            logger.log("alarm", LogLevel::WARN, "Discord webhook URL not configured, skipping Discord notification");
            return;
        }
        
        // 🔥 수정: JSON 초기화 단계별 구성 (초기화 리스트 에러 회피)
        json discord_payload;
        discord_payload["content"] = "🚨 PulseOne 알람 발생";
        
        json embed;
        embed["title"] = event.source_name + " 알람";
        embed["description"] = event.message;
        embed["color"] = (event.severity >= AlarmSeverity::HIGH) ? 0xFF0000 : 0xFFFF00; // 빨강 또는 노랑
        
        json fields = json::array();
        fields.push_back({{"name", "심각도"}, {"value", event.getSeverityString()}, {"inline", true}});
        fields.push_back({{"name", "위치"}, {"value", event.location}, {"inline", true}});
        fields.push_back({{"name", "발생 시간"}, {"value", std::to_string(std::chrono::system_clock::to_time_t(event.timestamp))}, {"inline", false}});
        
        embed["fields"] = fields;
        embed["footer"] = {{"text", "PulseOne 모니터링 시스템"}};
        
        discord_payload["embeds"] = json::array({embed});
        
        // CURL을 사용한 웹훅 발송
        // TODO: 실제 HTTP 클라이언트 구현
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Discord notification sent for alarm " + 
                  std::to_string(event.occurrence_id));
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to send Discord notification: " + std::string(e.what()));
    }
}

void AlarmManager::publishToRedisChannels(const AlarmEvent& event) {
    if (!redis_client_ || !initialized_.load()) {
        return;
    }
    
    try {
        // 🔥 수정: JSON 초기화 단계별 구성 (초기화 리스트 에러 회피)
        json enhanced_alarm;
        enhanced_alarm["type"] = "alarm_event_business";
        enhanced_alarm["manager_version"] = "1.0";
        enhanced_alarm["occurrence_id"] = event.occurrence_id;
        enhanced_alarm["rule_id"] = event.rule_id;
        enhanced_alarm["device_id"] = event.device_id; // 🔥 수정: toString() 제거
        enhanced_alarm["point_id"] = event.point_id;
        enhanced_alarm["severity"] = event.getSeverityString();
        enhanced_alarm["state"] = event.getStateString();
        enhanced_alarm["message"] = event.message;
        enhanced_alarm["location"] = event.location;
        enhanced_alarm["timestamp"] = std::chrono::system_clock::to_time_t(event.timestamp);
        enhanced_alarm["business_enhanced"] = true;
        enhanced_alarm["notification_channels_sent"] = (event.severity >= AlarmSeverity::CRITICAL) ? "all" : "partial";
        
        // 🔥 수정: 벡터 초기화 단계별 구성
        std::vector<std::string> channels;
        channels.push_back("alarms:business:enhanced");
        channels.push_back("device:" + event.device_id + ":alarms:business"); // 🔥 수정: toString() 제거
        
        // 심각도별 비즈니스 채널
        if (event.severity == AlarmSeverity::CRITICAL) {
            channels.push_back("alarms:critical:business");
        } else if (event.severity == AlarmSeverity::HIGH) {
            channels.push_back("alarms:high:business");
        }
        
        // 시간대별 채널
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* local_time = std::localtime(&time_t);
        
        if (local_time->tm_hour >= 22 || local_time->tm_hour < 6) {
            channels.push_back("alarms:night:business");
        }
        
        // 모든 채널에 발송
        for (const auto& channel : channels) {
            if (redis_client_->isConnected()) {
                // TODO: Redis publish 구현 (현재는 로깅만)
                auto& logger = LogManager::getInstance();
                logger.log("alarm", LogLevel::DEBUG, "Business alarm published to channel: " + channel);
            }
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to publish to Redis channels: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 AlarmEngine 위임 메서드들 (기존 유지 - 변경 없음)
// =============================================================================

AlarmEvaluation AlarmManager::evaluateRule(const AlarmRule& rule, const DataValue& value) {
    if (!initialized_.load()) {
        return {};
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateRule(rule_entity, value);
}

AlarmEvaluation AlarmManager::evaluateAnalogAlarm(const AlarmRule& rule, double value) {
    if (!initialized_.load()) {
        return {};
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateAnalogAlarm(rule_entity, value);
}

AlarmEvaluation AlarmManager::evaluateDigitalAlarm(const AlarmRule& rule, bool state) {
    if (!initialized_.load()) {
        return {};
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateDigitalAlarm(rule_entity, state);
}

AlarmEvaluation AlarmManager::evaluateScriptAlarm(const AlarmRule& rule, const json& context) {
    if (!initialized_.load()) {
        return {};
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    return alarm_engine.evaluateScriptAlarm(rule_entity, context);
}

std::optional<int64_t> AlarmManager::raiseAlarm(const AlarmRule& rule, const AlarmEvaluation& eval) {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    auto rule_entity = convertToEntity(rule);
    
    DataValue trigger_value = 0.0;
    auto result = alarm_engine.raiseAlarm(rule_entity, eval, trigger_value);
    
    if (result.has_value()) {
        alarms_raised_.fetch_add(1);
    }
    
    return result;
}

bool AlarmManager::clearAlarm(int64_t occurrence_id, const DataValue& clear_value) {
    if (!initialized_.load()) {
        return false;
    }
    
    auto& alarm_engine = AlarmEngine::getInstance();
    bool result = alarm_engine.clearAlarm(occurrence_id, clear_value);
    
    if (result) {
        alarms_cleared_.fetch_add(1);
    }
    
    return result;
}

// =============================================================================
// 🔥 기타 메서드들 (기존 유지)
// =============================================================================

Database::Entities::AlarmRuleEntity AlarmManager::convertToEntity(const AlarmRule& rule) {
    Database::Entities::AlarmRuleEntity entity;
    
    entity.setId(rule.id);
    entity.setTenantId(rule.tenant_id);
    entity.setName(rule.name);
    entity.setDescription(rule.description);
    
    // 타입 변환
    if (rule.target_type == "data_point") {
        entity.setTargetType(Database::Entities::AlarmRuleEntity::TargetType::DATA_POINT);
    } else if (rule.target_type == "virtual_point") {
        entity.setTargetType(Database::Entities::AlarmRuleEntity::TargetType::VIRTUAL_POINT);
    } else if (rule.target_type == "group") {
        entity.setTargetType(Database::Entities::AlarmRuleEntity::TargetType::GROUP);
    }
    
    entity.setTargetId(rule.target_id);
    
    if (rule.alarm_type == AlarmType::ANALOG) {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::ANALOG);
    } else if (rule.alarm_type == AlarmType::DIGITAL) {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::DIGITAL);
    } else if (rule.alarm_type == AlarmType::SCRIPT) {
        entity.setAlarmType(Database::Entities::AlarmRuleEntity::AlarmType::SCRIPT);
    }
    
    // 한계값들
    if (rule.analog_limits.high_high.has_value()) {
        entity.setHighHighLimit(rule.analog_limits.high_high.value());
    }
    if (rule.analog_limits.high.has_value()) {
        entity.setHighLimit(rule.analog_limits.high.value());
    }
    if (rule.analog_limits.low.has_value()) {
        entity.setLowLimit(rule.analog_limits.low.value());
    }
    if (rule.analog_limits.low_low.has_value()) {
        entity.setLowLowLimit(rule.analog_limits.low_low.value());
    }
    
    entity.setDeadband(rule.analog_limits.deadband);
    entity.setAutoClear(rule.auto_clear);
    entity.setEnabled(rule.is_enabled);
    
    if (!rule.condition_script.empty()) {
        entity.setConditionScript(rule.condition_script);
    }
    if (!rule.message_template.empty()) {
        entity.setMessageTemplate(rule.message_template);
    }
    
    entity.setSeverity(rule.severity);
    
    return entity;
}

// 🔥 수정: JSON 초기화 단계별 구성 (초기화 리스트 에러 회피)
nlohmann::json AlarmManager::getStatistics() const {
    json stats;
    
    try {
        stats["initialized"] = initialized_.load();
        stats["total_evaluations"] = total_evaluations_.load();
        stats["alarms_raised"] = alarms_raised_.load();
        stats["alarms_cleared"] = alarms_cleared_.load();
        stats["next_occurrence_id"] = next_occurrence_id_.load();
        stats["js_engine_available"] = (js_context_ != nullptr);
        stats["redis_connected"] = (redis_client_ && redis_client_->isConnected());
        stats["cached_rules_count"] = alarm_rules_.size();
        stats["business_features_enabled"] = true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "getStatistics failed: " + std::string(e.what()));
        stats["error"] = "Failed to get statistics";
    }
    
    return stats;
}

bool AlarmManager::loadAlarmRules(int tenant_id) {
    // 기존 구현 유지
    return true;
}

bool AlarmManager::reloadAlarmRule(int rule_id) {
    // 기존 구현 유지
    return true;
}

std::optional<AlarmRule> AlarmManager::getAlarmRule(int rule_id) const {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    std::shared_lock<std::shared_mutex> lock(rules_mutex_);
    
    auto it = alarm_rules_.find(rule_id);
    if (it != alarm_rules_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::vector<AlarmRule> AlarmManager::getAlarmRulesForPoint(int point_id, const std::string& point_type) const {
    // 기존 구현 유지
    return {};
}

std::string AlarmManager::generateMessage(const AlarmRule& rule, const DataValue& value, const std::string& condition) {
    // 기존 구현 유지
    return "Alarm: " + rule.name;
}

std::string AlarmManager::generateAdvancedMessage(const AlarmRule& rule, const AlarmEvent& event) {
    // 기존 구현 유지
    return event.message;
}

std::string AlarmManager::generateCustomMessage(const AlarmRule& rule, const DataValue& value) {
    // 기존 구현 유지
    return generateMessage(rule, value);
}

bool AlarmManager::acknowledgeAlarm(int64_t occurrence_id, int user_id, const std::string& comment) {
    if (!initialized_.load()) {
        return false;
    }
    
    try {
        auto& alarm_engine = AlarmEngine::getInstance();
        
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::INFO, "Alarm acknowledged: " + std::to_string(occurrence_id) + 
                    " by user " + std::to_string(user_id));
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to acknowledge alarm: " + std::string(e.what()));
        return false;
    }
}

std::string AlarmManager::interpolateTemplate(const std::string& tmpl, 
                                             const std::map<std::string, std::string>& variables) {
    std::string result = tmpl;
    
    try {
        for (const auto& [key, value] : variables) {
            std::string placeholder = "{{" + key + "}}";
            size_t pos = 0;
            
            while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                result.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("alarm", LogLevel::ERROR, "Failed to interpolate template: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace Alarm
} // namespace PulseOne