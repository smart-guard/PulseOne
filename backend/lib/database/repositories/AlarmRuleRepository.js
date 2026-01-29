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
            console.log('AlarmRuleRepository.findAll 호출 (Knex):', filters);

            const query = this.query('ar')
                .leftJoin('devices as d', (clause) => {
                    clause.on('ar.target_id', '=', 'd.id').andOn('ar.target_type', '=', this.knex.raw("'device'"));
                })
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .leftJoin('data_points as dp', (clause) => {
                    clause.on('ar.target_id', '=', 'dp.id').andOn('ar.target_type', '=', this.knex.raw("'data_point'"));
                })
                .leftJoin('devices as d2', 'dp.device_id', 'd2.id')
                .leftJoin('sites as s2', 'd2.site_id', 's2.id')
                .leftJoin('virtual_points as vp', (clause) => {
                    clause.on('ar.target_id', '=', 'vp.id').andOn('ar.target_type', '=', this.knex.raw("'virtual_point'"));
                })
                .select(
                    'ar.*',
                    this.knex.raw('COALESCE(d.name, d2.name) as device_name'),
                    this.knex.raw('COALESCE(d.id, d2.id) as device_id'), // Explicitly select Device ID for frontend hydration
                    'd.device_type',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    this.knex.raw('COALESCE(s.name, s2.name) as site_name'),
                    this.knex.raw('COALESCE(s.location, s2.location) as site_location'),
                    'dp.name as data_point_name',
                    'dp.unit',
                    'vp.name as virtual_point_name'
                );

            // 필터 적용
            if (filters.tenantId) {
                query.where('ar.tenant_id', filters.tenantId);
            }

            // 삭제된 항목 필터링 (기본값: 삭제되지 않은 항목만)
            if (filters.deleted === 'true' || filters.deleted === true) {
                query.where('ar.is_deleted', 1);
            } else {
                query.where('ar.is_deleted', 0);
            }

            if (filters.targetType && filters.targetType !== 'all') {
                query.where('ar.target_type', filters.targetType);
            }

            if (filters.targetId) {
                query.where('ar.target_id', parseInt(filters.targetId));
            }

            if (filters.alarmType && filters.alarmType !== 'all') {
                query.where('ar.alarm_type', filters.alarmType);
            }

            if (filters.severity && filters.severity !== 'all') {
                query.where('ar.severity', filters.severity);
            }

            if (filters.isEnabled !== undefined) {
                query.where('ar.is_enabled', filters.isEnabled === true || filters.isEnabled === 'true' ? 1 : 0);
            }

            if (filters.category && filters.category !== 'all') {
                query.where('ar.category', filters.category);
            }

            if (filters.search) {
                const search = `%${filters.search}%`;
                query.where(function () {
                    this.where('ar.name', 'like', search)
                        .orWhere('ar.description', 'like', search)
                        .orWhere('ar.category', 'like', search)
                        .orWhere('d.name', 'like', search)
                        .orWhere('dp.name', 'like', search)
                        .orWhere('vp.name', 'like', search);
                });
            }

            // 정렬 및 페이징
            const page = parseInt(filters.page) || 1;
            const limit = parseInt(filters.limit) || 50;
            const offset = (page - 1) * limit;
            const sortBy = filters.sortBy || 'created_at';
            const sortOrder = filters.sortOrder || 'DESC';

            const [items, countResult] = await Promise.all([
                query.clone().orderBy(`ar.${sortBy}`, sortOrder).limit(limit).offset(offset),
                query.clone().clearSelect().count('* as total').first()
            ]);

            const total = countResult ? parseInt(countResult.total) : 0;

            console.log(`✅ 알람 규칙 ${items.length}개 조회 완료 (전체: ${total}개)`);

            return {
                items: items.map(rule => this.parseAlarmRule(rule)),
                pagination: {
                    page,
                    limit,
                    total,
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
            console.log(`AlarmRuleRepository.findById 호출 (Knex): id=${id}, tenantId=${tenantId}`);

            const query = this.query('ar')
                .leftJoin('devices as d', (clause) => {
                    clause.on('ar.target_id', '=', 'd.id').andOn('ar.target_type', '=', this.knex.raw("'device'"));
                })
                .leftJoin('protocols as p', 'd.protocol_id', 'p.id')
                .leftJoin('sites as s', 'd.site_id', 's.id')
                .leftJoin('data_points as dp', (clause) => {
                    clause.on('ar.target_id', '=', 'dp.id').andOn('ar.target_type', '=', this.knex.raw("'data_point'"));
                })
                .leftJoin('devices as d2', 'dp.device_id', 'd2.id')
                .leftJoin('sites as s2', 'd2.site_id', 's2.id')
                .leftJoin('virtual_points as vp', (clause) => {
                    clause.on('ar.target_id', '=', 'vp.id').andOn('ar.target_type', '=', this.knex.raw("'virtual_point'"));
                })
                .select(
                    'ar.*',
                    this.knex.raw('COALESCE(d.name, d2.name) as device_name'),
                    this.knex.raw('COALESCE(d.id, d2.id) as device_id'), // Explicitly select Device ID
                    'd.device_type',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    this.knex.raw('COALESCE(s.name, s2.name) as site_name'),
                    this.knex.raw('COALESCE(s.location, s2.location) as site_location'),
                    'dp.name as data_point_name',
                    'dp.unit',
                    'vp.name as virtual_point_name'
                )
                .where('ar.id', id);

            if (tenantId) {
                query.where('ar.tenant_id', tenantId);
            }

            const rule = await query.first();

            if (!rule) {
                console.log(`알람 규칙 ID ${id} 찾을 수 없음`);
                return null;
            }

            console.log(`✅ 알람 규칙 ID ${id} 조회 성공`);
            return this.parseAlarmRule(rule);

        } catch (error) {
            console.error('AlarmRuleRepository.findById 실패:', error.message);
            throw error;
        }
    }

    async findByCategory(category, tenantId = null) {
        try {
            const rules = await this.query('ar')
                .where('ar.category', category)
                .where('ar.tenant_id', tenantId || 1)
                .orderBy('ar.created_at', 'DESC');

            return rules.map(rule => this.parseAlarmRule(rule));
        } catch (error) {
            console.error('AlarmRuleRepository.findByCategory 실패:', error.message);
            throw error;
        }
    }

    async findByTag(tag, tenantId = null) {
        try {
            const rules = await this.query('ar')
                .where('ar.tags', 'like', `%${tag}%`)
                .where('ar.tenant_id', tenantId || 1)
                .orderBy('ar.created_at', 'DESC');

            return rules.map(rule => this.parseAlarmRule(rule));
        } catch (error) {
            console.error('AlarmRuleRepository.findByTag 실패:', error.message);
            throw error;
        }
    }

    async findByTarget(targetType, targetId, tenantId = null) {
        try {
            const rules = await this.query('ar')
                .where('ar.target_type', targetType)
                .where('ar.target_id', targetId)
                .where('ar.tenant_id', tenantId || 1)
                .where('ar.is_enabled', 1)
                .orderBy('ar.created_at', 'DESC');

            return rules.map(rule => this.parseAlarmRule(rule));
        } catch (error) {
            console.error('AlarmRuleRepository.findByTarget 실패:', error.message);
            throw error;
        }
    }

    async findEnabled(tenantId = null) {
        try {
            const rules = await this.query('ar')
                .where('ar.is_enabled', 1)
                .where('ar.tenant_id', tenantId || 1)
                .orderBy('ar.severity', 'DESC')
                .orderBy('ar.created_at', 'DESC');

            return rules.map(rule => this.parseAlarmRule(rule));
        } catch (error) {
            console.error('AlarmRuleRepository.findEnabled 실패:', error.message);
            throw error;
        }
    }

    async findByType(alarmType, tenantId = null) {
        try {
            const rules = await this.query('ar')
                .where('ar.alarm_type', alarmType)
                .where('ar.tenant_id', tenantId || 1)
                .orderBy('ar.created_at', 'DESC');

            return rules.map(rule => this.parseAlarmRule(rule));
        } catch (error) {
            console.error('AlarmRuleRepository.findByType 실패:', error.message);
            throw error;
        }
    }

    async findBySeverity(severity, tenantId = null) {
        try {
            const rules = await this.query('ar')
                .where('ar.severity', severity)
                .where('ar.tenant_id', tenantId || 1)
                .orderBy('ar.created_at', 'DESC');

            return rules.map(rule => this.parseAlarmRule(rule));

        } catch (error) {
            console.error('AlarmRuleRepository.findBySeverity 실패:', error.message);
            throw error;
        }
    }

    async search(searchTerm, tenantId = null, limit = 50) {
        try {
            const searchParam = `%${searchTerm}%`;
            let query = this.query('ar')
                .where('ar.tenant_id', tenantId || 1)
                .andWhere(function () {
                    this.where('ar.name', 'like', searchParam)
                        .orWhere('ar.description', 'like', searchParam)
                        .orWhere('ar.category', 'like', searchParam)
                        .orWhere('ar.alarm_type', 'like', searchParam)
                        .orWhere('ar.severity', 'like', searchParam)
                        .orWhere('ar.target_type', 'like', searchParam)
                        .orWhere('ar.target_group', 'like', searchParam)
                        .orWhere('ar.message_template', 'like', searchParam)
                        .orWhere('ar.tags', 'like', searchParam);
                })
                .orderBy('ar.created_at', 'DESC');

            if (limit) {
                query = query.limit(limit);
            }

            const rules = await query;

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
            console.log('AlarmRuleRepository.create 호출 (Knex):', ruleData.name);

            const dataToInsert = {
                tenant_id: ruleData.tenant_id,
                name: ruleData.name,
                description: ruleData.description || null,
                target_type: ruleData.target_type,
                target_id: ruleData.target_id,
                target_group: ruleData.target_group || null,
                alarm_type: ruleData.alarm_type,
                high_high_limit: ruleData.high_high_limit || null,
                high_limit: ruleData.high_limit || null,
                low_limit: ruleData.low_limit || null,
                low_low_limit: ruleData.low_low_limit || null,
                deadband: ruleData.deadband || 0,
                rate_of_change: ruleData.rate_of_change || 0,
                trigger_condition: ruleData.trigger_condition || null,
                condition_script: ruleData.condition_script || null,
                message_script: ruleData.message_script || null,
                message_config: typeof ruleData.message_config === 'object' ? JSON.stringify(ruleData.message_config) : (ruleData.message_config || null),
                message_template: ruleData.message_template || null,
                severity: ruleData.severity || 'medium',
                priority: ruleData.priority || 100,
                auto_acknowledge: ruleData.auto_acknowledge ? 1 : 0,
                acknowledge_timeout_min: ruleData.acknowledge_timeout_min || 0,
                auto_clear: ruleData.auto_clear ? 1 : 0,
                suppression_rules: typeof ruleData.suppression_rules === 'object' ? JSON.stringify(ruleData.suppression_rules) : (ruleData.suppression_rules || null),
                notification_enabled: ruleData.notification_enabled ? 1 : 0,
                notification_delay_sec: ruleData.notification_delay_sec || 0,
                notification_repeat_interval_min: ruleData.notification_repeat_interval_min || 0,
                notification_channels: typeof ruleData.notification_channels === 'object' ? JSON.stringify(ruleData.notification_channels) : (ruleData.notification_channels || null),
                notification_recipients: typeof ruleData.notification_recipients === 'object' ? JSON.stringify(ruleData.notification_recipients) : (ruleData.notification_recipients || null),
                is_enabled: ruleData.is_enabled !== false ? 1 : 0,
                is_latched: ruleData.is_latched ? 1 : 0,
                created_by: userId || ruleData.created_by,
                template_id: ruleData.template_id || null,
                rule_group: ruleData.rule_group || null,
                created_by_template: ruleData.created_by_template ? 1 : 0,
                escalation_enabled: ruleData.escalation_enabled ? 1 : 0,
                escalation_max_level: ruleData.escalation_max_level || 3,
                escalation_rules: typeof ruleData.escalation_rules === 'object' ? JSON.stringify(ruleData.escalation_rules) : (ruleData.escalation_rules || null),
                category: ruleData.category || null,
                tags: typeof ruleData.tags === 'object' ? JSON.stringify(ruleData.tags) : (ruleData.tags || '[]')
            };

            const [id] = await this.query().insert(dataToInsert);

            if (id) {
                console.log(`✅ 알람 규칙 생성 완료 (ID: ${id})`);
                return await this.findById(id, ruleData.tenant_id);
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
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const fields = [
                'name', 'description', 'target_type', 'target_id', 'target_group',
                'alarm_type', 'high_high_limit', 'high_limit', 'low_limit', 'low_low_limit',
                'deadband', 'rate_of_change', 'trigger_condition', 'condition_script',
                'message_script', 'message_config', 'message_template', 'severity', 'priority',
                'auto_acknowledge', 'acknowledge_timeout_min', 'auto_clear', 'suppression_rules',
                'notification_enabled', 'notification_delay_sec', 'notification_repeat_interval_min',
                'notification_channels', 'notification_recipients', 'is_enabled', 'is_latched',
                'template_id', 'rule_group', 'created_by_template',
                'last_template_update', 'escalation_enabled', 'escalation_max_level', 'escalation_rules',
                'category', 'tags'
            ];

            fields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if (['message_config', 'suppression_rules', 'notification_channels', 'notification_recipients', 'escalation_rules', 'tags'].includes(field) && typeof updateData[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(updateData[field]);
                    } else if (['is_enabled', 'is_latched', 'auto_acknowledge', 'auto_clear', 'notification_enabled', 'created_by_template', 'escalation_enabled'].includes(field)) {
                        dataToUpdate[field] = updateData[field] ? 1 : 0;
                    } else {
                        dataToUpdate[field] = updateData[field];
                    }
                }
            });

            let query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);

            if (affected > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 업데이트 완료`);
                return await this.findById(id, tenantId);
            }
            return null;
        } catch (error) {
            console.error('AlarmRuleRepository.update 실패:', error.message);
            throw error;
        }
    }

    async updateEnabledStatus(id, isEnabled, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.updateEnabledStatus 호출 (Knex): ID ${id}, isEnabled=${isEnabled}`);
            let query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update({
                is_enabled: isEnabled ? 1 : 0,
                updated_at: this.knex.fn.now()
            });

            if (affected > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 상태 업데이트 완료`);
                return { id: parseInt(id), is_enabled: isEnabled };
            }
            throw new Error(`Alarm rule with ID ${id} not found`);
        } catch (error) {
            console.error('AlarmRuleRepository.updateEnabledStatus 실패:', error.message);
            throw error;
        }
    }

    async updateSettings(id, settings, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.updateSettings 호출 (Knex): ID ${id}`);

            const dataToUpdate = {
                message_config: typeof settings.message_config === 'object' ? JSON.stringify(settings.message_config) : (settings.message_config || null),
                suppression_rules: typeof settings.suppression_rules === 'object' ? JSON.stringify(settings.suppression_rules) : (settings.suppression_rules || null),
                notification_channels: typeof settings.notification_channels === 'object' ? JSON.stringify(settings.notification_channels) : (settings.notification_channels || null),
                notification_recipients: typeof settings.notification_recipients === 'object' ? JSON.stringify(settings.notification_recipients) : (settings.notification_recipients || null),
                escalation_rules: typeof settings.escalation_rules === 'object' ? JSON.stringify(settings.escalation_rules) : (settings.escalation_rules || null),
                updated_at: this.knex.fn.now()
            };

            let query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);

            if (affected > 0) {
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
            console.log(`AlarmRuleRepository.updateName 호출 (Knex): ID ${id}, name=${name}`);

            let query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update({
                name: name,
                updated_at: this.knex.fn.now()
            });

            if (affected > 0) {
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
            console.log(`AlarmRuleRepository.updateSeverity 호출 (Knex): ID ${id}, severity=${severity}`);

            let query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update({
                severity: severity,
                updated_at: this.knex.fn.now()
            });

            if (affected > 0) {
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
            console.log(`AlarmRuleRepository.delete (Soft Delete) 호출 (Knex): ID ${id}`);
            let query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update({
                is_deleted: 1,
                updated_at: this.knex.fn.now()
            });

            if (affected > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 삭제(Soft Delete) 완료`);
                return true;
            } else {
                return false;
            }
        } catch (error) {
            console.error('AlarmRuleRepository.delete 실패:', error.message);
            throw error;
        }
    }

    async restore(id, tenantId = null) {
        try {
            console.log(`AlarmRuleRepository.restore 호출 (Knex): ID ${id}`);
            let query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update({
                is_deleted: 0,
                updated_at: this.knex.fn.now()
            });

            if (affected > 0) {
                console.log(`✅ 알람 규칙 ID ${id} 복원 완료`);
                return true;
            } else {
                return false;
            }
        } catch (error) {
            console.error('AlarmRuleRepository.restore 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 통계 및 집계 연산
    // ==========================================================================

    async getStatsSummary(tenantId = null) {
        try {
            const query = this.query()
                .select(this.knex.raw('COUNT(*) as total_rules'))
                .select(this.knex.raw('SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_rules'))
                .select(this.knex.raw('COUNT(DISTINCT alarm_type) as alarm_types'))
                .select(this.knex.raw('COUNT(DISTINCT severity) as severity_levels'))
                .select(this.knex.raw('COUNT(DISTINCT target_type) as target_types'))
                .select(this.knex.raw('COUNT(DISTINCT category) as categories'))
                .select(this.knex.raw("COUNT(CASE WHEN tags IS NOT NULL AND tags != '[]' THEN 1 END) as rules_with_tags"));

            if (tenantId) query.where('tenant_id', tenantId);

            const stats = await query.first();
            return stats || {
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
            const query = this.query()
                .select('severity')
                .count('* as count')
                .select(this.knex.raw('SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count'))
                .groupBy('severity');

            if (tenantId) query.where('tenant_id', tenantId);

            return await query;
        } catch (error) {
            console.error('AlarmRuleRepository.getStatsBySeverity 실패:', error.message);
            throw error;
        }
    }

    async getStatsByType(tenantId = null) {
        try {
            const query = this.query()
                .select('alarm_type')
                .count('* as count')
                .select(this.knex.raw('SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count'))
                .groupBy('alarm_type');

            if (tenantId) query.where('tenant_id', tenantId);

            return await query;
        } catch (error) {
            console.error('AlarmRuleRepository.getStatsByType 실패:', error.message);
            throw error;
        }
    }

    async getStatsByCategory(tenantId = null) {
        try {
            const query = this.query()
                .select('category')
                .count('* as count')
                .select(this.knex.raw('SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count'))
                .whereNotNull('category')
                .groupBy('category');

            if (tenantId) query.where('tenant_id', tenantId);

            return await query;
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
            template_version: rule.template_version ? parseInt(rule.template_version) : 1,
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
            is_deleted: Boolean(rule.is_deleted),

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