// =============================================================================
// core/modbus-slave/include/DbMappingLoader.h
// SQLite DB에서 레지스터 매핑 설정 로드 (shared 라이브러리 기반)
// =============================================================================
#pragma once

#include "MappingConfig.h"
#include <string>
#include <vector>

// SQLite3 조건부 포함
#ifdef HAS_SQLITE
#include <sqlite3.h>
#endif

namespace PulseOne {
namespace ModbusSlave {

class DbMappingLoader {
public:
  explicit DbMappingLoader(const std::string &db_path);
  ~DbMappingLoader();

  // DB에서 전체 매핑 로드
  // modbus_slave_mappings 테이블에서 enabled=1인 항목 반환
  std::vector<RegisterMapping> Load();

  // 런타임 재로드 (변경 감지 시 호출)
  // 반환: 매핑이 변경됐으면 true
  bool Reload(std::vector<RegisterMapping> &mappings);

  bool IsConnected() const { return connected_; }

  // 마지막 로드 시각
  std::string LastLoadTime() const { return last_load_time_; }

private:
  std::string db_path_;
  bool connected_ = false;
  std::string last_load_time_;

  // 마지막 로드 시 매핑 해시 (변경 감지용)
  size_t last_mapping_hash_ = 0;

#ifdef HAS_SQLITE
  sqlite3 *db_ = nullptr;
  bool OpenDb();
  void CloseDb();
  std::vector<RegisterMapping> QueryMappings();
  size_t ComputeHash(const std::vector<RegisterMapping> &m) const;
#endif
};

} // namespace ModbusSlave
} // namespace PulseOne

// =============================================================================
// 편의 함수: device_id 기준으로 DB에서 SlaveConfig + 매핑 전체 로드
// HAS_SQLITE 정의 시만 구현됨, 아니면 false 반환
// =============================================================================
namespace PulseOne {
namespace ModbusSlave {

bool LoadDeviceFromDb(int device_id, const std::string &db_path,
                      SlaveConfig &config,
                      std::vector<RegisterMapping> &mappings);

} // namespace ModbusSlave
} // namespace PulseOne
