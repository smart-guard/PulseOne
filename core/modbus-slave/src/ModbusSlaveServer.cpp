// =============================================================================
// core/modbus-slave/src/ModbusSlaveServer.cpp  (v2 — 상업용)
// ClientManager + SlaveStatistics 통합, IP 추출, 응답시간 측정
// =============================================================================
#include "ModbusSlaveServer.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
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

// 소켓 FD에서 클라이언트 IP:port 추출
std::string ModbusSlaveServer::GetClientIp(int fd, uint16_t &port) {
  sockaddr_in addr{};
  socklen_t len = sizeof(addr);
  if (getpeername(fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0) {
    port = ntohs(addr.sin_port);
    char buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::string(buf);
  }
  port = 0;
  return "unknown";
}

bool ModbusSlaveServer::Start(RegisterTable &table, ClientManager &client_mgr,
                              SlaveStatistics &stats, int port, uint8_t unit_id,
                              int max_clients) {
#ifndef HAS_MODBUS
  std::cerr << "[ModbusSlaveServer] libmodbus 없음\n";
  return false;
#else
  table_ = &table;
  client_mgr_ = &client_mgr;
  stats_ = &stats;
  port_ = port;
  unit_id_ = unit_id;
  max_clients_ = max_clients;

  ctx_ = modbus_new_tcp("0.0.0.0", port_);
  if (!ctx_) {
    std::cerr << "[ModbusSlaveServer] modbus_new_tcp 실패\n";
    return false;
  }
  modbus_set_slave(ctx_, unit_id_);

  mb_map_ =
      modbus_mapping_new(RegisterTable::TABLE_SIZE, RegisterTable::TABLE_SIZE,
                         RegisterTable::TABLE_SIZE, RegisterTable::TABLE_SIZE);
  if (!mb_map_) {
    std::cerr << "[ModbusSlaveServer] 레지스터 테이블 할당 실패\n";
    modbus_free(ctx_);
    ctx_ = nullptr;
    return false;
  }

  running_ = true;
  server_thread_ = std::thread(&ModbusSlaveServer::ServerLoop, this);
  std::cout << "[ModbusSlaveServer] 시작: TCP :" << port_
            << " Unit=" << static_cast<int>(unit_id_) << "\n";
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
  std::cout << "[ModbusSlaveServer] 중지\n";
}

void ModbusSlaveServer::ServerLoop() {
#ifdef HAS_MODBUS
  server_socket_ = modbus_tcp_listen(ctx_, max_clients_);
  if (server_socket_ == -1) {
    std::cerr << "[ModbusSlaveServer] listen 실패: " << modbus_strerror(errno)
              << "\n";
    running_ = false;
    return;
  }

  fd_set refset, rdset;
  FD_ZERO(&refset);
  FD_SET(server_socket_, &refset);
  int fdmax = server_socket_;

  while (running_) {
    rdset = refset;
    timeval tv = {1, 0};
    int rc = select(fdmax + 1, &rdset, nullptr, nullptr, &tv);
    if (rc < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (rc == 0)
      continue;

    for (int fd = 0; fd <= fdmax; fd++) {
      if (!FD_ISSET(fd, &rdset))
        continue;

      if (fd == server_socket_) {
        // 신규 접속
        int newfd = modbus_tcp_accept(ctx_, &server_socket_);
        if (newfd < 0)
          continue;

        uint16_t client_port = 0;
        std::string ip = GetClientIp(newfd, client_port);

        // ClientManager에 등록 (IP 차단 여부 확인)
        if (!client_mgr_->OnConnect(newfd, ip, client_port)) {
          // 차단 or 동시 접속 초과 → 연결 거부
          close(newfd);
          continue;
        }

        FD_SET(newfd, &refset);
        fdmax = std::max(fdmax, newfd);

      } else {
        // 요청 처리
        modbus_set_socket(ctx_, fd);
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

        auto t0 = std::chrono::high_resolution_clock::now();
        int len = modbus_receive(ctx_, query);
        bool success = false;

        if (len > 0) {
          // RegisterTable → mb_map 동기화 (dirty 비트로 최적화 가능)
          table_->LockForModbus();
          std::memcpy(mb_map_->tab_registers, table_->RawHoldingRegs(),
                      RegisterTable::TABLE_SIZE * sizeof(uint16_t));
          std::memcpy(mb_map_->tab_input_registers, table_->RawInputRegs(),
                      RegisterTable::TABLE_SIZE * sizeof(uint16_t));
          std::memcpy(mb_map_->tab_bits, table_->RawCoils(),
                      RegisterTable::TABLE_SIZE);
          std::memcpy(mb_map_->tab_input_bits, table_->RawDiscreteInputs(),
                      RegisterTable::TABLE_SIZE);
          table_->UnlockForModbus();

          int reply = modbus_reply(ctx_, query, len, mb_map_);
          success = (reply >= 0);

          // FC05/06 역방향 쓰기 처리
          if (success) {
            int header_len = modbus_get_header_length(ctx_);
            uint8_t fc = query[header_len];
            if (fc == 0x05) { // Write Single Coil
              uint16_t addr =
                  (query[header_len + 1] << 8) | query[header_len + 2];
              bool val = (query[header_len + 3] == 0xFF);
              table_->OnExternalCoilWrite(addr, val);
            } else if (fc == 0x06) { // Write Single Register
              uint16_t addr =
                  (query[header_len + 1] << 8) | query[header_len + 2];
              uint16_t val =
                  (query[header_len + 3] << 8) | query[header_len + 4];
              table_->OnExternalRegisterWrite(addr, val);
            }
          }

          // 통계 기록
          auto t1 = std::chrono::high_resolution_clock::now();
          uint64_t us =
              std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
                  .count();
          int header_len = modbus_get_header_length(ctx_);
          uint8_t fc = query[header_len];
          stats_->Record(fc, success, us);
          client_mgr_->OnRequest(fd, fc, success, us);

        } else if (len == -1) {
          // 연결 해제
          client_mgr_->OnDisconnect(fd);
          close(fd);
          FD_CLR(fd, &refset);
        }
      }
    }
  }
  if (server_socket_ >= 0)
    close(server_socket_);
  server_socket_ = -1;
#endif
}

} // namespace ModbusSlave
} // namespace PulseOne
