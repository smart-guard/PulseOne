/**
 * @file TargetRegistry.cpp
 * @brief Target Registry Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/TargetRegistry.h"
#include "CSP/DynamicTargetLoader.h"
#include "Gateway/Target/FileTargetHandler.h"
#include "Gateway/Target/HttpTargetHandler.h"
#include "Gateway/Target/ITargetHandler.h"
#include "Gateway/Target/MqttTargetHandler.h"
#include "Gateway/Target/S3TargetHandler.h"
#include "Logging/LogManager.h"
#include <algorithm>

namespace PulseOne {
namespace Gateway {
namespace Service {

TargetRegistry::TargetRegistry(int gateway_id) : gateway_id_(gateway_id) {
  LogManager::getInstance().Info("TargetRegistry initialized for gateway: " +
                                 std::to_string(gateway_id));
}

TargetRegistry::~TargetRegistry() {}

bool TargetRegistry::loadFromDatabase() {
  try {
    PulseOne::CSP::DynamicTargetLoader loader;
    loader.setGatewayId(gateway_id_);

    auto data = loader.loadFromDatabase();

    std::unique_lock<std::shared_mutex> lock(targets_mutex_);
    targets_ = std::move(data.targets);

    {
      std::unique_lock<std::shared_mutex> m_lock(mappings_mutex_);
      target_point_mappings_ = std::move(data.target_point_mappings);
      target_point_site_mappings_ = std::move(data.target_point_site_mappings);
      target_site_mappings_ = std::move(data.target_site_mappings);
      assigned_device_ids_ = std::move(data.assigned_device_ids);
    }

    initializeHandlers();

    LogManager::getInstance().Info("TargetRegistry: Loaded " +
                                   std::to_string(targets_.size()) +
                                   " targets from database");
    return true;
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("TargetRegistry: Load failed: " +
                                    std::string(e.what()));
    return false;
  }
}

std::optional<DynamicTarget>
TargetRegistry::getTarget(const std::string &name) const {
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);
  auto it = std::find_if(targets_.begin(), targets_.end(),
                         [&](const auto &t) { return t.name == name; });
  if (it != targets_.end())
    return *it;
  return std::nullopt;
}

std::vector<DynamicTarget> TargetRegistry::getAllTargets() const {
  std::shared_lock<std::shared_mutex> lock(targets_mutex_);
  return targets_;
}

std::set<std::string> TargetRegistry::getAssignedDeviceIds() const {
  std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
  return assigned_device_ids_;
}

PulseOne::Gateway::Target::ITargetHandler *
TargetRegistry::getHandler(const std::string &target_name) const {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  auto it = handlers_.find(target_name);
  if (it != handlers_.end())
    return it->second.get();
  return nullptr;
}

std::string TargetRegistry::getTargetFieldName(int target_id,
                                               int point_id) const {
  std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
  auto it1 = target_point_mappings_.find(target_id);
  if (it1 != target_point_mappings_.end()) {
    auto it2 = it1->second.find(point_id);
    if (it2 != it1->second.end())
      return it2->second;
  }
  return "";
}

bool TargetRegistry::isPointMapped(int target_id, int point_id) const {
  std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
  auto it1 = target_point_mappings_.find(target_id);
  if (it1 != target_point_mappings_.end()) {
    return it1->second.find(point_id) != it1->second.end();
  }
  return false;
}

int TargetRegistry::getOverrideSiteId(int target_id, int point_id) const {
  std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
  auto it1 = target_point_site_mappings_.find(target_id);
  if (it1 != target_point_site_mappings_.end()) {
    auto it2 = it1->second.find(point_id);
    if (it2 != it1->second.end())
      return it2->second;
  }
  return -1;
}

std::string TargetRegistry::getExternalBuildingId(int target_id,
                                                  int site_id) const {
  std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
  auto it1 = target_site_mappings_.find(target_id);
  if (it1 != target_site_mappings_.end()) {
    auto it2 = it1->second.find(site_id);
    if (it2 != it1->second.end())
      return it2->second;
  }
  return "";
}

void TargetRegistry::initializeHandlers() {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  handlers_.clear();

  for (const auto &target : targets_) {
    std::unique_ptr<PulseOne::Gateway::Target::ITargetHandler> handler;
    if (target.type == "HTTP")
      handler =
          std::make_unique<PulseOne::Gateway::Target::HttpTargetHandler>();
    else if (target.type == "S3")
      handler = std::make_unique<PulseOne::Gateway::Target::S3TargetHandler>();
    else if (target.type == "MQTT")
      handler =
          std::make_unique<PulseOne::Gateway::Target::MqttTargetHandler>();
    else if (target.type == "FILE")
      handler =
          std::make_unique<PulseOne::Gateway::Target::FileTargetHandler>();

    if (handler) {
      if (handler->initialize(target.config)) {
        handlers_[target.name] = std::move(handler);
      } else {
        LogManager::getInstance().Warn(
            "TargetRegistry: Failed to initialize handler for " + target.name);
      }
    }
  }
}

} // namespace Service
} // namespace Gateway
} // namespace PulseOne
