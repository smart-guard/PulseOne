/**
 * backend/lib/services/DeviceService.js
 * 디바이스 관련 비즈니스 로직 처리 서비스
 */

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

// 헬퍼: JSON 스트링인 경우 파싱 (특히 {"value": 1.0} 형태 대응)
const safeParseValue = (val) => {
    if (typeof val !== 'string' || !val.trim().startsWith('{')) return val;
    try {
        const parsed = JSON.parse(val);
        if (parsed && typeof parsed === 'object' && 'value' in parsed) {
            return parsed.value;
        }
        return parsed;
    } catch (e) {
        return val;
    }
};

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

    get redis() {
        if (!this._redis) {
            this._redis = require('../connection/redis');
        }
        return this._redis;
    }

    get auditLogService() {
        if (!this._auditLogService) {
            const AuditLogService = require('./AuditLogService');
            this._auditLogService = new AuditLogService();
        }
        return this._auditLogService;
    }

    /**
     * 디바이스 트리 구조 데이터 반환
     */
    async getDeviceTree(options) {
        return await this.handleRequest(async () => {
            const tenantId = options.tenantId;
            const isSystemAdmin = options.isSystemAdmin === true;

            // 1. 테넌트 목록 준비
            const tenantRepo = RepositoryFactory.getInstance().getRepository('TenantRepository');
            let relevantTenants = [];

            if (isSystemAdmin) {
                // 시스템 관리자는 모든 활성 테넌트 조회
                const tenantsResult = await tenantRepo.findAll({ isActive: true, limit: 1000 });
                relevantTenants = (Array.isArray(tenantsResult) ? tenantsResult : tenantsResult.items) || [];
            } else {
                // 일반 사용자는 자신의 테넌트 정보만 조회
                const tenant = await tenantRepo.findById(tenantId);
                if (tenant) relevantTenants = [tenant];
            }

            if (relevantTenants.length === 0) {
                return { id: 'root', label: 'No Data', type: 'tenant', children: [] };
            }

            // 2. 모든 데이터 병렬 조회 (성능 최적화)
            this.logger.log(`🔍 [DeviceService] Fetching devices and collectors for tree... (SystemAdmin: ${isSystemAdmin}, TenantId: ${tenantId})`);

            const [allDevicesResult, allCollectorsResult] = await Promise.all([
                this.repository.findAll(isSystemAdmin ? null : tenantId, { limit: 5000 }),
                RepositoryFactory.getInstance().getRepository('EdgeServerRepository').findAll(isSystemAdmin ? null : tenantId)
            ]);

            // 결과 형식 표준화 (배열 또는 {items: []} 처리)
            const allDevices = Array.isArray(allDevicesResult) ? allDevicesResult : (allDevicesResult?.items || []);
            const allCollectors = Array.isArray(allCollectorsResult) ? allCollectorsResult : (allCollectorsResult?.items || []);

            this.logger.log(`📊 [DeviceService] Data fetched: ${allDevices.length} devices, ${allCollectors.length} collectors`);

            // 3. 테넌트별 트리 구성
            const tenantNodes = relevantTenants.map(tenant => {
                try {
                    const tenantDevices = allDevices.filter(d => d.tenant_id === tenant.id);
                    const tenantCollectors = allCollectors.filter(c => c.tenant_id === tenant.id);

                    const tenantTree = this.buildTreeData(tenantDevices, tenantCollectors, options);
                    return {
                        ...tenantTree,
                        id: `tenant-${tenant.id}`,
                        label: tenant.company_name || tenant.name || `Tenant ${tenant.id}`,
                        type: 'tenant',
                        level: 1
                    };
                } catch (error) {
                    this.logger.error(`❌ [DeviceService] Failed to build tree for tenant ${tenant.id}:`, error.message);
                    return {
                        id: `tenant-${tenant.id}-error`,
                        label: `${tenant.company_name || tenant.name} (Error)`,
                        type: 'tenant',
                        level: 1,
                        children: []
                    };
                }
            });

            // 4. 통계 계산 (프론트엔드 대시보드 및 탐색기용)
            const stats = {
                total_devices: allDevices.length,
                total_collectors: allCollectors.length,
                rtu_masters: allDevices.filter(d => d.protocol_type === 'MODBUS_RTU' && d.device_type === 'GATEWAY').length,
                rtu_slaves: allDevices.filter(d => d.protocol_type === 'MODBUS_RTU' && d.device_type !== 'GATEWAY').length,
                active_devices: allDevices.filter(d => d.is_enabled === 1).length
            };

            // 5. 최종 구조 반환
            // 시스템 관리자가 아니면 테넌트 노드를 루트로 사용하도록 변경 (중첩 방지)
            if (!isSystemAdmin && tenantNodes.length > 0) {
                return {
                    tree: tenantNodes[0],
                    statistics: stats
                };
            }

            return {
                tree: {
                    id: 'root-system',
                    label: isSystemAdmin ? 'System Overview' : 'PulseOne Factory',
                    type: 'tenant',
                    level: 0,
                    children: tenantNodes
                },
                statistics: stats
            };
        }, 'GetDeviceTree');
    }

    buildTreeData(devices, collectors, options) {
        // Site별로 그룹화
        const sitesMap = new Map();

        // 디바이스를 Site 하위로 직접 배치 (수집기 레이어 제거)
        devices.forEach(device => {
            const siteId = device.site_id || 0;
            const siteName = device.site_name || 'Unassigned Site';

            if (!sitesMap.has(siteId)) {
                sitesMap.set(siteId, {
                    id: `site-${siteId}`,
                    label: siteName,
                    type: 'site',
                    level: 1,
                    children: []
                });
            }

            const siteNode = sitesMap.get(siteId);

            // 이미 추가되었는지 확인 (중복 방지)
            const deviceNodeId = `dev-${device.id}`;
            if (!siteNode.children.find(d => d.id === deviceNodeId)) {
                siteNode.children.push({
                    id: deviceNodeId,
                    label: device.name,
                    type: 'master', // RTU 등 구분 위해 'master' 타입 사용
                    level: 2, // Site(1) -> Device(2)
                    device_info: {
                        device_id: device.id.toString(),
                        device_name: device.name,
                        device_type: device.device_type,
                        protocol_type: device.protocol_type,
                        status: device.connection_status,
                        is_enabled: device.is_enabled
                    },
                    children: []
                });
            }
        });

        // 수집기 정보가 필요하다면 (예: 통계용) 별도로 처리할 수 있지만, 트리에서는 제거됨

        // 단일 루트 노드(Tenant)로 감싸서 반환
        return {
            id: 'root-tenant',
            label: 'PulseOne Factory',
            type: 'tenant',
            level: 0,
            children: Array.from(sitesMap.values())
        };
    }
    /**
     * 디바이스 목록 조회 (RTU 및 Collector 상태 포함)
     */
    async getDevices(options) {
        return await this.handleRequest(async () => {
            const isStatusFiltering = options.status && options.status !== 'all';

            // 실시간 상태 필터링 작업이 필요한 경우, 전체를 가져와서 필터링 후 페이징 처리
            if (isStatusFiltering) {
                // DB에서 필터링된 전체 리스트를 가져옴 (페이징 없이)
                const allItems = await this.repository.findAll(options.tenantId, {
                    ...options,
                    page: 1,
                    limit: 10000, // 충분히 큰 수
                    includeCount: false
                });

                // 실시간 상태 주입
                await this.enrichWithCollectorStatus(allItems);

                // 상태 필터링 수행
                const filteredItems = allItems.filter(device => {
                    const statusValue = (device.collector_status?.status || 'unknown').toLowerCase();
                    return statusValue === options.status.toLowerCase();
                });

                // 정렬 수행
                const sortBy = options.sort_by || 'name';
                const sortOrder = (options.sort_order || 'ASC').toUpperCase();
                filteredItems.sort((a, b) => {
                    const valA = a[sortBy] || '';
                    const valB = b[sortBy] || '';
                    if (valA < valB) return sortOrder === 'ASC' ? -1 : 1;
                    if (valA > valB) return sortOrder === 'ASC' ? 1 : -1;
                    return 0;
                });

                // 페이징 처리
                const page = parseInt(options.page) || 1;
                const limit = parseInt(options.limit) || 100;
                const total = filteredItems.length;
                const totalPages = Math.ceil(total / limit);
                const offset = (page - 1) * limit;
                const items = filteredItems.slice(offset, offset + limit);

                // RTU 요약은 필터링된 전체 데이터 기준
                const rtu_summary = await this.repository.getRtuSummary(options.tenantId, options);

                return {
                    items,
                    pagination: {
                        page,
                        limit,
                        total,
                        totalPages,
                        hasNext: page < totalPages,
                        hasPrev: page > 1
                    },
                    rtu_summary
                };
            }

            // 일반적인 경우 (DB 페이징 사용)
            const result = await this.repository.findAll(options.tenantId, {
                ...options,
                includeCount: true
            });

            if (result && result.items) {
                result.items = this.enhanceDevicesWithRtuInfo(result.items);

                if (options.includeRtuRelations) {
                    result.items = await this.addRtuRelationships(result.items, options.tenantId);
                }

                if (options.includeCollectorStatus) {
                    await this.enrichWithCollectorStatus(result.items);
                }

                // RTU 통계 추가 (필터링된 전체 데이터 기준)
                result.rtu_summary = await this.repository.getRtuSummary(options.tenantId, options);
            }

            return result;
        }, 'GetDevices');
    }

    /**
     * 디바이스 통계 조회
     */
    async getDeviceStatistics(tenantId) {
        return await this.handleRequest(async () => {
            return await this.repository.getStatistics(tenantId);
        }, 'GetDeviceStatistics');
    }

    /**
     * 사용 가능한 프로토콜 목록 조회
     */
    async getAvailableProtocols() {
        return await this.handleRequest(async () => {
            const protocolRepo = RepositoryFactory.getInstance().getProtocolRepository();
            return await protocolRepo.findActive();
        }, 'GetAvailableProtocols');
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
     * 디바이스의 데이터 포인트 목록 조회
     */
    async getDeviceDataPoints(deviceId, options = {}) {
        return await this.handleRequest(async () => {
            let dataPoints = await this.repository.getDataPointsByDevice(deviceId);

            // JSON 필드 파싱 (protocol_params, metadata, tags)
            dataPoints = dataPoints.map(dp => ({
                ...dp,
                protocol_params: dp.protocol_params
                    ? (typeof dp.protocol_params === 'string' ? (() => { try { return JSON.parse(dp.protocol_params); } catch { return dp.protocol_params; } })() : dp.protocol_params)
                    : null,
                metadata: dp.metadata
                    ? (typeof dp.metadata === 'string' ? (() => { try { return JSON.parse(dp.metadata); } catch { return dp.metadata; } })() : dp.metadata)
                    : null,
                // 프론트엔드 CurrentValue 객체 구조에 맞춰 변환
                current_value: (dp.current_value !== null && dp.current_value !== undefined) ? {
                    value: safeParseValue(dp.current_value),
                    timestamp: dp.last_update,
                    quality: dp.quality || 'good'
                } : null
            }));

            // 필터링 적용
            if (options.data_type) {
                dataPoints = dataPoints.filter(dp => dp.data_type === options.data_type);
            }
            if (options.enabled_only === 'true' || options.enabled_only === true) {
                dataPoints = dataPoints.filter(dp => dp.is_enabled === 1 || dp.is_enabled === true);
            }

            // 페이징 (선택사항, 데이터 포인트가 많을 경우 대비)
            const page = parseInt(options.page) || 1;
            const limit = parseInt(options.limit) || 1000;
            const startIndex = (page - 1) * limit;
            const paginatedPoints = dataPoints.slice(startIndex, startIndex + limit);

            return {
                items: paginatedPoints,
                pagination: {
                    page,
                    limit,
                    total: dataPoints.length,
                    totalPages: Math.ceil(dataPoints.length / limit)
                }
            };
        }, 'GetDeviceDataPoints');
    }

    /**
     * 디바이스 생성 및 후크 실행
     */
    async createDevice(deviceData, tenantId, user = null) {
        return await this.handleRequest(async () => {
            // System Admin인 경우 body의 tenant_id를 우선 사용 (테넌트 간 이동/생성 지원)
            let targetTenantId = (user && user.role === 'system_admin' && deviceData.tenant_id)
                ? deviceData.tenant_id
                : tenantId;

            // [FIX] tenant_id가 없고 site_id가 있으면 Site에서 조회하여 Tenant ID 획득
            if (!targetTenantId && deviceData.site_id) {
                try {
                    const siteRepo = RepositoryFactory.getInstance().getRepository('SiteRepository');
                    // findById(id) 호출 (Tenant ID가 없으므로 null 전달하여 전역 조회 시도)
                    const site = await siteRepo.findById(deviceData.site_id);
                    if (site && site.tenant_id) {
                        targetTenantId = site.tenant_id;
                    }
                } catch (ignore) {
                    // Site 조회 실패 시 무시
                }
            }

            // [FIX] 여전히 없으면 System Admin은 Root Tenant(1)로 기본 설정
            if (!targetTenantId && user && user.role === 'system_admin') {
                targetTenantId = 1;
            }

            if (!targetTenantId) {
                throw new Error('Tenant ID is required for device creation');
            }


            // [FIX] Normalize data types (allow lowercase number -> FLOAT32)
            if (deviceData.data_points && Array.isArray(deviceData.data_points)) {
                deviceData.data_points.forEach(dp => {
                    if (dp.data_type) {
                        const type = String(dp.data_type).toUpperCase();
                        if (type === 'NUMBER') dp.data_type = 'FLOAT32';
                        else if (type === 'BOOLEAN') dp.data_type = 'BOOL';
                        else if (type === 'STRING') dp.data_type = 'STRING';
                        else dp.data_type = type;
                    } else {
                        dp.data_type = 'FLOAT32'; // Default
                    }
                });
            }

            // 한도 체크
            await this.validateDataPointLimit(targetTenantId);

            const newDevice = await this.repository.create(deviceData, targetTenantId);

            // Audit Log
            if (user) {
                await this.auditLogService.logChange(user, 'CREATE',
                    { type: 'DEVICE', id: newDevice.id, name: newDevice.name },
                    null, newDevice, 'New device created');
            }

            // 설정 동기화 후크 실행
            await this.configSyncHooks.afterDeviceCreate(newDevice);

            // Collector 자동 활성화: 디바이스가 생기면 해당 Collector를 enable + start
            if (newDevice.edge_server_id) {
                await this._syncCollectorLifecycle(newDevice.edge_server_id);
            }

            return newDevice;
        }, 'CreateDevice');
    }

    /**
     * 디바이스 업데이트 및 동기화
     */
    async updateDevice(id, updateData, tenantId, user = null) {
        return await this.handleRequest(async () => {
            this.logger.log(`🚀 [DeviceService] Updating device ${id}...`);
            this.logger.debug(`📦 [DeviceService] Update payload:`, JSON.stringify(updateData, null, 2));

            const oldDevice = await this.repository.findById(id, tenantId);
            if (!oldDevice) {
                this.logger.warn(`⚠️ [DeviceService] Device ${id} not found for tenant ${tenantId}`);
                throw new Error('Device not found');
            }

            // [FIX] Normalize data types (allow lowercase number -> FLOAT32)
            if (updateData.data_points && Array.isArray(updateData.data_points)) {
                updateData.data_points.forEach(dp => {
                    if (dp.data_type) {
                        const type = String(dp.data_type).toUpperCase();
                        if (type === 'NUMBER') dp.data_type = 'FLOAT32';
                        else if (type === 'BOOLEAN') dp.data_type = 'BOOL';
                        else if (type === 'STRING') dp.data_type = 'STRING';
                        else dp.data_type = type;
                    } else {
                        dp.data_type = 'FLOAT32'; // Default
                    }
                });
            }

            // DB 업데이트 실행
            const updatedDevice = await this.repository.update(id, updateData, tenantId);

            // 상태 변경 시(비활성 -> 활성) 한도 체크
            if (updateData.is_enabled === 1 || updateData.is_enabled === true) {
                if (!oldDevice.is_enabled) {
                    try {
                        await this.validateDataPointLimit(tenantId);
                    } catch (error) {
                        // 한도 초과 시 다시 비활성화하고 에러 발생
                        await this.repository.update(id, { is_enabled: 0 }, tenantId);
                        throw error;
                    }
                }
            }

            this.logger.log(`✅ [DeviceService] Database updated for device ${id}`);

            // Audit Log
            if (user && updatedDevice) {
                try {
                    await this.auditLogService.logChange(user, 'UPDATE',
                        { type: 'DEVICE', id: updatedDevice.id, name: updatedDevice.name },
                        oldDevice, updatedDevice, 'Device configuration updated');
                } catch (auditError) {
                    this.logger.warn(`⚠️ [DeviceService] Audit log failed (non-critical):`, auditError.message);
                }
            }

            // 설정 동기화 + Collector 생명주기 = fire-and-forget (Collector 오프라인이어도 즉시 응답)
            const oldEdgeId = oldDevice.edge_server_id;
            const newEdgeId = updatedDevice.edge_server_id;
            setImmediate(async () => {
                try {
                    await this.configSyncHooks.afterDeviceUpdate(oldDevice, updatedDevice);
                } catch (syncError) {
                    this.logger.warn(`⚠️ [DeviceService] Config sync failed (background, non-critical): ${syncError.message}`);
                }
                if (oldEdgeId !== newEdgeId) {
                    if (oldEdgeId) await this._syncCollectorLifecycle(oldEdgeId);
                    if (newEdgeId) await this._syncCollectorLifecycle(newEdgeId);
                }
            });

            return updatedDevice;
        }, 'UpdateDevice');
    }


    /**
     * Collector 생명주기 자동 관리
     * - 자동 활성화만: 디바이스가 생기면 is_enabled=1 + startCollector
     * - 자동 비활성화 없음: 디바이스가 0이 돼도 컬렉터를 강제로 끄지 않음
     *   (비활성화는 관리자가 명시적으로 처리)
     */
    async _syncCollectorLifecycle(edgeServerId) {
        try {
            const edgeRepo = RepositoryFactory.getInstance().getEdgeServerRepository();
            const edgeServer = await edgeRepo.findById(edgeServerId);
            if (!edgeServer || edgeServer.server_type !== 'collector') return;

            // 해당 Collector에 속한 활성 디바이스 수 조회
            const deviceCount = await this.repository.countByEdgeServer(edgeServerId);
            const isCurrentlyEnabled = !!edgeServer.is_enabled;

            if (deviceCount > 0 && !isCurrentlyEnabled) {
                // 0 → 1: 디바이스가 생겼는데 비활성화 상태면 활성화 + spawn
                this.logger.log(`🟢 [CollectorLifecycle] Collector ${edgeServerId} has ${deviceCount} device(s) → enabling & starting`);
                await edgeRepo.update(edgeServerId, { is_enabled: 1 });
                const crossPlatform = require('./crossPlatformManager');
                await crossPlatform.startCollector(edgeServerId).catch(e =>
                    this.logger.warn(`⚠️ [CollectorLifecycle] startCollector(${edgeServerId}) failed (non-critical): ${e.message}`)
                );
            } else {
                this.logger.log(`ℹ️ [CollectorLifecycle] Collector ${edgeServerId}: ${deviceCount} device(s), enabled=${isCurrentlyEnabled} → no change`);
            }
        } catch (e) {
            // 생명주기 실패는 디바이스 CRUD를 블록하지 않음
            this.logger.warn(`⚠️ [CollectorLifecycle] sync failed for collector ${edgeServerId}: ${e.message}`);
        }
    }

    /**
     * 디바이스 대량 업데이트 및 동기화
     */
    async bulkUpdateDevices(ids, updateData, tenantId, user = null) {
        return await this.handleRequest(async () => {
            if (!ids || ids.length === 0) return 0;

            const affected = await this.repository.bulkUpdate(ids, updateData, tenantId);

            // Audit Log
            if (user && affected > 0) {
                await this.auditLogService.logAction({
                    tenant_id: tenantId,
                    user_id: user.id,
                    action: 'BULK_UPDATE',
                    entity_type: 'DEVICE',
                    entity_name: `${ids.length} devices`,
                    change_summary: `Bulk update for devices: ${ids.join(', ')}`,
                    new_value: updateData
                });
            }

            // TODO: 설정 동기화 후크 연동 (성능 고려 필요)
            // 개별 디바이스별로 동기화가 필요한 경우 루프를 돌아야 할 수 있음

            return affected;
        }, 'BulkUpdateDevices');
    }

    /**
     * 디바이스 삭제
     */
    async delete(id, tenantId, user = null) {
        return await this.handleRequest(async () => {
            const device = await this.repository.findById(id, tenantId);
            if (!device) throw new Error('Device not found');

            const edgeServerId = device.edge_server_id; // 삭제 전에 기록
            const result = await this.repository.deleteById(id, tenantId);

            // Audit Log
            if (user && result) {
                await this.auditLogService.logAction({
                    tenant_id: tenantId,
                    user_id: user.id,
                    action: 'DELETE',
                    entity_type: 'DEVICE',
                    entity_id: id,
                    entity_name: device.name,
                    old_value: device,
                    change_summary: 'Device soft-deleted'
                });
            }

            // 동기화 후크 실행 (삭제 후 처리)
            try {
                await this.configSyncHooks.afterDeviceDelete(device);
            } catch (syncError) {
                this.logger.error(`❌ [DeviceService] Sync failed during deletion for ${id}:`, syncError.message);

                // Circuit breaker open 등의 통신 장애인 경우 무시하고 성공 리턴 (이미 DB에서는 삭제됨/처리됨)
                if (syncError.name === 'SyncError' || syncError.message.includes('Circuit breaker is OPEN')) {
                    this.logger.warn(`⚠️ [DeviceService] Device ${id} soft-deleted in DB, but Collector sync failed. Continuing.`);
                    return {
                        success: true,
                        result,
                        sync_warning: `Database updated, but Collector sync failed: ${syncError.message}`
                    };
                }
                // 다른 치명적 에러는 전파
                throw syncError;
            }

            // Collector 생명주기 재평가: 디바이스 없어진 Collector를 비활성화할 수 있음
            if (edgeServerId) {
                await this._syncCollectorLifecycle(edgeServerId);
            }

            return result;
        }, 'DeleteDevice');
    }

    /**
     * 삭제된 디바이스를 복구합니다.
     */
    async restore(id, tenantId, user = null) {
        return await this.handleRequest(async () => {
            // 삭제된 데이터도 포함해서 검색
            const device = await this.repository.findById(id, tenantId, null, { includeDeleted: true });
            if (!device) throw new Error('Device not found');
            if (device.is_deleted === 0) return { success: true, already_active: true };

            const result = await this.repository.restoreById(id, tenantId);

            // Audit Log
            if (user && result) {
                await this.auditLogService.logAction({
                    tenant_id: tenantId,
                    user_id: user.id,
                    action: 'UPDATE',
                    entity_type: 'DEVICE',
                    entity_id: id,
                    entity_name: device.name,
                    new_value: { ...device, is_deleted: 0 },
                    change_summary: 'Device restored from soft-delete'
                });
            }

            // 동기화 후크 실행 (삭제 복구는 신규 생성과 유사함)
            try {
                await this.configSyncHooks.afterDeviceCreate(device);
            } catch (syncError) {
                this.logger.error(`❌ [DeviceService] Sync failed during restoration for ${id}:`, syncError.message);
                return {
                    success: true,
                    result,
                    sync_warning: `Device restored in Database, but Collector sync failed: ${syncError.message}`
                };
            }

            return result;
        }, 'RestoreDevice');
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
            const proxyOptions = { ...options, edgeServerId: device.edge_server_id };

            switch (action) {
                case 'start':
                    result = await this.collectorProxy.startDevice(id.toString(), proxyOptions);
                    break;
                case 'stop':
                    result = await this.collectorProxy.stopDevice(id.toString(), proxyOptions);
                    break;
                case 'restart':
                    result = await this.collectorProxy.restartDevice(id.toString(), proxyOptions);
                    break;
                default:
                    throw new Error(`Invalid action: ${action}`);
            }

            return result;
        }, `ExecuteAction:${action}`);
    }

    /**
     * 디지털 출력 제어 (DO)
     */
    async controlDigitalOutput(deviceId, outputId, state, options, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.repository.findById(deviceId, tenantId);
            if (!device) throw new Error('Device not found');

            return await this.collectorProxy.controlDigitalOutput(deviceId, outputId, state, {
                ...options,
                edgeServerId: device.edge_server_id
            });
        }, 'ControlDigitalOutput');
    }

    /**
     * 아날로그 출력 제어 (AO)
     */
    async controlAnalogOutput(deviceId, outputId, value, options, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.repository.findById(deviceId, tenantId);
            if (!device) throw new Error('Device not found');

            return await this.collectorProxy.controlAnalogOutput(deviceId, outputId, value, {
                ...options,
                edgeServerId: device.edge_server_id
            });
        }, 'ControlAnalogOutput');
    }

    /**
     * 연결 진단 시행 (Ping/Modbus/BACnet 등 프로토콜별 체크)
     */
    async diagnoseConnection(id, tenantId) {
        return await this.handleRequest(async () => {
            const device = await this.repository.findById(id, tenantId);
            if (!device) throw new Error('Device not found');

            // CollectorProxy를 통한 실시간 진단 요청
            // TODO: Collector 측에 diagnose API 구현 필요. 현재는 통신 상태 확인으로 대체
            const startTime = Date.now();
            const status = await this.collectorProxy.getDeviceStatus(id.toString(), {
                edgeServerId: device.edge_server_id
            });
            const responseTimeMs = Date.now() - startTime;

            const isConnected = status.success && status.data && (status.data.connected === true || (status.data.data && status.data.data.connected === true));

            return {
                device_id: id,
                protocol: device.protocol_type,
                endpoint: device.endpoint,
                diag_result: isConnected ? 'Success' : 'Failed',
                test_successful: isConnected,
                success: isConnected, // 프론트엔드 호환용 추가
                response_time_ms: responseTimeMs,
                error_message: isConnected ? '' : (status.message || 'Connection failed'),
                details: status.data || {},
                timestamp: new Date().toISOString()
            };
        }, 'DiagnoseConnection');
    }

    /**
     * 네트워크 스캔 (Collector Discovery) Trigger
     */
    async scanNetwork(options) {
        return await this.handleRequest(async () => {
            // 1. Edge Server 결정 Logic
            let edgeServerId = options.edgeServerId;
            if (!edgeServerId && options.tenantId) {
                // Tenant의 첫번째 Edge Server를 조회하거나 0 (Default) 사용
                const edgeRepo = RepositoryFactory.getInstance().getEdgeServerRepository();
                const servers = await edgeRepo.findByTenant(options.tenantId);
                if (servers.length > 0) {
                    edgeServerId = servers[0].id;
                } else {
                    edgeServerId = 0;
                }
            }

            // 2. Collector 스캔 요청
            const result = await this.collectorProxy.scanNetwork({
                edgeServerId: edgeServerId,
                protocol: options.protocol || 'BACNET',
                range: options.range || '', // e.g., "192.168.1.0/24" or empty for local broadcast
                timeout: options.timeout || 10000
            });

            return {
                status: result.success ? 'started' : 'failed',
                job_id: Date.now().toString(), // 임시 Job ID
                message: result.data?.data?.message || result.data?.message || 'Scan initiated',
                target_edge_server: edgeServerId
            };
        }, 'ScanNetwork');
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

    /**
     * Redis에서 device 상태를 직접 읽어옴 (Collector HTTP 타임아웃 시 fallback)
     * Collector가 worker 상태 변경 시 `device:{num}:status`, `worker:{device_id}:status` 키에 씀
     */
    async getDeviceStatusFromRedis(device) {
        try {
            const redisClient = await this.redis?.getRedisClient?.();
            if (!redisClient) return null;

            // Collector writes: device:{numeric_id}:status
            const deviceNum = device.id.toString();
            const deviceKey = `device:${deviceNum}:status`;
            const raw = await redisClient.get(deviceKey);
            if (!raw) return null;

            const data = JSON.parse(raw);
            return {
                status: data.worker_status || 'unknown',
                connection_status: data.connection_status || 'disconnected',
                last_communication: data.last_communication || null,
                source: 'redis'
            };
        } catch (e) {
            return null;
        }
    }

    async enrichWithCollectorStatus(items) {
        if (!items || items.length === 0) return;

        try {
            // Edge Server ID별로 디바이스 그룹화
            const groupedByServer = items.reduce((acc, item) => {
                const serverId = item.edge_server_id || 0;
                if (!acc[serverId]) acc[serverId] = [];
                acc[serverId].push(item);
                return acc;
            }, {});

            // 각 서버별로 상태 조회 (병렬 처리)
            await Promise.all(Object.entries(groupedByServer).map(async ([serverId, devices]) => {
                try {
                    const id = parseInt(serverId);
                    // 🚀 [HOTFIX] 콜렉터 응답 지연이 전체 API를 죽이지 않도록 1초 타임아웃 강제 적용
                    const workerResult = await Promise.race([
                        this.collectorProxy.getWorkerStatus(id),
                        new Promise((_, reject) => setTimeout(() => reject(new Error('Collector status timeout')), 1000))
                    ]);

                    const statuses = (workerResult.data?.data?.workers || workerResult.data?.workers || {});

                    devices.forEach(d => {
                        const rawStatus = statuses[d.id.toString()] || { status: 'unknown' };
                        const rawState = rawStatus.state || rawStatus.status || 'unknown';
                        const status = rawState.toLowerCase();

                        d.collector_status = {
                            ...rawStatus,
                            status: status
                        };

                        // PAUSED 상태는 워커가 일시정지된 것이지 연결이 끊긴 게 아님
                        // RUNNING/PAUSED → connection_status 유지, STOPPED/ERROR/UNKNOWN → disconnected
                        if (status !== 'running' && status !== 'paused') {
                            d.connection_status = 'disconnected';
                        }
                    });
                } catch (e) {
                    // Collector HTTP 실패 → Redis에서 직접 읽기 (실시간 상태 보존)
                    this.logger.warn(`⚠️ Collector [${serverId}] HTTP 실패, Redis fallback: ${e.message}`);
                    await Promise.all(devices.map(async d => {
                        const redisStatus = await this.getDeviceStatusFromRedis(d);
                        if (redisStatus) {
                            d.collector_status = { status: redisStatus.status, source: 'redis' };
                            d.connection_status = redisStatus.connection_status;
                        } else {
                            // Redis에도 없으면 → Collector가 한 번도 상태를 안 썼거나 TTL 만료
                            d.collector_status = { status: 'unknown', error: e.message };
                            d.connection_status = 'disconnected';
                        }
                    }));
                }
            }));
        } catch (e) {
            this.logger.warn('Global collector status enrichment failed:', e.message);
        }
    }

    async validateDataPointLimit(tenantId, additionalPoints = 0, trx = null) {
        const TenantService = require('./TenantService');
        const tenantService = new TenantService();
        const tenantRes = await tenantService.getTenantById(tenantId, trx);

        if (!tenantRes.success) throw new Error(tenantRes.message || 'Tenant not found');
        const tenant = tenantRes.data;

        const currentCount = tenant.data_points_count || 0;
        const maxLimit = tenant.max_data_points || 0;

        if (maxLimit > 0 && (currentCount + additionalPoints) >= maxLimit) {
            throw new Error(`데이터 포인트 사용 한도(${maxLimit.toLocaleString()}개)를 초과했습니다. 현재 사용량: ${currentCount.toLocaleString()}개`);
        }
    }

    /**
     * 장치들을 대량으로 삭제합니다.
     */
    async bulkDeleteDevices(ids, tenantId, user = null) {
        return await this.handleRequest(async () => {
            const affected = await this.repository.bulkDelete(ids, tenantId);

            // 감사 로그 (선택 사항)
            if (affected > 0 && user) {
                this.logger.info(`User ${user.id} deleted ${affected} devices in bulk`, { deviceIds: ids });
            }

            return affected;
        }, 'BulkDeleteDevices');
    }

    /**
     * 스캔 결과 조회 (최근 생성된 디바이스 목록)
     */
    async getScanResults(options) {
        return await this.handleRequest(async () => {
            const { tenantId, since, protocol } = options;

            // since가 없으면 최근 5분 이내 데이터 조회 (기초값)
            const sinceTime = since ? new Date(parseInt(since)) : new Date(Date.now() - 5 * 60 * 1000);
            const isoSince = sinceTime.toISOString().slice(0, 19).replace('T', ' ');

            // 1. 기본 디바이스 스캔 결과 조회
            let deviceQuery = this.repository.query('d')
                .leftJoin('protocols as p', 'p.id', 'd.protocol_id')
                .where('d.created_at', '>=', isoSince)
                .where('d.tenant_id', tenantId)
                .select('d.*', 'p.protocol_type', 'p.display_name as protocol_name');

            if (protocol) {
                deviceQuery.where('p.protocol_type', protocol);
            }

            const discoveredDevices = await deviceQuery;
            let results = discoveredDevices.map(d => this.enhanceDeviceWithRtuInfo(this.repository.parseDevice(d)));

            // 2. MQTT인 경우 최근 자동 등록된 데이터포인트도 결과에 포함 (UX 개선)
            if (protocol === 'MQTT') {
                const factory = require('../database/repositories/RepositoryFactory').getInstance();
                const pointRepo = factory.getDataPointRepository();

                const discoveredPoints = await pointRepo.query('dp')
                    .leftJoin('devices as d', 'd.id', 'dp.device_id')
                    .leftJoin('protocols as p', 'p.id', 'd.protocol_id')
                    .where('dp.created_at', '>=', isoSince)
                    .where('p.protocol_type', 'MQTT')
                    .where('d.tenant_id', tenantId)
                    .select('dp.id', 'dp.name', 'dp.address_string', 'd.endpoint', 'p.display_name as protocol_name');

                const virtualDevices = discoveredPoints.map(p => ({
                    id: `pt_${p.id}`,
                    name: p.name,
                    protocol_name: p.protocol_name,
                    endpoint: p.endpoint,
                    is_point: true
                }));

                results = [...results, ...virtualDevices];
            }

            return results;
        }, 'GetScanResults');
    }
}

module.exports = new DeviceService();
