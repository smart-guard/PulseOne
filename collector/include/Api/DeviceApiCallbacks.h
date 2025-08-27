// =============================================================================
// collector/include/Api/DeviceApiCallbacks.h
// 디바이스 제어 및 워커 관리 관련 REST API 콜백 설정 - 그룹 기능 추가
// =============================================================================

#ifndef API_DEVICE_CALLBACKS_H
#define API_DEVICE_CALLBACKS_H

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

// 전방 선언
namespace PulseOne {
namespace Network {
    class RestApiServer;
}
namespace Workers {
    class WorkerFactory;
}
}

class ConfigManager;
class LogManager;

namespace PulseOne {
namespace Api {

/**
 * @brief 디바이스 제어 및 워커 관리 관련 REST API 콜백 설정 클래스
 * 
 * 디바이스 워커의 생명주기 제어와 상태 조회 기능을 제공합니다.
 * 개별 디바이스 제어와 그룹 단위 제어를 모두 지원합니다.
 * 실제 제어 로직은 WorkerFactory에 위임하여 처리합니다.
 */
class DeviceApiCallbacks {
public:
    /**
     * @brief RestApiServer에 디바이스 제어 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     * @param worker_factory WorkerFactory 인스턴스 (실제 워커 제어용)
     * @param logger LogManager 인스턴스
     */
    static void Setup(Network::RestApiServer* server, 
                     Workers::WorkerFactory* worker_factory, 
                     LogManager* logger);
    
    /**
     * @brief 전역 참조 설정 (재초기화에 필요)
     * @param config_manager ConfigManager 인스턴스
     */
    static void SetGlobalReferences(ConfigManager* config_manager);

    // ==========================================================================
    // 디바이스 그룹 정보 구조체
    // ==========================================================================
    
    /**
     * @brief 디바이스 그룹 정보
     */
    struct DeviceGroupInfo {
        std::string name;           ///< 그룹 표시 이름
        std::string description;    ///< 그룹 설명
        std::vector<std::string> devices;  ///< 그룹에 속한 디바이스 ID 목록
        
        DeviceGroupInfo() = default;
        DeviceGroupInfo(const std::string& n, const std::string& d, const std::vector<std::string>& devs);
    };

private:
    DeviceApiCallbacks() = delete;  // 정적 클래스
    
    // ==========================================================================
    // 워커 관리 헬퍼 함수들
    // ==========================================================================
    
    /**
     * @brief 모든 워커를 안전하게 정지
     * @param worker_factory WorkerFactory 인스턴스
     * @param logger LogManager 인스턴스
     * @return 성공시 true
     * 
     * 동작:
     * 1. 활성 워커 목록 조회
     * 2. 각 워커에게 정지 신호 전송 (연결 끊기)
     * 3. 모든 워커가 정지할 때까지 대기 (최대 10초)
     */
    static bool StopAllWorkersSafely(Workers::WorkerFactory* worker_factory, 
                                    LogManager* logger);
    
    /**
     * @brief 설정된 디바이스들을 재시작
     * @param worker_factory WorkerFactory 인스턴스
     * @param logger LogManager 인스턴스
     * @return 성공시 true
     * 
     * 동작:
     * 1. 설정 파일에서 활성화된 디바이스 목록 읽기
     * 2. 각 디바이스 워커 시작 (연결부터 새로 시작)
     * 3. 워커 간 시작 간격 적용 (200ms)
     */
    static bool RestartConfiguredDevices(Workers::WorkerFactory* worker_factory,
                                        LogManager* logger);
    
    // ==========================================================================
    // 디바이스 그룹 관리 함수들 - 새로 추가
    // ==========================================================================
    
    /**
     * @brief 설정된 디바이스 그룹 목록 조회
     * @return 그룹 ID를 키로 하는 그룹 정보 맵
     * 
     * 설정 파일 형식:
     * groups.list = ["production_line_a", "utility_systems"]
     * groups.production_line_a.name = "Production Line A"
     * groups.production_line_a.description = "Main production line devices"
     * groups.production_line_a.devices = ["1", "2", "3"]
     */
    static std::map<std::string, DeviceGroupInfo> GetConfiguredGroups();
    
    /**
     * @brief 특정 디바이스가 속한 그룹 목록 조회
     * @param device_id 디바이스 ID
     * @return 그룹 ID 목록
     */
    static std::vector<std::string> GetDeviceGroups(const std::string& device_id);
    
    // ==========================================================================
    // 유틸리티 함수들
    // ==========================================================================
    
    /**
     * @brief 디바이스 ID 유효성 검증
     * @param device_id 검증할 디바이스 ID
     * @param logger LogManager 인스턴스
     * @return 유효하면 true
     * 
     * 검증 조건:
     * - 빈 문자열 아님
     * - 영숫자, 하이픈, 언더스코어만 포함
     */
    static bool ValidateDeviceId(const std::string& device_id, LogManager* logger);
    
    /**
     * @brief 디바이스 에러 응답 JSON 생성
     * @param device_id 디바이스 ID
     * @param error 에러 메시지
     * @return JSON 에러 응답
     */
    static nlohmann::json CreateDeviceErrorResponse(const std::string& device_id, 
                                                    const std::string& error);
    
    /**
     * @brief 그룹 에러 응답 JSON 생성 - 새로 추가
     * @param group_id 그룹 ID
     * @param error 에러 메시지
     * @return JSON 에러 응답
     */
    static nlohmann::json CreateGroupErrorResponse(const std::string& group_id, 
                                                   const std::string& error);
};

} // namespace Api
} // namespace PulseOne

#endif // API_DEVICE_CALLBACKS_H