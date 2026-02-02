// ============================================================================
// frontend/src/api/services/userApi.ts
// UserService 기반 사용자 관리 API
// ============================================================================

import { apiClient, ApiResponse } from '../client';

export interface User {
    id: string | number;
    tenant_id?: number | null;
    username: string;
    email: string;
    full_name: string;
    phone?: string;
    department?: string;
    role: 'system_admin' | 'company_admin' | 'site_admin' | 'manager' | 'engineer' | 'operator' | 'viewer';
    permissions: string[];
    site_access?: number[];
    device_access?: number[];
    is_active: boolean | number;
    is_deleted?: number;
    last_login?: string | Date;
    created_at?: string | Date;
    updated_at?: string | Date;
}

export interface UserFilters {
    includeDeleted?: boolean;
    onlyDeleted?: boolean;
    tenant_id?: number | string;
}

export interface UserStats {
    total_users: number;
    active_users: number;
    deleted_users?: number;
    admin_users: number;
    active_today?: number;
}

export interface CreateUserRequest {
    tenant_id?: number | null;
    username: string;
    email: string;
    password?: string;
    full_name: string;
    phone?: string;
    department?: string;
    role: string;
    permissions?: string[];
    site_access?: number[];
    device_access?: number[];
    is_active?: boolean | number;
}

export interface UpdateUserRequest extends Partial<CreateUserRequest> { }

export class UserApiService {
    private static readonly BASE_URL = '/api/users';

    // 모든 사용자 목록 조회
    static async getAllUsers(filters: UserFilters = {}): Promise<ApiResponse<User[]>> {
        try {
            return await apiClient.get<User[]>(this.BASE_URL, { params: filters });
        } catch (error) {
            console.error('사용자 목록 조회 실패:', error);
            throw error;
        }
    }

    // 사용자 통계 조회
    static async getStats(): Promise<ApiResponse<UserStats>> {
        try {
            return await apiClient.get<UserStats>(`${this.BASE_URL}/stats`);
        } catch (error) {
            console.error('사용자 통계 조회 실패:', error);
            throw error;
        }
    }

    // 특정 사용자 상세 조회
    static async getUser(id: string | number): Promise<ApiResponse<User>> {
        try {
            return await apiClient.get<User>(`${this.BASE_URL}/${id}`);
        } catch (error) {
            console.error(`사용자 ${id} 조회 실패:`, error);
            throw error;
        }
    }

    // 새 사용자 생성
    static async createUser(data: CreateUserRequest): Promise<ApiResponse<{ id: number; username: string }>> {
        try {
            return await apiClient.post<{ id: number; username: string }>(this.BASE_URL, data);
        } catch (error) {
            console.error('사용자 생성 실패:', error);
            throw error;
        }
    }

    // 사용자 정보 업데이트 (전체)
    static async updateUser(id: string | number, data: UpdateUserRequest): Promise<ApiResponse<User>> {
        try {
            return await apiClient.put<User>(`${this.BASE_URL}/${id}`, data);
        } catch (error) {
            console.error(`사용자 ${id} 업데이트 실패:`, error);
            throw error;
        }
    }

    // 사용자 정보 부분 업데이트 (PATCH)
    static async patchUser(id: string | number, data: Partial<UpdateUserRequest>): Promise<ApiResponse<User>> {
        try {
            return await apiClient.patch<User>(`${this.BASE_URL}/${id}`, data);
        } catch (error) {
            console.error(`사용자 ${id} 부분 업데이트 실패:`, error);
            throw error;
        }
    }

    // 사용자 삭제 (Soft Delete)
    static async deleteUser(id: string | number): Promise<ApiResponse<{ id: string | number; deleted: boolean }>> {
        try {
            return await apiClient.delete<{ id: string | number; deleted: boolean }>(`${this.BASE_URL}/${id}`);
        } catch (error) {
            console.error(`사용자 ${id} 삭제 실패:`, error);
            throw error;
        }
    }

    // 사용자 복구
    static async restoreUser(id: string | number): Promise<ApiResponse<User>> {
        try {
            return await apiClient.post<User>(`${this.BASE_URL}/${id}/restore`);
        } catch (error) {
            console.error(`사용자 ${id} 복구 실패:`, error);
            throw error;
        }
    }
}

export default UserApiService;
