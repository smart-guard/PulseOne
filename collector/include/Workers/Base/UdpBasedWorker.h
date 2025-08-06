/**
 * @file UdpBasedWorker.h
 * @brief UDP 기반 디바이스 워커 클래스
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 * 
 * @details
 * BaseDeviceWorker를 상속받아 UDP 통신에 특화된 기능을 제공합니다.
 * BACnet/IP, DNP3 over UDP 등 UDP 기반 프로토콜에서 상속받아 사용합니다.
 */

#ifndef UDP_BASED_WORKER_H
#define UDP_BASED_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>

namespace PulseOne {
namespace Workers {

/**
 * @brief UDP 설정 구조체
 */
struct UdpConfig {
    std::string local_interface = "0.0.0.0";  ///< 로컬 바인딩 인터페이스
    uint16_t local_port = 0;                  ///< 로컬 포트 (0=자동 할당)
    std::string remote_host = "";             ///< 원격 호스트 (P2P 모드용)
    std::string remote_ip = "";               ///< 원격 IP (호환성용) 
    uint16_t remote_port = 0;                 ///< 원격 포트 (P2P 모드용)
    bool broadcast_enabled = true;            ///< 브로드캐스트 허용
    bool multicast_enabled = false;           ///< 멀티캐스트 허용
    std::string multicast_group = "";         ///< 멀티캐스트 그룹 주소
    uint32_t socket_timeout_ms = 5000;        ///< 소켓 타임아웃 (밀리초)
    uint32_t polling_interval_ms = 1000;      ///< 폴링 간격 (밀리초)
    uint32_t max_retries = 3;                 ///< 최대 재시도 횟수
    uint32_t receive_buffer_size = 65536;     ///< 수신 버퍼 크기
    uint32_t send_buffer_size = 65536;        ///< 송신 버퍼 크기
    bool reuse_address = true;                ///< SO_REUSEADDR 옵션
    bool reuse_port = false;                  ///< SO_REUSEPORT 옵션
    uint8_t ttl = 64;                         ///< Time-To-Live 값
};

/**
 * @brief UDP 연결 정보
 */
struct UdpConnectionInfo {
    int socket_fd = -1;                       ///< 소켓 파일 디스크립터
    struct sockaddr_in local_addr;            ///< 로컬 주소
    struct sockaddr_in remote_addr;           ///< 원격 주소 (P2P 모드용)
    bool is_bound = false;                    ///< 바인딩 상태
    bool is_connected = false;                ///< 연결 상태 (P2P 모드용)
    std::chrono::system_clock::time_point last_activity; ///< 마지막 활동 시간
};

/**
 * @brief UDP 통계 정보
 */
struct UdpStatistics {
    std::atomic<uint64_t> packets_sent{0};         ///< 송신 패킷 수
    std::atomic<uint64_t> packets_received{0};     ///< 수신 패킷 수
    std::atomic<uint64_t> bytes_sent{0};           ///< 송신 바이트 수
    std::atomic<uint64_t> bytes_received{0};       ///< 수신 바이트 수
    std::atomic<uint64_t> send_errors{0};          ///< 송신 에러 수
    std::atomic<uint64_t> receive_errors{0};       ///< 수신 에러 수
    std::atomic<uint64_t> timeouts{0};             ///< 타임아웃 수
    std::atomic<uint64_t> broadcast_packets{0};    ///< 브로드캐스트 패킷 수
    std::atomic<uint64_t> multicast_packets{0};    ///< 멀티캐스트 패킷 수
    
    std::chrono::system_clock::time_point start_time;     ///< 통계 시작 시간
    std::chrono::system_clock::time_point last_reset;     ///< 마지막 리셋 시간
};

/**
 * @brief UDP 수신 패킷 정보
 */
struct UdpPacket {
    std::vector<uint8_t> data;                ///< 패킷 데이터
    struct sockaddr_in sender_addr;           ///< 송신자 주소
    std::chrono::system_clock::time_point timestamp; ///< 수신 시간
    size_t data_length;                       ///< 실제 데이터 길이
};

/**
 * @class UdpBasedWorker
 * @brief UDP 통신 기반 디바이스 워커
 * 
 * @details
 * BaseDeviceWorker를 상속받아 UDP 통신에 특화된 기능을 제공합니다.
 * - UDP 소켓 생성 및 관리
 * - 브로드캐스트/멀티캐스트 지원
 * - 비동기 패킷 송수신
 * - 패킷 큐잉 및 처리
 * - P2P 및 서버 모드 지원
 */
class UdpBasedWorker : public BaseDeviceWorker {

public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트
     * @param influx_client InfluxDB 클라이언트
     */
    explicit UdpBasedWorker(const PulseOne::DeviceInfo& device_info);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~UdpBasedWorker();
    
    // =============================================================================
    // UDP 설정 관리 (공개 인터페이스)
    // =============================================================================
    
    /**
     * @brief UDP 설정
     * @param config UDP 설정
     */
    void ConfigureUdp(const UdpConfig& config);
    
    /**
     * @brief UDP 연결 정보 조회
     * @return JSON 형태의 연결 정보
     */
    std::string GetUdpConnectionInfo() const;
    
    /**
     * @brief UDP 통계 정보 조회
     * @return JSON 형태의 UDP 통계 + BaseDeviceWorker 통계
     */
    std::string GetUdpStats() const;
    
    /**
     * @brief UDP 통계 리셋
     */
    void ResetUdpStats();

    // =============================================================================
    // BaseDeviceWorker 순수 가상 함수 구현 (UDP 특화)
    // =============================================================================
    
    /**
     * @brief UDP 기반 연결 수립
     * @details UDP 소켓 생성 → 바인딩 → 프로토콜 연결 순서로 진행
     * @return 성공 시 true
     */
    bool EstablishConnection() override;
    
    /**
     * @brief UDP 기반 연결 해제
     * @details 프로토콜 해제 → UDP 소켓 해제 순서로 진행
     * @return 성공 시 true
     */
    bool CloseConnection() override;
    
    /**
     * @brief UDP 기반 연결 상태 확인
     * @details UDP 소켓 상태 + 프로토콜 상태 모두 확인
     * @return 연결 상태
     */
    bool CheckConnection() override;
    
    /**
     * @brief UDP 기반 Keep-alive 전송
     * @details UDP 소켓 확인 → 프로토콜별 Keep-alive 호출
     * @return 성공 시 true
     */
    bool SendKeepAlive() override;

protected:
    // =============================================================================
    // 파생 클래스에서 구현해야 하는 프로토콜별 메서드들 (순수 가상)
    // =============================================================================
    
    /**
     * @brief 프로토콜별 연결 수립 (BACnet, DNP3 등에서 구현)
     * @details UDP 소켓이 이미 준비된 상태에서 호출됨
     * @return 성공 시 true
     */
    virtual bool EstablishProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 해제 (BACnet, DNP3 등에서 구현)
     * @details UDP 소켓 해제 전에 호출됨
     * @return 성공 시 true
     */
    virtual bool CloseProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 상태 확인 (BACnet, DNP3 등에서 구현)
     * @return 연결 상태
     */
    virtual bool CheckProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 Keep-alive 전송 (파생 클래스에서 선택적 구현)
     * @return 성공 시 true
     */
    virtual bool SendProtocolKeepAlive() { return true; }
    
    /**
     * @brief 수신된 UDP 패킷 처리 (프로토콜별 구현 필수)
     * @param packet 수신된 패킷
     * @return 성공 시 true
     */
    virtual bool ProcessReceivedPacket(const UdpPacket& packet) = 0;

    // =============================================================================
    // UDP 소켓 관리 (파생 클래스에서 사용 가능)
    // =============================================================================
    
    /**
     * @brief UDP 소켓 생성 및 설정
     * @return 성공 시 true
     */
    bool CreateUdpSocket();
    
    /**
     * @brief UDP 소켓 바인딩
     * @return 성공 시 true
     */
    bool BindUdpSocket();
    
    /**
     * @brief UDP 소켓 해제
     */
    void CloseUdpSocket();
    
    /**
     * @brief 데이터 송신 (유니캐스트)
     * @param data 송신할 데이터
     * @param target_addr 대상 주소
     * @return 송신된 바이트 수 (-1: 실패)
     */
    ssize_t SendUdpData(const std::vector<uint8_t>& data, 
                        const struct sockaddr_in& target_addr);
    
    /**
     * @brief 데이터 송신 (문자열 오버로드)
     * @param data 송신할 데이터
     * @param target_host 대상 호스트
     * @param target_port 대상 포트
     * @return 송신된 바이트 수 (-1: 실패)
     */
    ssize_t SendUdpData(const std::string& data, 
                        const std::string& target_host, uint16_t target_port);
    
    /**
     * @brief 브로드캐스트 데이터 송신
     * @param data 송신할 데이터
     * @param port 대상 포트
     * @return 송신된 바이트 수 (-1: 실패)
     */
    ssize_t SendBroadcast(const std::vector<uint8_t>& data, uint16_t port);
    
    /**
     * @brief 멀티캐스트 데이터 송신
     * @param data 송신할 데이터
     * @param multicast_group 멀티캐스트 그룹
     * @param port 대상 포트
     * @return 송신된 바이트 수 (-1: 실패)
     */
    ssize_t SendMulticast(const std::vector<uint8_t>& data, 
                          const std::string& multicast_group, uint16_t port);
    
    /**
     * @brief 데이터 수신 (논블로킹)
     * @param packet 수신된 패킷 (출력)
     * @param timeout_ms 타임아웃 (밀리초, 0=즉시 반환)
     * @return 성공 시 true
     */
    bool ReceiveUdpData(UdpPacket& packet, uint32_t timeout_ms = 0);
    
    /**
     * @brief 수신 대기 중인 패킷 수 조회
     * @return 대기 중인 패킷 수
     */
    size_t GetPendingPacketCount() const;

    // =============================================================================
    // 유틸리티 메서드 (파생 클래스에서 사용 가능)
    // =============================================================================
    
    /**
     * @brief IP 주소 문자열을 sockaddr_in으로 변환
     * @param ip_str IP 주소 문자열  
     * @param port 포트 번호
     * @param addr 변환된 주소 (출력)
     * @return 성공 시 true
     */
    static bool StringToSockAddr(const std::string& ip_str, uint16_t port, 
                                 struct sockaddr_in& addr);
    
    /**
     * @brief sockaddr_in을 IP 주소 문자열로 변환
     * @param addr 주소    * @return IP 주소 문자열
     */
    static std::string SockAddrToString(const struct sockaddr_in& addr);
    
    /**
     * @brief 브로드캐스트 주소 계산
     * @param interface_ip 인터페이스 IP
     * @param subnet_mask 서브넷 마스크
     * @return 브로드캐스트 주소
     */
    static std::string CalculateBroadcastAddress(const std::string& interface_ip, 
                                                 const std::string& subnet_mask);

    // =============================================================================
    // 멤버 변수 (protected)
    // =============================================================================
    
    /// UDP 설정
    UdpConfig udp_config_;
    
    /// UDP 연결 정보
    UdpConnectionInfo udp_connection_;
    
    /// UDP 통계
    mutable UdpStatistics udp_stats_;
    
    /// 수신 패킷 큐
    std::queue<UdpPacket> receive_queue_;
    
    /// 수신 큐 뮤텍스
    mutable std::mutex receive_queue_mutex_;
    
    /// 수신 스레드
    std::unique_ptr<std::thread> receive_thread_;
    
    /// 수신 스레드 실행 플래그
    std::atomic<bool> receive_thread_running_;

private:
    // =============================================================================
    // 내부 메서드
    // =============================================================================
    
    /**
     * @brief UDP 설정 파싱
     * @details device_info의 config_json에서 UDP 설정 추출
     * @return 성공 시 true
     */
    bool ParseUdpConfig();
    
    /**
     * @brief 소켓 옵션 설정
     * @return 성공 시 true
     */
    bool SetSocketOptions();
    
    /**
     * @brief 수신 스레드 함수
     */
    void ReceiveThreadFunction();
    
    /**
     * @brief 통계 업데이트 (송신)
     * @param bytes_sent 송신된 바이트 수
     * @param is_broadcast 브로드캐스트 여부
     * @param is_multicast 멀티캐스트 여부
     */
    void UpdateSendStats(size_t bytes_sent, bool is_broadcast = false, 
                        bool is_multicast = false);
    
    /**
     * @brief 통계 업데이트 (수신)
     * @param bytes_received 수신된 바이트 수
     * @param is_broadcast 브로드캐스트 여부
     * @param is_multicast 멀티캐스트 여부
     */
    void UpdateReceiveStats(size_t bytes_received, bool is_broadcast = false, 
                           bool is_multicast = false);
    
    /**
     * @brief 에러 통계 업데이트
     * @param is_send_error 송신 에러 여부 (false=수신 에러)
     */
    void UpdateErrorStats(bool is_send_error);
};

} // namespace Workers
} // namespace PulseOne

#endif // UDP_BASED_WORKER_H