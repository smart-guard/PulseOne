// =============================================================================
// collector/include/Api/DeviceApiCallbacks.h
// 디바이스 제어 REST API 콜백 설정 - 싱글톤 직접 접근 방식
// =============================================================================

#ifndef API_DEVICE_CALLBACKS_H
#define API_DEVICE_CALLBACKS_H

#include <string>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Network {
    class RestApiServer;
}
}

namespace PulseOne {
namespace Api {

/**
 * @brief 디바이스 제어 관련 REST API 콜백 설정 클래스
 * 
 * 실제 WorkerManager와 연동하여 디바이스 워커의 생명주기를 제어하고
 * Repository를 통해 실시간 디바이스 정보를 조회합니다.
 * 
 * 특징:
 * - 싱글톤 매니저들에 직접 접근 (getInstance() 사용)
 * - 실제 워커 제어 로직 구현
 * - DB와 연동한 디바이스 정보 조회
 * - 전체 시스템 재초기화 지원
 */
class DeviceApiCallbacks {
public:
    /**
     * @brief RestApiServer에 디바이스 제어 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     * 
     * 등록되는 콜백들:
     * - 디바이스 목록 조회 (DB 연동)
     * - 디바이스 상태 조회 (실시간)
     * - 워커 생명주기 제어 (시작/중지/재시작/일시정지/재개)
     * - 전체 시스템 재초기화
     */
    static void Setup(Network::RestApiServer* server);

private:
    DeviceApiCallbacks() = delete;  // 정적 클래스
    
    // ==========================================================================
    // 워커 관리 헬퍼 함수들
    // ==========================================================================
    
    /**
     * @brief 모든 워커를 안전하게 정지
     * @return 성공시 true
     * 
     * 동작:
     * 1. WorkerManager를 통해 모든 워커에 정지 신호 전송
     * 2. 최대 10초간 정상 종료 대기
     * 3. 남은 워커 수 확인 및 로그 출력
     */
    static bool StopAllWorkersSafely();
    
    /**
     * @brief 설정된 디바이스들을 재시작
     * @return 성공시 true
     * 
     * 동작:
     * 1. DB에서 활성화된 디바이스 목록 조회 (isEnabled() == true)
     * 2. 각 디바이스 워커를 WorkerManager를 통해 시작
     * 3. 시작 간격 조절 (200ms) 및 결과 확인
     */
    static bool RestartConfiguredDevices();
    
    /**
     * @brief 디바이스 에러 응답 생성
     * @param device_id 디바이스 ID
     * @param error 에러 메시지
     * @return JSON 에러 응답
     */
    static nlohmann::json CreateDeviceErrorResponse(const std::string& device_id, 
                                                   const std::string& error);
};

} // namespace Api
} // namespace PulseOne

#endif // API_DEVICE_CALLBACKS_H