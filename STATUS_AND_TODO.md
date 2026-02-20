# PulseOne Release & Stability Project Status

## 🚨 Current Issues (현황 및 문제점)

1.  **Physical Memory Exhaustion (물리 메모리 고갈)**
    *   Mac 호스트의 가용 메모리가 100MB 미만으로 떨어져, 도커를 통한 빌드 속도가 극도로 저하됨.
    *   특히 `nlohmann/json` 같은 무거운 템플릿 라이브러리를 포함한 `ConfigManager.cpp` 컴파일 시 `cc1plus` 프로세스가 멈춘 것처럼 보일 정도로 지연됨.

2.  **Build Process Redundancy (빌드 프로세스 중복)**
    *   사용자 터미널과 에이전트 백그라운드에서 동일한 `release.sh`가 중복 실행되어 자원 경합 발생 (정리 완료).
    *   `deploy-docker.sh`의 버전 하드코딩 문제 해결 (`version.json` 동적 연동).

3.  **Windows Cross-Compilation Bottleneck (윈도우 크로스 컴파일 병목)**
    *   메모리 부족 상황에서 `-O2` 최적화 및 병렬 빌드(`-j2`)가 OOM(Out-Of-Memory)을 유발함.

## ✅ Completed Tasks (최근 조치 완료 사항)

-   **Build Optimization (빌드 최적화)**:
    *   `core/shared`, `core/collector`, `core/export-gateway`의 모든 Makefile에서 최적화 레벨을 `-O0`으로 하향 조정 완료.
    *   GCC 메모리 파라미터(`--param ggc-min-expand=0`)를 적용하여 가비지 컬렉션을 공격적으로 수행하도록 설정 완료.
-   **Script Fixes (스크립트 개선)**:
    *   `deploy-linux.sh` 및 `deploy-docker.sh`에서 불필요한 프론트엔드 재빌드 건너뛰기 로직 강화.
    *   `deploy-docker.sh` 버전 동적 연동 완료.
-   **System Status Accuracy (시스템 상태 정확도 개선)**:
    -   Docker API 연동을 통해 프로세스 상태를 "꼼수"가 아닌 실제 컨테이너 상태(`running`, `stopped`)로 정확히 표시하도록 수정 완료.
    -   Docker 소켓 권한 자동 설정 로직을 `docker-compose.yml`에 통합 완료.

## 🚀 Future Roadmap (향후 작업 계획)

### 1. Unified Release Execution (통합 릴리스 완결)
- [x] `./release.sh --all` 명령을 통한 Windows/Linux/Docker 전체 패키지 생성 가능 확인.

### 2. Artifact Verification (산출물 검증)
- [ ] 생성된 바이너리 무결성 최종 체크.

---
**Note**: 이제 모든 시스템 모니터링 로직과 빌드 설정이 안정화되었습니다. 실제 서비스 운영 시 시스템 상태 페이지(`/system/status`)를 통해 정확한 프로세스 현황을 파악할 수 있습니다.
