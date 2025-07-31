/**
 * @file DatabaseAbstractionLayer.h
 * @brief 올바른 DB 추상화 - 한 곳에서 모든 DB 차이점 처리
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 목표: Repository는 단순하게, DB 차이점은 한 곳에서만!
 */

#ifndef DATABASE_ABSTRACTION_LAYER_H
#define DATABASE_ABSTRACTION_LAYER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace PulseOne {
namespace Database {

/**
 * @brief DB별 SQL 방언(Dialect) 처리 인터페이스
 */
class ISQLDialect {
public:
    virtual ~ISQLDialect() = default;
    
    // 기본 타입 변환
    virtual std::string getBooleanType() const = 0;
    virtual std::string getTimestampType() const = 0;
    virtual std::string getAutoIncrementType() const = 0;
    virtual std::string getCurrentTimestamp() const = 0;
    
    // UPSERT 쿼리 생성
    virtual std::string buildUpsertQuery(const std::string& table_name,
                                       const std::vector<std::string>& columns,
                                       const std::vector<std::string>& primary_keys) const = 0;
    
    // CREATE TABLE 구문
    virtual std::string adaptCreateTableQuery(const std::string& base_query) const = 0;
    
    // BOOLEAN 값 변환
    virtual std::string formatBooleanValue(bool value) const = 0;
    virtual bool parseBooleanValue(const std::string& value) const = 0;
};

/**
 * @brief SQLite 방언
 */
class SQLiteDialect : public ISQLDialect {
public:
    std::string getBooleanType() const override { return "INTEGER"; }
    std::string getTimestampType() const override { return "DATETIME DEFAULT CURRENT_TIMESTAMP"; }
    std::string getAutoIncrementType() const override { return "INTEGER PRIMARY KEY AUTOINCREMENT"; }
    std::string getCurrentTimestamp() const override { return "datetime('now', 'localtime')"; }
    
    std::string buildUpsertQuery(const std::string& table_name,
                               const std::vector<std::string>& columns,
                               const std::vector<std::string>& primary_keys) const override {
        
        std::string placeholders;
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) placeholders += ", ";
            placeholders += "?";
        }
        
        return "INSERT OR REPLACE INTO " + table_name + 
               " (" + joinStrings(columns, ", ") + ") VALUES (" + placeholders + ")";
    }
    
    std::string adaptCreateTableQuery(const std::string& base_query) const override {
        std::string adapted = base_query;
        // BOOLEAN → INTEGER
        replaceAll(adapted, "BOOLEAN", "INTEGER");
        // TIMESTAMP → DATETIME
        replaceAll(adapted, "TIMESTAMP DEFAULT CURRENT_TIMESTAMP", "DATETIME DEFAULT CURRENT_TIMESTAMP");
        return adapted;
    }
    
    std::string formatBooleanValue(bool value) const override {
        return value ? "1" : "0";
    }
    
    bool parseBooleanValue(const std::string& value) const override {
        return value == "1" || value == "true";
    }

private:
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter) const {
        std::string result;
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) result += delimiter;
            result += strings[i];
        }
        return result;
    }
    
    void replaceAll(std::string& str, const std::string& from, const std::string& to) const {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
};

/**
 * @brief PostgreSQL 방언
 */
class PostgreSQLDialect : public ISQLDialect {
public:
    std::string getBooleanType() const override { return "BOOLEAN"; }
    std::string getTimestampType() const override { return "TIMESTAMP DEFAULT CURRENT_TIMESTAMP"; }
    std::string getAutoIncrementType() const override { return "SERIAL PRIMARY KEY"; }
    std::string getCurrentTimestamp() const override { return "CURRENT_TIMESTAMP"; }
    
    std::string buildUpsertQuery(const std::string& table_name,
                               const std::vector<std::string>& columns,
                               const std::vector<std::string>& primary_keys) const override {
        
        std::string placeholders;
        std::string updates;
        
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) placeholders += ", ";
            placeholders += "?";
            
            // PRIMARY KEY가 아닌 컬럼만 UPDATE
            bool is_primary = std::find(primary_keys.begin(), primary_keys.end(), columns[i]) != primary_keys.end();
            if (!is_primary) {
                if (!updates.empty()) updates += ", ";
                updates += columns[i] + " = EXCLUDED." + columns[i];
            }
        }
        
        std::string conflict_columns;
        for (size_t i = 0; i < primary_keys.size(); ++i) {
            if (i > 0) conflict_columns += ", ";
            conflict_columns += primary_keys[i];
        }
        
        return "INSERT INTO " + table_name + 
               " (" + joinStrings(columns, ", ") + ") VALUES (" + placeholders + ")" +
               " ON CONFLICT (" + conflict_columns + ") DO UPDATE SET " + updates;
    }
    
    std::string adaptCreateTableQuery(const std::string& base_query) const override {
        // PostgreSQL은 표준 SQL과 거의 동일
        return base_query;
    }
    
    std::string formatBooleanValue(bool value) const override {
        return value ? "true" : "false";
    }
    
    bool parseBooleanValue(const std::string& value) const override {
        return value == "true" || value == "TRUE" || value == "1";
    }

private:
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter) const {
        std::string result;
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) result += delimiter;
            result += strings[i];
        }
        return result;
    }
};

/**
 * @brief MySQL 방언
 */
class MySQLDialect : public ISQLDialect {
public:
    std::string getBooleanType() const override { return "TINYINT(1)"; }
    std::string getTimestampType() const override { return "TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"; }
    std::string getAutoIncrementType() const override { return "INT AUTO_INCREMENT PRIMARY KEY"; }
    std::string getCurrentTimestamp() const override { return "NOW()"; }
    
    std::string buildUpsertQuery(const std::string& table_name,
                               const std::vector<std::string>& columns,
                               const std::vector<std::string>& primary_keys) const override {
        
        std::string placeholders;
        std::string updates;
        
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) placeholders += ", ";
            placeholders += "?";
            
            // PRIMARY KEY가 아닌 컬럼만 UPDATE
            bool is_primary = std::find(primary_keys.begin(), primary_keys.end(), columns[i]) != primary_keys.end();
            if (!is_primary) {
                if (!updates.empty()) updates += ", ";
                updates += columns[i] + " = VALUES(" + columns[i] + ")";
            }
        }
        
        return "INSERT INTO " + table_name + 
               " (" + joinStrings(columns, ", ") + ") VALUES (" + placeholders + ")" +
               " ON DUPLICATE KEY UPDATE " + updates;
    }
    
    std::string adaptCreateTableQuery(const std::string& base_query) const override {
        std::string adapted = base_query;
        // BOOLEAN → TINYINT(1)
        replaceAll(adapted, "BOOLEAN", "TINYINT(1)");
        return adapted;
    }
    
    std::string formatBooleanValue(bool value) const override {
        return value ? "1" : "0";
    }
    
    bool parseBooleanValue(const std::string& value) const override {
        return value == "1" || value == "true";
    }

private:
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter) const {
        std::string result;
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) result += delimiter;
            result += strings[i];
        }
        return result;
    }
    
    void replaceAll(std::string& str, const std::string& from, const std::string& to) const {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
};

/**
 * @brief 통합 데이터베이스 추상화 레이어
 * 
 * 🎯 핵심 아이디어:
 * - Repository는 이 클래스만 사용
 * - DB별 차이점은 여기서만 처리
 * - 새 DB 추가시 Dialect만 추가하면 됨
 */
class DatabaseAbstractionLayer {
private:
    DatabaseManager* db_manager_;
    std::unique_ptr<ISQLDialect> dialect_;
    std::string current_db_type_;

public:
    DatabaseAbstractionLayer() 
        : db_manager_(&DatabaseManager::getInstance())
        , current_db_type_("SQLITE") {
        
        initializeDialect();
    }
    
    ~DatabaseAbstractionLayer() = default;

    // =======================================================================
    // 🎯 Repository가 사용할 간단한 인터페이스
    // =======================================================================
    
    /**
     * @brief SELECT 쿼리 실행 (표준 SQL 사용)
     * @param query 표준 SQL 쿼리
     * @return 결과 맵의 벡터
     */
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::vector<std::string>> raw_results;
        bool success = db_manager_->executeQuery(adaptQuery(query), raw_results);
        
        if (!success) {
            return {};
        }
        
        return convertToMapResults(raw_results, extractColumnsFromQuery(query));
    }
    
    /**
     * @brief INSERT/UPDATE/DELETE 실행
     * @param query 표준 SQL 쿼리
     * @return 성공 시 true
     */
    bool executeNonQuery(const std::string& query) {
        return db_manager_->executeNonQuery(adaptQuery(query));
    }
    
    /**
     * @brief UPSERT 쿼리 생성 및 실행
     * @param table_name 테이블명
     * @param data 컬럼명-값 맵
     * @param primary_keys 기본키 컬럼들
     * @return 성공 시 true
     */
    bool executeUpsert(const std::string& table_name,
                      const std::map<std::string, std::string>& data,
                      const std::vector<std::string>& primary_keys) {
        
        std::vector<std::string> columns;
        std::vector<std::string> values;
        
        for (const auto& [column, value] : data) {
            columns.push_back(column);
            values.push_back(value);
        }
        
        std::string upsert_query = dialect_->buildUpsertQuery(table_name, columns, primary_keys);
        
        // ? 플레이스홀더를 실제 값으로 치환
        for (size_t i = 0; i < values.size(); ++i) {
            size_t pos = upsert_query.find('?');
            if (pos != std::string::npos) {
                upsert_query.replace(pos, 1, "'" + escapeString(values[i]) + "'");
            }
        }
        
        return executeNonQuery(upsert_query);
    }
    
    /**
     * @brief CREATE TABLE 실행 (DB별 자동 적응)
     * @param base_create_query 기본 CREATE TABLE 쿼리 (표준 SQL)
     * @return 성공 시 true
     */
    bool executeCreateTable(const std::string& base_create_query) {
        std::string adapted_query = dialect_->adaptCreateTableQuery(base_create_query);
        return executeNonQuery(adapted_query);
    }
    
    /**
     * @brief BOOLEAN 값 포맷팅
     * @param value BOOLEAN 값
     * @return DB에 맞는 문자열
     */
    std::string formatBoolean(bool value) {
        return dialect_->formatBooleanValue(value);
    }
    
    /**
     * @brief BOOLEAN 값 파싱
     * @param value DB에서 가져온 문자열
     * @return BOOLEAN 값
     */
    bool parseBoolean(const std::string& value) {
        return dialect_->parseBooleanValue(value);
    }
    
    /**
     * @brief 현재 타임스탬프 구문
     * @return DB별 현재 시간 구문
     */
    std::string getCurrentTimestamp() {
        return dialect_->getCurrentTimestamp();
    }

private:
    // =======================================================================
    // 내부 구현
    // =======================================================================
    
    void initializeDialect() {
        // 환경변수나 설정에서 DB 타입 읽기
        ConfigManager& config = ConfigManager::getInstance();
        current_db_type_ = config.getActiveDatabaseType(); // 이미 대문자 변환됨
        
        // 대문자로 변환
        std::transform(current_db_type_.begin(), current_db_type_.end(), 
                      current_db_type_.begin(), ::toupper);
        
        // 적절한 Dialect 생성
         if (current_db_type_ == "POSTGRESQL") {
            dialect_ = std::make_unique<PostgreSQLDialect>();
        } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
            dialect_ = std::make_unique<MySQLDialect>();
        } else {
            dialect_ = std::make_unique<SQLiteDialect>();
        }

        LogManager::getInstance().Info("DatabaseAbstractionLayer - Using database type: " + current_db_type_);
    }
    
    std::string adaptQuery(const std::string& query) {
        // 기본적으로 쿼리는 그대로 통과
        // 필요시 여기서 추가 변환
        return query;
    }
    
    std::vector<std::map<std::string, std::string>> convertToMapResults(
        const std::vector<std::vector<std::string>>& raw_results,
        const std::vector<std::string>& column_names) {
        
        std::vector<std::map<std::string, std::string>> map_results;
        
        for (const auto& row : raw_results) {
            std::map<std::string, std::string> row_map;
            
            for (size_t i = 0; i < column_names.size() && i < row.size(); ++i) {
                row_map[column_names[i]] = row[i];
            }
            
            map_results.push_back(row_map);
        }
        
        return map_results;
    }
    
    std::vector<std::string> extractColumnsFromQuery(const std::string& query) {
        // 간단한 구현: SELECT 이후 FROM 이전의 컬럼들 추출
        // 실제로는 더 정교한 SQL 파싱이 필요할 수 있음
        
        // 임시로 고정된 컬럼명 반환 (DeviceSettings용)
        return {
            "device_id", "polling_interval_ms", "connection_timeout_ms", "max_retry_count",
            "retry_interval_ms", "backoff_time_ms", "keep_alive_enabled", "keep_alive_interval_s",
            "scan_rate_override", "read_timeout_ms", "write_timeout_ms", "backoff_multiplier",
            "max_backoff_time_ms", "keep_alive_timeout_s", "data_validation_enabled",
            "performance_monitoring_enabled", "diagnostic_mode_enabled", "updated_at"
        };
    }
    
    std::string escapeString(const std::string& str) {
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "''");
            pos += 2;
        }
        return escaped;
    }
};

} // namespace Database
} // namespace PulseOne

#endif // DATABASE_ABSTRACTION_LAYER_H