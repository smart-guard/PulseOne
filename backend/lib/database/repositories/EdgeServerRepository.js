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
            query.where('es.server_type', 'collector');
            const results = await query.orderBy('es.server_name', 'ASC');
            return results.map(item => this._parseItem(item));
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
            // query.where('es.server_type', 'collector'); // [Fix] Allow Gateways too
            const item = await query.first();
            return this._parseItem(item);
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
            const item = await this.query().where('registration_token', token).first();
            return this._parseItem(item);
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
                server_type: serverData.server_type || 'collector',
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
                'status', 'server_type', 'config', 'capabilities', 'last_seen', 'version',
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

    /**
     * 특정 사이트에 속한 Collector 목록 조회
     */
    async findBySiteId(siteId) {
        try {
            const results = await this.query('es')
                .leftJoin('sites as s', 's.id', 'es.site_id')
                .select('es.*', 'es.server_name as name', 's.name as site_name')
                .where('es.site_id', siteId)
                .where('es.is_deleted', 0)
                .where('es.server_type', 'collector');  // gateway는 수집기 파티션에서 제외
            return results.map(item => this._parseItem(item));
        } catch (error) {
            this.logger.error('EdgeServerRepository.findBySiteId 오류:', error);
            throw error;
        }
    }

    /**
     * 사이트별 Collector + 장치 수 (단일 JOIN 쿼리, N+1 없음)
     */
    async findBySiteIdWithDeviceCount(siteId) {
        try {
            const results = await this.knex('edge_servers as es')
                .leftJoin('devices as d', function () {
                    this.on('d.edge_server_id', '=', 'es.id')
                        .andOnVal('d.is_deleted', '=', 0);
                })
                .select(
                    'es.id', 'es.server_name', 'es.server_name as name',
                    'es.server_type', 'es.status', 'es.site_id', 'es.tenant_id'
                )
                .count('d.id as device_count')
                .where('es.site_id', siteId)
                .where('es.is_deleted', 0)
                .groupBy('es.id')
                .orderBy('es.server_name', 'asc');
            return results;
        } catch (error) {
            this.logger.error('EdgeServerRepository.findBySiteIdWithDeviceCount 오류:', error);
            throw error;
        }
    }

    /**
     * 테넌트별 Collector 사용 개수 조회 (쿼터 체크용)
     */
    async countByTenant(tenantId) {
        try {
            const result = await this.knex('edge_servers')
                .where('tenant_id', tenantId)
                .where('server_type', 'collector')
                .where('is_deleted', 0)
                .count('id as cnt')
                .first();
            return parseInt(result.cnt) || 0;
        } catch (error) {
            this.logger.error('EdgeServerRepository.countByTenant 오류:', error);
            throw error;
        }
    }

    /**
     * 특정 Collector에 연결된 Device 수 조회 (이동 가능 여부 체크용)
     */
    async countDevicesByCollector(collectorId) {
        try {
            const result = await this.knex('devices')
                .where('edge_server_id', collectorId)
                .where('is_deleted', 0)
                .count('id as cnt')
                .first();
            return parseInt(result.cnt) || 0;
        } catch (error) {
            this.logger.error('EdgeServerRepository.countDevicesByCollector 오류:', error);
            throw error;
        }
    }

    /**
     * Collector의 site_id 변경 (재배정)
     */
    async updateSiteId(collectorId, newSiteId, tenantId = null) {
        try {
            const query = this.knex('edge_servers').where('id', collectorId);
            if (tenantId) query.where('tenant_id', tenantId);

            const affected = await query.update({
                site_id: newSiteId,
                updated_at: this.knex.fn.now()
            });
            return affected > 0;
        } catch (error) {
            this.logger.error('EdgeServerRepository.updateSiteId 오류:', error);
            throw error;
        }
    }


    /**
     * 비활성화 대상 Collector 조회 (쿼터 감소 시)
     * 우선순위: 1) 연결된 디바이스 0개, 2) 최근 생성 순
     * @param {number} tenantId
     * @param {number} limit - 비활성화할 개수
     */
    async findCollectorsForDeactivation(tenantId, limit) {
        try {
            const results = await this.knex('edge_servers as es')
                .leftJoin('devices as d', function () {
                    this.on('d.edge_server_id', '=', 'es.id')
                        .andOnVal('d.is_deleted', '=', 0);
                })
                .select('es.id', 'es.server_name')
                .count('d.id as device_count')
                .where('es.tenant_id', tenantId)
                .where('es.server_type', 'collector')
                .where('es.is_deleted', 0)
                .groupBy('es.id')
                .orderByRaw('COUNT(d.id) ASC, es.created_at DESC')
                .limit(limit);
            return results;
        } catch (error) {
            this.logger.error('EdgeServerRepository.findCollectorsForDeactivation 오류:', error);
            throw error;
        }
    }

    /**
     * 미배정 Collector 목록 (site_id IS NULL, DB 서버사이드 필터)
     */
    async findUnassigned(tenantId) {
        try {
            const results = await this.knex('edge_servers')
                .select('id', 'tenant_id', 'site_id', 'server_name', 'server_type', 'status', 'created_at')
                .where('tenant_id', tenantId)
                .where('server_type', 'collector')
                .where('is_deleted', 0)
                .whereNull('site_id')
                .orderBy('created_at', 'asc');
            return results.map(r => ({ ...r, name: r.server_name }));
        } catch (error) {
            this.logger.error('EdgeServerRepository.findUnassigned 오류:', error);
            throw error;
        }
    }

    /**
     * JSON 필드 파싱 헬퍼
     */
    _parseItem(item) {
        if (!item) return null;

        // config 파싱
        if (typeof item.config === 'string') {
            try {
                item.config = JSON.parse(item.config);
            } catch (e) {
                item.config = {};
            }
        }

        // capabilities 파싱
        if (typeof item.capabilities === 'string') {
            try {
                item.capabilities = JSON.parse(item.capabilities);
            } catch (e) {
                item.capabilities = [];
            }
        }

        return item;
    }
}

module.exports = EdgeServerRepository;
