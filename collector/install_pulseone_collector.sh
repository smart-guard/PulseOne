#!/bin/bash
# =============================================================================
# collector/install_pulseone_collector.sh
# PulseOne Collector 통합 설치 스크립트
# 의존성 설치 + 시스템 설정 + 환경 구성을 한 번에 처리
# =============================================================================

set -e  # 에러 발생 시 스크립트 중단

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 로그 함수들
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${PURPLE}[STEP]${NC} $1"
}

log_system() {
    echo -e "${CYAN}[SYSTEM]${NC} $1"
}

# 전역 변수
INSTALL_TYPE="full"  # full, deps-only, system-only
WITH_TESTS=false
SKIP_ROOT_CHECK=false
PULSEONE_USER="pulseone"
PULSEONE_HOME="/opt/pulseone"
INSTALL_PREFIX="/usr/local"

# 도움말 출력
show_help() {
    echo "PulseOne Collector 통합 설치 스크립트"
    echo ""
    echo "사용법: $0 [옵션]"
    echo ""
    echo "옵션:"
    echo "  --deps-only        의존성 라이브러리만 설치"
    echo "  --system-only      시스템 설정만 수행 (의존성 건너뛰기)"
    echo "  --with-tests       테스트 라이브러리 포함 설치"
    echo "  --skip-root-check  root 권한 확인 건너뛰기"
    echo "  --user USER        PulseOne 실행 사용자 (기본값: pulseone)"
    echo "  --prefix PATH      설치 경로 prefix (기본값: /usr/local)"
    echo "  -h, --help         이 도움말 출력"
    echo ""
    echo "예시:"
    echo "  $0                          # 전체 설치"
    echo "  $0 --deps-only              # 의존성만 설치"
    echo "  $0 --with-tests             # 테스트 포함 전체 설치"
    echo "  $0 --system-only --user app # app 사용자로 시스템 설정만"
}

# 명령행 인자 파싱
parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --deps-only)
                INSTALL_TYPE="deps-only"
                shift
                ;;
            --system-only)
                INSTALL_TYPE="system-only"
                shift
                ;;
            --with-tests)
                WITH_TESTS=true
                shift
                ;;
            --skip-root-check)
                SKIP_ROOT_CHECK=true
                shift
                ;;
            --user)
                PULSEONE_USER="$2"
                shift 2
                ;;
            --prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                log_error "알 수 없는 옵션: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# OS 검출
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [ -f /etc/debian_version ]; then
            OS="debian"
            DISTRO=$(lsb_release -si 2>/dev/null || echo "Debian")
        elif [ -f /etc/redhat-release ]; then
            OS="redhat"
            DISTRO=$(cat /etc/redhat-release | cut -d' ' -f1)
        elif [ -f /etc/alpine-release ]; then
            OS="alpine"
            DISTRO="Alpine"
        else
            OS="linux"
            DISTRO="Unknown Linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
        DISTRO="macOS"
    else
        OS="unknown"
        DISTRO="Unknown"
    fi
    
    log_info "Detected OS: $DISTRO ($OS)"
}

# 권한 확인
check_permissions() {
    if [ "$SKIP_ROOT_CHECK" = true ]; then
        log_warning "Root 권한 확인을 건너뜁니다"
        return 0
    fi
    
    if [ "$INSTALL_TYPE" = "deps-only" ]; then
        return 0  # 의존성만 설치할 때는 root 권한 불필요할 수 있음
    fi
    
    if [ "$EUID" -ne 0 ]; then
        log_error "이 스크립트는 root 권한으로 실행되어야 합니다."
        log_info "다음 명령어로 실행하세요: sudo $0"
        log_info "또는 --skip-root-check 옵션을 사용하세요"
        exit 1
    fi
    
    log_success "Root 권한 확인 완료"
}

# 시스템 패키지 설치
install_system_packages() {
    log_step "시스템 패키지 설치 중..."
    
    case $OS in
        "debian")
            export DEBIAN_FRONTEND=noninteractive
            apt-get update
            apt-get install -y \
                build-essential \
                cmake \
                pkg-config \
                git \
                wget \
                curl \
                libtool \
                autoconf \
                automake \
                uuid-dev \
                libssl-dev \
                libpthread-stubs0-dev \
                logrotate \
                sqlite3 \
                libsqlite3-dev \
                postgresql-client \
                redis-tools
            ;;
        "redhat")
            yum groupinstall -y "Development Tools"
            yum install -y \
                cmake \
                pkgconfig \
                git \
                wget \
                curl \
                libtool \
                autoconf \
                automake \
                libuuid-devel \
                openssl-devel \
                logrotate \
                sqlite \
                sqlite-devel \
                postgresql \
                redis
            ;;
        "alpine")
            apk update
            apk add --no-cache \
                build-base \
                cmake \
                pkgconfig \
                git \
                wget \
                curl \
                libtool \
                autoconf \
                automake \
                util-linux-dev \
                openssl-dev \
                logrotate \
                sqlite \
                sqlite-dev \
                postgresql-client \
                redis
            ;;
        "macos")
            # Homebrew 설치 확인
            if ! command -v brew &> /dev/null; then
                log_error "Homebrew가 설치되어 있지 않습니다."
                log_info "다음 명령어로 Homebrew를 먼저 설치하세요:"
                echo '  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
                exit 1
            fi
            
            brew install \
                cmake \
                pkg-config \
                git \
                wget \
                curl \
                libtool \
                autoconf \
                automake \
                ossp-uuid \
                openssl \
                sqlite \
                postgresql \
                redis
            ;;
        *)
            log_error "지원하지 않는 OS: $OSTYPE"
            exit 1
            ;;
    esac
    
    log_success "시스템 패키지 설치 완료"
}

# libmodbus 설치
install_libmodbus() {
    log_step "libmodbus 설치 중..."
    
    # 이미 설치되어 있는지 확인
    if pkg-config --exists libmodbus; then
        log_success "libmodbus가 이미 설치되어 있습니다 ($(pkg-config --modversion libmodbus))"
        return 0
    fi
    
    case $OS in
        "debian")
            apt-get install -y libmodbus-dev
            ;;
        "redhat")
            yum install -y libmodbus-devel
            ;;
        "alpine")
            apk add --no-cache libmodbus-dev
            ;;
        "macos")
            brew install libmodbus
            ;;
        *)
            # 소스에서 빌드
            log_info "소스에서 libmodbus 빌드 중..."
            cd /tmp
            rm -rf libmodbus
            git clone https://github.com/stephane/libmodbus.git
            cd libmodbus
            ./autogen.sh
            ./configure --prefix="$INSTALL_PREFIX"
            make -j$(nproc 2>/dev/null || echo 4)
            make install
            ldconfig 2>/dev/null || true
            cd - > /dev/null
            rm -rf /tmp/libmodbus
            ;;
    esac
    
    log_success "libmodbus 설치 완료"
}

# Eclipse Paho MQTT 설치
install_paho_mqtt() {
    log_step "Eclipse Paho MQTT C/C++ 설치 중..."
    
    # Paho MQTT C 설치
    if [ -f "$INSTALL_PREFIX/lib/libpaho-mqtt3c.so" ] || [ -f "$INSTALL_PREFIX/lib/libpaho-mqtt3c.dylib" ]; then
        log_success "Paho MQTT C가 이미 설치되어 있습니다"
    else
        log_info "소스에서 Paho MQTT C 빌드 중..."
        cd /tmp
        rm -rf paho.mqtt.c
        git clone https://github.com/eclipse/paho.mqtt.c.git
        cd paho.mqtt.c
        mkdir -p build && cd build
        cmake .. \
            -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
            -DPAHO_WITH_SSL=ON \
            -DPAHO_ENABLE_TESTING=OFF \
            -DPAHO_BUILD_STATIC=ON \
            -DPAHO_BUILD_SHARED=ON
        make -j$(nproc 2>/dev/null || echo 4)
        make install
        cd - > /dev/null
        rm -rf /tmp/paho.mqtt.c
    fi
    
    # Paho MQTT C++ 설치
    if [ -f "$INSTALL_PREFIX/lib/libpaho-mqttpp3.so" ] || [ -f "$INSTALL_PREFIX/lib/libpaho-mqttpp3.dylib" ]; then
        log_success "Paho MQTT C++가 이미 설치되어 있습니다"
    else
        log_info "소스에서 Paho MQTT C++ 빌드 중..."
        cd /tmp
        rm -rf paho.mqtt.cpp
        git clone https://github.com/eclipse/paho.mqtt.cpp.git
        cd paho.mqtt.cpp
        mkdir -p build && cd build
        cmake .. \
            -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
            -DPAHO_BUILD_SAMPLES=OFF \
            -DPAHO_BUILD_TESTS=OFF \
            -DPAHO_BUILD_STATIC=ON \
            -DPAHO_BUILD_SHARED=ON
        make -j$(nproc 2>/dev/null || echo 4)
        make install
        cd - > /dev/null
        rm -rf /tmp/paho.mqtt.cpp
    fi
    
    # 라이브러리 캐시 업데이트
    ldconfig 2>/dev/null || true
    
    log_success "Paho MQTT C/C++ 설치 완료"
}

# BACnet Stack 설치
install_bacnet_stack() {
    log_step "BACnet Stack 설치 중..."
    
    if [ -d "$INSTALL_PREFIX/include/bacnet" ]; then
        log_success "BACnet Stack이 이미 설치되어 있습니다"
        return 0
    fi
    
    log_info "소스에서 BACnet Stack 빌드 중..."
    cd /tmp
    rm -rf bacnet-stack
    git clone https://github.com/bacnet-stack/bacnet-stack.git
    cd bacnet-stack
    
    # Make 빌드
    make clean
    make all -j$(nproc 2>/dev/null || echo 4)
    
    # 헤더 파일 설치
    mkdir -p "$INSTALL_PREFIX/include/bacnet"
    cp -r include/* "$INSTALL_PREFIX/include/bacnet/" 2>/dev/null || true
    
    # 소스 헤더들도 복사 (필요한 것들)
    find src -name "*.h" -exec cp {} "$INSTALL_PREFIX/include/bacnet/" \; 2>/dev/null || true
    
    # 라이브러리 파일 설치 (있다면)
    if [ -f lib/libbacnet.a ]; then
        cp lib/libbacnet.a "$INSTALL_PREFIX/lib/"
    fi
    
    cd - > /dev/null
    rm -rf /tmp/bacnet-stack
    
    log_success "BACnet Stack 설치 완료"
}

# GoogleTest 설치 (테스트용)
install_googletest() {
    log_step "GoogleTest 설치 중..."
    
    case $OS in
        "debian")
            apt-get install -y libgtest-dev libgmock-dev
            ;;
        "redhat")
            yum install -y gtest-devel gmock-devel
            ;;
        "alpine")
            apk add --no-cache gtest-dev gmock
            ;;
        "macos")
            brew install googletest
            ;;
        *)
            # 소스에서 빌드
            log_info "소스에서 GoogleTest 빌드 중..."
            cd /tmp
            rm -rf googletest
            git clone https://github.com/google/googletest.git
            cd googletest
            mkdir -p build && cd build
            cmake .. \
                -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
                -DBUILD_GMOCK=ON \
                -DGTEST_FORCE_SHARED_CRT=ON
            make -j$(nproc 2>/dev/null || echo 4)
            make install
            cd - > /dev/null
            rm -rf /tmp/googletest
            ;;
    esac
    
    log_success "GoogleTest 설치 완료"
}

# pkg-config 파일 생성
create_pkg_config_files() {
    log_step "pkg-config 파일 생성 중..."
    
    mkdir -p "$INSTALL_PREFIX/lib/pkgconfig"
    
    # libmodbus pkg-config 파일
    if ! pkg-config --exists libmodbus; then
        log_info "libmodbus.pc 생성 중..."
        cat > "$INSTALL_PREFIX/lib/pkgconfig/libmodbus.pc" << EOF
prefix=$INSTALL_PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: libmodbus
Description: Modbus library
Version: 3.1.10
Libs: -L\${libdir} -lmodbus
Cflags: -I\${includedir}
EOF
    fi
    
    # paho-mqtt3c pkg-config 파일
    if [ ! -f "$INSTALL_PREFIX/lib/pkgconfig/paho-mqtt3c.pc" ]; then
        log_info "paho-mqtt3c.pc 생성 중..."
        cat > "$INSTALL_PREFIX/lib/pkgconfig/paho-mqtt3c.pc" << EOF
prefix=$INSTALL_PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: paho-mqtt3c
Description: Eclipse Paho MQTT C Client Library
Version: 1.3.13
Libs: -L\${libdir} -lpaho-mqtt3c -lpaho-mqtt3cs
Cflags: -I\${includedir}
EOF
    fi
    
    # paho-mqttpp3 pkg-config 파일
    if [ ! -f "$INSTALL_PREFIX/lib/pkgconfig/paho-mqttpp3.pc" ]; then
        log_info "paho-mqttpp3.pc 생성 중..."
        cat > "$INSTALL_PREFIX/lib/pkgconfig/paho-mqttpp3.pc" << EOF
prefix=$INSTALL_PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: paho-mqttpp3
Description: Eclipse Paho MQTT C++ Client Library
Version: 1.3.2
Requires: paho-mqtt3c
Libs: -L\${libdir} -lpaho-mqttpp3
Cflags: -I\${includedir}
EOF
    fi
    
    log_success "pkg-config 파일 생성 완료"
}

# 사용자 및 그룹 생성
create_pulseone_user() {
    log_step "PulseOne 사용자 설정 중..."
    
    # 사용자가 이미 존재하는지 확인
    if id "$PULSEONE_USER" &>/dev/null; then
        log_success "사용자 '$PULSEONE_USER'가 이미 존재합니다"
    else
        case $OS in
            "macos")
                # macOS에서는 사용자 생성을 건너뜀
                log_warning "macOS에서는 별도 사용자 생성을 건너뜁니다"
                PULSEONE_USER=$(whoami)
                ;;
            *)
                log_info "사용자 '$PULSEONE_USER' 생성 중..."
                useradd -r -s /bin/bash -d "$PULSEONE_HOME" -m "$PULSEONE_USER" 2>/dev/null || true
                ;;
        esac
    fi
    
    log_success "사용자 설정 완료: $PULSEONE_USER"
}

# 디렉토리 구조 생성
create_directory_structure() {
    log_step "디렉토리 구조 생성 중..."
    
    # 주요 디렉토리들 생성
    directories=(
        "$PULSEONE_HOME"
        "$PULSEONE_HOME/bin"
        "$PULSEONE_HOME/config"
        "$PULSEONE_HOME/config/drivers"
        "$PULSEONE_HOME/logs"
        "$PULSEONE_HOME/logs/packets"
        "$PULSEONE_HOME/data"
        "$PULSEONE_HOME/backup"
        "/var/log/pulseone"
        "/etc/pulseone"
    )
    
    for dir in "${directories[@]}"; do
        mkdir -p "$dir"
        log_info "디렉토리 생성: $dir"
    done
    
    # SQLite 데이터베이스 초기화
    SQLITE_DB="$PULSEONE_HOME/data/pulseone.db"
    if [ ! -f "$SQLITE_DB" ]; then
        log_info "SQLite 데이터베이스 초기화: $SQLITE_DB"
        touch "$SQLITE_DB"
        chmod 644 "$SQLITE_DB"
    fi
    
    log_success "디렉토리 구조 생성 완료"
}

# logrotate 설정
setup_logrotate() {
    log_step "logrotate 설정 중..."
    
    LOGROTATE_CONFIG="/etc/logrotate.d/pulseone_collector"
    
    if [ -f "$LOGROTATE_CONFIG" ]; then
        log_warning "logrotate 설정이 이미 존재합니다: $LOGROTATE_CONFIG"
    else
        log_info "logrotate 설정 생성 중..."
        cat > "$LOGROTATE_CONFIG" << EOF
# PulseOne Collector Log Rotation Configuration
$PULSEONE_HOME/logs/*.log /var/log/pulseone/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 644 $PULSEONE_USER $PULSEONE_USER
    postrotate
        /bin/kill -HUP \`cat $PULSEONE_HOME/pulseone-collector.pid 2> /dev/null\` 2> /dev/null || true
    endscript
}

# Packet logs (shorter retention)
$PULSEONE_HOME/logs/packets/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 644 $PULSEONE_USER $PULSEONE_USER
}
EOF
        chmod 644 "$LOGROTATE_CONFIG"
        log_success "logrotate 설정 생성: $LOGROTATE_CONFIG"
    fi
}

# systemd 서비스 설정
setup_systemd_service() {
    log_step "systemd 서비스 설정 중..."
    
    if [ "$OS" = "macos" ]; then
        log_warning "macOS에서는 systemd 서비스 설정을 건너뜁니다"
        return 0
    fi
    
    SERVICE_FILE="/etc/systemd/system/pulseone-collector.service"
    
    if [ -f "$SERVICE_FILE" ]; then
        log_warning "systemd 서비스가 이미 존재합니다: $SERVICE_FILE"
    else
        log_info "systemd 서비스 생성 중..."
        cat > "$SERVICE_FILE" << EOF
[Unit]
Description=PulseOne Data Collector
Documentation=https://pulseone.example.com/docs
After=network.target postgresql.service redis.service
Wants=postgresql.service redis.service

[Service]
Type=simple
User=$PULSEONE_USER
Group=$PULSEONE_USER
WorkingDirectory=$PULSEONE_HOME
ExecStart=$PULSEONE_HOME/bin/pulseone-collector
ExecReload=/bin/kill -HUP \$MAINPID
KillMode=process
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal
SyslogIdentifier=pulseone-collector

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=$PULSEONE_HOME /var/log/pulseone /tmp

# Resource limits
LimitNOFILE=65536
MemoryMax=1G

# Environment
Environment=LD_LIBRARY_PATH=$INSTALL_PREFIX/lib:/usr/lib:/lib
Environment=PKG_CONFIG_PATH=$INSTALL_PREFIX/lib/pkgconfig:/usr/lib/pkgconfig
Environment=PULSEONE_HOME=$PULSEONE_HOME
Environment=ENV_STAGE=production

[Install]
WantedBy=multi-user.target
EOF
        chmod 644 "$SERVICE_FILE"
        
        # systemd 설정 재로드
        systemctl daemon-reload
        
        log_success "systemd 서비스 생성: $SERVICE_FILE"
        log_info "서비스 관리 명령어:"
        echo "  sudo systemctl enable pulseone-collector   # 부팅시 자동 시작"
        echo "  sudo systemctl start pulseone-collector    # 서비스 시작"
        echo "  sudo systemctl status pulseone-collector   # 상태 확인"
    fi
}

# 권한 설정
setup_permissions() {
    log_step "권한 설정 중..."
    
    # 디렉토리 소유권 설정
    if [ "$OS" != "macos" ]; then
        chown -R "$PULSEONE_USER:$PULSEONE_USER" "$PULSEONE_HOME"
        chown -R "$PULSEONE_USER:$PULSEONE_USER" "/var/log/pulseone" 2>/dev/null || true
    fi
    
    # 권한 설정
    chmod 755 "$PULSEONE_HOME"
    chmod 755 "$PULSEONE_HOME/bin" 2>/dev/null || true
    chmod 644 "$PULSEONE_HOME/config"/* 2>/dev/null || true
    chmod 755 "$PULSEONE_HOME/logs"
    chmod 755 "$PULSEONE_HOME/data"
    
    log_success "권한 설정 완료"
}

# 환경 설정 파일 생성
create_environment_files() {
    log_step "환경 설정 파일 생성 중..."
    
    # 메인 환경 설정 파일
    ENV_FILE="$PULSEONE_HOME/config/.env.production"
    if [ ! -f "$ENV_FILE" ]; then
        log_info "환경 설정 파일 생성: $ENV_FILE"
        cat > "$ENV_FILE" << EOF
# PulseOne Collector Production Configuration
ENV_STAGE=production
LOG_LEVEL=info
DATA_COLLECTION_INTERVAL=5000
ENABLE_PACKET_LOGGING=false

# Database Configuration
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_USER=pulseone
POSTGRES_PASSWORD=change_me
POSTGRES_DB=pulseone

REDIS_HOST=localhost
REDIS_PORT=6379

# InfluxDB Configuration
INFLUX_HOST=localhost
INFLUX_PORT=8086
INFLUX_TOKEN=change_me
INFLUX_ORG=pulseone
INFLUX_BUCKET=metrics

# RabbitMQ Configuration
RABBITMQ_HOST=localhost
RABBITMQ_PORT=5672
RABBITMQ_USER=pulseone
RABBITMQ_PASS=change_me

# Paths
PULSEONE_HOME=$PULSEONE_HOME
DRIVER_CONFIG_PATH=$PULSEONE_HOME/config/drivers
LOG_PATH=$PULSEONE_HOME/logs

# Security
ENABLE_API_AUTH=true
API_SECRET_KEY=change_me_to_secure_random_string
EOF
        chmod 600 "$ENV_FILE"
    fi
    
    # 드라이버 설정 파일
    DRIVER_CONFIG="$PULSEONE_HOME/config/drivers/drivers.production.conf"
    if [ ! -f "$DRIVER_CONFIG" ]; then
        log_info "드라이버 설정 파일 생성: $DRIVER_CONFIG"
        mkdir -p "$(dirname "$DRIVER_CONFIG")"
        cat > "$DRIVER_CONFIG" << EOF
# PulseOne Driver Configuration - Production
# Format: protocol=type,endpoint=address,device_id=id,options...

# Example Modbus TCP Configuration
# protocol=modbus_tcp,endpoint=192.168.1.100:502,device_id=plc_001,timeout=5000,retry_count=3,enabled=true

# Example MQTT Configuration  
# protocol=mqtt,endpoint=mqtt://mqtt.example.com:1883,device_id=sensors_001,timeout=5000,enabled=true,protocol_config=client_id=pulseone&username=user&password=pass

# Example BACnet Configuration
# protocol=bacnet_ip,endpoint=eth0,device_id=260001,timeout=6000,enabled=true,protocol_config=interface=eth0&port=47808&who_is_enabled=true
EOF
        chmod 644 "$DRIVER_CONFIG"
    fi
    
    log_success "환경 설정 파일 생성 완료"
}

# 설치 확인
verify_installation() {
    log_step "설치 확인 중..."
    
    # 컴파일러 확인
    if command -v g++ &> /dev/null; then
        log_success "g++ 컴파일러: $(g++ --version | head -n1)"
    else
        log_error "g++ 컴파일러를 찾을 수 없습니다"
        return 1
    fi
    
    # pkg-config 확인
    if command -v pkg-config &> /dev/null; then
        log_success "pkg-config: $(pkg-config --version)"
    else
        log_error "pkg-config을 찾을 수 없습니다"
        return 1
    fi
    
    # 라이브러리 확인
    libraries=("libmodbus")
    for lib in "${libraries[@]}"; do
        if pkg-config --exists "$lib"; then
            log_success "$lib: $(pkg-config --modversion $lib)"
        else
            log_warning "$lib: pkg-config으로 찾을 수 없습니다"
        fi
    done
    
    # 중요 헤더 파일 확인
    headers=(
        "$INSTALL_PREFIX/include/modbus/modbus.h"
        "$INSTALL_PREFIX/include/MQTTClient.h"
        "$INSTALL_PREFIX/include/bacnet"
    )
    
    for header in "${headers[@]}"; do
        if [ -f "$header" ] || [ -d "$header" ]; then
            log_success "헤더 발견: $header"
        else
            log_warning "헤더 없음: $header"
        fi
    done
    
    # 디렉토리 구조 확인
    if [ -d "$PULSEONE_HOME" ]; then
        log_success "PulseOne 홈 디렉토리: $PULSEONE_HOME"
    else
        log_error "PulseOne 홈 디렉토리가 없습니다: $PULSEONE_HOME"
    fi
    
    log_success "설치 확인 완료"
}

# 설치 후 안내
show_post_install_info() {
    echo ""
    echo "========================================"
    log_success "PulseOne Collector 설치가 완료되었습니다!"
    echo "========================================"
    echo ""
    
    log_info "📁 설치 정보:"
    echo "  PulseOne 홈: $PULSEONE_HOME"
    echo "  실행 사용자: $PULSEONE_USER"
    echo "  설치 경로: $INSTALL_PREFIX"
    echo "  로그 디렉토리: $PULSEONE_HOME/logs"
    echo "  설정 디렉토리: $PULSEONE_HOME/config"
    echo ""
    
    log_info "🔧 다음 단계:"
    echo "  1. cd $(pwd)"
    echo "  2. make clean && make all"
    echo "  3. sudo cp bin/pulseone-collector $PULSEONE_HOME/bin/"
    echo "  4. 설정 파일 편집: $PULSEONE_HOME/config/.env.production"
    echo "  5. 드라이버 설정: $PULSEONE_HOME/config/drivers/drivers.production.conf"
    echo ""
    
    if [ "$OS" != "macos" ]; then
        log_info "🚀 서비스 관리:"
        echo "  sudo systemctl enable pulseone-collector   # 부팅시 자동 시작"
        echo "  sudo systemctl start pulseone-collector    # 서비스 시작"
        echo "  sudo systemctl status pulseone-collector   # 상태 확인"
        echo "  sudo journalctl -u pulseone-collector -f   # 로그 확인"
        echo ""
    fi
    
    log_info "📚 라이브러리 경로 (빌드 시 필요):"
    echo "  export LD_LIBRARY_PATH=$INSTALL_PREFIX/lib:\$LD_LIBRARY_PATH"
    echo "  export PKG_CONFIG_PATH=$INSTALL_PREFIX/lib/pkgconfig:\$PKG_CONFIG_PATH"
    echo ""
    
    log_warning "🔐 보안 주의사항:"
    echo "  1. $PULSEONE_HOME/config/.env.production 파일의 패스워드를 변경하세요"
    echo "  2. 데이터베이스 연결 정보를 실제 환경에 맞게 수정하세요"
    echo "  3. API 시크릿 키를 안전한 값으로 변경하세요"
    echo ""
    
    log_info "💡 문제 해결:"
    echo "  - 빌드 오류: make info로 빌드 정보 확인"
    echo "  - 라이브러리 오류: ldconfig 실행 후 재시도"
    echo "  - 권한 문제: $PULSEONE_USER 사용자 권한 확인"
    echo ""
}

# 메인 함수
main() {
    echo "🚀 PulseOne Collector 통합 설치 스크립트 v2.0"
    echo "=============================================="
    
    # 명령행 인자 파싱
    parse_arguments "$@"
    
    # OS 검출
    detect_os
    
    # 권한 확인
    check_permissions
    
    # 설치 타입에 따른 처리
    case $INSTALL_TYPE in
        "deps-only")
            log_info "의존성 라이브러리만 설치합니다"
            install_system_packages
            install_libmodbus
            install_paho_mqtt
            install_bacnet_stack
            if [ "$WITH_TESTS" = true ]; then
                install_googletest
            fi
            create_pkg_config_files
            verify_installation
            ;;
            
        "system-only")
            log_info "시스템 설정만 수행합니다"
            create_pulseone_user
            create_directory_structure
            setup_logrotate
            setup_systemd_service
            create_environment_files
            setup_permissions
            ;;
            
        "full")
            log_info "전체 설치를 수행합니다"
            
            # 의존성 설치
            install_system_packages
            install_libmodbus
            install_paho_mqtt
            install_bacnet_stack
            if [ "$WITH_TESTS" = true ]; then
                install_googletest
            fi
            create_pkg_config_files
            
            # 시스템 설정
            create_pulseone_user
            create_directory_structure
            setup_logrotate
            setup_systemd_service
            create_environment_files
            setup_permissions
            
            # 확인
            verify_installation
            ;;
    esac
    
    # 설치 완료 안내
    show_post_install_info
}

# 스크립트 실행
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi