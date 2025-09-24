/**
 * @file S3TargetHandler.h
 * @brief S3 타겟 핸들러 - AWS S3 및 호환 스토리지 업로드 처리
 * @author PulseOne Development Team
 * @date 2025-09-23
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
 * 지원 기능:
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
 * - 액세스 로그 및 CloudTrail 호환
 */
class S3TargetHandler : public ITargetHandler {
private:
    mutable std::mutex client_mutex_;  // S3 클라이언트 보호용
    std::atomic<size_t> upload_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    std::atomic<size_t> total_bytes_uploaded_{0};
    
    // ========== 구현 파일에서 사용하는 멤버 변수들 ==========
    std::unique_ptr<PulseOne::Client::S3Client> s3_client_;
    bool compression_enabled_ = false;
    int compression_level_ = 6;
    std::string object_key_template_;
    std::unordered_map<std::string, std::string> metadata_template_;
    
public:
    /**
     * @brief 생성자
     */
    S3TargetHandler();
    
    /**
     * @brief 소멸자
     */
    ~S3TargetHandler() override;
    
    // 복사/이동 생성자 비활성화
    S3TargetHandler(const S3TargetHandler&) = delete;
    S3TargetHandler& operator=(const S3TargetHandler&) = delete;
    S3TargetHandler(S3TargetHandler&&) = delete;
    S3TargetHandler& operator=(S3TargetHandler&&) = delete;
    
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
     * @return 업로드 결과
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트
     * @param config 타겟별 설정
     * @return 연결 성공 여부
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 이름 반환
     */
    std::string getTypeName() const override;
    
    /**
     * @brief 핸들러 상태 반환
     */
    json getStatus() const override;
    
    /**
     * @brief 핸들러 정리
     */
    void cleanup() override;

private:
    // ========== 구현 파일에서 사용하는 모든 메서드들 선언 ==========
    
    /**
     * @brief 자격증명 로드
     * @param config 설정 객체
     * @param s3_config S3 설정 구조체 (출력용)
     */
    void loadCredentials(const json& config, PulseOne::Client::S3Config& s3_config);
    
    /**
     * @brief 메타데이터 템플릿 로드
     * @param config 설정 객체
     */
    void loadMetadataTemplate(const json& config);
    
    /**
     * @brief 객체 키 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 생성된 객체 키
     */
    std::string generateObjectKey(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 템플릿 확장
     * @param template_str 템플릿 문자열
     * @param alarm 알람 메시지
     * @return 확장된 문자열
     */
    std::string expandTemplate(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief JSON 내용 빌드
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return JSON 문자열
     */
    std::string buildJsonContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 메타데이터 빌드
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 메타데이터 맵
     */
    std::unordered_map<std::string, std::string> buildMetadata(const AlarmMessage& alarm, 
                                                              const json& config) const;
    
    /**
     * @brief 내용 압축
     * @param content 압축할 내용
     * @return 압축된 내용
     */
    std::string compressContent(const std::string& content) const;
    
    /**
     * @brief 테스트 업로드 수행
     * @return 성공 여부
     */
    bool performTestUpload();
    
    /**
     * @brief 타겟 이름 반환
     * @param config 설정 객체
     * @return 타겟 이름
     */
    std::string getTargetName(const json& config) const;
    
    /**
     * @brief 현재 타임스탬프 반환 (ISO 8601)
     * @return ISO 8601 형식 타임스탬프
     */
    std::string getCurrentTimestamp() const;
    
    /**
     * @brief 타임스탬프 문자열 생성 (파일명용)
     * @return 파일명용 타임스탬프
     */
    std::string generateTimestampString() const;
    
    /**
     * @brief 날짜 문자열 생성
     * @return 날짜 문자열
     */
    std::string generateDateString() const;
    
    /**
     * @brief 년도 문자열 생성
     * @return 년도 문자열
     */
    std::string generateYearString() const;
    
    /**
     * @brief 월 문자열 생성
     * @return 월 문자열
     */
    std::string generateMonthString() const;
    
    /**
     * @brief 일 문자열 생성
     * @return 일 문자열
     */
    std::string generateDayString() const;
    
    /**
     * @brief 시간 문자열 생성
     * @return 시간 문자열
     */
    std::string generateHourString() const;
};

} // namespace CSP
} // namespace PulseOne

#endif // S3_TARGET_HANDLER_H