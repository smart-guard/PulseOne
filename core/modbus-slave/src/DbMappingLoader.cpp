// =============================================================================
// core/modbus-slave/src/DbMappingLoader.cpp
// SQLite에서 레지스터 매핑 로드
// =============================================================================
#include "DbMappingLoader.h"
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace PulseOne {
namespace ModbusSlave {

DbMappingLoader::DbMappingLoader(const std::string &db_path)
    : db_path_(db_path) {
#ifdef HAS_SQLITE
  connected_ = OpenDb();
  if (!connected_)
    std::cerr << "[DbMappingLoader] DB 연결 실패: " << db_path << "\n";
  else
    std::cout << "[DbMappingLoader] DB 연결 성공: " << db_path << "\n";
#else
  std::cout << "[DbMappingLoader] SQLite 없음 — JSON 파일 폴백 사용\n";
#endif
}

DbMappingLoader::~DbMappingLoader() {
#ifdef HAS_SQLITE
  CloseDb();
#endif
}

std::vector<RegisterMapping> DbMappingLoader::Load(int device_id) {
#ifndef HAS_SQLITE
  return {};
#else
  if (!connected_) {
    connected_ = OpenDb();
    if (!connected_)
      return {};
  }

  auto mappings = QueryMappings(device_id);
  last_mapping_hash_ = ComputeHash(mappings);

  // 마지막 로드 시각 기록
  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
  last_load_time_ = ss.str();

  std::cout << "[DbMappingLoader] 매핑 " << mappings.size() << "개 로드 완료"
            << (device_id ? " (device=" + std::to_string(device_id) + ")"
                          : " (전체)")
            << "\n";
  return mappings;
#endif
}

bool DbMappingLoader::Reload(std::vector<RegisterMapping> &mappings,
                             int device_id) {
#ifndef HAS_SQLITE
  return false;
#else
  if (!connected_) {
    connected_ = OpenDb();
    if (!connected_)
      return false;
  }

  auto new_mappings = QueryMappings(device_id);
  size_t new_hash = ComputeHash(new_mappings);

  if (new_hash != last_mapping_hash_) {
    mappings = std::move(new_mappings);
    last_mapping_hash_ = new_hash;
    std::cout << "[DbMappingLoader] 매핑 변경 감지 → 재로드 완료 ("
              << mappings.size() << "개)\n";
    return true; // 변경됨
  }
  return false; // 변경 없음
#endif
}

#ifdef HAS_SQLITE

bool DbMappingLoader::OpenDb() {
  if (db_)
    return true;
  int rc =
      sqlite3_open_v2(db_path_.c_str(), &db_,
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "[DbMappingLoader] sqlite3_open 오류: " << sqlite3_errmsg(db_)
              << "\n";
    sqlite3_close(db_);
    db_ = nullptr;
    return false;
  }
  return true;
}

void DbMappingLoader::CloseDb() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
  connected_ = false;
}

std::vector<RegisterMapping> DbMappingLoader::QueryMappings(int device_id) {
  std::vector<RegisterMapping> result;
  if (!db_)
    return result;

  // device_id 필터링 SQL (조건부 WHERE)
  std::string sql = "SELECT m.id, m.point_id, p.name, m.register_type, "
                    "       m.register_address, m.data_type, m.byte_order, "
                    "       m.scale_factor, m.scale_offset, m.enabled "
                    "FROM modbus_slave_mappings m "
                    "JOIN data_points p ON p.id = m.point_id "
                    "WHERE m.enabled = 1";
  if (device_id > 0)
    sql += " AND m.device_id = " + std::to_string(device_id);
  sql += " ORDER BY m.register_address ASC;";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "[DbMappingLoader] SQL 준비 오류: " << sqlite3_errmsg(db_)
              << "\n";
    return result;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    RegisterMapping m;
    m.point_id = sqlite3_column_int(stmt, 1);
    const char *name =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    m.point_name = name ? name : "";

    const char *rt =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    m.register_type =
        rt ? ParseRegisterType(rt) : RegisterType::HOLDING_REGISTER;

    m.address = static_cast<uint16_t>(sqlite3_column_int(stmt, 4));

    const char *dt =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    m.data_type = dt ? ParseDataType(dt) : DataType::FLOAT32;

    // byte_order 파싱: big_endian / little_endian / big_endian_swap /
    // little_endian_swap
    const char *bo =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    if (bo) {
      std::string bo_str(bo);
      m.big_endian = (bo_str == "big_endian" || bo_str == "big_endian_swap");
      m.word_swap =
          (bo_str == "big_endian_swap" || bo_str == "little_endian_swap");
    } else {
      m.big_endian = true;
      m.word_swap = false;
    }

    m.scale_factor = sqlite3_column_double(stmt, 7);
    m.scale_offset = sqlite3_column_double(stmt, 8);
    m.enabled = sqlite3_column_int(stmt, 9) != 0;

    result.push_back(m);
  }

  sqlite3_finalize(stmt);
  return result;
}

size_t
DbMappingLoader::ComputeHash(const std::vector<RegisterMapping> &m) const {
  size_t h = 0;
  for (const auto &r : m) {
    h ^= std::hash<int>{}(r.point_id) ^ std::hash<int>{}(r.address) ^
         std::hash<int>{}(static_cast<int>(r.register_type)) ^
         std::hash<double>{}(r.scale_factor);
  }
  return h;
}

#endif // HAS_SQLITE

} // namespace ModbusSlave
} // namespace PulseOne

// =============================================================================
// 편의 함수 구현: device_id 기준 DB → SlaveConfig + 매핑 전체 로드
// =============================================================================
namespace PulseOne {
namespace ModbusSlave {

bool LoadDeviceFromDb(int device_id, const std::string &db_path,
                      SlaveConfig &config,
                      std::vector<RegisterMapping> &mappings) {
#ifndef HAS_SQLITE
  (void)device_id;
  (void)db_path;
  (void)config;
  (void)mappings;
  return false;
#else
  sqlite3 *db = nullptr;
  int rc =
      sqlite3_open_v2(db_path.c_str(), &db,
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "[LoadDeviceFromDb] DB 열기 실패 (rc=" << rc
              << "): " << (db ? sqlite3_errmsg(db) : "unknown")
              << " path=" << db_path << "\n";
    if (db)
      sqlite3_close(db);
    return false;
  }

  // WAL 모드에서 backend write lock 충돌 시 최대 5초 대기
  sqlite3_busy_timeout(db, 5000);

  // 1. 디바이스 기본 설정 로드
  const char *sql_dev =
      "SELECT tcp_port, unit_id, max_clients, COALESCE(packet_logging,0) "
      "FROM modbus_slave_devices WHERE id = ? AND enabled = 1 LIMIT 1;";
  sqlite3_stmt *stmt = nullptr;
  bool found = false;

  rc = sqlite3_prepare_v2(db, sql_dev, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "[LoadDeviceFromDb] SQL prepare 오류 (rc=" << rc
              << "): " << sqlite3_errmsg(db) << "\n";
    sqlite3_close(db);
    return false;
  }
  sqlite3_bind_int(stmt, 1, device_id);
  int step_rc = sqlite3_step(stmt);
  if (step_rc == SQLITE_ROW) {
    config.tcp_port = sqlite3_column_int(stmt, 0);
    config.unit_id = static_cast<uint8_t>(sqlite3_column_int(stmt, 1));
    config.max_clients = sqlite3_column_int(stmt, 2);
    config.packet_logging = (sqlite3_column_int(stmt, 3) != 0);
    found = true;
  } else if (step_rc != SQLITE_DONE) {
    std::cerr << "[LoadDeviceFromDb] SQL step 오류 (rc=" << step_rc
              << "): " << sqlite3_errmsg(db) << "\n";
  }
  sqlite3_finalize(stmt);

  if (!found) {
    std::cerr << "[LoadDeviceFromDb] device_id=" << device_id
              << " not found (enabled=1 조건 확인)\n";
    sqlite3_close(db);
    return false;
  }

  // 2. Redis 설정은 환경변수에서 (DB에 없으므로)
  if (const char *v = std::getenv("REDIS_PRIMARY_HOST"))
    config.redis_host = v;
  else if (const char *v2 = std::getenv("REDIS_HOST"))
    config.redis_host = v2;
  if (const char *v = std::getenv("REDIS_PRIMARY_PORT"))
    config.redis_port = std::atoi(v);
  else if (const char *v2 = std::getenv("REDIS_PORT"))
    config.redis_port = std::atoi(v2);

  // 3. 매핑 로드 (device_id 기준 필터)
  const char *sql_map =
      "SELECT m.point_id, p.name, m.register_type, m.register_address, "
      "       m.data_type, m.byte_order, m.scale_factor, m.scale_offset "
      "FROM modbus_slave_mappings m "
      "JOIN data_points p ON p.id = m.point_id "
      "WHERE m.device_id = ? AND m.enabled = 1 "
      "ORDER BY m.register_address ASC;";

  rc = sqlite3_prepare_v2(db, sql_map, -1, &stmt, nullptr);
  if (rc == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, device_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      RegisterMapping m;
      m.point_id = sqlite3_column_int(stmt, 0);
      const char *name =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      m.point_name = name ? name : "";
      const char *rt =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      m.register_type =
          rt ? ParseRegisterType(rt) : RegisterType::HOLDING_REGISTER;
      m.address = static_cast<uint16_t>(sqlite3_column_int(stmt, 3));
      const char *dt =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
      m.data_type = dt ? ParseDataType(dt) : DataType::FLOAT32;
      const char *bo =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
      if (bo) {
        std::string bo_str(bo);
        m.big_endian = (bo_str == "big_endian" || bo_str == "big_endian_swap");
        m.word_swap =
            (bo_str == "big_endian_swap" || bo_str == "little_endian_swap");
      } else {
        m.big_endian = true;
        m.word_swap = false;
      }
      m.scale_factor = sqlite3_column_double(stmt, 6);
      m.scale_offset = sqlite3_column_double(stmt, 7);
      m.enabled = true;
      mappings.push_back(m);
    }
    sqlite3_finalize(stmt);
  }

  sqlite3_close(db);
  std::cout << "[LoadDeviceFromDb] device_id=" << device_id
            << " port=" << config.tcp_port << " mappings=" << mappings.size()
            << "개\n";
  return true;
#endif
}

} // namespace ModbusSlave
} // namespace PulseOne
