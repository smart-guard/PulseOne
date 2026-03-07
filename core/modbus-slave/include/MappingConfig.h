// =============================================================================
// core/modbus-slave/include/MappingConfig.h
// 레지스터 매핑 설정 (JSON 파일 로드)
// =============================================================================
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace ModbusSlave {

// 레지스터 타입
enum class RegisterType {
  HOLDING_REGISTER, // HR — FC03/06/16
  INPUT_REGISTER,   // IR — FC04 (read-only)
  COIL,             // CO — FC01/05/15
  DISCRETE_INPUT    // DI — FC02 (read-only)
};

// 데이터 타입별 레지스터 소비 word 수
enum class DataType {
  BOOLEAN, // 1 bit (Coil/DI)
  INT16,   // 1 word
  UINT16,  // 1 word
  INT32,   // 2 words
  UINT32,  // 2 words
  FLOAT32, // 2 words
  FLOAT64  // 4 words
};

// DataPoint → Modbus 레지스터 매핑 단위
struct RegisterMapping {
  int point_id = 0;
  std::string point_name;
  RegisterType register_type = RegisterType::HOLDING_REGISTER;
  uint16_t address = 0;
  DataType data_type = DataType::FLOAT32;
  bool big_endian = true; // byte order within each word
  bool word_swap =
      false; // true: 상위/하위 word 교환 (big_endian_swap / little_endian_swap)
  // scale: register_raw = (collected_value * scale_factor) + scale_offset
  // SCADA Master가 역산: value = (raw - offset) / factor
  double scale_factor = 1.0;
  double scale_offset = 0.0;
  bool enabled = true;
};

// 전체 서비스 설정
struct SlaveConfig {
  int tcp_port = 502;
  uint8_t unit_id = 1;
  int max_clients = 10;
  std::string redis_host = "127.0.0.1";
  int redis_port = 6379;
  std::string redis_channel = "point:update"; // Collector가 발행하는 채널
  bool packet_logging = false;                // 패킷 로그 파일 활성화
  std::vector<RegisterMapping> mappings;
};

// 유틸 함수
RegisterType ParseRegisterType(const std::string &s);
DataType ParseDataType(const std::string &s);
std::string RegisterTypeToString(RegisterType t);
std::string DataTypeToString(DataType t);
int DataTypeWordCount(DataType t); // 레지스터 소비 word 수

class MappingConfig {
public:
  // JSON 파일에서 로드
  static SlaveConfig LoadFromFile(const std::string &path);

  // 환경변수 오버라이드 적용 (config에 병합)
  static void ApplyEnvOverrides(SlaveConfig &config);

  // 기본 설정 값 반환 (파일 없을 때)
  static SlaveConfig DefaultConfig();
};

} // namespace ModbusSlave
} // namespace PulseOne
