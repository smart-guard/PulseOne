#ifndef PULSEONE_PROTOCOL_MQTT_TRANSFER_MANAGER_H
#define PULSEONE_PROTOCOL_MQTT_TRANSFER_MANAGER_H

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace PulseOne {
namespace Workers {
namespace Protocol {

/**
 * @brief MQTT 조각 메시지(Chunked Messages) 재조합 관리 클래스
 */
class MQTTTransferManager {
public:
  struct ActiveTransfer {
    std::string transfer_id;
    int total_chunks = 0;
    std::map<int, std::vector<uint8_t>> chunks;
    std::chrono::steady_clock::time_point last_activity;
    std::string file_name;

    bool IsComplete() const {
      return total_chunks > 0 &&
             chunks.size() == static_cast<size_t>(total_chunks);
    }
  };

  /**
   * @brief 싱글톤 인스턴스 반환
   */
  static MQTTTransferManager &GetInstance();

  /**
   * @brief 새로운 메시지 조각 추가
   * @param transfer_id 전송 시퀀스 식별자
   * @param total_chunks 전체 조각 수
   * @param chunk_index 현재 조각 번호 (1-based or 0-based)
   * @param payload 실제 데이터 조각
   * @return 모든 조각이 모였을 경우 전체 데이터 반환, 아니면 std::nullopt
   */
  std::optional<std::vector<uint8_t>>
  AddChunk(const std::string &transfer_id, int total_chunks, int chunk_index,
           const std::vector<uint8_t> &payload);

  /**
   * @brief 오래된 미완성 전송 세션 정리
   * @param timeout_seconds 타임아웃 시간
   */
  void Cleanup(uint32_t timeout_seconds = 300);

private:
  MQTTTransferManager() = default;
  ~MQTTTransferManager() = default;

  std::map<std::string, ActiveTransfer> active_transfers_;
  std::mutex mutex_;
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // PULSEONE_PROTOCOL_MQTT_TRANSFER_MANAGER_H
