const BaseRepository = require('./BaseRepository');

/**
 * ExportGatewayRepository class
 * Handles database operations for export gateways.
 */
class ExportGatewayRepository extends BaseRepository {
    constructor() {
        super('export_gateways');
    }

    async findAll(tenantId = null) {
        try {
            const query = this.query();
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            query.where('is_deleted', 0);
            return await query.orderBy('name', 'ASC');
        } catch (error) {
            this.logger.error('ExportGatewayRepository.findAll 오류:', error);
            throw error;
        }
    }

    async findById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id).where('is_deleted', 0);
            if (tenantId) {
                query.where('tenant_id', tenantId);
            }
            return await query.first();
        } catch (error) {
            this.logger.error('ExportGatewayRepository.findById 오류:', error);
            throw error;
        }
    }

    async create(data, tenantId = null) {
        try {
            const dataToInsert = {
                tenant_id: tenantId || data.tenant_id,
                name: data.name || data.server_name,
                description: data.description || null,
                ip_address: data.ip_address || null,
                port: data.port || 8080,
                status: data.status || 'pending',
                version: data.version || null,
                config: typeof data.config === 'object' ? JSON.stringify(data.config) : (data.config || '{}')
            };

            const [id] = await this.knex('export_gateways').insert(dataToInsert);
            return await this.findById(id, tenantId);
        } catch (error) {
            this.logger.error('ExportGatewayRepository.create 오류:', error);
            throw error;
        }
    }

    async update(id, updateData, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = [
                'name', 'description', 'ip_address', 'port',
                'status', 'version', 'config', 'last_seen', 'last_heartbeat'
            ];

            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if (field === 'config' && typeof updateData[field] === 'object') {
                        dataToUpdate[field] = JSON.stringify(updateData[field]);
                    } else {
                        dataToUpdate[field] = updateData[field];
                    }
                }
            });

            const query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update(dataToUpdate);
            return affected > 0 ? await this.findById(id, tenantId) : null;
        } catch (error) {
            this.logger.error('ExportGatewayRepository.update 오류:', error);
            throw error;
        }
    }

    async updateHeartbeat(id) {
        try {
            return await this.knex('export_gateways')
                .where('id', id)
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

    async deleteById(id, tenantId = null) {
        try {
            const query = this.query().where('id', id);
            if (tenantId) query.where('tenant_id', tenantId);

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
