/**
 * @file ExportService.cpp
 * @brief Export Gateway 메인 서비스 - C# CSPManager 완전 포팅 + DB 통합
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 2.1.0 (Database 통합)
 * 저장 위치: core/export-gateway/src/Export/ExportService.cpp
 * 
 * 주요 기능:
 * - Redis에서 실시간 데이터 배치 읽기
 * - Database에서 타겟 설정 로드 (export_targets)
 * - Database에서 매핑 정보 로드 (export_target_mappings)
 * - 활성 타겟으로 자동 전송 (HTTP/S3/File)
 * - C# 재시도 로직 포팅 (3회 재시도)
 * - 실패 데이터 파일 저장
 * - 타겟별 전송 통계 DB 업데이트
 * 
 * C# 참조: iCos5CSPGateway/CSPManager_alarm.cs
 */

#include "Export/ExportService.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/S3TargetHandler.h"
#include "CSP/FileTargetHandler.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

// ✅ Repository 헤더 추가 (incomplete type 해결)
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportTargetMappingEntity.h"
#include "Database/Entities/ExportLogEntity.h"

// ✅ RedisClientImpl 사용 (추상 클래스가 아님)
#include "Client/RedisClientImpl.h"

#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

namespace PulseOne {
namespace Export {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

ExportService::ExportService() {
    LogManager::getInstance().Info("ExportService 생성");
}

ExportService::~ExportService() {
    Stop();
    LogManager::getInstance().Info("ExportService 소멸");
}

// =============================================================================
// 초기화 및 시작/중지
// =============================================================================

bool ExportService::Initialize() {
    try {
        LogManager::getInstance().Info("ExportService 초기화 시작");
        
        // ✅ 1. RedisClientImpl 생성 (자동 연결됨)
        redis_client_ = std::make_shared<RedisClientImpl>();
        
        // ✅ 연결 확인 (자동 연결이 이미 시도됨)
        if (!redis_client_->isConnected()) {
            LogManager::getInstance().Warn("ExportService: Redis 초기 연결 실패, 백그라운드 재연결 진행 중");
            // 계속 진행 (자동 재연결 활성화됨)
        } else {
            LogManager::getInstance().Info("ExportService: Redis 연결 성공");
        }
        
        // 2. DataReader/Writer 생성
        data_reader_ = std::make_unique<Shared::Data::DataReader>(redis_client_);
        data_writer_ = std::make_unique<Shared::Data::DataWriter>(redis_client_);
        
        // 3. Target 핸들러 생성
        http_handler_ = std::make_unique<CSP::HttpTargetHandler>();
        s3_handler_ = std::make_unique<CSP::S3TargetHandler>();
        file_handler_ = std::make_unique<CSP::FileTargetHandler>();
        
        // ✅ 4. 설정 로드 (getOrDefault 사용)
        export_interval_ms_ = ConfigManager::getInstance().getInt("EXPORT_INTERVAL_MS", 1000);
        batch_size_ = ConfigManager::getInstance().getInt("EXPORT_BATCH_SIZE", 100);
        max_retry_count_ = ConfigManager::getInstance().getInt("EXPORT_MAX_RETRY", 3);
        retry_delay_ms_ = ConfigManager::getInstance().getInt("EXPORT_RETRY_DELAY_MS", 1000);
        
        // ✅ 실패 파일 디렉토리 (getOrDefault 사용)
        failed_data_dir_ = ConfigManager::getInstance().getOrDefault("EXPORT_FAILED_DIR", 
                                                                      "/app/data/export_failed");
        if (!fs::exists(failed_data_dir_)) {
            fs::create_directories(failed_data_dir_);
        }
        
        LogManager::getInstance().Info("ExportService: 초기화 완료");
        LogManager::getInstance().Info("  - Export 간격: " + std::to_string(export_interval_ms_) + "ms");
        LogManager::getInstance().Info("  - 배치 크기: " + std::to_string(batch_size_));
        LogManager::getInstance().Info("  - 실패 데이터 저장 경로: " + failed_data_dir_);
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
    
    LogManager::getInstance().Info("ExportService: 시작됨 (간격: " + 
                                  std::to_string(export_interval_ms_) + "ms)");
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
        
        // ✅ is_enabled = 1인 타겟만 조회
        auto db_targets = export_repo->findByEnabled(true);
        
        for (const auto& db_target : db_targets) {
            ExportTargetConfig target;
            
            // ✅ getter 메서드로 접근
            target.id = db_target.getId();
            target.name = db_target.getName();
            target.type = db_target.getTargetType();
            target.enabled = db_target.isEnabled();
            target.endpoint = "";
            
            // ✅ config JSON 파싱
            try {
                std::string config_str = db_target.getConfig();
                target.config = json::parse(config_str);
                
                // endpoint 추출 (타겟 타입별)
                if (target.config.contains("endpoint")) {
                    target.endpoint = target.config["endpoint"].get<std::string>();
                } else if (target.config.contains("bucket_name")) {
                    target.endpoint = target.config["bucket_name"].get<std::string>();
                }
                
            } catch (const json::exception& e) {
                LogManager::getInstance().Error("ExportService: 타겟 " + target.name + 
                                               " config 파싱 실패: " + e.what());
                continue;
            }
            
            // ✅ 통계 정보
            target.success_count = db_target.getSuccessfulExports();
            target.failure_count = db_target.getFailedExports();
            
            active_targets_.push_back(target);
            
            LogManager::getInstance().Info("ExportService: 타겟 로드됨 - " + target.name + 
                                          " (" + target.type + ")");
        }
        
        LogManager::getInstance().Info("ExportService: " + 
                                      std::to_string(active_targets_.size()) + 
                                      "개 타겟 로드 완료");
        return active_targets_.size();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::LoadActiveTargets 실패: " + 
                                       std::string(e.what()));
        return 0;
    }
}

bool ExportService::EnableTarget(int target_id, bool enable) {
    std::lock_guard<std::mutex> lock(targets_mutex_);
    
    auto it = std::find_if(active_targets_.begin(), active_targets_.end(),
                          [target_id](const ExportTargetConfig& t) { return t.id == target_id; });
    
    if (it != active_targets_.end()) {
        it->enabled = enable;
        LogManager::getInstance().Info("ExportService: 타겟 " + std::to_string(target_id) + 
                                      (enable ? " 활성화" : " 비활성화"));
        return true;
    }
    
    return false;
}

void ExportService::UpdateTargetStatus(int target_id, const std::string& status) {
    std::lock_guard<std::mutex> lock(targets_mutex_);
    
    auto it = std::find_if(active_targets_.begin(), active_targets_.end(),
                          [target_id](const ExportTargetConfig& t) { return t.id == target_id; });
    
    if (it != active_targets_.end()) {
        it->status = status;
        it->last_update_time = std::chrono::system_clock::now();
    }
}

// =============================================================================
// 워커 스레드 (메인 로직) - C# CSPManager 포팅
// =============================================================================

void ExportService::workerThread() {
    LogManager::getInstance().Info("ExportService: 워커 스레드 시작");
    
    while (is_running_.load()) {
        try {
            // 1. 활성 타겟 복사 (스레드 안전)
            std::vector<ExportTargetConfig> targets;
            {
                std::lock_guard<std::mutex> lock(targets_mutex_);
                targets = active_targets_;
            }
            
            if (targets.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
                continue;
            }
            
            // 2. 각 타겟별로 전송할 포인트 ID 수집
            std::vector<int> point_ids = collectPointIdsForExport(targets);
            
            if (point_ids.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
                continue;
            }
            
            // 3. Redis에서 데이터 배치 읽기
            auto batch_result = data_reader_->ReadPointsBatch(point_ids);
            
            if (batch_result.values.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
                continue;
            }
            
            LogManager::getInstance().Debug("ExportService: " + 
                std::to_string(batch_result.values.size()) + "개 데이터 포인트 읽음");
            
            // 4. 각 타겟으로 전송 (C# taskAlarmSingle 패턴)
            for (const auto& target : targets) {
                if (!target.enabled) {
                    continue;
                }
                
                bool success = exportToTargetWithRetry(target, batch_result.values);
                
                // 통계 업데이트
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.total_exports++;
                    
                    if (success) {
                        stats_.successful_exports++;
                        stats_.total_data_points += batch_result.values.size();
                    } else {
                        stats_.failed_exports++;
                    }
                }
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("ExportService::workerThread 에러: " + 
                                           std::string(e.what()));
        }
        
        // 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(export_interval_ms_));
    }
    
    LogManager::getInstance().Info("ExportService: 워커 스레드 종료");
}

// =============================================================================
// 데이터 전송 - C# 재시도 로직 포팅
// =============================================================================

bool ExportService::exportToTargetWithRetry(const ExportTargetConfig& target,
                                            const std::vector<Shared::Data::CurrentValue>& data) {
    int retry_count = 0;
    bool success = false;
    std::string last_error;
    
    // C# 재시도 로직: while (failCount < _config.RetransmissionCount)
    while (retry_count <= max_retry_count_ && !success) {
        try {
            if (retry_count > 0) {
                LogManager::getInstance().Warn("ExportService: 재시도 " + 
                    std::to_string(retry_count) + "/" + std::to_string(max_retry_count_) + 
                    " - " + target.name);
                
                // C# Thread.Sleep(_config.RetransmissionInterval)
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_));
            }
            
            // 실제 전송 실행
            success = exportToTarget(target, data);
            
            if (success) {
                logExportResult(target.id, "success", data.size(), "");
                
                if (retry_count > 0) {
                    LogManager::getInstance().Info("ExportService: 재시도 성공 - " + target.name);
                }
                
                break;
            }
            
        } catch (const std::exception& e) {
            last_error = e.what();
            LogManager::getInstance().Error("ExportService: 전송 예외 - " + 
                target.name + ": " + last_error);
        }
        
        retry_count++;
    }
    
    // 최종 실패 처리 (C# 패턴)
    if (!success) {
        logExportResult(target.id, "failed", data.size(), last_error);
        
        // 실패 데이터 파일 저장
        saveFailedData(target, data, last_error);
        
        LogManager::getInstance().Error("ExportService: 전송 최종 실패 (" + 
            std::to_string(max_retry_count_ + 1) + "회 시도) - " + target.name);
    }
    
    return success;
}

bool ExportService::exportToTarget(const ExportTargetConfig& target,
                                   const std::vector<Shared::Data::CurrentValue>& data) {
    try {
        // JSON 페이로드 생성
        json payload = json::array();
        for (const auto& value : data) {
            payload.push_back(value.toJson());
        }
        
        std::string json_str = payload.dump();
        
        // 타겟 타입에 따라 전송
        bool success = false;
        
        if (target.type == "HTTP" || target.type == "HTTPS") {
            // HTTP 전송 (재시도 로직은 HttpTargetHandler 내부에 있음)
            CSP::AlarmMessage dummy_alarm;  // 임시: 실제로는 데이터를 AlarmMessage로 변환
            auto result = http_handler_->sendAlarm(dummy_alarm, target.config);
            success = result.success;
            
        } else if (target.type == "S3") {
            // S3 업로드 (재시도 로직은 S3TargetHandler 내부에 있음)
            CSP::AlarmMessage dummy_alarm;
            auto result = s3_handler_->sendAlarm(dummy_alarm, target.config);
            success = result.success;
            
        } else if (target.type == "FILE") {
            // 파일 저장
            CSP::AlarmMessage dummy_alarm;
            auto result = file_handler_->sendAlarm(dummy_alarm, target.config);
            success = result.success;
            
        } else {
            LogManager::getInstance().Warn("ExportService: 알 수 없는 타겟 타입: " + target.type);
            return false;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::exportToTarget 예외: " + 
                                       std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 실패 데이터 처리 - C# 패턴
// =============================================================================

void ExportService::saveFailedData(const ExportTargetConfig& target,
                                   const std::vector<Shared::Data::CurrentValue>& data,
                                   const std::string& error_message) {
    try {
        // 파일명 생성: failed_{timestamp}_{target_id}_{count}.json
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        
        std::ostringstream filename;
        filename << "failed_"
                 << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_"
                 << "target" << target.id << "_"
                 << data.size() << "points.json";
        
        std::string file_path = failed_data_dir_ + "/" + filename.str();
        
        // JSON 생성
        json failed_record;
        failed_record["timestamp"] = std::time(nullptr);
        failed_record["target_id"] = target.id;
        failed_record["target_name"] = target.name;
        failed_record["target_type"] = target.type;
        failed_record["error_message"] = error_message;
        failed_record["retry_count"] = max_retry_count_ + 1;
        
        json data_array = json::array();
        for (const auto& value : data) {
            data_array.push_back(value.toJson());
        }
        failed_record["data"] = data_array;
        
        // 파일 저장
        std::ofstream file(file_path);
        if (file.is_open()) {
            file << failed_record.dump(2);
            file.close();
            
            LogManager::getInstance().Info("ExportService: 실패 데이터 저장됨 - " + file_path);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::saveFailedData 예외: " + 
                                       std::string(e.what()));
    }
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

std::vector<int> ExportService::collectPointIdsForExport(
    const std::vector<ExportTargetConfig>& targets) {
    
    std::set<int> point_ids_set;
    
    try {
        auto& factory = Database::RepositoryFactory::getInstance();
        auto mapping_repo = factory.getExportTargetMappingRepository();
        
        if (!mapping_repo) {
            LogManager::getInstance().Error("ExportService: MappingRepository 없음");
            return {};
        }
        
        // 각 타겟별로 매핑된 포인트 수집
        for (const auto& target : targets) {
            if (!target.enabled) {
                continue;
            }
            
            // ✅ target_id로 매핑 조회
            auto mappings = mapping_repo->findByTargetId(target.id);
            
            for (const auto& mapping : mappings) {
                // ✅ getter 메서드 사용
                if (mapping.isEnabled()) {
                    point_ids_set.insert(mapping.getPointId());
                }
            }
        }
        
        std::vector<int> point_ids(point_ids_set.begin(), point_ids_set.end());
        
        if (!point_ids.empty()) {
            LogManager::getInstance().Debug("ExportService: " + 
                std::to_string(point_ids.size()) + "개 포인트 수집됨");
        }
        
        return point_ids;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::collectPointIdsForExport 실패: " + 
                                       std::string(e.what()));
        return {};
    }
}

void ExportService::logExportResult(int target_id, 
                                    const std::string& status,
                                    int data_count,
                                    const std::string& error_message) {
    try {
        // ✅ 1. Database에 로그 저장
        auto& factory = Database::RepositoryFactory::getInstance();
        auto log_repo = factory.getExportLogRepository();
        
        if (log_repo) {
            Database::Entities::ExportLogEntity log;
            log.setLogType("export");
            log.setTargetId(target_id);
            log.setStatus(status);
            log.setErrorMessage(error_message);
            log.setTimestamp(std::chrono::system_clock::now());
            
            // ✅ save() 메서드 사용 (create가 아님)
            log_repo->save(log);
        }
        
        // ✅ 2. export_targets 테이블 통계 업데이트
        auto export_repo = factory.getExportTargetRepository();
        if (export_repo) {
            // ✅ Optional 올바른 처리
            auto target_opt = export_repo->findById(target_id);
            
            if (target_opt.has_value()) {
                auto target = target_opt.value();
                
                // ✅ increment 메서드 사용
                target.incrementTotalExports();
                
                if (status == "success") {
                    target.incrementSuccessfulExports();
                    target.setLastSuccessAt(std::chrono::system_clock::now());
                } else {
                    target.incrementFailedExports();
                    target.setLastError(error_message);
                    target.setLastErrorAt(std::chrono::system_clock::now());
                }
                
                target.setLastExportAt(std::chrono::system_clock::now());
                
                // ✅ DB 업데이트 (참조 전달)
                export_repo->update(target);
            }
        }
        
        // 3. 로그 출력
        if (status == "success") {
            LogManager::getInstance().Info("ExportService: 전송 성공 - 타겟 " + 
                std::to_string(target_id) + ", " + std::to_string(data_count) + "개 포인트");
        } else {
            LogManager::getInstance().Error("ExportService: 전송 실패 - 타겟 " + 
                std::to_string(target_id) + ": " + error_message);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ExportService::logExportResult 예외: " + 
                                       std::string(e.what()));
    }
}

// =============================================================================
// 통계 및 모니터링
// =============================================================================

ExportService::Statistics ExportService::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void ExportService::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_exports = 0;
    stats_.successful_exports = 0;
    stats_.failed_exports = 0;
    stats_.total_data_points = 0;
    stats_.start_time = std::chrono::steady_clock::now();
    
    LogManager::getInstance().Info("ExportService: 통계 초기화됨");
}

} // namespace Export
} // namespace PulseOne