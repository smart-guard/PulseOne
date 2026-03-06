// =============================================================================
// core/modbus-slave/include/ModbusSlaveServer.h  (v2 — 상업용)
// ClientManager + SlaveStatistics 통합
// =============================================================================
#pragma once

#include "ClientManager.h"
#include "RegisterTable.h"
#include "SlaveStatistics.h"
#include <atomic>
#include <string>
#include <thread>

#ifdef HAS_MODBUS
#include <modbus/modbus.h>
#else
struct _modbus;
typedef struct _modbus modbus_t;
struct _modbus_mapping_t;
typedef struct _modbus_mapping_t modbus_mapping_t;
#endif

namespace PulseOne {
namespace ModbusSlave {

class ModbusSlaveServer {
public:
  ModbusSlaveServer();
  ~ModbusSlaveServer();

  bool Start(RegisterTable &table, ClientManager &client_mgr,
             SlaveStatistics &stats, int port = 502, uint8_t unit_id = 1,
             int max_clients = 20);
  void Stop();

  bool IsRunning() const { return running_.load(); }
  int ConnectedClients() const {
    return client_mgr_ ? (int)client_mgr_->CurrentClients() : 0;
  }

private:
  void ServerLoop();
  // 클라이언트 IP 추출
  static std::string GetClientIp(int fd, uint16_t &port);

  modbus_t *ctx_ = nullptr;
  modbus_mapping_t *mb_map_ = nullptr;
  RegisterTable *table_ = nullptr;
  ClientManager *client_mgr_ = nullptr;
  SlaveStatistics *stats_ = nullptr;

  int port_ = 502;
  uint8_t unit_id_ = 1;
  int max_clients_ = 20;
  int server_socket_ = -1;

  std::atomic<bool> running_{false};
  std::thread server_thread_;
};

} // namespace ModbusSlave
} // namespace PulseOne
