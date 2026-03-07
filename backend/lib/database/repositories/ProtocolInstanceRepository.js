
const BaseRepository = require('./BaseRepository');

class ProtocolInstanceRepository extends BaseRepository {
    constructor() {
        super('protocol_instances');
    }

    /**
     * 필터 조건으로 모든 인스턴스 조회
     */
    async findAll(filters = {}, pagination = { page: 1, limit: 100 }) {
        // 1. Base Query Construction
        let baseQuery = `FROM ${this.tableName} pi
                         LEFT JOIN tenants t ON pi.tenant_id = t.id`;
        const params = [];
        const whereClauses = [];

        if (filters.is_enabled !== undefined) {
            whereClauses.push('pi.is_enabled = ?');
            params.push(filters.is_enabled ? 1 : 0);
        }

        if (filters.protocol_id) {
            whereClauses.push('pi.protocol_id = ?');
            params.push(filters.protocol_id);
        }

        if (filters.tenant_id !== undefined) {
            whereClauses.push('(pi.tenant_id = ? OR pi.tenant_id IS NULL)');
            params.push(filters.tenant_id);
        }

        if (whereClauses.length > 0) {
            baseQuery += ' WHERE ' + whereClauses.join(' AND ');
        }

        // 2. Count Query
        const countQuery = `SELECT count(*) as total ${baseQuery}`;
        const countResult = await this.executeQuery(countQuery, params);
        const total = countResult[0]?.total || 0;

        // 3. Data Query with Pagination
        const { page, limit } = pagination;
        const offset = (page - 1) * limit;

        const dataQuery = `SELECT pi.*, t.company_name as tenant_name ${baseQuery} ORDER BY pi.created_at DESC LIMIT ? OFFSET ?`;
        const dataParams = [...params, limit, offset];

        const data = await this.executeQuery(dataQuery, dataParams);

        const totalPages = Math.ceil(total / limit);

        return {
            items: data,
            pagination: {
                total_count: total,
                current_page: page,
                limit,
                total_pages: totalPages,
                has_next: page < totalPages,
                has_prev: page > 1
            }
        };
    }

    /**
     * 특정 프로토콜의 모든 인스턴스 조회
     */
    async findByProtocolId(protocolId) {
        return await this.executeQuery(
            `SELECT pi.*, t.name as tenant_name FROM ${this.tableName} pi
             LEFT JOIN tenants t ON pi.tenant_id = t.id
             WHERE pi.protocol_id = ? ORDER BY pi.created_at DESC`,
            [protocolId]
        );
    }

    /**
     * 특정 프로토콜 타입명(MQTT 등)으로 인스턴스 조회
     */
    async findByProtocolType(protocolType) {
        return await this.executeQuery(
            `SELECT pi.*, t.name as tenant_name FROM ${this.tableName} pi 
             JOIN protocols p ON pi.protocol_id = p.id
             LEFT JOIN tenants t ON pi.tenant_id = t.id
             WHERE p.protocol_type = ? ORDER BY pi.created_at DESC`,
            [protocolType]
        );
    }

    /**
     * 신규 인스턴스 생성
     */
    async create(instanceData) {
        const query = `
            INSERT INTO ${this.tableName} (
                protocol_id, instance_name, description, vhost, api_key, api_key_updated_at, 
                connection_params, is_enabled, status, tenant_id, broker_type
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `;

        const params = [
            instanceData.protocol_id,
            instanceData.instance_name,
            instanceData.description || null,
            instanceData.vhost || '/',
            instanceData.api_key || require('crypto').randomBytes(16).toString('hex'), // 🔥 Auto-generate API Key if missing
            instanceData.api_key_updated_at || new Date().toISOString(),
            typeof instanceData.connection_params === 'string' ? instanceData.connection_params : JSON.stringify(instanceData.connection_params || {}),
            instanceData.is_enabled !== undefined ? (instanceData.is_enabled ? 1 : 0) : 1,
            instanceData.status || 'STOPPED',
            instanceData.tenant_id || null, // 🔥 NEW: Tenant ID
            instanceData.broker_type || 'INTERNAL' // 🔥 NEW: Broker Type
        ];

        const result = await this.executeNonQuery(query, params);
        return result.lastInsertRowid;
    }

    /**
     * 인스턴스 정보 업데이트 (knex 기반 — raw SQL 제거, 크로스 DB 호환)
     */
    async update(id, updateData) {
        const knexData = {};

        Object.entries(updateData).forEach(([key, value]) => {
            if (key === 'id') return;
            if (key === 'connection_params' && typeof value !== 'string') {
                knexData[key] = JSON.stringify(value);
            } else if (key === 'is_enabled') {
                knexData[key] = value ? 1 : 0;
            } else {
                knexData[key] = value;
            }
        });

        if (Object.keys(knexData).length === 0) return 0;

        knexData.updated_at = this.now();

        return await this.knex(this.tableName).where('id', id).update(knexData);
    }
}

module.exports = ProtocolInstanceRepository;
