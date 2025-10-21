# 🧪 Export Gateway 통합 테스트 가이드

## 빠른 시작

```bash
# 1. Redis 시작
sudo service redis-server start

# 2. 빌드
make clean && make all && make tests

# 3. 테스트 실행
./scripts/run_integration_tests.sh
```

## 테스트 시나리오

| # | 테스트 | 검증 내용 |
|---|--------|----------|
| 1 | DB 타겟 로드 | export_targets 테이블에서 설정 읽기 |
| 2 | Redis 쓰기 | 테스트 데이터 저장 |
| 3 | Redis 읽기 | 저장된 데이터 읽기 |
| 4 | 포맷 변환 | Redis → 타겟 형식 변환 |
| 5 | HTTP 전송 | Mock 서버에 전송 |
| 6 | 데이터 검증 | 원본 vs 전송 데이터 일치 확인 |

## 문제 해결

### Redis 연결 실패
```bash
sudo service redis-server start
redis-cli ping
```

### 컴파일 에러
```bash
cd /app/core/shared
make clean && make all

cd /app/core/export-gateway
make clean && make all && make tests
```

## Make 타겟

- `make tests` - 테스트 빌드
- `make test` - 모든 테스트 실행
- `make test-integration` - 통합 테스트만 실행
- `make clean-tests` - 테스트 파일 정리
