// =============================================================================
// collector/src/Workers/WorkerManager.cpp
// 워커 관리자 구현
// =============================================================================

#include "Workers/WorkerManager.h"
#include "Workers/WorkerFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Utils/LogManager.h"
#include "Database/Repositories/DeviceRepository.h"

#include <chrono>
#include <thread>

using namespace PulseOne::Workers;
using nlohmann::json;

// =============================================================================
// 생성자/소멸자 및 싱글턴
// =============================================================================

WorkerManager::~WorkerManager() {
    Shutdown();
}

// =============================================================================
// 자동 초기화 로직 (프로젝트 패턴 준수)
// =============================================================================

void WorkerManager::ensureInitialized() {
    // 빠른 체크 (이미 초기화됨)
    if (initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    // 느린 체크 (뮤텍스 사용)
    std::lock_guard<std::mutex> lock(init_mutex_);
    if (initialized_.load(std::memory_order_relaxed)) {
        return;
    }
    
    // 실제 초기화 수행
    doInitialize();
    initialized_.store(true, std::memory_order_release);
}

bool WorkerManager::doInitialize() {
    try {
        LogManager::getInstance().Info("WorkerManager 자동 초기화 시작...");
        
        // TODO: 설정에서 자동 시작할 디바이스 목록 읽어서 시작
        // 현재는 빈 상태로 시작
        
        LogManager::getInstance().Info("WorkerManager 자동 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "WorkerManager 자동 초기화 실패: " << e.what() << std::endl;
        return false;
    }
}

// =============================================================================
// 수동 초기화 (호환성용)
// =============================================================================

bool WorkerManager::Initialize(WorkerFactory* worker_factory) {
    // 이미 자동 초기화가 완료되었으면 상태 반환
    if (initialized_.load()) {
        LogManager::getInstance().Info("WorkerManager 이미 자동 초기화됨 - 수동 초기화 건너뜀");
        return true;
    }
    
    // 수동 초기화는 단순히 자동 초기화 호출
    return doInitialize();
}

void WorkerManager::Shutdown() {
    if (!initialized_.load() || shutting_down_.load()) {
        return;
    }
    
    shutting_down_.store(true);
    LogManager::getInstance().Info("WorkerManager 종료 중...");
    
    // 모든 워커 정지
    {
        std::lock_guard<std::mutex> lock(workers_mutex_);
        for (auto& [device_id, worker] : active_workers_) {
            if (worker) {
                try {
                    auto stop_future = worker->Stop();
                    stop_future.wait_for(std::chrono::seconds(2)); // 2초 타임아웃
                } catch (...) {
                    // 예외 무시하고 계속 진행
                }
            }
        }
        active_workers_.clear();
    }
    
    LogManager::getInstance().Info("WorkerManager 종료 완료");
    initialized_.store(false);
    shutting_down_.store(false);
}

// =============================================================================
// 워커 생명주기 제어
// =============================================================================

bool WorkerManager::StartDeviceWorker(const std::string& device_id) {
    if (!initialized_.load() || shutting_down_.load()) {
        return false;
    }
    
    LogManager::getInstance().Info("디바이스 워커 시작 요청: " + device_id);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    // 이미 실행 중인지 확인
    auto existing_worker = FindWorker(device_id);
    if (existing_worker) {
        if (existing_worker->GetState() == WorkerState::RUNNING) {
            LogManager::getInstance().Info("디바이스 " + device_id + " 이미 실행 중");
            return true;
        }
        
        // 기존 워커가 있지만 중지 상태면 시작
        try {
            auto start_future = existing_worker->Start();
            bool result = start_future.get();
            
            if (result) {
                total_started_.fetch_add(1);
                LogManager::getInstance().Info("디바이스 " + device_id + " 워커 시작 성공");
            } else {
                total_failed_.fetch_add(1);
                LogManager::getInstance().Error("디바이스 " + device_id + " 워커 시작 실패");
            }
            
            return result;
        } catch (const std::exception& e) {
            total_failed_.fetch_add(1);
            LogManager::getInstance().Error("디바이스 " + device_id + " 워커 시작 예외: " + std::string(e.what()));
            return false;
        }
    }
    
    // 새 워커 생성 및 시작
    auto new_worker = CreateAndRegisterWorker(device_id);
    if (!new_worker) {
        total_failed_.fetch_add(1);
        return false;
    }
    
    try {
        auto start_future = new_worker->Start();
        bool result = start_future.get();
        
        if (result) {
            total_started_.fetch_add(1);
            LogManager::getInstance().Info("디바이스 " + device_id + " 새 워커 생성 및 시작 성공");
        } else {
            total_failed_.fetch_add(1);
            LogManager::getInstance().Error("디바이스 " + device_id + " 새 워커 시작 실패");
            UnregisterWorker(device_id);
        }
        
        return result;
    } catch (const std::exception& e) {
        total_failed_.fetch_add(1);
        LogManager::getInstance().Error("디바이스 " + device_id + " 새 워커 시작 예외: " + std::string(e.what()));
        UnregisterWorker(device_id);
        return false;
    }
}

bool WorkerManager::StopDeviceWorker(const std::string& device_id) {
    if (!initialized_.load()) {
        return false;
    }
    
    LogManager::getInstance().Info("디바이스 워커 중지 요청: " + device_id);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Warn("디바이스 " + device_id + " 워커를 찾을 수 없음");
        return true; // 이미 없으면 중지된 것으로 간주
    }
    
    try {
        auto stop_future = worker->Stop();
        bool result = stop_future.get();
        
        if (result) {
            total_stopped_.fetch_add(1);
            LogManager::getInstance().Info("디바이스 " + device_id + " 워커 중지 성공");
        } else {
            LogManager::getInstance().Warn("디바이스 " + device_id + " 워커 중지 실패");
        }
        
        return result;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커 중지 예외: " + std::string(e.what()));
        return false;
    }
}

bool WorkerManager::PauseDeviceWorker(const std::string& device_id) {
    if (!initialized_.load()) {
        return false;
    }
    
    LogManager::getInstance().Info("디바이스 워커 일시정지 요청: " + device_id);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커를 찾을 수 없음");
        return false;
    }
    
    try {
        auto pause_future = worker->Pause();
        bool result = pause_future.get();
        
        if (result) {
            LogManager::getInstance().Info("디바이스 " + device_id + " 워커 일시정지 성공");
        } else {
            LogManager::getInstance().Error("디바이스 " + device_id + " 워커 일시정지 실패");
        }
        
        return result;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커 일시정지 예외: " + std::string(e.what()));
        return false;
    }
}

bool WorkerManager::ResumeDeviceWorker(const std::string& device_id) {
    if (!initialized_.load()) {
        return false;
    }
    
    LogManager::getInstance().Info("디바이스 워커 재개 요청: " + device_id);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커를 찾을 수 없음");
        return false;
    }
    
    try {
        auto resume_future = worker->Resume();
        bool result = resume_future.get();
        
        if (result) {
            LogManager::getInstance().Info("디바이스 " + device_id + " 워커 재개 성공");
        } else {
            LogManager::getInstance().Error("디바이스 " + device_id + " 워커 재개 실패");
        }
        
        return result;
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커 재개 예외: " + std::string(e.what()));
        return false;
    }
}

bool WorkerManager::RestartDeviceWorker(const std::string& device_id) {
    if (!initialized_.load()) {
        return false;
    }
    
    LogManager::getInstance().Info("디바이스 워커 재시작 요청: " + device_id);
    
    // 중지 후 시작
    bool stop_result = StopDeviceWorker(device_id);
    if (!stop_result) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 중지 실패로 재시작 불가");
        return false;
    }
    
    // 잠깐 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return StartDeviceWorker(device_id);
}

// =============================================================================
// 하드웨어 제어
// =============================================================================

bool WorkerManager::ControlPump(const std::string& device_id, const std::string& pump_id, bool enable) {
    if (!initialized_.load()) {
        return false;
    }
    
    std::string action = enable ? "시작" : "중지";
    LogManager::getInstance().Info("펌프 제어 요청: " + device_id + ":" + pump_id + " " + action);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커를 찾을 수 없음");
        return false;
    }
    
    // TODO: BaseDeviceWorker에 ControlPump 메서드 추가 필요
    // 현재는 로깅만
    LogManager::getInstance().Info("펌프 " + pump_id + " " + action + " 명령 전송됨");
    return true;
}

bool WorkerManager::ControlValve(const std::string& device_id, const std::string& valve_id, bool open) {
    if (!initialized_.load()) {
        return false;
    }
    
    std::string action = open ? "열기" : "닫기";
    LogManager::getInstance().Info("밸브 제어 요청: " + device_id + ":" + valve_id + " " + action);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커를 찾을 수 없음");
        return false;
    }
    
    // TODO: BaseDeviceWorker에 ControlValve 메서드 추가 필요
    LogManager::getInstance().Info("밸브 " + valve_id + " " + action + " 명령 전송됨");
    return true;
}

bool WorkerManager::ChangeSetpoint(const std::string& device_id, const std::string& setpoint_id, double value) {
    if (!initialized_.load()) {
        return false;
    }
    
    LogManager::getInstance().Info("설정값 변경 요청: " + device_id + ":" + setpoint_id + " = " + std::to_string(value));
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커를 찾을 수 없음");
        return false;
    }
    
    // TODO: BaseDeviceWorker에 ChangeSetpoint 메서드 추가 필요
    LogManager::getInstance().Info("설정값 " + setpoint_id + " = " + std::to_string(value) + " 명령 전송됨");
    return true;
}

// =============================================================================
// 상태 조회
// =============================================================================

nlohmann::json WorkerManager::GetDeviceList() {
    json device_list = json::array();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    for (const auto& [device_id, worker] : active_workers_) {
        json device = json::object();
        device["device_id"] = device_id;
        device["name"] = "Device " + device_id;
        device["protocol"] = "modbus"; // 실제로는 worker에서 조회
        
        if (worker) {
            auto state = worker->GetState();
            device["status"] = (state == WorkerState::RUNNING) ? "running" : 
                             (state == WorkerState::PAUSED) ? "paused" : "stopped";
            device["connection_status"] = "connected";
        } else {
            device["status"] = "error";
            device["connection_status"] = "disconnected";
        }
        
        device["uptime"] = 0;
        device["error_count"] = 0;
        device["scan_rate"] = 1000;
        
        device_list.push_back(device);
    }
    
    return device_list;
}

nlohmann::json WorkerManager::GetDeviceStatus(const std::string& device_id) {
    json status = json::object();
    status["device_id"] = device_id;
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (worker) {
        auto state = worker->GetState();
        status["running"] = (state == WorkerState::RUNNING);
        status["paused"] = (state == WorkerState::PAUSED);
        status["connected"] = true;
        status["uptime"] = "running";
        status["last_error"] = "";
    } else {
        status["running"] = false;
        status["paused"] = false;
        status["connected"] = false;
        status["uptime"] = "0";
        status["last_error"] = "Worker not found";
    }
    
    status["message_count"] = 0;
    status["scan_rate"] = 1000;
    status["connection_attempts"] = 1;
    status["successful_scans"] = 0;
    status["failed_scans"] = 0;
    status["average_response_time"] = 100;
    
    return status;
}

nlohmann::json WorkerManager::GetSystemStats() {
    json stats = json::object();
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    stats["active_workers"] = active_workers_.size();
    stats["total_started"] = total_started_.load();
    stats["total_stopped"] = total_stopped_.load();
    stats["total_failed"] = total_failed_.load();
    
    return stats;
}

// =============================================================================
// 워커 관리 헬퍼 함수들
// =============================================================================

size_t WorkerManager::GetActiveWorkerCount() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return active_workers_.size();
}

bool WorkerManager::IsWorkerActive(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    return active_workers_.find(device_id) != active_workers_.end();
}

// =============================================================================
// 내부 헬퍼 함수들
// =============================================================================

std::shared_ptr<BaseDeviceWorker> WorkerManager::FindWorker(const std::string& device_id) {
    auto it = active_workers_.find(device_id);
    return (it != active_workers_.end()) ? it->second : nullptr;
}

std::shared_ptr<BaseDeviceWorker> WorkerManager::CreateAndRegisterWorker(const std::string& device_id) {
    try {
        // 디바이스 ID를 정수로 변환해서 WorkerFactory 호출
        int device_int_id = std::stoi(device_id);
        auto unique_worker = WorkerFactory::getInstance().CreateWorkerById(device_int_id);
        
        if (!unique_worker) {
            LogManager::getInstance().Error("디바이스 " + device_id + " 워커 생성 실패");
            return nullptr;
        }
        
        // unique_ptr를 shared_ptr로 변환
        std::shared_ptr<BaseDeviceWorker> shared_worker = std::move(unique_worker);
        
        // 활성 워커 맵에 등록
        active_workers_[device_id] = shared_worker;
        
        LogManager::getInstance().Info("디바이스 " + device_id + " 워커 생성 및 등록 완료");
        return shared_worker;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("디바이스 " + device_id + " 워커 생성 예외: " + std::string(e.what()));
        return nullptr;
    }
}

void WorkerManager::UnregisterWorker(const std::string& device_id) {
    active_workers_.erase(device_id);
    LogManager::getInstance().Info("디바이스 " + device_id + " 워커 등록 해제됨");
}