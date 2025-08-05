// collector/include/Drivers/Common/IProtocolDriver.h - Phase 1 업데이트
#ifndef PULSEONE_IPROTOCOL_DRIVER_H
#define PULSEONE_IPROTOCOL_DRIVER_H

/**
 * @file IProtocolDriver.h
 * @brief 통합 프로토콜 드라이버 인터페이스 (Phase 1)
 * @author PulseOne Development Team
 * @date 2025-08-05
 * 
 * 🎯 Phase 1 주요 변경사항:
 * - 스마트 포인터 기반 DriverConfig 사용
 * - 통합 DeviceInfo + DataPoint 구조체 사용
 * - 간소화된 콜백 시스템 (상태/에러만)
 * - Worker 주도 데이터 수집 지원
 */

// 🔥 개별 헤더 include (순환참조 방지)
#include "Common/BasicTypes.h"
#include "Common/Enums.h"
#include "Common/Structs.h"              // 통합 구조체들
#include "Common/DriverStatistics.h"     // 표준 통계
#include <functional>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace PulseOne::Drivers {
    
    // =======================================================================
    // 🔥 기존 코드 호환성을 위한 타입 별칭들
    // =======================================================================
    
    using DriverConfig = Structs::DriverConfig;           // 기존 타입 유지
    using TimestampedValue = Structs::TimestampedValue;   // 기존 타입 유지
    using ErrorInfo = Structs::ErrorInfo;                 // 기존 타입 유지
    
    // =======================================================================
    // 🔥 간소화된 콜백 함수 타입들 (데이터 콜백 제거)
    // =======================================================================
    
    using StatusCallback = std::function<void(
        const UUID& device_id,
        ConnectionStatus old_status,
        ConnectionStatus new_status
    )>;
    
    using ErrorCallback = std::function<void(
        const UUID& device_id,
        const Structs::ErrorInfo& error
    )>;
    
    // =======================================================================
    // 🔥 통합 프로토콜 드라이버 인터페이스
    // =======================================================================
    
    /**
     * @brief 통합 프로토콜 드라이버 인터페이스
     * @details 
     * - 스마트 포인터 기반 DriverConfig 사용
     * - 통합 구조체 (DeviceInfo, DataPoint) 사용
     * - Worker 주도 데이터 수집 지원
     * - 간소화된 콜백 시스템
     */
    class IProtocolDriver {
    protected:
        // =======================================================================
        // 🔥 Driver 전용: 통신 통계 및 상태만
        // =======================================================================
        DriverStatistics statistics_;                  // 통신/프로토콜 통계 (Driver 소유)
        
        // =======================================================================
        // 🔥 간소화된 콜백들 (상태/에러만)
        // =======================================================================
        StatusCallback status_callback_;
        ErrorCallback error_callback_;
        mutable std::mutex callback_mutex_;
        
        // =======================================================================
        // 🔥 내부 상태 (연결 상태만)
        // =======================================================================
        std::atomic<ConnectionStatus> connection_status_{ConnectionStatus::DISCONNECTED};
        std::atomic<bool> is_running_{false};
        Structs::ErrorInfo last_error_;
        
    public:
        // =======================================================================
        // 🔥 생성자 및 소멸자
        // =======================================================================
        
        IProtocolDriver() : statistics_("UNKNOWN") {}
        virtual ~IProtocolDriver() = default;
        
        // =======================================================================
        // 🔥 필수 구현 메서드들 (순수 가상 함수)
        // =======================================================================
        
        /**
         * @brief 드라이버 초기화 (기존 DriverConfig 매개변수 타입 유지)
         */
        virtual bool Initialize(const DriverConfig& config) = 0;
        
        /**
         * @brief 디바이스 연결
         */
        virtual bool Connect() = 0;
        
        /**
         * @brief 디바이스 연결 해제
         */
        virtual bool Disconnect() = 0;
        
        /**
         * @brief 연결 상태 확인
         */
        virtual bool IsConnected() const = 0;
        
        /**
         * @brief 데이터 포인트들 읽기 (기존 시그니처 유지)
         */
        virtual bool ReadValues(const std::vector<Structs::DataPoint>& points,
                               std::vector<TimestampedValue>& values) = 0;
        
        /**
         * @brief 단일 값 쓰기 (기존 시그니처 유지)
         */
        virtual bool WriteValue(const Structs::DataPoint& point,
                               const Structs::DataValue& value) = 0;
        
        /**
         * @brief 프로토콜 타입 반환 (기존 시그니처 유지)
         */
        virtual ProtocolType GetProtocolType() const = 0;
        
        /**
         * @brief 드라이버 상태 반환 (기존 시그니처 유지)
         */
        virtual Structs::DriverStatus GetStatus() const = 0;
        
        /**
         * @brief 마지막 에러 정보 반환 (기존 시그니처 유지)
         */
        virtual ErrorInfo GetLastError() const = 0;
        
        // =======================================================================
        // 🔥 기존 코드 호환성을 위한 추가 메서드들
        // =======================================================================
        
        /**
         * @brief 통계 초기화 (기존 메서드 유지)
         */
        virtual void ResetStatistics() {
            statistics_.Reset();
        }
        
        /**
         * @brief 드라이버 시작 (기존 메서드 유지)
         */
        virtual bool Start() {
            return Connect();
        }
        
        /**
         * @brief 드라이버 정지 (기존 메서드 유지)
         */
        virtual bool Stop() {
            return Disconnect();
        }
        
        // =======================================================================
        // 🔥 Driver 전용 메서드들 (통신 통계만)
        // =======================================================================
        
        /**
         * @brief 프로토콜 이름 설정 (초기화 시 Worker가 호출)
         */
        virtual void SetProtocolName(const std::string& name) {
            statistics_.SetProtocolName(name);
        }
        
        /**
         * @brief 통계 정보 조회 (통신 통계만)
         */
        virtual const DriverStatistics& GetStatistics() const {
            return statistics_;
        }
        
        /**
         * @brief 연결 상태 조회
         */
        virtual ConnectionStatus GetConnectionStatus() const {
            return connection_status_.load();
        }
        
        /**
         * @brief 마지막 에러 조회
         */
        virtual const Structs::ErrorInfo& GetLastError() const {
            return last_error_;
        }
        
        /**
         * @brief 상태 변경 콜백 설정
         */
        virtual void SetStatusCallback(const StatusCallback& callback) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            status_callback_ = callback;
        }
        
        /**
         * @brief 에러 콜백 설정
         */
        virtual void SetErrorCallback(const ErrorCallback& callback) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            error_callback_ = callback;
        }
        
        // =======================================================================
        // 🔥 기존 코드 호환성 메서드들 (설정 정보는 Worker에서 받아 사용)
        // =======================================================================
        
        /**
         * @brief 드라이버 유효성 검증 (연결 설정만 확인)
         */
        virtual bool IsValid() const {
            return connection_status_.load() != ConnectionStatus::DISCONNECTED;
        }
        
    protected:
        // =======================================================================
        // 🔥 내부 헬퍼 메서드들
        // =======================================================================
        
        /**
         * @brief 상태 변경 알림 (Worker에게)
         */
        void NotifyStatusChanged(ConnectionStatus new_status) {
            ConnectionStatus old_status = connection_status_.exchange(new_status);
            
            if (old_status != new_status) {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (status_callback_) {
                    // Worker에서 설정한 device_id로 알림 (Driver는 device_id 모름)
                    status_callback_(UUID{}, old_status, new_status);
                }
            }
        }
        
        /**
         * @brief 에러 알림 (Worker에게)
         */
        void NotifyError(const Structs::ErrorInfo& error) {
            last_error_ = error;
            statistics_.connection_errors.fetch_add(1);
            
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (error_callback_) {
                // Worker에서 설정한 device_id로 알림 (Driver는 device_id 모름)
                error_callback_(UUID{}, error);
            }
        }
        
        /**
         * @brief 통계 업데이트 (읽기 작업)
         */
        void UpdateReadStatistics(bool success, size_t points_count, 
                                 std::chrono::milliseconds duration) {
            statistics_.total_reads.fetch_add(1);
            
            if (success) {
                statistics_.successful_reads.fetch_add(1);
                statistics_.total_points_read.fetch_add(points_count);
            } else {
                statistics_.failed_reads.fetch_add(1);
            }
            
            // 평균 응답 시간 업데이트
            statistics_.SetProtocolMetric("avg_response_time_ms", 
                                        static_cast<double>(duration.count()));
        }
        
        /**
         * @brief 통계 업데이트 (쓰기 작업)
         */
        void UpdateWriteStatistics(bool success, std::chrono::milliseconds duration) {
            statistics_.total_writes.fetch_add(1);
            
            if (success) {
                statistics_.successful_writes.fetch_add(1);
            } else {
                statistics_.failed_writes.fetch_add(1);
            }
        }
    };
    
    // =======================================================================
    // 🔥 기존 타입 호환성 (typedef)
    // =======================================================================
    
    // 기존 코드에서 사용하던 타입들을 그대로 사용 가능
    using DataPoint = Structs::DataPoint;
    using TimestampedValue = Structs::TimestampedValue;
    using DeviceInfo = Structs::DeviceInfo;
    using DriverConfig = Structs::DriverConfig;
}

#endif // PULSEONE_IPROTOCOL_DRIVER_H