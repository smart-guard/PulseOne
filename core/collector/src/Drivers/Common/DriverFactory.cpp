// =============================================================================
// collector/src/Drivers/Common/DriverFactory.cpp
// 드라이버 팩토리 구현 (IProtocolDriver.h와 호환)
// =============================================================================

#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <vector>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// Implementation of DriverFactory
// =============================================================================

DriverFactory& DriverFactory::GetInstance() {
    static DriverFactory instance;
    return instance;
}

DriverFactory::DriverFactory() = default;
DriverFactory::~DriverFactory() = default;

// Helper: 대문자로 변환
static std::string NormalizeKey(const std::string& key) {
    std::string normalized = key;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
    return normalized;
}

bool DriverFactory::RegisterDriver(ProtocolType protocol, DriverCreator creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (creators_.find(protocol) != creators_.end()) {
        std::cout << "[DriverFactory] Protocol Enum " << static_cast<int>(protocol) << " is already registered." << std::endl;
        return false;
    }
    
    creators_[protocol] = creator;
    std::cout << "[DriverFactory] Registered Protocol Enum: " << static_cast<int>(protocol) << std::endl;
    return true;
}

bool DriverFactory::UnregisterDriver(ProtocolType protocol) {
    std::lock_guard<std::mutex> lock(mutex_);
    return creators_.erase(protocol) > 0;
}

bool DriverFactory::RegisterDriver(const std::string& protocol_name, DriverCreator creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = NormalizeKey(protocol_name);
    
    if (custom_creators_.find(key) != custom_creators_.end()) {
        std::cout << "[DriverFactory] Driver '" << key << "' (raw: " << protocol_name << ") is already registered." << std::endl;
        return false;
    }
    custom_creators_[key] = creator;
    std::cout << "[DriverFactory] Registered Driver String: '" << key << "' (raw: " << protocol_name << ")" << std::endl;
    return true;
}

bool DriverFactory::UnregisterDriver(const std::string& protocol_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return custom_creators_.erase(NormalizeKey(protocol_name)) > 0;
}

std::unique_ptr<IProtocolDriver> DriverFactory::CreateDriver(ProtocolType protocol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = creators_.find(protocol);
    if (it == creators_.end()) {
        std::cout << "[DriverFactory] CreateDriver failed: Protocol Enum " << static_cast<int>(protocol) << " not found." << std::endl;
        return nullptr;
    }
    
    try {
        return it->second();
    } catch (const std::exception& e) {
        std::cout << "[DriverFactory] Exception creating driver for Protocol Enum " << static_cast<int>(protocol) << ": " << e.what() << std::endl;
        return nullptr;
    }
}

std::unique_ptr<IProtocolDriver> DriverFactory::CreateDriver(const std::string& protocol_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = NormalizeKey(protocol_name);
    
    auto it = custom_creators_.find(key);
    if (it != custom_creators_.end()) {
        try {
            return it->second();
        } catch (const std::exception& e) {
            std::cout << "[DriverFactory] Exception creating driver '" << key << "': " << e.what() << std::endl;
            return nullptr;
        } catch (...) {
            return nullptr;
        }
    }
    
    std::cout << "[DriverFactory] CreateDriver failed: '" << key << "' (raw: " << protocol_name << ") not found. Available keys: ";
    for (const auto& pair : custom_creators_) {
        std::cout << "'" << pair.first << "' ";
    }
    std::cout << std::endl;

    return nullptr;
}

std::vector<ProtocolType> DriverFactory::GetAvailableProtocols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ProtocolType> protocols;
    protocols.reserve(creators_.size());
    
    for (const auto& pair : creators_) {
        protocols.push_back(pair.first);
    }
    
    return protocols;
}

bool DriverFactory::IsProtocolSupported(ProtocolType protocol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return creators_.find(protocol) != creators_.end();
}

size_t DriverFactory::GetDriverCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return creators_.size();
}

} // namespace Drivers
} // namespace PulseOne