/**
 * @file test_step5_complete_db_integration_validation.cpp
 * @brief 수정된 완전한 End-to-End 알람 테스트 - 컴파일 에러 해결
 */

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

// PulseOne 시스템
#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Alarm/AlarmStartupRecovery.h"
#include "Alarm/AlarmTypes.h"
#include "Storage/BackendFormat.h"
#include "Storage/RedisDataWriter.h"
#include "Client/RedisClientImpl.h"  // 🔧 수정: 구체적 구현체 사용

using namespace PulseOne;
using namespace PulseOne::Database;
using namespace PulseOne::Alarm;
using namespace PulseOne::Storage;

class CompleteAlarmE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n🚀 === 완전한 End-to-End 알람 테스트 시작 ===" << std::endl;
        
        // 1. 먼저 시스템(DB 연결 등) 초기화
        InitializeSystem();

        // 2. 그 다음 데이터베이스 스키마 및 데이터 초기화 (test_schema_complete.sql 로드)
        std::ifstream sql_file("db/test_schema_complete.sql");
        if (sql_file.is_open()) {
            std::stringstream buffer;
            buffer << sql_file.rdbuf();
            std::string sql = buffer.str();
            
            if (DbLib::DatabaseManager::getInstance().executeNonQuery(sql)) {
                std::cout << "✅ DB Schema & Test Data 초기화 완료" << std::endl;
            } else {
                std::cerr << "❌ DB Schema & Test Data 초기화 실패" << std::endl;
            }
        } else {
            std::cerr << "⚠️ db/test_schema_complete.sql 파일을 열 수 없습니다. 기존 DB를 사용합니다." << std::endl;
        }
    }
    
    void TearDown() override {
        std::cout << "\n🏁 === 테스트 완료 ===" << std::endl;
    }
    
    void InitializeSystem() {
        // 기본 시스템 초기화
        config_manager_ = &ConfigManager::getInstance();
        logger_ = &LogManager::getInstance();
        db_manager_ = &DbLib::DatabaseManagerDbLib::DatabaseManager::getInstance();
        
        // DB Repository 초기화
        repo_factory_ = &RepositoryFactory::getInstance();
        if (!repo_factory_->isInitialized()) {
            ASSERT_TRUE(repo_factory_->initialize()) << "RepositoryFactory 초기화 실패";
        }
        
        alarm_occurrence_repo_ = repo_factory_->getAlarmOccurrenceRepository();
        ASSERT_TRUE(alarm_occurrence_repo_) << "AlarmOccurrenceRepository 획득 실패";
        
        // AlarmStartupRecovery 초기화
        alarm_recovery_ = &AlarmStartupRecovery::getInstance();
        
        // 🔧 수정: RedisClientImpl 구체적 구현체 생성
        redis_client_ = std::make_shared<RedisClientImpl>();
        
        // 🔧 수정: RedisClientImpl의 connect 메서드 매개변수 제공
        ASSERT_TRUE(redis_client_->connect("localhost", 6379)) << "Redis 연결 실패";
        
        // RedisDataWriter 초기화
        redis_data_writer_ = std::make_shared<RedisDataWriter>(redis_client_);
        ASSERT_TRUE(redis_data_writer_->IsConnected()) << "RedisDataWriter 연결 실패";
        
        std::cout << "✅ 시스템 초기화 완료" << std::endl;
    }

    ConfigManager* config_manager_;
    LogManager* logger_;
    DbLib::DatabaseManager* db_manager_;
    RepositoryFactory* repo_factory_;
    AlarmStartupRecovery* alarm_recovery_;
    std::shared_ptr<Repositories::AlarmOccurrenceRepository> alarm_occurrence_repo_;
    std::shared_ptr<RedisClientImpl> redis_client_;  // 🔧 수정: 구체적 타입 사용
    std::shared_ptr<RedisDataWriter> redis_data_writer_;
};

TEST_F(CompleteAlarmE2ETest, Complete_DB_To_Redis_Backend_Flow) {
    std::cout << "\n🎯 === 완전한 DB → Redis → Backend 플로우 테스트 ===" << std::endl;
    
    // ==========================================================================
    // 1단계: Redis 완전 리셋 - 🔧 수정: 실제 존재하는 메서드 사용
    // ==========================================================================
    std::cout << "\n🧹 === 1단계: Redis 완전 리셋 ===" << std::endl;
    
    try {
        // 🔧 수정: select(0) 후 모든 키 삭제하는 방식으로 변경
        redis_client_->select(0);  // DB 0 선택
        
        // 테스트 키 삭제로 리셋 시뮬레이션 (완전한 flushall 대신)
        std::vector<std::string> test_keys = {
            "alarm:active:1", "alarm:active:2", "alarm:active:3", 
            "alarm:active:999999"
        };
        
        for (const auto& key : test_keys) {
            redis_client_->del(key);
        }
        
        std::cout << "✅ Redis 테스트 키 정리 완료" << std::endl;
        
        // 리셋 확인
        std::string test_key = "alarm:active:999999";
        std::string before_reset = redis_client_->get(test_key);
        ASSERT_TRUE(before_reset.empty()) << "Redis 리셋 실패 - 데이터가 남아있음";
        std::cout << "✅ Redis 리셋 확인 완료" << std::endl;
        
    } catch (const std::exception& e) {
        FAIL() << "Redis 리셋 실패: " << e.what();
    }
    
    // ==========================================================================
    // 2단계: DB에서 활성 알람 조회
    // ==========================================================================
    std::cout << "\n📊 === 2단계: DB 활성 알람 조회 ===" << std::endl;
    
    auto active_alarms = alarm_occurrence_repo_->findActive();
    std::cout << "DB에서 조회된 활성 알람: " << active_alarms.size() << "개" << std::endl;
    
    ASSERT_GT(active_alarms.size(), 0u) << "활성 알람이 없어서 테스트 불가능";
    
    // 첫 번째 알람 상세 정보 출력
    const auto& test_alarm = active_alarms[0];
    std::cout << "\n🔍 테스트 대상 알람 (DB 원본):" << std::endl;
    std::cout << "  - ID: " << test_alarm.getId() << std::endl;
    std::cout << "  - Rule ID: " << test_alarm.getRuleId() << std::endl;
    std::cout << "  - Tenant ID: " << test_alarm.getTenantId() << std::endl;
    
    auto device_id_opt = test_alarm.getDeviceId();
    std::cout << "  - Device ID: " << (device_id_opt.has_value() ? std::to_string(device_id_opt.value()) : "NULL") << std::endl;
    
    auto point_id_opt = test_alarm.getPointId();  
    std::cout << "  - Point ID: " << (point_id_opt.has_value() ? std::to_string(point_id_opt.value()) : "NULL") << std::endl;
    
    std::cout << "  - Severity: " << static_cast<int>(test_alarm.getSeverity()) << " (" << test_alarm.getSeverityString() << ")" << std::endl;
    std::cout << "  - State: " << static_cast<int>(test_alarm.getState()) << " (" << test_alarm.getStateString() << ")" << std::endl;
    std::cout << "  - Message: '" << test_alarm.getAlarmMessage() << "'" << std::endl;
    
    // ==========================================================================
    // 3단계: AlarmStartupRecovery 실행
    // ==========================================================================
    std::cout << "\n🔄 === 3단계: AlarmStartupRecovery 실행 ===" << std::endl;
    
    size_t recovered_count = alarm_recovery_->RecoverActiveAlarms();
    std::cout << "AlarmStartupRecovery 복구 결과: " << recovered_count << "개" << std::endl;
    
    EXPECT_GT(recovered_count, 0) << "AlarmStartupRecovery 복구 실패";
    
    // 복구 처리 시간 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // ==========================================================================
    // 4단계: Redis 직접 조회로 저장 확인
    // ==========================================================================
    std::cout << "\n🔍 === 4단계: Redis 저장 데이터 직접 조회 ===" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> found_redis_data;
    
    for (const auto& alarm : active_alarms) {
        if (alarm.getState() == AlarmState::ACTIVE) {
            std::string redis_key = "alarm:active:" + std::to_string(alarm.getRuleId());
            std::string redis_data = redis_client_->get(redis_key);
            
            std::cout << "\nRedis 키 조회: " << redis_key << std::endl;
            
            if (!redis_data.empty()) {
                std::cout << "✅ Redis 데이터 발견 (길이: " << redis_data.length() << " bytes)" << std::endl;
                std::cout << "📄 저장된 데이터 (앞 200자): " << redis_data.substr(0, 200) << "..." << std::endl;
                found_redis_data.push_back({redis_key, redis_data});
            } else {
                std::cout << "❌ Redis 데이터 없음" << std::endl;
            }
        }
    }
    
    ASSERT_GT(found_redis_data.size(), 0u) << "Redis에 저장된 알람 데이터가 없음";
    std::cout << "\n✅ 총 " << found_redis_data.size() << "개의 Redis 키에서 데이터 발견" << std::endl;
    
    // ==========================================================================
    // 5단계: DB 데이터와 Redis 데이터 일치성 검증
    // ==========================================================================
    std::cout << "\n🔍 === 5단계: DB ↔ Redis 데이터 일치성 검증 ===" << std::endl;
    
    bool data_consistency_ok = true;
    
    for (const auto& [redis_key, redis_data] : found_redis_data) {
        try {
            // Redis JSON 파싱
            nlohmann::json redis_json = nlohmann::json::parse(redis_data);
            
            // Redis에서 rule_id 추출
            int redis_rule_id = redis_json.value("rule_id", 0);
            
            // DB에서 해당 rule_id 알람 찾기
            auto db_alarm_it = std::find_if(active_alarms.begin(), active_alarms.end(),
                [redis_rule_id](const auto& alarm) {
                    return alarm.getRuleId() == redis_rule_id;
                });
            
            if (db_alarm_it != active_alarms.end()) {
                const auto& db_alarm = *db_alarm_it;
                
                std::cout << "\n📋 Rule ID " << redis_rule_id << " 데이터 비교:" << std::endl;
                
                // 필드별 일치성 검증
                std::string db_severity = severityToString(db_alarm.getSeverity());
                std::string redis_severity = redis_json.value("severity", "");
                
                std::string db_state = stateToString(db_alarm.getState());
                std::string redis_state = redis_json.value("state", "");
                
                std::string db_message = db_alarm.getAlarmMessage();
                std::string redis_message = redis_json.value("message", "");
                
                std::cout << "  Severity: DB('" << db_severity << "') vs Redis('" << redis_severity << "') " 
                         << (db_severity == redis_severity ? "✅" : "❌") << std::endl;
                
                std::cout << "  State: DB('" << db_state << "') vs Redis('" << redis_state << "') " 
                         << (db_state == redis_state ? "✅" : "❌") << std::endl;
                
                std::cout << "  Message: DB('" << db_message << "') vs Redis('" << redis_message << "') " 
                         << (db_message == redis_message ? "✅" : "❌") << std::endl;
                
                // 일치하지 않으면 실패 플래그 설정
                if (db_severity != redis_severity || db_state != redis_state || db_message != redis_message) {
                    data_consistency_ok = false;
                    std::cout << "❌ Rule ID " << redis_rule_id << " 데이터 불일치 발견" << std::endl;
                }
                
            } else {
                std::cout << "❌ Redis Rule ID " << redis_rule_id << "에 해당하는 DB 알람을 찾을 수 없음" << std::endl;
                data_consistency_ok = false;
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ Redis 데이터 파싱 실패 (" << redis_key << "): " << e.what() << std::endl;
            data_consistency_ok = false;
        }
    }
    
    EXPECT_TRUE(data_consistency_ok) << "DB와 Redis 데이터 일치성 검증 실패";
    
    // ==========================================================================
    // 6단계: 백엔드 구독 포맷 호환성 검증
    // ==========================================================================
    std::cout << "\n🎯 === 6단계: 백엔드 구독 포맷 호환성 검증 ===" << std::endl;
    
    bool backend_compatibility_ok = true;
    
    for (const auto& [redis_key, redis_data] : found_redis_data) {
        try {
            nlohmann::json backend_json = nlohmann::json::parse(redis_data);
            
            std::cout << "\n📝 " << redis_key << " 백엔드 호환성 검사:" << std::endl;
            
            // 필수 필드 존재 확인
            std::vector<std::string> required_fields = {
                "type", "occurrence_id", "rule_id", "tenant_id", 
                "message", "severity", "state", "timestamp"
            };
            
            for (const std::string& field : required_fields) {
                bool has_field = backend_json.contains(field);
                std::cout << "  - " << field << ": " << (has_field ? "✅" : "❌") << std::endl;
                
                if (!has_field) {
                    backend_compatibility_ok = false;
                }
            }
            
            // 타입 검증 (severity, state는 반드시 string)
            if (backend_json.contains("severity")) {
                bool is_string = backend_json["severity"].is_string();
                std::cout << "  - severity type: " << (is_string ? "string ✅" : "not string ❌") << std::endl;
                if (!is_string) backend_compatibility_ok = false;
            }
            
            if (backend_json.contains("state")) {
                bool is_string = backend_json["state"].is_string();
                std::cout << "  - state type: " << (is_string ? "string ✅" : "not string ❌") << std::endl;
                if (!is_string) backend_compatibility_ok = false;
            }
            
            // 백엔드가 기대하는 값 형태 검증
            if (backend_json.contains("severity")) {
                std::string severity = backend_json["severity"].get<std::string>();
                bool valid_severity = (severity == "INFO" || severity == "LOW" || severity == "MEDIUM" || 
                                     severity == "HIGH" || severity == "CRITICAL");
                std::cout << "  - severity value ('" << severity << "'): " << (valid_severity ? "✅" : "❌") << std::endl;
                if (!valid_severity) backend_compatibility_ok = false;
            }
            
            if (backend_json.contains("state")) {
                std::string state = backend_json["state"].get<std::string>();
                bool valid_state = (state == "INACTIVE" || state == "ACTIVE" || state == "ACKNOWLEDGED" || 
                                  state == "CLEARED" || state == "SUPPRESSED" || state == "SHELVED");
                std::cout << "  - state value ('" << state << "'): " << (valid_state ? "✅" : "❌") << std::endl;
                if (!valid_state) backend_compatibility_ok = false;
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ 백엔드 호환성 검증 실패 (" << redis_key << "): " << e.what() << std::endl;
            backend_compatibility_ok = false;
        }
    }
    
    EXPECT_TRUE(backend_compatibility_ok) << "백엔드 구독 포맷 호환성 검증 실패";
    
    // ==========================================================================
    // 7단계: 최종 판정 및 결과 출력
    // ==========================================================================
    std::cout << "\n🏆 === 7단계: 최종 테스트 결과 ===" << std::endl;
    
    bool all_tests_passed = true;
    std::vector<std::string> failure_reasons;
    
    // 각 단계별 검증
    if (active_alarms.size() == 0) {
        failure_reasons.push_back("DB에서 활성 알람 조회 실패");
        all_tests_passed = false;
    }
    
    if (recovered_count == 0) {
        failure_reasons.push_back("AlarmStartupRecovery 복구 실패");
        all_tests_passed = false;
    }
    
    if (found_redis_data.size() == 0) {
        failure_reasons.push_back("Redis에 알람 데이터 저장되지 않음");
        all_tests_passed = false;
    }
    
    if (!data_consistency_ok) {
        failure_reasons.push_back("DB와 Redis 데이터 일치성 검증 실패");
        all_tests_passed = false;
    }
    
    if (!backend_compatibility_ok) {
        failure_reasons.push_back("백엔드 구독 포맷 호환성 실패");
        all_tests_passed = false;
    }
    
    // 최종 결과 출력
    if (all_tests_passed) {
        std::cout << "\n🎉 모든 테스트 통과!" << std::endl;
        std::cout << "✅ Redis 리셋 → DB 조회 → AlarmStartupRecovery → RedisDataWriter → Redis 저장 → 데이터 검증 → 백엔드 호환성" << std::endl;
        std::cout << "✅ 총 " << active_alarms.size() << "개 DB 알람 → " << recovered_count << "개 복구 → " 
                  << found_redis_data.size() << "개 Redis 저장" << std::endl;
        std::cout << "✅ 전체 End-to-End 플로우 정상 동작 확인" << std::endl;
        
    } else {
        std::cout << "\n💥 테스트 실패!" << std::endl;
        std::cout << "실패 원인:" << std::endl;
        for (const auto& reason : failure_reasons) {
            std::cout << "  ❌ " << reason << std::endl;
        }
        
        std::cout << "\n📊 상세 정보:" << std::endl;
        std::cout << "  - DB 활성 알람: " << active_alarms.size() << "개" << std::endl;
        std::cout << "  - 복구된 알람: " << recovered_count << "개" << std::endl;
        std::cout << "  - Redis 저장 확인: " << found_redis_data.size() << "개" << std::endl;
        std::cout << "  - 데이터 일치성: " << (data_consistency_ok ? "OK" : "FAIL") << std::endl;
        std::cout << "  - 백엔드 호환성: " << (backend_compatibility_ok ? "OK" : "FAIL") << std::endl;
        
        FAIL() << "End-to-End 테스트 실패 - 위 원인들을 확인하세요";
    }
}