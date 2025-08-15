
import { API_CONFIG } from '../config';
import { ENDPOINTS } from '../endpoints';
import { 
  ApiResponse, 
  PaginatedApiResponse, 
  PaginationParams, 
  BulkActionResponse 
} from '../../types/common';

export interface Device {
  id: number;
  name: string;
  protocol_type: string;
  device_type?: string;
  endpoint: string;
  is_enabled: boolean;
  connection_status: string;
  status: string;
  last_seen?: string;
  manufacturer?: string;
  model?: string;
  description?: string;
  created_at: string;
  updated_at: string;
}

export interface DeviceListParams extends PaginationParams {
  protocol_type?: string;
  device_type?: string;
  is_enabled?: boolean;
  connection_status?: string;
  search?: string;
}

export class DeviceApiService {
  private static async request<T>(endpoint: string, options: RequestInit = {}): Promise<T> {
    const url = `${API_CONFIG.BASE_URL}${endpoint}`;
    
    const config: RequestInit = {
      headers: API_CONFIG.DEFAULT_HEADERS,
      ...options,
    };

    const response = await fetch(url, config);
    
    if (!response.ok) {
      const errorData = await response.json().catch(() => ({}));
      throw new Error(errorData.error || `HTTP ${response.status}: ${response.statusText}`);
    }

    return await response.json();
  }

  static async getDevices(params?: DeviceListParams): Promise<PaginatedApiResponse<Device>> {
    const queryParams = new URLSearchParams();
    
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });
    }
    
    const endpoint = `${ENDPOINTS.DEVICES}?${queryParams.toString()}`;
    return this.request<PaginatedApiResponse<Device>>(endpoint);
  }

  static async enableDevice(id: number): Promise<ApiResponse<void>> {
    return this.request<ApiResponse<void>>(ENDPOINTS.DEVICE_ENABLE(id), { method: 'POST' });
  }

  static async disableDevice(id: number): Promise<ApiResponse<void>> {
    return this.request<ApiResponse<void>>(ENDPOINTS.DEVICE_DISABLE(id), { method: 'POST' });
  }

  static async restartDevice(id: number): Promise<ApiResponse<void>> {
    return this.request<ApiResponse<void>>(ENDPOINTS.DEVICE_RESTART(id), { method: 'POST' });
  }

  static async testDeviceConnection(id: number): Promise<ApiResponse<any>> {
    return this.request<ApiResponse<any>>(ENDPOINTS.DEVICE_TEST_CONNECTION(id), { method: 'POST' });
  }

  static async bulkEnableDevices(deviceIds: number[]): Promise<BulkActionResponse> {
    return this.request<BulkActionResponse>(ENDPOINTS.DEVICES_BULK_ENABLE, {
      method: 'POST',
      body: JSON.stringify({ device_ids: deviceIds })
    });
  }

  static async bulkDisableDevices(deviceIds: number[]): Promise<BulkActionResponse> {
    return this.request<BulkActionResponse>(ENDPOINTS.DEVICES_BULK_DISABLE, {
      method: 'POST',
      body: JSON.stringify({ device_ids: deviceIds })
    });
  }

  static async bulkDeleteDevices(deviceIds: number[]): Promise<BulkActionResponse> {
    return this.request<BulkActionResponse>(ENDPOINTS.DEVICES_BULK_DELETE, {
      method: 'DELETE',
      body: JSON.stringify({ device_ids: deviceIds })
    });
  }

  static async createDevice(data: any): Promise<ApiResponse<Device>> {
    return this.request<ApiResponse<Device>>(ENDPOINTS.DEVICES, {
      method: 'POST',
      body: JSON.stringify(data)
    });
  }

  static async updateDevice(id: number, data: any): Promise<ApiResponse<Device>> {
    return this.request<ApiResponse<Device>>(ENDPOINTS.DEVICE_BY_ID(id), {
      method: 'PUT',
      body: JSON.stringify(data)
    });
  }

  static async deleteDevice(id: number): Promise<ApiResponse<void>> {
    return this.request<ApiResponse<void>>(ENDPOINTS.DEVICE_BY_ID(id), { method: 'DELETE' });
  }
}
