


---

## 빌드 및 실행 방법

### 1. Docker 기반 통합 실행

cd docker
docker-compose up --build


### 2. 서비스별 수동 빌드/실행

- **backend**  
cd backend
npm install
npm start

- **collector**  
cd collector
make
./collector

- **driver/ModbusDriver**  
cd driver/ModbusDriver
make


각 폴더의 `README.md` 참고

---

## 협업 및 운영 규칙

- **브랜치 전략**: main(운영), develop(개발), feature/이름
- **커밋 메시지**: 기능 단위, 명확하게 작성
- **코드리뷰**: Pull Request 기반 리뷰 필수
- **환경 변수/비밀키**: `config/.env`는 Git에 포함하지 않고 별도 관리
- **신규 드라이버 추가**: `/driver` 하위에 폴더 생성, 빌드 후 plugins 폴더에 DLL/SO 배치

---

## 참고 문서 및 온보딩

- 각 서비스별 폴더의 `README.md`에서 상세 사용법, 의존성, 주의사항 확인
- PulseOne 프로젝트 요구사항 명세서(첨부)
- `.github/workflows/ci-cd.yml` (CI/CD 자동화 파이프라인)
- 신규 인력 온보딩/운영 관련 문의는 담당자 또는 이슈 게시판 활용

---

## 문의 및 기여

- 기능 추가, 드라이버 개발, 버그 리포트 등은 GitHub Issues 및 Pull Request로 요청
- 온보딩/운영 관련 문의는 담당자 또는 이슈 게시판 활용

