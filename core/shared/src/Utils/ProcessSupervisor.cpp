/**
 * @file ProcessSupervisor.cpp
 * @brief Cross-platform process supervisor implementation
 */

#include "Utils/ProcessSupervisor.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <thread>

#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace PulseOne {
namespace Utils {

ProcessSupervisor::ProcessSupervisor(const std::string &server_type,
                                     const std::string &exe_path,
                                     const std::string &config_path)
    : server_type_(server_type), exe_path_(exe_path),
      config_path_(config_path) {
  LogManager::getInstance().Info("🔧 ProcessSupervisor initialized for type: " +
                                 server_type_);
}

ProcessSupervisor::~ProcessSupervisor() { killAllChildren(); }

void ProcessSupervisor::run() {
  LogManager::getInstance().Info("🚀 Supervisor mode starting for: " +
                                 server_type_);

  // 1. Initial DB query + spawn
  auto active_ids = queryActiveInstances();
  if (active_ids.empty()) {
    LogManager::getInstance().Warn(
        "⚠️ No active " + server_type_ +
        " instances found in DB. Waiting for new registrations...");
  }
  reconcile(active_ids);

  // 2. Monitor loop
  auto last_reconcile = std::chrono::steady_clock::now();

  while (!shutdown_requested_.load()) {
    // Sleep 10 seconds (interruptible)
    for (int i = 0;
         i < MONITOR_INTERVAL_SEC * 10 && !shutdown_requested_.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (shutdown_requested_.load())
      break;

    // Monitor: restart dead children
    monitorChildren();

    // Reconcile: check DB for new/removed instances every 60s
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_reconcile)
            .count() >= RECONCILE_INTERVAL_SEC) {
      auto ids = queryActiveInstances();
      reconcile(ids);
      last_reconcile = now;
    }
  }

  LogManager::getInstance().Info("🛑 Supervisor shutting down...");
  killAllChildren();
  LogManager::getInstance().Info("✅ Supervisor shutdown complete");
}

void ProcessSupervisor::requestShutdown() { shutdown_requested_.store(true); }

bool ProcessSupervisor::isShutdownRequested() const {
  return shutdown_requested_.load();
}

std::vector<int> ProcessSupervisor::queryActiveInstances() {
  std::vector<int> ids;
  try {
    auto &db_mgr = DbLib::DatabaseManager::getInstance();
    std::string query =
        "SELECT id FROM edge_servers WHERE server_type = '" + server_type_ +
        "' AND status = 'active' AND is_deleted = 0 ORDER BY id";

    std::vector<std::vector<std::string>> results;
    if (db_mgr.executeQuery(query, results)) {
      for (const auto &row : results) {
        if (!row.empty()) {
          try {
            ids.push_back(std::stoi(row[0]));
          } catch (...) {
          }
        }
      }
    }
    LogManager::getInstance().Info("📋 Found " + std::to_string(ids.size()) +
                                   " active " + server_type_ +
                                   " instance(s) in DB");
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("DB query failed: " +
                                    std::string(e.what()));
  }
  return ids;
}

void ProcessSupervisor::reconcile(const std::vector<int> &active_ids) {
  std::lock_guard<std::mutex> lock(children_mutex_);

  // 1. Kill children that are no longer in active list
  std::vector<int> to_remove;
  for (auto &[id, child] : children_) {
    if (std::find(active_ids.begin(), active_ids.end(), id) ==
        active_ids.end()) {
      LogManager::getInstance().Info("🗑️ Instance " + std::to_string(id) +
                                     " removed from DB, killing child");
      killChild(child);
      to_remove.push_back(id);
    }
  }
  for (int id : to_remove) {
    children_.erase(id);
  }

  // 2. Spawn children for new active IDs
  for (int id : active_ids) {
    if (children_.find(id) == children_.end()) {
      spawnChild(id);
    }
  }
}

void ProcessSupervisor::monitorChildren() {
  std::lock_guard<std::mutex> lock(children_mutex_);

  for (auto &[id, child] : children_) {
    if (!isChildAlive(child)) {
      if (child.restart_count >= MAX_RESTART_COUNT) {
        LogManager::getInstance().Error(
            "💀 Instance " + std::to_string(id) + " exceeded max restarts (" +
            std::to_string(MAX_RESTART_COUNT) + "), giving up");
        continue;
      }

      LogManager::getInstance().Warn(
          "⚠️ Instance " + std::to_string(id) + " died, restarting (attempt " +
          std::to_string(child.restart_count + 1) + ")");

      // Rebuild command and spawn
      child.restart_count++;

#ifdef _WIN32
      child.process_handle = INVALID_HANDLE_VALUE;
      child.process_id = 0;
#else
      child.pid = -1;
#endif

      // Re-spawn (without lock since we already hold it - call internal)
      std::string cmd_args = "--id=" + std::to_string(id);
      if (!config_path_.empty()) {
        cmd_args += " --config=" + config_path_;
      }

#ifdef _WIN32
      std::string cmd_line = exe_path_ + " " + cmd_args;
      STARTUPINFOA si = {};
      si.cb = sizeof(si);
      PROCESS_INFORMATION pi = {};

      if (CreateProcessA(nullptr, const_cast<char *>(cmd_line.c_str()), nullptr,
                         nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        child.process_handle = pi.hProcess;
        child.process_id = pi.dwProcessId;
        child.last_start = std::chrono::steady_clock::now();
        CloseHandle(pi.hThread);
        LogManager::getInstance().Info("🔄 Restarted " + server_type_ +
                                       " ID=" + std::to_string(id) + " (PID " +
                                       std::to_string(pi.dwProcessId) + ")");
      } else {
        LogManager::getInstance().Error("Failed to restart " + server_type_ +
                                        " ID=" + std::to_string(id));
      }
#else
      pid_t pid = fork();
      if (pid == 0) {
        // Child process
        std::string id_arg = "--id=" + std::to_string(id);
        if (config_path_.empty()) {
          execl(exe_path_.c_str(), exe_path_.c_str(), id_arg.c_str(), nullptr);
        } else {
          std::string config_arg = "--config=" + config_path_;
          execl(exe_path_.c_str(), exe_path_.c_str(), id_arg.c_str(),
                config_arg.c_str(), nullptr);
        }
        _exit(1); // exec failed
      } else if (pid > 0) {
        child.pid = pid;
        child.last_start = std::chrono::steady_clock::now();
        LogManager::getInstance().Info("🔄 Restarted " + server_type_ +
                                       " ID=" + std::to_string(id) + " (PID " +
                                       std::to_string(pid) + ")");
      }
#endif
    }
  }
}

bool ProcessSupervisor::spawnChild(int instance_id) {
  ChildProcess child;
  child.instance_id = instance_id;
  child.name = server_type_ + "-" + std::to_string(instance_id);
  child.last_start = std::chrono::steady_clock::now();

  std::string cmd_args = "--id=" + std::to_string(instance_id);
  if (!config_path_.empty()) {
    cmd_args += " --config=" + config_path_;
  }

#ifdef _WIN32
  std::string cmd_line = exe_path_ + " " + cmd_args;
  STARTUPINFOA si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  if (!CreateProcessA(nullptr, const_cast<char *>(cmd_line.c_str()), nullptr,
                      nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    LogManager::getInstance().Error("❌ Failed to spawn " + child.name +
                                    " (error " +
                                    std::to_string(GetLastError()) + ")");
    return false;
  }

  child.process_handle = pi.hProcess;
  child.process_id = pi.dwProcessId;
  CloseHandle(pi.hThread);

  LogManager::getInstance().Info("✅ Spawned " + child.name + " (PID " +
                                 std::to_string(pi.dwProcessId) + ")");
#else
  pid_t pid = fork();
  if (pid < 0) {
    LogManager::getInstance().Error("❌ Failed to fork for " + child.name);
    return false;
  }

  if (pid == 0) {
    // Child process
    std::string id_arg = "--id=" + std::to_string(instance_id);
    if (config_path_.empty()) {
      execl(exe_path_.c_str(), exe_path_.c_str(), id_arg.c_str(), nullptr);
    } else {
      std::string config_arg = "--config=" + config_path_;
      execl(exe_path_.c_str(), exe_path_.c_str(), id_arg.c_str(),
            config_arg.c_str(), nullptr);
    }
    _exit(1); // exec failed
  }

  child.pid = pid;
  LogManager::getInstance().Info("✅ Spawned " + child.name + " (PID " +
                                 std::to_string(pid) + ")");
#endif

  children_[instance_id] = child;
  return true;
}

bool ProcessSupervisor::isChildAlive(const ChildProcess &child) {
#ifdef _WIN32
  if (child.process_handle == INVALID_HANDLE_VALUE)
    return false;
  DWORD exit_code;
  if (GetExitCodeProcess(child.process_handle, &exit_code)) {
    return exit_code == STILL_ACTIVE;
  }
  return false;
#else
  if (child.pid <= 0)
    return false;
  int status;
  pid_t result = waitpid(child.pid, &status, WNOHANG);
  return (result == 0); // 0 = still running
#endif
}

void ProcessSupervisor::killChild(ChildProcess &child) {
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
    // Wait up to 5 seconds
    for (int i = 0; i < 50; ++i) {
      int status;
      pid_t result = waitpid(child.pid, &status, WNOHANG);
      if (result != 0)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // Force kill if still alive
    kill(child.pid, SIGKILL);
    waitpid(child.pid, nullptr, 0);
    child.pid = -1;
  }
#endif
  LogManager::getInstance().Info("🛑 Killed child: " + child.name);
}

void ProcessSupervisor::killAllChildren() {
  std::lock_guard<std::mutex> lock(children_mutex_);
  for (auto &[id, child] : children_) {
    killChild(child);
  }
  children_.clear();
}

} // namespace Utils
} // namespace PulseOne
