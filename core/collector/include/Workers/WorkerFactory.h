// =============================================================================
// collector/include/Workers/WorkerFactory.h - 간단한 객체 생성 팩토리
// =============================================================================
#pragma once

#ifndef WORKER_FACTORY_H
#define WORKER_FACTORY_H

#include <memory>
#include <functional>
#include <map>
#include <string>

// ✅ 기존 프로젝트 구조체들
#include "Common/Structs.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"

namespace PulseOne {
namespace Workers {

class BaseDeviceWorker;

// ✅ 단순한 WorkerCreator 타입
using WorkerCreator = std::function<std::unique_ptr<BaseDeviceWorker>(const PulseOne::Structs::DeviceInfo&)>;

/**
 * @brief 단순한 Worker 생성 팩토리 (싱글톤 아님)
 * 
 * 핵심 기능:
 * - DeviceEntity에서 Worker 생성
 * - 프로토콜별 Creator 등록/조회
 * - 설정 변경시 리로드
 */
class WorkerFactory {
public:
    WorkerFactory() = default;
    ~WorkerFactory() = default;
    
    // ==========================================================================
    // 핵심 Worker 생성 기능
    // ==========================================================================
    
    /**
     * @brief 디바이스에서 Worker 생성
     */
    std::unique_ptr<BaseDeviceWorker> CreateWorker(const Database::Entities::DeviceEntity& device);
    
    /**
     * @brief 디바이스 ID로 Worker 생성 (DB에서 DeviceEntity 로드)
     */
    std::unique_ptr<BaseDeviceWorker> CreateWorkerById(int device_id);
    
    /**
     * @brief 디바이스 ID로 최신 DeviceInfo 로드
     */
    bool GetDeviceInfoById(int device_id, PulseOne::Structs::DeviceInfo& info);

    /**
     * @brief 디바이스의 데이터 포인트 목록 로드
     */
    std::vector<PulseOne::Structs::DataPoint> LoadDeviceDataPoints(int device_id);
    
    // ==========================================================================
    // 프로토콜 관리
    // ==========================================================================
    
    /**
     * @brief 지원하는 프로토콜 목록
     */
    std::vector<std::string> GetSupportedProtocols() const;
    
    /**
     * @brief 프로토콜 지원 여부 확인
     */
    bool IsProtocolSupported(const std::string& protocol_type) const;
    
    /**
     * @brief 프로토콜 설정 리로드 (DB 변경시)
     */
    void ReloadProtocols();

private:
    // ==========================================================================
    // 내부 변환 함수들 (안전 버전들)
    // ==========================================================================
    
    /**
     * @brief protocol_id로 실제 프로토콜 타입 조회
     */
    std::string GetProtocolTypeById(int protocol_id);
    
    /**
     * @brief DeviceEntity를 DeviceInfo로 안전하게 변환
     */
    bool ConvertToDeviceInfoSafe(const Database::Entities::DeviceEntity& device, 
                                PulseOne::Structs::DeviceInfo& info);
    
    /**
     * @brief endpoint 파싱 (IP:Port 추출) - 안전 버전
     */
    bool ParseEndpointSafe(PulseOne::Structs::DeviceInfo& info);
    
    /**
     * @brief JSON config를 properties로 변환 - 안전 버전
     */
    bool ParseConfigToPropertiesSafe(PulseOne::Structs::DeviceInfo& info);
    
    /**
     * @brief DeviceSettings 로드 및 적용 - 안전 버전
     */
    bool LoadDeviceSettingsSafe(PulseOne::Structs::DeviceInfo& info, int device_id);
    
    /**
     * @brief 프로토콜별 기본값 적용 - 안전 버전
     */
    bool ApplyProtocolDefaultsSafe(PulseOne::Structs::DeviceInfo& info);
    
    /**
     * @brief 기본 설정값 적용
     */
    void ApplyDefaultSettings(PulseOne::Structs::DeviceInfo& info);
    
    /**
     * @brief 프로토콜별 기본 포트 반환
     */
    int GetDefaultPort(const std::string& protocol_type);
    
    /**
     * @brief 프로토콜 Creator들 로드 (스레드 안전, 캐시됨)
     */
    std::map<std::string, WorkerCreator> LoadProtocolCreators() const;

};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_FACTORY_H