const BaseRepository = require('./BaseRepository');

class ProtocolRepository extends BaseRepository {
    constructor() {
        super('protocols');
    }

    // ==========================================================================
    // 기본 CRUD 연산 (Knex 사용)
    // ==========================================================================

    async findAll(filters = {}) {
        try {
            const query = this.query('p')
                .select('p.*')
                .count('d.id as device_count')
                .count({ enabled_count: this.knex.raw('CASE WHEN d.is_enabled = 1 THEN 1 END') })
                .count({ connected_count: this.knex.raw("CASE WHEN ds.connection_status = 'connected' THEN 1 END") })
                .leftJoin('devices as d', 'd.protocol_id', 'p.id')
                .leftJoin('device_status as ds', 'ds.device_id', 'd.id')
                .groupBy('p.id');

            // 테넌트 필터 (디바이스 통계용)
            if (filters.tenantId) {
                // 이 부분은 join된 devices에 대한 필터여야 함
                // 하지만 protocol 목록은 다 나와야 하므로 join 조건에 넣는 것이 좋음
                query.leftJoin('devices as d_stat', function () {
                    this.on('d_stat.protocol_id', '=', 'p.id')
                        .andOn('d_stat.tenant_id', '=', filters.tenantId);
                });
                // 위에서 이미 devices join이 있으니 이를 수정
                // query 초기화 다시 함
            }

            // --- 다시 작성 (통계 쿼리 포함) ---
            const subquery = this.knex('devices as d')
                .select('d.protocol_id')
                .count('* as device_count')
                .count({ enabled_count: this.knex.raw('CASE WHEN d.is_enabled = 1 THEN 1 END') })
                .count({ connected_count: this.knex.raw("CASE WHEN ds.connection_status = 'connected' THEN 1 END") })
                .leftJoin('device_status as ds', 'ds.device_id', 'd.id');

            if (filters.tenantId) {
                subquery.where('d.tenant_id', filters.tenantId);
            }
            subquery.groupBy('d.protocol_id').as('stats');

            const mainQuery = this.query('p')
                .select('p.*')
                .select({
                    device_count: this.knex.raw('COALESCE(stats.device_count, 0)'),
                    enabled_count: this.knex.raw('COALESCE(stats.enabled_count, 0)'),
                    connected_count: this.knex.raw('COALESCE(stats.connected_count, 0)')
                })
                .leftJoin(subquery, 'stats.protocol_id', 'p.id');

            // 필터 적용
            if (filters.category && filters.category !== 'all') {
                mainQuery.where('p.category', filters.category);
            }

            if (filters.enabled !== undefined) {
                mainQuery.where('p.is_enabled', filters.enabled === 'true' ? 1 : 0);
            }

            if (filters.deprecated !== undefined) {
                mainQuery.where('p.is_deprecated', filters.deprecated === 'true' ? 1 : 0);
            }

            if (filters.uses_serial !== undefined) {
                mainQuery.where('p.uses_serial', filters.uses_serial === 'true' ? 1 : 0);
            }

            if (filters.requires_broker !== undefined) {
                mainQuery.where('p.requires_broker', filters.requires_broker === 'true' ? 1 : 0);
            }

            if (filters.search) {
                mainQuery.where(function () {
                    this.where('p.display_name', 'like', `%${filters.search}%`)
                        .orWhere('p.protocol_type', 'like', `%${filters.search}%`)
                        .orWhere('p.description', 'like', `%${filters.search}%`);
                });
            }

            // 정렬 및 페이징
            const sortBy = filters.sortBy || 'display_name';
            const sortOrder = filters.sortOrder || 'ASC';
            mainQuery.orderBy(`p.${sortBy}`, sortOrder);

            if (filters.limit) {
                mainQuery.limit(parseInt(filters.limit));
                if (filters.offset) {
                    mainQuery.offset(parseInt(filters.offset));
                }
            }

            const protocols = await mainQuery;
            return protocols.map(p => this.parseProtocol(p));

        } catch (error) {
            this.logger.error('ProtocolRepository.findAll 실패:', error);
            throw error;
        }
    }

    async findById(id, tenantId = null) {
        try {
            // findAll의 로직을 재사용하여 통계 포함 조회
            const protocols = await this.findAll({ tenantId, search: null, limit: 1, offset: 0, id });
            // findAll은 복잡하니, 간단하게 findById용 쿼리 따로 작성

            const subquery = this.knex('devices as d')
                .select('d.protocol_id')
                .count('* as device_count')
                .count({ enabled_count: this.knex.raw('CASE WHEN d.is_enabled = 1 THEN 1 END') })
                .count({ connected_count: this.knex.raw("CASE WHEN ds.connection_status = 'connected' THEN 1 END") })
                .leftJoin('device_status as ds', 'ds.device_id', 'd.id');

            if (tenantId) {
                subquery.where('d.tenant_id', tenantId);
            }
            subquery.groupBy('d.protocol_id').as('stats');

            const protocol = await this.query('p')
                .select('p.*')
                .select({
                    device_count: this.knex.raw('COALESCE(stats.device_count, 0)'),
                    enabled_count: this.knex.raw('COALESCE(stats.enabled_count, 0)'),
                    connected_count: this.knex.raw('COALESCE(stats.connected_count, 0)')
                })
                .leftJoin(subquery, 'stats.protocol_id', 'p.id')
                .where('p.id', id)
                .first();

            return protocol ? this.parseProtocol(protocol) : null;
        } catch (error) {
            this.logger.error('ProtocolRepository.findById 실패:', error);
            throw error;
        }
    }

    async findByType(protocolType) {
        try {
            const protocol = await this.query().where('protocol_type', protocolType).first();
            return protocol ? this.parseProtocol(protocol) : null;
        } catch (error) {
            this.logger.error('ProtocolRepository.findByType 실패:', error);
            throw error;
        }
    }

    async findByCategory(category) {
        try {
            const protocols = await this.query().where('category', category).orderBy('display_name', 'asc');
            return protocols.map(p => this.parseProtocol(p));
        } catch (error) {
            this.logger.error('ProtocolRepository.findByCategory 실패:', error);
            throw error;
        }
    }

    async findActive() {
        try {
            const protocols = await this.query().where('is_enabled', 1).orderBy('display_name', 'asc');
            return protocols.map(p => this.parseProtocol(p));
        } catch (error) {
            this.logger.error('ProtocolRepository.findActive 실패:', error);
            throw error;
        }
    }

    // ==========================================================================
    // 생성, 수정, 삭제 연산
    // ==========================================================================

    async create(protocolData) {
        try {
            const insertData = {
                protocol_type: protocolData.protocol_type,
                display_name: protocolData.display_name,
                description: protocolData.description || null,
                default_port: protocolData.default_port || null,
                uses_serial: protocolData.uses_serial ? 1 : 0,
                requires_broker: protocolData.requires_broker ? 1 : 0,
                supported_operations: JSON.stringify(protocolData.supported_operations || []),
                supported_data_types: JSON.stringify(protocolData.supported_data_types || []),
                connection_params: JSON.stringify(protocolData.connection_params || {}),
                capabilities: JSON.stringify(protocolData.capabilities || {}),
                default_polling_interval: protocolData.default_polling_interval || 1000,
                default_timeout: protocolData.default_timeout || 5000,
                max_concurrent_connections: protocolData.max_concurrent_connections || 1,
                category: protocolData.category || 'custom',
                vendor: protocolData.vendor || protocolData.manufacturer || null,
                standard_reference: protocolData.standard_reference || protocolData.specification || null,
                is_enabled: protocolData.is_enabled !== false ? 1 : 0,
                is_deprecated: 0,
                min_firmware_version: protocolData.min_firmware_version || null
            };

            const [id] = await this.query().insert(insertData);
            return await this.findById(id);
        } catch (error) {
            this.logger.error('ProtocolRepository.create 실패:', error);
            throw error;
        }
    }

    async update(id, updateData) {
        try {
            const allowedFields = [
                'display_name', 'description', 'default_port', 'default_polling_interval',
                'default_timeout', 'max_concurrent_connections', 'category', 'vendor',
                'standard_reference', 'is_enabled', 'is_deprecated', 'min_firmware_version',
                'uses_serial', 'requires_broker', 'supported_operations', 'supported_data_types',
                'connection_params', 'capabilities'
            ];

            const updateFields = {};
            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if (field === 'is_enabled' || field === 'is_deprecated' || field === 'uses_serial' || field === 'requires_broker') {
                        updateFields[field] = updateData[field] ? 1 : 0;
                    } else if (['supported_operations', 'supported_data_types', 'connection_params', 'capabilities'].includes(field)) {
                        updateFields[field] = JSON.stringify(updateData[field] || (field === 'connection_params' || field === 'capabilities' ? {} : []));
                    } else {
                        updateFields[field] = updateData[field];
                    }
                }
            });

            // manufacturer mapping to vendor if vendor missing
            if (updateData.manufacturer && updateData.vendor === undefined) {
                updateFields.vendor = updateData.manufacturer;
            }
            if (updateData.specification && updateData.standard_reference === undefined) {
                updateFields.standard_reference = updateData.specification;
            }

            if (Object.keys(updateFields).length > 0) {
                await this.query().update(updateFields).where('id', id);
            }
            return await this.findById(id);
        } catch (error) {
            this.logger.error('ProtocolRepository.update 실패:', error);
            throw error;
        }
    }

    async delete(id, force = false) {
        try {
            const deviceCountResult = await this.knex('devices').where('protocol_id', id).count('* as count').first();
            const deviceCount = deviceCountResult ? deviceCountResult.count : 0;

            if (deviceCount > 0 && !force) {
                throw new Error(`Cannot delete protocol. ${deviceCount} devices are using this protocol.`);
            }

            return await this.knex.transaction(async trx => {
                if (force && deviceCount > 0) {
                    // 기본 프로토콜(가정: id=1, MODBUS_TCP)로 변경하거나, null로 설정
                    await trx('devices').update({ protocol_id: 1 }).where('protocol_id', id);
                }
                const deleted = await trx('protocols').where('id', id).del();
                return deleted > 0;
            });
        } catch (error) {
            this.logger.error('ProtocolRepository.delete 실패:', error);
            throw error;
        }
    }

    // ==========================================================================
    // 제어 연산
    // ==========================================================================

    async enable(id) {
        return await this.update(id, { is_enabled: true });
    }

    async disable(id) {
        return await this.update(id, { is_enabled: false });
    }

    // ==========================================================================
    // 통계 및 분석
    // ==========================================================================

    async getStatsByCategory() {
        try {
            const totalCountResult = await this.query().count('* as count').first();
            const totalCount = totalCountResult ? totalCountResult.count : 1;

            const stats = await this.query()
                .select('category')
                .count('* as count')
                .groupBy('category');

            return stats.map(s => ({
                category: s.category,
                count: parseInt(s.count) || 0,
                percentage: parseFloat((s.count * 100.0 / totalCount).toFixed(2))
            }));
        } catch (error) {
            this.logger.error('ProtocolRepository.getStatsByCategory 실패:', error);
            throw error;
        }
    }

    async getUsageStats(tenantId = null) {
        try {
            const subquery = this.knex('devices as d')
                .select('d.protocol_id')
                .count('* as device_count')
                .count({ enabled_devices: this.knex.raw('CASE WHEN d.is_enabled = 1 THEN 1 END') })
                .count({ connected_devices: this.knex.raw("CASE WHEN ds.connection_status = 'connected' THEN 1 END") })
                .leftJoin('device_status as ds', 'ds.device_id', 'd.id');

            if (tenantId) {
                subquery.where('d.tenant_id', tenantId);
            }
            subquery.groupBy('d.protocol_id').as('stats');

            const protocols = await this.query('p')
                .select('p.id', 'p.protocol_type', 'p.display_name', 'p.category')
                .select({
                    device_count: this.knex.raw('COALESCE(stats.device_count, 0)'),
                    enabled_devices: this.knex.raw('COALESCE(stats.enabled_devices, 0)'),
                    connected_devices: this.knex.raw('COALESCE(stats.connected_devices, 0)')
                })
                .leftJoin(subquery, 'stats.protocol_id', 'p.id')
                .orderBy('device_count', 'desc');

            return protocols.map(p => ({
                ...p,
                device_count: parseInt(p.device_count),
                enabled_devices: parseInt(p.enabled_devices),
                connected_devices: parseInt(p.connected_devices)
            }));
        } catch (error) {
            this.logger.error('ProtocolRepository.getUsageStats 실패:', error);
            throw error;
        }
    }

    async getTopUsedProtocols(limit = 10) {
        try {
            const stats = await this.query('p')
                .select('p.protocol_type', 'p.display_name')
                .count('d.id as device_count')
                .join('devices as d', 'd.protocol_id', 'p.id')
                .where('p.is_enabled', 1)
                .groupBy('p.protocol_type', 'p.display_name')
                .orderBy('device_count', 'desc')
                .limit(limit);

            return stats.map(s => ({
                protocol_type: s.protocol_type,
                display_name: s.display_name,
                device_count: parseInt(s.device_count)
            }));
        } catch (error) {
            this.logger.error('ProtocolRepository.getTopUsedProtocols 실패:', error);
            throw error;
        }
    }

    async getCounts() {
        try {
            const total = await this.query().count('* as count').first();
            const enabled = await this.query().where('is_enabled', 1).count('* as count').first();
            const deprecated = await this.query().where('is_deprecated', 1).count('* as count').first();

            return {
                total_protocols: total ? total.count : 0,
                enabled_protocols: enabled ? enabled.count : 0,
                deprecated_protocols: deprecated ? deprecated.count : 0
            };
        } catch (error) {
            this.logger.error('ProtocolRepository.getCounts 실패:', error);
            throw error;
        }
    }

    async getTotalCount(filters = {}) {
        try {
            const query = this.query();

            if (filters.category) query.where('category', filters.category);
            if (filters.enabled !== undefined) query.where('is_enabled', filters.enabled === 'true' ? 1 : 0);
            if (filters.deprecated !== undefined) query.where('is_deprecated', filters.deprecated === 'true' ? 1 : 0);
            if (filters.uses_serial !== undefined) query.where('uses_serial', filters.uses_serial === 'true' ? 1 : 0);
            if (filters.requires_broker !== undefined) query.where('requires_broker', filters.requires_broker === 'true' ? 1 : 0);

            if (filters.search) {
                query.where(function () {
                    this.where('protocol_type', 'like', `%${filters.search}%`)
                        .orWhere('display_name', 'like', `%${filters.search}%`)
                        .orWhere('description', 'like', `%${filters.search}%`);
                });
            }

            const result = await query.count('* as count').first();
            return result ? result.count : 0;
        } catch (error) {
            this.logger.error('ProtocolRepository.getTotalCount 실패:', error);
            throw error;
        }
    }

    async checkProtocolTypeExists(protocolType, excludeId = null) {
        try {
            const query = this.query().where('protocol_type', protocolType);
            if (excludeId) query.whereNot('id', excludeId);
            const count = await query.count('* as count').first();
            return count && count.count > 0;
        } catch (error) {
            this.logger.error('ProtocolRepository.checkProtocolTypeExists 실패:', error);
            throw error;
        }
    }

    async getDevicesByProtocol(protocolId, limit = 50, offset = 0) {
        try {
            const devices = await this.knex('devices as d')
                .select('d.id', 'd.name', 'd.device_type', 'd.is_enabled',
                    'ds.connection_status', 'ds.last_communication')
                .leftJoin('device_status as ds', 'ds.device_id', 'd.id')
                .where('d.protocol_id', protocolId)
                .orderBy('d.name')
                .limit(limit)
                .offset(offset);

            return devices.map(d => ({
                ...d,
                is_enabled: Boolean(d.is_enabled)
            }));
        } catch (error) {
            this.logger.error('ProtocolRepository.getDevicesByProtocol 실패:', error);
            throw error;
        }
    }

    parseProtocol(protocol) {
        if (!protocol) return null;

        return {
            ...protocol,
            supported_operations: this.parseJsonField(protocol.supported_operations),
            supported_data_types: this.parseJsonField(protocol.supported_data_types),
            connection_params: this.parseJsonField(protocol.connection_params),
            capabilities: this.parseJsonField(protocol.capabilities),
            is_enabled: Boolean(protocol.is_enabled),
            is_deprecated: Boolean(protocol.is_deprecated),
            uses_serial: Boolean(protocol.uses_serial),
            requires_broker: Boolean(protocol.requires_broker),
            device_count: parseInt(protocol.device_count) || 0,
            enabled_count: parseInt(protocol.enabled_count) || 0,
            connected_count: parseInt(protocol.connected_count) || 0
        };
    }

    parseJsonField(jsonStr) {
        if (!jsonStr) return null;
        try {
            return typeof jsonStr === 'string' ? JSON.parse(jsonStr) : jsonStr;
        } catch (error) {
            return null;
        }
    }
}

module.exports = ProtocolRepository;