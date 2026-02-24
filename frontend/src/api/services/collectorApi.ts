// ============================================================================
// frontend/src/api/services/collectorApi.ts
// Collector 인스턴스(Edge Servers) 관리 API
// ============================================================================

import { apiClient, ApiResponse } from '../client';

export interface EdgeServer {
    id: number;
    tenant_id: number;
    site_id?: number;
    site_name?: string;
    name: string;          // server_name alias
    server_name: string;   // DB 실제 컬럼명
    host?: string;
    api_port?: number;
    description?: string;
    status: 'online' | 'offline' | 'active' | 'inactive' | 'pending' | 'error' | 'maintenance';
    last_heartbeat?: string;
    is_enabled?: boolean;
    device_count?: number;
    live_status?: any;
    live_stats?: any;
}

export interface RegisterCollectorRequest {
    name: string;
    host: string;
    api_port: number;
    description?: string;
}

export class CollectorApiService {
    private static readonly BASE_URL = '/api/collectors';

    // 모든 Collector 목록 조회
    static async getAllCollectors(): Promise<ApiResponse<EdgeServer[]>> {
        return apiClient.get<EdgeServer[]>(this.BASE_URL);
    }

    // 활성 Collector 목록 조회
    static async getActiveCollectors(): Promise<ApiResponse<EdgeServer[]>> {
        return apiClient.get<EdgeServer[]>(`${this.BASE_URL}/active`);
    }

    // 소속 장치별 상태 집계
    static async getCollectorHealth(id: number): Promise<ApiResponse<any>> {
        return apiClient.get<any>(`${this.BASE_URL}/${id}/health`);
    }

    // Collector 등록
    static async registerCollector(data: RegisterCollectorRequest): Promise<ApiResponse<EdgeServer>> {
        return apiClient.post<EdgeServer>(this.BASE_URL, data);
    }

    // Collector 삭제
    static async unregisterCollector(id: number): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.BASE_URL}/${id}`);
    }

    // 사이트별 Collector 목록 조회 (장치 수 포함)
    static async getCollectorsBySite(siteId: number): Promise<ApiResponse<EdgeServer[]>> {
        return apiClient.get<EdgeServer[]>(`${this.BASE_URL}/by-site/${siteId}`);
    }

    // Collector 사이트 재배정 (연결 장치 0개일 때만 가능)
    static async reassignCollector(collectorId: number, newSiteId: number): Promise<ApiResponse<EdgeServer>> {
        return apiClient.patch<EdgeServer>(`${this.BASE_URL}/${collectorId}/reassign`, { site_id: newSiteId });
    }

    // 테넌트 Collector 쿼터 현황
    static async getQuotaStatus(): Promise<ApiResponse<{ used: number; max: number; available: number; is_exceeded: boolean; online: number; offline: number }>> {
        return apiClient.get(`${this.BASE_URL}/quota/status`);
    }

    // 미배정 Collector 목록 (site_id IS NULL, 서버사이드 필터)
    static async getUnassignedCollectors(): Promise<ApiResponse<EdgeServer[]>> {
        return apiClient.get<EdgeServer[]>(`${this.BASE_URL}/unassigned`);
    }
}
