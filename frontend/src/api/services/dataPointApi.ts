import { apiClient as api, ApiResponse } from '../client';
import { DataPoint } from './dataApi';

export interface DataPointCreateData extends Partial<DataPoint> {
    device_id: number;
    name: string;
    address: string;
}

export interface DataPointUpdateData extends Partial<DataPoint> {
    name?: string;
    address?: string;
}

export interface BulkUpsertResponse {
    count: number;
    device_id: number;
}

export const DataPointApiService = {
    /**
     * 데이터포인트 목록 조회 (디바이스별)
     */
    getDeviceDataPoints: async (deviceId: number, params?: any): Promise<ApiResponse<{ items: DataPoint[], total: number }>> => {
        return await api.get<{ items: DataPoint[], total: number }>(`/devices/${deviceId}/data-points`, params);
    },

    /**
     * 데이터포인트 생성
     */
    createDataPoint: async (deviceId: number, data: DataPointCreateData): Promise<ApiResponse<DataPoint>> => {
        return await api.post<DataPoint>(`/devices/${deviceId}/data-points`, data);
    },

    /**
     * 데이터포인트 수정
     */
    updateDataPoint: async (deviceId: number, pointId: number, data: DataPointUpdateData): Promise<ApiResponse<DataPoint>> => {
        return await api.put<DataPoint>(`/devices/${deviceId}/data-points/${pointId}`, data);
    },

    /**
     * 데이터포인트 삭제
     */
    deleteDataPoint: async (deviceId: number, pointId: number): Promise<ApiResponse<void>> => {
        return await api.delete<void>(`/devices/${deviceId}/data-points/${pointId}`);
    },

    /**
     * 데이터포인트 일괄 저장 (Upsert)
     */
    bulkUpsertDataPoints: async (deviceId: number, dataPoints: Partial<DataPoint>[]): Promise<ApiResponse<BulkUpsertResponse>> => {
        return await api.post<BulkUpsertResponse>(`/devices/${deviceId}/data-points/bulk`, { data_points: dataPoints });
    },

    /**
     * 읽기 테스트
     */
    testDataPointRead: async (pointId: number): Promise<ApiResponse<any>> => {
        return await api.get<any>(`/data/points/${pointId}/test-read`);
    },

    /**
     * 쓰기 테스트
     */
    testDataPointWrite: async (pointId: number, value: any): Promise<ApiResponse<any>> => {
        return await api.post<any>(`/data/points/${pointId}/test-write`, { value });
    }
};

export type { DataPoint };
