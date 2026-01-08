// =============================================================================
// collector/src/Api/LogApiCallbacks.cpp
// 로그 조회 및 관리 REST API 콜백 구현
// =============================================================================

#include "Api/LogApiCallbacks.h"
#include "Network/RestApiServer.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

namespace PulseOne {
namespace Api {

void LogApiCallbacks::Setup(Network::RestApiServer* server) {
    if (!server) return;

    server->SetLogListCallback([](const std::string& path) -> nlohmann::json {
        return ListLogFiles(path);
    });

    server->SetLogReadCallback([](const std::string& path, int lines, int offset) -> std::string {
        return ReadLogFile(path, lines, offset);
    });
}

nlohmann::json LogApiCallbacks::ListLogFiles(const std::string& path) {
    nlohmann::json result = nlohmann::json::array();
    std::string base_path = "./logs";
    
    fs::path target_path(base_path);
    if (!path.empty()) {
        target_path /= path;
    }

    try {
        if (!fs::exists(target_path)) {
            return result;
        }

        if (!fs::is_directory(target_path)) {
            // 파일인 경우 단일 항목 반환? (하지만 List 용도이므로 빈 배열 반환이 일반적)
            return result;
        }

        for (const auto& entry : fs::directory_iterator(target_path)) {
            nlohmann::json item;
            item["name"] = entry.path().filename().string();
            item["is_directory"] = entry.is_directory();
            
            // base_path로부터의 상대 경로 저장
            std::string rel_path = fs::relative(entry.path(), base_path).string();
            item["path"] = rel_path;
            
            if (!entry.is_directory()) {
                item["size"] = fs::file_size(entry.path());
                
                // 수정 시간 획득
                try {
                    auto ftime = fs::last_write_time(entry.path());
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
                    item["mtime"] = cftime;
                } catch (...) {
                    item["mtime"] = 0;
                }
            }
            result.push_back(item);
        }
        
        // 정렬 logic: 디렉토리 우선 -> 그 다음 알파벳순 (단, 날짜 형식 폴더는 역순이 보기 좋음)
        std::sort(result.begin(), result.end(), [](const nlohmann::json& a, const nlohmann::json& b) {
            if (a["is_directory"] != b["is_directory"]) {
                return a["is_directory"].get<bool>() > b["is_directory"].get<bool>();
            }
            // 이름 역순 (주로 날짜 때문)
            return a["name"].get<std::string>() > b["name"].get<std::string>();
        });

    } catch (const std::exception& e) {
        // 실제 운영 환경에서는 LogManager를 통해 기록하겠지만, 
        // 여기서는 std::cerr를 사용하거나 빈 결과를 반환
        // LogManager::getInstance().Error("Log listing error: " + std::string(e.what()));
    }

    return result;
}

std::string LogApiCallbacks::ReadLogFile(const std::string& path, int lines, int offset) {
    std::string base_path = "./logs";
    fs::path file_path = fs::path(base_path) / path;

    // 경로 조작 방지 (보안: .. 방지)
    std::string normalized_path = fs::absolute(file_path).string();
    std::string normalized_base = fs::absolute(fs::path(base_path)).string();
    
    if (normalized_path.find(normalized_base) != 0) {
        return "Access denied: Path escape detected";
    }

    if (!fs::exists(file_path) || fs::is_directory(file_path)) {
        return "File not found: " + path;
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "Failed to open file: " + path;
    }

    std::string content;
    std::string line;
    int current_line = 0;
    
    // 단순 라인 읽기 (최적화는 추후 뒤에서부터 읽기 등으로 고려)
    while (std::getline(file, line)) {
        if (current_line >= offset) {
            content += line + "\n";
            if (content.length() > 2 * 1024 * 1024) { // 2MB 제한 (브라우저 부하 방지)
                content += "... [Truncated due to result size limit (2MB)]";
                break;
            }
            if (current_line - offset >= lines - 1) {
                break;
            }
        }
        current_line++;
    }

    return content;
}

} // namespace Api
} // namespace PulseOne
