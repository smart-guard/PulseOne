# 1단계: 빌드 스테이지
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y g++ make

WORKDIR /app
COPY . .

RUN make build

# 2단계: 실행 스테이지
FROM ubuntu:22.04

WORKDIR /app

COPY --from=builder /app/bin/collector ./collector

CMD ["./collector"]
