import { apiClient, ApiResponse } from '../client';

export interface Permission {
    id: string;
    name: string;
    description: string;
    category: string;
    resource: string;
    actions: string[];
    is_system: number;
    created_at: string;
}

export interface Role {
    id: string;
    name: string;
    description: string;
    permissions: string[];
    userCount: number;
    permissionCount: number;
    is_system: number;
    created_at: string;
    updated_at: string;
}

export interface CreateRoleRequest {
    name: string;
    description: string;
    permissions: string[];
}

export interface UpdateRoleRequest {
    name?: string;
    description?: string;
    permissions?: string[];
}


class RoleApiService {
    private readonly API_PREFIX = '/api/system';

    private getRequestConfig(method: 'GET' | 'POST' | 'PUT' | 'DELETE', url: string, data?: any) {
        return {
            method,
            url: `${this.API_PREFIX}${url}`,
            baseUrl: window.location.origin, // Force use of current origin (proxy)
            body: data
        };
    }

    async getPermissions(): Promise<Permission[]> {
        const response = await apiClient.request<Permission[]>(
            this.getRequestConfig('GET', '/permissions')
        );
        if (!response.data) return [];

        return (response.data as any[]).map((p: any) => ({
            ...p,
            actions: typeof p.actions === 'string' ? JSON.parse(p.actions) : p.actions
        }));
    }

    async getRoles(): Promise<Role[]> {
        const response = await apiClient.request<Role[]>(
            this.getRequestConfig('GET', '/roles')
        );
        return response.data || [];
    }

    async getRole(id: string): Promise<Role> {
        const response = await apiClient.request<Role>(
            this.getRequestConfig('GET', `/roles/${id}`)
        );
        if (!response.data) throw new Error('Role not found');
        return response.data;
    }

    async createRole(data: CreateRoleRequest): Promise<Role> {
        const response = await apiClient.request<Role>(
            this.getRequestConfig('POST', '/roles', data)
        );
        if (!response.data) throw new Error('Failed to create role');
        return response.data;
    }

    async updateRole(id: string, data: UpdateRoleRequest): Promise<Role> {
        const response = await apiClient.request<Role>(
            this.getRequestConfig('PUT', `/roles/${id}`, data)
        );
        if (!response.data) throw new Error('Failed to update role');
        return response.data;
    }

    async deleteRole(id: string): Promise<void> {
        await apiClient.request(
            this.getRequestConfig('DELETE', `/roles/${id}`)
        );
    }
}

export const roleApi = new RoleApiService();

export default RoleApiService;
