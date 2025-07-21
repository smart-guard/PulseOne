# =============================================================================
# collector/CMakeLists.txt
# PulseOne Collector CMake 빌드 시스템 (Makefile 대안)
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(PulseOneCollector VERSION 1.0.0 LANGUAGES CXX C)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 컴파일러별 최적화 플래그
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
endif()

# 빌드 타입 기본값
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 디렉토리 설정
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")

# 인클루드 디렉토리
include_directories(${INCLUDE_DIR})

# =============================================================================
# 라이브러리 찾기
# =============================================================================

# pkg-config 사용
find_package(PkgConfig REQUIRED)

# libmodbus 찾기
pkg_check_modules(MODBUS libmodbus)
if(NOT MODBUS_FOUND)
    # 수동으로 찾기
    find_path(MODBUS_INCLUDE_DIR modbus/modbus.h PATHS /usr/local/include /usr/include)
    find_library(MODBUS_LIBRARY modbus PATHS /usr/local/lib /usr/lib)
    
    if(MODBUS_INCLUDE_DIR AND MODBUS_LIBRARY)
        set(MODBUS_FOUND TRUE)
        set(MODBUS_INCLUDE_DIRS ${MODBUS_INCLUDE_DIR})
        set(MODBUS_LIBRARIES ${MODBUS_LIBRARY})
    endif()
endif()

# Paho MQTT 찾기
find_path(PAHO_MQTT_INCLUDE_DIR MQTTClient.h PATHS /usr/local/include /usr/include)
find_library(PAHO_MQTT3C_LIBRARY paho-mqtt3c PATHS /usr/local/lib /usr/lib)
find_library(PAHO_MQTTPP3_LIBRARY paho-mqttpp3 PATHS /usr/local/lib /usr/lib)

if(PAHO_MQTT_INCLUDE_DIR AND PAHO_MQTT3C_LIBRARY AND PAHO_MQTTPP3_LIBRARY)
    set(PAHO_MQTT_FOUND TRUE)
    set(PAHO_MQTT_INCLUDE_DIRS ${PAHO_MQTT_INCLUDE_DIR})
    set(PAHO_MQTT_LIBRARIES ${PAHO_MQTTPP3_LIBRARY} ${PAHO_MQTT3C_LIBRARY})
endif()

# BACnet Stack 찾기
find_path(BACNET_INCLUDE_DIR bacnet PATHS /usr/local/include /usr/include)
if(BACNET_INCLUDE_DIR)
    set(BACNET_FOUND TRUE)
    set(BACNET_INCLUDE_DIRS ${BACNET_INCLUDE_DIR})
    # BACnet은 보통 헤더 온리이거나 정적 링크
    find_library(BACNET_LIBRARY bacnet PATHS /usr/local/lib /usr/lib)
    if(BACNET_LIBRARY)
        set(BACNET_LIBRARIES ${BACNET_LIBRARY})
    endif()
endif()

# 시스템 라이브러리들
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# UUID 라이브러리
if(APPLE)
    # macOS는 시스템에 UUID 포함
    set(UUID_LIBRARIES "")
else()
    find_library(UUID_LIBRARY uuid)
    set(UUID_LIBRARIES ${UUID_LIBRARY})
endif()

# =============================================================================
# 소스 파일 수집
# =============================================================================

# 메인 소스
set(MAIN_SOURCE ${SRC_DIR}/main.cpp)

# 드라이버 소스들
file(GLOB_RECURSE DRIVER_SOURCES "${SRC_DIR}/Drivers/*.cpp")

# 기타 모듈 소스들
file(GLOB_RECURSE CONFIG_SOURCES "${SRC_DIR}/Config/*.cpp")
file(GLOB_RECURSE DATABASE_SOURCES "${SRC_DIR}/Database/*.cpp")
file(GLOB_RECURSE UTILS_SOURCES "${SRC_DIR}/Utils/*.cpp")

# 모든 라이브러리 소스
set(LIB_SOURCES 
    ${DRIVER_SOURCES}
    ${CONFIG_SOURCES} 
    ${DATABASE_SOURCES}
    ${UTILS_SOURCES}
)

# =============================================================================
# 실행 파일 생성
# =============================================================================

# 메인 실행 파일
add_executable(${PROJECT_NAME} ${MAIN_SOURCE} ${LIB_SOURCES})

# 컴파일 정의
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_NAME="${PROJECT_NAME}"
    PROJECT_VERSION="${PROJECT_VERSION}"
)

# 인클루드 디렉토리 설정
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR})

# 라이브러리별 설정
if(MODBUS_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${MODBUS_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${MODBUS_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_LIBMODBUS)
else()
    message(WARNING "libmodbus not found - Modbus support disabled")
endif()

if(PAHO_MQTT_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PAHO_MQTT_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${PAHO_MQTT_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_PAHO_MQTT)
else()
    message(WARNING "Paho MQTT not found - MQTT support disabled")
endif()

if(BACNET_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${BACNET_INCLUDE_DIRS})
    if(BACNET_LIBRARIES)
        target_link_libraries(${PROJECT_NAME} ${BACNET_LIBRARIES})
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_BACNET)
else()
    message(WARNING "BACnet Stack not found - BACnet support disabled")
endif()

# 필수 시스템 라이브러리
target_link_libraries(${PROJECT_NAME} 
    Threads::Threads
    OpenSSL::SSL 
    OpenSSL::Crypto
    ${UUID_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

# =============================================================================
# 플러그인 라이브러리들 (선택적)
# =============================================================================

option(BUILD_PLUGINS "Build driver plugins as shared libraries" OFF)

if(BUILD_PLUGINS)
    # Modbus 드라이버 플러그인
    if(MODBUS_FOUND)
        add_library(modbus_driver SHARED 
            ${SRC_DIR}/Drivers/ModbusDriver.cpp
            ${SRC_DIR}/Drivers/CommonTypes.cpp
            ${SRC_DIR}/Drivers/DriverLogger.cpp
        )
        target_include_directories(modbus_driver PRIVATE ${INCLUDE_DIR} ${MODBUS_INCLUDE_DIRS})
        target_link_libraries(modbus_driver ${MODBUS_LIBRARIES} Threads::Threads)
        set_target_properties(modbus_driver PROPERTIES PREFIX "lib")
    endif()
    
    # MQTT 드라이버 플러그인
    if(PAHO_MQTT_FOUND)
        add_library(mqtt_driver SHARED 
            ${SRC_DIR}/Drivers/MQTTDriver.cpp
            ${SRC_DIR}/Drivers/CommonTypes.cpp
            ${SRC_DIR}/Drivers/DriverLogger.cpp
        )
        target_include_directories(mqtt_driver PRIVATE ${INCLUDE_DIR} ${PAHO_MQTT_INCLUDE_DIRS})
        target_link_libraries(mqtt_driver ${PAHO_MQTT_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
        set_target_properties(mqtt_driver PROPERTIES PREFIX "lib")
    endif()
    
    # BACnet 드라이버 플러그인
    if(BACNET_FOUND)
        add_library(bacnet_driver SHARED 
            ${SRC_DIR}/Drivers/BACnetDriver.cpp
            ${SRC_DIR}/Drivers/CommonTypes.cpp
            ${SRC_DIR}/Drivers/DriverLogger.cpp
        )
        target_include_directories(bacnet_driver PRIVATE ${INCLUDE_DIR} ${BACNET_INCLUDE_DIRS})
        if(BACNET_LIBRARIES)
            target_link_libraries(bacnet_driver ${BACNET_LIBRARIES})
        endif()
        target_link_libraries(bacnet_driver Threads::Threads)
        set_target_properties(bacnet_driver PROPERTIES PREFIX "lib")
    endif()
endif()

# =============================================================================
# 테스트 (선택적)
# =============================================================================

option(BUILD_TESTS "Build test suite" OFF)

if(BUILD_TESTS)
    # GoogleTest 찾기
    find_package(GTest)
    
    if(GTEST_FOUND)
        enable_testing()
        
        file(GLOB_RECURSE TEST_SOURCES "${TEST_DIR}/*.cpp")
        
        add_executable(run_tests ${TEST_SOURCES} ${LIB_SOURCES})
        
        target_include_directories(run_tests PRIVATE 
            ${INCLUDE_DIR} 
            ${TEST_DIR}
            ${GTEST_INCLUDE_DIRS}
        )
        
        target_link_libraries(run_tests 
            ${GTEST_LIBRARIES}
            ${GTEST_MAIN_LIBRARIES}
            Threads::Threads
        )
        
        # 라이브러리 설정 복사
        if(MODBUS_FOUND)
            target_include_directories(run_tests PRIVATE ${MODBUS_INCLUDE_DIRS})
            target_link_libraries(run_tests ${MODBUS_LIBRARIES})
        endif()
        
        if(PAHO_MQTT_FOUND)
            target_include_directories(run_tests PRIVATE ${PAHO_MQTT_INCLUDE_DIRS})
            target_link_libraries(run_tests ${PAHO_MQTT_LIBRARIES})
        endif()
        
        if(BACNET_FOUND)
            target_include_directories(run_tests PRIVATE ${BACNET_INCLUDE_DIRS})
            if(BACNET_LIBRARIES)
                target_link_libraries(run_tests ${BACNET_LIBRARIES})
            endif()
        endif()
        
        # 테스트 등록
        add_test(NAME unit_tests COMMAND run_tests)
    else()
        message(WARNING "GoogleTest not found - tests disabled")
    endif()
endif()

# =============================================================================
# 설치 규칙
# =============================================================================

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

if(BUILD_PLUGINS)
    install(TARGETS modbus_driver mqtt_driver bacnet_driver
        LIBRARY DESTINATION lib/pulseone/drivers
        OPTIONAL
    )
endif()

# =============================================================================
# 정보 출력
# =============================================================================

message(STATUS "")
message(STATUS "PulseOne Collector Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Library Support:")
message(STATUS "  libmodbus: ${MODBUS_FOUND}")
message(STATUS "  Paho MQTT: ${PAHO_MQTT_FOUND}")
message(STATUS "  BACnet Stack: ${BACNET_FOUND}")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Build Plugins: ${BUILD_PLUGINS}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "")