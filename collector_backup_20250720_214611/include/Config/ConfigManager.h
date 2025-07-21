#pragma once

#include <string>
#include <map>
#include <mutex>

class ConfigManager {
public:
    static ConfigManager& getInstance();

    // 초기 로딩 (.env 없을 경우 template 복사)
    void initialize();  // 기본 경로 ./config/.env

    // 강제 재로딩
    void reload();

    // key-value 접근
    std::string get(const std::string& key) const;
    std::string getOrDefault(const std::string& key, const std::string& defaultValue) const;
    void set(const std::string& key, const std::string& value);
    bool hasKey(const std::string& key) const;

    std::map<std::string, std::string> listAll() const;

private:
    ConfigManager() = default;
    void parseLine(const std::string& line);

    std::map<std::string, std::string> configMap;
    mutable std::mutex configMutex;
    std::string envFilePath;
};
