// =============================================================================
// core/modbus-slave/src/main.cpp  (v3 — ProcessSupervisor 패턴)
// export-gateway와 동일한 Supervisor/Worker 이중 실행 구조
//
// [Supervisor 모드] 인자 없음:
//   DB에서 modbus_slave_devices 조회 → 디바이스별 자식 프로세스 spawn
//
// [Worker 모드] --device-id=N:
//   DB에서 해당 디바이스 설정 로드 → Modbus TCP 서버 기동
// =============================================================================
#include "ClientManager.h"
#include "DbMappingLoader.h"
#include "MappingConfig.h"
#include "ModbusSlaveServer.h"
#include "ModbusSupervisor.h"
#include "RedisSubscriber.h"
#include "RegisterTable.h"
#include "SlaveStatistics.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace PulseOne::ModbusSlave;

static std::atomic<bool> g_shutdown{false};
static ModbusSupervisor *g_supervisor = nullptr;

static void signal_handler(int sig) {
  std::cout << "\n[ModbusSlave] 종료 신호(" << sig << ") 수신\n";
  g_shutdown = true;
  if (g_supervisor)
    g_supervisor->RequestShutdown();
}

// =============================================================================
// Worker 모드: --device-id=N
// DB에서 해당 디바이스 설정 로드 → TCP 서버 기동
// =============================================================================
static int RunWorker(int device_id, const std::string &db_path) {
  std::cout << "=== Modbus Slave Worker (device-id=" << device_id << ") ===\n";

  // 1. DB에서 디바이스 설정 로드
  SlaveConfig config;
  std::vector<RegisterMapping> mappings;

  bool loaded_from_db = false;

#ifdef HAS_SQLITE
  // DB에서 모든 설정 로드
  loaded_from_db = LoadDeviceFromDb(device_id, db_path, config, mappings);
#endif

  if (!loaded_from_db) {
    // 폴백: JSON + 환경변수
    std::cout << "[Worker] DB 로드 실패 → JSON 설정 사용\n";
    std::string config_path = "config/modbus-slave-mapping.json";
    if (const char *v = std::getenv("MODBUS_CONFIG"))
      config_path = v;
    config = MappingConfig::LoadFromFile(config_path);
    // DB 미사용 시만 환경변수 전체 오버라이드
    MappingConfig::ApplyEnvOverrides(config);
  } else {
    // DB 로드 성공: Redis 설정만 오버라이드, 포트/unit_id는 DB 값 유지
    if (const char *v = std::getenv("REDIS_PRIMARY_HOST"))
      config.redis_host = v;
    else if (const char *v2 = std::getenv("REDIS_HOST"))
      config.redis_host = v2;
    if (const char *v = std::getenv("REDIS_PRIMARY_PORT"))
      config.redis_port = std::atoi(v);
    else if (const char *v2 = std::getenv("REDIS_PORT"))
      config.redis_port = std::atoi(v2);
  }

  if (mappings.empty() && loaded_from_db) {
    // LoadDeviceFromDb는 성공했지만 매핑이 없음 (활성화된 매핑 없음)
    // 이 경우 DbMappingLoader를 사용하되 device_id 필터 적용
    DbMappingLoader db_loader(db_path);
    if (db_loader.IsConnected())
      mappings =
          db_loader.Load(device_id); // device_id 필터 없이 전체 로드 방지
  }
  if (mappings.empty() && !loaded_from_db)
    mappings = config.mappings;

  std::cout << "  Port     : " << config.tcp_port << "\n";
  std::cout << "  Unit ID  : " << (int)config.unit_id << "\n";
  std::cout << "  Redis    : " << config.redis_host << ":" << config.redis_port
            << "\n";
  std::cout << "  Mappings : " << mappings.size() << "개\n\n";

  // 2. 서비스 객체 초기화
  RegisterTable table;
  ClientManager client_mgr;
  SlaveStatistics stats;
  RedisSubscriber redis;
  ModbusSlaveServer server;

  // 3. Redis 구독 시작
  if (!redis.Start(config.redis_host, config.redis_port, table, mappings)) {
    std::cerr << "[Worker] Redis 연결 실패 (0값으로 응답)\n";
  }

  // 4. Modbus TCP 서버 시작
  server.SetPacketLogging(config.packet_logging, device_id); // 패킷 로그 설정
  if (!server.Start(table, client_mgr, stats, config.tcp_port, config.unit_id,
                    config.max_clients)) {
    std::cerr << "[Worker] Modbus TCP 서버 시작 실패\n";
    redis.Stop();
    return 1;
  }

  std::cout << "[Worker] device=" << device_id << " 준비 완료\n";

  // 5. 메인 루프
  auto next_status =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  auto next_reload = std::chrono::steady_clock::now() + std::chrono::minutes(5);

  while (!g_shutdown) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto now = std::chrono::steady_clock::now();

    if (now >= next_status) {
      auto ws = stats.GetWindowStats(5);
      std::cout << "[Worker-" << device_id << "] "
                << "clients=" << client_mgr.CurrentClients()
                << " redis=" << (redis.IsConnected() ? "OK" : "DISC")
                << " 5min_req=" << ws.requests << " success=" << std::fixed
                << ws.success_rate * 100.0 << "%\n";

      // Redis에 통계/클라이언트 세션 게시 (백엔드 API가 읽어감)
      stats.PublishToRedis(config.redis_host, config.redis_port, device_id);
      client_mgr.PublishToRedis(config.redis_host, config.redis_port,
                                device_id);

      next_status = now + std::chrono::seconds(30);
    }

    if (now >= next_reload) {
      DbMappingLoader db_loader(db_path);
      if (db_loader.IsConnected()) {
        std::vector<RegisterMapping> new_mappings = mappings;
        if (db_loader.Reload(new_mappings, device_id)) { // device_id 필터 적용
          redis.Stop();
          mappings = new_mappings;
          redis.Start(config.redis_host, config.redis_port, table, mappings);
          std::cout << "[Worker-" << device_id
                    << "] 매핑 재로드: " << mappings.size() << "개\n";
        }
      }
      next_reload = now + std::chrono::minutes(5);
    }
  }

  // 6. 종료
  std::cout << "[Worker-" << device_id << "] 통계:\n" << stats.ToJson() << "\n";
  server.Stop();
  redis.Stop();
  return 0;
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char **argv) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // DB / 설정 경로
  std::string db_path = "data/db/pulseone.db";
  if (const char *v = std::getenv("SQLITE_PATH"))
    db_path = v;

  int device_id = -1;

  // 인자 파싱
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--help") {
      std::cout << "용법:\n"
                << "  pulseone-modbus-slave              # Supervisor 모드\n"
                << "  pulseone-modbus-slave --device-id=N  # Worker 모드\n";
      return 0;
    } else if (arg.find("--device-id=") == 0) {
      device_id = std::stoi(arg.substr(12));
    } else if (arg == "--device-id" && i + 1 < argc) {
      device_id = std::stoi(argv[++i]);
    }
  }

  // ── Worker 모드 ──
  if (device_id >= 0)
    return RunWorker(device_id, db_path);

  // ── Supervisor 모드 ──
  std::cout << "=================================================\n";
  std::cout << "  Modbus Slave Supervisor\n";
  std::cout << "  DB: " << db_path << "\n";
  std::cout << "=================================================\n\n";

  ModbusSupervisor supervisor(argv[0], db_path);
  g_supervisor = &supervisor;
  supervisor.Run();
  g_supervisor = nullptr;

  return 0;
}
