// collector/include/Drivers/Common/UnifiedDriverBase.h
#ifndef PULSEONE_UNIFIED_DRIVER_BASE_H
#define PULSEONE_UNIFIED_DRIVER_BASE_H

/**
 * @file UnifiedDriverBase.h
 * @brief PulseOne 통합 드라이버 베이스 클래스 (혁신의 핵심!)
 * @author PulseOne Development Team
 * @date 2025-08-04
 * @details
 * 🎯 목적: **모든 드라이버가 동일한 패턴을 사용하는 통합 기반 클래스**
 * 📋 기존 문제점:
 * - ModbusDriver, MqttDriver, BACnetDriver 각각 다른 패턴
 * - 개별 통계 구조체, 개별 에러 처리, 개별 데이터 파이프라인
 * - 새 프로토콜 추가 시 모든 것을 다시 구현해야 함
 * 
 * ✅ 해결책: UnifiedDriverBase 하나로 모든 공통 로직 제공!
 * - 통일된 통계 시스템
 * - 통일된 데이터 파이프라인  
 * - 통일된 에러 처리
 * - 프로토콜별 특화는 단 2개 메서드만 구현하면 됨!
 */

#include "Common/UnifiedTypes.h"
#include "Common/DriverStatistics.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace PulseOne {
namespace Drivers {

    // 기본 타입들 import
    using namespace PulseOne::Common;
    using namespace PulseOne::Enums;

    // =========================================================================
    // 🔥 핵심! 통합 드라이버 베이스 클래스
    // =========================================================================
    
    /**
     * @brief 모든 프로토콜 드라이버의 통합 베이스 클래스
     * @details 
     * ✅ 모든 드라이버가 동일한 인터페이스 사용
     * ✅ 공통 로직은 베이스 클래스에서 처리
     * ✅ 프로토콜별 특화는 단 2개 메서드만 오버라이드
     * ✅ 통일된 데이터 파이프라인 자동 연결
     * ✅ 통일된 통계 및 로그 시스템
     */
    class UnifiedDriverBase {
    public:
        // ===== 콜백 함수 타입들 =====
        using DataCallback = std::function<void(const UnifiedDeviceInfo&, 
                                               const std::vector<UnifiedDataPoint>&)>;
        using StatusCallback = std::function<void(const UUID&, ConnectionStatus)>;
        using ErrorCallback = std::function<void(const UUID&, const std::string&)>;

    protected:
        // ===== 보호된 멤버 변수들 (파생 클래스에서 접근 가능) =====
        UnifiedDeviceInfo device_info_;                 // 디바이스 정보
        std::vector<UnifiedDataPoint> data_points_;     // 데이터 포인트들
        
        // 상태 관리
        std::atomic<DriverStatus> status_{DriverStatus::UNINITIALIZED};
        std::atomic<ConnectionStatus> connection_status_{ConnectionStatus::DISCONNECTED};
        std::atomic<bool> running_{false};
        std::atomic<bool> stop_requested_{false};
        
        // 통합 통계 시스템 (모든 드라이버 동일!)
        mutable PulseOne::Structs::DriverStatistics statistics_;
        
        // 스레드 관리
        std::unique_ptr<std::thread> worker_thread_;
        std::mutex thread_mutex_;
        std::condition_variable thread_cv_;
        
        // 콜백 관리
        DataCallback data_callback_;
        StatusCallback status_callback_;
        ErrorCallback error_callback_;
        std::mutex callback_mutex_;
        
        // 로깅 시스템 (기존 LogManager 활용)
        LogManager& logger_;
        ConfigManager& config_;

    public:
        // ===== 생성자 및 소멸자 =====
        
        /**
         * @brief 통합 드라이버 베이스 생성자
         * @param protocol_name 프로토콜 이름 (통계용)
         */
        explicit UnifiedDriverBase(const std::string& protocol_name)
            : statistics_(protocol_name)
            , logger_(LogManager::getInstance())
            , config_(ConfigManager::getInstance()) {
            
            logger_.Info("UnifiedDriverBase created for protocol: " + protocol_name);
        }
        
        /**
         * @brief 가상 소멸자
         */
        virtual ~UnifiedDriverBase() {
            Stop();
            if (worker_thread_ && worker_thread_->joinable()) {
                worker_thread_->join();
            }
            logger_.Info("UnifiedDriverBase destroyed");
        }

        // ===== 통합 공개 인터페이스 (모든 드라이버 동일!) =====
        
        /**
         * @brief 드라이버 초기화
         * @param device 통합 디바이스 정보
         * @return 성공 시 true
         */
        bool Initialize(const UnifiedDeviceInfo& device) {
            std::lock_guard<std::mutex> lock(thread_mutex_);
            
            logger_.Info("Initializing driver for device: " + device.name);
            
            device_info_ = device;
            status_ = DriverStatus::INITIALIZING;
            
            try {
                // 프로토콜별 초기화 호출
                if (InitializeProtocol()) {
                    status_ = DriverStatus::INITIALIZED;
                    logger_.Info("Driver initialized successfully");
                    return true;
                } else {
                    status_ = DriverStatus::ERROR;
                    logger_.Error("Protocol initialization failed");
                    return false;
                }
            } catch (const std::exception& e) {
                status_ = DriverStatus::ERROR;
                logger_.Error("Driver initialization exception: " + std::string(e.what()));
                return false;
            }
        }
        
        /**
         * @brief 드라이버 시작
         * @return 성공 시 true
         */
        bool Start() {
            std::lock_guard<std::mutex> lock(thread_mutex_);
            
            if (status_ != DriverStatus::INITIALIZED && status_ != DriverStatus::STOPPED) {
                logger_.Error("Cannot start driver - invalid state");
                return false;
            }
            
            logger_.Info("Starting driver...");
            status_ = DriverStatus::STARTING;
            stop_requested_ = false;
            
            try {
                // 워커 스레드 시작
                worker_thread_ = std::make_unique<std::thread>(&UnifiedDriverBase::WorkerThreadFunc, this);
                
                status_ = DriverStatus::RUNNING;
                running_ = true;
                
                logger_.Info("Driver started successfully");
                NotifyStatusChange(ConnectionStatus::CONNECTING);
                
                return true;
            } catch (const std::exception& e) {
                status_ = DriverStatus::ERROR;
                logger_.Error("Driver start exception: " + std::string(e.what()));
                return false;
            }
        }
        
        /**
         * @brief 드라이버 정지
         * @return 성공 시 true
         */
        bool Stop() {
            std::unique_lock<std::mutex> lock(thread_mutex_);
            
            if (!running_) {
                return true;  // 이미 정지됨
            }
            
            logger_.Info("Stopping driver...");
            status_ = DriverStatus::STOPPING;
            stop_requested_ = true;
            running_ = false;
            
            // 워커 스레드 깨우기
            thread_cv_.notify_all();
            lock.unlock();
            
            // 스레드 종료 대기
            if (worker_thread_ && worker_thread_->joinable()) {
                worker_thread_->join();
            }
            
            // 프로토콜별 정리 호출
            CleanupProtocol();
            
            status_ = DriverStatus::STOPPED;
            connection_status_ = ConnectionStatus::DISCONNECTED;
            
            logger_.Info("Driver stopped successfully");
            NotifyStatusChange(ConnectionStatus::DISCONNECTED);
            
            return true;
        }
        
        /**
         * @brief 데이터 포인트 추가/업데이트
         * @param points 데이터 포인트들
         */
        void UpdateDataPoints(const std::vector<UnifiedDataPoint>& points) {
            std::lock_guard<std::mutex> lock(thread_mutex_);
            data_points_ = points;
            logger_.Info("Updated " + std::to_string(points.size()) + " data points");
        }
        
        /**
         * @brief 현재 상태 반환
         */
        DriverStatus GetStatus() const {
            return status_.load();
        }
        
        /**
         * @brief 연결 상태 반환
         */
        ConnectionStatus GetConnectionStatus() const {
            return connection_status_.load();
        }
        
        /**
         * @brief 통계 정보 반환 (모든 드라이버 동일한 인터페이스!)
         */
        const PulseOne::Structs::DriverStatistics& GetStatistics() const {
            return statistics_;
        }
        
        /**
         * @brief 디바이스 정보 반환
         */
        const UnifiedDeviceInfo& GetDeviceInfo() const {
            return device_info_;
        }
        
        /**
         * @brief 데이터 포인트들 반환
         */
        const std::vector<UnifiedDataPoint>& GetDataPoints() const {
            return data_points_;
        }

        // ===== 콜백 설정 (데이터 파이프라인 연결) =====
        
        /**
         * @brief 데이터 콜백 설정 (자동 파이프라인 연결!)
         */
        void SetDataCallback(DataCallback callback) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            data_callback_ = callback;
            logger_.Info("Data callback set - pipeline connected");
        }
        
        /**
         * @brief 상태 변경 콜백 설정
         */
        void SetStatusCallback(StatusCallback callback) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            status_callback_ = callback;
        }
        
        /**
         * @brief 에러 콜백 설정
         */
        void SetErrorCallback(ErrorCallback callback) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            error_callback_ = callback;
        }

    protected:
        // ===== 파생 클래스가 구현해야 하는 순수 가상 함수들 (단 2개!) =====
        
        /**
         * @brief 프로토콜별 초기화 (파생 클래스에서 구현)
         * @return 성공 시 true
         * @details 이 메서드에서만 프로토콜별 특화 로직 구현
         */
        virtual bool InitializeProtocol() = 0;
        
        /**
         * @brief 프로토콜별 데이터 읽기 (파생 클래스에서 구현)
         * @return 성공 시 true
         * @details 이 메서드에서만 프로토콜별 읽기 로직 구현
         */
        virtual bool ReadDataPoints() = 0;
        
        /**
         * @brief 프로토콜별 정리 (선택적 구현)
         * @details 특별한 정리가 필요한 경우에만 오버라이드
         */
        virtual void CleanupProtocol() {
            // 기본 구현: 아무것도 하지 않음
        }

        // ===== 파생 클래스에서 사용할 수 있는 유틸리티 메서드들 =====
        
        /**
         * @brief 데이터 파이프라인으로 데이터 전송 (자동!)
         * @details 파생 클래스에서 데이터를 읽은 후 이 메서드만 호출하면 
         *         자동으로 파이프라인으로 전송됨!
         */
        void NotifyDataCollected() {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            
            if (data_callback_) {
                // 🎊 자동 파이프라인 전송!
                data_callback_(device_info_, data_points_);
                
                // 통계 업데이트
                statistics_.total_reads.fetch_add(1);
                statistics_.successful_reads.fetch_add(1);
                
                logger_.Debug("Data sent to pipeline - " + 
                            std::to_string(data_points_.size()) + " points");
            }
        }
        
        /**
         * @brief 연결 상태 변경 알림
         */
        void NotifyStatusChange(ConnectionStatus new_status) {
            connection_status_ = new_status;
            
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (status_callback_) {
                status_callback_(device_info_.id, new_status);
            }
        }
        
        /**
         * @brief 에러 발생 알림
         */
        void NotifyError(const std::string& error_message) {
            logger_.Error("Driver error: " + error_message);
            
            statistics_.failed_reads.fetch_add(1);
            statistics_.IncrementProtocolCounter("errors", 1);
            
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (error_callback_) {
                error_callback_(device_info_.id, error_message);
            }
        }
        
        /**
         * @brief 개별 데이터 포인트 값 업데이트
         */
        void UpdateDataPointValue(const UUID& point_id, const DataVariant& value, 
                                 DataQuality quality = DataQuality::GOOD) {
            for (auto& point : data_points_) {
                if (point.id == point_id) {
                    point.UpdateValue(value, quality);
                    break;
                }
            }
        }
        
        /**
         * @brief 폴링 간격 대기 (중단 가능)
         */
        bool WaitForNextPoll() {
            std::unique_lock<std::mutex> lock(thread_mutex_);
            return !thread_cv_.wait_for(lock, 
                std::chrono::milliseconds(device_info_.polling_interval_ms),
                [this] { return stop_requested_.load(); });
        }

    private:
        // ===== 내부 워커 스레드 =====
        
        /**
         * @brief 워커 스레드 메인 함수
         * @details 자동으로 폴링하고 데이터 파이프라인으로 전송
         */
        void WorkerThreadFunc() {
            logger_.Info("Worker thread started");
            
            try {
                while (!stop_requested_) {
                    // 프로토콜별 데이터 읽기 시도
                    try {
                        if (ReadDataPoints()) {
                            // 성공 시 자동으로 파이프라인 전송
                            NotifyDataCollected();
                            
                            // 연결 상태 업데이트
                            if (connection_status_ != ConnectionStatus::CONNECTED) {
                                NotifyStatusChange(ConnectionStatus::CONNECTED);
                            }
                        } else {
                            // 실패 시 에러 통계 업데이트
                            statistics_.failed_reads.fetch_add(1);
                            
                            // 연결 문제일 수 있음
                            if (connection_status_ == ConnectionStatus::CONNECTED) {
                                NotifyStatusChange(ConnectionStatus::ERROR);
                            }
                        }
                    } catch (const std::exception& e) {
                        NotifyError("Data read exception: " + std::string(e.what()));
                        NotifyStatusChange(ConnectionStatus::ERROR);
                    }
                    
                    // 다음 폴링까지 대기 (중단 가능)
                    if (!WaitForNextPoll()) {
                        break;  // 정지 요청됨
                    }
                }
            } catch (const std::exception& e) {
                logger_.Error("Worker thread exception: " + std::string(e.what()));
                status_ = DriverStatus::CRASHED;
            }
            
            logger_.Info("Worker thread terminated");
        }
    };

    // =========================================================================
    // 🔥 구체적 드라이버 예시 (단 20줄로 완성!)
    // =========================================================================
    
    /**
     * @brief Modbus 드라이버 예시 - 단 20줄로 완성!
     * @details UnifiedDriverBase 덕분에 프로토콜별 로직만 구현하면 됨
     */
    class ModbusUnifiedDriver : public UnifiedDriverBase {
    private:
        // Modbus 특화 클라이언트 (기존 코드 재사용)
        std::unique_ptr<class ModbusClient> modbus_client_;
        
    protected:
        /**
         * @brief Modbus 초기화 (프로토콜 특화 로직만!)
         */
        bool InitializeProtocol() override {
            const auto& modbus_config = device_info_.GetModbusConfig();
            
            // 기존 ModbusClient 재사용
            modbus_client_ = std::make_unique<ModbusClient>();
            modbus_client_->SetSlaveId(modbus_config.slave_id);
            modbus_client_->SetTimeout(device_info_.timeout_ms);
            
            return modbus_client_->Connect(device_info_.endpoint);
        }
        
        /**
         * @brief Modbus 데이터 읽기 (프로토콜 특화 로직만!)
         */
        bool ReadDataPoints() override {
            bool success = true;
            
            for (auto& point : data_points_) {
                try {
                    // Modbus 특화 읽기
                    auto modbus_value = modbus_client_->ReadRegister(point.address.numeric);
                    
                    // 🎊 통합 구조체로 값 저장
                    point.UpdateValue(modbus_value, DataQuality::GOOD);
                    
                } catch (const std::exception& e) {
                    point.UpdateValue(0, DataQuality::BAD);
                    success = false;
                }
            }
            
            // 끝! 베이스 클래스가 자동으로 파이프라인 전송 처리
            return success;
        }
        
    public:
        ModbusUnifiedDriver() : UnifiedDriverBase("MODBUS") {}
    };

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_UNIFIED_DRIVER_BASE_H