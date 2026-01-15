#ifndef EDGE_SERVER_REPOSITORY_H
#define EDGE_SERVER_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/EdgeServerEntity.h"
#include <memory>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

using EdgeServerEntity = PulseOne::Database::Entities::EdgeServerEntity;

class EdgeServerRepository : public IRepository<EdgeServerEntity> {
public:
    EdgeServerRepository();
    virtual ~EdgeServerRepository() = default;

    std::vector<EdgeServerEntity> findAll() override;
    std::optional<EdgeServerEntity> findById(int id) override;
    bool save(EdgeServerEntity& entity) override;
    bool update(const EdgeServerEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

private:
    EdgeServerEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::vector<EdgeServerEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // EDGE_SERVER_REPOSITORY_H
