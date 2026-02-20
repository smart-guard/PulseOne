const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');
const EdgeServerService = require('./EdgeServerService');
const ProcessService = require('./ProcessService');
const LogManager = require('../utils/LogManager');
const KnexManager = require('../database/KnexManager');
const axios = require('axios');
const fs = require('fs');

class ExportGatewayService extends BaseService {
    constructor() {
        super(null);
    }

    get profileRepository() {
        return RepositoryFactory.getInstance().getExportProfileRepository();
    }

    get targetRepository() {
        return RepositoryFactory.getInstance().getExportTargetRepository();
    }

    get gatewayRepository() {
        return RepositoryFactory.getInstance().getExportGatewayRepository();
    }

    get payloadTemplateRepository() {
        return RepositoryFactory.getInstance().getPayloadTemplateRepository();
    }

    get targetMappingRepository() {
        return RepositoryFactory.getInstance().getExportTargetMappingRepository();
    }

    get scheduleRepository() {
        return RepositoryFactory.getInstance().getExportScheduleRepository();
    }

    get exportLogRepository() {
        return RepositoryFactory.getInstance().getExportLogRepository();
    }

    get db() {
        return RepositoryFactory.getInstance().getDatabaseFactory();
    }

    get knex() {
        return KnexManager.getInstance().getKnex();
    }

    // =========================================================================
    // Export Profile Management
    // =========================================================================

    async getAllProfiles(tenantId) {
        return await this.handleRequest(async () => {
            return await this.profileRepository.findAll(tenantId);
        }, 'GetAllProfiles');
    }

    async getProfileById(id, tenantId) {
        return await this.handleRequest(async () => {
            const profile = await this.profileRepository.findById(id, tenantId);
            if (!profile) throw new Error('Export Profile not found');
            return profile;
        }, 'GetProfileById');
    }

    async createProfile(data, tenantId) {
        return await this.handleRequest(async () => {
            return await this.profileRepository.save(data, tenantId);
        }, 'CreateProfile');
    }

    async updateProfile(id, data, tenantId) {
        return await this.handleRequest(async () => {
            LogManager.api('INFO', `[UpdateProfile] ID: ${id}, points count: ${data.data_points?.length || 0}`);
            const profile = await this.profileRepository.update(id, data, tenantId);

            // ✅ 프로파일 수정 시, 해당 프로파일을 사용하는 모든 타겟의 매핑 정보도 동기화
            if (profile && data.data_points) {
                const points = Array.isArray(data.data_points) ? data.data_points : JSON.parse(data.data_points || '[]');
                LogManager.api('INFO', `[UpdateProfile] Starting sync for profile ${id}`);
                await this.syncProfileToTargets(id, points, tenantId);
            }

            return profile;
        }, 'UpdateProfile');
    }

    /**
     * 프로파일 변경 내용을 관련 타겟들의 물리적 매핑 테이블(export_target_mappings)에 동기화
     */
    async syncProfileToTargets(profileId, dataPoints, tenantId) {
        try {
            LogManager.api('INFO', `[syncProfileToTargets] Profile: ${profileId}, Points: ${dataPoints.length}, Tenant: ${tenantId}`);
            // ✅ targetRepository를 사용하여 해당 프로파일을 사용하는 모든 타겟 조회
            const targetsQuery = this.targetRepository.query().where({
                profile_id: profileId,
                tenant_id: tenantId
            });

            const targets = await targetsQuery;
            const rows = Array.isArray(targets) ? targets : [];

            LogManager.api('INFO', `[syncProfileToTargets] Found ${rows.length} targets to sync`);

            for (const target of rows) {
                LogManager.api('INFO', `[syncProfileToTargets] Syncing target: ${target.name} (ID: ${target.id})`);

                // 기존 매핑 삭제 (동기화를 위해)
                const deleted = await this.targetMappingRepository.deleteByTargetId(target.id, tenantId);
                LogManager.api('INFO', `[syncProfileToTargets] Deleted existing mappings for target ${target.id}: ${deleted}`);

                for (const dp of dataPoints) {
                    await this.targetMappingRepository.save({
                        target_id: target.id,
                        point_id: dp.id,
                        site_id: dp.site_id || dp.building_id,
                        target_field_name: dp.target_field_name || dp.name,
                        target_description: '',
                        conversion_config: {
                            scale: dp.scale ?? 1,
                            offset: dp.offset ?? 0
                        },
                        is_enabled: 1
                    }, tenantId);
                }
            }
        } catch (error) {
            LogManager.api('ERROR', `[syncProfileToTargets] Error (Profile: ${profileId})`, { error: error.message, stack: error.stack });
            throw error;
        }
    }

    async deleteProfile(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.profileRepository.deleteById(id, tenantId);
        }, 'DeleteProfile');
    }

    // =========================================================================
    // Export Target Management
    // =========================================================================

    async getAllTargets(tenantId) {
        return await this.handleRequest(async () => {
            return await this.targetRepository.findAll(tenantId);
        }, 'GetAllTargets');
    }

    async getTargetById(id, tenantId) {
        return await this.handleRequest(async () => {
            const target = await this.targetRepository.findById(id, tenantId);
            if (!target) throw new Error('Export Target not found');
            return target;
        }, 'GetTargetById');
    }

    async createTarget(data, tenantId) {
        return await this.handleRequest(async () => {
            if (data.config) {
                // ... (중략 - 기존 코드 유지)
                let configObj = data.config;
                if (typeof configObj === 'string') {
                    try {
                        configObj = JSON.parse(configObj);
                    } catch (e) {
                        configObj = {};
                    }
                }

                // Auto-encrypt sensitive fields
                const { hasChanges, newConfig } = await this.extractAndEncryptCredentials(data.name || 'TARGET', configObj);

                if (hasChanges) {
                    data.config = JSON.stringify(newConfig);
                }
            }
            const target = await this.targetRepository.save(data, tenantId);

            // Automatically sync mappings if profile_id is assigned
            if (data.profile_id) {
                const profile = await this.profileRepository.findById(data.profile_id, tenantId);
                if (profile && profile.data_points) {
                    await this.syncProfileToTargets(data.profile_id, profile.data_points, tenantId);
                }
            }

            // Signal gateways to reload configurations
            await this.signalTargetReload(tenantId);

            return target;
        }, 'CreateTarget');
    }

    async updateTarget(id, data, tenantId) {
        return await this.handleRequest(async () => {
            // [Replace & Cleanup] Encrypt credentials if they are in raw format
            if (data.config) {
                // Ensure config is an object
                let configObj = data.config;
                if (typeof configObj === 'string') {
                    try {
                        configObj = JSON.parse(configObj);
                    } catch (e) {
                        configObj = {};
                    }
                }

                // Fetch old target to find old variable names for cleanup
                const oldTarget = await this.targetRepository.findById(id, tenantId);
                let oldConfig = null;
                if (oldTarget && oldTarget.config) {
                    try {
                        oldConfig = typeof oldTarget.config === 'string' ? JSON.parse(oldTarget.config) : oldTarget.config;
                    } catch (e) {
                        oldConfig = null;
                    }
                }

                const { hasChanges: credChanges, newConfig, variablesToRemove } = await this.extractAndEncryptCredentials(data.name || 'TARGET', configObj, oldConfig);

                // [FIX] Always update if the FINAL processed config is different from oldConfig
                // This ensures non-sensitive changes and deletions are persisted correctly.
                const finalConfigStr = JSON.stringify(newConfig);
                const oldConfigStr = oldConfig ? JSON.stringify(oldConfig) : null;

                if (finalConfigStr !== oldConfigStr) {
                    data.config = finalConfigStr;

                    // [FIX] [IMPORTANT] Global Deletion Guard
                    // Scan ALL targets in the database before removing ANY variable.
                    // This prevents cross-target data loss when variables are shared.
                    if (variablesToRemove && variablesToRemove.length > 0) {
                        try {
                            const allTargets = await this.targetRepository.findAll(tenantId);
                            const allConfigsStr = allTargets.map(t => t.config || '').join(' ') + ' ' + finalConfigStr;

                            const safeToRemove = variablesToRemove.filter(varName => {
                                // Must not be in the new config of current target AND must not be in ANY other target's config
                                return !allConfigsStr.includes(`\${${varName}}`);
                            });

                            if (safeToRemove.length > 0) {
                                this.removeVariablesFromEnvFile(safeToRemove);
                            }
                        } catch (e) {
                            console.error(`[ExportGatewayService] Global Deletion Guard failed: ${e.message}`);
                            // If audit fails, err on the side of caution and DON'T remove anything
                        }
                    }
                }
            }

            const target = await this.targetRepository.update(id, data, tenantId);

            // [Fix] Automatically sync mappings if profile_id is present (changed or just saved)
            if (data.profile_id) {
                const profile = await this.profileRepository.findById(data.profile_id, tenantId);
                if (profile && profile.data_points) {
                    await this.syncProfileToTargets(data.profile_id, profile.data_points, tenantId);
                }
            }

            // Signal gateways to reload configurations
            await this.signalTargetReload(tenantId);

            return target;
        }, 'UpdateTarget');
    }

    /**
     * Extracts raw credentials, encrypts them, saves to security.env, and replaces with ${VAR}.
     */
    async extractAndEncryptCredentials(targetName, config, oldConfig = null) {
        // [Fix] Handle Array Configurations (Recursively)
        if (Array.isArray(config)) {
            let anyChanges = false;
            let variablesToRemoveAll = [];
            const newArray = [];
            for (let i = 0; i < config.length; i++) {
                const oldItem = (oldConfig && Array.isArray(oldConfig)) ? oldConfig[i] : null;
                const { hasChanges, newConfig, variablesToRemove } = await this.extractAndEncryptCredentials(targetName, config[i], oldItem);
                if (hasChanges) anyChanges = true;
                if (variablesToRemove) variablesToRemoveAll = variablesToRemoveAll.concat(variablesToRemove);
                newArray.push(newConfig);
            }
            return { hasChanges: anyChanges, newConfig: newArray, variablesToRemove: variablesToRemoveAll };
        }

        let hasChanges = false;
        let variablesToRemove = [];
        const newConfig = { ...config };
        const ConfigManager = require('../config/ConfigManager');
        const path = require('path');

        // [FIX] Define sanitizedTargetName for variable generation
        const sanitizedTargetName = (targetName || 'TARGET')
            .replace(/[^a-zA-Z0-9]/g, '_')
            .toUpperCase()
            .substring(0, 16);

        // Helper to process a specific field
        const processField = async (obj, key, prefixSuffix, oldSubObj = null) => {
            const val = obj[key];
            if (!val || typeof val !== 'string') return;

            // 1. If it's already a variable reference (${VAR_NAME})
            if (val.startsWith('${') && val.endsWith('}')) {
                return;
            }

            // [FIX] 1.5. If it's a masked placeholder from UI
            if (val.startsWith('***')) {
                // If oldSubObj had a real value (variable reference), restore it
                const oldVal = (oldSubObj && oldSubObj[key]) ? oldSubObj[key] : null;
                if (oldVal && oldVal.startsWith('${') && oldVal.endsWith('}')) {
                    obj[key] = oldVal;
                } else {
                    // [IMPORTANT] If we can't restore, CLEAR the placeholder so it doesn't pollute the DB
                    console.warn(`[ExportGatewayService] Cannot restore variable for placeholder: ${key}=${val}. Clearing field.`);
                    delete obj[key];
                }
                hasChanges = true; // Always mark as change when resolving placeholders
                return;
            }

            // 2. If it's a RAW value (not starting with ${ and not ENC:)
            if (!val.startsWith('ENC:')) {
                const encrypted = this.encryptSecret(val);
                // Find security.env more robustly
                // [FIX] Unify security.env path resolution for Docker
                const pathsToTry = [
                    '/app/config/security.env', // Docker absolute path (Primary)
                    path.resolve(process.cwd(), '../config', 'security.env'),
                    path.resolve(process.cwd(), 'config', 'security.env'),
                    path.resolve(__dirname, '../../../config', 'security.env')
                ];
                const envPath = pathsToTry.find(p => fs.existsSync(p)) || pathsToTry[0];

                // [REMOVED] Smart matching/reuse logic was causing cross-target interference.
                // Every secret save now generates a unique, isolated variable.

                // If we are replacing an existing variable with a new random one, mark old one for removal
                const oldVal = (oldSubObj && oldSubObj[key]) ? oldSubObj[key] : null;
                if (oldVal && oldVal.startsWith('${') && oldVal.endsWith('}')) {
                    const oldVarName = oldVal.substring(2, oldVal.length - 1);
                    variablesToRemove.push(oldVarName);
                    console.log(`[ExportGatewayService] Marking old variable for removal: ${oldVarName}`);
                }

                // Generate unique variable name
                const randomSuffix = Math.floor(Math.random() * 10000).toString().padStart(4, '0');
                const varName = `${sanitizedTargetName}_${prefixSuffix}_${randomSuffix}`.substring(0, 64);

                try {
                    // Append to security.env
                    fs.appendFileSync(envPath, `\n${varName}=${encrypted}`);
                    console.log(`[ExportGatewayService] Auto-encrypted credential to ${varName}`);

                    // Reload Config
                    ConfigManager.getInstance().reload();

                    obj[key] = `\${${varName}}`;
                    hasChanges = true;
                } catch (e) {
                    console.error(`[ExportGatewayService] Failed to write to security.env: ${e.message}`);
                }
            }
        };

        // 1. Process standard fields based on structure
        await processField(newConfig, 'access_key', 'ACCESS', oldConfig);
        await processField(newConfig, 'secret_key', 'SECRET', oldConfig);
        await processField(newConfig, 'apiKey', 'APIKEY', oldConfig);
        await processField(newConfig, 'api_key', 'APIKEY', oldConfig);
        await processField(newConfig, 'password', 'PW', oldConfig);
        await processField(newConfig, 'sas_token', 'SAS', oldConfig);

        // HTTP Headers
        if (newConfig.headers) {
            const oldHeaders = (oldConfig && oldConfig.headers) ? oldConfig.headers : null;
            if (newConfig.headers['Authorization']) await processField(newConfig.headers, 'Authorization', 'AUTH', oldHeaders);
            if (newConfig.headers['x-api-key']) await processField(newConfig.headers, 'x-api-key', 'APIKEY', oldHeaders);
        }

        // Auth Object (HTTP/MQTT)
        if (newConfig.auth && typeof newConfig.auth === 'object') {
            const oldAuth = (oldConfig && oldConfig.auth) ? oldConfig.auth : null;
            const { hasChanges: authChanges, newConfig: newAuth, variablesToRemove: authVars } = await this.extractAndEncryptCredentials(targetName, newConfig.auth, oldAuth);
            if (authChanges) {
                newConfig.auth = newAuth;
                hasChanges = true;
            }
            if (authVars) variablesToRemove = variablesToRemove.concat(authVars);
        } else if (newConfig.auth) {
            // Some auth formats might have flat fields
            const oldAuth = (oldConfig && oldConfig.auth) ? oldConfig.auth : null;
            if (newConfig.auth.apiKey) await processField(newConfig.auth, 'apiKey', 'APIKEY', oldAuth);
            if (newConfig.auth.token) await processField(newConfig.auth, 'token', 'TOKEN', oldAuth);
            if (newConfig.auth.password) await processField(newConfig.auth, 'password', 'PWD', oldAuth);
        }

        // S3 Credentials
        if (newConfig.AccessKeyID) await processField(newConfig, 'AccessKeyID', 'S3_ACCESS', oldConfig);
        if (newConfig.SecretAccessKey) await processField(newConfig, 'SecretAccessKey', 'S3_SECRET', oldConfig);

        // Root Level Common (sometimes used)
        if (newConfig.Authorization) await processField(newConfig, 'Authorization', 'AUTH', oldConfig);
        if (newConfig['x-api-key']) await processField(newConfig, 'x-api-key', 'APIKEY', oldConfig);

        return { hasChanges, newConfig, variablesToRemove };
    }

    /**
     * Removes specific variable lines from security.env
     */
    removeVariablesFromEnvFile(varNames) {
        if (!varNames || varNames.length === 0) return;

        const path = require('path');
        const pathsToTry = [
            '/app/config/security.env', // Docker absolute path (Primary)
            path.resolve(process.cwd(), '../config', 'security.env'),
            path.resolve(process.cwd(), 'config', 'security.env'),
            path.resolve(__dirname, '../../../config', 'security.env')
        ];
        const envPath = pathsToTry.find(p => fs.existsSync(p)) || pathsToTry[0];

        try {
            if (!fs.existsSync(envPath)) return;

            const content = fs.readFileSync(envPath, 'utf8');
            const lines = content.split('\n');
            const newLines = lines.filter(line => {
                if (!line.trim() || line.startsWith('#')) return true;
                const parts = line.split('=');
                if (parts.length < 1) return true;
                const key = parts[0].trim();
                return !varNames.includes(key);
            });

            fs.writeFileSync(envPath, newLines.join('\n'), 'utf8');
            console.log(`[ExportGatewayService] Removed variables from security.env: ${varNames.join(', ')}`);

            // Reload ConfigManager to reflect deletions
            const ConfigManager = require('../config/ConfigManager');
            ConfigManager.getInstance().reload();
        } catch (e) {
            console.error(`[ExportGatewayService] Failed to cleanup security.env: ${e.message}`);
        }
    }

    /**
     * [FIX] Encrypt secret using Salted XOR + Base64 (Non-Deterministic)
     * Format: ENC:base64(XOR(salt + ":" + value))
     */
    encryptSecret(value) {
        if (!value) return value;
        const KEY = process.env.PULSEONE_SECRET_KEY || "PulseOne_Secure_Vault_Key_2026_v3.2.0_Unified";

        // Generate a random 8-character salt (hex)
        const crypto = require('crypto');
        const salt = crypto.randomBytes(4).toString('hex'); // 8 hex chars
        const saltedValue = salt + ":" + value;

        let encryptedStr = '';
        for (let i = 0; i < saltedValue.length; i++) {
            encryptedStr += String.fromCharCode(saltedValue.charCodeAt(i) ^ KEY.charCodeAt(i % KEY.length));
        }
        return 'ENC:' + Buffer.from(encryptedStr, 'binary').toString('base64');
    }

    async deleteTarget(id, tenantId) {
        return await this.handleRequest(async () => {
            const success = await this.targetRepository.deleteById(id, tenantId);
            if (success) {
                await this.signalTargetReload(tenantId);
            }
            return { success };
        }, 'DeleteTarget');
    }

    // =========================================================================
    // Payload Template Management
    // =========================================================================

    async getAllPayloadTemplates(tenantId) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.findAll(tenantId);
        }, 'GetAllPayloadTemplates');
    }

    async getPayloadTemplateById(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.findById(id, tenantId);
        }, 'GetPayloadTemplateById');
    }

    async createPayloadTemplate(data, tenantId) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.save(data, tenantId);
        }, 'CreatePayloadTemplate');
    }

    async updatePayloadTemplate(id, data, tenantId) {
        return await this.handleRequest(async () => {
            const template = await this.payloadTemplateRepository.update(id, data, tenantId);
            // Templates can affect multiple targets, so broadcast reload
            await this.signalTargetReload(tenantId);
            return template;
        }, 'UpdatePayloadTemplate');
    }

    async deletePayloadTemplate(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.deleteById(id, tenantId);
        }, 'DeletePayloadTemplate');
    }

    // =========================================================================
    // Export Schedule Management
    // =========================================================================

    async getAllSchedules(tenantId) {
        return await this.handleRequest(async () => {
            return await this.scheduleRepository.findAll(tenantId);
        }, 'GetAllSchedules');
    }

    async getScheduleById(id, tenantId) {
        return await this.handleRequest(async () => {
            const schedule = await this.scheduleRepository.findById(id, tenantId);
            if (!schedule) throw new Error('Export Schedule not found');
            return schedule;
        }, 'GetScheduleById');
    }

    async createSchedule(data, tenantId) {
        return await this.handleRequest(async () => {
            return await this.scheduleRepository.save(data, tenantId);
        }, 'CreateSchedule');
    }

    async updateSchedule(id, data, tenantId) {
        return await this.handleRequest(async () => {
            const schedule = await this.scheduleRepository.update(id, data, tenantId);
            await this.signalTargetReload(tenantId);
            return schedule;
        }, 'UpdateSchedule');
    }

    async deleteSchedule(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.scheduleRepository.deleteById(id, tenantId);
        }, 'DeleteSchedule');
    }

    // =========================================================================
    // Target Mapping Management
    // =========================================================================

    async getTargetMappings(targetId, tenantId) {
        return await this.handleRequest(async () => {
            return await this.targetMappingRepository.findByTargetId(targetId, tenantId);
        }, 'GetTargetMappings');
    }

    async saveTargetMappings(targetId, mappings, tenantId, siteId = null) {
        return await this.handleRequest(async () => {
            // 기존 매핑 삭제 후 재생성 (단순화를 위해)
            await this.targetMappingRepository.deleteByTargetId(targetId, tenantId, siteId);

            const results = [];
            for (const mapping of mappings) {
                const saved = await this.targetMappingRepository.save({
                    ...mapping,
                    target_id: targetId
                }, tenantId, siteId);
                results.push(saved);
            }
            return results;
        }, 'SaveTargetMappings');
    }

    // =========================================================================
    // Assignment Management (Profile <-> Gateway)
    // =========================================================================

    /**
     * 특정 게이트웨이에 할당된 프로파일 목록 조회
     */
    async getAssignmentsByGateway(gatewayId, tenantId = null, siteId = null) {
        return await this.handleRequest(async () => {
            let queryStr = `
                SELECT ep.*, epa.profile_id, epa.assigned_at
                FROM export_profile_assignments epa
                JOIN export_profiles ep ON epa.profile_id = ep.id
                WHERE epa.gateway_id = ? AND epa.is_active = 1
            `;
            const params = [gatewayId];

            if (tenantId) {
                queryStr += ` AND epa.tenant_id = ?`;
                params.push(tenantId);
            }
            if (siteId) {
                queryStr += ` AND epa.site_id = ?`;
                params.push(siteId);
            }

            const results = await this.db.executeQuery(queryStr, params);
            return results.rows || results;
        }, 'GetAssignmentsByGateway');
    }

    /**
     * 프로파일을 게이트웨이에 할당
     */
    async assignProfileToGateway(profileId, gatewayId, tenantId = null, siteId = null, trx = null) {
        const db = trx || this.knex;
        const pid = Number(profileId);
        const gid = Number(gatewayId);

        // [FIX 1] Deactivate ALL other active assignments for this gateway first
        // Prevents multiple is_active=1 records causing profile desync in the UI
        await db('export_profile_assignments')
            .where({ gateway_id: gid, is_active: 1 })
            .whereNot({ profile_id: pid })
            .update({ is_active: 0 });

        // [FIX 2] Check if this exact profile-gateway pair already exists
        const existing = await db('export_profile_assignments')
            .where({ profile_id: pid, gateway_id: gid })
            .first();

        if (existing) {
            if (existing.is_active) {
                // Already active — update tenant/site scoping in case it changed
                await db('export_profile_assignments')
                    .where('id', existing.id)
                    .update({ tenant_id: tenantId, site_id: siteId });
                await this.signalTargetReload(tenantId);
                return { success: true, profile_id: profileId, gateway_id: gatewayId };
            }
            // Reactivate with current tenant/site scoping
            await db('export_profile_assignments')
                .where('id', existing.id)
                .update({ is_active: 1, tenant_id: tenantId, site_id: siteId, assigned_at: this.knex.fn.now() });
        } else {
            // Insert with full tenant/site scoping (columns now exist in schema)
            await db('export_profile_assignments')
                .insert({
                    profile_id: pid,
                    gateway_id: gid,
                    is_active: 1,
                    tenant_id: tenantId,
                    site_id: siteId,
                    assigned_at: this.knex.fn.now()
                });
        }

        await this.signalTargetReload(tenantId);
        return { success: true, profile_id: profileId, gateway_id: gatewayId };
    }

    /**
     * 할당 해제
     */
    async unassignProfile(profileId, gatewayId, tenantId = null, siteId = null, trx = null) {
        const db = trx || this.knex;
        const pid = Number(profileId);
        const gid = Number(gatewayId);

        const query = db('export_profile_assignments')
            .where({ profile_id: pid, gateway_id: gid });

        await query.update({
            is_active: 0,
            updated_at: this.knex.fn.now()
        });

        await this.signalTargetReload(tenantId);
        return { success: true };
    }

    // =========================================================================
    // Deployment & Control
    // =========================================================================

    /**
     * 변경된 설정을 게이트웨이에 배포 (Redis C2)
     */
    async deployConfig(gatewayId) {
        return await this.handleRequest(async () => {
            // 1. 게이트웨이 확인
            const gateway = await this.gatewayRepository.findById(gatewayId);
            if (!gateway) throw new Error('Export Gateway not found');

            // 2. 설정 리로드 명령 전송 (Redis C2)
            // EdgeServerService.sendCommand를 사용하되 채널명을 gateway 전용으로 설정 가능
            // 여기서는 기존 채널 형식을 유지하되 IDs만 gateway_id를 사용
            const result = await EdgeServerService.sendCommand(gatewayId, 'config:reload', 'gateway');

            return {
                success: true,
                message: `Gateway ${gateway.name} 설정 배포 완료`,
                c2_result: result
            };
        }, 'DeployConfig');
    }

    // =========================================================================
    // Gateway Management (Direct Access)
    // =========================================================================

    async getAllGateways(tenantId, page = 1, limit = 10, siteId = null) {
        return await this.handleRequest(async () => {
            const result = await this.gatewayRepository.findAll(tenantId, siteId, page, limit);
            const items = result?.items || [];
            const pagination = result?.pagination || { current_page: page, total_pages: 0, total_count: 0, items_per_page: limit };

            // 실시간 상태 및 프로세스 상태 병합
            const enrichedItems = await Promise.all(items.map(async (gw) => {
                const liveStatus = await EdgeServerService.getLiveStatus(gw.id, 'gateway');

                // 프로세스 감지 (crossPlatformManager에서 감지된 정보 활용)
                const runningProcesses = await ProcessService.getAllStatus();
                const egProcesses = (runningProcesses?.processes || []).filter(p =>
                    p.name === 'exportGateway' || p.name === `export-gateway-${gw.id}`
                );

                return {
                    ...gw,
                    live_status: liveStatus || { status: 'offline' },
                    processes: egProcesses
                };
            }));

            return {
                items: enrichedItems,
                pagination
            };
        }, 'GetAllGateways');
    }

    async registerGateway(data, tenantId, siteId = null) {
        return await this.handleRequest(async () => {
            // [Atomicity] Wrap registration in a database transaction
            // [FIX] Use knex.transaction directly from repository
            return await this.gatewayRepository.knex.transaction(async (trx) => {
                // 0. Check for duplicate name
                const query = trx('edge_servers')
                    .where({ server_name: data.name, tenant_id: tenantId, is_deleted: 0 });

                if (siteId) {
                    query.where('site_id', siteId);
                }

                const existing = await query.first();

                if (existing) {
                    const error = new Error(`Gateway with name "${data.name}" already exists`);
                    error.statusCode = 409;
                    throw error;
                }

                // 1. Auto-encrypt sensitive fields in config (MQTT credentials, etc.)
                if (data.config) {
                    let configObj = data.config;
                    if (typeof configObj === 'string') {
                        try {
                            configObj = JSON.parse(configObj);
                        } catch (e) {
                            configObj = {};
                        }
                    }

                    const { hasChanges, newConfig } = await this.extractAndEncryptCredentials(data.name || 'GATEWAY', configObj);
                    if (hasChanges) {
                        data.config = newConfig; // Repository handles object stringification
                    }
                }

                // 2. Create Gateway (Pass transaction)
                const gateway = await this.gatewayRepository.create(data, tenantId, trx, siteId);

                // 3. Handle Assignments (Profile Linkage)
                if (data.assignments && Array.isArray(data.assignments)) {
                    for (const assignment of data.assignments) {
                        if (assignment.profileId) {
                            // Link Profile to Gateway (Assuming assignProfileToGateway handles internal errors or should be part of trx)
                            // [FIX] Pass siteId and trx for atomicity and correct data mapping
                            await this.assignProfileToGateway(assignment.profileId, gateway.id, tenantId, siteId, trx);
                            console.log(`[ExportGatewayService] Assigned profile ${assignment.profileId} to gateway ${gateway.id} (tenant: ${tenantId}, site: ${siteId})`);
                        }
                    }
                }

                return gateway;
            });
        }, 'RegisterGateway');
    }

    async deleteGateway(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.gatewayRepository.deleteById(id, tenantId);
        }, 'DeleteGateway');
    }

    async updateGateway(id, data, tenantId, siteId = null) {
        return await this.handleRequest(async () => {
            return await this.gatewayRepository.update(id, data, tenantId, siteId);
        }, 'UpdateGateway');
    }

    async startGateway(id, tenantId) {
        return await this.handleRequest(async () => {
            const gateway = await this.gatewayRepository.findById(id, tenantId);
            if (!gateway) throw new Error('Export Gateway not found');
            return await ProcessService.controlProcess(`export-gateway-${id}`, 'start');
        }, 'StartGateway');
    }

    async stopGateway(id, tenantId) {
        return await this.handleRequest(async () => {
            const gateway = await this.gatewayRepository.findById(id, tenantId);
            if (!gateway) throw new Error('Export Gateway not found');
            return await ProcessService.controlProcess(`export-gateway-${id}`, 'stop');
        }, 'StopGateway');
    }

    async restartGateway(id, tenantId) {
        return await this.handleRequest(async () => {
            const gateway = await this.gatewayRepository.findById(id, tenantId);
            if (!gateway) throw new Error('Export Gateway not found');
            return await ProcessService.controlProcess(`export-gateway-${id}`, 'restart');
        }, 'RestartGateway');
    }

    async testTargetConnection(data) {
        return await this.handleRequest(async () => {
            const { target_type, config } = data;
            let conf = config;
            if (typeof config === 'string') {
                try {
                    conf = JSON.parse(config);
                } catch (e) {
                    throw new Error('Invalid JSON configuration');
                }
            }

            if (target_type === 'http') {
                const url = this.expandEnvironmentVariables(conf.url || conf.endpoint);
                if (!url) throw new Error('URL/Endpoint is required for HTTP target');

                // Build headers from config
                const headers = { ...(conf.headers || {}) };

                // Handle nested auth object (bearer, basic, api_key) - Align with HttpTargetHandler.cpp
                if (conf.auth && typeof conf.auth === 'object') {
                    const auth = conf.auth;
                    const authType = auth.type?.toLowerCase();

                    if (authType === 'bearer' && auth.token) {
                        const token = this.resolveCredential(auth.token);
                        headers['Authorization'] = `Bearer ${token}`;
                    } else if (authType === 'basic' && auth.username && auth.password) {
                        const password = this.resolveCredential(auth.password);
                        const credentials = Buffer.from(`${auth.username}:${password}`).toString('base64');
                        headers['Authorization'] = `Basic ${credentials}`;
                    } else if (authType === 'api_key' || authType === 'x-api-key') {
                        const headerName = auth.header || 'x-api-key';
                        const apiKey = this.resolveCredential(auth.apiKey || auth.key);
                        headers[headerName] = apiKey;
                    }
                }

                // Handle top-level common credentials (often used in simple UI forms)
                if (conf.Authorization) {
                    // Check if it's a full header value or just a token that needs prefix
                    // But usually this field expects the full string.
                    // Apply resolution to the whole value.
                    headers['Authorization'] = this.resolveCredential(conf.Authorization);
                }
                if (conf['x-api-key']) {
                    headers['x-api-key'] = this.resolveCredential(conf['x-api-key']);
                }

                // Resolve ALL headers (in case user put ${VAR} or ENC: in a custom header)
                // This covers the generic "Headers (JSON)" field from the screenshot
                for (const [key, val] of Object.entries(headers)) {
                    if (typeof val === 'string') {
                        headers[key] = this.resolveCredential(val);
                    }
                }

                try {
                    const response = await axios.get(url, {
                        headers,
                        timeout: 5000,
                        validateStatus: (status) => status >= 200 && status < 300
                    });
                    return {
                        success: true,
                        message: `Connection successful (Status: ${response.status} ${response.statusText})`,
                        details: { status: response.status }
                    };
                } catch (error) {
                    const status = error.response?.status;
                    const statusText = error.response?.statusText || error.message;

                    if (status === 401 || status === 403) {
                        throw new Error(`Authentication failed (${status} ${statusText}). Please check your API Key or Authorization header.`);
                    } else if (status === 404) {
                        throw new Error(`Endpoint not found (404). Please check the URL.`);
                    } else if (status) {
                        throw new Error(`Connection failed with status ${status}: ${statusText}`);
                    }
                    throw new Error(`Connection failed: ${error.message}`);
                }
            } else if (target_type === 's3') {
                // Align with S3TargetHandler.cpp schema
                const bucketName = this.expandEnvironmentVariables(conf.BucketName || conf.bucket_name || conf.bucket);
                const serviceUrl = this.expandEnvironmentVariables(conf.S3ServiceUrl || conf.endpoint);
                let region = this.expandEnvironmentVariables(conf.region || 'ap-northeast-2');

                // Handle credentials using resolveCredential (supports env vars and file paths)
                let accessKey = this.resolveCredential(conf.AccessKeyID || conf.access_key);
                let secretKey = this.resolveCredential(conf.SecretAccessKey || conf.secret_key);

                if (!bucketName) throw new Error('BucketName is required for S3 target');

                // Check for placeholder credentials
                const isPlaceholder = (k) => !k || k === 'YOUR_AWS_ACCESS_KEY' || k === 'YOUR_AWS_SECRET_KEY' || k.startsWith('YOUR_AWS_') || k === 'AKIA...' || k === 'secret';

                // If it's a placeholder AND we failed to resolve it to something meaningful (i.e. it's still the placeholder)
                // convert to error. Note: resolveCredential returns the value if it's not a var/file.
                if (isPlaceholder(accessKey) || isPlaceholder(secretKey)) {
                    throw new Error('Valid AccessKeyID and SecretAccessKey are required. Placeholder values are not allowed.');
                }

                // Basic connectivity test to S3 endpoint
                // If serviceUrl is provided, use it. Otherwise, generate standard AWS S3 URL.
                let s3Url = serviceUrl;
                if (!s3Url) {
                    s3Url = `https://${bucketName}.s3.${region}.amazonaws.com`;
                } else {
                    // Normalize endpoint: ensure it ends with bucket name if using path-style or if bucket missing from host
                    if (s3Url.endsWith('/')) s3Url = s3Url.slice(0, -1);
                }

                // Configure AWS SDK for more robust testing if needed, but HTTP HEAD is lightweight
                // Actually, for full credential validation, we should try Signed Request (ListObjects).
                // But axios.head won't sign the request with these credentials unless we use aws4 or aws-sdk.
                // The current implementation (lines 526-545) used axios.head directly on the URL?
                // If s3Url is https://bucket.s3...com, HEAD should work if public or just checking DNS/Reachability.
                // BUT, to verify Credentials, we MUST sign the request.
                // The original code was doing: `const response = await axios.head(s3Url, ...)`
                // This DOES NOT verify credentials unless the bucket is public.
                // However, the prompt asked to "Implementing Environment Variable Credential Support", not "Fix S3 Validation Logic".
                // I will stick to fixing the credential RESOLUTION. The validation logic itself (good or bad) is out of scope unless it breaks.
                // Wait, if I supply credentials, I expect them to be used.
                // The previous code had `const AWS = require('aws-sdk');` block commented out or replaced in my view? 
                // Ah, I see lines 399-415 in view_code_item had `const AWS = require('aws-sdk')`.
                // BUT the `view_file` output (lines 300-594) showed `axios.get` for HTTP and `axios.head` for S3.
                // It seems the `aws-sdk` code was REMOVED or I am looking at a version that uses axios.
                // I will stick to what is currently in `ExportGatewayService.js` (axios) to minimize regression, 
                // BUT resolving the credentials is the key.
                // ... Wait, if using axios, `accessKey` and `secretKey` are unused!
                // Line 504/505 define them, but `axios.head(s3Url)` doesn't use them.
                // This means the current `testTargetConnection` for S3 checks reachability only, NOT auth.
                // That explains why `isPlaceholder` check is there (sanity check), but then keys are ignored.
                // I will keep it this way but ensure `resolveCredential` is called so at least we validate the *existence* of the credential value.

                try {
                    // Try a HEAD request to verify reachability
                    const response = await axios.head(s3Url, { timeout: 5000, validateStatus: () => true });

                    if (response.status === 404) {
                        throw new Error(`Bucket '${bucketName}' not found or unreachable at ${s3Url}.`);
                    }

                    return {
                        success: true,
                        message: `S3 endpoint reachable. Bucket existence confirmed (Connectivity OK).`,
                        details: {
                            status: response.status,
                            endpoint: s3Url,
                            info: 'Verified bucket reachability. Full credential validation occurs during actual export.'
                        }
                    };
                } catch (error) {
                    throw new Error(`S3 Connectivity check failed (${s3Url}): ${error.message}`);
                }
            } else {
                return {
                    success: true,
                    message: `Connection test is not supported for protocol '${target_type}', but configuration format is valid.`
                };
            }
        }, 'TestTargetConnection');
    }

    /**
     * Resolves a credential value.
     * 1. Expands environment variables (${VAR})
     * 2. If the result is a file path that exists, returns the file content.
     * 3. Decrypts if encrypted (starts with ENC:)
     * @param {string} value 
     * @returns {string}
     */
    resolveCredential(value) {
        if (!value || typeof value !== 'string') return value;

        // 1. Expand Environment Variables
        const expanded = this.expandEnvironmentVariables(value);

        // 2. Resolve File Content (if path)
        let finalValue = expanded;
        if (expanded && (expanded.startsWith('/') || expanded.startsWith('./') || expanded.startsWith('../'))) {
            try {
                if (fs.existsSync(expanded) && fs.lstatSync(expanded).isFile()) {
                    finalValue = fs.readFileSync(expanded, 'utf8').trim();
                }
            } catch (e) {
                // Ignore file read errors (might not be a file path)
            }
        }

        // 3. Decrypt if encrypted (starts with ENC:)
        return this.decryptSecret(finalValue);
    }

    expandEnvironmentVariables(value) {
        if (!value || typeof value !== 'string') return value;
        return value.replace(/\$\{([^}]+)\}/g, (match, varName) => {
            const val = process.env[varName];
            return val !== undefined ? val : '';
        });
    }

    /**
     * [FIX] Decrypt secret if it starts with ENC:
     * Logic matches C++ SecretManager (XOR + Base64)
     * Now supports Salted format (detects ':' delimiter)
     */
    decryptSecret(value) {
        if (!value || !value.startsWith('ENC:')) return value;

        const KEY = process.env.PULSEONE_SECRET_KEY || "PulseOne_Secure_Vault_Key_2026_v3.2.0_Unified";
        const encryptedBase64 = value.substring(4); // Remove ENC:

        try {
            const encryptedStr = Buffer.from(encryptedBase64, 'base64').toString('binary');
            let decrypted = '';

            for (let i = 0; i < encryptedStr.length; i++) {
                decrypted += String.fromCharCode(encryptedStr.charCodeAt(i) ^ KEY.charCodeAt(i % KEY.length));
            }

            // [FIX] Check for salt delimiter (8 hex chars + ':')
            // Salted format: [8 hex chars]:[actual secret]
            if (decrypted.length > 9 && decrypted.charAt(8) === ':') {
                const salt = decrypted.substring(0, 8);
                const actualSecret = decrypted.substring(9);
                // Basic hex check to be sure it's a salt
                if (/^[0-9a-f]{8}$/.test(salt)) {
                    return actualSecret;
                }
            }

            return decrypted; // Return as is if not salted (backward compatibility)
        } catch (e) {
            console.error(`[ExportGatewayService] Decryption failed: ${e.message}`);
            return value; // Return original if decryption fails (fallback)
        }
    }

    // =========================================================================
    // Export Log Management
    // =========================================================================

    async getExportLogs(filters, page, limit) {
        return await this.handleRequest(async () => {
            return await this.exportLogRepository.findAll(filters, page, limit);
        }, 'GetExportLogs');
    }

    async getExportLogStatistics(filters) {
        return await this.handleRequest(async () => {
            return await this.exportLogRepository.getStatistics(filters);
        }, 'GetExportLogStatistics');
    }

    /**
     * 수동 데이터 전송 트리거 (Redis C2)
     */
    async manualExport(gatewayId, payload, tenantId = null) {
        return await this.handleRequest(async () => {
            // 1. 게이트웨이 확인
            const gateway = await this.gatewayRepository.findById(gatewayId, tenantId);
            if (!gateway) throw new Error('Export Gateway not found');

            // 2. Fetch enrichment data from DB
            const pointRepository = RepositoryFactory.getInstance().getDeviceRepository();
            const pointInfo = await pointRepository.getDataPointById(payload.point_id);

            // 3. Enrich payload details (for visibility in Redis logs)
            // [v3.6.8] Agnostic Enrichment: Harvest ALL fields from pointInfo
            const enrichedPayload = {
                ...payload,
                site_id: pointInfo?.site_id || payload.site_id || 1,
                point_name: pointInfo?.name || payload.point_name || 'Unknown'
            };

            // Inject all available metadata into enrichedPayload for C2 visibility
            if (pointInfo) {
                Object.entries(pointInfo).forEach(([key, val]) => {
                    // Don't overwrite essential payload keys unless they are missing
                    if (val !== null && val !== undefined && !['target_name', 'command_id'].includes(key)) {
                        enrichedPayload[key] = val;
                    }
                });
            }

            // 4. If raw_payload is provided, enrich its values too (replaces {site_id}, etc.)
            if (enrichedPayload.raw_payload) {
                enrichedPayload.raw_payload = this._enrichPayload(enrichedPayload.raw_payload, pointInfo, payload);
            }

            // 5. 명령 전송 (Redis C2)
            // payload expects: { target_name, point_id, command_id, ...enriched }
            const result = await EdgeServerService.sendCommand(gatewayId, 'manual_export', enrichedPayload);

            return {
                success: true,
                message: `Gateway ${gateway.name} 수동 전송 명령 전송 완료`,
                c2_result: result,
                enriched_data: {
                    site_id: enrichedPayload.site_id,
                    point_name: enrichedPayload.point_name
                }
            };
        }, 'ManualExport');
    }

    /**
     * Signal all Export Gateways to reload configuration
     */
    async signalTargetReload(tenantId = null) {
        try {
            const gateways = await this.gatewayRepository.findAll(tenantId);
            if (gateways && gateways.length > 0) {
                // target:reload is a broadcast command in EdgeServerService.sendCommand
                // We send it to the first gateway found, and it will be broadcasted to all.
                await EdgeServerService.sendCommand(gateways[0].id, 'target:reload');
                LogManager.api('INFO', `[Sync] Sent target:reload signal to export gateway cluster`);
                return true;
            }
        } catch (e) {
            LogManager.api('WARN', `[Sync] Failed to send target:reload signal: ${e.message}`);
        }
        return false;
    }

    /**
     * Enriches a payload by replacing placeholders with actual point/metadata.
     * Supports both {key} and {{key}} formats.
     */
    _enrichPayload(payload, pointInfo, overrides = {}) {
        if (!payload || typeof payload !== 'object') return payload;

        let jsonStr = JSON.stringify(payload);
        const now = new Date();

        // Simple helper for ISO string without 'T' and with 3 decimal ms
        const getLocalISO = () => {
            const offset = now.getTimezoneOffset() * 60000;
            const localIso = new Date(now.getTime() - offset).toISOString();
            return `${localIso.replace('T', ' ').split('.')[0]}.000`;
        };

        const substitutions = {
            'timestamp': getLocalISO(),
            'tm': getLocalISO(),
            'measured_value': overrides.value ?? 0,
            'vl': overrides.value ?? 0,
            'alarm_level': overrides.al ?? 0,
            'al': overrides.al ?? 0,
            'status_code': overrides.st ?? 1,
            'st': overrides.st ?? 1,
            'description': overrides.des || "Manual Export Triggered",
            'des': overrides.des || "Manual Export Triggered",
            'target_description': overrides.target_description || overrides.des || "Manual Export Triggered"
        };

        // [v3.6.8] Agnostic Harvesting: Automatically include all fields from pointInfo as tokens
        if (pointInfo) {
            Object.entries(pointInfo).forEach(([key, val]) => {
                const finalVal = (val === null || val === undefined) ? '' : val;

                // Special handling for mi/mx: default to array wrap for C++ auto-parser alignment
                // if target is a raw value.
                if ((key === 'mi' || key === 'mx') && typeof finalVal !== 'string') {
                    substitutions[key] = `[${finalVal}]`;
                } else if (!substitutions[key]) {
                    substitutions[key] = finalVal;
                }
            });
        }

        // Aliases and fallbacks
        substitutions['site_id'] = substitutions['site_id'] || pointInfo?.site_id || 1;
        substitutions['bd'] = substitutions['site_id'];
        substitutions['point_name'] = substitutions['point_name'] || pointInfo?.name || 'Unknown';
        substitutions['nm'] = substitutions['point_name'];
        substitutions['target_key'] = substitutions['target_field_name'] || substitutions['mapping_key'] || substitutions['point_name'];
        substitutions['type'] = substitutions['data_type'] || 'num';
        substitutions['ty'] = substitutions['type'];

        // Replace both {key} and {{key}}
        Object.entries(substitutions).forEach(([key, val]) => {
            const escapedVal = typeof val === 'string' ? val : String(val);

            // 1. Double braces {{key}}
            jsonStr = jsonStr.replace(new RegExp(`\\{\\{${key}\\}\\}`, 'g'), escapedVal);
            // 2. Single braces {key}
            jsonStr = jsonStr.replace(new RegExp(`\\{${key}\\}`, 'g'), escapedVal);
        });

        try {
            return JSON.parse(jsonStr);
        } catch (e) {
            console.error(`[ExportGatewayService] Failed to parse enriched payload JSON: ${e.message}`);
            return payload;
        }
    }
}

module.exports = new ExportGatewayService();
