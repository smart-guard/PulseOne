#ifndef DBLIB_DATABASE_ABSTRACTION_LAYER_HPP
#define DBLIB_DATABASE_ABSTRACTION_LAYER_HPP

/**
 * @file DatabaseAbstractionLayer.hpp
 * @brief Database Abstraction Layer for SQL Dialect handling in DbLib
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <iomanip>
#include "DbExport.hpp"

namespace DbLib {

// Forward declarations
class DatabaseManager;

/**
 * @brief SQL Statement Type
 */
enum class SQLStatementType {
    DDL,      // CREATE, ALTER, DROP, TRUNCATE
    DML,      // SELECT, INSERT, UPDATE, DELETE
    DCL,      // GRANT, REVOKE
    TCL,      // COMMIT, ROLLBACK, SAVEPOINT
    UNKNOWN
};

/**
 * @brief Interface for database-specific SQL dialects
 */
class DBLIB_API ISQLDialect {
public:
    virtual ~ISQLDialect() = default;
    
    // Type conversions
    virtual std::string getBooleanType() const = 0;
    virtual std::string getTimestampType() const = 0;
    virtual std::string getAutoIncrementType() const = 0;
    virtual std::string getCurrentTimestamp() const = 0;
    
    // UPSERT query generation
    virtual std::string buildUpsertQuery(
        const std::string& table_name,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& primary_keys) const = 0;
    
    // CREATE TABLE specific adaptation
    virtual std::string adaptCreateTableQuery(const std::string& base_query) const = 0;
    
    // BOOLEAN value handling
    virtual std::string formatBooleanValue(bool value) const = 0;
    virtual bool parseBooleanValue(const std::string& value) const = 0;

protected:
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter) const;
    void replaceAll(std::string& str, const std::string& from, const std::string& to) const;
};

/**
 * @brief SQLite dialect implementation
 */
class DBLIB_API SQLiteDialect : public ISQLDialect {
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
 * @brief PostgreSQL dialect implementation
 */
class DBLIB_API PostgreSQLDialect : public ISQLDialect {
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
 * @brief MySQL/MariaDB dialect implementation
 */
class DBLIB_API MySQLDialect : public ISQLDialect {
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
 * @brief MSSQL/SQL Server dialect implementation
 */
class DBLIB_API MSSQLDialect : public ISQLDialect {
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
 * @brief Database Abstraction Layer class
 */
class DBLIB_API DatabaseAbstractionLayer {
public:
    DatabaseAbstractionLayer();
    ~DatabaseAbstractionLayer() = default;

    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query);
    bool executeNonQuery(const std::string& query);
    
    bool executeUpsert(
        const std::string& table_name,
        const std::map<std::string, std::string>& data,
        const std::vector<std::string>& primary_keys);
    
    bool executeCreateTable(const std::string& create_sql);
    bool doesTableExist(const std::string& table_name);
    std::string extractTableNameFromCreateSQL(const std::string& create_sql);

    std::string formatBoolean(bool value);
    bool parseBoolean(const std::string& value);
    std::string getCurrentTimestamp();
    std::string getGenericTimestamp();

private:
    void initializeDialect();
    SQLStatementType detectStatementType(const std::string& query);
    
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
    std::string getCurrentDbType();
    std::string preprocessQuery(const std::string& query);
    std::vector<std::string> parseColumnList(const std::string& columns_section);
    std::string trimColumn(const std::string& column);
    std::string extractTableNameFromQuery(const std::string& query);
    std::vector<std::string> getTableColumnsFromSchema(const std::string& table_name);

private:
    DatabaseManager* db_manager_;
    std::unique_ptr<ISQLDialect> dialect_;
    std::string current_db_type_;
};

} // namespace DbLib

#endif // DBLIB_DATABASE_ABSTRACTION_LAYER_HPP