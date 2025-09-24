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
 * @brief 파일 옵션 구조체
 */
struct FileOptions {
    bool append_mode = false;
    bool create_directories = true;
    bool atomic_write = true;
    bool backup_on_overwrite = false;
};

/**
 * @brief 로테이션 설정 구조체
 */
struct RotationConfig {
    bool enabled = true;
    size_t max_file_size_mb = 100;
    size_t max_files_per_dir = 1000;
    int auto_cleanup_days = 30;
};

/**
 * @brief 로컬 파일 타겟 핸들러
 * 
 * 지원 기능:
 * - 계층적 디렉토리 구조 (빌딩/날짜별)
 * - 파일명 템플릿 지원
 * - JSON/텍스트/CSV/XML 형식 지원
 * - 파일 압축 (gzip, zip)
 * - 자동 로테이션 (일별, 시간별)
 * - 오래된 파일 자동 정리
 * - 파일 잠금 및 동시성 제어
 * - 디스크 공간 모니터링
 * - 백업 및 아카이빙
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
    
    // ========== 구현 파일에서 사용하는 멤버 변수들 ==========
    std::string base_path_;
    std::string file_format_ = "json";
    std::string filename_template_;
    std::string directory_template_;
    std::string compression_format_ = "gzip";
    bool compression_enabled_ = false;
    mode_t file_permissions_ = 0644;
    mode_t dir_permissions_ = 0755;
    
    FileOptions file_options_;
    RotationConfig rotation_config_;
    
    std::chrono::system_clock::time_point last_cleanup_time_;
    std::atomic<size_t> cleanup_file_count_{0};
    
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
     * @brief 기본 디렉토리들 생성
     */
    void createBaseDirectories();
    
    /**
     * @brief 파일용 디렉토리 생성
     * @param file_path 파일 경로
     */
    void createDirectoriesForFile(const std::string& file_path);
    
    /**
     * @brief 파일 경로 생성 (템플릿 기반)
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 생성된 파일 경로
     */
    std::string generateFilePath(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 템플릿 확장
     * @param template_str 템플릿 문자열
     * @param alarm 알람 메시지
     * @return 확장된 문자열
     */
    std::string expandTemplate(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief 파일 내용 빌드
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 파일 내용
     */
    std::string buildFileContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief JSON 내용 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return JSON 문자열
     */
    std::string buildJsonContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief CSV 내용 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return CSV 문자열
     */
    std::string buildCsvContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 텍스트 내용 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 텍스트 문자열
     */
    std::string buildTextContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief XML 내용 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return XML 문자열
     */
    std::string buildXmlContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 원자적 파일 쓰기
     * @param file_path 파일 경로
     * @param content 내용
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 성공 여부
     */
    bool writeFileAtomic(const std::string& file_path, const std::string& content,
                        const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 직접 파일 쓰기
     * @param file_path 파일 경로
     * @param content 내용
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 성공 여부
     */
    bool writeFileDirectly(const std::string& file_path, const std::string& content,
                          const AlarmMessage& alarm, const json& config);
    
    /**
     * @brief 백업 파일 생성
     * @param original_path 원본 파일 경로
     */
    void createBackupFile(const std::string& original_path);
    
    /**
     * @brief 로테이션 필요성 확인 및 수행
     * @param file_path 파일 경로
     */
    void checkAndRotateIfNeeded(const std::string& file_path);
    
    /**
     * @brief 파일 로테이션
     * @param file_path 파일 경로
     */
    void rotateFile(const std::string& file_path);
    
    /**
     * @brief 디렉토리 파일 수 확인
     * @param file_path 파일 경로
     */
    void checkDirectoryFileCount(const std::string& file_path);
    
    /**
     * @brief 오래된 파일 정리
     * @param file_path 파일 경로
     */
    void cleanupOldFiles(const std::string& file_path);
    
    /**
     * @brief 내용 압축
     * @param content 압축할 내용
     * @return 압축된 내용
     */
    std::string compressContent(const std::string& content) const;
    
    /**
     * @brief 파일 확장자 반환
     * @return 파일 확장자
     */
    std::string getFileExtension() const;
    
    /**
     * @brief 압축 확장자 반환
     * @return 압축 확장자
     */
    std::string getCompressionExtension() const;
    
    /**
     * @brief 파일 권한 설정
     * @param file_path 파일 경로
     */
    void setFilePermissions(const std::string& file_path);
    
    /**
     * @brief 파일명 정리 (안전한 문자로 변환)
     * @param filename 원본 파일명
     * @return 정리된 파일명
     */
    std::string sanitizeFilename(const std::string& filename) const;
    
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

#endif // FILE_TARGET_HANDLER_H