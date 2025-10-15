// =============================================================================
// core/export-gateway/include/Export/ExportService.h
// Export Gateway 메인 서비스 (shared 라이브러리 활용)
// =============================================================================

#ifndef EXPORT_SERVICE_H
#define EXPORT_SERVICE_H

#include "Data/DataReader.h"
#include "Data/DataWriter.h"
#include "Export/ExportTypes.h"
#include "Client/RedisClientImpl.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/MqttTargetHandler.h"
#include "CSP/S3TargetHandler.h"

#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

namespace PulseOne {
namespace Export {

/**
 * @brief Export Gateway 메인 서비스
 * 
 * 역할:
 * 1. Redis에서 실시간 데이터 읽기 (DataReader)
 * 2. 설정된 타겟으로 전송 (HTTP/MQTT/S3)
 * 3. 전송 결과 로깅 (DataWriter)
 * 4. 재시도 및 에러 처리
 */
class ExportService {
public:
    // ==========================================================================
    // 생성자 및 초기화
    // ==========================================================================
    
    ExportService();
    ~ExportService();
    
    /**
     * @brief 서비스 초기화
     * @return 성공 여부
     */
    bool Initialize();
    
    /**
     * @brief 서비스 시작
     * @return 성공 여부
     */
    bool Start();
    
    /**
     * @brief 서비스 중지
     */
    void Stop();
    
    /**
     * @brief 서비스 상태 확인
     */
    bool IsRunning() const { return is_running_.load(); }
    
    // ==========================================================================
    // Export Target 관리
    // ==========================================================================
    
    /**
     * @brief 활성화된 타겟 로드
     * @return 로드된 타겟 수
     */
    int LoadActiveTargets();
    
    /**
     * @brief 특정 타겟 활성화/비활성화
     */
    bool EnableTarget(int target_id, bool enable);
    
    /**
     * @brief 타겟 상태 업데이트
     */
    void UpdateTargetStatus(int target_id, const std::string& status);
    
    // ==========================================================================
    // 통계 및 모니터링
    // ==========================================================================
    
    struct Statistics {
        uint64_t total_exports = 0;
        uint64_t successful_exports = 0;
        uint64_t failed_exports = 0;
        uint64_t total_data_points = 0;
        std::chrono::steady_clock::time_point start_time;
    };
    
    Statistics GetStatistics() const;
    void ResetStatistics();
    
private:
    // ==========================================================================
    // 내부 메서드
    // ==========================================================================
    
    /**
     * @brief 메인 워커 스레드
     */
    void workerThread();
    
    /**
     * @brief 단일 타겟으로 데이터 전송
     */
    bool exportToTarget(const ExportTargetConfig& target,
                       const std::vector<Shared::Data::CurrentValue>& data);
    
    /**
     * @brief 전송 결과 로깅
     */
    void logExportResult(int target_id, 
                        const std::string& status,
                        int data_count,
                        const std::string& error_message = "");
    
    /**
     * @brief 재시도 처리
     */
    bool retryExport(const ExportTargetConfig& target,
                    const std::vector<Shared::Data::CurrentValue>& data,
                    int max_retries = 3);
    
    // ==========================================================================
    // 멤버 변수
    // ==========================================================================
    
    // Redis 클라이언트 및 데이터 리더/라이터
    std::shared_ptr<RedisClient> redis_client_;
    std::unique_ptr<Shared::Data::DataReader> data_reader_;
    std::unique_ptr<Shared::Data::DataWriter> data_writer_;
    
    // Target 핸들러들
    std::unique_ptr<CSP::HttpTargetHandler> http_handler_;
    std::unique_ptr<CSP::MqttTargetHandler> mqtt_handler_;
    std::unique_ptr<CSP::S3TargetHandler> s3_handler_;
    
    // Export 타겟 설정
    std::vector<ExportTargetConfig> active_targets_;
    mutable std::mutex targets_mutex_;
    
    // 워커 스레드
    std::unique_ptr<std::thread> worker_thread_;
    std::atomic<bool> is_running_{false};
    
    // 통계
    Statistics stats_;
    mutable std::mutex stats_mutex_;
    
    // 설정
    int export_interval_ms_{1000};  // 1초마다 체크
    int batch_size_{100};           // 한 번에 처리할 데이터 수
};

} // namespace Export
} // namespace PulseOne

#endif // EXPORT_SERVICE_H