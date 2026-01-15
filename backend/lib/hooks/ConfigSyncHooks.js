// =============================================================================
// backend/lib/hooks/ConfigSyncHooks.js
// 설정 변경 시 Collector 동기화 자동 처리 (에러 처리 개선 버전)
// =============================================================================

const { getInstance: getCollectorProxy } = require('../services/CollectorProxyService');

class ConfigSyncHooks {
    constructor() {
        this.hooks = new Map();
        this.isEnabled = process.env.CONFIG_SYNC_ENABLED !== 'false';

        // 🔥 NEW: 에러 처리 정책 설정
        this.errorPolicy = {
            // 'throw' | 'log' | 'ignore'
            // Default to 'log' if not explicitly 'throw' to prevent deadlocks in dev/unstable environments
            onSyncFailure: process.env.SYNC_ERROR_POLICY || 'log',

            // Critical operations (still default to throw if onSyncFailure is 'throw')
            criticalOperations: ['device_update', 'device_delete'],

            // 재시도 설정
            maxRetries: 2,
            retryDelayMs: 1000
        };

        console.log(`🔄 ConfigSyncHooks initialized (enabled: ${this.isEnabled}, errorPolicy: ${this.errorPolicy.onSyncFailure})`);
    }

    // =============================================================================
    // 🔥 NEW: 중앙집중식 에러 처리 메소드
    // =============================================================================

    async handleSyncOperation(operationType, operation, ...args) {
        if (!this.isEnabled) return { success: true, skipped: true };

        const isCritical = this.errorPolicy.criticalOperations.includes(operationType);

        for (let attempt = 0; attempt <= this.errorPolicy.maxRetries; attempt++) {
            try {
                await operation(...args);
                return { success: true, attempts: attempt + 1 };

            } catch (error) {
                console.error(`❌ Sync operation failed (attempt ${attempt + 1}): ${operationType}`, error.message);

                // 마지막 시도가 실패한 경우
                if (attempt === this.errorPolicy.maxRetries) {
                    const errorInfo = {
                        operation: operationType,
                        error: error.message,
                        attempts: attempt + 1
                    };

                    // 🔥 DEADLOCK PREVENTION: 
                    // Circuit breaker가 열려있는 경우(통신 불가 상태)는 Critical 작업이라도 
                    // 로그만 남기고 진행할 수 있도록 허용 (설정 수정을 통한 복구 기회 제공)
                    const isCircuitOpen = error.message.includes('Circuit breaker is OPEN');

                    // 에러 처리 정책에 따라 처리
                    if ((this.errorPolicy.onSyncFailure === 'throw' || isCritical) && !isCircuitOpen) {
                        // Critical 작업이거나 정책이 throw인 경우 예외 전파 (단, Circuit Open은 제외)
                        const syncError = new Error(`Sync failed: ${operationType} - ${error.message}`);
                        syncError.name = 'SyncError';
                        syncError.details = errorInfo;
                        throw syncError;

                    } else {
                        // 'log' 또는 'ignore' 또는 Circuit Open인 경우
                        console.warn(`⚠️ Sync failed (${operationType}) but continuing: ${error.message}`);
                        return { success: false, error: errorInfo, continued: true };
                    }
                }

                // 재시도 대기
                if (attempt < this.errorPolicy.maxRetries) {
                    await new Promise(resolve => setTimeout(resolve, this.errorPolicy.retryDelayMs));
                }
            }
        }
    }

    // =============================================================================
    // 🔥 개선된 디바이스 설정 변경 후크들
    // =============================================================================

    async afterDeviceCreate(device) {
        return await this.handleSyncOperation('device_create', async () => {
            console.log(`🔄 Device created hook: ${device.id} (${device.name})`);

            const proxy = getCollectorProxy();
            const edgeServerId = device.edge_server_id;

            // 1. 해당 콜렉터 설정 재로드
            await proxy.reloadAllConfigs(edgeServerId);

            // 2. 필요시 워커 시작
            if (device.is_enabled) {
                try {
                    await proxy.startDevice(device.id.toString(), { edgeServerId });
                    console.log(`✅ Device worker started for new device: ${device.id}`);
                } catch (error) {
                    console.warn(`⚠️ Failed to start worker for new device ${device.id}:`, error.message);
                }
            }
        });
    }

    async afterDeviceUpdate(oldDevice, newDevice) {
        // 🚨 Critical Operation - 항상 성공해야 함
        return await this.handleSyncOperation('device_update', async () => {
            console.log(`🔄 Device updated hook: ${newDevice.id} (${newDevice.name})`);

            const proxy = getCollectorProxy();
            const deviceId = newDevice.id.toString();
            const edgeServerId = newDevice.edge_server_id;

            // 1. 설정 동기화 (Critical)
            const syncPayload = {
                name: newDevice.name,
                protocol_type: newDevice.protocol_type,
                endpoint: newDevice.endpoint,
                polling_interval: newDevice.polling_interval,
                is_enabled: newDevice.is_enabled,
                settings: newDevice.settings || {}
            };

            console.log(`➡️ [ConfigSyncHooks] Syncing device settings to proxy: ${deviceId}`, JSON.stringify(syncPayload));

            await proxy.syncDeviceSettings(deviceId, syncPayload, { edgeServerId });

            // 2. 상태 변경 처리
            if (oldDevice.is_enabled !== newDevice.is_enabled) {
                if (newDevice.is_enabled) {
                    await proxy.startDevice(deviceId, { edgeServerId });
                    console.log(`✅ Device worker started: ${deviceId}`);
                } else {
                    await proxy.stopDevice(deviceId, { edgeServerId });
                    console.log(`✅ Device worker stopped: ${deviceId}`);
                }
            } else if (newDevice.is_enabled) {
                // 활성 상태에서 설정만 변경된 경우 재시작
                await proxy.restartDevice(deviceId, { edgeServerId });
                console.log(`✅ Device worker restarted for config change: ${deviceId}`);
            }
        });
    }

    async afterDeviceDelete(device) {
        // 🚨 Critical Operation - 항상 성공해야 함
        return await this.handleSyncOperation('device_delete', async () => {
            console.log(`🔄 Device deleted hook: ${device.id} (${device.name})`);

            const proxy = getCollectorProxy();
            const deviceId = device.id.toString();
            const edgeServerId = device.edge_server_id;

            // 1. 워커 중지
            try {
                await proxy.stopDevice(deviceId, { edgeServerId });
                console.log(`✅ Device worker stopped for deleted device: ${deviceId}`);
            } catch (error) {
                console.log(`ℹ️ Device worker was not running or failed: ${deviceId}`);
            }

            // 2. 해당 콜렉터 설정 재로드
            await proxy.reloadAllConfigs(edgeServerId);
        });
    }

    // =============================================================================
    // 🔥 알람 규칙 변경 후크들 (Non-Critical)
    // =============================================================================

    async afterAlarmRuleCreate(alarmRule) {
        return await this.handleSyncOperation('alarm_create', async () => {
            console.log(`🔄 Alarm rule created hook: ${alarmRule.id} (${alarmRule.name})`);

            const proxy = getCollectorProxy();
            await proxy.notifyConfigChange('alarm_rule', alarmRule.id, {
                action: 'create',
                target_type: alarmRule.target_type,
                target_id: alarmRule.target_id,
                is_enabled: alarmRule.is_enabled
            }, 'all'); // 알람 규칙은 모든 콜렉터에 전파
        });
    }

    async afterAlarmRuleUpdate(oldRule, newRule) {
        return await this.handleSyncOperation('alarm_update', async () => {
            console.log(`🔄 Alarm rule updated hook: ${newRule.id} (${newRule.name})`);

            const proxy = getCollectorProxy();

            // 변경사항 분석
            const changes = {
                action: 'update',
                target_type: newRule.target_type,
                target_id: newRule.target_id,
                enabled_changed: oldRule.is_enabled !== newRule.is_enabled,
                condition_changed: oldRule.condition_config !== newRule.condition_config,
                severity_changed: oldRule.severity !== newRule.severity
            };

            await proxy.notifyConfigChange('alarm_rule', newRule.id, changes, 'all');
        });
    }

    async afterAlarmRuleDelete(alarmRule) {
        return await this.handleSyncOperation('alarm_delete', async () => {
            console.log(`🔄 Alarm rule deleted hook: ${alarmRule.id} (${alarmRule.name})`);

            const proxy = getCollectorProxy();
            await proxy.notifyConfigChange('alarm_rule', alarmRule.id, {
                action: 'delete',
                target_type: alarmRule.target_type,
                target_id: alarmRule.target_id
            }, 'all');
        });
    }

    // =============================================================================
    // 🔥 가상포인트 변경 후크들 (Non-Critical)
    // =============================================================================

    async afterVirtualPointCreate(virtualPoint) {
        return await this.handleSyncOperation('virtual_point_create', async () => {
            console.log(`🔄 Virtual point created hook: ${virtualPoint.id} (${virtualPoint.name})`);

            const proxy = getCollectorProxy();
            await proxy.notifyConfigChange('virtual_point', virtualPoint.id, {
                action: 'create',
                is_enabled: virtualPoint.is_enabled,
                calculation_interval: virtualPoint.calculation_interval,
                calculation_trigger: virtualPoint.calculation_trigger
            }, 'all');
        });
    }

    async afterVirtualPointUpdate(oldPoint, newPoint) {
        return await this.handleSyncOperation('virtual_point_update', async () => {
            console.log(`🔄 Virtual point updated hook: ${newPoint.id} (${newPoint.name})`);

            const proxy = getCollectorProxy();

            const changes = {
                action: 'update',
                enabled_changed: oldPoint.is_enabled !== newPoint.is_enabled,
                formula_changed: oldPoint.formula !== newPoint.formula,
                interval_changed: oldPoint.calculation_interval !== newPoint.calculation_interval,
                trigger_changed: oldPoint.calculation_trigger !== newPoint.calculation_trigger
            };

            await proxy.notifyConfigChange('virtual_point', newPoint.id, changes, 'all');
        });
    }

    async afterVirtualPointDelete(virtualPoint) {
        return await this.handleSyncOperation('virtual_point_delete', async () => {
            console.log(`🔄 Virtual point deleted hook: ${virtualPoint.id} (${virtualPoint.name})`);

            const proxy = getCollectorProxy();
            await proxy.notifyConfigChange('virtual_point', virtualPoint.id, {
                action: 'delete'
            }, 'all');
        });
    }

    // =============================================================================
    // 🔥 데이터포인트 변경 후크 (Non-Critical)
    // =============================================================================

    async afterDataPointUpdate(oldPoint, newPoint) {
        return await this.handleSyncOperation('datapoint_update', async () => {
            console.log(`🔄 Data point updated hook: ${newPoint.id} (${newPoint.point_name})`);

            // 데이터포인트가 변경되면 해당 디바이스 재시작
            if (newPoint.device_id) {
                const proxy = getCollectorProxy();
                const DeviceService = require('../services/DeviceService');

                // 디바이스 정보를 가져와서 edge_server_id 확인
                // Note: 순환 참조 주의. 여기서는 DeviceService가 이미 로드되어 있을 확률이 높음.
                try {
                    const device = await DeviceService.getDeviceById(newPoint.device_id, newPoint.tenant_id);
                    if (device) {
                        await proxy.restartDevice(newPoint.device_id.toString(), {
                            edgeServerId: device.edge_server_id
                        });
                        console.log(`✅ Device restarted for data point change: ${newPoint.device_id}`);
                    }
                } catch (error) {
                    console.warn(`⚠️ Failed to fetch device for data point sync: ${error.message}`);
                }
            }
        });
    }

    // =============================================================================
    // 🔥 설정 관리 메소드들 (기존 + 개선)
    // =============================================================================

    setErrorPolicy(policy) {
        if (['throw', 'log', 'ignore'].includes(policy)) {
            this.errorPolicy.onSyncFailure = policy;
            console.log(`🔄 Sync error policy changed to: ${policy}`);
        } else {
            console.error(`❌ Invalid error policy: ${policy}. Must be 'throw', 'log', or 'ignore'`);
        }
    }

    getErrorPolicy() {
        return { ...this.errorPolicy };
    }

    // 기존 메소드들 유지
    setEnabled(enabled) {
        this.isEnabled = enabled;
        console.log(`🔄 ConfigSyncHooks ${enabled ? 'enabled' : 'disabled'}`);
    }

    isHookEnabled() {
        return this.isEnabled;
    }

    getRegisteredHooks() {
        return Array.from(this.hooks.keys());
    }

    clearAllHooks() {
        this.hooks.clear();
        console.log('🔄 All hooks cleared');
    }

    // =============================================================================
    // 기존 후크 등록 시스템 유지
    // =============================================================================

    registerHook(entityType, eventType, callback) {
        const key = `${entityType}.${eventType}`;
        if (!this.hooks.has(key)) {
            this.hooks.set(key, []);
        }
        this.hooks.get(key).push(callback);
        console.log(`🎣 Hook registered: ${key}`);
    }

    async executeHooks(entityType, eventType, ...args) {
        const key = `${entityType}.${eventType}`;
        const callbacks = this.hooks.get(key) || [];

        for (const callback of callbacks) {
            try {
                await callback(...args);
            } catch (error) {
                console.error(`❌ Hook execution failed for ${key}:`, error.message);
            }
        }
    }
}

// 싱글톤 인스턴스
let instance = null;

function getInstance() {
    if (!instance) {
        instance = new ConfigSyncHooks();

        // 기본 후크들 등록 (모든 원본 후크 포함)
        instance.registerHook('device', 'create', instance.afterDeviceCreate.bind(instance));
        instance.registerHook('device', 'update', instance.afterDeviceUpdate.bind(instance));
        instance.registerHook('device', 'delete', instance.afterDeviceDelete.bind(instance));

        instance.registerHook('alarm_rule', 'create', instance.afterAlarmRuleCreate.bind(instance));
        instance.registerHook('alarm_rule', 'update', instance.afterAlarmRuleUpdate.bind(instance));
        instance.registerHook('alarm_rule', 'delete', instance.afterAlarmRuleDelete.bind(instance));

        instance.registerHook('virtual_point', 'create', instance.afterVirtualPointCreate.bind(instance));
        instance.registerHook('virtual_point', 'update', instance.afterVirtualPointUpdate.bind(instance));
        instance.registerHook('virtual_point', 'delete', instance.afterVirtualPointDelete.bind(instance));

        instance.registerHook('data_point', 'update', instance.afterDataPointUpdate.bind(instance));
    }

    return instance;
}

module.exports = {
    getInstance,
    ConfigSyncHooks
};