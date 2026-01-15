// ============================================================================
// frontend/src/api/services/collectorApi.ts
// Collector 인스턴스(Edge Servers) 관리 API
// ============================================================================

import { apiClient, ApiResponse } from '../client';

export interface EdgeServer {
    id: number;
    tenant_id: number;
    name: string;
    host: string;
    api_port: number;
    description: string;
    status: 'online' | 'offline' | 'error' | 'maintenance';
    last_heartbeat: string;
    is_enabled: boolean;
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
}
