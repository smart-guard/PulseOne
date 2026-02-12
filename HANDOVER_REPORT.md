# 윈도우 드라이버 DLL 리팩토링 및 배포/테스트 전략 보고서

본 문서는 현재까지 진행된 윈도우 드라이버 DLL 리팩토링 작업 현황, 향후 세부 작업 계획, 그리고 플랫폼별 배포 및 테스트 전략을 상세히 설명합니다.

---

## 1. 현재 작업 현황 (Detailed Progress)

### ✅ 완료된 핵심 작업
- **드라이버 독립 컴파일 구조 (DLL)**:
    - 7개 드라이버(Modbus, BACnet, MQTT, OPCUA, ROS, BLE, HttpRest)를 각각 standalone `.dll`로 빌드하도록 `Makefile.windows` 전면 개편.
    - 메인 `collector.exe`에서 드라이버 소스 코드를 제거하여 바이너리 경량화 및 의존성 분리 성공.
- **동적 로딩 엔진 (Plugin Architecture)**:
    - `PluginLoader` 구현: 실행 파일과 동일한 경로의 `plugins/` 디렉토리를 자동 스캔하여 로드.
    - `Platform::DynamicLibrary` 추상화: Windows(`LoadLibraryA`)와 Linux(`dlopen`) 통합 지원.
    - 모든 드라이버에 공통 진입점 `extern "C" RegisterPlugin()` 구현 및 `__declspec(dllexport)` 적용.
- **코드 무결성 수정**:
    - `ROSDriver.cpp`, `BleBeaconDriver.cpp`, `MqttDriver.cpp` 등에서 발생한 네임스페이스 불일치 및 헤더 누락 수정 완료.
    - `S3Client`, `HttpClient`의 전역 로깅 의존성(`LogManager`) 해결.

### ⚠️ 현재 중단 지점 (Current Blocker)
- **공통 심볼 링크 결여**: 드라이버를 DLL로 링크할 때 `LogManager.o`, `ConfigManager.o` 등 공통 인프라 오브젝트가 포함되지 않아 `undefined reference` 오류 발생.
    - **원인**: 개별 드라이버 소스만 컴파일하고 전체 shared 라이브러리와 링크하지 않음.
    - **해결책**: `Makefile.windows` 내 DLL 빌드 규칙에 `$(ALL_CLEAN_SHARED_OBJECTS)` 추가 필요.

---

## 2. 향후 세부 작업 (Detailed Technical Next Steps)

1. **Makefile 의존성 주입**:
    - `Makefile.windows`의 각 DLL 타겟(`modbus_driver.dll` 등)의 링킹 커맨드에 공유 오브젝트 리스트(`$(ALL_CLEAN_SHARED_OBJECTS)`)를 명시적으로 추가하여 링크 오류 해결.
2. **DLL Export 가시성 검증**:
    - `x86_64-w64-mingw32-nm -D` 명령어를 사용하여 모든 DLL이 `RegisterPlugin` 심볼을 올바르게 Export 하는지 재확인.
3. **런타임 경로 처리 보강**:
    - Windows 환경에서 `GetModuleFileName` 등을 활용하여 상대 경로(`plugins/`)가 EXE 위치 기준으로 정확히 인식되도록 보강.
4. **Linker Audit**:
    - 메인 바이너리(`collector.exe`)가 특정 드라이버에 정적으로 의존하고 있지 않은지(드라이버 코드가 포함되지 않았는지) `objdump` 등으로 최종 확인.

---

## 3. 플랫폼별 배포 전략 (Deployment Strategy)

| 플랫폼 | 배포 방식 | 상세 전략 |
| :--- | :--- | :--- |
| **Windows Native** | **Hybrid DLL Loading** | `collector.exe` 본체 + `plugins/` 폴더 내 선택적 DLL 배치. WinSW를 사용하여 Windows Service로 등록 및 장애 시 자동 재시작 설정. |
| **Linux Native** | **Shared Object (.so)** | `collector` 바이너리 + `plugins/*.so`. Systemd 유닛 파일(`collector.service`)을 통해 라이프사이클 관리. |
| **Container (Docker)** | **Multistage Image** | Multi-stage build를 통해 빌드 도구는 제외하고 바이너리와 전용 배포 스크립트만 포함. `plugins/` 디렉토리를 볼륨 마운트하여 유연하게 드라이버 교체 가능. |

---

## 4. 테스트 전략 및 E2E 재수행 필요성

### ❓ 기존 E2E 테스트를 왜 다시 해야 하는가?
기본적인 드라이버 로직(Modbus Read/Write 등)은 검증되었으나, **아키텍처가 "정적 링킹"에서 "동적 로드"로 완전히 바뀌었기 때문에** 다음 항목들을 반드시 재검증해야 합니다.

1. **심볼 해석 시점**: 빌드 타임이 아닌 **실행 시점(Runtime)**에 함수 주소를 찾으므로, 런타임 링킹 오류나 세그먼테이션 폴트 방지 확인.
2. **초기화 순서**: 프로그램 시작 후 나중에 로드되는 드라이버가 `DriverFactory`와 정상적으로 통신하는지 확인.
3. **OS 경로 호환성**: Windows의 DLL 탐색 정책이 `plugins/` 디렉토리를 올바르게 가리키는지 실환경 확인.

### 배포 기반별 테스트 상세
- **컨테이너 기반**: Docker Compose로 Modbus/MQTT 시뮬레이터를 띄워 전체 데이터 흐름(Collector -> Redis -> Export Gateway -> S3) 검증.
- **네이티브 기반**: 실제 Windows/Linux 환경에서 로그를 통해 "Plugin Loaded Successfully" 메시지 및 실제 계측기와의 통신 응답성 확인.

---

> [!CAUTION]
> **재부팅 전 주의**: 현재 `Makefile.windows`는 링크 오류가 있는 상태입니다. 재개 시 `Makefile.windows` 수정부터 시작해야 함을 잊지 마세요.
