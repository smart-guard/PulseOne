// ============================================================================
// backend/lib/database/repositories/AlarmRuleRepository.js
// BaseRepository 상속받은 AlarmRuleRepository - C++ 패턴과 100% 일치
// ============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

/**
 * 알람 규칙 Repository 클래스 (C++ AlarmRuleRepository와 동일한 구조)
 * BaseRepository를 상속받아 공통 기능 활용
 */
class AlarmRuleRepository extends BaseRepository {
    constructor() {
        super('alarm_rules');
        
        // 알람 규칙 특화 필드 정의 (C++ AlarmRuleEntity와 동일)
        this.fields = {
            id: 'autoIncrement',
            tenant_id: 'int',
            name: 'varchar(100)',
            description: 'text',
            target_type: 'varchar(20)', // 'data_point', 'virtual_point', 'device', 'group'
            target_id: 'int',
            alarm_type: 'varchar(20)', // 'analog', 'digital', 'script'
            severity: 'varchar(20)', // 'critical', 'high', 'medium', 'low'
            priority: 'int',
            is_enabled: 'boolean',
            auto_clear: 'boolean',
            
            // Analog 알람 설정
            high_limit: 'double',
            low_limit: 'double',
            deadband: 'double',
            
            // Digital 알람 설정
            digital_trigger: 'varchar(20)', // 'on_true', 'on_false', 'on_change'
            
            // Script 알람 설정
            condition_script: 'text',
            message_script: 'text',
            
            // 고급 설정
            suppress_duration: 'int', // 초 단위
            escalation_rules: 'text', // JSON
            tags: 'text', // JSON 배열
            
            // 메타데이터
            created_by: 'int',
            created_at: 'timestamp',
            updated_at: 'timestamp'
        };

        console.log('✅ AlarmRuleRepository 초기화 완료');
    }

    // ========================================================================
    // 🔥 BaseRepository 필수 메서드 구현 (AlarmQueries 활용)
    // ========================================================================

    /**
     * 모든 알람 규칙 조회 (페이징, 필터링 지원)
     */
    async findAll(options = {}) {
        try {
            const {
                tenantId,
                enabled,
                alarmType,
                severity,
                targetType,
                page = 1,
                limit = 50,
                sortBy = 'created_at',
                sortOrder = 'DESC'
            } = options;

            // 기본 쿼리 시작
            let query = AlarmQueries.AlarmRule.FIND_ALL;
            const params = [];
            const conditions = [];

            // 테넌트 필터링
            if (tenantId) {
                conditions.push('tenant_id = ?');
                params.push(tenantId);
            }

            // 활성화 상태 필터링
            if (enabled !== undefined) {
                conditions.push('is_enabled = ?');
                params.push(enabled ? 1 : 0);
            }

            // 알람 타입 필터링
            if (alarmType) {
                conditions.push('alarm_type = ?');
                params.push(alarmType);
            }

            // 심각도 필터링
            if (severity) {
                conditions.push('severity = ?');
                params.push(severity);
            }

            // 대상 타입 필터링
            if (targetType) {
                conditions.push('target_type = ?');
                params.push(targetType);
            }

            // WHERE 절 추가
            if (conditions.length > 0) {
                query += ` WHERE ${conditions.join(' AND ')}`;
            }

            // 정렬 추가
            query += ` ORDER BY ${sortBy} ${sortOrder}`;

            // 페이징 추가
            if (limit) {
                query += ` LIMIT ? OFFSET ?`;
                params.push(limit, (page - 1) * limit);
            }

            const results = await this.executeQuery(query, params);

            // 결과 포맷팅
            const alarmRules = results.map(rule => this.formatAlarmRule(rule));

            // 전체 개수 조회 (페이징용)
            let countQuery = 'SELECT COUNT(*) as total FROM alarm_rules';
            if (conditions.length > 0) {
                countQuery += ` WHERE ${conditions.join(' AND ')}`;
            }
            const countResult = await this.executeQuerySingle(countQuery, params.slice(0, conditions.length));
            const total = countResult ? countResult.total : 0;

            console.log(`✅ 알람 규칙 ${alarmRules.length}개 조회 완료 (총 ${total}개)`);

            return {
                items: alarmRules,
                pagination: {
                    page,
                    limit,
                    total,
                    totalPages: Math.ceil(total / limit)
                }
            };

        } catch (error) {
            console.error('❌ AlarmRuleRepository.findAll 실패:', error);
            throw error;
        }
    }

    /**
     * ID로 알람 규칙 조회
     */
    async findById(id, tenantId = null) {
        try {
            const cacheKey = `alarmRule_${id}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            let query = AlarmQueries.AlarmRule.FIND_BY_ID;
            const params = [id];

            // 테넌트 필터링
            if (tenantId) {
                query += AlarmQueries.AlarmRule.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            const result = await this.executeQuerySingle(query, params);
            const alarmRule = result ? this.formatAlarmRule(result) : null;

            if (alarmRule) {
                this.setCache(cacheKey, alarmRule);
            }

            console.log(`✅ 알람 규칙 ID ${id} 조회: ${alarmRule ? '성공' : '없음'}`);
            return alarmRule;

        } catch (error) {
            console.error(`❌ AlarmRuleRepository.findById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 규칙 생성
     */
    async create(alarmRuleData, tenantId = null) {
        try {
            const data = {
                ...alarmRuleData,
                tenant_id: tenantId || alarmRuleData.tenant_id,
                created_at: new Date().toISOString(),
                updated_at: new Date().toISOString()
            };

            // 필수 필드 검증
            const requiredFields = ['name', 'target_type', 'target_id', 'alarm_type', 'severity'];
            for (const field of requiredFields) {
                if (!data[field]) {
                    throw new Error(`필수 필드 누락: ${field}`);
                }
            }

            // JSON 필드들 문자열화
            if (data.escalation_rules && typeof data.escalation_rules === 'object') {
                data.escalation_rules = JSON.stringify(data.escalation_rules);
            }
            if (data.tags && Array.isArray(data.tags)) {
                data.tags = JSON.stringify(data.tags);
            }

            const params = [
                data.tenant_id, data.name, data.description, data.target_type, data.target_id,
                data.alarm_type, data.severity, data.priority || 1, data.is_enabled ? 1 : 0, data.auto_clear ? 1 : 0,
                data.high_limit, data.low_limit, data.deadband, data.digital_trigger,
                data.condition_script, data.message_script, data.suppress_duration,
                data.escalation_rules, data.tags, data.created_by, data.created_at, data.updated_at
            ];

            const result = await this.executeNonQuery(AlarmQueries.AlarmRule.CREATE, params);

            if (result && result.lastID) {
                // 캐시 무효화
                this.invalidateCache('alarmRule');
                
                console.log(`✅ 새 알람 규칙 생성 완료: ID ${result.lastID}`);
                return await this.findById(result.lastID, tenantId);
            }

            throw new Error('알람 규칙 생성 실패');

        } catch (error) {
            console.error('❌ AlarmRuleRepository.create 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 규칙 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            const data = {
                ...updateData,
                updated_at: new Date().toISOString()
            };

            // JSON 필드들 문자열화
            if (data.escalation_rules && typeof data.escalation_rules === 'object') {
                data.escalation_rules = JSON.stringify(data.escalation_rules);
            }
            if (data.tags && Array.isArray(data.tags)) {
                data.tags = JSON.stringify(data.tags);
            }

            const params = [
                data.name, data.description, data.target_type, data.target_id,
                data.alarm_type, data.severity, data.priority, data.is_enabled ? 1 : 0, data.auto_clear ? 1 : 0,
                data.high_limit, data.low_limit, data.deadband, data.digital_trigger,
                data.condition_script, data.message_script, data.suppress_duration,
                data.escalation_rules, data.tags, data.updated_at, id
            ];

            let query = AlarmQueries.AlarmRule.UPDATE;
            
            // 테넌트 필터링
            if (tenantId) {
                query = query.replace('WHERE id = ?', 'WHERE id = ? AND tenant_id = ?');
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache('alarmRule');
                
                console.log(`✅ 알람 규칙 ID ${id} 업데이트 완료`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ AlarmRuleRepository.update(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 규칙 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            let query = AlarmQueries.AlarmRule.DELETE;
            const params = [id];

            // 테넌트 필터링
            if (tenantId) {
                query = 'DELETE FROM alarm_rules WHERE id = ? AND tenant_id = ?';
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache('alarmRule');
                
                console.log(`✅ 알람 규칙 ID ${id} 삭제 완료`);
                return true;
            }

            return false;

        } catch (error) {
            console.error(`❌ AlarmRuleRepository.deleteById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 규칙 존재 확인
     */
    async exists(id, tenantId = null) {
        try {
            let query = AlarmQueries.AlarmRule.EXISTS;
            const params = [id];

            if (tenantId) {
                query = 'SELECT 1 FROM alarm_rules WHERE id = ? AND tenant_id = ? LIMIT 1';
                params.push(tenantId);
            }

            const result = await this.executeQuerySingle(query, params);
            return !!result;

        } catch (error) {
            console.error(`❌ AlarmRuleRepository.exists(${id}) 실패:`, error);
            return false;
        }
    }

    // ========================================================================
    // 🔥 알람 규칙 특화 메서드들 (C++ 메서드와 동일)
    // ========================================================================

    /**
     * 대상별 알람 규칙 조회
     */
    async findByTarget(targetType, targetId, tenantId = null) {
        try {
            const cacheKey = `alarmRulesByTarget_${targetType}_${targetId}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            let query = AlarmQueries.AlarmRule.FIND_BY_TARGET;
            const params = [targetType, targetId];

            if (tenantId) {
                query += AlarmQueries.AlarmRule.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            const results = await this.executeQuery(query, params);
            const alarmRules = results.map(rule => this.formatAlarmRule(rule));

            this.setCache(cacheKey, alarmRules);
            console.log(`✅ ${targetType}:${targetId} 알람 규칙 ${alarmRules.length}개 조회 완료`);
            return alarmRules;

        } catch (error) {
            console.error(`❌ findByTarget(${targetType}, ${targetId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 활성화된 알람 규칙만 조회
     */
    async findEnabled(tenantId = null) {
        try {
            const cacheKey = `enabledAlarmRules_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            let query = AlarmQueries.AlarmRule.FIND_ENABLED;
            const params = [];

            if (tenantId) {
                query += AlarmQueries.AlarmRule.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            query += AlarmQueries.AlarmRule.CONDITIONS.ORDER_BY_SEVERITY;

            const results = await this.executeQuery(query, params);
            const alarmRules = results.map(rule => this.formatAlarmRule(rule));

            this.setCache(cacheKey, alarmRules);
            console.log(`✅ 활성화된 알람 규칙 ${alarmRules.length}개 조회 완료`);
            return alarmRules;

        } catch (error) {
            console.error('❌ findEnabled 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 타입별 조회
     */
    async findByType(alarmType, tenantId = null) {
        try {
            let query = AlarmQueries.AlarmRule.FIND_BY_TYPE;
            const params = [alarmType];

            if (tenantId) {
                query += AlarmQueries.AlarmRule.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            const results = await this.executeQuery(query, params);
            return results.map(rule => this.formatAlarmRule(rule));

        } catch (error) {
            console.error(`❌ findByType(${alarmType}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 통계 조회
     */
    async getStatsByTenant(tenantId) {
        try {
            const cacheKey = `alarmRuleStats_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            // 총 개수
            const totalResult = await this.executeQuerySingle(
                AlarmQueries.AlarmRule.COUNT_BY_TENANT, 
                [tenantId]
            );

            // 활성화된 개수
            const enabledResult = await this.executeQuerySingle(
                'SELECT COUNT(*) as count FROM alarm_rules WHERE tenant_id = ? AND is_enabled = 1',
                [tenantId]
            );

            // 심각도별 분포
            const severityResults = await this.executeQuery(
                'SELECT severity, COUNT(*) as count FROM alarm_rules WHERE tenant_id = ? GROUP BY severity',
                [tenantId]
            );

            // 타입별 분포
            const typeResults = await this.executeQuery(
                'SELECT alarm_type, COUNT(*) as count FROM alarm_rules WHERE tenant_id = ? GROUP BY alarm_type',
                [tenantId]
            );

            const stats = {
                total_rules: totalResult ? totalResult.count : 0,
                enabled_rules: enabledResult ? enabledResult.count : 0,
                disabled_rules: (totalResult ? totalResult.count : 0) - (enabledResult ? enabledResult.count : 0),
                severity_distribution: severityResults.reduce((acc, row) => {
                    acc[row.severity] = row.count;
                    return acc;
                }, {}),
                type_distribution: typeResults.reduce((acc, row) => {
                    acc[row.alarm_type] = row.count;
                    return acc;
                }, {})
            };

            this.setCache(cacheKey, stats);
            return stats;

        } catch (error) {
            console.error('❌ getStatsByTenant 실패:', error);
            throw error;
        }
    }

    // ========================================================================
    // 🔥 헬퍼 메서드들
    // ========================================================================

    /**
     * 알람 규칙 데이터 포맷팅 (JSON 필드 파싱 포함)
     */
    formatAlarmRule(rule) {
        if (!rule) return null;

        try {
            return {
                ...rule,
                is_enabled: !!rule.is_enabled,
                auto_clear: !!rule.auto_clear,
                escalation_rules: rule.escalation_rules ? JSON.parse(rule.escalation_rules) : null,
                tags: rule.tags ? JSON.parse(rule.tags) : [],
                created_at: rule.created_at,
                updated_at: rule.updated_at
            };
        } catch (error) {
            console.warn(`JSON 파싱 실패 for rule ${rule.id}:`, error);
            return rule;
        }
    }
}

module.exports = AlarmRuleRepository;