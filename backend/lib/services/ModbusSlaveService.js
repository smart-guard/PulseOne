// =============================================================================
// backend/lib/services/ModbusSlaveService.js
// export-gateway와 동일한 서비스 레이어 패턴
// =============================================================================

const BaseService = require('./BaseService');
const ProcessService = require('./ProcessService');
const LogManager = require('../utils/LogManager');
const KnexManager = require('../database/KnexManager');

class ModbusSlaveService extends BaseService {
    constructor() {
        super(null);
    }

    get knex() {
        return KnexManager.getInstance().getKnex();
    }

    // =========================================================================
    // 디바이스 CRUD
    // =========================================================================

    /**
     * 모든 Modbus Slave 디바이스 목록 조회 (tenant 격리)
     */
    async getAllDevices(tenantId, siteId = null) {
        return await this.handleRequest(async () => {
            let query = this.knex('modbus_slave_devices as msd')
                .join('sites as s', 'msd.site_id', 's.id')
                .where('s.tenant_id', tenantId)
                .select(
                    'msd.*',
                    's.name as site_name',
                    this.knex.raw('(SELECT COUNT(*) FROM modbus_slave_mappings msm WHERE msm.device_id = msd.id AND msm.enabled = 1) as mapping_count')
                )
                .orderBy('msd.id');

            if (siteId) {
                query = query.where('msd.site_id', siteId);
            }

            const devices = await query;

            // Docker 환경에서는 TCP 포트 연결로 상태 확인 (cross-container ps 불가)
            const isDocker = process.env.DOCKER_CONTAINER === 'true';

            if (isDocker) {
                // 모든 디바이스의 포트 상태를 병렬로 확인
                const portChecks = await Promise.all(
                    devices.map(dev => this._checkTcpPort('modbus-slave', dev.tcp_port || 502))
                );
                return devices.map((dev, i) => ({
                    ...dev,
                    process_status: portChecks[i] ? 'running' : 'stopped',
                    pid: null
                }));
            }

            // 네이티브 환경: 기존 프로세스 감지 방식
            const runningProcesses = await ProcessService.getAllStatus();
            const modbusProcesses = (runningProcesses?.processes || []).filter(p =>
                p.name === 'modbus-slave' || p.name?.startsWith('modbus-slave-')
            );

            return devices.map(dev => {
                const proc = modbusProcesses.find(p => p.deviceId === dev.id);
                return {
                    ...dev,
                    process_status: proc ? 'running' : 'stopped',
                    pid: proc?.pid || null
                };
            });
        }, 'GetAllModbusSlaveDevices');
    }

    /**
     * TCP 포트 오픈 여부 확인
     */
    _checkTcpPort(host, port, timeout = 1500) {
        const net = require('net');
        return new Promise((resolve) => {
            const socket = new net.Socket();
            let resolved = false;
            const timer = setTimeout(() => {
                if (!resolved) { resolved = true; socket.destroy(); resolve(false); }
            }, timeout);
            socket.connect(port, host, () => {
                if (!resolved) { resolved = true; socket.end(); clearTimeout(timer); resolve(true); }
            });
            socket.on('error', () => {
                if (!resolved) { resolved = true; socket.destroy(); clearTimeout(timer); resolve(false); }
            });
        });
    }


    /**
     * 단일 디바이스 조회
     */
    async getDeviceById(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.knex('modbus_slave_devices as msd')
                .join('sites as s', 'msd.site_id', 's.id')
                .where({ 'msd.id': id, 's.tenant_id': tenantId })
                .select('msd.*', 's.name as site_name')
                .first();

            if (!device) throw new Error('Modbus Slave 디바이스를 찾을 수 없습니다');
            return device;
        }, 'GetModbusSlaveDeviceById');
    }

    /**
     * 디바이스 등록
     */
    async createDevice(data, tenantId) {
        return await this.handleRequest(async () => {
            // 포트 중복 체크 (사이트 내)
            const existing = await this.knex('modbus_slave_devices')
                .where({ site_id: data.site_id, tcp_port: data.tcp_port })
                .first();

            if (existing) {
                const err = new Error(`사이트 내 포트 ${data.tcp_port}가 이미 사용 중입니다`);
                err.statusCode = 409;
                throw err;
            }

            const [id] = await this.knex('modbus_slave_devices').insert({
                tenant_id: tenantId,
                site_id: data.site_id,
                name: data.name,
                tcp_port: data.tcp_port || 502,
                unit_id: data.unit_id || 1,
                max_clients: data.max_clients || 10,
                enabled: data.enabled !== undefined ? data.enabled : 1,
                description: data.description || null
            });

            LogManager.api('INFO', `Modbus Slave 디바이스 등록: ${data.name} (포트 ${data.tcp_port})`);

            return await this.getDeviceById(id, tenantId);
        }, 'CreateModbusSlaveDevice');
    }

    /**
     * 디바이스 수정
     */
    async updateDevice(id, data, tenantId) {
        return await this.handleRequest(async () => {
            // tenant 소유권 확인
            await this.getDeviceById(id, tenantId);

            const updateData = {};
            if (data.name !== undefined) updateData.name = data.name;
            if (data.tcp_port !== undefined) updateData.tcp_port = data.tcp_port;
            if (data.unit_id !== undefined) updateData.unit_id = data.unit_id;
            if (data.max_clients !== undefined) updateData.max_clients = data.max_clients;
            if (data.enabled !== undefined) updateData.enabled = data.enabled;
            if (data.description !== undefined) updateData.description = data.description;
            updateData.updated_at = this.knex.raw("datetime('now', 'localtime')");

            await this.knex('modbus_slave_devices').where('id', id).update(updateData);

            LogManager.api('INFO', `Modbus Slave 디바이스 수정: ID ${id}`);
            return await this.getDeviceById(id, tenantId);
        }, 'UpdateModbusSlaveDevice');
    }

    /**
     * 디바이스 삭제 (매핑도 CASCADE)
     */
    async deleteDevice(id, tenantId) {
        return await this.handleRequest(async () => {
            await this.getDeviceById(id, tenantId); // 소유권 확인

            await this.knex('modbus_slave_devices').where('id', id).delete();

            LogManager.api('INFO', `Modbus Slave 디바이스 삭제: ID ${id}`);
            return { success: true };
        }, 'DeleteModbusSlaveDevice');
    }

    // =========================================================================
    // 매핑 관리 (device_id 기준)
    // =========================================================================

    /**
     * 디바이스의 레지스터 매핑 목록
     */
    async getMappings(deviceId, tenantId) {
        return await this.handleRequest(async () => {
            // 소유권 확인
            await this.getDeviceById(deviceId, tenantId);

            return await this.knex('modbus_slave_mappings as msm')
                .leftJoin('data_points as dp', 'msm.point_id', 'dp.id')
                .where('msm.device_id', deviceId)
                .select(
                    'msm.*',
                    'dp.name as point_name',
                    'dp.data_type as point_type',
                    'dp.unit'
                )
                .orderBy(['msm.register_type', 'msm.register_address']);
        }, 'GetModbusSlaveMappings');
    }

    /**
     * 매핑 저장 (전체 교체)
     */
    async saveMappings(deviceId, mappings, tenantId) {
        return await this.handleRequest(async () => {
            await this.getDeviceById(deviceId, tenantId);

            return await this.knex.transaction(async (trx) => {
                // 기존 매핑 전체 삭제
                await trx('modbus_slave_mappings').where('device_id', deviceId).delete();

                if (!mappings || mappings.length === 0) return [];

                // 신규 매핑 삽입
                const rows = mappings.map(m => ({
                    device_id: deviceId,
                    point_id: m.point_id,
                    register_type: m.register_type || 'HR',
                    register_address: m.register_address,
                    data_type: m.data_type || 'FLOAT32',
                    byte_order: m.byte_order || 'big_endian',
                    scale_factor: m.scale_factor || 1.0,
                    scale_offset: m.scale_offset || 0.0,
                    enabled: m.enabled !== undefined ? m.enabled : 1
                }));

                await trx('modbus_slave_mappings').insert(rows);

                LogManager.api('INFO', `Modbus Slave 매핑 저장: device=${deviceId}, count=${rows.length}`);
                return rows;
            });
        }, 'SaveModbusSlaveMappings');
    }

    // =========================================================================
    // 프로세스 제어 (start / stop / restart)
    // =========================================================================

    async startDevice(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.getDeviceById(id, tenantId);
            if (!device) throw new Error('Modbus Slave 디바이스를 찾을 수 없습니다');

            // enabled=1 확인 (중지된 상태에서 start 요청 시 enabled 활성화)
            if (!device.enabled) {
                await this.knex('modbus_slave_devices').where('id', id).update({ enabled: 1 });
            }

            return await ProcessService.controlProcess(`modbus-slave-${id}`, 'start');
        }, 'StartModbusSlaveDevice');
    }

    async stopDevice(id, tenantId) {
        return await this.handleRequest(async () => {
            await this.getDeviceById(id, tenantId);
            return await ProcessService.controlProcess(`modbus-slave-${id}`, 'stop');
        }, 'StopModbusSlaveDevice');
    }

    async restartDevice(id, tenantId) {
        return await this.handleRequest(async () => {
            await this.getDeviceById(id, tenantId);
            return await ProcessService.controlProcess(`modbus-slave-${id}`, 'restart');
        }, 'RestartModbusSlaveDevice');
    }

    // =========================================================================
    // 통계 조회 (향후 Redis에서 실시간 통계 읽기)
    // =========================================================================

    async getDeviceStats(id, tenantId) {
        return await this.handleRequest(async () => {
            await this.getDeviceById(id, tenantId);

            // TODO: Redis에서 실시간 통계 읽기 (SlaveStatistics가 Redis에 기록)
            // 현재는 프로세스 상태만 반환
            const runningProcesses = await ProcessService.getAllStatus();
            const proc = (runningProcesses?.processes || []).find(p => p.deviceId === id);

            return {
                device_id: id,
                process_status: proc ? 'running' : 'stopped',
                pid: proc?.pid || null,
                uptime: proc?.uptime || 'N/A',
                memory: proc?.memoryUsage || 'N/A',
                cpu: proc?.cpuUsage || 'N/A',
                // 향후 추가: connected_clients, requests_total, success_rate
            };
        }, 'GetModbusSlaveDeviceStats');
    }
}

module.exports = new ModbusSlaveService();
