#pragma once

/**
 * @file ConfigManager.h  
 * @brief 통합 설정 관리자 - 완전한 크로스 플랫폼 지원
 * @author PulseOne Development Team
 * @date 2025-09-09
 * 
 * 완전한 기능:
 * - Windows/Linux 크로스 플랫폼 경로 처리
 * - PlatformCompat.h의 Path 클래스 활용
 * - 설정 파일 자동 생성 및 로딩
 * - 환경변수 기반 설정 디렉토리 탐색
 */

// PlatformCompat.h가 모든 플랫폼 문제를 해결함
#include "Platform/PlatformCompat.h"

// 표준 헤더들
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <atomic>

/**
 * @class ConfigManager
 * @brief 통합 설정 관리자 (완전한 크로스 플랫폼 싱글톤)
 * 
 * 주요 기능:
 * - Windows/Linux 경로 처리 자동화
 * - 설정 파일 자동 탐색 및 생성
 * - 환경변수 기반 유연한 설정
 * - 멀티스레드 안전성
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

    // ==========================================================================
    // 확장 기능들 (크로스 플랫폼 강화)
    // ==========================================================================
    
    // 경로 관련 (크로스 플랫폼 자동 처리)
    std::string getConfigDirectory() const { return configDir_; }
    std::string getDataDirectory() const;
    std::string getSQLiteDbPath() const;
    std::string getBackupDirectory() const;
    std::string getSecretsDirectory() const;
    
    // 파일 관리
    std::vector<std::string> getLoadedFiles() const { return loadedFiles_; }
    void printConfigSearchLog() const;
    std::map<std::string, bool> checkAllConfigFiles() const;
    
    // 모듈 관리
    std::string getActiveDatabaseType() const;
    bool isModuleEnabled(const std::string& module_name) const;
    
    // 보안 관련
    std::string loadPasswordFromFile(const std::string& password_file_key) const;
    
    // 변수 확장
    void triggerVariableExpansion();

    // ==========================================================================
    // 편의 기능들 (타입 안전)
    // ==========================================================================
    
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
    // 핵심 메서드들 (크로스 플랫폼 강화)
    // ==========================================================================
    
    // 설정 파일 처리
    void parseLine(const std::string& line);
    void loadMainConfig();
    void loadAdditionalConfigs();
    void loadConfigFile(const std::string& filepath);
    
    // 경로 탐색 (PlatformCompat.h 활용)
    std::string findConfigDirectory();
    std::string findDataDirectory();
    
    // 파일시스템 유틸리티 (크로스 플랫폼)
    bool directoryExists(const std::string& path);
    std::string getExecutableDirectory();
    
    // 설정 파일 생성 (크로스 플랫폼 경로)
    void ensureConfigFilesExist();
    void createMainEnvFile();
    void createDatabaseEnvFile();
    void createRedisEnvFile();
    void createTimeseriesEnvFile();
    void createMessagingEnvFile();
    void createSecurityEnvFile();
    void createSecretsDirectory();
    bool createFileFromTemplate(const std::string& filepath, const std::string& content);
    
    // 디렉토리 관리
    void ensureDataDirectories();
    
    // 변수 확장
    void expandAllVariables();
    std::string expandVariables(const std::string& value) const;
    
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

// =============================================================================
// 전역 편의 함수들 (크로스 플랫폼)
// =============================================================================

/**
 * @brief 전역 ConfigManager 인스턴스 반환
 */
inline ConfigManager& Config() {
    return ConfigManager::getInstance();
}

/**
 * @brief 설정값 빠른 조회
 */
inline std::string GetConfig(const std::string& key, const std::string& defaultValue = "") {
    return ConfigManager::getInstance().getOrDefault(key, defaultValue);
}

/**
 * @brief 정수 설정값 빠른 조회
 */
inline int GetConfigInt(const std::string& key, int defaultValue = 0) {
    return ConfigManager::getInstance().getInt(key, defaultValue);
}

/**
 * @brief 불린 설정값 빠른 조회
 */
inline bool GetConfigBool(const std::string& key, bool defaultValue = false) {
    return ConfigManager::getInstance().getBool(key, defaultValue);
}

/**
 * @brief 크로스 플랫폼 경로 조회 편의 함수들
 */
inline std::string GetConfigDir() {
    return ConfigManager::getInstance().getConfigDirectory();
}

inline std::string GetDataDir() {
    return ConfigManager::getInstance().getDataDirectory();
}

inline std::string GetLogDir() {
    return GetConfig("LOG_FILE_PATH", "./logs/");
}

inline std::string GetSecretsDir() {
    return ConfigManager::getInstance().getSecretsDirectory();
}