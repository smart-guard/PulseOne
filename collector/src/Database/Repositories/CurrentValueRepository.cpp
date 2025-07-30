/**
 * @file CurrentValueRepository.cpp
 * @brief PulseOne Current Value Repository 구현 (기존 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/CurrentValueRepository.h"
#include "Client/RedisClientImpl.h"
#include <algorithm>
#include <sstream>
#include <set>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

CurrentValueRepository::~CurrentValueRepository() {
    logger_->Info("🗄️ CurrentValueRepository shutting down...");
    
    // 주기적 저장 스레드 중지
    stopPeriodicSaver();
    
    // Redis 연결 해제
    if (redis_client_) {
        redis_client_->disconnect();
    }
    
    logger_->Info("✅ CurrentValueRepository shutdown complete");
}

// =============================================================================
// Redis 클라이언트 초기화 (기존 패턴)
// =============================================================================

bool CurrentValueRepository::initializeRedisClient() {
    try {
        redis_client_ = std::make_unique<RedisClientImpl>();
        
        std::string redis_host = config_manager_->getOrDefault("REDIS_PRIMARY_HOST", "localhost");
        std::string redis_port_str = config_manager_->getOrDefault("REDIS_PRIMARY_PORT", "6379");
        int redis_port = std::stoi(redis_port_str);
        std::string redis_password = config_manager_->getOrDefault("REDIS_PRIMARY_PASSWORD", "");
        
        bool connected = redis_client_->connect(redis_host, redis_port, redis_password);
        
        if (connected) {
            logger_->Info("✅ Redis client connected: " + redis_host + ":" + std::to_string(redis_port));
            return true;
        } else {
            logger_->Error("❌ Failed to connect to Redis: " + redis_host + ":" + std::to_string(redis_port));
            redis_enabled_ = false;
            return false;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::initializeRedisClient failed: " + std::string(e.what()));
        redis_enabled_ = false;
        return false;
    }
}

bool CurrentValueRepository::isRedisConnected() const {
    return redis_enabled_ && redis_client_ && 
           static_cast<RedisClientImpl*>(redis_client_.get())->isConnected();
}

// =============================================================================
// IRepository 기본 CRUD 구현 (캐시 자동 적용)
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findAll() {
    try {
        std::string sql = "SELECT * FROM current_values ORDER BY data_point_id";
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        db_read_count_++;
        logger_->Info("CurrentValueRepository::findAll - Found " + std::to_string(entities.size()) + " current values");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<CurrentValueEntity> CurrentValueRepository::findById(int id) {
    if (id <= 0) {
        logger_->Warn("CurrentValueRepository::findById - Invalid ID: " + std::to_string(id));
        return std::nullopt;
    }
    
    try {
        std::string sql = "SELECT * FROM current_values WHERE id = " + std::to_string(id);
        auto result = executeDatabaseQuery(sql);
        
        if (result.empty()) {
            logger_->Debug("CurrentValueRepository::findById - Current value not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        CurrentValueEntity entity = mapRowToEntity(result[0]);
        
        // 🔥 IRepository의 캐시 자동 저장
        cacheEntity(entity);
        
        db_read_count_++;
        logger_->Debug("CurrentValueRepository::findById - Found current value: " + std::to_string(id));
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool CurrentValueRepository::save(CurrentValueEntity& entity) {
    try {
        bool success = entity.saveToDatabase();
        
        if (success) {
            // 🔥 IRepository의 캐시 자동 업데이트
            cacheEntity(entity);
            
            // Redis에도 저장 (활성화된 경우)
            if (isRedisConnected()) {
                saveToRedis(entity, default_ttl_seconds_);
            }
            
            db_write_count_++;
            logger_->Debug("CurrentValueRepository::save - Saved current value: " + std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::update(const CurrentValueEntity& entity) {
    if (entity.getId() <= 0) {
        logger_->Error("CurrentValueRepository::update - Invalid ID for update");
        return false;
    }
    
    try {
        CurrentValueEntity mutable_entity = entity; // const 제거를 위한 복사
        bool success = mutable_entity.updateToDatabase();
        
        if (success) {
            // 🔥 IRepository의 캐시 자동 업데이트
            cacheEntity(mutable_entity);
            
            // Redis에도 업데이트 (활성화된 경우)
            if (isRedisConnected()) {
                saveToRedis(mutable_entity, default_ttl_seconds_);
            }
            
            db_write_count_++;
            logger_->Debug("CurrentValueRepository::update - Updated current value: " + std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::deleteById(int id) {
    if (id <= 0) {
        logger_->Error("CurrentValueRepository::deleteById - Invalid ID");
        return false;
    }
    
    try {
        std::string sql = "DELETE FROM current_values WHERE id = " + std::to_string(id);
        bool success = executeDatabaseNonQuery(sql);
        
        if (success) {
            // 🔥 IRepository의 캐시에서 제거
            clearCacheForId(id);
            
            db_write_count_++;
            logger_->Debug("CurrentValueRepository::deleteById - Deleted current value and cleared cache: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::exists(int id) {
    return findById(id).has_value();
}

// =============================================================================
// 벌크 연산 구현 (캐시 자동 적용)
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) return {};
    
    std::string id_list;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) id_list += ",";
        id_list += std::to_string(ids[i]);
    }
    
    try {
        std::string sql = "SELECT * FROM current_values WHERE id IN (" + id_list + ")";
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        // 🔥 IRepository의 캐시 자동 저장
        for (const auto& entity : entities) {
            cacheEntity(entity);
        }
        
        db_read_count_++;
        logger_->Info("CurrentValueRepository::findByIds - Found " + 
                    std::to_string(entities.size()) + "/" + std::to_string(ids.size()) + " current values");
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        std::string sql = "SELECT * FROM current_values";
        sql += buildWhereClause(conditions);
        sql += buildOrderByClause(order_by);
        sql += buildLimitClause(pagination);
        
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        db_read_count_++;
        logger_->Debug("CurrentValueRepository::findByConditions - Found " + 
                     std::to_string(entities.size()) + " current values");
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// CurrentValue 전용 조회 메서드들
// =============================================================================

std::optional<CurrentValueEntity> CurrentValueRepository::findByDataPointId(int data_point_id) {
    if (data_point_id <= 0) {
        logger_->Warn("CurrentValueRepository::findByDataPointId - Invalid data point ID: " + std::to_string(data_point_id));
        return std::nullopt;
    }
    
    // Redis에서 먼저 확인
    if (isRedisConnected()) {
        auto redis_entity = loadFromRedis(data_point_id);
        if (redis_entity.has_value()) {
            redis_read_count_++;
            logger_->Debug("CurrentValueRepository::findByDataPointId - Redis hit for data point: " + std::to_string(data_point_id));
            return redis_entity;
        }
    }
    
    try {
        std::string sql = "SELECT * FROM current_values WHERE data_point_id = " + std::to_string(data_point_id);
        auto result = executeDatabaseQuery(sql);
        
        if (result.empty()) {
            logger_->Debug("CurrentValueRepository::findByDataPointId - Current value not found for data point: " + std::to_string(data_point_id));
            return std::nullopt;
        }
        
        CurrentValueEntity entity = mapRowToEntity(result[0]);
        
        // 캐시 및 Redis 저장
        cacheEntity(entity);
        if (isRedisConnected()) {
            saveToRedis(entity, default_ttl_seconds_);
        }
        
        db_read_count_++;
        logger_->Debug("CurrentValueRepository::findByDataPointId - Found current value for data point: " + std::to_string(data_point_id));
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByDataPointId failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByDataPointIds(const std::vector<int>& data_point_ids) {
    if (data_point_ids.empty()) return {};
    
    // Redis에서 배치 로드 시도
    std::vector<CurrentValueEntity> redis_entities;
    std::vector<int> missing_ids;
    
    if (isRedisConnected()) {
        redis_entities = loadMultipleFromRedis(data_point_ids);
        
        // Redis에서 찾지 못한 ID들 확인
        std::set<int> found_ids;
        for (const auto& entity : redis_entities) {
            found_ids.insert(entity.getDataPointId());
        }
        
        for (int id : data_point_ids) {
            if (found_ids.find(id) == found_ids.end()) {
                missing_ids.push_back(id);
            }
        }
    } else {
        missing_ids = data_point_ids;
    }
    
    // DB에서 누락된 데이터 조회
    std::vector<CurrentValueEntity> db_entities;
    if (!missing_ids.empty()) {
        std::string id_list;
        for (size_t i = 0; i < missing_ids.size(); ++i) {
            if (i > 0) id_list += ",";
            id_list += std::to_string(missing_ids[i]);
        }
        
        try {
            std::string sql = "SELECT * FROM current_values WHERE data_point_id IN (" + id_list + ")";
            auto result = executeDatabaseQuery(sql);
            db_entities = mapResultToEntities(result);
            
            // 캐시 및 Redis 저장
            for (const auto& entity : db_entities) {
                cacheEntity(entity);
                if (isRedisConnected()) {
                    saveToRedis(entity, default_ttl_seconds_);
                }
            }
            
            db_read_count_++;
            
        } catch (const std::exception& e) {
            logger_->Error("CurrentValueRepository::findByDataPointIds - DB query failed: " + std::string(e.what()));
        }
    }
    
    // Redis와 DB 결과 합치기
    std::vector<CurrentValueEntity> all_entities;
    all_entities.insert(all_entities.end(), redis_entities.begin(), redis_entities.end());
    all_entities.insert(all_entities.end(), db_entities.begin(), db_entities.end());
    
    if (!redis_entities.empty()) redis_read_count_++;
    
    logger_->Info("CurrentValueRepository::findByDataPointIds - Found " + 
                std::to_string(all_entities.size()) + "/" + std::to_string(data_point_ids.size()) + 
                " current values (Redis: " + std::to_string(redis_entities.size()) + 
                ", DB: " + std::to_string(db_entities.size()) + ")");
    
    return all_entities;
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByVirtualPointId(int virtual_point_id) {
    if (virtual_point_id <= 0) return {};
    
    try {
        std::string sql = "SELECT * FROM current_values WHERE virtual_point_id = " + std::to_string(virtual_point_id);
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        db_read_count_++;
        logger_->Debug("CurrentValueRepository::findByVirtualPointId - Found " + 
                     std::to_string(entities.size()) + " current values for virtual point: " + std::to_string(virtual_point_id));
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByVirtualPointId failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByQuality(CurrentValueEntity::DataQuality quality) {
    try {
        std::string quality_str = CurrentValueEntity::qualityToString(quality);
        std::string sql = "SELECT * FROM current_values WHERE quality = '" + quality_str + "'";
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        db_read_count_++;
        logger_->Debug("CurrentValueRepository::findByQuality - Found " + 
                     std::to_string(entities.size()) + " current values with quality: " + quality_str);
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByQuality failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByTimeRange(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {
    
    try {
        // 시간을 SQL 형식으로 변환 (SQLite 기준)
        auto start_time_t = std::chrono::system_clock::to_time_t(start_time);
        auto end_time_t = std::chrono::system_clock::to_time_t(end_time);
        
        std::string sql = "SELECT * FROM current_values WHERE timestamp BETWEEN " +
                         std::string("datetime(") + std::to_string(start_time_t) + ", 'unixepoch') AND " +
                         std::string("datetime(") + std::to_string(end_time_t) + ", 'unixepoch') " +
                         "ORDER BY timestamp";
        
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        db_read_count_++;
        logger_->Debug("CurrentValueRepository::findByTimeRange - Found " + 
                     std::to_string(entities.size()) + " current values in time range");
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByTimeRange failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// Redis 연동 핵심 메서드들
// =============================================================================

std::optional<CurrentValueEntity> CurrentValueRepository::loadFromRedis(int data_point_id) {
    if (!isRedisConnected()) return std::nullopt;
    
    try {
        std::string redis_key = generateRedisKey(data_point_id);
        std::string redis_value = redis_client_->get(redis_key);
        
        if (redis_value.empty()) {
            logger_->Debug("CurrentValueRepository::loadFromRedis - Key not found: " + redis_key);
            return std::nullopt;
        }
        
        // JSON 파싱
        json redis_data = json::parse(redis_value);
        
        CurrentValueEntity entity;
        if (entity.fromRedisJson(redis_data)) {
            redis_read_count_++;
            logger_->Debug("CurrentValueRepository::loadFromRedis - Loaded from Redis: " + redis_key);
            return entity;
        } else {
            logger_->Error("CurrentValueRepository::loadFromRedis - Failed to parse Redis data for: " + redis_key);
            return std::nullopt;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::loadFromRedis failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool CurrentValueRepository::saveToRedis(const CurrentValueEntity& entity, int /* ttl_seconds */) {
    if (!isRedisConnected()) return false;
    
    try {
        std::string redis_key = generateRedisKey(entity.getDataPointId());
        json redis_data = entity.toRedisJson();
        std::string redis_value = redis_data.dump();
        
        bool success = redis_client_->set(redis_key, redis_value);
        
        if (success) {
            redis_write_count_++;
            logger_->Debug("CurrentValueRepository::saveToRedis - Saved to Redis: " + redis_key);
        } else {
            logger_->Error("CurrentValueRepository::saveToRedis - Failed to save to Redis: " + redis_key);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::saveToRedis failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::loadMultipleFromRedis(const std::vector<int>& data_point_ids) {
    std::vector<CurrentValueEntity> entities;
    
    if (!isRedisConnected() || data_point_ids.empty()) return entities;
    
    try {
        for (int data_point_id : data_point_ids) {
            auto entity = loadFromRedis(data_point_id);
            if (entity.has_value()) {
                entities.push_back(entity.value());
            }
        }
        
        if (!entities.empty()) {
            logger_->Debug("CurrentValueRepository::loadMultipleFromRedis - Loaded " + 
                         std::to_string(entities.size()) + "/" + std::to_string(data_point_ids.size()) + " from Redis");
        }
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::loadMultipleFromRedis failed: " + std::string(e.what()));
    }
    
    return entities;
}

int CurrentValueRepository::saveMultipleToRedis(const std::vector<CurrentValueEntity>& entities, int ttl_seconds) {
    if (!isRedisConnected() || entities.empty()) return 0;
    
    int success_count = 0;
    
    try {
        for (const auto& entity : entities) {
            if (saveToRedis(entity, ttl_seconds)) {
                success_count++;
            }
        }
        
        logger_->Debug("CurrentValueRepository::saveMultipleToRedis - Saved " + 
                     std::to_string(success_count) + "/" + std::to_string(entities.size()) + " to Redis");
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::saveMultipleToRedis failed: " + std::string(e.what()));
    }
    
    return success_count;
}

bool CurrentValueRepository::syncRedisToRDB(bool force_sync) {
    if (!isRedisConnected()) {
        logger_->Warn("CurrentValueRepository::syncRedisToRDB - Redis not connected");
        return false;
    }
    
    try {
        // 현재는 간단한 구현만 제공
        // 실제로는 Redis에서 모든 키를 스캔하고 RDB와 동기화해야 함
        std::string sync_type = force_sync ? "forced" : "normal";
        logger_->Info("CurrentValueRepository::syncRedisToRDB - Sync " + sync_type);
        
        // TODO: 실제 동기화 로직 구현
        // 1. Redis SCAN으로 모든 current_value 키 조회
        // 2. 각 키의 데이터를 RDB와 비교
        // 3. 차이점이 있으면 RDB 업데이트
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::syncRedisToRDB failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 주기적 저장 시스템
// =============================================================================

bool CurrentValueRepository::startPeriodicSaver(int interval_seconds) {
    if (periodic_save_running_.load()) {
        logger_->Warn("CurrentValueRepository::startPeriodicSaver - Already running");
        return true;
    }
    
    try {
        periodic_save_interval_ = interval_seconds;
        periodic_save_running_.store(true);
        
        periodic_save_thread_ = std::make_unique<std::thread>(&CurrentValueRepository::periodicSaveWorker, this);
        
        logger_->Info("✅ Periodic saver started with interval: " + std::to_string(interval_seconds) + " seconds");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::startPeriodicSaver failed: " + std::string(e.what()));
        periodic_save_running_.store(false);
        return false;
    }
}

void CurrentValueRepository::stopPeriodicSaver() {
    if (!periodic_save_running_.load()) return;
    
    logger_->Info("🛑 Stopping periodic saver...");
    
    periodic_save_running_.store(false);
    periodic_save_cv_.notify_all();
    
    if (periodic_save_thread_ && periodic_save_thread_->joinable()) {
        periodic_save_thread_->join();
    }
    
    periodic_save_thread_.reset();
    logger_->Info("✅ Periodic saver stopped");
}

bool CurrentValueRepository::isPeriodicSaverRunning() const {
    return periodic_save_running_.load();
}

int CurrentValueRepository::executePeriodicSave() {
    try {
        // Redis에서 모든 현재값 조회하여 RDB에 저장
        // 현재는 기본 구현만 제공
        logger_->Debug("CurrentValueRepository::executePeriodicSave - Executing periodic save");
        
        // TODO: 실제 주기적 저장 로직 구현
        // 1. Redis에서 변경된 데이터 확인
        // 2. 주기적 저장이 필요한 데이터 식별
        // 3. 배치로 RDB에 저장
        
        return 0; // 임시
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::executePeriodicSave failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 배치 처리 최적화
// =============================================================================

int CurrentValueRepository::saveBatch(const std::vector<CurrentValueEntity>& entities) {
    if (entities.empty()) return 0;
    
    try {
        // UPSERT 배치 쿼리 생성
        std::stringstream ss;
        ss << "INSERT OR REPLACE INTO current_values ("
           << "id, data_point_id, virtual_point_id, value, raw_value, quality, "
           << "timestamp, redis_key, updated_at) VALUES ";
        
        for (size_t i = 0; i < entities.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << entities[i].getValuesForBatchInsert();
        }
        
        bool success = executeDatabaseNonQuery(ss.str());
        
        if (success) {
            // 캐시 업데이트
            for (const auto& entity : entities) {
                cacheEntity(entity);
            }
            
            batch_save_count_++;
            db_write_count_++;
            
            logger_->Info("CurrentValueRepository::saveBatch - Saved " + std::to_string(entities.size()) + " entities");
            return static_cast<int>(entities.size());
        } else {
            logger_->Error("CurrentValueRepository::saveBatch - Batch save failed");
            return 0;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::saveBatch failed: " + std::string(e.what()));
        return 0;
    }
}

int CurrentValueRepository::updateBatch(const std::vector<CurrentValueEntity>& entities) {
    // UPDATE는 일반적으로 UPSERT로 처리하는 것이 효율적
    return saveBatch(entities);
}

int CurrentValueRepository::saveOnChange(const std::vector<CurrentValueEntity>& entities, double deadband) {
    if (entities.empty()) return 0;
    
    std::vector<CurrentValueEntity> changed_entities;
    
    try {
        for (const auto& entity : entities) {
            if (entity.needsOnChangeSave(deadband)) {
                changed_entities.push_back(entity);
            }
        }
        
        if (!changed_entities.empty()) {
            int saved_count = saveBatch(changed_entities);
            logger_->Debug("CurrentValueRepository::saveOnChange - Saved " + 
                         std::to_string(saved_count) + "/" + std::to_string(entities.size()) + 
                         " changed entities (deadband: " + std::to_string(deadband) + ")");
            return saved_count;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::saveOnChange failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 캐시 관리 메서드들 (IRepository 오버라이드)
// =============================================================================

void CurrentValueRepository::setCacheEnabled(bool enabled) {
    IRepository::setCacheEnabled(enabled);
    std::string status = enabled ? "enabled" : "disabled";
    logger_->Info("CurrentValueRepository cache " + status);
}

bool CurrentValueRepository::isCacheEnabled() const {
    return IRepository::isCacheEnabled();
}

void CurrentValueRepository::clearCache() {
    IRepository::clearCache();
    logger_->Info("CurrentValueRepository cache cleared");
}

void CurrentValueRepository::clearCacheForId(int id) {
    IRepository::clearCacheForId(id);
    logger_->Debug("CurrentValueRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> CurrentValueRepository::getCacheStats() const {
    auto base_stats = IRepository::getCacheStats();
    
    // 추가 통계 정보
    base_stats["redis_read_count"] = redis_read_count_.load();
    base_stats["redis_write_count"] = redis_write_count_.load();
    base_stats["db_read_count"] = db_read_count_.load();
    base_stats["db_write_count"] = db_write_count_.load();
    base_stats["batch_save_count"] = batch_save_count_.load();
    
    return base_stats;
}

// =============================================================================
// 통계 및 모니터링
// =============================================================================

json CurrentValueRepository::getRepositoryStats() const {
    json stats = json::object();
    
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        stats["repository_name"] = "CurrentValueRepository";
        stats["redis_enabled"] = redis_enabled_;
        stats["redis_connected"] = isRedisConnected();
        stats["cache_enabled"] = isCacheEnabled();
        stats["periodic_saver_running"] = isPeriodicSaverRunning();
        stats["periodic_save_interval"] = periodic_save_interval_;
        
        // 성능 통계
        stats["performance"] = {
            {"redis_read_count", redis_read_count_.load()},
            {"redis_write_count", redis_write_count_.load()},
            {"db_read_count", db_read_count_.load()},
            {"db_write_count", db_write_count_.load()},
            {"batch_save_count", batch_save_count_.load()}
        };
        
        // 캐시 통계
        stats["cache"] = getCacheStats();
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::getRepositoryStats failed: " + std::string(e.what()));
    }
    
    return stats;
}

json CurrentValueRepository::getRedisStats() const {
    json stats = json::object();
    
    if (!isRedisConnected()) {
        stats["status"] = "disconnected";
        return stats;
    }
    
    try {
        stats["status"] = "connected";
        stats["prefix"] = redis_prefix_;
        stats["default_ttl"] = default_ttl_seconds_;
        stats["read_count"] = redis_read_count_.load();
        stats["write_count"] = redis_write_count_.load();
        
        // TODO: Redis INFO 명령어로 실제 서버 통계 조회
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::getRedisStats failed: " + std::string(e.what()));
    }
    
    return stats;
}

json CurrentValueRepository::getPerformanceMetrics() const {
    json metrics = json::object();
    
    try {
        auto cache_stats = getCacheStats();
        
        metrics["cache_hit_rate"] = cache_stats.count("cache_hits") && cache_stats.count("cache_requests") ?
            (cache_stats["cache_requests"] > 0 ? 
                static_cast<double>(cache_stats["cache_hits"]) / cache_stats["cache_requests"] * 100.0 : 0.0) : 0.0;
        
        metrics["redis_efficiency"] = redis_read_count_.load() + redis_write_count_.load();
        metrics["db_efficiency"] = db_read_count_.load() + db_write_count_.load();
        
        metrics["batch_operations"] = batch_save_count_.load();
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::getPerformanceMetrics failed: " + std::string(e.what()));
    }
    
    return metrics;
}

// =============================================================================
// 내부 헬퍼 메서드들 (기존 패턴)
// =============================================================================

std::vector<std::map<std::string, std::string>> CurrentValueRepository::executeDatabaseQuery(const std::string& sql) {
    try {
        std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            auto result = db_manager_->executeQueryPostgres(sql);
            std::vector<std::map<std::string, std::string>> results;
            
            for (const auto& row : result) {
                std::map<std::string, std::string> row_map;
                for (size_t i = 0; i < row.size(); ++i) {
                    std::string column_name = result.column_name(i);
                    std::string value = row[static_cast<int>(i)].is_null() ? 
                                      "" : row[static_cast<int>(i)].as<std::string>();
                    row_map[column_name] = value;
                }
                results.push_back(row_map);
            }
            return results;
        } else {
            // SQLite용 구조체 변환
            struct SQLiteCallbackData {
                std::vector<std::map<std::string, std::string>>* results;
                std::vector<std::string> column_names;
            };
            
            SQLiteCallbackData callback_data;
            callback_data.results = new std::vector<std::map<std::string, std::string>>();
            
            auto callback = [](void* data, int argc, char** argv, char** azColName) -> int {
                auto* cb_data = static_cast<SQLiteCallbackData*>(data);
                
                if (cb_data->column_names.empty()) {
                    for (int i = 0; i < argc; i++) {
                        cb_data->column_names.push_back(azColName[i] ? azColName[i] : "");
                    }
                }
                
                std::map<std::string, std::string> row;
                for (int i = 0; i < argc; i++) {
                    std::string col_name = cb_data->column_names[i];
                    std::string value = argv[i] ? argv[i] : "";
                    row[col_name] = value;
                }
                cb_data->results->push_back(row);
                return 0;
            };
            
            bool success = db_manager_->executeQuerySQLite(sql, callback, &callback_data);
            auto results = *callback_data.results;
            delete callback_data.results;
            
            return success ? results : std::vector<std::map<std::string, std::string>>{};
        }
        
    } catch (const std::exception& e) {
        logger_->Error("executeDatabaseQuery failed: " + std::string(e.what()));
        return {};
    }
}

bool CurrentValueRepository::executeDatabaseNonQuery(const std::string& sql) {
    try {
        std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            return db_manager_->executeNonQueryPostgres(sql);
        } else {
            return db_manager_->executeNonQuerySQLite(sql);
        }
        
    } catch (const std::exception& e) {
        logger_->Error("executeDatabaseNonQuery failed: " + std::string(e.what()));
        return false;
    }
}

CurrentValueEntity CurrentValueRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    CurrentValueEntity entity;
    
    try {
        // 기본 필드들
        if (row.count("id")) entity.setId(std::stoi(row.at("id")));
        if (row.count("data_point_id")) entity.setDataPointId(std::stoi(row.at("data_point_id")));
        if (row.count("virtual_point_id")) entity.setVirtualPointId(std::stoi(row.at("virtual_point_id")));
        if (row.count("value")) entity.setValue(std::stod(row.at("value")));
        if (row.count("raw_value")) entity.setRawValue(std::stod(row.at("raw_value")));
        if (row.count("quality")) entity.setQualityFromString(row.at("quality"));
        if (row.count("redis_key")) entity.setRedisKey(row.at("redis_key"));
        
        // 타임스탬프 파싱 (현재는 간단히 현재 시간 사용)
        entity.setTimestamp(std::chrono::system_clock::now());
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::mapRowToEntity failed: " + std::string(e.what()));
    }
    
    return entity;
}

std::vector<CurrentValueEntity> CurrentValueRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<CurrentValueEntity> entities;
    entities.reserve(result.size());
    
    for (const auto& row : result) {
        entities.push_back(mapRowToEntity(row));
    }
    
    return entities;
}

std::string CurrentValueRepository::buildWhereClause(const std::vector<QueryCondition>& conditions) const {
    if (conditions.empty()) return "";
    
    std::string clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) clause += " AND ";
        clause += conditions[i].field + " " + conditions[i].operation + " " + conditions[i].value;
    }
    return clause;
}

std::string CurrentValueRepository::buildOrderByClause(const std::optional<OrderBy>& order_by) const {
    if (!order_by.has_value()) return "";
    return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
}

std::string CurrentValueRepository::buildLimitClause(const std::optional<Pagination>& pagination) const {
    if (!pagination.has_value()) return "";
    
    std::string clause = " LIMIT " + std::to_string(pagination->limit);
    if (pagination->offset > 0) {
        clause += " OFFSET " + std::to_string(pagination->offset);
    }
    return clause;
}

std::string CurrentValueRepository::generateRedisKey(int data_point_id) const {
    return redis_prefix_ + "dp:" + std::to_string(data_point_id);
}

void CurrentValueRepository::periodicSaveWorker() {
    logger_->Info("🔄 Periodic save worker started");
    
    while (periodic_save_running_.load()) {
        try {
            std::unique_lock<std::mutex> lock(periodic_save_mutex_);
            
            // 지정된 간격만큼 대기
            periodic_save_cv_.wait_for(lock, std::chrono::seconds(periodic_save_interval_), 
                [this] { return !periodic_save_running_.load(); });
            
            if (!periodic_save_running_.load()) break;
            
            // 주기적 저장 실행
            int saved_count = executePeriodicSave();
            if (saved_count > 0) {
                logger_->Debug("Periodic save completed: " + std::to_string(saved_count) + " entities");
            }
            
        } catch (const std::exception& e) {
            logger_->Error("Periodic save worker error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 에러 시 잠깐 대기
        }
    }
    
    logger_->Info("🔄 Periodic save worker stopped");
}

void CurrentValueRepository::loadConfiguration() {
    try {
        // Redis 설정
        std::string redis_enabled_str = config_manager_->getOrDefault("REDIS_PRIMARY_ENABLED", "true");
        redis_enabled_ = (redis_enabled_str == "true" || redis_enabled_str == "1");
        redis_prefix_ = config_manager_->getOrDefault("REDIS_CV_PREFIX", "cv:");
        std::string ttl_str = config_manager_->getOrDefault("REDIS_CV_TTL_SEC", "300");
        default_ttl_seconds_ = std::stoi(ttl_str);
        
        // 주기적 저장 설정
        std::string interval_str = config_manager_->getOrDefault("CV_PERIODIC_SAVE_INTERVAL_SEC", "60");
        periodic_save_interval_ = std::stoi(interval_str);
        
        logger_->Debug("CurrentValueRepository configuration loaded");
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::loadConfiguration failed: " + std::string(e.what()));
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne