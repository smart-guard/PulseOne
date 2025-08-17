// ============================================================================
// frontend/src/api/services/deviceApi.ts
// 🔥 Device 인터페이스 수정 - 백엔드 API와 필드명 일치
// ============================================================================

// 🔥 수정된 Device 인터페이스 - 백엔드 API 응답과 완전 일치
export interface Device {
  // 🔥 기본 정보
  id: number;
  tenant_id?: number;
  site_id?: number;
  device_group_id?: number;
  edge_server_id?: number;
  
  // 🔥 디바이스 기본 속성
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  serial_number?: string;
  
  // 🔥 프로토콜 및 네트워크
  protocol_type: string;
  endpoint: string;
  config?: any; // JSON 객체
  
  // 🔥 운영 설정
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled: boolean;
  
  // 🔥 상태 정보
  connection_status?: string;
  status?: string | any; // 문자열 또는 객체
  last_seen?: string;
  last_communication?: string;
  
  // 🔥 ✅ 올바른 데이터포인트 필드명 (백엔드 API와 일치)
  data_point_count?: number;        // ✅ 백엔드: data_point_count
  enabled_point_count?: number;     // ✅ 백엔드: enabled_point_count
  
  // 🔥 성능 및 네트워크 정보
  response_time?: number;
  error_count?: number;
  last_error?: string;
  firmware_version?: string;
  hardware_info?: string;
  diagnostic_data?: any;
  
  // 🔥 확장된 설정 (API 응답에 포함)
  polling_interval_ms?: number;
  connection_timeout_ms?: number;
  max_retry_count?: number;
  retry_interval_ms?: number;
  backoff_time_ms?: number;
  keep_alive_enabled?: boolean;
  keep_alive_interval_s?: number;
  
  // 🔥 사이트 및 그룹 정보 (조인된 데이터)
  site_name?: string;
  site_code?: string;
  group_name?: string;
  group_type?: string;
  
  // 🔥 설정 및 상태 객체 (중첩된 JSON)
  settings?: {
    polling_interval_ms?: number;
    connection_timeout_ms?: number;
    max_retry_count?: number;
    retry_interval_ms?: number;
    backoff_time_ms?: number;
    keep_alive_enabled?: boolean;
    keep_alive_interval_s?: number;
    updated_at?: string;
  };
  
  status_info?: {
    status?: string;
    connection_status?: string;
    last_error?: string;
    response_time?: number;
    firmware_version?: string;
    hardware_info?: string;
    diagnostic_data?: any;
    error_count?: number;
    successful_requests?: number;
    total_requests?: number;
    updated_at?: string;
  };
  
  // 🔥 시간 정보
  installation_date?: string;
  last_maintenance?: string;
  created_at: string;
  updated_at: string;
  settings_updated_at?: string;
  status_updated_at?: string;
}

// 🔥 디바이스 통계 인터페이스
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

// 🔥 디바이스 생성 요청 인터페이스
export interface CreateDeviceRequest {
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  protocol_type: string;
  endpoint: string;
  config?: any;
  site_id?: number;
  device_group_id?: number;
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled: boolean;
}

// 🔥 디바이스 수정 요청 인터페이스
export interface UpdateDeviceRequest {
  name?: string;
  description?: string;
  device_type?: string;
  manufacturer?: string;
  model?: string;
  endpoint?: string;
  config?: any;
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled?: boolean;
}

// 🔥 디바이스 목록 조회 파라미터
export interface GetDevicesParams {
  page?: number;
  limit?: number;
  search?: string;
  protocol_type?: string;
  device_type?: string;
  connection_status?: string;
  status?: string;
  site_id?: number;
  device_group_id?: number;
  is_enabled?: boolean;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
}

// 🔥 API 응답 래퍼
export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  message?: string;
  error_code?: string;
  timestamp?: string;
}

// 🔥 페이징 정보
export interface PaginationInfo {
  page: number;
  limit: number;
  total: number;
  totalPages: number;
  hasNext: boolean;
  hasPrev: boolean;
}

// 🔥 디바이스 목록 응답
export interface DevicesResponse {
  items: Device[];
  pagination: PaginationInfo;
}

// 🔥 연결 테스트 결과
export interface ConnectionTestResult {
  device_id: number;
  device_name: string;
  endpoint: string;
  protocol_type: string;
  test_successful: boolean;
  response_time_ms?: number;
  test_timestamp: string;
  error_message?: string;
}

// 🔥 일괄 작업 요청
export interface BulkActionRequest {
  action: 'enable' | 'disable' | 'delete';
  device_ids: number[];
}

// 🔥 일괄 작업 결과
export interface BulkActionResult {
  action: string;
  total_requested: number;
  successful: number;
  failed: number;
  errors?: Array<{
    device_id: number;
    error: string;
  }>;
}

// 🔥 지원 프로토콜 정보
export interface ProtocolInfo {
  protocol_type: string;
  display_name: string;
  description?: string;
  supported_features?: string[];
  default_port?: number;
}

// 🔥 DeviceApiService 클래스 (실제 API 호출)
export class DeviceApiService {
  private static readonly BASE_URL = '/api/devices';

  // 🔥 디바이스 목록 조회
  static async getDevices(params?: GetDevicesParams): Promise<ApiResponse<DevicesResponse>> {
    try {
      const queryParams = new URLSearchParams();
      
      if (params) {
        Object.entries(params).forEach(([key, value]) => {
          if (value !== undefined && value !== null) {
            queryParams.append(key, value.toString());
          }
        });
      }
      
      const url = `${this.BASE_URL}?${queryParams.toString()}`;
      const response = await fetch(url);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('디바이스 목록 조회 실패:', error);
      throw error;
    }
  }

  // 🔥 디바이스 상세 조회
  static async getDevice(id: number): Promise<ApiResponse<Device>> {
    try {
      const response = await fetch(`${this.BASE_URL}/${id}`);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 조회 실패:`, error);
      throw error;
    }
  }

  // 🔥 디바이스 생성
  static async createDevice(data: CreateDeviceRequest): Promise<ApiResponse<Device>> {
    try {
      const response = await fetch(this.BASE_URL, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('디바이스 생성 실패:', error);
      throw error;
    }
  }

  // 🔥 디바이스 수정
  static async updateDevice(id: number, data: UpdateDeviceRequest): Promise<ApiResponse<Device>> {
    try {
      const response = await fetch(`${this.BASE_URL}/${id}`, {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 수정 실패:`, error);
      throw error;
    }
  }

  // 🔥 디바이스 삭제
  static async deleteDevice(id: number): Promise<ApiResponse<void>> {
    try {
      const response = await fetch(`${this.BASE_URL}/${id}`, {
        method: 'DELETE',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 삭제 실패:`, error);
      throw error;
    }
  }

  // 🔥 디바이스 연결 테스트
  static async testDeviceConnection(id: number): Promise<ApiResponse<ConnectionTestResult>> {
    try {
      const response = await fetch(`${this.BASE_URL}/${id}/test-connection`, {
        method: 'POST',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 연결 테스트 실패:`, error);
      throw error;
    }
  }

  // 🔥 디바이스 일괄 작업
  static async bulkAction(data: BulkActionRequest): Promise<ApiResponse<BulkActionResult>> {
    try {
      const response = await fetch(`${this.BASE_URL}/bulk-action`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('디바이스 일괄 작업 실패:', error);
      throw error;
    }
  }

  // 🔥 디바이스 통계 조회
  static async getDeviceStatistics(): Promise<ApiResponse<DeviceStats>> {
    try {
      const response = await fetch('/api/devices/statistics');
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('디바이스 통계 조회 실패:', error);
      throw error;
    }
  }

  // 🔥 지원 프로토콜 목록 조회
  static async getAvailableProtocols(): Promise<ApiResponse<ProtocolInfo[]>> {
    try {
      const response = await fetch('/api/devices/protocols');
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('지원 프로토콜 조회 실패:', error);
      throw error;
    }
  }

  // 🔥 디바이스 데이터포인트 조회
  static async getDeviceDataPoints(deviceId: number, params?: {
    page?: number;
    limit?: number;
    data_type?: string;
    enabled_only?: boolean;
  }): Promise<ApiResponse<any>> {
    try {
      const queryParams = new URLSearchParams();
      
      if (params) {
        Object.entries(params).forEach(([key, value]) => {
          if (value !== undefined && value !== null) {
            queryParams.append(key, value.toString());
          }
        });
      }
      
      const url = `${this.BASE_URL}/${deviceId}/data-points?${queryParams.toString()}`;
      const response = await fetch(url);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${deviceId} 데이터포인트 조회 실패:`, error);
      throw error;
    }
  }
}