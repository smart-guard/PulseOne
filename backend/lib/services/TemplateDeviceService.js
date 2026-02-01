const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');
const protocolValidationService = require('./ProtocolValidationService');

/**
 * TemplateDeviceService class
 * Handles business logic for device templates and instantiation.
 */
class TemplateDeviceService extends BaseService {
    constructor() {
        super(null);
    }

    get repository() {
        if (!this._repository) {
            this._repository = RepositoryFactory.getInstance().getTemplateDeviceRepository();
        }
        return this._repository;
    }

    get templateSettingsRepo() {
        if (!this._templateSettingsRepo) {
            this._templateSettingsRepo = RepositoryFactory.getInstance().getTemplateDeviceSettingsRepository();
        }
        return this._templateSettingsRepo;
    }

    get templatePointRepo() {
        if (!this._templatePointRepo) {
            this._templatePointRepo = RepositoryFactory.getInstance().getTemplateDataPointRepository();
        }
        return this._templatePointRepo;
    }

    get deviceRepo() {
        if (!this._deviceRepo) {
            this._deviceRepo = RepositoryFactory.getInstance().getDeviceRepository();
        }
        return this._deviceRepo;
    }

    get protocolRepo() {
        if (!this._protocolRepo) {
            this._protocolRepo = RepositoryFactory.getInstance().getProtocolRepository();
        }
        return this._protocolRepo;
    }

    get auditLogService() {
        if (!this._auditLogService) {
            const AuditLogService = require('./AuditLogService');
            this._auditLogService = new AuditLogService();
        }
        return this._auditLogService;
    }

    /**
     * 모든 템플릿 목록을 조회합니다.
     */
    async getAllTemplates(options = {}) {
        return await this.handleRequest(async () => {
            return await this.repository.findAll(options);
        }, 'GetAllTemplates');
    }

    /**
     * ID로 템플릿을 상세 조회합니다 (설정 및 데이터 포인트 포함).
     */
    async getTemplateDetail(id) {
        return await this.handleRequest(async () => {
            const template = await this.repository.findById(id);
            if (!template) {
                throw new Error(`템플릿을 찾을 수 없습니다. (ID: ${id})`);
            }

            const settings = await this.templateSettingsRepo.findByTemplateId(id);
            const points = await this.templatePointRepo.findByTemplateId(id);

            return {
                ...template,
                settings,
                data_points: points
            };
        }, 'GetTemplateDetail');
    }

    /**
     * 템플릿을 사용하여 새로운 디바이스를 생성(인스턴스화)합니다.
     */
    async instantiateFromTemplate(templateId, deviceData, tenantId, user = null) {
        return await this.handleRequest(async () => {
            const template = await this.getTemplateDetail(templateId);
            if (!template.success) {
                throw new Error(template.message);
            }
            const templateData = template.data;

            // System Admin인 경우 body의 tenant_id를 우선 사용 (DeviceService.createDevice와 동일 로직)
            const targetTenantId = (user && user.role === 'system_admin' && deviceData.tenant_id)
                ? deviceData.tenant_id
                : (tenantId || deviceData.tenant_id || 1); // Fallback to 1 for safety in dev

            if (!targetTenantId) {
                throw new Error('Tenant ID is required for device instantiation');
            }

            const instResult = await this.transaction(async (trx) => {
                // 한도 체크: 추가될 데이터 포인트 수 계산
                const sourcePoints = (deviceData.data_points && deviceData.data_points.length > 0)
                    ? deviceData.data_points
                    : (templateData.data_points || []);
                const pointsToAdd = sourcePoints.length;

                const DeviceService = require('./DeviceService');
                await DeviceService.validateDataPointLimit(targetTenantId, pointsToAdd, trx);

                // config 병합 로직
                const templateConfig = typeof templateData.config === 'string' ? JSON.parse(templateData.config) : (templateData.config || {});
                const overrideConfig = deviceData.config || {};
                const finalConfig = { ...templateConfig, ...overrideConfig };

                // 최종 설정 유효성 검증
                const configValidation = protocolValidationService.validateConfig(templateData.protocol_type, finalConfig);
                if (configValidation.length > 0) {
                    throw new Error(`설정 검증 실패: ${configValidation[0].message}`);
                }

                // Check for duplicate device (Same Site, Endpoint, and ID)
                const existingDevices = await trx('devices')
                    .where({
                        site_id: deviceData.site_id,
                        endpoint: deviceData.endpoint
                    });

                const newId = finalConfig.slave_id || finalConfig.device_id; // Support Modbus Slave ID & BACnet Device ID

                if (newId) {
                    const isDuplicate = existingDevices.some(d => {
                        try {
                            const conf = typeof d.config === 'string' ? JSON.parse(d.config) : d.config;
                            const oldId = conf.slave_id || conf.device_id;
                            return String(oldId) === String(newId);
                        } catch (e) {
                            return false;
                        }
                    });

                    if (isDuplicate) {
                        throw new Error(`이미 해당 Endpoint에 ID(${newId})를 사용하는 디바이스가 존재합니다.`);
                    }
                }

                // 1. 디바이스 기본 정보 생성
                const newDevice = {
                    tenant_id: targetTenantId,
                    site_id: deviceData.site_id,
                    device_group_id: deviceData.device_group_id || null,
                    name: deviceData.name,
                    description: deviceData.description || templateData.description,
                    device_type: templateData.device_type || 'PLC',
                    manufacturer: templateData.manufacturer_name,
                    model: templateData.model_name,
                    protocol_id: templateData.protocol_id,
                    endpoint: deviceData.endpoint,
                    edge_server_id: deviceData.edge_server_id || null,
                    config: JSON.stringify(finalConfig),
                    polling_interval: deviceData.polling_interval || templateData.polling_interval || 1000,
                    timeout: deviceData.timeout || templateData.timeout || 5000,
                    retry_count: deviceData.retry_count || 3,
                    is_enabled: deviceData.is_enabled !== undefined ? deviceData.is_enabled : 1,
                    tags: typeof deviceData.tags === 'object' ? JSON.stringify(deviceData.tags) : (deviceData.tags || null),
                    metadata: typeof deviceData.metadata === 'object' ? JSON.stringify(deviceData.metadata) : (deviceData.metadata || null),
                    custom_fields: typeof deviceData.custom_fields === 'object' ? JSON.stringify(deviceData.custom_fields) : (deviceData.custom_fields || null)
                };

                const results = await trx('devices').insert(newDevice);
                const deviceId = results[0];

                // 2. 디바이스 상태 초기화
                await trx('device_status').insert({
                    device_id: deviceId,
                    connection_status: 'disconnected',
                    error_count: 0
                });

                // 3. 데이터 포인트 복사 (변수는 위에서 이미 선언됨)
                if (sourcePoints.length > 0) {
                    const pointsToInsert = sourcePoints.map(tp => ({
                        device_id: deviceId,
                        name: tp.name,
                        description: tp.description,
                        address: tp.address,
                        address_string: tp.address_string,
                        // Fix: Normalize data_type to match schema constraints (BOOLEAN -> BOOL)
                        data_type: tp.data_type === 'BOOLEAN' ? 'BOOL' : tp.data_type,
                        access_mode: tp.access_mode,
                        unit: tp.unit,
                        scaling_factor: tp.scaling_factor,
                        scaling_offset: tp.scaling_offset,
                        is_writable: tp.is_writable,
                        is_enabled: 1,
                        // Collector/Engineering Fields
                        log_enabled: tp.log_enabled !== undefined ? (tp.log_enabled ? 1 : 0) : 1,
                        log_interval_ms: tp.log_interval_ms || 0,
                        log_deadband: tp.log_deadband || 0.0,
                        is_alarm_enabled: tp.is_alarm_enabled !== undefined ? (tp.is_alarm_enabled ? 1 : 0) : 0,
                        high_alarm_limit: tp.high_alarm_limit,
                        low_alarm_limit: tp.low_alarm_limit,
                        alarm_deadband: tp.alarm_deadband || 0.0,
                        group_name: tp.group_name || tp.group || null,
                        tags: typeof tp.tags === 'object' ? JSON.stringify(tp.tags) : tp.tags,
                        metadata: typeof tp.metadata === 'object' ? JSON.stringify(tp.metadata) : tp.metadata,
                        protocol_params: typeof tp.protocol_params === 'object' ? JSON.stringify(tp.protocol_params) : tp.protocol_params
                    }));

                    await trx('data_points').insert(pointsToInsert);
                }

                // 4. 스마트 설정 전파 (device_settings 테이블)
                // (위에서 파싱한 templateConfig 재사용)
                const deviceSettings = {
                    device_id: deviceId,
                    polling_interval_ms: deviceData.polling_interval || templateData.polling_interval || 1000,
                    connection_timeout_ms: deviceData.timeout || templateData.timeout || 5000,
                    read_timeout_ms: deviceData.read_timeout_ms || deviceData.read_timeout || deviceData.timeout || templateData.timeout || 5000,
                    write_timeout_ms: deviceData.write_timeout_ms || deviceData.write_timeout || deviceData.timeout || templateData.timeout || 5000,
                    max_retry_count: deviceData.retry_count ?? templateConfig.max_retry_count ?? 3,
                    retry_interval_ms: deviceData.retry_interval_ms || deviceData.retry_interval || templateConfig.retry_interval_ms || 5000,
                    backoff_time_ms: deviceData.backoff_time_ms || deviceData.backoff_time || templateConfig.backoff_time_ms || 60000,
                    backoff_multiplier: deviceData.backoff_multiplier || templateConfig.backoff_multiplier || 1.5,
                    max_backoff_time_ms: deviceData.max_backoff_time_ms || templateConfig.max_backoff_time_ms || 300000,
                    is_keep_alive_enabled: deviceData.is_keep_alive_enabled !== undefined ? (deviceData.is_keep_alive_enabled ? 1 : 0) : (deviceData.is_keep_alive !== undefined ? (deviceData.is_keep_alive ? 1 : 0) : (templateConfig.is_keep_alive_enabled ?? 1)),
                    keep_alive_interval_s: deviceData.keep_alive_interval_s || deviceData.ka_interval || templateConfig.keep_alive_interval_s || 30,
                    keep_alive_timeout_s: deviceData.keep_alive_timeout_s || templateConfig.keep_alive_timeout_s || 10,
                    is_data_validation_enabled: deviceData.is_data_validation_enabled !== undefined ? (deviceData.is_data_validation_enabled ? 1 : 0) : (deviceData.is_data_validation !== undefined ? (deviceData.is_data_validation ? 1 : 0) : 0),
                    is_performance_monitoring_enabled: deviceData.is_performance_monitoring_enabled !== undefined ? (deviceData.is_performance_monitoring_enabled ? 1 : 0) : (deviceData.is_performance_monitoring !== undefined ? (deviceData.is_performance_monitoring ? 1 : 0) : 0),
                    is_detailed_logging_enabled: deviceData.is_detailed_logging_enabled !== undefined ? (deviceData.is_detailed_logging_enabled ? 1 : 0) : (deviceData.is_detailed_logging !== undefined ? (deviceData.is_detailed_logging ? 1 : 0) : 0),
                    is_diagnostic_mode_enabled: deviceData.is_diagnostic_mode_enabled !== undefined ? (deviceData.is_diagnostic_mode_enabled ? 1 : 0) : (deviceData.is_diagnostic_mode !== undefined ? (deviceData.is_diagnostic_mode ? 1 : 0) : 0),
                    is_communication_logging_enabled: deviceData.is_communication_logging_enabled !== undefined ? (deviceData.is_communication_logging_enabled ? 1 : 0) : (deviceData.is_comm_logging !== undefined ? (deviceData.is_comm_logging ? 1 : 0) : 0),
                    // Missing Advanced Fields
                    read_buffer_size: deviceData.read_buffer_size || templateConfig.read_buffer_size || 1024,
                    write_buffer_size: deviceData.write_buffer_size || templateConfig.write_buffer_size || 1024,
                    queue_size: deviceData.queue_size || templateConfig.queue_size || 100,
                    is_outlier_detection_enabled: deviceData.is_outlier_detection_enabled !== undefined ? (deviceData.is_outlier_detection_enabled ? 1 : 0) : 0,
                    is_deadband_enabled: deviceData.is_deadband_enabled !== undefined ? (deviceData.is_deadband_enabled ? 1 : 0) : 1
                };

                await trx('device_settings').insert(deviceSettings);

                return {
                    device_id: deviceId,
                    name: deviceData.name,
                    point_count: templateData.data_points ? templateData.data_points.length : 0
                };
            });

            // Audit Log
            if (user && instResult) {
                await this.auditLogService.logAction({
                    tenant_id: tenantId,
                    user_id: user.id,
                    action: 'INSTANTIATE',
                    entity_type: 'DEVICE',
                    entity_id: instResult.device_id,
                    entity_name: instResult.name,
                    details: { template_id: templateId },
                    change_summary: `Device instantiated from template: ${templateData.name}`
                });
            }

            return instResult;
        }, 'InstantiateFromTemplate');
    }

    /**
     * 새로운 템플릿을 생성합니다.
     */
    async createTemplate(data, user = null) {
        return await this.handleRequest(async () => {
            // Validate protocol and data points
            const protocol = await this.protocolRepo.findById(data.protocol_id);
            if (!protocol) throw new Error('올바르지 않은 프로토콜 ID입니다.');

            const validation = protocolValidationService.validateTemplate(
                protocol.protocol_type,
                typeof data.config === 'string' ? JSON.parse(data.config) : data.config,
                data.data_points
            );

            if (!validation.isValid) {
                const errorLog = validation.errors.map(e => e.message).join(', ');
                throw new Error(`템플릿 검증 실패: ${errorLog}`);
            }

            return await this.transaction(async (trx) => {
                const template = await this.repository.create(data, trx);

                // Handle data points if provided
                if (data.data_points && data.data_points.length > 0) {
                    const pointsToInsert = data.data_points.map(p => ({
                        template_device_id: template.id,
                        name: p.name,
                        description: p.description || null,
                        address: p.address,
                        data_type: p.data_type,
                        access_mode: p.access_mode,
                        unit: p.unit || null,
                        scaling_factor: p.scaling_factor || 1,
                        scaling_offset: p.scaling_offset || 0,
                        metadata: p.metadata ? (typeof p.metadata === 'object' ? JSON.stringify(p.metadata) : p.metadata) : null
                    }));
                    await trx('template_data_points').insert(pointsToInsert);
                }

                // Audit Log
                if (user && template) {
                    await this.auditLogService.logChange(user, 'CREATE',
                        { type: 'TEMPLATE', id: template.id, name: template.name },
                        null, template, 'New device template created');
                }

                return await this.repository.findById(template.id, trx);
            });
        }, 'CreateTemplate');
    }

    /**
     * 템플릿에 데이터 포인트를 추가합니다.
     */
    async addDataPoint(templateId, pointData) {
        return await this.handleRequest(async () => {
            pointData.template_device_id = templateId;
            return await this.templatePointRepo.create(pointData);
        }, 'AddTemplateDataPoint');
    }

    /**
     * 템플릿 정보를 업데이트합니다.
     */
    async updateTemplate(id, data, user = null) {
        return await this.handleRequest(async () => {
            const oldTemplate = await this.repository.findById(id);
            if (!oldTemplate) throw new Error('Template not found');

            // Validate if data points or protocol are being changed
            const protocolId = data.protocol_id || oldTemplate.protocol_id;
            const protocol = await this.protocolRepo.findById(protocolId);

            const validation = protocolValidationService.validateTemplate(
                protocol.protocol_type,
                data.config ? (typeof data.config === 'string' ? JSON.parse(data.config) : data.config) : oldTemplate.config,
                data.data_points === undefined ? oldTemplate.data_points : data.data_points
            );

            if (!validation.isValid) {
                const errorLog = validation.errors.map(e => e.message).join(', ');
                throw new Error(`템플릿 업데이트 검증 실패: ${errorLog}`);
            }

            return await this.transaction(async (trx) => {
                const template = await this.repository.update(id, data, trx);

                // Handle data points update if provided (Replace all for simplicity)
                if (data.data_points !== undefined) {
                    await trx('template_data_points').where('template_device_id', id).del();

                    if (data.data_points.length > 0) {
                        const pointsToInsert = data.data_points.map(p => ({
                            template_device_id: id,
                            name: p.name,
                            description: p.description || null,
                            address: p.address,
                            data_type: p.data_type,
                            access_mode: p.access_mode,
                            unit: p.unit || null,
                            scaling_factor: p.scaling_factor || 1,
                            scaling_offset: p.scaling_offset || 0,
                            metadata: p.metadata ? (typeof p.metadata === 'object' ? JSON.stringify(p.metadata) : p.metadata) : null
                        }));
                        await trx('template_data_points').insert(pointsToInsert);
                    }
                }

                // Audit Log
                if (user && template) {
                    await this.auditLogService.logChange(user, 'UPDATE',
                        { type: 'TEMPLATE', id: template.id, name: template.name },
                        oldTemplate, template, 'Device template updated');
                }

                const result = await this.repository.findById(id, trx);
                return result;
            });
        }, 'UpdateTemplate');
    }

    /**
     * 템플릿을 삭제합니다.
     */
    async deleteTemplate(id, tenantId, user = null) {
        return await this.handleRequest(async () => {
            const template = await this.repository.findById(id);
            if (!template) throw new Error('Template not found');

            // 0. Check for active device usage
            // Assuming deviceRepo has count or query capability. If not, use internal Knex query via repository.
            // Using repository's underlying knex if accessible, or deviceRepo.

            // First try to use deviceRepo if it has a count method, otherwise use direct knex query via this.repository.knex
            const [{ count }] = await this.repository.knex('devices')
                .where('template_device_id', id)
                .count('* as count');

            if (count > 0) {
                throw new Error(`이 템플릿을 사용 중인 디바이스가 ${count}개 존재합니다. 먼저 디바이스에서 해제해주세요.`);
            }

            // Soft delete transaction
            return await this.transaction(async (trx) => {
                // Soft delete the template
                // Note: We do NOT delete points or settings so they can be recovered/viewed later if needed.
                // The repository.deleteById method has been updated to perform a soft delete (UPDATE is_deleted=1).

                // Use repository's deleteById which now handles soft delete logic
                const success = await this.repository.deleteById(id);

                if (success && user) {
                    await this.auditLogService.logAction({
                        tenant_id: tenantId,
                        user_id: user.id,
                        action: 'DELETE',
                        entity_type: 'TEMPLATE',
                        entity_id: id,
                        entity_name: template.name,
                        old_value: template,
                        change_summary: 'Device template soft deleted'
                    });
                }

                if (!success) {
                    throw new Error(`템플릿 삭제에 실패했습니다. (ID: ${id})`);
                }
                return { id, success: true };
            });
        }, 'DeleteTemplate');
    }
}

module.exports = TemplateDeviceService;
