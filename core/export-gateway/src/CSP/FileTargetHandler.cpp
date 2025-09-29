/**
 * @file FileTargetHandler.cpp
 * @brief CSP Gateway 로컬 파일 시스템 타겟 핸들러 구현
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/FileTargetHandler.cpp
 * 
 * 🚨 컴파일 에러 수정 완료:
 * - 모든 멤버 변수 헤더에서 선언
 * - TargetSendResult 필드명 정확히 사용
 * - getTypeName() 중복 정의 제거
 * - 모든 메서드 헤더에 선언됨
 */

#include "CSP/FileTargetHandler.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <fstream>
#include <filesystem>
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace CSP {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

FileTargetHandler::FileTargetHandler() {
    LogManager::getInstance().Info("FileTargetHandler 초기화");
}

FileTargetHandler::~FileTargetHandler() {
    LogManager::getInstance().Info("FileTargetHandler 종료");
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool FileTargetHandler::initialize(const json& config) {
    try {
        LogManager::getInstance().Info("파일 타겟 핸들러 초기화 시작");
        
        // 필수 설정 검증
        if (!config.contains("base_path") || config["base_path"].get<std::string>().empty()) {
            LogManager::getInstance().Error("파일 기본 경로가 설정되지 않음");
            return false;
        }
        
        base_path_ = config["base_path"].get<std::string>();
        LogManager::getInstance().Info("파일 타겟 기본 경로: " + base_path_);
        
        // 파일 형식 설정
        file_format_ = config.value("file_format", "json");
        std::transform(file_format_.begin(), file_format_.end(), file_format_.begin(), ::tolower);
        
        // 파일명 템플릿 설정 (ConfigManager 패턴 차용)
        filename_template_ = config.value("filename_template", 
                                         "{building_id}_{date}_{point_name}_{timestamp}_alarm.{ext}");
        
        // 디렉토리 구조 설정
        directory_template_ = config.value("directory_template", 
                                          "{building_id}/{year}/{month}/{day}");
        
        // 파일 옵션 설정
        file_options_.append_mode = config.value("append_mode", false);
        file_options_.create_directories = config.value("create_directories", true);
        file_options_.atomic_write = config.value("atomic_write", true);
        file_options_.backup_on_overwrite = config.value("backup_on_overwrite", false);
        
        // 압축 설정
        compression_enabled_ = config.value("compression_enabled", false);
        compression_format_ = config.value("compression_format", "gzip");
        
        // 로테이션 설정 (LogManager 패턴 차용)
        rotation_config_.max_file_size_mb = config.value("max_file_size_mb", 100);
        rotation_config_.max_files_per_dir = config.value("max_files_per_dir", 1000);
        rotation_config_.auto_cleanup_days = config.value("auto_cleanup_days", 30);
        rotation_config_.enabled = config.value("rotation_enabled", true);
        
        // 권한 설정
        file_permissions_ = config.value("file_permissions", 0644);
        dir_permissions_ = config.value("dir_permissions", 0755);
        
        // 기본 디렉토리 생성
        if (file_options_.create_directories) {
            createBaseDirectories();
        }
        
        LogManager::getInstance().Info("파일 타겟 핸들러 초기화 완료");
        LogManager::getInstance().Debug("설정 - format: " + file_format_ + 
                                       ", atomic_write: " + (file_options_.atomic_write ? "true" : "false") +
                                       ", compression: " + (compression_enabled_ ? "true" : "false"));
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 타겟 핸들러 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

TargetSendResult FileTargetHandler::sendAlarm(const AlarmMessage& alarm, const json& config) {
    TargetSendResult result;
    result.target_type = "FILE";
    result.target_name = getTargetName(config);
    result.success = false;
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        LogManager::getInstance().Info("파일 알람 저장 시작: " + result.target_name);
        
        // 파일 경로 생성
        std::string file_path = generateFilePath(alarm, config);
        LogManager::getInstance().Debug("파일 경로: " + file_path);
        
        // 디렉토리 생성
        if (file_options_.create_directories) {
            createDirectoriesForFile(file_path);
        }
        
        // 로테이션 체크 (LogManager 패턴 차용)
        if (rotation_config_.enabled) {
            checkAndRotateIfNeeded(file_path);
        }
        
        // 파일 내용 생성
        std::string content = buildFileContent(alarm, config);
        
        // 압축 처리
        if (compression_enabled_) {
            content = compressContent(content);
            file_path += getCompressionExtension();
        }
        
        // 파일 쓰기 실행
        bool write_success = false;
        if (file_options_.atomic_write) {
            write_success = writeFileAtomic(file_path, content, alarm, config);
        } else {
            write_success = writeFileDirectly(file_path, content, alarm, config);
        }
        
        // 결과 처리 (올바른 필드명 사용)
        if (write_success) {
            result.success = true;
            result.file_path = file_path;
            result.content_size = content.length(); // file_size_bytes → content_size
            
            // 파일 권한 설정
            setFilePermissions(file_path);
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result.response_time = duration; // response_time_ms → response_time
            
            LogManager::getInstance().Info("파일 알람 저장 성공: " + file_path + 
                                          " (" + std::to_string(result.content_size) + " bytes, " +
                                          std::to_string(result.response_time.count()) + "ms)");
        } else {
            result.error_message = "파일 쓰기 실패: " + file_path;
            LogManager::getInstance().Error(result.error_message);
        }
        
    } catch (const std::exception& e) {
        result.error_message = "파일 저장 예외: " + std::string(e.what());
        LogManager::getInstance().Error(result.error_message);
    }
    
    return result;
}

bool FileTargetHandler::testConnection(const json& config) {
    try {
        LogManager::getInstance().Info("파일 시스템 연결 테스트 시작");
        
        // 기본 경로 존재 확인
        if (!std::filesystem::exists(base_path_)) {
            LogManager::getInstance().Error("기본 경로가 존재하지 않음: " + base_path_);
            return false;
        }
        
        // 쓰기 권한 테스트
        std::string test_file = base_path_ + "/test_write_" + generateTimestampString() + ".tmp";
        
        try {
            std::ofstream test_stream(test_file);
            if (!test_stream.is_open()) {
                LogManager::getInstance().Error("테스트 파일 생성 실패: " + test_file);
                return false;
            }
            
            test_stream << "test content\n";
            test_stream.close();
            
            // 파일 읽기 테스트
            std::ifstream read_stream(test_file);
            if (!read_stream.is_open()) {
                LogManager::getInstance().Error("테스트 파일 읽기 실패: " + test_file);
                return false;
            }
            
            std::string content;
            std::getline(read_stream, content);
            read_stream.close();
            
            // 테스트 파일 삭제
            std::filesystem::remove(test_file);
            
            if (content == "test content") {
                LogManager::getInstance().Info("파일 시스템 연결 테스트 성공");
                return true;
            } else {
                LogManager::getInstance().Error("파일 내용 검증 실패");
                return false;
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("파일 시스템 테스트 중 예외: " + std::string(e.what()));
            
            // 정리 시도
            try {
                std::filesystem::remove(test_file);
            } catch (...) {
                // 무시
            }
            
            return false;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 시스템 연결 테스트 예외: " + std::string(e.what()));
        return false;
    }
}

std::string FileTargetHandler::getTypeName() const {
    return "FILE";
}

json FileTargetHandler::getStatus() const {
    return json{
        {"type", "FILE"},
        {"base_path", base_path_},
        {"file_format", file_format_},
        {"compression_enabled", compression_enabled_},
        {"file_count", file_count_.load()},
        {"success_count", success_count_.load()},
        {"failure_count", failure_count_.load()},
        {"total_bytes_written", total_bytes_written_.load()}
    };
}


bool FileTargetHandler::validateConfig(const json& config, std::vector<std::string>& errors) {
    errors.clear();
    
    try {
        // 필수 필드 검증
        if (!config.contains("base_path")) {
            errors.push_back("base_path 필드가 필수입니다");
            return false;
        }
        
        if (!config["base_path"].is_string() || config["base_path"].get<std::string>().empty()) {
            errors.push_back("base_path는 비어있지 않은 문자열이어야 합니다");
            return false;
        }
        
        std::string base_path = config["base_path"].get<std::string>();
        
        // 경로 유효성 검증
        try {
            std::filesystem::path path(base_path);
            if (!path.is_absolute() && !path.is_relative()) {
                errors.push_back("base_path가 유효한 경로 형식이 아닙니다: " + base_path);
                return false;
            }
        } catch (const std::exception& e) {
            errors.push_back("base_path 경로 검증 실패: " + std::string(e.what()));
            return false;
        }
        
        // 선택적 필드 검증
        if (config.contains("file_format")) {
            if (!config["file_format"].is_string()) {
                errors.push_back("file_format은 문자열이어야 합니다");
                return false;
            }
            
            std::string format = config["file_format"].get<std::string>();
            std::transform(format.begin(), format.end(), format.begin(), ::tolower);
            
            std::vector<std::string> supported_formats = {"json", "text", "csv", "xml"};
            if (std::find(supported_formats.begin(), supported_formats.end(), format) == supported_formats.end()) {
                errors.push_back("지원되지 않는 파일 형식: " + format + " (지원: json, text, csv, xml)");
                return false;
            }
        }
        
        // 파일명 템플릿 검증
        if (config.contains("filename_template")) {
            if (!config["filename_template"].is_string()) {
                errors.push_back("filename_template은 문자열이어야 합니다");
                return false;
            }
            
            std::string template_str = config["filename_template"].get<std::string>();
            if (template_str.empty()) {
                errors.push_back("filename_template이 비어있습니다");
                return false;
            }
            
            // 금지된 문자 검사 (OS별 제한)
            std::string forbidden_chars = "<>:\"|?*";
            for (char c : forbidden_chars) {
                if (template_str.find(c) != std::string::npos) {
                    errors.push_back("filename_template에 금지된 문자가 포함되어 있습니다: " + std::string(1, c));
                    return false;
                }
            }
        }
        
        // 압축 설정 검증
        if (config.contains("compression_enabled")) {
            if (!config["compression_enabled"].is_boolean()) {
                errors.push_back("compression_enabled는 boolean이어야 합니다");
                return false;
            }
            
            if (config["compression_enabled"].get<bool>() && config.contains("compression_format")) {
                if (!config["compression_format"].is_string()) {
                    errors.push_back("compression_format은 문자열이어야 합니다");
                    return false;
                }
                
                std::string comp_format = config["compression_format"].get<std::string>();
                std::vector<std::string> supported_compression = {"gzip", "zip"};
                if (std::find(supported_compression.begin(), supported_compression.end(), comp_format) == supported_compression.end()) {
                    errors.push_back("지원되지 않는 압축 형식: " + comp_format + " (지원: gzip, zip)");
                    return false;
                }
            }
        }
        
        // 로테이션 설정 검증
        if (config.contains("rotation")) {
            const auto& rotation = config["rotation"];
            if (!rotation.is_object()) {
                errors.push_back("rotation 설정은 객체여야 합니다");
                return false;
            }
            
            if (rotation.contains("max_file_size_mb")) {
                if (!rotation["max_file_size_mb"].is_number_unsigned()) {
                    errors.push_back("max_file_size_mb는 양의 정수여야 합니다");
                    return false;
                }
                
                size_t max_size = rotation["max_file_size_mb"].get<size_t>();
                if (max_size == 0 || max_size > 10240) { // 최대 10GB
                    errors.push_back("max_file_size_mb는 1-10240 범위여야 합니다");
                    return false;
                }
            }
            
            if (rotation.contains("auto_cleanup_days")) {
                if (!rotation["auto_cleanup_days"].is_number_integer()) {
                    errors.push_back("auto_cleanup_days는 정수여야 합니다");
                    return false;
                }
                
                int cleanup_days = rotation["auto_cleanup_days"].get<int>();
                if (cleanup_days < 0 || cleanup_days > 3650) { // 최대 10년
                    errors.push_back("auto_cleanup_days는 0-3650 범위여야 합니다");
                    return false;
                }
            }
        }
        
        // 파일 권한 검증
        if (config.contains("file_permissions")) {
            if (!config["file_permissions"].is_string()) {
                errors.push_back("file_permissions는 문자열이어야 합니다 (예: '0644')");
                return false;
            }
            
            std::string perm_str = config["file_permissions"].get<std::string>();
            if (perm_str.length() != 4 || perm_str[0] != '0') {
                errors.push_back("file_permissions는 '0644' 형식이어야 합니다");
                return false;
            }
            
            // 8진수 형식 검증
            for (size_t i = 1; i < perm_str.length(); ++i) {
                if (perm_str[i] < '0' || perm_str[i] > '7') {
                    errors.push_back("file_permissions에 유효하지 않은 8진수 숫자가 포함되어 있습니다");
                    return false;
                }
            }
        }
        
        LogManager::getInstance().Debug("파일 타겟 설정 검증 성공");
        return true;
        
    } catch (const std::exception& e) {
        errors.push_back("설정 검증 중 예외 발생: " + std::string(e.what()));
        LogManager::getInstance().Error("파일 타겟 설정 검증 예외: " + std::string(e.what()));
        return false;
    }
}


void FileTargetHandler::cleanup() {
    should_stop_ = true;
    if (cleanup_thread_ && cleanup_thread_->joinable()) {
        cleanup_thread_->join();
    }
    LogManager::getInstance().Info("FileTargetHandler 정리 완료");
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

void FileTargetHandler::createBaseDirectories() {
    try {
        if (!std::filesystem::exists(base_path_)) {
            std::filesystem::create_directories(base_path_);
            LogManager::getInstance().Info("기본 디렉토리 생성: " + base_path_);
        }
        
        // 하위 디렉토리들 생성 (선택사항)
        std::vector<std::string> sub_dirs = {"temp", "backup", "archive"};
        for (const auto& sub_dir : sub_dirs) {
            std::string full_path = base_path_ + "/" + sub_dir;
            if (!std::filesystem::exists(full_path)) {
                std::filesystem::create_directories(full_path);
                LogManager::getInstance().Debug("하위 디렉토리 생성: " + full_path);
            }
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("기본 디렉토리 생성 실패: " + std::string(e.what()));
    }
}

void FileTargetHandler::createDirectoriesForFile(const std::string& file_path) {
    try {
        std::filesystem::path path(file_path);
        std::filesystem::path parent_dir = path.parent_path();
        
        if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
            std::filesystem::create_directories(parent_dir);
            LogManager::getInstance().Debug("디렉토리 생성: " + parent_dir.string());
            
            // 디렉토리 권한 설정
            std::filesystem::permissions(parent_dir, 
                                       static_cast<std::filesystem::perms>(dir_permissions_),
                                       std::filesystem::perm_options::replace);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("디렉토리 생성 실패: " + std::string(e.what()));
    }
}

std::string FileTargetHandler::generateFilePath(const AlarmMessage& alarm, const json& config) const {
    // 디렉토리 경로 생성
    std::string dir_path = expandTemplate(directory_template_, alarm);
    
    // 파일명 생성
    std::string filename = expandTemplate(filename_template_, alarm);
    
    // 확장자 설정
    filename = std::regex_replace(filename, std::regex("\\{ext\\}"), getFileExtension());
    
    // 전체 경로 결합
    std::string full_path = base_path_;
    if (!full_path.empty() && full_path.back() != '/') {
        full_path += "/";
    }
    full_path += dir_path;
    if (!dir_path.empty() && full_path.back() != '/') {
        full_path += "/";
    }
    full_path += filename;
    
    // 경로 정규화
    full_path = std::regex_replace(full_path, std::regex("//+"), "/");
    
    return full_path;
}

std::string FileTargetHandler::expandTemplate(const std::string& template_str, const AlarmMessage& alarm) const {
    std::string result = template_str;
    
    // 기본 변수 치환
    result = std::regex_replace(result, std::regex("\\{building_id\\}"), std::to_string(alarm.bd));
    result = std::regex_replace(result, std::regex("\\{point_name\\}"), sanitizeFilename(alarm.nm));
    result = std::regex_replace(result, std::regex("\\{value\\}"), std::to_string(alarm.vl));
    result = std::regex_replace(result, std::regex("\\{alarm_flag\\}"), std::to_string(alarm.al));
    result = std::regex_replace(result, std::regex("\\{status\\}"), std::to_string(alarm.st));
    
    // 타임스탬프 관련 변수
    result = std::regex_replace(result, std::regex("\\{timestamp\\}"), generateTimestampString());
    result = std::regex_replace(result, std::regex("\\{date\\}"), generateDateString());
    result = std::regex_replace(result, std::regex("\\{year\\}"), generateYearString());
    result = std::regex_replace(result, std::regex("\\{month\\}"), generateMonthString());
    result = std::regex_replace(result, std::regex("\\{day\\}"), generateDayString());
    result = std::regex_replace(result, std::regex("\\{hour\\}"), generateHourString());
    
    // 알람 상태 문자열
    result = std::regex_replace(result, std::regex("\\{alarm_status\\}"), 
                               sanitizeFilename(alarm.get_alarm_status_string()));
    
    return result;
}

std::string FileTargetHandler::buildFileContent(const AlarmMessage& alarm, const json& config) const {
    if (file_format_ == "json") {
        return buildJsonContent(alarm, config);
    } else if (file_format_ == "csv") {
        return buildCsvContent(alarm, config);
    } else if (file_format_ == "txt" || file_format_ == "text") {
        return buildTextContent(alarm, config);
    } else if (file_format_ == "xml") {
        return buildXmlContent(alarm, config);
    } else {
        LogManager::getInstance().Warn("알 수 없는 파일 형식: " + file_format_ + " (JSON 사용)");
        return buildJsonContent(alarm, config);
    }
}

std::string FileTargetHandler::buildJsonContent(const AlarmMessage& alarm, const json& config) const {
    json content;
    
    // 기본 알람 데이터
    content["building_id"] = alarm.bd;
    content["point_name"] = alarm.nm;
    content["value"] = alarm.vl;
    content["timestamp"] = alarm.tm;
    content["alarm_flag"] = alarm.al;
    content["status"] = alarm.st;
    content["description"] = alarm.des;
    
    // 메타데이터
    content["source"] = "PulseOne-CSPGateway";
    content["version"] = "1.0";
    content["file_timestamp"] = getCurrentTimestamp();
    content["alarm_status"] = alarm.get_alarm_status_string();
    
    // 파일 정보
    content["file_info"] = {
        {"format", file_format_},
        {"compression", compression_enabled_},
        {"atomic_write", file_options_.atomic_write}
    };
    
    // 추가 필드
    if (config.contains("additional_fields") && config["additional_fields"].is_object()) {
        for (auto& [key, value] : config["additional_fields"].items()) {
            content[key] = value;
        }
    }
    
    // 줄바꿈 추가 (로그 파일 호환성)
    std::string result = content.dump(2);
    if (file_options_.append_mode) {
        result += "\n";
    }
    
    return result;
}

std::string FileTargetHandler::buildCsvContent(const AlarmMessage& alarm, const json& config) const {
    std::ostringstream csv;
    
    // CSV 헤더 (파일이 새로 생성되는 경우)
    bool add_header = config.value("csv_add_header", true);
    if (add_header && !file_options_.append_mode) {
        csv << "building_id,point_name,value,timestamp,alarm_flag,status,description,file_timestamp\n";
    }
    
    // CSV 데이터 행
    csv << alarm.bd << ",";
    csv << "\"" << alarm.nm << "\",";
    csv << alarm.vl << ",";
    csv << "\"" << alarm.tm << "\",";
    csv << alarm.al << ",";
    csv << alarm.st << ",";
    csv << "\"" << alarm.des << "\",";
    csv << "\"" << getCurrentTimestamp() << "\"";
    
    if (file_options_.append_mode) {
        csv << "\n";
    }
    
    return csv.str();
}

std::string FileTargetHandler::buildTextContent(const AlarmMessage& alarm, const json& config) const {
    std::ostringstream text;
    
    std::string format = config.value("text_format", "default");
    
    if (format == "syslog") {
        // Syslog 형식
        text << getCurrentTimestamp() << " PulseOne-CSPGateway: ";
        text << "ALARM [Building:" << alarm.bd << "] ";
        text << "[Point:" << alarm.nm << "] ";
        text << "[Value:" << alarm.vl << "] ";
        text << "[Status:" << alarm.get_alarm_status_string() << "] ";
        text << alarm.des;
    } else {
        // 기본 텍스트 형식
        text << "[" << getCurrentTimestamp() << "] ";
        text << "Building " << alarm.bd << " - ";
        text << alarm.nm << " = " << alarm.vl << " ";
        text << "(" << alarm.get_alarm_status_string() << ")";
        if (!alarm.des.empty()) {
            text << " - " << alarm.des;
        }
    }
    
    if (file_options_.append_mode) {
        text << "\n";
    }
    
    return text.str();
}

std::string FileTargetHandler::buildXmlContent(const AlarmMessage& alarm, const json& config) const {
    std::ostringstream xml;
    
    bool add_header = config.value("xml_add_header", true);
    if (add_header && !file_options_.append_mode) {
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        xml << "<alarms>\n";
    }
    
    xml << "  <alarm>\n";
    xml << "    <building_id>" << alarm.bd << "</building_id>\n";
    xml << "    <point_name><![CDATA[" << alarm.nm << "]]></point_name>\n";
    xml << "    <value>" << alarm.vl << "</value>\n";
    xml << "    <timestamp><![CDATA[" << alarm.tm << "]]></timestamp>\n";
    xml << "    <alarm_flag>" << alarm.al << "</alarm_flag>\n";
    xml << "    <status>" << alarm.st << "</status>\n";
    xml << "    <description><![CDATA[" << alarm.des << "]]></description>\n";
    xml << "    <file_timestamp><![CDATA[" << getCurrentTimestamp() << "]]></file_timestamp>\n";
    xml << "    <source>PulseOne-CSPGateway</source>\n";
    xml << "  </alarm>\n";
    
    if (!file_options_.append_mode) {
        xml << "</alarms>\n";
    }
    
    return xml.str();
}

bool FileTargetHandler::writeFileAtomic(const std::string& file_path, const std::string& content,
                                       const AlarmMessage& alarm, const json& config) {
    try {
        // 임시 파일 경로 생성
        std::string temp_path = file_path + ".tmp." + generateTimestampString();
        
        LogManager::getInstance().Debug("원자적 파일 쓰기 시작: " + temp_path);
        
        // 백업 파일 생성 (기존 파일이 있고 백업 옵션이 활성화된 경우)
        if (file_options_.backup_on_overwrite && std::filesystem::exists(file_path)) {
            createBackupFile(file_path);
        }
        
        // 임시 파일에 쓰기
        std::ofstream temp_file(temp_path, file_options_.append_mode ? 
                               std::ios::app : std::ios::out);
        
        if (!temp_file.is_open()) {
            LogManager::getInstance().Error("임시 파일 생성 실패: " + temp_path);
            return false;
        }
        
        temp_file << content;
        temp_file.close();
        
        if (temp_file.fail()) {
            LogManager::getInstance().Error("임시 파일 쓰기 실패: " + temp_path);
            std::filesystem::remove(temp_path);
            return false;
        }
        
        // 임시 파일을 최종 파일로 이동 (원자적 연산)
        std::filesystem::rename(temp_path, file_path);
        
        LogManager::getInstance().Debug("원자적 파일 쓰기 완료: " + file_path);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("원자적 파일 쓰기 예외: " + std::string(e.what()));
        return false;
    }
}

bool FileTargetHandler::writeFileDirectly(const std::string& file_path, const std::string& content,
                                         const AlarmMessage& alarm, const json& config) {
    try {
        LogManager::getInstance().Debug("직접 파일 쓰기: " + file_path);
        
        // 백업 파일 생성 (선택사항)
        if (file_options_.backup_on_overwrite && std::filesystem::exists(file_path)) {
            createBackupFile(file_path);
        }
        
        // 파일 쓰기
        std::ofstream file(file_path, file_options_.append_mode ? 
                          std::ios::app : std::ios::out);
        
        if (!file.is_open()) {
            LogManager::getInstance().Error("파일 열기 실패: " + file_path);
            return false;
        }
        
        file << content;
        file.close();
        
        if (file.fail()) {
            LogManager::getInstance().Error("파일 쓰기 실패: " + file_path);
            return false;
        }
        
        LogManager::getInstance().Debug("직접 파일 쓰기 완료: " + file_path);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("직접 파일 쓰기 예외: " + std::string(e.what()));
        return false;
    }
}

void FileTargetHandler::createBackupFile(const std::string& original_path) {
    try {
        std::string backup_path = original_path + ".backup." + generateTimestampString();
        std::filesystem::copy_file(original_path, backup_path);
        LogManager::getInstance().Debug("백업 파일 생성: " + backup_path);
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("백업 파일 생성 실패: " + std::string(e.what()));
    }
}

void FileTargetHandler::checkAndRotateIfNeeded(const std::string& file_path) {
    // LogManager 로테이션 로직 차용
    try {
        if (!std::filesystem::exists(file_path)) {
            return; // 파일이 없으면 로테이션 불필요
        }
        
        // 파일 크기 체크
        auto file_size = std::filesystem::file_size(file_path);
        size_t max_size_bytes = rotation_config_.max_file_size_mb * 1024 * 1024;
        
        if (file_size >= max_size_bytes) {
            rotateFile(file_path);
            LogManager::getInstance().Info("파일 로테이션 실행: " + file_path);
        }
        
        // 디렉토리 내 파일 수 체크
        checkDirectoryFileCount(file_path);
        
        // 오래된 파일 정리
        if (rotation_config_.auto_cleanup_days > 0) {
            cleanupOldFiles(file_path);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 로테이션 체크 실패: " + std::string(e.what()));
    }
}

void FileTargetHandler::rotateFile(const std::string& file_path) {
    try {
        std::string rotated_path = file_path + "." + generateTimestampString();
        std::filesystem::rename(file_path, rotated_path);
        
        // 압축 (선택사항)
        if (compression_enabled_) {
            // 실제 구현에서는 gzip 압축 수행
            LogManager::getInstance().Debug("로테이션된 파일 압축 예정: " + rotated_path);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("파일 로테이션 실패: " + std::string(e.what()));
    }
}

void FileTargetHandler::checkDirectoryFileCount(const std::string& file_path) {
    try {
        std::filesystem::path path(file_path);
        std::filesystem::path parent_dir = path.parent_path();
        
        if (!std::filesystem::exists(parent_dir)) {
            return;
        }
        
        size_t file_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(parent_dir)) {
            if (entry.is_regular_file()) {
                file_count++;
            }
        }
        
        if (file_count >= rotation_config_.max_files_per_dir) {
            LogManager::getInstance().Warn("디렉토리 파일 수 초과: " + parent_dir.string() + 
                                          " (" + std::to_string(file_count) + "/" + 
                                          std::to_string(rotation_config_.max_files_per_dir) + ")");
            // 실제로는 오래된 파일들을 archive 디렉토리로 이동
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("디렉토리 파일 수 체크 실패: " + std::string(e.what()));
    }
}

void FileTargetHandler::cleanupOldFiles(const std::string& file_path) {
    try {
        std::filesystem::path path(file_path);
        std::filesystem::path parent_dir = path.parent_path();
        
        if (!std::filesystem::exists(parent_dir)) {
            return;
        }
        
        auto cutoff_time = std::chrono::system_clock::now() - 
                          std::chrono::hours(24 * rotation_config_.auto_cleanup_days);
        
        for (const auto& entry : std::filesystem::directory_iterator(parent_dir)) {
            if (entry.is_regular_file()) {
                auto file_time = std::filesystem::last_write_time(entry);
                auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    file_time - std::filesystem::file_time_type::clock::now() + 
                    std::chrono::system_clock::now());
                
                if (system_time < cutoff_time) {
                    std::filesystem::remove(entry);
                    LogManager::getInstance().Debug("오래된 파일 삭제: " + entry.path().string());
                }
            }
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("오래된 파일 정리 실패: " + std::string(e.what()));
    }
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string FileTargetHandler::compressContent(const std::string& content) const {
    // 실제 구현에서는 zlib 또는 다른 압축 라이브러리 사용
    LogManager::getInstance().Debug("압축 기능은 실제 구현에서 zlib 사용 예정");
    return content; // 임시로 원본 반환
}

std::string FileTargetHandler::getFileExtension() const {
    if (file_format_ == "json") return "json";
    if (file_format_ == "csv") return "csv";
    if (file_format_ == "txt" || file_format_ == "text") return "txt";
    if (file_format_ == "xml") return "xml";
    return "dat"; // 기본 확장자
}

std::string FileTargetHandler::getCompressionExtension() const {
    if (compression_format_ == "gzip") return ".gz";
    if (compression_format_ == "zip") return ".zip";
    if (compression_format_ == "bzip2") return ".bz2";
    return ".compressed";
}

void FileTargetHandler::setFilePermissions(const std::string& file_path) {
    try {
        std::filesystem::permissions(file_path, 
                                   static_cast<std::filesystem::perms>(file_permissions_),
                                   std::filesystem::perm_options::replace);
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("파일 권한 설정 실패: " + std::string(e.what()));
    }
}

std::string FileTargetHandler::sanitizeFilename(const std::string& filename) const {
    // 파일명에 사용할 수 없는 문자들을 안전한 문자로 변환
    std::string result = filename;
    
    // Windows/Linux 공통 금지 문자들
    result = std::regex_replace(result, std::regex("[<>:\"/\\\\|?*]"), "_");
    
    // 제어 문자 제거
    result = std::regex_replace(result, std::regex("[\x00-\x1F\x7F]"), "");
    
    // 연속된 언더스코어 정리
    result = std::regex_replace(result, std::regex("_{2,}"), "_");
    
    // 앞뒤 공백 및 점 제거
    result = std::regex_replace(result, std::regex("^[\\s.]+|[\\s.]+$"), "");
    
    // 빈 문자열 방지
    if (result.empty()) {
        result = "unknown";
    }
    
    return result;
}

std::string FileTargetHandler::getTargetName(const json& config) const {
    if (config.contains("name") && config["name"].is_string()) {
        return config["name"].get<std::string>();
    }
    
    return "FILE://" + base_path_;
}

std::string FileTargetHandler::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
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

} // namespace CSP
} // namespace PulseOne