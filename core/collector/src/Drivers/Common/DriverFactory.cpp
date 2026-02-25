// =============================================================================
// collector/src/Drivers/Common/DriverFactory.cpp
// 드라이버 팩토리 구현 (IProtocolDriver.h와 호환)
// =============================================================================

#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <algorithm>

#include <stdexcept>
#include <vector>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// Implementation of DriverFactory
// =============================================================================

DriverFactory &DriverFactory::GetInstance() {
  static DriverFactory instance;
  return instance;
}

DriverFactory::DriverFactory() = default;
DriverFactory::~DriverFactory() = default;

// Helper: 대문자로 변환
static std::string NormalizeKey(const std::string &key) {
  std::string normalized = key;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 ::toupper);
  return normalized;
}

bool DriverFactory::RegisterDriver(const std::string &protocol_name,
                                   DriverCreator creator) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string key = NormalizeKey(protocol_name);

  if (creators_.find(key) != creators_.end()) {
    LogManager::getInstance().Warn("[DriverFactory] Driver '" + key +
                                   "' is already registered.");
    return false;
  }
  creators_[key] = creator;
  LogManager::getInstance().Info("[DriverFactory] Registered Driver: '" + key +
                                 "'");
  return true;
}

bool DriverFactory::UnregisterDriver(const std::string &protocol_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  return creators_.erase(NormalizeKey(protocol_name)) > 0;
}

std::unique_ptr<IProtocolDriver>
DriverFactory::CreateDriver(const std::string &protocol_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string key = NormalizeKey(protocol_name);

  auto it = creators_.find(key);
  if (it != creators_.end()) {
    try {
      return it->second();
    } catch (const std::exception &e) {
      LogManager::getInstance().Error(
          "[DriverFactory] Exception creating driver '" + key +
          "': " + e.what());
      return nullptr;
    } catch (...) {
      return nullptr;
    }
  }

  std::string available;
  for (const auto &pair : creators_) {
    available += "'" + pair.first + "' ";
  }
  LogManager::getInstance().Error("[DriverFactory] CreateDriver failed: '" +
                                  key + "' not found. Available: " + available);

  return nullptr;
}

std::vector<std::string> DriverFactory::GetAvailableProtocols() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<std::string> protocols;
  protocols.reserve(creators_.size());

  for (const auto &pair : creators_) {
    protocols.push_back(pair.first);
  }

  return protocols;
}

bool DriverFactory::IsProtocolSupported(
    const std::string &protocol_name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return creators_.find(NormalizeKey(protocol_name)) != creators_.end();
}

size_t DriverFactory::GetDriverCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return creators_.size();
}

} // namespace Drivers
} // namespace PulseOne