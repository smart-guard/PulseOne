// collector/src/Database/DeviceDataAccess.cpp (수정)
// 기존 DatabaseManager를 활용하는 버전

#include "Database/DeviceDataAccess.h"
#include "DatabaseManager.h"  // 기존 클래스 사용!
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace PulseOne {
namespace Database {

// =============================================================================
// PIMPL 구현 클래스 (기존 DatabaseManager 활용)
// =============================================================================
class DeviceDataAccess::Impl {
public:
    Impl(std::shared_ptr<LogManager> logger, 
         std::shared_ptr<ConfigManager> config)
        : logger_(logger), config_(config), tenant_id_("1") {}
    
    ~Impl() = default;
    
    // 의존성
    std::shared_ptr<LogManager> logger_;
    std::shared_ptr<ConfigManager> config_;
    
    // 기존 DatabaseManager 싱글턴 사용
    DatabaseManager* db_manager_;
    
    // 테넌트 컨텍스트
    std::string tenant_id_;
    
    // 연결 상태
    std::atomic<bool> connected_{false};
    mutable std::mutex connection_mutex_;
    
    // 캐시 (성능 최적화)
    mutable std::mutex cache_mutex_;
    std::map<int, DeviceInfo> device_cache_;
    std::map<int, std::vector<DataPointInfo>> datapoint_cache_;
    std::chrono::steady_clock::time_point last_cache_clear_;
    
    // 설정 가능한 파라미터
    std::chrono::seconds cache_ttl_{300}; // 5분
    size_t max_batch_size_{100};
    
    // ==========================================================================
    // 초기화 및 연결 관리
    // ==========================================================================
    
    bool Initialize() {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        try {
            // 기존 DatabaseManager 싱글턴 획득
            db_manager_ = &DatabaseManager::getInstance();
            
            // 연결 초기화는 이미 main에서 되어 있다고 가정
            if (!db_manager_->isPostgresConnected()) {
                logger_->Error("PostgreSQL connection not available in DatabaseManager");
                return false;
            }
            
            // 테넌트 ID 설정 (config에서 읽기)
            tenant_id_ = config_->GetValue("tenant_id", "1");
            
            // 스키마 검증
            if (!ValidateSchema()) {
                logger_->Error("Database schema validation failed");
                return false;
            }
            
            connected_ = true;
            logger_->Info("DeviceDataAccess initialized successfully for tenant: " + tenant_id_);
            
            return true;
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceDataAccess initialization failed: " + std::string(e.what()));
            return false;
        }
    }
    
    void Cleanup() {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        
        // 캐시 정리
        {
            std::lock_guard<std::mutex> cache_lock(cache_mutex_);
            device_cache_.clear();
            datapoint_cache_.clear();
        }
        
        connected_ = false;
        logger_->Info("DeviceDataAccess cleaned up");
    }
    
    bool IsConnected() const {
        return connected_ && db_manager_ && db_manager_->isPostgresConnected();
    }
    
    bool HealthCheck() {
        if (!IsConnected()) {
            return false;
        }
        
        try {
            // 간단한 쿼리로 연결 테스트
            auto result = db_manager_->executeQueryPostgres("SELECT 1 as health_check");
            return !result.empty();
            
        } catch (const std::exception& e) {
            logger_->Warning("Health check failed: " + std::string(e.what()));
            return false;
        }
    }
    
    // ==========================================================================
    // 스키마 검증 (기존 DatabaseManager 사용)
    // ==========================================================================
    
    bool ValidateSchema() {
        try {
            // 필수 테이블 존재 확인
            std::vector<std::string> required_tables = {
                "devices", "device_status", "data_points", "current_values"
            };
            
            for (const auto& table : required_tables) {
                std::string query = 
                    "SELECT COUNT(*) as count FROM information_schema.tables "
                    "WHERE table_name = '" + table + "'";
                
                auto result = db_manager_->executeQueryPostgres(query);
                
                if (result.empty() || result[0]["count"].as<int>() == 0) {
                    logger_->Error("Required table not found: " + table);
                    return false;
                }
            }
            
            logger_->Info("Database schema validation passed");
            return true;
            
        } catch (const std::exception& e) {
            logger_->Error("Schema validation error: " + std::string(e.what()));
            return false;
        }
    }
    
    // ==========================================================================
    // 디바이스 관리 구현 (기존 DatabaseManager의 executeQueryPostgres 사용)
    // ==========================================================================
    
    int CreateDevice(const DeviceInfo& device_info) {
        try {
            std::ostringstream sql;
            sql << "INSERT INTO devices ("
                << "tenant_id, edge_server_id, device_group_id, name, description, "
                << "device_type, manufacturer, model, serial_number, protocol_type, "
                << "endpoint, config, polling_interval, timeout, retry_count, "
                << "is_enabled, installation_date, last_maintenance, created_at, updated_at"
                << ") VALUES ("
                << device_info.tenant_id << ", "
                << device_info.edge_server_id << ", "
                << device_info.device_group_id << ", "
                << "'" << EscapeString(device_info.name) << "', "
                << "'" << EscapeString(device_info.description) << "', "
                << "'" << EscapeString(device_info.device_type) << "', "
                << "'" << EscapeString(device_info.manufacturer) << "', "
                << "'" << EscapeString(device_info.model) << "', "
                << "'" << EscapeString(device_info.serial_number) << "', "
                << "'" << EscapeString(device_info.protocol_type) << "', "
                << "'" << EscapeString(device_info.endpoint) << "', "
                << "'" << EscapeString(device_info.config) << "', "
                << device_info.polling_interval << ", "
                << device_info.timeout << ", "
                << device_info.retry_count << ", "
                << (device_info.is_enabled ? "true" : "false") << ", "
                << (device_info.installation_date.empty() ? "NULL" : "'" + device_info.installation_date + "'") << ", "
                << (device_info.last_maintenance.empty() ? "NULL" : "'" + device_info.last_maintenance + "'") << ", "
                << "NOW(), NOW()"
                << ") RETURNING id";
            
            auto result = db_manager_->executeQueryPostgres(sql.str());
            
            if (!result.empty()) {
                int new_device_id = result[0]["id"].as<int>();
                
                // 디바이스 상태 초기화
                InitializeDeviceStatus(new_device_id);
                
                // 캐시 무효화
                InvalidateDeviceCache(new_device_id);
                
                logger_->Info("Created device: " + device_info.name + " (ID: " + std::to_string(new_device_id) + ")");
                return new_device_id;
            }
            
            return -1;
            
        } catch (const std::exception& e) {
            logger_->Error("CreateDevice failed: " + std::string(e.what()));
            return -1;
        }
    }
    
    bool GetDevice(int device_id, DeviceInfo& device_info) {
        // 캐시 확인
        {
            std::lock_guard<std::mutex> cache_lock(cache_mutex_);
            auto cache_it = device_cache_.find(device_id);
            if (cache_it != device_cache_.end()) {
                device_info = cache_it->second;
                return true;
            }
        }
        
        try {
            std::ostringstream sql;
            sql << "SELECT id, tenant_id, edge_server_id, device_group_id, name, description, "
                << "device_type, manufacturer, model, serial_number, protocol_type, "
                << "endpoint, config, polling_interval, timeout, retry_count, "
                << "is_enabled, installation_date, last_maintenance, "
                << "created_at, updated_at "
                << "FROM devices "
                << "WHERE id = " << device_id << " AND tenant_id = " << GetTenantIdInt();
            
            auto result = db_manager_->executeQueryPostgres(sql.str());
            
            if (result.empty()) {
                return false;
            }
            
            // 결과를 DeviceInfo로 변환
            const auto& row = result[0];
            device_info.id = row["id"].as<int>();
            device_info.tenant_id = row["tenant_id"].as<int>();
            device_info.edge_server_id = row["edge_server_id"].as<int>();
            device_info.device_group_id = row["device_group_id"].as<int>();
            device_info.name = row["name"].as<std::string>();
            device_info.description = row["description"].as<std::string>();
            device_info.device_type = row["device_type"].as<std::string>();
            device_info.manufacturer = row["manufacturer"].as<std::string>();
            device_info.model = row["model"].as<std::string>();
            device_info.serial_number = row["serial_number"].as<std::string>();
            device_info.protocol_type = row["protocol_type"].as<std::string>();
            device_info.endpoint = row["endpoint"].as<std::string>();
            device_info.config = row["config"].as<std::string>();
            device_info.polling_interval = row["polling_interval"].as<int>();
            device_info.timeout = row["timeout"].as<int>();
            device_info.retry_count = row["retry_count"].as<int>();
            device_info.is_enabled = row["is_enabled"].as<bool>();
            device_info.installation_date = row["installation_date"].is_null() ? "" : row["installation_date"].as<std::string>();
            device_info.last_maintenance = row["last_maintenance"].is_null() ? "" : row["last_maintenance"].as<std::string>();
            device_info.created_at = row["created_at"].as<std::string>();
            device_info.updated_at = row["updated_at"].as<std::string>();
            
            // 캐시에 저장
            {
                std::lock_guard<std::mutex> cache_lock(cache_mutex_);
                device_cache_[device_id] = device_info;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            logger_->Error("GetDevice failed: " + std::string(e.what()));
            return false;
        }
    }
    
    std::vector<DeviceInfo> GetDevicesByTenant(int tenant_id) {
        std::vector<DeviceInfo> devices;
        
        try {
            std::ostringstream sql;
            sql << "SELECT id, tenant_id, edge_server_id, device_group_id, name, description, "
                << "device_type, manufacturer, model, serial_number, protocol_type, "
                << "endpoint, config, polling_interval, timeout, retry_count, "
                << "is_enabled, installation_date, last_maintenance, "
                << "created_at, updated_at "
                << "FROM devices "
                << "WHERE tenant_id = " << tenant_id << " "
                << "ORDER BY name";
            
            auto result = db_manager_->executeQueryPostgres(sql.str());
            
            for (const auto& row : result) {
                DeviceInfo device;
                device.id = row["id"].as<int>();
                device.tenant_id = row["tenant_id"].as<int>();
                device.edge_server_id = row["edge_server_id"].as<int>();
                device.device_group_id = row["device_group_id"].as<int>();
                device.name = row["name"].as<std::string>();
                device.description = row["description"].as<std::string>();
                device.device_type = row["device_type"].as<std::string>();
                device.manufacturer = row["manufacturer"].as<std::string>();
                device.model = row["model"].as<std::string>();
                device.serial_number = row["serial_number"].as<std::string>();
                device.protocol_type = row["protocol_type"].as<std::string>();
                device.endpoint = row["endpoint"].as<std::string>();
                device.config = row["config"].as<std::string>();
                device.polling_interval = row["polling_interval"].as<int>();
                device.timeout = row["timeout"].as<int>();
                device.retry_count = row["retry_count"].as<int>();
                device.is_enabled = row["is_enabled"].as<bool>();
                device.installation_date = row["installation_date"].is_null() ? "" : row["installation_date"].as<std::string>();
                device.last_maintenance = row["last_maintenance"].is_null() ? "" : row["last_maintenance"].as<std::string>();
                device.created_at = row["created_at"].as<std::string>();
                device.updated_at = row["updated_at"].as<std::string>();
                
                devices.push_back(device);
            }
            
        } catch (const std::exception& e) {
            logger_->Error("GetDevicesByTenant failed: " + std::string(e.what()));
        }
        
        return devices;
    }
    
    // ==========================================================================
    // 디바이스 상태 관리 (DeviceWorker 연동)
    // ==========================================================================
    
    bool UpdateDeviceStatus(const DeviceStatus& status) {
        try {
            std::ostringstream sql;
            sql << "INSERT INTO device_status ("
                << "device_id, connection_status, last_communication, "
                << "error_count, last_error, response_time, firmware_version, "
                << "hardware_info, diagnostic_data, updated_at"
                << ") VALUES ("
                << status.device_id << ", "
                << "'" << EscapeString(status.connection_status) << "', "
                << "NOW(), "
                << status.error_count << ", "
                << "'" << EscapeString(status.last_error) << "', "
                << status.response_time << ", "
                << "'" << EscapeString(status.firmware_version) << "', "
                << "'" << EscapeString(status.hardware_info) << "', "
                << "'" << EscapeString(status.diagnostic_data) << "', "
                << "NOW()"
                << ") ON CONFLICT (device_id) DO UPDATE SET "
                << "connection_status = EXCLUDED.connection_status, "
                << "last_communication = EXCLUDED.last_communication, "
                << "error_count = EXCLUDED.error_count, "
                << "last_error = EXCLUDED.last_error, "
                << "response_time = EXCLUDED.response_time, "
                << "firmware_version = EXCLUDED.firmware_version, "
                << "hardware_info = EXCLUDED.hardware_info, "
                << "diagnostic_data = EXCLUDED.diagnostic_data, "
                << "updated_at = EXCLUDED.updated_at";
            
            bool success = db_manager_->executeNonQueryPostgres(sql.str());
            return success;
            
        } catch (const std::exception& e) {
            logger_->Error("UpdateDeviceStatus failed: " + std::string(e.what()));
            return false;
        }
    }
    
    // ==========================================================================
    // 현재값 관리 (성능 최적화된 배치 처리)
    // ==========================================================================
    
    bool UpdateCurrentValuesBatch(const std::map<int, CurrentValue>& values) {
        if (values.empty()) {
            return true;
        }
        
        try {
            // PostgreSQL의 경우 VALUES 절을 사용한 배치 삽입
            std::ostringstream sql;
            sql << "INSERT INTO current_values (point_id, value, raw_value, string_value, quality, timestamp) VALUES ";
            
            bool first = true;
            for (const auto& [point_id, value] : values) {
                if (!first) sql << ", ";
                first = false;
                
                sql << "(" << point_id << ", "
                    << value.value << ", "
                    << value.raw_value << ", "
                    << "'" << EscapeString(value.string_value) << "', "
                    << "'" << EscapeString(value.quality) << "', "
                    << "NOW())";
            }
            
            sql << " ON CONFLICT (point_id) DO UPDATE SET "
                << "value = EXCLUDED.value, "
                << "raw_value = EXCLUDED.raw_value, "
                << "string_value = EXCLUDED.string_value, "
                << "quality = EXCLUDED.quality, "
                << "timestamp = EXCLUDED.timestamp";
            
            bool success = db_manager_->executeNonQueryPostgres(sql.str());
            
            if (success) {
                logger_->Debug("Updated " + std::to_string(values.size()) + " current values in batch");
            }
            
            return success;
            
        } catch (const std::exception& e) {
            logger_->Error("UpdateCurrentValuesBatch failed: " + std::string(e.what()));
            return false;
        }
    }
    
    // ==========================================================================
    // 활성화된 데이터 포인트 조회 (DeviceWorker에서 필요!)
    // ==========================================================================
    
    std::vector<DataPointInfo> GetEnabledDataPoints(int device_id) {
        std::vector<DataPointInfo> datapoints;
        
        try {
            std::ostringstream sql;
            sql << "SELECT id, device_id, name, description, address, data_type, access_mode, "
                << "unit, scaling_factor, scaling_offset, min_value, max_value, "
                << "is_enabled, scan_rate, deadband, config, tags, created_at, updated_at "
                << "FROM data_points "
                << "WHERE device_id = " << device_id << " AND is_enabled = true "
                << "ORDER BY address";
            
            auto result = db_manager_->executeQueryPostgres(sql.str());
            
            for (const auto& row : result) {
                DataPointInfo point;
                point.id = row["id"].as<int>();
                point.device_id = row["device_id"].as<int>();
                point.name = row["name"].as<std::string>();
                point.description = row["description"].as<std::string>();
                point.address = row["address"].as<int>();
                point.data_type = row["data_type"].as<std::string>();
                point.access_mode = row["access_mode"].as<std::string>();
                point.unit = row["unit"].is_null() ? "" : row["unit"].as<std::string>();
                point.scaling_factor = row["scaling_factor"].as<double>();
                point.scaling_offset = row["scaling_offset"].as<double>();
                point.min_value = row["min_value"].is_null() ? 0.0 : row["min_value"].as<double>();
                point.max_value = row["max_value"].is_null() ? 0.0 : row["max_value"].as<double>();
                point.is_enabled = row["is_enabled"].as<bool>();
                point.scan_rate = row["scan_rate"].is_null() ? 0 : row["scan_rate"].as<int>();
                point.deadband = row["deadband"].as<double>();
                point.config = row["config"].is_null() ? "" : row["config"].as<std::string>();
                point.tags = row["tags"].is_null() ? "" : row["tags"].as<std::string>();
                point.created_at = row["created_at"].as<std::string>();
                point.updated_at = row["updated_at"].as<std::string>();
                
                datapoints.push_back(point);
            }
            
        } catch (const std::exception& e) {
            logger_->Error("GetEnabledDataPoints failed: " + std::string(e.what()));
        }
        
        return datapoints;
    }
    
    // ==========================================================================
    // 유틸리티 메소드들
    // ==========================================================================
    
    void InitializeDeviceStatus(int device_id) {
        DeviceStatus initial_status;
        initial_status.device_id = device_id;
        initial_status.connection_status = "disconnected";
        initial_status.error_count = 0;
        initial_status.response_time = 0;
        initial_status.firmware_version = "";
        initial_status.hardware_info = "{}";
        initial_status.diagnostic_data = "{}";
        
        UpdateDeviceStatus(initial_status);
    }
    
    void InvalidateDeviceCache(int device_id) {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        device_cache_.erase(device_id);
        datapoint_cache_.erase(device_id);
    }
    
    int GetTenantIdInt() const {
        try {
            return std::stoi(tenant_id_);
        } catch (...) {
            return 1; // 기본값
        }
    }
    
    // SQL 인젝션 방지를 위한 문자열 이스케이프
    std::string EscapeString(const std::string& input) {
        std::string result;
        result.reserve(input.size() * 2);
        
        for (char c : input) {
            if (c == '\'') {
                result += "''";  // Single quote escape
            } else if (c == '\\') {
                result += "\\\\";  // Backslash escape
            } else {
                result += c;
            }
        }
        
        return result;
    }
};

// DeviceDataAccess 클래스 구현은 동일 (PIMPL 인터페이스만 사용)
// ... (이전과 동일한 인터페이스 메소드들)

} // namespace Database
} // namespace PulseOne

// 팩토리 자동 등록
REGISTER_DATA_ACCESS_FACTORY("DeviceDataAccess", PulseOne::Database::DeviceDataAccessFactory);