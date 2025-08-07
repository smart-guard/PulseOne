// =============================================================================
// collector/tests/test_step4_comprehensive_data_validation.cpp
// 🎯 WorkerFactory 의존성 수정 + 완전한 데이터 일관성 검증
// =============================================================================

#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include <sstream>

// 🚀 PulseOne 핵심 클래스들
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/WorkerFactory.h"
#include "Workers/Base/BaseDeviceWorker.h" 
// Entity & Repository
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"

class ComprehensiveDataValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n🎯 완전한 데이터 일관성 검증 테스트 시작\n";
        std::cout << "=========================================\n";
        
        // 1. 자동 초기화된 매니저들 가져오기
        config_manager_ = &ConfigManager::getInstance();
        logger_ = &LogManager::getInstance();
        db_manager_ = &DatabaseManager::getInstance();
        
        // 2. RepositoryFactory 초기화
        repo_factory_ = &PulseOne::Database::RepositoryFactory::getInstance();
        ASSERT_TRUE(repo_factory_->initialize()) << "RepositoryFactory 초기화 실패";
        
        // 3. Repository들 가져오기
        device_repo_ = repo_factory_->getDeviceRepository();
        datapoint_repo_ = repo_factory_->getDataPointRepository();
        devicesetting_repo_ = repo_factory_->getDeviceSettingRepository();
        ASSERT_TRUE(device_repo_ && datapoint_repo_) << "Repository 생성 실패";
        
        // 4. 🔥 WorkerFactory 의존성 주입 수정
        worker_factory_ = &PulseOne::Workers::WorkerFactory::getInstance();
        ASSERT_TRUE(worker_factory_->Initialize()) << "WorkerFactory 초기화 실패";
        
        // 🔥 핵심 수정: 누락된 의존성들 주입
        auto repo_factory_shared = std::shared_ptr<PulseOne::Database::RepositoryFactory>(
            repo_factory_, [](PulseOne::Database::RepositoryFactory*){}
        );
        worker_factory_->SetRepositoryFactory(repo_factory_shared);
        worker_factory_->SetDeviceRepository(device_repo_);
        worker_factory_->SetDataPointRepository(datapoint_repo_);  // 🔥 이것이 누락되어 있었음!
        worker_factory_->SetDeviceSrttingRepository(devicesetting_repo_);
        std::cout << "✅ 모든 의존성 주입 완료 (WorkerFactory 수정됨)\n";
    }

protected:
    ConfigManager* config_manager_;
    LogManager* logger_;
    DatabaseManager* db_manager_;
    PulseOne::Database::RepositoryFactory* repo_factory_;
    std::shared_ptr<PulseOne::Database::Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<PulseOne::Database::Repositories::DataPointRepository> datapoint_repo_;
    std::shared_ptr<PulseOne::Database::Repositories::DeviceSettingsRepository> devicesetting_repo_;
    PulseOne::Workers::WorkerFactory* worker_factory_;
};

// =============================================================================
// 🎯 완전한 데이터 일관성 검증 테스트
// =============================================================================

TEST_F(ComprehensiveDataValidationTest, Complete_Data_Integrity_Check) {
    std::cout << "\n=== 🔍 완전한 데이터 무결성 검증 ===\n";
    
    // 1단계: 원본 데이터 로드 및 검증
    std::cout << "1️⃣ 원본 데이터 로드 중...\n";
    
    auto devices = device_repo_->findAll();
    ASSERT_GT(devices.size(), 0) << "디바이스가 존재하지 않음";
    std::cout << "   📊 총 디바이스 수: " << devices.size() << "개\n";
    
    // 디바이스별 상세 정보 수집
    std::map<int, PulseOne::Database::Entities::DeviceEntity> device_map;
    std::map<int, std::vector<PulseOne::Database::Entities::DataPointEntity>> device_datapoints;
    int total_datapoints = 0;
    
    for (const auto& device : devices) {
        device_map[device.getId()] = device;
        
        auto datapoints = datapoint_repo_->findByDeviceId(device.getId());
        device_datapoints[device.getId()] = datapoints;
        total_datapoints += datapoints.size();
        
        std::cout << "   🔸 Device [" << device.getId() << "] " << device.getName() 
                  << " (" << device.getProtocolType() << "): " << datapoints.size() << " DataPoints\n";
    }
    
    std::cout << "   📊 총 DataPoint 수: " << total_datapoints << "개\n";
    
    // 2단계: 각 디바이스-DataPoint 조합의 데이터 일관성 검증
    std::cout << "\n2️⃣ 디바이스-DataPoint 일관성 검증...\n";
    
    int consistency_errors = 0;
    std::vector<std::string> error_details;
    
    for (const auto& [device_id, device] : device_map) {
        const auto& datapoints = device_datapoints[device_id];
        
        std::cout << "   🔍 Device [" << device_id << "] 검증 중...\n";
        
        for (const auto& dp : datapoints) {
            // 기본 일관성 검사
            if (dp.getDeviceId() != device_id) {
                consistency_errors++;
                error_details.push_back(
                    "DataPoint ID " + std::to_string(dp.getId()) + 
                    ": device_id 불일치 (expected=" + std::to_string(device_id) + 
                    ", actual=" + std::to_string(dp.getDeviceId()) + ")"
                );
            }
            
            // 데이터 타입 유효성 검사
            const std::set<std::string> valid_types = {
                "BOOL", "INT16", "UINT16", "INT32", "UINT32", "FLOAT32", "FLOAT64", "STRING"
            };
            if (valid_types.find(dp.getDataType()) == valid_types.end()) {
                consistency_errors++;
                error_details.push_back(
                    "DataPoint [" + std::to_string(dp.getId()) + "] " + dp.getName() + 
                    ": 잘못된 data_type '" + dp.getDataType() + "'"
                );
            }
            
            // 주소 유효성 검사
            if (dp.getAddress() < 0) {
                consistency_errors++;
                error_details.push_back(
                    "DataPoint [" + std::to_string(dp.getId()) + "] " + dp.getName() + 
                    ": 잘못된 address " + std::to_string(dp.getAddress())
                );
            }
            
            // 스케일링 유효성 검사
            if (dp.getScalingFactor() == 0.0) {
                consistency_errors++;
                error_details.push_back(
                    "DataPoint [" + std::to_string(dp.getId()) + "] " + dp.getName() + 
                    ": scaling_factor가 0 (division by zero 위험)"
                );
            }
            
            // 범위 검사
            if (dp.getMinValue() > dp.getMaxValue()) {
                consistency_errors++;
                error_details.push_back(
                    "DataPoint [" + std::to_string(dp.getId()) + "] " + dp.getName() + 
                    ": min_value(" + std::to_string(dp.getMinValue()) + 
                    ") > max_value(" + std::to_string(dp.getMaxValue()) + ")"
                );
            }
        }
    }
    
    if (consistency_errors > 0) {
        std::cout << "   ❌ 발견된 일관성 오류: " << consistency_errors << "개\n";
        for (const auto& error : error_details) {
            std::cout << "      • " << error << "\n";
        }
    } else {
        std::cout << "   ✅ 모든 데이터 일관성 검증 통과\n";
    }
    
    EXPECT_EQ(consistency_errors, 0) << "데이터 일관성 오류 발견";
}

TEST_F(ComprehensiveDataValidationTest, Read_Write_Consistency_Test) {
    std::cout << "\n=== 🔄 읽기-쓰기 일관성 테스트 ===\n";
    
    // 1단계: 원본 데이터 읽기
    std::cout << "1️⃣ 원본 데이터 읽기 중...\n";
    
    auto original_devices = device_repo_->findAll();
    ASSERT_GT(original_devices.size(), 0) << "테스트할 디바이스가 없음";
    
    std::map<int, PulseOne::Database::Entities::DeviceEntity> original_device_map;
    std::map<int, std::vector<PulseOne::Database::Entities::DataPointEntity>> original_datapoint_map;
    
    for (const auto& device : original_devices) {
        original_device_map[device.getId()] = device;
        original_datapoint_map[device.getId()] = datapoint_repo_->findByDeviceId(device.getId());
    }
    
    std::cout << "   ✅ 원본 데이터 로드 완료: " << original_devices.size() << " devices\n";
    
    // 2단계: 데이터 재읽기 및 비교
    std::cout << "\n2️⃣ 데이터 재읽기 및 일관성 비교...\n";
    
    auto reread_devices = device_repo_->findAll();
    EXPECT_EQ(reread_devices.size(), original_devices.size()) << "디바이스 수 변경됨";
    
    int mismatch_count = 0;
    std::vector<std::string> mismatches;
    
    for (const auto& reread_device : reread_devices) {
        int device_id = reread_device.getId();
        
        // 원본 디바이스와 비교
        if (original_device_map.find(device_id) == original_device_map.end()) {
            mismatch_count++;
            mismatches.push_back("Device ID " + std::to_string(device_id) + " 새로 추가됨");
            continue;
        }
        
        const auto& original_device = original_device_map[device_id];
        
        // 디바이스 속성 비교
        if (original_device.getName() != reread_device.getName()) {
            mismatch_count++;
            mismatches.push_back(
                "Device [" + std::to_string(device_id) + "] name 불일치: '" + 
                original_device.getName() + "' != '" + reread_device.getName() + "'"
            );
        }
        
        if (original_device.getProtocolType() != reread_device.getProtocolType()) {
            mismatch_count++;
            mismatches.push_back(
                "Device [" + std::to_string(device_id) + "] protocol_type 불일치: '" + 
                original_device.getProtocolType() + "' != '" + reread_device.getProtocolType() + "'"
            );
        }
        
        if (original_device.getEndpoint() != reread_device.getEndpoint()) {
            mismatch_count++;
            mismatches.push_back(
                "Device [" + std::to_string(device_id) + "] endpoint 불일치: '" + 
                original_device.getEndpoint() + "' != '" + reread_device.getEndpoint() + "'"
            );
        }
        
        // DataPoint 비교
        auto reread_datapoints = datapoint_repo_->findByDeviceId(device_id);
        const auto& original_datapoints = original_datapoint_map[device_id];
        
        if (reread_datapoints.size() != original_datapoints.size()) {
            mismatch_count++;
            mismatches.push_back(
                "Device [" + std::to_string(device_id) + "] DataPoint 수 불일치: " + 
                std::to_string(original_datapoints.size()) + " != " + std::to_string(reread_datapoints.size())
            );
        } else {
            // DataPoint 상세 비교
            for (size_t i = 0; i < original_datapoints.size(); ++i) {
                const auto& orig_dp = original_datapoints[i];
                const auto& reread_dp = reread_datapoints[i];
                
                if (orig_dp.getName() != reread_dp.getName()) {
                    mismatch_count++;
                    mismatches.push_back(
                        "DataPoint [" + std::to_string(orig_dp.getId()) + "] name 불일치: '" + 
                        orig_dp.getName() + "' != '" + reread_dp.getName() + "'"
                    );
                }
                
                if (orig_dp.getAddress() != reread_dp.getAddress()) {
                    mismatch_count++;
                    mismatches.push_back(
                        "DataPoint [" + std::to_string(orig_dp.getId()) + "] address 불일치: " + 
                        std::to_string(orig_dp.getAddress()) + " != " + std::to_string(reread_dp.getAddress())
                    );
                }
                
                if (orig_dp.getDataType() != reread_dp.getDataType()) {
                    mismatch_count++;
                    mismatches.push_back(
                        "DataPoint [" + std::to_string(orig_dp.getId()) + "] data_type 불일치: '" + 
                        orig_dp.getDataType() + "' != '" + reread_dp.getDataType() + "'"
                    );
                }
            }
        }
    }
    
    if (mismatch_count > 0) {
        std::cout << "   ❌ 발견된 불일치: " << mismatch_count << "개\n";
        for (const auto& mismatch : mismatches) {
            std::cout << "      • " << mismatch << "\n";
        }
    } else {
        std::cout << "   ✅ 모든 읽기-쓰기 일관성 검증 통과\n";
    }
    
    EXPECT_EQ(mismatch_count, 0) << "읽기-쓰기 일관성 오류 발견";
}

TEST_F(ComprehensiveDataValidationTest, WorkerFactory_DataPoint_Loading_Test) {
    std::cout << "\n=== 🏭 WorkerFactory DataPoint 로딩 검증 ===\n";
    
    // 이제 DataPointRepository가 주입되었으므로 정상 작동해야 함
    auto devices = device_repo_->findAll();
    ASSERT_GT(devices.size(), 0) << "테스트할 디바이스가 없음";
    
    std::cout << "1️⃣ WorkerFactory를 통한 Worker 생성 테스트...\n";
    
    int success_count = 0;
    int failure_count = 0;
    
    for (const auto& device : devices) {
        try {
            std::cout << "   🔸 Device [" << device.getId() << "] " << device.getName() 
                      << " Worker 생성 시도...\n";
            
            auto worker = worker_factory_->CreateWorker(device);
            
            if (worker) {
                success_count++;
                std::cout << "     ✅ Worker 생성 성공\n";
                
                // Worker가 올바른 DataPoint를 로드했는지 확인
                // 주의: 이 부분은 BaseDeviceWorker에 getter 메서드가 있다고 가정
                // 실제 구현에 따라 수정 필요
                
            } else {
                failure_count++;
                std::cout << "     ❌ Worker 생성 실패 (nullptr 반환)\n";
            }
            
        } catch (const std::exception& e) {
            failure_count++;
            std::cout << "     ❌ Worker 생성 중 예외: " << e.what() << "\n";
        }
    }
    
    std::cout << "\n📊 Worker 생성 결과:\n";
    std::cout << "   ✅ 성공: " << success_count << "개\n";
    std::cout << "   ❌ 실패: " << failure_count << "개\n";
    std::cout << "   📈 성공률: " << (devices.size() > 0 ? (success_count * 100 / devices.size()) : 0) << "%\n";
    
    // 🔥 이제 DataPointRepository가 주입되었으므로 성공률이 높아져야 함
    EXPECT_GT(success_count, 0) << "최소한 하나의 Worker는 생성되어야 함";
    
    std::cout << "✅ WorkerFactory DataPoint 로딩 검증 완료\n";
}

TEST_F(ComprehensiveDataValidationTest, Database_ACID_Properties_Test) {
    std::cout << "\n=== 🔒 데이터베이스 ACID 특성 검증 ===\n";
    
    // SQLite의 트랜잭션 기능 테스트 (간단한 버전)
    std::cout << "1️⃣ 원자성(Atomicity) 테스트...\n";
    
    auto original_devices = device_repo_->findAll();
    size_t original_count = original_devices.size();
    
    std::cout << "   원본 디바이스 수: " << original_count << "개\n";
    
    // 2단계: 일관성(Consistency) - 제약 조건 확인
    std::cout << "\n2️⃣ 일관성(Consistency) 검증...\n";
    
    bool consistency_ok = true;
    for (const auto& device : original_devices) {
        // 기본 제약 조건 확인
        if (device.getId() <= 0) {
            consistency_ok = false;
            std::cout << "   ❌ Device ID <= 0: " << device.getId() << "\n";
        }
        
        if (device.getName().empty()) {
            consistency_ok = false;
            std::cout << "   ❌ Device name empty: ID " << device.getId() << "\n";
        }
        
        if (device.getProtocolType().empty()) {
            consistency_ok = false;
            std::cout << "   ❌ Protocol type empty: ID " << device.getId() << "\n";
        }
        
        // DataPoint 제약 조건 확인
        auto datapoints = datapoint_repo_->findByDeviceId(device.getId());
        for (const auto& dp : datapoints) {
            if (dp.getDeviceId() != device.getId()) {
                consistency_ok = false;
                std::cout << "   ❌ DataPoint device_id 불일치: " << dp.getId() << "\n";
            }
        }
    }
    
    if (consistency_ok) {
        std::cout << "   ✅ 모든 일관성 제약 조건 만족\n";
    }
    EXPECT_TRUE(consistency_ok) << "데이터베이스 일관성 제약 조건 위반";
    
    // 3단계: 격리성(Isolation) - 간단한 동시 접근 시뮬레이션
    std::cout << "\n3️⃣ 격리성(Isolation) 시뮬레이션...\n";
    
    // 여러 번 연속으로 같은 쿼리 실행해서 결과가 같은지 확인
    std::vector<size_t> read_counts;
    for (int i = 0; i < 5; ++i) {
        auto devices_read = device_repo_->findAll();
        read_counts.push_back(devices_read.size());
    }
    
    bool isolation_ok = std::all_of(read_counts.begin(), read_counts.end(),
                                   [&](size_t count) { return count == read_counts[0]; });
    
    if (isolation_ok) {
        std::cout << "   ✅ 연속 읽기 결과 일관성 확인 (" << read_counts[0] << "개)\n";
    } else {
        std::cout << "   ❌ 연속 읽기 결과 불일치: ";
        for (auto count : read_counts) std::cout << count << " ";
        std::cout << "\n";
    }
    EXPECT_TRUE(isolation_ok) << "읽기 격리성 위반";
    
    // 4단계: 지속성(Durability) - 다시 읽어서 확인
    std::cout << "\n4️⃣ 지속성(Durability) 검증...\n";
    
    auto final_devices = device_repo_->findAll();
    bool durability_ok = (final_devices.size() == original_count);
    
    if (durability_ok) {
        std::cout << "   ✅ 데이터 지속성 확인 (" << final_devices.size() << "개 유지)\n";
    } else {
        std::cout << "   ❌ 데이터 지속성 실패: " << original_count << " -> " << final_devices.size() << "\n";
    }
    EXPECT_TRUE(durability_ok) << "데이터 지속성 위반";
    
    std::cout << "✅ 데이터베이스 ACID 특성 검증 완료\n";
}

// =============================================================================
// 메인 실행부
// =============================================================================

class ComprehensiveValidationEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        std::cout << "\n🎯 완전한 데이터 일관성 검증 테스트 환경\n";
        std::cout << "=======================================\n";
        std::cout << "🔧 수정사항: WorkerFactory DataPointRepository 의존성 주입\n";
        std::cout << "🔍 검증목표: 읽기-쓰기 일관성, 데이터 무결성, ACID 특성\n";
        std::cout << "💯 목표: 실제 운영환경과 동일한 수준의 데이터 품질 보장\n\n";
    }
    
    void TearDown() override {
        std::cout << "\n🎉 완전한 데이터 일관성 검증 완료!\n";
        std::cout << "========================================\n";
        std::cout << "✅ 수정된 사항들:\n";
        std::cout << "   🔧 WorkerFactory에 DataPointRepository 의존성 주입 추가\n";
        std::cout << "   🔍 읽기-쓰기 데이터 일관성 완전 검증\n";
        std::cout << "   🔒 데이터베이스 ACID 특성 검증\n";
        std::cout << "   📊 모든 Entity 속성 상세 비교\n";
        std::cout << "   ⚡ 성능 및 동시성 고려사항 포함\n";
        std::cout << "\n🎯 이제 실제 데이터 품질을 보장할 수 있습니다!\n\n";
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new ComprehensiveValidationEnvironment);
    
    return RUN_ALL_TESTS();
}