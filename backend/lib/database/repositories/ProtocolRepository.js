// =============================================================================
// backend/lib/database/repositories/ProtocolRepository.js
// DeviceRepository 패턴 100% 동일한 프로토콜 전용 Repository
// =============================================================================

const BaseRepository = require('./BaseRepository');
const ProtocolQueries = require('../queries/ProtocolQueries');

class ProtocolRepository extends BaseRepository {
    constructor() {
        // DeviceRepository와 동일한 패턴: 매개변수 없는 생성자
        super('protocols');
        console.log('🔌 ProtocolRepository initialized with standard pattern');
    }

    // ==========================================================================
    // 기본 CRUD 연산 (ProtocolQueries 사용)
    // ==========================================================================

    async findAll(filters = {}) {
        try {
            console.log('ProtocolRepository.findAll 호출:', filters);
            
            let query = ProtocolQueries.getProtocolsWithDeviceStats();
            const params = [];
            const conditions = [];

            // 테넌트 ID 파라미터 추가 (디바이스 통계용)
            if (filters.tenantId) {
                params.push(filters.tenantId);
            } else {
                params.push(1); // 기본값
            }

            // 필터 조건 추가
            if (filters.category && filters.category !== 'all') {
                conditions.push('p.category = ?');
                params.push(filters.category);
            }

            if (filters.enabled === 'true') {
                conditions.push('p.is_enabled = 1');
            } else if (filters.enabled === 'false') {
                conditions.push('p.is_enabled = 0');
            }

            if (filters.deprecated === 'true') {
                conditions.push('p.is_deprecated = 1');
            } else if (filters.deprecated === 'false') {
                conditions.push('p.is_deprecated = 0');
            }

            if (filters.uses_serial === 'true') {
                conditions.push('p.uses_serial = 1');
            } else if (filters.uses_serial === 'false') {
                conditions.push('p.uses_serial = 0');
            }

            if (filters.requires_broker === 'true') {
                conditions.push('p.requires_broker = 1');
            } else if (filters.requires_broker === 'false') {
                conditions.push('p.requires_broker = 0');
            }

            if (filters.search) {
                conditions.push('(p.display_name LIKE ? OR p.protocol_type LIKE ? OR p.description LIKE ?)');
                const searchParam = `%${filters.search}%`;
                params.push(searchParam, searchParam, searchParam);
            }

            // 조건들을 쿼리에 추가
            if (conditions.length > 0) {
                query += ' WHERE ' + conditions.join(' AND ');
            }

            // 정렬 추가
            query += ProtocolQueries.addSortBy(filters.sortBy, filters.sortOrder);

            // 페이징 처리
            if (filters.limit) {
                query += ProtocolQueries.addLimit();
                params.push(parseInt(filters.limit));
                
                if (filters.offset) {
                    query += ProtocolQueries.addOffset();
                    params.push(parseInt(filters.offset));
                }
            }

            const protocols = await this.executeQuery(query, params);

            console.log(`✅ 프로토콜 ${protocols.length}개 조회 완료`);

            return protocols.map(protocol => this.parseProtocol(protocol));
            
        } catch (error) {
            console.error('ProtocolRepository.findAll 실패:', error.message);
            throw error;
        }
    }

    async findById(id, tenantId = null) {
        try {
            console.log(`ProtocolRepository.findById 호출: id=${id}, tenantId=${tenantId}`);
            
            let query = ProtocolQueries.getProtocolsWithDeviceStats();
            const params = [tenantId || 1]; // 디바이스 통계용 테넌트 ID

            query += ' WHERE p.id = ?';
            params.push(id);

            const protocols = await this.executeQuery(query, params);
            
            if (protocols.length === 0) {
                console.log(`프로토콜 ID ${id} 찾을 수 없음`);
                return null;
            }
            
            console.log(`✅ 프로토콜 ID ${id} 조회 성공: ${protocols[0].display_name}`);
            
            return this.parseProtocol(protocols[0]);
            
        } catch (error) {
            console.error('ProtocolRepository.findById 실패:', error.message);
            throw error;
        }
    }

    async findByType(protocolType) {
        try {
            const query = ProtocolQueries.findByType();
            const protocols = await this.executeQuery(query, [protocolType]);
            
            if (protocols.length === 0) {
                return null;
            }
            
            return this.parseProtocol(protocols[0]);
            
        } catch (error) {
            console.error('ProtocolRepository.findByType 실패:', error.message);
            throw error;
        }
    }

    async findByCategory(category) {
        try {
            const query = ProtocolQueries.findByCategory();
            const protocols = await this.executeQuery(query, [category]);
            
            return protocols.map(protocol => this.parseProtocol(protocol));
            
        } catch (error) {
            console.error('ProtocolRepository.findByCategory 실패:', error.message);
            throw error;
        }
    }

    async findActive() {
        try {
            const query = ProtocolQueries.findActive();
            const protocols = await this.executeQuery(query);
            
            return protocols.map(protocol => this.parseProtocol(protocol));
            
        } catch (error) {
            console.error('ProtocolRepository.findActive 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 생성, 수정, 삭제 연산
    // ==========================================================================

    async create(protocolData, userId = null) {
        try {
            console.log('ProtocolRepository.create 호출:', protocolData.protocol_type);
            
            const query = ProtocolQueries.createProtocol();
            const currentTime = ProtocolQueries.getCurrentTimestamp(this.dbType);
            
            const params = [
                protocolData.protocol_type,
                protocolData.display_name,
                protocolData.description || null,
                protocolData.default_port || null,
                protocolData.uses_serial ? 1 : 0,
                protocolData.requires_broker ? 1 : 0,
                JSON.stringify(protocolData.supported_operations || []),
                JSON.stringify(protocolData.supported_data_types || []),
                JSON.stringify(protocolData.connection_params || {}),
                protocolData.default_polling_interval || 1000,
                protocolData.default_timeout || 5000,
                protocolData.max_concurrent_connections || 1,
                protocolData.category || 'custom',
                protocolData.vendor || protocolData.manufacturer || null,
                protocolData.standard_reference || protocolData.specification || null,
                protocolData.is_enabled !== false ? 1 : 0,
                0, // is_deprecated 기본값
                protocolData.min_firmware_version || null,
                currentTime, // created_at
                currentTime  // updated_at
            ];

            const result = await this.executeNonQuery(query, params);
            const insertId = result.lastInsertRowid || result.insertId || result.lastID;

            if (insertId) {
                console.log(`✅ 프로토콜 ${protocolData.protocol_type} 생성 완료 (ID: ${insertId})`);
                return await this.findById(insertId);
            } else {
                throw new Error('Protocol creation failed - no ID returned');
            }
            
        } catch (error) {
            console.error('ProtocolRepository.create 실패:', error.message);
            throw error;
        }
    }

    async update(id, updateData) {
        try {
            console.log(`ProtocolRepository.update 호출: ID ${id}`, updateData);

            // 존재 확인
            const existing = await this.findById(id);
            if (!existing) {
                throw new Error(`Protocol with ID ${id} not found`);
            }

            const query = ProtocolQueries.updateProtocol();
            const currentTime = ProtocolQueries.getCurrentTimestamp(this.dbType);
            
            const params = [
                updateData.display_name !== undefined ? updateData.display_name : existing.display_name,
                updateData.description !== undefined ? updateData.description : existing.description,
                updateData.default_port !== undefined ? updateData.default_port : existing.default_port,
                updateData.default_polling_interval !== undefined ? updateData.default_polling_interval : existing.default_polling_interval,
                updateData.default_timeout !== undefined ? updateData.default_timeout : existing.default_timeout,
                updateData.max_concurrent_connections !== undefined ? updateData.max_concurrent_connections : existing.max_concurrent_connections,
                updateData.category !== undefined ? updateData.category : existing.category,
                updateData.vendor !== undefined ? updateData.vendor : (updateData.manufacturer !== undefined ? updateData.manufacturer : existing.vendor),
                updateData.standard_reference !== undefined ? updateData.standard_reference : (updateData.specification !== undefined ? updateData.specification : existing.standard_reference),
                updateData.is_enabled !== undefined ? (updateData.is_enabled ? 1 : 0) : existing.is_enabled,
                updateData.is_deprecated !== undefined ? (updateData.is_deprecated ? 1 : 0) : existing.is_deprecated,
                updateData.min_firmware_version !== undefined ? updateData.min_firmware_version : existing.min_firmware_version,
                currentTime, // updated_at
                id
            ];

            const result = await this.executeNonQuery(query, params);

            if (result.changes > 0) {
                console.log(`✅ 프로토콜 ID ${id} 수정 완료`);
                return await this.findById(id);
            } else {
                throw new Error('No changes made to protocol');
            }
            
        } catch (error) {
            console.error('ProtocolRepository.update 실패:', error.message);
            throw error;
        }
    }

    async delete(id, force = false) {
        try {
            console.log(`ProtocolRepository.delete 호출: ID ${id}, force=${force}`);

            // 존재 확인
            const existing = await this.findById(id);
            if (!existing) {
                throw new Error(`Protocol with ID ${id} not found`);
            }

            // 연결된 디바이스 확인
            const deviceCountQuery = ProtocolQueries.countDevicesByProtocol();
            const deviceCountResult = await this.executeQuerySingle(deviceCountQuery, [id]);
            const deviceCount = deviceCountResult ? deviceCountResult.count : 0;

            if (deviceCount > 0 && !force) {
                throw new Error(`Cannot delete protocol. ${deviceCount} devices are using this protocol. Use force=true to delete anyway.`);
            }

            // 강제 삭제인 경우 연결된 디바이스들 처리
            if (force && deviceCount > 0) {
                console.log(`⚠️ 강제 삭제: ${deviceCount}개 디바이스의 프로토콜을 기본값으로 변경합니다`);
                
                // 연결된 디바이스들의 프로토콜을 기본값으로 변경 (예: MODBUS_TCP의 ID가 1이라고 가정)
                await this.executeNonQuery(
                    'UPDATE devices SET protocol_id = 1 WHERE protocol_id = ?',
                    [id]
                );
            }

            // 프로토콜 삭제
            const deleteQuery = ProtocolQueries.deleteProtocol();
            const result = await this.executeNonQuery(deleteQuery, [id]);

            if (result.changes > 0) {
                console.log(`✅ 프로토콜 ID ${id} 삭제 완료`);
                return true;
            } else {
                throw new Error('Protocol deletion failed');
            }
            
        } catch (error) {
            console.error('ProtocolRepository.delete 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 제어 연산
    // ==========================================================================

    async enable(id) {
        try {
            const query = ProtocolQueries.enableProtocol();
            const currentTime = ProtocolQueries.getCurrentTimestamp(this.dbType);
            const result = await this.executeNonQuery(query, [currentTime, id]);

            if (result.changes > 0) {
                console.log(`✅ 프로토콜 ID ${id} 활성화 완료`);
                return await this.findById(id);
            } else {
                throw new Error(`Protocol with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('ProtocolRepository.enable 실패:', error.message);
            throw error;
        }
    }

    async disable(id) {
        try {
            const query = ProtocolQueries.disableProtocol();
            const currentTime = ProtocolQueries.getCurrentTimestamp(this.dbType);
            const result = await this.executeNonQuery(query, [currentTime, id]);

            if (result.changes > 0) {
                console.log(`✅ 프로토콜 ID ${id} 비활성화 완료`);
                return await this.findById(id);
            } else {
                throw new Error(`Protocol with ID ${id} not found`);
            }
            
        } catch (error) {
            console.error('ProtocolRepository.disable 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 통계 및 분석
    // ==========================================================================

    async getStatsByCategory() {
        try {
            const query = ProtocolQueries.getStatsByCategory();
            const stats = await this.executeQuery(query);
            
            return stats.map(stat => ({
                category: stat.category,
                count: parseInt(stat.count) || 0,
                percentage: parseFloat(stat.percentage) || 0
            }));
            
        } catch (error) {
            console.error('ProtocolRepository.getStatsByCategory 실패:', error.message);
            throw error;
        }
    }

    async getUsageStats(tenantId = null) {
        try {
            const query = ProtocolQueries.getUsageStatsByTenant();
            const stats = await this.executeQuery(query, [tenantId || 1]);
            
            return stats.map(stat => ({
                protocol_id: stat.id,
                protocol_type: stat.protocol_type,
                display_name: stat.display_name,
                category: stat.category,
                device_count: parseInt(stat.device_count) || 0,
                enabled_devices: parseInt(stat.enabled_devices) || 0,
                connected_devices: parseInt(stat.connected_devices) || 0
            }));
            
        } catch (error) {
            console.error('ProtocolRepository.getUsageStats 실패:', error.message);
            throw error;
        }
    }

    async getTopUsedProtocols(limit = 10) {
        try {
            const query = ProtocolQueries.getTopUsedProtocols();
            const protocols = await this.executeQuery(query, [limit]);
            
            return protocols.map(protocol => ({
                protocol_type: protocol.protocol_type,
                display_name: protocol.display_name,
                device_count: parseInt(protocol.device_count) || 0
            }));
            
        } catch (error) {
            console.error('ProtocolRepository.getTopUsedProtocols 실패:', error.message);
            throw error;
        }
    }

    async getCounts() {
        try {
            const [totalResult, enabledResult, deprecatedResult] = await Promise.all([
                this.executeQuerySingle(ProtocolQueries.countTotal()),
                this.executeQuerySingle(ProtocolQueries.countEnabled()),
                this.executeQuerySingle(ProtocolQueries.countDeprecated())
            ]);

            return {
                total_protocols: totalResult ? totalResult.count : 0,
                enabled_protocols: enabledResult ? enabledResult.count : 0,
                deprecated_protocols: deprecatedResult ? deprecatedResult.count : 0
            };
            
        } catch (error) {
            console.error('ProtocolRepository.getCounts 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 검증 및 관계
    // ==========================================================================

    async checkProtocolTypeExists(protocolType, excludeId = null) {
        try {
            const query = ProtocolQueries.checkProtocolTypeExists();
            const result = await this.executeQuerySingle(query, [protocolType, excludeId || -1]);
            
            return result ? result.count > 0 : false;
            
        } catch (error) {
            console.error('ProtocolRepository.checkProtocolTypeExists 실패:', error.message);
            throw error;
        }
    }

    async getDevicesByProtocol(protocolId, limit = 50, offset = 0) {
        try {
            const query = ProtocolQueries.getDevicesByProtocol();
            const devices = await this.executeQuery(query, [protocolId, limit, offset]);
            
            return devices.map(device => ({
                id: device.id,
                name: device.name,
                device_type: device.device_type,
                is_enabled: Boolean(device.is_enabled),
                connection_status: device.connection_status || 'unknown',
                last_communication: device.last_communication
            }));
            
        } catch (error) {
            console.error('ProtocolRepository.getDevicesByProtocol 실패:', error.message);
            throw error;
        }
    }

    // ==========================================================================
    // 유틸리티 메소드들 - DeviceRepository 패턴과 동일
    // ==========================================================================

    parseProtocol(protocol) {
        if (!protocol) return null;

        return {
            ...protocol,
            supported_operations: this.parseJsonField(protocol.supported_operations),
            supported_data_types: this.parseJsonField(protocol.supported_data_types),
            connection_params: this.parseJsonField(protocol.connection_params),
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
            console.warn('JSON 파싱 실패:', error.message);
            return null;
        }
    }
}

module.exports = ProtocolRepository;