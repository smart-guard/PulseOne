/**
 * @file FileTargetHandler.h - 컴파일 에러 완전 수정
 * @brief 로컬 파일 타겟 핸들러 - ITargetHandler 인터페이스 정확 구현
 * @author PulseOne Development Team
 * @date 2025-09-29
 * @version 3.0.0 (컴파일 에러 수정)
 * 저장 위치: core/export-gateway/include/CSP/FileTargetHandler.h
 * 
 * 🚨 컴파일 에러 수정사항:
 * 1. getTypeName() → getHandlerType() 수정 (ITargetHandler 인터페이스 준수)
 * 2. validateConfig() 메서드 추가 (순수 가상 함수 구현)
 * 3. 모든 필수 ITargetHandler 메서드 구현
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
    
    // 구현 파일에서 사용하는 멤버 변수들
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
    
    // =======================================================================
    // 🚨 ITargetHandler 인터페이스 정확 구현 (CSPDynamicTargets.h 준수)
    // =======================================================================
    
    /**
     * @brief 핸들러 초기화
     */
    bool initialize(const json& config) override;
    
    /**
     * @brief 알람 메시지 전송 (파일 저장)
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트 (디렉토리 접근 및 쓰기 권한 확인)
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 이름 반환
     * 🚨 수정: getTypeName() → getHandlerType() (ITargetHandler 인터페이스 준수)
     */
    std::string getHandlerType() const override { return "FILE"; }
    
    /**
     * @brief 설정 유효성 검증
     * 🚨 추가: 순수 가상 함수 구현 (ITargetHandler 인터페이스 준수)
     */
    bool validateConfig(const json& config, std::vector<std::string>& errors) override;
    
    /**
     * @brief 핸들러 상태 반환
     */
    json getStatus() const override;
    
    /**
     * @brief 핸들러 정리
     */
    void cleanup() override;

private:
    // =======================================================================
    // 내부 구현 메서드들
    // =======================================================================
    
    void createBaseDirectories();
    void createDirectoriesForFile(const std::string& file_path);
    std::string generateFilePath(const AlarmMessage& alarm, const json& config) const;
    std::string expandTemplate(const std::string& template_str, const AlarmMessage& alarm) const;
    std::string buildFileContent(const AlarmMessage& alarm, const std::string& format) const;
    bool writeFileContent(const std::string& file_path, const std::string& content, bool append);
    bool writeFileAtomic(const std::string& file_path, const std::string& content);
    bool writeFileDirect(const std::string& file_path, const std::string& content, bool append);
    void createBackupFile(const std::string& original_path);
    void checkAndRotateIfNeeded(const std::string& file_path);
    void rotateFile(const std::string& file_path);
    void checkDirectoryFileCount(const std::string& file_path);
    void cleanupOldFiles(const std::string& file_path);
    std::string compressContent(const std::string& content) const;
    std::string getFileExtension() const;
    std::string getCompressionExtension() const;
    void setFilePermissions(const std::string& file_path);
    std::string sanitizeFilename(const std::string& filename) const;
    std::string getTargetName(const json& config) const;
    std::string getCurrentTimestamp() const;
    std::string generateTimestampString() const;
    std::string generateDateString() const;
    std::string generateYearString() const;
    std::string generateMonthString() const;
    std::string generateDayString() const;
    std::string generateHourString() const;
};

} // namespace CSP
} // namespace PulseOne

#endif // FILE_TARGET_HANDLER_H