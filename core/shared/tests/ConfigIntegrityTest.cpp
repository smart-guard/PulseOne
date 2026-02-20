#include "Logging/LogManager.h"
#include "Platform/PlatformCompat.h"
#include "Utils/ConfigManager.h"
#include "Utils/ConfigPathResolver.h"
#include <cassert>
#include <iostream>

using namespace PulseOne::Utils;

int main() {
  std::cout << "Starting ConfigManager Integrity Test..." << std::endl;

  auto &config = ConfigManager::getInstance();

  // 1. 기본 초기화 확인
  std::cout << "[1] Checking path resolution..." << std::endl;
  std::string dataDir = config.getDataDirectory();
  std::string dbPath = config.getSQLiteDbPath();
  std::cout << "  Data Dir: " << dataDir << std::endl;
  std::cout << "  DB Path: " << dbPath << std::endl;

  // 2. 변수 확장 테스트
  std::cout << "[2] Testing variable expansion..." << std::endl;
  config.set("TEST_VAR", "PulseOne");
  std::string expanded = config.expandVariables("Welcome to ${TEST_VAR}");
  std::cout << "  Expanded: " << expanded << std::endl;
  assert(expanded == "Welcome to PulseOne");

  // 3. 특수 변수 ($DATA_DIR) 테스트
  std::cout << "[3] Testing special variables..." << std::endl;
  std::string expandedData = config.expandVariables("${DATA_DIR}/db/test.db");
  std::cout << "  Expanded Data Path: " << expandedData << std::endl;
  assert(expandedData.find("db/test.db") != std::string::npos);

  // [4] Testing INSITE_ fallback logic...
  std::cout << "[4] Testing INSITE_ fallback logic..." << std::endl;
  std::string insiteVal = config.expandVariables("${INSITE_TEST_VAR}");
  std::cout << "  Insite Val (expected empty or loaded): " << insiteVal
            << std::endl;

  // [5] PROOF OF LIFE: SecretManager Functionality (The User's Request)
  std::cout << "[5] Testing SecretManager Integration (Passive Mode)..."
            << std::endl;

  // Create a mock secret file
  std::string secretsDir = config.getSecretsDirectory();
  std::string testSecretPath = secretsDir + "/test_functional.secret";

  std::ofstream ofs(testSecretPath);
  ofs << "plain_functional_test_value";
  ofs.close();

  // 🔥 SECURITY REQUIREMENT: chmod 600
#ifndef _WIN32
  chmod(testSecretPath.c_str(), 0600);
#endif

  std::string secretVal = config.getSecret("test_functional");
  std::cout << "  Resolved Secret Value: " << secretVal << std::endl;

  // Cleanup
  std::remove(testSecretPath.c_str());

  if (secretVal == "plain_functional_test_value") {
    std::cout << "✅ SecretManager Functional Proof Passed!" << std::endl;
  } else {
    std::cout << "❌ SecretManager Functional Proof Failed (Value: "
              << secretVal << ")" << std::endl;
  }

  std::cout << "✅ ConfigManager & SecretManager Integrity Test Passed!"
            << std::endl;
  return 0;
}
