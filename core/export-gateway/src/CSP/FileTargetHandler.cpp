/**
 * @file FileTargetHandler.cpp
 * @brief 로컬 파일 타겟 핸들러 - Stateless 패턴 (v2.0)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 2.0.0 - Production-Ready Stateless
 * 저장 위치: core/export-gateway/src/CSP/FileTargetHandler.cpp
 *
 * 🚀 v2.0 주요 변경:
 * - Stateless 핸들러 패턴 적용
 * - 모든 상태 멤버 변수 제거
 * - initialize() 선택적 (없어도 동작)
 * - config 기반 동작
 */

#include "CSP/FileTargetHandler.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace PulseOne {
namespace CSP {

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

FileTargetHandler::FileTargetHandler() {
  LogManager::getInstance().Info("FileTargetHandler 초기화 (Stateless)");
}

FileTargetHandler::~FileTargetHandler() {
  LogManager::getInstance().Info("FileTargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool FileTargetHandler::initialize(const ordered_json &config) {
  // ✅ Stateless 패턴: initialize()는 선택적
  // 설정 검증 + 기본 디렉토리 생성

  std::vector<std::string> errors;
  bool valid = validateConfig(config, errors);

  if (!valid) {
    for (const auto &error : errors) {
      LogManager::getInstance().Error("초기화 검증 실패: " + error);
    }
    return false;
  }

  // 기본 디렉토리 생성 (선택적)
  if (config.value("create_directories", true)) {
    try {
      std::string base_path = extractBasePath(config);
      if (!std::filesystem::exists(base_path)) {
        std::filesystem::create_directories(base_path);
        LogManager::getInstance().Info("기본 디렉토리 생성: " + base_path);
      }
    } catch (const std::exception &e) {
      LogManager::getInstance().Warn("디렉토리 생성 실패: " +
                                     std::string(e.what()));
    }
  }

  LogManager::getInstance().Info("파일 타겟 핸들러 초기화 완료 (Stateless)");
  return true;
}

TargetSendResult FileTargetHandler::sendAlarm(const AlarmMessage &alarm,
                                              const ordered_json &config) {
  TargetSendResult result;
  result.target_type = "FILE";
  result.target_name = getTargetName(config);
  result.success = false;

  auto start_time = std::chrono::steady_clock::now();

  try {
    std::cout << "[DEBUG][FileTargetHandler] sendAlarm for: "
              << result.target_name << " point=" << alarm.nm << std::endl;
    LogManager::getInstance().Info("파일 알람 저장: " + result.target_name);

    // ✅ 파일 경로 생성 (config 기반)
    std::string file_path = generateFilePath(alarm, config);
    LogManager::getInstance().Debug("파일 경로: " + file_path);

    // ✅ 디렉토리 생성
    if (config.value("create_directories", true)) {
      createDirectoriesForFile(file_path);
    }

    // ✅ 파일 내용 생성 (config 기반)
    std::string content = buildFileContent(alarm, config);

    // ✅ 압축 처리 (선택적)
    bool compression_enabled = config.value("compression_enabled", false);
    if (compression_enabled) {
      LogManager::getInstance().Debug("압축은 실제 구현 시 zlib 사용");
      // content = compressContent(content);
      // file_path += ".gz";
    }

    // ✅ 파일 쓰기
    std::cout << "[DEBUG][FileTargetHandler] Writing file: " << file_path
              << std::endl;
    bool write_success = writeFile(file_path, content, config);

    if (write_success) {
      result.success = true;
      result.file_path = file_path;
      result.content_size = content.length();

      success_count_++;
      total_bytes_written_ += content.length();

      auto end_time = std::chrono::steady_clock::now();
      result.response_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time);

      std::cout << "[DEBUG][FileTargetHandler] File save SUCCESS: " << file_path
                << std::endl;
      LogManager::getInstance().Info(
          "[FileTargetHandler] 파일 저장 성공: " + file_path + " (" +
          std::to_string(result.content_size) + " bytes)");
    } else {
      result.error_message = "파일 저장 실패: " + file_path;
      LogManager::getInstance().Error("[FileTargetHandler] " +
                                      result.error_message);
      failure_count_++;
    }

    file_count_++;

  } catch (const std::exception &e) {
    result.error_message = "파일 저장 예외: " + std::string(e.what());
    LogManager::getInstance().Error(result.error_message);
    failure_count_++;
  }

  return result;
}

std::vector<TargetSendResult> FileTargetHandler::sendValueBatch(
    const std::vector<PulseOne::CSP::ValueMessage> &values,
    const ordered_json &config) {

  std::vector<TargetSendResult> results;
  TargetSendResult result;
  result.target_type = "FILE";
  result.target_name = getTargetName(config);
  result.success = false;

  auto start_time = std::chrono::steady_clock::now();

  try {
    LogManager::getInstance().Info("파일 값 배치 저장: " + result.target_name +
                                   " (" + std::to_string(values.size()) +
                                   "개)");

    // 1. 파일 경로 생성
    std::string base_path = extractBasePath(config);
    std::string filename_pattern =
        config.value("fileNamePattern", "export_{timestamp}.json");

    // {timestamp} 치환
    size_t pos = filename_pattern.find("{timestamp}");
    if (pos != std::string::npos) {
      filename_pattern.replace(pos, 11, generateTimestampString());
    }

    std::filesystem::path full_path(base_path);
    full_path /= filename_pattern;
    std::string file_path = full_path.string();

    // 2. 디렉토리 생성
    createDirectoriesForFile(file_path);

    // 3. JSON 배열 생성
    json data_array = json::array();
    for (const auto &val : values) {
      data_array.push_back(val.to_json());
    }

    std::string content = data_array.dump(2);

    // 4. 파일 쓰기
    bool write_success = writeFile(file_path, content, config);

    if (write_success) {
      result.success = true;
      result.file_path = file_path;
      result.content_size = content.length();

      success_count_++;
      total_bytes_written_ += content.length();

      auto end_time = std::chrono::steady_clock::now();
      result.response_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time);

      LogManager::getInstance().Info("✅ 파일 배치 저장 성공: " + file_path);
    } else {
      result.error_message = "파일 쓰기 실패: " + file_path;
      failure_count_++;
    }

    results.push_back(result);

  } catch (const std::exception &e) {
    result.error_message = "파일 배치 저장 예외: " + std::string(e.what());
    LogManager::getInstance().Error(result.error_message);
    failure_count_++;
    results.push_back(result);
  }

  return results;
}

bool FileTargetHandler::testConnection(const ordered_json &config) {
  try {
    LogManager::getInstance().Info("파일 시스템 연결 테스트");

    std::string base_path = extractBasePath(config);
    if (base_path.empty()) {
      LogManager::getInstance().Error("테스트 실패: base_path가 없음");
      return false;
    }

    // 디렉토리 존재 확인
    if (!std::filesystem::exists(base_path)) {
      if (config.value("create_directories", true)) {
        std::filesystem::create_directories(base_path);
        LogManager::getInstance().Info("테스트용 디렉토리 생성: " + base_path);
      } else {
        LogManager::getInstance().Error("디렉토리가 존재하지 않음: " +
                                        base_path);
        return false;
      }
    }

    // 쓰기 권한 테스트
    std::string test_file =
        base_path + "/test_" + generateTimestampString() + ".tmp";

    std::ofstream test_stream(test_file);
    if (!test_stream.is_open()) {
      LogManager::getInstance().Error("테스트 파일 생성 실패");
      return false;
    }

    test_stream << "test\n";
    test_stream.close();

    // 읽기 테스트
    std::ifstream read_stream(test_file);
    if (!read_stream.is_open()) {
      LogManager::getInstance().Error("테스트 파일 읽기 실패");
      return false;
    }

    std::string content;
    std::getline(read_stream, content);
    read_stream.close();

    // 정리
    std::filesystem::remove(test_file);

    bool success = (content == "test");
    if (success) {
      LogManager::getInstance().Info("✅ 파일 시스템 연결 테스트 성공");
    } else {
      LogManager::getInstance().Error("❌ 파일 내용 검증 실패");
    }

    return success;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("파일 시스템 테스트 예외: " +
                                    std::string(e.what()));
    return false;
  }
}

bool FileTargetHandler::validateConfig(const ordered_json &config,
                                       std::vector<std::string> &errors) {
  errors.clear();

  // base_path 검증
  if (!config.contains("base_path")) {
    errors.push_back("base_path 필드가 필수입니다");
    return false;
  }

  std::string base_path = extractBasePath(config);
  if (base_path.empty()) {
    errors.push_back("base_path가 비어있습니다");
    return false;
  }

  // file_format 검증 (선택적)
  if (config.contains("file_format")) {
    std::string format = extractFileFormat(config);
    std::vector<std::string> supported = {"json", "csv", "txt", "text", "xml"};
    if (std::find(supported.begin(), supported.end(), format) ==
        supported.end()) {
      errors.push_back("지원되지 않는 파일 형식: " + format);
      return false;
    }
  }

  return true;
}

ordered_json FileTargetHandler::getStatus() const {
  return ordered_json{{"type", "FILE"},
                      {"file_count", file_count_.load()},
                      {"success_count", success_count_.load()},
                      {"failure_count", failure_count_.load()},
                      {"total_bytes_written", total_bytes_written_.load()}};
}

void FileTargetHandler::cleanup() {
  // 통계 리셋 (선택적)
  // file_count_ = 0;
  // success_count_ = 0;
  // failure_count_ = 0;
  // total_bytes_written_ = 0;

  LogManager::getInstance().Info("FileTargetHandler 정리 완료");
}

// =============================================================================
// Private 핵심 메서드
// =============================================================================

std::string
FileTargetHandler::extractBasePath(const ordered_json &config) const {
  std::string base_path;
  if (config.contains("base_path") &&
      !config["base_path"].get<std::string>().empty()) {
    base_path = config["base_path"].get<std::string>();
  } else if (config.contains("path") &&
             !config["path"].get<std::string>().empty()) {
    base_path = config["path"].get<std::string>();
  }

  if (!base_path.empty()) {
    // 환경변수 치환
    auto &config_manager = ConfigManager::getInstance();
    return config_manager.expandVariables(base_path);
  }
  return "";
}

std::string
FileTargetHandler::extractFileFormat(const ordered_json &config) const {
  std::string format = config.value("file_format", "json");
  std::transform(format.begin(), format.end(), format.begin(), ::tolower);
  return format;
}

std::string
FileTargetHandler::generateFilePath(const AlarmMessage &alarm,
                                    const ordered_json &config) const {
  // base_path
  std::string base_path = extractBasePath(config);

  // directory_template
  std::string dir_template =
      config.value("directory_template", "{building_id}/{year}/{month}/{day}");
  std::string dir_path = expandTemplate(dir_template, alarm);

  // filename_template
  std::string file_format = extractFileFormat(config);
  std::string extension = getFileExtension(file_format);

  std::string filename_template = config.value(
      "filename_template",
      "{building_id}_{date}_{point_name}_{timestamp}_alarm." + extension);
  std::string filename = expandTemplate(filename_template, alarm);

  // 전체 경로 결합
  std::filesystem::path full_path(base_path);
  if (!dir_path.empty()) {
    full_path /= dir_path;
  }
  full_path /= filename;

  return full_path.string();
}

void FileTargetHandler::createDirectoriesForFile(
    const std::string &file_path) const {
  try {
    std::filesystem::path path(file_path);
    std::filesystem::path parent_dir = path.parent_path();

    if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
      std::filesystem::create_directories(parent_dir);
      LogManager::getInstance().Debug("디렉토리 생성: " + parent_dir.string());
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("디렉토리 생성 실패: " +
                                    std::string(e.what()));
  }
}

std::string
FileTargetHandler::buildFileContent(const AlarmMessage &alarm,
                                    const ordered_json &config) const {
  std::string format = extractFileFormat(config);

  if (format == "json") {
    return buildJsonContent(alarm, config);
  } else if (format == "csv") {
    return buildCsvContent(alarm, config);
  } else if (format == "txt" || format == "text") {
    return buildTextContent(alarm, config);
  } else if (format == "xml") {
    return buildXmlContent(alarm, config);
  } else {
    LogManager::getInstance().Warn("알 수 없는 형식 (" + format +
                                   "), JSON 사용");
    return buildJsonContent(alarm, config);
  }
}

std::string
FileTargetHandler::buildJsonContent(const AlarmMessage &alarm,
                                    const ordered_json &config) const {
  json request_body;

  // ✅ 템플릿이 있으면 템플릿을 기반으로 생성 (기본 필드 무시)
  if (config.contains("body_template")) {
    request_body = config["body_template"];
    expandTemplateVariables(request_body, alarm);

    // 이미 배열이면 그대로 반환, 객체면 배열로 감쌈
    if (request_body.is_array()) {
      return request_body.dump(); // compact JSON
    } else {
      return json::array({request_body}).dump();
    }
  }

  // ✅ 템플릿이 없으면 기본 AlarmMessage 포맷 사용
  request_body["bd"] = alarm.bd;
  request_body["nm"] = alarm.nm;
  request_body["vl"] = alarm.vl;
  request_body["tm"] = alarm.tm;
  request_body["al"] = alarm.al;
  request_body["st"] = alarm.st;
  request_body["des"] = alarm.des;

  // 메타데이터 (기본)
  request_body["source"] = "PulseOne-CSPGateway";
  request_body["version"] = "2.0";
  request_body["alarm_status"] = alarm.get_alarm_status_string();

  // ✅ 사용자 요청 포맷: 배열로 감싸서 반환
  return json::array({request_body}).dump();
}

std::string
FileTargetHandler::buildCsvContent(const AlarmMessage &alarm,
                                   const ordered_json &config) const {
  std::ostringstream csv;

  // 헤더 (append 모드가 아닐 때만)
  if (config.value("csv_add_header", true) &&
      !config.value("append_mode", false)) {
    csv << "bd,nm,vl,tm,al,st,des,file_timestamp\n";
  }

  // 데이터 행
  csv << alarm.bd << ",";
  csv << "\"" << alarm.nm << "\",";
  csv << alarm.vl << ",";
  csv << "\"" << alarm.tm << "\",";
  csv << alarm.al << ",";
  csv << alarm.st << ",";
  csv << "\"" << alarm.des << "\",";
  csv << "\"" << getCurrentTimestamp() << "\"\n";

  return csv.str();
}

std::string
FileTargetHandler::buildTextContent(const AlarmMessage &alarm,
                                    const ordered_json &config) const {
  std::ostringstream text;

  std::string format = config.value("text_format", "default");

  if (format == "syslog") {
    text << getCurrentTimestamp() << " PulseOne: ";
    text << "ALARM [BD:" << alarm.bd << "] ";
    text << "[" << alarm.nm << "=" << alarm.vl << "] ";
    text << alarm.des;
  } else {
    text << "[" << getCurrentTimestamp() << "] ";
    text << "Building " << alarm.bd << " - ";
    text << alarm.nm << " = " << alarm.vl << " ";
    text << "(" << alarm.get_alarm_status_string() << ")";
    if (!alarm.des.empty()) {
      text << " - " << alarm.des;
    }
  }

  if (config.value("append_mode", false)) {
    text << "\n";
  }

  return text.str();
}

std::string
FileTargetHandler::buildXmlContent(const AlarmMessage &alarm,
                                   const ordered_json &config) const {
  std::ostringstream xml;

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  xml << "<alarm>\n";
  xml << "  <bd>" << alarm.bd << "</bd>\n";
  xml << "  <nm>" << escapeXml(alarm.nm) << "</nm>\n";
  xml << "  <vl>" << alarm.vl << "</vl>\n";
  xml << "  <tm>" << alarm.tm << "</tm>\n";
  xml << "  <al>" << alarm.al << "</al>\n";
  xml << "  <st>" << alarm.st << "</st>\n";
  xml << "  <des>" << escapeXml(alarm.des) << "</des>\n";
  xml << "  <source>PulseOne-CSPGateway</source>\n";
  xml << "  <file_timestamp>" << getCurrentTimestamp() << "</file_timestamp>\n";
  xml << "</alarm>\n";

  return xml.str();
}

bool FileTargetHandler::writeFile(const std::string &file_path,
                                  const std::string &content,
                                  const ordered_json &config) const {
  LogManager::getInstance().Debug("[FileTargetHandler] Writing file to: " +
                                  file_path);
  try {
    bool atomic_write = config.value("atomic_write", true);
    bool append_mode = config.value("append_mode", false);

    if (atomic_write && !append_mode) {
      // 원자적 쓰기: 임시 파일 → rename
      std::string temp_path = file_path + ".tmp." + generateTimestampString();
      LogManager::getInstance().Debug(
          "[FileTargetHandler] Atomic write using temp: " + temp_path);

      std::ofstream temp_file(temp_path);
      if (!temp_file.is_open()) {
        LogManager::getInstance().Error(
            "[FileTargetHandler] Failed to open temp file: " + temp_path);
        return false;
      }

      temp_file << content;
      temp_file.close();

      if (temp_file.fail()) {
        LogManager::getInstance().Error(
            "[FileTargetHandler] Failed to write content to temp file");
        std::filesystem::remove(temp_path);
        return false;
      }

      LogManager::getInstance().Debug("[FileTargetHandler] Renaming " +
                                      temp_path + " to " + file_path);
      std::filesystem::rename(temp_path, file_path);
      LogManager::getInstance().Debug(
          "[FileTargetHandler] Atomic write successful");

    } else {
      // 직접 쓰기
      std::ios_base::openmode mode = std::ios::out;
      if (append_mode) {
        mode |= std::ios::app;
      }

      std::ofstream file(file_path, mode);
      if (!file.is_open()) {
        LogManager::getInstance().Error(
            "[FileTargetHandler] Failed to open file for writing: " +
            file_path);
        return false;
      }

      file << content;
      file.close();

      if (file.fail()) {
        LogManager::getInstance().Error(
            "[FileTargetHandler] Failed to write content to file");
        return false;
      }

      LogManager::getInstance().Debug(
          "[FileTargetHandler] Direct write successful");
    }

    return true;
  } catch (const std::exception &e) {
    LogManager::getInstance().Error(
        "[FileTargetHandler] Exception in writeFile: " + std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string FileTargetHandler::expandTemplate(const std::string &template_str,
                                              const AlarmMessage &alarm) const {
  std::string result = template_str;

  auto replaceAll = [](std::string &str, const std::string &from,
                       const std::string &to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
      str.replace(pos, from.length(), to);
      pos += to.length();
    }
  };

  // 환경변수 치환
  auto &config_manager = ConfigManager::getInstance();
  result = config_manager.expandVariables(result);

  // 알람 변수 치환
  replaceAll(result, "{building_id}", std::to_string(alarm.bd));
  replaceAll(result, "{point_name}", sanitizeFilename(alarm.nm));
  replaceAll(result, "{value}", std::to_string(alarm.vl));
  replaceAll(result, "{alarm_flag}", std::to_string(alarm.al));
  replaceAll(result, "{status}", std::to_string(alarm.st));
  replaceAll(result, "{timestamp}", generateTimestampString());
  replaceAll(result, "{date}", generateDateString());
  replaceAll(result, "{year}", generateYearString());
  replaceAll(result, "{month}", generateMonthString());
  replaceAll(result, "{day}", generateDayString());
  replaceAll(result, "{hour}", generateHourString());
  replaceAll(result, "{alarm_status}",
             sanitizeFilename(alarm.get_alarm_status_string()));

  return result;
}

void FileTargetHandler::expandTemplateVariables(
    json &template_json, const AlarmMessage &alarm) const {
  try {
    std::string target_field_name = "";
    std::string target_description = "";
    std::string converted_value = std::to_string(alarm.vl);

    auto &transformer =
        ::PulseOne::Transform::PayloadTransformer::getInstance();
    auto context = transformer.createContext(
        alarm, target_field_name, target_description, converted_value);

    template_json = transformer.transform(template_json, context);

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("FileTargetHandler 템플릿 변환 실패: " +
                                    std::string(e.what()));
  }
}

std::string
FileTargetHandler::sanitizeFilename(const std::string &filename) const {
  std::string result = filename;

  // 금지 문자 치환
  const std::string forbidden = "<>:\"/\\|?*";
  for (char &c : result) {
    if (forbidden.find(c) != std::string::npos || c < 32 || c == 127) {
      c = '_';
    }
  }

  // 중복 언더스코어 제거
  size_t pos = 0;
  while ((pos = result.find("__", pos)) != std::string::npos) {
    result.replace(pos, 2, "_");
  }

  // 앞뒤 공백 제거
  result.erase(0, result.find_first_not_of(" \t."));
  result.erase(result.find_last_not_of(" \t.") + 1);

  if (result.empty()) {
    result = "unknown";
  }

  return result;
}

std::string FileTargetHandler::getTargetName(const ordered_json &config) const {
  if (config.contains("name") && config["name"].is_string()) {
    return config["name"].get<std::string>();
  }

  std::string base_path = extractBasePath(config);
  if (!base_path.empty()) {
    return "FILE://" + base_path;
  }

  return "FILE-Target";
}

std::string FileTargetHandler::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
  oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
  return oss.str();
}

std::string FileTargetHandler::generateTimestampString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
  return oss.str();
}

std::string FileTargetHandler::generateDateString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
  return oss.str();
}

std::string FileTargetHandler::generateYearString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y");
  return oss.str();
}

std::string FileTargetHandler::generateMonthString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%m");
  return oss.str();
}

std::string FileTargetHandler::generateDayString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%d");
  return oss.str();
}

std::string FileTargetHandler::generateHourString() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%H");
  return oss.str();
}

std::string FileTargetHandler::escapeXml(const std::string &text) const {
  std::string result;
  result.reserve(text.length() + 16);

  for (char c : text) {
    switch (c) {
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '&':
      result += "&amp;";
      break;
    case '"':
      result += "&quot;";
      break;
    case '\'':
      result += "&apos;";
      break;
    default:
      result += c;
      break;
    }
  }

  return result;
}

std::string
FileTargetHandler::getFileExtension(const std::string &format) const {
  if (format == "json")
    return "json";
  if (format == "csv")
    return "csv";
  if (format == "txt" || format == "text")
    return "txt";
  if (format == "xml")
    return "xml";
  return "dat";
}

} // namespace CSP
} // namespace PulseOne