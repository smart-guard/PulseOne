// =============================================================================
// collector/include/Api/ConfigApiCallbacks.h
// 설정 관련 REST API 콜백 설정
// =============================================================================

#ifndef API_CONFIG_CALLBACKS_H
#define API_CONFIG_CALLBACKS_H

#include <string>

// 전방 선언
namespace PulseOne {
namespace Network {
class RestApiServer;
}
}

class ConfigManager;
class LogManager;

namespace PulseOne {
namespace Api {

/**
 * @brief 설정 관련 REST API 콜백 설정 클래스
 */
class ConfigApiCallbacks {
public:
    /**
     * @brief RestApiServer에 설정 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     * @param config_mgr ConfigManager 인스턴스
     * @param logger LogManager 인스턴스
     */
    static void Setup(Network::RestApiServer* server, 
                     ConfigManager* config_mgr, 
                     LogManager* logger);

private:
    ConfigApiCallbacks() = delete;  // 정적 클래스
};

} // namespace Api
} // namespace PulseOne

#endif // API_CONFIG_CALLBACKS_H