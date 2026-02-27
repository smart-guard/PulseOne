/**
 * @file TargetRegistry.cpp
 * @brief Target Registry Implementation - PulseOne::Gateway::Service
 */

#include "Gateway/Service/TargetRegistry.h"
// #include "CSP/DynamicTargetLoader.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Database/RepositoryFactory.h"
#include "Gateway/Target/FileTargetHandler.h"
#include "Gateway/Target/HttpTargetHandler.h"
#include "Gateway/Target/ITargetHandler.h"
#include "Gateway/Target/MqttTargetHandler.h"
#include "Gateway/Target/S3TargetHandler.h"
#include "Logging/LogManager.h"
#include <algorithm>
#include <climits>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Gateway {
namespace Service {

TargetRegistry::TargetRegistry(int gateway_id) : gateway_id_(gateway_id) {
  LogManager::getInstance().Info("TargetRegistry initialized for gateway: " +
                                 std::to_string(gateway_id));
}

TargetRegistry::~TargetRegistry() {}

void TargetRegistry::setTargetPriorities(
    const std::map<int, int> &priority_map) {
  // priority_map: {target_id -> priority_value}. Lower value = dispatch first.
  // Build a sorted list: {priority_value, target_id} then extract target_ids
  std::vector<std::pair<int, int>> pv_list; // {priority_val, target_id}
  allowed_target_ids_.clear();
  ordered_target_ids_.clear();
  for (const auto &kv : priority_map) {
    pv_list.push_back(std::make_pair(kv.second, kv.first));
    allowed_target_ids_.insert(kv.first);
  }
  // Sort by priority value (ascending)
  for (int i = 0; i < (int)pv_list.size(); ++i) {
    for (int j = i + 1; j < (int)pv_list.size(); ++j) {
      if (pv_list[j].first < pv_list[i].first) {
        std::pair<int, int> tmp = pv_list[i];
        pv_list[i] = pv_list[j];
        pv_list[j] = tmp;
      }
    }
  }
  for (int i = 0; i < (int)pv_list.size(); ++i) {
    ordered_target_ids_.push_back(pv_list[i].second);
  }
}

bool TargetRegistry::loadFromDatabase() {
  try {
    auto &factory = Database::RepositoryFactory::getInstance();
    auto target_repo = factory.getExportTargetRepository();
    auto mapping_repo = factory.getExportTargetMappingRepository();
    auto payload_repo = factory.getPayloadTemplateRepository();

    if (!target_repo || !mapping_repo) {
      LogManager::getInstance().Error(
          "TargetRegistry: Repositories not available");
      return false;
    }

    auto db_targets = target_repo->findByEnabled(true);
    std::vector<DynamicTarget> new_targets;

    // Mappings containers
    std::unordered_map<int, std::unordered_map<int, std::string>>
        new_point_mappings;
    std::unordered_map<int, std::unordered_map<int, int>>
        new_point_site_mappings;
    std::unordered_map<int, std::unordered_map<int, std::string>>
        new_site_mappings;
    std::unordered_map<int, std::unordered_map<int, double>>
        new_point_scale_mappings;
    std::unordered_map<int, std::unordered_map<int, double>>
        new_point_offset_mappings;
    std::set<std::string> new_assigned_devices;
    // [v3.2] point_id → device_id 조회를 위한 DataPointRepository
    auto dp_repo = factory.getDataPointRepository();

    for (const auto &db_target : db_targets) {
      // [v3.2 FIX] edge_servers.config target_priorities 필터링
      // 허용 ID 셋이 비어있으면 전체 허용 (fallback)
      if (!allowed_target_ids_.empty() &&
          allowed_target_ids_.find(db_target.getId()) ==
              allowed_target_ids_.end()) {
        LogManager::getInstance().Info(
            "TargetRegistry: Skipping target id=" +
            std::to_string(db_target.getId()) + " (" + db_target.getName() +
            ") - not assigned to gateway " + std::to_string(gateway_id_));
        continue;
      }

      DynamicTarget target;
      target.id = db_target.getId();
      target.name = db_target.getName();
      target.type = db_target.getTargetType();
      target.is_active = db_target.isEnabled();

      try {
        auto raw = nlohmann::json::parse(db_target.getConfig());
        // DB config가 [{...}] 배열인 경우 첫 번째 element unwrap
        if (raw.is_array() && !raw.empty())
          target.config = raw[0];
        else
          target.config = raw;
      } catch (...) {
        LogManager::getInstance().Error(
            "TargetRegistry: Config parse failed for " + target.name);
        continue;
      }

      // Payload Template logic has been decoupled from targets.
      // Templates should be resolved at the profile assignment level if needed.

      // Load Mappings for this target
      auto mappings = mapping_repo->findByTargetId(target.id);
      for (const auto &m : mappings) {
        if (!m.isEnabled())
          continue;

        auto point_id_opt = m.getPointId();
        if (point_id_opt) {
          int pid = point_id_opt.value();

          // [FIX] Point Name (Field Name) Mapping
          std::string mapped_nm = m.getTargetFieldName();
          if (!mapped_nm.empty()) {
            new_point_mappings[target.id][pid] = mapped_nm;
          }

          // [FIX] Override Site ID Mapping
          auto site_id_opt = m.getSiteId();
          if (site_id_opt.has_value() && site_id_opt.value() > 0) {
            new_point_site_mappings[target.id][pid] = site_id_opt.value();
            LogManager::getInstance().Info(
                "TargetRegistry: SiteID override loaded: Target=" +
                target.name + " Point=" + std::to_string(pid) +
                " -> SiteID=" + std::to_string(site_id_opt.value()));
          }

          // [FIX] Scale/Offset from conversion_config JSON
          std::string conv_cfg = m.getConversionConfig();
          if (!conv_cfg.empty()) {
            try {
              auto j = nlohmann::json::parse(conv_cfg);
              double scale = j.value("scale", 1.0);
              double offset = j.value("offset", 0.0);
              new_point_scale_mappings[target.id][pid] = scale;
              new_point_offset_mappings[target.id][pid] = offset;
            } catch (...) {
            }
          }
          // [v3.2 FIX] point_id → device_id 조회하여 assigned_device_ids_
          // 채우기 Selective 모드 시 device:<ID>:alarms 채널 구독에 사용
          if (dp_repo && point_id_opt) {
            int pid = point_id_opt.value();
            try {
              auto dp_entity = dp_repo->findById(pid);
              if (dp_entity) {
                new_assigned_devices.insert(
                    std::to_string(dp_entity->getDeviceId()));
              }
            } catch (...) {
              // 개별 포인트 조회 실패는 무시
            }
          }
        }
      }

      new_targets.push_back(target);
    }

    // [v3.2] ordered_target_ids_ 기반으로 정렬 (priority ASC 순서)
    if (!ordered_target_ids_.empty()) {
      std::vector<DynamicTarget> sorted_targets;
      sorted_targets.reserve(new_targets.size());
      // ordered_target_ids_ 순서대로 new_targets에서 해당 target 찾아 추가
      for (int i = 0; i < (int)ordered_target_ids_.size(); ++i) {
        int tid = ordered_target_ids_[i];
        for (int j = 0; j < (int)new_targets.size(); ++j) {
          if (new_targets[j].id == tid) {
            sorted_targets.push_back(new_targets[j]);
            break;
          }
        }
      }
      // ordered에 없는 나머지 targets 뒤에 추가
      for (int j = 0; j < (int)new_targets.size(); ++j) {
        bool found = false;
        for (int k = 0; k < (int)ordered_target_ids_.size(); ++k) {
          if (new_targets[j].id == ordered_target_ids_[k]) {
            found = true;
            break;
          }
        }
        if (!found)
          sorted_targets.push_back(new_targets[j]);
      }
      new_targets = sorted_targets;
    }

    {
      std::unique_lock<std::shared_mutex> lock(targets_mutex_);
      targets_ = std::move(new_targets);
    }

    {
      std::unique_lock<std::shared_mutex> m_lock(mappings_mutex_);
      target_point_mappings_ = std::move(new_point_mappings);
      target_point_site_mappings_ =
          std::move(new_point_site_mappings); // [FIX] was commented out
      target_site_mappings_ = std::move(new_site_mappings);
      target_point_scale_mappings_ = std::move(new_point_scale_mappings);
      target_point_offset_mappings_ = std::move(new_point_offset_mappings);
      assigned_device_ids_ = std::move(new_assigned_devices);
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

double TargetRegistry::getScale(int target_id, int point_id) const {
  std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
  auto it1 = target_point_scale_mappings_.find(target_id);
  if (it1 != target_point_scale_mappings_.end()) {
    auto it2 = it1->second.find(point_id);
    if (it2 != it1->second.end())
      return it2->second;
  }
  return 1.0; // default: no scaling
}

double TargetRegistry::getOffset(int target_id, int point_id) const {
  std::shared_lock<std::shared_mutex> lock(mappings_mutex_);
  auto it1 = target_point_offset_mappings_.find(target_id);
  if (it1 != target_point_offset_mappings_.end()) {
    auto it2 = it1->second.find(point_id);
    if (it2 != it1->second.end())
      return it2->second;
  }
  return 0.0; // default: no offset
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
