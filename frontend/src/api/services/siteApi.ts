import { apiClient } from '../client';
import { ApiResponse, PaginatedApiResponse, Site } from '../../types/common';

export class SiteApiService {
    private static readonly BASE_URL = '/api/sites';

    /**
     * 사이트 목록 조회
     */
    static async getSites(params?: any): Promise<PaginatedApiResponse<Site>> {
        return await apiClient.get<any>(this.BASE_URL, params);
    }

    /**
     * 사이트 트리 구조 조회
     */
    static async getSiteTree(): Promise<ApiResponse<Site[]>> {
        return await apiClient.get<Site[]>(`${this.BASE_URL}/tree`);
    }

    /**
     * 사이트 상세 조회
     */
    static async getSite(id: number): Promise<ApiResponse<Site>> {
        return await apiClient.get<Site>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 사이트 생성
     */
    static async createSite(data: Partial<Site>): Promise<ApiResponse<Site>> {
        return await apiClient.post<Site>(this.BASE_URL, data);
    }

    /**
     * 사이트 정보 업데이트
     */
    static async updateSite(id: number, data: Partial<Site>): Promise<ApiResponse<Site>> {
        return await apiClient.put<Site>(`${this.BASE_URL}/${id}`, data);
    }

    /**
     * 사이트 삭제 (소프트 삭제)
     */
    static async deleteSite(id: number): Promise<ApiResponse<any>> {
        return await apiClient.delete<any>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 사이트 복구
     */
    static async restoreSite(id: number): Promise<ApiResponse<any>> {
        return await apiClient.post<any>(`${this.BASE_URL}/${id}/restore`);
    }
}
