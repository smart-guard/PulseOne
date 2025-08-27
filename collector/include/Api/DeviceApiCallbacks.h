// =============================================================================
// collector/include/Api/DeviceApiCallbacks.h
// 디바이스 제어 관련 REST API 콜백 설정
// =============================================================================

#ifndef API_DEVICE_CALLBACKS_H
#define API_DEVICE_CALLBACKS_H

#include <string>

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
 * @brief 디바이스 제어 관련 REST API 콜백 설정 클래스
 */
class DeviceApiCallbacks {
public:
    /**
     * @brief RestApiServer에 디바이스 제어 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     * @param worker_factory WorkerFactory 인스턴스
     * @param logger LogManager 인스턴스
     */
    static void Setup(Network::RestApiServer* server,
                     Workers::WorkerFactory* worker_factory,
                     LogManager* logger);

private:
    DeviceApiCallbacks() = delete;  // 정적 클래스
};

} // namespace Api
} // namespace PulseOne

#endif // API_DEVICE_CALLBACKS_H