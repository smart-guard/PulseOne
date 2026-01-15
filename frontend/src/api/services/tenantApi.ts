import { apiClient } from '../client';
import { ApiResponse, PaginatedApiResponse, Tenant } from '../../types/common';

export class TenantApiService {
    private static readonly BASE_URL = '/api/tenants';

    /**
     * 테넌트 목록 조회
     */
    static async getTenants(params?: any): Promise<PaginatedApiResponse<Tenant>> {
        return await apiClient.get<any>(this.BASE_URL, params);
    }

    /**
     * 테넌트 상세 조회
     */
    static async getTenant(id: number): Promise<ApiResponse<Tenant>> {
        return await apiClient.get<Tenant>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 테넌트 생성
     */
    static async createTenant(data: Partial<Tenant>): Promise<ApiResponse<Tenant>> {
        return await apiClient.post<Tenant>(this.BASE_URL, data);
    }

    /**
     * 테넌트 정보 업데이트
     */
    static async updateTenant(id: number, data: Partial<Tenant>): Promise<ApiResponse<Tenant>> {
        return await apiClient.put<Tenant>(`${this.BASE_URL}/${id}`, data);
    }

    /**
     * 테넌트 삭제 (소프트 삭제)
     */
    static async deleteTenant(id: number): Promise<ApiResponse<any>> {
        return await apiClient.delete<any>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 테넌트 복구
     */
    static async restoreTenant(id: number): Promise<ApiResponse<any>> {
        return await apiClient.post<any>(`${this.BASE_URL}/${id}/restore`);
    }

    /**
     * 테넌트 통계 조회
     */
    static async getTenantStats(): Promise<ApiResponse<any>> {
        return await apiClient.get<any>(`${this.BASE_URL}/stats`);
    }
}
