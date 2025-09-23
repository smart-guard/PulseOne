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
     * 
     * 필수 설정:
     * - bucket_name: S3 버킷 이름
     * - access_key_file: 액세스 키 파일 경로
     * - secret_key_file: 시크릿 키 파일 경로
     * 
     * 선택 설정:
     * - region: AWS 리전 (기본: ap-northeast-2)
     * - endpoint: S3 엔드포인트 (기본: AWS S3)
     * - object_prefix: 객체 키 접두사 (기본: "alarms/")
     * - file_name_pattern: 파일명 패턴 (기본: "{building_id}_{timestamp}_alarm.json")
     * - storage_class: 스토리지 클래스 (기본: STANDARD)
     * - server_side_encryption: 서버사이드 암호화 (AES256, aws:kms)
     * - kms_key_id: KMS 키 ID (SSE-KMS 사용시)
     * - timeout_ms: 업로드 타임아웃 (기본: 30000ms)
     * - connect_timeout_ms: 연결 타임아웃 (기본: 10000ms)
     * - max_retry: 최대 재시도 횟수 (기본: 3)
     * - retry_delay_ms: 재시도 간격 (기본: 2000ms)
     * - multipart_threshold: 멀티파트 업로드 임계치 (기본: 5MB)
     * - compression: 압축 사용 여부 (기본: false)
     * - verify_ssl: SSL 인증서 검증 (기본: true)
     * - path_style: 경로 스타일 사용 (기본: false, virtual-hosted style)
     * - metadata: 추가 메타데이터 (객체)
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
     * 
     * 테스트 방법:
     * 1. 버킷 존재 여부 확인 (HeadBucket)
     * 2. 쓰기 권한 확인 (작은 테스트 객체 업로드 후 삭제)
     * 3. 리전 및 엔드포인트 검증
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 이름 반환
     */
    std::string getTypeName() const override { return "s3"; }
    
    /**
     * @brief 핸들러 상태 반환
     */
    json getStatus() const override;

private:
    /**
     * @brief S3 설정 구성
     * @param config JSON 설정 객체
     * @param access_key 액세스 키
     * @param secret_key 시크릿 키
     * @return S3 클라이언트 설정 구조체
     */
    PulseOne::Client::S3Config createS3Config(
        const json& config,
        const std::string& access_key,
        const std::string& secret_key) const;
    
    /**
     * @brief 암호화된 자격증명 로드
     * @param key_file 키 파일 경로 (환경변수 또는 절대경로)
     * @return 복호화된 자격증명
     */
    std::string loadCredentials(const std::string& key_file) const;
    
    /**
     * @brief 객체 키 생성 (템플릿 기반)
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 생성된 객체 키
     * 
     * 지원 템플릿 변수:
     * - {building_id}: 빌딩 ID
     * - {timestamp}: 현재 시간 (YYYYMMDD_HHMMSS)
     * - {date}: 현재 날짜 (YYYYMMDD)
     * - {time}: 현재 시간 (HHMMSS)
     * - {year}: 연도 (YYYY)
     * - {month}: 월 (MM)
     * - {day}: 일 (DD)
     * - {hour}: 시 (HH)
     * - {minute}: 분 (MM)
     * - {second}: 초 (SS)
     * - {nm}: 알람 이름
     * - {lvl}: 알람 레벨
     * - {uuid}: 랜덤 UUID
     * - {sequence}: 시퀀스 번호
     */
    std::string generateObjectKey(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 메타데이터 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 메타데이터 맵
     */
    std::unordered_map<std::string, std::string> generateMetadata(
        const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief JSON 데이터 전처리 (압축, 포맷팅)
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 전처리된 JSON 문자열
     */
    std::string preprocessJsonData(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief S3 업로드 결과를 TargetSendResult로 변환
     * @param s3_result S3 업로드 결과
     * @param object_key 업로드된 객체 키
     * @param response_time 응답 시간
     * @param content_size 업로드된 데이터 크기
     * @return 타겟 전송 결과
     */
    TargetSendResult convertS3Result(
        const PulseOne::Client::S3UploadResult& s3_result,
        const std::string& object_key,
        std::chrono::milliseconds response_time,
        size_t content_size) const;
    
    /**
     * @brief 재시도 로직 실행
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 최종 업로드 결과
     */
    TargetSendResult executeWithRetry(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 단일 업로드 실행
     * @param s3_client S3 클라이언트
     * @param object_key 객체 키
     * @param content 업로드할 내용
     * @param metadata 메타데이터
     * @param config 설정 객체
     * @return S3 업로드 결과
     */
    PulseOne::Client::S3UploadResult executeSingleUpload(
        PulseOne::Client::S3Client& s3_client,
        const std::string& object_key,
        const std::string& content,
        const std::unordered_map<std::string, std::string>& metadata,
        const json& config) const;
    
    /**
     * @brief 템플릿 변수 확장
     * @param template_str 템플릿 문자열
     * @param alarm 알람 메시지 (변수 치환용)
     * @return 확장된 문자열
     */
    std::string expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief 버킷 이름 유효성 검증
     * @param bucket_name 검증할 버킷 이름
     * @return 유효한 버킷 이름인지 여부
     */
    bool isValidBucketName(const std::string& bucket_name) const;
    
    /**
     * @brief 리전 이름 유효성 검증
     * @param region 검증할 리전 이름
     * @return 유효한 리전 이름인지 여부
     */
    bool isValidRegion(const std::string& region) const;
    
    /**
     * @brief 스토리지 클래스 유효성 검증
     * @param storage_class 검증할 스토리지 클래스
     * @return 유효한 스토리지 클래스인지 여부
     */
    bool isValidStorageClass(const std::string& storage_class) const;
    
    /**
     * @brief 서버사이드 암호화 설정 유효성 검증
     * @param encryption 암호화 설정
     * @return 유효한 설정인지 여부
     */
    bool isValidEncryption(const std::string& encryption) const;
    
    /**
     * @brief 객체 키 유효성 검증
     * @param object_key 검증할 객체 키
     * @return 유효한 객체 키인지 여부
     */
    bool isValidObjectKey(const std::string& object_key) const;
    
    /**
     * @brief 설정 검증
     * @param config 검증할 설정
     * @param error_message 오류 메시지 (출력용)
     * @return 설정이 유효한지 여부
     */
    bool validateConfig(const json& config, std::string& error_message) const;
    
    /**
     * @brief 데이터 압축
     * @param data 원본 데이터
     * @param compression_type 압축 타입 ("gzip", "deflate")
     * @return 압축된 데이터
     */
    std::string compressData(const std::string& data, const std::string& compression_type) const;
    
    /**
     * @brief 파일 크기를 인간이 읽기 쉬운 형태로 변환
     * @param bytes 바이트 크기
     * @return 포맷된 크기 문자열 (예: "1.2 MB")
     */
    std::string formatFileSize(size_t bytes) const;
    
    /**
     * @brief UUID 생성
     * @return 랜덤 UUID 문자열
     */
    std::string generateUUID() const;
    
    /**
     * @brief 시퀀스 번호 생성 (스레드 안전)
     * @return 증가하는 시퀀스 번호
     */
    uint64_t generateSequenceNumber() const;
    
    /**
     * @brief 현재 시각을 ISO 8601 형식으로 변환
     * @param use_utc UTC 사용 여부 (기본: true)
     * @return ISO 8601 형식 시간 문자열
     */
    std::string getCurrentISOTime(bool use_utc = true) const;
    
    /**
     * @brief 객체 ACL 설정
     * @param config 설정 객체
     * @return ACL 문자열 ("private", "public-read" 등)
     */
    std::string getObjectACL(const json& config) const;
    
    /**
     * @brief 멀티파트 업로드 임계치 확인
     * @param content_size 콘텐츠 크기
     * @param config 설정 객체
     * @return 멀티파트 업로드를 사용해야 하는지 여부
     */
    bool shouldUseMultipartUpload(size_t content_size, const json& config) const;
    
    /**
     * @brief 업로드 로깅 (디버그용)
     * @param object_key 객체 키
     * @param content_size 콘텐츠 크기
     * @param metadata 메타데이터
     */
    void logUpload(const std::string& object_key, size_t content_size,
                  const std::unordered_map<std::string, std::string>& metadata) const;
    
    /**
     * @brief 업로드 결과 로깅
     * @param result 업로드 결과
     * @param object_key 객체 키
     * @param response_time 응답 시간
     */
    void logUploadResult(const PulseOne::Client::S3UploadResult& result,
                        const std::string& object_key,
                        std::chrono::milliseconds response_time) const;

    // 정적 멤버
    static std::atomic<uint64_t> sequence_counter_;  // 전역 시퀀스 카운터
};

} // namespace CSP
} // namespace PulseOne

#endif // S3_TARGET_HANDLER_H