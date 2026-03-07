// =============================================================================
// core/modbus-slave/include/ModbusSupervisor.h
// export-gateway ProcessSupervisor와 동일한 패턴
// modbus_slave_devices 테이블 기반 멀티 인스턴스 프로세스 관리
// =============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

namespace PulseOne {
namespace ModbusSlave {

struct ChildProcess {
  int device_id = 0;
  std::string name;
  int restart_count = 0;
  std::chrono::steady_clock::time_point last_start;

#ifdef _WIN32
  HANDLE process_handle = INVALID_HANDLE_VALUE;
  DWORD process_id = 0;
#else
  pid_t pid = -1;
#endif
};

// =============================================================================
// ModbusSupervisor
// - 인자 없이 기동 → Supervisor 모드
//   DB에서 modbus_slave_devices 조회 → 디바이스별 자식 프로세스 spawn
// - --device-id=N 으로 기동 → Worker 모드 (실제 TCP 서버)
// =============================================================================
class ModbusSupervisor {
public:
  ModbusSupervisor(const std::string &exe_path, const std::string &db_path);
  ~ModbusSupervisor();

  void Run();
  void RequestShutdown() { shutdown_requested_ = true; }

  // DB 연결 없이 기동 가능 여부 (개발/테스트용)
  bool IsDbConnected() const { return db_connected_; }

private:
  // DB에서 활성 device ID 목록 조회
  std::vector<int> QueryActiveDevices();

  // 자식 프로세스 spawn / kill / 상태 확인
  bool SpawnChild(int device_id);
  bool IsChildAlive(const ChildProcess &child) const;
  void KillChild(ChildProcess &child);
  void KillAll();

  // DB 변경 감지 → 신규 spawn / 삭제된 것 kill
  void Reconcile(const std::vector<int> &active_ids);

  // 죽은 자식 프로세스 재시작
  void MonitorChildren();

  std::string exe_path_;
  std::string db_path_;
  bool db_connected_ = false;

  std::atomic<bool> shutdown_requested_{false};

  mutable std::mutex children_mutex_;
  std::map<int, ChildProcess> children_; // device_id → child

  static constexpr int MONITOR_INTERVAL_SEC = 5;
  static constexpr int RECONCILE_INTERVAL_SEC = 5; // 60→5: Stop/Start 즉시 반영
  static constexpr int MAX_RESTART_COUNT = 5;
};

} // namespace ModbusSlave
} // namespace PulseOne
