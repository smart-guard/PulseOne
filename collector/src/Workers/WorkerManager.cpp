// =============================================================================
// collector/src/Workers/WorkerManager.cpp  
// WorkerManager 구현 파일 - 기존 패턴 준수
// =============================================================================

#include "Workers/WorkerManager.h"
#include "Workers/WorkerFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Utils/LogManager.h"
#include "Database/RepositoryFactory.h" 
#include "Database/DatabaseManager.h" 

#include <chrono>
#include <thread>
#include <algorithm>

namespace PulseOne {
namespace Workers {

using nlohmann::json;

// =============================================================================
// 싱글톤 및 초기화
// =============================================================================

WorkerManager& WorkerManager::getInstance() {
    static WorkerManager instance;
    return instance;
}

WorkerManager::~WorkerManager() {
    if (initialized_.load()) {
        Shutdown();
    }
}

void WorkerManager::ensureInitialized() {
    if (initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (initialized_.load(std::memory_order_relaxed)) {
        return;
    }
    
    if (doInitialize()) {
        initialized_.store(true, std::memory_order_release);
    }
}


bool WorkerManager::doInitialize() {
    try {
        LogManager::getInstance().Info("WorkerManager 자동 초기화 시작...");
        
        // WorkerFactory 초기화 및 의존성 주입
        auto& worker_factory = WorkerFactory::getInstance();
        if (!worker_factory.Initialize()) {
            LogManager::getInstance().Error("WorkerFactory 초기화 실패");
            return false;
        }
        
        // Repository 의존성 주입 - 🔧 네임스페이스 명시
        auto& repository_factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto repo_factory_shared = std::shared_ptr<PulseOne::Database::RepositoryFactory>(
            &repository_factory, [](PulseOne::Database::RepositoryFactory*){}
        );
        worker_factory.SetRepositoryFactory(repo_factory_shared);
        
        // Database Client 의존성 주입 - 🔧 네임스페이스 명시
        auto& db_manager = PulseOne::DatabaseManager::getInstance();
        auto redis_shared = std::shared_ptr<RedisClient>(
            db_manager.getRedisClient(), [](RedisClient*){}
        );
        auto influx_shared = std::shared_ptr<InfluxClient>(
            db_manager.getInfluxClient(), [](InfluxClient*){}
        );
        worker_factory.SetDatabaseClients(redis_shared, influx_shared);
        
        LogManager::getInstance().Info("WorkerManager 자동 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("WorkerManager 자동 초기화 실패: " + std::string(e.what()));
        return false;
    }
}


void WorkerManager::Shutdown() {
    if (shutting_down_.load()) {
        return;
    }
    
    shutting_down_.store(true);
    LogManager::getInstance().Info("WorkerManager 종료 시작");
    
    StopAllWorkers();
    
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        active_workers_.clear();
    }
    
    initialized_.store(false);
    shutting_down_.store(false);
    LogManager::getInstance().Info("WorkerManager 종료 완료");
}

// =============================================================================
// 워커 생명주기 관리
// =============================================================================

bool WorkerManager::StartWorker(const std::string& device_id) {
    ensureInitialized();
    
    if (shutting_down_.load()) {
        return false;
    }
    
    LogManager::getInstance().Info("워커 시작 요청: " + device_id);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    // 기존 워커가 있는지 확인
    auto existing_worker = FindWorker(device_id);
    if (existing_worker) {
        auto current_state = existing_worker->GetState();
        
        if (current_state == WorkerState::RUNNING) {
            LogManager::getInstance().Info("워커 이미 실행 중: " + device_id);
            return true;
        }
        
        // 기존 워커가 있지만 중지 상태면 시작
        try {
            auto start_future = existing_worker->Start();
            bool result = start_future.get();
            
            if (result) {
                total_started_.fetch_add(1);
                LogManager::getInstance().Info("기존 워커 시작 성공: " + device_id);
            } else {
                total_errors_.fetch_add(1);
                LogManager::getInstance().Error("기존 워커 시작 실패: " + device_id);
            }
            
            return result;
        } catch (const std::exception& e) {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("기존 워커 시작 예외: " + device_id + " - " + e.what());
            UnregisterWorker(device_id);
        }
    }
    
    // 새 워커 생성 및 시작
    auto new_worker = CreateAndRegisterWorker(device_id);
    if (!new_worker) {
        total_errors_.fetch_add(1);
        return false;
    }
    
    try {
        auto start_future = new_worker->Start();
        bool result = start_future.get();
        
        if (result) {
            total_started_.fetch_add(1);
            LogManager::getInstance().Info("새 워커 생성 및 시작 성공: " + device_id);
        } else {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("새 워커 시작 실패: " + device_id);
            UnregisterWorker(device_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("새 워커 시작 예외: " + device_id + " - " + e.what());
        UnregisterWorker(device_id);
        return false;
    }
}

bool WorkerManager::StopWorker(const std::string& device_id) {
    ensureInitialized();
    
    LogManager::getInstance().Info("워커 중지 요청: " + device_id);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Warn("워커를 찾을 수 없음: " + device_id);
        return true; // 이미 없으면 중지된 것으로 간주
    }
    
    try {
        auto stop_future = worker->Stop();
        bool result = stop_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        
        if (result && stop_future.get()) {
            total_stopped_.fetch_add(1);
            LogManager::getInstance().Info("워커 중지 성공: " + device_id);
            return true;
        } else {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("워커 중지 실패 또는 타임아웃: " + device_id);
            return false;
        }
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("워커 중지 예외: " + device_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::PauseWorker(const std::string& device_id) {
    ensureInitialized();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("워커를 찾을 수 없음: " + device_id);
        return false;
    }
    
    try {
        auto pause_future = worker->Pause();
        bool result = pause_future.get();
        
        if (result) {
            LogManager::getInstance().Info("워커 일시정지 성공: " + device_id);
        } else {
            LogManager::getInstance().Error("워커 일시정지 실패: " + device_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("워커 일시정지 예외: " + device_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::ResumeWorker(const std::string& device_id) {
    ensureInitialized();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("워커를 찾을 수 없음: " + device_id);
        return false;
    }
    
    try {
        auto resume_future = worker->Resume();
        bool result = resume_future.get();
        
        if (result) {
            LogManager::getInstance().Info("워커 재개 성공: " + device_id);
        } else {
            LogManager::getInstance().Error("워커 재개 실패: " + device_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("워커 재개 예외: " + device_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::RestartWorker(const std::string& device_id) {
    ensureInitialized();
    
    LogManager::getInstance().Info("워커 재시작 요청: " + device_id);
    
    // 먼저 중지
    if (!StopWorker(device_id)) {
        LogManager::getInstance().Warn("워커 중지 실패, 강제 제거 후 재시작: " + device_id);
        UnregisterWorker(device_id);
    }
    
    // 잠시 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 다시 시작
    return StartWorker(device_id);
}

int WorkerManager::StartAllActiveWorkers() {
    ensureInitialized();
    
    LogManager::getInstance().Info("모든 활성 워커 시작 요청");
    
    auto active_device_ids = LoadActiveDeviceIds();
    int success_count = 0;
    
    for (const auto& device_id : active_device_ids) {
        if (StartWorker(device_id)) {
            success_count++;
        }
        
        // 시스템 부하 방지를 위한 짧은 지연
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LogManager::getInstance().Info("활성 워커 시작 완료: " + 
                                  std::to_string(success_count) + "/" + 
                                  std::to_string(active_device_ids.size()));
    
    return success_count;
}

void WorkerManager::StopAllWorkers() {
    LogManager::getInstance().Info("모든 워커 중지 요청");
    
    std::vector<std::string> device_ids;
    
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        for (const auto& [device_id, worker] : active_workers_) {
            device_ids.push_back(device_id);
        }
    }
    
    int stopped_count = 0;
    for (const auto& device_id : device_ids) {
        if (StopWorker(device_id)) {
            stopped_count++;
        }
    }
    
    LogManager::getInstance().Info("워커 중지 완료: " + 
                                  std::to_string(stopped_count) + "/" + 
                                  std::to_string(device_ids.size()));
}

// =============================================================================
// 디바이스 제어
// =============================================================================

bool WorkerManager::ControlDigitalOutput(const std::string& device_id, 
                                        const std::string& output_id, 
                                        bool enable) {
    ensureInitialized();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("워커를 찾을 수 없음: " + device_id);
        return false;
    }
    
    total_control_commands_.fetch_add(1);
    
    try {
        // 🔧 수정: 실제 존재하는 메서드 사용
        bool result = worker->ControlPump(output_id, enable);
        
        if (result) {
            LogManager::getInstance().Info("디지털 출력 제어 성공: " + device_id + "/" + output_id + " -> " + (enable ? "ON" : "OFF"));
        } else {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("디지털 출력 제어 실패: " + device_id + "/" + output_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("디지털 출력 제어 예외: " + device_id + "/" + output_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::ControlAnalogOutput(const std::string& device_id, 
                                       const std::string& output_id, 
                                       double value) {
    ensureInitialized();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("워커를 찾을 수 없음: " + device_id);
        return false;
    }
    
    total_control_commands_.fetch_add(1);
    
    try {
        bool result = worker->WriteDataPoint(output_id, std::to_string(value));
        
        if (result) {
            LogManager::getInstance().Info("아날로그 출력 제어 성공: " + device_id + "/" + output_id + " -> " + std::to_string(value));
        } else {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("아날로그 출력 제어 실패: " + device_id + "/" + output_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("아날로그 출력 제어 예외: " + device_id + "/" + output_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::ChangeParameter(const std::string& device_id, 
                                   const std::string& parameter_id, 
                                   double value) {
    ensureInitialized();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("워커를 찾을 수 없음: " + device_id);
        return false;
    }
    
    total_control_commands_.fetch_add(1);
    
    try {
        bool result = worker->SetParameterValue(parameter_id, value);
        
        if (result) {
            LogManager::getInstance().Info("파라미터 변경 성공: " + device_id + "/" + parameter_id + " -> " + std::to_string(value));
        } else {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("파라미터 변경 실패: " + device_id + "/" + parameter_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("파라미터 변경 예외: " + device_id + "/" + parameter_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::WriteDataPoint(const std::string& device_id, 
                                  const std::string& point_id, 
                                  const std::string& value) {
    ensureInitialized();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("워커를 찾을 수 없음: " + device_id);
        return false;
    }
    
    total_control_commands_.fetch_add(1);
    
    try {
        // 문자열 값을 double로 변환해서 설정
        double numeric_value = std::stod(value);
        bool result = worker->SetOutputValue(point_id, numeric_value);
        
        if (result) {
            LogManager::getInstance().Debug("데이터포인트 쓰기 성공: " + device_id + "/" + point_id + " -> " + value);
        } else {
            total_errors_.fetch_add(1);
            LogManager::getInstance().Error("데이터포인트 쓰기 실패: " + device_id + "/" + point_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("데이터포인트 쓰기 예외: " + device_id + "/" + point_id + " - " + e.what());
        return false;
    }
}

// =============================================================================
// 상태 조회
// =============================================================================

json WorkerManager::GetWorkerStatus(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        return json{
            {"error", "Worker not found"}, 
            {"device_id", device_id}
        };
    }
    
    json status = json::object();
    status["device_id"] = device_id;
    status["state"] = static_cast<int>(worker->GetState());
    status["is_running"] = (worker->GetState() == WorkerState::RUNNING);
    status["is_paused"] = (worker->GetState() == WorkerState::PAUSED);
    status["connection_status"] = "connected"; // TODO: 실제 연결 상태 확인
    
    // 현재 시간을 ISO 문자열로 반환
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    status["last_update"] = ss.str();
    
    return status;
}

json WorkerManager::GetWorkerList() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    json worker_list = json::array();
    
    for (const auto& [device_id, worker] : active_workers_) {
        if (!worker) continue;
        
        json worker_info = json::object();
        worker_info["device_id"] = device_id;
        worker_info["state"] = static_cast<int>(worker->GetState());
        worker_info["is_running"] = (worker->GetState() == WorkerState::RUNNING);
        worker_info["is_paused"] = (worker->GetState() == WorkerState::PAUSED);
        
        worker_list.push_back(worker_info);
    }
    
    return worker_list;
}

json WorkerManager::GetManagerStats() const {
    json stats = json::object();
    
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        stats["active_workers"] = active_workers_.size();
    }
    
    stats["total_started"] = total_started_.load();
    stats["total_stopped"] = total_stopped_.load();
    stats["total_control_commands"] = total_control_commands_.load();
    stats["total_errors"] = total_errors_.load();
    stats["is_initialized"] = initialized_.load();
    
    return stats;
}

size_t WorkerManager::GetActiveWorkerCount() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return active_workers_.size();
}

bool WorkerManager::HasWorker(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return active_workers_.find(device_id) != active_workers_.end();
}

bool WorkerManager::IsWorkerRunning(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    return worker ? (worker->GetState() == WorkerState::RUNNING) : false;
}

// =============================================================================
// 워커 등록 관리
// =============================================================================

void WorkerManager::RegisterWorker(const std::string& device_id, 
                                  std::shared_ptr<BaseDeviceWorker> worker) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    active_workers_[device_id] = worker;
    LogManager::getInstance().Debug("워커 수동 등록: " + device_id);
}

void WorkerManager::UnregisterWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    active_workers_.erase(device_id);
    LogManager::getInstance().Debug("워커 등록 해제: " + device_id);
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::shared_ptr<BaseDeviceWorker> WorkerManager::FindWorker(const std::string& device_id) const {
    auto it = active_workers_.find(device_id);
    return (it != active_workers_.end()) ? it->second : nullptr;
}

std::shared_ptr<BaseDeviceWorker> WorkerManager::CreateAndRegisterWorker(const std::string& device_id) {
    try {
        int device_int_id = std::stoi(device_id);
        
        // WorkerFactory에서 unique_ptr로 생성
        auto unique_worker = WorkerFactory::getInstance().CreateWorkerById(device_int_id);
        
        if (!unique_worker) {
            LogManager::getInstance().Error("워커 생성 실패: " + device_id);
            return nullptr;
        }
        
        // shared_ptr로 변환하여 소유권을 가져옴
        std::shared_ptr<BaseDeviceWorker> shared_worker = std::move(unique_worker);
        active_workers_[device_id] = shared_worker;
        
        LogManager::getInstance().Info("워커 생성 및 등록 완료: " + device_id);
        return shared_worker;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("워커 생성 예외: " + device_id + " - " + e.what());
        return nullptr;
    }
}

std::vector<std::string> WorkerManager::LoadActiveDeviceIds() {
    std::vector<std::string> device_ids;
    
    try {
        // TODO: 데이터베이스에서 활성화된 디바이스 목록 조회
        // 현재는 빈 목록 반환
        LogManager::getInstance().Debug("활성 디바이스 목록 로딩 - TODO: DB 연동 구현 필요");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("활성 디바이스 목록 조회 실패: " + std::string(e.what()));
    }
    
    return device_ids;
}

} // namespace Workers
} // namespace PulseOne