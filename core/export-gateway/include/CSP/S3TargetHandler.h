/**
 * @file S3TargetHandler.h
 * @brief S3 타겟 핸들러 - Stateless 패턴 (v2.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0 - Production-Ready with ClientCacheManager
 * 저장 위치: core/export-gateway/include/CSP/S3TargetHandler.h
 *
 * 🚀 v2.0 주요 변경:
 * - s3_client_ 멤버 변수 제거 (Stateless)
 * - ClientCacheManager 사용
 * - initialize() 선택적 (없어도 동작)
 * - Thread-safe 보장
 */

#ifndef S3_TARGET_HANDLER_H
#define S3_TARGET_HANDLER_H

#include "Export/ExportTypes.h"
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

// PulseOne S3Client 전방 선언
namespace PulseOne {
namespace Client {
class S3Client;
struct S3Config;
} // namespace Client
} // namespace PulseOne

namespace PulseOne {
namespace CSP {

/**
 * @brief S3 타겟 핸들러 (Stateless v2.0)
 *
 * 특징:
 * - 상태 없음 (s3_client_ 멤버 제거)
 * - 각 sendAlarm() 호출마다 config 기반으로 클라이언트 획득
 * - ClientCacheManager로 클라이언트 재사용 (버킷별 캐싱)
 * - initialize() 선택적 (호출 안 해도 동작)
 * - Thread-safe 보장
 *
 * 주요 기능:
 * - AWS S3 업로드
 * - S3 호환 스토리지 (MinIO, Ceph, R2 등)
 * - 객체 키 템플릿
 * - 압축 지원 (gzip)
 * - 메타데이터 자동 추가
 */
class S3TargetHandler : public ITargetHandler {
private:
  // ✅ 통계만 유지 (경량)
  std::atomic<size_t> upload_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> failure_count_{0};
  std::atomic<size_t> total_bytes_uploaded_{0};

public:
  S3TargetHandler();
  ~S3TargetHandler() override;

  S3TargetHandler(const S3TargetHandler &) = delete;
  S3TargetHandler &operator=(const S3TargetHandler &) = delete;
  S3TargetHandler(S3TargetHandler &&) = delete;
  S3TargetHandler &operator=(S3TargetHandler &&) = delete;

  // =======================================================================
  // ITargetHandler 인터페이스 구현
  // =======================================================================

  /**
   * @brief 선택적 초기화 (설정 검증만 수행)
   */
  bool initialize(const json &config) override;

  /**
   * @brief 알람 업로드 (Stateless - config 기반 동작)
   */
  TargetSendResult sendAlarm(const AlarmMessage &alarm,
                             const json &config) override;

  /**
   * @brief 배치 알람 업로드 (단일 파일로 묶어서 전송)
   */
  std::vector<TargetSendResult>
  sendAlarmBatch(const std::vector<AlarmMessage> &alarms,
                 const json &config) override;

  /**
   * @brief 배치 값 업로드 (단일 파일로 묶어서 전송)
   */
  std::vector<TargetSendResult>
  sendValueBatch(const std::vector<ValueMessage> &values,
                 const json &config) override;

  /**
   * @brief 연결 테스트
   */
  bool testConnection(const json &config) override;

  /**
   * @brief 핸들러 타입
   */
  std::string getHandlerType() const override { return "S3"; }

  /**
   * @brief 설정 검증
   */
  bool validateConfig(const json &config,
                      std::vector<std::string> &errors) override;

  /**
   * @brief 정리 (캐시 비우기)
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
   * @brief S3Client 가져오기 또는 생성 (캐시 사용)
   * @param config 설정
   * @param bucket_name 버킷명 (캐시 키)
   * @return S3Client 공유 포인터
   */
  std::shared_ptr<Client::S3Client>
  getOrCreateClient(const json &config, const std::string &bucket_name);

  /**
   * @brief config에서 버킷명 추출
   */
  std::string extractBucketName(const json &config) const;

  /**
   * @brief S3Config 구성
   */
  Client::S3Config buildS3Config(const json &config) const;

  /**
   * @brief 자격증명 로드
   */
  void loadCredentials(const json &config, Client::S3Config &s3_config) const;

  /**
   * @brief 객체 키 생성
   */
  std::string generateObjectKey(const AlarmMessage &alarm,
                                const json &config) const;

  /**
   * @brief 템플릿 확장
   */
  std::string expandTemplate(const std::string &template_str,
                             const AlarmMessage &alarm) const;

  /**
   * @brief JSON 객체 템플릿 변수 확장
   */
  void expandTemplateVariables(json &template_json,
                               const AlarmMessage &alarm) const;

  void expandTemplateVariables(json &template_json,
                               const ValueMessage &value) const;

  /**
   * @brief JSON 내용 빌드
   */
  std::string buildJsonContent(const AlarmMessage &alarm,
                               const json &config) const;

  /**
   * @brief 메타데이터 빌드
   */
  std::unordered_map<std::string, std::string>
  buildMetadata(const AlarmMessage &alarm, const json &config) const;

  /**
   * @brief 내용 압축
   */
  std::string compressContent(const std::string &content, int level) const;

  /**
   * @brief 타겟 이름 반환
   */
  std::string getTargetName(const json &config) const;

  /**
   * @brief 현재 타임스탬프 (ISO 8601)
   */
  std::string getCurrentTimestamp() const;

  /**
   * @brief 요청 ID 생성
   */
  std::string generateRequestId() const;

  /**
   * @brief 타임스탬프 문자열 (파일명용)
   */
  std::string generateTimestampString() const;

  /**
   * @brief 날짜 문자열
   */
  std::string generateDateString() const;

  /**
   * @brief 년도 문자열
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
   * @brief 환경변수 치환
   */
  std::string expandEnvironmentVariables(const std::string &str) const;

  /**
   * @brief S3 엔드포인트 자동 생성
   */
  std::string generateS3Endpoint(const std::string &region) const;
};

} // namespace CSP
} // namespace PulseOne

#endif // S3_TARGET_HANDLER_H