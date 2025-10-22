/**
 * @file PeriodicExporter.h
 * @brief 주기적 데이터 스캔 및 전송 클래스
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
 * 
 * 기능:
 * - Redis에서 device:{num}:* 패턴 주기적 스캔
 * - 설정된 주기마다 데이터 읽기
 * - 배치로 묶어서 DynamicTargetManager로 전송
 * - 멀티스레드 안전
 * 
 * 저장 위치: core/export-gateway/include/Periodic/PeriodicExporter.h
 */

#ifndef PERIODIC_EXPORTER_H
#define PERIODIC_EXPORTER_H

#include "Client/RedisClient.h"
#include "CSP/DynamicTargetManager.h"
#include "CSP/AlarmMessage.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace Periodic {

// =============================================================================
// 설정 구조체
// =============================================================================

struct PeriodicExporterConfig {
    // Redis 연결 정보
    std::string redis_host = "localhost";
    int redis_port = 6379;
    std::string redis_password = "";
    
    // 스캔 설정
    int export_interval_seconds = 60;      // 전송 주기 (초)
    int batch_size = 100;                  // 배치 크기
    std::vector<int> device_numbers;       // 스캔할 디바이스 번호들
    
    // 타겟 설정
    bool use_dynamic_targets = true;
    std::string target_config_file = "targets.json";
    
    // 로깅
    bool enable_debug_log = false;
};

// =============================================================================
// 데이터 포인트 구조체
// =============================================================================

struct DataPoint {
    int point_id = 0;
    std::string point_name;
    std::string value;
    int64_t timestamp = 0;
    int quality = 0;
    int device_num = 0;
    
    // JSON 변환
    json to_json() const {
        return json{
            {"point_id", point_id},
            {"point_name", point_name},
            {"value", value},
            {"timestamp", timestamp},
            {"quality", quality},
            {"device_num", device_num}
        };
    }
    
    // AlarmMessage로 변환 (호환성)
    PulseOne::CSP::AlarmMessage toAlarmMessage() const {
        PulseOne::CSP::AlarmMessage msg;
        msg.bd = device_num;
        msg.nm = point_name;
        
        // value를 double로 변환 시도
        try {
            msg.vl = std::stod(value);
        } catch (...) {
            msg.vl = 0.0;
        }
        
        // timestamp는 문자열로 변환
        msg.tm = std::to_string(timestamp);
        msg.al = 0;  // 데이터 전송이므로 알람 아님
        msg.st = 0;
        msg.des = "Periodic data export";
        
        return msg;
    }
};

// =============================================================================
// 전송 결과 구조체
// =============================================================================

struct ExportResult {
    bool success = false;
    size_t total_points = 0;
    size_t exported_points = 0;
    size_t failed_points = 0;
    std::chrono::milliseconds total_time{0};
    std::string error_message;
    std::vector<std::string> failed_point_names;
};

// =============================================================================
// PeriodicExporter 클래스
// =============================================================================

class PeriodicExporter {
public:
    // =========================================================================
    // 생성자 및 소멸자
    // =========================================================================
    
    explicit PeriodicExporter(const PeriodicExporterConfig& config);
    ~PeriodicExporter();
    
    // 복사/이동 방지
    PeriodicExporter(const PeriodicExporter&) = delete;
    PeriodicExporter& operator=(const PeriodicExporter&) = delete;
    PeriodicExporter(PeriodicExporter&&) = delete;
    PeriodicExporter& operator=(PeriodicExporter&&) = delete;
    
    // =========================================================================
    // 라이프사이클 관리
    // =========================================================================
    
    /**
     * @brief 주기적 스캔 시작
     * @return 성공 시 true
     */
    bool start();
    
    /**
     * @brief 주기적 스캔 중지
     */
    void stop();
    
    /**
     * @brief 실행 중 여부
     * @return 실행 중이면 true
     */
    bool isRunning() const { return is_running_.load(); }
    
    // =========================================================================
    // 수동 트리거
    // =========================================================================
    
    /**
     * @brief 즉시 스캔 및 전송 (수동 트리거)
     * @return 전송 결과
     */
    ExportResult triggerExportNow();
    
    /**
     * @brief 특정 디바이스만 스캔 및 전송
     * @param device_num 디바이스 번호
     * @return 전송 결과
     */
    ExportResult exportDevice(int device_num);
    
    // =========================================================================
    // 설정 관리
    // =========================================================================
    
    /**
     * @brief 전송 주기 변경
     * @param seconds 주기 (초)
     */
    void setExportInterval(int seconds);
    
    /**
     * @brief 배치 크기 변경
     * @param size 배치 크기
     */
    void setBatchSize(int size);
    
    /**
     * @brief 스캔할 디바이스 목록 설정
     * @param device_nums 디바이스 번호 목록
     */
    void setDeviceNumbers(const std::vector<int>& device_nums);
    
    /**
     * @brief 디바이스 추가
     * @param device_num 디바이스 번호
     */
    void addDevice(int device_num);
    
    /**
     * @brief 디바이스 제거
     * @param device_num 디바이스 번호
     */
    void removeDevice(int device_num);
    
    // =========================================================================
    // 통계
    // =========================================================================
    
    /**
     * @brief 통계 조회
     * @return 통계 정보 JSON
     */
    json getStatistics() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStatistics();
    
private:
    // =========================================================================
    // 내부 메서드
    // =========================================================================
    
    /**
     * @brief 주기적 스캔 루프 (워커 스레드)
     */
    void periodicScanLoop();
    
    /**
     * @brief 디바이스 데이터 스캔
     * @param device_num 디바이스 번호
     * @return 데이터 포인트 목록
     */
    std::vector<DataPoint> scanDeviceData(int device_num);
    
    /**
     * @brief 배치 생성
     * @param points 데이터 포인트 목록
     * @return 배치 목록
     */
    std::vector<std::vector<DataPoint>> createBatches(const std::vector<DataPoint>& points);
    
    /**
     * @brief 배치 전송
     * @param batch 배치
     * @return 성공한 포인트 수
     */
    size_t exportBatch(const std::vector<DataPoint>& batch);
    
    /**
     * @brief Redis 연결 초기화
     * @return 성공 시 true
     */
    bool initializeRedisConnection();
    
    /**
     * @brief DynamicTargetManager 초기화
     * @return 성공 시 true
     */
    bool initializeDynamicTargetManager();
    
    // =========================================================================
    // 멤버 변수
    // =========================================================================
    
    // 설정
    PeriodicExporterConfig config_;
    
    // Redis 클라이언트
    std::shared_ptr<RedisClient> redis_client_;
    
    // DynamicTargetManager
    std::unique_ptr<PulseOne::CSP::DynamicTargetManager> dynamic_target_manager_;
    
    // 스레드 관리
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    
    // 동기화
    mutable std::mutex config_mutex_;
    mutable std::mutex stats_mutex_;
    
    // 통계
    std::atomic<size_t> total_scans_{0};
    std::atomic<size_t> successful_scans_{0};
    std::atomic<size_t> failed_scans_{0};
    std::atomic<size_t> total_points_scanned_{0};
    std::atomic<size_t> total_points_exported_{0};
    std::atomic<size_t> total_points_failed_{0};
    std::atomic<int64_t> last_scan_timestamp_{0};
    std::atomic<int64_t> total_scan_time_ms_{0};
};

} // namespace Periodic
} // namespace PulseOne

#endif // PERIODIC_EXPORTER_H