// =============================================================================
// core/modbus-slave/src/RegisterTable.cpp
// =============================================================================
#include "RegisterTable.h"
#include <cstring>

namespace PulseOne {
namespace ModbusSlave {

RegisterTable::RegisterTable() {
  holding_regs_.fill(0);
  input_regs_.fill(0);
  coils_.fill(0);
  discrete_inputs_.fill(0);
}

// --- Holding Registers -------------------------------------------------------

void RegisterTable::SetHoldingRegister(uint16_t addr, uint16_t value) {
  std::lock_guard<std::mutex> lock(mutex_);
  holding_regs_[addr] = value;
}

uint16_t RegisterTable::GetHoldingRegister(uint16_t addr) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return holding_regs_[addr];
}

void RegisterTable::SetFloat32(uint16_t addr, float value, bool big_endian) {
  uint32_t raw;
  std::memcpy(&raw, &value, 4);
  std::lock_guard<std::mutex> lock(mutex_);
  if (big_endian) {
    holding_regs_[addr] = static_cast<uint16_t>((raw >> 16) & 0xFFFF);
    holding_regs_[addr + 1] = static_cast<uint16_t>(raw & 0xFFFF);
  } else {
    holding_regs_[addr] = static_cast<uint16_t>(raw & 0xFFFF);
    holding_regs_[addr + 1] = static_cast<uint16_t>((raw >> 16) & 0xFFFF);
  }
}

float RegisterTable::GetFloat32(uint16_t addr, bool big_endian) const {
  std::lock_guard<std::mutex> lock(mutex_);
  uint32_t raw;
  if (big_endian) {
    raw = (static_cast<uint32_t>(holding_regs_[addr]) << 16) |
          static_cast<uint32_t>(holding_regs_[addr + 1]);
  } else {
    raw = (static_cast<uint32_t>(holding_regs_[addr + 1]) << 16) |
          static_cast<uint32_t>(holding_regs_[addr]);
  }
  float f;
  std::memcpy(&f, &raw, 4);
  return f;
}

void RegisterTable::SetInt32(uint16_t addr, int32_t value, bool big_endian) {
  SetUint32(addr, static_cast<uint32_t>(value), big_endian);
}

void RegisterTable::SetUint32(uint16_t addr, uint32_t value, bool big_endian) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (big_endian) {
    holding_regs_[addr] = static_cast<uint16_t>((value >> 16) & 0xFFFF);
    holding_regs_[addr + 1] = static_cast<uint16_t>(value & 0xFFFF);
  } else {
    holding_regs_[addr] = static_cast<uint16_t>(value & 0xFFFF);
    holding_regs_[addr + 1] = static_cast<uint16_t>((value >> 16) & 0xFFFF);
  }
}

int32_t RegisterTable::GetInt32(uint16_t addr, bool big_endian) const {
  return static_cast<int32_t>(GetUint32(addr, big_endian));
}

uint32_t RegisterTable::GetUint32(uint16_t addr, bool big_endian) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (big_endian) {
    return (static_cast<uint32_t>(holding_regs_[addr]) << 16) |
           static_cast<uint32_t>(holding_regs_[addr + 1]);
  } else {
    return (static_cast<uint32_t>(holding_regs_[addr + 1]) << 16) |
           static_cast<uint32_t>(holding_regs_[addr]);
  }
}

// --- Coils -------------------------------------------------------------------

void RegisterTable::SetCoil(uint16_t addr, bool value) {
  std::lock_guard<std::mutex> lock(mutex_);
  coils_[addr] = value ? 1 : 0;
}

bool RegisterTable::GetCoil(uint16_t addr) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return coils_[addr] != 0;
}

// --- Input Registers ---------------------------------------------------------

void RegisterTable::SetInputRegister(uint16_t addr, uint16_t value) {
  std::lock_guard<std::mutex> lock(mutex_);
  input_regs_[addr] = value;
}

uint16_t RegisterTable::GetInputRegister(uint16_t addr) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return input_regs_[addr];
}

// --- Discrete Inputs ---------------------------------------------------------

void RegisterTable::SetDiscreteInput(uint16_t addr, bool value) {
  std::lock_guard<std::mutex> lock(mutex_);
  discrete_inputs_[addr] = value ? 1 : 0;
}

bool RegisterTable::GetDiscreteInput(uint16_t addr) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return discrete_inputs_[addr] != 0;
}

// --- 외부 쓰기 요청
// -----------------------------------------------------------

void RegisterTable::OnExternalRegisterWrite(uint16_t addr, uint16_t value) {
  SetHoldingRegister(addr, value);
  if (reg_write_cb_)
    reg_write_cb_(addr, value);
}

void RegisterTable::OnExternalCoilWrite(uint16_t addr, bool value) {
  SetCoil(addr, value);
  if (coil_write_cb_)
    coil_write_cb_(addr, value);
}

} // namespace ModbusSlave
} // namespace PulseOne
