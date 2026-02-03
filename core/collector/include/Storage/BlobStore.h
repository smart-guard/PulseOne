#ifndef PULSEONE_STORAGE_BLOB_STORE_H
#define PULSEONE_STORAGE_BLOB_STORE_H

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace PulseOne {
namespace Storage {

/**
 * @brief 대용량 데이터(Blob)를 파일 시스템에 저장하는 유틸리티 클래스
 * Redis 부하를 줄이기 위해 사용됨
 */
class BlobStore {
public:
  /**
   * @brief 싱글톤 인스턴스 반환
   */
  static BlobStore &GetInstance();

  /**
   * @brief 기본 저장 경로 설정
   * @param path 저장할 디렉토리 경로
   */
  void SetBaseDirectory(const std::string &path);

  /**
   * @brief 데이터를 파일로 저장
   * @param data 저장할 바이너리 데이터
   * @param extension 파일 확장자 (예: ".bin", ".json")
   * @return 저장된 파일의 file:// URI (실패 시 빈 문자열)
   */
  std::string SaveBlob(const std::vector<uint8_t> &data,
                       const std::string &extension = ".bin");

  /**
   * @brief 데이터를 파일로 저장 (문자열 버전)
   * @param data 저장할 문자열 데이터
   * @param extension 파일 확장자
   * @return 저장된 파일의 file:// URI
   */
  std::string SaveBlob(const std::string &data,
                       const std::string &extension = ".bin");

  /**
   * @brief 오래된 임시 파일 정리
   * @param max_age_seconds 유지할 최대 시간
   */
  void Cleanup(uint32_t max_age_seconds = 86400);

private:
  BlobStore();
  ~BlobStore() = default;

  BlobStore(const BlobStore &) = delete;
  BlobStore &operator=(const BlobStore &) = delete;

  std::string GenerateUniqueFilename(const std::string &extension);
  bool EnsureDirectoryExists();

  std::string base_dir_ = "/app/data/blobs";
  std::mutex mutex_;
};

} // namespace Storage
} // namespace PulseOne

#endif // PULSEONE_STORAGE_BLOB_STORE_H
