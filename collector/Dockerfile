# collector/Dockerfile - 프로덕션용
FROM gcc:latest as builder

# 빌드에 필요한 패키지만 설치
RUN apt-get update && apt-get install -y \
    cmake \
    make \
    libpqxx-dev \
    libpq-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app/collector

# Collector 소스 복사
COPY collector/ /app/collector/
COPY config/ /app/config/

# 프로덕션 빌드
RUN make ci

# 런타임 스테이지
FROM debian:bullseye-slim

# 런타임에 필요한 라이브러리만 설치
RUN apt-get update && apt-get install -y \
    libpqxx-6.4 \
    libpq5 \
    libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 빌드된 실행파일만 복사
COPY --from=builder /app/collector/bin/collector /app/collector
COPY --from=builder /app/config /app/config

# 프로덕션에서는 collector 실행
CMD ["./collector"]