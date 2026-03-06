// =============================================================================
// core/modbus-slave/src/ModbusSupervisor.cpp
// export-gateway ProcessSupervisor와 동일한 로직
// =============================================================================
#include "ModbusSupervisor.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#ifdef HAS_SQLITE
#include <sqlite3.h>
#endif

#ifdef _WIN32
#pragma comment(lib, "kernel32.lib")
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace PulseOne {
namespace ModbusSlave {

ModbusSupervisor::ModbusSupervisor(const std::string &exe_path,
                                   const std::string &db_path)
    : exe_path_(exe_path), db_path_(db_path) {
#ifdef HAS_SQLITE
  // DB 접근 가능한지 확인
  sqlite3 *db = nullptr;
  int rc = sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
  db_connected_ = (rc == SQLITE_OK);
  if (db)
    sqlite3_close(db);
#endif
  std::cout << "[ModbusSupervisor] 초기화: exe=" << exe_path_
            << " db=" << db_path_ << " db_ok=" << db_connected_ << "\n";
}

ModbusSupervisor::~ModbusSupervisor() { KillAll(); }

void ModbusSupervisor::Run() {
  std::cout << "[ModbusSupervisor] Supervisor 모드 시작\n";

  // 1. 최초 DB 조회 + spawn
  auto active_ids = QueryActiveDevices();
  if (active_ids.empty())
    std::cout << "[ModbusSupervisor] 활성 디바이스 없음 — DB 변경 대기\n";
  Reconcile(active_ids);

  auto last_reconcile = std::chrono::steady_clock::now();

  // 2. 모니터링 루프
  while (!shutdown_requested_.load()) {
    // 100ms 슬립 (종료 신호 빠른 감지)
    for (int i = 0;
         i < MONITOR_INTERVAL_SEC * 10 && !shutdown_requested_.load(); i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (shutdown_requested_.load())
      break;

    // 자식 프로세스 생존 확인 + 재시작
    MonitorChildren();

    // 60초마다 DB 재조회 (새 디바이스 추가/삭제 감지)
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_reconcile)
            .count() >= RECONCILE_INTERVAL_SEC) {
      auto ids = QueryActiveDevices();
      Reconcile(ids);
      last_reconcile = now;
    }
  }

  std::cout << "[ModbusSupervisor] Supervisor 종료 중...\n";
  KillAll();
  std::cout << "[ModbusSupervisor] Supervisor 종료 완료\n";
}

std::vector<int> ModbusSupervisor::QueryActiveDevices() {
  std::vector<int> ids;

#ifdef HAS_SQLITE
  sqlite3 *db = nullptr;
  int rc =
      sqlite3_open_v2(db_path_.c_str(), &db,
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "[ModbusSupervisor] DB 열기 실패: " << sqlite3_errmsg(db)
              << "\n";
    if (db)
      sqlite3_close(db);
    return ids;
  }

  const char *sql = "SELECT id FROM modbus_slave_devices "
                    "WHERE enabled = 1 ORDER BY id";

  sqlite3_stmt *stmt = nullptr;
  rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (rc == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW)
      ids.push_back(sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);

  std::cout << "[ModbusSupervisor] 활성 디바이스: " << ids.size() << "개\n";
#else
  std::cout << "[ModbusSupervisor] SQLite 없음 — 환경변수 DEVICE_IDS 사용\n";
  // 폴백: DEVICE_IDS=1,2,3 환경변수
  if (const char *v = std::getenv("DEVICE_IDS")) {
    std::string s(v);
    size_t pos = 0;
    while ((pos = s.find(',')) != std::string::npos) {
      ids.push_back(std::stoi(s.substr(0, pos)));
      s.erase(0, pos + 1);
    }
    if (!s.empty())
      ids.push_back(std::stoi(s));
  }
#endif

  return ids;
}

void ModbusSupervisor::Reconcile(const std::vector<int> &active_ids) {
  std::lock_guard<std::mutex> lock(children_mutex_);

  // 비활성화된 디바이스 kill
  std::vector<int> to_remove;
  for (auto &[id, child] : children_) {
    if (std::find(active_ids.begin(), active_ids.end(), id) ==
        active_ids.end()) {
      std::cout << "[ModbusSupervisor] 디바이스 " << id << " 비활성화 → 종료\n";
      KillChild(child);
      to_remove.push_back(id);
    }
  }
  for (int id : to_remove)
    children_.erase(id);

  // 신규 디바이스 spawn
  for (int id : active_ids) {
    if (children_.find(id) == children_.end())
      SpawnChild(id);
  }
}

void ModbusSupervisor::MonitorChildren() {
  std::lock_guard<std::mutex> lock(children_mutex_);

  for (auto &[id, child] : children_) {
    if (!IsChildAlive(child)) {
      if (child.restart_count >= MAX_RESTART_COUNT) {
        std::cerr << "[ModbusSupervisor] 디바이스 " << id
                  << " 최대 재시작 초과 — 포기\n";
        continue;
      }
      std::cout << "[ModbusSupervisor] 디바이스 " << id
                << " 크래시 감지 → 재시작 (" << child.restart_count + 1
                << "회)\n";
      child.restart_count++;

      // 재시작
      std::string args = "--device-id=" + std::to_string(id);
#ifdef _WIN32
      std::string cmd = exe_path_ + " " + args;
      STARTUPINFOA si{};
      si.cb = sizeof(si);
      PROCESS_INFORMATION pi{};
      if (CreateProcessA(nullptr, const_cast<char *>(cmd.c_str()), nullptr,
                         nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        child.process_handle = pi.hProcess;
        child.process_id = pi.dwProcessId;
        child.last_start = std::chrono::steady_clock::now();
        CloseHandle(pi.hThread);
        std::cout << "[ModbusSupervisor] 재시작 완료: device=" << id
                  << " PID=" << pi.dwProcessId << "\n";
      }
#else
      pid_t pid = fork();
      if (pid == 0) {
        execl(exe_path_.c_str(), exe_path_.c_str(), args.c_str(), nullptr);
        _exit(1);
      } else if (pid > 0) {
        child.pid = pid;
        child.last_start = std::chrono::steady_clock::now();
        std::cout << "[ModbusSupervisor] 재시작 완료: device=" << id
                  << " PID=" << pid << "\n";
      }
#endif
    }
  }
}

bool ModbusSupervisor::SpawnChild(int device_id) {
  ChildProcess child;
  child.device_id = device_id;
  child.name = "modbus-slave-" + std::to_string(device_id);
  child.last_start = std::chrono::steady_clock::now();

  std::string args = "--device-id=" + std::to_string(device_id);

#ifdef _WIN32
  std::string cmd = exe_path_ + " " + args;
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessA(nullptr, const_cast<char *>(cmd.c_str()), nullptr,
                      nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    std::cerr << "[ModbusSupervisor] spawn 실패: device=" << device_id
              << " err=" << GetLastError() << "\n";
    return false;
  }
  child.process_handle = pi.hProcess;
  child.process_id = pi.dwProcessId;
  CloseHandle(pi.hThread);
  std::cout << "[ModbusSupervisor] spawn 완료: device=" << device_id
            << " PID=" << pi.dwProcessId << "\n";
#else
  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "[ModbusSupervisor] fork 실패: device=" << device_id << "\n";
    return false;
  }
  if (pid == 0) {
    execl(exe_path_.c_str(), exe_path_.c_str(), args.c_str(), nullptr);
    _exit(1);
  }
  child.pid = pid;
  std::cout << "[ModbusSupervisor] spawn 완료: device=" << device_id
            << " PID=" << pid << "\n";
#endif

  children_[device_id] = child;
  return true;
}

bool ModbusSupervisor::IsChildAlive(const ChildProcess &child) const {
#ifdef _WIN32
  if (child.process_handle == INVALID_HANDLE_VALUE)
    return false;
  DWORD exit_code;
  return GetExitCodeProcess(child.process_handle, &exit_code) &&
         exit_code == STILL_ACTIVE;
#else
  if (child.pid <= 0)
    return false;
  int status;
  return waitpid(child.pid, &status, WNOHANG) == 0;
#endif
}

void ModbusSupervisor::KillChild(ChildProcess &child) {
#ifdef _WIN32
  if (child.process_handle != INVALID_HANDLE_VALUE) {
    TerminateProcess(child.process_handle, 0);
    WaitForSingleObject(child.process_handle, 5000);
    CloseHandle(child.process_handle);
    child.process_handle = INVALID_HANDLE_VALUE;
  }
#else
  if (child.pid > 0) {
    kill(child.pid, SIGTERM);
    for (int i = 0; i < 50; i++) {
      int st;
      if (waitpid(child.pid, &st, WNOHANG) != 0)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    kill(child.pid, SIGKILL);
    waitpid(child.pid, nullptr, 0);
    child.pid = -1;
  }
#endif
  std::cout << "[ModbusSupervisor] 종료: " << child.name << "\n";
}

void ModbusSupervisor::KillAll() {
  std::lock_guard<std::mutex> lock(children_mutex_);
  for (auto &[id, child] : children_)
    KillChild(child);
  children_.clear();
}

} // namespace ModbusSlave
} // namespace PulseOne
