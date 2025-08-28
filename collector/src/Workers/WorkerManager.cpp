// =============================================================================
// collector/src/Workers/WorkerManager.cpp - 간단한 구현
// =============================================================================

#include "Workers/WorkerManager.h"
#include "Workers/WorkerFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Database/RepositoryFactory.h"
#include "Utils/LogManager.h"

#include <thread>
#include <chrono>

namespace PulseOne::Workers {

using nlohmann::json;

// =============================================================================
// 싱글톤
// =============================================================================

WorkerManager& WorkerManager::getInstance() {
    static WorkerManager instance;
    return instance;
}

WorkerManager::~WorkerManager() {
    StopAllWorkers();
}

// =============================================================================
// Worker 생명주기 관리
// =============================================================================

bool WorkerManager::StartWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    // 기존 Worker가 있으면 확인
    auto existing = FindWorker(device_id);
    if (existing && existing->GetState() == WorkerState::RUNNING) {
        LogManager::getInstance().Info("Worker 이미 실행중: " + device_id);
        return true;
    }
    
    // 기존 Worker 정리
    if (existing) {
        UnregisterWorker(device_id);
    }
    
    // 새 Worker 생성 및 시작
    auto worker = CreateAndRegisterWorker(device_id);
    if (!worker) {
        total_errors_.fetch_add(1);
        return false;
    }
    
    try {
        auto start_future = worker->Start();
        bool result = start_future.get();
        
        if (result) {
            total_started_.fetch_add(1);
            LogManager::getInstance().Info("Worker 시작 완료: " + device_id);
        } else {
            total_errors_.fetch_add(1);
            UnregisterWorker(device_id);
            LogManager::getInstance().Error("Worker 시작 실패: " + device_id);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        UnregisterWorker(device_id);
        LogManager::getInstance().Error("Worker 시작 예외: " + device_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::StopWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Warn("Worker 없음: " + device_id);
        return true;
    }
    
    try {
        auto stop_future = worker->Stop();
        bool result = stop_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        
        if (result) {
            total_stopped_.fetch_add(1);
            LogManager::getInstance().Info("Worker 중지 완료: " + device_id);
        } else {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("Worker 중지 타임아웃: " + device_id);
        }
        
        UnregisterWorker(device_id);
        return result;
        
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("Worker 중지 예외: " + device_id + " - " + e.what());
        UnregisterWorker(device_id);
        return false;
    }
}

bool WorkerManager::RestartWorker(const std::string& device_id) {
    LogManager::getInstance().Info("Worker 재시작: " + device_id);
    
    StopWorker(device_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return StartWorker(device_id);
}

bool WorkerManager::ReloadWorker(const std::string& device_id) {
    LogManager::getInstance().Info("Worker 설정 리로드: " + device_id);
    
    // WorkerFactory에서 프로토콜 리로드
    if (worker_factory_) {
        worker_factory_->ReloadProtocols();
    }
    
    // Worker 재시작
    return RestartWorker(device_id);
}

// =============================================================================
// 대량 작업
// =============================================================================

int WorkerManager::StartAllActiveWorkers() {
    LogManager::getInstance().Info("모든 활성 Worker 시작...");
    
    try {
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        auto device_repo = repo_factory.getDeviceRepository();
        
        if (!device_repo) {
            LogManager::getInstance().Error("DeviceRepository 없음");
            return 0;
        }
        
        // findEnabled 메서드가 없으므로 findAll로 대체하고 필터링
        auto all_devices = device_repo->findAll();
        
        int success_count = 0;
        
        for (const auto& device : all_devices) {
            if (!device.isEnabled()) continue;  // 활성화된 디바이스만
            
            std::string device_id = std::to_string(device.getId());
            
            if (StartWorker(device_id)) {
                success_count++;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        LogManager::getInstance().Info("Worker 시작 완료: " + std::to_string(success_count) + "/" + 
                                     std::to_string(all_devices.size()));
        return success_count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("활성 Worker 시작 실패: " + std::string(e.what()));
        return 0;
    }
}

void WorkerManager::StopAllWorkers() {
    LogManager::getInstance().Info("모든 Worker 중지...");
    
    std::vector<std::string> device_ids;
    
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        for (const auto& [device_id, worker] : workers_) {
            device_ids.push_back(device_id);
        }
    }
    
    for (const auto& device_id : device_ids) {
        StopWorker(device_id);
    }
    
    LogManager::getInstance().Info("모든 Worker 중지 완료");
}

// =============================================================================
// 디바이스 제어
// =============================================================================

bool WorkerManager::WriteDataPoint(const std::string& device_id, const std::string& point_id, const std::string& value) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("Worker 없음: " + device_id);
        return false;
    }
    
    try {
        // BaseDeviceWorker에는 WriteDataPoint가 없고 AddDataPoint만 있음
        // 임시로 구현하거나 실제 메서드 확인 필요
        LogManager::getInstance().Info("데이터 포인트 쓰기 요청: " + device_id + "/" + point_id + " = " + value);
        // TODO: 실제 Worker에 write 기능 구현 필요
        return true; // 임시
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("데이터 쓰기 예외: " + device_id + "/" + point_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::ControlOutput(const std::string& device_id, const std::string& output_id, bool enable) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("Worker 없음: " + device_id);
        return false;
    }
    
    try {
        // BaseDeviceWorker에는 ControlPump가 없음
        // 임시로 구현하거나 실제 메서드 확인 필요
        LogManager::getInstance().Info("출력 제어 요청: " + device_id + "/" + output_id + " -> " + 
                                     (enable ? "ON" : "OFF"));
        // TODO: 실제 Worker에 control 기능 구현 필요
        return true; // 임시
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("출력 제어 예외: " + device_id + "/" + output_id + " - " + e.what());
        return false;
    }
}

// =============================================================================
// 상태 조회
// =============================================================================

nlohmann::json WorkerManager::GetWorkerStatus(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        return {{"error", "Worker not found"}, {"device_id", device_id}};
    }
    
    return {
        {"device_id", device_id},
        {"state", static_cast<int>(worker->GetState())},
        {"is_running", worker->GetState() == WorkerState::RUNNING},
        {"is_paused", worker->GetState() == WorkerState::PAUSED},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

nlohmann::json WorkerManager::GetWorkerList() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    nlohmann::json worker_list = nlohmann::json::array();
    
    for (const auto& [device_id, worker] : workers_) {
        if (!worker) continue;
        
        worker_list.push_back({
            {"device_id", device_id},
            {"state", static_cast<int>(worker->GetState())},
            {"is_running", worker->GetState() == WorkerState::RUNNING},
            {"is_paused", worker->GetState() == WorkerState::PAUSED}
        });
    }
    
    return worker_list;
}

nlohmann::json WorkerManager::GetManagerStats() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    return {
        {"active_workers", workers_.size()},
        {"total_started", total_started_.load()},
        {"total_stopped", total_stopped_.load()},
        {"total_errors", total_errors_.load()}
    };
}

size_t WorkerManager::GetActiveWorkerCount() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return workers_.size();
}

bool WorkerManager::HasWorker(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return workers_.find(device_id) != workers_.end();
}

bool WorkerManager::IsWorkerRunning(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    auto worker = FindWorker(device_id);
    return worker ? (worker->GetState() == WorkerState::RUNNING) : false;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::shared_ptr<BaseDeviceWorker> WorkerManager::FindWorker(const std::string& device_id) const {
    auto it = workers_.find(device_id);
    return (it != workers_.end()) ? it->second : nullptr;
}

std::shared_ptr<BaseDeviceWorker> WorkerManager::CreateAndRegisterWorker(const std::string& device_id) {
    if (device_id.empty()) {
        LogManager::getInstance().Error("빈 device_id");
        return nullptr;
    }
    
    try {
        // device_id 검증 (숫자만 허용)
        int device_int_id;
        try {
            device_int_id = std::stoi(device_id);
            if (device_int_id <= 0) {
                LogManager::getInstance().Error("잘못된 device_id: " + device_id);
                return nullptr;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("device_id 파싱 실패: " + device_id);
            return nullptr;
        }
        
        // WorkerFactory 초기화 (thread-safe)
        if (!worker_factory_) {
            static std::mutex factory_init_mutex;
            std::lock_guard<std::mutex> lock(factory_init_mutex);
            if (!worker_factory_) {
                worker_factory_ = std::make_unique<WorkerFactory>();
            }
        }
        
        // Worker 생성 (안전한 변환)
        auto unique_worker = worker_factory_->CreateWorkerById(device_int_id);
        if (!unique_worker) {
            LogManager::getInstance().Error("Worker 생성 실패: " + device_id);
            return nullptr;
        }
        
        // unique_ptr → shared_ptr 안전 변환
        std::shared_ptr<BaseDeviceWorker> shared_worker(std::move(unique_worker));
        if (!shared_worker) {
            LogManager::getInstance().Error("shared_ptr 변환 실패: " + device_id);
            return nullptr;
        }
        
        // 등록
        RegisterWorker(device_id, shared_worker);
        return shared_worker;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("CreateAndRegisterWorker 예외: " + device_id + " - " + e.what());
        return nullptr;
    }
}

void WorkerManager::RegisterWorker(const std::string& device_id, std::shared_ptr<BaseDeviceWorker> worker) {
    workers_[device_id] = worker;
    LogManager::getInstance().Debug("Worker 등록: " + device_id);
}

void WorkerManager::UnregisterWorker(const std::string& device_id) {
    workers_.erase(device_id);
    LogManager::getInstance().Debug("Worker 등록 해제: " + device_id);
}

} // namespace PulseOne::Workers