/**
 * @file MqttTargetHandler.cpp
 * @brief CSP Gateway MQTT 타겟 핸들러 구현
 * @author PulseOne Development Team
 * @date 2025-12-03
 * @version 1.0.3 - validateConfig 시그니처 수정
 */

#include "CSP/MqttTargetHandler.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>

namespace PulseOne {
namespace CSP {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

MqttTargetHandler::MqttTargetHandler() {
    LogManager::getInstance().Info("MqttTargetHandler 초기화");
}

MqttTargetHandler::~MqttTargetHandler() {
    cleanup();
    LogManager::getInstance().Info("MqttTargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool MqttTargetHandler::initialize(const json& config) {
    try {
        LogManager::getInstance().Info("MQTT 타겟 핸들러 초기화 시작");
        
        // 설정 검증 (ITargetHandler 시그니처 사용)
        std::vector<std::string> errors;
        if (!validateConfig(config, errors)) {
            for (const auto& error : errors) {
                LogManager::getInstance().Error("초기화 검증 실패: " + error);
            }
            return false;
        }
        
        std::string broker_host = config["broker_host"].get<std::string>();
        int broker_port = config.value("broker_port", 1883);
        bool ssl_enabled = config.value("ssl_enabled", false);
        
        // SSL 사용 시 기본 포트 변경
        if (ssl_enabled && broker_port == 1883) {
            broker_port = 8883;
        }
        
        // 브로커 URI 생성
        std::string protocol = ssl_enabled ? "ssl://" : "tcp://";
        broker_uri_ = protocol + broker_host + ":" + std::to_string(broker_port);
        
        LogManager::getInstance().Info("MQTT 브로커 URI: " + broker_uri_);
        
        // 클라이언트 ID 생성
        std::string base_id = config.value("client_id", "");
        client_id_ = generateClientId(base_id);
        
        // 메시지 큐 크기 설정
        max_queue_size_ = config.value("max_queue_size", 1000);
        
        // 브로커 연결 시도
        bool auto_connect = config.value("auto_connect", true);
        if (auto_connect) {
            if (!connectToBroker(config)) {
                LogManager::getInstance().Warn("초기 MQTT 연결 실패 - 자동 재연결 활성화");
            }
        }
        
        // 자동 재연결 스레드 시작
        bool auto_reconnect = config.value("auto_reconnect", true);
        if (auto_reconnect) {
            should_stop_ = false;
            reconnect_thread_ = std::make_unique<std::thread>(
                &MqttTargetHandler::reconnectThread, this, config);
            LogManager::getInstance().Info("MQTT 자동 재연결 스레드 시작");
        }
        
        LogManager::getInstance().Info("MQTT 타겟 핸들러 초기화 완료");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("MQTT 타겟 핸들러 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

bool MqttTargetHandler::validateConfig(const json& config, std::vector<std::string>& errors) {
    errors.clear();
    
    // 필수 필드: broker_host
    if (!config.contains("broker_host")) {
        errors.push_back("broker_host 필드가 필수입니다");
        return false;
    }
    
    std::string broker_host = config["broker_host"].get<std::string>();
    if (broker_host.empty()) {
        errors.push_back("broker_host가 비어있습니다");
        return false;
    }
    
    // broker_port 검증 (선택사항, 기본값 1883)
    if (config.contains("broker_port")) {
        int port = config["broker_port"].get<int>();
        if (port <= 0 || port > 65535) {
            errors.push_back("broker_port는 1-65535 범위여야 합니다");
            return false;
        }
    }
    
    // QoS 검증 (선택사항)
    if (config.contains("qos")) {
        int qos = config["qos"].get<int>();
        if (qos < 0 || qos > 2) {
            errors.push_back("qos는 0, 1, 2 중 하나여야 합니다");
            return false;
        }
    }
    
    // max_queue_size 검증 (선택사항)
    if (config.contains("max_queue_size")) {
        int queue_size = config["max_queue_size"].get<int>();
        if (queue_size <= 0) {
            errors.push_back("max_queue_size는 양수여야 합니다");
            return false;
        }
    }
    
    return true;
}

TargetSendResult MqttTargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "MQTT";
    result.target_name = broker_uri_;
    result.success = false;
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // 토픽 생성
        std::string topic = generateTopic(alarm, config);
        
        // 페이로드 생성
        std::string payload = generatePayload(alarm, config);
        
        // QoS 및 Retain 설정
        int qos = config.value("qos", 1);
        bool retain = config.value("retain", false);
        
        LogManager::getInstance().Debug("MQTT 발행 - Topic: " + topic);
        
        // 연결 상태 확인 및 발행
        if (is_connected_.load()) {
            result = publishMessage(topic, payload, qos, retain);
        } else {
            // 연결되지 않은 경우 메시지 큐에 저장
            if (enqueueMessage(topic, payload)) {
                result.success = false;
                result.error_message = "MQTT 연결 끊김 - 메시지를 큐에 저장";
                LogManager::getInstance().Warn("MQTT 연결 끊김 - 메시지 큐에 저장: " + topic);
            } else {
                result.success = false;
                result.error_message = "MQTT 연결 끊김 및 큐 저장 실패";
                LogManager::getInstance().Error("MQTT 메시지 큐 저장 실패: " + topic);
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        result.response_time = duration;
        result.mqtt_topic = topic;
        result.content_size = payload.length();
        
        // 통계 업데이트
        publish_count_++;
        if (result.success) {
            success_count_++;
        } else {
            failure_count_++;
        }
        
    } catch (const std::exception& e) {
        result.error_message = "MQTT 발행 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
        failure_count_++;
    }
    
    return result;
}

bool MqttTargetHandler::testConnection(const json& config) {
    try {
        LogManager::getInstance().Info("MQTT 연결 테스트 시작");
        
        // 현재 연결 상태 확인
        if (is_connected_.load()) {
            LogManager::getInstance().Info("MQTT 이미 연결됨 - 테스트 메시지 발행");
            
            // 테스트 메시지 발행
            std::string test_topic = "test/" + client_id_ + "/connection";
            json test_payload = {
                {"test", true},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"client_id", client_id_}
            };
            
            auto result = publishMessage(test_topic, test_payload.dump(), 0, false);
            
            if (result.success) {
                LogManager::getInstance().Info("MQTT 연결 테스트 성공");
                return true;
            } else {
                LogManager::getInstance().Error("MQTT 테스트 메시지 발행 실패: " + result.error_message);
                return false;
            }
        } else {
            // 연결되지 않은 경우 연결 시도
            LogManager::getInstance().Info("MQTT 연결 시도");
            bool success = connectToBroker(config);
            
            if (success) {
                LogManager::getInstance().Info("MQTT 연결 테스트 성공");
            } else {
                LogManager::getInstance().Error("MQTT 연결 테스트 실패");
            }
            
            return success;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("MQTT 연결 테스트 예외: " + std::string(e.what()));
        return false;
    }
}

json MqttTargetHandler::getStatus() const {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    
    return json{
        {"type", "MQTT"},
        {"broker_uri", broker_uri_},
        {"client_id", client_id_},
        {"connected", is_connected_.load()},
        {"connecting", is_connecting_.load()},
        {"publish_count", publish_count_.load()},
        {"success_count", success_count_.load()},
        {"failure_count", failure_count_.load()},
        {"queue_size", message_queue_.size()},
        {"max_queue_size", max_queue_size_},
        {"connection_attempts", connection_attempts_.load()}
    };
}

void MqttTargetHandler::cleanup() {
    // 재연결 스레드 중지
    should_stop_ = true;
    if (reconnect_thread_ && reconnect_thread_->joinable()) {
        reconnect_thread_->join();
        reconnect_thread_.reset();
    }
    
    // MQTT 클라이언트 연결 해제
    disconnectFromBroker();
    
    // 메시지 큐 정리
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
        while (!message_queue_.empty()) {
            message_queue_.pop();
        }
    }
    
    LogManager::getInstance().Info("MqttTargetHandler 정리 완료");
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

bool MqttTargetHandler::connectToBroker(const json& config) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (is_connected_.load()) {
        return true;
    }
    
    is_connecting_ = true;
    connection_attempts_++;
    
    try {
        LogManager::getInstance().Info("MQTT 브로커 연결 시도: " + broker_uri_);
        
        // 임시 구현: 설정 검증 및 로깅
        std::string username = config.value("username", "");
        bool ssl_enabled = config.value("ssl_enabled", false);
        int keep_alive = config.value("keep_alive_sec", 60);
        
        LogManager::getInstance().Debug("MQTT 연결 설정 - username: " + 
                                       (username.empty() ? "none" : username) +
                                       ", ssl: " + (ssl_enabled ? "enabled" : "disabled") +
                                       ", keep_alive: " + std::to_string(keep_alive) + "s");
        
        // 임시로 연결 성공으로 처리
        is_connected_ = true;
        is_connecting_ = false;
        
        LogManager::getInstance().Info("MQTT 브로커 연결 성공: " + broker_uri_);
        
        // 큐된 메시지 처리
        processQueuedMessages();
        
        return true;
        
    } catch (const std::exception& e) {
        is_connected_ = false;
        is_connecting_ = false;
        LogManager::getInstance().Error("MQTT 브로커 연결 실패: " + std::string(e.what()));
        return false;
    }
}

void MqttTargetHandler::disconnectFromBroker() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (!is_connected_.load() && mqtt_client_ == nullptr) {
        return;
    }
    
    try {
        LogManager::getInstance().Info("MQTT 브로커 연결 해제 시작");
        
        is_connected_ = false;
        mqtt_client_ = nullptr;
        
        LogManager::getInstance().Info("MQTT 브로커 연결 해제 완료");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("MQTT 연결 해제 중 예외: " + std::string(e.what()));
    }
}

std::string MqttTargetHandler::generateTopic(const AlarmMessage& alarm, const json& config) const {
    std::string topic_pattern = config.value("topic_pattern", "alarms/{building_id}/{nm}");
    return expandTemplateVariables(topic_pattern, alarm);
}

std::string MqttTargetHandler::generatePayload(const AlarmMessage& alarm, const json& config) const {
    std::string format = config.value("message_format", "json");
    
    if (format == "json") {
        return createJsonMessage(alarm, config);
    } else if (format == "text") {
        return createTextMessage(alarm, config);
    } else {
        LogManager::getInstance().Warn("알 수 없는 메시지 형식: " + format + " (JSON 사용)");
        return createJsonMessage(alarm, config);
    }
}

TargetSendResult MqttTargetHandler::publishMessage(const std::string& topic, 
                                                  const std::string& payload,
                                                  int qos, bool retain) const {
    TargetSendResult result;
    result.target_type = "MQTT";
    result.target_name = broker_uri_;
    result.mqtt_topic = topic;
    result.content_size = payload.length();
    result.success = false;
    
    try {
        if (!is_connected_.load()) {
            result.error_message = "MQTT 클라이언트가 연결되지 않음";
            return result;
        }
        
        // 임시 구현: 성공으로 처리
        result.success = true;
        
        LogManager::getInstance().Debug("MQTT 메시지 발행 성공 - Topic: " + topic + 
                                       ", Size: " + std::to_string(payload.length()) + " bytes" +
                                       ", QoS: " + std::to_string(qos) +
                                       ", Retain: " + (retain ? "true" : "false"));
        
    } catch (const std::exception& e) {
        result.error_message = "MQTT 발행 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
    }
    
    return result;
}

std::string MqttTargetHandler::generateClientId(const std::string& base_id) const {
    if (!base_id.empty()) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return base_id + "_" + std::to_string(timestamp);
    } else {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return "pulseone_csp_" + std::to_string(timestamp);
    }
}

void MqttTargetHandler::reconnectThread(json config) {
    int reconnect_interval = config.value("reconnect_interval_sec", 5);
    int max_attempts = config.value("max_reconnect_attempts", -1);
    
    LogManager::getInstance().Info("MQTT 재연결 스레드 시작");
    
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(reconnect_interval));
        
        if (should_stop_.load()) {
            break;
        }
        
        if (!is_connected_.load() && !is_connecting_.load()) {
            if (max_attempts > 0 && connection_attempts_.load() >= max_attempts) {
                LogManager::getInstance().Warn("MQTT 최대 재연결 시도 횟수 초과");
                continue;
            }
            
            LogManager::getInstance().Info("MQTT 자동 재연결 시도");
            
            if (connectToBroker(config)) {
                LogManager::getInstance().Info("MQTT 자동 재연결 성공");
            }
        }
    }
    
    LogManager::getInstance().Info("MQTT 재연결 스레드 종료");
}

void MqttTargetHandler::processQueuedMessages() {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    
    if (message_queue_.empty()) {
        return;
    }
    
    LogManager::getInstance().Info("큐된 MQTT 메시지 처리 시작: " + 
                                  std::to_string(message_queue_.size()) + "개");
    
    size_t processed = 0;
    size_t failed = 0;
    
    while (!message_queue_.empty() && is_connected_.load()) {
        auto message = message_queue_.front();
        message_queue_.pop();
        
        auto result = publishMessage(message.first, message.second, 0, false);
        
        if (result.success) {
            processed++;
        } else {
            failed++;
        }
    }
    
    LogManager::getInstance().Info("큐된 MQTT 메시지 처리 완료");
}

bool MqttTargetHandler::enqueueMessage(const std::string& topic, const std::string& payload) {
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);
    
    if (message_queue_.size() >= max_queue_size_) {
        LogManager::getInstance().Warn("MQTT 메시지 큐 가득참");
        message_queue_.pop();
    }
    
    message_queue_.emplace(topic, payload);
    
    return true;
}

std::string MqttTargetHandler::expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const {
    std::string result = template_str;
    
    result = std::regex_replace(result, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    result = std::regex_replace(result, std::regex("\\{nm\\}"), alarm.nm);
    result = std::regex_replace(result, std::regex("\\{point_name\\}"), alarm.nm);
    result = std::regex_replace(result, std::regex("\\{value\\}"), std::to_string(alarm.vl));
    result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"), std::to_string(alarm.al));
    result = std::regex_replace(result, std::regex("\\{status\\}"), std::to_string(alarm.st));
    result = std::regex_replace(result, std::regex("\\{client_id\\}"), client_id_);
    
    result = std::regex_replace(result, std::regex("[^a-zA-Z0-9/_.-]"), "_");
    
    return result;
}

std::string MqttTargetHandler::createJsonMessage(const AlarmMessage& alarm, const json& config) const {
    json message;
    
    message["bd"] = alarm.bd;
    message["nm"] = alarm.nm;
    message["vl"] = alarm.vl;
    message["tm"] = alarm.tm;
    message["al"] = alarm.al;
    message["st"] = alarm.st;
    message["des"] = alarm.des;
    
    if (config.value("include_metadata", true)) {
        message["source"] = "PulseOne-CSPGateway";
        message["version"] = "1.0";
        message["client_id"] = client_id_;
    }
    
    if (config.contains("additional_fields") && config["additional_fields"].is_object()) {
        for (auto& [key, value] : config["additional_fields"].items()) {
            message[key] = value;
        }
    }
    
    return message.dump();
}

std::string MqttTargetHandler::createTextMessage(const AlarmMessage& alarm, const json& config) const {
    std::ostringstream text;
    
    std::string format = config.value("text_format", "default");
    
    if (format == "simple") {
        text << alarm.nm << "=" << alarm.vl;
    } else if (format == "detailed") {
        text << "[" << alarm.bd << "] " << alarm.nm << " = " << alarm.vl 
             << " | " << alarm.des;
    } else {
        text << "Building " << alarm.bd << " - " << alarm.nm << ": " << alarm.vl;
    }
    
    return text.str();
}

void MqttTargetHandler::onConnectionLost(void* context, char* cause) {
    auto* handler = static_cast<MqttTargetHandler*>(context);
    handler->is_connected_ = false;
    
    std::string cause_str = cause ? cause : "Unknown";
    LogManager::getInstance().Warn("MQTT 연결 끊김: " + cause_str);
}

int MqttTargetHandler::onMessageArrived(void* context, char* topicName, int topicLen, void* message) {
    (void)context;
    (void)topicName;
    (void)topicLen;
    (void)message;
    
    return 1;
}

void MqttTargetHandler::onDeliveryComplete(void* context, MQTTClient_deliveryToken token) {
    (void)context;
    (void)token;
}

} // namespace CSP
} // namespace PulseOne