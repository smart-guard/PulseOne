// ============================================================================
// frontend/src/api/services/exportGatewayApi.ts
// Export Gateway (Edge Server) & Configuration Management API
// ============================================================================

import { apiClient, ApiResponse } from '../client';

// =============================================================================
// Interfaces
// =============================================================================

export interface Gateway {
    id: number;
    tenant_id: number;
    name: string;
    server_name?: string; // Optional for compatibility
    factory_name?: string;
    ip_address: string;
    port?: number;
    status: string;
    last_seen: string | null;
    last_heartbeat?: string;
    description?: string;
    version?: string;
    subscription_mode?: 'all' | 'selective';

    live_status?: {
        id: number;
        status: string;  // 'online' | 'offline' | 'running' 등
        cpu?: string;
        ram?: string;
        ups?: string;
        last_heartbeat?: string;
        memory_usage?: number; // compat
    };

    processes?: any;
    config?: any;
    site_id?: number; // [NEW] Site ID
}

export interface ExportProfile {
    id: number;
    tenant_id?: number | null;
    name: string;
    description: string;
    data_points: any[]; // JSON
    is_enabled: boolean;
    created_at: string;
}

export interface ExportTarget {
    id: number;
    tenant_id?: number | null;
    name: string;
    target_type: string;
    description: string;
    config: any;
    is_enabled: boolean;
    export_mode: string;
    export_interval: number;
    batch_size: number;
    execution_delay_ms: number;
    created_at: string;
    updated_at: string;
}

export interface PayloadTemplate {
    id: number;
    tenant_id?: number | null;
    name: string;
    system_type: string;
    description: string;
    template_json: any;
    is_active: boolean;
    created_at: string;
    updated_at: string;
}

export interface ExportTargetMapping {
    id: number;
    target_id: number;
    point_id: number;
    site_id: number; // [RESTORE] Canonical identifier
    target_field_name: string;
    target_description: string;
    conversion_config: any;
    is_enabled: boolean;
    created_at: string;
}

export interface Assignment {
    id: number;
    profile_id: number;
    server_id: number;
    is_active: boolean;
    assigned_at: string;
    name?: string; // Profile Name (Joined)
}

export interface ExportSchedule {
    id: number;
    profile_id?: number;
    target_id?: number | null;
    schedule_name: string;
    description: string;
    cron_expression: string;
    timezone: string;
    data_range: 'minute' | 'hour' | 'day';
    lookback_periods: number;
    is_enabled: boolean;
    last_run_at: string | null;
    last_status: string | null;
    next_run_at: string | null;
    total_runs: number;
    successful_runs: number;
    failed_runs: number;
    created_at: string;
    updated_at: string;

    // UI Only
    profile_name?: string;
    target_name?: string;
}

export interface ExportLog {
    id: number;
    log_type: string;
    service_id: number;
    target_id: number;
    target_name?: string;
    mapping_id: number;
    point_id: number;
    source_value: string;
    converted_value: string;
    status: 'success' | 'failure' | 'pending';
    http_status_code: number;
    error_message: string;
    error_code: string;
    response_data: string;
    processing_time_ms: number;
    timestamp: string;
    profile_name?: string;
    gateway_name?: string;
}

export interface ExportLogStatistics {
    total: number;
    success: number;
    failure: number;
}

export interface DataPoint {
    id: number;
    name: string;
    device_name: string;
    site_id: number; // [RESTORE] Canonical identifier
    site_name: string;
    data_type: string;
    unit: string;
    address?: string; // PLC Address (e.g., 40001)
    latest_value?: any; // [NEW] For UI testing
}

// =============================================================================
// API Service Class
// =============================================================================

export class ExportGatewayApiService {
    private static readonly GATEWAY_URL = '/api/export/gateways';
    private static readonly EXPORT_URL = '/api/export';

    // -------------------------------------------------------------------------
    // Gateways
    // -------------------------------------------------------------------------
    static async getGateways(params: { page?: number; limit?: number; siteId?: number | null; tenantId?: number | null } = {}): Promise<ApiResponse<{ items: Gateway[]; pagination: any }>> {
        return apiClient.get<any>(this.GATEWAY_URL, params);
    }

    static async getGatewayById(id: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<Gateway>> {
        return apiClient.get<Gateway>(`${this.GATEWAY_URL}/${id}`, { siteId, tenantId });
    }

    static async registerGateway(data: Partial<Gateway>, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<Gateway>> {
        return apiClient.post<Gateway>(this.GATEWAY_URL, { ...data, site_id: siteId || data.site_id, tenant_id: tenantId });
    }

    static async updateGateway(id: number, data: Partial<Gateway>, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<Gateway>> {
        return apiClient.put<Gateway>(`${this.GATEWAY_URL}/${id}`, { ...data, site_id: siteId, tenant_id: tenantId });
    }

    static async deleteGateway(id: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.GATEWAY_URL}/${id}?siteId=${siteId || ''}&tenantId=${tenantId || ''}`);
    }

    // -------------------------------------------------------------------------
    // Commands & Deploy
    // -------------------------------------------------------------------------
    static async sendCommand(id: number, command: string, payload: any = {}): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.GATEWAY_URL}/${id}/command`, { command, payload });
    }

    static async deployConfig(id: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/deploy?siteId=${siteId || ''}&tenantId=${tenantId || ''}`);
    }

    static async startGatewayProcess(id: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/start?siteId=${siteId || ''}&tenantId=${tenantId || ''}`);
    }

    static async stopGatewayProcess(id: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/stop?siteId=${siteId || ''}&tenantId=${tenantId || ''}`);
    }

    static async restartGatewayProcess(id: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/restart?siteId=${siteId || ''}&tenantId=${tenantId || ''}`);
    }

    static async triggerManualExport(gatewayId: number, payload: {
        target_name: string;
        target_id?: number;
        point_id: number;
        command_id?: string;
        value?: number;
        [key: string]: any; // Allow extra fields like 'al', 'st', etc.
    }): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${gatewayId}/manual-export`, payload);
    }

    // -------------------------------------------------------------------------
    // Export Profiles
    // -------------------------------------------------------------------------
    static async getProfiles(params: { tenantId?: number | null } = {}): Promise<ApiResponse<ExportProfile[]>> {
        return apiClient.get<ExportProfile[]>(`${this.EXPORT_URL}/profiles`, params);
    }

    static async getProfileById(id: number, tenantId?: number | null): Promise<ApiResponse<ExportProfile>> {
        return apiClient.get<ExportProfile>(`${this.EXPORT_URL}/profiles/${id}`, { tenantId });
    }

    static async createProfile(data: Partial<ExportProfile>, tenantId?: number | null): Promise<ApiResponse<ExportProfile>> {
        return apiClient.post<ExportProfile>(`${this.EXPORT_URL}/profiles`, { ...data, tenant_id: tenantId });
    }

    static async updateProfile(id: number, data: Partial<ExportProfile>, tenantId?: number | null): Promise<ApiResponse<ExportProfile>> {
        return apiClient.put<ExportProfile>(`${this.EXPORT_URL}/profiles/${id}`, { ...data, tenant_id: tenantId });
    }

    static async deleteProfile(id: number, tenantId?: number | null): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/profiles/${id}?tenantId=${tenantId || ''}`);
    }

    // -------------------------------------------------------------------------
    // Export Targets
    // -------------------------------------------------------------------------
    static async getTargets(params: { tenantId?: number | null } = {}): Promise<ApiResponse<ExportTarget[]>> {
        return apiClient.get<ExportTarget[]>(`${this.EXPORT_URL}/targets`, params);
    }

    static async createTarget(data: Partial<ExportTarget>, tenantId?: number | null): Promise<ApiResponse<ExportTarget>> {
        return apiClient.post<ExportTarget>(`${this.EXPORT_URL}/targets`, { ...data, tenant_id: tenantId });
    }

    static async updateTarget(id: number, data: Partial<ExportTarget>, tenantId?: number | null): Promise<ApiResponse<ExportTarget>> {
        return apiClient.put<ExportTarget>(`${this.EXPORT_URL}/targets/${id}`, { ...data, tenant_id: tenantId });
    }

    static async deleteTarget(id: number, tenantId?: number | null): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/targets/${id}?tenantId=${tenantId || ''}`);
    }

    static async testTargetConnection(data: { target_type: string, config: any }): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/targets/test`, data);
    }

    // -------------------------------------------------------------------------
    // Payload Templates
    // -------------------------------------------------------------------------
    static async getTemplates(params: { tenantId?: number | null } = {}): Promise<ApiResponse<PayloadTemplate[]>> {
        return apiClient.get<PayloadTemplate[]>(`${this.EXPORT_URL}/templates`, params);
    }

    static async createTemplate(data: Partial<PayloadTemplate>, tenantId?: number | null): Promise<ApiResponse<PayloadTemplate>> {
        return apiClient.post<PayloadTemplate>(`${this.EXPORT_URL}/templates`, { ...data, tenant_id: tenantId });
    }

    static async updateTemplate(id: number, data: Partial<PayloadTemplate>, tenantId?: number | null): Promise<ApiResponse<PayloadTemplate>> {
        return apiClient.put<PayloadTemplate>(`${this.EXPORT_URL}/templates/${id}`, { ...data, tenant_id: tenantId });
    }

    static async deleteTemplate(id: number, tenantId?: number | null): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/templates/${id}?tenantId=${tenantId || ''}`);
    }

    // -------------------------------------------------------------------------
    // Target Mappings
    // -------------------------------------------------------------------------
    static async getTargetMappings(targetId: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<ExportTargetMapping[]>> {
        const url = `${this.EXPORT_URL}/targets/${targetId}/mappings`;
        return apiClient.get<ExportTargetMapping[]>(url, { siteId, tenantId });
    }

    static async saveTargetMappings(targetId: number, mappings: Partial<ExportTargetMapping>[], siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<ExportTargetMapping[]>> {
        const url = `${this.EXPORT_URL}/targets/${targetId}/mappings`;
        return apiClient.post<ExportTargetMapping[]>(url, { mappings, site_id: siteId, tenant_id: tenantId });
    }

    // -------------------------------------------------------------------------
    // Export Schedules
    // -------------------------------------------------------------------------
    static async getSchedules(params: { tenantId?: number | null } = {}): Promise<ApiResponse<ExportSchedule[]>> {
        return apiClient.get<ExportSchedule[]>(`${this.EXPORT_URL}/schedules`, params);
    }

    static async createSchedule(data: Partial<ExportSchedule>, tenantId?: number | null): Promise<ApiResponse<ExportSchedule>> {
        return apiClient.post<ExportSchedule>(`${this.EXPORT_URL}/schedules`, { ...data, tenant_id: tenantId });
    }

    static async updateSchedule(id: number, data: Partial<ExportSchedule>, tenantId?: number | null): Promise<ApiResponse<ExportSchedule>> {
        return apiClient.put<ExportSchedule>(`${this.EXPORT_URL}/schedules/${id}`, { ...data, tenant_id: tenantId });
    }

    static async deleteSchedule(id: number, tenantId?: number | null): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/schedules/${id}?tenantId=${tenantId || ''}`);
    }

    // -------------------------------------------------------------------------
    // Assignments
    // -------------------------------------------------------------------------
    static async getAssignments(gatewayId: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<Assignment[]>> {
        const url = `${this.EXPORT_URL}/gateways/${gatewayId}/assignments`;
        return apiClient.get<Assignment[]>(url, { siteId, tenantId });
    }

    static async assignProfile(gatewayId: number, profileId: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<void>> {
        const url = `${this.EXPORT_URL}/gateways/${gatewayId}/assign`;
        return apiClient.post<void>(url, { profileId, site_id: siteId, tenant_id: tenantId });
    }

    static async unassignProfile(gatewayId: number, profileId: number, siteId?: number | null, tenantId?: number | null): Promise<ApiResponse<void>> {
        const url = `${this.EXPORT_URL}/gateways/${gatewayId}/unassign`;
        return apiClient.post<void>(url, { profileId, site_id: siteId, tenant_id: tenantId });
    }

    // -------------------------------------------------------------------------
    // Export Logs
    // -------------------------------------------------------------------------
    static async getExportLogs(params: {
        target_id?: number;
        gateway_id?: number;
        status?: string;
        log_type?: string;
        date_from?: string;
        date_to?: string;
        target_type?: string;
        search_term?: string;
        page?: number;
        limit?: number;
    } = {}): Promise<ApiResponse<{ items: ExportLog[]; pagination: any }>> {
        return apiClient.get<any>(`${this.EXPORT_URL}/logs`, params);
    }

    static async getExportLogStatistics(params: {
        target_id?: number;
        gateway_id?: number;
        status?: string;
        log_type?: string;
        date_from?: string;
        date_to?: string;
        target_type?: string;
        search_term?: string;
    } = {}): Promise<ApiResponse<ExportLogStatistics>> {
        return apiClient.get<ExportLogStatistics>(`${this.EXPORT_URL}/logs/statistics`, params);
    }

    // -------------------------------------------------------------------------
    // Shared / Utility
    // -------------------------------------------------------------------------
    static async getDataPoints(searchTerm: string = '', deviceId?: number, siteId?: number | null, tenantId?: number | null): Promise<DataPoint[]> {
        try {
            const response = await apiClient.get<any>('/api/data/points', {
                search: searchTerm,
                device_id: deviceId,
                site_id: siteId,
                tenant_id: tenantId,
                limit: 500
            });

            if (!response.success || !response.data) {
                return [];
            }

            // Defensive extraction: handle both { items: [] } and direct []
            const rawPoints = response.data.items || (Array.isArray(response.data) ? response.data : []);

            return rawPoints.map((dp: any) => ({
                id: dp.id,
                name: dp.name,
                device_name: dp.device_name || dp.device_info?.name || 'Unknown Device',
                site_id: dp.site_id || dp.building_id, // [RESTORE] site_id focus
                site_name: dp.site_name || 'System',
                data_type: dp.data_type,
                unit: dp.unit || '',
                address: dp.address,
                latest_value: dp.latest_value // [NEW]
            }));
        } catch (error) {
            console.error('Failed to fetch data points:', error);
            return [];
        }
    }
}

// 명칭 통일을 위해 default export 제공 (기존 코드와의 호환성)
const exportGatewayApi = ExportGatewayApiService;
export default exportGatewayApi;
