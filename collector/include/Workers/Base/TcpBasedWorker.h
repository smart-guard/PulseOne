/**
 * @file TcpBasedWorker.h
 * @brief TCP 기반 프로토콜 워커 (중복 제거 완성본)
 * @details BaseDeviceWorker의 재연결/Keep-alive 기능 활용, TCP 특화 기능만 추가
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 3.0.0
 */

#ifndef WORKERS_BASE_TCP_BASED_WORKER_H
#define WORKERS_BASE_TCP_BASED_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Common/CommonTypes.h"
#include <string>
#include <atomic>
#include <chrono>

namespace PulseOne {
namespace Workers {

/**
 * @brief TCP 기반 프로토콜 워커 기반 클래스 (간소화됨)
 * @details 재연결/Keep-alive는 BaseDeviceWorker가 처리, TCP 설정만 관리
 */
class TcpBasedWorker : public BaseDeviceWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트  
     * @param influx_client InfluxDB 클라이언트
     */
    TcpBasedWorker(const Drivers::DeviceInfo& device_info,
                   std::shared_ptr<RedisClient> redis_client,
                   std::shared_ptr<InfluxClient> influx_client);
    
    /**
     * @brief 소멸자
     */
    virtual ~TcpBasedWorker();

    // =============================================================================
    // TCP 특화 설정 (BaseDeviceWorker에 없는 기능만)
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
     * @return JSON 형태의 TCP 연결 정보
     */
    std::string GetTcpConnectionInfo() const;
    
    /**
     * @brief TCP 연결 품질 평가 (0-100점)
     * @return 연결 품질 점수
     */
    int GetTcpConnectionQuality() const;

    // =============================================================================
    // BaseDeviceWorker 순수 가상 함수 구현 (TCP 공통 로직)
    // =============================================================================
    
    /**
     * @brief TCP 소켓 연결 수립
     * @return 성공 시 true
     */
    bool EstablishConnection() override;
    
    /**
     * @brief TCP 소켓 연결 해제
     * @return 성공 시 true
     */
    bool CloseConnection() override;
    
    /**
     * @brief TCP 소켓 연결 상태 확인
     * @return 연결된 상태면 true
     */
    bool CheckConnection() override;
    
    /**
     * @brief TCP Keep-alive 전송 (기본 구현)
     * @return 성공 시 true
     */
    bool SendKeepAlive() override;

protected:
    // =============================================================================
    // 파생 클래스에서 구현해야 하는 프로토콜별 메서드들
    // =============================================================================
    
    /**
     * @brief 프로토콜별 연결 수립 (TCP 연결 후 호출됨)
     * @return 성공 시 true
     */
    virtual bool EstablishProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 해제 (TCP 해제 전 호출됨)
     */
    virtual void CloseProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 상태 확인
     * @return 프로토콜 연결 상태
     */
    virtual bool CheckProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 Keep-alive 전송 (선택사항)
     * @return 성공 시 true (기본 구현: true 반환)
     */
    virtual bool SendProtocolKeepAlive() { return true; }

    // =============================================================================
    // TCP 전용 멤버 변수들
    // =============================================================================
    
    std::string ip_address_;                    ///< IP 주소
    uint16_t port_{0};                         ///< 포트 번호
    int connection_timeout_seconds_{5};        ///< TCP 연결 타임아웃 (초)
    int socket_fd_{-1};                        ///< TCP 소켓 파일 디스크립터

private:
    // =============================================================================
    // 내부 도우미 메서드들
    // =============================================================================
    
    /**
     * @brief TCP 연결 설정 유효성 검사
     * @return 유효하면 true
     */
    bool ValidateTcpConfig() const;
    
    /**
     * @brief TCP 소켓 생성 및 연결
     * @return 성공 시 true
     */
    bool CreateTcpSocket();
    
    /**
     * @brief TCP 소켓 종료
     */
    void CloseTcpSocket();
    
    /**
     * @brief TCP 연결 정보 포맷팅
     * @return "IP:Port" 형태 문자열
     */
    std::string FormatTcpEndpoint() const;
    
    /**
     * @brief TCP 소켓 상태 확인
     * @return 소켓이 연결된 상태면 true
     */
    bool IsTcpSocketConnected() const;
    
    /**
     * @brief TCP 연결 활동 로깅
     * @param activity 활동 내용
     * @param level 로그 레벨
     */
    void LogTcpActivity(const std::string& activity, LogLevel level = LogLevel::INFO);
};

} // namespace Workers  
} // namespace PulseOne

#endif // WORKERS_BASE_TCP_BASED_WORKER_H