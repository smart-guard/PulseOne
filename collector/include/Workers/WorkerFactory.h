// ============================================================================
// 1. WorkerFactory.h 수정 - UnifiedCommonTypes.h 제거하고 전방선언만
// ============================================================================
#ifndef WORKER_FACTORY_H
#define WORKER_FACTORY_H

#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <future>

namespace PulseOne {

// ✅ 전방 선언만 사용 - 순환 참조 방지
class LogManager;
class ConfigManager;
class RedisClient;
class InfluxClient;

// ✅ Structs 네임스페이스도 전방 선언
namespace Structs {
    struct DeviceInfo;
    struct DataPoint;
}

namespace Database {
namespace Entities {
    class DeviceEntity;
    class DataPointEntity;
}
namespace Repositories {
    class DeviceRepository;
    class DataPointRepository;
}
}

namespace Workers {

class BaseDeviceWorker;

using WorkerCreator = std::function<std::unique_ptr<BaseDeviceWorker>(
    const PulseOne::Structs::DeviceInfo& device_info,
    std::shared_ptr<::RedisClient> redis_client,    // ✅ 이제 전역 클래스
    std::shared_ptr<::InfluxClient> influx_client   // ✅ 이제 전역 클래스
)>;

struct FactoryStats {
    uint64_t workers_created = 0;
    uint64_t creation_failures = 0;
    uint32_t registered_protocols = 0;
    std::chrono::system_clock::time_point factory_start_time;
    std::chrono::milliseconds total_creation_time{0};
    
    std::string ToString() const;
    void Reset();
};

class WorkerFactory {
public:
    // ✅ 기존 코드와 동일한 메서드명 사용
    static WorkerFactory& getInstance();
    
    WorkerFactory(const WorkerFactory&) = delete;
    WorkerFactory& operator=(const WorkerFactory&) = delete;

    // ✅ 기존 코드에 맞춰서 Initialize() 오버로드 추가
    bool Initialize();  // 매개변수 없는 버전 (기존 코드용)
    bool Initialize(LogManager* logger, ConfigManager* config_manager);  // 새 버전
    
    void SetDeviceRepository(std::shared_ptr<Database::Repositories::DeviceRepository> device_repo);
    void SetDataPointRepository(std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo);
    void SetDatabaseClients(
        std::shared_ptr<::RedisClient> redis_client,     // ✅ 전역 클래스
        std::shared_ptr<::InfluxClient> influx_client    // ✅ 전역 클래스
    );
    std::unique_ptr<BaseDeviceWorker> CreateWorker(const Database::Entities::DeviceEntity& device_entity);
    std::unique_ptr<BaseDeviceWorker> CreateWorkerById(int device_id);
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateAllActiveWorkers();
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateAllActiveWorkers(int tenant_id);
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateWorkersByProtocol(const std::string& protocol_type, int tenant_id = 0);

    std::vector<std::string> GetSupportedProtocols() const;
    bool IsProtocolSupported(const std::string& protocol_type) const;
    FactoryStats GetFactoryStats() const;
    std::string GetFactoryStatsString() const;
    void RegisterWorkerCreator(const std::string& protocol_type, WorkerCreator creator);

private:
    WorkerFactory() = default;
    ~WorkerFactory() = default;

    void RegisterWorkerCreators();
    std::string ValidateWorkerConfig(const Database::Entities::DeviceEntity& device_entity) const;
    
    // ✅ 전방 선언된 타입 사용
    PulseOne::Structs::DeviceInfo ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const;
    std::vector<PulseOne::Structs::DataPoint> LoadDataPointsForDevice(int device_id) const;

    std::atomic<bool> initialized_{false};
    mutable std::mutex factory_mutex_;
    
    LogManager* logger_ = nullptr;
    ConfigManager* config_manager_ = nullptr;
    
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo_;
    
    std::shared_ptr<::RedisClient> redis_client_;       // ✅ 전역 클래스
    std::shared_ptr<::InfluxClient> influx_client_;     // ✅ 전역 클래스
    
    std::map<std::string, WorkerCreator> worker_creators_;
    
    mutable std::atomic<uint64_t> workers_created_{0};
    mutable std::atomic<uint64_t> creation_failures_{0};
    std::chrono::system_clock::time_point factory_start_time_;
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_FACTORY_H