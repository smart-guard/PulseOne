// =============================================================================
// core/shared/include/Database/RuntimeSQLQueries.h
// 🎯 런타임 동적 SQL 상수 - 코드 내 인라인 작성 쿼리 중앙 관리
//
// 대상: 파라미터를 C++ 문자열 연결로 바인딩하는 동적 쿼리들
//       (? 바인딩이 불가능한 케이스 또는 런타임 조건부 쿼리)
//
// 포함 네임스페이스:
//   - EdgeServer      : Application.cpp (collector identity / heartbeat)
//   - VirtualPointBatch : VirtualPointBatchWriter.cpp
//   - DeviceStatus    : DataProcessingService.cpp
//   - MaintenanceLog  : LogLevelManager.cpp
// =============================================================================

#ifndef RUNTIME_SQL_QUERIES_H
#define RUNTIME_SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {
namespace Runtime {

// =============================================================================
// 🔵 EdgeServer — collector identity 및 heartbeat (Application.cpp)
// =============================================================================
namespace EdgeServer {

// Collector 시작 시 자신의 slot을 instance_key로 클레임
// 파라미터: {instance_key}, {id}
inline std::string CLAIM_BY_ID(const std::string &instance_key, int id) {
  return "UPDATE edge_servers SET instance_key = '" + instance_key +
         "', last_seen = (datetime('now', 'localtime')) WHERE id = " +
         std::to_string(id) + " AND server_type = 'collector'";
}

// 이미 이 instance_key로 등록된 collector 조회
// 파라미터: {instance_key}
inline std::string FIND_BY_INSTANCE_KEY(const std::string &instance_key) {
  return "SELECT id FROM edge_servers WHERE instance_key = '" + instance_key +
         "' AND server_type = 'collector' LIMIT 1";
}

// 사용 가능한 빈 slot 또는 5분 이상 heartbeat 없는 slot을 클레임
// 파라미터: {instance_key}
// ✅ datetime(..., '-5 minutes') — SQLite/PG/MySQL/MSSQL 모두 호환
// ❌ INTERVAL '5 minutes' 는 PostgreSQL 전용 (이전 버그 코드)
inline std::string CLAIM_STALE_SLOT(const std::string &instance_key) {
  return "UPDATE edge_servers SET instance_key = '" + instance_key +
         "', last_seen = (datetime('now', 'localtime')) "
         "WHERE id = (SELECT id FROM edge_servers WHERE server_type = "
         "'collector' "
         "AND ((instance_key IS NULL OR instance_key = '') "
         "OR (last_heartbeat < datetime('now', 'localtime', '-5 minutes'))) "
         "LIMIT 1)";
}

// 주기적 heartbeat 갱신 (30초마다)
// 파라미터: {collector_id}
inline std::string UPDATE_HEARTBEAT(int collector_id) {
  return "UPDATE edge_servers SET "
         "last_seen = (datetime('now', 'localtime')), "
         "last_heartbeat = (datetime('now', 'localtime')) "
         "WHERE id = " +
         std::to_string(collector_id);
}

} // namespace EdgeServer

// =============================================================================
// 🟢 VirtualPointBatch — VP 배치 실행 결과 기록 (VirtualPointBatchWriter.cpp)
// =============================================================================
namespace VirtualPointBatch {

// VP 실행 이력 삽입
// 파라미터: {vp_id}, {value}
inline std::string INSERT_HISTORY(int vp_id, double value) {
  return "INSERT INTO virtual_point_execution_history "
         "(virtual_point_id, execution_time, result_value, success) VALUES (" +
         std::to_string(vp_id) +
         ", "
         "datetime('now', 'localtime'), '" +
         std::to_string(value) + "', 1)";
}

// VP 현재값 UPSERT (INSERT OR REPLACE)
// 파라미터: {vp_id}, {value}, {quality}
inline std::string UPSERT_CURRENT_VALUE(int vp_id, double value,
                                        const std::string &quality) {
  return "INSERT OR REPLACE INTO virtual_point_values "
         "(virtual_point_id, value, quality, last_calculated) VALUES (" +
         std::to_string(vp_id) + ", " + std::to_string(value) + ", '" +
         quality +
         "', "
         "datetime('now', 'localtime'))";
}

// current_values 테이블에도 저장 (통합 현재값 관리)
// ✅ 실제 스키마 컬럼: point_id(PK), current_value, value_timestamp, quality,
// value_type 파라미터: {vp_id}, {value}, {quality}
inline std::string UPSERT_CURRENT_VALUES_TABLE(int vp_id, double value,
                                               const std::string &quality) {
  return "INSERT OR REPLACE INTO current_values "
         "(point_id, current_value, value_timestamp, quality, value_type) "
         "VALUES (" +
         std::to_string(vp_id) + ", '" + std::to_string(value) +
         "', "
         "datetime('now', 'localtime'), '" +
         quality + "', 'virtual_point')";
}

} // namespace VirtualPointBatch

// =============================================================================
// 🟡 DeviceStatus — 장치 연결 상태 UPSERT (DataProcessingService.cpp)
// =============================================================================
namespace DeviceStatus {

// 장치 통신 상태 INSERT ON CONFLICT UPDATE
// 파라미터: {device_id}, {status_str}, {response_time_ms}, {is_online}
inline std::string UPSERT_STATUS(const std::string &device_id,
                                 const std::string &status_str,
                                 long long response_time_ms, bool is_online) {
  return "INSERT INTO device_status "
         "(device_id, connection_status, last_communication, updated_at, "
         "response_time, total_requests, successful_requests) "
         "VALUES (" +
         device_id + ", '" + status_str +
         "', datetime('now', 'localtime'), datetime('now', 'localtime'), " +
         std::to_string(response_time_ms) + ", 1, " + (is_online ? "1" : "0") +
         ") "
         "ON CONFLICT(device_id) DO UPDATE SET "
         "connection_status = excluded.connection_status, "
         "last_communication = excluded.last_communication, "
         "updated_at = excluded.updated_at, "
         "response_time = excluded.response_time, "
         "total_requests = device_status.total_requests + 1" +
         (is_online
              ? ", successful_requests = device_status.successful_requests + 1"
              : "") +
         ";";
}

} // namespace DeviceStatus

// =============================================================================
// 🔴 MaintenanceLog — 로그 레벨 변경 이력 (LogLevelManager.cpp)
// =============================================================================
namespace MaintenanceLog {

// 엔지니어 모드 시작 기록
// 파라미터: {engineer_id}, {log_level}
inline std::string INSERT_ENGINEER_START(const std::string &engineer_id,
                                         const std::string &log_level) {
  return "INSERT INTO maintenance_log "
         "(engineer_id, action, log_level, timestamp, source) "
         "VALUES ('" +
         engineer_id + "', 'START', '" + log_level +
         "', datetime('now', 'localtime'), 'WEB_API')";
}

// 엔지니어 모드 종료 기록
// 파라미터: {engineer_id}
inline std::string INSERT_ENGINEER_END(const std::string &engineer_id) {
  return "INSERT INTO maintenance_log "
         "(engineer_id, action, timestamp, source) "
         "VALUES ('" +
         engineer_id +
         "', 'END', "
         "datetime('now', 'localtime'), 'WEB_API')";
}

// 기본 로그 레벨 설정값 조회
// ✅ 실제 스키마 컬럼: key_name, value (system_settings)
const std::string GET_DEFAULT_LOG_LEVEL =
    "SELECT value FROM system_settings "
    "WHERE key_name = 'default_log_level'";

// 기본 로그 레벨 저장 (system_settings)
// ✅ 실제 스키마 컬럼: key_name, value, updated_by, updated_at
// 파라미터: {level_str}, {changed_by}
inline std::string UPSERT_DEFAULT_LOG_LEVEL(const std::string &level_str,
                                            const std::string &changed_by,
                                            const std::string & /*reason*/) {
  return "INSERT OR REPLACE INTO system_settings "
         "(key_name, value, updated_by, updated_at) "
         "VALUES ('default_log_level', '" +
         level_str + "', '" + changed_by + "', datetime('now', 'localtime'))";
}

// 로그 레벨 변경 이력 기록
// 파라미터: {old_level}, {new_level}, {source}, {changed_by}, {reason}
inline std::string INSERT_LEVEL_HISTORY(const std::string &old_level,
                                        const std::string &new_level,
                                        const std::string &source,
                                        const std::string &changed_by,
                                        const std::string &reason) {
  return "INSERT INTO log_level_history "
         "(old_level, new_level, source, changed_by, reason, change_time) "
         "VALUES ('" +
         old_level + "', '" + new_level + "', '" + source + "', '" +
         changed_by + "', '" + reason + "', datetime('now', 'localtime'))";
}

// 카테고리별 로그 레벨 전체 조회
const std::string GET_CATEGORY_LEVELS =
    "SELECT category, log_level FROM driver_log_levels";

// 카테고리 로그 레벨 저장 (driver_log_levels)
// 파라미터: {category}, {level}, {changed_by}
inline std::string UPSERT_CATEGORY_LEVEL(const std::string &category,
                                         const std::string &level,
                                         const std::string &changed_by) {
  return "INSERT OR REPLACE INTO driver_log_levels "
         "(category, log_level, updated_by, updated_at) "
         "VALUES ('" +
         category + "', '" + level + "', '" + changed_by +
         "', datetime('now', 'localtime'))";
}

} // namespace MaintenanceLog

} // namespace Runtime
} // namespace SQL
} // namespace Database
} // namespace PulseOne

#endif // RUNTIME_SQL_QUERIES_H
