#ifndef DBLIB_DATABASE_TYPES_HPP
#define DBLIB_DATABASE_TYPES_HPP

/**
 * @file DatabaseTypes.hpp
 * @brief Generic Database Type Definitions for DbLib
 */

#include <string>
#include <algorithm>
#include <sstream>
#include <vector>
#include "DbExport.hpp"

// Matro conflict prevention
#ifdef max
#undef max
#endif
#ifdef min  
#undef min
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

namespace DbLib {

    /**
     * @brief Type of query value
     */
    enum class ValueType {
        STRING,      // String
        INTEGER,     // Integer
        REAL,        // Real/Double
        BOOLEAN,     // Boolean
        NULL_VALUE,  // NULL
        RAW          // Raw value (for IN, BETWEEN, etc.)
    };

    /**
     * @brief SQL Query Condition structure
     */
    struct DBLIB_API QueryCondition {
        std::string field;        // Field name
        std::string operation;    // Operator (=, !=, >, <, >=, <=, LIKE, IN, BETWEEN)
        std::string value;        // Value as string
        ValueType value_type;     // Type of the value
        
        QueryCondition() 
            : field(""), operation("="), value(""), value_type(ValueType::STRING) {}
        
        QueryCondition(const std::string& f, const std::string& op, const std::string& v)
            : field(f), operation(op), value(v), value_type(ValueType::STRING) {}
        
        QueryCondition(const std::string& f, const std::string& op, 
                      const std::string& v, ValueType vt)
            : field(f), operation(op), value(v), value_type(vt) {}
        
        // Type-safe factory methods
        static QueryCondition String(const std::string& field, 
                                    const std::string& op, 
                                    const std::string& value) {
            return QueryCondition(field, op, value, ValueType::STRING);
        }
        
        static QueryCondition Equal(const std::string& field, const std::string& value) {
            return String(field, "=", value);
        }
        
        static QueryCondition Like(const std::string& field, const std::string& value) {
            return String(field, "LIKE", value);
        }
        
        static QueryCondition Integer(const std::string& field, 
                                     const std::string& op, 
                                     int value) {
            return QueryCondition(field, op, std::to_string(value), ValueType::INTEGER);
        }
        
        static QueryCondition EqualInt(const std::string& field, int value) {
            return Integer(field, "=", value);
        }
        
        static QueryCondition Real(const std::string& field, 
                                  const std::string& op, 
                                  double value) {
            return QueryCondition(field, op, std::to_string(value), ValueType::REAL);
        }
        
        static QueryCondition Boolean(const std::string& field, bool value) {
            return QueryCondition(field, "=", value ? "1" : "0", ValueType::BOOLEAN);
        }
        
        static QueryCondition In(const std::string& field, 
                                const std::vector<std::string>& values) {
            std::ostringstream oss;
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) oss << ",";
                oss << "'" << escapeString(values[i]) << "'";
            }
            return QueryCondition(field, "IN", oss.str(), ValueType::RAW);
        }
        
        static QueryCondition InInt(const std::string& field, 
                                   const std::vector<int>& values) {
            std::ostringstream oss;
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) oss << ",";
                oss << values[i];
            }
            return QueryCondition(field, "IN", oss.str(), ValueType::RAW);
        }
        
        static QueryCondition Between(const std::string& field, 
                                     const std::string& start, 
                                     const std::string& end) {
            return QueryCondition(field, "BETWEEN", 
                                "'" + escapeString(start) + "' AND '" + escapeString(end) + "'",
                                ValueType::RAW);
        }
        
        static QueryCondition IsNull(const std::string& field) {
            return QueryCondition(field, "IS", "NULL", ValueType::NULL_VALUE);
        }
        
        static QueryCondition IsNotNull(const std::string& field) {
            return QueryCondition(field, "IS NOT", "NULL", ValueType::NULL_VALUE);
        }
        
        std::string toSql() const {
            std::ostringstream sql;
            sql << field << " " << operation << " ";
            
            switch (value_type) {
                case ValueType::STRING:
                    sql << "'" << escapeString(value) << "'";
                    break;
                    
                case ValueType::INTEGER:
                case ValueType::REAL:
                    sql << value;
                    break;
                    
                case ValueType::BOOLEAN:
                    sql << value;
                    break;
                    
                case ValueType::NULL_VALUE:
                    sql << value;
                    break;
                    
                case ValueType::RAW:
                    if (operation == "IN") {
                        sql << "(" << value << ")";
                    } else {
                        sql << value;
                    }
                    break;
            }
            
            return sql.str();
        }
        
    private:
        static std::string escapeString(const std::string& str) {
            std::string result;
            result.reserve(str.size());
            
            for (char c : str) {
                if (c == '\'') {
                    result += "''";
                } else if (c == '\\') {
                    result += "\\\\";
                } else {
                    result += c;
                }
            }
            
            return result;
        }
    };
    
    /**
     * @brief SQL ORDER BY structure
     */
    struct DBLIB_API OrderBy {
        std::string field;
        bool ascending;
        
        OrderBy() : field("id"), ascending(true) {}
        
        OrderBy(const std::string& f, bool asc = true)
            : field(f), ascending(asc) {}
        
        static OrderBy Asc(const std::string& field) {
            return OrderBy(field, true);
        }
        
        static OrderBy Desc(const std::string& field) {
            return OrderBy(field, false);
        }
        
        std::string toSql() const {
            return field + (ascending ? " ASC" : " DESC");
        }
    };
    
    /**
     * @brief Pagination structure
     */
    struct DBLIB_API Pagination {
        int limit;
        int offset;
        
        Pagination() : limit(100), offset(0) {}
        
        Pagination(int lim, int off = 0)
            : limit(lim), offset(off) {}
        
        static Pagination FromPage(int page, int page_size) {
            return Pagination(page_size, (page - 1) * page_size);
        }
        
        int getLimit() const { return limit; }
        int getOffset() const { return offset; }
        int getPage() const { return (offset / limit) + 1; }
        int getPageSize() const { return limit; }
        
        Pagination nextPage() const {
            return Pagination(limit, offset + limit);
        }
        
        Pagination prevPage() const {
            int new_offset = (offset - limit > 0) ? (offset - limit) : 0;
            return Pagination(limit, new_offset);
        }
    };

} // namespace DbLib

#endif // DBLIB_DATABASE_TYPES_HPP