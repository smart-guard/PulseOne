import { apiClient } from '../client';
import { ApiResponse, PaginatedApiResponse } from '../../types/common';
import { DeviceTemplate } from '../../types/manufacturing';

export class TemplateApiService {
    private static readonly BASE_URL = '/api/templates';

    /**
     * 템플릿 목록 조회
     */
    static async getTemplates(params?: any): Promise<PaginatedApiResponse<DeviceTemplate>> {
        return await apiClient.get<any>(this.BASE_URL, params);
    }

    /**
     * 템플릿 상세 조회 (데이터 포인트 포함)
     */
    static async getTemplate(id: number): Promise<ApiResponse<DeviceTemplate>> {
        return await apiClient.get<DeviceTemplate>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 템플릿 생성
     */
    static async createTemplate(data: Partial<DeviceTemplate>): Promise<ApiResponse<DeviceTemplate>> {
        return await apiClient.post<DeviceTemplate>(this.BASE_URL, data);
    }

    /**
     * 템플릿 정보 업데이트
     */
    static async updateTemplate(id: number, data: Partial<DeviceTemplate>): Promise<ApiResponse<DeviceTemplate>> {
        return await apiClient.put<DeviceTemplate>(`${this.BASE_URL}/${id}`, data);
    }

    /**
     * 템플릿 삭제
     */
    static async deleteTemplate(id: number): Promise<ApiResponse<any>> {
        return await apiClient.delete<any>(`${this.BASE_URL}/${id}`);
    }

    /**
     * 템플릿 데이터 포인트 추가
     */
    static async addTemplatePoint(id: number, pointData: any): Promise<ApiResponse<any>> {
        return await apiClient.post<any>(`${this.BASE_URL}/${id}/points`, pointData);
    }

    /**
     * 템플릿을 사용하여 디바이스 인스턴스화
     */
    static async instantiateTemplate(id: number, data: {
        name: string,
        site_id: number | null,
        edge_server_id?: number | null,
        device_group_id?: number,
        description?: string,
        endpoint?: string,
        config?: any,
        polling_interval?: number,
        timeout?: number,
        retry_count?: number,
        is_enabled?: number,
        data_points?: any[],
        // Extended operational settings
        read_timeout_ms?: number,
        write_timeout_ms?: number,
        retry_interval_ms?: number,
        backoff_time_ms?: number,
        backoff_multiplier?: number,
        max_backoff_time_ms?: number,
        read_buffer_size?: number,
        write_buffer_size?: number,
        queue_size?: number,
        is_keep_alive_enabled?: boolean,
        keep_alive_interval_s?: number,
        keep_alive_timeout_s?: number,
        is_data_validation_enabled?: boolean,
        is_performance_monitoring_enabled?: boolean,
        is_detailed_logging_enabled?: boolean,
        is_diagnostic_mode_enabled?: boolean,
        is_communication_logging_enabled?: boolean,
        is_outlier_detection_enabled?: boolean,
        is_deadband_enabled?: boolean,
        tags?: string[],
        metadata?: any,
        custom_fields?: any
    }): Promise<ApiResponse<{ device_id: number, name: string }>> {
        return await apiClient.post<any>(`${this.BASE_URL}/${id}/instantiate`, data);
    }
}
