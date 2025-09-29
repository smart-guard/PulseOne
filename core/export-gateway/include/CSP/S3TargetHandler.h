/**
 * @file S3TargetHandler.h
 * @brief S3 타겟 핸들러 - 통합 타입 사용
 * @author PulseOne Development Team
 * @date 2025-09-24
 * @version 1.1.0 (TargetTypes.h 통합 타입 사용)
 * 저장 위치: core/export-gateway/include/CSP/S3TargetHandler.h
 */

#ifndef S3_TARGET_HANDLER_H
#define S3_TARGET_HANDLER_H

#include "CSPDynamicTargets.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>

// PulseOne S3Client 전방 선언
namespace PulseOne {
namespace Client {
    class S3Client;
    struct S3Config;
    struct S3UploadResult;
}
}

namespace PulseOne {
namespace CSP {

/**
 * @brief S3 타겟 핸들러 클래스
 * 
 * 주요 기능:
 * - AWS S3 업로드
 * - S3 호환 스토리지 업로드 (MinIO, Ceph, Cloudflare R2 등)
 * - 암호화된 자격증명 관리
 * - 객체 키 템플릿 지원
 * - 스토리지 클래스 설정 (STANDARD, STANDARD_IA, GLACIER 등)
 * - 메타데이터 자동 추가
 * - 서버사이드 암호화 (SSE-S3, SSE-KMS)
 * - 멀티파트 업로드 (대용량 파일용)
 * - 재시도 로직 내장
 * - 압축 지원 (gzip)
 */
class S3TargetHandler : public ITargetHandler {
private:
    mutable std::mutex client_mutex_;
    std::atomic<size_t> upload_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    std::atomic<size_t> total_bytes_uploaded_{0};
    
    // S3 클라이언트 및 설정
    std::unique_ptr<PulseOne::Client::S3Client> s3_client_;
    bool compression_enabled_ = false;
    int compression_level_ = 6;
    std::string object_key_template_;
    std::unordered_map<std::string, std::string> metadata_template_;
    
public:
    S3TargetHandler();
    ~S3TargetHandler() override;
    
    // 복사/이동 생성자 비활성화
    S3TargetHandler(const S3TargetHandler&) = delete;
    S3TargetHandler& operator=(const S3TargetHandler&) = delete;
    S3TargetHandler(S3TargetHandler&&) = delete;
    S3TargetHandler& operator=(S3TargetHandler&&) = delete;
    
    // ITargetHandler 인터페이스 구현 - 통합 타입 사용
    
    /**
     * @brief 핸들러 초기화
     * @param config JSON 설정 객체
     * @return 초기화 성공 여부
     */
    bool initialize(const json& config) override;
    
    /**
     * @brief 알람 메시지 전송 (S3 업로드)
     * @param alarm 업로드할 알람 메시지
     * @param config 타겟별 설정
     * @return 업로드 결과 (TargetTypes.h의 TargetSendResult 사용)
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트
     * @param config 타겟별 설정
     * @return 연결 성공 여부
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 반환
     */
    std::string getHandlerType() const override;
    
    /**
     * @brief 설정 유효성 검증
     */
    bool validateConfig(const json& config, std::vector<std::string>& errors) override;

private:
    // 내부 구현 메서드들
    
    /**
     * @brief 자격증명 로드
     */
    void loadCredentials(const json& config, PulseOne::Client::S3Config& s3_config);
    
    /**
     * @brief 메타데이터 템플릿 로드
     */
    void loadMetadataTemplate(const json& config);
    
    /**
     * @brief 객체 키 생성
     */
    std::string generateObjectKey(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 템플릿 확장
     */
    std::string expandTemplate(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief JSON 내용 빌드
     */
    std::string buildJsonContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 메타데이터 빌드
     */
    std::unordered_map<std::string, std::string> buildMetadata(const AlarmMessage& alarm, 
                                                              const json& config) const;
    
    /**
     * @brief 내용 압축
     */
    std::string compressContent(const std::string& content) const;
    
    /**
     * @brief 테스트 업로드 수행
     */
    bool performTestUpload();
    
    /**
     * @brief 타겟 이름 반환
     */
    std::string getTargetName(const json& config) const;
    
    /**
     * @brief 현재 타임스탬프 반환 (ISO 8601)
     */
    std::string getCurrentTimestamp() const;
    
    /**
     * @brief 타임스탬프 문자열 생성 (파일명용)
     */
    std::string generateTimestampString() const;
    
    /**
     * @brief 날짜 문자열 생성
     */
    std::string generateDateString() const;
    
    /**
     * @brief 년도 문자열 생성
     */
    std::string generateYearString() const;
    
    /**
     * @brief 월 문자열 생성
     */
    std::string generateMonthString() const;
    
    /**
     * @brief 일 문자열 생성
     */
    std::string generateDayString() const;
    
    /**
     * @brief 시간 문자열 생성
     */
    std::string generateHourString() const;
};

} // namespace CSP
} // namespace PulseOne

#endif // S3_TARGET_HANDLER_H