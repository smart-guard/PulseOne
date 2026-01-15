import { apiClient } from '../client';
import { ApiResponse, PaginatedApiResponse } from '../../types/common';
import { AuditLog } from '../../types/manufacturing';

export class AuditLogApiService {
    private static readonly BASE_URL = '/api/audit-logs';

    /**
     * 감사 로그 목록 조회
     */
    static async getAuditLogs(params?: any): Promise<PaginatedApiResponse<AuditLog>> {
        return await apiClient.get<any>(this.BASE_URL, params);
    }

    /**
     * 특정 엔티티의 감사 로그 조회
     */
    static async getEntityAuditLogs(type: string, id: number | string): Promise<PaginatedApiResponse<AuditLog>> {
        return await apiClient.get<any>(this.BASE_URL, { entity_type: type, entity_id: id });
    }
}
