/**
 * @file ProcessSupervisor.h
 * @brief Cross-platform process supervisor for spawning and monitoring
 *        multiple child worker processes based on DB entries.
 *
 * Usage:
 *   ProcessSupervisor supervisor("collector", argv[0]);
 *   supervisor.run();  // blocks until shutdown signal
 */
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

namespace PulseOne {
namespace Utils {

struct ChildProcess {
  int instance_id;
  std::string name;
#ifdef _WIN32
  HANDLE process_handle = INVALID_HANDLE_VALUE;
  DWORD process_id = 0;
#else
  pid_t pid = -1;
#endif
  int restart_count = 0;
  std::chrono::steady_clock::time_point last_start;
};

class ProcessSupervisor {
public:
  /**
   * @param server_type  "collector" or "gateway"
   * @param exe_path     Path to the current executable (argv[0])
   * @param config_path  Optional config directory path
   */
  ProcessSupervisor(const std::string &server_type, const std::string &exe_path,
                    const std::string &config_path = "");

  ~ProcessSupervisor();

  /// Main loop: query DB, spawn children, monitor. Blocks until shutdown.
  void run();

  /// Request graceful shutdown
  void requestShutdown();

  /// Check if shutdown was requested
  bool isShutdownRequested() const;

private:
  /// Query edge_servers for active instances of this server_type
  std::vector<int> queryActiveInstances();

  /// Spawn a child process with --id=N
  bool spawnChild(int instance_id);

  /// Check if a child process is still alive
  bool isChildAlive(const ChildProcess &child);

  /// Kill a child process
  void killChild(ChildProcess &child);

  /// Kill all children and wait for exit
  void killAllChildren();

  /// Reconcile: spawn new, kill removed
  void reconcile(const std::vector<int> &active_ids);

  /// Monitor loop iteration
  void monitorChildren();

  std::string server_type_;
  std::string exe_path_;
  std::string config_path_;
  std::atomic<bool> shutdown_requested_{false};

  std::mutex children_mutex_;
  std::map<int, ChildProcess> children_; // instance_id -> child

  static constexpr int MONITOR_INTERVAL_SEC = 10;
  static constexpr int RECONCILE_INTERVAL_SEC = 60;
  static constexpr int MAX_RESTART_COUNT = 10;
};

} // namespace Utils
} // namespace PulseOne
