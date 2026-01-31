#pragma once

/**
 * @file DynamicTargetLoader.h
 * @brief DynamicTargetManager의 DB 로딩 로직을 분리한 클래스
 * @author PulseOne Development Team
 * @date 2026-01-31
 */

#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/PayloadTemplateEntity.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Export/ExportTypes.h"
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace CSP {

class DynamicTargetLoader {
public:
  struct LoadedData {
    std::vector<PulseOne::Export::DynamicTarget> targets;

    // Mapping Caches
    std::unordered_map<int, std::unordered_map<int, std::string>>
        target_point_mappings;
    std::unordered_map<int, std::unordered_map<int, int>>
        target_point_site_mappings;
    std::unordered_map<int, std::unordered_map<int, std::string>>
        target_site_mappings;

    // Device IDs for selective subscription
    std::set<std::string> assigned_device_ids;
  };

  DynamicTargetLoader();
  ~DynamicTargetLoader() = default;

  /**
   * @brief DB에서 타겟 및 관련 정보를 로드합니다.
   * @return 로드된 데이터 구조체 (실패 시 빈 벡터/맵)
   */
  LoadedData loadFromDatabase();

private:
  // 내부 헬퍼 메서드
  std::map<int, PulseOne::Database::Entities::PayloadTemplateEntity>
  loadPayloadTemplates(
      PulseOne::Database::Repositories::PayloadTemplateRepository *repo);

  std::unordered_map<
      int, std::vector<PulseOne::Database::Entities::ExportTargetMappingEntity>>
  loadMappings(
      PulseOne::Database::Repositories::ExportTargetMappingRepository *repo,
      LoadedData &data);

  std::vector<PulseOne::Database::Entities::ExportTargetEntity>
  fetchTargets(PulseOne::Database::Repositories::ExportTargetRepository *repo);

public: // Moved to public as per instruction's implied placement
  PulseOne::Export::DynamicTarget createTargetFromEntity(
      const PulseOne::Database::Entities::ExportTargetEntity &entity,
      const std::map<int, PulseOne::Database::Entities::PayloadTemplateEntity>
          &templates,
      const std::unordered_map<
          int,
          std::vector<PulseOne::Database::Entities::ExportTargetMappingEntity>>
          &mappings);

  void setGatewayId(int id) { gateway_id_ = id; }

private:
  int gateway_id_{0};
};

} // namespace CSP
} // namespace PulseOne
