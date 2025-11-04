/**
 * @file FileTargetHandler.h
 * @brief 로컬 파일 타겟 핸들러 - Stateless 패턴 (v2.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0 - Production-Ready Stateless
 * 저장 위치: core/export-gateway/include/CSP/FileTargetHandler.h
 * 
 * 🚀 v2.0 주요 변경:
 * - 상태 멤버 변수 제거 (base_path_, file_format_, templates 등)
 * - initialize() 선택적 (없어도 동작)
 * - config 기반 동작
 * - Thread-safe 보장
 */

#ifndef FILE_TARGET_HANDLER_H
#define FILE_TARGET_HANDLER_H

#include "Export/ExportTypes.h"
#include <string>
#include <atomic>

namespace PulseOne {
namespace CSP {

/**
 * @brief 로컬 파일 타겟 핸들러 (Stateless v2.0)
 * 
 * 특징:
 * - 상태 없음 (base_path, templates 멤버 제거)
 * - 각 sendAlarm() 호출마다 config에서 설정 읽음
 * - initialize() 선택적 (호출 안 해도 동작)
 * - Thread-safe 보장
 * 
 * 지원 기능:
 * - 계층적 디렉토리 구조 (빌딩/날짜별)
 * - 파일명 템플릿
 * - JSON/CSV/TXT/XML 형식
 * - 파일 압축 (gzip)
 * - 자동 로테이션
 * - 오래된 파일 정리
 * - 원자적 쓰기
 */
class FileTargetHandler : public ITargetHandler {
private:
    // ✅ 통계만 유지 (경량)
    std::atomic<size_t> file_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    std::atomic<size_t> total_bytes_written_{0};
    
public:
    FileTargetHandler();
    ~FileTargetHandler() override;
    
    FileTargetHandler(const FileTargetHandler&) = delete;
    FileTargetHandler& operator=(const FileTargetHandler&) = delete;
    FileTargetHandler(FileTargetHandler&&) = delete;
    FileTargetHandler& operator=(FileTargetHandler&&) = delete;
    
    // =======================================================================
    // ITargetHandler 인터페이스 구현
    // =======================================================================
    
    /**
     * @brief 선택적 초기화 (설정 검증 + 디렉토리 생성)
     */
    bool initialize(const json& config) override;
    
    /**
     * @brief 알람 파일 저장 (Stateless - config 기반 동작)
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입
     */
    std::string getHandlerType() const override { return "FILE"; }
    
    /**
     * @brief 설정 검증
     */
    bool validateConfig(const json& config, std::vector<std::string>& errors) override;
    
    /**
     * @brief 정리 (통계 리셋)
     */
    void cleanup() override;
    
    /**
     * @brief 상태 조회
     */
    json getStatus() const override;

private:
    // =======================================================================
    // Private 핵심 메서드
    // =======================================================================
    
    /**
     * @brief config에서 base_path 추출
     */
    std::string extractBasePath(const json& config) const;
    
    /**
     * @brief config에서 file_format 추출
     */
    std::string extractFileFormat(const json& config) const;
    
    /**
     * @brief 파일 경로 생성
     */
    std::string generateFilePath(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 디렉토리 생성
     */
    void createDirectoriesForFile(const std::string& file_path) const;
    
    /**
     * @brief 파일 내용 생성
     */
    std::string buildFileContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief JSON 형식 내용
     */
    std::string buildJsonContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief CSV 형식 내용
     */
    std::string buildCsvContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 텍스트 형식 내용
     */
    std::string buildTextContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief XML 형식 내용
     */
    std::string buildXmlContent(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 파일 쓰기 (원자적/직접)
     */
    bool writeFile(const std::string& file_path, const std::string& content, 
                   const json& config) const;
    
    /**
     * @brief 템플릿 확장
     */
    std::string expandTemplate(const std::string& template_str, 
                               const AlarmMessage& alarm) const;
    
    /**
     * @brief 파일명 안전화
     */
    std::string sanitizeFilename(const std::string& filename) const;
    
    /**
     * @brief 타겟 이름 반환
     */
    std::string getTargetName(const json& config) const;
    
    /**
     * @brief 현재 타임스탬프 (ISO 8601)
     */
    std::string getCurrentTimestamp() const;
    
    /**
     * @brief 타임스탬프 문자열 (파일명용)
     */
    std::string generateTimestampString() const;
    
    /**
     * @brief 날짜 문자열
     */
    std::string generateDateString() const;
    
    /**
     * @brief 연도 문자열
     */
    std::string generateYearString() const;
    
    /**
     * @brief 월 문자열
     */
    std::string generateMonthString() const;
    
    /**
     * @brief 일 문자열
     */
    std::string generateDayString() const;
    
    /**
     * @brief 시간 문자열
     */
    std::string generateHourString() const;
    
    /**
     * @brief XML 이스케이프
     */
    std::string escapeXml(const std::string& text) const;
    
    /**
     * @brief 파일 확장자 반환
     */
    std::string getFileExtension(const std::string& format) const;
};

} // namespace CSP
} // namespace PulseOne

#endif // FILE_TARGET_HANDLER_H