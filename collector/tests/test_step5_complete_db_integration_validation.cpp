/**
 * @file test_updated_alarm_conversion.cpp
 * @brief 수정된 알람 변환 과정 테스트 (enum 직접 사용 버전)
 * @date 2025-09-01
 * 
 * 목적:
 * 1. DB에서 가져온 AlarmOccurrenceEntity의 원시 enum 값 확인
 * 2. enum을 직접 int로 변환하는 새로운 방식 검증
 * 3. Redis 저장 조건 만족 여부 확인
 */

#include <gtest/gtest.h>
#include <iostream>

// PulseOne 시스템
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Alarm/AlarmStartupRecovery.h"
#include "Alarm/AlarmTypes.h"
#include "Storage/BackendFormat.h"

using namespace PulseOne;
using namespace PulseOne::Database;
using namespace PulseOne::Alarm;

class UpdatedAlarmConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n🔍 === 수정된 알람 변환 테스트 시작 ===" << std::endl;
        InitializeSystem();
    }
    
    void InitializeSystem() {
        config_manager_ = &ConfigManager::getInstance();
        logger_ = &LogManager::getInstance();
        db_manager_ = &DatabaseManager::getInstance();
        
        repo_factory_ = &RepositoryFactory::getInstance();
        if (!repo_factory_->isInitialized()) {
            ASSERT_TRUE(repo_factory_->initialize()) << "RepositoryFactory 초기화 실패";
        }
        
        alarm_occurrence_repo_ = repo_factory_->getAlarmOccurrenceRepository();
        ASSERT_TRUE(alarm_occurrence_repo_) << "AlarmOccurrenceRepository 획득 실패";
        
        alarm_recovery_ = &AlarmStartupRecovery::getInstance();
        
        std::cout << "✅ 시스템 초기화 완료" << std::endl;
    }

    ConfigManager* config_manager_;
    LogManager* logger_;
    DatabaseManager* db_manager_;
    RepositoryFactory* repo_factory_;
    AlarmStartupRecovery* alarm_recovery_;
    std::shared_ptr<Repositories::AlarmOccurrenceRepository> alarm_occurrence_repo_;
};

TEST_F(UpdatedAlarmConversionTest, Test_Direct_Enum_Conversion) {
    std::cout << "\n🎯 === 직접 Enum 변환 테스트 ===" << std::endl;
    
    // ==========================================================================
    // 1. 수정된 enum 값 검증
    // ==========================================================================
    std::cout << "\n📊 === 수정된 Enum 매핑 검증 ===" << std::endl;
    std::cout << "AlarmSeverity 매핑:" << std::endl;
    std::cout << "  - INFO = " << static_cast<int>(AlarmSeverity::INFO) << " (should be 0)" << std::endl;
    std::cout << "  - LOW = " << static_cast<int>(AlarmSeverity::LOW) << " (should be 1)" << std::endl;
    std::cout << "  - MEDIUM = " << static_cast<int>(AlarmSeverity::MEDIUM) << " (should be 2)" << std::endl;
    std::cout << "  - HIGH = " << static_cast<int>(AlarmSeverity::HIGH) << " (should be 3)" << std::endl;
    std::cout << "  - CRITICAL = " << static_cast<int>(AlarmSeverity::CRITICAL) << " (should be 4)" << std::endl;
    
    std::cout << "\nAlarmState 매핑:" << std::endl;
    std::cout << "  - INACTIVE = " << static_cast<int>(AlarmState::INACTIVE) << " (should be 0)" << std::endl;
    std::cout << "  - ACTIVE = " << static_cast<int>(AlarmState::ACTIVE) << " (should be 1)" << std::endl;
    std::cout << "  - ACKNOWLEDGED = " << static_cast<int>(AlarmState::ACKNOWLEDGED) << " (should be 2)" << std::endl;
    std::cout << "  - CLEARED = " << static_cast<int>(AlarmState::CLEARED) << " (should be 3)" << std::endl;
    
    // 컴파일 타임 검증 (이미 AlarmTypes.h에 있음)
    ASSERT_EQ(static_cast<int>(AlarmSeverity::INFO), 0);
    ASSERT_EQ(static_cast<int>(AlarmSeverity::LOW), 1);
    ASSERT_EQ(static_cast<int>(AlarmSeverity::MEDIUM), 2);
    ASSERT_EQ(static_cast<int>(AlarmSeverity::HIGH), 3);
    ASSERT_EQ(static_cast<int>(AlarmSeverity::CRITICAL), 4);
    
    ASSERT_EQ(static_cast<int>(AlarmState::INACTIVE), 0);
    ASSERT_EQ(static_cast<int>(AlarmState::ACTIVE), 1);
    ASSERT_EQ(static_cast<int>(AlarmState::ACKNOWLEDGED), 2);
    ASSERT_EQ(static_cast<int>(AlarmState::CLEARED), 3);
    
    std::cout << "✅ Enum 매핑 검증 통과" << std::endl;
    
    // ==========================================================================
    // 2. DB에서 활성 알람 가져오기
    // ==========================================================================
    auto active_alarms = alarm_occurrence_repo_->findActive();
    ASSERT_GT(active_alarms.size(), 0) << "활성 알람이 없어서 테스트 불가능";
    
    const auto& test_alarm = active_alarms[0];
    
    std::cout << "\n📋 === 테스트 알람 정보 ===" << std::endl;
    std::cout << "DB 알람 ID: " << test_alarm.getId() << std::endl;
    std::cout << "Rule ID: " << test_alarm.getRuleId() << std::endl;
    std::cout << "Tenant ID: " << test_alarm.getTenantId() << std::endl;
    
    // ==========================================================================
    // 3. 수정된 직접 변환 방식 테스트
    // ==========================================================================
    auto raw_severity_enum = test_alarm.getSeverity();
    auto raw_state_enum = test_alarm.getState();
    
    // 🎯 새로운 방식: enum 직접 변환
    int converted_severity = static_cast<int>(raw_severity_enum);
    int converted_state = static_cast<int>(raw_state_enum);
    
    std::cout << "\n🔧 === 직접 변환 결과 ===" << std::endl;
    std::cout << "Raw Severity Enum → Int: " << static_cast<int>(raw_severity_enum) << " → " << converted_severity << std::endl;
    std::cout << "Raw State Enum → Int: " << static_cast<int>(raw_state_enum) << " → " << converted_state << std::endl;
    
    // 문자열 변환도 확인 (로그용)
    std::string severity_str = PulseOne::Alarm::severityToString(raw_severity_enum);
    std::string state_str = PulseOne::Alarm::stateToString(raw_state_enum);
    
    std::cout << "Severity 문자열: " << severity_str << std::endl;
    std::cout << "State 문자열: " << state_str << std::endl;
    
    // ==========================================================================
    // 4. BackendFormat 변환 시뮬레이션
    // ==========================================================================
    std::cout << "\n🔄 === BackendFormat 변환 시뮬레이션 ===" << std::endl;
    
    Storage::BackendFormat::AlarmEventData backend_alarm;
    backend_alarm.occurrence_id = test_alarm.getId();
    backend_alarm.rule_id = test_alarm.getRuleId();
    backend_alarm.tenant_id = test_alarm.getTenantId();
    backend_alarm.message = test_alarm.getAlarmMessage();
    backend_alarm.device_id = test_alarm.getDeviceId();
    backend_alarm.point_id = test_alarm.getPointId();
    
    // 🎯 핵심: enum을 직접 int로 변환
    backend_alarm.severity = converted_severity;
    backend_alarm.state = converted_state;
    
    // 시간 변환
    auto duration = test_alarm.getOccurrenceTime().time_since_epoch();
    backend_alarm.occurred_at = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    std::cout << "변환된 BackendFormat:" << std::endl;
    std::cout << "  - occurrence_id: " << backend_alarm.occurrence_id << std::endl;
    std::cout << "  - rule_id: " << backend_alarm.rule_id << std::endl;
    std::cout << "  - severity: " << backend_alarm.severity << std::endl;
    std::cout << "  - state: " << backend_alarm.state << std::endl;
    std::cout << "  - tenant_id: " << backend_alarm.tenant_id << std::endl;
    
    // ==========================================================================
    // 5. Redis 저장 조건 검증
    // ==========================================================================
    std::cout << "\n🔍 === Redis 저장 조건 검증 ===" << std::endl;
    
    // RedisDataWriter::PublishAlarmEvent()의 조건들
    bool state_condition = (backend_alarm.state == 1);  // ACTIVE
    bool severity_critical = (backend_alarm.severity >= 4);  // CRITICAL 이상
    bool severity_high = (backend_alarm.severity >= 3);  // HIGH 이상
    
    std::cout << "조건 검사:" << std::endl;
    std::cout << "  - State == 1 (ACTIVE): " << (state_condition ? "✅ 통과" : "❌ 실패") << std::endl;
    std::cout << "  - Severity >= 4 (CRITICAL): " << (severity_critical ? "✅ 통과" : "❌ 실패") << std::endl;
    std::cout << "  - Severity >= 3 (HIGH+): " << (severity_high ? "✅ 통과" : "❌ 실패") << std::endl;
    
    std::cout << "\n예상 Redis 동작:" << std::endl;
    if (state_condition) {
        std::cout << "  ✅ alarm:active:" << backend_alarm.rule_id << " 키 저장될 예정" << std::endl;
    } else {
        std::cout << "  ❌ 활성 알람 키 저장 안됨 (state != 1)" << std::endl;
    }
    
    if (severity_critical) {
        std::cout << "  ✅ alarms:critical 채널 발행될 예정" << std::endl;
    } else if (severity_high) {
        std::cout << "  ✅ alarms:high 채널 발행될 예정" << std::endl;
    } else {
        std::cout << "  💡 특별 채널 발행 안됨 (일반 alarms:all만)" << std::endl;
    }
    
    // ==========================================================================
    // 6. 실제 AlarmStartupRecovery 호출 테스트
    // ==========================================================================
    std::cout << "\n🚀 === 실제 Recovery 변환 테스트 ===" << std::endl;
    
    try {
        // private 메서드이므로 직접 호출은 불가능하지만, 전체 복구 과정을 통해 검증
        size_t recovered_count = alarm_recovery_->RecoverActiveAlarms();
        
        std::cout << "Recovery 실행 결과: " << recovered_count << "개 복구됨" << std::endl;
        
        auto recovery_stats = alarm_recovery_->GetRecoveryStats();
        std::cout << "Recovery 통계:" << std::endl;
        std::cout << "  - 총 활성 알람: " << recovery_stats.total_active_alarms << std::endl;
        std::cout << "  - 성공 발행: " << recovery_stats.successfully_published << std::endl;
        std::cout << "  - 실패: " << recovery_stats.failed_to_publish << std::endl;
        
        EXPECT_GT(recovery_stats.successfully_published, 0) << "최소 1개 알람은 성공해야 함";
        
    } catch (const std::exception& e) {
        std::cout << "Recovery 실행 중 예외: " << e.what() << std::endl;
        FAIL() << "Recovery 실행 실패";
    }
    
    // ==========================================================================
    // 7. 최종 결론
    // ==========================================================================
    std::cout << "\n🎯 === 최종 결론 ===" << std::endl;
    
    bool conversion_success = true;
    std::vector<std::string> issues;
    
    if (converted_severity < 0 || converted_severity > 4) {
        conversion_success = false;
        issues.push_back("Severity 변환값 범위 초과: " + std::to_string(converted_severity));
    }
    
    if (converted_state < 0 || converted_state > 3) {
        conversion_success = false;
        issues.push_back("State 변환값 범위 초과: " + std::to_string(converted_state));
    }
    
    if (!state_condition) {
        issues.push_back("ACTIVE 상태가 아님 (Redis 키 저장 안됨)");
    }
    
    if (conversion_success && issues.empty()) {
        std::cout << "✅ 모든 변환이 성공적으로 완료됨!" << std::endl;
        std::cout << "✅ Redis 저장 조건 만족" << std::endl;
        std::cout << "🚀 새로운 enum 직접 변환 방식이 정상 동작함!" << std::endl;
    } else {
        std::cout << "⚠️ 발견된 이슈들:" << std::endl;
        for (const auto& issue : issues) {
            std::cout << "  - " << issue << std::endl;
        }
    }
    
    std::cout << "\n📋 === 테스트 완료 ===" << std::endl;
}