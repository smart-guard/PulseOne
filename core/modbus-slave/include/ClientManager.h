// =============================================================================
// core/modbus-slave/include/ClientManager.h
// 접속 클라이언트 생명주기 및 통계 관리 (상업용)
// =============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace PulseOne {
namespace ModbusSlave {

// 단일 클라이언트 세션 정보
struct ClientSession {
  int fd = -1;
  std::string ip;
  uint16_t port = 0;
  std::chrono::system_clock::time_point connected_at;
  std::chrono::system_clock::time_point last_request_at;

  // 요청 통계 (세션 단위)
  uint64_t total_requests = 0;
  uint64_t failed_requests = 0;
  uint64_t fc01_count = 0; // Read Coils
  uint64_t fc02_count = 0; // Read Discrete Inputs
  uint64_t fc03_count = 0; // Read Holding Registers
  uint64_t fc04_count = 0; // Read Input Registers
  uint64_t fc05_count = 0; // Write Single Coil
  uint64_t fc06_count = 0; // Write Single Register
  uint64_t fc15_count = 0; // Write Multiple Coils
  uint64_t fc16_count = 0; // Write Multiple Registers

  double avg_response_us = 0.0; // 평균 응답 시간 (마이크로초)
  uint64_t max_response_us = 0; // 최대 응답 시간

  bool blocked = false; // IP 블랙리스트 차단 여부

  // 연결 지속 시간 (초)
  int64_t DurationSeconds() const;
  // 성공률 (0.0~1.0)
  double SuccessRate() const;
};

// IP별 누적 통계 (세션 종료 후에도 유지)
struct IpStats {
  std::string ip;
  uint64_t total_sessions = 0;
  uint64_t total_requests = 0;
  uint64_t total_failures = 0;
  std::chrono::system_clock::time_point last_seen;
  bool blocked = false; // 영구 차단
};

class ClientManager {
public:
  ClientManager() = default;

  // 클라이언트 접속 등록 (accept 직후)
  // 반환: false이면 IP 차단됨 → 연결 거부해야 함
  bool OnConnect(int fd, const std::string &ip, uint16_t port);

  // 클라이언트 요청 처리 (modbus_receive 직후)
  void OnRequest(int fd, uint8_t func_code, bool success, uint64_t response_us);

  // 클라이언트 연결 해제
  void OnDisconnect(int fd);

  // IP 차단/해제
  void BlockIp(const std::string &ip);
  void UnblockIp(const std::string &ip);
  bool IsBlocked(const std::string &ip) const;

  // 현재 접속 클라이언트 목록
  std::vector<ClientSession> GetActiveSessions() const;

  // IP별 통계
  std::vector<IpStats> GetIpStats() const;

  // 전체 누적 통계
  uint64_t TotalConnections() const { return total_connections_.load(); }
  uint64_t CurrentClients() const { return current_clients_.load(); }
  uint64_t TotalRequests() const { return total_requests_.load(); }
  uint64_t TotalFailures() const { return total_failures_.load(); }
  double OverallSuccessRate() const;

  // fd → 클라이언트 IP 조회 (패킷 로그 기록용)
  std::string GetClientIp(int fd) const;

  // JSON 직렬화 (백엔드 API 응답용)
  std::string ToJson() const;

  // Redis에 클라이언트 세션 게시 (SETEX modbus:clients:{device_id} 120 {json})
  void PublishToRedis(const std::string &host, int port, int device_id) const;

  // 오래된 IP 통계 정리 (24시간 이상 미접속)
  void Cleanup();

private:
  mutable std::mutex mutex_;

  std::map<int, ClientSession> sessions_;   // fd → session
  std::map<std::string, IpStats> ip_stats_; // ip → stats

  std::atomic<uint64_t> total_connections_{0};
  std::atomic<uint64_t> current_clients_{0};
  std::atomic<uint64_t> total_requests_{0};
  std::atomic<uint64_t> total_failures_{0};

  // IP 최대 동시 접속 수 제한
  int max_connections_per_ip_ = 3;

  static std::string
  FormatTime(const std::chrono::system_clock::time_point &tp);
};

} // namespace ModbusSlave
} // namespace PulseOne
