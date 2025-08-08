/**
 * @file WorkerFactory.h
 * @brief PulseOne WorkerFactory - 완전한 DB 통합 버전 헤더
 * @author PulseOne Development Team
 * @date 2025-08-08
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

// ✅ 필수 헤더들
#include "Common/Enums.h"
#include "Common/BasicTypes.h"
#include "Utils/LogManager.h"

// 🔧 전역 네임스페이스 전방선언
class LogManager;
class ConfigManager;
class RedisClient;
class InfluxClient;

namespace PulseOne {

// ✅ PulseOne 네임스페이스 전방선언들
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

// ✅ WorkerCreator 타입 정의
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
    // ==========================================================================
    // 싱글톤 및 초기화
    // ==========================================================================
    static WorkerFactory& getInstance();
    
    WorkerFactory(const WorkerFactory&) = delete;
    WorkerFactory& operator=(const WorkerFactory&) = delete;

    bool Initialize();
    bool Initialize(::LogManager* logger, ::ConfigManager* config_manager);
    
    // ==========================================================================
    // 의존성 주입
    // ==========================================================================
    void SetRepositoryFactory(std::shared_ptr<Database::RepositoryFactory> repo_factory);
    void SetDeviceRepository(std::shared_ptr<Database::Repositories::DeviceRepository> device_repo);
    void SetDataPointRepository(std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo);
    void SetCurrentValueRepository(std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo);
    void SetDeviceSettingsRepository(std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo);
    void SetDatabaseClients(std::shared_ptr<RedisClient> redis_client, 
                           std::shared_ptr<InfluxClient> influx_client);

    // ==========================================================================
    // Worker 생성
    // ==========================================================================
    std::unique_ptr<BaseDeviceWorker> CreateWorker(const Database::Entities::DeviceEntity& device_entity);
    std::unique_ptr<BaseDeviceWorker> CreateWorkerById(int device_id);
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateAllActiveWorkers();
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateAllActiveWorkers(int max_workers);
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateWorkersByProtocol(const std::string& protocol_type, int max_workers = 0);

    // ==========================================================================
    // 팩토리 정보
    // ==========================================================================
    std::vector<std::string> GetSupportedProtocols() const;
    bool IsProtocolSupported(const std::string& protocol_type) const;
    FactoryStats GetFactoryStats() const;
    std::string GetFactoryStatsString() const;
    void RegisterWorkerCreator(const std::string& protocol_type, WorkerCreator creator);
    
    // ==========================================================================
    // 데이터 헬퍼 함수들
    // ==========================================================================
    bool ShouldLogDataPoint(const PulseOne::Structs::DataPoint& data_point,
        const PulseOne::BasicTypes::DataVariant& new_value) const;
    void UpdateDataPointValue(PulseOne::Structs::DataPoint& data_point,
        const PulseOne::BasicTypes::DataVariant& new_value,
        PulseOne::Enums::DataQuality new_quality) const;

private:
    WorkerFactory() = default;
    ~WorkerFactory() = default;

    // ==========================================================================
    // 내부 초기화 및 등록
    // ==========================================================================
    void RegisterWorkerCreators();
    std::string ValidateWorkerConfig(const Database::Entities::DeviceEntity& device_entity) const;
    
    // ==========================================================================
    // 🔥 완전한 DB 통합 변환 메서드들
    // ==========================================================================
    PulseOne::Structs::DeviceInfo ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const;
    PulseOne::Structs::DataPoint ConvertToDataPoint(
        const Database::Entities::DataPointEntity& datapoint_entity,
        const std::string& device_id_string) const;
    
    // ==========================================================================
    // 🔥 JSON 파싱 및 데이터 로딩 헬퍼들
    // ==========================================================================
    void ParseDeviceConfigToProperties(PulseOne::Structs::DeviceInfo& device_info) const;
    void ParseEndpoint(PulseOne::Structs::DeviceInfo& device_info) const;
    PulseOne::BasicTypes::DataVariant ParseJSONValue(
        const std::string& json_value, 
        const std::string& data_type) const;
    
    void LoadCurrentValueForDataPoint(PulseOne::Structs::DataPoint& data_point) const; 
    std::vector<PulseOne::Structs::DataPoint> LoadDataPointsForDevice(int device_id) const;
    
    // ==========================================================================
    // 설정 및 검증 헬퍼들
    // ==========================================================================
    void ApplyDefaultSettings(PulseOne::Structs::DeviceInfo& device_info, 
                             const std::string& protocol_type) const;
    void ValidateAndCorrectSettings(PulseOne::Structs::DeviceInfo& device_info) const;
    
    // ==========================================================================
    // 품질 및 통계 헬퍼들
    // ==========================================================================
    std::string GetCurrentValueAsString(const PulseOne::Structs::DataPoint& data_point) const;
    std::string GetQualityString(const PulseOne::Structs::DataPoint& data_point) const;
    
    bool IsInitialized() const { return initialized_.load(); }

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    std::atomic<bool> initialized_{false};
    mutable std::mutex factory_mutex_;
    
    // 🔧 전역 클래스 포인터들
    ::LogManager* logger_ = nullptr;
    ::ConfigManager* config_manager_ = nullptr;
    
    // Repository들
    std::shared_ptr<Database::RepositoryFactory> repo_factory_;
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo_;
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo_;
    std::shared_ptr<Database::Repositories::DeviceSettingsRepository> device_settings_repo_;
    
    // 데이터베이스 클라이언트들
    std::shared_ptr<::RedisClient> redis_client_;
    std::shared_ptr<::InfluxClient> influx_client_;
    
    // Worker Creator 맵
    std::map<std::string, WorkerCreator> worker_creators_;
    
    // 통계
    mutable std::atomic<uint64_t> workers_created_{0};
    mutable std::atomic<uint64_t> creation_failures_{0};
    std::chrono::system_clock::time_point factory_start_time_;

    void LogSupportedProtocols() const; 
    std::string GetProtocolConfigInfo(const std::string& protocol_type) const;

};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_FACTORY_H