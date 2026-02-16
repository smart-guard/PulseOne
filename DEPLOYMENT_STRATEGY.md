# PulseOne Unified Deployment Strategy

PulseOne의 배포는 **"단일 코드베이스(Single Source)"**와 **"완전 도커화된 빌드(Docker-Only Build)"** 원칙을 엄격히 준수한다. OS별로 코드가 파편화되는 것을 방지하고, 어떤 환경에서도 동일 기능이 보장되도록 한다.

## 1. 3대 철칙 (Iron Rules)

### 🚨 Rule 1: 무조건 도커에서 빌드 (Docker-Only)
- 로컬 맥(Mac) 터미널에서의 직접적인 `make`나 `gcc` 컴파일은 원천 금지한다.
- 모든 운영용 바이너리는 플랫폼별 전용 도커 빌드 이미지(Linux, Windows-Cross) 내에서 생성한다.
- 맥은 오직 `docker` 명령어와 스크립트 실행기 역할만 수행한다.

### 🚨 Rule 2: 코드 파편화 절대 금지 (Single Codebase)
- 윈도우용, 리눅스용 코드를 따로 파일로 분리하거나 복제하지 않는다.
- 플랫폼 간의 차이가 필요한 경우, 소스 내에서 전처리기(`C++: #ifdef`, `Node.js: process.platform`)를 사용하여 조건부로 처리한다.
- 배포 준비 중 발생한 문제를 해결하기 위해 배포 시점에만 유효한 임시 코드를 삽입하는 행위는 금지한다.

### 🚨 Rule 3: 빌드 시점 코드 변조 금지 (No Ad-hoc Fixes)
- 빌드가 실패할 경우 배포 스크립트나 빌드 환경 내에서 코드를 직접 수정하지 않는다.
- 모든 빌드 오류는 개발 소스 코드에 정식으로 반영되어야 하며, `git` 이력이 남지 않는 수정은 인정하지 않는다.

---

## 2. 플랫폼별 컴파일 전략

### A. Windows 배포
- **도구**: Docker 기반 `MinGW-w64` 크로스 컴파일러 사용.
- **방식**: 소스 루트의 `Makefile.windows`를 도커 내부에서 호출.
- **심볼**: `PULSEONE_WINDOWS` 매크로 정의.

### B. Linux / Container 배포
- **도구**: Docker 전용 Multi-stage Build 활용.
- **방식**: `Dockerfile.prod` 또는 플랫폼 기반 빌드 이미지 사용.
- **심볼**: `PULSEONE_LINUX` 매크로 정의.

---

## 3. 조건부 컴파일 가이드 (C++)

OS별 로직이 필요한 경우 반드시 아래 표준 패턴을 사용한다.

```cpp
#ifdef _WIN32
    // Windows 전용 핸들러 (예: WinAPI, WinSW 연동)
#else
    // Linux/Container 전용 핸들러 (예: Systemd, Docker Signal)
#endif
```

## 4. 플랫폼별 빌드 실행 가이드 (How-to Build)

맥 터미널에서는 오직 아래의 도커 명령어만 수행하며, 소스 코드는 볼륨 마운트를 통해 컨테이너와 공유한다.

### A. Windows 바이너리 (Cross-Compile)
```bash
# 빌드용 이미지 생성 (이미 있는 경우 생략)
docker build -t pulseone-win-builder -f core/collector/Dockerfile.windows .

# 빌드 실행 (collector.exe, export-gateway.exe 생성)
docker run --rm -v $(pwd):/src pulseone-win-builder /bin/bash -c "
  cd /src/core/shared && make windows-cross && \
  cd /src/core/collector && make windows-cross && \
  cd /src/core/export-gateway && make windows-cross
"
```

### B. Linux 바이너리 (Prod Build)
```bash
# 리눅스 전용 빌드 및 패키징
docker-compose -f docker-compose.prod.yml build --no-cache
```

## 5. 배포 검증 체크리스트

- [ ] `core/shared/lib`에 `.a` 정적 라이브러리들이 생성되었는가?
- [ ] `bin/` 또는 `bin-windows/` 폴더에 확장자별 바이너리가 정확히 추출되었는가?
- [ ] 임시 코드가 아닌 `#ifdef` 기반의 조건부 로직이 적용되었는가?
- [ ] 맥 터미널에서 바이너리를 직접 실행하여 아키텍처 오류가 발생하지 않는가? (Docker 위에서만 실행 확인)
