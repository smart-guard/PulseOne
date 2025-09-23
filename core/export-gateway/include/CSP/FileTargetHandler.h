/**
 * @file FileTargetHandler.h
 * @brief 로컬 파일 타겟 핸들러 - 로컬 파일 시스템에 알람 저장
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/include/CSP/FileTargetHandler.h
 */

#ifndef FILE_TARGET_HANDLER_H
#define FILE_TARGET_HANDLER_H

#include "CSPDynamicTargets.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <filesystem>

namespace PulseOne {
namespace CSP {

/**
 * @brief 로컬 파일 타겟 핸들러
 * 
 * 지원 기능:
 * - 계층적 디렉토리 구조 (빌딩/날짜별)
 * - 파일명 템플릿 지원
 * - JSON/텍스트 형식 지원
 * - 파일 압축 (gzip, zip)
 * - 자동 로테이션 (일별, 시간별)
 * - 오래된 파일 자동 정리
 * - 파일 잠금 및 동시성 제어
 * - 디스크 공간 모니터링
 * - 백업 및 아카이빙
 * - 심볼릭 링크 지원
 * - 권한 관리
 */
class FileTargetHandler : public ITargetHandler {
private:
    mutable std::mutex file_mutex_;    // 파일 작업 보호용
    mutable std::mutex cleanup_mutex_; // 정리 작업 보호용
    
    std::atomic<size_t> file_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    std::atomic<size_t> total_bytes_written_{0};
    
    // 백그라운드 정리 스레드
    std::unique_ptr<std::thread> cleanup_thread_;
    std::atomic<bool> should_stop_{false};
    
public:
    /**
     * @brief 생성자
     */
    FileTargetHandler();
    
    /**
     * @brief 소멸자
     */
    ~FileTargetHandler() override;
    
    // 복사/이동 생성자 비활성화
    FileTargetHandler(const FileTargetHandler&) = delete;
    FileTargetHandler& operator=(const FileTargetHandler&) = delete;
    FileTargetHandler(FileTargetHandler&&) = delete;
    FileTargetHandler& operator=(FileTargetHandler&&) = delete;
    
    /**
     * @brief 핸들러 초기화
     * @param config JSON 설정 객체
     * @return 초기화 성공 여부
     * 
     * 필수 설정:
     * - base_path: 기본 저장 경로
     * 
     * 선택 설정:
     * - file_pattern: 파일명 패턴 (기본: "{building_id}/{date}/alarm_{timestamp}_{nm}.json")
     * - format: 파일 형식 ("json", "text", "csv") (기본: "json")
     * - compression: 압축 타입 ("none", "gzip", "zip") (기본: "none")
     * - rotation: 로테이션 설정 ("none", "daily", "hourly", "size") (기본: "daily")
     * - max_file_size: 최대 파일 크기 (바이트) (기본: 100MB)
     * - max_files_per_dir: 디렉토리당 최대 파일 수 (기본: 1000)
     * - cleanup_enabled: 자동 정리 활성화 (기본: true)
     * - cleanup_after_days: 정리할 파일 보관 일수 (기본: 30일)
     * - cleanup_interval_hours: 정리 작업 간격 (기본: 24시간)
     * - min_free_space_mb: 최소 여유 공간 (MB) (기본: 1024MB)
     * - file_permissions: 파일 권한 (8진수) (기본: 0644)
     * - dir_permissions: 디렉토리 권한 (8진수) (기본: 0755)
     * - create_symlinks: 심볼릭 링크 생성 (기본: false)
     * - backup_enabled: 백업 활성화 (기본: false)
     * - backup_path: 백업 경로
     * - include_metadata: 메타데이터 포함 (기본: true)
     * - pretty_print: JSON 포맷팅 (기본: true)
     * - atomic_write: 원자적 쓰기 (임시파일 사용) (기본: true)
     */
    bool initialize(const json& config) override;
    
    /**
     * @brief 알람 메시지 전송 (파일 저장)
     * @param alarm 저장할 알람 메시지
     * @param config 타겟별 설정
     * @return 저장 결과
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트 (디렉토리 접근 및 쓰기 권한 확인)
     * @param config 타겟별 설정
     * @return 테스트 성공 여부
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 이름 반환
     */
    std::string getTypeName() const override { return "file"; }
    
    /**
     * @brief 핸들러 상태 반환
     */
    json getStatus() const override;
    
    /**
     * @brief 핸들러 정리
     */
    void cleanup() override;

private:
    /**
     * @brief 파일 경로 생성 (템플릿 기반)
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 생성된 파일 경로
     */
    std::string generateFilePath(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 디렉토리 생성 (계층적)
     * @param dir_path 생성할 디렉토리 경로
     * @param permissions 디렉토리 권한
     * @return 생성 성공 여부
     */
    bool createDirectories(const std::string& dir_path, std::filesystem::perms permissions) const;
    
    /**
     * @brief 파일 내용 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 파일 내용
     */
    std::string generateFileContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 원자적 파일 쓰기 (임시파일 사용)
     * @param file_path 최종 파일 경로
     * @param content 파일 내용
     * @param permissions 파일 권한
     * @return 쓰기 성공 여부
     */
    bool writeFileAtomically(const std::string& file_path, const std::string& content,
                            std::filesystem::perms permissions) const;
    
    /**
     * @brief 파일 압축
     * @param file_path 압축할 파일 경로
     * @param compression_type 압축 타입
     * @return 압축 성공 여부
     */
    bool compressFile(const std::string& file_path, const std::string& compression_type) const;
    
    /**
     * @brief 파일 로테이션 확인 및 수행
     * @param file_path 파일 경로
     * @param config 설정 객체
     * @return 로테이션 수행 여부
     */
    bool checkAndRotateFile(const std::string& file_path, const json& config) const;
    
    /**
     * @brief 심볼릭 링크 생성 (latest 링크)
     * @param file_path 대상 파일 경로
     * @param config 설정 객체
     * @return 링크 생성 성공 여부
     */
    bool createSymbolicLinks(const std::string& file_path, const json& config) const;
    
    /**
     * @brief 파일 백업
     * @param file_path 백업할 파일 경로
     * @param config 설정 객체
     * @return 백업 성공 여부
     */
    bool backupFile(const std::string& file_path, const json& config) const;
    
    /**
     * @brief 오래된 파일 정리
     * @param config 설정 객체
     * @return 정리된 파일 수
     */
    size_t cleanupOldFiles(const json& config) const;
    
    /**
     * @brief 디스크 공간 확인
     * @param path 확인할 경로
     * @param min_free_space_mb 최소 필요 공간 (MB)
     * @return 충분한 공간이 있는지 여부
     */
    bool checkDiskSpace(const std::string& path, size_t min_free_space_mb) const;
    
    /**
     * @brief 디렉토리별 파일 수 확인
     * @param dir_path 디렉토리 경로
     * @param max_files 최대 파일 수
     * @return 파일 수가 제한 내인지 여부
     */
    bool checkFileCount(const std::string& dir_path, size_t max_files) const;
    
    /**
     * @brief 백그라운드 정리 스레드
     * @param config 설정 객체 (복사본)
     */
    void cleanupThread(json config);
    
    /**
     * @brief 템플릿 변수 확장
     * @param template_str 템플릿 문자열
     * @param alarm 알람 메시지
     * @return 확장된 문자열
     */
    std::string expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief 파일 경로 유효성 검증
     * @param file_path 검증할 파일 경로
     * @return 유효한 경로인지 여부
     */
    bool isValidFilePath(const std::string& file_path) const;
    
    /**
     * @brief 압축 타입 유효성 검증
     * @param compression_type 압축 타입
     * @return 지원하는 압축 타입인지 여부
     */
    bool isValidCompressionType(const std::string& compression_type) const;
    
    /**
     * @brief 파일 형식 유효성 검증
     * @param format 파일 형식
     * @return 지원하는 형식인지 여부
     */
    bool isValidFileFormat(const std::string& format) const;
    
    /**
     * @brief 설정 검증
     * @param config 검증할 설정
     * @param error_message 오류 메시지 (출력용)
     * @return 설정이 유효한지 여부
     */
    bool validateConfig(const json& config, std::string& error_message) const;
    
    /**
     * @brief JSON 형식 파일 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return JSON 문자열
     */
    std::string createJsonContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 텍스트 형식 파일 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 텍스트 문자열
     */
    std::string createTextContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief CSV 형식 파일 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return CSV 문자열
     */
    std::string createCsvContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 파일 크기를 인간이 읽기 쉬운 형태로 변환
     * @param bytes 바이트 크기
     * @return 포맷된 크기 문자열
     */
    std::string formatFileSize(size_t bytes) const;
    
    /**
     * @brief 디스크 사용량 정보 반환
     * @param path 확인할 경로
     * @return 디스크 사용량 정보
     */
    struct DiskUsage {
        size_t total_bytes;
        size_t free_bytes;
        size_t used_bytes;
        double usage_percent;
    };
    
    DiskUsage getDiskUsage(const std::string& path) const;
    
    /**
     * @brief 파일 메타데이터 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 메타데이터 객체
     */
    json createMetadata(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 로테이션된 파일명 생성
     * @param original_path 원본 파일 경로
     * @param rotation_type 로테이션 타입
     * @return 로테이션된 파일 경로
     */
    std::string generateRotatedFileName(const std::string& original_path, 
                                       const std::string& rotation_type) const;
    
    /**
     * @brief 파일 쓰기 로깅
     * @param file_path 파일 경로
     * @param content_size 내용 크기
     * @param compression_type 압축 타입
     */
    void logFileWrite(const std::string& file_path, size_t content_size,
                     const std::string& compression_type) const;
    
    /**
     * @brief 파일 쓰기 결과 로깅
     * @param result 쓰기 결과
     * @param file_path 파일 경로
     * @param response_time 처리 시간
     */
    void logFileWriteResult(const TargetSendResult& result, const std::string& file_path,
                           std::chrono::milliseconds response_time) const;

    // 내부 상태
    std::string base_path_;
    std::chrono::system_clock::time_point last_cleanup_time_;
    std::atomic<size_t> cleanup_file_count_{0};
};

} // namespace CSP
} // namespace PulseOne

#endif // FILE_TARGET_HANDLER_H