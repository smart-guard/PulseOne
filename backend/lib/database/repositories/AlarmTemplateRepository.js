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
            let query = AlarmQueries.AlarmTemplate.FIND_ALL;
            const params = [filters.tenant_id || 1];

            // 동적 필터 추가
            if (filters.category) {
                query = query.replace('ORDER BY', 'AND category = ? ORDER BY');
                params.push(filters.category);
            }

            if (filters.is_system_template !== undefined) {
                query = query.replace('ORDER BY', 'AND is_system_template = ? ORDER BY');
                params.push(filters.is_system_template ? 1 : 0);
            }

            if (filters.search) {
                query = query.replace('ORDER BY', 'AND (name LIKE ? OR description LIKE ?) ORDER BY');
                params.push(`%${filters.search}%`, `%${filters.search}%`);
            }

            // 페이징 추가
            if (filters.limit) {
                query += ' LIMIT ? OFFSET ?';
                params.push(parseInt(filters.limit), (parseInt(filters.page) - 1) * parseInt(filters.limit) || 0);
            }

            const results = await this.executeQuery(query, params);
            const items = results || [];
            
            return {
                items: items.map(template => this.formatAlarmTemplate(template)),
                pagination: {
                    page: parseInt(filters.page) || 1,
                    limit: parseInt(filters.limit) || 50,
                    total: items.length,
                    totalPages: Math.ceil(items.length / (parseInt(filters.limit) || 50))
                }
            };

        } catch (error) {
            console.error('AlarmTemplateRepository.findAll 실패:', error);
            throw error;
        }
    }

    /**
     * ID로 알람 템플릿 조회
     */
    async findById(id, tenantId = null) {
        try {
            const cacheKey = `alarmTemplate_${id}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmTemplate.FIND_BY_ID;
            const results = await this.executeQuery(query, [id, tenantId || 1]);
            
            const template = results && results.length > 0 ? this.formatAlarmTemplate(results[0]) : null;
            
            if (template) {
                this.setCache(cacheKey, template);
            }

            return template;
        } catch (error) {
            console.error('AlarmTemplateRepository.findById 실패:', error);
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
                tenant_id: tenantId || templateData.tenant_id || 1
            };

            // 필수 필드 검증
            AlarmQueries.validateTemplateRequiredFields(data);
            AlarmQueries.validateTemplateConfig(data);

            const query = AlarmQueries.AlarmTemplate.CREATE;
            const params = AlarmQueries.buildCreateTemplateParams(data);

            console.log(`INSERT 쿼리 실행 - 컬럼: 19개, 값: ${params.length}개`);

            const result = await this.executeNonQuery(query, params);
            
            let newId = null;
            if (result) {
                newId = result.lastInsertRowid || result.insertId || result.lastID;
            }
            
            if (newId) {
                console.log(`알람 템플릿 생성 성공 - ID: ${newId}`);
                this.clearCache();
                return await this.findById(newId);
            } else {
                // 대안: 최근 생성된 템플릿 조회
                const recentQuery = `
                    SELECT * FROM alarm_rule_templates 
                    WHERE tenant_id = ? AND name = ? 
                    ORDER BY created_at DESC, id DESC 
                    LIMIT 1
                `;
                const recent = await this.executeQuerySingle(recentQuery, [data.tenant_id, data.name]);
                
                if (recent) {
                    console.log(`최근 생성된 템플릿 발견 - ID: ${recent.id}`);
                    return this.formatAlarmTemplate(recent);
                } else {
                    throw new Error('알람 템플릿 생성 실패 - 생성된 템플릿을 찾을 수 없음');
                }
            }

        } catch (error) {
            console.error('AlarmTemplateRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            // 필수 필드 검증
            AlarmQueries.validateTemplateRequiredFields(updateData);
            AlarmQueries.validateTemplateConfig(updateData);

            const query = AlarmQueries.AlarmTemplate.UPDATE;
            const params = AlarmQueries.buildUpdateTemplateParams(updateData, id, tenantId);

            const result = await this.executeNonQuery(query, params);
            
            if (result && result.changes > 0) {
                this.clearCache();
                return await this.findById(id, tenantId);
            } else {
                return null;
            }

        } catch (error) {
            console.error('AlarmTemplateRepository.update 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 삭제 (소프트 삭제)
     */
    async delete(id, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmTemplate.DELETE;
            const result = await this.executeNonQuery(query, [id, tenantId || 1]);
            
            if (result && result.changes > 0) {
                this.clearCache();
                return true;
            }
            return false;

        } catch (error) {
            console.error('AlarmTemplateRepository.delete 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 완전 삭제 (하드 삭제)
     */
    async hardDelete(id, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmTemplate.HARD_DELETE;
            const result = await this.executeNonQuery(query, [id, tenantId || 1]);
            
            if (result && result.changes > 0) {
                this.clearCache();
                return true;
            }
            return false;

        } catch (error) {
            console.error('AlarmTemplateRepository.hardDelete 실패:', error);
            throw error;
        }
    }

    /**
     * 알람 템플릿 존재 확인
     */
    async exists(id, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmTemplate.EXISTS;
            const result = await this.executeQuerySingle(query, [id, tenantId || 1]);
            return !!result;

        } catch (error) {
            console.error(`AlarmTemplateRepository.exists(${id}) 실패:`, error);
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
            const cacheKey = `templatesByCategory_${category}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmTemplate.FIND_BY_CATEGORY;
            const results = await this.executeQuery(query, [category, tenantId || 1]);
            
            const templates = results.map(template => this.formatAlarmTemplate(template));
            this.setCache(cacheKey, templates);
            
            console.log(`카테고리 ${category} 템플릿 ${templates.length}개 조회 완료`);
            return templates;

        } catch (error) {
            console.error(`findByCategory(${category}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 시스템 템플릿만 조회
     */
    async findSystemTemplates() {
        try {
            const cacheKey = 'systemTemplates';
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmTemplate.FIND_SYSTEM_TEMPLATES;
            const results = await this.executeQuery(query, []);
            
            const templates = results.map(template => this.formatAlarmTemplate(template));
            this.setCache(cacheKey, templates, 300000); // 5분 캐시
            
            return templates;

        } catch (error) {
            console.error('findSystemTemplates 실패:', error);
            throw error;
        }
    }

    /**
     * 사용자 템플릿만 조회
     */
    async findUserTemplates(tenantId = null) {
        try {
            const query = AlarmQueries.AlarmTemplate.FIND_USER_TEMPLATES;
            const results = await this.executeQuery(query, [tenantId || 1]);
            
            return results.map(template => this.formatAlarmTemplate(template));

        } catch (error) {
            console.error('findUserTemplates 실패:', error);
            throw error;
        }
    }

    /**
     * 데이터 타입별 템플릿 조회
     */
    async findByDataType(dataType, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmTemplate.FIND_BY_DATA_TYPE;
            const searchPattern = `%"${dataType}"%`;
            const results = await this.executeQuery(query, [tenantId || 1, searchPattern]);
            
            return results.map(template => this.formatAlarmTemplate(template));

        } catch (error) {
            console.error(`findByDataType(${dataType}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 템플릿 사용량 증가
     */
    async incrementUsage(id, incrementBy = 1) {
        try {
            const query = AlarmQueries.AlarmTemplate.INCREMENT_USAGE;
            const result = await this.executeNonQuery(query, [incrementBy, id]);
            
            if (result && result.changes > 0) {
                this.clearCache();
                console.log(`템플릿 ID ${id} 사용량 +${incrementBy} 증가`);
                return true;
            }
            return false;

        } catch (error) {
            console.error(`incrementUsage(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 가장 많이 사용된 템플릿들 조회
     */
    async findMostUsed(tenantId = null, limit = 10) {
        try {
            const query = AlarmQueries.AlarmTemplate.MOST_USED;
            const results = await this.executeQuery(query, [tenantId || 1, limit]);
            
            return results.map(template => this.formatAlarmTemplate(template));

        } catch (error) {
            console.error('findMostUsed 실패:', error);
            throw error;
        }
    }

    /**
     * 템플릿으로 생성된 규칙들 조회
     */
    async findAppliedRules(templateId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmTemplate.FIND_APPLIED_RULES;
            const results = await this.executeQuery(query, [templateId, tenantId || 1]);
            
            console.log(`템플릿 ID ${templateId}로 생성된 규칙 ${results.length}개 조회`);
            return results;

        } catch (error) {
            console.error(`findAppliedRules(${templateId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 템플릿 적용 횟수 조회
     */
    async getAppliedRulesCount(templateId, tenantId = null) {
        try {
            const query = AlarmQueries.AlarmTemplate.APPLIED_RULES_COUNT;
            const result = await this.executeQuerySingle(query, [templateId, tenantId || 1]);
            
            return result ? result.applied_count : 0;

        } catch (error) {
            console.error(`getAppliedRulesCount(${templateId}) 실패:`, error);
            return 0;
        }
    }

    /**
     * 검색 기능
     */
    async search(searchTerm, tenantId = 1, limit = 50) {
        try {
            const query = AlarmQueries.AlarmTemplate.SEARCH;
            const searchPattern = `%${searchTerm}%`;
            const params = [tenantId, searchPattern, searchPattern, searchPattern];
            
            if (limit) {
                const queryWithLimit = query + ' LIMIT ?';
                params.push(limit);
            }
            
            const results = await this.executeQuery(query + (limit ? ' LIMIT ?' : ''), params);
            return results.map(template => this.formatAlarmTemplate(template));

        } catch (error) {
            console.error(`search("${searchTerm}") 실패:`, error);
            throw error;
        }
    }

    /**
     * 통계 조회
     */
    async getStatistics(tenantId = 1) {
        try {
            const cacheKey = `templateStats_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            const query = AlarmQueries.AlarmTemplate.STATS_SUMMARY;
            const result = await this.executeQuerySingle(query, [tenantId]);
            
            const stats = result || {
                total_templates: 0,
                categories: 0,
                total_usage: 0,
                avg_usage: 0,
                max_usage: 0
            };

            // 카테고리별 분포 추가
            const categoryQuery = AlarmQueries.AlarmTemplate.COUNT_BY_CATEGORY;
            const categoryResults = await this.executeQuery(categoryQuery, [tenantId]);

            stats.category_distribution = categoryResults.reduce((acc, row) => {
                acc[row.category] = {
                    count: row.count,
                    total_usage: row.total_usage
                };
                return acc;
            }, {});

            this.setCache(cacheKey, stats, 60000); // 1분 캐시
            return stats;

        } catch (error) {
            console.error('getStatistics 실패:', error);
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