#ifndef WORKER_FACTORY_H
#define WORKER_FACTORY_H

/**
 * @file WorkerFactory.h
 * @brief PulseOne WorkerFactory - 의존성 최소화 버전
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🎯 핵심 역할:
 * - DeviceEntity → 적절한 Worker 자동 생성
 * - DataPointEntity들 자동 로드 및 Worker에 주입
 * - 기존 Worker 구조 (BaseDeviceWorker, ModbusTcpWorker 등) 완전 재사용
 * - Repository 패턴 활용 (DeviceRepository, DataPointRepository)
 * 
 * 🔄 동작 플로우:
 * DeviceEntity → WorkerFactory → ModbusTcpWorker/MQTTWorker/BACnetWorker
 *                              → DataPointEntity들 자동 로드 및 주입
 */

// 🔥 의존성 최소화: 필수 헤더만 include
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
// 🔥 전방 선언 (Forward Declarations) - 헤더 include 대신 사용
// =============================================================================

// Utils 네임스페이스
namespace PulseOne {
    class LogManager;
}
class ConfigManager;

// Database 클라이언트들
class RedisClient;
class InfluxClient;

// Common Types (구조체들만 전방 선언)
struct DeviceInfo;
struct DataPoint;

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
 */
using WorkerCreator = std::function<std::unique_ptr<BaseDeviceWorker>(
    const DeviceInfo& device_info,
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
 * 
 * 기능:
 * - DeviceEntity 기반 Worker 자동 생성
 * - 프로토콜별 Worker 타입 선택 (MODBUS_TCP, MQTT, BACNET)
 * - DataPointEntity들 자동 로드 및 Worker에 주입
 * - 기존 Worker 생태계 100% 활용
 */
class WorkerFactory {
public:
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    
    /**
     * @brief 싱글톤 인스턴스 획득
     */
    static WorkerFactory& getInstance();
    
    /**
     * @brief 복사 생성자 및 대입 연산자 삭제
     */
    WorkerFactory(const WorkerFactory&) = delete;
    WorkerFactory& operator=(const WorkerFactory&) = delete;

    // =======================================================================
    // 초기화 및 설정
    // =======================================================================
    
    /**
     * @brief 팩토리 초기화
     * @return 성공 여부
     */
    bool Initialize();
    
    /**
     * @brief Repository 설정 (의존성 주입)
     */
    void SetDeviceRepository(std::shared_ptr<Database::Repositories::DeviceRepository> device_repo);
    void SetDataPointRepository(std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo);
    
    /**
     * @brief 데이터베이스 클라이언트 설정
     */
    void SetDatabaseClients(
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client
    );

    // =======================================================================
    // Worker 생성 메서드들
    // =======================================================================
    
    /**
     * @brief DeviceEntity로부터 Worker 생성
     * @param device_entity 디바이스 엔티티
     * @return 생성된 Worker (실패 시 nullptr)
     */
    std::unique_ptr<BaseDeviceWorker> CreateWorker(const Database::Entities::DeviceEntity& device_entity);
    
    /**
     * @brief 디바이스 ID로 Worker 생성
     * @param device_id 디바이스 ID
     * @return 생성된 Worker (실패 시 nullptr)
     */
    std::unique_ptr<BaseDeviceWorker> CreateWorkerById(int device_id);
    
    /**
     * @brief 테넌트의 모든 활성 디바이스용 Worker 생성
     * @param tenant_id 테넌트 ID
     * @return 생성된 Worker 목록
     */
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateAllActiveWorkers(int tenant_id = 1);
    
    /**
     * @brief 프로토콜별 활성 디바이스용 Worker 생성
     * @param protocol_type 프로토콜 타입 (MODBUS_TCP, MQTT, BACNET)
     * @param tenant_id 테넌트 ID
     * @return 생성된 Worker 목록
     */
    std::vector<std::unique_ptr<BaseDeviceWorker>> CreateWorkersByProtocol(
        const std::string& protocol_type, 
        int tenant_id = 1
    );

    // =======================================================================
    // 팩토리 정보 조회
    // =======================================================================
    
    /**
     * @brief 지원되는 프로토콜 목록 조회
     */
    std::vector<std::string> GetSupportedProtocols() const;
    
    /**
     * @brief 프로토콜 지원 여부 확인
     */
    bool IsProtocolSupported(const std::string& protocol_type) const;
    
    /**
     * @brief 팩토리 통계 조회
     */
    FactoryStats GetFactoryStats() const;
    
    /**
     * @brief 팩토리 통계 조회 (문자열 버전)
     */
    std::string GetFactoryStatsString() const;

    // =======================================================================
    // 확장성 지원 (새 프로토콜 추가)
    // =======================================================================
    
    /**
     * @brief 새 Worker 생성자 등록
     * @param protocol_type 프로토콜 타입
     * @param creator Worker 생성 함수
     */
    void RegisterWorkerCreator(const std::string& protocol_type, WorkerCreator creator);

private:
    // =======================================================================
    // 생성자 (싱글톤)
    // =======================================================================
    
    WorkerFactory() = default;
    ~WorkerFactory() = default;

    // =======================================================================
    // 내부 초기화 메서드들
    // =======================================================================
    
    /**
     * @brief 기본 Worker 생성자들 등록
     */
    void RegisterWorkerCreators();
    
    /**
     * @brief Worker 설정 검증
     */
    std::string ValidateWorkerConfig(const Database::Entities::DeviceEntity& device_entity) const;

    // =======================================================================
    // 변환 메서드들 (Entity → 기존 구조체)
    // =======================================================================
    
    /**
     * @brief DeviceEntity → DeviceInfo 변환
     */
    DeviceInfo ConvertToDeviceInfo(const Database::Entities::DeviceEntity& device_entity) const;
    
    /**
     * @brief 디바이스의 DataPoint들 로드
     */
    std::vector<DataPoint> LoadDataPointsForDevice(int device_id) const;

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 초기화 상태
    std::atomic<bool> initialized_{false};
    mutable std::mutex factory_mutex_;
    
    // 🔥 포인터로 변경 (헤더 include 회피)
    LogManager* logger_ = nullptr;
    ConfigManager* config_manager_ = nullptr;
    
    // Repository들 (의존성 주입)
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo_;
    
    // 데이터베이스 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // Worker 생성자 맵
    std::map<std::string, WorkerCreator> worker_creators_;
    
    // 통계 정보
    mutable std::atomic<uint64_t> workers_created_{0};
    mutable std::atomic<uint64_t> creation_failures_{0};
    std::chrono::system_clock::time_point factory_start_time_;
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_FACTORY_H