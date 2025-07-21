#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <vector>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

void ConfigManager::initialize() {
    std::vector<std::string> possible_paths = {
        "../config/.env",
        "./config/.env",
        "../../config/.env",
        "/etc/pulseone/.env",
        ".env"
    };
    
    bool found = false;
    for (const auto& path : possible_paths) {
        if (std::filesystem::exists(path)) {
            envFilePath = path;
            PulseOne::LogManager::getInstance().log("config", LogLevel::INFO, "Found config file: " + path);
            found = true;
            break;
        }
    }
    
    if (!found) {
        PulseOne::LogManager::getInstance().log("config", LogLevel::WARN, "No .env file found in any location");
        return;
    }

    std::ifstream file(envFilePath);
    if (!file.is_open()) {
        PulseOne::LogManager::getInstance().log("config", LogLevel::ERROR, ".env 열기 실패: " + envFilePath);
        return;
    }

    std::string line;
    int line_count = 0;
    int parsed_count = 0;
    
#ifdef DEBUG
    PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, "🔍 Parsing .env file line by line:");
#endif
    
    while (std::getline(file, line)) {
        line_count++;
        
#ifdef DEBUG
        // 디버그 모드일 때만 각 라인 파싱 과정 출력
        if (line_count <= 10) {
            PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
                "Line " + std::to_string(line_count) + ": [" + line + "]");
        }
#endif
        
        size_t original_size = configMap.size();
        parseLine(line);
        
        if (configMap.size() > original_size) {
            parsed_count++;
#ifdef DEBUG
            if (line_count <= 10) {
                PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
                    "  → Parsed successfully! ConfigMap now has " + std::to_string(configMap.size()) + " entries");
            }
#endif
        }
#ifdef DEBUG
        else if (line_count <= 10) {
            PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
                "  → Skipped (empty/comment/invalid)");
        }
#endif
    }

    PulseOne::LogManager::getInstance().log("config", LogLevel::INFO, 
        ".env 로딩 완료: " + envFilePath + 
        " (" + std::to_string(parsed_count) + "/" + std::to_string(line_count) + " entries)");
    
#ifdef DEBUG
    // 디버그 모드일 때만 전체 configMap 내용 출력
    PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
        "🗂️ Final configMap contents (" + std::to_string(configMap.size()) + " entries):");
    
    int count = 0;
    for (const auto& [key, value] : configMap) {
        count++;
        if (count <= 15) {
            PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
                "  [" + std::to_string(count) + "] " + key + " = '" + value + "'");
        } else if (count == 16) {
            PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
                "  ... (총 " + std::to_string(configMap.size()) + "개 - 나머지 생략)");
            break;
        }
    }
#endif
}

void ConfigManager::reload() {
    PulseOne::LogManager::getInstance().log("config", LogLevel::INFO, "환경설정 재로딩 시작");
    configMap.clear();
    initialize();
}

void ConfigManager::parseLine(const std::string& line) {
    if (line.empty() || line[0] == '#') return;

    size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) return;

    std::string key = line.substr(0, eqPos);
    std::string value = line.substr(eqPos + 1);

#ifdef DEBUG
    std::string original_key = key;
    std::string original_value = value;
#endif

    key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
    value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());

    std::lock_guard<std::mutex> lock(configMutex);
    configMap[key] = value;
    
#ifdef DEBUG
    // 디버그 모드일 때만 상세 파싱 로그
    if (configMap.size() <= 5) {
        PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
            "    Parsed: '" + original_key + "' = '" + original_value + 
            "' → '" + key + "' = '" + value + "'");
    }
#endif
}

std::string ConfigManager::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find(key);
    
#ifdef DEBUG
    // 디버그 모드일 때만 get 호출 로깅
    static int get_call_count = 0;
    get_call_count++;
    if (get_call_count <= 10) {
        std::string result = (it != configMap.end()) ? it->second : "";
        PulseOne::LogManager::getInstance().log("config", LogLevel::DEBUG, 
            "🔍 get('" + key + "') → '" + result + "' (존재: " + 
            (it != configMap.end() ? "YES" : "NO") + ")");
    }
#endif
    
    return (it != configMap.end()) ? it->second : "";
}

std::string ConfigManager::getOrDefault(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find(key);
    return (it != configMap.end()) ? it->second : defaultValue;
}

void ConfigManager::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(configMutex);
    configMap[key] = value;
}

bool ConfigManager::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex);
    return configMap.find(key) != configMap.end();
}

std::map<std::string, std::string> ConfigManager::listAll() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return configMap;
}
