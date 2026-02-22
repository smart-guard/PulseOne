// collector/include/Common/Constants.h
#ifndef PULSEONE_COMMON_CONSTANTS_H
#define PULSEONE_COMMON_CONSTANTS_H

/**
 * @file Constants.h
 * @brief PulseOne 통합 상수 정의 (중복 제거)
 * @author PulseOne Development Team
 * @date 2025-08-04
 * @details
 * 🎯 목적: 기존 프로젝트의 모든 중복 상수들을 하나로 통합
 * 📋 기존 코드 분석 결과:
 * - 분산된 #define 매크로들 통합
 * - 프로토콜별 중복 상수들 정리
 * - ConfigManager에서 사용하는 기본값들 포함
 * - 타입 안전한 constexpr 사용
 */

#include <chrono>
#include <cstdint>
#include <string_view>

namespace PulseOne {
namespace Constants {

// =========================================================================
// 🔥 시간 관련 상수들 (기존 여러 정의 통합)
// =========================================================================

// 기본 타임아웃 및 주기
constexpr int DEFAULT_TIMEOUT_MS = 5000;             // 5초 기본 타임아웃
constexpr int DEFAULT_CONNECTION_TIMEOUT_MS = 10000; // 10초 연결 타임아웃
constexpr int DEFAULT_READ_TIMEOUT_MS = 3000;        // 3초 읽기 타임아웃
constexpr int DEFAULT_WRITE_TIMEOUT_MS = 5000;       // 5초 쓰기 타임아웃
constexpr int DEFAULT_POLLING_INTERVAL_MS = 1000;    // 1초 폴링 간격
constexpr int DEFAULT_RETRY_INTERVAL_MS = 2000;      // 2초 재시도 간격
constexpr int DEFAULT_RECONNECT_DELAY_MS = 10000;    // 10초 재연결 지연
constexpr int DEFAULT_HEARTBEAT_INTERVAL_MS = 30000; // 30초 하트비트

// 최대값들
constexpr int MAX_TIMEOUT_MS = 300000;           // 5분 최대 타임아웃
constexpr int MAX_POLLING_INTERVAL_MS = 3600000; // 1시간 최대 폴링
constexpr int MAX_RETRY_COUNT = 10;              // 최대 재시도 횟수
constexpr int MAX_RECONNECT_ATTEMPTS = 5;        // 최대 재연결 시도

// 최소값들
constexpr int MIN_TIMEOUT_MS = 100;         // 100ms 최소 타임아웃
constexpr int MIN_POLLING_INTERVAL_MS = 50; // 50ms 최소 폴링

// 🆕 점검 관련 시간 상수
constexpr int MAINTENANCE_CHECK_INTERVAL_MS = 60000;    // 1분 점검 상태 확인
constexpr int MAINTENANCE_AUTO_RELEASE_HOURS = 8;       // 8시간 후 자동 해제
constexpr int MAINTENANCE_WARNING_MINUTES = 15;         // 15분 전 점검 경고
constexpr int REMOTE_CONTROL_BLOCK_DURATION_MS = 30000; // 30초 원격제어 차단

// =========================================================================
// 🔥 프로토콜별 기본 포트 및 설정 (중복 제거)
// =========================================================================

// Modbus 관련 상수
constexpr int DEFAULT_MODBUS_TCP_PORT = 502;
constexpr int DEFAULT_MODBUS_RTU_BAUDRATE = 9600;
constexpr int DEFAULT_MODBUS_SLAVE_ID = 1;
constexpr int DEFAULT_MODBUS_MAX_REGISTERS =
    125; // 한번에 읽을 수 있는 최대 레지스터
constexpr int MODBUS_MAX_SLAVE_ID = 247;
constexpr int MODBUS_MIN_SLAVE_ID = 1;

// MQTT 관련 상수
constexpr int DEFAULT_MQTT_PORT = 1883;
constexpr int DEFAULT_MQTT_SSL_PORT = 8883;
constexpr int DEFAULT_MQTT_WEBSOCKET_PORT = 8080;
constexpr int DEFAULT_MQTT_QOS = 1;
constexpr int DEFAULT_MQTT_KEEPALIVE_S = 60;
constexpr int MAX_MQTT_PAYLOAD_SIZE =
    268435455; // MQTT 최대 페이로드 (256MB - 1)
constexpr int MAX_MQTT_TOPIC_LENGTH = 65535;

// BACnet 관련 상수
constexpr int DEFAULT_BACNET_PORT = 47808;
constexpr int DEFAULT_BACNET_DEVICE_ID = 1234;
constexpr int DEFAULT_BACNET_MAX_APDU = 1476;
constexpr int BACNET_MIN_DEVICE_ID = 1;
constexpr int BACNET_MAX_DEVICE_ID = 4194303; // 22비트 최대값

// OPC-UA 관련 상수 (향후 확장용)
constexpr int DEFAULT_OPC_UA_PORT = 4840;
constexpr int DEFAULT_OPC_UA_SECURE_PORT = 4843;

// =========================================================================
// 🔥 데이터 품질 및 임계값 상수들
// =========================================================================

// 데이터 품질 관련
constexpr int DATA_STALE_THRESHOLD_MS = 30000; // 30초 후 오래된 데이터
constexpr int DATA_VERY_STALE_THRESHOLD_MS =
    300000; // 5분 후 매우 오래된 데이터
constexpr int DATA_QUALITY_CHECK_INTERVAL_MS = 10000; // 10초마다 품질 확인
constexpr double DATA_QUALITY_THRESHOLD = 0.95;       // 95% 품질 임계값

// 성능 관련 임계값
constexpr int SCAN_DELAY_WARNING_MS = 5000;          // 5초 이상 지연 시 경고
constexpr int SCAN_DELAY_ERROR_MS = 30000;           // 30초 이상 지연 시 에러
constexpr double CPU_USAGE_WARNING_THRESHOLD = 80.0; // CPU 사용률 80% 경고
constexpr double MEMORY_USAGE_WARNING_THRESHOLD =
    85.0; // 메모리 사용률 85% 경고

// =========================================================================
// 🔥 버퍼 및 큐 크기 상수들
// =========================================================================

// 메시지 큐 크기
constexpr size_t DEFAULT_MESSAGE_QUEUE_SIZE = 10000;
constexpr size_t MAX_MESSAGE_QUEUE_SIZE = 100000;
constexpr size_t DEFAULT_ASYNC_QUEUE_SIZE = 1000;

// 버퍼 크기
constexpr size_t DEFAULT_BUFFER_SIZE = 8192;      // 8KB 기본 버퍼
constexpr size_t MAX_BUFFER_SIZE = 1048576;       // 1MB 최대 버퍼
constexpr size_t DEFAULT_LOG_BUFFER_SIZE = 65536; // 64KB 로그 버퍼

// 배치 처리 크기
constexpr size_t DEFAULT_BATCH_SIZE = 100;     // 기본 배치 크기
constexpr size_t MAX_BATCH_SIZE = 1000;        // 최대 배치 크기
constexpr size_t DEFAULT_READ_BATCH_SIZE = 50; // 읽기 배치 크기

// =========================================================================
// 🔥 스레드 및 동시성 관련 상수들
// =========================================================================

// 스레드 풀 크기
constexpr size_t DEFAULT_THREAD_POOL_SIZE = 4;
constexpr size_t MAX_THREAD_POOL_SIZE = 32;
constexpr size_t MIN_THREAD_POOL_SIZE = 1;

// 동시 연결 수
constexpr size_t DEFAULT_MAX_CONNECTIONS = 100;
constexpr size_t MAX_CONCURRENT_CONNECTIONS = 1000;
constexpr size_t DEFAULT_CONNECTION_POOL_SIZE = 10;

// =========================================================================
// 🔥 로그 관련 상수들 (기존 LogManager 호환)
// =========================================================================

// 로그 파일 관련
constexpr size_t DEFAULT_LOG_FILE_SIZE = 10485760; // 10MB 기본 로그 파일 크기
constexpr size_t MAX_LOG_FILE_SIZE = 104857600;    // 100MB 최대 로그 파일 크기
constexpr int DEFAULT_LOG_RETENTION_DAYS = 30;     // 30일 로그 보관
constexpr int MAX_LOG_FILES = 10;                  // 최대 로그 파일 수

// 로그 레벨 및 출력
constexpr bool DEFAULT_CONSOLE_LOG_ENABLED = true;
constexpr bool DEFAULT_FILE_LOG_ENABLED = true;
constexpr bool DEFAULT_REMOTE_LOG_ENABLED = false;

// 🆕 점검 관련 로그
constexpr bool DEFAULT_MAINTENANCE_ALERT_ENABLED = true;
constexpr int MAINTENANCE_LOG_RETENTION_DAYS = 365; // 점검 로그는 1년 보관

// =========================================================================
// 🔥 데이터베이스 관련 상수들
// =========================================================================

// 연결 풀 설정
constexpr size_t DEFAULT_DB_CONNECTION_POOL_SIZE = 10;
constexpr size_t MAX_DB_CONNECTION_POOL_SIZE = 50;
constexpr int DEFAULT_DB_CONNECTION_TIMEOUT_MS = 30000; // 30초

// 쿼리 관련
constexpr int DEFAULT_DB_QUERY_TIMEOUT_MS = 30000; // 30초
constexpr size_t DEFAULT_DB_BATCH_SIZE = 1000;     // 배치 삽입 크기
constexpr int DEFAULT_DB_RETRY_COUNT = 3;

// Redis 관련
constexpr int DEFAULT_REDIS_PORT = 6379;
constexpr int DEFAULT_REDIS_DB = 0;
constexpr int DEFAULT_REDIS_POOL_SIZE = 10;

// InfluxDB 관련
constexpr int DEFAULT_INFLUXDB_PORT = 8086;
constexpr size_t DEFAULT_INFLUXDB_BATCH_SIZE = 5000;

// =========================================================================
// 🔥 보안 및 인증 관련 상수들
// =========================================================================

// 인증 관련
constexpr int DEFAULT_SESSION_TIMEOUT_MINUTES = 480; // 8시간 세션 타임아웃
constexpr int DEFAULT_TOKEN_EXPIRY_HOURS = 24;       // 24시간 토큰 만료
constexpr int MAX_LOGIN_ATTEMPTS = 5;                // 최대 로그인 시도
constexpr int LOGIN_LOCKOUT_MINUTES = 15;            // 15분 로그인 잠금

// 🆕 점검 모드 보안
constexpr int MAINTENANCE_ACCESS_TIMEOUT_MINUTES =
    60; // 1시간 점검 접근 타임아웃
constexpr int MAINTENANCE_PASSWORD_MIN_LENGTH = 8;
constexpr bool MAINTENANCE_REQUIRE_TWO_FACTOR = false; // 이중 인증 필요 여부

// =========================================================================
// 🔥 성능 모니터링 관련 상수들
// =========================================================================

// 통계 수집 간격
constexpr int STATISTICS_COLLECTION_INTERVAL_MS = 5000; // 5초마다 통계 수집
constexpr int PERFORMANCE_REPORT_INTERVAL_MS = 60000;   // 1분마다 성능 리포트
constexpr int HEALTH_CHECK_INTERVAL_MS = 30000;         // 30초마다 헬스 체크

// 성능 임계값
constexpr double MAX_CPU_USAGE_PERCENT = 90.0;
constexpr double MAX_MEMORY_USAGE_PERCENT = 90.0;
constexpr double MAX_DISK_USAGE_PERCENT = 85.0;
constexpr int MAX_RESPONSE_TIME_MS = 10000; // 10초 최대 응답 시간

// =========================================================================
// 🔥 문자열 상수들 (기존 하드코딩된 문자열들 통합)
// =========================================================================

// 시스템 모듈명들 (로그 모듈 식별용)
constexpr std::string_view LOG_MODULE_SYSTEM = "SYSTEM";
constexpr std::string_view LOG_MODULE_DRIVER = "DRIVER";
constexpr std::string_view LOG_MODULE_DATABASE = "DATABASE";
constexpr std::string_view LOG_MODULE_CONFIG = "CONFIG";
constexpr std::string_view LOG_MODULE_NETWORK = "NETWORK";
constexpr std::string_view LOG_MODULE_SECURITY = "SECURITY";
constexpr std::string_view LOG_MODULE_MAINTENANCE = "MAINTENANCE"; // 🆕

// 프로토콜 문자열들
constexpr std::string_view PROTOCOL_MODBUS_TCP = "modbus_tcp";
constexpr std::string_view PROTOCOL_MODBUS_RTU = "modbus_rtu";
constexpr std::string_view PROTOCOL_MQTT = "mqtt";
constexpr std::string_view PROTOCOL_MQTT_5 = "mqtt5";
constexpr std::string_view PROTOCOL_BACNET_IP = "bacnet_ip";
constexpr std::string_view PROTOCOL_BACNET_MSTP = "bacnet_mstp";
constexpr std::string_view PROTOCOL_OPC_UA = "opc_ua";

// 기본 설정 키들
constexpr std::string_view CONFIG_LOG_LEVEL = "LOG_LEVEL";
constexpr std::string_view CONFIG_LOG_FILE_PATH = "LOG_FILE_PATH";
constexpr std::string_view CONFIG_DB_CONNECTION_STRING = "DB_CONNECTION_STRING";
constexpr std::string_view CONFIG_REDIS_HOST = "REDIS_HOST";
constexpr std::string_view CONFIG_REDIS_PORT = "REDIS_PORT";
constexpr std::string_view CONFIG_MAINTENANCE_MODE = "MAINTENANCE_MODE"; // 🆕

// 파일 경로들
constexpr std::string_view DEFAULT_CONFIG_DIR = "./config";
constexpr std::string_view DEFAULT_LOG_DIR = "./logs";
constexpr std::string_view DEFAULT_DATA_DIR = "./data";
constexpr std::string_view DEFAULT_PLUGINS_DIR = "./drivers";
constexpr std::string_view DEFAULT_MAINTENANCE_LOG_FILE =
    "maintenance.log"; // 🆕

// =========================================================================
// 🔥 버전 및 식별 정보
// =========================================================================

// 시스템 정보
constexpr std::string_view SYSTEM_NAME = "PulseOne";
constexpr std::string_view SYSTEM_VERSION = "2.0.0";
constexpr std::string_view BUILD_DATE = __DATE__;
constexpr std::string_view BUILD_TIME = __TIME__;

// API 버전
constexpr int API_VERSION_MAJOR = 2;
constexpr int API_VERSION_MINOR = 0;
constexpr int API_VERSION_PATCH = 0;

// 호환성 정보
constexpr int MIN_SUPPORTED_PROTOCOL_VERSION = 1;
constexpr int MAX_SUPPORTED_PROTOCOL_VERSION = 2;

// =========================================================================
// 🔥 하드웨어 및 시스템 제한값들
// =========================================================================

// 메모리 제한
constexpr size_t MAX_MEMORY_USAGE_BYTES = 2147483648ULL; // 2GB 최대 메모리 사용
constexpr size_t DEFAULT_CACHE_SIZE_BYTES = 268435456;   // 256MB 기본 캐시 크기

// 네트워크 제한
constexpr size_t MAX_PACKET_SIZE_BYTES = 65536; // 64KB 최대 패킷 크기
constexpr int MAX_NETWORK_RETRY_COUNT = 5;
constexpr int NETWORK_KEEPALIVE_TIMEOUT_S =
    300; // 5분 킵얼라이브

// =========================================================================
// 🔥 기존 프로젝트 호환성을 위한 매크로들 (점진적 제거 예정)
// =========================================================================

// 기존 코드에서 사용했을 수 있는 매크로들을 constexpr로 대체
#ifndef PULSEONE_DISABLE_LEGACY_MACROS
#define DEFAULT_TIMEOUT DEFAULT_TIMEOUT_MS
#define DEFAULT_PORT_MODBUS DEFAULT_MODBUS_TCP_PORT
#define DEFAULT_PORT_MQTT DEFAULT_MQTT_PORT
#define DEFAULT_PORT_BACNET DEFAULT_BACNET_PORT
         // 점진적으로 제거할 예정 - 새 코드에서는 사용 금지
#endif

} // namespace Constants
} // namespace PulseOne

#endif // PULSEONE_COMMON_CONSTANTS_H