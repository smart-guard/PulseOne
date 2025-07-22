/**
 * @file MQTTWorker.cpp
 * @brief MQTT 워커 구현
 * @author PulseOne Development Team
 * @date 2025-01-22
 * @version 1.0.0
 */

#include "Workers/Protocol/MQTTWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <cstring>
#include <shared_mutex>
#include <nlohmann/json.hpp>

// Eclipse Paho MQTT C++ 헤더들
#include <mqtt/async_client.h>
#include <mqtt/connect_options.h>
#include <mqtt/disconnect_options.h>
#include <mqtt/ssl_options.h>

using namespace std::chrono;
using namespace PulseOne::Drivers;
using json = nlohmann::json;

namespace PulseOne {
namespace Workers {
namespace Protocol {

// =============================================================================
// MqttCallbackHandler 구현
// =============================================================================

void MqttCallbackHandler::connection_lost(const std::string& cause) {
    if (mqtt_worker_) {
        mqtt_worker_->OnConnectionLost(cause);
    }
}

void MqttCallbackHandler::message_arrived(mqtt::const_message_ptr message) {
    if (mqtt_worker_) {
        mqtt_worker_->OnMessageArrived(message);
    }
}

void MqttCallbackHandler::delivery_complete(mqtt::delivery_token_ptr token) {
    if (mqtt_worker_) {
        mqtt_worker_->OnDeliveryComplete(token);
    }
}

void MqttCallbackHandler::on_failure(const mqtt::token& /* token */) {
    // 연결/구독/발행 실패 처리
    // 구체적인 처리는 MQTTWorker에서 수행
}

void MqttCallbackHandler::on_success(const mqtt::token& token) {
    // 연결/구독/발행 성공 처리
    if (mqtt_worker_ && token.get_type() == mqtt::token::CONNECT) {
        mqtt_worker_->OnConnected(false);
    }
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

MQTTWorker::MQTTWorker(const Drivers::DeviceInfo& device_info,
                       std::shared_ptr<RedisClient> redis_client,
                       std::shared_ptr<InfluxClient> influx_client)
    : TcpBasedWorker(device_info, redis_client, influx_client)
    , next_subscription_id_(1)
    , next_publication_id_(1)
    , stop_workers_(false)
    , total_messages_sent_(0), total_messages_received_(0), failed_sends_(0)
    , connection_attempts_(0), successful_connections_(0) {
    
    LogMqttMessage(LogLevel::INFO, "MQTTWorker created for device: " + device_info.name);
    
    // 기본 재연결 설정
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 10000;         // MQTT는 긴 재연결 주기
    settings.max_retries_per_cycle = 5;
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 60;  // MQTT Keep-alive
    UpdateReconnectionSettings(settings);
    
    // DeviceInfo에서 브로커 정보 파싱
    if (!device_info.endpoint.empty()) {
        broker_url_ = device_info.endpoint;
        if (broker_url_.find("://") == std::string::npos) {
            broker_url_ = "tcp://" + broker_url_;  // 기본 프로토콜 추가
        }
    } else {
        broker_url_ = "tcp://localhost:1883";  // 기본값
    }
    
    // 기본 클라이언트 ID 생성
    mqtt_config_.client_id = "PulseOne_" + device_info.id;
}

MQTTWorker::~MQTTWorker() {
    // 워커 스레드 정리
    stop_workers_ = true;
    message_queue_cv_.notify_all();
    
    if (publish_worker_thread_.joinable()) {
        publish_worker_thread_.join();
    }
    
    // MQTT 클라이언트 정리
    if (mqtt_client_ && mqtt_client_->is_connected()) {
        try {
            mqtt_client_->disconnect()->wait();
        } catch (const std::exception& e) {
            LogMqttMessage(LogLevel::WARN, "Exception during MQTT disconnect: " + std::string(e.what()));
        }
    }
    
    LogMqttMessage(LogLevel::INFO, "MQTTWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> MQTTWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::RUNNING) {
            LogMqttMessage(LogLevel::WARN, "Worker already running");
            return true;
        }
        
        LogMqttMessage(LogLevel::INFO, "Starting MQTTWorker...");
        
        try {
            // 기본 연결 설정
            if (!EstablishConnection()) {
                LogMqttMessage(LogLevel::ERROR, "Failed to establish connection");
                return false;
            }
            
            // 상태 변경
            ChangeState(WorkerState::RUNNING);
            
            // 발행 워커 스레드 시작
            stop_workers_ = false;
            publish_worker_thread_ = std::thread(&MQTTWorker::PublishWorkerLoop, this);
            
            LogMqttMessage(LogLevel::INFO, "MQTTWorker started successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogMqttMessage(LogLevel::ERROR, "Exception during start: " + std::string(e.what()));
            return false;
        }
    });
}

std::future<bool> MQTTWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        if (GetState() == WorkerState::STOPPED) {
            LogMqttMessage(LogLevel::WARN, "Worker already stopped");
            return true;
        }
        
        LogMqttMessage(LogLevel::INFO, "Stopping MQTTWorker...");
        
        try {
            // 워커 스레드 중지
            stop_workers_ = true;
            message_queue_cv_.notify_all();
            
            if (publish_worker_thread_.joinable()) {
                publish_worker_thread_.join();
            }
            
            // 연결 종료
            CloseConnection();
            
            // 상태 변경
            ChangeState(WorkerState::STOPPED);
            
            LogMqttMessage(LogLevel::INFO, "MQTTWorker stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            LogMqttMessage(LogLevel::ERROR, "Exception during stop: " + std::string(e.what()));
            return false;
        }
    });
}

WorkerState MQTTWorker::GetState() const {
    return TcpBasedWorker::GetState();
}

// =============================================================================
// TcpBasedWorker 인터페이스 구현
// =============================================================================

bool MQTTWorker::EstablishProtocolConnection() {
    LogMqttMessage(LogLevel::INFO, "Establishing MQTT protocol connection...");
    
    try {
        // MQTT 클라이언트 생성
        if (!mqtt_client_) {
            mqtt_client_ = std::make_unique<mqtt::async_client>(broker_url_, mqtt_config_.client_id);
            callback_handler_ = std::make_unique<MqttCallbackHandler>(this);
            mqtt_client_->set_callback(*callback_handler_);
        }
        
        // 연결 옵션 설정
        mqtt::connect_options conn_opts;
        conn_opts.set_keep_alive_interval(mqtt_config_.keep_alive_interval_seconds);
        conn_opts.set_clean_session(mqtt_config_.clean_session);
        conn_opts.set_connect_timeout(mqtt_config_.connection_timeout_seconds);
        
        // 사용자 인증 설정
        if (!mqtt_config_.username.empty()) {
            conn_opts.set_user_name(mqtt_config_.username);
            if (!mqtt_config_.password.empty()) {
                conn_opts.set_password(mqtt_config_.password);
            }
        }
        
        // SSL 설정
        if (mqtt_config_.use_ssl) {
            mqtt::ssl_options ssl_opts;
            if (!mqtt_config_.ca_cert_file.empty()) {
                ssl_opts.set_trust_store(mqtt_config_.ca_cert_file);
            }
            if (!mqtt_config_.client_cert_file.empty() && !mqtt_config_.client_key_file.empty()) {
                ssl_opts.set_key_store(mqtt_config_.client_cert_file);
                ssl_opts.set_private_key(mqtt_config_.client_key_file);
            }
            conn_opts.set_ssl(ssl_opts);
        }
        
        // Will 메시지 설정
        if (mqtt_config_.use_will_message) {
            mqtt::will_options will_opts(mqtt_config_.will_topic,
                                        mqtt_config_.will_payload,
                                        QosToInt(mqtt_config_.will_qos),
                                        mqtt_config_.will_retain);
            conn_opts.set_will(will_opts);
        }
        
        // 연결 시도
        UpdateMqttStats("connect", false);
        auto token = mqtt_client_->connect(conn_opts);
        token->wait_for(std::chrono::seconds(mqtt_config_.connection_timeout_seconds));
        
        if (token->is_complete()) {
            LogMqttMessage(LogLevel::INFO, "MQTT protocol connection established to " + broker_url_);
            UpdateMqttStats("connect", true);
            
            // 기존 구독 복구
            RestoreSubscriptions();
            
            return true;
        } else {
            std::string error_msg = "MQTT connection failed";
            LogMqttMessage(LogLevel::ERROR, error_msg);
            return false;
        }
        
    } catch (const std::exception& e) {
        LogMqttMessage(LogLevel::ERROR, "Exception in MQTT connection: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::CloseProtocolConnection() {
    LogMqttMessage(LogLevel::INFO, "Closing MQTT protocol connection...");
    
    try {
        if (mqtt_client_ && mqtt_client_->is_connected()) {
            mqtt::disconnect_options disc_opts;
            disc_opts.set_timeout(std::chrono::seconds(5));
            
            auto token = mqtt_client_->disconnect(disc_opts);
            token->wait();
            
            LogMqttMessage(LogLevel::INFO, "MQTT protocol connection closed");
        }
        return true;
        
    } catch (const std::exception& e) {
        LogMqttMessage(LogLevel::ERROR, "Exception during MQTT disconnect: " + std::string(e.what()));
        return false;
    }
}

bool MQTTWorker::CheckProtocolConnection() {
    return mqtt_client_ && mqtt_client_->is_connected();
}

bool MQTTWorker::SendProtocolKeepAlive() {
    // MQTT는 자동으로 keep-alive를 관리하므로 연결 상태만 확인
    return CheckProtocolConnection();
}

// =============================================================================
// MQTT 클라이언트 설정 관리
// =============================================================================

void MQTTWorker::ConfigureMqttClient(const MqttClientConfig& config) {
    mqtt_config_ = config;
    
    LogMqttMessage(LogLevel::INFO, 
        "MQTT client configured - ClientID: " + config.client_id +
        ", KeepAlive: " + std::to_string(config.keep_alive_interval_seconds) + "s" +
        ", CleanSession: " + (config.clean_session ? "true" : "false"));
}

void MQTTWorker::ConfigureBroker(const std::string& broker_url,
                                const std::string& client_id,
                                const std::string& username,
                                const std::string& password) {
    broker_url_ = broker_url;
    mqtt_config_.client_id = client_id;
    mqtt_config_.username = username;
    mqtt_config_.password = password;
    
    LogMqttMessage(LogLevel::INFO, 
        "Broker configured - URL: " + broker_url + 
        ", ClientID: " + client_id +
        ", Username: " + (username.empty() ? "none" : username));
}

// =============================================================================
// 구독 관리
// =============================================================================

uint32_t MQTTWorker::AddSubscription(const std::string& topic, MqttQoS qos) {
    if (!ValidateMqttTopic(topic)) {
        LogMqttMessage(LogLevel::ERROR, "Invalid MQTT topic: " + topic);
        return 0;
    }
    
    std::unique_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    uint32_t subscription_id = next_subscription_id_++;
    MqttSubscription subscription;
    subscription.subscription_id = subscription_id;
    subscription.topic = topic;
    subscription.qos = qos;
    subscription.is_active = true;
    
    subscriptions_.emplace(subscription_id, std::move(subscription));
    
    // 즉시 구독 시도 (연결된 경우)
    if (CheckProtocolConnection()) {
        try {
            auto token = mqtt_client_->subscribe(topic, QosToInt(qos));
            token->wait_for(std::chrono::seconds(10));
            
            if (token->is_complete()) {
                LogMqttMessage(LogLevel::INFO, 
                    "Subscribed to topic '" + topic + "' (ID: " + std::to_string(subscription_id) + 
                    ", QoS: " + std::to_string(QosToInt(qos)) + ")");
            } else {
                LogMqttMessage(LogLevel::WARN, "Failed to subscribe to topic: " + topic);
            }
        } catch (const std::exception& e) {
            LogMqttMessage(LogLevel::ERROR, "Exception during subscription: " + std::string(e.what()));
        }
    }
    
    return subscription_id;
}

bool MQTTWorker::RemoveSubscription(uint32_t subscription_id) {
    std::unique_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        LogMqttMessage(LogLevel::WARN, "Subscription not found: " + std::to_string(subscription_id));
        return false;
    }
    
    std::string topic = it->second.topic;
    
    // 구독 해제 시도 (연결된 경우)
    if (CheckProtocolConnection()) {
        try {
            auto token = mqtt_client_->unsubscribe(topic);
            token->wait_for(std::chrono::seconds(5));
        } catch (const std::exception& e) {
            LogMqttMessage(LogLevel::WARN, "Exception during unsubscription: " + std::string(e.what()));
        }
    }
    
    subscriptions_.erase(it);
    
    LogMqttMessage(LogLevel::INFO, "Removed subscription '" + topic + "' (ID: " + std::to_string(subscription_id) + ")");
    return true;
}

bool MQTTWorker::SetSubscriptionActive(uint32_t subscription_id, bool active) {
    std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        LogMqttMessage(LogLevel::WARN, "Subscription not found: " + std::to_string(subscription_id));
        return false;
    }
    
    const_cast<MqttSubscription&>(it->second).is_active = active;
    
    LogMqttMessage(LogLevel::INFO, 
        "Subscription " + std::to_string(subscription_id) + " " + (active ? "activated" : "deactivated"));
    return true;
}

bool MQTTWorker::AddDataPointToSubscription(uint32_t subscription_id, const Drivers::DataPoint& data_point) {
    std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        LogMqttMessage(LogLevel::WARN, "Subscription not found: " + std::to_string(subscription_id));
        return false;
    }
    
    const_cast<MqttSubscription&>(it->second).data_points.push_back(data_point);
    
    LogMqttMessage(LogLevel::DEBUG, 
        "Added data point '" + data_point.name + "' to subscription " + std::to_string(subscription_id));
    return true;
}

// =============================================================================
// 발행 관리
// =============================================================================

uint32_t MQTTWorker::AddPublication(const std::string& topic,
                                   MqttQoS qos,
                                   bool retain,
                                   int publish_interval_ms) {
    if (!ValidateMqttTopic(topic)) {
        LogMqttMessage(LogLevel::ERROR, "Invalid MQTT topic: " + topic);
        return 0;
    }
    
    std::unique_lock<std::shared_mutex> lock(publications_mutex_);
    
    uint32_t publication_id = next_publication_id_++;
    MqttPublication publication;
    publication.publication_id = publication_id;
    publication.topic = topic;
    publication.qos = qos;
    publication.retain = retain;
    publication.publish_interval_ms = publish_interval_ms;
    publication.is_active = true;
    publication.next_publish_time = std::chrono::system_clock::now();
    
    publications_.emplace(publication_id, std::move(publication));
    
    LogMqttMessage(LogLevel::INFO, 
        "Added publication '" + topic + "' (ID: " + std::to_string(publication_id) + 
        ") with interval " + std::to_string(publish_interval_ms) + "ms");
    
    return publication_id;
}

bool MQTTWorker::RemovePublication(uint32_t publication_id) {
    std::unique_lock<std::shared_mutex> lock(publications_mutex_);
    
    auto it = publications_.find(publication_id);
    if (it == publications_.end()) {
        LogMqttMessage(LogLevel::WARN, "Publication not found: " + std::to_string(publication_id));
        return false;
    }
    
    std::string topic = it->second.topic;
    publications_.erase(it);
    
    LogMqttMessage(LogLevel::INFO, "Removed publication '" + topic + "' (ID: " + std::to_string(publication_id) + ")");
    return true;
}

bool MQTTWorker::SetPublicationActive(uint32_t publication_id, bool active) {
    std::shared_lock<std::shared_mutex> lock(publications_mutex_);
    
    auto it = publications_.find(publication_id);
    if (it == publications_.end()) {
        LogMqttMessage(LogLevel::WARN, "Publication not found: " + std::to_string(publication_id));
        return false;
    }
    
    const_cast<MqttPublication&>(it->second).is_active = active;
    
    LogMqttMessage(LogLevel::INFO, 
        "Publication " + std::to_string(publication_id) + " " + (active ? "activated" : "deactivated"));
    return true;
}

bool MQTTWorker::AddDataPointToPublication(uint32_t publication_id, const Drivers::DataPoint& data_point) {
    std::shared_lock<std::shared_mutex> lock(publications_mutex_);
    
    auto it = publications_.find(publication_id);
    if (it == publications_.end()) {
        LogMqttMessage(LogLevel::WARN, "Publication not found: " + std::to_string(publication_id));
        return false;
    }
    
    const_cast<MqttPublication&>(it->second).data_points.push_back(data_point);
    
    LogMqttMessage(LogLevel::DEBUG, 
        "Added data point '" + data_point.name + "' to publication " + std::to_string(publication_id));
    return true;
}

// =============================================================================
// 메시지 송수신
// =============================================================================

bool MQTTWorker::PublishMessage(const std::string& topic,
                               const std::string& payload,
                               MqttQoS qos,
                               bool retain) {
    if (!CheckProtocolConnection()) {
        LogMqttMessage(LogLevel::WARN, "Cannot publish - MQTT not connected");
        return false;
    }
    
    try {
        auto message = mqtt::make_message(topic, payload);
        message->set_qos(QosToInt(qos));
        message->set_retained(retain);
        
        auto token = mqtt_client_->publish(message);
        
        // QoS 0이면 대기하지 않음
        if (qos == MqttQoS::AT_MOST_ONCE) {
            UpdateMqttStats("send", true);
            return true;
        } else {
            // QoS 1,2는 완료까지 대기
            bool success = token->wait_for(std::chrono::seconds(10));
            UpdateMqttStats("send", success);
            
            if (success) {
                LogMqttMessage(LogLevel::DEBUG, "Published to topic: " + topic);
            } else {
                LogMqttMessage(LogLevel::WARN, "Publish timeout for topic: " + topic);
            }
            return success;
        }
        
    } catch (const std::exception& e) {
        LogMqttMessage(LogLevel::ERROR, "Exception during publish: " + std::string(e.what()));
        UpdateMqttStats("send", false);
        return false;
    }
}

bool MQTTWorker::PublishJsonMessage(const std::string& topic,
                                   const nlohmann::json& json_data,
                                   MqttQoS qos,
                                   bool retain) {
    try {
        std::string payload = json_data.dump();
        return PublishMessage(topic, payload, qos, retain);
    } catch (const std::exception& e) {
        LogMqttMessage(LogLevel::ERROR, "Exception during JSON publish: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 상태 및 통계 조회
// =============================================================================

std::string MQTTWorker::GetMqttStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    json stats;
    stats["total_messages_sent"] = total_messages_sent_;
    stats["total_messages_received"] = total_messages_received_;
    stats["failed_sends"] = failed_sends_;
    stats["connection_attempts"] = connection_attempts_;
    stats["successful_connections"] = successful_connections_;
    stats["send_success_rate"] = total_messages_sent_ > 0 ? 
        (double)(total_messages_sent_ - failed_sends_) / total_messages_sent_ * 100.0 : 0.0;
    stats["connection_success_rate"] = connection_attempts_ > 0 ?
        (double)successful_connections_ / connection_attempts_ * 100.0 : 0.0;
    
    // const 메서드에서 non-const 메서드 호출을 피하기 위해 직접 확인
    bool is_connected = mqtt_client_ && mqtt_client_->is_connected();
    stats["is_connected"] = is_connected;
    
    stats["broker_url"] = broker_url_;
    stats["client_id"] = mqtt_config_.client_id;
    
    return stats.dump(2);
}

std::string MQTTWorker::GetSubscriptionStatus() const {
    std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    json subscriptions = json::array();
    
    for (const auto& pair : subscriptions_) {
        const auto& sub = pair.second;
        json sub_info;
        sub_info["subscription_id"] = sub.subscription_id;
        sub_info["topic"] = sub.topic;
        sub_info["qos"] = QosToInt(sub.qos);
        sub_info["is_active"] = sub.is_active;
        sub_info["messages_received"] = sub.messages_received;
        sub_info["data_points_count"] = sub.data_points.size();
        
        subscriptions.push_back(sub_info);
    }
    
    return subscriptions.dump(2);
}

std::string MQTTWorker::GetPublicationStatus() const {
    std::shared_lock<std::shared_mutex> lock(publications_mutex_);
    
    json publications = json::array();
    
    for (const auto& pair : publications_) {
        const auto& pub = pair.second;
        json pub_info;
        pub_info["publication_id"] = pub.publication_id;
        pub_info["topic"] = pub.topic;
        pub_info["qos"] = QosToInt(pub.qos);
        pub_info["retain"] = pub.retain;
        pub_info["publish_interval_ms"] = pub.publish_interval_ms;
        pub_info["is_active"] = pub.is_active;
        pub_info["messages_published"] = pub.messages_published;
        pub_info["failed_publishes"] = pub.failed_publishes;
        pub_info["data_points_count"] = pub.data_points.size();
        
        publications.push_back(pub_info);
    }
    
    return publications.dump(2);
}

// =============================================================================
// 콜백 메서드
// =============================================================================

void MQTTWorker::OnConnectionLost(const std::string& cause) {
    LogMqttMessage(LogLevel::WARN, "MQTT connection lost: " + cause);
    
    // 자동 재연결은 BaseDeviceWorker가 처리하므로 상태만 업데이트
    SetConnectionState(false);
}

void MQTTWorker::OnMessageArrived(mqtt::const_message_ptr message) {
    std::string topic = message->get_topic();
    std::string payload = message->to_string();
    
    LogMqttMessage(LogLevel::DEBUG, "Message received on topic: " + topic);
    
    // 메시지 큐에 추가
    {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        incoming_messages_.push({topic, payload});
    }
    message_queue_cv_.notify_one();
    
    // 즉시 처리
    ProcessReceivedMessage(topic, payload);
    
    UpdateMqttStats("receive", true);
}

void MQTTWorker::OnDeliveryComplete(mqtt::delivery_token_ptr /* token */) {
    // 메시지 전송 완료 처리
    LogMqttMessage(LogLevel::DEBUG, "Message delivery completed");
}

void MQTTWorker::OnConnected(bool reconnect) {
    LogMqttMessage(LogLevel::INFO, reconnect ? "MQTT reconnected" : "MQTT connected");
    SetConnectionState(true);
}

// =============================================================================
// 내부 헬퍼 메소드들
// =============================================================================

void MQTTWorker::PublishWorkerLoop() {
    LogMqttMessage(LogLevel::INFO, "Publish worker started");
    
    while (!stop_workers_) {
        try {
            auto current_time = std::chrono::system_clock::now();
            
            std::shared_lock<std::shared_mutex> lock(publications_mutex_);
            for (auto& pair : publications_) {
                auto& publication = const_cast<MqttPublication&>(pair.second);
                
                if (!publication.is_active) continue;
                
                if (current_time >= publication.next_publish_time) {
                    ProcessPublication(publication);
                    
                    // 다음 발행 시간 설정
                    publication.last_publish_time = current_time;
                    publication.next_publish_time = current_time + 
                        std::chrono::milliseconds(publication.publish_interval_ms);
                }
            }
            
        } catch (const std::exception& e) {
            LogMqttMessage(LogLevel::ERROR, "Exception in publish worker: " + std::string(e.what()));
        }
        
        // 100ms 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LogMqttMessage(LogLevel::INFO, "Publish worker stopped");
}

bool MQTTWorker::ProcessPublication(MqttPublication& publication) {
    if (!CheckProtocolConnection() || publication.data_points.empty()) {
        return false;
    }
    
    try {
        // 데이터 포인트들로부터 JSON 메시지 생성
        json message_data = json::object();
        message_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        message_data["device_id"] = GetDeviceInfo().id;
        message_data["data_points"] = json::array();
        
        for (const auto& data_point : publication.data_points) {
            json point_data;
            point_data["id"] = data_point.id;
            point_data["name"] = data_point.name;
            point_data["address"] = data_point.address;
            point_data["unit"] = data_point.unit;
            
            // 실제 값은 데이터 수집 시스템에서 가져와야 함
            // 여기서는 예시로 랜덤 값 사용
            point_data["value"] = 0.0;  // TODO: 실제 값으로 대체
            
            message_data["data_points"].push_back(point_data);
        }
        
        bool success = PublishJsonMessage(publication.topic, message_data, 
                                         publication.qos, publication.retain);
        
        if (success) {
            publication.messages_published++;
            LogMqttMessage(LogLevel::DEBUG, 
                "Published data to topic: " + publication.topic);
        } else {
            publication.failed_publishes++;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMqttMessage(LogLevel::ERROR, 
            "Exception in publication processing: " + std::string(e.what()));
        publication.failed_publishes++;
        return false;
    }
}

void MQTTWorker::ProcessReceivedMessage(const std::string& topic, const std::string& payload) {
    // 구독과 연결된 데이터 포인트 찾기
    std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    for (auto& pair : subscriptions_) {
        auto& subscription = const_cast<MqttSubscription&>(pair.second);
        
        if (!subscription.is_active || subscription.topic != topic) {
            continue;
        }
        
        subscription.messages_received++;
        subscription.last_message_time = std::chrono::system_clock::now();
        
        // JSON 파싱 시도
        try {
            json message_data = json::parse(payload);
            
            // 데이터 포인트별 값 처리
            for (const auto& data_point : subscription.data_points) {
                if (message_data.contains("data_points")) {
                    for (const auto& point_data : message_data["data_points"]) {
                        if (point_data.contains("id") && 
                            point_data["id"] == data_point.id &&
                            point_data.contains("value")) {
                            
                            // 값 추출 및 저장
                            double value = point_data["value"];
                            TimestampedValue timestamped_value;
                            timestamped_value.value = value;
                            timestamped_value.quality = DataQuality::GOOD;
                            timestamped_value.timestamp = std::chrono::system_clock::now();
                            
                            // Redis/InfluxDB에 저장
                            SaveToInfluxDB(data_point.id, timestamped_value);
                        }
                    }
                }
            }
            
        } catch (const json::exception& e) {
            // JSON이 아닌 경우 단순 문자열로 처리
            LogMqttMessage(LogLevel::DEBUG, 
                "Non-JSON message on topic " + topic + ": " + payload);
        }
    }
}

Drivers::DataValue MQTTWorker::ParsePayloadToDataValue(const std::string& payload, Drivers::DataType data_type) {
    try {
        switch (data_type) {
            case DataType::BOOL:
                return (payload == "true" || payload == "1" || payload == "on");
                
            case DataType::INT32:
                return std::stoi(payload);
                
            case DataType::FLOAT32:
                return std::stof(payload);
                
            case DataType::FLOAT64:
                return std::stod(payload);
                
            case DataType::STRING:
            default:
                return payload;
        }
    } catch (const std::exception& e) {
        LogMqttMessage(LogLevel::WARN, "Failed to parse payload: " + payload);
        return payload;  // 기본값으로 문자열 반환
    }
}

std::string MQTTWorker::DataValueToPayload(const Drivers::DataValue& value) {
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<int>(value)) {
            return std::to_string(std::get<int>(value));
        } else if (std::holds_alternative<float>(value)) {
            return std::to_string(std::get<float>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
    } catch (const std::exception& e) {
        LogMqttMessage(LogLevel::WARN, "Failed to convert value to payload");
    }
    
    return "";
}

std::string MQTTWorker::MqttErrorToString(int return_code) const {
    switch (return_code) {
        case 0: return "Success";
        case -1: return "General failure";
        case -2: return "Persistence error";
        case -3: return "Client disconnected";
        case -4: return "Max messages in-flight";
        case -5: return "Bad UTF8 string";
        case -6: return "Null parameter";
        case -7: return "Topic name truncated";
        case -8: return "Bad structure";
        case -9: return "Bad QoS";
        case -10: return "SSL not supported";
        case -11: return "Bad protocol";
        case -14: return "Bad MQTT version";
        case -15: return "Disk full";
        default:
            return "Unknown error (" + std::to_string(return_code) + ")";
    }
}

void MQTTWorker::UpdateMqttStats(const std::string& operation, bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (operation == "send") {
        total_messages_sent_++;
        if (!success) {
            failed_sends_++;
        }
    } else if (operation == "receive") {
        total_messages_received_++;
    } else if (operation == "connect") {
        connection_attempts_++;
        if (success) {
            successful_connections_++;
        }
    }
}

void MQTTWorker::LogMqttMessage(LogLevel level, const std::string& message) const {
    // BaseDeviceWorker의 LogMessage 사용 (const_cast 필요)
    const_cast<MQTTWorker*>(this)->LogMessage(level, "[MQTT] " + message);
}

int MQTTWorker::QosToInt(MqttQoS qos) const {
    return static_cast<int>(qos);
}

bool MQTTWorker::ValidateBrokerUrl(const std::string& url) const {
    return !url.empty() && 
           (url.find("tcp://") == 0 || url.find("ssl://") == 0 || url.find("ws://") == 0);
}

bool MQTTWorker::ValidateMqttTopic(const std::string& topic) const {
    if (topic.empty() || topic.length() > 65535) {
        return false;
    }
    
    // MQTT 토픽 유효성 검사
    for (char c : topic) {
        if (c == '+' || c == '#') {
            // 와일드카드는 특정 위치에서만 허용
            continue;
        }
        if (c < 32 || c > 126) {
            return false;  // 인쇄 가능한 ASCII만 허용
        }
    }
    
    return true;
}

void MQTTWorker::RestoreSubscriptions() {
    if (!CheckProtocolConnection()) {
        return;
    }
    
    std::shared_lock<std::shared_mutex> lock(subscriptions_mutex_);
    
    for (const auto& pair : subscriptions_) {
        const auto& subscription = pair.second;
        
        if (!subscription.is_active) {
            continue;
        }
        
        try {
            auto token = mqtt_client_->subscribe(subscription.topic, QosToInt(subscription.qos));
            token->wait_for(std::chrono::seconds(5));
            
            LogMqttMessage(LogLevel::INFO, "Restored subscription to: " + subscription.topic);
            
        } catch (const std::exception& e) {
            LogMqttMessage(LogLevel::WARN, 
                "Failed to restore subscription to " + subscription.topic + ": " + e.what());
        }
    }
}


} // namespace Protocol
} // namespace Workers
} // namespace PulseOne