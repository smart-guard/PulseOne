// =============================================================================
// collector/include/Api/ConfigApiCallbacks.h
// 설정 관리 REST API 콜백 설정 - 싱글톤 직접 접근 방식
// =============================================================================

#ifndef API_CONFIG_CALLBACKS_H
#define API_CONFIG_CALLBACKS_H

#include <string>

namespace PulseOne {
namespace Network {
    class RestApiServer;
}
}

namespace PulseOne {
namespace Api {

/**
 * @brief 설정 관리 관련 REST API 콜백 설정 클래스
 * 
 * ConfigManager, LogManager, WorkerManager, DbLib::DatabaseManager 등의 
 * 싱글톤들과 직접 연동하여 실제 시스템 재구성을 수행합니다.
 * 
 * 주요 기능:
 * - 설정 파일 리로드 및 유효성 검증
 * - 설정 롤백 (실패시 이전 상태로 복구)
 * - 즉시 적용 가능한 설정들 처리 (로그레벨 등)
 * - 데이터베이스 재연결
 * - 모든 워커에게 설정 변경 통지
 */
class ConfigApiCallbacks {
public:
    /**
     * @brief RestApiServer에 설정 관련 콜백들을 등록
     * @param server RestApiServer 인스턴스
     * 
     * 등록되는 콜백:
     * - 설정 리로드 (실제 시스템 재구성 수행)
     */
    static void Setup(Network::RestApiServer* server);

private:
    ConfigApiCallbacks() = delete;  // 정적 클래스
    
    // ==========================================================================
    // 설정 처리 핵심 로직들
    // ==========================================================================
    
    /**
     * @brief 새로 로드된 설정의 유효성 검증
     * @return 검증 통과시 true
     * 
     * 검증 항목:
     * - 필수 설정 키 존재 여부
     * - 포트 번호 범위 (1-65535)
     * - 로그 레벨 유효성 (DEBUG/INFO/WARNING/ERROR)
     * - 데이터베이스 호스트명 존재 여부
     */
    static bool ValidateReloadedConfig();
    
    /**
     * @brief 즉시 적용 가능한 설정들을 런타임에 적용
     * 
     * 적용 대상:
     * - 로그 레벨 변경 (LogManager.SetLevel)
     * - 스캔 간격 설정
     * - 디버그 모드 활성화/비활성화
     */
    static void ApplyImmediateConfigChanges();
    
    /**
     * @brief 데이터베이스 연결 정보가 변경된 경우 재연결
     * @return 성공시 true
     * 
     * 동작:
     * 1. 새 DB 연결 정보 확인
     * 2. DbLib::DatabaseManager 재연결
     * 3. RepositoryFactory 재초기화
     * 4. Redis 연결도 업데이트 (필요시)
     */
    static bool UpdateDatabaseConnections();
    
    /**
     * @brief 모든 활성 워커들에게 설정 변경 알림
     * 
     * 동작:
     * 1. WorkerManager에서 활성 워커 수 확인
     * 2. 모든 워커의 설정 재로드 신호 전송
     * 3. 재로드 완료 대기 및 상태 확인
     */
    static void NotifyWorkersConfigChanged();
};

} // namespace Api
} // namespace PulseOne

#endif // API_CONFIG_CALLBACKS_H