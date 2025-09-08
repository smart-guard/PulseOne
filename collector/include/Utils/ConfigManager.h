#pragma once

/**
 * @file ConfigManager.h  
 * @brief 통합 설정 관리자 - PlatformCompat.h 사용
 * @author PulseOne Development Team
 * @date 2025-09-06
 */

// PlatformCompat.h가 모든 플랫폼 문제를 해결함
#include "Platform/PlatformCompat.h"

// 표준 헤더들
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <atomic>

// 파일시스템 헤더 (C++17 호환성)
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
    namespace fs = std::filesystem;
    #define HAS_FILESYSTEM 1
#else
    #define HAS_FILESYSTEM 0
#endif

/**
 * @class ConfigManager
 * @brief 통합 설정 관리자 (크로스 플랫폼 싱글톤)
 */
class ConfigManager {
public:
    // ==========================================================================
    // 전역 싱글톤 패턴 (Meyer's Singleton + 자동 초기화)
    // ==========================================================================
    
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        instance.ensureInitialized();
        return instance;
    }
    
    bool isInitialized() const {
        return initialized_.load(std::memory_order_acquire);
    }
    
    // ==========================================================================
    // 기존 인터페이스 (100% 호환 유지)
    // ==========================================================================
    
    void initialize() {
        doInitialize();
    }
    
    void reload();
    std::string get(const std::string& key) const;
    std::string getOrDefault(const std::string& key, const std::string& defaultValue) const;
    void set(const std::string& key, const std::string& value);
    bool hasKey(const std::string& key) const;
    std::map<std::string, std::string> listAll() const;

    // 확장 기능들
    std::string getConfigDirectory() const { return configDir_; }
    std::string getDataDirectory() const;
    std::string getSQLiteDbPath() const;
    std::string getBackupDirectory() const;
    std::vector<std::string> getLoadedFiles() const { return loadedFiles_; }
    void printConfigSearchLog() const;
    
    std::string getActiveDatabaseType() const;
    bool isModuleEnabled(const std::string& module_name) const;
    std::string loadPasswordFromFile(const std::string& password_file_key) const;
    std::string getSecretsDirectory() const;
    std::map<std::string, bool> checkAllConfigFiles() const;
    void triggerVariableExpansion();

    // 편의 기능들
    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;

private:
    // ==========================================================================
    // 생성자/소멸자 (싱글톤)
    // ==========================================================================
    
    ConfigManager() : initialized_(false) {}
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;
    
    // ==========================================================================
    // 크로스 플랫폼 초기화 로직
    // ==========================================================================
    
    void ensureInitialized() {
        // 빠른 체크 (이미 초기화됨)
        if (initialized_.load(std::memory_order_acquire)) {
            return;
        }
        
        // 느린 체크 (뮤텍스 사용)
        std::lock_guard<std::mutex> lock(init_mutex_);
        if (initialized_.load(std::memory_order_relaxed)) {
            return;
        }
        
        // 실제 초기화 수행
        doInitialize();
        initialized_.store(true, std::memory_order_release);
    }
    
    bool doInitialize();
    
    // ==========================================================================
    // 핵심 메서드들
    // ==========================================================================
    
    void parseLine(const std::string& line);
    std::string findConfigDirectory();
    bool directoryExists(const std::string& path);
    void loadMainConfig();
    void loadAdditionalConfigs();
    void loadConfigFile(const std::string& filepath);
    void expandAllVariables();
    std::string expandVariables(const std::string& value) const;
    
    void ensureConfigFilesExist();
    void createMainEnvFile();
    void createDatabaseEnvFile();
    void createRedisEnvFile();
    void createTimeseriesEnvFile();
    void createMessagingEnvFile();
    void createSecurityEnvFile();
    void createSecretsDirectory();
    bool createFileFromTemplate(const std::string& filepath, const std::string& content);
    
    std::string findDataDirectory();
    void ensureDataDirectories();
    std::string getExecutableDirectory();
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    /// 초기화 상태 (원자적 연산)
    std::atomic<bool> initialized_;
    
    /// 초기화용 뮤텍스
    mutable std::mutex init_mutex_;
    
    /// 설정 데이터 저장소
    std::map<std::string, std::string> configMap;
    
    /// 멀티스레드 보호용 뮤텍스
    mutable std::mutex configMutex;
    
    /// 경로 관련
    std::string envFilePath;
    std::string configDir_;
    std::string dataDir_;
    std::vector<std::string> loadedFiles_;
    std::vector<std::string> searchLog_;
};