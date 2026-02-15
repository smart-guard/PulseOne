# collector/Dockerfile.dev - BACnet + Redis + HTTP 라이브러리 완전 설정
FROM gcc:12

# 기본 패키지 설치 (httplib 헤더 포함)
RUN apt-get update && apt-get install -y \
    sqlite3 \
    git \
    build-essential \
    cmake \
    libpqxx-dev libpq-dev libsqlite3-dev \
    sqlite3 sqlite3-doc \
    curl git nodejs npm pkg-config \
    wget unzip vim nano \
    libssl-dev uuid-dev \
    libbluetooth-dev \
    libmbedtls-dev \
    autotools-dev autoconf automake libtool \
    tree htop procps \
    && rm -rf /var/lib/apt/lists/*

# httplib 직접 다운로드 (패키지 매니저에 없는 경우 대비)
RUN echo "Installing httplib header..." && \
    mkdir -p /usr/local/include && \
    wget -O /usr/local/include/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.14.1/httplib.h && \
    echo "httplib header installed successfully"

# SQLite 설치 확인
RUN echo "SQLite 설치 확인:" && \
    sqlite3 --version && \
    echo "SQLite3 설치 완료"

# =============================================================================
# 필수 라이브러리들 설치 (기존 그대로)
# =============================================================================

# 1. libmodbus 설치
RUN echo "Installing libmodbus..." && \
    cd /tmp && git clone --depth 1 --branch v3.1.10 https://github.com/stephane/libmodbus.git && \
    cd libmodbus && ./autogen.sh && ./configure --prefix=/usr/local && \
    make -j$(nproc) && make install && ldconfig && \
    cd / && rm -rf /tmp/libmodbus && echo "libmodbus installed"

# 2. hiredis 설치
RUN echo "Installing hiredis..." && \
    cd /tmp && git clone --depth 1 --branch v1.2.0 https://github.com/redis/hiredis.git && \
    cd hiredis && mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_SSL=ON && \
    make -j$(nproc) && make install && ldconfig && \
    cd / && rm -rf /tmp/hiredis && echo "hiredis installed"

# 3. Paho MQTT C 설치
RUN echo "Installing Paho MQTT C..." && \
    cd /tmp && git clone --depth 1 --branch v1.3.13 https://github.com/eclipse/paho.mqtt.c.git && \
    cd paho.mqtt.c && mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_DOCUMENTATION=FALSE && \
    make -j$(nproc) && make install && ldconfig && \
    cd / && rm -rf /tmp/paho.mqtt.c && echo "Paho MQTT C installed"

# 3-1. Paho MQTT C++ 설치
RUN echo "Installing Paho MQTT C++..." && \
    cd /tmp && git clone --depth 1 --branch v1.3.2 https://github.com/eclipse/paho.mqtt.cpp.git && \
    cd paho.mqtt.cpp && mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DPAHO_WITH_SSL=TRUE \
    -DPAHO_BUILD_DOCUMENTATION=FALSE \
    -DPAHO_BUILD_SAMPLES=FALSE \
    -DPAHO_BUILD_TESTS=FALSE && \
    make -j$(nproc) && make install && ldconfig && \
    cd / && rm -rf /tmp/paho.mqtt.cpp && echo "Paho MQTT C++ installed"

# 4. nlohmann/json 설치
RUN echo "Installing nlohmann/json..." && \
    cd /tmp && git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git && \
    cd json && mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DJSON_BuildTests=OFF && \
    make -j$(nproc) && make install && \
    cd / && rm -rf /tmp/json && echo "nlohmann/json installed"

# 5. QuickJS JavaScript 엔진 설치
RUN echo "Installing QuickJS..." && \
    cd /tmp && git clone --depth 1 https://github.com/bellard/quickjs.git && \
    cd quickjs && \
    make && \
    make install PREFIX=/usr/local && \
    ldconfig && \
    cd / && rm -rf /tmp/quickjs && echo "QuickJS installed"

# 6. spdlog 설치
RUN echo "Installing spdlog..." && \
    cd /tmp && git clone --depth 1 --branch v1.12.0 https://github.com/gabime/spdlog.git && \
    cd spdlog && mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DSPDLOG_BUILD_SHARED=ON && \
    make -j$(nproc) && make install && ldconfig && \
    cd / && rm -rf /tmp/spdlog && echo "spdlog installed"

# 7. BACnet Stack 설치 (기존과 동일)
RUN echo "Installing BACnet Stack..." && \
    cd /tmp && \
    git clone --depth 1 https://github.com/bacnet-stack/bacnet-stack.git && \
    cd bacnet-stack && \
    make BACNET_PORT=linux BACDL_DEFINE=-DBACDL_BIP=1 CC="gcc -fPIC" library && \
    cp apps/lib/libbacnet.a /usr/local/lib/ && \
    cp -r src/bacnet /usr/local/include/ && \
    ldconfig && \
    cd / && rm -rf /tmp/bacnet-stack && \
    echo "BACnet Stack installed"

# 8. open62541 (OPC UA) 설치
RUN echo "Installing open62541..." && \
    cd /tmp && git clone --depth 1 --branch v1.3.9 https://github.com/open62541/open62541.git && \
    cd open62541 && mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DUA_ENABLE_AMALGAMATION=ON \
    -DUA_ENABLE_ENCRYPTION=ON && \
    make -j$(nproc) && make install && ldconfig && \
    cd / && rm -rf /tmp/open62541 && echo "open62541 installed"

# pkg-config 파일들 생성 (기존과 동일)
RUN mkdir -p /usr/local/lib/pkgconfig && \
    cat > /usr/local/lib/pkgconfig/bacnet.pc << 'EOF'
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: BACnet Stack
Description: BACnet protocol library for building automation
Version: 1.0.0
Libs: -L${libdir} -lbacnet -lm
Cflags: -I${includedir} -DPRINT_ENABLED=1 -DBACAPP_ALL -DINTRINSIC_REPORTING
EOF

RUN cat > /usr/local/lib/pkgconfig/libmodbus.pc << 'EOF'
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: libmodbus
Description: Modbus library
Version: 3.1.10
Libs: -L${libdir} -lmodbus
Cflags: -I${includedir}
EOF

RUN cat > /usr/local/lib/pkgconfig/paho-mqttpp3.pc << 'EOF'
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: Paho MQTT C++
Description: Eclipse Paho MQTT C++ Client Library
Version: 1.3.2
Requires: paho-mqtt3c
Libs: -L${libdir} -lpaho-mqttpp3
Cflags: -I${includedir}
EOF

RUN cat > /usr/local/lib/pkgconfig/quickjs.pc << 'EOF'
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: QuickJS
Description: QuickJS JavaScript Engine
Version: 2024-01-13
Libs: -L${libdir} -lquickjs
Cflags: -I${includedir}
EOF

# httplib pkg-config 파일 생성 (새로 추가)
RUN cat > /usr/local/lib/pkgconfig/httplib.pc << 'EOF'
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: cpp-httplib
Description: A C++ header-only HTTP/HTTPS server and client library
Version: 0.14.1
Libs: -lpthread
Cflags: -I${includedir}
EOF

# Redis 설정 파일들 (기존과 동일)
RUN mkdir -p /app/collector/config && \
    cat > /app/collector/config/redis_docker.env << 'EOF'
REDIS_HOST=host.docker.internal
REDIS_PORT=6379
REDIS_DB=0
REDIS_PASSWORD=
REDIS_SSL=false
REDIS_CONNECTION_TIMEOUT_MS=5000
REDIS_COMMAND_TIMEOUT_MS=3000
REDIS_MAX_CONNECTIONS=20
REDIS_MIN_CONNECTIONS=5
REDIS_RETRY_ATTEMPTS=3
REDIS_RETRY_DELAY_MS=1000
REDIS_TEST_MODE=true
REDIS_FLUSH_ON_START=false
REDIS_AUTO_PING=true
REDIS_PING_INTERVAL_S=30
REDIS_HOST_COMPOSE=redis
REDIS_HOST_EXTERNAL=host.docker.internal
EOF

# 개발 도구 스크립트들 생성 (httplib 확인 추가)
RUN echo '#!/bin/bash' > /usr/local/bin/check-deps.sh && \
    echo 'echo "Library dependency check:"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  libmodbus: $(pkg-config --exists libmodbus && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  hiredis: $(ls -la /usr/local/lib/libhiredis* 2>/dev/null && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  BACnet: $(ls -la /usr/local/lib/libbacnet.a 2>/dev/null && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  Paho MQTT C: $(ls -la /usr/local/lib/libpaho-mqtt3c.so* 2>/dev/null && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  Paho MQTT C++: $(ls -la /usr/local/lib/libpaho-mqttpp*.so* 2>/dev/null && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  QuickJS: $(pkg-config --exists quickjs && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  httplib: $(ls -la /usr/local/include/httplib.h 2>/dev/null && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    echo 'echo "  nlohmann/json: $(find /usr/local/include -name json.hpp 2>/dev/null && echo "available" || echo "unavailable")"' >> /usr/local/bin/check-deps.sh && \
    chmod +x /usr/local/bin/check-deps.sh

# httplib 테스트 스크립트 추가
RUN echo '#!/bin/bash' > /usr/local/bin/test-httplib.sh && \
    echo 'echo "httplib 테스트"' >> /usr/local/bin/test-httplib.sh && \
    echo '===============' >> /usr/local/bin/test-httplib.sh && \
    echo '' >> /usr/local/bin/test-httplib.sh && \
    echo 'echo "Header file check:"' >> /usr/local/bin/test-httplib.sh && \
    echo 'ls -la /usr/local/include/httplib.h 2>/dev/null || echo "httplib.h not found"' >> /usr/local/bin/test-httplib.sh && \
    echo '' >> /usr/local/bin/test-httplib.sh && \
    echo 'echo "Compile test:"' >> /usr/local/bin/test-httplib.sh && \
    echo 'cat > /tmp/test_httplib.cpp << "TESTEOF"' >> /usr/local/bin/test-httplib.sh && \
    echo '#include <httplib.h>' >> /usr/local/bin/test-httplib.sh && \
    echo '#include <iostream>' >> /usr/local/bin/test-httplib.sh && \
    echo 'int main() {' >> /usr/local/bin/test-httplib.sh && \
    echo '    httplib::Server server;' >> /usr/local/bin/test-httplib.sh && \
    echo '    std::cout << "httplib compilation test passed" << std::endl;' >> /usr/local/bin/test-httplib.sh && \
    echo '    return 0;' >> /usr/local/bin/test-httplib.sh && \
    echo '}' >> /usr/local/bin/test-httplib.sh && \
    echo 'TESTEOF' >> /usr/local/bin/test-httplib.sh && \
    echo 'if g++ -std=c++17 -o /tmp/test_httplib /tmp/test_httplib.cpp -lpthread 2>/dev/null; then' >> /usr/local/bin/test-httplib.sh && \
    echo '    echo "httplib compilation successful"' >> /usr/local/bin/test-httplib.sh && \
    echo '    /tmp/test_httplib && echo "httplib execution test successful"' >> /usr/local/bin/test-httplib.sh && \
    echo 'else' >> /usr/local/bin/test-httplib.sh && \
    echo '    echo "httplib compilation failed"' >> /usr/local/bin/test-httplib.sh && \
    echo 'fi' >> /usr/local/bin/test-httplib.sh && \
    echo 'rm -f /tmp/test_httplib /tmp/test_httplib.cpp 2>/dev/null' >> /usr/local/bin/test-httplib.sh && \
    chmod +x /usr/local/bin/test-httplib.sh

# 환경 변수 설정
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
ENV BACNET_PORT=linux
ENV BACDL_DEFINE="-DBACDL_BIP=1"
ENV REDIS_HOST=host.docker.internal
ENV REDIS_PORT=6379
ENV REDIS_DB=0
ENV REDIS_TEST_MODE=true

# Bash 프롬프트 설정 (httplib 정보 추가)
RUN echo 'export PS1="\[\033[01;32m\]\u@collector-dev\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$ "' >> /root/.bashrc && \
    echo 'echo "PulseOne Collector Development Environment Ready!"' >> /root/.bashrc && \
    echo 'echo "Installed libraries:"' >> /root/.bashrc && \
    echo 'echo "  - libmodbus (Modbus communication)"' >> /root/.bashrc && \
    echo 'echo "  - Paho MQTT C/C++ (MQTT communication)"' >> /root/.bashrc && \
    echo 'echo "  - BACnet Stack (BACnet communication)"' >> /root/.bashrc && \
    echo 'echo "  - hiredis (Redis client) - ${REDIS_HOST:-host.docker.internal}:${REDIS_PORT:-6379}"' >> /root/.bashrc && \
    echo 'echo "  - httplib (HTTP server/client)"' >> /root/.bashrc && \
    echo 'echo "  - nlohmann/json (JSON parsing)"' >> /root/.bashrc && \
    echo 'echo "  - QuickJS (JavaScript engine)"' >> /root/.bashrc && \
    echo 'echo "  - spdlog (logging)"' >> /root/.bashrc && \
    echo 'echo ""' >> /root/.bashrc && \
    echo 'echo "Useful commands:"' >> /root/.bashrc && \
    echo 'echo "  check-deps.sh          - Check all library dependencies"' >> /root/.bashrc && \
    echo 'echo "  test-httplib.sh        - Test httplib compilation"' >> /root/.bashrc && \
    echo 'echo "  make help              - Makefile help"' >> /root/.bashrc && \
    echo 'echo ""' >> /root/.bashrc

# 환경 변수 설정
ENV CC=gcc CXX=g++
ENV CFLAGS="-std=c11 -Wall -Wextra"
ENV CXXFLAGS="-std=c++17 -Wall -Wextra"

# 소스 코드 복사 (core 폴더 전체를 복사하여 상대 경로 유지)
WORKDIR /app
COPY core /app/core

# 빌드 수행
RUN cd /app/core/shared && make all -j$(nproc)
RUN cd /app/core/collector && make all -j$(nproc)

WORKDIR /app/core/collector
CMD ["/app/core/collector/bin/pulseone-collector"]