// ============================================================================
// backend/lib/database/repositories/DeviceRepository.js
// BaseRepository 상속받은 DeviceRepository - 실제 SQLite DB 연동
// ============================================================================

const BaseRepository = require('./BaseRepository');

class DeviceRepository extends BaseRepository {
    constructor() {
        super('devices');
        
        // 디바이스 특화 필드 정의
        this.fields = {
            id: 'autoIncrement',
            tenant_id: 'int',
            site_id: 'int', 
            device_group_id: 'int',
            edge_server_id: 'int',
            name: 'varchar(100)',
            description: 'text',
            device_type: 'varchar(50)',
            manufacturer: 'varchar(100)',
            model: 'varchar(100)',
            serial_number: 'varchar(100)',
            protocol: 'varchar(50)',
            connection_string: 'varchar(255)',
            connection_params: 'text',
            polling_interval_ms: 'int',
            timeout_ms: 'int',
            retry_count: 'int',
            is_enabled: 'boolean',
            firmware_version: 'varchar(50)',
            last_seen: 'datetime',
            notes: 'text',
            is_connected: 'boolean',
            created_at: 'timestamp',
            updated_at: 'timestamp'
        };

        console.log('✅ DeviceRepository 초기화 완료');
    }

    // =========================================================================
    // 🔥 BaseRepository 필수 메서드 구현
    // =========================================================================

    /**
     * 모든 디바이스 조회 (페이징, 필터링 지원)
     */
    async findAll(options = {}) {
        try {
            const {
                tenantId,
                siteId,
                deviceType,
                protocol,
                status,
                search,
                page = 1,
                limit = 20,
                sortBy = 'name',
                sortOrder = 'asc'
            } = options;

            let query = `
                SELECT 
                    d.*,
                    s.name as site_name,
                    s.location as site_location,
                    t.company_name as tenant_name,
                    (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
                FROM ${this.tableName} d
                LEFT JOIN sites s ON d.site_id = s.id
                LEFT JOIN tenants t ON d.tenant_id = t.id
                WHERE 1=1
            `;

            const params = [];

            // 🔥 테넌트 필터링 (중요!)
            if (tenantId) {
                query += ` AND d.tenant_id = ?`;
                params.push(tenantId);
            }

            // 사이트 필터링
            if (siteId) {
                query += ` AND d.site_id = ?`;
                params.push(siteId);
            }

            // 디바이스 타입 필터링
            if (deviceType) {
                query += ` AND d.device_type = ?`;
                params.push(deviceType);
            }

            // 프로토콜 필터링
            if (protocol) {
                query += ` AND d.protocol = ?`;
                params.push(protocol);
            }

            // 상태 필터링
            if (status) {
                switch (status) {
                    case 'enabled':
                        query += ` AND d.is_enabled = 1`;
                        break;
                    case 'disabled':
                        query += ` AND d.is_enabled = 0`;
                        break;
                    case 'connected':
                        query += ` AND d.is_connected = 1`;
                        break;
                    case 'disconnected':
                        query += ` AND d.is_connected = 0`;
                        break;
                    case 'running':
                        query += ` AND d.is_enabled = 1 AND d.is_connected = 1`;
                        break;
                    case 'stopped':
                        query += ` AND d.is_enabled = 0 OR d.is_connected = 0`;
                        break;
                }
            }

            // 검색 필터링
            if (search) {
                query += ` AND (d.name LIKE ? OR d.description LIKE ? OR d.manufacturer LIKE ? OR d.model LIKE ?)`;
                const searchParam = `%${search}%`;
                params.push(searchParam, searchParam, searchParam, searchParam);
            }

            // 정렬 추가
            query += this.buildOrderClause(`d.${sortBy}`, sortOrder);

            // 페이징 추가  
            query += this.buildLimitClause(page, limit);

            // 캐시 키 생성
            const cacheKey = `findAll_${JSON.stringify(options)}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) {
                console.log('📋 캐시에서 디바이스 목록 반환');
                return cached;
            }

            // 쿼리 실행
            const devices = await this.executeQuery(query, params);

            // 전체 개수 조회 (페이징용)
            let countQuery = `
                SELECT COUNT(*) as total 
                FROM ${this.tableName} d 
                WHERE 1=1
            `;
            const countParams = [];

            // 동일한 필터 조건 적용
            if (tenantId) {
                countQuery += ` AND d.tenant_id = ?`;
                countParams.push(tenantId);
            }
            if (siteId) {
                countQuery += ` AND d.site_id = ?`;
                countParams.push(siteId);
            }
            if (deviceType) {
                countQuery += ` AND d.device_type = ?`;
            // 🔍 DatabaseFactory executeQuery 결과 확인
            console.log("🔍 Raw executeQuery 결과:", typeof devices, devices);
            console.log("🔍 devices 배열 여부:", Array.isArray(devices));
            if (devices && typeof devices === "object" && !Array.isArray(devices)) {
                console.log("🔍 devices 객체 키:", Object.keys(devices));
            }
                countParams.push(deviceType);
            }
            if (protocol) {
                countQuery += ` AND d.protocol = ?`;
                countParams.push(protocol);
            }
            if (search) {
                countQuery += ` AND (d.name LIKE ? OR d.description LIKE ? OR d.manufacturer LIKE ? OR d.model LIKE ?)`;
                const searchParam = `%${search}%`;
                countParams.push(searchParam, searchParam, searchParam, searchParam);
            }

            const countResult = await this.executeQuerySingle(countQuery, countParams);
            const totalCount = countResult ? countResult.total : 0;

            // 결과 구성
            const result = {
                items: (Array.isArray(devices) ? devices : []).map(device => this.formatDevice(device)),
                pagination: {
                    current_page: parseInt(page),
                    total_pages: Math.ceil(totalCount / limit),
                    total_items: totalCount,
                    items_per_page: parseInt(limit)
                },
                summary: {
                    total_devices: totalCount,
                    running: (Array.isArray(devices) ? devices : []).filter(d => d.is_enabled && d.is_connected).length,
                    stopped: (Array.isArray(devices) ? devices : []).filter(d => !d.is_enabled || !d.is_connected).length,
                    enabled: (Array.isArray(devices) ? devices : []).filter(d => d.is_enabled).length,
                    disabled: (Array.isArray(devices) ? devices : []).filter(d => !d.is_enabled).length
                }
            };

            // 캐시 저장
            this.setCache(cacheKey, result);

            console.log(`✅ ${(Array.isArray(devices) ? devices : []).length}개 디바이스 조회 완료 (총 ${totalCount}개)`);
            return result;

        } catch (error) {
            console.error('❌ DeviceRepository.findAll 실패:', error);
            throw error;
        }
    }

    /**
     * ID로 디바이스 조회
     */
    async findById(id, tenantId = null) {
        try {
            // 캐시 확인
            const cacheKey = `findById_${id}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) {
                return cached;
            }

            let query = `
                SELECT 
                    d.*,
                    s.name as site_name,
                    s.location as site_location,
                    t.company_name as tenant_name,
                    (SELECT COUNT(*) FROM data_points dp WHERE dp.device_id = d.id) as data_point_count
                FROM ${this.tableName} d
                LEFT JOIN sites s ON d.site_id = s.id
                LEFT JOIN tenants t ON d.tenant_id = t.id
                WHERE d.id = ?
            `;

            const params = [id];

            // 테넌트 필터링
            if (tenantId) {
                query += ` AND d.tenant_id = ?`;
                params.push(tenantId);
            }

            const device = await this.executeQuerySingle(query, params);
            
            if (!device) {
                return null;
            }

            const formattedDevice = this.formatDevice(device);
            
            // 캐시 저장
            this.setCache(cacheKey, formattedDevice);

            console.log(`✅ 디바이스 ID ${id} 조회 완료`);
            return formattedDevice;

        } catch (error) {
            console.error(`❌ DeviceRepository.findById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 디바이스 생성
     */
    async create(deviceData, tenantId = null) {
        try {
            // 테넌트 ID 자동 설정
            if (tenantId) {
                deviceData.tenant_id = tenantId;
            }

            // 기본값 설정
            const now = new Date().toISOString();
            const data = {
                ...deviceData,
                is_enabled: deviceData.is_enabled !== undefined ? deviceData.is_enabled : true,
                is_connected: deviceData.is_connected !== undefined ? deviceData.is_connected : false,
                polling_interval_ms: deviceData.polling_interval_ms || 1000,
                timeout_ms: deviceData.timeout_ms || 3000,
                retry_count: deviceData.retry_count || 3,
                created_at: now,
                updated_at: now
            };

            // 필수 필드 검증
            if (!data.name) {
                throw new Error('디바이스 이름은 필수입니다');
            }
            if (!data.protocol) {
                throw new Error('프로토콜은 필수입니다');
            }

            // INSERT 쿼리 생성
            const fields = Object.keys(data);
            const placeholders = fields.map(() => '?').join(', ');
            const values = fields.map(field => data[field]);

            const query = `
                INSERT INTO ${this.tableName} (${fields.join(', ')})
                VALUES (${placeholders})
            `;

            const result = await this.executeNonQuery(query, values);
            
            if (result && result.lastInsertRowid) {
                // 캐시 무효화
                this.invalidateCache();
                
                console.log(`✅ 디바이스 생성 완료: ID ${result.lastInsertRowid}`);
                return await this.findById(result.lastInsertRowid, tenantId);
            }

            throw new Error('디바이스 생성 실패');

        } catch (error) {
            console.error('❌ DeviceRepository.create 실패:', error);
            throw error;
        }
    }

    /**
     * 디바이스 업데이트
     */
    async update(id, updateData, tenantId = null) {
        try {
            // 업데이트 시간 설정
            updateData.updated_at = new Date().toISOString();

            // SET 절 생성
            const fields = Object.keys(updateData);
            const setClause = fields.map(field => `${field} = ?`).join(', ');
            const values = fields.map(field => updateData[field]);

            let query = `
                UPDATE ${this.tableName} 
                SET ${setClause}
                WHERE id = ?
            `;

            values.push(id);

            // 테넌트 필터링
            if (tenantId) {
                query += ` AND tenant_id = ?`;
                values.push(tenantId);
            }

            const result = await this.executeNonQuery(query, values);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache();
                
                console.log(`✅ 디바이스 ID ${id} 업데이트 완료`);
                return await this.findById(id, tenantId);
            }

            throw new Error('디바이스 업데이트 실패 또는 권한 없음');

        } catch (error) {
            console.error(`❌ DeviceRepository.update(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 디바이스 삭제
     */
    async deleteById(id, tenantId = null) {
        try {
            let query = `DELETE FROM ${this.tableName} WHERE id = ?`;
            const params = [id];

            // 테넌트 필터링
            if (tenantId) {
                query += ` AND tenant_id = ?`;
                params.push(tenantId);
            }

            const result = await this.executeNonQuery(query, params);

            if (result && result.changes > 0) {
                // 캐시 무효화
                this.invalidateCache();
                
                console.log(`✅ 디바이스 ID ${id} 삭제 완료`);
                return true;
            }

            return false;

        } catch (error) {
            console.error(`❌ DeviceRepository.deleteById(${id}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 디바이스 존재 확인
     */
    async exists(id, tenantId = null) {
        try {
            let query = `SELECT 1 FROM ${this.tableName} WHERE id = ?`;
            const params = [id];

            if (tenantId) {
                query += ` AND tenant_id = ?`;
                params.push(tenantId);
            }

            const result = await this.executeQuerySingle(query, params);
            return !!result;

        } catch (error) {
            console.error(`❌ DeviceRepository.exists(${id}) 실패:`, error);
            return false;
        }
    }

    // =========================================================================
    // 🔥 디바이스 특화 메서드들
    // =========================================================================

    /**
     * 프로토콜별 디바이스 조회
     */
    async findByProtocol(protocol, tenantId = null) {
        try {
            const cacheKey = `findByProtocol_${protocol}_${tenantId}`;
            const cached = this.getFromCache(cacheKey);
            if (cached) return cached;

            let query = `
                SELECT d.*, s.name as site_name, t.company_name as tenant_name
                FROM ${this.tableName} d
                LEFT JOIN sites s ON d.site_id = s.id
                LEFT JOIN tenants t ON d.tenant_id = t.id
                WHERE d.protocol = ?
            `;

            const params = [protocol];

            if (tenantId) {
                query += ` AND d.tenant_id = ?`;
                params.push(tenantId);
            }

            query += ` ORDER BY d.name`;

            const devices = await this.executeQuery(query, params);
            const result = (Array.isArray(devices) ? devices : []).map(device => this.formatDevice(device));

            this.setCache(cacheKey, result);
            return result;

        } catch (error) {
            console.error(`❌ DeviceRepository.findByProtocol(${protocol}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 사이트별 디바이스 조회
     */
    async findBySite(siteId, tenantId = null) {
        try {
            return await this.findAll({ siteId, tenantId, limit: 1000 });
        } catch (error) {
            console.error(`❌ DeviceRepository.findBySite(${siteId}) 실패:`, error);
            throw error;
        }
    }

    /**
     * 활성화된 디바이스만 조회
     */
    async findEnabledDevices(tenantId = null) {
        try {
            return await this.findAll({ status: 'enabled', tenantId, limit: 1000 });
        } catch (error) {
            console.error('❌ DeviceRepository.findEnabledDevices 실패:', error);
            throw error;
        }
    }

    /**
     * 연결된 디바이스만 조회
     */
    async findConnectedDevices(tenantId = null) {
        try {
            return await this.findAll({ status: 'connected', tenantId, limit: 1000 });
        } catch (error) {
            console.error('❌ DeviceRepository.findConnectedDevices 실패:', error);
            throw error;
        }
    }

    // =========================================================================
    // 🔥 헬퍼 메서드들
    // =========================================================================

    /**
     * 디바이스 데이터 포맷팅
     */
    formatDevice(device) {
        if (!device) return null;

        // connection_params JSON 파싱
        let connectionConfig = {};
        if (device.connection_params) {
            try {
                connectionConfig = JSON.parse(device.connection_params);
            } catch (e) {
                console.warn(`디바이스 ${device.id} 연결 설정 파싱 실패`);
            }
        }

        // connection_string에서 호스트:포트 추출
        let host = 'N/A', port = null;
        if (device.connection_string) {
            const match = device.connection_string.match(/(\d+\.\d+\.\d+\.\d+):(\d+)/);
            if (match) {
                host = match[1];
                port = parseInt(match[2]);
            } else if (device.connection_string.startsWith('mqtt://')) {
                const urlMatch = device.connection_string.match(/mqtt:\/\/([^:]+):(\d+)/);
                if (urlMatch) {
                    host = urlMatch[1];
                    port = parseInt(urlMatch[2]);
                }
            }
        }

        return {
            ...device,
            // 네트워크 정보
            host: host,
            port: port || connectionConfig.port || (device.protocol === 'MODBUS_TCP' ? 502 : 1883),
            slave_id: connectionConfig.slave_id || 1,
            
            // 상태 정보 정규화
            status: device.is_enabled ? 
                (device.is_connected ? 'running' : 'stopped') : 'disabled',
            connection_status: device.is_connected ? 'connected' : 'disconnected',
            
            // 연결 설정
            connection_config: connectionConfig,
            
            // 타임스탬프 처리
            last_communication: device.last_seen || device.updated_at,
            
            // 데이터 포인트 수 (이미 조인되어 있음)
            data_point_count: device.data_point_count || 0
        };
    }

    /**
     * 디바이스 연결 테스트 (시뮬레이션)
     */
    async testConnection(id, tenantId = null) {
        try {
            const device = await this.findById(id, tenantId);
            if (!device) {
                throw new Error('디바이스를 찾을 수 없습니다');
            }

            // 실제 환경에서는 프로토콜별 연결 테스트 로직 구현
            // 현재는 시뮬레이션
            const success = Math.random() > 0.2; // 80% 성공률

            const result = {
                device_id: id,
                success: success,
                response_time: success ? Math.floor(Math.random() * 100) + 10 : null,
                error: success ? null : '연결 시간 초과',
                tested_at: new Date().toISOString()
            };

            console.log(`🧪 디바이스 ${id} 연결 테스트: ${success ? '성공' : '실패'}`);
            return result;

        } catch (error) {
            console.error(`❌ DeviceRepository.testConnection(${id}) 실패:`, error);
            throw error;
        }
    }
}

module.exports = DeviceRepository;