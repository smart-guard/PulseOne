/**
 * @file TcpBasedWorker.h
 * @brief TCP 기반 프로토콜들의 공통 기능을 제공하는 중간 클래스 (수정됨)
 * @details Modbus TCP, MQTT, BACnet/IP 등이 상속받아 사용 (재연결은 BaseDeviceWorker에서 처리)
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 2.1.0
 */

#ifndef WORKERS_TCP_BASED_WORKER_H
#define WORKERS_TCP_BASED_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Common/CommonTypes.h"
#include <string>
#include <atomic>
#include <chrono>
#include <memory>

namespace PulseOne {
namespace Workers {

/**
 * @brief TCP 기반 프로토콜 워커의 기반 클래스 (간소화됨)
 * @details TCP 연결 설정만 관리, 재연결은 BaseDeviceWorker가 처리
 */
class TcpBasedWorker : public BaseDeviceWorker {
public:
    /**
     * @brief 생성자 (BaseDeviceWorker와 일치)
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트
     * @param influx_client InfluxDB 클라이언트
     */
    TcpBasedWorker(const Drivers::DeviceInfo& device_info,
                   std::shared_ptr<RedisClient> redis_client,
                   std::shared_ptr<InfluxClient> influx_client);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~TcpBasedWorker();
    
    // =============================================================================
    // TCP 설정 관리
    // =============================================================================
    
    /**
     * @brief TCP 연결 설정
     * @param ip IP 주소
     * @param port 포트 번호
     * @param timeout_seconds 연결 타임아웃 (초)
     */
    void ConfigureTcpConnection(const std::string& ip, 
                               uint16_t port,
                               int timeout_seconds = 5);
    
    /**
     * @brief TCP 연결 정보 조회
     * @return JSON 형태의 연결 정보
     */
    std::string GetTcpConnectionInfo() const;
    
    // =============================================================================
    // BaseDeviceWorker 순수 가상 함수 구현 (TCP 공통)
    // =============================================================================
    
    /**
     * @brief TCP 기반 연결 수립 (파생 클래스에서 오버라이드 가능)
     * @return 성공 시 true
     */
    bool EstablishConnection() override;
    
    /**
     * @brief TCP 기반 연결 해제 (파생 클래스에서 오버라이드 가능)
     * @return 성공 시 true
     */
    bool CloseConnection() override;
    
    /**
     * @brief TCP 기반 연결 상태 확인 (파생 클래스에서 오버라이드 가능)
     * @return 연결 상태
     */
    bool CheckConnection() override;
    
    /**
     * @brief TCP 기반 Keep-alive 전송 (파생 클래스에서 오버라이드 가능)
     * @return 성공 시 true
     */
    bool SendKeepAlive() override;

protected:
    // =============================================================================
    // 파생 클래스에서 구현해야 하는 프로토콜별 메서드들
    // =============================================================================
    
    /**
     * @brief 프로토콜별 연결 수립 (Modbus, MQTT, BACnet 등에서 구현)
     * @return 성공 시 true
     */
    virtual bool EstablishProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 해제 (Modbus, MQTT, BACnet 등에서 구현)
     * @return 성공 시 true
     */
    virtual bool CloseProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 상태 확인 (Modbus, MQTT, BACnet 등에서 구현)
     * @return 연결 상태
     */
    virtual bool CheckProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 Keep-alive 전송 (파생 클래스에서 선택적 구현)
     * @return 성공 시 true
     */
    bool SendProtocolKeepAlive();
    
    // =============================================================================
    // TCP 소켓 관리 (파생 클래스에서 사용 가능)
    // =============================================================================
    
    /**
     * @brief TCP 설정 검증
     * @return 유효한 설정인 경우 true
     */
    bool ValidateTcpConfig() const;
    
    /**
     * @brief TCP 소켓 생성 및 연결
     * @return 성공 시 true
     */
    bool CreateTcpSocket();
    
    /**
     * @brief TCP 소켓 해제
     */
    void CloseTcpSocket();
    
    /**
     * @brief TCP 소켓 연결 상태 확인
     * @return 연결된 경우 true
     */
    bool IsTcpSocketConnected() const;
    
    // =============================================================================
    // 파생 클래스에서 사용할 수 있는 보호된 멤버들
    // =============================================================================
    
    std::string ip_address_;                              ///< IP 주소
    uint16_t port_;                                       ///< 포트 번호
    int connection_timeout_seconds_;                      ///< 연결 타임아웃
    int socket_fd_;                                       ///< TCP 소켓 파일 디스크립터

private:
    // =============================================================================
    // TCP 연결 관리 (내부용)
    // =============================================================================
    void ParseEndpoint() {  // 
        // device_info_가 이제 protected이므로 접근 가능
        if (!device_info_.endpoint.empty()) {
            size_t colon_pos = device_info_.endpoint.find(':');
            if (colon_pos != std::string::npos) {
                ip_address_ = device_info_.endpoint.substr(0, colon_pos);
                port_ = static_cast<uint16_t>(std::stoi(device_info_.endpoint.substr(colon_pos + 1)));
            }
        }
    }
    /**
     * @brief 기본 TCP 소켓 연결 확인 (내부용)
     * @return 연결 상태
     */
    bool CheckTcpSocket() const;
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_TCP_BASED_WORKER_H