# Export Gateway Windows Build Fix Report

본 문서는 Export Gateway의 Windows 크로스 컴파일 빌드 에러를 해결하기 위해 진행된 근본적인 구조 개선 작업과 향후 남은 과제를 정리합니다.

## 1. 완료된 작업 (Work Completed)

### 📂 헤더 이름 충돌 해결 (Header Collision Resolution)
- **문제**: `shared`와 `export-gateway` 양쪽 모두에 `ExportTypes.h`가 존재하여 윈도우 빌드 시 엉뚱한 헤더를 참조하는 문제 발생.
- **해결**:
    - 로컬 헤더를 `GatewayExportTypes.h`로 개명.
    - 소스 파일 `GatewayExportTypes.cpp`로 개명 및 `Makefile` 빌드 대상 업데이트.
    - 모든 소스 코드 내 인클루드 경로를 명시적 경로로 치환하여 모호성 제거.

### 📦 의존성 구조 정규화 (Dependency Normalization)
- **문제**: `nlohmann/json.hpp`가 `collector` 폴더 안에 숨어 있어 `shared` 라이브러리 컴파일 시 참조가 꼬이는 문제.
- **해결**:
    - JSON 라이브러리를 `core/shared/include`로 이동하여 공통 의존성으로 승격.
    - `Makefile`에서 불필요하게 `collector` 인클루드 경로를 참조하던 로직 제거.

### 🛠️ MinGW/Windows 빌드 호환성 패치
- **문제**: MinGW 컴파일러에서 `_errno` 선언 누락 및 헤더 간섭 발생.
- **해결**:
    - `shared/include/nlohmann/json.hpp` 최상단에 `cerrno` 강제 인클루드 패치.
    - `Common/ProtocolConfigs.h` 등 주요 헤더에 호환성 매크로 및 헤더 보정.
    - `Makefile` 컴파일러 설정을 환경 변수로 덮어씌울 수 있도록 개선 (`CC ?=`, `CXX ?=`).

## 2. 현재 상태 (Current Status)

`GatewayExportTypes.h`와 관련된 정적 타입 및 네임스페이스 에러는 **모두 해결**되었습니다. 현재는 플랫폼 간 표준 함수 차이로 인한 하위 에러를 해결하는 단계입니다.

### ❌ 남은 빌드 에러 (Remaining Errors)
- **Time Functions**: 윈도우/MinGW에서 `gmtime_r`이 미지원됨 (`gmtime_s` 또는 래퍼 필요).
- **Std Functions**: `AlarmTypes.h` 등에서 `std::strftime` 참조 방식 문제 (C++17 표준 준수 보정 필요).

## 3. 향후 계획 (Next Steps)

1. **플랫폼 호환 래퍼 적용**: `gmtime_r` 등을 윈도우에서도 동작하는 매크로 또는 래퍼 함수로 교체.
2. **최종 빌드 검증**: Docker(`pulseone-windows-builder`) 환경에서 `make CROSS_COMPILE_WINDOWS=1` 성공 확인.
3. **배포 패키지 생성**: 프로젝트 루트의 `deploy-windows.sh`를 실행하여 최종 바이너리 및 패키지 추출.
4. **회귀 테스트**: 리눅스 빌드 및 서비스 동작에 영향이 없는지 최종 확인.

---
*Last Updated: 2026-02-12*
