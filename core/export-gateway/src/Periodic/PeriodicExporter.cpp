/**
 * @file PeriodicExporter.cpp
 * @brief 주기적 데이터 스캔 및 전송 구현
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
 */

#include "Periodic/PeriodicExporter.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Periodic {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

PeriodicExporter::PeriodicExporter(const PeriodicExporterConfig& config)
    : config_(config) {
    
    LogManager::getInstance().Info("PeriodicExporter 초기화 시작");
    LogManager::getInstance().Info("전송 주기: " + std::to_string(config_.export_interval_seconds) + "초");
    LogManager::getInstance().Info("배치 크기: " + std::to_string(config_.batch_size));
    LogManager::getInstance().Info("디바이스 수: " + std::to_string(config_.device_numbers.size()));
}

PeriodicExporter::~PeriodicExporter() {
    stop();
    LogManager::getInstance().Info("PeriodicExporter 종료 완료");
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool PeriodicExporter::start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("PeriodicExporter가 이미 실행 중입니다");
        return false;
    }
    
    LogManager::getInstance().Info("PeriodicExporter 시작 중...");
    
    // 1. Redis 연결 초기화
    if (!initializeRedisConnection()) {
        LogManager::getInstance().Error("Redis 연결 초기화 실패");
        return false;
    }
    
    // 2. DynamicTargetManager 초기화
    if (config_.use_dynamic_targets) {
        if (!initializeDynamicTargetManager()) {
            LogManager::getInstance().Error("DynamicTargetManager 초기화 실패");
            return false;
        }
    }
    
    // 3. 워커 스레드 시작
    should_stop_ = false;
    is_running_ = true;
    worker_thread_ = std::make_unique<std::thread>(
        &PeriodicExporter::periodicScanLoop, this);
    
    LogManager::getInstance().Info("PeriodicExporter 시작 완료");
    return true;
}

void PeriodicExporter::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("PeriodicExporter 중지 중...");
    
    should_stop_ = true;
    
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
    
    is_running_ = false;
    LogManager::getInstance().Info("PeriodicExporter 중지 완료");
}

// =============================================================================
// 수동 트리거
// =============================================================================

ExportResult PeriodicExporter::triggerExportNow() {
    LogManager::getInstance().Info("수동 전송 트리거");
    
    ExportResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // 모든 디바이스 스캔
        std::vector<DataPoint> all_points;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            for (int device_num : config_.device_numbers) {
                auto device_points = scanDeviceData(device_num);
                all_points.insert(all_points.end(), 
                                 device_points.begin(), 
                                 device_points.end());
            }
        }
        
        result.total_points = all_points.size();
        
        // 배치 생성 및 전송
        auto batches = createBatches(all_points);
        for (const auto& batch : batches) {
            result.exported_points += exportBatch(batch);
        }
        
        result.failed_points = result.total_points - result.exported_points;
        result.success = (result.exported_points > 0);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        LogManager::getInstance().Error("수동 전송 실패: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

ExportResult PeriodicExporter::exportDevice(int device_num) {
    LogManager::getInstance().Info("디바이스 " + std::to_string(device_num) + " 전송");
    
    ExportResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        auto points = scanDeviceData(device_num);
        result.total_points = points.size();
        
        auto batches = createBatches(points);
        for (const auto& batch : batches) {
            result.exported_points += exportBatch(batch);
        }
        
        result.failed_points = result.total_points - result.exported_points;
        result.success = (result.exported_points > 0);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        LogManager::getInstance().Error("디바이스 전송 실패: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

// =============================================================================
// 설정 관리
// =============================================================================

void PeriodicExporter::setExportInterval(int seconds) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.export_interval_seconds = seconds;
    LogManager::getInstance().Info("전송 주기 변경: " + std::to_string(seconds) + "초");
}

void PeriodicExporter::setBatchSize(int size) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.batch_size = size;
    LogManager::getInstance().Info("배치 크기 변경: " + std::to_string(size));
}

void PeriodicExporter::setDeviceNumbers(const std::vector<int>& device_nums) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.device_numbers = device_nums;
    LogManager::getInstance().Info("디바이스 목록 변경: " + std::to_string(device_nums.size()) + "개");
}

void PeriodicExporter::addDevice(int device_num) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    auto it = std::find(config_.device_numbers.begin(), 
                       config_.device_numbers.end(), 
                       device_num);
    if (it == config_.device_numbers.end()) {
        config_.device_numbers.push_back(device_num);
        LogManager::getInstance().Info("디바이스 추가: " + std::to_string(device_num));
    }
}

void PeriodicExporter::removeDevice(int device_num) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    auto it = std::remove(config_.device_numbers.begin(), 
                         config_.device_numbers.end(), 
                         device_num);
    if (it != config_.device_numbers.end()) {
        config_.device_numbers.erase(it, config_.device_numbers.end());
        LogManager::getInstance().Info("디바이스 제거: " + std::to_string(device_num));
    }
}

// =============================================================================
// 통계
// =============================================================================

json PeriodicExporter::getStatistics() const {
    json stats;
    
    stats["is_running"] = is_running_.load();
    stats["total_scans"] = total_scans_.load();
    stats["successful_scans"] = successful_scans_.load();
    stats["failed_scans"] = failed_scans_.load();
    stats["total_points_scanned"] = total_points_scanned_.load();
    stats["total_points_exported"] = total_points_exported_.load();
    stats["total_points_failed"] = total_points_failed_.load();
    stats["last_scan_timestamp"] = last_scan_timestamp_.load();
    stats["total_scan_time_ms"] = total_scan_time_ms_.load();
    
    // 평균 계산
    size_t total = total_scans_.load();
    if (total > 0) {
        stats["avg_scan_time_ms"] = static_cast<double>(total_scan_time_ms_.load()) / total;
        stats["avg_points_per_scan"] = static_cast<double>(total_points_scanned_.load()) / total;
    } else {
        stats["avg_scan_time_ms"] = 0.0;
        stats["avg_points_per_scan"] = 0.0;
    }
    
    // 성공률
    size_t scanned = total_points_scanned_.load();
    if (scanned > 0) {
        stats["export_success_rate"] = 
            static_cast<double>(total_points_exported_.load()) / scanned * 100.0;
    } else {
        stats["export_success_rate"] = 0.0;
    }
    
    return stats;
}

void PeriodicExporter::resetStatistics() {
    total_scans_ = 0;
    successful_scans_ = 0;
    failed_scans_ = 0;
    total_points_scanned_ = 0;
    total_points_exported_ = 0;
    total_points_failed_ = 0;
    last_scan_timestamp_ = 0;
    total_scan_time_ms_ = 0;
    
    LogManager::getInstance().Info("통계 초기화 완료");
}

// =============================================================================
// 내부 메서드
// =============================================================================

void PeriodicExporter::periodicScanLoop() {
    using namespace std::chrono;
    
    LogManager::getInstance().Info("주기적 스캔 루프 시작");
    
    while (!should_stop_.load()) {
        auto start_time = steady_clock::now();
        
        try {
            // 1. 모든 디바이스 스캔
            std::vector<DataPoint> all_points;
            std::vector<int> device_list;
            
            {
                std::lock_guard<std::mutex> lock(config_mutex_);
                device_list = config_.device_numbers;
            }
            
            for (int device_num : device_list) {
                if (should_stop_.load()) break;
                
                try {
                    auto device_points = scanDeviceData(device_num);
                    all_points.insert(all_points.end(), 
                                     device_points.begin(), 
                                     device_points.end());
                    
                    if (config_.enable_debug_log) {
                        LogManager::getInstance().Debug(
                            "디바이스 " + std::to_string(device_num) + 
                            " 스캔: " + std::to_string(device_points.size()) + "개 포인트");
                    }
                } catch (const std::exception& e) {
                    LogManager::getInstance().Error(
                        "디바이스 " + std::to_string(device_num) + 
                        " 스캔 실패: " + std::string(e.what()));
                }
            }
            
            // 2. 배치 생성
            auto batches = createBatches(all_points);
            
            // 3. 배치 전송
            size_t total_exported = 0;
            for (const auto& batch : batches) {
                if (should_stop_.load()) break;
                total_exported += exportBatch(batch);
            }
            
            // 4. 통계 업데이트
            total_scans_.fetch_add(1);
            successful_scans_.fetch_add(1);
            total_points_scanned_.fetch_add(all_points.size());
            total_points_exported_.fetch_add(total_exported);
            total_points_failed_.fetch_add(all_points.size() - total_exported);
            
            auto now = system_clock::now();
            last_scan_timestamp_ = duration_cast<milliseconds>(
                now.time_since_epoch()).count();
            
            auto end_time = steady_clock::now();
            auto duration = duration_cast<milliseconds>(end_time - start_time);
            total_scan_time_ms_.fetch_add(duration.count());
            
            LogManager::getInstance().Info(
                "스캔 완료 - 포인트: " + std::to_string(all_points.size()) + 
                ", 전송: " + std::to_string(total_exported) +
                ", 실패: " + std::to_string(all_points.size() - total_exported) +
                ", 소요: " + std::to_string(duration.count()) + "ms");
            
        } catch (const std::exception& e) {
            failed_scans_.fetch_add(1);
            LogManager::getInstance().Error("스캔 루프 에러: " + std::string(e.what()));
        }
        
        // 5. 대기 (1초씩 체크하면서)
        int interval_sec;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            interval_sec = config_.export_interval_seconds;
        }
        
        for (int i = 0; i < interval_sec && !should_stop_.load(); ++i) {
            std::this_thread::sleep_for(seconds(1));
        }
    }
    
    LogManager::getInstance().Info("주기적 스캔 루프 종료");
}

std::vector<DataPoint> PeriodicExporter::scanDeviceData(int device_num) {
    std::vector<DataPoint> points;
    
    if (!redis_client_ || !redis_client_->isConnected()) {
        LogManager::getInstance().Warn("Redis 연결이 끊어짐");
        return points;
    }
    
    // device:{num}:* 패턴으로 키 조회
    std::string pattern = "device:" + std::to_string(device_num) + ":*";
    auto keys = redis_client_->keys(pattern);
    
    if (config_.enable_debug_log) {
        LogManager::getInstance().Debug(
            "패턴 " + pattern + " 검색 결과: " + std::to_string(keys.size()) + "개 키");
    }
    
    // 각 키의 값 읽기
    for (const auto& key : keys) {
        try {
            std::string json_str = redis_client_->get(key);
            if (json_str.empty()) continue;
            
            auto data = json::parse(json_str);
            
            DataPoint point;
            point.device_num = device_num;
            point.point_id = data.value("point_id", 0);
            point.point_name = data.value("point_name", "");
            
            // value는 문자열 또는 숫자일 수 있음
            if (data.contains("value")) {
                if (data["value"].is_string()) {
                    point.value = data["value"].get<std::string>();
                } else {
                    point.value = std::to_string(data["value"].get<double>());
                }
            }
            
            point.timestamp = data.value("timestamp", 0LL);
            point.quality = data.value("quality", 0);
            
            // point_name이 키에서 추출되어야 하는 경우
            if (point.point_name.empty()) {
                // device:1:temperature -> temperature
                size_t last_colon = key.find_last_of(':');
                if (last_colon != std::string::npos) {
                    point.point_name = key.substr(last_colon + 1);
                }
            }
            
            points.push_back(point);
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error(
                "키 " + key + " 파싱 실패: " + std::string(e.what()));
        }
    }
    
    return points;
}

std::vector<std::vector<DataPoint>> PeriodicExporter::createBatches(
    const std::vector<DataPoint>& points) {
    
    std::vector<std::vector<DataPoint>> batches;
    
    if (points.empty()) {
        return batches;
    }
    
    int batch_size;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        batch_size = config_.batch_size;
    }
    
    for (size_t i = 0; i < points.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, points.size());
        batches.emplace_back(points.begin() + i, points.begin() + end);
    }
    
    if (config_.enable_debug_log) {
        LogManager::getInstance().Debug(
            "배치 생성: " + std::to_string(batches.size()) + "개 배치");
    }
    
    return batches;
}

size_t PeriodicExporter::exportBatch(const std::vector<DataPoint>& batch) {
    if (!dynamic_target_manager_) {
        LogManager::getInstance().Warn("DynamicTargetManager가 초기화되지 않음");
        return 0;
    }
    
    size_t success_count = 0;
    
    for (const auto& point : batch) {
        if (should_stop_.load()) break;
        
        try {
            // DataPoint를 AlarmMessage로 변환
            auto alarm_msg = point.toAlarmMessage();
            
            // DynamicTargetManager를 통해 전송
            auto results = dynamic_target_manager_->sendAlarmToAllTargets(alarm_msg);
            
            // 결과 확인 (하나라도 성공하면 성공으로 간주)
            bool any_success = false;
            for (const auto& result : results) {
                if (result.success) {
                    any_success = true;
                    break;
                }
            }
            
            if (any_success) {
                success_count++;
            } else {
                if (config_.enable_debug_log) {
                    LogManager::getInstance().Debug(
                        "포인트 " + point.point_name + " 전송 실패");
                }
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error(
                "포인트 " + point.point_name + " 전송 중 예외: " + 
                std::string(e.what()));
        }
    }
    
    return success_count;
}

bool PeriodicExporter::initializeRedisConnection() {
    try {
        LogManager::getInstance().Info("Redis 연결 초기화 중...");
        
        redis_client_ = std::make_shared<RedisClientImpl>();
        
        if (!redis_client_->isConnected()) {
            LogManager::getInstance().Error("Redis 자동 연결 실패");
            return false;
        }
        
        LogManager::getInstance().Info("Redis 연결 성공");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Redis 연결 실패: " + std::string(e.what()));
        return false;
    }
}

bool PeriodicExporter::initializeDynamicTargetManager() {
    try {
        LogManager::getInstance().Info("DynamicTargetManager 초기화 중...");
        
        dynamic_target_manager_ = std::make_unique<PulseOne::CSP::DynamicTargetManager>(
            config_.target_config_file);
        
        if (!dynamic_target_manager_->start()) {
            LogManager::getInstance().Error("DynamicTargetManager 시작 실패");
            return false;
        }
        
        LogManager::getInstance().Info("DynamicTargetManager 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "DynamicTargetManager 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

} // namespace Periodic
} // namespace PulseOne