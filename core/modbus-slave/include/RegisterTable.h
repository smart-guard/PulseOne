// =============================================================================
// core/modbus-slave/include/RegisterTable.h
// 스레드 안전 Modbus 레지스터 메모리 테이블
// =============================================================================
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace PulseOne {
namespace ModbusSlave {

// 레지스터 쓰기 콜백 (외부 SCADA FC05/06으로 쓸 때 역방향 제어용)
using CoilWriteCallback = std::function<void(uint16_t addr, bool value)>;
using RegisterWriteCallback =
    std::function<void(uint16_t addr, uint16_t value)>;

class RegisterTable {
public:
  RegisterTable();

  // -------------------------------------------------------------------------
  // Holding Registers (FC03 읽기, FC06/16 쓰기)
  // -------------------------------------------------------------------------
  void SetHoldingRegister(uint16_t addr, uint16_t value);
  uint16_t GetHoldingRegister(uint16_t addr) const;

  // FLOAT32 ↔ 2 레지스터 (big/little endian)
  void SetFloat32(uint16_t addr, float value, bool big_endian = true);
  float GetFloat32(uint16_t addr, bool big_endian = true) const;

  // INT32 / UINT32 ↔ 2 레지스터
  void SetInt32(uint16_t addr, int32_t value, bool big_endian = true);
  void SetUint32(uint16_t addr, uint32_t value, bool big_endian = true);
  int32_t GetInt32(uint16_t addr, bool big_endian = true) const;
  uint32_t GetUint32(uint16_t addr, bool big_endian = true) const;

  // -------------------------------------------------------------------------
  // Coils (FC01 읽기, FC05/15 쓰기)
  // -------------------------------------------------------------------------
  void SetCoil(uint16_t addr, bool value);
  bool GetCoil(uint16_t addr) const;

  // -------------------------------------------------------------------------
  // Input Registers (FC04, read-only — 수집 데이터용)
  // -------------------------------------------------------------------------
  void SetInputRegister(uint16_t addr, uint16_t value);
  uint16_t GetInputRegister(uint16_t addr) const;

  // -------------------------------------------------------------------------
  // Discrete Inputs (FC02, read-only)
  // -------------------------------------------------------------------------
  void SetDiscreteInput(uint16_t addr, bool value);
  bool GetDiscreteInput(uint16_t addr) const;

  // -------------------------------------------------------------------------
  // libmodbus raw 포인터 접근 (modbus_reply에서 직접 사용)
  // 주의: 반드시 LockForModbus() / UnlockForModbus() 짝으로 사용
  // -------------------------------------------------------------------------
  uint16_t *RawHoldingRegs() { return holding_regs_.data(); }
  uint16_t *RawInputRegs() { return input_regs_.data(); }
  uint8_t *RawCoils() { return coils_.data(); }
  uint8_t *RawDiscreteInputs() { return discrete_inputs_.data(); }

  void LockForModbus() { mutex_.lock(); }
  void UnlockForModbus() { mutex_.unlock(); }

  // -------------------------------------------------------------------------
  // 역방향 제어 콜백 (외부 SCADA가 FC06/05로 쓸 때 호출됨)
  // -------------------------------------------------------------------------
  void SetRegisterWriteCallback(RegisterWriteCallback cb) {
    reg_write_cb_ = cb;
  }
  void SetCoilWriteCallback(CoilWriteCallback cb) { coil_write_cb_ = cb; }

  // 외부 쓰기 요청 처리 (ModbusSlaveServer에서 호출)
  void OnExternalRegisterWrite(uint16_t addr, uint16_t value);
  void OnExternalCoilWrite(uint16_t addr, bool value);

  static constexpr size_t TABLE_SIZE = 65536;

private:
  mutable std::mutex mutex_;

  std::array<uint16_t, TABLE_SIZE> holding_regs_{};
  std::array<uint16_t, TABLE_SIZE> input_regs_{};
  std::array<uint8_t, TABLE_SIZE> coils_{};
  std::array<uint8_t, TABLE_SIZE> discrete_inputs_{};

  RegisterWriteCallback reg_write_cb_;
  CoilWriteCallback coil_write_cb_;
};

} // namespace ModbusSlave
} // namespace PulseOne
