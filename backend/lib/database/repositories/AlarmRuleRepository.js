// ============================================================================
// backend/lib/database/repositories/AlarmRuleRepository.js
// BaseRepository 상속받은 AlarmRuleRepository - 쿼리 분리 적용, INSERT 오류 수정
// ============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

/**
 * 알람 규칙 Repository 클래스 (C++ AlarmRuleRepository와 동일한 구조)
 * BaseRepository를 상속받아 공통 기능 활용
 * 
 * 🔧 주요 개선사항:
 * - AlarmQueries.js 쿼리 분리 패턴 적용
 * - INSERT 컬럼/값 개수 불일치 문제 해결 (15개 컬럼 사용)
 * - 헬퍼 메서드로 파라미터 생성 자동화
 * - 필드 검증 로직 추가
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

        console.log('✅ AlarmRuleRepository 초기화 완료 (쿼리 분리 적용)');
    }

    // ========================================================================
    // 🔥 BaseRepository 필수 메서드 구현 (AlarmQueries 활용)
    // ========================================================================

    /**
     * 모든 알람 규칙 조회 (페이징, 필터링 지원)
     */
    async findAll(filters = {}) {
        try {
            let query = AlarmQueries.AlarmRule.FIND_ALL;
            const params = [filters.tenant_id || 1];

            // 동적 WHERE 절 추가
            if (filters.target_type || filters.alarm_type || filters.severity || 
                filters.is_enabled !== undefined || filters.search) {
                
                const { query: updatedQuery, params: additionalParams } = 
                    AlarmQueries.buildAlarmRuleWhereClause(query, filters);
                
                query = updatedQuery;
                params.push(...additionalParams.slice(1)); // tenant_id 제외하고 추가
            }

            // 페이징 추가
            if (filters.limit) {
                query = AlarmQueries.addPagination(query, filters.limit, filters.offset);
                params.push(parseInt(filters.limit));
                if (filters.offset) {
                    params.push(parseInt(filters.offset));
                }
            }

            const results = await this.executeQuery(query, params);
            
            // 🔥 results가 undefined인 경우 처리
            const items = results || [];
            
            return {
                items: items.map(rule => this.formatAlarmRule(rule)),
                pagination: {
                    page: parseInt(filters.page) || 1,
                    limit: parseInt(filters.limit) || 50,
                    total: items.length,
                    totalPages: Math.ceil(items.length / (parseInt(filters.limit) || 50))
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
    async findById(id) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_BY_ID;
            const results = await this.executeQuery(query, [id, 1]); // tenant_id = 1 기본값
            return results && results.length > 0 ? results[0] : null;
        } catch (error) {
            console.error('Error in AlarmRuleRepository.findById:', error);
            throw error;
        }
    }

    /**
     * 🔧 알람 규칙 생성 - INSERT 오류 완전 해결
     * AlarmQueries의 헬퍼 메서드 사용으로 안전한 파라미터 생성
     */
    async create(alarmRuleData, tenantId = null) {
        try {
            const data = {
                ...alarmRuleData,
                tenant_id: tenantId || alarmRuleData.tenant_id || 1
            };

            // 필수 필드 검증
            AlarmQueries.validateRequiredFields(data);

            const query = AlarmQueries.AlarmRule.CREATE;
            const params = AlarmQueries.buildCreateParams(data);

            console.log(`🔧 INSERT 쿼리 실행 - 컬럼: 15개, 값: ${params.length}개`);

            const result = await this.executeNonQuery(query, params);
            
            // 🔥 수정: SQLite 결과 처리 방식 개선
            let newId = null;
            if (result) {
                newId = result.lastInsertRowid || result.insertId || result.lastID;
                console.log('INSERT 결과:', result);
                console.log('추출된 ID:', newId);
            }
            
            if (newId) {
                console.log(`✅ 알람 규칙 생성 성공 - ID: ${newId}`);
                return await this.findById(newId);
            } else {
                // 🔥 대안: 최근 생성된 규칙 조회
                console.log('⚠️ ID 반환 실패, 최근 생성된 규칙 조회 시도...');
                const recentQuery = `
                    SELECT * FROM alarm_rules 
                    WHERE tenant_id = ? AND name = ? 
                    ORDER BY created_at DESC, id DESC 
                    LIMIT 1
                `;
                const recent = await this.executeQuerySingle(recentQuery, [data.tenant_id, data.name]);
                
                if (recent) {
                    console.log(`✅ 최근 생성된 규칙 발견 - ID: ${recent.id}`);
                    return this.formatAlarmRule(recent);
                } else {
                    throw new Error('알람 규칙 생성 실패 - 생성된 규칙을 찾을 수 없음');
                }
            }

        } catch (error) {
            console.error('❌ AlarmRuleRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 알람 규칙 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            // 필수 필드 검증
            AlarmQueries.validateRequiredFields(updateData);
            AlarmQueries.validateAlarmTypeSpecificFields(updateData);

            const query = AlarmQueries.AlarmRule.UPDATE;
            const params = AlarmQueries.buildUpdateParams(updateData, id, tenantId);

            const result = await this.executeNonQuery(query, params);
            
            if (result && result.changes > 0) {
                return await this.findById(id);
            } else {
                return null;
            }

        } catch (error) {
            console.error('Error updating alarm rule:', error);
            throw error;
        }
    }

    /**
     * 알람 규칙 삭제
     */
    async delete(id, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.DELETE;
            const result = await this.executeNonQuery(query, [id, tenantId || 1]);
            return result && result.changes > 0;
        } catch (error) {
            console.error('Error deleting alarm rule:', error);
            throw error;
        }
    }

    /**
     * 알람 규칙 존재 확인
     */
    async exists(id, tenantId = null) {
        try {
            const query = tenantId 
                ? AlarmQueries.AlarmRule.EXISTS
                : AlarmQueries.AlarmRule.EXISTS_SIMPLE;
            
            const params = tenantId ? [id, tenantId] : [id];
            const result = await this.executeQuerySingle(query, params);
            return !!result;

        } catch (error) {
            console.error(`❌ AlarmRuleRepository.exists(${id}) 실패:`, error);
            return false;
        }
    }

    // ========================================================================
    // 🔥 알람 규칙 특화 메서드들 (AlarmQueries 활용)
    // ========================================================================

    /**
     * 대상별 알람 규칙 조회
     */
    async findByTarget(targetType, targetId, tenantId = null) {
        try {
            const cacheKey = `alarmRulesByTarget_${targetType}_${targetId}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmRule.FIND_BY_TARGET;
            const params = [targetType, targetId, tenantId || 1];

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

            const query = AlarmQueries.AlarmRule.FIND_ENABLED;
            const params = [tenantId || 1];

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
            const query = AlarmQueries.AlarmRule.FIND_BY_TYPE;
            const params = [alarmType, tenantId || 1];

            const results = await this.executeQuery(query, params);
            return results.map(rule => this.formatAlarmRule(rule));

        } catch (error) {
            console.error(`❌ findByType(${alarmType}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 심각도별 조회
     */
    async findBySeverity(severity, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_BY_SEVERITY;
            const params = [severity, tenantId || 1];

            const results = await this.executeQuery(query, params);
            return results.map(rule => this.formatAlarmRule(rule));

        } catch (error) {
            console.error(`❌ findBySeverity(${severity}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 통계 조회
     */
    async getStatistics(tenantId = 1) {
        try {
            const query = AlarmQueries.AlarmRule.STATS_SUMMARY;
            const result = await this.executeQuerySingle(query, [tenantId]);
            return result || {
                total_rules: 0,
                enabled_rules: 0,
                target_types: 0,
                alarm_types: 0,
                severity_levels: 0
            };
        } catch (error) {
            console.error('Error getting alarm rule statistics:', error);
            return { 
                total_rules: 0, 
                enabled_rules: 0, 
                target_types: 0, 
                alarm_types: 0, 
                severity_levels: 0 
            };
        }
    }

    /**
     * 심각도별 통계
     */
    async getStatsBySeverity(tenantId = 1) {
        try {
            const query = AlarmQueries.AlarmRule.STATS_BY_SEVERITY;
            const results = await this.executeQuery(query, [tenantId]);
            
            return results.reduce((acc, row) => {
                acc[row.severity] = {
                    total: row.count,
                    enabled: row.enabled_count
                };
                return acc;
            }, {});
        } catch (error) {
            console.error('Error getting severity statistics:', error);
            return {};
        }
    }

    /**
     * 타입별 통계
     */
    async getStatsByType(tenantId = 1) {
        try {
            const query = AlarmQueries.AlarmRule.STATS_BY_TYPE;
            const results = await this.executeQuery(query, [tenantId]);
            
            return results.reduce((acc, row) => {
                acc[row.alarm_type] = {
                    total: row.count,
                    enabled: row.enabled_count
                };
                return acc;
            }, {});
        } catch (error) {
            console.error('Error getting type statistics:', error);
            return {};
        }
    }

    /**
     * 검색 기능
     */
    async search(searchTerm, tenantId = 1, limit = 50) {
        try {
            const query = AlarmQueries.AlarmRule.SEARCH;
            const searchPattern = `%${searchTerm}%`;
            const params = [tenantId, searchPattern, searchPattern, searchPattern];
            
            if (limit) {
                const queryWithLimit = AlarmQueries.addPagination(query, limit);
                params.push(limit);
                const results = await this.executeQuery(queryWithLimit, params);
                return results.map(rule => this.formatAlarmRule(rule));
            }
            
            const results = await this.executeQuery(query, params);
            return results.map(rule => this.formatAlarmRule(rule));

        } catch (error) {
            console.error(`❌ search("${searchTerm}") 실패:`, error);
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
                notification_enabled: !!rule.notification_enabled,
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

    /**
     * 캐시에서 데이터 조회
     */
    getFromCache(key) {
        if (!this.cacheEnabled || !this.cache) return null;
        
        const cached = this.cache.get(key);
        if (cached && Date.now() - cached.timestamp < this.cacheTimeout) {
            return cached.data;
        }
        
        // 만료된 캐시 제거
        if (cached) {
            this.cache.delete(key);
        }
        return null;
    }

    /**
     * 캐시에 데이터 저장
     */
    setCache(key, data) {
        if (!this.cacheEnabled || !this.cache) return;
        
        this.cache.set(key, {
            data: data,
            timestamp: Date.now()
        });
    }

    /**
     * 캐시 초기화
     */
    clearCache() {
        if (this.cache) {
            this.cache.clear();
            console.log('🗑️ AlarmRuleRepository 캐시 초기화 완료');
        }
    }
}

module.exports = AlarmRuleRepository;