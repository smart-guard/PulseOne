// =============================================================================
// collector/include/Drivers/DriverFactory.h
// 드라이버 팩토리 클래스 (IProtocolDriver.h와 호환)
// =============================================================================

#ifndef PULSEONE_DRIVERS_DRIVER_FACTORY_H
#define PULSEONE_DRIVERS_DRIVER_FACTORY_H

#include "IProtocolDriver.h"
#include <functional>
#include <mutex>
#include <memory>
#include <map>
#include <vector>

namespace PulseOne {
namespace Drivers {

/**
 * @brief 드라이버 생성자 함수 타입 (IProtocolDriver.h와 동일)
 */
using DriverCreator = std::function<std::unique_ptr<IProtocolDriver>()>;

/**
 * @brief 드라이버 팩토리 클래스
 * 
 * 싱글턴 패턴으로 구현된 드라이버 팩토리입니다.
 * 다양한 프로토콜 드라이버를 등록하고 생성할 수 있습니다.
 */
class DriverFactory {
public:
    /**
     * @brief 팩토리 인스턴스 반환
     * @return 팩토리 인스턴스 참조
     */
    static DriverFactory& GetInstance();
    
    /**
     * @brief 드라이버 등록
     * @param protocol 프로토콜 타입
     * @param creator 드라이버 생성자 함수
     * @return 성공 시 true
     */
    virtual bool RegisterDriver(ProtocolType protocol, DriverCreator creator);
    
    /**
     * @brief 드라이버 등록 해제
     * @param protocol 프로토콜 타입
     * @return 성공 시 true
     */
    virtual bool UnregisterDriver(ProtocolType protocol);

    /**
     * @brief 드라이버 등록 (문자열 기반 - 플러그인용)
     */
    virtual bool RegisterDriver(const std::string& protocol_name, DriverCreator creator);

    /**
     * @brief 드라이버 등록 해제 (문자열 기반)
     */
    virtual bool UnregisterDriver(const std::string& protocol_name);
    
    /**
     * @brief 드라이버 생성
     * @param protocol 프로토콜 타입
     * @return 드라이버 인스턴스 (실패 시 nullptr)
     */
    virtual std::unique_ptr<IProtocolDriver> CreateDriver(ProtocolType protocol);

    /**
     * @brief 드라이버 생성 (문자열 기반)
     */
    virtual std::unique_ptr<IProtocolDriver> CreateDriver(const std::string& protocol_name);
    
    /**
     * @brief 등록된 프로토콜 목록 반환
     * @return 프로토콜 타입 벡터
     */
    virtual std::vector<ProtocolType> GetAvailableProtocols() const;
    
    /**
     * @brief 프로토콜 지원 여부 확인
     * @param protocol 프로토콜 타입
     * @return 지원 시 true
     */
    virtual bool IsProtocolSupported(ProtocolType protocol) const;
    
    /**
     * @brief 등록된 드라이버 수 반환
     * @return 드라이버 수
     */
    virtual size_t GetDriverCount() const;
    
    /**
     * @brief 가상 소멸자 (상속 가능하도록)
     */
    virtual ~DriverFactory();

protected:
    /**
     * @brief 생성자 (보호된 접근)
     */
    DriverFactory();
    
    // 복사 및 이동 방지
    DriverFactory(const DriverFactory&) = delete;
    DriverFactory& operator=(const DriverFactory&) = delete;
    DriverFactory(DriverFactory&&) = delete;
    DriverFactory& operator=(DriverFactory&&) = delete;
    
    mutable std::mutex mutex_;
    std::map<ProtocolType, DriverCreator> creators_;
    std::map<std::string, DriverCreator> custom_creators_;
};

// =============================================================================
// 편의를 위한 매크로
// =============================================================================

/**
 * @brief 드라이버 자동 등록 매크로
 * 
 * 드라이버 구현부에서 이 매크로를 사용하면 프로그램 시작 시
 * 자동으로 드라이버가 팩토리에 등록됩니다.
 */
#define REGISTER_DRIVER(protocol_type, driver_class) \
    namespace { \
        class driver_class##Registrar { \
        public: \
            driver_class##Registrar() { \
                DriverFactory::GetInstance().RegisterDriver( \
                    protocol_type, \
                    []() -> std::unique_ptr<IProtocolDriver> { \
                        return std::make_unique<driver_class>(); \
                    } \
                ); \
            } \
        }; \
        static driver_class##Registrar g_##driver_class##_registrar; \
    }

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_DRIVER_FACTORY_H