/**
 * @file ExportTargetEntity.cpp
 * @brief Export Target 엔티티 구현부
 * @version 3.1.0 - template_id 필드 추가
 * @date 2025-10-23
 *
 * 저장 위치: core/shared/src/Database/Entities/ExportTargetEntity.cpp
 *
 * 주요 변경사항 (v3.0.0 → v3.1.0):
 *   - template_id 필드 초기화 추가
 *   - toJson()에 template_id 직렬화 추가
 *   - fromJson()에 template_id 역직렬화 추가
 *
 * 이전 변경사항 (v3.0.0):
 *   - 통계 필드 관련 모든 코드 제거
 *   - 설정 정보만 처리
 *   - validate() 간소화
 *   - JSON 직렬화 단순화
 */

#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ExportTargetEntity::ExportTargetEntity()
    : BaseEntity(), is_enabled_(true), export_mode_("on_change"),
      export_interval_(0), batch_size_(100), execution_delay_ms_(0) {
  // 통계 필드 초기화 코드 모두 제거됨
}

ExportTargetEntity::ExportTargetEntity(int id)
    : BaseEntity(id), is_enabled_(true), export_mode_("on_change"),
      export_interval_(0), batch_size_(100), execution_delay_ms_(0) {
  // 통계 필드 초기화 코드 모두 제거됨
}

ExportTargetEntity::~ExportTargetEntity() {}

// =============================================================================
// Repository 패턴 지원
// =============================================================================

std::shared_ptr<Repositories::ExportTargetRepository>
ExportTargetEntity::getRepository() const {
  return RepositoryFactory::getInstance().getExportTargetRepository();
}

// =============================================================================
// 비즈니스 로직
// =============================================================================

bool ExportTargetEntity::validate() const {
  // 기본 유효성 검사
  if (!BaseEntity<ExportTargetEntity>::isValid()) {
    return false;
  }

  // 필수 필드 검사
  if (name_.empty()) {
    return false;
  }

  if (target_type_.empty()) {
    return false;
  }

  if (config_.empty()) {
    return false;
  }

  // export_mode 검증 ('on_change', 'periodic', 'batch')
  if (export_mode_ != "on_change" && export_mode_ != "periodic" &&
      export_mode_ != "batch") {
    return false;
  }

  // periodic 모드일 때 interval 체크
  if (export_mode_ == "periodic" && export_interval_ <= 0) {
    return false;
  }

  // batch_size 범위 체크 (1~10000)
  if (batch_size_ <= 0 || batch_size_ > 10000) {
    return false;
  }

  return true;
}

json ExportTargetEntity::parseConfig() const {
  try {
    return json::parse(config_);
  } catch (const std::exception &) {
    // JSON 파싱 실패 시 빈 객체 반환
    return json::object();
  }
}

void ExportTargetEntity::setConfigJson(const json &config) {
  try {
    config_ = config.dump();
    markModified();
  } catch (const std::exception &) {
    // JSON 직렬화 실패 시 빈 객체
    config_ = "{}";
    markModified();
  }
}

std::string ExportTargetEntity::getEntityTypeName() const {
  return "ExportTargetEntity";
}

// =============================================================================
// DB 연산 - Repository 위임
// =============================================================================

bool ExportTargetEntity::loadFromDatabase() {
  if (getId() <= 0) {
    return false;
  }

  try {
    auto repo = getRepository();
    if (!repo) {
      return false;
    }

    auto loaded = repo->findById(getId());
    if (loaded.has_value()) {
      // Repository에서 로드한 데이터를 현재 객체에 복사
      *this = loaded.value();
      markSaved();
      return true;
    }

    return false;

  } catch (const std::exception &e) {
    markError();
    return false;
  }
}

bool ExportTargetEntity::saveToDatabase() {
  try {
    auto repo = getRepository();
    if (!repo) {
      return false;
    }

    bool success = false;

    if (getId() <= 0) {
      // 신규 저장 (INSERT)
      success = repo->save(*this);
    } else {
      // 업데이트 (UPDATE)
      success = repo->update(*this);
    }

    if (success) {
      markSaved();
    }

    return success;

  } catch (const std::exception &e) {
    markError();
    return false;
  }
}

bool ExportTargetEntity::updateToDatabase() {
  // saveToDatabase()와 동일 (UPDATE 로직)
  return saveToDatabase();
}

bool ExportTargetEntity::deleteFromDatabase() {
  if (getId() <= 0) {
    return false;
  }

  try {
    auto repo = getRepository();
    if (!repo) {
      return false;
    }

    bool success = repo->deleteById(getId());

    if (success) {
      markDeleted();
    }

    return success;

  } catch (const std::exception &e) {
    markError();
    return false;
  }
}

// =============================================================================
// JSON 직렬화
// =============================================================================

json ExportTargetEntity::toJson() const {
  json j;

  try {
    // 기본 정보
    j["id"] = getId();
    j["name"] = name_;
    j["target_type"] = target_type_;
    j["description"] = description_;
    j["is_enabled"] = is_enabled_;

    // 설정 정보
    j["config"] = parseConfig(); // JSON 문자열을 객체로 파싱

    j["export_mode"] = export_mode_;
    j["export_interval"] = export_interval_;
    j["batch_size"] = batch_size_;
    j["execution_delay_ms"] = execution_delay_ms_;

    // 타임스탬프 (epoch seconds)
    j["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                          getCreatedAt().time_since_epoch())
                          .count();
    j["updated_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                          getUpdatedAt().time_since_epoch())
                          .count();

    // ❌ 통계 필드 제거됨 - export_logs에서 조회해야 함
    // j["total_exports"] = ...           // 제거
    // j["successful_exports"] = ...      // 제거
    // j["failed_exports"] = ...          // 제거
    // j["last_export_at"] = ...          // 제거
    // j["last_success_at"] = ...         // 제거
    // j["last_error"] = ...              // 제거
    // j["avg_export_time_ms"] = ...      // 제거

  } catch (const std::exception &) {
    // JSON 생성 실패 시 기본 객체 반환
  }

  return j;
}

bool ExportTargetEntity::fromJson(const json &data) {
  try {
    if (data.contains("id")) {
      setId(data["id"].get<int>());
    }

    if (data.contains("name")) {
      name_ = data["name"].get<std::string>();
    }

    if (data.contains("target_type")) {
      target_type_ = data["target_type"].get<std::string>();
    }

    if (data.contains("description")) {
      description_ = data["description"].get<std::string>();
    }

    if (data.contains("is_enabled")) {
      is_enabled_ = data["is_enabled"].get<bool>();
    }

    if (data.contains("config")) {
      if (data["config"].is_string()) {
        // 이미 JSON 문자열인 경우
        config_ = data["config"].get<std::string>();
      } else if (data["config"].is_object()) {
        // JSON 객체인 경우 문자열로 변환
        config_ = data["config"].dump();
      }
    }

    if (data.contains("export_mode")) {
      export_mode_ = data["export_mode"].get<std::string>();
    }

    if (data.contains("export_interval")) {
      export_interval_ = data["export_interval"].get<int>();
    }

    if (data.contains("batch_size")) {
      batch_size_ = data["batch_size"].get<int>();
    }

    if (data.contains("execution_delay_ms")) {
      execution_delay_ms_ = data["execution_delay_ms"].get<int>();
    }

    // ❌ 통계 필드 파싱 코드 제거됨

    markModified();
    return true;

  } catch (const std::exception &) {
    return false;
  }
}

std::string ExportTargetEntity::toString() const {
  std::ostringstream oss;
  oss << "ExportTargetEntity[";
  oss << "id=" << getId();
  oss << ", name=" << name_;
  oss << ", type=" << target_type_;
  oss << ", mode=" << export_mode_;
  oss << ", batch=" << batch_size_;
  oss << ", delay=" << execution_delay_ms_;
  oss << ", enabled=" << (is_enabled_ ? "true" : "false");
  oss << "]";
  return oss.str();
}

// ❌ 제거된 메서드들 (통계 관련)
// double ExportTargetEntity::getSuccessRate() const { ... }  // 제거
// bool ExportTargetEntity::isHealthy() const { ... }          // 제거

} // namespace Entities
} // namespace Database
} // namespace PulseOne