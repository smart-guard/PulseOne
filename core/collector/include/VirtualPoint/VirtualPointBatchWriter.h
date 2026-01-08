// =============================================================================
// collector/include/VirtualPoint/VirtualPointBatchWriter.h
// 🔥 컴파일 에러 수정 완성본
// =============================================================================

#ifndef PULSEONE_VIRTUAL_POINT_BATCH_WRITER_H
#define PULSEONE_VIRTUAL_POINT_BATCH_WRITER_H

#include "Common/Structs.h"
#include "DatabaseManager.hpp"  // 🔥 수정: 전체 include
#include "Logging/LogManager.h"
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace VirtualPoint {

/**
 * @brief 🔥 성능 최적화된 가상포인트 배치 저장 시스템
 * @details 가상포인트 계산 결과를 비동기 배치로 DB에 저장하여 파이프라인 성능 최적화
 */
class VirtualPointBatchWriter {
public:
    // ==========================================================================
    // 가상포인트 결과 구조체
    // ==========================================================================
    
    struct VPResult {
        int vp_id;                                          // 가상포인트 ID
        double value;                                       // 계산된 값
        std::string quality;                               // 품질 상태 ("good", "bad", "uncertain")
        std::chrono::system_clock::time_point timestamp;   // 계산 시각
        std::string device_id;                             // 관련 디바이스 ID (선택)
        std::string execution_info;                        // 실행 정보 (선택)
        
        VPResult() = default;
        
        VPResult(int id, double val, const std::string& qual = "good")
            : vp_id(id), value(val), quality(qual), timestamp(std::chrono::system_clock::now()) {}
    };
    
    // ==========================================================================
    // 🔥 수정: std::atomic 복사 문제 해결된 통계 구조체
    // ==========================================================================
    
    struct BatchWriterStatistics {
        std::atomic<size_t> total_queued{0};              // 총 큐에 추가된 항목 수
        std::atomic<size_t> total_written{0};             // 총 DB에 저장된 항목 수
        std::atomic<size_t> total_batches{0};             // 총 배치 처리 횟수
        std::atomic<size_t> failed_writes{0};             // 실패한 쓰기 횟수
        std::atomic<size_t> current_queue_size{0};        // 현재 큐 크기
        std::atomic<double> avg_batch_size{0.0};          // 평균 배치 크기
        std::atomic<double> avg_write_time_ms{0.0};       // 평균 쓰기 시간 (ms)
        std::chrono::system_clock::time_point last_write; // 마지막 쓰기 시각
        
        BatchWriterStatistics() {
            last_write = std::chrono::system_clock::now();
        }
        
        // 🔥 수정: 복사/이동 생성자 명시적 정의
        BatchWriterStatistics(const BatchWriterStatistics& other) 
            : total_queued(other.total_queued.load())
            , total_written(other.total_written.load())
            , total_batches(other.total_batches.load())
            , failed_writes(other.failed_writes.load())
            , current_queue_size(other.current_queue_size.load())
            , avg_batch_size(other.avg_batch_size.load())
            , avg_write_time_ms(other.avg_write_time_ms.load())
            , last_write(other.last_write) {}
        
        BatchWriterStatistics& operator=(const BatchWriterStatistics& other) {
            if (this != &other) {
                total_queued.store(other.total_queued.load());
                total_written.store(other.total_written.load());
                total_batches.store(other.total_batches.load());
                failed_writes.store(other.failed_writes.load());
                current_queue_size.store(other.current_queue_size.load());
                avg_batch_size.store(other.avg_batch_size.load());
                avg_write_time_ms.store(other.avg_write_time_ms.load());
                last_write = other.last_write;
            }
            return *this;
        }
        
        // 🔥 추가: 통계 리셋 메서드
        void reset() {
            total_queued.store(0);
            total_written.store(0);
            total_batches.store(0);
            failed_writes.store(0);
            current_queue_size.store(0);
            avg_batch_size.store(0.0);
            avg_write_time_ms.store(0.0);
            last_write = std::chrono::system_clock::now();
        }
    };

private:
    // ==========================================================================
    // 설정 상수들
    // ==========================================================================
    
    static constexpr size_t DEFAULT_BATCH_SIZE = 100;                    // 기본 배치 크기
    static constexpr std::chrono::seconds DEFAULT_FLUSH_INTERVAL{30};    // 기본 플러시 간격 (30초)
    static constexpr size_t MAX_BATCH_SIZE = 500;                        // 최대 배치 크기
    static constexpr size_t MAX_QUEUE_SIZE = 10000;                      // 최대 큐 크기
    static constexpr std::chrono::milliseconds MAX_FLUSH_INTERVAL{300000}; // 최대 플러시 간격 (5분)

    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 큐 및 스레드 관리
    std::queue<VPResult> pending_results_;              // 대기 중인 결과 큐
    mutable std::mutex queue_mutex_;                    // 큐 보호 뮤텍스
    std::thread batch_writer_thread_;                   // 배치 처리 스레드
    std::atomic<bool> should_stop_{false};             // 중지 플래그
    std::condition_variable cv_;                        // 조건 변수
    
    // 설정
    size_t batch_size_;                                 // 배치 크기
    std::chrono::seconds flush_interval_;               // 플러시 간격
    
    // 통계
    mutable std::mutex stats_mutex_;                    // 통계 보호 뮤텍스
    BatchWriterStatistics statistics_;                  // 성능 통계
    
    // 🔥 수정: DbLib::DatabaseManager 포인터 타입 수정
    DbLib::DatabaseManager* db_manager_;                       // DB 매니저 (non-owning)

public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    explicit VirtualPointBatchWriter(
        size_t batch_size = DEFAULT_BATCH_SIZE,
        int flush_interval_sec = DEFAULT_FLUSH_INTERVAL.count()
    );
    
    ~VirtualPointBatchWriter();
    
    // 복사/이동 생성자 금지 (리소스 관리 복잡성 방지)
    VirtualPointBatchWriter(const VirtualPointBatchWriter&) = delete;
    VirtualPointBatchWriter& operator=(const VirtualPointBatchWriter&) = delete;
    VirtualPointBatchWriter(VirtualPointBatchWriter&&) = delete;
    VirtualPointBatchWriter& operator=(VirtualPointBatchWriter&&) = delete;
    
    // ==========================================================================
    // 라이프사이클 관리
    // ==========================================================================
    
    bool Start();
    void Stop();
    bool IsRunning() const;
    
    // ==========================================================================
    // 데이터 처리 인터페이스
    // ==========================================================================
    
    bool QueueVirtualPointResult(const Structs::TimestampedValue& vp_result);
    bool QueueResult(const VPResult& vp_result);
    size_t QueueResults(const std::vector<VPResult>& results);
    bool FlushNow(bool wait_for_completion = false);
    
    // ==========================================================================
    // 상태 및 통계 조회
    // ==========================================================================
    
    size_t GetQueueSize() const;
    BatchWriterStatistics GetStatistics() const;
    nlohmann::json GetStatisticsJson() const;
    void ResetStatistics();
    
    // ==========================================================================
    // 설정 관리
    // ==========================================================================
    
    void SetBatchSize(size_t new_batch_size);
    void SetFlushInterval(int new_interval_sec);
    std::pair<size_t, std::chrono::seconds> GetCurrentSettings() const;

private:
    // ==========================================================================
    // 내부 구현 메서드들
    // ==========================================================================
    
    void BatchWriterLoop();
    bool WriteBatchToDatabase(const std::vector<VPResult>& batch);
    VPResult ConvertTimestampedValue(const Structs::TimestampedValue& ts_value);
    std::vector<VPResult> ExtractBatch(size_t max_items);
    void UpdateStatistics(size_t batch_size, double write_time_ms, bool success);
    bool ValidateSettings(size_t batch_size, const std::chrono::seconds& flush_interval) const;
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // PULSEONE_VIRTUAL_POINT_BATCH_WRITER_H