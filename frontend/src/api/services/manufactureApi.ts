import { apiClient } from '../client';
import { ApiResponse, PaginatedApiResponse } from '../../types/common';
import { Manufacturer, DeviceModel } from '../../types/manufacturing';

export class ManufactureApiService {
    private static readonly BASE_URL = '/api/manufacturers';
    private static readonly MODELS_URL = '/api/models';

    /**
     * 제조사 목록 조회
     */
    static async getManufacturers(params?: any): Promise<PaginatedApiResponse<Manufacturer>> {
        return await apiClient.get<any>(this.BASE_URL, params);
    }

    /**
     * 제조사 통계 조회
     */
    static async getStatistics(): Promise<ApiResponse<{
        total_manufacturers: number;
        total_models: number;
        total_countries: number;
    }>> {
        return await apiClient.get<any>(`${this.BASE_URL}/stats`);
    }

    /**
     * 제조사 상세 조회
     */
    static async getManufacturer(id: number): Promise<ApiResponse<Manufacturer>> {
        return await apiClient.get<Manufacturer>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 제조사 모델 목록 조회
     */
    static async getModels(params?: any): Promise<PaginatedApiResponse<DeviceModel>> {
        return await apiClient.get<any>(this.MODELS_URL, params);
    }

    /**
     * 특정 제조사의 모델 목록 조회
     */
    static async getModelsByManufacturer(manufacturerId: number, params?: any): Promise<PaginatedApiResponse<DeviceModel>> {
        return await apiClient.get<any>(this.MODELS_URL, { manufacturer_id: manufacturerId, ...params });
    }

    /**
     * 모델 상세 조회
     */
    static async getModel(id: number): Promise<ApiResponse<DeviceModel>> {
        return await apiClient.get<DeviceModel>(`${this.MODELS_URL}/${id}`);
    }

    /**
     * 제조사 생성
     */
    static async createManufacturer(data: Partial<Manufacturer>): Promise<ApiResponse<Manufacturer>> {
        return await apiClient.post<Manufacturer>(this.BASE_URL, data);
    }

    /**
     * 제조사 정보 업데이트
     */
    static async updateManufacturer(id: number, data: Partial<Manufacturer>): Promise<ApiResponse<Manufacturer>> {
        return await apiClient.put<Manufacturer>(`${this.BASE_URL}/${id}`, data);
    }

    /**
     * 모델 생성
     */
    static async createModel(data: Partial<DeviceModel>): Promise<ApiResponse<DeviceModel>> {
        return await apiClient.post<DeviceModel>(this.MODELS_URL, data);
    }

    /**
     * 모델 정보 업데이트
     */
    static async updateModel(id: number, data: Partial<DeviceModel>): Promise<ApiResponse<DeviceModel>> {
        return await apiClient.put<DeviceModel>(`${this.MODELS_URL}/${id}`, data);
    }

    /**
     * 제조사 삭제
     */
    static async deleteManufacturer(id: number): Promise<ApiResponse<any>> {
        return await apiClient.delete<any>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 제조사 복구
     */
    static async restoreManufacturer(id: number): Promise<ApiResponse<any>> {
        return await apiClient.post<any>(`${this.BASE_URL}/${id}/restore`);
    }
}
