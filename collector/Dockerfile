FROM ubuntu:22.04

# 필수 패키지 설치
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    cmake \
    pkg-config \
    libpq-dev \
    libssl-dev \
    uuid-dev \
    && rm -rf /var/lib/apt/lists/*

# 작업 디렉토리 설정
WORKDIR /app/collector

# 소스 코드 복사
COPY . .

# 빌드
RUN make clean && make

# 실행 명령
CMD ["./pulseone_collector"]