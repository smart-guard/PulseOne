#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include "DatabaseManager.hpp"
#include "DatabaseAbstractionLayer.hpp"
#include "CoreEntity.hpp"
#include "CoreRepository.hpp"

using namespace DbLib;

// Mock Logger
class TestLogger : public IDbLogger {
public:
    void log(const std::string& category, int level, const std::string& message) override {
        std::cout << "[" << category << "] (" << level << ") " << message << std::endl;
    }
};

// Test Entity
class UserEntity : public CoreEntity<UserEntity> {
public:
    using CoreEntity::CoreEntity;
    std::string name;
};

// Test Repository
class UserRepository : public CoreRepository<UserEntity> {
public:
    UserRepository(DatabaseAbstractionLayer& db_layer) 
        : CoreRepository(db_layer, "UserRepository", "users") {}
    
    std::optional<UserEntity> findById(int id) override {
        std::string query = "SELECT * FROM " + table_name_ + " WHERE id = " + std::to_string(id);
        auto results = db_layer_.executeQuery(query);
        if (results.empty()) return std::nullopt;
        
        UserEntity user(id);
        user.name = results[0]["name"];
        return user;
    }
    
    std::vector<UserEntity> findAll() override { return {}; }
    bool save(UserEntity& entity) override {
        std::map<std::string, std::string> data;
        data["name"] = entity.name;
        std::vector<std::string> pks = {"id"};
        return db_layer_.executeUpsert(table_name_, data, pks);
    }
    bool update(const UserEntity&) override { return false; }
    bool deleteById(int) override { return false; }
};

int main() {
    std::cout << "Starting DbLib Core Tests..." << std::endl;
    
    DatabaseConfig config;
    config.type = "SQLITE";
    config.sqlite_path = ":memory:"; // In-memory database for testing
    
    TestLogger logger;
    
    DatabaseManager& db_manager = DatabaseManager::getInstance();
    if (!db_manager.initialize(config, &logger)) {
        std::cerr << "Failed to initialize DatabaseManager" << std::endl;
        return 1;
    }
    
    DatabaseAbstractionLayer db_layer;
    
    // Create table
    bool success = db_layer.executeNonQuery("CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)");
    assert(success);
    std::cout << "Table created successfully" << std::endl;
    
    UserRepository repo(db_layer);
    
    // Test Save
    UserEntity user1;
    user1.name = "Alice";
    success = repo.save(user1);
    assert(success);
    std::cout << "User Alice saved" << std::endl;
    
    // Test Find
    auto found = repo.findById(1);
    assert(found.has_value());
    assert(found->name == "Alice");
    std::cout << "User Alice found by ID 1" << std::endl;
    
    // Test Multiple Inserts
    UserEntity user2;
    user2.name = "Bob";
    success = repo.save(user2);
    assert(success);
    
    found = repo.findById(2);
    assert(found.has_value());
    assert(found->name == "Bob");
    std::cout << "User Bob found by ID 2" << std::endl;
    
    std::cout << "All DbLib Core Tests Passed!" << std::endl;
    
    return 0;
}
