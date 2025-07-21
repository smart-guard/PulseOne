/**
 * @file MaintenanceManager.cpp
 * @brief 디바이스 점검 모드 관리 클래스 구현
 * @details 점검 작업 중 알람 억제 및 상태 관리
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 2.0.0
 */

#include "Engine/MaintenanceManager.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Engine {

using json = nlohmann::json;
using namespace std::chrono;

// =============================================================================
// MaintenanceManager 클래스 정의 (헤더에 없는 부분 포함)
// =============================================================================

/**
 * @brief 점검 세션 정보 구조체
 */
struct MaintenanceSession {
    UUID session_id;                                ///< 세션 고유 ID
    std::string reason;                             ///< 점검 사유
    std::chrono::system_clock::time_point start_time;    ///< 시작 시간
    std::chrono::minutes estimated_duration;             ///< 예상 소요 시간
    std::string operator_name;                      ///< 작업자 이름
    std::vector<std::string> work_items;            ///< 작업 항목들
    bool is_active;                                 ///< 활성 상태
    
    MaintenanceSession() : is_active(false) {}
};

/**
 * @brief 점검 모드 관리자 클래스
 */
class MaintenanceManager {
public:
    /**
     * @brief 생성자
     * @param device_id 디바이스 ID
     * @param redis_client Redis 클라이언트
     * @param logger 로거
     */
    MaintenanceManager(const UUID& device_id,
                      std::shared_ptr<RedisClient> redis_client,
                      std::shared_ptr<LogManager> logger)
        : device_id_(device_id)
        , redis_client_(redis_client)
        , logger_(logger)
        , is_maintenance_active_(false)
    {
        maintenance_channel_ = "device_maintenance:" + device_id_.to_string();
    }
    
    /**
     * @brief 소멸자
     */
    ~MaintenanceManager() {
        if (is_maintenance_active_) {
            DisableMaintenanceMode();
        }
    }
    
    /**
     * @brief 점검 모드 활성화
     * @param reason 점검 사유
     * @param estimated_duration 예상 소요 시간
     * @param operator_name 작업자 이름
     * @return 성공 시 true
     */
    bool EnableMaintenanceMode(const std::string& reason, 
                              std::chrono::minutes estimated_duration = std::chrono::minutes(60),
                              const std::string& operator_name = "Unknown") {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        
        if (is_maintenance_active_) {
            logger_->Warn("Maintenance mode already active for device: " + device_id_.to_string());
            return false;
        }
        
        try {
            // 새 세션 생성
            current_session_ = MaintenanceSession();
            current_session_.session_id = GenerateUUID();
            current_session_.reason = reason;
            current_session_.start_time = system_clock::now();
            current_session_.estimated_duration = estimated_duration;
            current_session_.operator_name = operator_name;
            current_session_.is_active = true;
            
            is_maintenance_active_ = true;
            
            // Redis에 상태 발행
            PublishMaintenanceStatus();
            
            logger_->Info("Maintenance mode enabled - Device: " + device_id_.to_string() + 
                         ", Reason: " + reason + 
                         ", Duration: " + std::to_string(estimated_duration.count()) + " minutes");
            
            return true;
            
        } catch (const std::exception& e) {
            logger_->Error("Failed to enable maintenance mode: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 점검 모드 비활성화
     * @return 성공 시 true
     */
    bool DisableMaintenanceMode() {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        
        if (!is_maintenance_active_) {
            return true; // 이미 비활성화됨
        }
        
        try {
            auto end_time = system_clock::now();
            auto actual_duration = duration_cast<minutes>(end_time - current_session_.start_time);
            
            current_session_.is_active = false;
            is_maintenance_active_ = false;
            
            // Redis에 상태 발행
            PublishMaintenanceStatus();
            
            logger_->Info("Maintenance mode disabled - Device: " + device_id_.to_string() + 
                         ", Actual duration: " + std::to_string(actual_duration.count()) + " minutes");
            
            return true;
            
        } catch (const std::exception& e) {
            logger_->Error("Failed to disable maintenance mode: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 점검 모드 상태 확인
     * @return 점검 모드 여부
     */
    bool IsInMaintenanceMode() const {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        return is_maintenance_active_;
    }
    
    /**
     * @brief 점검 작업 항목 추가
     * @param work_item 작업 항목
     */
    void AddWorkItem(const std::string& work_item) {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        
        if (is_maintenance_active_) {
            current_session_.work_items.push_back(work_item);
            PublishMaintenanceStatus();
            
            logger_->Info("Work item added to maintenance session: " + work_item);
        }
    }
    
    /**
     * @brief 점검 진행률 업데이트
     * @param completed_items 완료된 작업 수
     * @param total_items 전체 작업 수
     */
    void UpdateProgress(int completed_items, int total_items) {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        
        if (is_maintenance_active_) {
            double progress_percent = total_items > 0 ? 
                (static_cast<double>(completed_items) / total_items) * 100.0 : 0.0;
            
            logger_->Info("Maintenance progress: " + std::to_string(progress_percent) + "% " +
                         "(" + std::to_string(completed_items) + "/" + std::to_string(total_items) + ")");
            
            PublishMaintenanceStatus();
        }
    }
    
    /**
     * @brief 현재 점검 세션 정보 반환
     * @return 세션 정보 JSON
     */
    std::string GetMaintenanceInfo() const {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        
        json info;
        
        try {
            info["device_id"] = device_id_.to_string();
            info["is_active"] = is_maintenance_active_;
            
            if (is_maintenance_active_) {
                info["session_id"] = current_session_.session_id.to_string();
                info["reason"] = current_session_.reason;
                info["operator_name"] = current_session_.operator_name;
                info["estimated_duration_minutes"] = current_session_.estimated_duration.count();
                
                // 시작 시간 (ISO 8601 형식)
                auto start_time_t = system_clock::to_time_t(current_session_.start_time);
                std::stringstream ss;
                ss << std::put_time(std::gmtime(&start_time_t), "%Y-%m-%dT%H:%M:%SZ");
                info["start_time"] = ss.str();
                
                // 경과 시간 계산
                auto now = system_clock::now();
                auto elapsed = duration_cast<minutes>(now - current_session_.start_time);
                info["elapsed_minutes"] = elapsed.count();
                
                // 남은 시간 계산
                auto remaining = current_session_.estimated_duration - elapsed;
                info["remaining_minutes"] = std::max(0L, remaining.count());
                
                // 작업 항목들
                info["work_items"] = current_session_.work_items;
                info["work_items_count"] = current_session_.work_items.size();
                
                // 예상 종료 시간
                auto estimated_end = current_session_.start_time + current_session_.estimated_duration;
                auto end_time_t = system_clock::to_time_t(estimated_end);
                std::stringstream end_ss;
                end_ss << std::put_time(std::gmtime(&end_time_t), "%Y-%m-%dT%H:%M:%SZ");
                info["estimated_end_time"] = end_ss.str();
            }
            
            info["timestamp"] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            
        } catch (const std::exception& e) {
            logger_->Error("Failed to generate maintenance info: " + std::string(e.what()));
            info["error"] = "Failed to generate maintenance info";
        }
        
        return info.dump();
    }
    
    /**
     * @brief 알람 억제 여부 확인
     * @param alarm_id 알람 ID
     * @param severity 심각도
     * @return 억제해야 하는 경우 true
     */
    bool ShouldSuppressAlarm(const UUID& alarm_id, const std::string& severity) const {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        
        if (!is_maintenance_active_) {
            return false; // 점검 모드가 아니면 억제하지 않음
        }
        
        // 심각도별 억제 정책
        if (severity == "critical") {
            // 위험 알람은 점검 중에도 발생시킴
            return false;
        } else if (severity == "warning" || severity == "info") {
            // 경고 및 정보 알람은 점검 중 억제
            return true;
        }
        
        return true; // 기본적으로 억제
    }

private:
    // =============================================================================
    // 내부 데이터 멤버
    // =============================================================================
    
    UUID device_id_;                                      ///< 디바이스 ID
    std::shared_ptr<RedisClient> redis_client_;           ///< Redis 클라이언트
    std::shared_ptr<LogManager> logger_;                  ///< 로거
    
    mutable std::mutex maintenance_mutex_;                ///< 점검 상태 뮤텍스
    std::atomic<bool> is_maintenance_active_;             ///< 점검 활성 상태
    MaintenanceSession current_session_;                  ///< 현재 점검 세션
    
    std::string maintenance_channel_;                     ///< Redis 채널명
    
    // =============================================================================
    // 내부 유틸리티 함수들
    // =============================================================================
    
    /**
     * @brief Redis에 점검 상태 발행
     */
    void PublishMaintenanceStatus() {
        if (!redis_client_) return;
        
        try {
            std::string status_json = GetMaintenanceInfo();
            redis_client_->Publish(maintenance_channel_, status_json);
        } catch (const std::exception& e) {
            logger_->Error("Failed to publish maintenance status: " + std::string(e.what()));
        }
    }
    
    /**
     * @brief UUID 생성 (간단한 구현)
     * @return 생성된 UUID
     */
    UUID GenerateUUID() {
        // 실제로는 더 복잡한 UUID 생성 로직 필요
        static std::atomic<uint64_t> counter{0};
        auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        
        std::string uuid_str = "maint-" + std::to_string(now) + "-" + std::to_string(counter.fetch_add(1));
        
        UUID uuid;
        // UUID 문자열을 적절히 변환 (실제 구현 필요)
        return uuid;
    }
};

} // namespace Engine
} // namespace PulseOne