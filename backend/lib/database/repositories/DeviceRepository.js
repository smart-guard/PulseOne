const BaseRepository = require('./BaseRepository');
const DeviceQueries = require('../queries/DeviceQueries');
const redis = require('../../connection/redis');

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
            const query = this.query('d')
                .leftJoin('protocols as p', 'p.id', 'd.protocol_id')
                .leftJoin('device_groups as dg', 'dg.id', 'd.device_group_id')
                .leftJoin('sites as s', 's.id', 'd.site_id')
                .leftJoin('device_status as dst', 'dst.device_id', 'd.id')
                .leftJoin('data_points as dp', 'dp.device_id', 'd.id')
                .leftJoin('template_devices as td', 'td.id', 'd.template_device_id')
                .select(
                    'd.*',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    'dg.name as device_group_name',
                    's.name as site_name',
                    'td.name as template_name',
                    'dst.connection_status',
                    'dst.last_communication',
                    'dst.error_count',
                    this.knex.raw('COUNT(DISTINCT dp.id) as data_point_count'),
                    this.knex.raw('SUM(CASE WHEN dp.is_enabled = 1 THEN 1 ELSE 0 END) as enabled_point_count'),
                    this.knex.raw('(SELECT GROUP_CONCAT(dga.group_id || ":" || dg_inner.name) FROM device_group_assignments dga JOIN device_groups dg_inner ON dg_inner.id = dga.group_id WHERE dga.device_id = d.id) as assigned_groups')
                )
                .groupBy('d.id');

            // Apply common filters
            this._applyFilters(query, tenantId, options);

            // Pagination & Sorting
            const page = parseInt(options.page) || 1;
            const limit = parseInt(options.limit) || 1000;
            const offset = (page - 1) * limit;
            const sortBy = options.sort_by || 'name';
            const sortOrder = (options.sort_order || 'ASC').toUpperCase();

            // Field mapping for sorting
            const sortFieldMap = {
                'name': 'd.name',
                'protocol_type': 'p.protocol_type',
                'status': 'dst.connection_status',
                'connection_status': 'dst.connection_status',
                'created_at': 'd.created_at'
            };

            const sortCol = sortFieldMap[sortBy] || 'd.name';
            query.orderBy(sortCol, sortOrder);

            const items = await query.limit(limit).offset(offset);

            // Total count for pagination if requested
            let total = 0;
            if (options.includeCount) {
                const countQuery = this.query('d');
                this._applyFilters(countQuery, tenantId, options);

                const countResult = await countQuery.count('d.id as total').first();
                total = countResult ? (countResult.total || 0) : 0;
            }

            const parsedItems = items.map(device => this.parseDevice(device));

            // Return standardized structure for frontend
            if (options.includeCount) {
                const totalPages = Math.ceil(total / limit);
                return {
                    items: parsedItems,
                    pagination: {
                        page,
                        limit,
                        total,
                        totalPages,
                        hasNext: page < totalPages,
                        hasPrev: page > 1
                    }
                };
            }

            return parsedItems;
        } catch (error) {
            this.logger.error('DeviceRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스를 조회합니다.
     */
    async findById(id, tenantId = null, trx = null, options = {}) {
        try {
            const query = (trx || this.knex)('devices as d')
                .leftJoin('protocols as p', 'p.id', 'd.protocol_id')
                .leftJoin('device_groups as dg', 'dg.id', 'd.device_group_id')
                .leftJoin('sites as s', 's.id', 'd.site_id')
                .leftJoin('device_status as dst', 'dst.device_id', 'd.id')
                .leftJoin('template_devices as td', 'td.id', 'd.template_device_id')
                .select(
                    'd.*',
                    'p.protocol_type',
                    'p.display_name as protocol_name',
                    'dg.name as device_group_name',
                    's.name as site_name',
                    'td.name as template_name',
                    'dst.connection_status',
                    'dst.last_communication',
                    'dst.error_count'
                )
                .where('d.id', id);

            if (tenantId) {
                query.where('d.tenant_id', tenantId);
            }

            // [Soft Delete] 
            const includeDeleted = options.includeDeleted === true || options.includeDeleted === 'true';
            if (!includeDeleted) {
                query.where(builder => {
                    builder.where('d.is_deleted', 0).orWhereNull('d.is_deleted');
                });
            }

            const device = await query.first();
            if (!device) return null;

            // Fetch associated groups
            const groups = await (trx || this.knex)('device_group_assignments as dga')
                .join('device_groups as dg', 'dg.id', 'dga.group_id')
                .where('dga.device_id', id)
                .select('dg.id', 'dg.name', 'dga.is_primary');

            // Map groups to the device object
            device.assigned_groups = groups.map(g => `${g.id}:${g.name}`).join(',');

            // Fetch device settings
            const settings = await (trx || this.knex)('device_settings')
                .where('device_id', id)
                .first();

            if (settings) {
                device.settings = settings;
            }

            return this.parseDevice(device);
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
                installation_date: deviceData.installation_date || null,
                metadata: typeof deviceData.metadata === 'object' ? JSON.stringify(deviceData.metadata) : (deviceData.metadata || '{}'),
                custom_fields: typeof deviceData.custom_fields === 'object' ? JSON.stringify(deviceData.custom_fields) : (deviceData.custom_fields || '{}')
            };

            if (deviceData.device_group_id !== undefined) dataToInsert.device_group_id = deviceData.device_group_id;
            if (deviceData.edge_server_id !== undefined) dataToInsert.edge_server_id = deviceData.edge_server_id;
            if (deviceData.tags !== undefined) dataToInsert.tags = typeof deviceData.tags === 'object' ? JSON.stringify(deviceData.tags) : deviceData.tags;

            return await this.knex.transaction(async (trx) => {
                const results = await trx('devices').insert(dataToInsert);
                const id = results[0];

                // Create initial device status
                await trx('device_status').insert({
                    device_id: id,
                    connection_status: 'disconnected',
                    error_count: 0
                });

                // Create initial device settings
                const allowedSettingsFields = [
                    'polling_interval_ms', 'scan_rate_override', 'scan_group',
                    'connection_timeout_ms', 'read_timeout_ms', 'write_timeout_ms',
                    'inter_frame_delay_ms', 'max_retry_count', 'retry_interval_ms',
                    'backoff_multiplier', 'backoff_time_ms', 'max_backoff_time_ms',
                    'is_keep_alive_enabled', 'keep_alive_interval_s', 'keep_alive_timeout_s',
                    'is_data_validation_enabled', 'is_outlier_detection_enabled', 'is_deadband_enabled',
                    'is_detailed_logging_enabled', 'is_performance_monitoring_enabled',
                    'is_diagnostic_mode_enabled', 'is_communication_logging_enabled',
                    'read_buffer_size', 'write_buffer_size', 'queue_size', 'updated_by'
                ];

                const initialSettings = {};
                if (deviceData.settings) {
                    allowedSettingsFields.forEach(f => {
                        if (deviceData.settings[f] !== undefined) {
                            initialSettings[f] = deviceData.settings[f];
                        }
                    });
                }
                // Ensure polling_interval_ms is set if not provided in settings but in deviceData
                if (initialSettings.polling_interval_ms === undefined) {
                    initialSettings.polling_interval_ms = deviceData.polling_interval || 1000;
                }

                // Boolean fields to integer (0/1)
                const booleanFields = [
                    'is_keep_alive_enabled', 'is_data_validation_enabled',
                    'is_outlier_detection_enabled', 'is_deadband_enabled',
                    'is_detailed_logging_enabled', 'is_performance_monitoring_enabled',
                    'is_diagnostic_mode_enabled', 'is_communication_logging_enabled'
                ];

                booleanFields.forEach(f => {
                    if (initialSettings[f] !== undefined) {
                        initialSettings[f] = initialSettings[f] ? 1 : 0;
                    }
                });

                await trx('device_settings').insert({
                    device_id: id,
                    ...initialSettings
                });

                // Handle group assignments
                const groupIds = Array.isArray(deviceData.group_ids) ? deviceData.group_ids :
                    (deviceData.device_group_id ? [deviceData.device_group_id] : []);

                if (groupIds.length > 0) {
                    const assignments = groupIds.map(gid => ({
                        device_id: id,
                        group_id: gid,
                        is_primary: gid === deviceData.device_group_id ? 1 : 0
                    }));
                    await trx('device_group_assignments').insert(assignments);
                }

                // 🔥 NEW: Handle initial data points if provided
                if (Array.isArray(deviceData.data_points) && deviceData.data_points.length > 0) {
                    this.logger.log(`🛠️ [DeviceRepository] Inserting ${deviceData.data_points.length} initial data points for device ${id}...`);

                    const pointsToInsert = deviceData.data_points.map(dp => {
                        const point = {
                            device_id: id,
                            name: dp.name,
                            description: dp.description || '',
                            address: dp.address,
                            address_string: dp.address_string || '',
                            data_type: dp.data_type || 'uint16',
                            access_mode: dp.access_mode || 'read',
                            is_enabled: dp.is_enabled !== false ? 1 : 0,
                            is_writable: dp.is_writable ? 1 : 0,
                            unit: dp.unit || '',
                            scaling_factor: dp.scaling_factor !== undefined ? dp.scaling_factor : 1.0,
                            scaling_offset: dp.scaling_offset !== undefined ? dp.scaling_offset : 0.0,
                            min_value: dp.min_value !== undefined ? dp.min_value : 0.0,
                            max_value: dp.max_value !== undefined ? dp.max_value : 0.0,
                            log_enabled: dp.is_log_enabled !== false ? 1 : 0,
                            log_interval_ms: dp.log_interval_ms !== undefined ? dp.log_interval_ms : 0,
                            log_deadband: dp.log_deadband !== undefined ? dp.log_deadband : 0.0,
                            group_name: dp.group_name || '',
                            created_at: this.knex.fn.now(),
                            updated_at: this.knex.fn.now()
                        };

                        // JSON fields
                        if (dp.tags) point.tags = typeof dp.tags === 'object' ? JSON.stringify(dp.tags) : dp.tags;
                        if (dp.metadata) point.metadata = typeof dp.metadata === 'object' ? JSON.stringify(dp.metadata) : dp.metadata;
                        if (dp.protocol_params) point.protocol_params = typeof dp.protocol_params === 'object' ? JSON.stringify(dp.protocol_params) : dp.protocol_params;

                        return point;
                    });

                    await trx('data_points').insert(pointsToInsert);
                }

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
                'retry_count', 'is_enabled', 'installation_date', 'device_group_id', 'edge_server_id',
                'tags', 'metadata', 'custom_fields'
            ];

            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if ((field === 'config' || field === 'tags' || field === 'metadata' || field === 'custom_fields') && typeof updateData[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(updateData[field]);
                    } else if (field === 'is_enabled') {
                        dataToUpdate[field] = updateData[field] ? 1 : 0;
                    } else {
                        dataToUpdate[field] = updateData[field];
                    }
                }
            });

            return await this.knex.transaction(async trx => {
                this.logger.log(`🛠️ [DeviceRepository] Starting update transaction for device ${id}...`);

                let query = trx('devices').where('id', id);
                if (tenantId) query = query.where('tenant_id', tenantId);

                const affected = await query.update(dataToUpdate);
                if (affected === 0) {
                    this.logger.warn(`⚠️ [DeviceRepository] No rows affected by update for device ${id} (Tenant: ${tenantId})`);
                }

                // Update settings if provided
                if (updateData.settings) {
                    this.logger.log(`🛠️ [DeviceRepository] Updating settings for device ${id}...`);
                    const allowedSettingsFields = [
                        'polling_interval_ms', 'scan_rate_override', 'scan_group',
                        'connection_timeout_ms', 'read_timeout_ms', 'write_timeout_ms',
                        'inter_frame_delay_ms', 'max_retry_count', 'retry_interval_ms',
                        'backoff_multiplier', 'backoff_time_ms', 'max_backoff_time_ms',
                        'is_keep_alive_enabled', 'keep_alive_interval_s', 'keep_alive_timeout_s',
                        'is_data_validation_enabled', 'is_outlier_detection_enabled', 'is_deadband_enabled',
                        'is_detailed_logging_enabled', 'is_performance_monitoring_enabled',
                        'is_diagnostic_mode_enabled', 'is_communication_logging_enabled',
                        'read_buffer_size', 'write_buffer_size', 'queue_size', 'updated_by'
                    ];

                    const settingsToUpdate = {
                        updated_at: this.knex.fn.now()
                    };

                    allowedSettingsFields.forEach(field => {
                        if (updateData.settings[field] !== undefined) {
                            settingsToUpdate[field] = updateData.settings[field];
                        }
                    });

                    // Boolean fields to integer (0/1)
                    const booleanFields = [
                        'is_keep_alive_enabled', 'is_data_validation_enabled',
                        'is_outlier_detection_enabled', 'is_deadband_enabled',
                        'is_detailed_logging_enabled', 'is_performance_monitoring_enabled',
                        'is_diagnostic_mode_enabled', 'is_communication_logging_enabled'
                    ];

                    booleanFields.forEach(f => {
                        if (settingsToUpdate[f] !== undefined) {
                            settingsToUpdate[f] = settingsToUpdate[f] ? 1 : 0;
                        }
                    });

                    const affectedSettings = await trx('device_settings')
                        .where('device_id', id)
                        .update(settingsToUpdate);

                    if (affectedSettings === 0) {
                        this.logger.log(`🛠️ [DeviceRepository] device_settings for ${id} not found, inserting new row...`);
                        await trx('device_settings').insert({
                            device_id: id,
                            ...settingsToUpdate
                        });
                    }
                }

                // Handle group assignments
                if (updateData.group_ids !== undefined) {
                    this.logger.log(`🛠️ [DeviceRepository] Updating group assignments for device ${id}...`);
                    // Delete old assignments
                    await trx('device_group_assignments').where('device_id', id).del();

                    // Insert new assignments
                    if (Array.isArray(updateData.group_ids) && updateData.group_ids.length > 0) {
                        const assignments = updateData.group_ids.map(gid => ({
                            device_id: id,
                            group_id: gid,
                            is_primary: gid === (updateData.device_group_id || dataToUpdate.device_group_id) ? 1 : 0
                        }));
                        await trx('device_group_assignments').insert(assignments);
                    }
                }

                // 🔥 NEW: Handle bulk data point replacement with Upsert logic
                // Deleting all and re-inserting breaks foreign key relations (alarms, history)
                if (updateData.data_points !== undefined) {
                    this.logger.log(`🛠️ [DeviceRepository] Upserting data points for device ${id}...`);

                    const newPoints = Array.isArray(updateData.data_points) ? updateData.data_points : [];

                    // 1. Get existing points for this device to preserve metadata/types
                    const existingPoints = await trx('data_points').where('device_id', id).select('*');
                    const existingIds = existingPoints.map(p => p.id);
                    const existingMap = new Map(existingPoints.map(p => [p.id, p]));

                    // 2. Separate points into groups
                    const toInsert = [];
                    const toUpdate = [];
                    const retainedIds = [];

                    for (const dp of newPoints) {
                        // We consider IDs > 2000000000 (Date.now()) or non-existent IDs as new
                        const isNew = !dp.id || dp.id > 2000000000 || !existingIds.includes(Number(dp.id));

                        const existingPoint = !isNew ? existingMap.get(Number(dp.id)) : null;
                        const pointData = this._mapDataPointToDb(dp, id, existingPoint);

                        if (isNew) {
                            pointData.created_at = this.knex.fn.now();
                            toInsert.push(pointData);
                        } else {
                            toUpdate.push({ id: Number(dp.id), data: pointData });
                            retainedIds.push(Number(dp.id));
                        }
                    }

                    // 3. Delete points not in the new list
                    const idsToDelete = existingIds.filter(eid => !retainedIds.includes(eid));
                    if (idsToDelete.length > 0) {
                        await trx('data_points').whereIn('id', idsToDelete).del();
                        this.logger.log(`🗑️ [DeviceRepository] Deleted ${idsToDelete.length} removed data points`);
                    }

                    // 4. Perform Updates
                    for (const item of toUpdate) {
                        await trx('data_points').where('id', item.id).update(item.data);
                    }
                    if (toUpdate.length > 0) {
                        this.logger.log(`upd [DeviceRepository] Updated ${toUpdate.length} existing data points`);
                    }

                    // 5. Perform Inserts
                    if (toInsert.length > 0) {
                        await trx('data_points').insert(toInsert);
                        this.logger.log(`➕ [DeviceRepository] Inserted ${toInsert.length} new data points`);
                    }
                }

                this.logger.log(`✅ [DeviceRepository] Update transaction finished for ${id}`);
                return await this.findById(id, tenantId, trx);
            });
        } catch (error) {
            this.logger.error(`❌ [DeviceRepository] update failure for ${id}:`, error.message);
            throw error;
        }
    }

    /**
     * 여러 디바이스 정보를 일괄 업데이트합니다.
     */
    async bulkUpdate(ids, updateData, tenantId = null) {
        try {
            const dataToUpdate = {
                ...updateData,
                updated_at: this.knex.fn.now()
            };

            // JSON 필드 처리
            if (dataToUpdate.config && typeof dataToUpdate.config === 'object') {
                dataToUpdate.config = JSON.stringify(dataToUpdate.config);
            }
            if (dataToUpdate.tags && typeof dataToUpdate.tags === 'object') {
                dataToUpdate.tags = JSON.stringify(dataToUpdate.tags);
            }

            let query = this.knex('devices').whereIn('id', ids);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected;
        } catch (error) {
            this.logger.error('DeviceRepository.bulkUpdate 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스를 삭제합니다.
     */
    async deleteById(id, tenantId = null) {
        try {
            this.logger.log(`🗑️ [DeviceRepository] Soft-deleting device ${id}...`);
            const dataToUpdate = {
                is_deleted: 1,
                updated_at: this.knex.fn.now()
            };

            let query = this.knex('devices').where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected > 0;
        } catch (error) {
            this.logger.error('DeviceRepository.deleteById (Soft) 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스를 복구합니다.
     */
    async restoreById(id, tenantId = null) {
        try {
            this.logger.log(`♻️ [DeviceRepository] Restoring device ${id}...`);
            const dataToUpdate = {
                is_deleted: 0,
                updated_at: this.knex.fn.now()
            };

            let query = this.knex('devices').where('id', id);
            if (tenantId) query = query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected > 0;
        } catch (error) {
            this.logger.error('DeviceRepository.restoreById 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스를 영구 삭제합니다 (개발/테스트용)
     */
    async hardDeleteById(id, tenantId = null) {
        try {
            return await this.knex.transaction(async (trx) => {
                let query = trx('devices').where('id', id);
                if (tenantId) query = query.where('tenant_id', tenantId);

                // Delete related records first
                await trx('device_status').where('device_id', id).del();
                await trx('device_settings').where('device_id', id).del();
                await trx('device_group_assignments').where('device_id', id).del();

                const affected = await query.del();
                return affected > 0;
            });
        } catch (error) {
            this.logger.error('DeviceRepository.hardDeleteById 오류:', error);
            throw error;
        }
    }

    /**
     * 디바이스의 데이터 포인트 목록을 조회합니다.
     */
    async getDataPointsByDevice(deviceId) {
        try {
            const sql = DeviceQueries.getDataPointsByDevice();
            return await this.executeQuery(sql, [deviceId]);
        } catch (error) {
            this.logger.error(`DeviceRepository.getDataPointsByDevice 실패 (DeviceID: ${deviceId}):`, error);
            throw error;
        }
    }

    /**
     * 특정 데이터 포인트를 상세 조회합니다.
     */
    async getDataPointById(pointId) {
        try {
            return await this.knex('data_points').where('id', pointId).first();
        } catch (error) {
            this.logger.error(`DeviceRepository.getDataPointById 실패 (PointID: ${pointId}):`, error);
            throw error;
        }
    }

    /**
     * 디바이스의 모든 현재값을 조회합니다.
     */
    async getCurrentValuesByDevice(deviceId, tenantId = null) {
        try {
            const sql = DeviceQueries.getCurrentValuesByDevice();
            return await this.executeQuery(sql, [deviceId]);
        } catch (error) {
            this.logger.error(`DeviceRepository.getCurrentValuesByDevice 실패 (DeviceID: ${deviceId}):`, error);
            throw error;
        }
    }

    /**
     * 특정 디바이스의 실시간 데이터(Redis) 존재 여부 확인
     */
    async hasCurrentValues(deviceId, tenantId = null) {
        try {
            const exists = await redis.exists(`current_values:${deviceId}`);
            return exists === 1 || exists === true;
        } catch (error) {
            this.logger.error(`DeviceRepository.hasCurrentValues 실패 (DeviceID: ${deviceId}):`, error.message);
            return false;
        }
    }

    // ==========================================================================
    // 통계 및 집계 메소드 (DashboardService 등에서 사용)
    // ==========================================================================

    /**
     * 프로토콜별 디바이스 통계 조회
     */
    async getDeviceStatsByProtocol(tenantId) {
        try {
            const sql = DeviceQueries.getDeviceCountByProtocol();
            return await this.executeQuery(sql, [tenantId]);
        } catch (error) {
            this.logger.error('DeviceRepository.getDeviceStatsByProtocol 실패:', error);
            return [];
        }
    }

    /**
     * 사이트별 디바이스 통계 조회
     */
    async getDeviceStatsBySite(tenantId) {
        try {
            const sql = DeviceQueries.getDeviceCountBySite();
            return await this.executeQuery(sql, [tenantId]);
        } catch (error) {
            this.logger.error('DeviceRepository.getDeviceStatsBySite 실패:', error);
            return [];
        }
    }

    /**
     * 전체 시스템 상태 요약 조회
     */
    async getSystemStatusSummary(tenantId) {
        try {
            const sql = DeviceQueries.getSystemStatusSummary();
            const result = await this.executeQuerySingle(sql, [tenantId]);
            this.logger.log(`📊 [DeviceRepository] systemSummary result for tenant ${tenantId}:`, result);
            return result || {
                total_devices: 0,
                active_devices: 0,
                connected_devices: 0,
                disconnected_devices: 0,
                error_devices: 0,
                protocol_types: 0,
                sites_with_devices: 0,
                total_data_points: 0,
                enabled_data_points: 0
            };
        } catch (error) {
            this.logger.error('DeviceRepository.getSystemStatusSummary 실패:', error);
            return {};
        }
    }

    /**
     * 데이터 포인트 검색 (크로스 디바이스)
     */
    async searchDataPoints(tenantId, searchTerm = '') {
        try {
            const sql = DeviceQueries.searchDataPoints(tenantId);
            const term = `%${searchTerm}%`;
            const params = tenantId ? [tenantId, term, term, term] : [term, term, term];
            return await this.executeQuery(sql, params);
        } catch (error) {
            this.logger.error('DeviceRepository.searchDataPoints 실패:', error);
            return [];
        }
    }

    /**
     * 최근 활동한 디바이스 목록 조회
     */
    async getRecentActiveDevices(tenantId, limit = 10) {
        try {
            const sql = DeviceQueries.getRecentActiveDevices();
            return await this.executeQuery(sql, [tenantId, limit]);
        } catch (error) {
            this.logger.error('DeviceRepository.getRecentActiveDevices 실패:', error);
            return [];
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

        // Parse tags JSON if it's a string
        if (typeof device.tags === 'string' && device.tags.length > 0) {
            try {
                result.tags = JSON.parse(device.tags);
            } catch (e) {
                this.logger.warn(`Device ${device.id}: Tags parsing failed:`, e.message);
                result.tags = [];
            }
        } else if (!device.tags) {
            result.tags = [];
        }

        // Parse metadata JSON if it's a string
        if (typeof device.metadata === 'string' && device.metadata.length > 0) {
            try {
                result.metadata = JSON.parse(device.metadata);
            } catch (e) {
                this.logger.warn(`Device ${device.id}: Metadata parsing failed:`, e.message);
                result.metadata = {};
            }
        } else if (!device.metadata) {
            result.metadata = {};
        }

        // Parse custom_fields JSON if it's a string
        if (typeof device.custom_fields === 'string' && device.custom_fields.length > 0) {
            try {
                result.custom_fields = JSON.parse(device.custom_fields);
            } catch (e) {
                this.logger.warn(`Device ${device.id}: Custom fields parsing failed:`, e.message);
                result.custom_fields = {};
            }
        } else if (!device.custom_fields) {
            result.custom_fields = {};
        }

        // Parse assigned_groups (from GROUP_CONCAT)
        if (device.assigned_groups) {
            result.groups = device.assigned_groups.split(',').map(item => {
                const [id, ...nameParts] = item.split(':');
                return {
                    id: parseInt(id),
                    name: nameParts.join(':')
                };
            });
        } else if (!result.groups) {
            result.groups = [];
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

    /**
     * 장치들을 대량으로 삭제합니다.
     */
    async bulkDelete(ids, tenantId = null) {
        try {
            this.logger.log(`🗑️ [DeviceRepository] Soft-deleting ${ids.length} devices...`);
            const query = this.knex('devices').whereIn('id', ids);
            if (tenantId) query.where('tenant_id', tenantId);

            return await query.update({
                is_deleted: 1,
                updated_at: this.knex.fn.now()
            });
        } catch (error) {
            this.logger.error('DeviceRepository.bulkDelete 오류:', error);
            throw error;
        }
    }

    /**
     * 테넌트별 디바이스 통계 조회
     */
    async getStatistics(tenantId) {
        try {
            let statsQuery = this.query('d')
                .leftJoin('device_status as ds', 'ds.device_id', 'd.id')
                .andWhere(builder => {
                    builder.where('d.is_deleted', 0).orWhereNull('d.is_deleted');
                });

            if (tenantId) {
                statsQuery.where('d.tenant_id', tenantId);
            }

            const stats = await statsQuery.select(
                this.knex.raw('COUNT(*) as total'),
                this.knex.raw("SUM(CASE WHEN ds.connection_status = 'connected' THEN 1 ELSE 0 END) as connected"),
                this.knex.raw("SUM(CASE WHEN ds.connection_status = 'error' THEN 1 ELSE 0 END) as error"),
                this.knex.raw("SUM(CASE WHEN ds.connection_status = 'disconnected' OR ds.connection_status IS NULL THEN 1 ELSE 0 END) as disconnected")
            ).first();

            let protocolsQuery = this.query('d')
                .join('protocols as p', 'p.id', 'd.protocol_id')
                .andWhere(builder => {
                    builder.where('d.is_deleted', 0).orWhereNull('d.is_deleted');
                });

            if (tenantId) {
                protocolsQuery.where('d.tenant_id', tenantId);
            }

            const protocols = await protocolsQuery.select('p.protocol_type', 'p.display_name')
                .count('* as count')
                .groupBy('p.protocol_type', 'p.display_name');

            return {
                total_devices: stats.total || 0,
                connected_devices: stats.connected || 0,
                active_devices: stats.connected || 0, // Alias for connected
                disconnected_devices: stats.disconnected || 0,
                error_devices: stats.error || 0,
                enabled_devices: stats.total || 0, // Alias for total in this context
                by_protocol: protocols.map(p => ({
                    type: p.protocol_type,
                    name: p.display_name,
                    count: parseInt(p.count) || 0
                }))
            };
        } catch (error) {
            this.logger.error('DeviceRepository.getStatistics 오류:', error);
            throw error;
        }
    }

    /**
     * 공통 필터 적용 (findAll 및 countQuery에서 공유)
     * @private
     */
    _applyFilters(query, tenantId, options) {
        if (tenantId) {
            query.where('d.tenant_id', tenantId);
        }

        // [Soft Delete] 
        const includeDeleted = options.includeDeleted === true || options.includeDeleted === 'true';
        const onlyDeleted = options.onlyDeleted === true || options.onlyDeleted === 'true';

        if (onlyDeleted) {
            query.where('d.is_deleted', 1);
        } else if (!includeDeleted) {
            query.where(builder => {
                builder.where('d.is_deleted', 0).orWhereNull('d.is_deleted');
            });
        }

        if (options.siteId || options.site_id) {
            query.where('d.site_id', options.siteId || options.site_id);
        }

        if (options.groupId) {
            const groupIds = Array.isArray(options.groupId) ? options.groupId : [options.groupId];
            query.where(builder => {
                builder.whereIn('d.device_group_id', groupIds)
                    .orWhereExists(function () {
                        this.select('*')
                            .from('device_group_assignments')
                            .whereRaw('device_group_assignments.device_id = d.id')
                            .whereIn('device_group_assignments.group_id', groupIds);
                    });
            });
        }

        if (options.edgeServerId) {
            query.where('d.edge_server_id', options.edgeServerId);
        }

        if (options.tag) {
            query.where('d.tags', 'like', `%${options.tag}%`);
        }

        if (options.search) {
            query.where(builder => {
                builder.where('d.name', 'like', `%${options.search}%`)
                    .orWhere('d.description', 'like', `%${options.search}%`)
                    .orWhere('d.endpoint', 'like', `%${options.search}%`);
            });
        }

        if (options.endpoint) {
            query.where('d.endpoint', options.endpoint);
        }

        // 필터 추가: 프로토콜 타입
        if (options.protocol_type && options.protocol_type !== 'all') {
            query.whereExists(function () {
                this.select('*')
                    .from('protocols')
                    .whereRaw('protocols.id = d.protocol_id')
                    .where('protocols.protocol_type', options.protocol_type);
            });
        }

        // 필터 추가: 통신 상태
        if (options.connection_status && options.connection_status !== 'all') {
            query.whereExists(function () {
                this.select('*')
                    .from('device_status')
                    .whereRaw('device_status.device_id = d.id')
                    .where('device_status.connection_status', options.connection_status);
            });
        }
    }

    /**
     * 필드 매핑 및 데이터 타입 변환을 처리하는 헬퍼 메서드
     * @private
     */
    _mapDataPointToDb(dp, deviceId, existingPoint = null) {
        const VALID_DB_TYPES = [
            'BOOL', 'INT8', 'UINT8', 'INT16', 'UINT16', 'INT32', 'UINT32',
            'INT64', 'UINT64', 'FLOAT32', 'FLOAT64', 'STRING', 'UNKNOWN'
        ];

        let dbDataType = 'UNKNOWN';
        const incomingType = String(dp.data_type || '').toUpperCase();

        // 1. 이미 유효한 DB 타입인 경우 (INT16, FLOAT32 등) 그대로 사용
        if (VALID_DB_TYPES.includes(incomingType)) {
            dbDataType = incomingType;
        }
        // 2. 단순화된 타입(number, boolean, string)인 경우
        else {
            const feType = String(dp.data_type || '').toLowerCase();

            // 기존 포인트가 있다면 기존 타입을 최대한 유지 (가장 중요!)
            if (existingPoint && VALID_DB_TYPES.includes(existingPoint.data_type)) {
                dbDataType = existingPoint.data_type;
            } else {
                // 신규이거나 기존 타입이 없을 경우 매핑
                if (feType === 'boolean') dbDataType = 'BOOL';
                else if (feType === 'string') dbDataType = 'STRING';
                else dbDataType = 'FLOAT32'; // number의 기본값으로 FLOAT64보다 안전한 FLOAT32 선택
            }
        }

        const pointData = {
            device_id: deviceId,
            name: dp.name,
            description: dp.description || '',
            address: parseInt(dp.address, 10),
            address_string: dp.address_string || String(dp.address),
            data_type: dbDataType,
            access_mode: dp.access_mode || 'read',
            is_enabled: dp.is_enabled !== false ? 1 : 0,
            is_writable: (dp.access_mode === 'write' || dp.access_mode === 'read_write') ? 1 : 0,
            unit: dp.unit || '',
            scaling_factor: dp.scaling_factor !== undefined ? dp.scaling_factor : 1.0,
            scaling_offset: dp.scaling_offset !== undefined ? dp.scaling_offset : 0.0,
            min_value: dp.min_value !== undefined ? dp.min_value : 0.0,
            max_value: dp.max_value !== undefined ? dp.max_value : 0.0,
            log_enabled: (dp.is_log_enabled !== false && dp.log_enabled !== false) ? 1 : 0,
            log_interval_ms: dp.log_interval_ms !== undefined ? dp.log_interval_ms : 0,
            // 알람 필드
            alarm_enabled: (dp.is_alarm_enabled || dp.alarm_enabled) ? 1 : 0,
            high_alarm_limit: dp.high_alarm_limit,
            low_alarm_limit: dp.low_alarm_limit,
            alarm_deadband: dp.alarm_deadband || 0.0,
            updated_at: this.knex.fn.now()
        };

        // JSON fields
        if (dp.tags) pointData.tags = typeof dp.tags === 'object' ? JSON.stringify(dp.tags) : dp.tags;
        if (dp.metadata) pointData.metadata = typeof dp.metadata === 'object' ? JSON.stringify(dp.metadata) : dp.metadata;
        if (dp.protocol_params) pointData.protocol_params = typeof dp.protocol_params === 'object' ? JSON.stringify(dp.protocol_params) : dp.protocol_params;

        return pointData;
    }
}

module.exports = DeviceRepository;