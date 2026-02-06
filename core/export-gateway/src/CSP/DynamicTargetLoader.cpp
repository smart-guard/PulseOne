/**
 * @file DynamicTargetLoader.cpp
 * @brief DynamicTargetLoader 구현 (Refactored from DynamicTargetManager)
 * @author PulseOne Development Team
 * @date 2026-01-31
 */

#include "CSP/DynamicTargetLoader.h"
#include "Constants/ExportConstants.h"
#include "Database/Entities/EdgeServerEntity.h"
#include "Database/Repositories/EdgeServerRepository.h"
#include "Database/RepositoryFactory.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>

namespace PulseOne {
namespace CSP {

using json = nlohmann::json;
namespace ExportConst = PulseOne::Constants::Export;
using namespace PulseOne::Database;
using namespace PulseOne::Database::Entities;

DynamicTargetLoader::DynamicTargetLoader() {}

DynamicTargetLoader::LoadedData DynamicTargetLoader::loadFromDatabase() {
  LoadedData result;

  try {
    LogManager::getInstance().Info(
        "DB에서 타겟 로딩 시작 (DynamicTargetLoader)...");

    auto &factory = RepositoryFactory::getInstance();

    // RepositoryFactory 초기화 확인
    if (!factory.isInitialized()) {
      if (!factory.initialize()) {
        LogManager::getInstance().Error("❌ RepositoryFactory 초기화 실패");
        return result;
      }
    }

    // 1. Repository 획득
    auto target_repo = factory.getExportTargetRepository();
    auto template_repo = factory.getPayloadTemplateRepository();
    auto mapping_repo = factory.getExportTargetMappingRepository();
    auto gateway_repo = factory.getEdgeServerRepository();

    if (!target_repo || !template_repo || !mapping_repo || !gateway_repo) {
      LogManager::getInstance().Error("❌ Repository 생성 실패");
      return result;
    }

    // [v3.2] Gateway-Scoped Priority Overrides 로드
    std::unordered_map<int, int> priority_overrides;
    if (gateway_id_ > 0) {
      auto gateway = gateway_repo->findById(gateway_id_);
      if (gateway.has_value()) {
        const auto &config = gateway->getConfig();
        if (config.contains("target_priorities") &&
            config["target_priorities"].is_object()) {
          for (auto it = config["target_priorities"].begin();
               it != config["target_priorities"].end(); ++it) {
            try {
              int target_id = std::stoi(it.key());
              int priority = it.value(); // Implicit conversion from json
              priority_overrides[target_id] = priority;
            } catch (...) {
            }
          }
          LogManager::getInstance().Info(
              "✅ 게이트웨이 전용 우선순위 오버라이드 로드됨 (개수: " +
              std::to_string(priority_overrides.size()) + ")");
        }
      }
    }

    // 2. 템플릿 로드
    std::map<int, PayloadTemplateEntity> template_map =
        loadPayloadTemplates(template_repo.get());

    // 3. 매핑 로드 (LoadedData의 캐시 업데이트)
    auto point_mappings = loadMappings(mapping_repo.get(), result);

    // 4. 타겟 엔티티 조회
    auto entities = fetchTargets(target_repo.get());

    if (entities.empty()) {
      LogManager::getInstance().Warn("활성화된 타겟이 없음");
      return result;
    }

    // 5. 타겟 리스트 생성
    int loaded_count = 0;
    for (const auto &entity : entities) {
      // Helper를 통해 엔티티 -> DynamicTarget 변환
      auto target =
          createTargetFromEntity(entity, template_map, point_mappings);

      // Handler 초기화 등은 Manager에서 수행해야 함 (Loader는 데이터만 로드)
      // 하지만 Manager의 loadFromDatabase 로직에서는 여기서 Handler도
      // 초기화했음. Manager가 HandlerFactory 의존성을 가지므로, Loader는
      // "DynamicTarget" 객체(설정 포함)만 구성해서 반환하고 Manager가 이를
      // 받아서 Handler를 생성/초기화하는 것이 맞음. 따라서 여기서는 순수한 Data
      // Object만 생성.

      // Apply Priority Override
      if (priority_overrides.count(entity.getId())) {
        target.execution_order = priority_overrides[entity.getId()];
      }

      result.targets.push_back(std::move(target));
      loaded_count++;
    }

    // 실행 순서 정렬
    std::sort(result.targets.begin(), result.targets.end(),
              [](const PulseOne::Export::DynamicTarget &a,
                 const PulseOne::Export::DynamicTarget &b) {
                if (a.execution_order != b.execution_order)
                  return a.execution_order < b.execution_order;
                return a.priority < b.priority;
              });

    // 할당된 디바이스 ID 목록 갱신 (Selective Subscription)
    // NOTE: This logic uses DatabaseManager directly in original code.
    // Ideally should be in Repository, but moving as-is for now.
    result.assigned_device_ids.clear();
    if (gateway_id_ > 0) {
      auto &db_manager = DbLib::DatabaseManager::getInstance();
      std::string device_query =
          "SELECT DISTINCT dp.device_id "
          "FROM data_points dp "
          "JOIN export_target_mappings etm ON dp.id = etm.point_id "
          "JOIN export_targets et ON etm.target_id = et.id "
          "JOIN export_profile_assignments epa ON et.profile_id = "
          "epa.profile_id "
          "WHERE epa.gateway_id = " +
          std::to_string(gateway_id_) +
          " AND et.is_enabled = 1 AND etm.is_enabled = 1";

      std::vector<std::vector<std::string>> device_result;
      if (db_manager.executeQuery(device_query, device_result)) {
        for (const auto &row : device_result) {
          if (!row[0].empty())
            result.assigned_device_ids.insert(row[0]);
        }
      }
    }

    LogManager::getInstance().Info(
        "✅ 타겟 로딩 완료 (Loader): " + std::to_string(loaded_count) + "개");

    return result;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("DB 로드 중 예외 발생: " +
                                    std::string(e.what()));
    return result;
  }
}

std::map<int, PayloadTemplateEntity> DynamicTargetLoader::loadPayloadTemplates(
    PulseOne::Database::Repositories::PayloadTemplateRepository *repo) {
  std::map<int, PayloadTemplateEntity> template_map;
  if (!repo)
    return template_map;

  auto templates = repo->findAll();
  for (const auto &tmpl : templates) {
    // 템플릿 엔티티 자체를 맵에 저장 (JSON 파싱은 createTargetFromEntity에서
    // 수행됨 or 여기서 미리 파싱?) Original logic: template_map[tmpl.getId()] =
    // json::parse(tmpl.getTemplateJson()); But signature expects Entity map.
    // Wait, Manager caching logic used std::map<int, json>. My header signature
    // used Entity map. Let's stick to Entity map to keep Loader generic, OR
    // parse it here. Getting JSON string is enough.
    template_map[tmpl.getId()] = tmpl;
  }
  return template_map;
}

std::unordered_map<int, std::vector<ExportTargetMappingEntity>>
DynamicTargetLoader::loadMappings(
    PulseOne::Database::Repositories::ExportTargetMappingRepository *repo,
    LoadedData &data) {
  std::unordered_map<int, std::vector<ExportTargetMappingEntity>> mappings_map;
  if (!repo)
    return mappings_map;

  // Clear caches in data
  data.target_point_mappings.clear();
  data.target_point_site_mappings.clear();
  data.target_site_mappings.clear();

  auto mappings = repo->findAll();
  for (const auto &m : mappings) {
    if (m.isEnabled()) {
      mappings_map[m.getTargetId()].push_back(m);

      // 캐시 갱신 (data 구조체에 저장)
      if (m.getPointId().has_value()) {
        data.target_point_mappings[m.getTargetId()][m.getPointId().value()] =
            m.getTargetFieldName();
        if (m.getSiteId().has_value()) {
          data.target_point_site_mappings[m.getTargetId()]
                                         [m.getPointId().value()] =
              m.getSiteId().value();
        }
      } else if (m.getSiteId().has_value()) {
        data.target_site_mappings[m.getTargetId()][m.getSiteId().value()] =
            m.getTargetFieldName();
      }
    }
  }
  return mappings_map;
}

std::vector<ExportTargetEntity> DynamicTargetLoader::fetchTargets(
    PulseOne::Database::Repositories::ExportTargetRepository *repo) {
  if (!repo)
    return {};

  LogManager::getInstance().Debug("fetchTargets - Gateway ID: " +
                                  std::to_string(gateway_id_));

  if (gateway_id_ > 0) {
    auto &db_manager = DbLib::DatabaseManager::getInstance();
    std::string query = "SELECT profile_id FROM export_profile_assignments "
                        "WHERE gateway_id = " +
                        std::to_string(gateway_id_);

    LogManager::getInstance().Debug("Query: " + query);

    std::vector<std::vector<std::string>> result;
    if (db_manager.executeQuery(query, result) && !result.empty()) {
      std::vector<ExportTargetEntity> all_targets;
      std::vector<int> profile_ids;

      for (const auto &row : result) {
        if (!row[0].empty()) {
          int profile_id = std::stoi(row[0]);
          // 중복 프로파일 ID 방지
          if (std::find(profile_ids.begin(), profile_ids.end(), profile_id) ==
              profile_ids.end()) {
            profile_ids.push_back(profile_id);
            auto profile_targets = repo->findByProfileId(profile_id);
            all_targets.insert(all_targets.end(), profile_targets.begin(),
                               profile_targets.end());
            LogManager::getInstance().Info(
                "Added targets from profile_id: " + std::to_string(profile_id) +
                " (Total targets so far: " +
                std::to_string(all_targets.size()) + ")");
          }
        }
      }
      return all_targets;
    }
    LogManager::getInstance().Warn(
        "📡 No profile assignment found for gateway: " +
        std::to_string(gateway_id_));
    return {};
  } else {
    LogManager::getInstance().Info(
        "📡 Fetching all enabled targets (Gateway ID <= 0)");
    return repo->findByEnabled(true);
  }
}

PulseOne::Export::DynamicTarget DynamicTargetLoader::createTargetFromEntity(
    const ExportTargetEntity &entity,
    const std::map<int, PayloadTemplateEntity> &templates,
    const std::unordered_map<int, std::vector<ExportTargetMappingEntity>>
        &mappings) {

  PulseOne::Export::DynamicTarget target;
  // Initialize default

  try {
    target.id = entity.getId();
    target.name = entity.getName();
    target.type = entity.getTargetType();
    std::transform(target.type.begin(), target.type.end(), target.type.begin(),
                   ::toupper);
    std::transform(target.type.begin(), target.type.end(), target.type.begin(),
                   ::toupper);
    target.enabled = entity.isEnabled();
    target.execution_order =
        100; // Default runtime priority (obsolete DB field removed)
    target.execution_delay_ms = entity.getExecutionDelayMs();
    target.priority = 100;
    target.description = entity.getDescription();

    // Config 파싱
    target.config = json::parse(entity.getConfig());
    if (target.config.is_array() && !target.config.empty()) {
      target.config = target.config[0];
    }

    // Export Mode 우선순위: Config > Entity Column
    if (!target.config.contains(ExportConst::ConfigKeys::EXPORT_MODE)) {
      target.config[ExportConst::ConfigKeys::EXPORT_MODE] =
          entity.getExportMode();
    }

    // 만약 여전히 비어있다면 기본값 ALARM
    if (target.config.value(ExportConst::ConfigKeys::EXPORT_MODE, "").empty()) {
      target.config[ExportConst::ConfigKeys::EXPORT_MODE] =
          ExportConst::ExportMode::ALARM;
    }

    // 기본 설정 주입
    if (!target.config.contains(ExportConst::ConfigKeys::MAX_QUEUE_SIZE))
      target.config[ExportConst::ConfigKeys::MAX_QUEUE_SIZE] = 1000;
    if (!target.config.contains(ExportConst::ConfigKeys::BATCH_SIZE))
      target.config[ExportConst::ConfigKeys::BATCH_SIZE] = 10;
    if (!target.config.contains(ExportConst::ConfigKeys::FLUSH_INTERVAL))
      target.config[ExportConst::ConfigKeys::FLUSH_INTERVAL] = 5000;
    if (!target.config.contains(ExportConst::ConfigKeys::RETRY_COUNT))
      target.config[ExportConst::ConfigKeys::RETRY_COUNT] = 3;

    // 템플릿 적용
    if (entity.getTemplateId().has_value()) {
      int tmpl_id = entity.getTemplateId().value();
      if (templates.count(tmpl_id)) {
        // Original expected json map, but now we have Entity map.
        // We need to parse json string from entity.
        try {
          target.config[ExportConst::ConfigKeys::BODY_TEMPLATE] =
              json::parse(templates.at(tmpl_id).getTemplateJson());
        } catch (...) {
          LogManager::getInstance().Warn(
              "Template parsing failed for target: " + target.name);
        }
      }
    }

    // 매핑 적용
    if (mappings.count(target.id)) {
      json mapping_config = json::array();
      for (const auto &m : mappings.at(target.id)) {
        json item;
        item[ExportConst::ConfigKeys::TARGET_FIELD] = m.getTargetFieldName();
        if (m.getPointId())
          item[ExportConst::ConfigKeys::POINT_ID] = *m.getPointId();
        if (m.getSiteId())
          item[ExportConst::ConfigKeys::SITE_ID] = *m.getSiteId();
        mapping_config.push_back(item);
      }
      target.config[ExportConst::ConfigKeys::FIELD_MAPPINGS] = mapping_config;
    }

    return target;
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("타겟 변환 실패 (" + entity.getName() +
                                    "): " + e.what());
    // Return empty/invalid target or rethrow? Original code returned
    // std::nullopt. Here we return object, caller handles? Let's assume valid
    // config is essential. If failed, we return 'empty' target, loop skips it?
    // Actually, createTargetFromEntity in Manager returned optional.
    // Here we return value.
    // Let's mark it disabled if invalid.
    target.enabled = false;
    return target;
  }
}

} // namespace CSP
} // namespace PulseOne
