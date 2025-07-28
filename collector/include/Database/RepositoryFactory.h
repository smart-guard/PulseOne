#ifndef PULSEONE_REPOSITORY_FACTORY_H
#define PULSEONE_REPOSITORY_FACTORY_H

/**
 * @file RepositoryFactory.h
 * @brief PulseOne Repository 팩토리 (싱글톤) - 모든 Repository 통합 관리
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 완전한 Repository 생태계:
 * - DeviceRepository, DataPointRepository (기존 완료)
 * - UserRepository, TenantRepository, AlarmConfigRepository (신규 추가)
 * - IRepository 기반 통합 캐시 시스템
 */

#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/UserRepository.h"
#include "Database/Repositories/TenantRepository.h"
#include "Database/Repositories/AlarmConfigRepository.h"

#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <mutex>
#include <functional>
#include <map>

namespace PulseOne {
namespace Database {

// 🔥 타입 별칭 정의 (Repositories 네임스페이스 해결)
using DeviceRepository = PulseOne::Database::Repositories::DeviceRepository;
using DataPointRepository = PulseOne::Database::Repositories::DataPointRepository;
using UserRepository = PulseOne::Database::Repositories::UserRepository;
using TenantRepository = PulseOne::Database::Repositories::TenantRepository;
using AlarmConfigRepository = PulseOne::Database::Repositories::AlarmConfigRepository;

/**
 * @brief Repository 팩토리 (싱글톤)
 * @details 모든 Repository 인스턴스를 중앙에서 관리하며 통합 캐시 시스템 제공
 */
class RepositoryFactory {
public:
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    
    /**
     * @brief 싱글톤 인스턴스 조회
     * @return RepositoryFactory 인스턴스
     */
    static RepositoryFactory& getInstance();
    
    /**
     * @brief 팩토리 초기화
     * @return 성공 시 true
     */
    bool initialize();
    
    /**
     * @brief 팩토리 종료 및 리소스 정리
     */
    void shutdown();

    // =======================================================================
    // Repository 인스턴스 조회 (완전한 라인업)
    // =======================================================================
    
    /**
     * @brief DeviceRepository 인스턴스 조회
     * @return DeviceRepository 참조
     */
    DeviceRepository& getDeviceRepository();
    
    /**
     * @brief DataPointRepository 인스턴스 조회
     * @return DataPointRepository 참조
     */
    DataPointRepository& getDataPointRepository();   
    
    /**
     * @brief UserRepository 인스턴스 조회
     * @return UserRepository 참조
     */
    UserRepository& getUserRepository();
    
    /**
     * @brief TenantRepository 인스턴스 조회
     * @return TenantRepository 참조
     */
    TenantRepository& getTenantRepository();
    
    /**
     * @brief AlarmConfigRepository 인스턴스 조회
     * @return AlarmConfigRepository 참조
     */
    AlarmConfigRepository& getAlarmConfigRepository();

    // =======================================================================
    // 전역 캐싱 제어 (IRepository 통합 관리)
    // =======================================================================
    
    /**
     * @brief 모든 Repository 캐시 활성화/비활성화
     * @param enabled 캐시 사용 여부
     */
    void setCacheEnabled(bool enabled);
    
    /**
     * @brief 모든 Repository 캐시 삭제
     */
    void clearAllCaches();
    
    /**
     * @brief 전체 캐시 통계 조회
     * @return Repository별 캐시 통계
     */
    std::map<std::string, std::map<std::string, int>> getAllCacheStats();
    
    /**
     * @brief 전체 캐시 메모리 사용량 조회
     * @return 총 캐시 메모리 사용량 (bytes)
     */
    size_t getTotalCacheMemoryUsage();
    
    /**
     * @brief 캐시 히트율 조회
     * @return Repository별 캐시 히트율
     */
    std::map<std::string, double> getCacheHitRates();

    // =======================================================================
    // 성능 모니터링 및 분석
    // =======================================================================
    
    /**
     * @brief Repository별 성능 통계 조회
     * @return 성능 지표 맵
     */
    std::map<std::string, std::map<std::string, double>> getPerformanceStats();
    
    /**
     * @brief 전체 Repository 상태 확인
     * @return 상태 정보 맵
     */
    std::map<std::string, std::string> getRepositoryHealthStatus();
    
    /**
     * @brief 캐시 최적화 실행
     * @param max_memory_mb 최대 메모리 사용량 (MB)
     * @return 최적화된 Repository 수
     */
    int optimizeCaches(int max_memory_mb = 100);

    // =======================================================================
    // 배치 작업 지원
    // =======================================================================
    
    /**
     * @brief 모든 Repository 백업
     * @param backup_path 백업 파일 경로
     * @return 성공 시 true
     */
    bool backupAllRepositories(const std::string& backup_path);
    
    /**
     * @brief Repository 데이터 무결성 검사
     * @return 문제가 발견된 Repository 목록
     */
    std::vector<std::string> validateDataIntegrity();
    
    /**
     * @brief 트랜잭션 기반 배치 작업 시작
     * @return 트랜잭션 ID
     */
    std::string beginBatchTransaction();
    
    /**
     * @brief 배치 트랜잭션 커밋
     * @param transaction_id 트랜잭션 ID
     * @return 성공 시 true
     */
    bool commitBatchTransaction(const std::string& transaction_id);
    
    /**
     * @brief 배치 트랜잭션 롤백
     * @param transaction_id 트랜잭션 ID
     * @return 성공 시 true
     */
    bool rollbackBatchTransaction(const std::string& transaction_id);

    // =======================================================================
    // 이벤트 및 알림 시스템
    // =======================================================================
    
    /**
     * @brief Repository 이벤트 리스너 등록
     * @param event_type 이벤트 타입 (CREATE, UPDATE, DELETE)
     * @param callback 콜백 함수
     */
    void addEventListener(const std::string& event_type, 
                         std::function<void(const std::string&, int)> callback);
    
    /**
     * @brief 캐시 이벤트 리스너 등록
     * @param callback 캐시 이벤트 콜백
     */
    void addCacheEventListener(std::function<void(const std::string&, const std::string&)> callback);

    // =======================================================================
    // 디버깅 및 개발 지원
    // =======================================================================
    
    /**
     * @brief Repository 상세 정보 덤프
     * @return Repository 정보 문자열
     */
    std::string dumpRepositoryInfo();
    
    /**
     * @brief SQL 쿼리 로깅 활성화/비활성화
     * @param enabled 로깅 활성화 여부
     */
    void setQueryLoggingEnabled(bool enabled);
    
    /**
     * @brief 느린 쿼리 감지 임계값 설정
     * @param threshold_ms 임계값 (밀리초)
     */
    void setSlowQueryThreshold(int threshold_ms);

    // =======================================================================
    // 복사 및 이동 제한 (싱글톤)
    // =======================================================================
    
    RepositoryFactory(const RepositoryFactory&) = delete;
    RepositoryFactory& operator=(const RepositoryFactory&) = delete;
    RepositoryFactory(RepositoryFactory&&) = delete;
    RepositoryFactory& operator=(RepositoryFactory&&) = delete;

private:
    // =======================================================================
    // 생성자 및 소멸자 (private)
    // =======================================================================
    
    RepositoryFactory();
    ~RepositoryFactory();

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 기본 시스템 컴포넌트들
    std::unique_ptr<LogManager> logger_;
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<DatabaseManager> db_manager_;
    
    // Repository 인스턴스들
    std::unique_ptr<DeviceRepository> device_repository_;
    std::unique_ptr<DataPointRepository> data_point_repository_;
    std::unique_ptr<UserRepository> user_repository_;
    std::unique_ptr<TenantRepository> tenant_repository_;
    std::unique_ptr<AlarmConfigRepository> alarm_config_repository_;
    
    // 동기화 및 상태 관리
    mutable std::mutex factory_mutex_;
    bool initialized_;
    bool cache_enabled_;
    
    // 성능 모니터링
    std::map<std::string, std::chrono::high_resolution_clock::time_point> performance_timers_;
    std::map<std::string, std::vector<double>> query_times_;
    
    // 이벤트 시스템
    std::map<std::string, std::vector<std::function<void(const std::string&, int)>>> event_listeners_;
    std::vector<std::function<void(const std::string&, const std::string&)>> cache_event_listeners_;
    
    // 배치 트랜잭션 관리
    std::map<std::string, std::vector<std::string>> active_transactions_;
    
    // 디버깅 및 로깅
    bool query_logging_enabled_;
    int slow_query_threshold_ms_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief Repository 초기화
     * @return 성공 시 true
     */
    bool initializeRepositories();
    
    /**
     * @brief 캐시 통계 수집
     * @param repo_name Repository 이름
     * @return 캐시 통계
     */
    std::map<std::string, int> collectCacheStats(const std::string& repo_name);
    
    /**
     * @brief 성능 타이머 시작
     * @param operation_name 작업 이름
     */
    void startPerformanceTimer(const std::string& operation_name);
    
    /**
     * @brief 성능 타이머 종료
     * @param operation_name 작업 이름
     * @return 경과 시간 (밀리초)
     */
    double endPerformanceTimer(const std::string& operation_name);
    
    /**
     * @brief Repository 이벤트 트리거
     * @param event_type 이벤트 타입
     * @param repo_name Repository 이름
     * @param entity_id 엔티티 ID
     */
    void triggerRepositoryEvent(const std::string& event_type, 
                               const std::string& repo_name, 
                               int entity_id);
    
    /**
     * @brief 캐시 이벤트 트리거
     * @param event_type 이벤트 타입
     * @param details 상세 정보
     */
    void triggerCacheEvent(const std::string& event_type, const std::string& details);
    
    /**
     * @brief 쿼리 성능 로깅
     * @param repo_name Repository 이름
     * @param query SQL 쿼리
     * @param execution_time_ms 실행 시간 (밀리초)
     */
    void logQueryPerformance(const std::string& repo_name, 
                            const std::string& query, 
                            double execution_time_ms);
    
    /**
     * @brief 메모리 정리
     */
    void cleanupResources();
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_REPOSITORY_FACTORY_H