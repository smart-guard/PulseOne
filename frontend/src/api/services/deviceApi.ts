// ============================================================================
// frontend/src/api/services/deviceApi.ts
// 디바이스 API 서비스 - 새로운 Backend API 완전 호환
// ============================================================================

import { API_CONFIG } from '../config';
import { ENDPOINTS } from '../endpoints';
import { 
  ApiResponse, 
  PaginatedApiResponse, 
  PaginationParams, 
  BulkActionResponse 
} from '../../types/common';

// ============================================================================
// 🏭 디바이스 관련 인터페이스들
// ============================================================================

export interface Device {
  id: number;
  name: string;
  protocol_type: string;
  device_type?: string;
  endpoint: string;
  is_enabled: boolean;
  connection_status: 'connected' | 'disconnected' | 'error';
  status: 'running' | 'stopped' | 'error' | 'disabled' | 'restarting';
  last_seen?: string;
  site_id?: number;
  site_name?: string;
  manufacturer?: string;
  model?: string;
  description?: string;
  data_points_count?: number;
  polling_interval?: number;
  response_time?: number;
  error_count?: number;
  uptime?: string;
  created_at: string;
  updated_at: string;
  created_by?: number;
  tenant_id?: number;
}

export interface DeviceStats {
  total_devices: number;
  connected_devices: number;
  disconnected_devices: number;
  error_devices: number;
  protocols_count: number;
  sites_count: number;
  protocol_distribution: Array<{
    protocol_type: string;
    count: number;
    percentage: number;
  }>;
  site_distribution: Array<{
    site_id: number;
    site_name: string;
    device_count: number;
  }>;
}

export interface DeviceListParams extends PaginationParams {
  protocol_type?: string;
  device_type?: string;
  connection_status?: string;
  status?: string;
  site_id?: number;
  search?: string;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
}

export interface DeviceCreateData {
  name: string;
  protocol_type: string;
  device_type?: string;
  endpoint: string;
  site_id?: number;
  manufacturer?: string;
  model?: string;
  description?: string;
  polling_interval?: number;
  is_enabled?: boolean;
}

export interface DeviceUpdateData {
  name?: string;
  endpoint?: string;
  device_type?: string;
  site_id?: number;
  manufacturer?: string;
  model?: string;
  description?: string;
  polling_interval?: number;
  is_enabled?: boolean;
}

export interface ConnectionTestResult {
  device_id: number;
  device_name: string;
  endpoint: string;
  protocol_type: string;
  test_successful: boolean;
  response_time_ms: number;
  test_timestamp: string;
  error_message?: string;
}

export interface BulkActionRequest {
  action: 'enable' | 'disable' | 'delete';
  device_ids: number[];
}

export interface BulkActionResult {
  total_processed: number;
  successful: number;
  failed: number;
  errors?: Array<{
    device_id: number;
    error: string;
  }>;
}

// ============================================================================
// 🔧 HTTP 클라이언트 클래스
// ============================================================================

class HttpClient {
  private baseUrl: string;

  constructor(baseUrl: string = API_CONFIG.BASE_URL) {
    this.baseUrl = baseUrl;
  }

  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    const url = `${this.baseUrl}${endpoint}`;
    
    console.log('🌐 API Request:', {
      method: options.method || 'GET',
      url: url,
      endpoint: endpoint
    });
    
    const config: RequestInit = {
      timeout: API_CONFIG.TIMEOUT,
      headers: {
        ...API_CONFIG.DEFAULT_HEADERS,
        ...options.headers,
      },
      ...options,
    };

    try {
      const response = await fetch(url, config);
      
      console.log('📡 API Response:', {
        status: response.status,
        ok: response.ok,
        url: response.url
      });
      
      if (!response.ok) {
        const errorData = await response.json().catch(() => ({}));
        throw new Error(errorData.message || errorData.error || `HTTP ${response.status}: ${response.statusText}`);
      }

      const data: ApiResponse<T> = await response.json();
      
      // Backend 응답이 success 필드를 사용하므로 변환
      if ('success' in data) {
        return {
          success: data.success,
          data: data.data,
          message: data.message,
          error: data.error,
          timestamp: data.timestamp
        };
      }
      
      // 기존 형식 호환
      return data;
      
    } catch (error) {
      console.error('❌ API Request failed:', {
        endpoint,
        url,
        error: error instanceof Error ? error.message : 'Unknown error'
      });
      
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error',
        data: null as any
      };
    }
  }

  async get<T>(endpoint: string, params?: Record<string, any>): Promise<ApiResponse<T>> {
    const queryParams = new URLSearchParams();
    
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });
    }
    
    const url = params && queryParams.toString() ? 
      `${endpoint}?${queryParams.toString()}` : endpoint;
    
    return this.request<T>(url, { method: 'GET' });
  }

  async post<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'POST',
      body: data ? JSON.stringify(data) : undefined
    });
  }

  async put<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'PUT',
      body: data ? JSON.stringify(data) : undefined
    });
  }

  async delete<T>(endpoint: string): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, { method: 'DELETE' });
  }
}

// ============================================================================
// 🏭 디바이스 API 서비스 클래스
// ============================================================================

export class DeviceApiService {
  private static httpClient = new HttpClient();

  // ========================================================================
  // 📋 디바이스 목록 및 조회
  // ========================================================================

  /**
   * 디바이스 목록 조회 (페이징, 필터링, 정렬 지원)
   */
  static async getDevices(params?: DeviceListParams): Promise<ApiResponse<{
    items: Device[];
    pagination: {
      page: number;
      limit: number;
      total: number;
      totalPages: number;
      hasNext: boolean;
      hasPrev: boolean;
    };
  }>> {
    console.log('📱 디바이스 목록 조회:', params);
    return this.httpClient.get<any>(ENDPOINTS.DEVICES, params);
  }

  /**
   * 특정 디바이스 상세 조회
   */
  static async getDevice(id: number, options?: {
    include_data_points?: boolean;
  }): Promise<ApiResponse<Device>> {
    console.log('📱 디바이스 상세 조회:', { id, options });
    return this.httpClient.get<Device>(ENDPOINTS.DEVICE_BY_ID(id), options);
  }

  /**
   * 디바이스의 데이터포인트 목록 조회
   */
  static async getDeviceDataPoints(id: number, params?: {
    page?: number;
    limit?: number;
    data_type?: string;
    enabled_only?: boolean;
  }): Promise<ApiResponse<any>> {
    console.log('📊 디바이스 데이터포인트 조회:', { id, params });
    return this.httpClient.get<any>(ENDPOINTS.DEVICE_DATA_POINTS(id), params);
  }

  // ========================================================================
  // ✏️ 디바이스 생성, 수정, 삭제
  // ========================================================================

  /**
   * 새 디바이스 생성
   */
  static async createDevice(data: DeviceCreateData): Promise<ApiResponse<Device>> {
    console.log('➕ 디바이스 생성:', data);
    return this.httpClient.post<Device>(ENDPOINTS.DEVICES, data);
  }

  /**
   * 디바이스 정보 수정
   */
  static async updateDevice(id: number, data: DeviceUpdateData): Promise<ApiResponse<Device>> {
    console.log('✏️ 디바이스 수정:', { id, data });
    return this.httpClient.put<Device>(ENDPOINTS.DEVICE_BY_ID(id), data);
  }

  /**
   * 디바이스 삭제
   */
  static async deleteDevice(id: number, force: boolean = false): Promise<ApiResponse<{ deleted: boolean }>> {
    console.log('🗑️ 디바이스 삭제:', { id, force });
    const endpoint = force ? `${ENDPOINTS.DEVICE_BY_ID(id)}?force=true` : ENDPOINTS.DEVICE_BY_ID(id);
    return this.httpClient.delete<{ deleted: boolean }>(endpoint);
  }

  // ========================================================================
  // 🔄 디바이스 제어 및 상태 관리
  // ========================================================================

  /**
   * 디바이스 활성화
   */
  static async enableDevice(id: number): Promise<ApiResponse<Device>> {
    console.log('🟢 디바이스 활성화:', id);
    return this.httpClient.post<Device>(ENDPOINTS.DEVICE_ENABLE(id));
  }

  /**
   * 디바이스 비활성화
   */
  static async disableDevice(id: number): Promise<ApiResponse<Device>> {
    console.log('🔴 디바이스 비활성화:', id);
    return this.httpClient.post<Device>(ENDPOINTS.DEVICE_DISABLE(id));
  }

  /**
   * 디바이스 재시작
   */
  static async restartDevice(id: number): Promise<ApiResponse<Device>> {
    console.log('🔄 디바이스 재시작:', id);
    return this.httpClient.post<Device>(ENDPOINTS.DEVICE_RESTART(id));
  }

  /**
   * 디바이스 연결 테스트
   */
  static async testDeviceConnection(id: number): Promise<ApiResponse<ConnectionTestResult>> {
    console.log('🔗 디바이스 연결 테스트:', id);
    return this.httpClient.post<ConnectionTestResult>(ENDPOINTS.DEVICE_TEST_CONNECTION(id));
  }

  // ========================================================================
  // 📊 통계 및 검색
  // ========================================================================

  /**
   * 지원하는 프로토콜 목록 조회
   */
  static async getAvailableProtocols(): Promise<ApiResponse<Array<{
    protocol_type: string;
    display_name: string;
    description: string;
    supported_features: string[];
  }>>> {
    console.log('📋 지원 프로토콜 조회');
    return this.httpClient.get<any>('/api/devices/protocols');
  }

  /**
   * 디바이스 통계 조회
   */
  static async getDeviceStatistics(): Promise<ApiResponse<DeviceStats>> {
    console.log('📊 디바이스 통계 조회');
    return this.httpClient.get<DeviceStats>('/api/devices/statistics');
  }

  // ========================================================================
  // 🔄 일괄 작업
  // ========================================================================

  /**
   * 일괄 작업 실행 (활성화, 비활성화, 삭제)
   */
  static async bulkAction(request: BulkActionRequest): Promise<ApiResponse<BulkActionResult>> {
    console.log('🔄 일괄 작업 실행:', request);
    return this.httpClient.post<BulkActionResult>('/api/devices/bulk-action', request);
  }

  /**
   * 여러 디바이스 활성화
   */
  static async bulkEnableDevices(deviceIds: number[]): Promise<ApiResponse<BulkActionResult>> {
    console.log('🟢 일괄 활성화:', deviceIds);
    return this.bulkAction({ action: 'enable', device_ids: deviceIds });
  }

  /**
   * 여러 디바이스 비활성화
   */
  static async bulkDisableDevices(deviceIds: number[]): Promise<ApiResponse<BulkActionResult>> {
    console.log('🔴 일괄 비활성화:', deviceIds);
    return this.bulkAction({ action: 'disable', device_ids: deviceIds });
  }

  /**
   * 여러 디바이스 삭제
   */
  static async bulkDeleteDevices(deviceIds: number[]): Promise<ApiResponse<BulkActionResult>> {
    console.log('🗑️ 일괄 삭제:', deviceIds);
    return this.bulkAction({ action: 'delete', device_ids: deviceIds });
  }

  // ========================================================================
  // 🔧 유틸리티 메서드들
  // ========================================================================

  /**
   * 디바이스 상태별 필터링
   */
  static filterDevicesByStatus(devices: Device[], status: string): Device[] {
    if (status === 'all') return devices;
    return devices.filter(device => device.status === status);
  }

  /**
   * 디바이스 연결 상태별 필터링
   */
  static filterDevicesByConnection(devices: Device[], connectionStatus: string): Device[] {
    if (connectionStatus === 'all') return devices;
    return devices.filter(device => device.connection_status === connectionStatus);
  }

  /**
   * 디바이스 프로토콜별 필터링
   */
  static filterDevicesByProtocol(devices: Device[], protocol: string): Device[] {
    if (protocol === 'all') return devices;
    return devices.filter(device => device.protocol_type === protocol);
  }

  /**
   * 디바이스 검색 (이름, 설명, 제조사, 모델)
   */
  static searchDevices(devices: Device[], searchTerm: string): Device[] {
    if (!searchTerm.trim()) return devices;
    
    const term = searchTerm.toLowerCase();
    return devices.filter(device => 
      device.name.toLowerCase().includes(term) ||
      device.description?.toLowerCase().includes(term) ||
      device.manufacturer?.toLowerCase().includes(term) ||
      device.model?.toLowerCase().includes(term) ||
      device.endpoint.toLowerCase().includes(term)
    );
  }

  /**
   * 디바이스 상태 요약 계산
   */
  static calculateDevicesSummary(devices: Device[]): {
    total: number;
    running: number;
    stopped: number;
    error: number;
    connected: number;
    disconnected: number;
    enabled: number;
    disabled: number;
  } {
    return {
      total: devices.length,
      running: devices.filter(d => d.status === 'running').length,
      stopped: devices.filter(d => d.status === 'stopped').length,
      error: devices.filter(d => d.status === 'error').length,
      connected: devices.filter(d => d.connection_status === 'connected').length,
      disconnected: devices.filter(d => d.connection_status === 'disconnected').length,
      enabled: devices.filter(d => d.is_enabled).length,
      disabled: devices.filter(d => !d.is_enabled).length,
    };
  }

  /**
   * 프로토콜별 디바이스 그룹화
   */
  static groupDevicesByProtocol(devices: Device[]): Record<string, Device[]> {
    return devices.reduce((groups, device) => {
      const protocol = device.protocol_type;
      if (!groups[protocol]) {
        groups[protocol] = [];
      }
      groups[protocol].push(device);
      return groups;
    }, {} as Record<string, Device[]>);
  }
}