// =============================================================================
// frontend/src/api/services/modbusSlaveApi.ts
// Modbus Slave 디바이스 관리 API
// exportGatewayApi.ts 와 동일한 패턴
// =============================================================================

import { apiClient, ApiResponse } from '../client';

// =============================================================================
// Interfaces
// =============================================================================

export interface ModbusSlaveDevice {
    id: number;
    tenant_id: number;
    site_id: number;
    site_name?: string;
    name: string;
    tcp_port: number;
    unit_id: number;
    max_clients: number;
    enabled: number; // 1 | 0
    description?: string | null;
    created_at: string;
    updated_at: string;

    // 런타임 상태 (백엔드에서 프로세스 감지 후 주입)
    process_status?: 'running' | 'stopped';
    pid?: number | null;
    mapping_count?: number;
}

export interface ModbusSlaveMapping {
    id?: number;
    device_id: number;
    point_id: number;
    point_name?: string;
    point_type?: string;
    unit?: string;
    register_type: 'HR' | 'IR' | 'CO' | 'DI'; // Holding / Input / Coil / Discrete
    register_address: number;
    data_type: 'FLOAT32' | 'INT16' | 'INT32' | 'UINT16' | 'UINT32' | 'BOOL';
    byte_order: 'big_endian' | 'little_endian';
    scale_factor: number;
    scale_offset: number;
    enabled: number;
}

export interface DeviceStats {
    device_id: number;
    process_status: 'running' | 'stopped';
    pid: number | null;
    uptime: string;
    memory: string;
    cpu: string;
}

// =============================================================================
// API Service
// =============================================================================

const BASE_URL = '/api/modbus-slave';

export class ModbusSlaveApiService {
    // -----------------------------------------------------------------------
    // 디바이스 CRUD
    // -----------------------------------------------------------------------
    static async getDevices(params: { site_id?: number | null } = {}): Promise<ApiResponse<ModbusSlaveDevice[]>> {
        return apiClient.get<ModbusSlaveDevice[]>(BASE_URL, params);
    }

    static async getDeviceById(id: number): Promise<ApiResponse<ModbusSlaveDevice>> {
        return apiClient.get<ModbusSlaveDevice>(`${BASE_URL}/${id}`);
    }

    static async createDevice(data: Partial<ModbusSlaveDevice>): Promise<ApiResponse<ModbusSlaveDevice>> {
        return apiClient.post<ModbusSlaveDevice>(BASE_URL, data);
    }

    static async updateDevice(id: number, data: Partial<ModbusSlaveDevice>): Promise<ApiResponse<ModbusSlaveDevice>> {
        return apiClient.put<ModbusSlaveDevice>(`${BASE_URL}/${id}`, data);
    }

    static async deleteDevice(id: number): Promise<ApiResponse<void>> {
        return apiClient.delete<void>(`${BASE_URL}/${id}`);
    }

    // -----------------------------------------------------------------------
    // 매핑
    // -----------------------------------------------------------------------
    static async getMappings(deviceId: number): Promise<ApiResponse<ModbusSlaveMapping[]>> {
        return apiClient.get<ModbusSlaveMapping[]>(`${BASE_URL}/${deviceId}/mappings`);
    }

    static async saveMappings(deviceId: number, mappings: any[]): Promise<ApiResponse<any>> {
        return apiClient.put<any>(`${BASE_URL}/${deviceId}/mappings`, { mappings });
    }

    // -----------------------------------------------------------------------
    // 프로세스 제어
    // -----------------------------------------------------------------------
    static async startDevice(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${BASE_URL}/${id}/start`);
    }

    static async stopDevice(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${BASE_URL}/${id}/stop`);
    }

    static async restartDevice(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${BASE_URL}/${id}/restart`);
    }

    // -----------------------------------------------------------------------
    // 통계
    // -----------------------------------------------------------------------
    static async getDeviceStats(id: number): Promise<ApiResponse<DeviceStats>> {
        return apiClient.get<DeviceStats>(`${BASE_URL}/${id}/stats`);
    }
}

const modbusSlaveApi = ModbusSlaveApiService;
export default modbusSlaveApi;
