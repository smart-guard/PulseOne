// =============================================================================
// core/modbus-slave/src/main.cpp
// pulseone-modbus-slave 진입점
// =============================================================================
#include "MappingConfig.h"
#include "ModbusSlaveServer.h"
#include "RedisSubscriber.h"
#include "RegisterTable.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace PulseOne::ModbusSlave;

// =============================================================================
// 종료 시그널 처리
// =============================================================================
static std::atomic<bool> g_shutdown{false};

static void signal_handler(int sig) {
  std::cout << "\n[ModbusSlave] 종료 신호(" << sig << ") 수신\n";
  g_shutdown = true;
}

// =============================================================================
// 배너
// =============================================================================
static void PrintBanner(const SlaveConfig &cfg) {
  std::cout << R"(
 ____        _          ___
|  _ \ _   _| |___  ___/ _ \ _ __   ___
| |_) | | | | / __|/ _ \ | | | '_ \ / _ \
|  __/| |_| | \__ \  __/ |_| | | | |  __/
|_|    \__,_|_|___/\___|\___/|_| |_|\___|
  ModbusSlave v1.0  —  PulseOne Platform
)" << "\n";
  std::cout << "  TCP Port  : " << cfg.tcp_port << "\n";
  std::cout << "  Unit ID   : " << static_cast<int>(cfg.unit_id) << "\n";
  std::cout << "  Redis     : " << cfg.redis_host << ":" << cfg.redis_port
            << "\n";
  std::cout << "  Mappings  : " << cfg.mappings.size() << " points\n";
  std::cout << "─────────────────────────────────────────\n\n";
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char **argv) {
  // 1. 설정 로드
  std::string config_path = "config/modbus-slave-mapping.json";
  if (argc > 1)
    config_path = argv[1];

  SlaveConfig config = MappingConfig::LoadFromFile(config_path);
  MappingConfig::ApplyEnvOverrides(config);

  PrintBanner(config);

  // 2. 시그널 등록
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // 3. 서비스 초기화
  RegisterTable table;
  RedisSubscriber redis;
  ModbusSlaveServer server;

  // 4. Redis 구독 시작 (Collector 수집값 수신)
  if (!redis.Start(config.redis_host, config.redis_port, table,
                   config.mappings)) {
    std::cerr << "[ModbusSlave] Redis 연결 실패. "
                 "Redis 없이 Modbus 서버만 기동.\n";
    // Redis 없이도 서버는 기동 (0값으로 응답)
  }

  // 5. Modbus TCP 서버 시작
  if (!server.Start(table, config.tcp_port, config.unit_id,
                    config.max_clients)) {
    std::cerr << "[ModbusSlave] Modbus TCP 서버 시작 실패. 종료합니다.\n";
    redis.Stop();
    return 1;
  }

  std::cout << "[ModbusSlave] 서비스 준비 완료. 종료하려면 Ctrl+C\n\n";

  // 6. 메인 루프 (상태 출력)
  auto next_status =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!g_shutdown) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (std::chrono::steady_clock::now() >= next_status) {
      std::cout << "[ModbusSlave] 상태 — "
                << "연결 클라이언트: " << server.ConnectedClients()
                << " | Redis: " << (redis.IsConnected() ? "OK" : "DISCONNECTED")
                << "\n";
      next_status += std::chrono::seconds(30);
    }
  }

  // 7. 종료
  std::cout << "[ModbusSlave] 종료 중...\n";
  server.Stop();
  redis.Stop();
  std::cout << "[ModbusSlave] 종료 완료.\n";
  return 0;
}
