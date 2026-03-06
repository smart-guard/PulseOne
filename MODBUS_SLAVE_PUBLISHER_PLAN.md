# Modbus Slave 출력(Publisher) 기능 구현 계획

> 작성일: 2026-03-06  
> 브랜치: `feature/modbus-slave-publisher` (신규 브랜치 권장)

---

## 1. 용어 정리 — 마스터 vs 슬레이브

| 역할 | 표준 용어 | 현대 용어 | PulseOne 현재 |
|------|---------|---------|-------------|
| 요청하는 쪽 | Master | Client | ✅ **기존 ModbusDriver** |
| 응답하는 쪽 | **Slave** | **Server** | ❌ 미구현 |

**이 계획서의 범위**: PulseOne이 Modbus **Slave(Server)** 역할을 하여, 외부 SCADA/DCS/HMI가 PulseOne에 저장된 데이터를 Modbus로 읽어갈 수 있게 합니다.

```
[외부 SCADA/DCS]  ──Modbus TCP Read──→  [PulseOne Modbus Slave]
                      FC03 HR:100                ↑
                                         수집된 device_points 데이터
```

---

## 2. 어디에 구현할 것인가

### 방식 A: Export Gateway에 `ModbusTargetHandler` 추가 ✅ **권장**

기존 Export Gateway는 이미 `TargetHandler` 플러그인 구조를 갖추고 있음:
- `S3TargetHandler` — S3 업로드
- `HttpTargetHandler` — HTTP POST
- `MqttTargetHandler` — MQTT 발행
- `FileTargetHandler` — 파일 저장
- **→ `ModbusSlaveHandler` 추가** — Modbus TCP 서버로 외부 요청 응답

**장점:**
- 기존 Export Gateway 파이프라인 (Redis 구독 → 데이터 수신 → 타겟 전송) 그대로 재사용
- UI에서 "내보내기 타겟" 설정으로 통합 관리
- 다른 타겟(S3, HTTP)과 동시 병행 가능

### 방식 B: 별도 `modbus-slave-server` 서비스

- 독립 프로세스 (별도 바이너리)
- 장점: 순수 분리, Gateway 코드 불간섭
- 단점: 별도 배포/관리 비용, 기존 파이프라인과 중복

---

## 3. 아키텍처

### 3.1 전체 흐름

```
[Field Device]
      │ Modbus TCP (Master 역할)
      ↓
[PulseOne Collector]
      │ Redis Pub/Sub (측정값 발행)
      ↓
[Redis]
      │ 구독
      ↓
[Export Gateway]
      │
      ├── 기존: S3TargetHandler, HttpTargetHandler, MqttTargetHandler
      │
      └── 신규: ModbusSlaveHandler
                    │ TCP Listen :502 (또는 커스텀 포트)
                    ↓
           [외부 SCADA / DCS / HMI]
                 FC03(Read Holding Registers) 요청
```

### 3.2 ModbusSlaveHandler 내부 구조

```
ModbusSlaveHandler
├── ModbusTcpServer       ← libmodbus 기반 TCP 서버 루프
│   ├── accept() → 클라이언트 연결 수락
│   └── modbus_receive() → FC01/02/03/04/05/06/15/16 처리
├── RegisterTable         ← 메모리 매핑 테이블
│   ├── holding_registers[0..65535]  (uint16_t)
│   ├── input_registers[0..65535]    (uint16_t, read-only)
│   ├── coils[0..65535]              (bool)
│   └── discrete_inputs[0..65535]   (bool, read-only)
└── MappingEngine         ← DataPoint → Register 주소 매핑
    └── device_points.protocol_address → 레지스터 번호
```

---

## 4. 파일 구조

```
core/export-gateway/
├── include/Export/
│   └── ModbusSlaveHandler.h      ← [NEW]
├── src/Gateway/Target/
│   └── ModbusSlaveHandler.cpp    ← [NEW]  (~400줄 예상)
├── include/Export/
│   └── RegisterTable.h           ← [NEW]  레지스터 메모리 테이블
└── src/
    └── RegisterTable.cpp         ← [NEW]

data/sql/
└── schema.sql                    ← [MODIFY] modbus_slave_mappings 테이블

backend/routes/
└── export-gateways.js            ← [MODIFY] Modbus Slave 설정 API

frontend/src/pages/export-gateway/
└── ModbusSlaveConfigPanel.tsx    ← [NEW]  레지스터 매핑 설정 UI
```

---

## 5. 레지스터 주소 매핑 규칙

### 5.1 자동 매핑 (기본)

DataPoint의 `protocol_address` (기존 Modbus 수집 주소)를 그대로 출력 레지스터로 사용.

```
device_points.protocol_address = "HR:100"
→ Modbus Slave Holding Register 100번으로 자동 노출
```

### 5.2 수동 매핑 (커스텀)

사용자가 UI에서 임의 레지스터 번호 지정.

```sql
-- modbus_slave_mappings 테이블
-- point_id → slave_register_address
SELECT p.id, m.slave_register, p.cached_value
FROM device_points p
JOIN modbus_slave_mappings m ON p.id = m.point_id
WHERE m.gateway_id = ?
```

### 5.3 레지스터 타입별 매핑

| DataPoint data_type | Modbus 영역 | 레지스터 수 |
|--------------------|-----------|-----------| 
| BOOLEAN | Coil (FC01) / Discrete Input (FC02) | 1 bit |
| INT16, UINT16 | Holding Register (FC03) | 1 word |
| INT32, UINT32 | Holding Register (FC03) | 2 words |
| FLOAT32 | Holding Register (FC03) | 2 words |
| FLOAT64 | Holding Register (FC03) | 4 words |

---

## 6. DB 스키마

### `schema.sql` 추가

```sql
-- Modbus Slave 출력 매핑 설정
CREATE TABLE IF NOT EXISTS modbus_slave_mappings (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    gateway_id        INTEGER NOT NULL,               -- export_gateways.id
    point_id          INTEGER NOT NULL,               -- device_points.id
    register_type     TEXT    NOT NULL DEFAULT 'HR',  -- HR, IR, CO, DI
    register_address  INTEGER NOT NULL,               -- 0~65535
    data_type         TEXT    NOT NULL DEFAULT 'FLOAT32',
    byte_order        TEXT    NOT NULL DEFAULT 'big_endian',
    scale_factor      REAL    DEFAULT 1.0,
    scale_offset      REAL    DEFAULT 0.0,
    description       TEXT,
    enabled           INTEGER DEFAULT 1,
    created_at        DATETIME DEFAULT (datetime('now','localtime')),
    UNIQUE(gateway_id, register_type, register_address),
    FOREIGN KEY (gateway_id) REFERENCES export_gateways(id) ON DELETE CASCADE,
    FOREIGN KEY (point_id)   REFERENCES device_points(id)   ON DELETE CASCADE
);

-- export_gateways 테이블에 Modbus Slave 설정 컬럼 추가
ALTER TABLE export_gateways ADD COLUMN modbus_slave_port    INTEGER DEFAULT 502;
ALTER TABLE export_gateways ADD COLUMN modbus_slave_unit_id INTEGER DEFAULT 1;
ALTER TABLE export_gateways ADD COLUMN modbus_slave_enabled INTEGER DEFAULT 0;
```

---

## 7. 핵심 C++ 헤더 설계

### `include/Export/RegisterTable.h`

```cpp
#pragma once
#include <array>
#include <mutex>
#include <cstdint>
#include <string>

namespace PulseOne {
namespace Export {

// Modbus 레지스터 메모리 테이블 (스레드 안전)
class RegisterTable {
public:
    // 레지스터 읽기/쓰기 (외부 SCADA 요청 처리)
    uint16_t GetHoldingRegister(uint16_t addr) const;
    uint16_t GetInputRegister(uint16_t addr) const;
    bool     GetCoil(uint16_t addr) const;
    bool     GetDiscreteInput(uint16_t addr) const;

    // 수집 데이터로 테이블 업데이트 (Redis 이벤트 → 레지스터 기록)
    void SetHoldingRegister(uint16_t addr, uint16_t value);
    void SetInputRegister(uint16_t addr, uint16_t value);  // read-only 영역
    void SetCoil(uint16_t addr, bool value);
    void SetDiscreteInput(uint16_t addr, bool value);      // read-only 영역

    // FLOAT32 → 2 레지스터 자동 변환
    void SetFloat32(uint16_t addr, float value, bool big_endian = true);
    float GetFloat32(uint16_t addr, bool big_endian = true) const;

    // Coil 쓰기 요청 콜백 (외부 SCADA가 FC05/15로 쓸 때)
    using WriteCallback = std::function<void(uint16_t addr, bool value)>;
    void SetCoilWriteCallback(WriteCallback cb) { coil_write_cb_ = cb; }
    using RegisterWriteCallback = std::function<void(uint16_t addr, uint16_t value)>;
    void SetRegisterWriteCallback(RegisterWriteCallback cb) { reg_write_cb_ = cb; }

private:
    mutable std::mutex mutex_;
    std::array<uint16_t, 65536> holding_regs_{};
    std::array<uint16_t, 65536> input_regs_{};
    std::array<bool,     65536> coils_{};
    std::array<bool,     65536> discrete_inputs_{};
    WriteCallback         coil_write_cb_;
    RegisterWriteCallback reg_write_cb_;
};

} // namespace Export
} // namespace PulseOne
```

---

### `include/Export/ModbusSlaveHandler.h`

```cpp
#pragma once
#include "Export/TargetHandlerFactory.h"  // ITargetHandler 인터페이스
#include "Export/RegisterTable.h"
#include <thread>
#include <atomic>
#include <vector>
#include <map>

// libmodbus 조건부 포함
#ifdef HAS_MODBUS
  #include <modbus/modbus.h>
#endif

namespace PulseOne {
namespace Export {

// 레지스터 매핑 단위
struct RegisterMapping {
    int      point_id;
    std::string register_type;  // "HR", "IR", "CO", "DI"
    uint16_t address;
    std::string data_type;      // "FLOAT32", "INT16", "BOOLEAN" 등
    std::string byte_order;     // "big_endian", "little_endian"
    double   scale_factor = 1.0;
    double   scale_offset = 0.0;
};

class ModbusSlaveHandler : public ITargetHandler {
public:
    ModbusSlaveHandler();
    ~ModbusSlaveHandler() override;

    // ITargetHandler 인터페이스
    bool Initialize(const GatewayConfig& config) override;
    bool Start() override;
    bool Stop() override;
    TargetSendResult Send(const ExportPayload& payload) override;
    std::string GetHandlerType() const override { return "MODBUS_SLAVE"; }

    // 레지스터 매핑 설정
    void SetMappings(const std::vector<RegisterMapping>& mappings);
    void UpdatePoint(int point_id, double value, const std::string& quality);

private:
    int          port_;
    uint8_t      unit_id_;
    RegisterTable table_;
    std::vector<RegisterMapping> mappings_;
    std::map<int, RegisterMapping*> point_to_mapping_;

    // TCP 서버
    std::thread  server_thread_;
    std::atomic<bool> running_{false};
    void ServerLoop();
    void HandleRequest(modbus_t* ctx);

    // DataPoint 값 → 레지스터 변환
    void WriteToRegister(const RegisterMapping& m, double value);
};

} // namespace Export
} // namespace PulseOne
```

---

## 8. 핵심 구현 — `ModbusSlaveHandler.cpp`

```cpp
bool ModbusSlaveHandler::Start() {
    // libmodbus TCP 서버 생성
    modbus_t* ctx = modbus_new_tcp("0.0.0.0", port_);
    modbus_set_slave(ctx, unit_id_);

    modbus_mapping_t* mb_map = modbus_mapping_new(
        65536,  // coils
        65536,  // discrete inputs
        65536,  // holding registers
        65536   // input registers
    );

    // RegisterTable과 mb_map 동기화 (포인터 공유)
    // mb_map->tab_registers → RegisterTable::holding_regs_ 미러링

    running_ = true;
    server_thread_ = std::thread([this, ctx, mb_map]() {
        ServerLoop(ctx, mb_map);
    });
    return true;
}

void ModbusSlaveHandler::ServerLoop(modbus_t* ctx, modbus_mapping_t* mb_map) {
    int server_socket = modbus_tcp_listen(ctx, 5);  // 최대 5 동시 연결
    fd_set refset;
    FD_ZERO(&refset);
    FD_SET(server_socket, &refset);
    int fdmax = server_socket;

    while (running_) {
        fd_set rdset = refset;
        timeval tv = {1, 0};  // 1초 타임아웃 (종료 체크)

        if (select(fdmax + 1, &rdset, nullptr, nullptr, &tv) <= 0) continue;

        for (int fd = 0; fd <= fdmax; fd++) {
            if (!FD_ISSET(fd, &rdset)) continue;

            if (fd == server_socket) {
                // 신규 클라이언트 접속
                int newfd = modbus_tcp_accept(ctx, &server_socket);
                FD_SET(newfd, &refset);
                fdmax = std::max(fdmax, newfd);
                LOG_INFO("Modbus Client 접속: fd=" + std::to_string(newfd));
            } else {
                // 기존 클라이언트 요청 처리
                uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
                int rc = modbus_set_socket(ctx, fd);
                rc = modbus_receive(ctx, query);

                if (rc > 0) {
                    // RegisterTable → mb_map 동기화 후 응답
                    SyncTableToMbMap(mb_map);
                    modbus_reply(ctx, query, rc, mb_map);
                    // FC05/06/15/16 쓰기 요청 → WriteCallback 호출
                    HandleWriteRequest(ctx, query, rc, mb_map);
                } else if (rc == -1) {
                    // 클라이언트 연결 끊김
                    FD_CLR(fd, &refset);
                    close(fd);
                }
            }
        }
    }
}

// Redis에서 수신한 실시간 값 → 레지스터 업데이트
TargetSendResult ModbusSlaveHandler::Send(const ExportPayload& payload) {
    for (const auto& point : payload.data_points) {
        auto it = point_to_mapping_.find(point.id);
        if (it == point_to_mapping_.end()) continue;
        WriteToRegister(*it->second, point.value);
    }
    return TargetSendResult::Success();
}

void ModbusSlaveHandler::WriteToRegister(const RegisterMapping& m, double val) {
    // 스케일링 역변환: 저장값 = (원시값 - offset) / factor
    double raw = (val - m.scale_offset);
    if (m.scale_factor != 0) raw /= m.scale_factor;

    if (m.register_type == "HR" || m.register_type == "IR") {
        if (m.data_type == "FLOAT32") {
            table_.SetFloat32(m.address, static_cast<float>(raw),
                              m.byte_order == "big_endian");
        } else if (m.data_type == "INT32" || m.data_type == "UINT32") {
            uint32_t v = static_cast<uint32_t>(static_cast<int32_t>(raw));
            bool be = (m.byte_order == "big_endian");
            table_.SetHoldingRegister(m.address,     be ? (v >> 16) : (v & 0xFFFF));
            table_.SetHoldingRegister(m.address + 1, be ? (v & 0xFFFF) : (v >> 16));
        } else {
            table_.SetHoldingRegister(m.address, static_cast<uint16_t>(raw));
        }
    } else if (m.register_type == "CO") {
        table_.SetCoil(m.address, raw != 0.0);
    } else if (m.register_type == "DI") {
        table_.SetDiscreteInput(m.address, raw != 0.0);
    }
}
```

---

## 9. Backend API

### `backend/routes/export-gateways.js` 추가

```
GET    /api/export-gateways/:id/modbus-slave/status
  → 현재 연결된 클라이언트 수, 포트, Unit ID

GET    /api/export-gateways/:id/modbus-slave/mappings
  → modbus_slave_mappings 조회

POST   /api/export-gateways/:id/modbus-slave/mappings
  → 신규 매핑 추가
  { point_id, register_type, register_address, data_type, byte_order }

DELETE /api/export-gateways/:id/modbus-slave/mappings/:mapping_id
  → 매핑 삭제

POST   /api/export-gateways/:id/modbus-slave/mappings/auto-generate
  → DataPoint의 기존 protocol_address 기준 자동 매핑 생성
```

---

## 10. 프론트엔드

### Export Gateway 설정 화면에 "Modbus Slave" 탭 추가

```
[ 기본 정보 ] [ 프로파일 ] [ 템플릿 ] [ 타겟 ] [ 스케줄 ] [ Modbus Slave ]

Modbus Slave 탭:
┌─────────────────────────────────────┐
│  Modbus Slave 출력 활성화  [ ON/OFF ]│
│  포트: [ 502   ]                    │
│  Unit ID: [ 1  ]                   │
│                                     │
│  레지스터 매핑 테이블               │
│  ┌────┬────────┬──────┬──────┐     │
│  │ HR │ 주소   │ 포인트│ 타입 │     │
│  ├────┼────────┼──────┼──────┤     │
│  │ HR │  100   │ 온도1 │ F32 │     │
│  │ HR │  102   │ 압력1 │ F32 │     │
│  │ CO │    0   │ 밸브1 │ BOOL│     │
│  └────┴────────┴──────┴──────┘     │
│  [자동 매핑] [행 추가] [삭제]       │
└─────────────────────────────────────┘
```

---

## 11. 빌드 통합

### `core/export-gateway/Makefile` 수정

```makefile
ifdef HAS_MODBUS
CXXFLAGS += -DHAS_MODBUS=1
CXXFLAGS += -I/usr/local/include/modbus
SRCS += src/Gateway/Target/ModbusSlaveHandler.cpp \
        src/RegisterTable.cpp
LDFLAGS += -lmodbus
endif
```

### TargetHandlerFactory 등록

```cpp
// src/Export/TargetHandlerFactory.cpp에 추가
#ifdef HAS_MODBUS
  #include "Export/ModbusSlaveHandler.h"
  {"MODBUS_SLAVE", []() { return std::make_unique<ModbusSlaveHandler>(); }},
#endif
```

---

## 12. Modbus Slave vs 기존 Modbus Master 비교

| 항목 | Modbus Master (기존) | Modbus Slave (신규) |
|------|--------------------|--------------------|
| **역할** | 외부 장치에서 데이터 읽기 | 외부 시스템에 데이터 제공 |
| **연결 방향** | PulseOne → 장치 (Active) | 외부 → PulseOne (Passive) |
| **라이브러리** | libmodbus 클라이언트 모드 | libmodbus 서버 모드 |
| **위치** | Collector 드라이버 | Export Gateway 타겟 |
| **포트** | 원격 장치 포트로 접속 | PulseOne이 포트 Listen |
| **함수코드** | FC01~04 읽기 | FC01~04 응답, FC05/06 수신 |
| **데이터 흐름** | 장치값 → PulseOne 수집 | 수집값 → 외부 SCADA |

---

## 13. 구현 순서

### Phase 1 — 코어 서버 (2주)

```
Week 1:
  [ ] RegisterTable 구현 및 단위 테스트
  [ ] ModbusSlaveHandler 기본 골격 (Initialize/Start/Stop)
  [ ] libmodbus TCP 서버 루프 (단일 클라이언트 우선)
  [ ] FC03 (Read Holding Registers) 처리
  [ ] Send() 함수: Redis 이벤트 → 레지스터 업데이트

Week 2:
  [ ] 멀티 클라이언트 (fd_set select 루프)
  [ ] FC01(Coil), FC02(DI), FC04(IR) 지원
  [ ] FC05/06/15/16 쓰기 요청 처리 (WriteCallback)
  [ ] FLOAT32 2-word 조립 (big/little endian)
  [ ] TargetHandlerFactory 등록
  [ ] libiec61850 예제와 동일한 Modbus Slave 테스트툴 기반 E2E 검증
```

### Phase 2 — DB + Backend + Frontend (1주)

```
  [ ] schema.sql modbus_slave_mappings 테이블
  [ ] backend CRUD API
  [ ] 자동 매핑 생성 API
  [ ] 프론트엔드 레지스터 매핑 테이블 UI
  [ ] Modbus Slave 탭 (활성화/포트/Unit ID 설정)
```

---

## 14. 테스트 환경

```bash
# PyModbus로 간단히 읽기 테스트 (Python)
pip install pymodbus
python3 -c "
from pymodbus.client import ModbusTcpClient
c = ModbusTcpClient('127.0.0.1', port=502)
c.connect()
r = c.read_holding_registers(100, 2)  # FLOAT32 @ HR100
import struct
f = struct.unpack('>f', struct.pack('>HH', r.registers[0], r.registers[1]))[0]
print(f'HR100 (FLOAT32) = {f}')
c.close()
"

# mbpoll (CLI 도구)
mbpoll -a 1 -r 100 -c 10 -t 4 127.0.0.1

# Modbus Poll (Windows GUI 도구)
# → 레지스터 전체 모니터링
```

---

## 15. 파일 영향 요약

| 파일/폴더 | 구분 | 내용 |
|----------|------|------|
| `core/export-gateway/src/Gateway/Target/ModbusSlaveHandler.cpp` | **[NEW]** | TCP 서버 + 레지스터 응답 |
| `core/export-gateway/include/Export/ModbusSlaveHandler.h` | **[NEW]** | 헤더 |
| `core/export-gateway/src/RegisterTable.cpp` | **[NEW]** | 레지스터 메모리 테이블 |
| `core/export-gateway/include/Export/RegisterTable.h` | **[NEW]** | 헤더 |
| `core/export-gateway/src/Export/TargetHandlerFactory.cpp` | **[MODIFY]** | MODBUS_SLAVE 등록 |
| `core/export-gateway/Makefile` | **[MODIFY]** | HAS_MODBUS 블록 |
| `data/sql/schema.sql` | **[MODIFY]** | modbus_slave_mappings 테이블 |
| `backend/routes/export-gateways.js` | **[MODIFY]** | Slave 설정 API |
| `frontend/src/pages/export-gateway/` | **[MODIFY]** | Modbus Slave 탭 |
