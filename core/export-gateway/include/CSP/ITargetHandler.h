/**
 * @file ITargetHandler.h
 * @brief 타겟 핸들러 공통 인터페이스 정의
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 1.0.0
 * 저장 위치: core/export-gateway/include/CSP/ITargetHandler.h
 * 
 * 모든 타겟 핸들러가 구현해야 하는 공통 인터페이스
 * - HTTP, S3, File, MQTT 등 모든 타겟 타입의 부모 클래스
 */

#ifndef ITARGET_HANDLER_H
#define ITARGET_HANDLER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "AlarmMessage.h"

using json = nlohmann::json;

namespace PulseOne {
namespace CSP {

// =============================================================================
// 전방 선언 (순환 참조 방지)
// =============================================================================
struct TargetSendResult;  // CSPDynamicTargets.h에 정의됨

// =============================================================================
// 타겟 핸들러 공통 인터페이스
// =============================================================================

/**
 * @brief 타겟 핸들러 공통 인터페이스
 * 
 * 모든 구체적인 핸들러(HttpTargetHandler, S3TargetHandler 등)가
 * 이 인터페이스를 구현해야 합니다.
 * 
 * 핵심 메서드:
 * - sendAlarm(): 알람 메시지를 타겟으로 전송
 * - testConnection(): 타겟 연결 테스트
 * - validateConfig(): 설정 유효성 검증
 * - getHandlerType(): 핸들러 타입 반환 (HTTP, S3, FILE, MQTT 등)
 */
class ITargetHandler {
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~ITargetHandler() = default;
    
    // =======================================================================
    // 필수 구현 메서드 (pure virtual)
    // =======================================================================
    
    /**
     * @brief 알람 메시지를 타겟으로 전송
     * @param alarm 전송할 알람 메시지
     * @param config 타겟별 설정 JSON
     * @return 전송 결과 (성공/실패, 응답 시간 등)
     */
    virtual TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) = 0;
    
    /**
     * @brief 타겟 연결 테스트
     * @param config 타겟별 설정 JSON
     * @return 연결 성공 여부
     */
    virtual bool testConnection(const json& config) = 0;
    
    /**
     * @brief 핸들러 타입 반환
     * @return 핸들러 타입 문자열 (예: "HTTP", "S3", "FILE", "MQTT")
     */
    virtual std::string getHandlerType() const = 0;
    
    /**
     * @brief 설정 유효성 검증
     * @param config 검증할 설정 JSON
     * @param errors 에러 메시지 목록 (출력 파라미터)
     * @return 설정이 유효하면 true
     */
    virtual bool validateConfig(const json& config, std::vector<std::string>& errors) = 0;
    
    // =======================================================================
    // 선택적 구현 메서드 (기본 구현 제공)
    // =======================================================================
    
    /**
     * @brief 핸들러 초기화
     * @param config 초기화 설정 JSON
     * @return 초기화 성공 여부
     * 
     * 기본 구현: true 반환
     * 필요시 자식 클래스에서 오버라이드
     */
    virtual bool initialize(const json& /*config*/) { 
        return true; 
    }
    
    /**
     * @brief 핸들러 정리 (리소스 해제)
     * 
     * 기본 구현: 빈 구현
     * 필요시 자식 클래스에서 오버라이드하여
     * 네트워크 연결 종료, 파일 닫기 등 수행
     */
    virtual void cleanup() { 
        // 기본: 아무 작업 없음
    }
    
    /**
     * @brief 핸들러 상태 반환
     * @return 핸들러 상태 JSON
     * 
     * 기본 구현: 타입과 기본 상태만 반환
     * 필요시 자식 클래스에서 오버라이드하여
     * 상세 통계 정보 추가
     */
    virtual json getStatus() const {
        return json{
            {"type", getHandlerType()},
            {"status", "active"}
        };
    }
    
    // =======================================================================
    // 유틸리티 메서드 (보호됨)
    // =======================================================================
    
protected:
    /**
     * @brief 설정에서 문자열 값 안전하게 추출
     * @param config 설정 JSON
     * @param key 키 이름
     * @param default_value 기본값 (키가 없을 경우)
     * @return 추출된 문자열
     */
    std::string getConfigString(const json& config, const std::string& key, 
                               const std::string& default_value = "") const {
        if (config.contains(key) && config[key].is_string()) {
            return config[key].get<std::string>();
        }
        return default_value;
    }
    
    /**
     * @brief 설정에서 정수 값 안전하게 추출
     * @param config 설정 JSON
     * @param key 키 이름
     * @param default_value 기본값 (키가 없을 경우)
     * @return 추출된 정수
     */
    int getConfigInt(const json& config, const std::string& key, int default_value = 0) const {
        if (config.contains(key) && config[key].is_number_integer()) {
            return config[key].get<int>();
        }
        return default_value;
    }
    
    /**
     * @brief 설정에서 불린 값 안전하게 추출
     * @param config 설정 JSON
     * @param key 키 이름
     * @param default_value 기본값 (키가 없을 경우)
     * @return 추출된 불린값
     */
    bool getConfigBool(const json& config, const std::string& key, bool default_value = false) const {
        if (config.contains(key) && config[key].is_boolean()) {
            return config[key].get<bool>();
        }
        return default_value;
    }
};

} // namespace CSP
} // namespace PulseOne

#endif // ITARGET_HANDLER_H