/**
 * backend/lib/services/DeviceService.js
 * 디바이스 관련 비즈니스 로직 처리 서비스
 */

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

class DeviceService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getRepository('DeviceRepository');
        }
        return this._repository;
    }

    get collectorProxy() {
        if (!this._collectorProxy) {
            this._collectorProxy = require('./CollectorProxyService').getInstance();
        }
        return this._collectorProxy;
    }

    get configSyncHooks() {
        if (!this._configSyncHooks) {
            this._configSyncHooks = require('../hooks/ConfigSyncHooks').getInstance();
        }
        return this._configSyncHooks;
    }

    /**
     * 디바이스 트리 구조 데이터 반환
     */
    async getDeviceTree(options) {
        return await this.handleRequest(async () => {
            const tenantId = options.tenantId || 1;

            // 1. 모든 디바이스 조회
            const devicesResult = await this.repository.findAll(tenantId, {
                includeCount: true,
                page: 1,
                limit: 1000
            });

            let devices = devicesResult.items || [];
            devices = this.enhanceDevicesWithRtuInfo(devices);
            devices = await this.addRtuRelationships(devices, tenantId);

            // 2. 트리 데이터 구성 (기존 route/devices.js의 buildTreeData 로직)
            return this.buildTreeData(devices, options);
        }, 'GetDeviceTree');
    }

    buildTreeData(devices, options) {
        const { includeDataPoints = false } = options;

        // Site별로 그룹화
        const sitesMap = new Map();

        devices.forEach(device => {
            const siteId = device.site_id || 0;
            const siteName = device.site_name || 'Unassigned Site';

            if (!sitesMap.has(siteId)) {
                sitesMap.set(siteId, {
                    id: siteId,
                    name: siteName,
                    type: 'site',
                    children: []
                });
            }

            const siteNode = sitesMap.get(siteId);
            const deviceNode = {
                id: device.id,
                name: device.name,
                type: 'device',
                protocol: device.protocol_type,
                device_type: device.device_type,
                status: device.connection_status || 'unknown',
                is_enabled: device.is_enabled,
                rtu_info: device.rtu_info,
                children: [] // 데이터 포인트 등이 들어갈 수 있음
            };

            siteNode.children.push(deviceNode);
        });

        return Array.from(sitesMap.values());
    }

    /**
     * 디바이스 목록 조회 (RTU 및 Collector 상태 포함)
     */
    async getDevices(options) {
        return await this.handleRequest(async () => {
            const result = await this.repository.findAll(options.tenantId, {
                ...options,
                includeCount: true
            });

            // RTU 정보 추가 및 가공 로직 (기존 route에서 이동)
            if (result && result.items) {
                result.items = this.enhanceDevicesWithRtuInfo(result.items);

                if (options.includeRtuRelations) {
                    result.items = await this.addRtuRelationships(result.items, options.tenantId);
                }

                if (options.includeCollectorStatus) {
                    await this.enrichWithCollectorStatus(result.items);
                }

                // RTU 통계 추가
                result.rtu_summary = this.calculateRtuSummary(result.items);
            }

            return result;
        }, 'GetDevices');
    }

    calculateRtuSummary(items) {
        const rtuDevices = items.filter(d => d.protocol_type === 'MODBUS_RTU');
        const rtuMasters = rtuDevices.filter(d => d.rtu_info && d.rtu_info.is_master);
        const rtuSlaves = rtuDevices.filter(d => d.rtu_info && d.rtu_info.is_slave);

        return {
            total_rtu_devices: rtuDevices.length,
            rtu_masters: rtuMasters.length,
            rtu_slaves: rtuSlaves.length,
            rtu_networks: rtuMasters.map(master => ({
                master_id: master.id,
                master_name: master.name,
                serial_port: master.endpoint,
                baud_rate: master.rtu_info ? master.rtu_info.baud_rate : null,
                slave_count: master.rtu_info ? master.rtu_info.slave_count : 0,
                connection_status: master.connection_status
            }))
        };
    }

    /**
     * 디바이스 상세 조회
     */
    async getDeviceById(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.repository.findById(id, tenantId);
            if (!device) throw new Error('Device not found');

            return this.enhanceDeviceWithRtuInfo(device);
        }, 'GetDeviceById');
    }

    /**
     * 디바이스 생성 및 후크 실행
     */
    async createDevice(deviceData, tenantId) {
        return await this.handleRequest(async () => {
            const newDevice = await this.repository.create(deviceData, tenantId);

            // 설정 동기화 후크 실행
            await this.configSyncHooks.afterDeviceCreate(newDevice);

            return newDevice;
        }, 'CreateDevice');
    }

    /**
     * 디바이스 업데이트 및 동기화
     */
    async updateDevice(id, updateData, tenantId) {
        return await this.handleRequest(async () => {
            const oldDevice = await this.repository.findById(id, tenantId);
            if (!oldDevice) throw new Error('Device not found');

            const updatedDevice = await this.repository.update(id, updateData, tenantId);

            // 설정 동기화 후크 실행
            await this.configSyncHooks.afterDeviceUpdate(oldDevice, updatedDevice);

            return updatedDevice;
        }, 'UpdateDevice');
    }

    /**
     * 디바이스 삭제
     */
    async delete(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.repository.findById(id, tenantId);
            if (!device) throw new Error('Device not found');

            const result = await this.repository.deleteById(id, tenantId);

            // 동기화 후크 실행 (삭제 후 처리)
            await this.configSyncHooks.afterDeviceDelete(device);

            return result;
        }, 'DeleteDevice');
    }

    /**
     * Collector 액션 실행 (Start/Stop/Restart)
     */
    async executeAction(id, action, options = {}, tenantId) {
        return await this.handleRequest(async () => {
            // 권한 및 존재 확인
            const device = await this.repository.findById(id, tenantId);
            if (!device) throw new Error('Device not found or access denied');

            let result;
            switch (action) {
                case 'start':
                    result = await this.collectorProxy.startDevice(id.toString(), options);
                    break;
                case 'stop':
                    result = await this.collectorProxy.stopDevice(id.toString(), options);
                    break;
                case 'restart':
                    result = await this.collectorProxy.restartDevice(id.toString(), options);
                    break;
                default:
                    throw new Error(`Invalid action: ${action}`);
            }

            return result;
        }, `ExecuteAction:${action}`);
    }

    // --- Helper Methods (기존 route/devices.js의 유틸리티들을 서비스로 이동) ---

    enhanceDeviceWithRtuInfo(device) {
        if (!device) return device;

        let config = device.config;
        if (typeof device.config === 'string' && device.config.trim() !== '') {
            try {
                config = JSON.parse(device.config);
            } catch (error) {
                this.logger.warn(`Device ${device.id}: Config parsing failed:`, error.message);
                config = {};
            }
        } else if (!device.config) {
            config = {};
        }

        const enhanced = {
            ...device,
            config: config,
            rtu_info: null
        };

        if (device.protocol_type === 'MODBUS_RTU') {
            enhanced.rtu_info = {
                slave_id: config.slave_id || null,
                master_device_id: config.master_device_id || null,
                baud_rate: config.baud_rate || 9600,
                data_bits: config.data_bits || 8,
                stop_bits: config.stop_bits || 1,
                parity: config.parity || 'N',
                frame_delay_ms: config.frame_delay_ms || null,
                response_timeout_ms: config.response_timeout_ms || 1000,
                is_master: device.device_type === 'GATEWAY',
                is_slave: device.device_type !== 'GATEWAY' && config.master_device_id,
                serial_port: device.endpoint,
                network_info: {
                    protocol: 'Modbus RTU',
                    connection_type: 'Serial',
                    port: device.endpoint
                }
            };
        }

        return enhanced;
    }

    enhanceDevicesWithRtuInfo(devices) {
        if (!Array.isArray(devices)) return devices;
        return devices.map(device => this.enhanceDeviceWithRtuInfo(device));
    }

    async addRtuRelationships(devices, tenantId) {
        if (!Array.isArray(devices)) return devices;

        const rtuMasters = devices.filter(d =>
            d.protocol_type === 'MODBUS_RTU' && d.device_type === 'GATEWAY'
        );

        const rtuSlaves = devices.filter(d =>
            d.protocol_type === 'MODBUS_RTU' && d.device_type !== 'GATEWAY'
        );

        for (const master of rtuMasters) {
            const slaves = rtuSlaves.filter(slave => {
                const slaveConfig = slave.rtu_info;
                return slaveConfig && slaveConfig.master_device_id === master.id;
            });

            if (master.rtu_info) {
                master.rtu_info.slave_count = slaves.length;
                master.rtu_info.slaves = slaves.map(slave => ({
                    device_id: slave.id,
                    device_name: slave.name,
                    slave_id: slave.rtu_info ? slave.rtu_info.slave_id : null,
                    device_type: slave.device_type,
                    connection_status: slave.connection_status,
                    manufacturer: slave.manufacturer,
                    model: slave.model
                }));
            }
        }

        return devices;
    }

    async enrichWithCollectorStatus(items) {
        try {
            const workerResult = await this.collectorProxy.getWorkerStatus();
            const statuses = workerResult.data?.workers || {};
            items.forEach(d => {
                d.collector_status = statuses[d.id.toString()] || { status: 'unknown' };
            });
        } catch (e) {
            this.logger.warn('Collector status enrichment failed:', e.message);
        }
    }
}

module.exports = new DeviceService();
