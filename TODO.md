# PulseOne 빌드 재개 가이드

## 현재 상태
- `--param ggc-min-expand=0` 제거 완료 (Makefile.windows)
- `plugins/` → `drivers/` 폴더명 변경 완료 (전체)
- 기존 1시간+ hung 빌드 종료 필요

## 재시작 절차

### 1. 기존 빌드 종료 (터미널에서)
```bash
docker ps -q | xargs docker kill
```

### 2. Windows 배포 패키지 빌드
```bash
cd /Users/kyungho/Project/PulseOne
SKIP_FRONTEND=true bash deploy-windows.sh
```
> `SKIP_FRONTEND=true`를 **앞에** 붙여야 합니다 (argv가 아닌 env var)

### 3. 빌드 완료 확인
```
dist/deploy-v6.1.0/
├── pulseone-collector.exe
├── pulseone-export-gateway.exe
└── drivers/
    ├── modbus_driver.dll
    ├── bacnet_driver.dll
    ├── mqtt_driver.dll
    ├── opcua_driver.dll
    ├── ros_driver.dll
    ├── ble_driver.dll
    └── httprest_driver.dll
```

## 변경된 파일 목록
| 파일 | 변경 내용 |
|---|---|
| `core/collector/Makefile.windows` | shared .a 링킹 방식으로 재작성, `--param ggc-min-expand=0` 제거, `-O1` |
| `core/collector/src/Core/Application.cpp` | 드라이버 로드 경로 `plugins` → `drivers` |
| `core/collector/Makefile` | `PLUGINS_DIR` → `drivers` |
| `core/collector/src/Drivers/*/Makefile.windows` (7개) | `PLUGINS_DIR` → `drivers` |
| `deploy-windows.sh` | Export Gateway 경로 `bin/` → `bin-win/`, drivers 경로 수정 |
| `deploy-linux.sh` | 절대경로 수정, .so 복사 로직 개선 |
