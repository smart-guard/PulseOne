// =============================================================================
// collector/include/Database/DatabaseAbstractionLayer.h
// 🎯 올바른 DB 추상화 - 헤더 파일 (선언만)
// =============================================================================

#ifndef DATABASE_ABSTRACTION_LAYER_H
#define DATABASE_ABSTRACTION_LAYER_H

#include <string>
#include <vector>
#include <map>
#include <memory>

// Forward declarations
class DatabaseManager;

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
    virtual std::string buildUpsertQuery(
        const std::string& table_name,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& primary_keys) const = 0;
    
    // CREATE TABLE 구문
    virtual std::string adaptCreateTableQuery(const std::string& base_query) const = 0;
    
    // BOOLEAN 값 변환
    virtual std::string formatBooleanValue(bool value) const = 0;
    virtual bool parseBooleanValue(const std::string& value) const = 0;

protected:
    // 공통 유틸리티 메서드들
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter) const;
    void replaceAll(std::string& str, const std::string& from, const std::string& to) const;
};

/**
 * @brief SQLite 방언
 */
class SQLiteDialect : public ISQLDialect {
public:
    std::string getBooleanType() const override;
    std::string getTimestampType() const override;
    std::string getAutoIncrementType() const override;
    std::string getCurrentTimestamp() const override;
    
    std::string buildUpsertQuery(
        const std::string& table_name,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& primary_keys) const override;
    
    std::string adaptCreateTableQuery(const std::string& base_query) const override;
    
    std::string formatBooleanValue(bool value) const override;
    bool parseBooleanValue(const std::string& value) const override;
};

/**
 * @brief PostgreSQL 방언
 */
class PostgreSQLDialect : public ISQLDialect {
public:
    std::string getBooleanType() const override;
    std::string getTimestampType() const override;
    std::string getAutoIncrementType() const override;
    std::string getCurrentTimestamp() const override;
    
    std::string buildUpsertQuery(
        const std::string& table_name,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& primary_keys) const override;
    
    std::string adaptCreateTableQuery(const std::string& base_query) const override;
    
    std::string formatBooleanValue(bool value) const override;
    bool parseBooleanValue(const std::string& value) const override;
};

/**
 * @brief MySQL/MariaDB 방언
 */
class MySQLDialect : public ISQLDialect {
public:
    std::string getBooleanType() const override;
    std::string getTimestampType() const override;
    std::string getAutoIncrementType() const override;
    std::string getCurrentTimestamp() const override;
    
    std::string buildUpsertQuery(
        const std::string& table_name,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& primary_keys) const override;
    
    std::string adaptCreateTableQuery(const std::string& base_query) const override;
    
    std::string formatBooleanValue(bool value) const override;
    bool parseBooleanValue(const std::string& value) const override;
};

/**
 * @brief MSSQL/SQL Server 방언
 */
class MSSQLDialect : public ISQLDialect {
public:
    std::string getBooleanType() const override;
    std::string getTimestampType() const override;
    std::string getAutoIncrementType() const override;
    std::string getCurrentTimestamp() const override;
    
    std::string buildUpsertQuery(
        const std::string& table_name,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& primary_keys) const override;
    
    std::string adaptCreateTableQuery(const std::string& base_query) const override;
    
    std::string formatBooleanValue(bool value) const override;
    bool parseBooleanValue(const std::string& value) const override;
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
public:
    DatabaseAbstractionLayer();
    ~DatabaseAbstractionLayer() = default;

    // =======================================================================
    // 🎯 Repository가 사용할 간단한 인터페이스
    // =======================================================================
    
    /**
     * @brief SELECT 쿼리 실행 (표준 SQL → DB별 방언 변환)
     * @param query 표준 SQL 쿼리
     * @return 결과 맵의 벡터
     */
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query);
    
    /**
     * @brief INSERT/UPDATE/DELETE 실행 (표준 SQL → DB별 방언 변환)
     * @param query 표준 SQL 쿼리
     * @return 성공 시 true
     */
    bool executeNonQuery(const std::string& query);
    
    /**
     * @brief UPSERT 쿼리 생성 및 실행
     * @param table_name 테이블명
     * @param data 컬럼명-값 맵
     * @param primary_keys 기본키 컬럼들
     * @return 성공 시 true
     */
    bool executeUpsert(
        const std::string& table_name,
        const std::map<std::string, std::string>& data,
        const std::vector<std::string>& primary_keys);
    
    /**
     * @brief CREATE TABLE 실행 (DB별 자동 적응)
     * @param base_create_query 기본 CREATE TABLE 쿼리 (표준 SQL)
     * @return 성공 시 true
     */
    bool executeCreateTable(const std::string& base_create_query);
    
    /**
     * @brief BOOLEAN 값 포맷팅
     * @param value BOOLEAN 값
     * @return DB에 맞는 문자열
     */
    std::string formatBoolean(bool value);
    
    /**
     * @brief BOOLEAN 값 파싱
     * @param value DB에서 가져온 문자열
     * @return BOOLEAN 값
     */
    bool parseBoolean(const std::string& value);
    
    /**
     * @brief 현재 타임스탬프 구문
     * @return DB별 현재 시간 구문
     */
    std::string getCurrentTimestamp();

private:
    // =======================================================================
    // 내부 구현 메서드들
    // =======================================================================
    
    void initializeDialect();
    
    std::string adaptQuery(const std::string& query);
    std::string adaptUpsertQuery(const std::string& query);
    std::string adaptBooleanValues(const std::string& query);
    std::string adaptTimestampFunctions(const std::string& query);
    std::string adaptLimitOffset(const std::string& query);
    std::string adaptSpecialSyntax(const std::string& query);
    
    std::vector<std::string> extractColumnsFromQuery(const std::string& query);
    
    std::vector<std::map<std::string, std::string>> convertToMapResults(
        const std::vector<std::vector<std::string>>& raw_results,
        const std::vector<std::string>& column_names);
    
    std::string escapeString(const std::string& str);

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    DatabaseManager* db_manager_;
    std::unique_ptr<ISQLDialect> dialect_;
    std::string current_db_type_;
};

} // namespace Database
} // namespace PulseOne

#endif // DATABASE_ABSTRACTION_LAYER_H