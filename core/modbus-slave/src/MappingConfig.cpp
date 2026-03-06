// =============================================================================
// core/modbus-slave/src/MappingConfig.cpp
// JSON 설정 파일 로드 + 환경변수 오버라이드
// =============================================================================
#include "MappingConfig.h"
#include <cstdlib>
#include <fstream>
#include <iostream>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace ModbusSlave {

RegisterType ParseRegisterType(const std::string &s) {
  if (s == "HR")
    return RegisterType::HOLDING_REGISTER;
  if (s == "IR")
    return RegisterType::INPUT_REGISTER;
  if (s == "CO")
    return RegisterType::COIL;
  if (s == "DI")
    return RegisterType::DISCRETE_INPUT;
  return RegisterType::HOLDING_REGISTER; // 기본값
}

DataType ParseDataType(const std::string &s) {
  if (s == "BOOLEAN")
    return DataType::BOOLEAN;
  if (s == "INT16")
    return DataType::INT16;
  if (s == "UINT16")
    return DataType::UINT16;
  if (s == "INT32")
    return DataType::INT32;
  if (s == "UINT32")
    return DataType::UINT32;
  if (s == "FLOAT32")
    return DataType::FLOAT32;
  if (s == "FLOAT64")
    return DataType::FLOAT64;
  return DataType::FLOAT32;
}

int DataTypeWordCount(DataType t) {
  switch (t) {
  case DataType::BOOLEAN:
  case DataType::INT16:
  case DataType::UINT16:
    return 1;
  case DataType::INT32:
  case DataType::UINT32:
  case DataType::FLOAT32:
    return 2;
  case DataType::FLOAT64:
    return 4;
  }
  return 1;
}

SlaveConfig MappingConfig::DefaultConfig() { return SlaveConfig{}; }

SlaveConfig MappingConfig::LoadFromFile(const std::string &path) {
  SlaveConfig config = DefaultConfig();

#ifdef HAS_NLOHMANN_JSON
  std::ifstream f(path);
  if (!f.is_open()) {
    std::cout << "[MappingConfig] 파일 없음: " << path << " — 기본 설정 사용\n";
    return config;
  }

  try {
    json j;
    f >> j;

    if (j.contains("tcp_port"))
      config.tcp_port = j["tcp_port"];
    if (j.contains("unit_id"))
      config.unit_id = j["unit_id"];
    if (j.contains("max_clients"))
      config.max_clients = j["max_clients"];
    if (j.contains("redis_host"))
      config.redis_host = j["redis_host"];
    if (j.contains("redis_port"))
      config.redis_port = j["redis_port"];
    if (j.contains("redis_channel"))
      config.redis_channel = j["redis_channel"];

    for (const auto &m : j.value("mappings", json::array())) {
      RegisterMapping rm;
      rm.point_id = m.value("point_id", 0);
      rm.point_name = m.value("point_name", "");
      rm.register_type = ParseRegisterType(m.value("register_type", "HR"));
      rm.address = static_cast<uint16_t>(m.value("register_address", 0));
      rm.data_type = ParseDataType(m.value("data_type", "FLOAT32"));
      rm.big_endian = (m.value("byte_order", "big_endian") == "big_endian");
      rm.scale_factor = m.value("scale_factor", 1.0);
      rm.scale_offset = m.value("scale_offset", 0.0);
      rm.enabled = m.value("enabled", true);
      config.mappings.push_back(rm);
    }

    std::cout << "[MappingConfig] 로드 완료: " << path << " ("
              << config.mappings.size() << "개 매핑)\n";
  } catch (const std::exception &e) {
    std::cerr << "[MappingConfig] JSON 파싱 오류: " << e.what() << "\n";
  }
#else
  std::cout << "[MappingConfig] nlohmann/json 없음 — 기본 설정 사용\n";
#endif

  return config;
}

void MappingConfig::ApplyEnvOverrides(SlaveConfig &config) {
  if (const char *v = std::getenv("MODBUS_SLAVE_PORT"))
    config.tcp_port = std::atoi(v);
  if (const char *v = std::getenv("MODBUS_SLAVE_UNIT"))
    config.unit_id = static_cast<uint8_t>(std::atoi(v));
  if (const char *v = std::getenv("REDIS_HOST"))
    config.redis_host = v;
  if (const char *v = std::getenv("REDIS_PORT"))
    config.redis_port = std::atoi(v);
}

} // namespace ModbusSlave
} // namespace PulseOne
