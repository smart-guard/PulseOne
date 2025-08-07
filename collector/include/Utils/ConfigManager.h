// =============================================================================
// collector/include/Utils/ConfigManager.h - 완전한 기능 보존 + 자동 초기화
// 기존 모든 기능 100% 유지 + getInstance()에서 자동 초기화 추가
// =============================================================================

#pragma once

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <filesystem>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * @class ConfigManager
 * @brief 통합 설정 관리자 (싱글톤) - 자동 초기화 지원
 * 
 * 기능:
 * - 🔥 NEW: getInstance() 호출 시 자동 초기화
 * - 기존 .env 파일 100% 호환
 * - 모듈별 설정 파일 분리 (database.env, redis.env 등)
 * - 자동 템플릿 생성
 * - 변수 확장 (${VAR} 문법)
 * - 보안 강화 (secrets/ 디렉토리)
 * - 멀티스레드 안전성
 */
class ConfigManager {
public:
    // ==========================================================================
    // 🔥 핵심 개선: 자동 초기화 getInstance
    // ==========================================================================
    
    /**
     * @brief 싱글톤 인스턴스 반환 (자동 초기화됨)
     * @return ConfigManager 인스턴스 참조
     */
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        
        // 🔥 자동 초기화: 처음 호출 시 한 번만 실행
        static std::once_flag initialized;
        std::call_once(initialized, [&instance] {
            instance.doInitialize();
        });
        
        return instance;
    }
    
    /**
     * @brief 초기화 상태 확인
     * @return 초기화 완료 시 true
     */
    bool isInitialized() const {
        return initialized_.load();
    }
    
    // ==========================================================================
    // 기존 인터페이스 (100% 호환 유지)
    // ==========================================================================
    
    /**
     * @brief ConfigManager 수동 초기화 (기존 호환성)
     * 설정 디렉토리 찾기, 템플릿 생성, 설정 로드 수행
     */
    void initialize() {
        doInitialize();
    }
    
    /**
     * @brief 설정 파일들 재로딩
     */
    void reload();
    
    /**
     * @brief 설정값 조회
     * @param key 설정 키
     * @return 설정값 (없으면 빈 문자열)
     */
    std::string get(const std::string& key) const;
    
    /**
     * @brief 기본값을 가진 설정값 조회
     * @param key 설정 키
     * @param defaultValue 기본값
     * @return 설정값 또는 기본값
     */
    std::string getOrDefault(const std::string& key, const std::string& defaultValue) const;
    
    /**
     * @brief 설정값 저장
     * @param key 설정 키
     * @param value 설정값
     */
    void set(const std::string& key, const std::string& value);
    
    /**
     * @brief 설정 키 존재 여부 확인
     * @param key 설정 키
     * @return 존재하면 true
     */
    bool hasKey(const std::string& key) const;
    
    /**
     * @brief 모든 설정 항목 반환
     * @return 설정 맵의 복사본
     */
    std::map<std::string, std::string> listAll() const;

    // ==========================================================================
    // 확장 기능들 (기존 유지)
    // ==========================================================================
    
    /**
     * @brief 설정 디렉토리 경로 반환
     */
    std::string getConfigDirectory() const { return configDir_; }
    
    /**
     * @brief 데이터 디렉토리 경로 반환
     */
    std::string getDataDirectory() const;
    
    /**
     * @brief SQLite DB 파일 경로 반환
     */
    std::string getSQLiteDbPath() const;
    
    /**
     * @brief 백업 디렉토리 경로 반환
     */
    std::string getBackupDirectory() const;
    
    /**
     * @brief 로드된 설정 파일 목록 반환
     */
    std::vector<std::string> getLoadedFiles() const { return loadedFiles_; }
    
    /**
     * @brief 설정 검색 로그 출력 (디버깅용)
     */
    void printConfigSearchLog() const;

    // ==========================================================================
    // 새로 추가된 확장 메서드들 (기존 유지)
    // ==========================================================================
    
    /**
     * @brief 현재 활성화된 데이터베이스 타입 반환
     * @return "SQLITE", "POSTGRESQL", "MYSQL", "MARIADB", "MSSQL" 중 하나
     */
    std::string getActiveDatabaseType() const;
    
    /**
     * @brief 모듈 활성화 상태 확인
     * @param module_name 모듈명 (database, timeseries, redis, messaging, security)
     * @return 활성화되면 true
     */
    bool isModuleEnabled(const std::string& module_name) const;
    
    /**
     * @brief 비밀번호 파일에서 비밀번호 로드
     * @param password_file_key 비밀번호 파일 경로를 담고 있는 설정 키
     * @return 로드된 비밀번호 (실패 시 빈 문자열)
     */
    std::string loadPasswordFromFile(const std::string& password_file_key) const;
    
    /**
     * @brief secrets/ 디렉토리 경로 반환
     */
    std::string getSecretsDirectory() const;
    
    /**
     * @brief 모든 설정 파일 존재 여부 확인
     * @return 파일명과 존재 여부의 맵
     */
    std::map<std::string, bool> checkAllConfigFiles() const;

    /**
     * @brief 수동으로 변수 확장 트리거 (테스트용)
     */
    void triggerVariableExpansion();

    // ==========================================================================
    // 편의 기능들 (기존 유지)
    // ==========================================================================
    
    /**
     * @brief 정수형 설정값 조회
     * @param key 설정 키
     * @param defaultValue 기본값
     * @return 정수형 설정값
     */
    int getInt(const std::string& key, int defaultValue = 0) const;
    
    /**
     * @brief 불린형 설정값 조회 (true, yes, 1, on을 true로 인식)
     * @param key 설정 키
     * @param defaultValue 기본값
     * @return 불린형 설정값
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    /**
     * @brief 실수형 설정값 조회
     * @param key 설정 키
     * @param defaultValue 기본값
     * @return 실수형 설정값
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const;

private:
    // ==========================================================================
    // 생성자/소멸자 (싱글톤)
    // ==========================================================================
    
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // ==========================================================================
    // 🔥 핵심: 실제 초기화 로직 (내부용)
    // ==========================================================================
    
    /**
     * @brief 실제 초기화 로직 (thread-safe)
     * @return 초기화 성공 여부
     */
    bool doInitialize();
    
    // ==========================================================================
    // 핵심 메서드들 (기존 + 확장)
    // ==========================================================================
    
    /**
     * @brief 설정 파일 한 줄 파싱
     * @param line 파싱할 라인
     */
    void parseLine(const std::string& line);
    
    /**
     * @brief 설정 디렉토리 찾기
     * @return 찾은 설정 디렉토리 경로 (실패 시 빈 문자열)
     */
    std::string findConfigDirectory();
    
    /**
     * @brief 디렉토리 존재 여부 확인
     * @param path 확인할 경로
     * @return 존재하면 true
     */
    bool directoryExists(const std::string& path);
    
    /**
     * @brief 메인 .env 파일 로드
     */
    void loadMainConfig();
    
    /**
     * @brief 추가 설정 파일들 로드 (CONFIG_FILES에서 지정된)
     */
    void loadAdditionalConfigs();
    
    /**
     * @brief 개별 설정 파일 로드
     * @param filepath 로드할 파일 경로
     */
    void loadConfigFile(const std::string& filepath);
    
    /**
     * @brief 모든 변수 확장 (${VAR} 문법 처리)
     */
    void expandAllVariables();
    
    /**
     * @brief 단일 문자열의 변수 확장
     * @param value 확장할 문자열
     * @return 확장된 문자열
     */
    std::string expandVariables(const std::string& value) const;
    
    // ==========================================================================
    // 템플릿 생성 메서드들 (기존 유지)
    // ==========================================================================
    
    /**
     * @brief 모든 설정 파일 존재 여부 확인 및 생성
     */
    void ensureConfigFilesExist();
    
    /**
     * @brief 메인 .env 파일 템플릿 생성
     */
    void createMainEnvFile();
    
    /**
     * @brief 데이터베이스 설정 파일 템플릿 생성
     */
    void createDatabaseEnvFile();
    
    /**
     * @brief Redis 설정 파일 템플릿 생성
     */
    void createRedisEnvFile();
    
    /**
     * @brief 시계열 DB 설정 파일 템플릿 생성
     */
    void createTimeseriesEnvFile();
    
    /**
     * @brief 메시징 설정 파일 템플릿 생성
     */
    void createMessagingEnvFile();
    
    /**
     * @brief 보안 설정 파일 템플릿 생성
     */
    void createSecurityEnvFile();
    
    /**
     * @brief secrets/ 디렉토리 및 키 파일들 생성
     */
    void createSecretsDirectory();
    
    /**
     * @brief 템플릿에서 파일 생성
     * @param filepath 생성할 파일 경로
     * @param content 파일 내용
     * @return 성공하면 true
     */
    bool createFileFromTemplate(const std::string& filepath, const std::string& content);
    
    // ==========================================================================
    // 데이터 경로 관리 (기존 유지)
    // ==========================================================================
    
    /**
     * @brief 데이터 디렉토리 찾기
     * @return 데이터 디렉토리 경로
     */
    std::string findDataDirectory();
    
    /**
     * @brief 데이터 디렉토리들 생성 (db, logs, backup, temp)
     */
    void ensureDataDirectories();
    
    // ==========================================================================
    // 멤버 변수들 (기존 + 초기화 상태)
    // ==========================================================================
    
    /// 🔥 NEW: 초기화 상태 추적
    std::atomic<bool> initialized_{false};
    
    /// 설정 데이터 저장소
    std::map<std::string, std::string> configMap;
    
    /// 멀티스레드 보호용 뮤텍스
    mutable std::mutex configMutex;
    
    /// 메인 .env 파일 경로 (기존 호환성)
    std::string envFilePath;
    
    /// 설정 디렉토리 경로
    std::string configDir_;
    
    /// 데이터 디렉토리 경로
    std::string dataDir_;
    
    /// 로드된 설정 파일 목록
    std::vector<std::string> loadedFiles_;
    
    /// 설정 디렉토리 검색 로그 (디버깅용)
    std::vector<std::string> searchLog_;
};
