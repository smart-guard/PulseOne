const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');
const EdgeServerService = require('./EdgeServerService');
const ProcessService = require('./ProcessService');
const LogManager = require('../utils/LogManager');
const axios = require('axios');

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

    get db() {
        return RepositoryFactory.getInstance().getDatabaseFactory();
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
            return await this.profileRepository.update(id, data, tenantId);
        }, 'UpdateProfile');
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
            return await this.targetRepository.save(data, tenantId);
        }, 'CreateTarget');
    }

    async updateTarget(id, data, tenantId) {
        return await this.handleRequest(async () => {
            return await this.targetRepository.update(id, data, tenantId);
        }, 'UpdateTarget');
    }

    async deleteTarget(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.targetRepository.deleteById(id, tenantId);
        }, 'DeleteTarget');
    }

    // =========================================================================
    // Payload Template Management
    // =========================================================================

    async getAllPayloadTemplates() {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.findAll();
        }, 'GetAllPayloadTemplates');
    }

    async getPayloadTemplateById(id) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.findById(id);
        }, 'GetPayloadTemplateById');
    }

    async createPayloadTemplate(data) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.save(data);
        }, 'CreatePayloadTemplate');
    }

    async updatePayloadTemplate(id, data) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.update(id, data);
        }, 'UpdatePayloadTemplate');
    }

    async deletePayloadTemplate(id) {
        return await this.handleRequest(async () => {
            return await this.payloadTemplateRepository.deleteById(id);
        }, 'DeletePayloadTemplate');
    }

    // =========================================================================
    // Export Schedule Management
    // =========================================================================

    async getAllSchedules() {
        return await this.handleRequest(async () => {
            return await this.scheduleRepository.findAll();
        }, 'GetAllSchedules');
    }

    async getScheduleById(id) {
        return await this.handleRequest(async () => {
            const schedule = await this.scheduleRepository.findById(id);
            if (!schedule) throw new Error('Export Schedule not found');
            return schedule;
        }, 'GetScheduleById');
    }

    async createSchedule(data) {
        return await this.handleRequest(async () => {
            return await this.scheduleRepository.save(data);
        }, 'CreateSchedule');
    }

    async updateSchedule(id, data) {
        return await this.handleRequest(async () => {
            return await this.scheduleRepository.update(id, data);
        }, 'UpdateSchedule');
    }

    async deleteSchedule(id) {
        return await this.handleRequest(async () => {
            return await this.scheduleRepository.deleteById(id);
        }, 'DeleteSchedule');
    }

    // =========================================================================
    // Target Mapping Management
    // =========================================================================

    async getTargetMappings(targetId) {
        return await this.handleRequest(async () => {
            return await this.targetMappingRepository.findByTargetId(targetId);
        }, 'GetTargetMappings');
    }

    async saveTargetMappings(targetId, mappings) {
        return await this.handleRequest(async () => {
            // 기존 매핑 삭제 후 재생성 (단순화를 위해)
            await this.targetMappingRepository.deleteByTargetId(targetId);

            const results = [];
            for (const mapping of mappings) {
                const saved = await this.targetMappingRepository.save({
                    ...mapping,
                    target_id: targetId
                });
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
    async getAssignmentsByGateway(gatewayId) {
        return await this.handleRequest(async () => {
            const query = `
                SELECT ep.*, epa.profile_id, epa.assigned_at
                FROM export_profile_assignments epa
                JOIN export_profiles ep ON epa.profile_id = ep.id
                WHERE epa.gateway_id = ? AND epa.is_active = 1
            `;
            const results = await this.db.executeQuery(query, [gatewayId]);
            return results.rows || results;
        }, 'GetAssignmentsByGateway');
    }

    /**
     * 프로파일을 게이트웨이에 할당
     */
    async assignProfileToGateway(profileId, gatewayId) {
        return await this.handleRequest(async () => {
            // [Fix] Ensure IDs are numbers for SQLite compatibility
            const pid = Number(profileId);
            const gid = Number(gatewayId);

            // 1. 기존 할당 확인
            const checkQuery = `SELECT id, is_active FROM export_profile_assignments WHERE profile_id = ? AND gateway_id = ?`;
            const existing = await this.db.executeQuery(checkQuery, [pid, gid]);

            // [Fix] Handle both array (direct) and object (wrapped) return types from DatabaseFactory
            const rows = existing.rows || (Array.isArray(existing) ? existing : []);

            if (rows.length > 0) {
                // [Modified] If already active, throw error so frontend can show "Duplicate" popup
                if (rows[0].is_active) {
                    const error = new Error('Profile is already assigned to this gateway');
                    error.statusCode = 409;
                    throw error;
                }

                // If exists but inactive (soft deleted), reactivate it
                await this.db.executeQuery(
                    `UPDATE export_profile_assignments SET assigned_at = CURRENT_TIMESTAMP, is_active = 1 WHERE id = ?`,
                    [rows[0].id]
                );
            } else {
                // 신규 할당 (UNIQUE 인덱스가 있어도 애플리케이션 레벨에서 먼저 체크)
                await this.db.executeQuery(
                    `INSERT INTO export_profile_assignments (profile_id, gateway_id, is_active) VALUES (?, ?, 1)`,
                    [profileId, gatewayId]
                );
            }

            return { success: true, profile_id: profileId, gateway_id: gatewayId };
        }, 'AssignProfileToGateway');
    }

    /**
     * 할당 해제
     */
    async unassignProfile(profileId, gatewayId) {
        return await this.handleRequest(async () => {
            // 현재 프론트엔드는 토글 방식을 사용하므로 데이터 정합성을 위해 삭제 처리
            const query = `DELETE FROM export_profile_assignments WHERE profile_id = ? AND gateway_id = ?`;
            await this.db.executeQuery(query, [profileId, gatewayId]);
            return { success: true };
        }, 'UnassignProfile');
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

    async getAllGateways(tenantId, page = 1, limit = 10) {
        return await this.handleRequest(async () => {
            const result = await this.gatewayRepository.findAll(tenantId, page, limit);
            const items = result?.items || [];
            const pagination = result?.pagination || { current_page: page, total_pages: 0, total_count: 0 };

            // 실시간 상태 및 프로세스 상태 병합
            const enrichedItems = await Promise.all(items.map(async (gw) => {
                const liveStatus = await EdgeServerService.getLiveStatus(gw.id);

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

    async registerGateway(data, tenantId) {
        return await this.handleRequest(async () => {
            return await this.gatewayRepository.create(data, tenantId);
        }, 'RegisterGateway');
    }

    async deleteGateway(id, tenantId) {
        return await this.handleRequest(async () => {
            return await this.gatewayRepository.deleteById(id, tenantId);
        }, 'DeleteGateway');
    }

    async updateGateway(id, data, tenantId) {
        return await this.handleRequest(async () => {
            return await this.gatewayRepository.update(id, data, tenantId);
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
                if (!conf.url) throw new Error('URL is required for HTTP target');
                try {
                    const response = await axios.get(conf.url, {
                        headers: conf.headers || {},
                        timeout: 5000,
                        validateStatus: (status) => status >= 200 && status < 300 // Only 2xx is success
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
                if (!conf.bucket || !conf.region) throw new Error('Bucket and Region are required for S3 target');

                // Check for placeholder credentials
                const isPlaceholder = (k) => !k || k === 'YOUR_AWS_ACCESS_KEY' || k === 'YOUR_AWS_SECRET_KEY' || k.startsWith('YOUR_AWS_');
                if (isPlaceholder(conf.accessKey) || isPlaceholder(conf.secretKey)) {
                    throw new Error('Valid AWS Access Key and Secret Key are required. Placeholder values are not allowed.');
                }

                // Basic connectivity test to S3 endpoint
                const s3Url = `https://${conf.bucket}.s3.${conf.region}.amazonaws.com`;
                try {
                    const response = await axios.head(s3Url, { timeout: 5000, validateStatus: () => true });
                    // S3 returns 404 if bucket doesn't exist.
                    // 403 (Forbidden) is common without full SDK signing but confirms bucket exists and is reachable.
                    if (response.status === 404) {
                        throw new Error(`Bucket '${conf.bucket}' not found or unreachable in region '${conf.region}'.`);
                    }

                    return {
                        success: true,
                        message: `S3 endpoint reachable. Bucket existence confirmed (Connectivity OK).`,
                        details: {
                            status: response.status,
                            info: 'Verified bucket reachability. Full credential validation requires the actual export process.'
                        }
                    };
                } catch (error) {
                    throw new Error(`S3 Connectivity check failed: ${error.message}`);
                }
            } else {
                return {
                    success: true,
                    message: `Connection test is not supported for protocol '${target_type}', but configuration format is valid.`
                };
            }
        }, 'TestTargetConnection');
    }
}

module.exports = new ExportGatewayService();
