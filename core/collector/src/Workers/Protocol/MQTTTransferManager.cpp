#include "Workers/Protocol/MQTTTransferManager.h"
#include "Logging/LogManager.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace PulseOne {
namespace Workers {
namespace Protocol {

MQTTTransferManager &MQTTTransferManager::GetInstance() {
  static MQTTTransferManager instance;
  return instance;
}

std::optional<std::vector<uint8_t>>
MQTTTransferManager::AddChunk(const std::string &transfer_id, int total_chunks,
                              int chunk_index,
                              const std::vector<uint8_t> &payload) {

  std::lock_guard<std::mutex> lock(mutex_);

  auto &transfer = active_transfers_[transfer_id];

  // 초기화
  if (transfer.transfer_id.empty()) {
    transfer.transfer_id = transfer_id;
    transfer.total_chunks = total_chunks;
    LogManager::getInstance().Info(
        "New MQTT transfer session started: " + transfer_id +
        " (Total chunks: " + std::to_string(total_chunks) + ")");
  }

  transfer.chunks[chunk_index] = payload;
  transfer.last_activity = std::chrono::steady_clock::now();

  if (transfer.IsComplete()) {
    LogManager::getInstance().Info("MQTT transfer session complete: " +
                                   transfer_id);

    // 데이터 하나로 합치기
    std::vector<uint8_t> complete_data;
    size_t total_size = 0;
    for (const auto &[idx, chunk] : transfer.chunks) {
      total_size += chunk.size();
    }

    complete_data.reserve(total_size);
    for (const auto &[idx, chunk] : transfer.chunks) {
      complete_data.insert(complete_data.end(), chunk.begin(), chunk.end());
    }

    // 세션 삭제
    active_transfers_.erase(transfer_id);

    return complete_data;
  }

  return std::nullopt;
}

void MQTTTransferManager::Cleanup(uint32_t timeout_seconds) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto now = std::chrono::steady_clock::now();
  uint32_t count = 0;

  for (auto it = active_transfers_.begin(); it != active_transfers_.end();) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - it->second.last_activity)
                       .count();

    if (elapsed > timeout_seconds) {
      LogManager::getInstance().Warn(
          "Cleaning up stale MQTT transfer session: " + it->first);
      it = active_transfers_.erase(it);
      count++;
    } else {
      ++it;
    }
  }

  if (count > 0) {
    LogManager::getInstance().Info("Cleaned up " + std::to_string(count) +
                                   " stale MQTT transfers");
  }
}

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne
