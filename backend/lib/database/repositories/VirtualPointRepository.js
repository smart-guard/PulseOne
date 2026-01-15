// =============================================================================
// backend/lib/database/repositories/VirtualPointRepository.js
// 가상포인트 리포지토리 - Knex 및 BaseRepository 패턴 적용
// =============================================================================

const BaseRepository = require('./BaseRepository');

class VirtualPointRepository extends BaseRepository {
    constructor() {
        super('virtual_points');
    }

    // ==========================================================================
    // 조회 메소드들
    // ==========================================================================

    /**
     * ID로 가상포인트 상세 조회 (관련 데이터 포함)
     */
    async findById(id, tenantId = null, trx = null) {
        try {
            const query = (trx || this.knex)('virtual_points').where('id', id);

            if (tenantId) {
                query.where('tenant_id', tenantId);
            }

            const vp = await query.first();
            if (!vp) return null;

            const virtualPoint = this.parseVirtualPoint(vp);

            // 관련 데이터 조회
            virtualPoint.inputs = await this.getInputsByVirtualPoint(id, trx);
            virtualPoint.currentValue = await this.getCurrentValue(id, trx);
            virtualPoint.dependencies = await this.getDependencies(id, trx);

            return virtualPoint;
        } catch (error) {
            this.logger.error(`VirtualPointRepository.findById 실패 (ID: ${id}):`, error);
            throw error;
        }
    }

    /**
     * 이름으로 가상포인트 조회
     */
    async findByName(name, tenantId = null, trx = null) {
        try {
            const query = (trx || this.knex)('virtual_points').where('name', name);
            if (tenantId) query.where('tenant_id', tenantId);

            const vp = await query.first();
            return vp ? this.parseVirtualPoint(vp) : null;
        } catch (error) {
            this.logger.error(`VirtualPointRepository.findByName 실패 (Name: ${name}):`, error);
            throw error;
        }
    }

    /**
     * 조건부 목록 조회 (필터 및 페이징)
     */
    async findAllVirtualPoints(filters = {}) {
        try {
            const query = this.query();
            const tenantId = filters.tenantId || filters.tenant_id || 1;

            query.where('tenant_id', tenantId);

            if (filters.siteId || filters.site_id) {
                query.where('site_id', filters.siteId || filters.site_id);
            }

            if (filters.deviceId || filters.device_id) {
                query.where('device_id', filters.deviceId || filters.device_id);
            }

            if (filters.scopeType || filters.scope_type) {
                query.where('scope_type', filters.scopeType || filters.scope_type);
            }

            if (filters.is_enabled !== undefined) {
                query.where('is_enabled', filters.is_enabled ? 1 : 0);
            }

            if (filters.category) {
                query.where('category', filters.category);
            }

            if (filters.calculation_trigger) {
                query.where('calculation_trigger', filters.calculation_trigger);
            }

            if (filters.search) {
                const search = `%${filters.search}%`;
                query.andWhere(function () {
                    this.where('name', 'like', search)
                        .orWhere('description', 'like', search);
                });
            }

            // Pagination
            const limit = parseInt(filters.limit) || 25;
            const page = parseInt(filters.page) || 1;
            const offset = (page - 1) * limit;

            // Total count
            const countQuery = query.clone().clearSelect().clearOrder().count('id as total').first();
            const countResult = await countQuery;
            const totalCount = countResult ? countResult.total : 0;

            // Execution
            const items = await query.orderBy('created_at', 'desc').limit(limit).offset(offset);
            const parsedItems = items.map(vp => this.parseVirtualPoint(vp));

            return {
                items: parsedItems,
                pagination: {
                    page,
                    limit,
                    total_items: totalCount,
                    has_next: page * limit < totalCount,
                    has_prev: page > 1
                }
            };
        } catch (error) {
            this.logger.error('VirtualPointRepository.findAllVirtualPoints 실패:', error);
            throw error;
        }
    }

    // ==========================================================================
    // CRUD 메소드들
    // ==========================================================================

    /**
     * 가상포인트 생성 (관련 테이블 포함)
     */
    async createVirtualPoint(virtualPointData, inputs = [], tenantId = null) {
        return await this.transaction(async (trx) => {
            // 1. 메인 가상포인트 생성
            const dataToInsert = {
                tenant_id: tenantId || virtualPointData.tenant_id || 1,
                name: virtualPointData.name,
                formula: virtualPointData.formula || virtualPointData.expression || 'return 0;',
                description: virtualPointData.description || null,
                data_type: virtualPointData.data_type || 'float',
                unit: virtualPointData.unit || null,
                calculation_trigger: virtualPointData.calculation_trigger || 'timer',
                is_enabled: virtualPointData.is_enabled !== false ? 1 : 0,
                category: virtualPointData.category || 'calculation',
                scope_type: virtualPointData.scope_type || 'system',
                site_id: virtualPointData.site_id || null,
                device_id: virtualPointData.device_id || null,
                tags: virtualPointData.tags ? (typeof virtualPointData.tags === 'string' ? virtualPointData.tags : JSON.stringify(virtualPointData.tags)) : null,
                created_at: this.knex.fn.now(),
                updated_at: this.knex.fn.now()
            };

            const [virtualPointId] = await trx('virtual_points').insert(dataToInsert);

            // 2. 초기값 생성
            await trx('virtual_point_values').insert({
                virtual_point_id: virtualPointId,
                value: null,
                quality: 'initialization',
                last_calculated: this.knex.fn.now(),
                calculation_duration_ms: 0,
                is_stale: 1
            });

            // 3. 입력 매핑 생성
            if (inputs && inputs.length > 0) {
                const inputsToInsert = inputs.map(input => ({
                    virtual_point_id: virtualPointId,
                    variable_name: input.variable_name,
                    source_type: input.source_type || 'constant',
                    source_id: input.source_id || null,
                    constant_value: input.constant_value !== undefined ? input.constant_value : null,
                    source_formula: input.source_formula || null,
                    is_required: input.is_required !== false ? 1 : 0,
                    sort_order: input.sort_order || 0
                }));
                await trx('virtual_point_inputs').insert(inputsToInsert);
            } else {
                // 기본 입력
                await trx('virtual_point_inputs').insert({
                    virtual_point_id: virtualPointId,
                    variable_name: 'defaultInput',
                    source_type: 'constant',
                    constant_value: 0,
                    is_required: 1,
                    sort_order: 0
                });
            }

            // 4. 초기 실행 이력
            await trx('virtual_point_execution_history').insert({
                virtual_point_id: virtualPointId,
                execution_time: this.knex.fn.now(),
                execution_duration_ms: 0,
                result_type: 'success',
                result_value: JSON.stringify({ action: 'created', status: 'initialized' }),
                trigger_source: 'system',
                success: 1
            });

            return await this.findById(virtualPointId, tenantId, trx);
        });
    }

    /**
     * 가상포인트 업데이트
     */
    async updateVirtualPoint(id, virtualPointData, inputs = null, tenantId = null) {
        return await this.transaction(async (trx) => {
            // 1. 메인 정보 업데이트
            const dataToUpdate = {
                name: virtualPointData.name,
                formula: virtualPointData.formula || virtualPointData.expression,
                description: virtualPointData.description,
                data_type: virtualPointData.data_type,
                unit: virtualPointData.unit,
                calculation_trigger: virtualPointData.calculation_trigger,
                is_enabled: virtualPointData.is_enabled ? 1 : 0,
                category: virtualPointData.category,
                scope_type: virtualPointData.scope_type,
                site_id: virtualPointData.site_id,
                device_id: virtualPointData.device_id,
                tags: virtualPointData.tags ? (typeof virtualPointData.tags === 'string' ? virtualPointData.tags : JSON.stringify(virtualPointData.tags)) : undefined,
                updated_at: this.knex.fn.now()
            };

            // undefined 필드 제거
            Object.keys(dataToUpdate).forEach(key => dataToUpdate[key] === undefined && delete dataToUpdate[key]);

            const query = trx('virtual_points').where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const updatedRows = await query.update(dataToUpdate);
            if (updatedRows === 0) {
                throw new Error(`가상포인트 ID ${id}를 업데이트할 수 없습니다 (존재하지 않거나 권한 없음)`);
            }

            // 2. 입력 매핑 업데이트
            if (inputs !== null) {
                await trx('virtual_point_inputs').where('virtual_point_id', id).delete();

                if (inputs.length > 0) {
                    const inputsToInsert = inputs.map(input => ({
                        virtual_point_id: id,
                        variable_name: input.variable_name,
                        source_type: input.source_type || 'constant',
                        source_id: input.source_id || null,
                        constant_value: input.constant_value !== undefined ? input.constant_value : null,
                        source_formula: input.source_formula || null,
                        is_required: input.is_required !== false ? 1 : 0,
                        sort_order: input.sort_order || 0
                    }));
                    await trx('virtual_point_inputs').insert(inputsToInsert);
                }
            }

            // 3. 현재값 무효화
            await trx('virtual_point_values')
                .where('virtual_point_id', id)
                .update({
                    quality: 'pending_update',
                    is_stale: 1,
                    last_calculated: this.knex.fn.now()
                });

            // 4. 업데이트 이력
            await trx('virtual_point_execution_history').insert({
                virtual_point_id: id,
                execution_time: this.knex.fn.now(),
                execution_duration_ms: 0,
                result_type: 'success',
                result_value: JSON.stringify({ action: 'updated', status: 'completed' }),
                trigger_source: 'manual',
                success: 1
            });

            return await this.findById(id, tenantId, trx);
        });
    }

    /**
     * 상태 토글 전용
     */
    async updateEnabledStatus(id, isEnabled, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            await query.update({
                is_enabled: isEnabled ? 1 : 0,
                updated_at: this.knex.fn.now()
            });

            return await this.findById(id, tenantId);
        } catch (error) {
            this.logger.error(`VirtualPointRepository.updateEnabledStatus 실패 (ID: ${id}):`, error);
            throw error;
        }
    }

    /**
     * 삭제 (관련 데이터 모두 삭제)
     */
    async deleteById(id, tenantId = null) {
        return await this.transaction(async (trx) => {
            const query = trx('virtual_points').where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const existing = await query.first();
            if (!existing) return false;

            // 1. 실행 이력
            await trx('virtual_point_execution_history').where('virtual_point_id', id).delete();
            // 2. 의존성
            await trx('virtual_point_dependencies').where('virtual_point_id', id).delete();
            // 3. 현재값
            await trx('virtual_point_values').where('virtual_point_id', id).delete();
            // 4. 입력 매핑
            await trx('virtual_point_inputs').where('virtual_point_id', id).delete();

            // 5. 알람 발생에서 참조 제거
            await trx('alarm_occurrences')
                .where('point_id', id)
                .whereIn('rule_id', function () {
                    this.select('id').from('alarm_rules').where('target_type', 'virtual_point');
                })
                .update({ point_id: null });

            // 6. 알람 룰 삭제
            await trx('alarm_rules')
                .where('target_type', 'virtual_point')
                .where('target_id', id)
                .delete();

            // 7. 다른 VP 참조 제거
            await trx('virtual_point_inputs')
                .where('source_type', 'virtual_point')
                .where('source_id', id)
                .delete();

            await trx('virtual_point_dependencies')
                .where('depends_on_type', 'virtual_point')
                .where('depends_on_id', id)
                .delete();

            // 8. 본체 삭제
            await trx('virtual_points').where('id', id).delete();

            return true;
        });
    }

    // ==========================================================================
    // 관련 데이터 조회
    // ==========================================================================

    async getInputsByVirtualPoint(virtualPointId, trx = null) {
        return (trx || this.knex)('virtual_point_inputs')
            .where('virtual_point_id', virtualPointId)
            .orderBy('sort_order', 'asc')
            .orderBy('id', 'asc');
    }

    async getCurrentValue(virtualPointId, trx = null) {
        return (trx || this.knex)('virtual_point_values')
            .where('virtual_point_id', virtualPointId)
            .first();
    }

    async getDependencies(virtualPointId, trx = null) {
        return (trx || this.knex)('virtual_point_dependencies')
            .where('virtual_point_id', virtualPointId)
            .where('is_active', 1)
            .orderBy('dependency_level', 'asc');
    }

    // ==========================================================================
    // 통계 및 정리
    // ==========================================================================

    async getStatsByCategory(tenantId) {
        return this.query()
            .where('tenant_id', tenantId)
            .select('category')
            .count('* as count')
            .sum('is_enabled as enabled_count')
            .groupBy('category')
            .orderBy('count', 'desc');
    }

    async getPerformanceStats(tenantId) {
        const result = await this.query()
            .where('tenant_id', tenantId)
            .count('* as total_points')
            .sum('is_enabled as enabled_points')
            .first();

        return result || { total_points: 0, enabled_points: 0 };
    }

    async cleanupOrphanedRecords() {
        return await this.transaction(async (trx) => {
            const vpSubquery = trx('virtual_points').select('id');

            const results = [];

            const deletedInputs = await trx('virtual_point_inputs').whereNotIn('virtual_point_id', vpSubquery).delete();
            results.push({ table: 'virtual_point_inputs', cleaned: deletedInputs });

            const deletedValues = await trx('virtual_point_values').whereNotIn('virtual_point_id', vpSubquery).delete();
            results.push({ table: 'virtual_point_values', cleaned: deletedValues });

            const deletedDeps = await trx('virtual_point_dependencies').whereNotIn('virtual_point_id', vpSubquery).delete();
            results.push({ table: 'virtual_point_dependencies', cleaned: deletedDeps });

            const deletedHistory = await trx('virtual_point_execution_history').whereNotIn('virtual_point_id', vpSubquery).delete();
            results.push({ table: 'virtual_point_execution_history', cleaned: deletedHistory });

            return results;
        });
    }

    // ==========================================================================
    // 헬퍼 메소드들
    // ==========================================================================

    parseVirtualPoint(vp) {
        if (!vp) return null;

        return {
            ...vp,
            is_enabled: !!vp.is_enabled,
            tags: vp.tags ? (typeof vp.tags === 'string' ? JSON.parse(vp.tags) : vp.tags) : []
        };
    }
}

module.exports = VirtualPointRepository;