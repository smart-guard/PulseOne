#ifndef DBLIB_CORE_REPOSITORY_HPP
#define DBLIB_CORE_REPOSITORY_HPP

#include "DbExport.hpp"
#include "DatabaseTypes.hpp"
#include "DatabaseAbstractionLayer.hpp"
#include <vector>
#include <optional>
#include <string>
#include <memory>
#include <sstream>

namespace DbLib {

/**
 * @brief Generic Repository interface for DbLib
 * @tparam EntityType The entity type managed by this repository
 */
template<typename EntityType>
class DBLIB_API CoreRepository {
public:
    explicit CoreRepository(DatabaseAbstractionLayer& db_layer, const std::string& repository_name, const std::string& table_name)
        : db_layer_(db_layer), repository_name_(repository_name), table_name_(table_name) {}
    
    virtual ~CoreRepository() = default;

    // CRUD Operations
    virtual std::optional<EntityType> findById(int id) = 0;
    virtual std::vector<EntityType> findAll() = 0;
    virtual bool save(EntityType& entity) = 0;
    virtual bool update(const EntityType& entity) = 0;
    virtual bool deleteById(int id) = 0;

    // Common query building
    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) {
        if (conditions.empty()) return "";
        std::ostringstream ss;
        ss << " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) ss << " AND ";
            ss << conditions[i].field << " " << conditions[i].operation << " '" << escapeString(conditions[i].value) << "'";
        }
        return ss.str();
    }

    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) {
        if (!order_by.has_value()) return "";
        return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
    }

    std::string buildLimitClause(const std::optional<Pagination>& pagination) {
        if (!pagination.has_value()) return "";
        std::ostringstream ss;
        ss << " LIMIT " << pagination->limit;
        if (pagination->offset > 0) ss << " OFFSET " << pagination->offset;
        return ss.str();
    }

    std::string escapeString(const std::string& str) const {
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "''");
            pos += 2;
        }
        return escaped;
    }

protected:
    DatabaseAbstractionLayer& db_layer_;
    std::string repository_name_;
    std::string table_name_;
};

} // namespace DbLib

#endif // DBLIB_CORE_REPOSITORY_HPP
