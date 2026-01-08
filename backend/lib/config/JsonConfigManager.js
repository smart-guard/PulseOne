// ============================================================================
// backend/lib/config/JsonConfigManager.js
// JSON 설정 파일 전용 관리 시스템 (기존 ConfigManager에서 이름 변경)
// ============================================================================

const fs = require('fs').promises;
const path = require('path');

/**
 * JSON 설정 관리자 클래스 (싱글톤)
 * 데이터베이스와 JSON 파일 기반 설정을 통합 관리
 */
class JsonConfigManager {
    constructor() {
        this.configs = new Map();
        this.configPaths = {
            protocols: path.join(__dirname, '../../../config/protocols.json'),
            deviceTypes: path.join(__dirname, '../../../config/device-types.json'),
            deviceStatus: path.join(__dirname, '../../../config/device-status.json'),
            alarmTypes: path.join(__dirname, '../../../config/alarm-types.json'),
            userRoles: path.join(__dirname, '../../../config/user-roles.json'),
            siteTypes: path.join(__dirname, '../../../config/site-types.json')
        };
        
        this.dbConfigs = new Map(); // 데이터베이스에서 로드된 설정
        this.fileConfigs = new Map(); // 파일에서 로드된 설정
        this.cacheTimeout = 5 * 60 * 1000; // 5분 캐시
        this.lastLoad = new Map();
        
        this.logger = console; // TODO: 실제 LogManager로 교체
    }

    // ========================================================================
    // 싱글톤 패턴
    // ========================================================================

    static getInstance() {
        if (!JsonConfigManager.instance) {
            JsonConfigManager.instance = new JsonConfigManager();
        }
        return JsonConfigManager.instance;
    }

    // ========================================================================
    // 초기화 및 로딩
    // ========================================================================

    /**
     * 모든 설정 초기화
     */
    async initialize() {
        try {
            this.logger.log('📋 Initializing JsonConfigManager...');
            
            // 파일 기반 설정 로드
            await this.loadAllFileConfigs();
            
            // 데이터베이스 설정 로드 (나중에 구현)
            // await this.loadDatabaseConfigs();
            
            this.logger.log('✅ JsonConfigManager initialized successfully');
        } catch (error) {
            this.logger.error('❌ JsonConfigManager initialization failed:', error);
            throw error;
        }
    }

    /**
     * 모든 파일 설정 로드
     */
    async loadAllFileConfigs() {
        const promises = Object.entries(this.configPaths).map(async ([key, filePath]) => {
            try {
                await this.loadFileConfig(key, filePath);
            } catch (error) {
                this.logger.warn(`⚠️  Failed to load ${key} config:`, error.message);
                // 기본값 설정
                await this.setDefaultConfig(key);
            }
        });

        await Promise.all(promises);
    }

    /**
     * 개별 파일 설정 로드
     */
    async loadFileConfig(key, filePath) {
        try {
            const content = await fs.readFile(filePath, 'utf8');
            const config = JSON.parse(content);
            
            this.fileConfigs.set(key, config);
            this.lastLoad.set(key, Date.now());
            
            this.logger.log(`✅ Loaded ${key} config from ${filePath}`);
        } catch (error) {
            if (error.code === 'ENOENT') {
                // 파일이 없으면 기본값으로 생성
                await this.createDefaultConfigFile(key, filePath);
            } else {
                throw error;
            }
        }
    }

    /**
     * 기본 설정 파일 생성
     */
    async createDefaultConfigFile(key, filePath) {
        const defaultConfig = this.getDefaultConfigData(key);
        
        // 디렉토리 생성
        const dir = path.dirname(filePath);
        await fs.mkdir(dir, { recursive: true });
        
        // 파일 생성
        await fs.writeFile(filePath, JSON.stringify(defaultConfig, null, 2), 'utf8');
        
        this.fileConfigs.set(key, defaultConfig);
        this.lastLoad.set(key, Date.now());
        
        this.logger.log(`📄 Created default ${key} config at ${filePath}`);
    }

    // ========================================================================
    // 설정 조회 메서드들
    // ========================================================================

    /**
     * 지원하는 프로토콜 목록 조회
     */
    async getSupportedProtocols() {
        return await this.getConfig('protocols');
    }

    /**
     * 디바이스 타입 목록 조회
     */
    async getDeviceTypes() {
        return await this.getConfig('deviceTypes');
    }

    /**
     * 디바이스 상태 목록 조회
     */
    async getDeviceStatus() {
        return await this.getConfig('deviceStatus');
    }

    /**
     * 알람 타입 목록 조회
     */
    async getAlarmTypes() {
        return await this.getConfig('alarmTypes');
    }

    /**
     * 사용자 역할 목록 조회
     */
    async getUserRoles() {
        return await this.getConfig('userRoles');
    }

    /**
     * 사이트 타입 목록 조회
     */
    async getSiteTypes() {
        return await this.getConfig('siteTypes');
    }

    /**
     * 범용 설정 조회
     */
    async getConfig(key) {
        // 캐시 확인
        if (this.shouldReloadConfig(key)) {
            await this.reloadConfig(key);
        }

        const config = this.fileConfigs.get(key) || this.dbConfigs.get(key);
        
        if (!config) {
            this.logger.warn(`⚠️  Config not found: ${key}`);
            return this.getDefaultConfigData(key);
        }

        return config;
    }

    /**
     * 설정 유효성 검증
     */
    async validateValue(configType, value) {
        const config = await this.getConfig(configType);
        
        if (Array.isArray(config)) {
            return config.includes(value);
        }

        if (config.values && Array.isArray(config.values)) {
            return config.values.includes(value);
        }

        if (config.items) {
            return config.items.some(item => item.value === value || item.code === value);
        }

        return false;
    }

    // ========================================================================
    // 설정 업데이트 메서드들
    // ========================================================================

    /**
     * 설정 업데이트 (파일 기반)
     */
    async updateConfig(key, newConfig) {
        try {
            const filePath = this.configPaths[key];
            if (!filePath) {
                throw new Error(`Config path not found for: ${key}`);
            }

            // 백업 생성
            await this.createBackup(key);

            // 파일 업데이트
            await fs.writeFile(filePath, JSON.stringify(newConfig, null, 2), 'utf8');
            
            // 캐시 업데이트
            this.fileConfigs.set(key, newConfig);
            this.lastLoad.set(key, Date.now());

            this.logger.log(`✅ Updated ${key} config`);
            return true;
        } catch (error) {
            this.logger.error(`❌ Failed to update ${key} config:`, error);
            throw error;
        }
    }

    /**
     * 프로토콜 추가
     */
    async addProtocol(protocolData) {
        const protocols = await this.getSupportedProtocols();
        
        // 중복 체크
        const exists = protocols.items.some(p => 
            p.value === protocolData.value || p.name === protocolData.name
        );
        
        if (exists) {
            throw new Error(`Protocol already exists: ${protocolData.value}`);
        }

        protocols.items.push({
            value: protocolData.value,
            name: protocolData.name,
            description: protocolData.description,
            port: protocolData.port,
            config_schema: protocolData.config_schema || {},
            enabled: protocolData.enabled !== false,
            added_at: new Date().toISOString()
        });

        await this.updateConfig('protocols', protocols);
        return true;
    }

    /**
     * 디바이스 타입 추가
     */
    async addDeviceType(typeData) {
        const deviceTypes = await this.getDeviceTypes();
        
        // 중복 체크
        const exists = deviceTypes.items.some(t => 
            t.value === typeData.value || t.name === typeData.name
        );
        
        if (exists) {
            throw new Error(`Device type already exists: ${typeData.value}`);
        }

        deviceTypes.items.push({
            value: typeData.value,
            name: typeData.name,
            description: typeData.description,
            icon: typeData.icon || 'fas fa-microchip',
            category: typeData.category || 'general',
            enabled: typeData.enabled !== false,
            added_at: new Date().toISOString()
        });

        await this.updateConfig('deviceTypes', deviceTypes);
        return true;
    }

    // ========================================================================
    // 유틸리티 메서드들
    // ========================================================================

    /**
     * 설정 리로드 필요 여부 확인
     */
    shouldReloadConfig(key) {
        const lastLoadTime = this.lastLoad.get(key);
        if (!lastLoadTime) return true;
        
        return (Date.now() - lastLoadTime) > this.cacheTimeout;
    }

    /**
     * 설정 리로드
     */
    async reloadConfig(key) {
        const filePath = this.configPaths[key];
        if (filePath) {
            await this.loadFileConfig(key, filePath);
        }
    }

    /**
     * 백업 생성
     */
    async createBackup(key) {
        const filePath = this.configPaths[key];
        if (!filePath) return;

        const backupDir = path.join(path.dirname(filePath), 'backups');
        await fs.mkdir(backupDir, { recursive: true });

        const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
        const backupPath = path.join(backupDir, `${key}_${timestamp}.json`);

        try {
            await fs.copyFile(filePath, backupPath);
            this.logger.log(`📦 Backup created: ${backupPath}`);
        } catch (error) {
            this.logger.warn(`⚠️  Backup failed for ${key}:`, error.message);
        }
    }

    /**
     * 기본값 설정
     */
    async setDefaultConfig(key) {
        const defaultConfig = this.getDefaultConfigData(key);
        this.fileConfigs.set(key, defaultConfig);
        this.lastLoad.set(key, Date.now());
    }

    /**
     * 기본 설정 데이터 반환
     */
    getDefaultConfigData(key) {
        const defaultConfigs = {
            protocols: {
                version: '1.0.0',
                last_updated: new Date().toISOString(),
                items: [
                    {
                        value: 'modbus_tcp',
                        name: 'Modbus TCP',
                        description: 'Modbus TCP/IP protocol',
                        port: 502,
                        config_schema: {
                            ip_address: { type: 'string', required: true },
                            port: { type: 'number', default: 502 },
                            slave_id: { type: 'number', default: 1 },
                            timeout: { type: 'number', default: 3000 }
                        },
                        enabled: true
                    },
                    {
                        value: 'modbus_rtu',
                        name: 'Modbus RTU',
                        description: 'Modbus RTU serial protocol',
                        config_schema: {
                            serial_port: { type: 'string', required: true },
                            baud_rate: { type: 'number', default: 9600 },
                            data_bits: { type: 'number', default: 8 },
                            stop_bits: { type: 'number', default: 1 },
                            parity: { type: 'string', default: 'none' },
                            slave_id: { type: 'number', default: 1 }
                        },
                        enabled: true
                    },
                    {
                        value: 'mqtt',
                        name: 'MQTT',
                        description: 'Message Queuing Telemetry Transport',
                        port: 1883,
                        config_schema: {
                            broker_url: { type: 'string', required: true },
                            port: { type: 'number', default: 1883 },
                            username: { type: 'string' },
                            password: { type: 'string' },
                            client_id: { type: 'string' },
                            topics: { type: 'array', required: true }
                        },
                        enabled: true
                    },
                    {
                        value: 'bacnet',
                        name: 'BACnet',
                        description: 'Building Automation and Control Networks',
                        port: 47808,
                        config_schema: {
                            device_id: { type: 'number', required: true },
                            network_number: { type: 'number', default: 0 },
                            max_apdu: { type: 'number', default: 1476 }
                        },
                        enabled: true
                    }
                ]
            },

            deviceTypes: {
                version: '1.0.0',
                last_updated: new Date().toISOString(),
                items: [
                    { value: 'PLC', name: 'PLC (Programmable Logic Controller)', description: '산업용 제어기', icon: 'fas fa-microchip', category: 'control' },
                    { value: 'HMI', name: 'HMI (Human Machine Interface)', description: '사람-기계 인터페이스', icon: 'fas fa-desktop', category: 'interface' },
                    { value: 'SENSOR', name: 'Sensor', description: '각종 센서', icon: 'fas fa-thermometer-half', category: 'sensor' },
                    { value: 'GATEWAY', name: 'Gateway', description: '프로토콜 게이트웨이', icon: 'fas fa-exchange-alt', category: 'network' },
                    { value: 'METER', name: 'Meter', description: '계측기', icon: 'fas fa-tachometer-alt', category: 'measurement' },
                    { value: 'CONTROLLER', name: 'Controller', description: '제어기', icon: 'fas fa-cogs', category: 'control' },
                    { value: 'DRIVE', name: 'Drive', description: '모터 드라이브', icon: 'fas fa-bolt', category: 'power' },
                    { value: 'ROBOT', name: 'Robot', description: '로봇', icon: 'fas fa-robot', category: 'automation' },
                    { value: 'CAMERA', name: 'Camera', description: '비전 카메라', icon: 'fas fa-camera', category: 'vision' },
                    { value: 'OTHER', name: 'Other', description: '기타', icon: 'fas fa-question', category: 'general' }
                ]
            },

            deviceStatus: {
                version: '1.0.0',
                last_updated: new Date().toISOString(),
                items: [
                    { value: 'connected', name: 'Connected', description: '정상 연결됨', color: 'green', icon: 'fas fa-check-circle' },
                    { value: 'disconnected', name: 'Disconnected', description: '연결 끊어짐', color: 'red', icon: 'fas fa-times-circle' },
                    { value: 'error', name: 'Error', description: '오류 상태', color: 'red', icon: 'fas fa-exclamation-triangle' },
                    { value: 'maintenance', name: 'Maintenance', description: '유지보수 모드', color: 'yellow', icon: 'fas fa-wrench' },
                    { value: 'offline', name: 'Offline', description: '오프라인', color: 'gray', icon: 'fas fa-power-off' }
                ]
            },

            alarmTypes: {
                version: '1.0.0',
                last_updated: new Date().toISOString(),
                items: [
                    { value: 'critical', name: 'Critical', description: '심각한 알람', priority: 1, color: 'red', auto_notify: true },
                    { value: 'high', name: 'High', description: '높은 우선순위', priority: 2, color: 'orange', auto_notify: true },
                    { value: 'medium', name: 'Medium', description: '중간 우선순위', priority: 3, color: 'yellow', auto_notify: false },
                    { value: 'low', name: 'Low', description: '낮은 우선순위', priority: 4, color: 'blue', auto_notify: false },
                    { value: 'info', name: 'Information', description: '정보성 알람', priority: 5, color: 'gray', auto_notify: false }
                ]
            },

            userRoles: {
                version: '1.0.0',
                last_updated: new Date().toISOString(),
                items: [
                    { 
                        value: 'system_admin', 
                        name: 'System Administrator', 
                        description: '시스템 최고 관리자',
                        permissions: ['*'],
                        hierarchy_level: 0
                    },
                    { 
                        value: 'company_admin', 
                        name: 'Company Administrator', 
                        description: '회사 관리자',
                        permissions: ['manage_company', 'manage_users', 'manage_sites', 'manage_devices', 'view_all_data'],
                        hierarchy_level: 1
                    },
                    { 
                        value: 'site_admin', 
                        name: 'Site Administrator', 
                        description: '사이트 관리자',
                        permissions: ['manage_site', 'manage_devices', 'manage_alarms', 'view_site_data'],
                        hierarchy_level: 2
                    },
                    { 
                        value: 'engineer', 
                        name: 'Engineer', 
                        description: '엔지니어',
                        permissions: ['manage_devices', 'manage_data_points', 'view_data', 'control_devices'],
                        hierarchy_level: 3
                    },
                    { 
                        value: 'operator', 
                        name: 'Operator', 
                        description: '운영자',
                        permissions: ['view_data', 'acknowledge_alarms'],
                        hierarchy_level: 4
                    },
                    { 
                        value: 'viewer', 
                        name: 'Viewer', 
                        description: '조회 전용',
                        permissions: ['view_data'],
                        hierarchy_level: 5
                    }
                ]
            },

            siteTypes: {
                version: '1.0.0',
                last_updated: new Date().toISOString(),
                items: [
                    { value: 'company', name: 'Company', description: '회사 본부', hierarchy_level: 0, icon: 'fas fa-building' },
                    { value: 'factory', name: 'Factory', description: '공장', hierarchy_level: 1, icon: 'fas fa-industry' },
                    { value: 'office', name: 'Office', description: '사무소', hierarchy_level: 1, icon: 'fas fa-building' },
                    { value: 'building', name: 'Building', description: '건물', hierarchy_level: 2, icon: 'fas fa-building' },
                    { value: 'floor', name: 'Floor', description: '층', hierarchy_level: 3, icon: 'fas fa-layer-group' },
                    { value: 'line', name: 'Line', description: '생산라인', hierarchy_level: 4, icon: 'fas fa-project-diagram' },
                    { value: 'area', name: 'Area', description: '구역', hierarchy_level: 5, icon: 'fas fa-map-marked-alt' },
                    { value: 'zone', name: 'Zone', description: '영역', hierarchy_level: 6, icon: 'fas fa-map-pin' }
                ]
            }
        };

        return defaultConfigs[key] || { version: '1.0.0', items: [] };
    }

    // ========================================================================
    // 정보 조회 메서드들
    // ========================================================================

    /**
     * 모든 설정 정보 조회
     */
    async getAllConfigs() {
        const configs = {};
        
        for (const key of Object.keys(this.configPaths)) {
            configs[key] = await this.getConfig(key);
        }
        
        return configs;
    }

    /**
     * 설정 상태 정보
     */
    getConfigStatus() {
        return {
            file_configs: Array.from(this.fileConfigs.keys()),
            db_configs: Array.from(this.dbConfigs.keys()),
            last_loads: Object.fromEntries(this.lastLoad),
            cache_timeout: this.cacheTimeout
        };
    }
}

module.exports = JsonConfigManager;