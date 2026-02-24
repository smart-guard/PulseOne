# 🪟 PulseOne Windows 배포 진행 상황 (2026-02-24)

## 📌 1. 현재 시스템 상태 (Current Status)
Windows 배포 환경(Docker MinGW)에서 발생한 **모든 주요 버그가 수정**되었으며, 크로스 컴파일 및 패키징 스크립트가 성공적으로 구동되어 최종 설치 패키지(`PulseOne_Windows-v6.1.0_20260224_084310.zip`) 생성이 완료되었습니다.

## ✅ 2. 지금까지 해결된 주요 문제 (Completed Tasks)
1. **Windows 파일 경로 파싱 및 SQLite 에러 해결**: 
   - `\` 역슬래시와 `/` 슬래시가 혼용되어 발생하던 "unable to open database file" 에러를 방지하기 위해 C++ `ConfigPathResolver.cpp` 내 경로 정규화(`Path::Normalize`) 로직을 도입했습니다.
   - 한글 경로가 깨지는 현상을 방지하기 위해 UTF-16 기반의 Windows 전용 파일 시스템 API(`GetModuleFileNameW` 등)로 대체했습니다.
2. **MinGW 컴파일러 필수 통신 DLL 누락 (Entry Point Error)**: 
   - `libgcc_s_seh-1.dll`, `libstdc++-6.dll` 등의 부재로 C++ 바이너리가 실행되지 않던 문제를 정적 링킹(`-static-libstdc++ -static-libgcc`) 적용 및 배포 스크립트 내 자동 복사를 통해 픽스했습니다.
3. **WinSW 서비스 및 백엔드 시작 오류 (CWD 문제)**:
   - Node 백엔드가 `.env` 파일을 로드하지 못하는 문제를 찾기 위해 실행 위치(`cwd`)가 고정되는 버그를 고쳤으며, `--config` 인자를 명시적으로 제공하도록 `start.bat`을 구조화했습니다.
   - 모스퀴토 등 서비스가 콘솔 창에 계속 남아있던 문제를 `runHidden.vbs` 호출로 숨겼습니다.
4. **Collector 플러그인 동적 로딩 에러 (`WinError: 126`)**:
   - 과거 파일명(`collector.exe`)의 IMPLIB 구조체를 참조하던 오래된 Makefile을 현재 버전인 `libpulseone-collector.a`로 동기화했습니다.
   - 윈도우 스레드 충돌 링킹 에러(`pthread_mutex_lock`)를 `-Wl,--allow-multiple-definition` 플래그로 억제해 7개 Driver DLL들을 모두 정상 빌드했습니다.

## 🚀 3. 앞으로 해야 할 일 (Next Steps)
이제 수정된 최종 `.zip` 패키지를 **실제 윈도우 머신 혹은 VM으로 가져가서 최종 인테그레이션(E2E) 테스트**를 진행해야 합니다.

* [ ] **Windows 환경에서 설치**: `install.bat`을 관리자 권한으로 실행하여 WinSW 서비스 등록 및 의존성 정상 세팅 유무 점검.
* [ ] **대시보드 접속 테스트**: 브라우저에서 `http://localhost:3000` 접속 후 대시보드가 로딩되고 Tenant/Site 관리가 동작하는지 확인.
* [ ] **로그 건전성 체크**: `data/logs/` 경로 아래 생성되는 `collector.log`, `export-gateway.log`, `backend-start.log` 등을 점검하여 Redis/DB 커넥션이나 Plugin 로딩 간 런타임 에러가 없는지 모니터링.
* [ ] **Edge Pipeline 연동**: BACnet, Modbus 등 실제 현장 장비(또는 시뮬레이터)를 Collector에 연결하여 텔레메트리 값을 정상적으로 Gateway를 통해 발송하는지 확인.
