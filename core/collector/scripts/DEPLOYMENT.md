# PulseOne Collector - Native Service Deployment Guide

이 문서는 Docker를 사용하지 않고 Windows 및 Linux(베어메탈/라즈베리 파이) 환경에 Collector를 서비스(데몬)로 등록하는 방법을 설명합니다.

## 1. 개요
Docker는 배포의 편의성을 제공하지만, 저사양 기기(Raspberry Pi 3/Zero 등)에서는 메모리 및 디스크 I/O 오버헤드가 발생할 수 있습니다. 네이티브 서비스 등록을 통해 시스템 자원을 최적화할 수 있습니다.

## 2. Linux (Raspberry Pi / Ubuntu / Debian)
`systemd`를 사용하여 서비스를 등록합니다.

### 설치 방법
```bash
cd core/collector/scripts
sudo ./install-service.sh
```

### 성능 최적화 (라즈베리 파이)
- **Swap 절감**: Docker 오버레이 파일시스템을 사용하지 않아 디스크 I/O가 감소합니다.
- **CPU 우선순위**: `pulseone-collector.service` 파일 내 `CPUWeight` 설정을 통해 다른 프로세스보다 높은 우선순위를 줄 수 있습니다.
- **메모리 제한**: `MemoryLimit` 설정을 통해 시스템 전체가 멈추는 현상을 방지합니다.

---

## 3. Windows
PowerShell을 사용하여 Windows Service로 등록합니다.

### 설치 방법
1. PowerShell을 **관리자 권한**으로 실행합니다.
2. 다음 명령을 실행합니다:
```powershell
cd core/collector/scripts
.\register-windows-service.ps1
```

### 직접 설치의 이점
- **IT 인프라 통합**: Docker 없이 표준 Windows 서비스 관리자(`services.msc`)에서 관리 가능합니다.
- **네이티브 성능**: 가상화 레이어 없이 하드웨어 자원을 직접 활용합니다.
- **자동 복구**: 서비스 실패 시 Windows가 자동으로 재시작하도록 설정되어 있습니다.

---

## 4. 공통 사항
- 서비스를 등록하기 전에 반드시 해당 플랫폼에 맞게 Collector를 빌드해야 합니다 (`make` 또는 `make windows-cross`).
- 설정 파일은 `/opt/pulseone/collector/config` (Linux) 또는 실행 파일 경로의 `config` 폴더를 참조합니다.
