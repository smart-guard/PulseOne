#ifndef PULSEONE_COMMON_CONSTANTS_H
#define PULSEONE_COMMON_CONSTANTS_H

/**
 * @file Constants.h
 * @brief PulseOne 전역 상수 정의
 * @author PulseOne Development Team
 * @date 2025-07-24
 */

#include <chrono>
#include <cstdint>

namespace PulseOne::Constants {
    
    // ========================================
    // 통신 타임아웃 및 재시도 상수
    // ========================================
    constexpr int DEFAULT_TIMEOUT_MS = 5000;
    constexpr int DEFAULT_RETRY_COUNT = 3;
    constexpr int DEFAULT_POLLING_INTERVAL_MS = 1000;
    constexpr int DEFAULT_RECONNECT_DELAY_MS = 10000;
    
    // ========================================
    // 프로토콜별 기본 포트
    // ========================================
    constexpr int DEFAULT_MODBUS_TCP_PORT = 502;
    constexpr int DEFAULT_MQTT_PORT = 1883;
    constexpr int DEFAULT_MQTT_SSL_PORT = 8883;
    constexpr int DEFAULT_BACNET_PORT = 47808;
    constexpr int DEFAULT_OPC_UA_PORT = 4840;
    
    // ========================================
    // 데이터 품질 및 점검 관련 상수
    // ========================================
    constexpr int DATA_STALE_THRESHOLD_MS = 30000;     // 30초 후 오래된 데이터
    constexpr int DATA_VERY_STALE_THRESHOLD_MS = 300000; // 5분 후 매우 오래된 데이터
    constexpr int SCAN_DELAY_WARNING_MS = 5000;        // 스캔 지연 경고 임계값
    constexpr int MAINTENANCE_CHECK_INTERVAL_MS = 60000; // 점검 상태 확인 주기 (1분)
    
    // ========================================
    // 점검 모드 및 권한 제어
    // ========================================
    constexpr int MAINTENANCE_AUTO_RELEASE_HOURS = 8;   // 8시간 후 자동 점검 해제
    constexpr int REMOTE_CONTROL_BLOCK_DURATION_MS = 30000; // 원격제어 차단 지속시간
    constexpr bool DEFAULT_MAINTENANCE_ALLOWED = true;  // 기본 점검 허용 여부
    constexpr bool DEFAULT_REMOTE_CONTROL_ENABLED = true; // 기본 원격제어 허용
    
    // ========================================
    // Modbus 프로토콜 상수
    // ========================================
    constexpr int MODBUS_MAX_REGISTERS_PER_READ = 125;
    constexpr int MODBUS_MAX_COILS_PER_READ = 2000;
    constexpr uint8_t MODBUS_DEFAULT_SLAVE_ID = 1;
    constexpr uint16_t MODBUS_MAX_ADDRESS = 65535;
    constexpr uint8_t MODBUS_MIN_SLAVE_ID = 1;
    constexpr uint8_t MODBUS_MAX_SLAVE_ID = 247;

    // ========================================
    // MQTT 프로토콜 상수
    // ========================================
    constexpr int MQTT_QOS_AT_MOST_ONCE = 0;
    constexpr int MQTT_QOS_AT_LEAST_ONCE = 1;
    constexpr int MQTT_QOS_EXACTLY_ONCE = 2;
    constexpr int MQTT_KEEP_ALIVE_SECONDS = 60;
    constexpr int MQTT_MAX_PAYLOAD_SIZE = 1024 * 1024; // 1MB
    constexpr int MQTT_MAX_TOPIC_LENGTH = 256;
    constexpr int MQTT_MAX_CLIENT_ID_LENGTH = 23;

    // ========================================
    // BACnet 프로토콜 상수 (⭐ 확장됨)
    // ========================================
    constexpr uint16_t BACNET_MAX_APDU_LENGTH = 1476;
    constexpr uint32_t BACNET_DEFAULT_DEVICE_INSTANCE = 260001;
    constexpr uint8_t BACNET_MAX_SEGMENTS = 64;
    constexpr uint16_t BACNET_DEFAULT_PORT = 47808;
    
    // 🔥 BACnet Instance 범위 및 제한값
    constexpr uint32_t BACNET_MIN_INSTANCE = 0;
    constexpr uint32_t BACNET_MAX_INSTANCE = 4194303;  // 2^22 - 1 (표준 최대값)
    constexpr uint32_t BACNET_BROADCAST_DEVICE_ID = 4194303;
    
    // 🔥 BACnet APDU 설정
    constexpr uint32_t BACNET_MIN_APDU_LENGTH = 50;
    constexpr uint32_t BACNET_MAX_APDU_LENGTH_EXTENDED = 1476;
    constexpr uint8_t BACNET_DEFAULT_APDU_RETRIES = 3;
    constexpr uint32_t BACNET_DEFAULT_APDU_TIMEOUT_MS = 6000;
    
    // 🔥 BACnet 배열 인덱스
    constexpr uint32_t BACNET_ARRAY_ALL = 0xFFFFFFFF;
    constexpr uint32_t BACNET_NO_ARRAY_INDEX = 0xFFFFFFFF;
    
    // 🔥 BACnet Priority 배열
    constexpr uint8_t BACNET_MIN_PRIORITY = 1;
    constexpr uint8_t BACNET_MAX_PRIORITY = 16;
    constexpr uint8_t BACNET_DEFAULT_PRIORITY = 16; // 가장 낮은 우선순위
    
    // 🔥 BACnet 세그멘테이션
    constexpr uint8_t BACNET_NO_SEGMENTATION = 0;
    constexpr uint8_t BACNET_SEGMENTED_REQUEST = 1;
    constexpr uint8_t BACNET_SEGMENTED_RESPONSE = 2;
    constexpr uint8_t BACNET_SEGMENTED_BOTH = 3;
    
    // 🔥 BACnet 타임아웃 및 재시도
    constexpr uint32_t BACNET_WHO_IS_INTERVAL_MS = 30000;    // Who-Is 브로드캐스트 간격
    constexpr uint32_t BACNET_SCAN_INTERVAL_MS = 5000;       // 스캔 간격
    constexpr uint32_t BACNET_COV_LIFETIME_SECONDS = 3600;   // COV 구독 수명 (1시간)
    constexpr uint32_t BACNET_I_AM_TIMEOUT_MS = 10000;       // I-Am 응답 대기시간
    
    // 🔥 BACnet 객체 제한값
    constexpr uint32_t BACNET_MAX_OBJECTS_PER_DEVICE = 1000;
    constexpr uint16_t BACNET_MAX_OBJECT_NAME_LENGTH = 64;
    constexpr uint16_t BACNET_MAX_DEVICE_NAME_LENGTH = 64;
    constexpr uint16_t BACNET_MAX_DESCRIPTION_LENGTH = 256;
    
    // ========================================
    // OPC-UA 프로토콜 상수 (새로 추가)
    // ========================================
    constexpr int OPCUA_DEFAULT_SESSION_TIMEOUT_MS = 120000;  // 2분
    constexpr int OPCUA_DEFAULT_KEEPALIVE_INTERVAL_MS = 30000; // 30초
    constexpr int OPCUA_MAX_NODES_PER_READ = 1000;
    constexpr int OPCUA_MAX_NODES_PER_WRITE = 100;
    constexpr int OPCUA_DEFAULT_PUBLISH_INTERVAL_MS = 1000;
    
    // ========================================
    // 시스템 제한값
    // ========================================
    constexpr int MAX_CONCURRENT_WORKERS = 50;
    constexpr int MAX_DATA_POINTS_PER_DEVICE = 10000;
    constexpr int MAX_DEVICE_NAME_LENGTH = 128;
    constexpr int MAX_ENGINEER_NAME_LENGTH = 64;
    constexpr int MAX_ERROR_MESSAGE_LENGTH = 512;
    
    // ========================================
    // 로그 및 디버그
    // ========================================
    constexpr int LOG_ROTATION_SIZE_MB = 100;
    constexpr int LOG_KEEP_DAYS = 30;
    constexpr bool DEFAULT_DEBUG_MODE = false;
    
    // ========================================
    // 로그 모듈 상수들 (기존 LogLevels.h에서 이동)
    // ========================================
    constexpr const char* LOG_MODULE_DATABASE = "database";
    constexpr const char* LOG_MODULE_SYSTEM = "system";
    constexpr const char* LOG_MODULE_CONFIG = "config";
    constexpr const char* LOG_MODULE_PLUGIN = "plugin";
    constexpr const char* LOG_MODULE_ENGINE = "engine";
    constexpr const char* LOG_MODULE_DRIVER = "driver";
    
    // ========================================
    // 문자열 상수들
    // ========================================
    constexpr const char* UNKNOWN_ENGINEER_ID = "UNKNOWN";
    constexpr const char* SYSTEM_ENGINEER_ID = "SYSTEM";
    constexpr const char* AUTO_MAINTENANCE_ID = "AUTO";
    constexpr const char* DEFAULT_DEVICE_GROUP = "Default";
    constexpr const char* DEFAULT_INTERFACE_NAME = "eth0";
    
} // namespace PulseOne::Constants

#endif // PULSEONE_COMMON_CONSTANTS_H