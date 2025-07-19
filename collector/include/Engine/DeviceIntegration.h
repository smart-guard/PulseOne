// collector/include/Engine/DeviceIntegration.h
// PulseOne DeviceWorker와 Database 연동 레이어

#ifndef DEVICE_INTEGRATION_H
#define DEVICE_INTEGRATION_H

#include <memory>
#include <vector>
#include <map>
#include <atomic>
#include <chrono>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "Database/DeviceDataAccess.h"
#include "Database/DataAccessManager.h"
#include "Drivers/CommonTypes.h"
#include "Utils/LogManager.h"
#include "Config/ConfigManager.h"

namespace PulseOne {
namespace Engine {

// Drivers 네임스페이스의 타입들을 Engine에서 사용
using DataValue = Drivers::DataValue;
using DeviceConfig = Drivers::DriverConfig;
using ProtocolType = Drivers::ProtocolType;

// =============================================================================
// 데이터 동기화 이벤트 타입
// =============================================================================
enum class SyncEventType {
    DEVICE_STATUS_CHANGED,    // 디바이스 상태 변경
    DATA_VALUE_UPDATED,       // 데이터 값 업데이트
    DEVICE_CONFIGURATION_CHANGED, // 디바이스 설정 변경
    BULK_DATA_UPDATE,         // 대량 데이터 업데이트
    CONNECTION_STATE_CHANGED  // 연결 상태 변경
};

// =============================================================================
// 동기화 이벤트 구조체
// =============================================================================
struct SyncEvent {
    SyncEventType type;
    int device_id;
    int point_id;
    std::string timestamp;
    nlohmann::json data;  // 이벤트별 추가 데이터
    int priority;         // 우선순위 (0=최고, 10=최저)
    
    SyncEvent(SyncEventType t, int dev_id = -1, int pt_id = -1, int prio = 5)
        : type(t), device_id(dev_id), point_id(pt_id), priority(prio) {
        timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
};

// =============================================================================
// 배치 처리용 데이터 버퍼
// =============================================================================
struct DataUpdateBatch {
    std::map<int, Database::CurrentValue> current_values; // point_id -> value
    std::map<int, Database::DeviceStatus> device_statuses; // device_id -> status
    std::chrono::steady_clock::time_point created_at;
    
    DataUpdateBatch() : created_at(std::chrono::steady_clock::now()) {}
    
    size_t size() const {
        return current_values.size() + device_statuses.size();
    }
    
    bool empty() const {
        return current_values.empty() && device_statuses.empty();
    }
    
    void clear() {
        current_values.clear();
        device_statuses.clear();
    }
};

// =============================================================================
// DeviceWorker와 Database 연동 클래스
// =============================================================================
class DeviceIntegration {
public:
    // 생성자
    DeviceIntegration(std::shared_ptr<LogManager> logger,
                     std::shared_ptr<ConfigManager> config);
    
    // 소멸자
    ~DeviceIntegration();
    
    /**
     * @brief 초기화
     * @return 성공 시 true
     */
    bool Initialize();
    
    /**
     * @brief 종료 및 정리
     */
    void Shutdown();
    
    // ==========================================================================
    // DeviceWorker에서 호출하는 메소드들
    // ==========================================================================
    
    /**
     * @brief 디바이스 현재값 업데이트 (비동기)
     * @param device_id 디바이스 ID
     * @param point_id 포인트 ID
     * @param value 업데이트할 값
     * @param quality 데이터 품질
     * @param priority 우선순위 (기본값: 5)
     */
    void UpdateDataValue(int device_id, int point_id, 
                        const Drivers::DataValue& value, 
                        const std::string& quality = "good",
                        int priority = 5);
    
    /**
     * @brief 디바이스 상태 업데이트 (비동기)
     * @param device_id 디바이스 ID
     * @param connection_status 연결 상태
     * @param response_time 응답 시간 (ms)
     * @param error_message 에러 메시지 (선택적)
     */
    void UpdateDeviceStatus(int device_id, 
                           const std::string& connection_status,
                           int response_time,
                           const std::string& error_message = "");
    
    /**
     * @brief 대량 데이터 업데이트 (배치 처리)
     * @param device_id 디바이스 ID
     * @param data_updates 데이터 업데이트 맵 (point_id -> value)
     */
    void UpdateDataValuesBatch(int device_id, 
                              const std::map<int, Drivers::DataValue>& data_updates);
    
    /**
     * @brief 디바이스 설정 변경 알림
     * @param device_id 디바이스 ID
     * @param config_changes 변경된 설정 (JSON)
     */
    void NotifyConfigurationChange(int device_id, const nlohmann::json& config_changes);
    
    // ==========================================================================
    // 디바이스 설정 조회 (DeviceWorker용)
    // ==========================================================================
    
    /**
     * @brief 디바이스 정보 조회 (캐시됨)
     * @param device_id 디바이스 ID
     * @param device_info 출력 디바이스 정보
     * @return 성공 시 true
     */
    bool GetDeviceInfo(int device_id, Database::DeviceInfo& device_info);
    
    /**
     * @brief 활성화된 데이터 포인트 목록 조회
     * @param device_id 디바이스 ID
     * @return 활성화된 데이터 포인트 목록
     */
    std::vector<Database::DataPointInfo> GetEnabledDataPoints(int device_id);
    
    /**
     * @brief 디바이스 설정을 DriverConfig로 변환
     * @param device_id 디바이스 ID
     * @param config 출력 설정
     * @return 성공 시 true
     */
    bool GetDeviceConfig(int device_id, Drivers::DriverConfig& config);
    
    // ==========================================================================
    // 성능 및 모니터링
    // ==========================================================================
    
    /**
     * @brief 처리 통계 정보 반환
     * @return JSON 형태의 통계 정보
     */
    nlohmann::json GetProcessingStats() const;
    
    /**
     * @brief 대기열 상태 정보 반환
     * @return JSON 형태의 대기열 정보
     */
    nlohmann::json GetQueueStatus() const;
    
    /**
     * @brief 캐시 통계 정보 반환
     * @return JSON 형태의 캐시 통계
     */
    nlohmann::json GetCacheStats() const;
    
    /**
     * @brief 설정 새로고침 (캐시 무효화)
     */
    void RefreshConfiguration();
    
    // ==========================================================================
    // 설정 가능한 파라미터들
    // ==========================================================================
    
    /**
     * @brief 배치 처리 설정 변경
     * @param max_batch_size 최대 배치 크기
     * @param batch_timeout_ms 배치 타임아웃 (ms)
     */
    void SetBatchSettings(size_t max_batch_size, int batch_timeout_ms);
    
    /**
     * @brief 캐시 설정 변경
     * @param cache_ttl_seconds 캐시 TTL (초)
     * @param max_cache_size 최대 캐시 크기
     */
    void SetCacheSettings(int cache_ttl_seconds, size_t max_cache_size);

private:
    // ==========================================================================
    // 내부 구현
    // ==========================================================================
    
    // 의존성
    std::shared_ptr<LogManager> logger_;
    std::shared_ptr<ConfigManager> config_;
    std::shared_ptr<Database::DeviceDataAccess> device_data_access_;
    
    // 워커 스레드 및 동기화
    std::thread sync_worker_thread_;
    std::thread batch_processor_thread_;
    std::atomic<bool> running_;
    
    // 이벤트 큐 (우선순위 큐)
    mutable std::mutex event_queue_mutex_;
    std::condition_variable event_queue_cv_;
    std::priority_queue<SyncEvent, std::vector<SyncEvent>, 
                       std::function<bool(const SyncEvent&, const SyncEvent&)>> event_queue_;
    
    // 배치 처리 버퍼
    mutable std::mutex batch_mutex_;
    std::condition_variable batch_cv_;
    DataUpdateBatch current_batch_;
    
    // 설정 캐시
    mutable std::mutex cache_mutex_;
    std::map<int, Database::DeviceInfo> device_info_cache_;
    std::map<int, std::vector<Database::DataPointInfo>> datapoints_cache_;
    std::map<int, std::chrono::steady_clock::time_point> cache_timestamps_;
    
    // 설정 가능한 파라미터들
    size_t max_batch_size_ = 100;
    std::chrono::milliseconds batch_timeout_{1000};
    std::chrono::seconds cache_ttl_{300};  // 5분
    size_t max_cache_size_ = 1000;
    
    // 통계
    struct ProcessingStats {
        std::atomic<uint64_t> events_processed{0};
        std::atomic<uint64_t> batch_operations{0};
        std::atomic<uint64_t> cache_hits{0};
        std::atomic<uint64_t> cache_misses{0};
        std::atomic<uint64_t> database_errors{0};
        
        void reset() {
            events_processed = 0;
            batch_operations = 0;
            cache_hits = 0;
            cache_misses = 0;
            database_errors = 0;
        }
    } stats_;
    
    // ==========================================================================
    // 내부 워커 스레드 함수들
    // ==========================================================================
    
    /**
     * @brief 동기화 워커 스레드 (이벤트 처리)
     */
    void SyncWorkerThread();
    
    /**
     * @brief 배치 프로세서 스레드 (배치 처리)
     */
    void BatchProcessorThread();
    
    /**
     * @brief 단일 동기화 이벤트 처리
     * @param event 처리할 이벤트
     */
    void ProcessSyncEvent(const SyncEvent& event);
    
    /**
     * @brief 배치 데이터 플러시
     */
    void FlushBatch();
    
    // ==========================================================================
    // 캐시 관리
    // ==========================================================================
    
    /**
     * @brief 캐시에서 디바이스 정보 조회
     * @param device_id 디바이스 ID
     * @param device_info 출력 디바이스 정보
     * @return 캐시 히트 시 true
     */
    bool GetFromCache(int device_id, Database::DeviceInfo& device_info);
    
    /**
     * @brief 캐시에 디바이스 정보 저장
     * @param device_id 디바이스 ID
     * @param device_info 디바이스 정보
     */
    void PutToCache(int device_id, const Database::DeviceInfo& device_info);
    
    /**
     * @brief 만료된 캐시 엔트리 정리
     */
    void CleanupExpiredCache();
    
    // ==========================================================================
    // 설정 및 초기화
    // ==========================================================================
    
    /**
     * @brief 설정 로드
     */
    void LoadConfiguration();
    
    /**
     * @brief 캐시 무효화
     * @param device_id 디바이스 ID
     */
    void InvalidateDeviceCache(int device_id);
    
    // ==========================================================================
    // 배치 처리
    // ==========================================================================
    
    /**
     * @brief 배치에 데이터 추가
     * @param device_id 디바이스 ID
     * @param data_updates 데이터 업데이트 맵
     */
    void AddToBatch(int device_id, const std::map<int, Drivers::DataValue>& data_updates);
    
    /**
     * @brief 배치에 상태 추가
     * @param device_id 디바이스 ID
     * @param connection_status 연결 상태
     * @param response_time 응답 시간
     * @param error_message 에러 메시지
     */
    void AddToStatusBatch(int device_id, const std::string& connection_status,
                         int response_time, const std::string& error_message);
    
    // ==========================================================================
    // 이벤트 처리
    // ==========================================================================
    
    /**
     * @brief 디바이스 상태 이벤트 처리
     * @param event 이벤트
     */
    void ProcessDeviceStatusEvent(const SyncEvent& event);
    
    /**
     * @brief 데이터 값 이벤트 처리
     * @param event 이벤트
     */
    void ProcessDataValueEvent(const SyncEvent& event);
    
    /**
     * @brief 설정 변경 이벤트 처리
     * @param event 이벤트
     */
    void ProcessConfigurationChangeEvent(const SyncEvent& event);
    
    /**
     * @brief 대량 데이터 이벤트 처리
     * @param event 이벤트
     */
    void ProcessBulkDataEvent(const SyncEvent& event);
    
    /**
     * @brief 연결 상태 이벤트 처리
     * @param event 이벤트
     */
    void ProcessConnectionStateEvent(const SyncEvent& event);
    
    // ==========================================================================
    // 유틸리티 메소드들
    // ==========================================================================
    
    /**
     * @brief DataValue를 Database::CurrentValue로 변환
     */
    Database::CurrentValue ConvertToCurrentValue(int point_id, 
                                                const Drivers::DataValue& value,
                                                const std::string& quality);
    
    /**
     * @brief JSON을 Database::CurrentValue로 변환
     */
    Database::CurrentValue ConvertToCurrentValue(int point_id,
                                                const nlohmann::json& value_json,
                                                const std::string& quality);
    
    /**
     * @brief DataValue를 JSON으로 변환
     */
    nlohmann::json ConvertDataValueToJson(const Drivers::DataValue& value);
    
    /**
     * @brief 문자열을 ProtocolType으로 변환
     */
    Drivers::ProtocolType StringToProtocolType(const std::string& protocol_str);
    
    /**
     * @brief 현재 타임스탬프 문자열 생성
     */
    std::string GetCurrentTimestamp() const;
    
    /**
     * @brief 우선순위 큐 비교 함수
     */
    static bool EventComparator(const SyncEvent& a, const SyncEvent& b) {
        return a.priority > b.priority;  // 낮은 숫자가 높은 우선순위
    }
};

} // namespace Engine
} // namespace PulseOne

#endif // DEVICE_INTEGRATION_H