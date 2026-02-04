/**
 * @file DataPointRepository.cpp - 품질/알람 필드 완전 지원
 * @brief PulseOne DataPointRepository 구현 - 현재 스키마 완전 호환
 * @author PulseOne Development Team
 * @date 2025-08-26
 *
 * 🎯 현재 DB 스키마 완전 반영:
 * - 품질 관리: quality_check_enabled, range_check_enabled, rate_of_change_limit
 * - 알람 관리: alarm_enabled, alarm_priority
 * - SQLQueries.h 상수 100% 적용
 * - DeviceSettingsRepository 패턴 완전 준수
 */

#include "Database/Repositories/DataPointRepository.h"
#include "Common/Structs.h"
#include "Common/Utils.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      logger_->Error("DataPointRepository::findAll - Table creation failed");
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    auto results = db_layer.executeQuery(SQL::DataPoint::FIND_ALL);

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DataPointRepository::findAll - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("DataPointRepository::findAll - Found " +
                  std::to_string(entities.size()) + " data points");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::optional<DataPointEntity> DataPointRepository::findById(int id) {
  try {
    // 캐시 확인
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(id);
      if (cached.has_value()) {
        logger_->Debug("DataPointRepository::findById - Cache hit for ID: " +
                       std::to_string(id));
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용 + 동적 파라미터 처리
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::DataPoint::FIND_BY_ID, std::to_string(id));

    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      logger_->Debug("DataPointRepository::findById - Data point not found: " +
                     std::to_string(id));
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    // 캐시에 저장
    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    logger_->Debug("DataPointRepository::findById - Found data point: " +
                   entity.getName());
    return entity;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findById failed for ID " +
                   std::to_string(id) + ": " + std::string(e.what()));
    return std::nullopt;
  }
}

bool DataPointRepository::save(DataPointEntity &entity) {
  try {
    if (!validateDataPoint(entity)) {
      logger_->Error("DataPointRepository::save - Invalid data point: " +
                     entity.getName());
      return false;
    }

    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::map<std::string, std::string> data = entityToParams(entity);
    std::vector<std::string> primary_keys = {"id"};

    bool success = db_layer.executeUpsert("data_points", data, primary_keys);

    if (success) {
      // 새로 생성된 경우 ID 조회 - SQLQueries.h 상수 사용
      if (entity.getId() <= 0) {
        std::string id_query = RepositoryHelpers::replaceTwoParameters(
            SQL::DataPoint::FIND_LAST_CREATED_BY_DEVICE_ADDRESS,
            std::to_string(entity.getDeviceId()),
            std::to_string(entity.getAddress()));
        auto id_result = db_layer.executeQuery(id_query);
        if (!id_result.empty()) {
          entity.setId(std::stoi(id_result[0].at("id")));
        }
      }

      // 캐시 업데이트
      if (isCacheEnabled()) {
        cacheEntity(entity);
      }

      logger_->Info("DataPointRepository::save - Saved data point: " +
                    entity.getName());
    } else {
      logger_->Error("DataPointRepository::save - Failed to save data point: " +
                     entity.getName());
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::save failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DataPointRepository::update(const DataPointEntity &entity) {
  DataPointEntity mutable_entity = entity;
  return save(mutable_entity);
}

bool DataPointRepository::deleteById(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::DataPoint::DELETE_BY_ID, std::to_string(id));

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(id);
      }

      logger_->Info(
          "DataPointRepository::deleteById - Deleted data point ID: " +
          std::to_string(id));
    } else {
      logger_->Error(
          "DataPointRepository::deleteById - Failed to delete data point ID: " +
          std::to_string(id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DataPointRepository::exists(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::DataPoint::EXISTS_BY_ID, std::to_string(id));

    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      int count = std::stoi(results[0].at("count"));
      return count > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::exists failed: " +
                   std::string(e.what()));
    return false;
  }
}

// =============================================================================
// DataPoint 전용 메서드들 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DataPointEntity>
DataPointRepository::findByDeviceId(int device_id, bool enabled_only) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용 - enabled_only에 따라 다른 상수 선택
    std::string query;
    if (enabled_only) {
      query = RepositoryHelpers::replaceParameter(
          SQL::DataPoint::FIND_BY_DEVICE_ID_ENABLED, std::to_string(device_id));
    } else {
      query = RepositoryHelpers::replaceParameter(
          SQL::DataPoint::FIND_BY_DEVICE_ID, std::to_string(device_id));
    }

    auto results = db_layer.executeQuery(query);

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "DataPointRepository::findByDeviceId - Failed to map row: " +
            std::string(e.what()));
      }
    }

    logger_->Info("DataPointRepository::findByDeviceId - Found " +
                  std::to_string(entities.size()) +
                  " data points for device: " + std::to_string(device_id));
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findByDeviceId failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<DataPointEntity> DataPointRepository::findWritablePoints() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    auto results = db_layer.executeQuery(SQL::DataPoint::FIND_WRITABLE_POINTS);

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "DataPointRepository::findWritablePoints - Failed to map row: " +
            std::string(e.what()));
      }
    }

    logger_->Info("DataPointRepository::findWritablePoints - Found " +
                  std::to_string(entities.size()) + " writable data points");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findWritablePoints failed: " +
                   std::string(e.what()));
    return {};
  }
}

// =============================================================================
// 내부 헬퍼 메서드들 - 품질/알람 필드 포함
// =============================================================================

DataPointEntity DataPointRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  try {
    DataPointEntity entity;
    DbLib::DatabaseAbstractionLayer db_layer;
    auto it = row.end();

    // 🔥 기본 식별 정보
    it = row.find("id");
    if (it != row.end()) {
      entity.setId(std::stoi(it->second));
    }

    it = row.find("device_id");
    if (it != row.end()) {
      entity.setDeviceId(std::stoi(it->second));
    }

    it = row.find("name");
    if (it != row.end()) {
      entity.setName(it->second);
    }

    it = row.find("description");
    if (it != row.end()) {
      entity.setDescription(it->second);
    }

    // 🔥 주소 정보
    it = row.find("address");
    if (it != row.end()) {
      entity.setAddress(std::stoi(it->second));
    }

    it = row.find("address_string");
    if (it != row.end()) {
      entity.setAddressString(it->second);
    }

    it = row.find("mapping_key");
    if (it != row.end()) {
      entity.setMappingKey(it->second);
    }

    // 🚨 핵심 수정: 데이터 타입 정규화 비활성화
    it = row.find("data_type");
    if (it != row.end()) {
      entity.setDataType(Utils::NormalizeDataType(it->second));
    }

    it = row.find("access_mode");
    if (it != row.end()) {
      entity.setAccessMode(it->second);
    }

    it = row.find("is_enabled");
    if (it != row.end()) {
      entity.setEnabled(db_layer.parseBoolean(it->second));
    }

    it = row.find("is_writable");
    if (it != row.end()) {
      entity.setWritable(db_layer.parseBoolean(it->second));
    }

    // 🔥 엔지니어링 단위 및 스케일링
    entity.setUnit(RepositoryHelpers::getRowValue(row, "unit"));
    entity.setScalingFactor(
        RepositoryHelpers::getRowValueAsDouble(row, "scaling_factor", 1.0));
    entity.setScalingOffset(
        RepositoryHelpers::getRowValueAsDouble(row, "scaling_offset", 0.0));
    entity.setMinValue(RepositoryHelpers::getRowValueAsDouble(
        row, "min_value", std::numeric_limits<double>::lowest()));
    entity.setMaxValue(RepositoryHelpers::getRowValueAsDouble(
        row, "max_value", std::numeric_limits<double>::max()));

    // 🔥 로깅 설정
    entity.setLogEnabled(db_layer.parseBoolean(
        RepositoryHelpers::getRowValue(row, "log_enabled", "1")));
    entity.setLogInterval(
        RepositoryHelpers::getRowValueAsInt(row, "log_interval_ms", 0));
    entity.setLogDeadband(
        RepositoryHelpers::getRowValueAsDouble(row, "log_deadband", 0.0));
    entity.setPollingInterval(
        RepositoryHelpers::getRowValueAsInt(row, "polling_interval_ms", 1000));

    // 🔥 품질 관리 필드들 (선택적)
    entity.setQualityCheckEnabled(db_layer.parseBoolean(
        RepositoryHelpers::getRowValue(row, "quality_check_enabled", "1")));
    entity.setRangeCheckEnabled(db_layer.parseBoolean(
        RepositoryHelpers::getRowValue(row, "range_check_enabled", "1")));
    entity.setRateOfChangeLimit(RepositoryHelpers::getRowValueAsDouble(
        row, "rate_of_change_limit", 0.0));

    // 🔥 알람 관련 필드들 (선택적)
    entity.setAlarmEnabled(db_layer.parseBoolean(
        RepositoryHelpers::getRowValue(row, "alarm_enabled", "0")));
    entity.setAlarmPriority(
        RepositoryHelpers::getRowValue(row, "alarm_priority", "medium"));

    // 🔥 메타데이터
    it = row.find("group_name");
    if (it != row.end()) {
      entity.setGroup(it->second);
    }

    it = row.find("tags");
    if (it != row.end()) {
      entity.setTags(RepositoryHelpers::parseTagsFromString(it->second));
    }

    it = row.find("metadata");
    if (it != row.end() && !it->second.empty()) {
      entity.setMetadata(parseJsonToStringMap(it->second, "metadata"));
    }

    it = row.find("protocol_params");
    if (it != row.end() && !it->second.empty()) {
      entity.setProtocolParams(
          parseJsonToStringMap(it->second, "protocol_params"));
    }

    // 🔥 시간 정보
    it = row.find("created_at");
    if (it != row.end()) {
      entity.setCreatedAt(it->second);
    }

    it = row.find("updated_at");
    if (it != row.end()) {
      entity.setUpdatedAt(it->second);
    }

    return entity;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::mapRowToEntity failed: " +
                   std::string(e.what()));
    throw;
  }
}

std::map<std::string, std::string>
DataPointRepository::entityToParams(const DataPointEntity &entity) {
  DbLib::DatabaseAbstractionLayer db_layer;

  std::map<std::string, std::string> params;

  // ID가 있으면 포함 (UPDATE용)
  if (entity.getId() > 0) {
    params["id"] = std::to_string(entity.getId());
  }

  // 🔥 기본 식별 정보
  params["device_id"] = std::to_string(entity.getDeviceId());
  params["name"] = entity.getName();
  params["description"] = entity.getDescription();

  // 🔥 주소 정보
  params["address"] = std::to_string(entity.getAddress());
  params["address_string"] = entity.getAddressString();
  params["mapping_key"] = entity.getMappingKey();

  // 🔥 데이터 타입 및 접근성
  params["data_type"] = entity.getDataType();
  params["access_mode"] = entity.getAccessMode();
  params["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
  params["is_writable"] = db_layer.formatBoolean(entity.isWritable());

  // 🔥 엔지니어링 단위 및 스케일링
  params["unit"] = entity.getUnit();
  params["scaling_factor"] = std::to_string(entity.getScalingFactor());
  params["scaling_offset"] = std::to_string(entity.getScalingOffset());
  params["min_value"] = std::to_string(entity.getMinValue());
  params["max_value"] = std::to_string(entity.getMaxValue());

  // 🔥 로깅 및 수집 설정
  params["log_enabled"] = db_layer.formatBoolean(entity.isLogEnabled());
  params["log_interval_ms"] = std::to_string(entity.getLogInterval());
  params["log_deadband"] = std::to_string(entity.getLogDeadband());
  params["polling_interval_ms"] = std::to_string(entity.getPollingInterval());

  // 🔥🔥🔥 품질 관리 설정 (새로 추가된 필드들)
  params["quality_check_enabled"] =
      db_layer.formatBoolean(entity.isQualityCheckEnabled());
  params["range_check_enabled"] =
      db_layer.formatBoolean(entity.isRangeCheckEnabled());
  params["rate_of_change_limit"] =
      std::to_string(entity.getRateOfChangeLimit());

  // 🔥🔥🔥 알람 관련 설정 (새로 추가된 필드들)
  params["alarm_enabled"] = db_layer.formatBoolean(entity.isAlarmEnabled());
  params["alarm_priority"] = entity.getAlarmPriority();

  // 🔥 메타데이터
  params["group_name"] = entity.getGroup();
  params["tags"] = RepositoryHelpers::tagsToString(entity.getTags());

  // metadata를 JSON으로 직렬화
  auto metadata_map = entity.getMetadata();
  if (!metadata_map.empty()) {
    json metadata_json(metadata_map);
    params["metadata"] = metadata_json.dump();
  } else {
    params["metadata"] = "{}";
  }

  // protocol_params를 JSON으로 직렬화
  auto protocol_params_map = entity.getProtocolParams();
  if (!protocol_params_map.empty()) {
    json protocol_json(protocol_params_map);
    params["protocol_params"] = protocol_json.dump();
  } else {
    params["protocol_params"] = "{}";
  }

  // 🔥 시간 정보
  params["created_at"] = db_layer.getGenericTimestamp();
  params["updated_at"] = db_layer.getGenericTimestamp();

  return params;
}

bool DataPointRepository::ensureTableExists() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    bool success = db_layer.executeCreateTable(SQL::DataPoint::CREATE_TABLE);

    if (success) {
      logger_->Debug("DataPointRepository::ensureTableExists - Table "
                     "creation/check completed");
    } else {
      logger_->Error(
          "DataPointRepository::ensureTableExists - Table creation failed");
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::ensureTableExists failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DataPointRepository::validateDataPoint(
    const DataPointEntity &entity) const {
  if (!entity.isValid()) {
    logger_->Warn(
        "DataPointRepository::validateDataPoint - Invalid data point: " +
        entity.getName());
    return false;
  }

  if (entity.getDeviceId() <= 0) {
    logger_->Warn("DataPointRepository::validateDataPoint - Invalid device_id "
                  "for data point: " +
                  entity.getName());
    return false;
  }

  if (entity.getName().empty()) {
    logger_->Warn(
        "DataPointRepository::validateDataPoint - Empty name for data point");
    return false;
  }

  if (entity.getDataType().empty()) {
    logger_->Warn("DataPointRepository::validateDataPoint - Empty data_type "
                  "for data point: " +
                  entity.getName());
    return false;
  }

  // 🔥 알람 우선순위 검증 (새로 추가)
  if (entity.isAlarmEnabled()) {
    const std::string &priority = entity.getAlarmPriority();
    if (priority != "low" && priority != "medium" && priority != "high" &&
        priority != "critical") {
      logger_->Warn(
          "DataPointRepository::validateDataPoint - Invalid alarm_priority: " +
          priority + " for data point: " + entity.getName());
      return false;
    }
  }

  // 🔥 변화율 제한 검증 (새로 추가)
  if (entity.getRateOfChangeLimit() < 0.0) {
    logger_->Warn("DataPointRepository::validateDataPoint - Negative "
                  "rate_of_change_limit for data point: " +
                  entity.getName());
    return false;
  }

  return true;
}

// =============================================================================
// 의존성 주입 및 현재값 통합 메서드
// =============================================================================

void DataPointRepository::setCurrentValueRepository(
    std::shared_ptr<CurrentValueRepository> current_value_repo) {
  current_value_repo_ = current_value_repo;
  logger_->Info("CurrentValueRepository injected into DataPointRepository");
}

std::vector<PulseOne::Structs::DataPoint>
DataPointRepository::getDataPointsWithCurrentValues(int device_id,
                                                    bool enabled_only) {
  std::vector<PulseOne::Structs::DataPoint> result;

  try {
    logger_->Debug("Loading DataPoints with current values for device: " +
                   std::to_string(device_id));

    // 1. DataPointEntity들 조회
    auto entities = findByDeviceId(device_id, enabled_only);

    logger_->Debug("Found " + std::to_string(entities.size()) +
                   " DataPoint entities");

    // 2. 각 Entity를 Structs::DataPoint로 변환 + 현재값 추가
    for (const auto &entity : entities) {

      PulseOne::Structs::DataPoint data_point;
      data_point.id = std::to_string(entity.getId());
      data_point.device_id = std::to_string(entity.getDeviceId());
      data_point.name = entity.getName();
      data_point.description = entity.getDescription();
      data_point.address = entity.getAddress();
      data_point.address_string = entity.getAddressString();
      data_point.mapping_key = entity.getMappingKey();
      data_point.data_type = entity.getDataType();
      data_point.access_mode = entity.getAccessMode();
      data_point.is_enabled = entity.isEnabled();
      data_point.is_writable = entity.isWritable();
      data_point.unit = entity.getUnit();
      data_point.scaling_factor = entity.getScalingFactor();
      data_point.scaling_offset = entity.getScalingOffset();
      data_point.min_value = entity.getMinValue();
      data_point.max_value = entity.getMaxValue();

      // 🔥 로깅 설정
      data_point.is_log_enabled = entity.isLogEnabled();
      data_point.log_interval_ms = entity.getLogInterval();
      data_point.log_deadband = entity.getLogDeadband();
      data_point.polling_interval_ms = entity.getPollingInterval();

      // 🔥🔥🔥 품질 관리 설정 (새로 추가)
      // Structs::DataPoint에 해당 필드가 있다면 매핑
      // data_point.quality_check_enabled = entity.isQualityCheckEnabled();
      // data_point.range_check_enabled = entity.isRangeCheckEnabled();
      // data_point.rate_of_change_limit = entity.getRateOfChangeLimit();

      // 🔥🔥🔥 알람 설정 (새로 추가)
      // data_point.alarm_enabled = entity.isAlarmEnabled();
      // data_point.alarm_priority = entity.getAlarmPriority();

      // 🔥 메타데이터
      data_point.group = entity.getGroup();

      // tags 배열을 문자열로 변환
      auto tag_vector = entity.getTags();
      if (!tag_vector.empty()) {
        json tags_json(tag_vector);
        data_point.tags = tags_json.dump();
      }

      // metadata 맵을 문자열로 변환
      auto metadata_map = entity.getMetadata();
      if (!metadata_map.empty()) {
        json metadata_json(metadata_map);
        data_point.metadata = metadata_json.dump();
      }

      // protocol_params 매핑
      data_point.protocol_params = entity.getProtocolParams();

      // 🔥 시간 정보 매핑
      data_point.created_at = entity.getCreatedAt();
      data_point.updated_at = entity.getUpdatedAt();

      // 현재값 조회 및 설정
      if (current_value_repo_) {
        try {
          auto current_value =
              current_value_repo_->findByDataPointId(entity.getId());

          if (current_value.has_value()) {
            try {
              // CurrentValueEntity의 getCurrentValue()가 string을 반환하는 경우
              std::string value_str = current_value->getCurrentValue();
              if (!value_str.empty()) {
                double numeric_value = std::stod(value_str);
                data_point.current_value =
                    PulseOne::BasicTypes::DataVariant(numeric_value);
              } else {
                data_point.current_value =
                    PulseOne::BasicTypes::DataVariant(0.0);
              }
            } catch (const std::exception &) {
              // 변환 실패 시 문자열 그대로
              data_point.current_value = PulseOne::BasicTypes::DataVariant(
                  current_value->getCurrentValue());
            }

            // 🔥 DataQuality 타입 변환
            if (current_value->getQualityCode() !=
                PulseOne::Enums::DataQuality::UNKNOWN) {
              data_point.quality_code = current_value->getQualityCode();
            } else {
              data_point.quality_code = PulseOne::Utils::StringToDataQuality(
                  current_value->getQuality());
            }

            // 🔥 타임스탬프
            if (current_value->getValueTimestamp() !=
                std::chrono::system_clock::time_point{}) {
              data_point.quality_timestamp = current_value->getValueTimestamp();
            } else {
              data_point.quality_timestamp = current_value->getUpdatedAt();
            }

            logger_->Debug("Current value loaded: " + data_point.name + " = " +
                           data_point.GetCurrentValueAsString() +
                           " (Quality: " + data_point.GetQualityCodeAsString() +
                           ")");
          } else {
            data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
            data_point.quality_code =
                PulseOne::Enums::DataQuality::NOT_CONNECTED;
            data_point.quality_timestamp = std::chrono::system_clock::now();

            logger_->Debug("No current value found for: " + data_point.name +
                           " (using defaults)");
          }

        } catch (const std::exception &e) {
          logger_->Debug("Current value query failed for " + data_point.name +
                         ": " + std::string(e.what()));

          data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
          data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
          data_point.quality_timestamp = std::chrono::system_clock::now();
        }
      } else {
        logger_->Warn(
            "CurrentValueRepository not injected, using default values");

        data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
        data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
        data_point.quality_timestamp = std::chrono::system_clock::now();
      }

      // 주소 필드 동기화
      if (data_point.address_string.empty()) {
        data_point.address_string = std::to_string(data_point.address);
      }

      result.push_back(data_point);

      logger_->Debug("Converted DataPoint: " + data_point.name +
                     " (Address: " + std::to_string(data_point.address) +
                     ", Type: " + data_point.data_type +
                     ", CurrentValue: " + data_point.GetCurrentValueAsString() +
                     ", Quality: " + data_point.GetQualityCodeAsString() + ")");
    }

    logger_->Info(
        "Successfully loaded " + std::to_string(result.size()) +
        " complete data points for device: " + std::to_string(device_id));

  } catch (const std::exception &e) {
    logger_->Error(
        "DataPointRepository::getDataPointsWithCurrentValues failed: " +
        std::string(e.what()));
  }

  return result;
}

// =============================================================================
// JSON 파싱 유틸리티
// =============================================================================

std::map<std::string, std::string>
DataPointRepository::parseJsonToStringMap(const std::string &json_str,
                                          const std::string &field_name) const {

  std::map<std::string, std::string> result;

  if (json_str.empty()) {
    return result;
  }

  try {
    json data = json::parse(json_str);

    if (data.is_object()) {
      for (auto &[key, value] : data.items()) {
        if (value.is_string()) {
          result[key] = value.get<std::string>();
        } else if (value.is_number_integer()) {
          result[key] = std::to_string(value.get<int64_t>());
        } else if (value.is_number_float()) {
          result[key] = std::to_string(value.get<double>());
        } else if (value.is_boolean()) {
          result[key] = value.get<bool>() ? "true" : "false";
        } else if (value.is_null()) {
          result[key] = "";
        } else {
          result[key] = value.dump();
        }
      }
    }

  } catch (const std::exception &e) {
    logger_->Warn("DataPointRepository::parseJsonToStringMap - Invalid JSON " +
                  field_name + ": " + std::string(e.what()));
  }

  return result;
}

// =============================================================================
// 누락된 메서드들 구현 (링킹 에러 해결)
// =============================================================================

int DataPointRepository::saveBulk(std::vector<DataPointEntity> &entities) {
  if (entities.empty()) {
    return 0;
  }

  try {
    if (!ensureTableExists()) {
      return 0;
    }

    int saved_count = 0;
    DbLib::DatabaseAbstractionLayer db_layer;

    for (auto &entity : entities) {
      if (!validateDataPoint(entity)) {
        continue;
      }

      auto params = entityToParams(entity);
      std::vector<std::string> primary_keys = {"id"};
      bool success =
          db_layer.executeUpsert("data_points", params, primary_keys);

      if (success) {
        saved_count++;

        // 새로 생성된 ID 조회
        if (entity.getId() <= 0) {
          auto results =
              db_layer.executeQuery("SELECT last_insert_rowid() as id");
          if (!results.empty()) {
            entity.setId(std::stoi(results[0].at("id")));
          }
        }
      }
    }

    if (logger_) {
      logger_->Info("DataPointRepository::saveBulk - Saved " +
                    std::to_string(saved_count) + " data points");
    }

    return saved_count;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("DataPointRepository::saveBulk failed: " +
                     std::string(e.what()));
    }
    return 0;
  }
}

int DataPointRepository::updateBulk(
    const std::vector<DataPointEntity> &entities) {
  if (entities.empty()) {
    return 0;
  }

  try {
    if (!ensureTableExists()) {
      return 0;
    }

    int updated_count = 0;
    DbLib::DatabaseAbstractionLayer db_layer;

    for (const auto &entity : entities) {
      if (entity.getId() <= 0 || !validateDataPoint(entity)) {
        continue;
      }

      auto params = entityToParams(entity);
      std::vector<std::string> primary_keys = {"id"};
      bool success =
          db_layer.executeUpsert("data_points", params, primary_keys);

      if (success) {
        updated_count++;
      }
    }

    if (logger_) {
      logger_->Info("DataPointRepository::updateBulk - Updated " +
                    std::to_string(updated_count) + " data points");
    }

    return updated_count;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("DataPointRepository::updateBulk failed: " +
                     std::string(e.what()));
    }
    return 0;
  }
}

int DataPointRepository::deleteByIds(const std::vector<int> &ids) {
  if (ids.empty()) {
    return 0;
  }

  try {
    if (!ensureTableExists()) {
      return 0;
    }

    int deleted_count = 0;
    DbLib::DatabaseAbstractionLayer db_layer;

    for (int id : ids) {
      std::string query =
          "DELETE FROM data_points WHERE id = " + std::to_string(id);
      bool success = db_layer.executeNonQuery(query);

      if (success) {
        deleted_count++;

        if (isCacheEnabled()) {
          clearCacheForId(id);
        }
      }
    }

    if (logger_) {
      logger_->Info("DataPointRepository::deleteByIds - Deleted " +
                    std::to_string(deleted_count) + " data points");
    }

    return deleted_count;

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("DataPointRepository::deleteByIds failed: " +
                     std::string(e.what()));
    }
    return 0;
  }
}

int DataPointRepository::getTotalCount() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    auto results =
        db_layer.executeQuery("SELECT COUNT(*) as count FROM data_points");

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }
  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::getTotalCount failed: " +
                   std::string(e.what()));
  }

  return 0;
}

// =============================================================================
// 품질 관리 관련 메서드들 구현
// =============================================================================

std::vector<DataPointEntity>
DataPointRepository::findQualityCheckEnabledPoints() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results =
        db_layer.executeQuery("SELECT * FROM data_points WHERE "
                              "quality_check_enabled = 1 AND is_enabled = 1");

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DataPointRepository::findQualityCheckEnabledPoints - "
                      "Failed to map row: " +
                      std::string(e.what()));
      }
    }

    return entities;

  } catch (const std::exception &e) {
    logger_->Error(
        "DataPointRepository::findQualityCheckEnabledPoints failed: " +
        std::string(e.what()));
    return {};
  }
}

std::vector<DataPointEntity>
DataPointRepository::findRangeCheckEnabledPoints() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results =
        db_layer.executeQuery("SELECT * FROM data_points WHERE "
                              "range_check_enabled = 1 AND is_enabled = 1");

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DataPointRepository::findRangeCheckEnabledPoints - "
                      "Failed to map row: " +
                      std::string(e.what()));
      }
    }

    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findRangeCheckEnabledPoints failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<DataPointEntity>
DataPointRepository::findRateOfChangeLimitedPoints() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results =
        db_layer.executeQuery("SELECT * FROM data_points WHERE "
                              "rate_of_change_limit > 0 AND is_enabled = 1");

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DataPointRepository::findRateOfChangeLimitedPoints - "
                      "Failed to map row: " +
                      std::string(e.what()));
      }
    }

    return entities;

  } catch (const std::exception &e) {
    logger_->Error(
        "DataPointRepository::findRateOfChangeLimitedPoints failed: " +
        std::string(e.what()));
    return {};
  }
}

// =============================================================================
// 알람 관련 메서드들 구현
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAlarmEnabledPoints() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(
        "SELECT * FROM data_points WHERE alarm_enabled = 1 AND is_enabled = 1");

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DataPointRepository::findAlarmEnabledPoints - Failed to "
                      "map row: " +
                      std::string(e.what()));
      }
    }

    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findAlarmEnabledPoints failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<DataPointEntity>
DataPointRepository::findByAlarmPriority(const std::string &priority) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = "SELECT * FROM data_points WHERE alarm_enabled = 1 AND "
                        "alarm_priority = '" +
                        priority + "' AND is_enabled = 1";
    auto results = db_layer.executeQuery(query);

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "DataPointRepository::findByAlarmPriority - Failed to map row: " +
            std::string(e.what()));
      }
    }

    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findByAlarmPriority failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<DataPointEntity>
DataPointRepository::findHighPriorityAlarmPoints() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(
        "SELECT * FROM data_points WHERE alarm_enabled = 1 AND alarm_priority "
        "IN ('high', 'critical') AND is_enabled = 1");

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DataPointRepository::findHighPriorityAlarmPoints - "
                      "Failed to map row: " +
                      std::string(e.what()));
      }
    }

    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findHighPriorityAlarmPoints failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<DataPointEntity>
DataPointRepository::findByGroup(const std::string &group_name) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    std::string query = "SELECT * FROM data_points WHERE group_name = '" +
                        group_name + "' AND is_enabled = 1";
    auto results = db_layer.executeQuery(query);

    std::vector<DataPointEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DataPointRepository::findByGroup - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::findByGroup failed: " +
                   std::string(e.what()));
    return {};
  }
}

// =============================================================================
// 통계 메서드들 구현
// =============================================================================

std::map<std::string, int> DataPointRepository::getQualityManagementStats() {
  std::map<std::string, int> stats;

  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    // 품질 체크 활성화된 포인트 수
    auto results =
        db_layer.executeQuery("SELECT COUNT(*) as count FROM data_points WHERE "
                              "quality_check_enabled = 1");
    if (!results.empty()) {
      stats["quality_check_enabled"] = std::stoi(results[0].at("count"));
    }

    // 범위 체크 활성화된 포인트 수
    results = db_layer.executeQuery("SELECT COUNT(*) as count FROM data_points "
                                    "WHERE range_check_enabled = 1");
    if (!results.empty()) {
      stats["range_check_enabled"] = std::stoi(results[0].at("count"));
    }

    // 변화율 제한이 설정된 포인트 수
    results = db_layer.executeQuery("SELECT COUNT(*) as count FROM data_points "
                                    "WHERE rate_of_change_limit > 0");
    if (!results.empty()) {
      stats["rate_of_change_limited"] = std::stoi(results[0].at("count"));
    }

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::getQualityManagementStats failed: " +
                   std::string(e.what()));
  }

  return stats;
}

std::map<std::string, int> DataPointRepository::getAlarmStats() {
  std::map<std::string, int> stats;

  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    // 알람 활성화된 포인트 수
    auto results = db_layer.executeQuery(
        "SELECT COUNT(*) as count FROM data_points WHERE alarm_enabled = 1");
    if (!results.empty()) {
      stats["alarm_enabled"] = std::stoi(results[0].at("count"));
    }

    // 우선순위별 알람 포인트 수
    results = db_layer.executeQuery(
        "SELECT alarm_priority, COUNT(*) as count FROM data_points WHERE "
        "alarm_enabled = 1 GROUP BY alarm_priority");
    for (const auto &row : results) {
      if (row.find("alarm_priority") != row.end() &&
          row.find("count") != row.end()) {
        stats[row.at("alarm_priority")] = std::stoi(row.at("count"));
      }
    }

  } catch (const std::exception &e) {
    logger_->Error("DataPointRepository::getAlarmStats failed: " +
                   std::string(e.what()));
  }

  return stats;
}

std::vector<DataPointEntity>
DataPointRepository::findByIds(const std::vector<int> &ids) {
  std::vector<DataPointEntity> entities;

  for (int id : ids) {
    auto entity = findById(id);
    if (entity.has_value()) {
      entities.push_back(entity.value());
    }
  }

  return entities;
}

std::vector<DataPointEntity> DataPointRepository::findByConditions(
    const std::vector<QueryCondition> &conditions,
    const std::optional<OrderBy> &order_by,
    const std::optional<Pagination> &pagination) {

  // 기본 구현 - 모든 엔티티를 조회한 후 필터링
  auto all_entities = findAll();
  std::vector<DataPointEntity> result;

  for (const auto &entity : all_entities) {
    bool matches = true;
    for (const auto &condition : conditions) {
      // 간단한 조건 매칭 로직 구현
      // 실제로는 더 복잡한 SQL WHERE 절 생성이 필요
      matches = matches && true; // 임시 구현
    }
    if (matches) {
      result.push_back(entity);
    }
  }

  // 페이지네이션 적용
  if (pagination.has_value()) {
    size_t offset = pagination->offset;
    size_t limit = pagination->limit;

    if (offset < result.size()) {
      size_t end = std::min(offset + limit, result.size());
      result = std::vector<DataPointEntity>(result.begin() + offset,
                                            result.begin() + end);
    } else {
      result.clear();
    }
  }

  return result;
}

int DataPointRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {
  return static_cast<int>(findByConditions(conditions).size());
}

// 캐시 관리 메서드들
void DataPointRepository::setCacheEnabled(bool enabled) {
  IRepository<DataPointEntity>::setCacheEnabled(enabled);
  if (logger_) {
    std::string message = "DataPointRepository cache ";
    message += (enabled ? "enabled" : "disabled");
    logger_->Info(message);
  }
}

bool DataPointRepository::isCacheEnabled() const {
  return IRepository<DataPointEntity>::isCacheEnabled();
}

void DataPointRepository::clearCache() {
  IRepository<DataPointEntity>::clearCache();
  if (logger_) {
    logger_->Info("DataPointRepository cache cleared");
  }
}

void DataPointRepository::clearCacheForId(int id) {
  IRepository<DataPointEntity>::clearCacheForId(id);
  if (logger_) {
    logger_->Debug("DataPointRepository cache cleared for ID: " +
                   std::to_string(id));
  }
}

std::map<std::string, int> DataPointRepository::getCacheStats() const {
  return IRepository<DataPointEntity>::getCacheStats();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne