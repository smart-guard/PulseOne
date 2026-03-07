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
    packet_logging: number; // 1 | 0 — 패킷 로그 파일 활성화
    created_at: string;
    updated_at: string;

    // 런타임 상태 (백엔드에서 프로세스 감지 후 주입)
    process_status?: 'running' | 'stopped';
    pid?: number | null;
    mapping_count?: number;
    last_activity?: string | null; // MAX(recorded_at) from access_logs
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
    byte_order: 'big_endian' | 'little_endian' | 'big_endian_swap' | 'little_endian_swap';
    scale_factor: number;
    scale_offset: number;
    enabled: number;
}

export interface ClientSession {
    fd?: number;
    ip: string;
    port: number;
    connected_at: string;
    duration_sec: number;
    requests: number;
    failures: number;
    success_rate: number;
    avg_response_us: number;
    fc03_count: number;
}

export interface FcWindowStats {
    requests: number;
    failures: number;
    success_rate: number;
    avg_resp_us?: number;
    req_per_min: number;
}

export interface SlaveStats {
    uptime_sec: number;
    total_requests: number;
    total_failures: number;
    overall_success_rate: number;
    avg_response_us: number;
    max_response_us: number;
    min_response_us: number;
    fc_counters: {
        fc01: number; fc02: number; fc03: number; fc04: number;
        fc05: number; fc06: number; fc15: number; fc16: number;
    };
    window_5min: FcWindowStats;
    window_60min: FcWindowStats;
}

export interface ClientsData {
    total_connections: number;
    current_clients: number;
    total_requests: number;
    total_failures: number;
    success_rate: number;
    sessions: ClientSession[];
}

export interface DeviceStats {
    device_id: number;
    process_status: 'running' | 'stopped';
    pid: number | null;
    uptime: string;
    memory: string;
    cpu: string;
    // C++ Worker 실시간 통계 (Worker 실행 중이면 존재)
    slave_stats: SlaveStats | null;
    clients: ClientsData | null;
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

    static async getDeletedDevices(params: { site_id?: number | null } = {}): Promise<ApiResponse<ModbusSlaveDevice[]>> {
        return apiClient.get<ModbusSlaveDevice[]>(`${BASE_URL}/deleted`, params);
    }

    static async restoreDevice(id: number): Promise<ApiResponse<any>> {
        return apiClient.post<any>(`${BASE_URL}/${id}/restore`);
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

    // -----------------------------------------------------------------------
    // 통신 이력 (Access Logs)
    // -----------------------------------------------------------------------
    static async getAccessLogs(params?: {
        device_id?: number;
        client_ip?: string;
        date_from?: string;
        date_to?: string;
        page?: number;
        limit?: number;
    }): Promise<ApiResponse<{ items: AccessLog[]; pagination: { page: number; limit: number; total: number } }>> {
        // params를 직접 전달 (apiClient.get의 두 번째 인자로 flat 전달)
        return apiClient.get(`${BASE_URL}/access-logs`, params || {});
    }

    static async getAccessLogStats(params?: {
        device_id?: number;
        date_from?: string;
        date_to?: string;
    }): Promise<ApiResponse<AccessLogStats>> {
        return apiClient.get(`${BASE_URL}/access-logs/stats`, params || {});
    }

    static async getReservedPorts(): Promise<ApiResponse<{ ports: number[]; labels: Record<number, string> }>> {
        return apiClient.get(`${BASE_URL}/reserved-ports`);
    }

    static async getRegisterValues(deviceId: number): Promise<ApiResponse<RegisterValue[]>> {
        return apiClient.get(`${BASE_URL}/${deviceId}/register-values`);
    }
}

const modbusSlaveApi = ModbusSlaveApiService;
export default modbusSlaveApi;

// ─── 이력 관련 타입 ────────────────────────────────────────────────────────

export interface AccessLog {
    id: number;
    device_id: number;
    device_name?: string;
    tenant_id?: number;
    client_ip: string;
    client_port?: number;
    unit_id?: number;
    period_start: string;
    period_end: string;
    total_requests: number;
    failed_requests: number;
    fc01_count: number;
    fc02_count: number;
    fc03_count: number;
    fc04_count: number;
    fc05_count: number;
    fc06_count: number;
    fc15_count: number;
    fc16_count: number;
    avg_response_us: number;
    duration_sec: number;
    success_rate: number;
    is_active: number;
    recorded_at: string;
}

export interface AccessLogStats {
    total_snapshots: number;
    total_requests: number;
    total_failures: number;
    unique_clients: number;
    avg_response_us: number;
    avg_success_rate: number;
}

export interface RegisterValue {
    point_id: number | null;
    register_type: string;
    register_address: number;
    data_type: string;
    point_name: string | null;
    value: number | string | null;         // 공학단위 값 (Collector 수집값)
    register_raw: number | null;           // Modbus 레지스터 기록값 = value * scale + offset
    quality: string | null;
    value_timestamp: string | null;
}
