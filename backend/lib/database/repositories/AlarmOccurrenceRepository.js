// ============================================================================
// backend/lib/database/repositories/AlarmOccurrenceRepository.js
// BaseRepository 상속받은 AlarmOccurrenceRepository - C++ 패턴과 100% 일치
// ============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

/**
 * 알람 발생 Repository 클래스 (C++ AlarmOccurrenceRepository와 동일한 구조)
 * BaseRepository를 상속받아 공통 기능 활용
 */
class AlarmOccurrenceRepository extends BaseRepository {
    constructor() {
        super('alarm_occurrences');
        
        // 알람 발생 특화 필드 정의 (C++ AlarmOccurrenceEntity와 동일)
        this.fields = {
            id: 'autoIncrement',
            rule_id: 'int',
            device_id: 'int',
            point_id: 'int',
            occurrence_time: 'timestamp',
            state: 'varchar(20)', // 'active', 'acknowledged', 'cleared', 'suppressed'
            trigger_value: 'varchar(100)', // 트리거된 값
            message: 'text',
            severity: 'varchar(20)', // 'critical', 'high', 'medium', 'low'
            priority: 'int',
            
            // 확인 관련
            acknowledged_at: 'timestamp',
            acknowledged_by: 'int',
            
            // 해제 관련
            cleared_at: 'timestamp',
            cleared_by: 'int',
            clear_comment: 'text',
            
            // 고급 기능
            escalation_level: 'int',
            suppress_until: 'timestamp',
            tags: 'text', // JSON 배열
            
            // 메타데이터
            created_at: 'timestamp',
            updated_at: 'timestamp'
        };

        console.log('✅ AlarmOccurrenceRepository 초기화 완료');
    }

    // ========================================================================
    // 🔥 BaseRepository 필수 메서드 구현 (AlarmQueries 활용)
    // ========================================================================

    /**
     * 모든 알람 발생 조회 (페이징, 필터링 지원)
     */
    async findAll(options = {}) {
        try {
            const {
                tenantId,
                state,
                severity,
                ruleId,
                deviceId,
                acknowledged,
                page = 1,
                limit = 50,
                sortBy = 'occurrence_time',
                sortOrder = 'DESC'
            } = options;

            // 기본 쿼리 시작
            let query = AlarmQueries.AlarmOccurrence.FIND_ALL;
            const params = [];
            const conditions = [];

            // 테넌트 필터링 (알람 규칙을 통해)
            if (tenantId) {
                conditions.push('rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)');
                params.push(tenantId);
            }

            // 상태 필터링
            if (state) {
                conditions.push('state = ?');
                params.push(state);
            }

            // 심각도 필터링
            if (severity) {
                conditions.push('severity = ?');
                params.push(severity);
            }

            // 규칙 ID 필터링
            if (ruleId) {
                conditions.push('rule_id = ?');
                params.push(ruleId);
            }

            // 디바이스 ID 필터링
            if (deviceId) {
                conditions.push('device_id = ?');
                params.push(deviceId);
            }

            // 확인 상태 필터링
            if (acknowledged !== undefined) {
                if (acknowledged) {
                    conditions.push('acknowledged_at IS NOT NULL');
                } else {
                    conditions.push('acknowledged_at IS NULL');
                }
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
            const alarmOccurrences = results.map(occurrence => this.formatAlarmOccurrence(occurrence));

            // 전체 개수 조회 (페이징용)
            let countQuery = 'SELECT COUNT(*) as total FROM alarm_occurrences';
            if (conditions.length > 0) {
                countQuery += ` WHERE ${conditions.join(' AND ')}`;
            }
            const countResult = await this.executeQuerySingle(countQuery, params.slice(0, conditions.length));
            const total = countResult ? countResult.total : 0;

            console.log(`✅ 알람 발생 ${alarmOccurrences.length}개 조회 완료 (총 ${total}개)`);

            return {
                items: alarmOccurrences,
                pagination: {
                    page,
                    limit,
                    total,
                    totalPages: Math.ceil(total / limit)
                }
            };

        } catch (error) {
            console.error('❌ AlarmOccurrenceRepository.findAll 실패:', error);
            throw error;
        }
    }

    /**
     * ID로 알람 발생 조회
     */
    async findById(id, tenantId = null) {
        try {
            const cacheKey = `alarmOccurrence_${id}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            let query = AlarmQueries.AlarmOccurrence.FIND_BY_ID;
            const params = [id];

            // 테넌트 필터링
            if (tenantId) {
                query += AlarmQueries.AlarmOccurrence.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            const result = await this.executeQuerySingle(query, params);
            const alarmOccurrence = result ? this.formatAlarmOccurrence(result) : null;

            if (alarmOccurrence) {
                this.setCache(cacheKey, alarmOccurrence);
            }

            console.log(`✅ 알람 발생 ID ${id} 조회: ${alarmOccurrence ? '성공' : '없음'}`);
            return alarmOccurrence;

        } catch (error) {
            console.error(`❌ AlarmOccurrenceRepository.findById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 발생 생성
     */
    async create(alarmOccurrenceData) {
        try {
            const data = {
                ...alarmOccurrenceData,
                occurrence_time: alarmOccurrenceData.occurrence_time || new Date().toISOString(),
                state: alarmOccurrenceData.state || 'active',
                created_at: new Date().toISOString(),
                updated_at: new Date().toISOString()
            };

            // 필수 필드 검증
            const requiredFields = ['rule_id', 'message', 'severity'];
            for (const field of requiredFields) {
                if (!data[field]) {
                    throw new Error(`필수 필드 누락: ${field}`);
                }
            }

            // JSON 필드들 문자열화
            if (data.tags && Array.isArray(data.tags)) {
                data.tags = JSON.stringify(data.tags);
            }

            const params = [
                data.rule_id, data.device_id, data.point_id, data.occurrence_time, data.state,
                data.trigger_value, data.message, data.severity, data.priority || 1, data.acknowledged_at,
                data.acknowledged_by, data.cleared_at, data.cleared_by, data.clear_comment,
                data.escalation_level || 0, data.suppress_until, data.tags, data.created_at, data.updated_at
            ];

            const result = await this.executeNonQuery(AlarmQueries.AlarmOccurrence.CREATE, params);

            if (result && result.lastID) {
                // 캐시 무효화
                this.invalidateCache('alarmOccurrence');
                
                console.log(`✅ 새 알람 발생 생성 완료: ID ${result.lastID}`);
                return await this.findById(result.lastID);
            }

            throw new Error('알람 발생 생성 실패');

        } catch (error) {
            console.error('❌ AlarmOccurrenceRepository.create 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 발생 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            const data = {
                ...updateData,
                updated_at: new Date().toISOString()
            };

            // JSON 필드들 문자열화
            if (data.tags && Array.isArray(data.tags)) {
                data.tags = JSON.stringify(data.tags);
            }

            const params = [
                data.state, data.acknowledged_at, data.acknowledged_by,
                data.cleared_at, data.cleared_by, data.clear_comment,
                data.escalation_level, data.suppress_until, data.tags, data.updated_at, id
            ];

            let query = AlarmQueries.AlarmOccurrence.UPDATE;
            
            // 테넌트 필터링
            if (tenantId) {
                query = query.replace('WHERE id = ?', 
                    'WHERE id = ? AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)');
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache('alarmOccurrence');
                
                console.log(`✅ 알람 발생 ID ${id} 업데이트 완료`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ AlarmOccurrenceRepository.update(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 발생 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            let query = AlarmQueries.AlarmOccurrence.DELETE;
            const params = [id];

            // 테넌트 필터링
            if (tenantId) {
                query = `DELETE FROM alarm_occurrences 
                        WHERE id = ? AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)`;
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache('alarmOccurrence');
                
                console.log(`✅ 알람 발생 ID ${id} 삭제 완료`);
                return true;
            }

            return false;

        } catch (error) {
            console.error(`❌ AlarmOccurrenceRepository.deleteById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 발생 존재 확인
     */
    async exists(id, tenantId = null) {
        try {
            let query = AlarmQueries.AlarmOccurrence.EXISTS;
            const params = [id];

            if (tenantId) {
                query = `SELECT 1 FROM alarm_occurrences 
                        WHERE id = ? AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?) LIMIT 1`;
                params.push(tenantId);
            }

            const result = await this.executeQuerySingle(query, params);
            return !!result;

        } catch (error) {
            console.error(`❌ AlarmOccurrenceRepository.exists(${id}) 실패:`, error);
            return false;
        }
    }

    // ========================================================================
    // 🔥 알람 발생 특화 메서드들 (C++ 메서드와 동일)
    // ========================================================================

    /**
     * 활성 알람만 조회
     */
    async findActive(tenantId = null) {
        try {
            const cacheKey = `activeAlarms_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            let query = AlarmQueries.AlarmOccurrence.FIND_ACTIVE;
            const params = [];

            if (tenantId) {
                query += AlarmQueries.AlarmOccurrence.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            query += AlarmQueries.AlarmOccurrence.CONDITIONS.ORDER_BY_SEVERITY;

            const results = await this.executeQuery(query, params);
            const activeAlarms = results.map(occurrence => this.formatAlarmOccurrence(occurrence));

            this.setCache(cacheKey, activeAlarms, 60000); // 1분 캐시
            console.log(`✅ 활성 알람 ${activeAlarms.length}개 조회 완료`);
            return activeAlarms;

        } catch (error) {
            console.error('❌ findActive 실패:', error);
            throw error;
        }
    }

    /**
     * 미확인 알람만 조회
     */
    async findUnacknowledged(tenantId = null) {
        try {
            let query = AlarmQueries.AlarmOccurrence.FIND_UNACKNOWLEDGED;
            const params = [];

            if (tenantId) {
                query += AlarmQueries.AlarmOccurrence.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            query += AlarmQueries.AlarmOccurrence.CONDITIONS.ORDER_BY_TIME;

            const results = await this.executeQuery(query, params);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error('❌ findUnacknowledged 실패:', error);
            throw error;
        }
    }

    /**
     * 규칙별 알람 발생 조회
     */
    async findByRule(ruleId, tenantId = null) {
        try {
            let query = AlarmQueries.AlarmOccurrence.FIND_BY_RULE;
            const params = [ruleId];

            if (tenantId) {
                query += AlarmQueries.AlarmOccurrence.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            const results = await this.executeQuery(query, params);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`❌ findByRule(${ruleId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 디바이스별 알람 발생 조회
     */
    async findByDevice(deviceId, tenantId = null) {
        try {
            let query = AlarmQueries.AlarmOccurrence.FIND_BY_DEVICE;
            const params = [deviceId];

            if (tenantId) {
                query += AlarmQueries.AlarmOccurrence.CONDITIONS.TENANT_ID;
                params.push(tenantId);
            }

            const results = await this.executeQuery(query, params);
            return results.map(occurrence => this.formatAlarmOccurrence(occurrence));

        } catch (error) {
            console.error(`❌ findByDevice(${deviceId}) 실패:`, error);
            throw error;
        }
    }

    // ========================================================================
    // 🔥 알람 상태 변경 메서드들 (C++ 메서드와 동일)
    // ========================================================================

    /**
     * 알람 확인 (Acknowledge)
     */
    async acknowledge(id, userId, comment = '', tenantId = null) {
        try {
            const acknowledgeTime = new Date().toISOString();
            const params = [acknowledgeTime, userId, acknowledgeTime, id];

            let query = AlarmQueries.AlarmOccurrence.ACKNOWLEDGE;
            
            // 테넌트 필터링
            if (tenantId) {
                query = query.replace('WHERE id = ?', 
                    'WHERE id = ? AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)');
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache();
                
                console.log(`✅ 알람 ID ${id} 확인 처리 완료 (사용자: ${userId})`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ acknowledge(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 해제 (Clear)
     */
    async clear(id, userId, comment = '', tenantId = null) {
        try {
            const clearTime = new Date().toISOString();
            const params = [clearTime, userId, comment, clearTime, id];

            let query = AlarmQueries.AlarmOccurrence.CLEAR;
            
            // 테넌트 필터링
            if (tenantId) {
                query = query.replace('WHERE id = ?', 
                    'WHERE id = ? AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)');
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache();
                
                console.log(`✅ 알람 ID ${id} 해제 처리 완료 (사용자: ${userId})`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ clear(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 알람 억제 (Suppress)
     */
    async suppress(id, suppressUntil, tenantId = null) {
        try {
            const updateTime = new Date().toISOString();
            const params = [suppressUntil, updateTime, id];

            let query = AlarmQueries.AlarmOccurrence.SUPPRESS;
            
            // 테넌트 필터링
            if (tenantId) {
                query = query.replace('WHERE id = ?', 
                    'WHERE id = ? AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)');
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache();
                
                console.log(`✅ 알람 ID ${id} 억제 처리 완료 (${suppressUntil}까지)`);
                return await this.findById(id, tenantId);
            }

            return null;

        } catch (error) {
            console.error(`❌ suppress(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 통계 조회
     */
    async getStatsByTenant(tenantId) {
        try {
            const cacheKey = `alarmOccurrenceStats_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            // 활성 알람 개수
            const activeResult = await this.executeQuerySingle(
                `SELECT COUNT(*) as count FROM alarm_occurrences 
                 WHERE state = 'active' AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)`,
                [tenantId]
            );

            // 미확인 알람 개수
            const unackResult = await this.executeQuerySingle(
                `SELECT COUNT(*) as count FROM alarm_occurrences 
                 WHERE acknowledged_at IS NULL AND state = 'active' 
                 AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)`,
                [tenantId]
            );

            // 심각도별 분포
            const severityResults = await this.executeQuery(
                `SELECT severity, COUNT(*) as count FROM alarm_occurrences 
                 WHERE state = 'active' AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?) 
                 GROUP BY severity`,
                [tenantId]
            );

            const stats = {
                active_alarms: activeResult ? activeResult.count : 0,
                unacknowledged_alarms: unackResult ? unackResult.count : 0,
                acknowledged_alarms: (activeResult ? activeResult.count : 0) - (unackResult ? unackResult.count : 0),
                severity_distribution: severityResults.reduce((acc, row) => {
                    acc[row.severity] = row.count;
                    return acc;
                }, {})
            };

            this.setCache(cacheKey, stats, 30000); // 30초 캐시
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
     * 알람 발생 데이터 포맷팅 (JSON 필드 파싱 포함)
     */
    formatAlarmOccurrence(occurrence) {
        if (!occurrence) return null;

        try {
            return {
                ...occurrence,
                tags: occurrence.tags ? JSON.parse(occurrence.tags) : [],
                is_acknowledged: !!occurrence.acknowledged_at,
                is_cleared: !!occurrence.cleared_at,
                occurrence_time: occurrence.occurrence_time,
                acknowledged_at: occurrence.acknowledged_at,
                cleared_at: occurrence.cleared_at,
                created_at: occurrence.created_at,
                updated_at: occurrence.updated_at
            };
        } catch (error) {
            console.warn(`JSON 파싱 실패 for occurrence ${occurrence.id}:`, error);
            return occurrence;
        }
    }
}

module.exports = AlarmOccurrenceRepository;