/**
 * @file WorkerFactory.h
 * @brief PulseOne WorkerFactory - 컴파일 에러 완전 해결
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

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

// ✅ 새로 추가: DataQuality 타입 사용을 위해 Enums.h include
#include "Common/Enums.h"
#include "Common/BasicTypes.h"
#include "Utils/LogManager.h"
// 🔧 중요: 전역 네임스페이스에서 전방선언 (PulseOne:: 제거)
class LogManager;
class ConfigManager;
class RedisClient;
class InfluxClient;

namespace PulseOne {

// ✅ PulseOne 네임스페이스 안의 전방선언들
namespace Structs {
    struct DeviceInfo;
    struct DataPoint;
}

namespace Database {
    class RepositoryFactory;
    namespace Entities {
        class DeviceEntity;
        class DataPointEntity;
    }
    namespace Repositories {
        class DeviceRepository;
        class DataPointRepository;
        class CurrentValueRepository;
        class DeviceSettingsRepository;
    }   
}

namespace Workers {

class BaseDeviceWorker;

// ✅ WorkerCreator 타입 정의 - 전역 클래스 사용
using WorkerCreator = std::function<std::unique_ptr<BaseDeviceWorker>(
    const PulseOne::Structs::DeviceInfo& device_info)>;

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
    // 🔧 수정: 메서드명 통일 - getInstance (소문자 g)
    static WorkerFactory& getInstance();
    
    WorkerFactory(const WorkerFactory&) = delete;
    WorkerFactory& operator=(const WorkerFactory&) = delete;

    // 🔧 수정: Initialize() 메서드 정리
    bool Initialize();  // 기본 버전 - 내부에서 싱글톤들 가져오기
    bool Initialize(::LogManager* logger, ::ConfigManager* config_manager);  // 직접 주입 버전
    
    void SetRepositoryFactory(std::shared_ptr<Database::RepositoryFactory> repo_factory);
    void SetDeviceRepository(std::shared_ptr<Database::Repositories::DeviceRepository> device_repo);
    void SetDataPointRepository(std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo);
    void SetCurrentValueRepository(std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo);
    void SetDatabaseClients(std::shared_ptr<RedisClient> redis_client, 
                       std::shared_ptr<InfluxClient> influx_client);
    void SetDeviceSettingsRepository(std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo);

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
    bool ShouldLogDataPoint(const PulseOne::Structs::DataPoint& data_point,
        const PulseOne::BasicTypes::DataVariant& new_value) const;
    void UpdateDataPointValue(PulseOne::Structs::DataPoint& data_point,
        const PulseOne::BasicTypes::DataVariant& new_value,
        PulseOne::Enums::DataQuality new_quality) const;

private:
    WorkerFactory() = default;
    ~WorkerFactory() = default;

    void RegisterWorkerCreators();
    std::string ValidateWorkerConfig(const Database::Entities::DeviceEntity& device_entity) const;
    
    // ✅ 전방 선언된 타입 사용
    PulseOne::Structs::DeviceInfo ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const;
    // 🔥 새로 추가: DataPoint 변환 메서드
    PulseOne::Structs::DataPoint ConvertToDataPoint(
        const Database::Entities::DataPointEntity& datapoint_entity,
        const std::string& device_id_string) const;
    
    // 🔥 새로 추가: 현재값 로드 헬퍼
    void LoadCurrentValueForDataPoint(PulseOne::Structs::DataPoint& data_point) const; 
    std::vector<PulseOne::Structs::DataPoint> LoadDataPointsForDevice(int device_id) const;
    
    // ✅ 새로 추가: 데이터 품질 헬퍼 함수
    std::string DataQualityToString(PulseOne::Enums::DataQuality quality) const;
    void ApplyDefaultSettings(PulseOne::Structs::DeviceInfo& device_info, 
                                        const std::string& protocol_type) const;

    void ValidateAndCorrectSettings(PulseOne::Structs::DeviceInfo& device_info) const;
    void ApplyProtocolSpecificDefaults(PulseOne::Structs::DeviceInfo& device_info, 
                                      const std::string& protocol_type) const;
    std::atomic<bool> initialized_{false};
    mutable std::mutex factory_mutex_;
    
    // 🔧 수정: 전역 클래스 포인터들 (PulseOne:: 제거)
    ::LogManager* logger_ = nullptr;
    ::ConfigManager* config_manager_ = nullptr;
    
    std::shared_ptr<Database::RepositoryFactory> repo_factory_;
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo_;
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo_;
    std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo_;
    // ✅ 전역 클래스 shared_ptr
    std::shared_ptr<::RedisClient> redis_client_;
    std::shared_ptr<::InfluxClient> influx_client_;
    
    std::map<std::string, WorkerCreator> worker_creators_;
    
    mutable std::atomic<uint64_t> workers_created_{0};
    mutable std::atomic<uint64_t> creation_failures_{0};
    std::chrono::system_clock::time_point factory_start_time_;

    std::string GetCurrentValueAsString(const PulseOne::Structs::DataPoint& data_point) const;
    std::string GetQualityString(const PulseOne::Structs::DataPoint& data_point) const;
    bool IsInitialized() const { return initialized_.load(); }
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_FACTORY_H