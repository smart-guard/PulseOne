#ifndef WORKER_FACTORY_H
#define WORKER_FACTORY_H

/**
 * @file WorkerFactory.h
 * @brief PulseOne WorkerFactory - 전방 선언 수정 버전
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

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

// =============================================================================
// 🔥 전방 선언 (Forward Declarations) - Common/Structs.h 참조
// =============================================================================

// Utils 네임스페이스
class LogManager;
class ConfigManager;

// Database 클라이언트들
class RedisClient;
class InfluxClient;

// 🔧 Common Types - Common/Structs.h에 정의된 타입들은 전방 선언만
namespace Structs {
    struct DeviceInfo;   // Common/Structs.h에 정의됨
    struct DataPoint;    // Common/Structs.h에 정의됨
}

// 🔧 또는 UnifiedCommonTypes.h의 using 선언 사용
// (이미 다른 곳에서 include되어 있으므로 중복 정의 방지)

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

// Worker 클래스들 전방 선언
class BaseDeviceWorker;
class ModbusTcpWorker;
class MQTTWorker;
class BACnetWorker;

/**
 * @brief Worker 생성 함수 타입 정의
 * 🔧 Common/Structs.h의 타입 사용
 */
using WorkerCreator = std::function<std::unique_ptr<BaseDeviceWorker>(
    const PulseOne::Structs::DeviceInfo& device_info,
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client
)>;

/**
 * @brief WorkerFactory 통계 정보
 */
struct FactoryStats {
    uint64_t workers_created = 0;
    uint64_t creation_failures = 0;
    uint32_t registered_protocols = 0;
    std::chrono::system_clock::time_point factory_start_time;
    std::chrono::milliseconds total_creation_time{0};
    
    std::string ToString() const;
};

/**
 * @brief PulseOne Worker Factory (싱글톤 패턴)
 */
class WorkerFactory {
public:
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    
    static WorkerFactory& getInstance();
    
    WorkerFactory(const WorkerFactory&) = delete;
    WorkerFactory& operator=(const WorkerFactory&) = delete;

    // =======================================================================
    // 초기화 및 설정
    // =======================================================================
    
    bool Initialize();
    
    void SetDeviceRepository(std::shared_ptr<Database::Repositories::DeviceRepository> device_repo);
    void SetDataPointRepository(std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo);
    
    void SetDatabaseClients(
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client
    );

    // =======================================================================
    // Worker 생성 메서드들
    // =======================================================================
    
    std::unique_ptr<BaseDeviceWorker> CreateWorker(const Database::Entities::DeviceEntity& device_entity);
    
    std::unique_ptr<BaseDeviceWorker> CreateWorkerById(int device_id);
    
    // 🔧 CreateAllActiveWorkers 메서드 오버로드 추가
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateAllActiveWorkers();  // tenant_id 없는 버전
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateAllActiveWorkers(int tenant_id);  // 기존 버전
    
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateWorkersByProtocol(
        const std::string& protocol_type, 
        int tenant_id = 1
    );

    // =======================================================================
    // 팩토리 정보 조회
    // =======================================================================
    
    std::vector<std::string> GetSupportedProtocols() const;
    bool IsProtocolSupported(const std::string& protocol_type) const;
    FactoryStats GetFactoryStats() const;
    std::string GetFactoryStatsString() const;

    // =======================================================================
    // 확장성 지원
    // =======================================================================
    
    void RegisterWorkerCreator(const std::string& protocol_type, WorkerCreator creator);

private:
    WorkerFactory() = default;
    ~WorkerFactory() = default;

    void RegisterWorkerCreators();
    std::string ValidateWorkerConfig(const Database::Entities::DeviceEntity& device_entity) const;
    DeviceInfo ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const;
    std::vector<DataPoint> LoadDataPointsForDevice(int device_id) const;

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    std::atomic<bool> initialized_{false};
    mutable std::mutex factory_mutex_;
    
    LogManager* logger_ = nullptr;
    ConfigManager* config_manager_ = nullptr;
    
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo_;
    
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    std::map<std::string, WorkerCreator> worker_creators_;
    
    mutable std::atomic<uint64_t> workers_created_{0};
    mutable std::atomic<uint64_t> creation_failures_{0};
    std::chrono::system_clock::time_point factory_start_time_;
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_FACTORY_H