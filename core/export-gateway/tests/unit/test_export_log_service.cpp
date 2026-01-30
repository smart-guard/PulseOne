/**
 * @file test_export_log_service.cpp
 * @brief Unit test for ExportLogService verifying persistence against live DB
 */

#include "Database/Repositories/ExportLogRepository.h"
#include "Database/RepositoryFactory.h"
#include "Export/ExportLogService.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include <cassert>
#include <chrono>
#include <cstdlib> // for setenv
#include <iostream>
#include <thread>
#include <vector>

using namespace PulseOne::Export;
using namespace PulseOne::Database::Entities;
using namespace PulseOne::Database::Repositories;

// Use the standard Docker path for the database
const std::string LIVE_DB_PATH = "/app/data/db/pulseone.db";

void setup_env() {
  // Force RepositoryFactory to use the live DB path
#ifdef _WIN32
  _putenv_s("DATABASE_PATH", LIVE_DB_PATH.c_str());
#else
  setenv("DATABASE_PATH", LIVE_DB_PATH.c_str(), 1);
#endif
  std::cout << "[Setup] Database Path set to: " << LIVE_DB_PATH << std::endl;
}

void test_persistence() {
  std::cout << "[Test] Database Persistence..." << std::endl;
  auto &service = ExportLogService::getInstance();

  // 1. Start Service
  service.start();

  // 2. Create a specific test log entity
  // Use a unique source value to easily identify this test run
  std::string unique_val =
      "UNIT_TEST_" +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count());

  ExportLogEntity log;
  log.setServiceId(999); // Test Service ID
  log.setTargetId(888);  // Test Target ID
  log.setMappingId(777);
  log.setPointId(666);
  log.setSourceValue(unique_val);
  log.setConvertedValue(unique_val);
  log.setStatus("success");
  log.setTimestamp(std::chrono::system_clock::now());
  log.setProcessingTimeMs(10);
  log.setClientInfo("Docker Unit Test");

  // 3. Enqueue Log
  service.enqueueLog(log);
  std::cout << "   -> Log enqueued with SourceValue: " << unique_val
            << std::endl;

  // 4. Wait for Async Worker
  // ExportLogService has a batch timeout (usually 1s), so we wait longer to be
  // safe
  std::cout << "   -> Waiting for async worker (2s)..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // 5. Verification
  auto repo = PulseOne::Database::RepositoryFactory::getInstance()
                  .getExportLogRepository();
  if (!repo) {
    std::cerr << "❌ Failed to get ExportLogRepository!" << std::endl;
    exit(1);
  }

  // We scan the DB for our unique value
  // Note: optimized implementation would use a specific query, but findAll is
  // sufficient for functional verification here
  auto logs = repo->findAll();

  bool found = false;
  for (const auto &l : logs) {
    if (l.getTargetId() == 888 && l.getSourceValue() == unique_val) {
      found = true;
      std::cout << "✅ Found inserted log!" << std::endl;
      auto time_t = std::chrono::system_clock::to_time_t(l.getTimestamp());
      std::cout << "   ID: " << l.getId()
                << ", Created At: " << std::ctime(&time_t);
      break;
    }
  }

  if (!found) {
    std::cerr << "❌ Failed to find the inserted log in DB!" << std::endl;
    std::cerr << "   Searched for SourceValue: " << unique_val << std::endl;
    // Don't dump all logs to avoid spamming output if DB is large
    throw std::runtime_error("Persistence verification failed");
  }

  // 6. Stop Service
  service.stop();
}

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "ExportLogService Docker Integration Test" << std::endl;
  std::cout << "========================================" << std::endl;

  setup_env();

  std::cout << "ENV DATABASE_PATH: "
            << (getenv("DATABASE_PATH") ? getenv("DATABASE_PATH") : "NULL")
            << std::endl;

  // Force ConfigManager to use the correct path, overriding
  // integration_test_config.ini
  ConfigManager::getInstance().set("DATABASE_PATH", LIVE_DB_PATH);

  std::cout << "ConfigManager DATABASE_PATH: "
            << ConfigManager::getInstance().get("DATABASE_PATH") << std::endl;

  // Initialize Core Systems
  LogManager::getInstance().Info("Starting Docker Integration Test");

  if (!PulseOne::Database::RepositoryFactory::getInstance().initialize()) {
    std::cerr << "❌ Failed to initialize RepositoryFactory" << std::endl;
    return 1;
  }

  try {
    test_persistence();
  } catch (const std::exception &e) {
    std::cerr << "❌ Test Failed: " << e.what() << std::endl;
    PulseOne::Database::RepositoryFactory::getInstance().shutdown();
    return 1;
  }

  PulseOne::Database::RepositoryFactory::getInstance().shutdown();

  std::cout << "========================================" << std::endl;
  std::cout << "ALL TESTS PASSED" << std::endl;
  std::cout << "========================================" << std::endl;
  return 0;
}
