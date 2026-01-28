#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using json = nlohmann::json;

int main() {
  // 1. Standard json (sorted by key)
  json j;
  j["timestamp"] = "2026-01-26";
  j["device_id"] = 1;
  j["value"] = 100.5;

  std::cout << "Standard JSON (sorted): " << j.dump() << std::endl;

  // 2. Ordered json (inserted order)
  json oj;
  oj["timestamp"] = "2026-01-26";
  oj["device_id"] = 1;
  oj["value"] = 100.5;

  std::cout << "Ordered JSON (preserved): " << oj.dump() << std::endl;

  return 0;
}
