FROM node:22

# 시스템 종속 패키지 설치
RUN apt-get update && apt-get install -y \
	g++ make cmake libpq-dev libsqlite3-dev libpqxx-dev logrotate

WORKDIR /app
COPY . .

# 백엔드 의존성 설치
WORKDIR /app/backend
RUN rm -f package-lock.json && npm install || true


# Collector 빌드
WORKDIR /app/collector
RUN make ci || true
