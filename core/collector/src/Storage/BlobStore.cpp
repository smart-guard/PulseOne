#include "Storage/BlobStore.h"
#include "Logging/LogManager.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace PulseOne {
namespace Storage {

BlobStore &BlobStore::GetInstance() {
  static BlobStore instance;
  return instance;
}

BlobStore::BlobStore() { EnsureDirectoryExists(); }

void BlobStore::SetBaseDirectory(const std::string &path) {
  std::lock_guard<std::mutex> lock(mutex_);
  base_dir_ = path;
  EnsureDirectoryExists();
}

std::string BlobStore::SaveBlob(const std::vector<uint8_t> &data,
                                const std::string &extension,
                                const std::string &directory) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string target_dir = directory.empty() ? base_dir_ : directory;

  // 디렉토리 존재 확인 및 생성 (Override 된 경우)
  if (!directory.empty() && !std::filesystem::exists(directory)) {
    try {
      std::filesystem::create_directories(directory);
    } catch (const std::exception &e) {
      LogManager::getInstance().Error(
          "Failed to create blob storage directory: " + directory +
          " Error: " + e.what());
      return "";
    }
  } else if (directory.empty()) {
    // 기본 디렉토리 체크
    if (!EnsureDirectoryExists()) {
      return "";
    }
  }

  std::string filename = GenerateUniqueFilename(extension);
  std::filesystem::path full_path =
      std::filesystem::path(target_dir) / filename;

  std::ofstream ofs(full_path, std::ios::binary);
  if (!ofs) {
    LogManager::getInstance().Error("Failed to open file for writing blob: " +
                                    full_path.string());
    return "";
  }

  ofs.write(reinterpret_cast<const char *>(data.data()), data.size());
  ofs.close();

  std::string uri = "file://" + full_path.string();
  LogManager::getInstance().Info("Saved blob to: " + uri + " (Size: " +
                                 std::to_string(data.size()) + " bytes)");

  return uri;
}

std::string BlobStore::SaveBlob(const std::string &data,
                                const std::string &extension,
                                const std::string &directory) {
  std::vector<uint8_t> vec(data.begin(), data.end());
  return SaveBlob(vec, extension, directory);
}

void BlobStore::Cleanup(uint32_t max_age_seconds) {
  std::lock_guard<std::mutex> lock(mutex_);

  try {
    auto now = std::filesystem::file_time_type::clock::now();
    uint32_t count = 0;

    for (const auto &entry : std::filesystem::directory_iterator(base_dir_)) {
      if (entry.is_regular_file()) {
        auto last_write = entry.last_write_time();
        auto age =
            std::chrono::duration_cast<std::chrono::seconds>(now - last_write)
                .count();

        if (age > max_age_seconds) {
          std::filesystem::remove(entry.path());
          count++;
        }
      }
    }

    if (count > 0) {
      LogManager::getInstance().Info("Cleaned up " + std::to_string(count) +
                                     " old blobs from " + base_dir_);
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Error during BlobStore cleanup: " +
                                    std::string(e.what()));
  }
}

std::string BlobStore::GenerateUniqueFilename(const std::string &extension) {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");

  // Add random suffix to avoid collisions
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1000, 9999);
  ss << "_" << dis(gen);

  if (!extension.empty() && extension[0] != '.') {
    ss << ".";
  }
  ss << extension;

  return ss.str();
}

bool BlobStore::EnsureDirectoryExists() {
  try {
    if (!std::filesystem::exists(base_dir_)) {
      std::filesystem::create_directories(base_dir_);
      LogManager::getInstance().Info("Created blob storage directory: " +
                                     base_dir_);
    }
    return true;
  } catch (const std::exception &e) {
    LogManager::getInstance().Error(
        "Failed to create blob storage directory: " + base_dir_ +
        " Error: " + e.what());
    return false;
  }
}

} // namespace Storage
} // namespace PulseOne
