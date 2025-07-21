/**
 * @file IProtocolDriver.h
 * @brief 프로토콜 드라이버 표준 인터페이스 정의
 * @details 기존 PulseOne 아키텍처와 호환되는 프로토콜 드라이버 추상 클래스
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 1.0.0
 */

#ifndef DRIVERS_IPROTOCOL_DRIVER_H
#define DRIVERS_IPROTOCOL_DRIVER_H

#include "CommonTypes.h"
#include "DriverLogger.h"
#include "Utils/ConfigManager.h"    // 기존 ConfigManager 사용
#include <functional>
#include <map>
#include <atomic>
#include <future>
#include <queue>
#include <condition_variable>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 전방 선언 및 타입 정의
// =============================================================================

class IProtocolDriver;

/**
 * @brief 콜백 함수 타입 정의
 * @details 드라이버에서 상위 시스템으로 데이터/상태 전달을 위한 콜백들
 */
using DataCallback = std::function<void(const UUID& point_id, 
                                       const TimestampedValue& value)>;
using StatusCallback = std::function<void(const UUID& device_id, 
                                        ConnectionStatus status)>;
using ErrorCallback = std::function<void(const UUID& device_id, 
                                       const ErrorInfo& error)>;
using DiagnosticsCallback = std::function<void(const UUID& device_id,
                                              const std::string& diagnostics_json)>;

// =============================================================================
// 드라이버 설정 및 통계 구조체
// =============================================================================

/**
 * @brief 드라이버 통계 정보 구조체
 * @details 성능 모니터링 및 진단을 위한 통계 데이터
 */

// =============================================================================
// 비동기 요청 구조체들
// =============================================================================

/**
 * @brief 비동기 읽기 요청 구조체
 */
struct AsyncReadRequest {
    UUID request_id;                                  ///< 요청 고유 ID
    std::vector<DataPoint> points;                    ///< 읽을 데이터 포인트들
    std::promise<std::vector<TimestampedValue>> promise; ///< 결과 promise
    Timestamp requested_at;                           ///< 요청 시간
    int priority;                                     ///< 우선순위 (높을수록 우선)
    int timeout_ms;                                   ///< 개별 타임아웃
    
    /**
     * @brief 생성자
     * @param data_points 읽을 데이터 포인트들
     * @param req_priority 우선순위 (기본값: 0)
     */
    AsyncReadRequest(const std::vector<DataPoint>& data_points, int req_priority = 0)
        : request_id(GenerateUUID())
        , points(data_points)
        , requested_at(std::chrono::system_clock::now())
        , priority(req_priority)
        , timeout_ms(Constants::DEFAULT_TIMEOUT_MS) {}
};

/**
 * @brief 비동기 쓰기 요청 구조체
 */
struct AsyncWriteRequest {
    UUID request_id;                                  ///< 요청 고유 ID
    DataPoint point;                                  ///< 쓸 데이터 포인트
    DataValue value;                                  ///< 쓸 값
    std::promise<bool> promise;                       ///< 결과 promise
    Timestamp requested_at;                           ///< 요청 시간
    int priority;                                     ///< 우선순위 (높을수록 우선)
    int timeout_ms;                                   ///< 개별 타임아웃
    
    /**
     * @brief 생성자
     * @param data_point 쓸 데이터 포인트
     * @param data_value 쓸 값
     * @param req_priority 우선순위 (기본값: 10, 쓰기는 일반적으로 높은 우선순위)
     */
    AsyncWriteRequest(const DataPoint& data_point, const DataValue& data_value, 
                     int req_priority = 10)
        : request_id(GenerateUUID())
        , point(data_point)
        , value(data_value)
        , requested_at(std::chrono::system_clock::now())
        , priority(req_priority)
        , timeout_ms(Constants::DEFAULT_TIMEOUT_MS) {}
};

// =============================================================================
// 프로토콜 드라이버 추상 인터페이스
// =============================================================================

/**
 * @brief 프로토콜 드라이버 추상 클래스
 * @details 모든 프로토콜 드라이버가 구현해야 하는 표준 인터페이스
 * 
 * 주요 설계 원칙:
 * - RAII 패턴으로 리소스 관리
 * - 스레드 안전성 보장
 * - 예외 대신 에러 코드 사용 (C 라이브러리 호환성)
 * - 비동기 처리 지원
 * - 기존 PulseOne 아키텍처와의 호환성
 */
class IProtocolDriver {
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~IProtocolDriver() = default;
    
    // =============================================================================
    // 필수 구현 메소드들 (순수 가상 함수)
    // =============================================================================
    
    /**
     * @brief 드라이버 초기화
     * @param config 드라이버 설정
     * @return 성공 시 true, 실패 시 false
     * @details 설정 검증, 내부 구조 초기화, 리소스 할당 등을 수행
     */
    virtual bool Initialize(const DriverConfig& config) = 0;
    
    /**
     * @brief 디바이스 연결
     * @return 성공 시 true, 실패 시 false
     * @details 물리적/논리적 연결을 설정하고 초기 통신 확인
     */
    virtual bool Connect() = 0;
    
    /**
     * @brief 디바이스 연결 해제
     * @return 성공 시 true, 실패 시 false
     * @details 안전한 연결 종료 및 리소스 정리
     */
    virtual bool Disconnect() = 0;
    
    /**
     * @brief 연결 상태 확인
     * @return 연결된 경우 true, 아니면 false
     * @details 실시간 연결 상태를 확인 (캐시된 값이 아님)
     */
    virtual bool IsConnected() const = 0;
    
    /**
     * @brief 다중 데이터 포인트 읽기 (동기)
     * @param points 읽을 데이터 포인트들
     * @param[out] values 읽은 값들이 저장될 벡터
     * @return 성공 시 true, 실패 시 false
     * @details 배치 읽기로 성능 최적화, 부분 실패 처리 포함
     */
    virtual bool ReadValues(const std::vector<DataPoint>& points,
                           std::vector<TimestampedValue>& values) = 0;
    
    /**
     * @brief 단일 값 쓰기 (동기)
     * @param point 쓸 데이터 포인트
     * @param value 쓸 값
     * @return 성공 시 true, 실패 시 false
     * @details 타입 변환 및 유효성 검사 포함
     */
    virtual bool WriteValue(const DataPoint& point, 
                           const DataValue& value) = 0;
    
    /**
     * @brief 프로토콜 타입 반환
     * @return 프로토콜 타입
     */
    virtual ProtocolType GetProtocolType() const = 0;
    
    /**
     * @brief 드라이버 상태 반환
     * @return 현재 드라이버 상태
     */
    virtual DriverStatus GetStatus() const = 0;
    
    /**
     * @brief 마지막 에러 정보 반환
     * @return 에러 정보
     */
    virtual ErrorInfo GetLastError() const = 0;
    
    // =============================================================================
    // 선택적 구현 메소드들 (기본 구현 제공)
    // =============================================================================
    
    /**
     * @brief 비동기 읽기 요청
     * @param points 읽을 데이터 포인트들
     * @param priority 우선순위 (기본값: 0)
     * @return future 객체
     * @details 기본 구현은 별도 스레드에서 동기 읽기 수행
     */
    virtual std::future<std::vector<TimestampedValue>> 
    ReadValuesAsync(const std::vector<DataPoint>& points, int priority = 0) {
        (void)priority;
        auto promise = std::make_shared<std::promise<std::vector<TimestampedValue>>>();
        auto future = promise->get_future();
        
        // 기본 구현: 별도 스레드에서 동기 읽기 수행
        std::thread([this, points, promise]() {
            try {
                std::vector<TimestampedValue> values;
                bool success = ReadValues(points, values);
                if (success) {
                    promise->set_value(std::move(values));
                } else {
                    promise->set_exception(std::make_exception_ptr(
                        std::runtime_error("ReadValues failed: " + GetLastError().message)));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
        return future;
    }
    
    /**
     * @brief 비동기 쓰기 요청
     * @param point 쓸 데이터 포인트
     * @param value 쓸 값
     * @param priority 우선순위 (기본값: 10)
     * @return future 객체
     */
    virtual std::future<bool> WriteValueAsync(const DataPoint& point,
                                         const DataValue& value, 
                                         int /* priority */ = 10) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();
        
        // 기본 구현: 별도 스레드에서 동기 쓰기 수행
        std::thread([this, point, value, promise]() {
            try {
                bool success = WriteValue(point, value);
                promise->set_value(success);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        
        return future;
    }
    
    /**
     * @brief 다중 값 쓰기 (동기)
     * @param points_and_values 쓸 데이터 포인트와 값들의 맵
     * @return 성공 시 true, 실패 시 false
     * @details 기본 구현은 순차적으로 WriteValue 호출
     */
    virtual bool WriteValues(const std::map<DataPoint, DataValue>& points_and_values) {
        bool all_success = true;
        for (const auto& pair : points_and_values) {
            if (!WriteValue(pair.first, pair.second)) {
                all_success = false;
                // 실패해도 계속 진행 (부분 성공 허용)
            }
        }
        return all_success;
    }
    
    /**
     * @brief 콜백 함수 등록
     */
    virtual void SetDataCallback(DataCallback callback) { 
        std::lock_guard<std::mutex> lock(callback_mutex_);
        data_callback_ = callback; 
    }
    
    virtual void SetStatusCallback(StatusCallback callback) { 
        std::lock_guard<std::mutex> lock(callback_mutex_);
        status_callback_ = callback; 
    }
    
    virtual void SetErrorCallback(ErrorCallback callback) { 
        std::lock_guard<std::mutex> lock(callback_mutex_);
        error_callback_ = callback; 
    }
    
    virtual void SetDiagnosticsCallback(DiagnosticsCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        diagnostics_callback_ = callback;
    }
    
    /**
     * @brief 통계 정보 반환
     * @return 드라이버 통계 (읽기 전용)
     */
    virtual const DriverStatistics& GetStatistics() const { 
        return statistics_; 
    }
    
    /**
     * @brief 통계 정보 초기화
     */
    virtual void ResetStatistics() {
        // statistics_.Reset(); // TODO: implement
        logger_.Info("Driver statistics reset", DriverLogCategory::GENERAL);
    }
    
    /**
     * @brief 디바이스 진단 정보 반환
     * @return 진단 정보 (JSON 형태)
     */
    virtual std::string GetDiagnosticInfo() const {
        std::ostringstream oss;
        const auto& stats = GetStatistics();
        oss << "{"
            << "\"protocol\":\"" << ProtocolTypeToString(GetProtocolType()) << "\","
            << "\"status\":\"" << static_cast<int>(GetStatus()) << "\","
            << "\"connection_status\":\"" << ConnectionStatusToString(current_connection_status_) << "\","
            << "\"statistics\":" << "{}" << ","
            << "\"uptime_seconds\":" << stats.uptime_seconds << ","
            << "\"last_error\":\"" << GetLastError().message << "\""
            << "}";
        return oss.str();
    }
    
    /**
     * @brief 설정 업데이트 (핫리로드 지원)
     * @param config 새로운 설정
     * @return 성공 시 true
     * @details 기본 구현은 재초기화
     */
    virtual bool UpdateConfig(const DriverConfig& config) {
        logger_.Info("Updating driver configuration", DriverLogCategory::GENERAL);
        
        // 연결 상태 확인 후 안전한 업데이트
        bool was_connected = IsConnected();
        if (was_connected) {
            Disconnect();
        }
        
        bool success = Initialize(config);
        
        if (success && was_connected) {
            success = Connect();
        }
        
        if (success) {
            logger_.Info("Driver configuration updated successfully", DriverLogCategory::GENERAL);
        } else {
            logger_.Error("Failed to update driver configuration", DriverLogCategory::ERROR_HANDLING);
        }
        
        return success;
    }
    
    /**
     * @brief 헬스체크 수행
     * @return 정상 시 true
     * @details 기본 구현은 연결 상태와 드라이버 상태 확인
     */
    virtual bool HealthCheck() {
        bool is_healthy = IsConnected() && 
                         (GetStatus() == DriverStatus::RUNNING ||
                          GetStatus() == DriverStatus::INITIALIZED);
        
        if (!is_healthy) {
            logger_.Warn("Health check failed: " + GetDiagnosticInfo(), 
                        DriverLogCategory::DIAGNOSTICS);
        }
        
        return is_healthy;
    }
    
    /**
     * @brief 드라이버 시작
     * @return 성공 시 true
     * @details 폴링 스레드 등 백그라운드 작업 시작
     */
    virtual bool Start() {
        if (GetStatus() != DriverStatus::INITIALIZED) {
            logger_.Error("Cannot start driver: not initialized", 
                         DriverLogCategory::ERROR_HANDLING);
            return false;
        }
        
        status_ = DriverStatus::STARTING;
        logger_.Info("Driver starting", DriverLogCategory::GENERAL);
        
        // 실제 시작 로직은 파생 클래스에서 구현
        bool success = DoStart();
        
        if (success) {
            status_ = DriverStatus::RUNNING;
            logger_.Info("Driver started successfully", DriverLogCategory::GENERAL);
        } else {
            status_ = DriverStatus::ERROR;
            logger_.Error("Driver start failed", DriverLogCategory::ERROR_HANDLING);
        }
        
        return success;
    }
    
    /**
     * @brief 드라이버 정지
     * @return 성공 시 true
     */
    virtual bool Stop() {
        if (GetStatus() != DriverStatus::RUNNING) {
            logger_.Warn("Driver is not running", DriverLogCategory::GENERAL);
            return true; // 이미 정지된 상태는 성공으로 간주
        }
        
        status_ = DriverStatus::STOPPING;
        logger_.Info("Driver stopping", DriverLogCategory::GENERAL);
        
        // 연결 해제
        Disconnect();
        
        // 실제 정지 로직은 파생 클래스에서 구현
        bool success = DoStop();
        
        if (success) {
            status_ = DriverStatus::STOPPED;
            logger_.Info("Driver stopped successfully", DriverLogCategory::GENERAL);
        } else {
            status_ = DriverStatus::ERROR;
            logger_.Error("Driver stop failed", DriverLogCategory::ERROR_HANDLING);
        }
        
        return success;
    }

protected:
    // =============================================================================
    // 파생 클래스에서 사용할 수 있는 보호된 멤버들
    // =============================================================================
    
    DriverConfig config_;                             ///< 드라이버 설정
    mutable DriverStatistics statistics_;            ///< 통계 정보
    std::atomic<DriverStatus> status_{DriverStatus::UNINITIALIZED}; ///< 드라이버 상태
    std::atomic<ConnectionStatus> current_connection_status_{ConnectionStatus::DISCONNECTED}; ///< 연결 상태
    ErrorInfo last_error_{ErrorCode::SUCCESS, "No error"}; ///< 마지막 에러 정보
    
    // 로깅 시스템 (기존 LogManager와 호환)
    mutable DriverLogger logger_;                     ///< 드라이버 전용 로거
    
    // 콜백 함수들 및 동기화
    DataCallback data_callback_;                      ///< 데이터 수신 콜백
    StatusCallback status_callback_;                  ///< 상태 변경 콜백
    ErrorCallback error_callback_;                    ///< 에러 발생 콜백
    DiagnosticsCallback diagnostics_callback_;        ///< 진단 정보 콜백
    mutable std::mutex callback_mutex_;               ///< 콜백 동기화 뮤텍스
    
    // =============================================================================
    // 파생 클래스가 구현해야 하는 보호된 가상 함수들
    // =============================================================================
    
    /**
     * @brief 실제 시작 로직 구현 (파생 클래스에서 오버라이드)
     * @return 성공 시 true
     */
    virtual bool DoStart() { return true; }
    
    /**
     * @brief 실제 정지 로직 구현 (파생 클래스에서 오버라이드)
     * @return 성공 시 true
     */
    virtual bool DoStop() { return true; }
    
    // =============================================================================
    // 파생 클래스에서 사용할 수 있는 유틸리티 함수들
    // =============================================================================
    
    /**
     * @brief 데이터 수신 알림
     * @param point_id 데이터 포인트 ID
     * @param value 수신된 값
     */
    void NotifyDataReceived(const UUID& point_id, const TimestampedValue& value) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (data_callback_) {
            try {
                data_callback_(point_id, value);
            } catch (const std::exception& e) {
                logger_.Error("Data callback exception: " + std::string(e.what()),
                             DriverLogCategory::ERROR_HANDLING);
            }
        }
    }
    
    /**
     * @brief 상태 변경 알림
     * @param device_id 디바이스 ID
     * @param status 새로운 상태
     */
    void NotifyStatusChanged(const UUID& device_id, ConnectionStatus status) {
        // 상태가 실제로 변경된 경우만 알림
        ConnectionStatus old_status = current_connection_status_.exchange(status);
        if (old_status != status) {
            logger_.LogConnectionStatusChange(old_status, status);
            
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (status_callback_) {
                try {
                    status_callback_(device_id, status);
                } catch (const std::exception& e) {
                    logger_.Error("Status callback exception: " + std::string(e.what()),
                                 DriverLogCategory::ERROR_HANDLING);
                }
            }
        }
    }
    
    /**
     * @brief 에러 발생 알림
     * @param device_id 디바이스 ID
     * @param error 에러 정보
     */
    void NotifyError(const UUID& device_id, const ErrorInfo& error) {
        last_error_ = error;
        statistics_.last_error_time = error.occurred_at;
        
        // 로그 기록
        logger_.LogError(error);
        
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (error_callback_) {
            try {
                error_callback_(device_id, error);
            } catch (const std::exception& e) {
                logger_.Error("Error callback exception: " + std::string(e.what()),
                             DriverLogCategory::ERROR_HANDLING);
            }
        }
    }
    
    /**
     * @brief 진단 정보 알림
     * @param device_id 디바이스 ID
     * @param diagnostics_json 진단 정보 (JSON 형태)
     */
    void NotifyDiagnostics(const UUID& device_id, const std::string& diagnostics_json) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (diagnostics_callback_) {
            try {
                diagnostics_callback_(device_id, diagnostics_json);
            } catch (const std::exception& e) {
                logger_.Error("Diagnostics callback exception: " + std::string(e.what()),
                             DriverLogCategory::ERROR_HANDLING);
            }
        }
    }
    
    /**
     * @brief 응답 시간 업데이트 (성능 통계용)
     * @param response_time 응답 시간
     */
    //     void UpdateResponseTime(std::chrono::milliseconds response_time) {
    //         uint32_t response_ms = static_cast<uint32_t>(response_time.count());
    //         
    //         // 이동 평균으로 평균 응답 시간 업데이트 (90% 이전값, 10% 새값)
    //         uint32_t current_avg = statistics_.avg_response_time_ms;
    //         uint32_t new_avg = (current_avg * 9 + response_ms) / 10;
    //         statistics_.avg_response_time_ms.store(new_avg);
    //         
    //         // 최소/최대 응답 시간 업데이트
    //         uint32_t current_min = statistics_.min_response_time_ms;
    //         while (response_ms < current_min) {
    //             if (statistics_.min_response_time_ms.compare_exchange_weak(current_min, response_ms)) {
    //                 break;
    //             }
    //         }
    //         
    //         uint32_t current_max = statistics_.max_response_time_ms;
    //         while (response_ms > current_max) {
    //             if (statistics_.max_response_time_ms.compare_exchange_weak(current_max, response_ms)) {
    //                 break;
    //             }
    //         }
    //     }
    
    /**
     * @brief 바이트 통계 업데이트
     * @param bytes_sent 송신 바이트 수
     * @param bytes_received 수신 바이트 수
     */
    //     void UpdateByteCounters(uint64_t bytes_sent, uint64_t bytes_received) {
    //         statistics_.total_bytes_sent.fetch_add(bytes_sent);
    //         statistics_.total_bytes_received.fetch_add(bytes_received);
    //     }
};

// =============================================================================
// 드라이버 생성 함수 타입 (팩토리 패턴용)
// =============================================================================

/**
 * @brief 드라이버 생성 함수 타입
 * @details 팩토리 패턴에서 사용할 함수 포인터 타입
 */
using DriverCreatorFunc = std::function<std::unique_ptr<IProtocolDriver>()>;

} // namespace Drivers
} // namespace PulseOne

#endif // DRIVERS_IPROTOCOL_DRIVER_H