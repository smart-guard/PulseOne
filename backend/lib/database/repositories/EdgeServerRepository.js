const BaseRepository = require('./BaseRepository');

/**
 * EdgeServerRepository class
 * Handles database operations for collector instances (edge_servers).
 */
class EdgeServerRepository extends BaseRepository {
    constructor() {
        super('edge_servers');
    }

    async findAll(tenantId = null) {
        try {
            const query = this.query('es')
                .leftJoin('sites as s', 's.id', 'es.site_id')
                .select('es.*', 'es.server_name as name', 's.name as site_name');

            if (tenantId) {
                query.where('es.tenant_id', tenantId);
            }
            query.where('es.is_deleted', 0);
            return await query.orderBy('es.server_name', 'ASC');
        } catch (error) {
            this.logger.error('EdgeServerRepository.findAll 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 에지 서버를 조회합니다.
     */
    async findById(id, tenantId = null) {
        try {
            const query = this.query('es')
                .leftJoin('sites as s', 's.id', 'es.site_id')
                .select('es.*', 'es.server_name as name', 's.name as site_name')
                .where('es.id', id);

            if (tenantId) {
                query.where('es.tenant_id', tenantId);
            }
            return await query.first();
        } catch (error) {
            this.logger.error('EdgeServerRepository.findById 오류:', error);
            throw error;
        }
    }

    /**
     * 토큰으로 에지 서버를 조회합니다.
     */
    async findByToken(token) {
        try {
            return await this.query().where('registration_token', token).first();
        } catch (error) {
            this.logger.error('EdgeServerRepository.findByToken 오류:', error);
            throw error;
        }
    }

    /**
     * 새로운 에지 서버를 등록합니다.
     */
    async create(serverData, tenantId = null) {
        try {
            const dataToInsert = {
                tenant_id: tenantId || serverData.tenant_id,
                server_name: serverData.server_name,
                factory_name: serverData.factory_name || null,
                location: serverData.location || null,
                ip_address: serverData.ip_address || null,
                port: serverData.port || 8080,
                registration_token: serverData.registration_token || null,
                status: serverData.status || 'pending',
                config: typeof serverData.config === 'object' ? JSON.stringify(serverData.config) : (serverData.config || '{}'),
                capabilities: typeof serverData.capabilities === 'object' ? JSON.stringify(serverData.capabilities) : (serverData.capabilities || '[]')
            };

            const [id] = await this.knex('edge_servers').insert(dataToInsert);
            return await this.findById(id, tenantId);
        } catch (error) {
            this.logger.error('EdgeServerRepository.create 오류:', error);
            throw error;
        }
    }

    /**
     * 서버 정보를 업데이트합니다.
     */
    async update(id, updateData, tenantId = null) {
        try {
            const dataToUpdate = {
                updated_at: this.knex.fn.now()
            };

            const allowedFields = [
                'server_name', 'factory_name', 'location', 'ip_address', 'port',
                'status', 'config', 'capabilities', 'last_seen', 'version',
                'cpu_usage', 'memory_usage', 'disk_usage', 'uptime_seconds'
            ];

            allowedFields.forEach(field => {
                if (updateData[field] !== undefined) {
                    if ((field === 'config' || field === 'capabilities') && typeof updateData[field] === 'object') {
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
            this.logger.error('EdgeServerRepository.update 오류:', error);
            throw error;
        }
    }

    /**
     * 하트비트를 업데이트합니다.
     */
    async updateHeartbeat(id) {
        try {
            return await this.knex('edge_servers')
                .where('id', id)
                .update({
                    last_seen: this.knex.fn.now(),
                    last_heartbeat: this.knex.fn.now(),
                    status: 'active'
                });
        } catch (error) {
            this.logger.error('EdgeServerRepository.updateHeartbeat 오류:', error);
            throw error;
        }
    }

    /**
     * ID로 에지 서버를 삭제합니다 (Soft Delete).
     */
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
            this.logger.error('EdgeServerRepository.deleteById 오류:', error);
            throw error;
        }
    }
}

module.exports = EdgeServerRepository;
