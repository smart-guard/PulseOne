# Stage 1: 빌드
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y g++ make

WORKDIR /app
COPY . .

RUN make build

# Stage 2: 실행
FROM ubuntu:22.04

WORKDIR /app
RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/bin/collector ./collector

CMD ["./collector"]
