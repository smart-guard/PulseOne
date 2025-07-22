# ============================================================================
# collector/tests/CMakeLists.txt
# MQTT 드라이버 테스트 빌드 설정
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(PulseOneTests)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 컴파일러 옵션
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

# Google Test 찾기
find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# 포함 디렉토리 설정
include_directories(
    ${CMAKE_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/../src
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# MQTT 라이브러리 찾기
find_library(PAHO_MQTT_CPP_LIB paho-mqttpp3)
find_library(PAHO_MQTT_C_LIB paho-mqtt3c)

# PostgreSQL 라이브러리
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQ REQUIRED libpq)

# Redis 라이브러리
pkg_check_modules(HIREDIS REQUIRED hiredis)

# 소스 파일들
set(MQTT_DRIVER_SOURCES
    ${CMAKE_SOURCE_DIR}/../src/Drivers/Mqtt/MqttDriver.cpp
    ${CMAKE_SOURCE_DIR}/../src/Drivers/Common/DriverLogger.cpp
    ${CMAKE_SOURCE_DIR}/../src/Config/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/../src/Utils/LogManager.cpp
)

set(TEST_SOURCES
    Drivers/Mqtt/MqttDriverTest.cpp
)

# 테스트 실행 파일 생성
add_executable(mqtt_driver_tests
    ${TEST_SOURCES}
    ${MQTT_DRIVER_SOURCES}
)

# 라이브러리 링크
target_link_libraries(mqtt_driver_tests
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${PAHO_MQTT_CPP_LIB}
    ${PAHO_MQTT_C_LIB}
    ${LIBPQ_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    pthread
)

# 컴파일러 플래그 추가
target_compile_options(mqtt_driver_tests PRIVATE
    ${LIBPQ_CFLAGS_OTHER}
    ${HIREDIS_CFLAGS_OTHER}
)

# 테스트 등록
enable_testing()
add_test(NAME MqttDriverTests COMMAND mqtt_driver_tests)

# ============================================================================
# collector/tests/Drivers/Mqtt/CMakeLists.txt
# MQTT 드라이버 단위 테스트 개별 설정
# ============================================================================

# MQTT 드라이버 테스트만 별도로 빌드하는 경우
add_executable(mqtt_tests_only
    MqttDriverTest.cpp
    ${CMAKE_SOURCE_DIR}/../../src/Drivers/Mqtt/MqttDriver.cpp
)

target_link_libraries(mqtt_tests_only
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${PAHO_MQTT_CPP_LIB}
    ${PAHO_MQTT_C_LIB}
    pthread
)

# ============================================================================
# collector/tests/build_and_run.sh
# 테스트 빌드 및 실행 스크립트
# ============================================================================

#!/bin/bash

# 테스트 빌드 및 실행 스크립트
set -e

echo "🔧 PulseOne MQTT Driver Tests"
echo "==============================="

# 빌드 디렉토리 생성
mkdir -p build
cd build

# CMake 설정
echo "📋 Configuring CMake..."
cmake ..

# 빌드
echo "🔨 Building tests..."
make -j$(nproc)

# 테스트 실행
echo "🧪 Running tests..."
./mqtt_driver_tests --gtest_output=xml:mqtt_test_results.xml

# 테스트 결과 출력
echo "📊 Test results:"
if [ -f mqtt_test_results.xml ]; then
    echo "✅ Test results saved to mqtt_test_results.xml"
else
    echo "❌ Test results file not found"
fi

echo "🎉 Tests completed!"

# ============================================================================
# collector/Makefile에 추가할 테스트 타겟
# ============================================================================

# 기존 Makefile에 추가할 내용:

# Google Test 설정
GTEST_LIBS = -lgtest -lgtest_main -lgmock -lgmock_main
TEST_DIR = tests
TEST_BUILD_DIR = $(BUILD_DIR)/tests

# 테스트 소스 파일들
TEST_SOURCES = $(shell find $(TEST_DIR) -name "*.cpp")
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(TEST_BUILD_DIR)/%.o)

# 테스트 타겟
.PHONY: test test-mqtt test-clean test-install

# 모든 테스트 빌드 및 실행
test: test-mqtt
	@echo -e "$(GREEN)✅ All tests completed$(NC)"

# MQTT 드라이버 테스트만 실행
test-mqtt: $(BIN_DIR)/mqtt_tests
	@echo -e "$(BLUE)🧪 Running MQTT driver tests...$(NC)"
	@$(BIN_DIR)/mqtt_tests

# MQTT 테스트 실행 파일 생성
$(BIN_DIR)/mqtt_tests: $(TEST_BUILD_DIR)/Drivers/Mqtt/MqttDriverTest.o $(filter-out $(BUILD_DIR)/main.o, $(OBJECTS))
	@echo -e "$(BLUE)🔗 Linking MQTT tests...$(NC)"
	@mkdir -p $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS) $(GTEST_LIBS)

# 테스트 객체 파일 생성
$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp
	@echo -e "$(YELLOW)📝 Compiling test: $<$(NC)"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Google Test 설치 (Ubuntu/Debian)
test-install:
	@echo -e "$(BLUE)📦 Installing Google Test...$(NC)"
	@sudo apt-get update
	@sudo apt-get install -y libgtest-dev libgmock-dev
	@echo -e "$(GREEN)✅ Google Test installed$(NC)"

# 테스트 정리
test-clean:
	@echo -e "$(BLUE)🧹 Cleaning test files...$(NC)"
	@rm -rf $(TEST_BUILD_DIR)
	@rm -f $(BIN_DIR)/mqtt_tests
	@echo -e "$(GREEN)✅ Test files cleaned$(NC)"

# ============================================================================
# .github/workflows/tests.yml
# GitHub Actions CI/CD 설정
# ============================================================================

name: Unit Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest
    
    services:
      mosquitto:
        image: eclipse-mosquitto:latest
        ports:
          - 1883:1883
        options: >-
          --health-cmd "mosquitto_sub -t '$SYS/broker/uptime' -C 1"
          --health-interval 10s
          --health-timeout 5s
          --health-retries 5
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y \
          build-essential \
          cmake \
          libgtest-dev \
          libgmock-dev \
          libpaho-mqtt-dev \
          libpq-dev \
          libhiredis-dev \
          libnlohmann-json3-dev
    
    - name: Build tests
      run: |
        cd collector/tests
        mkdir -p build && cd build
        cmake ..
        make -j$(nproc)
    
    - name: Run tests
      run: |
        cd collector/tests/build
        ./mqtt_driver_tests --gtest_output=xml:test_results.xml
    
    - name: Upload test results
      uses: actions/upload-artifact@v3
      if: always()
      with:
        name: test-results
        path: collector/tests/build/test_results.xml