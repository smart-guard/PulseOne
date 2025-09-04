// =============================================================================
// backend/lib/database/repositories/AlarmRuleRepository.js
// ProtocolRepository 패턴과 100% 동일하게 구현
// =============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

class AlarmRuleRepository extends BaseRepository {
    constructor() {
        // ProtocolRepository와 동일한 패턴: 매개변수 없는 생성자
        super('alarm_rules');
        console.log('📋 AlarmRuleRepository initialized with standard pattern');
    }

    // ==========================================================================
    // 기본 CRUD 연산 (AlarmQueries 사용)
    // ==========================================================================

    async findAll(filters = {}) {
        try {
            console.log('AlarmRuleRepository.findAll 호출:', filters);
            
            let query = AlarmQueries.AlarmRule.FIND_ALL;
            const params = [];
            const conditions = [];

            // 테넌트 ID 필수
            params.push(filters.tenantId || 1);

            // 필터 조건 추가
            if (filters.targetType && filters.targetType !== 'all') {
                conditions.push('ar.target_type = ?');
                params.push(filters.targetType);
            }

            if (filters.targetId) {
                conditions.push('ar.target_id = ?');
                params.push(parseInt(filters.targetId));
            }

            if (filters.alarmType && filters.alarmType !== 'all') {
                conditions.push('ar.alarm_type = ?');
                params.push(filters.alarmType);
            }

            if (filters.severity && filters.severity !== 'all') {
                conditions.push('ar.severity = ?');
                params.push(filters.severity);
            }

            if (filters.isEnabled === true || filters.isEnabled === 'true') {
                conditions.push('ar.is_enabled = 1');
            } else if (filters.isEnabled === false || filters.isEnabled === 'false') {
                conditions.push('ar.is_enabled = 0');
            }

            if (filters.category && filters.category !== 'all') {
                conditions.push('ar.category = ?');
                params.push(filters.category);
            }

            if (filters.tag) {
                conditions.push('ar.tags LIKE ?');
                params.push(`%${filters.tag}%`);
            }

            if (filters.templateId) {
                conditions.push('ar.template_id = ?');
                params.push(parseInt(filters.templateId));
            }

            if (filters.ruleGroup) {
                conditions.push('ar.rule_group = ?');
                params.push(filters.ruleGroup);
            }

            if (filters.search) {
                conditions.push('(ar.name LIKE ? OR ar.description LIKE ? OR ar.category LIKE ? OR ar.tags LIKE ?)');
                const searchParam = `%${filters.search}%`;
                params.push(searchParam, searchParam, searchParam, searchParam);
            }

            // 조건들을 쿼리에 추가
            if (conditions.length > 0) {
                query += ' AND ' + conditions.join(' AND ');
            }

            // 정렬 추가
            const sortBy = filters.sortBy || 'created_at';
            const sortOrder = filters.sortOrder || 'DESC';
            query += ` ORDER BY ar.${sortBy} ${sortOrder}`;

            // 페이징 처리를 위한 변수
            const page = filters.page || 1;
            const limit = filters.limit || 50;
            const offset = (page - 1) * limit;

            // 전체 개수 먼저 조회 (페이징 정보를 위해)
            let countQuery = 'SELECT COUNT(*) as total FROM alarm_rules ar WHERE ar.tenant_id = ?';
            const countParams = [filters.tenantId || 1];
            
            if (conditions.length > 0) {
                countQuery += ' AND ' + conditions.join(' AND ');
                countParams.push(...params.slice(1));
            }
            
            const countResult = await this.executeQuery(countQuery, countParams);
            const total = countResult.length > 0 ? countResult[0].total : 0;

            // 페이징 적용한 실제 데이터 조회
            query += ' LIMIT ? OFFSET ?';
            params.push(limit, offset);

            const rules = await this.executeQuery(query, params);

            console.log(`✅ 알람 규칙 ${rules.length}개 조회 완료 (전체: ${total}개)`);

            // 페이징 정보와 함께 반환
            return {
                items: rules.map(rule => this.parseAlarmRule(rule)),
                pagination: {
                    page: page,
                    limit: limit,
                    total: total,
                    totalPages: Math.ceil(total / limit),
                    hasNext: page < Math.ceil(total / limit),
                    hasPrev: page > 1
                }
            };
            
        } catch (error) {
            console.error('AlarmRuleRepository.findAll 실패:', error.message);
            throw error;
        }
    }

    async findById(id, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.findById 호출: id=${id}, tenantId=${tenantId}`);
            
            const query = AlarmQueries.AlarmRule.FIND_BY_ID;
            const params = [id, tenantId || 1];

            const rules = await this.executeQuery(query, params);
            
            if (rules.length === 0) {
                console.log(`알람 규칙 ID ${id} 찾을 수 없음`);
                return null;
            }
            
            console.log(`✅ 알람 규칙 ID ${id} 조회 성공`);
            
            return this.parseAlarmRule(rules[0]);
            
        } catch (error) {
            console.error('AlarmRuleRepository.findById 실패:', error.message);
            throw error;
        }
    }

    async findByCategory(category, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_BY_CATEGORY;
            const params = [category, tenantId || 1];
            
            const rules = await this.executeQuery(query, params);
            
            return rules.map(rule => this.parseAlarmRule(rule));
            
        } catch (error) {
            console.error('AlarmRuleRepository.findByCategory 실패:', error.message);
            throw error;
        }
    }

    async findByTag(tag, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_BY_TAG;
            const params = [`%${tag}%`, tenantId || 1];
            
            const rules = await this.executeQuery(query, params);
            
            return rules.map(rule => this.parseAlarmRule(rule));
            
        } catch (error) {
            console.error('AlarmRuleRepository.findByTag 실패:', error.message);
            throw error;
        }
    }

    async findByTarget(targetType, targetId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_BY_TARGET;
            const params = [targetType, targetId, tenantId || 1];
            
            const rules = await this.executeQuery(query, params);
            
            return rules.map(rule => this.parseAlarmRule(rule));
            
        } catch (error) {
            console.error('AlarmRuleRepository.findByTarget 실패:', error.message);
            throw error;
        }
    }

    async findEnabled(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_ENABLED;
            const params = [tenantId || 1];
            
            const rules = await this.executeQuery(query, params);
            
            return rules.map(rule => this.parseAlarmRule(rule));
            
        } catch (error) {
            console.error('AlarmRuleRepository.findEnabled 실패:', error.message);
            throw error;
        }
    }

    async findByType(alarmType, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_BY_TYPE;
            const params = [alarmType, tenantId || 1];
            
            const rules = await this.executeQuery(query, params);
            
            return rules.map(rule => this.parseAlarmRule(rule));
            
        } catch (error) {
            console.error('AlarmRuleRepository.findByType 실패:', error.message);
            throw error;
        }
    }

    async findBySeverity(severity, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.FIND_BY_SEVERITY;
            const params = [severity, tenantId || 1];
            
            const rules = await this.executeQuery(query, params);
            
            return rules.map(rule => this.parseAlarmRule(rule));
            
        } catch (error) {
            console.error('AlarmRuleRepository.findBySeverity 실패:', error.message);
            throw error;
        }
    }

    async search(searchTerm, tenantId = null, limit = 50) {
        try {
            const query = AlarmQueries.AlarmRule.SEARCH;
            const searchParam = `%${searchTerm}%`;
            const params = [
                tenantId || 1,
                searchParam, searchParam, searchParam, searchParam, searchParam,
                searchParam, searchParam, searchParam, searchParam, searchParam
            ];
            
            let finalQuery = query;
            if (limit) {
                finalQuery += ' LIMIT ?';
                params.push(limit);
            }
            
            const rules = await this.executeQuery(finalQuery, params);
            
            return rules.map(rule => this.parseAlarmRule(rule));
            
        } catch (error) {
            console.error('AlarmRuleRepository.search 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 생성, 수정, 삭제 연산
    // ==========================================================================

    async create(ruleData, userId = null) {
        try {
            console.log('AlarmRuleRepository.create 호출:', ruleData.name);
            
            // 필수 필드 검증
            AlarmQueries.validateAlarmRule(ruleData);
            
            const query = AlarmQueries.AlarmRule.CREATE;
            const params = AlarmQueries.buildCreateRuleParams({
                ...ruleData,
                created_by: userId || ruleData.created_by
            });

            const result = await this.executeNonQuery(query, params);
            const insertId = result.lastInsertRowid || result.insertId || result.lastID;

            if (insertId) {
                console.log(`✅ 알람 규칙 생성 완료 (ID: ${insertId})`);
                return await this.findById(insertId, ruleData.tenant_id);
            } else {
                throw new Error('Alarm rule creation failed - no ID returned');
            }
            
        } catch (error) {
            console.error('AlarmRuleRepository.create 실패:', error.message);
            throw error;
        }
    }

    async update(id, updateData, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.update 호출: ID ${id}`);

            // 존재 확인
            const existing = await this.findById(id, tenantId);
            if (!existing) {
                throw new Error(`Alarm rule with ID ${id} not found`);
            }

            const query = AlarmQueries.AlarmRule.UPDATE;
            const params = AlarmQueries.buildUpdateRuleParams(updateData, id, tenantId || 1);
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 업데이트 완료`);
                return await this.findById(id, tenantId);
            } else {
                throw new Error(`Alarm rule with ID ${id} update failed`);
            }
            
        } catch (error) {
            console.error('AlarmRuleRepository.update 실패:', error.message);
            throw error;
        }
    }

    async updateEnabledStatus(id, isEnabled, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.updateEnabledStatus 호출: ID ${id}, isEnabled=${isEnabled}`);

            const query = AlarmQueries.AlarmRule.UPDATE_ENABLED_STATUS;
            const params = AlarmQueries.buildEnabledStatusParams(isEnabled, id, tenantId || 1);
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 상태 업데이트 완료`);
                return { id: parseInt(id), is_enabled: isEnabled };
            } else {
                throw new Error(`Alarm rule with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('AlarmRuleRepository.updateEnabledStatus 실패:', error.message);
            throw error;
        }
    }

    async updateSettings(id, settings, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.updateSettings 호출: ID ${id}`);

            const query = AlarmQueries.AlarmRule.UPDATE_SETTINGS_ONLY;
            const params = AlarmQueries.buildSettingsParams(settings, id, tenantId || 1);
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 설정 업데이트 완료`);
                return { id: parseInt(id), updated_settings: settings };
            } else {
                throw new Error(`Alarm rule with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('AlarmRuleRepository.updateSettings 실패:', error.message);
            throw error;
        }
    }

    async updateName(id, name, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.updateName 호출: ID ${id}, name=${name}`);

            const query = AlarmQueries.AlarmRule.UPDATE_NAME_ONLY;
            const params = AlarmQueries.buildNameParams(name, id, tenantId || 1);
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 이름 업데이트 완료`);
                return { id: parseInt(id), name: name };
            } else {
                throw new Error(`Alarm rule with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('AlarmRuleRepository.updateName 실패:', error.message);
            throw error;
        }
    }

    async updateSeverity(id, severity, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.updateSeverity 호출: ID ${id}, severity=${severity}`);

            const query = AlarmQueries.AlarmRule.UPDATE_SEVERITY_ONLY;
            const params = AlarmQueries.buildSeverityParams(severity, id, tenantId || 1);
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 심각도 업데이트 완료`);
                return { id: parseInt(id), severity: severity };
            } else {
                throw new Error(`Alarm rule with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('AlarmRuleRepository.updateSeverity 실패:', error.message);
            throw error;
        }
    }

    async delete(id, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.delete 호출: ID ${id}`);

            const query = AlarmQueries.AlarmRule.DELETE;
            const params = [id, tenantId || 1];
            
            const result = await this.executeNonQuery(query, params);

            if (result.changes && result.changes > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 삭제 완료`);
                return true;
            } else {
                return false;
            }
            
        } catch (error) {
            console.error('AlarmRuleRepository.delete 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 통계 및 집계 연산
    // ==========================================================================

    async getStatsSummary(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.STATS_SUMMARY;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats.length > 0 ? stats[0] : {
                total_rules: 0,
                enabled_rules: 0,
                alarm_types: 0,
                severity_levels: 0,
                target_types: 0,
                categories: 0,
                rules_with_tags: 0
            };
            
        } catch (error) {
            console.error('AlarmRuleRepository.getStatsSummary 실패:', error.message);
            throw error;
        }
    }

    async getStatsBySeverity(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.STATS_BY_SEVERITY;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats;
            
        } catch (error) {
            console.error('AlarmRuleRepository.getStatsBySeverity 실패:', error.message);
            throw error;
        }
    }

    async getStatsByType(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.STATS_BY_TYPE;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats;
            
        } catch (error) {
            console.error('AlarmRuleRepository.getStatsByType 실패:', error.message);
            throw error;
        }
    }

    async getStatsByCategory(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmRule.STATS_BY_CATEGORY;
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats;
            
        } catch (error) {
            console.error('AlarmRuleRepository.getStatsByCategory 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 추가 메서드들 - 기존 기능 유지
    // ==========================================================================

    async exists(id, tenantId = null) {
        try {
            const query = 'SELECT 1 FROM alarm_rules WHERE id = ? AND tenant_id = ? LIMIT 1';
            const params = [id, tenantId || 1];

            const result = await this.executeQuery(query, params);
            return result.length > 0;

        } catch (error) {
            console.error(`AlarmRuleRepository.exists(${id}) 실패:`, error.message);
            return false;
        }
    }

    // ==========================================================================
    // 유틸리티 메소드들 - ProtocolRepository 패턴과 동일
    // ==========================================================================

    parseAlarmRule(rule) {
        if (!rule) return null;

        return {
            ...rule,
            // INTEGER 필드 변환
            id: parseInt(rule.id),
            tenant_id: parseInt(rule.tenant_id),
            target_id: rule.target_id ? parseInt(rule.target_id) : null,
            priority: parseInt(rule.priority) || 100,
            acknowledge_timeout_min: parseInt(rule.acknowledge_timeout_min) || 0,
            notification_delay_sec: parseInt(rule.notification_delay_sec) || 0,
            notification_repeat_interval_min: parseInt(rule.notification_repeat_interval_min) || 0,
            template_id: rule.template_id ? parseInt(rule.template_id) : null,
            created_by: rule.created_by ? parseInt(rule.created_by) : null,
            escalation_max_level: parseInt(rule.escalation_max_level) || 3,
            
            // FLOAT/DOUBLE 필드 변환
            high_high_limit: rule.high_high_limit ? parseFloat(rule.high_high_limit) : null,
            high_limit: rule.high_limit ? parseFloat(rule.high_limit) : null,
            low_limit: rule.low_limit ? parseFloat(rule.low_limit) : null,
            low_low_limit: rule.low_low_limit ? parseFloat(rule.low_low_limit) : null,
            deadband: rule.deadband ? parseFloat(rule.deadband) : 0,
            rate_of_change: rule.rate_of_change ? parseFloat(rule.rate_of_change) : 0,
            
            // Boolean 필드 변환
            is_enabled: Boolean(rule.is_enabled),
            is_latched: Boolean(rule.is_latched),
            auto_acknowledge: Boolean(rule.auto_acknowledge),
            auto_clear: Boolean(rule.auto_clear),
            notification_enabled: Boolean(rule.notification_enabled),
            created_by_template: Boolean(rule.created_by_template),
            escalation_enabled: Boolean(rule.escalation_enabled),
            
            // JSON 필드 파싱
            message_config: this.parseJsonField(rule.message_config),
            suppression_rules: this.parseJsonField(rule.suppression_rules),
            notification_channels: this.parseJsonField(rule.notification_channels),
            notification_recipients: this.parseJsonField(rule.notification_recipients),
            escalation_rules: this.parseJsonField(rule.escalation_rules),
            tags: this.parseJsonField(rule.tags) || []
        };
    }

    parseJsonField(jsonStr) {
        if (!jsonStr) return null;
        try {
            return typeof jsonStr === 'string' ? JSON.parse(jsonStr) : jsonStr;
        } catch (error) {
            console.warn('JSON 파싱 실패:', error.message);
            return null;
        }
    }
}

module.exports = AlarmRuleRepository;