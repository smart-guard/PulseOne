/**
 * @file BaseDeviceWorker.h
 * @brief 모든 프로토콜 워커의 공통 기반 클래스
 * @details 프로토콜에 독립적인 핵심 인터페이스와 공통 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef WORKERS_BASE_DEVICE_WORKER_H
#define WORKERS_BASE_DEVICE_WORKER_H

#include "Drivers/Common/CommonTypes.h"
#include "Utils/LogManager.h"
#include "Client/InfluxClient.h"
#include <memory>
#include <future>
#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace PulseOne {
namespace Workers {

// 워커 상태 열거형 (현장 운영 상황 반영)
enum class WorkerState {
    STOPPED = 0,                ///< 정지됨
    STARTING = 1,               ///< 시작 중
    RUNNING = 2,                ///< 정상 실행 중
    PAUSED = 3,                 ///< 일시정지됨 (소프트웨어)
    STOPPING = 4,               ///< 정지 중
    ERROR = 5,                  ///< 오류 상태
    
    // 현장 운영 상태들
    MAINTENANCE = 10,           ///< 점검 모드 (하드웨어 점검 중)
    SIMULATION = 11,            ///< 시뮬레이션 모드 (테스트 데이터)
    CALIBRATION = 12,           ///< 교정 모드 (센서 교정 중)
    COMMISSIONING = 13,         ///< 시운전 모드 (초기 설정 중)
    
    // 장애 상황들
    DEVICE_OFFLINE = 20,        ///< 디바이스 오프라인
    COMMUNICATION_ERROR = 21,   ///< 통신 오류
    DATA_INVALID = 22,          ///< 데이터 이상 (범위 벗어남)
    SENSOR_FAULT = 23,          ///< 센서 고장
    
    // 특수 운영 모드들
    MANUAL_OVERRIDE = 30,       ///< 수동 제어 모드
    EMERGENCY_STOP = 31,        ///< 비상 정지
    BYPASS_MODE = 32,           ///< 우회 모드 (백업 시스템 사용)
    DIAGNOSTIC_MODE = 33        ///< 진단 모드 (상세 로깅)
};

/**
 * @brief 모든 디바이스 워커의 기반 클래스
 */
class BaseDeviceWorker {
public:
    BaseDeviceWorker() = default;
    virtual ~BaseDeviceWorker() = default;
    
    // 순수 가상 함수들
    virtual std::future<bool> Start() = 0;
    virtual std::future<bool> Stop() = 0;
    virtual WorkerState GetState() const = 0;
    
    // 공통 인터페이스
    virtual std::future<bool> Pause() { 
        // 기본 구현
        auto promise = std::make_shared<std::promise<bool>>();
        promise->set_value(true);
        return promise->get_future();
    }
    
    virtual std::future<bool> Resume() {
        // 기본 구현  
        auto promise = std::make_shared<std::promise<bool>>();
        promise->set_value(true);
        return promise->get_future();
    }
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_BASE_DEVICE_WORKER_H
