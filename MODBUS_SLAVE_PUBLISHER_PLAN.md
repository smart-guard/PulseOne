# Modbus Slave Server 구현 계획

> 작성일: 2026-03-06 (v2 — 독립 서비스 방식)  
> 브랜치: `feature/modbus-slave-publisher` (신규 브랜치 권장)  
> **Export Gateway 코드 무변경** — Redis를 공용 버스로 직접 사용

---

## 1. 용어 정리

| 역할 | 표준 용어 | PulseOne 현재 |
|------|---------|-------------|
| 필드 장치에서 읽는 쪽 | Modbus Master(Client) | ✅ Collector ModbusDriver |
| 외부 요청에 응답하는 쪽 | **Modbus Slave(Server)** | ❌ 미구현 |

**이 계획서의 목표**: 외부 SCADA/DCS/HMI가 PulseOne에 수집된 데이터를 Modbus TCP로 읽어갈 수 있는 **독립 서비스** 구현.

---

## 2. 아키텍처

### 2.1 전체 흐름

```
[Field Device]
      │ Modbus TCP (Master)
      ↓
[pulseone-collector.exe]
      │ Redis PUBLISH "point:update" {point_id, value, ...}
      ↓
[Redis :6379]
      │ SUBSCRIBE (독립 연결)
      ↓
[pulseone-modbus-slave.exe]  ← 이것만 신규
      │ Register Table 메모리 갱신
      ↓ TCP :502 Listen
[외부 SCADA / DCS / HMI]
      FC03: Read Holding Registers 요청
```

### 2.2 Export Gateway와 무관계

```
[Export Gateway] ──S3/HTTP/MQTT──→ 외부 시스템  ← 기존, 건드리지 않음
[Modbus Slave  ] ←── FC03 ─────── 외부 SCADA  ← 신규, 완전 독립
```

두 서비스 모두 Redis에서 값을 읽지만 **서로 코드 공유 없음**.

---

## 3. 배포 구조

```
dist_windows/
├── pulseone-backend.exe           ← 기존 (변경 없음)
├── pulseone-collector.exe         ← 기존 (변경 없음)
├── pulseone-export-gateway.exe    ← 기존 (변경 없음)
└── pulseone-modbus-slave.exe      ← 신규
```

`install.bat`에서 다른 서비스들과 동일하게 WinSW 등록:

```batch
rem ModbusSlave 서비스
(
  echo ^<service^>
  echo   ^<id^>PulseOne-ModbusSlave^</id^>
  echo   ^<name^>PulseOne Modbus Slave^</name^>
  echo   ^<executable^>%ROOT%\pulseone-modbus-slave.exe^</executable^>
  echo   ^<workingdirectory^>%ROOT%^</workingdirectory^>
  echo   ^<startuptype^>Automatic^</startuptype^>
  echo   ^<onfailure action="restart" delay="5000"/^>
  echo   ^<onfailure action="restart" delay="15000"/^>
  echo   ^<onfailure action="restart" delay="30000"/^>
  echo   ^<resetfailure^>3600^</resetfailure^>
  echo ^</service^>
) > pulseone-modbus-slave.xml
winsw install pulseone-modbus-slave.xml
winsw start   pulseone-modbus-slave.xml
```

---

## 4. 파일 구조

```
core/
└── modbus-slave/                          ← 완전 신규 (Export Gateway 무관)
    ├── CMakeLists.txt
    ├── Makefile
    ├── Makefile.windows                   ← MinGW 크로스 컴파일
    ├── main.cpp                           ← 진입점
    ├── include/
    │   ├── ModbusSlaveServer.h            ← TCP 서버 클래스
    │   ├── RegisterTable.h               ← 레지스터 메모리 테이블
    │   └── RedisSubscriber.h             ← Redis 값 수신
    └── src/
        ├── ModbusSlaveServer.cpp
        ├── RegisterTable.cpp
        ├── RedisSubscriber.cpp
        └── MappingConfig.cpp              ← JSON 매핑 파일 로드

data/
└── modbus-slave-mapping.json              ← 레지스터 매핑 설정

data/sql/
└── schema.sql                            ← [MODIFY] modbus_slave_mappings 테이블

backend/routes/
└── modbus-slave.js                       ← [NEW] 매핑 설정 API

frontend/src/pages/
└── ModbusSlavePage.tsx                   ← [NEW] 레지스터 모니터링 + 설정 UI
```

---

## 5. 클래스 설계

### 5.1 `RegisterTable` — 레지스터 메모리 (스레드 안전)

```cpp
class RegisterTable {
public:
    // 레지스터 쓰기 (Redis 구독 스레드가 갱신)
    void SetHoldingRegister(uint16_t addr, uint16_t value);
    void SetFloat32(uint16_t addr, float value, bool big_endian = true);
    void SetCoil(uint16_t addr, bool value);

    // 레지스터 읽기 (TCP 서버 스레드가 응답)
    uint16_t GetHoldingRegister(uint16_t addr) const;
    bool     GetCoil(uint16_t addr) const;

    // libmodbus가 직접 접근할 수 있는 raw 포인터 제공
    uint16_t* tab_registers()  { return holding_regs_.data(); }
    uint8_t*  tab_bits()       { return coils_.data(); }

private:
    mutable std::mutex mutex_;
    std::array<uint16_t, 65536> holding_regs_{};
    std::array<uint8_t,  65536> coils_{};
    // input_registers, discrete_inputs 필요 시 추가
};
```

### 5.2 `RedisSubscriber` — Redis → 레지스터 갱신

```cpp
class RedisSubscriber {
public:
    // Redis에 연결하고 point:update 채널 구독
    bool Start(const std::string& host, int port,
               RegisterTable& table,
               const std::vector<RegisterMapping>& mappings);
    void Stop();

private:
    void SubscribeLoop();   // 별도 스레드
    void OnMessage(const std::string& channel, const std::string& payload);
    // payload 예시: {"point_id":42,"value":23.5,"quality":"GOOD"}
    //               → mapping에서 point_id=42 → HR:100 FLOAT32
    //               → table.SetFloat32(100, 23.5f)
};
```

### 5.3 `ModbusSlaveServer` — TCP 서버

```cpp
class ModbusSlaveServer {
public:
    bool Start(int port, uint8_t unit_id, RegisterTable& table);
    void Stop();

private:
    void ServerLoop();     // 별도 스레드
    // libmodbus modbus_tcp_listen + select() 루프
    // modbus_receive() → modbus_reply() (RegisterTable raw ptr 사용)
```

### 5.4 `main.cpp` — 진입점

```cpp
int main(int argc, char** argv) {
    // 1. 설정 로드 (환경변수 or config/.env)
    int tcp_port  = getenv("MODBUS_SLAVE_PORT")  ? atoi(getenv("MODBUS_SLAVE_PORT"))  : 502;
    int unit_id   = getenv("MODBUS_SLAVE_UNIT")  ? atoi(getenv("MODBUS_SLAVE_UNIT"))  : 1;
    std::string redis_host = getenv("REDIS_HOST") ?: "127.0.0.1";

    // 2. 레지스터 매핑 로드 (DB or JSON)
    auto mappings = MappingConfig::Load("data/modbus-slave-mapping.json");

    // 3. 서비스 시작
    RegisterTable   table;
    RedisSubscriber redis;
    ModbusSlaveServer server;

    redis.Start(redis_host, 6379, table, mappings);
    server.Start(tcp_port, unit_id, table);

    // 4. 시그널 대기 (SIGTERM/SIGINT)
    WaitForShutdown();

    server.Stop();
    redis.Stop();
    return 0;
}
```

---

## 6. 레지스터 매핑

### 6.1 매핑 설정 파일 (`data/modbus-slave-mapping.json`)

```json
{
  "unit_id": 1,
  "tcp_port": 502,
  "mappings": [
    {
      "point_id": 42,
      "point_name": "온도_1번_센서",
      "register_type": "HR",
      "register_address": 100,
      "data_type": "FLOAT32",
      "byte_order": "big_endian"
    },
    {
      "point_id": 43,
      "point_name": "압력_1번_센서",
      "register_type": "HR",
      "register_address": 102,
      "data_type": "FLOAT32",
      "byte_order": "big_endian"
    },
    {
      "point_id": 77,
      "point_name": "밸브_개폐",
      "register_type": "CO",
      "register_address": 0,
      "data_type": "BOOLEAN"
    }
  ]
}
```

### 6.2 데이터타입 → 레지스터 수

| DataType | 레지스터 영역 | 소비 word 수 |
|---------|------------|------------|
| BOOLEAN | Coil (CO) | 1 bit |
| INT16, UINT16 | HR | 1 word |
| INT32, UINT32 | HR | 2 words |
| **FLOAT32** | HR | 2 words |
| FLOAT64 | HR | 4 words |

### 6.3 DB 스키마 (백엔드 연동 시)

```sql
CREATE TABLE IF NOT EXISTS modbus_slave_mappings (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    point_id         INTEGER NOT NULL,
    register_type    TEXT    NOT NULL DEFAULT 'HR',   -- HR, CO
    register_address INTEGER NOT NULL,                -- 0~65535
    data_type        TEXT    NOT NULL DEFAULT 'FLOAT32',
    byte_order       TEXT    NOT NULL DEFAULT 'big_endian',
    scale_factor     REAL    DEFAULT 1.0,
    scale_offset     REAL    DEFAULT 0.0,
    enabled          INTEGER DEFAULT 1,
    UNIQUE(register_type, register_address),
    FOREIGN KEY (point_id) REFERENCES device_points(id) ON DELETE CASCADE
);
```

---

## 7. 빌드 통합

### `core/modbus-slave/Makefile.windows` (MinGW 크로스 컴파일)

```makefile
CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -DWIN32 -I../../core/shared/include \
           -I/usr/x86_64-w64-mingw32/include
LDFLAGS  = -lws2_32 -lmodbus

SRCS = src/main.cpp src/ModbusSlaveServer.cpp \
       src/RegisterTable.cpp src/RedisSubscriber.cpp \
       src/MappingConfig.cpp

TARGET = pulseone-modbus-slave.exe

$(TARGET): $(SRCS:.cpp=.o)
	$(CXX) -o $@ $^ $(LDFLAGS)
```

### `deploy-windows.sh`에 추가 (빌드 단계)

```bash
# [4.5/5] ModbusSlave 빌드
echo "🔨 Modbus Slave 빌드 중..."
docker exec pulseone-win-builder bash -c \
  "cd /workspace/core/modbus-slave && make -f Makefile.windows"
cp core/modbus-slave/pulseone-modbus-slave.exe bin-windows/
echo "✅ ModbusSlave → bin-windows/ ($(du -sh bin-windows/pulseone-modbus-slave.exe | cut -f1))"
```

---

## 8. 성능 특성

| 항목 | 수치 |
|------|------|
| 레지스터 읽기 응답 | < 1ms (메모리 조회) |
| Redis → 레지스터 갱신 지연 | < 5ms (Pub/Sub) |
| 외부 SCADA 읽기 지연 | Collector 폴링 주기 (보통 1~5초) |
| 동시 SCADA 클라이언트 | select() 멀티플렉싱으로 10개 이상 |
| 메모리 사용 | 65536 × 2byte × 2(HR+IR) = 256KB |

---

## 9. 구현 순서

### Phase 1 — 코어 서버 (2주)

```
Week 1:
  [ ] core/modbus-slave/ 폴더 및 CMakeLists.txt 생성
  [ ] RegisterTable 구현 + 단위 테스트
  [ ] RedisSubscriber: hiredis 기반 SUBSCRIBE 루프
  [ ] MappingConfig: JSON 파싱 (nlohmann/json)

Week 2:
  [ ] ModbusSlaveServer: libmodbus TCP Listen 루프
  [ ] FC03(HR 읽기), FC01(Coil 읽기) 처리
  [ ] FC06/05 쓰기 요청 처리 (WriteCallback → Redis PUBLISH)
  [ ] main.cpp 통합
  [ ] Makefile.windows (MinGW 크로스 컴파일)
  [ ] deploy-windows.sh 빌드 단계 추가
  [ ] install.bat WinSW 등록 추가
```

### Phase 2 — Backend + Frontend (1주)

```
  [ ] schema.sql modbus_slave_mappings 추가
  [ ] backend/routes/modbus-slave.js CRUD API
  [ ] JSON 매핑 파일 ↔ DB 동기화
  [ ] frontend: 레지스터 매핑 설정 UI
  [ ] frontend: 실시간 레지스터 값 모니터링 (polling /api/modbus-slave/status)
```

---

## 10. 테스트

```bash
# PyModbus로 빠른 검증
pip install pymodbus
python3 -c "
from pymodbus.client import ModbusTcpClient
import struct
c = ModbusTcpClient('127.0.0.1', port=502)
c.connect()

# FLOAT32 HR100 읽기
r = c.read_holding_registers(100, 2, unit=1)
val = struct.unpack('>f', struct.pack('>HH', r.registers[0], r.registers[1]))[0]
print(f'온도 = {val:.2f}')

c.close()
"

# CLI 도구
mbpoll -a 1 -r 100 -c 10 -t 4:float 127.0.0.1
```

---

## 11. 파일 영향 요약

| 파일/폴더 | 구분 | 설명 |
|----------|------|------|
| `core/modbus-slave/` | **[NEW]** | 독립 C++ 프로젝트 |
| `deploy-windows.sh` | **[MODIFY]** | 빌드 단계 추가 |
| `dist_windows/install.bat` | **[MODIFY]** | WinSW 서비스 등록 추가 |
| `data/sql/schema.sql` | **[MODIFY]** | 매핑 테이블 추가 |
| `backend/routes/modbus-slave.js` | **[NEW]** | 매핑 CRUD API |
| `frontend/src/pages/ModbusSlavePage.tsx` | **[NEW]** | 설정 + 모니터링 UI |
| `core/export-gateway/` | **변경 없음** ✅ | 완전 독립 |
| `core/collector/` | **변경 없음** ✅ | 완전 독립 |
