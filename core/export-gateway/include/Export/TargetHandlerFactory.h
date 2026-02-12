/**
 * @file TargetHandlerFactory.h
 * @brief Target Handler Factory - PulseOne::Export
 */

#ifndef EXPORT_TARGET_HANDLER_FACTORY_H
#define EXPORT_TARGET_HANDLER_FACTORY_H

#include "Export/GatewayExportTypes.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace PulseOne {
namespace Export {

/**
 * @brief Factory for Target Handlers
 */
class TargetHandlerFactory {
public:
  static TargetHandlerFactory &getInstance() {
    static TargetHandlerFactory instance;
    return instance;
  }

  void registerCreator(const std::string &type, TargetHandlerCreator creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    creators_[type] = std::move(creator);
  }

  std::unique_ptr<ITargetHandler>
  createHandler(const std::string &type, const std::string &config = "{}") {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = creators_.find(type);
    if (it != creators_.end()) {
      return it->second(config);
    }
    return nullptr;
  }

private:
  TargetHandlerFactory() = default;
  ~TargetHandlerFactory() = default;

  std::mutex mutex_;
  std::map<std::string, TargetHandlerCreator> creators_;
};

} // namespace Export
} // namespace PulseOne

#endif // EXPORT_TARGET_HANDLER_FACTORY_H
