// =============================================================================
// core/modbus-slave/include/ModbusSlaveServer.h
// Modbus TCP 슬레이브 서버 (libmodbus 기반)
// =============================================================================
#pragma once

#include "RegisterTable.h"
#include <atomic>
#include <string>
#include <thread>
#include <vector>

// libmodbus 조건부 포함
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

  // 서버 시작
  // port: TCP 포트 (기본 502)
  // unit_id: Modbus Unit ID (Slave ID, 기본 1)
  bool Start(RegisterTable &table, int port = 502, uint8_t unit_id = 1,
             int max_clients = 10);
  void Stop();

  bool IsRunning() const { return running_.load(); }
  int ConnectedClients() const { return connected_clients_.load(); }

private:
  void ServerLoop();

  modbus_t *ctx_ = nullptr;
  modbus_mapping_t *mb_map_ = nullptr;
  RegisterTable *table_ = nullptr;

  int port_ = 502;
  uint8_t unit_id_ = 1;
  int max_clients_ = 10;
  int server_socket_ = -1;

  std::atomic<bool> running_{false};
  std::atomic<int> connected_clients_{0};
  std::thread server_thread_;

  // 클라이언트 fd 집합
  std::vector<int> client_fds_;

  // FC05/06/15/16 쓰기 요청 → RegisterTable 콜백
  void HandleWriteRequest(const uint8_t *query, int rc);
};

} // namespace ModbusSlave
} // namespace PulseOne
