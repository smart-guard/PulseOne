// =============================================================================
// core/modbus-slave/src/ModbusSlaveServer.cpp
// Modbus TCP 서버 — select() 멀티플렉싱
// =============================================================================
#include "ModbusSlaveServer.h"
#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

#ifdef HAS_MODBUS
#include <errno.h>
#include <modbus/modbus.h>
#endif

namespace PulseOne {
namespace ModbusSlave {

ModbusSlaveServer::ModbusSlaveServer() = default;

ModbusSlaveServer::~ModbusSlaveServer() { Stop(); }

bool ModbusSlaveServer::Start(RegisterTable &table, int port, uint8_t unit_id,
                              int max_clients) {
#ifndef HAS_MODBUS
  std::cerr << "[ModbusSlaveServer] libmodbus not available\n";
  return false;
#else
  table_ = &table;
  port_ = port;
  unit_id_ = unit_id;
  max_clients_ = max_clients;

  ctx_ = modbus_new_tcp("0.0.0.0", port_);
  if (!ctx_) {
    std::cerr << "[ModbusSlaveServer] modbus_new_tcp failed\n";
    return false;
  }

  modbus_set_slave(ctx_, unit_id_);
  // modbus_set_debug(ctx_, TRUE);  // 디버그 시 활성화

  // RegisterTable의 raw 포인터를 mb_map에 연결
  // 주의: modbus_mapping_new_start_address는 0 기반
  mb_map_ = modbus_mapping_new(RegisterTable::TABLE_SIZE, // coils
                               RegisterTable::TABLE_SIZE, // discrete inputs
                               RegisterTable::TABLE_SIZE, // holding registers
                               RegisterTable::TABLE_SIZE  // input registers
  );
  if (!mb_map_) {
    std::cerr << "[ModbusSlaveServer] modbus_mapping_new failed\n";
    modbus_free(ctx_);
    ctx_ = nullptr;
    return false;
  }

  // mb_map 내부 배열을 RegisterTable raw 포인터로 교체
  // (libmodbus가 mb_map 내부 메모리를 free하지 않도록 주의)
  // → 대신 모든 modbus_reply 전후에 동기화 방식 사용

  running_ = true;
  server_thread_ = std::thread(&ModbusSlaveServer::ServerLoop, this);

  std::cout << "[ModbusSlaveServer] Started on TCP port " << port_
            << " (Unit ID " << static_cast<int>(unit_id_) << ")\n";
  return true;
#endif
}

void ModbusSlaveServer::Stop() {
  running_ = false;
  if (server_thread_.joinable())
    server_thread_.join();

#ifdef HAS_MODBUS
  if (mb_map_) {
    modbus_mapping_free(mb_map_);
    mb_map_ = nullptr;
  }
  if (ctx_) {
    modbus_close(ctx_);
    modbus_free(ctx_);
    ctx_ = nullptr;
  }
#endif
  std::cout << "[ModbusSlaveServer] Stopped\n";
}

void ModbusSlaveServer::ServerLoop() {
#ifdef HAS_MODBUS
  server_socket_ = modbus_tcp_listen(ctx_, max_clients_);
  if (server_socket_ == -1) {
    std::cerr << "[ModbusSlaveServer] modbus_tcp_listen failed: "
              << modbus_strerror(errno) << "\n";
    running_ = false;
    return;
  }

  fd_set refset, rdset;
  FD_ZERO(&refset);
  FD_SET(server_socket_, &refset);
  int fdmax = server_socket_;

  while (running_) {
    rdset = refset;
    timeval tv = {1, 0}; // 1초 타임아웃 — 종료 체크용

    int rc = select(fdmax + 1, &rdset, nullptr, nullptr, &tv);
    if (rc < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (rc == 0)
      continue; // 타임아웃, 루프 계속

    for (int fd = 0; fd <= fdmax; fd++) {
      if (!FD_ISSET(fd, &rdset))
        continue;

      if (fd == server_socket_) {
        // 신규 클라이언트 접속
        int newfd = modbus_tcp_accept(ctx_, &server_socket_);
        if (newfd != -1) {
          FD_SET(newfd, &refset);
          fdmax = std::max(fdmax, newfd);
          connected_clients_++;
          std::cout << "[ModbusSlaveServer] Client connected fd=" << newfd
                    << " (total=" << connected_clients_.load() << ")\n";
        }
      } else {
        // 기존 클라이언트 요청 처리
        modbus_set_socket(ctx_, fd);

        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int len = modbus_receive(ctx_, query);

        if (len > 0) {
          // RegisterTable → mb_map 동기화 (읽기 요청 처리 전)
          table_->LockForModbus();
          memcpy(mb_map_->tab_registers, table_->RawHoldingRegs(),
                 RegisterTable::TABLE_SIZE * sizeof(uint16_t));
          memcpy(mb_map_->tab_input_registers, table_->RawInputRegs(),
                 RegisterTable::TABLE_SIZE * sizeof(uint16_t));
          memcpy(mb_map_->tab_bits, table_->RawCoils(),
                 RegisterTable::TABLE_SIZE);
          memcpy(mb_map_->tab_input_bits, table_->RawDiscreteInputs(),
                 RegisterTable::TABLE_SIZE);
          table_->UnlockForModbus();

          // 응답 전송
          modbus_reply(ctx_, query, len, mb_map_);

          // FC05/06/15/16 쓰기 요청이면 RegisterTable에 역방향 기록
          HandleWriteRequest(query, len);

        } else if (len == -1) {
          // 클라이언트 연결 종료
          std::cout << "[ModbusSlaveServer] Client disconnected fd=" << fd
                    << "\n";
          close(fd);
          FD_CLR(fd, &refset);
          connected_clients_--;
        }
      }
    }
  }

  close(server_socket_);
  server_socket_ = -1;
#endif
}

void ModbusSlaveServer::HandleWriteRequest(const uint8_t *query, int rc) {
#ifdef HAS_MODBUS
  if (rc < 2)
    return;
  uint8_t func_code = query[modbus_get_header_length(ctx_)];

  switch (func_code) {
  case MODBUS_FC_WRITE_SINGLE_COIL: { // FC05
    uint16_t addr = (query[8] << 8) | query[9];
    bool value = (query[10] == 0xFF);
    table_->OnExternalCoilWrite(addr, value);
    break;
  }
  case MODBUS_FC_WRITE_SINGLE_REGISTER: { // FC06
    uint16_t addr = (query[8] << 8) | query[9];
    uint16_t value = (query[10] << 8) | query[11];
    table_->OnExternalRegisterWrite(addr, value);
    break;
  }
  case MODBUS_FC_WRITE_MULTIPLE_COILS:     // FC15
  case MODBUS_FC_WRITE_MULTIPLE_REGISTERS: // FC16
    // 멀티 쓰기는 mb_map에서 이미 반영됨
    // 역방향 콜백 필요 시 추가 구현
    break;
  default:
    break;
  }
#else
  (void)query;
  (void)rc;
#endif
}

} // namespace ModbusSlave
} // namespace PulseOne
