// =============================================================================
// core/shared/include/Data/DataReader.h
// Redis 데이터 읽기 전용 클래스
// =============================================================================

#ifndef DATA_READER_H
#define DATA_READER_H

#include "Data/RedisDataTypes.h"
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <chrono>

// Forward declaration
class RedisClient;

namespace PulseOne {
namespace Shared {
namespace Data {

// =============================================================================
// DataReader 옵션
// =============================================================================

struct DataReaderOptions {
    bool enable_cache = false;
    int cache_ttl_seconds = 60;
    size_t max_cache_size = 10000;
    int batch_size = 1000;
    int retry_count = 3;
    int retry_delay_ms = 100;
    int read_timeout_ms = 1000;
};

// =============================================================================
// DataReader 클래스
// =============================================================================

/**
 * @brief Redis 데이터 읽기 전용 클래스
 * 
 * 특징:
 * - 모든 Redis 데이터 패턴 지원
 * - 배치 읽기 최적화 (MGET)
 * - 선택적 캐싱
 * - Pub/Sub 구독
 * - 멀티스레드 안전
 * 
 * 사용 예:
 * @code
 * auto redis = std::make_shared<RedisClientImpl>();
 * DataReader reader(redis);
 * 
 * auto value = reader.ReadDevicePoint(1, "temperature");
 * auto result = reader.ReadPointsBatch({1, 2, 3});
 * @endcode
 */
class DataReader {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    explicit DataReader(std::shared_ptr<RedisClient> redis,
                       const DataReaderOptions& options = DataReaderOptions());
    ~DataReader();
    
    // 복사/이동 금지
    DataReader(const DataReader&) = delete;
    DataReader& operator=(const DataReader&) = delete;
    DataReader(DataReader&&) = delete;
    DataReader& operator=(DataReader&&) = delete;
    
    // ==========================================================================
    // 데이터 포인트 읽기 (device:{num}:{name})
    // ==========================================================================
    
    /**
     * @brief 디바이스 포인트 읽기
     * @param device_num 디바이스 번호
     * @param point_name 포인트 이름
     * @return 성공 시 CurrentValue, 실패 시 nullopt
     */
    std::optional<CurrentValue> ReadDevicePoint(int device_num, 
                                                const std::string& point_name);
    
    /**
     * @brief 디바이스의 모든 포인트 읽기
     * @param device_num 디바이스 번호
     * @return 포인트 벡터
     */
    std::vector<CurrentValue> ReadAllDevicePoints(int device_num);
    
    // ==========================================================================
    // Point Latest 읽기 (point:{id}:latest)
    // ==========================================================================
    
    /**
     * @brief 포인트 최신값 읽기 (Legacy 패턴)
     * @param point_id 포인트 ID
     * @return 성공 시 CurrentValue, 실패 시 nullopt
     */
    std::optional<CurrentValue> ReadPointLatest(int point_id);
    
    /**
     * @brief 여러 포인트 배치 읽기
     * @param point_ids 포인트 ID 벡터
     * @return 배치 읽기 결과
     */
    BatchReadResult ReadPointsBatch(const std::vector<int>& point_ids);
    
    // ==========================================================================
    // 가상포인트 읽기 (virtualpoint:{id})
    // ==========================================================================
    
    /**
     * @brief 가상포인트 읽기
     * @param point_id 가상포인트 ID
     * @return 성공 시 VirtualPointValue, 실패 시 nullopt
     */
    std::optional<VirtualPointValue> ReadVirtualPoint(int point_id);
    
    /**
     * @brief 여러 가상포인트 배치 읽기
     * @param point_ids 가상포인트 ID 벡터
     * @return 가상포인트 벡터
     */
    std::vector<VirtualPointValue> ReadVirtualPointsBatch(const std::vector<int>& point_ids);
    
    // ==========================================================================
    // 알람 읽기 (alarm:active:{rule_id})
    // ==========================================================================
    
    /**
     * @brief 활성 알람 읽기
     * @param rule_id 알람 규칙 ID
     * @return 성공 시 ActiveAlarm, 실패 시 nullopt
     */
    std::optional<ActiveAlarm> ReadActiveAlarm(int rule_id);
    
    /**
     * @brief 모든 활성 알람 읽기
     * @return 활성 알람 벡터
     */
    std::vector<ActiveAlarm> ReadAllActiveAlarms();
    
    /**
     * @brief 알람 카운터 읽기 (alarms:count:today)
     * @return 오늘의 알람 카운트
     */
    int ReadAlarmCount();
    
    // ==========================================================================
    // Pub/Sub 구독
    // ==========================================================================
    
    /**
     * @brief 알람 실시간 구독
     * @param callback 알람 수신 시 호출될 콜백
     * @param channel 구독할 채널 (기본: "alarms:all")
     * @return 구독 성공 여부
     */
    bool SubscribeToAlarms(std::function<void(const ActiveAlarm&)> callback,
                          const std::string& channel = PubSubChannels::ALARMS_ALL);
    
    /**
     * @brief 모든 구독 중지
     */
    void UnsubscribeAll();
    
    /**
     * @brief 현재 구독 중인지 확인
     */
    bool IsSubscribed() const;
    
    // ==========================================================================
    // 캐시 관리
    // ==========================================================================
    
    /**
     * @brief 캐시 활성화/비활성화
     */
    void EnableCache(bool enable);
    
    /**
     * @brief 캐시 지우기
     */
    void ClearCache();
    
    /**
     * @brief 캐시 통계
     */
    struct CacheStats {
        size_t size;
        int hit_count;
        int miss_count;
        double hit_rate;
    };
    CacheStats GetCacheStats() const;
    
    // ==========================================================================
    // 유틸리티
    // ==========================================================================
    
    /**
     * @brief 키 존재 여부 확인
     */
    bool KeyExists(const std::string& key);
    
    /**
     * @brief 패턴으로 키 검색
     * @param pattern 검색 패턴 (예: "device:1:*")
     * @return 매칭되는 키 벡터
     */
    std::vector<std::string> FindKeys(const std::string& pattern);
    
    /**
     * @brief 통계 정보
     */
    struct Statistics {
        uint64_t total_reads;
        uint64_t successful_reads;
        uint64_t failed_reads;
        uint64_t cache_hits;
        uint64_t cache_misses;
        uint64_t batch_reads;
        double avg_read_time_ms;
    };
    Statistics GetStatistics() const;
    
    /**
     * @brief 통계 초기화
     */
    void ResetStatistics();
    
    /**
     * @brief 연결 상태 확인
     */
    bool IsConnected() const;

private:
    // ==========================================================================
    // 내부 메서드들
    // ==========================================================================
    
    std::optional<CurrentValue> parseCurrentValue(const std::string& json_str);
    std::optional<VirtualPointValue> parseVirtualPoint(const std::string& json_str);
    std::optional<ActiveAlarm> parseActiveAlarm(const std::string& json_str);
    
    int extractDeviceNumber(const std::string& device_id) const;
    std::string readWithRetry(const std::string& key);
    
    std::optional<std::string> getFromCache(const std::string& key);
    void putToCache(const std::string& key, const std::string& value);
    void cleanExpiredCache();
    
    void subscriptionThreadFunc(const std::string& channel,
                               std::function<void(const std::string&, const std::string&)> callback);
    
    void updateStats(bool success, double elapsed_ms, bool is_batch = false);
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    std::shared_ptr<RedisClient> redis_;
    DataReaderOptions options_;
    
    // 캐시
    struct CacheEntry {
        std::string value;
        std::chrono::steady_clock::time_point timestamp;
    };
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::atomic<int> cache_hit_count_{0};
    std::atomic<int> cache_miss_count_{0};
    
    // 구독
    std::atomic<bool> is_subscribed_{false};
    std::vector<std::unique_ptr<std::thread>> subscription_threads_;
    mutable std::mutex subscription_mutex_;
    
    // 통계
    mutable std::mutex stats_mutex_;
    Statistics stats_;
};

} // namespace Data
} // namespace Shared
} // namespace PulseOne

#endif // DATA_READER_H