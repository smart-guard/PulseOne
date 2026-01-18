// ============================================================================
// backend/lib/database/repositories/AlarmTemplateRepository.js
// BaseRepository 상속받은 AlarmTemplateRepository - 기존 패턴 100% 준수
// ============================================================================

const BaseRepository = require('./BaseRepository');
const AlarmQueries = require('../queries/AlarmQueries');

/**
 * 알람 템플릿 Repository 클래스 (기존 AlarmRuleRepository 패턴 완전 준수)
 * BaseRepository를 상속받아 공통 기능 활용
 * 
 * 주요 특징:
 * - AlarmQueries.AlarmTemplate 쿼리 활용
 * - 기존 Repository들과 동일한 메서드 구조
 * - 캐싱 및 에러 처리 일관성 유지
 * - 템플릿 적용 횟수 추적 기능
 */
class AlarmTemplateRepository extends BaseRepository {
    constructor() {
        super('alarm_rule_templates');

        // 알람 템플릿 필드 정의 (실제 테이블 스키마와 일치)
        this.fields = {
            id: 'autoIncrement',
            tenant_id: 'int',
            name: 'varchar(100)',
            description: 'text',
            category: 'varchar(50)',
            condition_type: 'varchar(50)',
            condition_template: 'text',
            default_config: 'text',                    // JSON
            severity: 'varchar(20)',
            message_template: 'text',
            applicable_data_types: 'text',             // JSON 배열
            applicable_device_types: 'text',           // JSON 배열
            notification_enabled: 'boolean',
            email_notification: 'boolean',
            sms_notification: 'boolean',
            auto_acknowledge: 'boolean',
            auto_clear: 'boolean',
            usage_count: 'int',
            is_active: 'boolean',
            is_system_template: 'boolean',
            created_by: 'int',
            created_at: 'timestamp',
            updated_at: 'timestamp'
        };

        console.log('AlarmTemplateRepository 초기화 완료 (패턴 준수)');
    }

    // ========================================================================
    // BaseRepository 필수 메서드 구현 (기존 패턴 준수)
    // ========================================================================

    /**
     * 모든 알람 템플릿 조회 (페이징, 필터링 지원)
     */
    async findAll(filters = {}) {
        try {
            const query = this.query()
                .where('tenant_id', filters.tenant_id || 1);

            if (filters.category) {
                query.where('category', filters.category);
            }

            if (filters.is_system_template !== undefined) {
                query.where('is_system_template', filters.is_system_template ? 1 : 0);
            }

            if (filters.search) {
                query.where(builder => {
                    builder.where('name', 'like', `%${filters.search}%`)
                        .orWhere('description', 'like', `%${filters.search}%`);
                });
            }

            // 페이징 처리 전 클론하여 카운트
            const page = parseInt(filters.page) || 1;
            const limit = parseInt(filters.limit) || 50;
            const offset = (page - 1) * limit;

            const [items, countResult] = await Promise.all([
                query.clone().orderBy('created_at', 'desc').limit(limit).offset(offset),
                query.clone().count('* as total').first()
            ]);

            const total = countResult ? countResult.total : 0;

            return {
                items: (items || []).map(template => this.formatAlarmTemplate(template)),
                pagination: {
                    page,
                    limit,
                    total,
                    totalPages: Math.ceil(total / limit)
                }
            };

        } catch (error) {
            this.logger?.error('AlarmTemplateRepository.findAll failed:', error);
            throw error;
        }
    }

    /**
     * ID로 알람 템플릿 조회
     */
    async findById(id, tenantId = null) {
        try {
            const item = await this.query()
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .first();

            return item ? this.formatAlarmTemplate(item) : null;
        } catch (error) {
            this.logger?.error('AlarmTemplateRepository.findById failed:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 생성
     */
    async create(templateData, tenantId = null) {
        try {
            const data = {
                ...templateData,
                tenant_id: tenantId || templateData.tenant_id || 1,
                created_at: this.knex.fn.now(),
                updated_at: this.knex.fn.now()
            };

            // 필수 필드 검증 (유지)
            AlarmQueries.validateTemplateRequiredFields(data);
            AlarmQueries.validateTemplateConfig(data);

            // JSON 필드 처리
            if (data.default_config && typeof data.default_config !== 'string') data.default_config = JSON.stringify(data.default_config);
            if (data.applicable_data_types && typeof data.applicable_data_types !== 'string') data.applicable_data_types = JSON.stringify(data.applicable_data_types);
            if (data.applicable_device_types && typeof data.applicable_device_types !== 'string') data.applicable_device_types = JSON.stringify(data.applicable_device_types);

            const [result] = await this.knex(this.tableName).insert(data).returning('id');
            const newId = result?.id || result;

            if (newId) {
                this.logger?.info(`Alarm template created - ID: ${newId}`);
                return await this.findById(newId, data.tenant_id);
            } else {
                throw new Error('Alarm template creation failed - no ID returned');
            }

        } catch (error) {
            this.logger?.error('AlarmTemplateRepository.create failed:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            const data = { ...updateData };

            // 필수 필드 검증 (유지 - 설정 누락 방지)
            AlarmQueries.validateTemplateConfig(data);

            // JSON 필드 처리
            if (data.default_config && typeof data.default_config !== 'string') data.default_config = JSON.stringify(data.default_config);
            if (data.applicable_data_types && typeof data.applicable_data_types !== 'string') data.applicable_data_types = JSON.stringify(data.applicable_data_types);
            if (data.applicable_device_types && typeof data.applicable_device_types !== 'string') data.applicable_device_types = JSON.stringify(data.applicable_device_types);

            data.updated_at = this.knex.fn.now();

            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .update(data);

            if (result > 0) {
                return await this.findById(id, tenantId);
            } else {
                return null;
            }

        } catch (error) {
            this.logger?.error('AlarmTemplateRepository.update failed:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 삭제 (소프트 삭제)
     */
    async delete(id, tenantId = null) {
        try {
            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .update({
                    is_active: false,
                    updated_at: this.knex.fn.now()
                });

            return result > 0;

        } catch (error) {
            this.logger?.error('AlarmTemplateRepository.delete failed:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 완전 삭제 (하드 삭제)
     */
    async hardDelete(id, tenantId = null) {
        try {
            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .del();

            return result > 0;

        } catch (error) {
            this.logger?.error('AlarmTemplateRepository.hardDelete failed:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 존재 확인
     */
    async exists(id, tenantId = null) {
        try {
            const result = await this.knex(this.tableName)
                .where('id', id)
                .where('tenant_id', tenantId || 1)
                .select(1)
                .first();
            return !!result;

        } catch (error) {
            this.logger?.error(`AlarmTemplateRepository.exists(${id}) failed:`, error);
            return false;
        }
    }

    // ========================================================================
    // 알람 템플릿 특화 메서드들
    // ========================================================================

    /**
     * 카테고리별 템플릿 조회
     */
    async findByCategory(category, tenantId = null) {
        try {
            const items = await this.query()
                .where('category', category)
                .where('tenant_id', tenantId || 1)
                .orderBy('name', 'asc');

            return (items || []).map(item => this.formatAlarmTemplate(item));

        } catch (error) {
            this.logger?.error(`findByCategory(${category}) failed:`, error);
            throw error;
        }
    }

    /**
     * 시스템 템플릿만 조회
     */
    async findSystemTemplates() {
        try {
            const items = await this.query()
                .where('is_system_template', 1)
                .orderBy('name', 'asc');

            return (items || []).map(item => this.formatAlarmTemplate(item));

        } catch (error) {
            this.logger?.error('findSystemTemplates failed:', error);
            throw error;
        }
    }

    /**
     * 사용자 템플릿만 조회
     */
    async findUserTemplates(tenantId = null) {
        try {
            const items = await this.query()
                .where('tenant_id', tenantId || 1)
                .where('is_system_template', 0)
                .orderBy('name', 'asc');

            return (items || []).map(item => this.formatAlarmTemplate(item));

        } catch (error) {
            this.logger?.error('findUserTemplates failed:', error);
            throw error;
        }
    }

    /**
     * 데이터 타입별 템플릿 조회
     */
    async findByDataType(dataType, tenantId = null) {
        try {
            const items = await this.query()
                .where('tenant_id', tenantId || 1)
                .where('applicable_data_types', 'like', `%"${dataType}"%`)
                .orderBy('name', 'asc');

            return (items || []).map(item => this.formatAlarmTemplate(item));

        } catch (error) {
            this.logger?.error(`findByDataType(${dataType}) failed:`, error);
            throw error;
        }
    }

    /**
     * 템플릿 사용량 증가
     */
    async incrementUsage(id, incrementBy = 1) {
        try {
            const result = await this.knex(this.tableName)
                .where('id', id)
                .increment('usage_count', incrementBy);

            return result > 0;

        } catch (error) {
            this.logger?.error(`incrementUsage(${id}) failed:`, error);
            throw error;
        }
    }

    /**
     * 가장 많이 사용된 템플릿들 조회
     */
    async findMostUsed(tenantId = null, limit = 10) {
        try {
            const items = await this.query()
                .where('tenant_id', tenantId || 1)
                .orderBy('usage_count', 'desc')
                .limit(limit);

            return (items || []).map(item => this.formatAlarmTemplate(item));

        } catch (error) {
            this.logger?.error('findMostUsed failed:', error);
            throw error;
        }
    }

    /**
     * 템플릿으로 생성된 규칙들 조회
     */
    async findAppliedRules(templateId, tenantId = null) {
        try {
            const items = await this.knex('alarm_rules')
                .where('template_id', templateId)
                .where('tenant_id', tenantId || 1)
                .orderBy('created_at', 'desc');

            this.logger?.info(`Found ${items.length} rules applied from template ${templateId}`);
            return items;

        } catch (error) {
            this.logger?.error(`findAppliedRules(${templateId}) failed:`, error);
            throw error;
        }
    }

    /**
     * 템플릿 적용 횟수 조회
     */
    async getAppliedRulesCount(templateId, tenantId = null) {
        try {
            const result = await this.knex('alarm_rules')
                .where('template_id', templateId)
                .where('tenant_id', tenantId || 1)
                .count('* as count')
                .first();

            return result ? result.count : 0;

        } catch (error) {
            this.logger?.error(`getAppliedRulesCount(${templateId}) failed:`, error);
            return 0;
        }
    }

    /**
     * 검색 기능
     */
    async search(searchTerm, tenantId = 1, limit = 50) {
        try {
            const items = await this.query()
                .where('tenant_id', tenantId || 1)
                .where(builder => {
                    builder.where('name', 'like', `%${searchTerm}%`)
                        .orWhere('description', 'like', `%${searchTerm}%`)
                        .orWhere('category', 'like', `%${searchTerm}%`);
                })
                .limit(limit)
                .orderBy('name', 'asc');

            return items.map(template => this.formatAlarmTemplate(template));

        } catch (error) {
            this.logger?.error(`search("${searchTerm}") failed:`, error);
            throw error;
        }
    }

    /**
     * 통계 조회
     */
    async getStatistics(tenantId = 1) {
        try {
            const result = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .select([
                    this.knex.raw('COUNT(*) as total_templates'),
                    this.knex.raw('COUNT(DISTINCT category) as categories'),
                    this.knex.raw('SUM(usage_count) as total_usage'),
                    this.knex.raw('AVG(usage_count) as avg_usage'),
                    this.knex.raw('MAX(usage_count) as max_usage')
                ])
                .first();

            const stats = result ? {
                total_templates: parseInt(result.total_templates) || 0,
                categories: parseInt(result.categories) || 0,
                total_usage: parseInt(result.total_usage) || 0,
                avg_usage: parseFloat(result.avg_usage) || 0,
                max_usage: parseInt(result.max_usage) || 0
            } : {
                total_templates: 0,
                categories: 0,
                total_usage: 0,
                avg_usage: 0,
                max_usage: 0
            };

            // 카테고리별 분포 추가
            const categoryResults = await this.knex(this.tableName)
                .where('tenant_id', tenantId || 1)
                .select('category')
                .count('* as count')
                .sum('usage_count as total_usage')
                .groupBy('category');

            stats.category_distribution = (categoryResults || []).reduce((acc, row) => {
                acc[row.category] = {
                    count: parseInt(row.count) || 0,
                    total_usage: parseInt(row.total_usage) || 0
                };
                return acc;
            }, {});

            return stats;

        } catch (error) {
            this.logger?.error('getStatistics failed:', error);
            return {
                total_templates: 0,
                categories: 0,
                total_usage: 0,
                avg_usage: 0,
                max_usage: 0,
                category_distribution: {}
            };
        }
    }

    // ========================================================================
    // 헬퍼 메서드들 (기존 패턴 준수)
    // ========================================================================

    /**
     * 알람 템플릿 데이터 포맷팅 (JSON 필드 파싱 포함)
     */
    formatAlarmTemplate(template) {
        if (!template) return null;

        try {
            return {
                ...template,
                is_active: !!template.is_active,
                is_system_template: !!template.is_system_template,
                notification_enabled: !!template.notification_enabled,
                email_notification: !!template.email_notification,
                sms_notification: !!template.sms_notification,
                auto_acknowledge: !!template.auto_acknowledge,
                auto_clear: !!template.auto_clear,
                default_config: template.default_config ? JSON.parse(template.default_config) : {},
                applicable_data_types: template.applicable_data_types ? JSON.parse(template.applicable_data_types) : ['*'],
                applicable_device_types: template.applicable_device_types ? JSON.parse(template.applicable_device_types) : ['*'],
                created_at: template.created_at,
                updated_at: template.updated_at
            };
        } catch (error) {
            console.warn(`JSON 파싱 실패 for template ${template.id}:`, error);
            return template;
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

        if (cached) {
            this.cache.delete(key);
        }
        return null;
    }

    /**
     * 캐시에 데이터 저장
     */
    setCache(key, data, ttl = null) {
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
            console.log('AlarmTemplateRepository 캐시 초기화 완료');
        }
    }
}

module.exports = AlarmTemplateRepository;