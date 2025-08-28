// =============================================================================
// collector/include/Api/HardwareApiCallbacks.h
// 하드웨어 제어 REST API 콜백 설정 - 싱글톤 직접 접근 방식
// =============================================================================

#ifndef API_HARDWARE_CALLBACKS_H
#define API_HARDWARE_CALLBACKS_H

#include <string>

namespace PulseOne {
namespace Network {
    class RestApiServer;
}
}

namespace PulseOne {
namespace Api {

/**
 * @brief 하드웨어 제어 관련 REST API 콜백 설정 클래스
 * 
 * WorkerManager를 통해 실제 하드웨어 제어를 수행합니다.
 * 디지털 출력(ON/OFF), 아날로그 출력(0-100%), 파라미터 변경을 지원합니다.
 * 
 * 특징:
 * - 싱글톤 매니저들에 직접 접근 (getInstance() 사용)
 * - WorkerManager를 통한 실제 제어 로직 구현
 * - 간단하고 실용적인 검증 로직
 */
class HardwareApiCallbacks {
public:
    /**
     * @brief RestApiServer에 하드웨어 제어 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     * 
     * 등록되는 콜백들:
     * - 디지털 출력 제어 (펌프, 모터, 밸브 등 ON/OFF)
     * - 아날로그 출력 제어 (밸브 개도, 모터 속도 등 0-100%)
     * - 파라미터 변경 (온도/압력 설정값 등)
     */
    static void Setup(Network::RestApiServer* server);

private:
    HardwareApiCallbacks() = delete;  // 정적 클래스
    
    // ==========================================================================
    // 값 검증 헬퍼 함수들
    // ==========================================================================
    
    /**
     * @brief 아날로그 출력 값 범위 검증 (0-100%)
     * @param value 출력 값
     * @return 유효하면 true
     */
    static bool ValidateAnalogValue(double value);
    
    /**
     * @brief 파라미터 값 범위 검증 (기본 안전 범위)
     * @param value 파라미터 값
     * @return 유효하면 true
     */
    static bool ValidateParameterValue(double value);
};

} // namespace Api
} // namespace PulseOne

#endif // API_HARDWARE_CALLBACKS_H