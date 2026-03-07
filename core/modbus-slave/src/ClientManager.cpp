// =============================================================================
// core/modbus-slave/src/ClientManager.cpp
// =============================================================================
#include "ClientManager.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

namespace PulseOne {
namespace ModbusSlave {

// --- ClientSession 유틸
// -------------------------------------------------------

int64_t ClientSession::DurationSeconds() const {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now - connected_at)
      .count();
}

double ClientSession::SuccessRate() const {
  if (total_requests == 0)
    return 1.0;
  return static_cast<double>(total_requests - failed_requests) /
         static_cast<double>(total_requests);
}

// --- ClientManager
// ------------------------------------------------------------

bool ClientManager::OnConnect(int fd, const std::string &ip, uint16_t port) {
  std::lock_guard<std::mutex> lock(mutex_);

  // IP 차단 확인
  auto it = ip_stats_.find(ip);
  if (it != ip_stats_.end() && it->second.blocked) {
    std::cout << "[ClientManager] 차단된 IP 접속 거부: " << ip << "\n";
    return false;
  }

  // 동일 IP 최대 동시 접속 수 확인
  int count_from_ip = 0;
  for (const auto &[sfd, sess] : sessions_)
    if (sess.ip == ip)
      count_from_ip++;

  if (count_from_ip >= max_connections_per_ip_) {
    std::cout << "[ClientManager] IP 동시 접속 초과 거부: " << ip << " (현재 "
              << count_from_ip << "개)\n";
    return false;
  }

  // 세션 등록
  ClientSession sess;
  sess.fd = fd;
  sess.ip = ip;
  sess.port = port;
  sess.connected_at = std::chrono::system_clock::now();
  sess.last_request_at = sess.connected_at;
  sessions_[fd] = sess;

  // IP 통계 갱신
  auto &stats = ip_stats_[ip];
  stats.ip = ip;
  stats.total_sessions++;
  stats.last_seen = sess.connected_at;

  total_connections_++;
  current_clients_++;

  std::cout << "[ClientManager] 클라이언트 접속: " << ip << ":" << port
            << " fd=" << fd << " (현재 접속=" << current_clients_.load()
            << ")\n";
  return true;
}

void ClientManager::OnRequest(int fd, uint8_t func_code, bool success,
                              uint64_t response_us) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = sessions_.find(fd);
  if (it == sessions_.end())
    return;

  auto &sess = it->second;
  sess.last_request_at = std::chrono::system_clock::now();
  sess.total_requests++;
  if (!success)
    sess.failed_requests++;

  // 응답 시간 지수이동평균
  double alpha = 0.1;
  sess.avg_response_us =
      alpha * response_us + (1.0 - alpha) * sess.avg_response_us;
  if (response_us > sess.max_response_us)
    sess.max_response_us = response_us;

  // FC별 카운터
  switch (func_code) {
  case 0x01:
    sess.fc01_count++;
    break;
  case 0x02:
    sess.fc02_count++;
    break;
  case 0x03:
    sess.fc03_count++;
    break;
  case 0x04:
    sess.fc04_count++;
    break;
  case 0x05:
    sess.fc05_count++;
    break;
  case 0x06:
    sess.fc06_count++;
    break;
  case 0x0F:
    sess.fc15_count++;
    break;
  case 0x10:
    sess.fc16_count++;
    break;
  default:
    break;
  }

  // IP 전체 통계
  auto &ip = ip_stats_[sess.ip];
  ip.total_requests++;
  if (!success)
    ip.total_failures++;

  total_requests_++;
  if (!success)
    total_failures_++;
}

void ClientManager::OnDisconnect(int fd) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = sessions_.find(fd);
  if (it == sessions_.end())
    return;

  std::cout << "[ClientManager] 클라이언트 해제: " << it->second.ip
            << " fd=" << fd << " 지속=" << it->second.DurationSeconds() << "초"
            << " 요청=" << it->second.total_requests << "\n";

  sessions_.erase(it);
  current_clients_--;
}

void ClientManager::BlockIp(const std::string &ip) {
  std::lock_guard<std::mutex> lock(mutex_);
  ip_stats_[ip].blocked = true;
  ip_stats_[ip].ip = ip;
  std::cout << "[ClientManager] IP 차단: " << ip << "\n";
}

void ClientManager::UnblockIp(const std::string &ip) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = ip_stats_.find(ip);
  if (it != ip_stats_.end())
    it->second.blocked = false;
}

bool ClientManager::IsBlocked(const std::string &ip) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = ip_stats_.find(ip);
  return it != ip_stats_.end() && it->second.blocked;
}

std::vector<ClientSession> ClientManager::GetActiveSessions() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<ClientSession> result;
  result.reserve(sessions_.size());
  for (const auto &[fd, sess] : sessions_)
    result.push_back(sess);
  return result;
}

std::vector<IpStats> ClientManager::GetIpStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<IpStats> result;
  result.reserve(ip_stats_.size());
  for (const auto &[ip, stats] : ip_stats_)
    result.push_back(stats);
  return result;
}

double ClientManager::OverallSuccessRate() const {
  uint64_t total = total_requests_.load();
  if (total == 0)
    return 1.0;
  return static_cast<double>(total - total_failures_.load()) /
         static_cast<double>(total);
}

std::string
ClientManager::FormatTime(const std::chrono::system_clock::time_point &tp) {
  auto t = std::chrono::system_clock::to_time_t(tp);
  auto tm = *std::localtime(&t);
  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
  return ss.str();
}

std::string ClientManager::ToJson() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ostringstream j;
  j << "{"
    << "\"total_connections\":" << total_connections_.load() << ","
    << "\"current_clients\":" << current_clients_.load() << ","
    << "\"total_requests\":" << total_requests_.load() << ","
    << "\"total_failures\":" << total_failures_.load() << ","
    << "\"success_rate\":" << std::fixed << std::setprecision(4)
    << OverallSuccessRate() << ","
    << "\"sessions\":[";

  bool first = true;
  for (const auto &[fd, sess] : sessions_) {
    if (!first)
      j << ",";
    j << "{"
      << "\"fd\":" << fd << ","
      << "\"ip\":\"" << sess.ip << "\","
      << "\"port\":" << sess.port << ","
      << "\"connected_at\":\"" << FormatTime(sess.connected_at) << "\","
      << "\"duration_sec\":" << sess.DurationSeconds() << ","
      << "\"requests\":" << sess.total_requests << ","
      << "\"failures\":" << sess.failed_requests << ","
      << "\"success_rate\":" << std::setprecision(4) << sess.SuccessRate()
      << ","
      << "\"avg_response_us\":" << sess.avg_response_us << ","
      << "\"fc01_count\":" << sess.fc01_count << ","
      << "\"fc02_count\":" << sess.fc02_count << ","
      << "\"fc03_count\":" << sess.fc03_count << ","
      << "\"fc04_count\":" << sess.fc04_count << ","
      << "\"fc05_count\":" << sess.fc05_count << ","
      << "\"fc06_count\":" << sess.fc06_count << ","
      << "\"fc15_count\":" << sess.fc15_count << ","
      << "\"fc16_count\":" << sess.fc16_count << "}";
    first = false;
  }
  j << "]}";
  return j.str();
}

void ClientManager::Cleanup() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);
  for (auto it = ip_stats_.begin(); it != ip_stats_.end();) {
    if (it->second.last_seen < cutoff && !it->second.blocked)
      it = ip_stats_.erase(it);
    else
      ++it;
  }
}

void ClientManager::PublishToRedis(const std::string &host, int port,
                                   int device_id) const {
#ifdef HAVE_REDIS
  redisContext *rc = redisConnect(host.c_str(), port);
  if (!rc || rc->err) {
    if (rc)
      redisFree(rc);
    return;
  }
  std::string key = "modbus:clients:" + std::to_string(device_id);
  std::string json = ToJson();
  redisReply *reply = static_cast<redisReply *>(
      redisCommand(rc, "SETEX %s 120 %s", key.c_str(), json.c_str()));
  if (reply)
    freeReplyObject(reply);
  redisFree(rc);
#else
  (void)host;
  (void)port;
  (void)device_id;
#endif
}

std::string ClientManager::GetClientIp(int fd) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sessions_.find(fd);
  if (it != sessions_.end())
    return it->second.ip;
  return "unknown";
}

} // namespace ModbusSlave
} // namespace PulseOne
