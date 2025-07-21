// collector/include/Database/IDataAccess.h
// PulseOne 데이터 액세스 베이스 인터페이스

#ifndef IDATAACCESS_H
#define IDATAACCESS_H

#include <string>
#include <memory>
#include <vector>
#include <map>
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

namespace PulseOne {
namespace Database {

// =============================================================================
// 데이터 액세스 결과 코드
// =============================================================================
enum class DataAccessResult {
    SUCCESS = 0,
    ERROR_CONNECTION = -1,
    ERROR_QUERY = -2,
    ERROR_TRANSACTION = -3,
    ERROR_VALIDATION = -4,
    ERROR_NOT_FOUND = -5,
    ERROR_PERMISSION = -6,
    ERROR_TIMEOUT = -7
};

// =============================================================================
// 트랜잭션 스코프 관리
// =============================================================================
class TransactionScope {
public:
    virtual ~TransactionScope() = default;
    virtual bool Commit() = 0;
    virtual bool Rollback() = 0;
    virtual bool IsActive() const = 0;
};

// =============================================================================
// 데이터 액세스 베이스 인터페이스
// =============================================================================
class IDataAccess {
public:
    virtual ~IDataAccess() = default;
    
    /**
     * @brief 도메인 이름 반환 (DeviceDataAccess, UserDataAccess 등)
     * @return 도메인 이름 문자열
     */
    virtual std::string GetDomainName() const = 0;
    
    /**
     * @brief 초기화 (DB 연결, 스키마 검증 등)
     * @return 성공 시 true
     */
    virtual bool Initialize() = 0;
    
    /**
     * @brief 정리 (리소스 해제)
     */
    virtual void Cleanup() = 0;
    
    /**
     * @brief 연결 상태 확인
     * @return 연결 상태
     */
    virtual bool IsConnected() const = 0;
    
    /**
     * @brief 헬스 체크 (연결 테스트)
     * @return 정상 시 true
     */
    virtual bool HealthCheck() = 0;
    
    /**
     * @brief 트랜잭션 시작
     * @return 트랜잭션 스코프 객체
     */
    virtual std::unique_ptr<TransactionScope> BeginTransaction() = 0;
    
protected:
    // 공통 의존성 - 상속 클래스에서 사용
    std::shared_ptr<LogManager> logger_;
    std::shared_ptr<ConfigManager> config_;
    
    // 생성자 (protected - 직접 인스턴스화 방지)
    IDataAccess(std::shared_ptr<LogManager> logger, 
                std::shared_ptr<ConfigManager> config)
        : logger_(logger), config_(config) {}
    
    /**
     * @brief 마지막 에러 메시지 저장
     */
    mutable std::string last_error_;
    
    /**
     * @brief 에러 설정 헬퍼
     */
    void SetError(const std::string& error_message) const {
        last_error_ = error_message;
        if (logger_) {
            logger_->Error("[" + GetDomainName() + "] " + error_message);
        }
    }
    
public:
    /**
     * @brief 마지막 에러 메시지 반환
     */
    virtual std::string GetLastError() const {
        return last_error_;
    }
};

// =============================================================================
// 도메인별 플러그인 팩토리 인터페이스  
// =============================================================================
class IDataAccessFactory {
public:
    virtual ~IDataAccessFactory() = default;
    
    /**
     * @brief 데이터 액세스 인스턴스 생성
     * @param logger 로그 매니저
     * @param config 설정 매니저
     * @return 데이터 액세스 인스턴스
     */
    virtual std::unique_ptr<IDataAccess> Create(
        std::shared_ptr<LogManager> logger,
        std::shared_ptr<ConfigManager> config) = 0;
        
    /**
     * @brief 지원하는 도메인 이름 반환
     */
    virtual std::string GetDomainName() const = 0;
};

// =============================================================================
// 팩토리 등록 매크로
// =============================================================================
#define REGISTER_DATA_ACCESS_FACTORY(domain_name, factory_class) \
    namespace { \
        static bool registered_##factory_class = \
            DataAccessManager::RegisterFactory(domain_name, \
                std::make_unique<factory_class>()); \
    }

} // namespace Database
} // namespace PulseOne

#endif // IDATAACCESS_H