# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(PulseOne VERSION 2.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 플랫폼 감지
if(WIN32)
    set(PULSEONE_WINDOWS 1)
    set(PULSEONE_LINUX 0)
else()
    set(PULSEONE_WINDOWS 0) 
    set(PULSEONE_LINUX 1)
endif()

# 라이브러리 찾기
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# 옵셔널 라이브러리
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBMODBUS libmodbus)
    pkg_check_modules(HIREDIS hiredis)
    pkg_check_modules(QUICKJS quickjs)
endif()

# 소스 수집
file(GLOB_RECURSE SOURCES 
    "src/Core/*.cpp"
    "src/Utils/*.cpp"
    "src/Config/*.cpp"
    "src/Database/*.cpp"
    "src/Workers/*.cpp"
    "src/Drivers/*.cpp"
    "src/Pipeline/*.cpp"
    "src/Alarm/*.cpp"
    "src/VirtualPoint/*.cpp"
    "src/Platform/*.cpp"
)

# 실행 파일
add_executable(${PROJECT_NAME} ${SOURCES} src/main.cpp)

# 헤더 경로
target_include_directories(${PROJECT_NAME} PRIVATE 
    include
    include/Platform
    include/Api
    include/Alarm
    include/VirtualPoint
    include/Network
    include/Workers
    include/Database
    include/Utils
    include/Config
    include/Drivers
    include/Pipeline
)

# 링크 라이브러리
target_link_libraries(${PROJECT_NAME} 
    Threads::Threads
    SQLite::SQLite3
)

# 조건부 링크
if(LIBMODBUS_FOUND)
    target_link_libraries(${PROJECT_NAME} ${LIBMODBUS_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_MODBUS=1)
endif()

if(HIREDIS_FOUND)
    target_link_libraries(${PROJECT_NAME} ${HIREDIS_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_HIREDIS=1)
endif()

# Windows 특화
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32 iphlpapi)
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        PULSEONE_WINDOWS=1
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    
    # 정적 링크 (배포용)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "-static-libgcc -static-libstdc++ -static"
        )
    endif()
endif()

# CPack 설정 (Windows 인스톨러)
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "PulseOne")
    set(CPACK_PACKAGE_VENDOR "PulseOne Technologies")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Industrial IoT Data Collector")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "PulseOne")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$DESKTOP\\\\PulseOne.lnk' '$INSTDIR\\\\bin\\\\PulseOne.exe'"
    )
    include(CPack)
endif()