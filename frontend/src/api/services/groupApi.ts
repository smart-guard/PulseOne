// ============================================================================
// frontend/src/api/services/groupApi.ts
// 장치 그룹(Logical Groups) 관리 API
// ============================================================================

import { apiClient, ApiResponse } from '../client';

export interface DeviceGroup {
    id: number;
    tenant_id: number;
    name: string;
    parent_group_id: number | null;
    path: string;
    description: string;
    group_type: string;
    sort_order: number;
    is_expanded?: boolean; // UI 전용
    children?: DeviceGroup[];
    device_count?: number;
}

export interface CreateGroupRequest {
    name: string;
    parent_group_id?: number | null;
    description?: string;
    group_type?: string;
    sort_order?: number;
}

export interface UpdateGroupRequest {
    name?: string;
    parent_group_id?: number | null;
    description?: string;
    group_type?: string;
    sort_order?: number;
}

export class GroupApiService {
    private static readonly BASE_URL = '/api/groups';

    // 그룹 트리 구조 조회
    static async getGroupTree(): Promise<ApiResponse<DeviceGroup[]>> {
        return apiClient.get<DeviceGroup[]>(this.BASE_URL);
    }

    // 그룹 상세 조회
    static async getGroup(id: number): Promise<ApiResponse<DeviceGroup>> {
        return apiClient.get<DeviceGroup>(`${this.BASE_URL}/${id}`);
    }

    // 그룹 생성
    static async createGroup(data: CreateGroupRequest): Promise<ApiResponse<DeviceGroup>> {
        return apiClient.post<DeviceGroup>(this.BASE_URL, data);
    }

    // 그룹 수정
    static async updateGroup(id: number, data: UpdateGroupRequest): Promise<ApiResponse<DeviceGroup>> {
        return apiClient.put<DeviceGroup>(`${this.BASE_URL}/${id}`, data);
    }

    // 그룹 삭제
    static async deleteGroup(id: number, force: boolean = false): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${this.BASE_URL}/${id}${force ? '?force=true' : ''}`);
    }

    // 특정 그룹의 장치 목록 조회
    static async getGroupDevices(id: number): Promise<ApiResponse<any[]>> {
        return apiClient.get<any[]>(`${this.BASE_URL}/${id}/devices`);
    }
}
