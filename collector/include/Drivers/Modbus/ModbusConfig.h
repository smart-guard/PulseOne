/**
 * 🔧 공통 ModbusConfig 헤더 파일 - 수정된 버전
 * 
 * 파일: include/Drivers/Modbus/ModbusConfig.h
 * 
 * 용도: ModbusRtuWorker, ModbusTcpWorker, ModbusDriver에서 공통 사용
 * 
 * 🚨 수정사항: 헤더 가드 중복 및 네임스페이스 중복 제거
 */

#ifndef PULSEONE_DRIVERS_MODBUS_CONFIG_H
#define PULSEONE_DRIVERS_MODBUS_CONFIG_H

#include <string>
#include <cstdint>

namespace PulseOne {
namespace Drivers {

/**
 * @brief Modbus 통합 설정 구조체
 * @details 모든 Modbus 관련 클래스에서 공통 사용
 * 
 * 사용처:
 * - ModbusTcpWorker
 * - ModbusRtuWorker  
 * - ModbusDriver
 * 
 * 포함 사항:
 * - 프로토콜 기본 설정
 * - 통신 타이밍 설정
 * - RTU 전용 설정
 * - Worker 레벨 설정
 */
struct ModbusConfig {
    // =======================================================================
    // 프로토콜 기본 설정 (connection_string JSON에서 파싱)
    // =======================================================================
    uint8_t slave_id = 1;                        ///< 슬레이브 ID (1-247)
    std::string byte_order = "big_endian";       ///< 바이트 순서 (big_endian/little_endian)
    uint16_t max_registers_per_group = 125;      ///< 그룹당 최대 레지스터 수
    bool auto_group_creation = true;             ///< 자동 그룹 생성 활성화
    
    // =======================================================================
    // 통신 타이밍 설정 (DeviceInfo/DeviceSettings에서 매핑)
    // =======================================================================
    uint32_t timeout_ms = 3000;                  ///< 연결 타임아웃 (connection_timeout_ms)
    uint32_t response_timeout_ms = 1000;         ///< 응답 타임아웃 (read_timeout_ms)
    uint32_t byte_timeout_ms = 100;              ///< 바이트 간 타임아웃
    uint8_t max_retries = 3;                     ///< 최대 재시도 횟수 (max_retry_count)
    
    // =======================================================================
    // Worker 레벨 설정 (중복 제거를 위해 추가)
    // =======================================================================
    uint32_t default_polling_interval_ms = 1000; ///< 기본 폴링 간격
    
    // =======================================================================
    // RTU 전용 설정 (RTU에서만 사용)
    // =======================================================================
    uint32_t baud_rate = 9600;                   ///< 보드레이트 (RTU 전용)
    char parity = 'N';                           ///< 패리티 (N/E/O, RTU 전용)
    uint8_t data_bits = 8;                       ///< 데이터 비트 (RTU 전용)
    uint8_t stop_bits = 1;                       ///< 스톱 비트 (RTU 전용)
    uint32_t frame_delay_ms = 50;                ///< 프레임 간 지연 (RTU 전용)
    
    // =======================================================================
    // 검증 및 유틸리티 메서드
    // =======================================================================
    
    /**
     * @brief 설정 검증
     * @param is_rtu RTU 모드인지 여부 (기본값: false, TCP)
     * @return 유효하면 true
     */
    bool IsValid(bool is_rtu = false) const {
        // 공통 검증
        bool common_valid = (slave_id >= 1 && slave_id <= 247) &&
                           (timeout_ms >= 100 && timeout_ms <= 30000) &&
                           (response_timeout_ms >= 100 && response_timeout_ms <= 10000) &&
                           (max_retries <= 10) &&
                           (max_registers_per_group >= 1 && max_registers_per_group <= 125) &&
                           (byte_order == "big_endian" || byte_order == "little_endian") &&
                           (default_polling_interval_ms >= 100 && default_polling_interval_ms <= 60000);
        
        if (!common_valid) return false;
        
        // RTU 추가 검증
        if (is_rtu) {
            return (baud_rate >= 1200 && baud_rate <= 115200) &&
                   (parity == 'N' || parity == 'E' || parity == 'O') &&
                   (data_bits == 7 || data_bits == 8) &&
                   (stop_bits == 1 || stop_bits == 2) &&
                   (frame_delay_ms >= 10 && frame_delay_ms <= 500);
        }
        
        return true;
    }
    
    /**
     * @brief 설정을 문자열로 변환 (디버깅용)
     * @param is_rtu RTU 정보도 포함할지 여부
     * @return 설정 정보 문자열
     */
    std::string ToString(bool is_rtu = false) const {
        std::string result = "ModbusConfig{" 
                           "slave_id=" + std::to_string(slave_id) + 
                           ", byte_order=" + byte_order + 
                           ", timeout=" + std::to_string(timeout_ms) + "ms" +
                           ", max_registers=" + std::to_string(max_registers_per_group) +
                           ", max_retries=" + std::to_string(max_retries) +
                           ", polling_interval=" + std::to_string(default_polling_interval_ms) + "ms" +
                           ", auto_group=" + (auto_group_creation ? "true" : "false");
        
        if (is_rtu) {
            result += ", baud_rate=" + std::to_string(baud_rate) +
                     ", parity=" + std::string(1, parity) +
                     ", data_bits=" + std::to_string(data_bits) +
                     ", stop_bits=" + std::to_string(stop_bits) +
                     ", frame_delay=" + std::to_string(frame_delay_ms) + "ms";
        }
        
        result += "}";
        return result;
    }
    
    /**
     * @brief TCP 기본값으로 리셋
     */
    void ResetToTcpDefaults() {
        slave_id = 1;
        byte_order = "big_endian";
        max_registers_per_group = 125;
        auto_group_creation = true;
        timeout_ms = 3000;
        response_timeout_ms = 1000;
        byte_timeout_ms = 100;
        max_retries = 3;
        default_polling_interval_ms = 1000;
    }
    
    /**
     * @brief RTU 기본값으로 리셋
     */
    void ResetToRtuDefaults() {
        ResetToTcpDefaults();  // 공통 설정 먼저
        
        // RTU 특화 설정
        baud_rate = 9600;
        parity = 'N';
        data_bits = 8;
        stop_bits = 1;
        frame_delay_ms = 50;
        response_timeout_ms = 500;  // RTU는 더 짧게
        byte_timeout_ms = 50;       // RTU는 더 짧게
        default_polling_interval_ms = 1000;
    }
    
    /**
     * @brief 공통 기본값으로 리셋 (TCP/RTU 구분 없음)
     */
    void ResetToDefaults() {
        ResetToTcpDefaults();
    }
    
    // =======================================================================
    // Worker 레벨 편의 메서드들
    // =======================================================================
    
    /**
     * @brief 폴링 간격을 밀리초로 가져오기
     * @return 폴링 간격 (ms)
     */
    uint32_t GetPollingIntervalMs() const {
        return default_polling_interval_ms;
    }
    
    /**
     * @brief 폴링 간격 설정 (검증 포함)
     * @param interval_ms 폴링 간격 (ms)
     */
    void SetPollingIntervalMs(uint32_t interval_ms) {
        if (interval_ms >= 100 && interval_ms <= 60000) {
            default_polling_interval_ms = interval_ms;
        }
    }
    
    /**
     * @brief 자동 그룹 생성 활성화 여부
     * @return 활성화되면 true
     */
    bool IsAutoGroupCreationEnabled() const {
        return auto_group_creation;
    }
    
    /**
     * @brief 최대 레지스터 그룹 크기 가져오기
     * @return 최대 레지스터 수
     */
    uint16_t GetMaxRegistersPerGroup() const {
        return max_registers_per_group;
    }
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MODBUS_CONFIG_H