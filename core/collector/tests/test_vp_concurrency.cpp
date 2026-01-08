#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "VirtualPoint/VirtualPointEngine.h"
#include "Logging/LogManager.h"

#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"

using namespace PulseOne::VirtualPoint;
using namespace PulseOne::Database;
using json = nlohmann::json;

class VirtualPointConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Database for Registry
        auto& db_manager = DbLib::DatabaseManager::getInstance();
        DbLib::DatabaseConfig db_config;
        db_config.type = "SQLITE";
        db_config.sqlite_path = ":memory:";
        db_manager.initialize(db_config);
        
        RepositoryFactory::getInstance().initialize();

        auto& engine = VirtualPointEngine::getInstance();
        engine.initialize();
    }
};

TEST_F(VirtualPointConcurrencyTest, ConcurrentCalculationStressTest) {
    auto& engine = VirtualPointEngine::getInstance();
    
    // Formula that simulates some work and uses inputs
    std::string formula = "var x = a + b; x * 2;";
    json inputs = {{"a", 10}, {"b", 20}};
    
    const int num_threads = 10;
    const int iterations_per_thread = 100;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < iterations_per_thread; ++j) {
                // We use different inputs for some threads to ensure no cross-talk
                json local_inputs = inputs;
                local_inputs["thread_id"] = i;
                local_inputs["iter"] = j;
                
                try {
                    auto result = engine.calculateWithFormula(formula, local_inputs);
                    if (result.success && result.value == 60.0) {
                        success_count++;
                    } else {
                        failure_count++;
                    }
                } catch (...) {
                    failure_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(success_count, num_threads * iterations_per_thread);
    EXPECT_EQ(failure_count, 0);
    
    std::cout << "✅ Concurrent Stress Test Passed: " << success_count << " successful calculations." << std::endl;
}
