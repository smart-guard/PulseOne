// collector/src/RedisClientImpl.cpp
#include "RedisClientImpl.h"
#include <iostream>

RedisClientImpl::RedisClientImpl() 
    : connected_(false), host_(""), port_(0), password_("") {
}

RedisClientImpl::~RedisClientImpl() {
    disconnect();
}

bool RedisClientImpl::connect(const std::string& host, int port, const std::string& password) {
    host_ = host;
    port_ = port;
    password_ = password;
    
    // 임시 구현: 항상 연결 성공으로 처리
    connected_ = true;
    
    std::cout << "[RedisClientImpl] Connected to " << host << ":" << port << std::endl;
    return true;
}

bool RedisClientImpl::set(const std::string& key, const std::string& value) {
    if (!connected_) {
        return false;
    }
    
    // 임시 구현: 메모리에 저장
    memory_storage_[key] = value;
    
    std::cout << "[RedisClientImpl] SET " << key << " = " << value << std::endl;
    return true;
}

std::string RedisClientImpl::get(const std::string& key) {
    if (!connected_) {
        return "";
    }
    
    // 임시 구현: 메모리에서 조회
    auto it = memory_storage_.find(key);
    if (it != memory_storage_.end()) {
        std::cout << "[RedisClientImpl] GET " << key << " = " << it->second << std::endl;
        return it->second;
    }
    
    std::cout << "[RedisClientImpl] GET " << key << " = (not found)" << std::endl;
    return "";
}

void RedisClientImpl::disconnect() {
    if (connected_) {
        connected_ = false;
        memory_storage_.clear();
        std::cout << "[RedisClientImpl] Disconnected from " << host_ << ":" << port_ << std::endl;
    }
}

bool RedisClientImpl::publish(const std::string& channel, const std::string& message) {
    if (!connected_) {
        return false;
    }
    
    // 임시 구현: 콘솔에 출력만
    std::cout << "[RedisClientImpl] PUBLISH " << channel << " -> " << message << std::endl;
    return true;
}

bool RedisClientImpl::subscribe(const std::string& channel) {
    if (!connected_) {
        return false;
    }
    
    // 임시 구현: 콘솔에 출력만
    std::cout << "[RedisClientImpl] SUBSCRIBE " << channel << std::endl;
    return true;
}

bool RedisClientImpl::unsubscribe(const std::string& channel) {
    if (!connected_) {
        return false;
    }
    
    // 임시 구현: 콘솔에 출력만
    std::cout << "[RedisClientImpl] UNSUBSCRIBE " << channel << std::endl;
    return true;
}