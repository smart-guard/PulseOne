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
                .select(
                    'msd.*',
                    's.name as site_name',
                    this.knex.raw('(SELECT COUNT(*) FROM modbus_slave_mappings msm WHERE msm.device_id = msd.id AND msm.enabled = 1) as mapping_count'),
                    this.knex.raw('(SELECT MAX(recorded_at) FROM modbus_slave_access_logs mal WHERE mal.device_id = msd.id) as last_activity')
                )
                .where(this.knex.raw('(msd.is_deleted IS NULL OR msd.is_deleted = 0)'))
                .orderBy('msd.id');

            // tenantId=null → System Admin: 전체 테넌트 조회 (필터 없음)
            if (tenantId !== null && tenantId !== undefined) {
                query = query.where('s.tenant_id', tenantId);
            }

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
            let query = this.knex('modbus_slave_devices as msd')
                .join('sites as s', 'msd.site_id', 's.id')
                .where('msd.id', id)
                .select('msd.*', 's.name as site_name');

            // tenantId=null → System Admin: 소유권 체크 없이 id만으로 조회
            if (tenantId !== null && tenantId !== undefined) {
                query = query.where('s.tenant_id', tenantId);
            }

            const device = await query.first();
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
                description: data.description || null,
                packet_logging: data.packet_logging !== undefined ? (data.packet_logging ? 1 : 0) : 0,
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
            if (data.packet_logging !== undefined)
                updateData.packet_logging = data.packet_logging ? 1 : 0;
            updateData.updated_at = this.knex.fn.now();

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

            // 소프트 삭제: enabled=0 (Supervisor가 Worker 종료), is_deleted=1 (UI에서 숨김)
            // 이력 데이터(access_logs, mappings)는 그대로 보존됨
            await this.knex('modbus_slave_devices').where('id', id).update({
                is_deleted: 1,
                enabled: 0,
                updated_at: this.knex.fn.now()
            });

            LogManager.api('INFO', `Modbus Slave 디바이스 소프트 삭제: ID ${id}`);
            return { success: true };
        }, 'DeleteModbusSlaveDevice');
    }

    /**
     * 삭제된 디바이스 복구 (is_deleted=0, enabled=1)
     */
    async restoreDevice(id, tenantId) {
        return await this.handleRequest(async () => {
            // 삭제된 것 포함 조회 (tenantId 소유권 확인용)
            const query = this.knex('modbus_slave_devices as msd')
                .join('sites as s', 'msd.site_id', 's.id')
                .where('msd.id', id)
                .where('msd.is_deleted', 1)
                .first();
            if (tenantId !== null && tenantId !== undefined) {
                query.where('s.tenant_id', tenantId);
            }
            const device = await query;
            if (!device) throw new Error('삭제된 디바이스를 찾을 수 없습니다');

            await this.knex('modbus_slave_devices').where('id', id).update({
                is_deleted: 0,
                enabled: 1,
                updated_at: this.knex.fn.now()
            });

            LogManager.api('INFO', `Modbus Slave 디바이스 복구: ID ${id}`);
            return { success: true, message: `디바이스 ID ${id} 복구 완료` };
        }, 'RestoreModbusSlaveDevice');
    }

    /**
     * 소프트 삭제된 디바이스 목록 조회
     */
    async getDeletedDevices(tenantId, siteId = null) {
        return await this.handleRequest(async () => {
            let query = this.knex('modbus_slave_devices as msd')
                .join('sites as s', 'msd.site_id', 's.id')
                .select('msd.*', 's.name as site_name')
                .where('msd.is_deleted', 1)
                .orderBy('msd.updated_at', 'desc');

            if (tenantId !== null && tenantId !== undefined) {
                query = query.where('s.tenant_id', tenantId);
            }
            if (siteId) {
                query = query.where('msd.site_id', siteId);
            }
            return await query;
        }, 'GetDeletedModbusSlaveDevices');
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
     * 디바이스의 레지스터 매핑별 현재 값 조회 (current_values JOIN)
     * - value: Collector가 수집한 공학단위 값 (그대로 표시)
     * - register_raw: Modbus 레지스터에 실제 기록되는 raw 값 = value * scale_factor + offset
     */
    async getRegisterValues(deviceId, tenantId) {
        return await this.handleRequest(async () => {
            await this.getDeviceById(deviceId, tenantId);

            const rows = await this.knex('modbus_slave_mappings as msm')
                .leftJoin('current_values as cv', 'msm.point_id', 'cv.id')
                .leftJoin('data_points as dp', 'msm.point_id', 'dp.id')
                .where('msm.device_id', deviceId)
                .select(
                    'msm.point_id',
                    'msm.register_type',
                    'msm.register_address',
                    'msm.data_type',
                    'msm.scale_factor',
                    'msm.scale_offset',
                    'dp.name as point_name',
                    'cv.current_value as raw_value',
                    'cv.quality',
                    'cv.value_timestamp'
                )
                .orderBy(['msm.register_type', 'msm.register_address']);

            // current_value 컬럼은 JSON 문자열 → 파싱
            // current_values에 저장된 값은 이미 공학단위 값 — 스케일 중복 적용 금지
            return rows.map(r => {
                let value = null;
                let register_raw = null;
                try {
                    if (r.raw_value) {
                        const parsed = JSON.parse(r.raw_value);
                        value = parsed.value ?? null;
                        // Modbus 레지스터 raw 값 계산 (SCADA Master가 역산하는 값)
                        if (typeof value === 'number' && (r.scale_factor || 1) !== 0) {
                            register_raw = value * (r.scale_factor || 1) + (r.scale_offset || 0);
                        }
                    }
                } catch { /* ignore */ }
                return {
                    point_id: r.point_id,
                    register_type: r.register_type,
                    register_address: r.register_address,
                    data_type: r.data_type,
                    point_name: r.point_name,
                    value,           // 공학단위 값 (Collector 수집값)
                    register_raw,    // Modbus 레지스터 기록값 (= value * factor + offset)
                    quality: r.quality,
                    value_timestamp: r.value_timestamp,
                };
            });
        }, 'GetModbusSlaveRegisterValues');
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

            // DB enabled=1 설정 → C++ Supervisor가 다음 Reconcile 주기에 Worker 시작
            await this.knex('modbus_slave_devices').where('id', id).update({
                enabled: 1,
                updated_at: this.knex.fn.now()
            });

            const isDocker = process.env.DOCKER_CONTAINER === 'true';
            if (!isDocker) {
                // 네이티브 환경에서만 ProcessService 사용
                await ProcessService.controlProcess(`modbus-slave-${id}`, 'start').catch(() => { });
            }

            return { success: true, message: `디바이스 ${id} 시작 요청 완료 (Supervisor가 곧 처리합니다)` };
        }, 'StartModbusSlaveDevice');
    }

    async stopDevice(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.getDeviceById(id, tenantId);
            if (!device) throw new Error('Modbus Slave 디바이스를 찾을 수 없습니다');

            // DB enabled=0 설정 → C++ Supervisor가 다음 Reconcile 주기에 Worker 종료
            await this.knex('modbus_slave_devices').where('id', id).update({
                enabled: 0,
                updated_at: this.knex.fn.now()
            });

            const isDocker = process.env.DOCKER_CONTAINER === 'true';
            if (!isDocker) {
                await ProcessService.controlProcess(`modbus-slave-${id}`, 'stop').catch(() => { });
            }

            return { success: true, message: `디바이스 ${id} 중지 요청 완료 (Supervisor가 곧 처리합니다)` };
        }, 'StopModbusSlaveDevice');
    }

    async restartDevice(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.getDeviceById(id, tenantId);
            if (!device) throw new Error('Modbus Slave 디바이스를 찾을 수 없습니다');

            // 재시작: enabled=0 → 잠시 대기 → enabled=1
            await this.knex('modbus_slave_devices').where('id', id).update({
                enabled: 0,
                updated_at: this.knex.fn.now()
            });

            // Supervisor Reconcile 주기(~60s)가 있으므로 즉시 재활성화하면 stop이 먼저 적용됨
            // 3초 후 enabled=1로 복원 (Supervisor가 stop 후 재시작 처리)
            setTimeout(async () => {
                try {
                    await this.knex('modbus_slave_devices').where('id', id).update({
                        enabled: 1,
                        updated_at: this.knex.fn.now()
                    });
                } catch (e) {
                    console.error(`[ModbusSlave] Restart re-enable 실패 device=${id}:`, e.message);
                }
            }, 3000);

            const isDocker = process.env.DOCKER_CONTAINER === 'true';
            if (!isDocker) {
                await ProcessService.controlProcess(`modbus-slave-${id}`, 'restart').catch(() => { });
            }

            return { success: true, message: `디바이스 ${id} 재시작 요청 완료 (Supervisor가 곧 처리합니다)` };
        }, 'RestartModbusSlaveDevice');
    }


    // =========================================================================
    // 통계 조회 (향후 Redis에서 실시간 통계 읽기)
    // =========================================================================

    async getDeviceStats(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.getDeviceById(id, tenantId);

            // 프로세스 상태 확인
            const isDocker = process.env.DOCKER_CONTAINER === 'true';
            let proc = null;
            if (isDocker) {
                // device.tcp_port 로 실제 Worker 것인지 정확히 확인
                const alive = await this._checkTcpPort('modbus-slave', device.tcp_port || 502);
                proc = alive ? { pid: null, uptime: 'N/A', memoryUsage: 'N/A', cpuUsage: 'N/A' } : null;
            } else {
                const runningProcesses = await ProcessService.getAllStatus();
                proc = (runningProcesses?.processes || []).find(p => p.deviceId === parseInt(id));
            }

            // Redis에서 C++ Worker가 게시한 통계 읽기
            let slaveStats = null;
            let clientData = null;
            try {
                const RedisManager = require('../database/RedisManager');
                const redisClient = RedisManager.getInstance().getClient();
                if (redisClient) {
                    const [statsJson, clientsJson] = await Promise.all([
                        redisClient.get(`modbus:stats:${id}`),
                        redisClient.get(`modbus:clients:${id}`)
                    ]);
                    if (statsJson) {
                        try { slaveStats = JSON.parse(statsJson); } catch { slaveStats = null; }
                    }
                    if (clientsJson) {
                        try { clientData = JSON.parse(clientsJson); } catch { clientData = null; }
                    }
                }
            } catch (e) {
                LogManager.api('WARN', `Modbus Slave Redis 통계 읽기 실패 (device=${id})`, { error: e.message });
            }

            // Redis 데이터가 있으면(=Worker 실행 중) running으로 보완
            const processStatus = (proc || slaveStats) ? 'running' : 'stopped';

            // 클라이언트 세션이 있으면 DB에 스냅샷 기록 (비동기, 실패 무시)
            // → Communication Log 탭과 last_activity 카드가 실제 데이터를 표시할 수 있게 됨
            if (clientData?.sessions?.length > 0) {
                this.snapshotAccessLogs(id, null).catch(() => { });
            }

            return {
                device_id: parseInt(id),
                process_status: processStatus,
                pid: proc?.pid || null,
                uptime: proc?.uptime || 'N/A',
                memory: proc?.memoryUsage || 'N/A',
                cpu: proc?.cpuUsage || 'N/A',
                slave_stats: slaveStats,
                clients: clientData,
            };
        }, 'GetModbusSlaveDeviceStats');
    }
    // =========================================================================
    // Modbus Slave 통신 이력
    // =========================================================================

    /**
     * Redis에서 클라이언트 스냅샷을 읽어 DB에 삽입
     * 백엔드 cron 또는 getDeviceStats 호출 시 자동 실행
     */
    async snapshotAccessLogs(deviceId, tenantId) {
        try {
            const RedisManager = require('../database/RedisManager');
            const redisClient = RedisManager.getInstance().getClient();
            if (!redisClient) return;

            const clientsJson = await redisClient.get(`modbus:clients:${deviceId}`);
            if (!clientsJson) return;

            let clientData;
            try { clientData = JSON.parse(clientsJson); } catch { return; }

            const sessions = clientData?.sessions || [];
            if (sessions.length === 0) return;

            const now = new Date().toISOString();
            const periodStart = new Date(Date.now() - 30000).toISOString(); // 30초 전

            const rows = sessions.map(sess => ({
                device_id: deviceId,
                tenant_id: tenantId || null,
                client_ip: sess.ip,
                client_port: sess.port || null,
                unit_id: null,
                period_start: periodStart,
                period_end: now,
                total_requests: sess.requests || 0,
                failed_requests: sess.failures || 0,
                fc01_count: sess.fc01_count || 0,
                fc02_count: sess.fc02_count || 0,
                fc03_count: sess.fc03_count || 0,
                fc04_count: sess.fc04_count || 0,
                fc05_count: sess.fc05_count || 0,
                fc06_count: sess.fc06_count || 0,
                fc15_count: sess.fc15_count || 0,
                fc16_count: sess.fc16_count || 0,
                avg_response_us: sess.avg_response_us || 0,
                duration_sec: sess.duration_sec || 0,
                success_rate: sess.success_rate ?? 1.0,
                is_active: 1,
            }));

            if (rows.length > 0) {
                await this.knex('modbus_slave_access_logs').insert(rows);
            }
        } catch (e) {
            LogManager.api('WARN', `Modbus Slave 이력 스냅샷 실패 (device=${deviceId})`, { error: e.message });
        }
    }

    /**
     * 통신 이력 목록 조회 (페이지네이션 + 필터)
     */
    async getAccessLogs(tenantId, { deviceId, clientIp, dateFrom, dateTo, page = 1, limit = 50 } = {}) {
        return await this.handleRequest(async () => {
            // l.tenant_id 기준 필터 (access_logs 자체에 tenant_id 보존)
            // leftJoin은 device_name 조회 전용 — tenant 필터를 JOIN 결과에 걸면 로그 누락 가능
            let query = this.knex('modbus_slave_access_logs as l')
                .leftJoin('modbus_slave_devices as d', 'l.device_id', 'd.id')
                .where(function () {
                    this.where('l.tenant_id', tenantId).orWhereNull('l.tenant_id');
                });

            if (deviceId) query = query.where('l.device_id', parseInt(deviceId));
            if (clientIp) query = query.where('l.client_ip', clientIp);
            if (dateFrom) query = query.where('l.recorded_at', '>=', dateFrom);
            if (dateTo) query = query.where('l.recorded_at', '<=', dateTo);

            const totalResult = await query.clone().count('l.id as cnt').first();
            const total = parseInt(totalResult?.cnt || 0);

            const offset = (page - 1) * limit;
            const items = await query
                .orderBy('l.recorded_at', 'desc')
                .select(
                    'l.*',
                    'd.name as device_name',
                    'd.tcp_port'
                )
                .limit(limit)
                .offset(offset);

            return {
                items: items || [],
                pagination: { page, limit, total, total_pages: Math.ceil(total / limit) }
            };
        }, 'GetModbusSlaveAccessLogs');
    }

    /**
     * 이력 통계 (성공률, 총 요청, 클라이언트 수)
     */
    async getAccessLogStats(tenantId, { deviceId, dateFrom, dateTo } = {}) {
        return await this.handleRequest(async () => {
            let query = this.knex('modbus_slave_access_logs as l')
                .leftJoin('modbus_slave_devices as d', 'l.device_id', 'd.id')
                .where(function () {
                    this.where('l.tenant_id', tenantId).orWhereNull('l.tenant_id');
                });

            if (deviceId) query = query.where('l.device_id', parseInt(deviceId));
            if (dateFrom) query = query.where('l.recorded_at', '>=', dateFrom);
            if (dateTo) query = query.where('l.recorded_at', '<=', dateTo);

            const stats = await query.select(
                this.knex.raw('COUNT(*) as total_snapshots'),
                this.knex.raw('SUM(l.total_requests) as total_requests'),
                this.knex.raw('SUM(l.failed_requests) as total_failures'),
                this.knex.raw('COUNT(DISTINCT l.client_ip) as unique_clients'),
                this.knex.raw('AVG(l.avg_response_us) as avg_response_us'),
                this.knex.raw('AVG(l.success_rate) as avg_success_rate')
            ).first();

            return {
                total_snapshots: parseInt(stats?.total_snapshots || 0),
                total_requests: parseInt(stats?.total_requests || 0),
                total_failures: parseInt(stats?.total_failures || 0),
                unique_clients: parseInt(stats?.unique_clients || 0),
                avg_response_us: parseFloat(stats?.avg_response_us || 0),
                avg_success_rate: parseFloat(stats?.avg_success_rate || 1.0),
            };
        }, 'GetModbusSlaveAccessLogStats');
    }

    // =========================================================================
    // 패킷 로그 파일 (LoggerEngine이 생성하는 logs/packets/날짜/ 파일)
    // =========================================================================

    /**
     * 패킷 로그 파일 목록 조회
     * 경로: LOG_FILE_PATH/packets/YYYYMMDD/modbus-slave_dN.log
     */
    async getPacketLogFiles(deviceId = null) {
        return await this.handleRequest(async () => {
            const fs = require('fs').promises;
            const path = require('path');
            const logBase = process.env.LOG_FILE_PATH || './logs';
            const packetDir = path.join(logBase, 'packets');

            let files = [];
            try {
                const dates = await fs.readdir(packetDir);
                for (const dateDir of dates.sort().reverse()) {
                    const dayPath = path.join(packetDir, dateDir);
                    const stat = await fs.stat(dayPath).catch(() => null);
                    if (!stat?.isDirectory()) continue;

                    const dayFiles = await fs.readdir(dayPath);
                    for (const f of dayFiles) {
                        if (!f.endsWith('.log')) continue;
                        // 파일명 패턴: modbus-slave_dN_clientIP.log
                        if (!f.startsWith('modbus-slave_')) continue;
                        // 특정 device_id 필터
                        if (deviceId && !f.includes(`_d${deviceId}`)) continue;

                        const filePath = path.join(dayPath, f);
                        const fstat = await fs.stat(filePath).catch(() => null);
                        files.push({
                            date: dateDir,
                            filename: f,
                            path: filePath,
                            size_bytes: fstat?.size || 0,
                            modified_at: fstat?.mtime?.toISOString() || null,
                        });
                    }
                }
            } catch (e) {
                // 디렉토리 없음 → 빈 배열 반환
            }
            return files;
        }, 'GetModbusSlavePacketLogFiles');
    }

    /**
     * 패킷 로그 파일 내용 읽기 (마지막 N줄)
     */
    async readPacketLogFile(filePath, tailLines = 500) {
        return await this.handleRequest(async () => {
            const fs = require('fs').promises;
            const path = require('path');

            // 경로 탈출 방지
            const logBase = path.resolve(process.env.LOG_FILE_PATH || './logs');
            const resolved = path.resolve(filePath);
            if (!resolved.startsWith(logBase)) {
                throw Object.assign(new Error('허용되지 않는 경로'), { statusCode: 403 });
            }

            const content = await fs.readFile(resolved, 'utf-8').catch(() => '');
            const lines = content.split('\n');
            const tail = lines.slice(-tailLines).join('\n');
            return {
                path: resolved,
                total_lines: lines.length,
                shown_lines: Math.min(tailLines, lines.length),
                content: tail,
            };
        }, 'ReadModbusSlavePacketLogFile');
    }
}

module.exports = new ModbusSlaveService();


