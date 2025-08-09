// =============================================================================
// collector/src/Drivers/Bacnet/BACnetServiceManager.cpp
// 🔥 BACnet 고급 서비스 관리자 구현
// =============================================================================

#include "Drivers/Bacnet/BACnetServiceManager.h"
#include "Drivers/Bacnet/BACnetDriver.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <chrono>

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetServiceManager::BACnetServiceManager(BACnetDriver* driver)
    : driver_(driver)
    , next_invoke_id_(1)
    , is_active_(false) {
    
    if (!driver_) {
        throw std::invalid_argument("BACnetDriver cannot be null");
    }
    
    // 통계 초기화
    statistics_.Reset();
    
    // 캐시 초기화
    device_cache_.reserve(MAX_CACHED_DEVICES);
    
    PulseOne::LogManager::getInstance().LogInfo(
        "BACnetServiceManager created successfully"
    );
}

BACnetServiceManager::~BACnetServiceManager() {
    // 활성 요청들 정리
    CleanupPendingRequests();
    
    // 캐시 정리
    CleanupDeviceCache();
    
    PulseOne::LogManager::getInstance().LogInfo(
        "BACnetServiceManager destroyed"
    );
}

// =============================================================================
// 🔥 고급 읽기 서비스
// =============================================================================

bool BACnetServiceManager::ReadPropertyMultiple(uint32_t device_id,
                                               const std::vector<DataPoint>& objects,
                                               std::vector<TimestampedValue>& results,
                                               uint32_t timeout_ms) {
    try {
        if (objects.empty()) {
            LogServiceError("ReadPropertyMultiple", "Empty objects list");
            return false;
        }
        
        auto start_time = steady_clock::now();
        statistics_.rpm_requests++;
        
        uint8_t invoke_id = GetNextInvokeId();
        
        // 요청 등록
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_.emplace(invoke_id, 
                PendingRequest(invoke_id, "RPM", timeout_ms));
        }
        
        bool success = false;
        
#ifdef HAS_BACNET_STACK
        success = SendRPMRequest(device_id, objects, invoke_id);
        
        if (success) {
            // 응답 대기 (실제 구현에서는 콜백으로 처리)
            success = WaitForResponse(invoke_id, timeout_ms);
            
            if (success) {
                statistics_.rpm_success++;
                
                // 응답 파싱하여 결과 반환
                auto response_it = rpm_responses_.find(invoke_id);
                if (response_it != rpm_responses_.end()) {
                    results = response_it->second;
                    rpm_responses_.erase(response_it);
                }
            }
        }
#else
        // 스택 없이 더미 구현
        LogServiceError("ReadPropertyMultiple", "BACnet stack not available");
        success = false;
#endif
        
        // 요청 정리
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_.erase(invoke_id);
        }
        
        // 통계 업데이트
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start_time);
        UpdateServiceStatistics("RPM", success, duration.count());
        
        return success;
        
    } catch (const std::exception& e) {
        LogServiceError("ReadPropertyMultiple", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

bool BACnetServiceManager::ReadOptimizedBatch(uint32_t device_id,
                                            const std::vector<DataPoint>& points,
                                            std::vector<TimestampedValue>& results) {
    try {
        if (points.empty()) {
            return true; // 빈 배치는 성공으로 처리
        }
        
        // RPM 그룹들로 분할
        auto rpm_groups = CreateOptimalRPMGroups(points);
        
        results.clear();
        results.reserve(points.size());
        
        bool overall_success = true;
        
        for (const auto& group : rpm_groups) {
            std::vector<TimestampedValue> group_results;
            
            bool group_success = ReadPropertyMultiple(device_id, 
                                                    group.objects, 
                                                    group_results);
            
            if (group_success) {
                // 결과를 원래 순서로 재배열
                ReorderResults(group, group_results, results);
            } else {
                overall_success = false;
                // 실패한 그룹에 대해 기본값 추가
                AddDefaultValues(group, results);
            }
        }
        
        return overall_success;
        
    } catch (const std::exception& e) {
        LogServiceError("ReadOptimizedBatch", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🔥 고급 쓰기 서비스
// =============================================================================

bool BACnetServiceManager::WriteProperty(uint32_t device_id,
                                       BACnetObjectType object_type,
                                       uint32_t object_instance,
                                       BACnetPropertyIdentifier property_id,
                                       const DataValue& value,
                                       uint8_t priority,
                                       uint32_t timeout_ms) {
    try {
        auto start_time = steady_clock::now();
        statistics_.wpm_requests++;
        
        bool success = false;
        
        if (driver_ && driver_->IsConnected()) {
            // 단일 속성 쓰기는 일반적인 WriteProperty 사용
            success = driver_->WriteProperty(device_id, object_type, 
                                           object_instance, property_id, 
                                           value, priority);
        }
        
        if (success) {
            statistics_.wpm_success++;
        }
        
        // 통계 업데이트
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start_time);
        UpdateServiceStatistics("WPM", success, duration.count());
        
        return success;
        
    } catch (const std::exception& e) {
        LogServiceError("WriteProperty", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

bool BACnetServiceManager::WritePropertyMultiple(uint32_t device_id,
                                                const std::map<DataPoint, DataValue>& values,
                                                uint8_t priority,
                                                uint32_t timeout_ms) {
    try {
        if (values.empty()) {
            return true;
        }
        
        auto start_time = steady_clock::now();
        statistics_.wpm_requests++;
        
        uint8_t invoke_id = GetNextInvokeId();
        
        // 요청 등록
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_.emplace(invoke_id, 
                PendingRequest(invoke_id, "WPM", timeout_ms));
        }
        
        bool success = false;
        
#ifdef HAS_BACNET_STACK
        success = SendWPMRequest(device_id, values, invoke_id);
        
        if (success) {
            success = WaitForResponse(invoke_id, timeout_ms);
            
            if (success) {
                statistics_.wmp_success++;
            }
        }
#else
        // 스택 없이 폴백: 개별 쓰기
        success = true;
        for (const auto& [point, value] : values) {
            bool write_success = WriteProperty(device_id, 
                                             static_cast<BACnetObjectType>(point.object_type),
                                             point.object_instance, 
                                             static_cast<BACnetPropertyIdentifier>(point.property_id),
                                             value, priority, timeout_ms);
            if (!write_success) {
                success = false;
            }
        }
#endif
        
        // 요청 정리
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_.erase(invoke_id);
        }
        
        // 통계 업데이트
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start_time);
        UpdateServiceStatistics("WPM", success, duration.count());
        
        return success;
        
    } catch (const std::exception& e) {
        LogServiceError("WritePropertyMultiple", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🔥 객체 관리 서비스
// =============================================================================

bool BACnetServiceManager::CreateObject(uint32_t device_id,
                                       BACnetObjectType object_type,
                                       uint32_t object_instance,
                                       const std::map<BACnetPropertyIdentifier, DataValue>& properties,
                                       uint32_t timeout_ms) {
    try {
        auto start_time = steady_clock::now();
        statistics_.object_creates++;
        
        // 객체 생성은 일반적으로 WriteProperty를 통해 구현
        bool success = true;
        
        for (const auto& [prop_id, value] : properties) {
            bool prop_success = WriteProperty(device_id, object_type, 
                                            object_instance, prop_id, 
                                            value, BACNET_MAX_PRIORITY, timeout_ms);
            if (!prop_success) {
                success = false;
                break;
            }
        }
        
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start_time);
        UpdateServiceStatistics("CreateObject", success, duration.count());
        
        return success;
        
    } catch (const std::exception& e) {
        LogServiceError("CreateObject", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

bool BACnetServiceManager::DeleteObject(uint32_t device_id,
                                       BACnetObjectType object_type,
                                       uint32_t object_instance,
                                       uint32_t timeout_ms) {
    try {
        auto start_time = steady_clock::now();
        statistics_.object_deletes++;
        
        // 실제 BACnet 스택에서는 DeleteObject 서비스 사용
        bool success = false;
        
        if (driver_ && driver_->IsConnected()) {
            // 여기서 실제 BACnet DeleteObject 서비스 호출
            // 현재는 더미 구현
            success = true;
        }
        
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start_time);
        UpdateServiceStatistics("DeleteObject", success, duration.count());
        
        return success;
        
    } catch (const std::exception& e) {
        LogServiceError("DeleteObject", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🔥 디바이스 발견 최적화
// =============================================================================

bool BACnetServiceManager::PerformOptimizedDiscovery(const std::vector<uint32_t>& target_device_ids,
                                                    std::vector<DeviceInfo>& discovered_devices,
                                                    uint32_t timeout_ms) {
    try {
        discovered_devices.clear();
        
        for (uint32_t device_id : target_device_ids) {
            DeviceInfo device_info;
            
            if (DiscoverSingleDevice(device_id, device_info, timeout_ms)) {
                discovered_devices.push_back(device_info);
                
                // 캐시에 저장
                UpdateDeviceCache(device_id, device_info);
            }
        }
        
        return !discovered_devices.empty();
        
    } catch (const std::exception& e) {
        LogServiceError("PerformOptimizedDiscovery", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

bool BACnetServiceManager::DiscoverSingleDevice(uint32_t device_id,
                                               DeviceInfo& device_info,
                                               uint32_t timeout_ms) {
    try {
        // 캐시 확인
        if (GetCachedDeviceInfo(device_id, device_info)) {
            return true;
        }
        
        // 실제 발견 로직
        if (driver_ && driver_->IsConnected()) {
            // ReadProperty로 기본 디바이스 정보 읽기
            std::vector<DataPoint> device_props = {
                {device_id, OBJECT_DEVICE, device_id, PROP_OBJECT_NAME},
                {device_id, OBJECT_DEVICE, device_id, PROP_MODEL_NAME},
                {device_id, OBJECT_DEVICE, device_id, PROP_VENDOR_NAME},
                {device_id, OBJECT_DEVICE, device_id, PROP_APPLICATION_SOFTWARE_VERSION}
            };
            
            std::vector<TimestampedValue> results;
            
            if (ReadPropertyMultiple(device_id, device_props, results, timeout_ms)) {
                // 결과를 DeviceInfo로 변환
                ConvertResultsToDeviceInfo(device_id, results, device_info);
                return true;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogServiceError("DiscoverSingleDevice", 
                       "Exception: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🔥 내부 헬퍼 메서드들
// =============================================================================

std::vector<BACnetServiceManager::RPMGroup> 
BACnetServiceManager::CreateOptimalRPMGroups(const std::vector<DataPoint>& points) {
    std::vector<RPMGroup> groups;
    RPMGroup current_group;
    
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& point = points[i];
        
        // 예상 크기 계산 (객체 식별자 + 속성 + 값)
        size_t estimated_bytes = 16; // 기본 오버헤드
        
        if (current_group.objects.size() >= MAX_RPM_OBJECTS ||
            current_group.estimated_size_bytes + estimated_bytes > MAX_RPM_SIZE_BYTES) {
            
            // 현재 그룹을 완료하고 새 그룹 시작
            if (!current_group.objects.empty()) {
                statistics_.max_objects_per_rpm = std::max(
                    statistics_.max_objects_per_rpm,
                    static_cast<uint32_t>(current_group.objects.size())
                );
                groups.push_back(std::move(current_group));
                current_group = RPMGroup();
            }
        }
        
        current_group.objects.push_back(point);
        current_group.original_indices.push_back(i);
        current_group.estimated_size_bytes += estimated_bytes;
    }
    
    // 마지막 그룹 추가
    if (!current_group.objects.empty()) {
        statistics_.max_objects_per_rpm = std::max(
            statistics_.max_objects_per_rpm,
            static_cast<uint32_t>(current_group.objects.size())
        );
        groups.push_back(std::move(current_group));
    }
    
    return groups;
}

void BACnetServiceManager::ReorderResults(const RPMGroup& group,
                                        const std::vector<TimestampedValue>& group_results,
                                        std::vector<TimestampedValue>& final_results) {
    // 그룹 결과를 원래 인덱스 순서로 재배열
    for (size_t i = 0; i < group.original_indices.size() && i < group_results.size(); ++i) {
        size_t original_index = group.original_indices[i];
        
        // final_results 크기 조정
        if (final_results.size() <= original_index) {
            final_results.resize(original_index + 1);
        }
        
        final_results[original_index] = group_results[i];
    }
}

void BACnetServiceManager::AddDefaultValues(const RPMGroup& group,
                                          std::vector<TimestampedValue>& results) {
    // 실패한 그룹에 대해 기본값 추가
    for (size_t original_index : group.original_indices) {
        if (results.size() <= original_index) {
            results.resize(original_index + 1);
        }
        
        // 에러 값으로 설정
        results[original_index] = TimestampedValue{
            DataValue{0.0}, // 기본값
            std::chrono::system_clock::now(),
            false // 유효하지 않음
        };
    }
}

uint8_t BACnetServiceManager::GetNextInvokeId() {
    uint8_t id = next_invoke_id_.fetch_add(1, std::memory_order_acq_rel);
    if (id == 0) {
        id = next_invoke_id_.fetch_add(1, std::memory_order_acq_rel);
    }
    return id;
}

bool BACnetServiceManager::WaitForResponse(uint8_t invoke_id, uint32_t timeout_ms) {
    // 실제 구현에서는 이벤트나 조건 변수를 사용
    // 여기서는 단순화된 구현
    auto timeout_time = steady_clock::now() + milliseconds(timeout_ms);
    
    while (steady_clock::now() < timeout_time) {
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_requests_.find(invoke_id);
            if (it != pending_requests_.end() && it->second.completed) {
                return it->second.success;
            }
        }
        
        std::this_thread::sleep_for(milliseconds(10));
    }
    
    return false; // 타임아웃
}

void BACnetServiceManager::CleanupPendingRequests() {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    
    auto now = steady_clock::now();
    auto it = pending_requests_.begin();
    
    while (it != pending_requests_.end()) {
        if (now > it->second.timeout_time) {
            LogServiceError("CleanupPendingRequests", 
                           "Request " + std::to_string(it->first) + " timed out");
            it = pending_requests_.erase(it);
        } else {
            ++it;
        }
    }
}

void BACnetServiceManager::LogServiceError(const std::string& service_name, 
                                         const std::string& error_message) {
    PulseOne::LogManager::getInstance().LogError(
        "BACnetServiceManager::" + service_name + " - " + error_message
    );
}

void BACnetServiceManager::UpdateServiceStatistics(const std::string& service_type, 
                                                  bool success, 
                                                  double duration_ms) {
    if (service_type == "RPM") {
        if (success) {
            statistics_.avg_rpm_time_ms = 
                (statistics_.avg_rpm_time_ms * (statistics_.rpm_success - 1) + duration_ms) / 
                statistics_.rpm_success;
        }
    } else if (service_type == "WPM") {
        if (success) {
            statistics_.avg_wpm_time_ms = 
                (statistics_.avg_wpm_time_ms * (statistics_.wmp_success - 1) + duration_ms) / 
                statistics_.wmp_success;
        }
    }
}

void BACnetServiceManager::UpdateDeviceCache(uint32_t device_id, 
                                           const DeviceInfo& device_info) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    device_cache_[device_id] = {
        device_info,
        steady_clock::now()
    };
    
    // 캐시 크기 제한
    if (device_cache_.size() > MAX_CACHED_DEVICES) {
        CleanupDeviceCache();
    }
}

bool BACnetServiceManager::GetCachedDeviceInfo(uint32_t device_id, 
                                             DeviceInfo& device_info) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = device_cache_.find(device_id);
    if (it != device_cache_.end()) {
        auto age = steady_clock::now() - it->second.timestamp;
        
        if (age < DEVICE_CACHE_TTL) {
            device_info = it->second.device_info;
            return true;
        } else {
            // 만료된 엔트리 제거
            device_cache_.erase(it);
        }
    }
    
    return false;
}

void BACnetServiceManager::CleanupDeviceCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto now = steady_clock::now();
    auto it = device_cache_.begin();
    
    while (it != device_cache_.end()) {
        auto age = now - it->second.timestamp;
        if (age > DEVICE_CACHE_TTL) {
            it = device_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void BACnetServiceManager::ConvertResultsToDeviceInfo(uint32_t device_id,
                                                    const std::vector<TimestampedValue>& results,
                                                    DeviceInfo& device_info) {
    device_info.device_id = device_id;
    device_info.protocol_type = ProtocolType::BACNET;
    device_info.is_online = true;
    
    // 결과에서 디바이스 정보 추출
    if (results.size() >= 4) {
        if (results[0].is_valid && 
            std::holds_alternative<std::string>(results[0].value.data)) {
            device_info.name = std::get<std::string>(results[0].value.data);
        }
        
        if (results[1].is_valid && 
            std::holds_alternative<std::string>(results[1].value.data)) {
            device_info.properties["model_name"] = std::get<std::string>(results[1].value.data);
        }
        
        if (results[2].is_valid && 
            std::holds_alternative<std::string>(results[2].value.data)) {
            device_info.properties["vendor_name"] = std::get<std::string>(results[2].value.data);
        }
        
        if (results[3].is_valid && 
            std::holds_alternative<std::string>(results[3].value.data)) {
            device_info.properties["software_version"] = std::get<std::string>(results[3].value.data);
        }
    }
}

#ifdef HAS_BACNET_STACK
// BACnet 스택 사용 시 구현
bool BACnetServiceManager::SendRPMRequest(uint32_t device_id, 
                                         const std::vector<DataPoint>& objects, 
                                         uint8_t invoke_id) {
    // 실제 BACnet 스택을 사용한 RPM 요청 구현
    // 이 부분은 사용하는 BACnet 스택에 따라 달라짐
    return true;
}

bool BACnetServiceManager::SendWPMRequest(uint32_t device_id, 
                                         const std::map<DataPoint, DataValue>& values, 
                                         uint8_t invoke_id) {
    // 실제 BACnet 스택을 사용한 WPM 요청 구현
    return true;
}

bool BACnetServiceManager::ParseRPMResponse(const uint8_t* service_data, 
                                          uint16_t service_len,
                                          const std::vector<DataPoint>& expected_objects,
                                          std::vector<TimestampedValue>& results) {
    // 실제 BACnet 스택을 사용한 RPM 응답 파싱
    return true;
}

bool BACnetServiceManager::ParseWPMResponse(const uint8_t* service_data, 
                                          uint16_t service_len) {
    // 실제 BACnet 스택을 사용한 WPM 응답 파싱
    return true;
}

bool BACnetServiceManager::GetDeviceAddress(uint32_t device_id, BACNET_ADDRESS& address) {
    // 디바이스 주소 조회
    return true;
}

void BACnetServiceManager::CacheDeviceAddress(uint32_t device_id, const BACNET_ADDRESS& address) {
    // 디바이스 주소 캐싱
}
#endif

} // namespace Drivers
} // namespace PulseOne