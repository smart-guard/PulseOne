#ifndef LOGGER_ENGINE_HPP
#define LOGGER_ENGINE_HPP

#include "LogTypes.hpp"
#include "LogExport.hpp"

#include <string>
#include <fstream>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>
#include <filesystem>
#include <vector>
#include <functional>

namespace LogLib {

/**
 * @brief Core logging engine responsible for file management and rotation.
 * @details This class is project-agnostic and can be distributed as a DLL/SO.
 */
class LOGLIB_API LoggerEngine {
public:
    static LoggerEngine& getInstance();

    // Configuration
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    
    // Dynamic Configuration
    using LogLevelProvider = std::function<LogLevel()>;
    void setLogLevelProvider(LogLevelProvider provider);
    
    void setLogBasePath(const std::string& path);
    std::string getLogBasePath() const;
    
    void setConsoleOutput(bool enabled);
    void setFileOutput(bool enabled);
    
    void setMaxLogSizeMB(size_t size_mb);
    void setMaxLogFiles(int count);

    // Logging
    void log(const std::string& category, LogLevel level, const std::string& message);
    void logPacket(const std::string& driver, const std::string& device,
                   const std::string& rawPacket, const std::string& decoded);

    // Maintenance & Stats
    void flushAll();
    void rotateLogs();
    LogStatistics getStatistics() const;
    void resetStatistics();

    // Built-in Config Loader (Simple Key=Value parser)
    bool loadFromConfigFile(const std::string& path);

private:
    LoggerEngine();
    ~LoggerEngine();
    
    LoggerEngine(const LoggerEngine&) = delete;
    LoggerEngine& operator=(const LoggerEngine&) = delete;

    // Internal Utilities
    std::string buildLogPath(const std::string& category);
    std::filesystem::path buildLogDirectoryPath(const std::string& category);
    std::filesystem::path buildLogFilePath(const std::string& category);
    
    void writeToFile(const std::string& filePath, const std::string& message);
    void checkAndRotateLogFile(const std::string& filePath, std::ofstream& stream);
    
    // Config Parser Helpers
    std::string trim(const std::string& s);
    void applyConfig(const std::string& key, const std::string& value);
    LogLevel stringToLogLevel(const std::string& level);

    std::string getCurrentDate();
    std::string getCurrentTimestamp();
    void updateStatistics(LogLevel level);

    // Member Variables
    mutable std::recursive_mutex mutex_;
    std::map<std::string, std::ofstream> logFiles_;
    
    LogLevel minLevel_;
    std::string log_base_path_;
    bool console_output_enabled_;
    bool file_output_enabled_;
    
    size_t max_log_size_mb_;
    int max_log_files_;
    
    LogLevelProvider log_level_provider_;
    
    mutable LogStatistics statistics_;
};

} // namespace LogLib

#endif // LOGGER_ENGINE_HPP
