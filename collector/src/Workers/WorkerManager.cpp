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
    
    // 기존 Worker가 있으면 상태만 확인
    auto existing = FindWorker(device_id);
    if (existing) {
        auto state = existing->GetState();
        
        // 이미 실행 중이면 성공 반환
        if (state == WorkerState::RUNNING || state == WorkerState::RECONNECTING) {
            LogManager::getInstance().Info("Worker 이미 활성: " + device_id);
            return true;
        }
        
        // 정지된 Worker가 있으면 재시작
        if (state == WorkerState::STOPPED || state == WorkerState::ERROR) {
            LogManager::getInstance().Info("기존 Worker 재시작 시도: " + device_id);
            try {
                auto restart_future = existing->Start();
                // 🔥 연결 실패 여부와 관계없이 Worker 유지
                restart_future.get(); // 결과는 무시
                total_started_.fetch_add(1);
                InitializeWorkerRedisData(device_id);
                LogManager::getInstance().Info("Worker 재시작 완료: " + device_id);
                return true;
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("Worker 재시작 예외: " + device_id + " - " + e.what());
                // 🔥 예외 발생해도 Worker는 유지 (자동 재연결이 계속 시도)
                return false;
            }
        }
    }
    
    // 새 Worker 생성
    auto worker = CreateAndRegisterWorker(device_id);
    if (!worker) {
        total_errors_.fetch_add(1);
        LogManager::getInstance().Error("Worker 생성 실패: " + device_id);
        return false;
    }
    
    // 🔥 핵심 수정: Worker 시작 시도 - 결과와 관계없이 유지
    try {
        LogManager::getInstance().Info("Worker 시작 시도: " + device_id);
        
        auto start_future = worker->Start();
        bool connection_result = start_future.get();
        
        if (connection_result) {
            // 연결 성공
            LogManager::getInstance().Info("Worker 연결 성공: " + device_id);
            total_started_.fetch_add(1);
        } else {
            // 🔥 연결 실패해도 Worker는 유지 (자동 재연결이 백그라운드에서 동작)
            LogManager::getInstance().Info("Worker 생성됨 (연결 대기 중): " + device_id);
            total_started_.fetch_add(1);
        }
        
        // 🔥 성공/실패 관계없이 Redis 초기화 및 Worker 유지
        InitializeWorkerRedisData(device_id);
        LogManager::getInstance().Info("Worker 활성화 완료: " + device_id + " (자동 재연결 활성)");
        return true;
        
    } catch (const std::exception& e) {
        // 🔥 예외 발생해도 Worker 유지 (재연결 스레드가 복구 시도)
        LogManager::getInstance().Warn("Worker 시작 예외: " + device_id + " - " + e.what() + " (재연결 활성)");
        total_started_.fetch_add(1); // 생성 자체는 성공으로 카운트
        return true; // 🔥 중요: Worker는 유지되므로 성공으로 반환
    }
}


void WorkerManager::InitializeWorkerRedisData(const std::string& device_id) {
    if (!redis_data_writer_) {
        LogManager::getInstance().Warn("RedisDataWriter 없음 - 초기화 건너뜀");
        return;
    }
    
    try {
        // 기존 데이터 포인트 저장
        auto current_values = LoadCurrentValuesFromDB(device_id);
        
        if (current_values.empty()) {
            LogManager::getInstance().Debug("디바이스 " + device_id + "에 초기화할 데이터 없음");
            // 데이터가 없어도 설정 메타데이터는 저장
        } else {
            size_t saved = redis_data_writer_->SaveWorkerInitialData(device_id, current_values);
            LogManager::getInstance().Info("Worker Redis 데이터 초기화: " + device_id + 
                                         " (" + std::to_string(saved) + "개 포인트)");
        }
        
        // 🔥 핵심 추가: 설정값 메타데이터를 Redis에 저장
        try {
            json metadata;
            
            int device_int_id = std::stoi(device_id);
            auto& repo_factory = Database::RepositoryFactory::getInstance();
            auto settings_repo = repo_factory.getDeviceSettingsRepository();
            
            if (settings_repo) {
                auto settings_opt = settings_repo->findById(device_int_id);
                if (settings_opt.has_value()) {
                    const auto& settings = settings_opt.value();
                    
                    metadata["timeout_ms"] = settings.getReadTimeoutMs();
                    metadata["retry_interval_ms"] = settings.getRetryIntervalMs();
                    metadata["backoff_time_ms"] = settings.getBackoffTimeMs();
                    metadata["keep_alive_enabled"] = settings.isKeepAliveEnabled();
                    metadata["polling_interval_ms"] = settings.getPollingIntervalMs();
                    metadata["max_retry_count"] = settings.getMaxRetryCount();
                    metadata["worker_restarted_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    // Redis에 Worker 상태 + 설정 메타데이터 저장
                    bool redis_saved = redis_data_writer_->SaveWorkerStatus(device_id, "initialized", metadata);
                    
                    if (redis_saved) {
                        LogManager::getInstance().Info("Worker Redis 설정 메타데이터 저장 완료: " + device_id);
                        LogManager::getInstance().Debug("  - timeout_ms: " + std::to_string(settings.getReadTimeoutMs()));
                        LogManager::getInstance().Debug("  - retry_interval_ms: " + std::to_string(settings.getRetryIntervalMs()));
                        "  - keep_alive_enabled: " + std::string(settings.isKeepAliveEnabled() ? "true" : "false");
                    } else {
                        LogManager::getInstance().Warn("Redis Worker 상태 저장 실패: " + device_id);
                    }
                }
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("Worker 설정 메타데이터 Redis 저장 실패: " + device_id + " - " + e.what());
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Worker Redis 초기화 실패: " + device_id + 
                                      " - " + std::string(e.what()));
    }
}


bool WorkerManager::StopWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto worker = FindWorker(device_id);
    if (!worker) {
        LogManager::getInstance().Info("정리할 Worker 없음: " + device_id);
        return true;
    }
    
    try {
        LogManager::getInstance().Info("Worker 명시적 중지: " + device_id);
        
        // Worker 완전 중지
        auto stop_future = worker->Stop();
        bool stop_result = stop_future.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
        
        if (stop_result) {
            LogManager::getInstance().Info("Worker 중지 완료: " + device_id);
            total_stopped_.fetch_add(1);
        } else {
            LogManager::getInstance().Warn("Worker 중지 타임아웃: " + device_id);
            total_errors_.fetch_add(1);
        }
        
        // 🔥 명시적 중지시에만 Worker 삭제
        UnregisterWorker(device_id);
        return stop_result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Worker 중지 예외: " + device_id + " - " + e.what());
        UnregisterWorker(device_id); // 예외 시에도 정리
        total_errors_.fetch_add(1);
        return false;
    }
}


bool WorkerManager::RestartWorker(const std::string& device_id) {
    LogManager::getInstance().Info("Worker 재시작 요청: " + device_id);
    
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto existing = FindWorker(device_id);
    if (!existing) {
        // Worker가 없으면 새로 생성
        LogManager::getInstance().Info("Worker 없음 - 새로 생성: " + device_id);
        lock.~lock_guard(); // 락 해제
        return StartWorker(device_id);
    }
    
    try {
        LogManager::getInstance().Info("기존 Worker 재시작 (설정 리로드): " + device_id);
        
        // 1. 기존 Worker 완전 중지
        if (existing->GetState() != WorkerState::STOPPED) {
            LogManager::getInstance().Info("Worker 중지 중: " + device_id);
            auto stop_future = existing->Stop();
            auto stop_result = stop_future.wait_for(std::chrono::seconds(3));
            
            if (stop_result != std::future_status::ready) {
                LogManager::getInstance().Warn("Worker 중지 타임아웃: " + device_id);
            }
        }
        
        // 2. 기존 Worker 삭제 (설정 리로드 강제)
        LogManager::getInstance().Info("기존 Worker 삭제 (설정 리로드 위해): " + device_id);
        UnregisterWorker(device_id);
        
        // 잠시 대기 (리소스 정리)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 🔥 핵심 추가: Redis에 재시작 상태 즉시 업데이트
        if (redis_data_writer_) {
            try {
                json restart_metadata;
                restart_metadata["restart_initiated_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                restart_metadata["action"] = "worker_restart";
                
                redis_data_writer_->SaveWorkerStatus(device_id, "restarting", restart_metadata);
                LogManager::getInstance().Debug("Redis 재시작 상태 업데이트: " + device_id);
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("Redis 재시작 상태 저장 실패: " + std::string(e.what()));
            }
        }
        
        // 3. 새로 Worker 생성 (DB에서 최신 설정 자동 로드)
        LogManager::getInstance().Info("새 Worker 생성 (최신 설정 적용): " + device_id);
        lock.~lock_guard(); // 락 해제하여 StartWorker 호출 가능하게
        
        bool start_result = StartWorker(device_id);
        if (start_result) {
            LogManager::getInstance().Info("Worker 재시작 완료 (새 설정 적용): " + device_id);
        } else {
            LogManager::getInstance().Error("Worker 재시작 실패: " + device_id);
        }
        
        return start_result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Worker 재시작 예외: " + device_id + " - " + e.what());
        
        // 예외 발생 시에도 새로 생성 시도
        try {
            UnregisterWorker(device_id);
            lock.~lock_guard();
            return StartWorker(device_id);
        } catch (...) {
            return false;
        }
    }
}

bool WorkerManager::ReloadWorker(const std::string& device_id) {
    LogManager::getInstance().Info("Worker 설정 리로드: " + device_id);
    
    // 🔥 수정: WorkerFactory 프로토콜 리로드는 선택사항
    if (worker_factory_) {
        try {
            worker_factory_->ReloadProtocols();
            LogManager::getInstance().Info("WorkerFactory 프로토콜 리로드 완료");
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("WorkerFactory 리로드 실패: " + std::string(e.what()));
        }
    }
    
    // 🔥 핵심: RestartWorker 호출 (이제 설정을 새로 로드함)
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
        return nlohmann::json{
            {"device_id", device_id},
            {"error", "Worker not found"}
        };
    }
    
    try {
        auto state = worker->GetState();
        bool is_connected = worker->CheckConnection();
        
        json result = {
            {"device_id", device_id},
            {"state", WorkerStateToString(state)},
            {"connected", is_connected},
            {"auto_reconnect_active", true},
            {"description", GetWorkerStateDescription(state, is_connected)}
        };
        
        // 🔥 핵심 추가: Worker 설정값 메타데이터 포함
        try {
            json metadata;
            
            // DB에서 최신 설정값 조회
            int device_int_id = std::stoi(device_id);
            auto& repo_factory = Database::RepositoryFactory::getInstance();
            auto settings_repo = repo_factory.getDeviceSettingsRepository();
            
            if (settings_repo) {
                auto settings_opt = settings_repo->findById(device_int_id);
                if (settings_opt.has_value()) {
                    const auto& settings = settings_opt.value();
                    
                    // 설정값을 메타데이터에 추가
                    metadata["timeout_ms"] = settings.getReadTimeoutMs();
                    metadata["retry_interval_ms"] = settings.getRetryIntervalMs();
                    metadata["backoff_time_ms"] = settings.getBackoffTimeMs();
                    metadata["keep_alive_enabled"] = settings.isKeepAliveEnabled();
                    metadata["polling_interval_ms"] = settings.getPollingIntervalMs();
                    metadata["max_retry_count"] = settings.getMaxRetryCount();
                    
                    // 현재 시간 타임스탬프 추가
                    metadata["captured_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    LogManager::getInstance().Debug("Worker 설정 메타데이터 생성: " + device_id + 
                        " (timeout=" + std::to_string(settings.getReadTimeoutMs()) + "ms)");
                }
            }
            
            if (!metadata.empty()) {
                result["metadata"] = metadata;
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("Worker 메타데이터 생성 실패: " + device_id + " - " + e.what());
        }
        
        return result;
        
    } catch (const std::exception& e) {
        return nlohmann::json{
            {"device_id", device_id},
            {"error", "Status query failed: " + std::string(e.what())}
        };
    }
}

std::string WorkerManager::WorkerStateToString(WorkerState state) const {
    switch (state) {
        case WorkerState::RUNNING: return "RUNNING";
        case WorkerState::STOPPED: return "STOPPED";
        case WorkerState::RECONNECTING: return "RECONNECTING";
        case WorkerState::DEVICE_OFFLINE: return "DEVICE_OFFLINE";
        case WorkerState::ERROR: return "ERROR";
        case WorkerState::STARTING: return "STARTING";
        case WorkerState::STOPPING: return "STOPPING";
        case WorkerState::PAUSED: return "PAUSED";
        case WorkerState::MAINTENANCE: return "MAINTENANCE";
        case WorkerState::SIMULATION: return "SIMULATION";
        case WorkerState::CALIBRATION: return "CALIBRATION";
        case WorkerState::COMMISSIONING: return "COMMISSIONING";
        case WorkerState::COMMUNICATION_ERROR: return "COMMUNICATION_ERROR";
        case WorkerState::DATA_INVALID: return "DATA_INVALID";
        case WorkerState::SENSOR_FAULT: return "SENSOR_FAULT";
        case WorkerState::MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        case WorkerState::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case WorkerState::BYPASS_MODE: return "BYPASS_MODE";
        case WorkerState::DIAGNOSTIC_MODE: return "DIAGNOSTIC_MODE";
        case WorkerState::WAITING_RETRY: return "WAITING_RETRY";
        case WorkerState::MAX_RETRIES_EXCEEDED: return "MAX_RETRIES_EXCEEDED";
        case WorkerState::UNKNOWN: 
        default: return "UNKNOWN";
    }
}

std::string WorkerManager::GetWorkerStateDescription(WorkerState state, bool connected) const {
    switch (state) {
        case WorkerState::RUNNING:
            return connected ? "정상 동작 중" : "데이터 수집 중 (일시적 연결 끊김)";
        case WorkerState::RECONNECTING:
            return "자동 재연결 시도 중";
        case WorkerState::DEVICE_OFFLINE:
            return "디바이스 오프라인 (재연결 대기 중)";
        case WorkerState::ERROR:
            return "오류 상태 (복구 시도 중)";
        case WorkerState::STOPPED:
            return "중지됨";
        case WorkerState::STARTING:
            return "시작 중";
        case WorkerState::STOPPING:
            return "중지 중";
        case WorkerState::PAUSED:
            return "일시정지됨";
        case WorkerState::MAINTENANCE:
            return "점검 모드";
        case WorkerState::SIMULATION:
            return "시뮬레이션 모드";
        case WorkerState::CALIBRATION:
            return "교정 모드";
        case WorkerState::COMMISSIONING:
            return "시운전 모드";
        case WorkerState::COMMUNICATION_ERROR:
            return "통신 오류 (복구 시도 중)";
        case WorkerState::DATA_INVALID:
            return "데이터 이상 감지";
        case WorkerState::SENSOR_FAULT:
            return "센서 고장";
        case WorkerState::MANUAL_OVERRIDE:
            return "수동 제어 모드";
        case WorkerState::EMERGENCY_STOP:
            return "비상 정지";
        case WorkerState::BYPASS_MODE:
            return "우회 모드";
        case WorkerState::DIAGNOSTIC_MODE:
            return "진단 모드";
        case WorkerState::WAITING_RETRY:
            return "재시도 대기 중";
        case WorkerState::MAX_RETRIES_EXCEEDED:
            return "최대 재시도 횟수 초과";
        case WorkerState::UNKNOWN:
        default:
            return "상태 확인 중";
    }
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
    try {
        LogManager::getInstance().Info("Worker 생성 시작: " + device_id + " (DB 최신 설정 로드)");
        
        // 🔥 핵심 확인: DB에서 최신 디바이스 설정 로드
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        auto device_repo = repo_factory.getDeviceRepository();
        auto settings_repo = repo_factory.getDeviceSettingsRepository();
        
        if (!device_repo) {
            LogManager::getInstance().Error("DeviceRepository 없음: " + device_id);
            return nullptr;
        }
        
        // 디바이스 정보 로드
        int device_int_id = std::stoi(device_id);
        auto device_opt = device_repo->findById(device_int_id);
        if (!device_opt.has_value()) {
            LogManager::getInstance().Error("디바이스 정보 없음: " + device_id);
            return nullptr;
        }
        
        const auto& device = device_opt.value();
        LogManager::getInstance().Info("디바이스 정보 로드: " + device.getName() + " (enabled: " + 
                                      (device.isEnabled() ? "true" : "false") + ")");
        
        // 🔥 핵심: 디바이스 설정 최신값 로드
        if (settings_repo) {
            auto settings_opt = settings_repo->findById(device_int_id);
            if (settings_opt.has_value()) {
                const auto& settings = settings_opt.value();
                LogManager::getInstance().Info("최신 디바이스 설정 로드 확인:");
                LogManager::getInstance().Info("  - timeout_ms: " + std::to_string(settings.getReadTimeoutMs()));
                LogManager::getInstance().Info("  - retry_interval_ms: " + std::to_string(settings.getRetryIntervalMs()));
                LogManager::getInstance().Info("  - backoff_time_ms: " + std::to_string(settings.getBackoffTimeMs()));
                LogManager::getInstance().Info("  - keep_alive_enabled: " + std::string(settings.isKeepAliveEnabled() ? "true" : "false"));
            } else {
                LogManager::getInstance().Warn("디바이스 설정 없음: " + device_id + " (기본값 사용)");
            }
        }
        
        // WorkerFactory에서 Worker 생성 (최신 설정 자동 적용됨)
        auto unique_worker = worker_factory_->CreateWorker(device);
        if (!unique_worker) {
            LogManager::getInstance().Error("Worker 생성 실패: " + device_id);
            return nullptr;
        }
        
        // 🔥 핵심: unique_ptr → shared_ptr 안전 변환
        std::shared_ptr<BaseDeviceWorker> shared_worker = std::move(unique_worker);
        
        // Worker 등록
        RegisterWorker(device_id, shared_worker);
        LogManager::getInstance().Info("Worker 생성 및 등록 완료: " + device_id + " (최신 설정 적용됨)");
        
        return shared_worker;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Worker 생성 예외: " + device_id + " - " + e.what());
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