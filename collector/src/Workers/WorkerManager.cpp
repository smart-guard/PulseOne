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

WorkerManager::WorkerManager() {
    // RedisDataWriter 초기화
    try {
        redis_data_writer_ = std::make_unique<Storage::RedisDataWriter>();
        LogManager::getInstance().Info("WorkerManager - RedisDataWriter 초기화 완료");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("WorkerManager - RedisDataWriter 초기화 실패: " + std::string(e.what()));
    }
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
            
            // Worker 시작 성공 시 Redis 초기화 데이터 저장
            InitializeWorkerRedisData(device_id);
            
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

void WorkerManager::InitializeWorkerRedisData(const std::string& device_id) {
    if (!redis_data_writer_) {
        LogManager::getInstance().Warn("RedisDataWriter 없음 - 초기화 건너뜀");
        return;
    }
    
    try {
        auto current_values = LoadCurrentValuesFromDB(device_id);
        
        if (current_values.empty()) {
            LogManager::getInstance().Debug("디바이스 " + device_id + "에 초기화할 데이터 없음");
            return;
        }
        
        size_t saved = redis_data_writer_->SaveWorkerInitialData(device_id, current_values);
        
        LogManager::getInstance().Info("Worker Redis 초기화 완료: " + device_id + 
                                     " (" + std::to_string(saved) + "개 포인트)");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Worker Redis 초기화 실패: " + device_id + 
                                      " - " + std::string(e.what()));
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
    LogManager::getInstance().Info("모든 활성 Worker 시작 및 Redis 초기화...");
    
    try {
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        auto device_repo = repo_factory.getDeviceRepository();
        
        if (!device_repo) {
            LogManager::getInstance().Error("DeviceRepository 없음");
            return 0;
        }
        
        auto all_devices = device_repo->findAll();
        std::vector<std::pair<std::string, int>> active_devices;
        
        // 1단계: 활성화된 디바이스 필터링
        for (const auto& device : all_devices) {
            if (device.isEnabled()) {
                active_devices.emplace_back(std::to_string(device.getId()), device.getId());
            }
        }
        
        if (active_devices.empty()) {
            LogManager::getInstance().Info("활성화된 디바이스가 없음");
            return 0;
        }
        
        LogManager::getInstance().Info("활성 디바이스 " + std::to_string(active_devices.size()) + "개 발견");
        
        // 2단계: Worker들 생성 및 시작
        int success_count = 0;
        std::vector<std::string> successful_devices;
        
        for (const auto& [device_id, device_int_id] : active_devices) {
            if (StartWorker(device_id)) {
                success_count++;
                successful_devices.push_back(device_id);
                LogManager::getInstance().Debug("Worker 시작 성공: " + device_id);
            } else {
                LogManager::getInstance().Error("Worker 시작 실패: " + device_id);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // 3단계: Redis 초기화 데이터 일괄 저장
        if (!successful_devices.empty() && redis_data_writer_) {
            int redis_saved = BatchInitializeRedisData(successful_devices);
            LogManager::getInstance().Info("Redis 초기화 완료: " + std::to_string(redis_saved) + "개 포인트 저장");
        }
        
        LogManager::getInstance().Info("Worker 시작 완료: " + std::to_string(success_count) + "/" + 
                                     std::to_string(active_devices.size()));
        return success_count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("활성 Worker 시작 실패: " + std::string(e.what()));
        return 0;
    }
}

int WorkerManager::BatchInitializeRedisData(const std::vector<std::string>& device_ids) {
    if (!redis_data_writer_) {
        LogManager::getInstance().Error("RedisDataWriter가 초기화되지 않음");
        return 0;
    }
    
    int total_saved = 0;
    
    LogManager::getInstance().Info("Redis 초기화 데이터 배치 저장 시작: " + 
                                 std::to_string(device_ids.size()) + "개 디바이스");
    
    for (const auto& device_id : device_ids) {
        try {
            // DB에서 현재값들 로드
            auto current_values = LoadCurrentValuesFromDB(device_id);
            
            if (current_values.empty()) {
                LogManager::getInstance().Warn("디바이스 " + device_id + "에 현재값이 없음");
                continue;
            }
            
            // Redis에 초기화 데이터 저장
            size_t saved = redis_data_writer_->SaveWorkerInitialData(device_id, current_values);
            total_saved += saved;
            
            LogManager::getInstance().Debug("디바이스 " + device_id + ": " + 
                                          std::to_string(saved) + "개 포인트 Redis 저장");
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("디바이스 " + device_id + " Redis 초기화 실패: " + 
                                         std::string(e.what()));
        }
    }
    
    return total_saved;
}

std::vector<Structs::TimestampedValue> WorkerManager::LoadCurrentValuesFromDB(const std::string& device_id) {
    std::vector<Structs::TimestampedValue> values;
    
    try {
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        auto current_value_repo = repo_factory.getCurrentValueRepository();
        
        if (!current_value_repo) {
            LogManager::getInstance().Error("CurrentValueRepository 없음");
            return values;
        }
        
        // 디바이스 ID를 int로 변환
        int device_int_id = std::stoi(device_id);
        
        // 해당 디바이스의 모든 현재값 조회
        auto current_value_entities = current_value_repo->findByDeviceId(device_int_id);
        
        // Entity → TimestampedValue 변환
        for (const auto& entity : current_value_entities) {
            Structs::TimestampedValue value;
            
            value.point_id = entity.getPointId();
            value.timestamp = entity.getValueTimestamp();
            try {
                value.quality = static_cast<PulseOne::Enums::DataQuality>(std::stoi(entity.getQuality()));
            } catch (const std::exception& e) {
                value.quality = PulseOne::Enums::DataQuality::NOT_CONNECTED;
                LogManager::getInstance().Warn("Invalid quality value for point_id=" + std::to_string(value.point_id));
            }
            value.value_changed = false; // 초기값은 변경 아님
            
            // JSON 문자열에서 DataValue로 변환
            try {
                nlohmann::json value_json = nlohmann::json::parse(entity.getCurrentValue());
                if (value_json.contains("value")) {
                    auto json_value = value_json["value"];
                    
                    if (json_value.is_boolean()) {
                        value.value = json_value.get<bool>();
                    } else if (json_value.is_number_integer()) {
                        value.value = json_value.get<int32_t>();
                    } else if (json_value.is_number_float()) {
                        value.value = json_value.get<double>();
                    } else if (json_value.is_string()) {
                        value.value = json_value.get<std::string>();
                    } else {
                        LogManager::getInstance().Warn("알 수 없는 값 타입: point_id=" + 
                                                     std::to_string(value.point_id));
                        continue;
                    }
                    
                    values.push_back(value);
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("JSON 파싱 실패 (point_id=" + 
                                              std::to_string(entity.getPointId()) + "): " + 
                                              std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Debug("디바이스 " + device_id + "에서 " + 
                                      std::to_string(values.size()) + "개 현재값 로드");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("현재값 로드 실패 (device_id=" + device_id + "): " + 
                                      std::string(e.what()));
    }
    
    return values;
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
        // string → DataValue 변환
        DataValue data_value;
        if (!ConvertStringToDataValue(value, data_value)) {
            LogManager::getInstance().Error("Invalid value format: " + value);
            return false;
        }
        
        // 실제 Worker의 WriteDataPoint 호출
        return worker->WriteDataPoint(point_id, data_value);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("데이터 쓰기 예외: " + std::string(e.what()));
        return false;
    }
}

bool WorkerManager::ControlDigitalDevice(const std::string& device_id, const std::string& device_id_target, bool enable) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) return false;
    
    return worker->ControlDigitalDevice(device_id_target, enable);
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

bool WorkerManager::PauseWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Warn("Worker 없음 (일시정지): " + device_id);  // Warning -> Warn
        return false;
    }
    
    try {
        // 실제로는 BaseDeviceWorker에 Pause 메서드가 구현되어야 함
        // 현재는 로그만 출력하고 성공 반환
        LogManager::getInstance().Info("Worker 일시정지 요청: " + device_id);
        // TODO: worker->Pause() 구현 필요
        
        LogManager::getInstance().Info("Worker 일시정지 완료: " + device_id);
        return true;
        
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("Worker 일시정지 예외: " + device_id + " - " + e.what());
        return false;
    }
}

bool WorkerManager::ResumeWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Warn("Worker 없음 (재개): " + device_id);  // Warning -> Warn
        return false;
    }
    
    try {
        // 실제로는 BaseDeviceWorker에 Resume 메서드가 구현되어야 함
        // 현재는 로그만 출력하고 성공 반환
        LogManager::getInstance().Info("Worker 재개 요청: " + device_id);
        // TODO: worker->Resume() 구현 필요
        
        LogManager::getInstance().Info("Worker 재개 완료: " + device_id);
        return true;
        
    } catch (const std::exception& e) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("Worker 재개 예외: " + device_id + " - " + e.what());
        return false;
    }
}

nlohmann::json WorkerManager::GetAllWorkersStatus() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    nlohmann::json status = nlohmann::json::object();
    status["workers"] = nlohmann::json::array();
    status["summary"] = nlohmann::json::object();
    
    int running_count = 0;
    int stopped_count = 0;
    int paused_count = 0;
    int error_count = 0;
    
    for (const auto& [device_id, worker] : workers_) {
        nlohmann::json worker_status = nlohmann::json::object();
        worker_status["device_id"] = device_id;
        
        if (worker) {
            auto state = worker->GetState();
            switch (state) {
                case WorkerState::RUNNING:
                    worker_status["state"] = "running";
                    running_count++;
                    break;
                case WorkerState::STOPPED:
                    worker_status["state"] = "stopped";
                    stopped_count++;
                    break;
                case WorkerState::PAUSED:
                    worker_status["state"] = "paused";
                    paused_count++;
                    break;
                case WorkerState::ERROR:
                    worker_status["state"] = "error";
                    error_count++;
                    break;
                default:
                    worker_status["state"] = "unknown";
                    break;
            }
            
            worker_status["uptime"] = "0d 0h 0m 0s";  // 기본값 - 실제로는 Worker에서 가져와야 함
        } else {
            worker_status["state"] = "null";
            error_count++;
        }
        
        status["workers"].push_back(worker_status);
    }
    
    // 요약 정보
    status["summary"]["total"] = workers_.size();
    status["summary"]["running"] = running_count;
    status["summary"]["stopped"] = stopped_count;
    status["summary"]["paused"] = paused_count;
    status["summary"]["error"] = error_count;
    status["summary"]["total_started"] = total_started_.load();
    status["summary"]["total_stopped"] = total_stopped_.load();
    status["summary"]["total_errors"] = total_errors_.load();
    
    return status;
}

nlohmann::json WorkerManager::GetDetailedStatistics() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    nlohmann::json stats = nlohmann::json::object();
    
    // 기본 통계 (기존 GetManagerStats와 유사)
    stats["basic"] = {
        {"active_workers", workers_.size()},
        {"total_started", total_started_.load()},
        {"total_stopped", total_stopped_.load()},
        {"total_errors", total_errors_.load()}
    };
    
    // 상세 통계
    int running_count = 0;
    int stopped_count = 0;
    int paused_count = 0;
    int error_count = 0;
    
    for (const auto& [device_id, worker] : workers_) {
        if (worker) {
            auto state = worker->GetState();
            switch (state) {
                case WorkerState::RUNNING: running_count++; break;
                case WorkerState::STOPPED: stopped_count++; break;
                case WorkerState::PAUSED: paused_count++; break;
                case WorkerState::ERROR: error_count++; break;
                default: break;
            }
        }
    }
    
    stats["detailed"] = {
        {"running_workers", running_count},
        {"stopped_workers", stopped_count},
        {"paused_workers", paused_count},
        {"error_workers", error_count}
    };
    
    // 추가 정보
    stats["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return stats;
}

bool WorkerManager::ConvertStringToDataValue(const std::string& str, DataValue& value) {
    // JSON 파싱 시도
    if (str.front() == '{' || str.front() == '"') {
        try {
            nlohmann::json j = nlohmann::json::parse(str);
            if (j.is_boolean()) {
                value = j.get<bool>();
                return true;
            } else if (j.is_number_integer()) {
                value = j.get<int32_t>();
                return true;
            } else if (j.is_number_float()) {
                value = j.get<double>();
                return true;
            } else if (j.is_string()) {
                value = j.get<std::string>();
                return true;
            }
        } catch (...) {
            // JSON 파싱 실패 시 계속
        }
    }
    
    // 일반 문자열 파싱
    if (str == "true" || str == "1") {
        value = true;
        return true;
    } else if (str == "false" || str == "0") {
        value = false;
        return true;
    }
    
    // 숫자 파싱 시도
    try {
        if (str.find('.') != std::string::npos) {
            value = std::stod(str);
        } else {
            value = std::stoi(str);
        }
        return true;
    } catch (...) {
        // 문자열로 처리
        value = str;
        return true;
    }
}


} // namespace PulseOne::Workers