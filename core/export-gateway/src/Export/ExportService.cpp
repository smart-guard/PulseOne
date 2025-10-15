// =============================================================================
// core/export-gateway/src/Export/ExportService.cpp
// Export Service 구현 (shared 라이브러리 활용)
// =============================================================================

#include "Export/ExportService.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"

#include <nlohmann/json.hpp>

using namespace PulseOne::Shared::Data;
using json = nlohmann::json;

namespace PulseOne {
namespace Export {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ExportService::ExportService() {
    LogManager::getInstance().Info("ExportService: 초기화 시작");
}

ExportService::~ExportService() {
    Stop();
}

// =============================================================================
// 초기화 및 시작/중지
// =============================================================================

bool ExportService::Initialize() {
    try {
        // 1. Redis 클라이언트 생성 및 연결
        redis_client_ = std::make_shared<RedisClientImpl>();
        
        std::string redis_host = ConfigManager::getInstance().getOrDefault("REDIS_HOST", "localhost");
        int redis_port = ConfigManager::getInstance().getInt("REDIS_PORT", 6379);
        
        if (!redis_client_->isConnected()) {
            LogManager::getInstance().Warn("ExportService: Redis 연결 실패, 자동 재연결 대기 중");
        }
        
        // 2. DataReader/Writer 생성
        data_reader_ = std::make_unique<DataReader>(redis_client_);
        data_writer_ = std::make_unique<DataWriter>(redis_client_);
        
        // 3. Target 핸들러 생성
        http_handler_ = std::make_unique<CSP::HttpTargetHandler>();
        mqtt_handler_ = std::make_unique<CSP::MqttTargetHandler>();
        s3_handler_ = std::make_unique<CSP::S3TargetHandler>();
        
        // 4. 설정 로드
        export_interval_ms_ = ConfigManager::getInstance().getInt("EXPORT_INTERVAL_MS", 1000);
        batch_size_ = ConfigManager::getInstance().getInt("EXPORT_BATCH_SIZE", 100);
        
        LogManager::getInstance().Info("ExportService: 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::Initialize 실패: " + std::string(e.what()));
        return false;
    }
}

bool ExportService::Start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("ExportService: 이미 실행 중");
        return false;
    }
    
    // Active 타겟 로드
    int target_count = LoadActiveTargets();
    LogManager::getInstance().Info("ExportService: " + std::to_string(target_count) + "개 타겟 로드됨");
    
    if (target_count == 0) {
        LogManager::getInstance().Warn("ExportService: 활성화된 타겟이 없습니다");
    }
    
    // 워커 스레드 시작
    is_running_.store(true);
    stats_.start_time = std::chrono::steady_clock::now();
    
    worker_thread_ = std::make_unique<std::thread>(&ExportService::workerThread, this);
    
    LogManager::getInstance().Info("ExportService: 시작됨");
    return true;
}

void ExportService::Stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("ExportService: 중지 중...");
    
    is_running_.store(false);
    
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
    
    LogManager::getInstance().Info("ExportService: 중지 완료");
}

// =============================================================================
// Target 관리
// =============================================================================

int ExportService::LoadActiveTargets() {
    std::lock_guard<std::mutex> lock(targets_mutex_);
    active_targets_.clear();
    
    try {
        auto& factory = Database::RepositoryFactory::getInstance();
        auto export_repo = factory.getExportTargetRepository();
        
        if (!export_repo) {
            LogManager::getInstance().Error("ExportService: ExportTargetRepository 없음");
            return 0;
        }
        
        // 활성화된 타겟만 로드
        // TODO: Repository에 findByEnabled() 메서드 추가 필요
        // 임시: 모든 타겟 로드 후 필터링
        
        LogManager::getInstance().Info("ExportService: 타겟 로드 완료");
        return active_targets_.size();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::LoadActiveTargets 실패: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 워커 스레드 (메인 로직)
// =============================================================================

void ExportService::workerThread() {
    LogManager::getInstance().Info("ExportService: 워커 스레드 시작");
    
    while (is_running_.load()) {
        try {
            // 1. 활성 타겟 확인
            std::vector<ExportTargetConfig> targets;
            {
                std::lock_guard<std::mutex> lock(targets_mutex_);
                targets = active_targets_;
            }
            
            if (targets.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            
            // 2. Redis에서 데이터 읽기
            // 예: 모든 point:*:latest 키 읽기
            auto keys = data_reader_->FindKeys("point:*:latest");
            
            if (keys.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
                continue;
            }
            
            // 3. 포인트 ID 추출 및 배치 읽기
            std::vector<int> point_ids;
            for (const auto& key : keys) {
                // "point:123:latest" → 123
                size_t start = key.find(':') + 1;
                size_t end = key.find(':', start);
                if (start != std::string::npos && end != std::string::npos) {
                    int point_id = std::stoi(key.substr(start, end - start));
                    point_ids.push_back(point_id);
                    
                    if (point_ids.size() >= static_cast<size_t>(batch_size_)) {
                        break;
                    }
                }
            }
            
            if (point_ids.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
                continue;
            }
            
            // 4. 데이터 배치 읽기
            auto batch_result = data_reader_->ReadPointsBatch(point_ids);
            
            if (batch_result.values.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
                continue;
            }
            
            LogManager::getInstance().Info("ExportService: " + 
                std::to_string(batch_result.values.size()) + "개 데이터 포인트 읽음");
            
            // 5. 각 타겟으로 전송
            for (const auto& target : targets) {
                bool success = exportToTarget(target, batch_result.values);
                
                if (success) {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.successful_exports++;
                    stats_.total_data_points += batch_result.values.size();
                } else {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.failed_exports++;
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_exports++;
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("ExportService::workerThread 에러: " + std::string(e.what()));
        }
        
        // 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
    }
    
    LogManager::getInstance().Info("ExportService: 워커 스레드 종료");
}

// =============================================================================
// 데이터 전송
// =============================================================================

bool ExportService::exportToTarget(const ExportTargetConfig& target,
                                   const std::vector<CurrentValue>& data) {
    try {
        // JSON 변환
        json payload = json::array();
        for (const auto& value : data) {
            payload.push_back(value.toJson());
        }
        
        std::string json_str = payload.dump();
        
        // 타겟 타입에 따라 전송
        bool success = false;
        
        if (target.target_type == "HTTP" || target.target_type == "HTTPS") {
            success = http_handler_->Send(target.endpoint, json_str, target.headers);
        } 
        else if (target.target_type == "MQTT") {
            success = mqtt_handler_->Publish(target.endpoint, json_str);
        }
        else if (target.target_type == "S3") {
            success = s3_handler_->Upload(target.endpoint, json_str);
        }
        else {
            LogManager::getInstance().Warn("ExportService: 알 수 없는 타겟 타입: " + target.target_type);
            return false;
        }
        
        // 결과 로깅
        logExportResult(target.id, 
                       success ? "success" : "failed",
                       data.size(),
                       success ? "" : "전송 실패");
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::exportToTarget 실패: " + std::string(e.what()));
        logExportResult(target.id, "error", data.size(), e.what());
        return false;
    }
}

void ExportService::logExportResult(int target_id,
                                    const std::string& status,
                                    int data_count,
                                    const std::string& error_message) {
    try {
        Shared::Data::ExportLogEntry log;
        log.target_id = target_id;
        log.status = status;
        log.records_count = data_count;
        log.error_message = error_message;
        log.exported_at = std::chrono::system_clock::now();
        
        data_writer_->WriteExportLog(log);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::logExportResult 실패: " + std::string(e.what()));
    }
}

// =============================================================================
// 통계
// =============================================================================

ExportService::Statistics ExportService::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void ExportService::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Statistics{};
    stats_.start_time = std::chrono::steady_clock::now();
}

} // namespace Export
} // namespace PulseOne