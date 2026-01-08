#ifndef DBLIB_CORE_ENTITY_HPP
#define DBLIB_CORE_ENTITY_HPP

#include "DbExport.hpp"
#include <chrono>
#include <string>

namespace DbLib {

/**
 * @brief Generic Entity State
 */
enum class EntityState {
    NEW,
    LOADED,
    MODIFIED,
    DELETED,
    ERROR
};

/**
 * @brief Base class for all entities in DbLib
 * @tparam DerivedType CRTP for type-safe operations if needed
 */
template<typename DerivedType>
class DBLIB_API CoreEntity {
public:
    CoreEntity() 
        : id_(0)
        , state_(EntityState::NEW)
        , created_at_(std::chrono::system_clock::now())
        , updated_at_(std::chrono::system_clock::now()) {}
    
    explicit CoreEntity(int id) : CoreEntity() {
        id_ = id;
        if (id > 0) state_ = EntityState::LOADED;
    }
    
    virtual ~CoreEntity() = default;

    // Accessors
    int getId() const { return id_; }
    void setId(int id) { id_ = id; markModified(); }
    
    EntityState getState() const { return state_; }
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }

    // State Management
    bool isLoaded() const { return state_ == EntityState::LOADED; }
    bool isNew() const { return state_ == EntityState::NEW; }
    bool isModified() const { return state_ == EntityState::MODIFIED; }
    bool isDeleted() const { return state_ == EntityState::DELETED; }
    bool isError() const { return state_ == EntityState::ERROR; }
    
    void markModified() {
        if (state_ == EntityState::LOADED) {
            state_ = EntityState::MODIFIED;
            updated_at_ = std::chrono::system_clock::now();
        }
    }
    
    void markDeleted() {
        state_ = EntityState::DELETED;
        updated_at_ = std::chrono::system_clock::now();
    }
    
    void markError() {
        state_ = EntityState::ERROR;
        updated_at_ = std::chrono::system_clock::now();
    }
    
    void markSaved() {
        state_ = EntityState::LOADED;
        updated_at_ = std::chrono::system_clock::now();
    }

protected:
    int id_;
    EntityState state_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
};

} // namespace DbLib

#endif // DBLIB_CORE_ENTITY_HPP
