# IEC 61850 드라이버 구현 계획

> 브랜치: `feature/iec61850-driver`  
> 작성일: 2026-03-06  
> 대상 용도: **HMI 모니터링 및 제어 (MMS 전용)**  
> GOOSE: 제외 (보호계전기 간 통신, HMI 불필요)

---

## 1. IEC 61850이 기존 드라이버와 다른 점

### 1.1 데이터 주소 체계

```
Modbus:     address = 40001                 (레지스터 번호)
BACnet:     address = "AI:1"                (객체 타입:인스턴스)
IEC 61850:  address = "CTRL/XCBR1.Pos.stVal"  (LD/LN.DO.DA)
```

**IEC 61850 데이터 계층 구조:**
```
IED (지능형 전자장치, e.g. "IED_BREAKER_01")
└── LD (논리장치, e.g. "CTRL", "MEAS", "PROT")
     └── LN (논리노드, IEC 61850 표준 정의)
          ├── XCBR: 차단기
          ├── MMXU: 전력 측정
          ├── PTOC: 과전류 보호
          └── LLN0: 논리노드 0 (RCB 포함)
               └── DO (데이터객체)
                    ├── Pos: 위치
                    ├── A:   전류
                    └── Op:  동작
                         └── DA (데이터속성)
                              ├── stVal:    상태값 (bool/enum)
                              ├── mag.f:    크기 (float)
                              └── Oper.ctlVal: 제어값
```

**device_points 테이블 매핑:**
| 필드 | Modbus | IEC 61850 |
|------|--------|-----------|
| `protocol_address` | `"40001"` | `"CTRL/XCBR1.Pos.stVal"` |
| `data_type` | `"INT16"` | `"BOOLEAN"` / `"FLOAT32"` |
| `unit` | `"V"` | `"A"` (IEC 61850 단위 자동 파싱) |

---

### 1.2 통신 패턴

| | Modbus/BACnet | IEC 61850 MMS |
|--|--------------|---------------|
| **수신 방식** | 주기 폴링 (2초마다 ReadValues) | **BRCB Report** (변화 시 IED가 자동 푸시) |
| **연결 유지** | 요청-응답 후 선택 | **상시 TCP 연결 유지** |
| **포트** | 502, 47808 | **102** (MMS/ACSE 표준) |
| **데이터 포맷** | 바이너리 레지스터 | **ASN.1 BER (MMS PDU)** |

**BRCB (Buffered Report Control Block) 흐름:**
```
[PulseOne] → EnableReport("LLN0.BR.urcb01") → [IED]
                                                  ↓ 값 변화 발생
[PulseOne] ← Report(XCBR1.Pos.stVal = TRUE) ← [IED]
[PulseOne] ← Report(MMXU1.A.phsA.mag.f = 12.3) ← [IED]
```

→ `ReadValues()` 폴링은 보조 수단 (초기값 읽기, 폴백)  
→ **주 경로는 `Subscribe()` + `MessageCallback` 패턴**

---

### 1.3 설정 방식 — SCL 파일

모든 IED 구성은 **SCL (Substation Configuration Language)** XML로 정의됨.

```xml
<!-- example.cid — IED 인스턴스 파일 -->
<IED name="IED_BREAKER_01" manufacturer="ABB" type="REX521">
  <AccessPoint name="S1">
    <Server>
      <LDevice inst="CTRL">
        <LN lnClass="LLN0" inst="">
          <DataSet name="dsWarnings">
            <FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1"
                  doName="Pos" daName="stVal" fc="ST"/>
          </DataSet>
          <ReportControl name="urcb01" confRev="1" datSet="dsWarnings"
                         rptID="IED1/CTRL/LLN0.BR.urcb01"
                         buffered="false" intgPd="0">
            <TrgOps dchg="true" qchg="true" period="false"/>
            <RptEnabled max="5"/>
          </ReportControl>
        </LN>
        <LN lnClass="XCBR" inst="1">
          <DO name="Pos" cdc="DPS"/>  <!-- 이중 위치 상태 -->
        </LN>
      </LDevice>
    </Server>
  </AccessPoint>
</IED>
```

---

## 2. 라이브러리: libiec61850

```
출처:  https://github.com/mz-automation/libiec61850
언어:  C (C++ 바인딩 포함)
라이선스: GPLv3 (오픈소스) / 상업 라이선스 (유료)
성숙도: ✅ KEMA 테스트 레퍼런스 구현으로 검증됨
```

### 2.1 라이선스 전략 — 프로세스 격리

GPL 전파는 **링킹(linking)** 에서만 발생. 프로세스 경계 IPC는 해당 없음.

```
[pulseone-collector] ──IPC(Unix Socket)──→ [iec61850-bridge]
     (비공개 가능)                              (GPLv3, 소스 공개)
          ↑                                          ↑
   IProtocolDriver                          libiec61850 직접 링킹
   인터페이스 구현                           MMS/ACSE/ASN.1 처리
```

**장점:**
- 비용 0원
- PulseOne 본체 소스 비공개 유지
- `iec61850-bridge` 소스 공개 (어차피 표준 구현이라 문제 없음)
- `iec61850-bridge` 충돌/크래시가 collector 메인 프로세스 격리

---

## 3. 전체 파일 구조

### 3.1 신규 생성 파일

```
PulseOne/
├── IEC61850_DRIVER_PLAN.md              ← 이 파일
│
├── third_party/
│   └── libiec61850/                     ← git submodule (GPLv3)
│       (cmake 빌드 → static lib)
│
├── core/
│   ├── iec61850-bridge/                 ← 독립 프로세스 (GPLv3 구역)
│   │   ├── CMakeLists.txt
│   │   ├── Makefile
│   │   ├── Makefile.windows
│   │   ├── main.cpp                     ← 진입점, IPC 서버 루프
│   │   ├── include/
│   │   │   ├── BridgeProtocol.h         ← IPC 메시지 구조체 (JSON)
│   │   │   ├── MmsClientWrapper.h
│   │   │   ├── BrcbManager.h
│   │   │   └── SclParser.h
│   │   └── src/
│   │       ├── MmsClientWrapper.cpp     ← libiec61850 MMS API 래퍼
│   │       ├── BrcbManager.cpp          ← BRCB 활성화/콜백 관리
│   │       ├── SclParser.cpp            ← SCL XML → 내부 IED 모델
│   │       └── IpcServer.cpp            ← Unix Socket / Named Pipe 서버
│   │
│   └── collector/
│       ├── include/Drivers/Iec61850/
│       │   ├── Iec61850Driver.h         ← IProtocolDriver 구현 헤더
│       │   └── Iec61850Types.h          ← IED/LD/LN/DO/DA 내부 타입
│       └── src/Drivers/Iec61850/
│           ├── Iec61850Driver.cpp       ← IProtocolDriver 구현 (IPC 클라이언트)
│           ├── Iec61850BridgeClient.cpp ← bridge 프로세스 관리 + 소켓 통신
│           └── Makefile                 ← 드라이버 플러그인 빌드
│
└── data/
    └── scl/                             ← IED SCL 파일 저장소
        └── README.md
```

### 3.2 수정 파일

```
core/collector/
├── include/Drivers/Common/DriverFactory.h   ← HAS_IEC61850 블록 추가
├── Makefile                                  ← IEC61850 드라이버 링킹 추가
└── Makefile.windows                          ← Windows 빌드 추가

data/sql/
├── schema.sql                                ← devices, iec61850_rcb_settings
├── seed.sql                                  ← IEC61850 프로토콜 메타데이터

backend/routes/
└── devices.js                                ← SCL 업로드 API

frontend/src/pages/devices/
└── (신규 컴포넌트)                            ← IED 트리 뷰, SCL 업로드
```

---

## 4. IPC 프로토콜 (collector ↔ bridge)

Unix Domain Socket (Linux) / Named Pipe (Windows) 기반.  
메시지 형식: **JSON Lines** (개행 구분, 길이 프리픽스 4바이트)

### 4.1 요청 메시지 종류

```json
// CONNECT — IED 연결
{"cmd":"CONNECT","id":1,"host":"192.168.1.10","port":102,"ied_name":"IED01",
 "scl_path":"/data/scl/IED01.cid","timeout_ms":5000}

// READ — MMS GetDataValues
{"cmd":"READ","id":2,"refs":["CTRL/XCBR1.Pos.stVal","MEAS/MMXU1.A.phsA.mag.f"]}

// WRITE — MMS SetDataValues / Control
{"cmd":"WRITE","id":3,"ref":"CTRL/XCBR1.Pos.Oper.ctlVal","value":true,"type":"BOOLEAN"}

// SUBSCRIBE — BRCB 활성화
{"cmd":"SUBSCRIBE","id":4,"rcb_ref":"CTRL/LLN0.BR.urcb01","dataset":"dsWarnings"}

// UNSUBSCRIBE
{"cmd":"UNSUBSCRIBE","id":5,"rcb_ref":"CTRL/LLN0.BR.urcb01"}

// DISCOVER — SCL 파싱 → DataPoint 목록
{"cmd":"DISCOVER","id":6}

// DISCONNECT
{"cmd":"DISCONNECT","id":7}
```

### 4.2 응답 메시지 종류

```json
// 연결 성공
{"id":1,"ok":true,"ied_name":"IED01","server_version":"IEC61850-8-1"}

// 읽기 결과
{"id":2,"ok":true,"values":[
  {"ref":"CTRL/XCBR1.Pos.stVal","value":true,"type":"BOOLEAN",
   "quality":"GOOD","timestamp":"2026-03-06T10:00:00.000Z"},
  {"ref":"MEAS/MMXU1.A.phsA.mag.f","value":12.34,"type":"FLOAT32",
   "quality":"GOOD","timestamp":"2026-03-06T10:00:00.000Z"}
]}

// BRCB 리포트 이벤트 (비동기, id=-1)
{"id":-1,"event":"REPORT","rcb_ref":"CTRL/LLN0.BR.urcb01",
 "values":[{"ref":"CTRL/XCBR1.Pos.stVal","value":false,
            "quality":"GOOD","timestamp":"2026-03-06T10:00:05.123Z"}]}

// Discover 결과
{"id":6,"ok":true,"points":[
  {"ref":"CTRL/XCBR1.Pos.stVal","cdc":"DPS","da":"stVal",
   "fc":"ST","type":"BOOLEAN","description":"차단기 위치"},
  {"ref":"MEAS/MMXU1.A.phsA.mag.f","cdc":"MV","da":"mag.f",
   "fc":"MX","type":"FLOAT32","unit":"A","description":"A상 전류"}
]}

// 에러
{"id":2,"ok":false,"error":"MMS_ERROR_ACCESS_OBJECT_DOES_NOT_EXIST",
 "message":"데이터 객체를 찾을 수 없습니다"}
```

---

## 5. 헤더 설계

### 5.1 `include/Drivers/Iec61850/Iec61850Types.h`

```cpp
#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace PulseOne {
namespace Drivers {

// IEC 61850 주소 파싱 결과
struct Iec61850Ref {
    std::string ld_inst;   // "CTRL"
    std::string ln_class;  // "XCBR"
    std::string ln_inst;   // "1"
    std::string do_name;   // "Pos"
    std::string da_name;   // "stVal"
    std::string fc;        // "ST", "MX", "CO" 등

    // "CTRL/XCBR1.Pos.stVal" → 파싱
    static Iec61850Ref Parse(const std::string& ref);
    std::string ToString() const;
};

// SCL에서 파싱된 DataPoint 메타데이터
struct Iec61850PointMeta {
    std::string ref;         // 전체 참조 문자열
    std::string cdc;         // Common Data Class (DPS, MV, SPS 등)
    std::string da_name;     // 데이터속성 이름
    std::string fc;          // Functional Constraint
    std::string type;        // BOOLEAN, FLOAT32, INT32, ENUM
    std::string unit;        // A, V, W, Hz 등
    std::string description; // SCL desc 속성
};

// MMS 값 래퍼 (타입 안전)
struct MmsValue {
    enum class Type { BOOLEAN, INT32, UINT32, FLOAT32, FLOAT64,
                      VISIBLE_STRING, UTC_TIME, BIT_STRING };
    Type type;
    union {
        bool     b;
        int32_t  i32;
        uint32_t u32;
        float    f32;
        double   f64;
    } val;
    std::string str_val;          // VISIBLE_STRING용
    std::chrono::system_clock::time_point timestamp;
    std::string quality;          // "GOOD", "INVALID", "QUESTIONABLE"
};

// BRCB 설정
struct BrcbConfig {
    std::string rcb_ref;       // "CTRL/LLN0.BR.urcb01"
    std::string dataset_ref;   // "CTRL/LLN0.dsWarnings"
    bool trigger_dchg = true;  // 데이터 변화
    bool trigger_qchg = true;  // 품질 변화
    bool trigger_period = false;
    uint32_t period_ms = 0;
    bool buffered = true;      // BRCB (true) vs URCB (false)
};

} // namespace Drivers
} // namespace PulseOne
```

---

### 5.2 `include/Drivers/Iec61850/Iec61850Driver.h`

```cpp
#pragma once
#include "Drivers/Common/IProtocolDriver.h"
#include "Iec61850Types.h"
#include <memory>
#include <thread>
#include <atomic>

namespace PulseOne {
namespace Drivers {

class Iec61850BridgeClient;  // 전방 선언

class Iec61850Driver : public IProtocolDriver {
public:
    Iec61850Driver();
    ~Iec61850Driver() override;

    // IProtocolDriver 필수 구현
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    bool Start() override;
    bool Stop() override;

    // 폴링 기반 읽기 (초기값 조회, 폴백용)
    bool ReadValues(const std::vector<DataPoint>& points,
                    std::vector<TimestampedValue>& values) override;

    // MMS SetDataValues / Control Service
    bool WriteValue(const DataPoint& point,
                    const DataValue& value) override;

    // BRCB 리포트 구독 — ref = "CTRL/LLN0.BR.urcb01"
    bool Subscribe(const std::string& rcb_ref, int qos = 0) override;
    bool Unsubscribe(const std::string& rcb_ref) override;

    // SCL → DataPoint 자동 생성
    std::vector<DataPoint> DiscoverPoints() override;

    std::string GetProtocolType() const override { return "IEC61850"; }
    DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;

    // IEC 61850 전용 확장
    bool LoadScl(const std::string& scl_path);
    bool SetBrcbConfig(const BrcbConfig& config);

private:
    std::unique_ptr<Iec61850BridgeClient> bridge_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_running_{false};
    std::string ied_name_;
    std::string scl_path_;
    ErrorInfo last_error_;

    // BRCB 이벤트 → MessageCallback 변환
    void OnBridgeReport(const std::string& json);
    // bridge IPC 수신 스레드
    std::thread recv_thread_;
    void RecvLoop();
};

// DriverFactory.h에서 자동 등록
// REGISTER_DRIVER("IEC61850", Iec61850Driver)

} // namespace Drivers
} // namespace PulseOne
```

---

## 6. 핵심 구현 — `Iec61850BridgeClient.cpp`

```cpp
// === BridgeClient: bridge 프로세스 수명 관리 + IPC ===

bool Iec61850BridgeClient::Start(const std::string& host,
                                  int port,
                                  const std::string& scl_path,
                                  const std::string& ied_name) {
    // 1. Unix Socket 경로
    socket_path_ = "/tmp/pulseone_iec61850_" + std::to_string(getpid()) + ".sock";

    // 2. iec61850-bridge 프로세스 fork/spawn
    pid_ = SpawnBridge(socket_path_);
    if (pid_ < 0) return false;

    // 3. 소켓 연결
    if (!ConnectSocket(socket_path_, 3000)) return false;

    // 4. CONNECT 명령 전송
    nlohmann::json req = {
        {"cmd", "CONNECT"}, {"id", NextId()},
        {"host", host}, {"port", port},
        {"ied_name", ied_name}, {"scl_path", scl_path}
    };
    auto resp = SendAndWait(req.dump());
    return resp["ok"].get<bool>();
}

// Windows: CreateProcess + Named Pipe
// Linux:   fork/exec + Unix Domain Socket
pid_t Iec61850BridgeClient::SpawnBridge(const std::string& socket_path) {
#ifdef _WIN32
    // ... CreateProcess("iec61850-bridge.exe", ...)
#else
    pid_t pid = fork();
    if (pid == 0) {
        // 자식 프로세스
        execlp("iec61850-bridge", "iec61850-bridge",
               "--socket", socket_path.c_str(), nullptr);
        exit(1);
    }
    return pid;
#endif
}
```

---

## 7. `core/iec61850-bridge/src/MmsClientWrapper.cpp`

```cpp
// libiec61850 API를 bridge 내부에서 사용
#include <iec61850_client.h>

bool MmsClientWrapper::Connect(const std::string& host, int port) {
    IedClientError error;
    connection_ = IedConnection_create();
    IedConnection_connect(connection_, &error, host.c_str(), port);
    if (error != IED_ERROR_OK) {
        last_error_ = IedClientError_toString(error);
        return false;
    }
    return true;
}

// BRCB 리포트 활성화
bool BrcbManager::EnableReport(const std::string& rcb_ref,
                                ReportCallback cb) {
    IedClientError error;
    auto* rcb = IedConnection_getRCBValues(connection_, &error,
                                            rcb_ref.c_str(), nullptr);
    if (error != IED_ERROR_OK) return false;

    // 리포트 활성화
    ClientReportControlBlock_setRptEna(rcb, true);
    IedConnection_setRCBValues(connection_, &error, rcb,
                               RCB_ELEMENT_RPT_ENA, true);

    // 콜백 등록
    IedConnection_installReportHandler(connection_, rcb_ref.c_str(),
        ClientReportControlBlock_getRptId(rcb),
        [](void* param, ClientReport report) {
            auto* self = static_cast<BrcbManager*>(param);
            self->OnReport(report);
        }, this);

    ClientReportControlBlock_destroy(rcb);
    return true;
}

// 리포트 이벤트 → JSON 직렬화 → IPC 전송
void BrcbManager::OnReport(ClientReport report) {
    nlohmann::json msg;
    msg["id"] = -1; // 비동기 이벤트
    msg["event"] = "REPORT";
    msg["rcb_ref"] = ClientReport_getRcbReference(report);

    auto* values = ClientReport_getDataSetValues(report);
    nlohmann::json vals = nlohmann::json::array();

    int count = MmsValue_getArraySize(values);
    for (int i = 0; i < count; i++) {
        MmsValue* val = MmsValue_getElement(values, i);
        nlohmann::json v;
        v["ref"] = entry_refs_[i]; // SCL에서 파싱한 참조
        v["value"] = MmsValueToJson(val);
        v["type"] = MmsTypeToString(MmsValue_getType(val));
        v["quality"] = "GOOD"; // 품질 파싱 추가 필요
        v["timestamp"] = Timestamp();
        vals.push_back(v);
    }
    msg["values"] = vals;

    ipc_server_->Send(msg.dump() + "\n");
}
```

---

## 8. SclParser 핵심 로직

```cpp
// SCL .cid/.iid/.scd → DataPoint 목록 생성
std::vector<Iec61850PointMeta> SclParser::Parse(const std::string& filepath) {
    tinyxml2::XMLDocument doc;
    doc.LoadFile(filepath.c_str());

    std::string ied_name = // IED name 속성 파싱
    std::vector<Iec61850PointMeta> points;

    // LDevice → LN → DO 순서로 순회
    for (auto* ld = ...; ld; ld = ld->NextSiblingElement("LDevice")) {
        std::string ld_inst = ld->Attribute("inst");
        for (auto* ln = ...; ln; ln = ln->NextSiblingElement("LN")) {
            std::string ln_class = ln->Attribute("lnClass");
            std::string ln_inst  = ln->Attribute("inst") ?: "";
            for (auto* do_ = ...; do_; do_ = do_->NextSiblingElement("DO")) {
                // DataAttributes 파싱
                auto meta = ResolveCdc(ln_class, do_->Attribute("name"),
                                        do_->Attribute("cdc"));
                for (auto& da : meta) {
                    da.ref = ld_inst + "/" + ln_class + ln_inst
                           + "." + do_->Attribute("name")
                           + "." + da.da_name;
                    points.push_back(da);
                }
            }
        }
    }
    return points;
}

// CDC(Common Data Class) → DA 목록 자동 생성
std::vector<Iec61850PointMeta> SclParser::ResolveCdc(
        const std::string& cdc) {
    // IEC 61850-7-3 CDC 정의에 따라
    static const std::map<std::string, std::vector<Iec61850PointMeta>> cdc_map = {
        {"SPS", {{"stVal","ST","BOOLEAN"}, {"q","ST","QUALITY"}}},
        {"DPS", {{"stVal","ST","ENUM"},   {"q","ST","QUALITY"}}},
        {"MV",  {{"mag.f","MX","FLOAT32"},{"q","MX","QUALITY"},
                 {"units.SIUnit","MX","INT8"}}},
        {"CMV", {{"cVal.mag.f","MX","FLOAT32"},{"cVal.ang.f","MX","FLOAT32"}}},
        {"BSC", {{"valWTr.posVal","CO","ENUM"},{"valWTr.transInd","CO","BOOLEAN"}}},
        // ... 전체 CDC 목록
    };
    auto it = cdc_map.find(cdc);
    return it != cdc_map.end() ? it->second : std::vector<Iec61850PointMeta>{};
}
```

---

## 9. DriverFactory 등록

### `core/collector/src/Drivers/Iec61850/Iec61850Driver.cpp` 하단에 추가:

```cpp
// 자동 등록 매크로 (DriverFactory.h 정의)
#ifdef HAS_IEC61850
REGISTER_DRIVER("IEC61850", PulseOne::Drivers::Iec61850Driver)
#endif
```

### `core/collector/Makefile` 수정:

```makefile
# IEC 61850 드라이버 (선택적 빌드)
ifdef HAS_IEC61850
CXXFLAGS += -DHAS_IEC61850=1
DRIVERS   += src/Drivers/Iec61850/Iec61850Driver.o \
             src/Drivers/Iec61850/Iec61850BridgeClient.o
endif
```

---

## 10. DB 스키마 변경

### `data/sql/schema.sql` 추가:

```sql
-- devices 테이블 IEC 61850 컬럼 추가
ALTER TABLE devices ADD COLUMN scl_file_path TEXT;    -- "/data/scl/IED01.cid"
ALTER TABLE devices ADD COLUMN ied_name      TEXT;    -- "IED_BREAKER_01"
ALTER TABLE devices ADD COLUMN ld_inst       TEXT;    -- "CTRL,MEAS" (복수)

-- IEC 61850 BRCB 설정 테이블
CREATE TABLE IF NOT EXISTS iec61850_rcb_settings (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id    INTEGER NOT NULL,
    rcb_ref      TEXT    NOT NULL,  -- "CTRL/LLN0.BR.urcb01"
    dataset_ref  TEXT,              -- "CTRL/LLN0.dsWarnings"
    trigger_dchg INTEGER DEFAULT 1, -- 데이터 변화 트리거
    trigger_qchg INTEGER DEFAULT 1, -- 품질 변화 트리거
    trigger_period INTEGER DEFAULT 0,
    period_ms    INTEGER DEFAULT 0,
    buffered     INTEGER DEFAULT 1, -- 1=BRCB, 0=URCB
    enabled      INTEGER DEFAULT 1,
    created_at   DATETIME DEFAULT (datetime('now','localtime')),
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- protocols 테이블에 IEC61850 추가 (seed.sql)
-- INSERT INTO protocols (name, display_name, default_port) 
--   VALUES ('IEC61850', 'IEC 61850 MMS', 102);
```

---

## 11. Backend API

### `backend/routes/devices.js` 추가:

```
POST   /api/devices/:id/scl-upload
  → multer로 SCL 파일 수신
  → /data/scl/ 저장
  → SclParser (Node.js 버전 또는 bridge 호출)로 DataPoint 파싱
  → device_points 일괄 삽입
  → devices.scl_file_path 업데이트
  → 임포트된 DataPoint 수 반환

GET    /api/devices/:id/ied-tree
  → SCL 파일 파싱 → IED/LD/LN 트리 JSON 반환
  {
    "ied_name": "IED01",
    "logical_devices": [{
      "inst": "CTRL",
      "logical_nodes": [{
        "lnClass": "XCBR", "inst": "1",
        "data_objects": [{"name":"Pos","cdc":"DPS","points":[...]}]
      }]
    }]
  }

GET    /api/devices/:id/rcb-settings
  → iec61850_rcb_settings 조회

PUT    /api/devices/:id/rcb-settings
  → BRCB 설정 저장/업데이트
```

---

## 12. 프론트엔드 컴포넌트

### 장치 등록 위자드에 IEC 61850 탭 추가

```
장치 등록 (IEC 61850)
├── 1. 기본 정보 (host, port=102, ied_name)
├── 2. SCL 파일 업로드
│   ├── 드래그-앤-드롭 영역
│   ├── IED 트리 미리보기
│   └── DataPoint 임포트 버튼 (N개 자동 등록)
└── 3. BRCB 설정
    ├── Report Control Block 선택
    ├── 트리거 옵션 (dchg/qchg/period)
    └── 인터벌 설정
```

---

## 13. 빌드 시스템

### `third_party/libiec61850` git submodule

```bash
# 최초 설정
git submodule add https://github.com/mz-automation/libiec61850 third_party/libiec61850
git submodule update --init

# bridge 빌드 (Linux)
cd core/iec61850-bridge && cmake . && make

# bridge 빌드 (Windows cross-compile)
cd core/iec61850-bridge && cmake -DCMAKE_TOOLCHAIN_FILE=../../mingw-toolchain.cmake . && make
```

### `core/iec61850-bridge/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.10)
project(iec61850-bridge)

set(LIBIEC61850_DIR ${CMAKE_SOURCE_DIR}/../../third_party/libiec61850)

# libiec61850 정적 빌드
add_subdirectory(${LIBIEC61850_DIR} libiec61850_build)

add_executable(iec61850-bridge
    src/main.cpp
    src/MmsClientWrapper.cpp
    src/BrcbManager.cpp
    src/SclParser.cpp
    src/IpcServer.cpp
)

target_include_directories(iec61850-bridge PRIVATE
    include
    ${LIBIEC61850_DIR}/src/mms/inc
    ${LIBIEC61850_DIR}/src/iec61850/inc
    ${LIBIEC61850_DIR}/src/common/inc
    ${LIBIEC61850_DIR}/third_party/tinyxml2  # SCL 파싱
)

target_link_libraries(iec61850-bridge
    iec61850
    pthread   # Linux
    ws2_32    # Windows
)
```

---

## 14. 구현 순서

### Phase 1 — 코어 드라이버 (3주 예상)

```
Week 1:
  [ ] libiec61850 git submodule 추가 및 빌드 검증
  [ ] SclParser: tinyxml2 기반 SCL 파싱 (IED/LD/LN/DO/DA → 내부 모델)
  [ ] CDC → DataPoint 매핑 테이블 (SPS/DPS/MV/CMV 우선)

Week 2:
  [ ] Iec61850Types.h 정의 완성
  [ ] MmsClientWrapper: Connect/Disconnect/GetDataValues/SetDataValues
  [ ] BrcbManager: BRCB 활성화, Report 콜백 → JSON 직렬화

Week 3:
  [ ] IpcServer (Unix Socket / Windows Named Pipe)
  [ ] Iec61850BridgeClient (bridge spawn, 소켓 통신)
  [ ] Iec61850Driver (IProtocolDriver 구현 완성)
  [ ] DriverFactory 등록 및 빌드 통합
  [ ] libiec61850 제공 server_example로 E2E 테스트
```

### Phase 2 — DB + Backend + Frontend (1.5주 예상)

```
  [ ] schema.sql 마이그레이션 작성
  [ ] backend: SCL 업로드 API (multer + SclParser Node.js 포트 or bridge 호출)
  [ ] backend: IED 트리 API
  [ ] backend: RCB 설정 CRUD API
  [ ] frontend: SCL 업로드 드롭존
  [ ] frontend: IED 트리 컴포넌트
  [ ] frontend: BRCB 설정 패널
```

---

## 15. 테스트 환경

```bash
# libiec61850 제공 가상 IED 서버 (로컬 테스트)
cd third_party/libiec61850/examples/server_example_basic
./server_example_basic

# 연결 확인 (IEDExplorer 또는 직접)
./iec61850-bridge --host 127.0.0.1 --port 102 \
                  --ied IED1 --scl server_example.cid

# Wireshark 캡처 (MMS 패킷 확인)
tshark -i lo -f "port 102" -d tcp.port==102,mms
```

---

## 16. 라이선스 요약

| 구성요소 | 라이선스 | 배포 시 의무 |
|---------|---------|------------|
| `iec61850-bridge` 소스 | GPLv3 | 소스 공개 필요 |
| `libiec61850` | GPLv3 | `iec61850-bridge`에 포함됨 |
| `Iec61850Driver.cpp` | PulseOne 독자 | 비공개 가능 |
| PulseOne 본체 | PulseOne 독자 | 비공개 가능 |

> **인증**: MMS 인증(KEMA/UCAIug)은 네트워크 레벨 검사 → 아키텍처 무관.  
> libiec61850은 실제 KEMA 인증 레퍼런스 구현으로 검증된 라이브러리.
