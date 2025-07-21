// collector/include/Database/DataAccessManager.h (단순화된 버전)
// 기존 DatabaseManager와 호환되는 경량 버전

#ifndef DATAACCESS_MANAGER_H
#define DATAACCESS_MANAGER_H

#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>
#include "IDataAccess.h"
#include "Utils/LogManager.h"
#include "Config/ConfigManager.h"

namespace PulseOne {
namespace Database {

/**
 * @brief 경량 데이터 액세스 관리자 (기존 DatabaseManager 활용)
 */
class DataAccessManager {
public:
    // 싱글턴 인스턴스 접근
    static DataAccessManager& GetInstance();
    
    // 복사/이동 생성자 삭제 (싱글턴 패턴)
    DataAccessManager(const DataAccessManager&) = delete;
    DataAccessManager& operator=(const DataAccessManager&) = delete;
    
    /**
     * @brief 초기화 (의존성 주입)
     * @param logger 로그 매니저
     * @param config 설정 매니저
     * @return 성공 시 true
     */
    bool Initialize(std::shared_ptr<LogManager> logger, 
                   std::shared_ptr<ConfigManager> config);
    
    /**
     * @brief 정리 및 리소스 해제
     */
    void Cleanup();
    
    /**
     * @brief 도메인별 데이터 액세스 가져오기
     * @tparam T 반환할 데이터 액세스 타입
     * @return 데이터 액세스 인스턴스 (캐스팅됨)
     */
    template<typename T>
    std::shared_ptr<T> GetDomain() {
        static_assert(std::is_base_of_v<IDataAccess, T>, 
                     "T must inherit from IDataAccess");
        
        std::string domain_name = T::GetStaticDomainName();
        auto base_access = GetDomainByName(domain_name);
        
        if (!base_access) {
            return nullptr;
        }
        
        return std::dynamic_pointer_cast<T>(base_access);
    }
    
    /**
     * @brief 도메인 이름으로 데이터 액세스 가져오기
     * @param domain_name 도메인 이름
     * @return 베이스 데이터 액세스 인스턴스
     */
    std::shared_ptr<IDataAccess> GetDomainByName(const std::string& domain_name);
    
    /**
     * @brief 팩토리 등록 (플러그인 시스템)
     * @param domain_name 도메인 이름
     * @param factory 팩토리 인스턴스
     * @return 성공 시 true
     */
    static bool RegisterFactory(const std::string& domain_name,
                               std::unique_ptr<IDataAccessFactory> factory);
    
    /**
     * @brief 전체 헬스 체크
     * @return 모든 도메인이 정상이면 true
     */
    bool HealthCheckAll();

private:
    // private 생성자 (싱글턴)
    DataAccessManager() = default;
    ~DataAccessManager() = default;
    
    // 의존성
    std::shared_ptr<LogManager> logger_;
    std::shared_ptr<ConfigManager> config_;
    
    // 인스턴스 캐시 (스레드 안전)
    mutable std::mutex instances_mutex_;
    std::unordered_map<std::string, std::shared_ptr<IDataAccess>> instances_;
    
    // 팩토리 레지스트리 (static - 컴파일 타임에 등록됨)
    static std::unordered_map<std::string, std::unique_ptr<IDataAccessFactory>>& 
    GetFactoryRegistry();
    
    /**
     * @brief 도메인 인스턴스 생성 (내부용)
     */
    std::shared_ptr<IDataAccess> CreateDomainInstance(const std::string& domain_name);
    
    // 초기화 상태
    bool initialized_ = false;
    mutable std::mutex init_mutex_;
};

} // namespace Database
} // namespace PulseOne

#endif // DATAACCESS_MANAGER_H