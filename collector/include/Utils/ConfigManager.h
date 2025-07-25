// =============================================================================
// collector/include/Utils/ConfigManager.h - 깔끔한 버전
// =============================================================================

#pragma once

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * @class ConfigManager
 * @brief 통합 설정 관리자 (싱글톤)
 */
class ConfigManager {
public:
    // ==========================================================================
    // 기본 인터페이스 (기존 100% 호환)
    // ==========================================================================
    
    static ConfigManager& getInstance();
    
    void initialize();
    void reload();
    
    std::string get(const std::string& key) const;
    std::string getOrDefault(const std::string& key, const std::string& defaultValue) const;
    void set(const std::string& key, const std::string& value);
    bool hasKey(const std::string& key) const;
    std::map<std::string, std::string> listAll() const;

    // ==========================================================================
    // 확장 기능들
    // ==========================================================================
    
    std::string getConfigDirectory() const { return configDir_; }
    std::string getDataDirectory() const;
    std::string getSQLiteDbPath() const;
    std::string getBackupDirectory() const;
    std::vector<std::string> getLoadedFiles() const { return loadedFiles_; }
    void printConfigSearchLog() const;

    // ==========================================================================
    // 편의 기능들
    // ==========================================================================
    
    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // ==========================================================================
    // 핵심 메서드들 (최소한만)
    // ==========================================================================
    
    void parseLine(const std::string& line);
    std::string findConfigDirectory();
    bool directoryExists(const std::string& path);
    void loadMainConfig();
    void loadAdditionalConfigs();
    void loadConfigFile(const std::string& filepath);
    void expandAllVariables();
    std::string expandVariables(const std::string& value) const;
    
    // 템플릿 생성 (모든 기본 파일들)
    void ensureConfigFilesExist();
    void createMainEnvFile();
    void createDatabaseEnvFile();
    void createRedisEnvFile();
    void createTimeseriesEnvFile();
    void createMessagingEnvFile();
    void createSecretsDirectory();
    bool createFileFromTemplate(const std::string& filepath, const std::string& content);
    
    // 데이터 경로
    std::string findDataDirectory();
    void ensureDataDirectories();
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    std::map<std::string, std::string> configMap;
    mutable std::mutex configMutex;
    std::string envFilePath;
    std::string configDir_;
    std::string dataDir_;
    std::vector<std::string> loadedFiles_;
    std::vector<std::string> searchLog_;
};