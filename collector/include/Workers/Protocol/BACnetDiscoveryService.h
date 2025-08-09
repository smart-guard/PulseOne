/**
 * @file BACnetDiscoveryService.h 
 * @brief BACnet 발견 서비스 - 🔥 동적 Worker 생성 기능 완전 추가
 * @author PulseOne Development Team
 * @date 2025-08-09
 * @version 6.0.0 - 동적 확장
 * 
 * ✅ 새로운 핵심 기능:
 * 1. 새 디바이스 발견 시 자동으로 Worker 생성
 * 2. WorkerFactory와 연동하여 프로토콜별 Worker 동적 생성
 * 3. 1:1 구조 (1 Device = 1 Worker) 완전 지원
 * 4. Worker 생명주기 관리 (생성/시작/중지/삭제)
 */

#ifndef BACNET_DISCOVERY_SERVICE_H
#define BACNET_DISCOVERY_SERVICE_H

#include "Database/Repositories/DeviceRepository.h" 
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"  // 🔥 추가
#include "Database/DatabaseTypes.h"    
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Drivers/Bacnet/BACnetTypes.h"

// 🔥 WorkerFactory 추가
#include "Workers/WorkerFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"

#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>
#include <map>
#include <vector>

// 🔥 전방 선언
namespace PulseOne {
namespace Workers {
    class BACnetWorker;
}
}

namespace PulseOne {
namespace Workers {

class BACnetDiscoveryService {
public:
    // 🔥 Repository 의존성 주입
    void SetDeviceSettingsRepository(std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo);
    
    // =======================================================================
    // 🔥 DeviceInfo ↔ DeviceEntity 변환 함수들
    // =======================================================================
    
    void ConvertDeviceInfoToEntity(const DeviceInfo& device_info, Database::Entities::DeviceEntity& entity);
    Database::Entities::DeviceEntity ConvertDeviceInfoToEntity(const DeviceInfo& device_info);
    
    // =======================================================================
    // 통계 구조체 - 동적 Worker 생성 통계 추가
    // =======================================================================
    
    struct Statistics {
        size_t devices_processed = 0;
        size_t devices_saved = 0;
        size_t objects_processed = 0;
        size_t objects_saved = 0;
        size_t values_processed = 0;
        size_t values_saved = 0;
        size_t database_errors = 0;
        
        // 🔥 새로운 통계: 동적 Worker 관리
        size_t workers_created = 0;
        size_t workers_started = 0;
        size_t workers_stopped = 0;
        size_t workers_failed = 0;
        
        std::chrono::system_clock::time_point last_activity;
    };

    // =======================================================================
    // 🔥 Worker 관리 정보 구조체
    // =======================================================================
    
    struct ManagedWorker {
        std::shared_ptr<BaseDeviceWorker> worker;
        PulseOne::Structs::DeviceInfo device_info;
        std::chrono::system_clock::time_point created_time;
        std::chrono::system_clock::time_point last_activity;
        bool is_running = false;
        bool is_failed = false;
        std::string last_error;
        
        ManagedWorker(std::shared_ptr<BaseDeviceWorker> w, 
                     const PulseOne::Structs::DeviceInfo& info)
            : worker(w), device_info(info), 
              created_time(std::chrono::system_clock::now()),
              last_activity(created_time) {}
    };

    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    explicit BACnetDiscoveryService(
        std::shared_ptr<Database::Repositories::DeviceRepository> device_repo,
        std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo,
        std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo = nullptr,
        std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo = nullptr,  // 🔥 추가
        std::shared_ptr<WorkerFactory> worker_factory = nullptr
    );
    
    ~BACnetDiscoveryService();

    // =======================================================================
    // 🔥 기존 기능 + 새로운 Worker 관리 기능
    // =======================================================================
    
    // 기존 Worker 등록 (레거시 호환)
    bool RegisterToWorker(std::shared_ptr<BACnetWorker> worker);
    void UnregisterFromWorker();
    
    // 🔥 새로운 동적 Worker 관리 기능
    bool StartDynamicDiscovery();
    void StopDynamicDiscovery();
    bool IsDiscoveryActive() const;
    
    // 🔥 발견된 디바이스별 Worker 관리
    std::shared_ptr<BaseDeviceWorker> CreateWorkerForDevice(const PulseOne::Structs::DeviceInfo& device_info);
    bool StartWorkerForDevice(const std::string& device_id);
    bool StopWorkerForDevice(const std::string& device_id);
    bool RemoveWorkerForDevice(const std::string& device_id);
    
    // 🔥 Worker 상태 조회
    std::vector<std::string> GetManagedWorkerIds() const;
    std::shared_ptr<BaseDeviceWorker> GetWorkerForDevice(const std::string& device_id) const;
    ManagedWorker* GetManagedWorkerInfo(const std::string& device_id) const;
    std::map<std::string, bool> GetAllWorkerStates() const;
    
    // 기존 기능
    Statistics GetStatistics() const;
    bool IsActive() const;
    void ResetStatistics();

    // =======================================================================
    // 🔥 콜백 핸들러들 - 동적 Worker 생성 로직 추가
    // =======================================================================
    
    void OnDeviceDiscovered(const PulseOne::Structs::DeviceInfo& device);
    void OnObjectDiscovered(const PulseOne::Structs::DataPoint& object);
    void OnObjectDiscovered(uint32_t device_id, const std::vector<PulseOne::Structs::DataPoint>& objects);
    void OnValueChanged(const std::string& object_id, const PulseOne::Structs::TimestampedValue& value);

    // =======================================================================
    // 🔥 네트워크 스캔 기능 (선택사항)
    // =======================================================================
    
    bool StartNetworkScan(const std::string& network_range = "");
    void StopNetworkScan();
    bool IsNetworkScanActive() const;

private:
    // =======================================================================
    // 🔥 동적 Worker 생성 및 관리 메서드들
    // =======================================================================
    
    bool CreateAndStartWorkerForNewDevice(const PulseOne::Structs::DeviceInfo& device_info);
    bool IsDeviceAlreadyManaged(const std::string& device_id) const;
    void CleanupFailedWorkers();
    void UpdateWorkerActivity(const std::string& device_id);
    
    // =======================================================================
    // 기존 데이터베이스 저장 메서드들
    // =======================================================================
    
    bool SaveDiscoveredDeviceToDatabase(const PulseOne::Structs::DeviceInfo& device);
    bool SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<PulseOne::Structs::DataPoint>& objects);
    bool UpdateCurrentValueInDatabase(const std::string& object_id, const PulseOne::Structs::TimestampedValue& value);

    // =======================================================================
    // 유틸리티 함수들
    // =======================================================================
    
    int FindDeviceIdInDatabase(uint32_t bacnet_device_id);
    std::string GenerateDataPointId(uint32_t device_id, const PulseOne::Structs::DataPoint& object);
    std::string ObjectTypeToString(int type);
    PulseOne::Enums::DataType DetermineDataType(int type);
    std::string DataTypeToString(PulseOne::Enums::DataType type);
    std::string ConvertDataValueToString(const PulseOne::Structs::DataValue& value);
    double ConvertDataValueToDouble(const PulseOne::Structs::DataValue& value);
    void HandleError(const std::string& context, const std::string& error);
    int GuessObjectTypeFromName(const std::string& object_name);
    std::string BACnetAddressToString(const BACNET_ADDRESS& address);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // Repository들
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repository_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repository_;
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repository_;
    std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repository_;  // 🔥 추가
    
    // 🔥 WorkerFactory 추가
    std::shared_ptr<WorkerFactory> worker_factory_;
    
    // 기존 워커 연결 (레거시 호환)
    std::weak_ptr<BACnetWorker> registered_worker_;
    
    // 🔥 동적 Worker 관리
    mutable std::mutex managed_workers_mutex_;
    std::map<std::string, std::unique_ptr<ManagedWorker>> managed_workers_;  // device_id -> ManagedWorker
    
    // 서비스 상태
    std::atomic<bool> is_active_;
    std::atomic<bool> is_discovery_active_;
    std::atomic<bool> is_network_scan_active_;
    
    // 통계 및 동기화
    mutable Statistics statistics_;
    mutable std::mutex stats_mutex_;
    
    // 🔥 네트워크 스캔 스레드 (선택사항)
    std::unique_ptr<std::thread> network_scan_thread_;
    std::atomic<bool> network_scan_running_;
};

} // namespace Workers
} // namespace PulseOne

#endif // BACNET_DISCOVERY_SERVICE_H