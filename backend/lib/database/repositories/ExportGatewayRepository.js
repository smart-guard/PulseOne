const BaseRepository = require('./BaseRepository');

/**
 * ExportGatewayRepository class
 * Handles database operations for export gateways.
 * Now points to `edge_servers` with server_type='gateway' (Unified Schema)
 */
class ExportGatewayRepository extends BaseRepository {
    constructor() {
        super('edge_servers'); // Changed from export_gateways
    }

    async findAll(tenantId = null, siteId = null, page = 1, limit = 10) {
        try {
            // Filter by server_type = 'gateway'
            const query = this.query().where('is_deleted', 0).where('server_type', 'gateway');

            if (tenantId) {
                query.where('tenant_id', tenantId);
            }

            if (siteId) {
                query.where('site_id', siteId);
            }

            // Get total count
            const countResult = await query.clone().clearSelect().clearOrder().count('id as total').first();
            const total = parseInt(countResult?.total || 0);

            // Fetch paginated items with alias for API compatibility
            const offset = (page - 1) * limit;
            const itemsRaw = await query.select('*', 'server_name as name')
                .orderBy('server_name', 'ASC')
                .offset(offset)
                .limit(limit);

            const items = itemsRaw.map(item => this._parseItem(item));

            return {
                items,
                pagination: {
                    current_page: parseInt(page),
                    total_pages: Math.ceil(total / limit),
                    total_count: total,
                    items_per_page: parseInt(limit)
                }
            };
        } catch (error) {
            this.logger.error('ExportGatewayRepository.findAll 오류:', error);
            throw error;
        }
    }

    async findById(id, tenantId = null, trx = null, siteId = null) {
        try {
            const query = this.query(trx)
                .select('*', 'server_name as name')
                .where('id', id)
                .where('is_deleted', 0)
                .where('server_type', 'gateway');

            if (tenantId) {
                query.where('tenant_id', tenantId);
            }

            if (siteId) {
                query.where('site_id', siteId);
            }

            const item = await query.first();
            return this._parseItem(item);
        } catch (error) {
            this.logger.error('ExportGatewayRepository.findById 오류:', error);
            throw error;
        }
    }

    async create(data, tenantId = null, trx = null, siteId = null) {
        try {
            const dataToInsert = {
                tenant_id: tenantId || data.tenant_id,
                site_id: siteId || data.site_id,
                server_name: data.name || data.server_name, // Map to server_name
                server_type: 'gateway',                     // Force type
                description: data.description || null,
                ip_address: data.ip_address || null,
                port: data.port || 8080,
                status: data.status || 'pending',
                subscription_mode: data.subscription_mode || 'all',
                config: typeof data.config === 'object' ? JSON.stringify(data.config) : (data.config || '{}')
            };

            const [id] = await (trx || this.knex)('edge_servers').insert(dataToInsert);
            return await this.findById(id, tenantId, trx, siteId);
        } catch (error) {
            this.logger.error('ExportGatewayRepository.create 오류:', error);
            throw error;
        }
    }

    async update(id, updateData, tenantId = null, siteId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = [
                'name', 'server_name', 'description', 'ip_address', 'port',
                'status', 'config', 'last_seen', 'last_heartbeat', 'subscription_mode', 'site_id', 'tenant_id'
            ];

            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    // Map name -> server_name
                    const dbField = field === 'name' ? 'server_name' : field;

                    if (field === 'config' && typeof updateData[field] === 'object') {
                        dataToUpdate[dbField] = JSON.stringify(updateData[field]);
                    } else {
                        dataToUpdate[dbField] = updateData[field];
                    }
                }
            });

            const query = this.query().where('id', id).where('server_type', 'gateway');
            if (tenantId) query.where('tenant_id', tenantId);
            // [REM] Do NOT filter by siteId in WHERE clause for updates, 
            // as we may be moving the gateway between sites.
            // if (siteId) query.where('site_id', siteId);

            const affected = await query.update(dataToUpdate);
            return affected > 0 ? await this.findById(id, tenantId, null, dataToUpdate.site_id || siteId) : null;
        } catch (error) {
            this.logger.error('ExportGatewayRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * JSON 필드 파싱 헬퍼
     */
    _parseItem(item) {
        if (!item) return null;
        if (typeof item.config === 'string') {
            try {
                item.config = JSON.parse(item.config);
            } catch (e) {
                item.config = {};
            }
        }
        return item;
    }

    async updateHeartbeat(id) {
        try {
            return await this.knex('edge_servers') // Changed from export_gateways
                .where('id', id)
                .where('server_type', 'gateway')
                .update({
                    last_seen: this.knex.fn.now(),
                    last_heartbeat: this.knex.fn.now(),
                    status: 'online',
                    updated_at: this.knex.fn.now()
                });
        } catch (error) {
            this.logger.error('ExportGatewayRepository.updateHeartbeat 오류:', error);
            throw error;
        }
    }

    async findActive(tenantId = null, siteId = null) {
        const query = this.query().where({ status: 'online', is_deleted: 0 }).where('server_type', 'gateway');
        if (tenantId) query.where('tenant_id', tenantId);
        if (siteId) query.where('site_id', siteId);
        const results = await query.orderBy('server_name', 'ASC');
        return results.map(item => this._parseItem(item));
    }

    async deleteById(id, tenantId = null, siteId = null) {
        try {
            const query = this.query().where('id', id).where('server_type', 'gateway');
            if (tenantId) query.where('tenant_id', tenantId);
            // [REM] siteId filter removed from WHERE for identity-based deletion
            // if (siteId) query.where('site_id', siteId);

            const affected = await query.update({
                is_deleted: 1,
                updated_at: this.knex.fn.now()
            });
            return affected > 0;
        } catch (error) {
            this.logger.error('ExportGatewayRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = ExportGatewayRepository;
