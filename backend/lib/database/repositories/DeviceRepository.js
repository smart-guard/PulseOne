const BaseRepository = require('./BaseRepository');
const DeviceQueries = require('../queries/DeviceQueries');

/**
 * DeviceRepository class
 * Handles database operations for devices using Knex and BaseRepository.
 */
class DeviceRepository extends BaseRepository {
    constructor() {
        super('devices');
    }

    // =============================================================================
    // 디바이스 조회 메소드들 (Read)
    // =============================================================================

    /**
     * 모든 디바이스를 조회합니다.
     */
    async findAll(tenantId = null, options = {}) {
        try {
            const query = this.query()
                .leftJoin('protocols as p', 'p.id', 'devices.protocol_id')
                .leftJoin('sites as s', 's.id', 'devices.site_id')
                .leftJoin('device_status as dst', 'dst.device_id', 'devices.id')
                .select(
                    'devices.*',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    's.name as site_name',
                    'dst.connection_status',
                    'dst.last_communication',
                    'dst.error_count'
                );

            if (tenantId) {
                query.where('devices.tenant_id', tenantId);
            }

            if (options.siteId) {
                query.where('devices.site_id', options.siteId);
            }

            // Pagination defaults
            const page = parseInt(options.page) || 1;
            const limit = parseInt(options.limit) || 1000;
            const offset = (page - 1) * limit;

            const items = await query.limit(limit).offset(offset);

            // Total count for pagination if requested
            let total = 0;
            if (options.includeCount) {
                const countQuery = this.query();
                if (tenantId) countQuery.where('tenant_id', tenantId);
                const countResult = await countQuery.count('id as total').first();
                total = countResult ? (countResult.total || 0) : 0;
            }

            const parsedItems = items.map(device => this.parseDevice(device));

            return options.includeCount ? { items: parsedItems, total, page, limit } : parsedItems;
        } catch (error) {
            this.logger.error('DeviceRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스를 조회합니다.
     */
    async findById(id, tenantId = null, trx = null) {
        try {
            const query = (trx || this.knex)('devices as d')
                .leftJoin('protocols as p', 'p.id', 'd.protocol_id')
                .leftJoin('sites as s', 's.id', 'd.site_id')
                .leftJoin('device_status as dst', 'dst.device_id', 'd.id')
                .select(
                    'd.*',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    's.name as site_name',
                    'dst.connection_status',
                    'dst.last_communication',
                    'dst.error_count'
                )
                .where('d.id', id);

            if (tenantId) {
                query.where('d.tenant_id', tenantId);
            }

            const device = await query.first();
            return device ? this.parseDevice(device) : null;
        } catch (error) {
            this.logger.error('DeviceRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 이름으로 디바이스를 조회합니다.
     */
    async findByName(name, tenantId = null) {
        try {
            const query = this.query().where('name', name);
            if (tenantId) query.where('tenant_id', tenantId);
            const device = await query.first();
            return device ? this.parseDevice(device) : null;
        } catch (error) {
            this.logger.error('DeviceRepository.findByName 오류:', error);
            throw error;
        }
    }

    // =============================================================================
    // CRUD 메소드들 (Create, Update, Delete)
    // =============================================================================

    /**
     * 새로운 디바이스를 생성합니다.
     */
    async create(deviceData, tenantId = null) {
        try {
            const dataToInsert = {
                tenant_id: tenantId || deviceData.tenant_id,
                site_id: deviceData.site_id || 1,
                device_group_id: deviceData.device_group_id || null,
                edge_server_id: deviceData.edge_server_id || null,
                name: deviceData.name,
                description: deviceData.description || null,
                device_type: deviceData.device_type || 'PLC',
                manufacturer: deviceData.manufacturer || null,
                model: deviceData.model || null,
                serial_number: deviceData.serial_number || null,
                protocol_id: deviceData.protocol_id,
                endpoint: deviceData.endpoint,
                config: typeof deviceData.config === 'object' ? JSON.stringify(deviceData.config) : (deviceData.config || '{}'),
                polling_interval: deviceData.polling_interval || 1000,
                timeout: deviceData.timeout || 3000,
                retry_count: deviceData.retry_count || 3,
                is_enabled: deviceData.is_enabled !== false ? 1 : 0,
                installation_date: deviceData.installation_date || null
            };

            return await this.knex.transaction(async (trx) => {
                const results = await trx('devices').insert(dataToInsert);
                const id = results[0];

                // Create initial device status
                await trx('device_status').insert({
                    device_id: id,
                    connection_status: 'disconnected',
                    error_count: 0
                });

                return await this.findById(id, tenantId, trx);
            });
        } catch (error) {
            this.logger.error('DeviceRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 디바이스 정보를 업데이트합니다.
     */
    async update(id, updateData, tenantId = null) {
        try {
            const allowedFields = [
                'name', 'description', 'device_type', 'manufacturer', 'model', 'serial_number',
                'protocol_id', 'endpoint', 'config', 'polling_interval', 'timeout',
                'retry_count', 'is_enabled', 'installation_date'
            ];

            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if (field === 'config' && typeof updateData[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(updateData[field]);
                    } else if (field === 'is_enabled') {
                        dataToUpdate[field] = updateData[field] ? 1 : 0;
                    } else {
                        dataToUpdate[field] = updateData[field];
                    }
                }
            });

            let query = this.query().where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);

            if (affected > 0) {
                return await this.findById(id, tenantId);
            }
            return null;
        } catch (error) {
            this.logger.error('DeviceRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스를 삭제합니다.
     */
    async deleteById(id, tenantId = null) {
        try {
            return await this.knex.transaction(async (trx) => {
                let query = trx('devices').where('id', id);
                if (tenantId) query = query.where('tenant_id', tenantId);

                // Delete related records first (if not handled by DB cascades)
                await trx('device_status').where('device_id', id).del();

                const affected = await query.del();
                return affected > 0;
            });
        } catch (error) {
            this.logger.error('DeviceRepository.deleteById 오류:', error);
            throw error;
        }
    }

    // =============================================================================
    // Helper Methods
    // =============================================================================

    /**
     * DB 결과를 응답용 객체로 파싱합니다.
     */
    parseDevice(device) {
        if (!device) return null;

        const result = { ...device };

        // Parse config JSON if it's a string
        if (typeof device.config === 'string' && device.config.length > 0) {
            try {
                result.config = JSON.parse(device.config);
            } catch (e) {
                this.logger.warn(`Device ${device.id}: Config parsing failed:`, e.message);
                result.config = {};
            }
        } else if (!device.config) {
            result.config = {};
        }

        // Standardize status object
        result.status = {
            connection: device.connection_status || 'unknown',
            last_seen: device.last_communication,
            error_count: device.error_count || 0,
            is_connected: device.connection_status === 'connected'
        };

        // Standardize RTU info if applicable
        if (device.protocol_type === 'MODBUS_RTU') {
            result.rtu_info = {
                slave_id: result.config?.slave_id || null,
                master_device_id: result.config?.master_device_id || null,
                serial_port: device.endpoint
            };
        }

        return result;
    }
}

module.exports = DeviceRepository;