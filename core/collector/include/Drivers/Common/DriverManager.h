// collector/include/Drivers/DriverManager.h
// PulseOne 드라이버 매니저 - 중앙집중식 드라이버 관리

#ifndef DRIVER_MANAGER_H
#define DRIVER_MANAGER_H

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "Logging/LogManager.h"
#include "Config/ConfigManager.h"

#ifdef HAVE_DRIVER_SYSTEM
#include "Drivers/IProtocolDriver.h"
#include "Drivers/DriverFactory.h"
#include "Drivers/CommonTypes.h"
#endif

namespace PulseOne {
namespace Drivers {

/**
 * @brief 드라이버 매니저 - 모든 프로토콜 드라이버 관리
 */
class DriverManager {
public:
    DriverManager(std::shared_ptr<LogManager> logger, 
                 std::shared_ptr<ConfigManager> config);
    ~DriverManager();

    /**
     * @brief 드라이버 매니저 초기화
     * @return 성공 시 true
     */
    bool Initialize();

    /**
     * @brief 설정 파일에서 드라이버들 로드
     * @param config_path 드라이버 설정 파일 경로
     * @return 성공 시 true
     */
    bool LoadDriversFromConfig(const std::string& config_path);

    /**
     * @brief 모든 드라이버 시작
     * @return 성공 시 true
     */
    bool StartAllDrivers();

    /**
     * @brief 모든 드라이버 중지
     */
    void StopAllDrivers();

    /**
     * @brief 드라이버 상태 확인
     * @return 활성화된 드라이버 수
     */
    size_t GetActiveDriverCount() const;

    /**
     * @brief 드라이버 통계 정보
     * @return 통계 문자열
     */
    std::string GetStatistics() const;

    /**
     * @brief 정리
     */
    void Cleanup();

private:
    std::shared_ptr<LogManager> logger_;
    std::shared_ptr<ConfigManager> config_;

#ifdef HAVE_DRIVER_SYSTEM
    std::vector<std::unique_ptr<IProtocolDriver>> active_drivers_;
    std::unordered_map<std::string, size_t> driver_statistics_;
#endif

    bool is_initialized_;
    bool is_running_;

    // 내부 헬퍼 함수들
    bool ValidateConfiguration() const;
    void UpdateStatistics();
};

} // namespace Drivers
} // namespace PulseOne

#endif // DRIVER_MANAGER_H