// =============================================================================
// collector/include/Api/HardwareApiCallbacks.h
// 하드웨어 제어 관련 REST API 콜백 설정 - 새로운 네이밍 반영
// =============================================================================

#ifndef API_HARDWARE_CALLBACKS_H
#define API_HARDWARE_CALLBACKS_H

#include <string>
#include <algorithm>

// 전방 선언
namespace PulseOne {
namespace Network {
class RestApiServer;
}
namespace Workers {
class WorkerFactory;
}
}

class LogManager;

namespace PulseOne {
namespace Api {

/**
 * @brief 하드웨어 제어 관련 REST API 콜백 설정 클래스
 * 
 * 디지털 출력(ON/OFF), 아날로그 출력(0-100%), 파라미터 변경을 처리합니다.
 */
class HardwareApiCallbacks {
public:
    /**
     * @brief RestApiServer에 하드웨어 제어 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     * @param worker_factory WorkerFactory 인스턴스 (실제 하드웨어 제어용)
     * @param logger LogManager 인스턴스
     */
    static void Setup(Network::RestApiServer* server, 
                     Workers::WorkerFactory* worker_factory, 
                     LogManager* logger);

private:
    HardwareApiCallbacks() = delete;  // 정적 클래스
    
    // ==========================================================================
    // 하드웨어 타입 판별 함수들
    // ==========================================================================
    
    /**
     * @brief 디지털 출력 ID로부터 장비 타입 판별
     * @param output_id 출력 ID (예: "pump1", "motor_main", "heater_zone1")
     * @return 장비 타입 문자열 ("pump", "motor", "heater", "fan", "indicator", "alarm", "digital_valve", "generic_digital")
     */
    static std::string DetermineDigitalOutputType(const std::string& output_id);
    
    /**
     * @brief 아날로그 출력 ID로부터 장비 타입 판별
     * @param output_id 출력 ID (예: "valve1", "speed_control", "power_output")
     * @return 장비 타입 문자열 ("analog_valve", "speed_control", "power_control", "position_control", "generic_analog")
     */
    static std::string DetermineAnalogOutputType(const std::string& output_id);
    
    /**
     * @brief 파라미터 ID로부터 파라미터 타입 판별
     * @param parameter_id 파라미터 ID (예: "temperature_setpoint", "pressure_target", "flow_rate")
     * @return 파라미터 타입 문자열 ("temperature", "pressure", "flow_rate", "speed", "level", "setpoint", "generic_parameter")
     */
    static std::string DetermineParameterType(const std::string& parameter_id);
    
    /**
     * @brief 파라미터 타입에 따른 단위 반환
     * @param parameter_id 파라미터 ID
     * @return 단위 문자열 ("°C", "bar", "L/min", "RPM", "%", "units")
     */
    static std::string GetParameterUnit(const std::string& parameter_id);
    
    // ==========================================================================
    // 값 유효성 검증 함수들
    // ==========================================================================
    
    /**
     * @brief 디지털 출력 값 검증
     * @param output_id 출력 ID
     * @param value 출력 값 (true/false)
     * @param logger 로거 인스턴스
     * @return 유효하면 true
     */
    static bool ValidateDigitalOutput(const std::string& output_id,
                                     bool value,
                                     LogManager* logger);
    
    /**
     * @brief 아날로그 출력 값 검증 (0-100% 범위)
     * @param output_id 출력 ID
     * @param value 출력 값 (0.0-100.0)
     * @param logger 로거 인스턴스
     * @return 유효하면 true
     */
    static bool ValidateAnalogOutputValue(const std::string& output_id,
                                         double value,
                                         LogManager* logger);
    
    /**
     * @brief 파라미터 값 검증 (타입별 범위 체크)
     * @param parameter_id 파라미터 ID
     * @param value 파라미터 값
     * @param logger 로거 인스턴스
     * @return 유효하면 true
     * 
     * 타입별 범위:
     * - temperature: -50~200°C
     * - pressure: 0~50 bar
     * - flow_rate: 0~1000 L/min
     * - speed: 0~10000 RPM
     * - level: 0~100%
     * - generic: -999999~999999
     */
    static bool ValidateParameterValue(const std::string& parameter_id,
                                      double value,
                                      LogManager* logger);
};

} // namespace Api
} // namespace PulseOne

#endif // API_HARDWARE_CALLBACKS_H