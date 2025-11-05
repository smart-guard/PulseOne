# Export Gateway Unit Tests

Phase 1 핵심 단위 테스트

## 📁 구조

```
tests/unit/
├── test_circuit_breaker.cpp    # Circuit Breaker 10개 테스트
├── test_http_handler.cpp        # HTTP Handler 9개 테스트
├── test_dynamic_target.cpp      # DynamicTargetManager 10개 테스트
├── Makefile                     # 빌드 및 실행
└── README.md                    # 이 파일
```

## 🚀 빠른 시작

### 1. 빌드

```bash
cd core/export-gateway/tests/unit
make
```

### 2. 전체 테스트 실행

```bash
make test
```

### 3. 개별 테스트 실행

```bash
# Circuit Breaker만
make test-circuit

# HTTP Handler만
make test-http

# DynamicTargetManager만
make test-dynamic
```

## 📊 테스트 항목

### Circuit Breaker (10개 테스트)

1. ✅ 초기 상태 검증 (CLOSED)
2. ✅ CLOSED → OPEN 전환
3. ✅ OPEN → HALF_OPEN 전환
4. ✅ HALF_OPEN → CLOSED 복구
5. ✅ HALF_OPEN → OPEN 재실패
6. ✅ 수동 리셋
7. ✅ 통계 정확성
8. ✅ Exponential Backoff
9. ✅ 강제 OPEN
10. ✅ HALF_OPEN 최대 시도 횟수

### HTTP Handler (9개 테스트)

1. ✅ 초기화
2. ✅ 정상 POST 요청
3. ✅ 타임아웃 처리
4. ✅ 연결 거부
5. ✅ HTTP 400 에러
6. ✅ HTTP 500 에러
7. ✅ 잘못된 URL
8. ✅ 커스텀 헤더
9. ✅ 리소스 정리

### DynamicTargetManager (10개 테스트)

1. ✅ 싱글턴 인스턴스
2. ✅ DB에서 타겟 로드
3. ✅ 전체 타겟 조회
4. ✅ 동적 타겟 추가
5. ✅ 타겟 제거
6. ✅ 타겟 활성화/비활성화
7. ✅ 알람 전송
8. ✅ 통계 조회
9. ✅ Failure Protector 통계
10. ✅ 헬스체크

## 🔧 문제 해결

### 빌드 에러

```bash
# include 경로 확인
ls ../../include/CSP/
ls ../../../shared/include/

# 라이브러리 확인
ls ../../../shared/lib/
```

### 실행 에러

```bash
# 라이브러리 경로 설정
export LD_LIBRARY_PATH=../../../shared/lib:$LD_LIBRARY_PATH

# 데이터베이스 권한 확인
ls -la /tmp/test_*.db
```

## 📈 다음 단계

Phase 1 완료 후:
- Phase 2: S3, File, MQTT Handler 테스트
- Phase 3: 통합 테스트
- Phase 4: 성능 테스트

## 🎯 목표

- ✅ 모든 테스트 통과
- ✅ 컴파일 에러 0개
- ✅ 메모리 누수 0개
- ✅ 실행 시간 < 1분