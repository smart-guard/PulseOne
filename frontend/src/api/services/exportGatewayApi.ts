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

    live_status?: {
        id: number;
        status: 'online' | 'offline';
        cpu?: string;
        ram?: string;
        ups?: string;
        last_heartbeat?: string;
        memory_usage?: number; // compat
    };
    processes?: any;
}

export interface ExportProfile {
    id: number;
    name: string;
    description: string;
    data_points: any[]; // JSON
    is_enabled: boolean;
    created_at: string;
}

export interface ExportTarget {
    id: number;
    profile_id: number;
    name: string;
    target_type: string;
    description: string;
    config: any;
    is_enabled: boolean;
    template_id?: number;
    export_mode: string;
    export_interval: number;
    batch_size: number;
    created_at: string;
    updated_at: string;
}

export interface PayloadTemplate {
    id: number;
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
    target_id: number;
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

export interface DataPoint {
    id: number;
    name: string;
    device_name: string;
    site_name: string;
    data_type: string;
    unit: string;
    address?: string; // PLC Address (e.g., 40001)
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
    static async getGateways(params: { page?: number; limit?: number } = {}): Promise<ApiResponse<{ items: Gateway[]; pagination: any }>> {
        return apiClient.get<any>(this.GATEWAY_URL, params);
    }

    static async getGatewayById(id: number): Promise<ApiResponse<Gateway>> {
        return apiClient.get<Gateway>(`${this.GATEWAY_URL}/${id}`);
    }

    static async registerGateway(data: Partial<Gateway>): Promise<ApiResponse<Gateway>> {
        return apiClient.post<Gateway>(this.GATEWAY_URL, data);
    }

    static async deleteGateway(id: number): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.GATEWAY_URL}/${id}`);
    }

    // -------------------------------------------------------------------------
    // Commands & Deploy
    // -------------------------------------------------------------------------
    static async sendCommand(id: number, command: string, payload: any = {}): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.GATEWAY_URL}/${id}/command`, { command, payload });
    }

    static async deployConfig(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/deploy`);
    }

    static async startGatewayProcess(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/start`);
    }

    static async stopGatewayProcess(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/stop`);
    }

    static async restartGatewayProcess(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/gateways/${id}/restart`);
    }

    // -------------------------------------------------------------------------
    // Export Profiles
    // -------------------------------------------------------------------------
    static async getProfiles(): Promise<ApiResponse<ExportProfile[]>> {
        return apiClient.get<ExportProfile[]>(`${this.EXPORT_URL}/profiles`);
    }

    static async getProfileById(id: number): Promise<ApiResponse<ExportProfile>> {
        return apiClient.get<ExportProfile>(`${this.EXPORT_URL}/profiles/${id}`);
    }

    static async createProfile(data: Partial<ExportProfile>): Promise<ApiResponse<ExportProfile>> {
        return apiClient.post<ExportProfile>(`${this.EXPORT_URL}/profiles`, data);
    }

    static async updateProfile(id: number, data: Partial<ExportProfile>): Promise<ApiResponse<ExportProfile>> {
        return apiClient.put<ExportProfile>(`${this.EXPORT_URL}/profiles/${id}`, data);
    }

    static async deleteProfile(id: number): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/profiles/${id}`);
    }

    // -------------------------------------------------------------------------
    // Export Targets
    // -------------------------------------------------------------------------
    static async getTargets(): Promise<ApiResponse<ExportTarget[]>> {
        return apiClient.get<ExportTarget[]>(`${this.EXPORT_URL}/targets`);
    }

    static async createTarget(data: Partial<ExportTarget>): Promise<ApiResponse<ExportTarget>> {
        return apiClient.post<ExportTarget>(`${this.EXPORT_URL}/targets`, data);
    }

    static async updateTarget(id: number, data: Partial<ExportTarget>): Promise<ApiResponse<ExportTarget>> {
        return apiClient.put<ExportTarget>(`${this.EXPORT_URL}/targets/${id}`, data);
    }

    static async deleteTarget(id: number): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/targets/${id}`);
    }

    static async testTargetConnection(data: { target_type: string, config: any }): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${this.EXPORT_URL}/targets/test`, data);
    }

    // -------------------------------------------------------------------------
    // Payload Templates
    // -------------------------------------------------------------------------
    static async getTemplates(): Promise<ApiResponse<PayloadTemplate[]>> {
        return apiClient.get<PayloadTemplate[]>(`${this.EXPORT_URL}/templates`);
    }

    static async createTemplate(data: Partial<PayloadTemplate>): Promise<ApiResponse<PayloadTemplate>> {
        return apiClient.post<PayloadTemplate>(`${this.EXPORT_URL}/templates`, data);
    }

    static async updateTemplate(id: number, data: Partial<PayloadTemplate>): Promise<ApiResponse<PayloadTemplate>> {
        return apiClient.put<PayloadTemplate>(`${this.EXPORT_URL}/templates/${id}`, data);
    }

    static async deleteTemplate(id: number): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/templates/${id}`);
    }

    // -------------------------------------------------------------------------
    // Target Mappings
    // -------------------------------------------------------------------------
    static async getTargetMappings(targetId: number): Promise<ApiResponse<ExportTargetMapping[]>> {
        return apiClient.get<ExportTargetMapping[]>(`${this.EXPORT_URL}/targets/${targetId}/mappings`);
    }

    static async saveTargetMappings(targetId: number, mappings: Partial<ExportTargetMapping>[]): Promise<ApiResponse<ExportTargetMapping[]>> {
        return apiClient.post<ExportTargetMapping[]>(`${this.EXPORT_URL}/targets/${targetId}/mappings`, { mappings });
    }

    // -------------------------------------------------------------------------
    // Export Schedules
    // -------------------------------------------------------------------------
    static async getSchedules(): Promise<ApiResponse<ExportSchedule[]>> {
        return apiClient.get<ExportSchedule[]>(`${this.EXPORT_URL}/schedules`);
    }

    static async createSchedule(data: Partial<ExportSchedule>): Promise<ApiResponse<ExportSchedule>> {
        return apiClient.post<ExportSchedule>(`${this.EXPORT_URL}/schedules`, data);
    }

    static async updateSchedule(id: number, data: Partial<ExportSchedule>): Promise<ApiResponse<ExportSchedule>> {
        return apiClient.put<ExportSchedule>(`${this.EXPORT_URL}/schedules/${id}`, data);
    }

    static async deleteSchedule(id: number): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.EXPORT_URL}/schedules/${id}`);
    }

    // -------------------------------------------------------------------------
    // Assignments
    // -------------------------------------------------------------------------
    static async getAssignments(gatewayId: number): Promise<ApiResponse<Assignment[]>> {
        return apiClient.get<Assignment[]>(`${this.EXPORT_URL}/gateways/${gatewayId}/assignments`);
    }

    static async assignProfile(gatewayId: number, profileId: number): Promise<ApiResponse<void>> {
        return apiClient.post<void>(`${this.EXPORT_URL}/gateways/${gatewayId}/assign`, { profileId });
    }

    static async unassignProfile(gatewayId: number, profileId: number): Promise<ApiResponse<void>> {
        return apiClient.post<void>(`${this.EXPORT_URL}/gateways/${gatewayId}/unassign`, { profileId });
    }

    // -------------------------------------------------------------------------
    // Shared / Utility
    // -------------------------------------------------------------------------
    static async getDataPoints(searchTerm: string = '', deviceId?: number): Promise<DataPoint[]> {
        try {
            const response = await apiClient.get<any>('/api/data/points', {
                search: searchTerm,
                device_id: deviceId,
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
                site_name: dp.site_name || 'System',
                data_type: dp.data_type,
                unit: dp.unit || '',
                address: dp.address
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
