#include "Config/ConfigManager.h"
#include "Utils/LogManager.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

#define DEFAULT_ENV_PATH "./config/.env"
#define TEMPLATE_ENV_PATH "./config/.env.template"

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

void ConfigManager::initialize() {
    envFilePath = DEFAULT_ENV_PATH;

    if (!std::filesystem::exists(envFilePath)) {
        if (std::filesystem::exists(TEMPLATE_ENV_PATH)) {
            try {
                std::filesystem::create_directories("./config");
                std::filesystem::copy_file(TEMPLATE_ENV_PATH, envFilePath);
                PulseOne::LogManager::getInstance().log("config", LOG_WARN, ".env 파일 없음 → 템플릿으로 복사: " + envFilePath);
            } catch (const std::exception& e) {
                PulseOne::LogManager::getInstance().log("config", LOG_ERROR, ".env 템플릿 복사 실패: " + std::string(e.what()));
                return;
            }
        } else {
            PulseOne::LogManager::getInstance().log("config", LOG_ERROR, ".env와 템플릿 모두 없음. 환경설정 초기화 실패");
            return;
        }
    }

    std::ifstream file(envFilePath);
    if (!file.is_open()) {
        PulseOne::LogManager::getInstance().log("config", LOG_ERROR, ".env 열기 실패: " + envFilePath);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        parseLine(line);
    }

    PulseOne::LogManager::getInstance().log("config", LOG_INFO, ".env 로딩 완료: " + envFilePath);
}

void ConfigManager::reload() {
    PulseOne::LogManager::getInstance().log("config", LOG_INFO, "환경설정 재로딩 시작: " + envFilePath);
    initialize();
}

void ConfigManager::parseLine(const std::string& line) {
    if (line.empty() || line[0] == '#') return;

    size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) return;

    std::string key = line.substr(0, eqPos);
    std::string value = line.substr(eqPos + 1);

    key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
    value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());

    std::lock_guard<std::mutex> lock(configMutex);
    configMap[key] = value;
}

std::string ConfigManager::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find(key);
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
    return std::map<std::string, std::string>(configMap.begin(), configMap.end());
}
