// =============================================================================
// collector/include/Api/LogApiCallbacks.h
// 로그 조회 및 관리 REST API 콜백 설정
// =============================================================================

#ifndef API_LOG_CALLBACKS_H
#define API_LOG_CALLBACKS_H

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
 * @brief 로그 조회 관련 REST API 콜백 설정 클래스
 */
class LogApiCallbacks {
public:
    /**
     * @brief RestApiServer에 로그 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     */
    static void Setup(Network::RestApiServer* server);

private:
    LogApiCallbacks() = delete;

    /**
     * @brief 로그 파일 목록 조회
     * @param path 상대 경로 (empty, "packets", "20260107" 등)
     * @return 로그 파일 목록 JSON
     */
    static nlohmann::json ListLogFiles(const std::string& path);

    /**
     * @brief 로그 파일 내용 조회
     * @param path 파일 상대 경로 (e.g., "20260107/system.log")
     * @param lines 읽을 라인 수
     * @param offset 시작 오프셋
     * @return 로그 내용 문자열
     */
    static std::string ReadLogFile(const std::string& path, 
                                 int lines, int offset);
};

} // namespace Api
} // namespace PulseOne

#endif // API_LOG_CALLBACKS_H
