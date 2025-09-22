// =============================================================================
// collector/include/Database/Entities/VirtualPointDependencyEntity.h
// PulseOne VirtualPointDependencyEntity - 가상포인트 의존성 관리
// =============================================================================

#ifndef VIRTUAL_POINT_DEPENDENCY_ENTITY_H
#define VIRTUAL_POINT_DEPENDENCY_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <vector>

namespace PulseOne {
namespace Database {
namespace Entities {

class VirtualPointDependencyEntity : public BaseEntity<VirtualPointDependencyEntity> {
public:
    // 의존 타입
    enum class DependsOnType {
        DATA_POINT = 0,
        VIRTUAL_POINT = 1
    };

public:
    // 생성자
    VirtualPointDependencyEntity();
    explicit VirtualPointDependencyEntity(int id);
    VirtualPointDependencyEntity(int virtual_point_id, DependsOnType type, int depends_on_id);
    
    // BaseEntity 순수 가상 함수 구현
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool validate() const override;
    std::string getTableName() const override { return "virtual_point_dependencies"; }
    
    // Getters
    int getVirtualPointId() const { return virtual_point_id_; }
    DependsOnType getDependsOnType() const { return depends_on_type_; }
    int getDependsOnId() const { return depends_on_id_; }
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    
    // Setters
    void setVirtualPointId(int vp_id) { virtual_point_id_ = vp_id; markModified(); }
    void setDependsOnType(DependsOnType type) { depends_on_type_ = type; markModified(); }
    void setDependsOnId(int id) { depends_on_id_ = id; markModified(); }
    
    // 비즈니스 로직
    bool isDataPointDependency() const { return depends_on_type_ == DependsOnType::DATA_POINT; }
    bool isVirtualPointDependency() const { return depends_on_type_ == DependsOnType::VIRTUAL_POINT; }
    bool createsCycle(int target_vp_id) const;  // 순환 의존성 체크
    
    // 헬퍼 메서드
    static std::string dependsOnTypeToString(DependsOnType type);
    static DependsOnType stringToDependsOnType(const std::string& str);
    
    // 정적 메서드 - 의존성 조회
    static std::vector<VirtualPointDependencyEntity> getDependenciesForVirtualPoint(
        int virtual_point_id, std::shared_ptr<Database::DatabaseManager> db_manager);
    
    static std::vector<int> getDependentVirtualPoints(
        int point_id, DependsOnType type, std::shared_ptr<Database::DatabaseManager> db_manager);
    
private:
    // 의존성 정보
    int virtual_point_id_ = 0;
    DependsOnType depends_on_type_ = DependsOnType::DATA_POINT;
    int depends_on_id_ = 0;
    
    // 메타데이터
    std::chrono::system_clock::time_point created_at_;
    
    // 내부 헬퍼
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_DEPENDENCY_ENTITY_H