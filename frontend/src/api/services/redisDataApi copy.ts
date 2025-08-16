// ============================================================================
// frontend/src/api/services/redisDataApi.ts
// 📝 Redis 데이터 조회 API 서비스
// ============================================================================

// ============================================================================
// frontend/src/api/services/redisDataApi.ts
// 📝 Redis 데이터 조회 API 서비스 - 간소화된 버전
// ============================================================================

// =============================================================================
// 타입 정의
// =============================================================================

export interface RedisTreeNode {
  id: string;
  name: string;
  path: string;
  type: 'tenant' | 'site' | 'device' | 'folder' | 'datapoint';
  children?: RedisTreeNode[];
  isExpanded: boolean;
  isLoaded: boolean;
  dataPoint?: RedisDataPoint;
  childCount?: number;
  metadata?: {
    tenant_id?: number;
    site_id?: number;
    device_id?: number;
    last_update?: string;
  };
}

export interface RedisDataPoint {
  id: string;
  key: string;
  name: string;
  value: any;
  dataType: 'string' | 'number' | 'boolean' | 'object' | 'array' | 'hash' | 'list' | 'set';
  unit?: string;
  timestamp: string;
  quality: 'good' | 'bad' | 'uncertain';
  size: number;
  ttl?: number;
  metadata?: {
    tenant_id?: number;
    device_id?: number;
    data_point_id?: number;
    protocol?: string;
    last_communication?: string;
  };
}

export interface RedisKeyPattern {
  pattern: string;
  description: string;
  type: 'device' | 'datapoint' | 'alarm' | 'history' | 'cache';
  example?: string;
}

export interface RedisStats {
  total_keys: number;
  memory_usage: number;
  connected_clients: number;
  commands_processed: number;
  hits: number;
  misses: number;
  expired_keys: number;
}

export interface RedisSearchParams {
  pattern?: string;
  type?: string;
  tenant_id?: number;
  device_id?: number;
  limit?: number;
  cursor?: string;
}

interface ApiResponse<T = any> {
  success: boolean;
  data: T;
  message?: string;
  error?: string;
  timestamp?: string;
}

// =============================================================================
// 간단한 HTTP 클라이언트
// =============================================================================

class SimpleHttpClient {
  private baseURL: string;

  constructor(baseURL: string = 'http://localhost:3000') {
    this.baseURL = baseURL;
  }

  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    const url = `${this.baseURL}${endpoint}`;
    
    const config: RequestInit = {
      headers: {
        'Content-Type': 'application/json',
        ...options.headers,
      },
      ...options,
    };

    try {
      const response = await fetch(url, config);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      return data;
    } catch (error) {
      console.error(`API Request failed: ${endpoint}`, error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error',
        data: null as any
      };
    }
  }

  async get<T>(endpoint: string, params?: Record<string, any>): Promise<ApiResponse<T>> {
    let url = endpoint;
    if (params) {
      const searchParams = new URLSearchParams();
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          searchParams.append(key, String(value));
        }
      });
      url += `?${searchParams.toString()}`;
    }
    return this.request<T>(url, { method: 'GET' });
  }

  async post<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'POST',
      body: data ? JSON.stringify(data) : undefined,
    });
  }

  async put<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'PUT',
      body: data ? JSON.stringify(data) : undefined,
    });
  }

  async delete<T>(endpoint: string): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, { method: 'DELETE' });
  }
}

// HTTP 클라이언트 인스턴스
const httpClient = new SimpleHttpClient();

// =============================================================================
// 타입 정의
// =============================================================================

export interface RedisTreeNode {
  id: string;
  name: string;
  path: string;
  type: 'tenant' | 'site' | 'device' | 'folder' | 'datapoint';
  children?: RedisTreeNode[];
  isExpanded: boolean;
  isLoaded: boolean;
  dataPoint?: RedisDataPoint;
  childCount?: number;
  metadata?: {
    tenant_id?: number;
    site_id?: number;
    device_id?: number;
    last_update?: string;
  };
}

export interface RedisDataPoint {
  id: string;
  key: string;
  name: string;
  value: any;
  dataType: 'string' | 'number' | 'boolean' | 'object' | 'array' | 'hash' | 'list' | 'set';
  unit?: string;
  timestamp: string;
  quality: 'good' | 'bad' | 'uncertain';
  size: number;
  ttl?: number;
  metadata?: {
    tenant_id?: number;
    device_id?: number;
    data_point_id?: number;
    protocol?: string;
    last_communication?: string;
  };
}

export interface RedisKeyPattern {
  pattern: string;
  description: string;
  type: 'device' | 'datapoint' | 'alarm' | 'history' | 'cache';
  example?: string;
}

export interface RedisStats {
  total_keys: number;
  memory_usage: number;
  connected_clients: number;
  commands_processed: number;
  hits: number;
  misses: number;
  expired_keys: number;
}

export interface RedisSearchParams {
  pattern?: string;
  type?: string;
  tenant_id?: number;
  device_id?: number;
  limit?: number;
  cursor?: string;
}

// =============================================================================
// Redis 데이터 API 서비스 클래스
// =============================================================================

export class RedisDataApiService {
  /**
   * Redis 연결 상태 확인
   */
  static async getConnectionStatus(): Promise<ApiResponse<{ status: 'connected' | 'disconnected' | 'connecting'; info?: any }>> {
    try {
      const response = await httpClient.get('/api/redis/status');
      return response;
    } catch (error) {
      console.error('Redis 연결 상태 확인 실패:', error);
      return {
        success: false,
        error: '연결 상태 확인 중 오류가 발생했습니다.',
        data: { status: 'disconnected' }
      };
    }
  }

  /**
   * Redis 통계 정보 조회
   */
  static async getStats(): Promise<ApiResponse<RedisStats>> {
    return httpClient.get<RedisStats>('/api/redis/stats');
  }

  /**
   * Redis 키 트리 구조 조회 (계층적 표시용)
   */
  static async getKeyTree(parentPath: string = ''): Promise<ApiResponse<RedisTreeNode[]>> {
    const params = parentPath ? { parent_path: parentPath } : {};
    return httpClient.get<RedisTreeNode[]>('/api/redis/tree', params);
  }

  /**
   * 특정 노드의 자식 노드들 조회
   */
  static async getNodeChildren(nodeId: string): Promise<ApiResponse<RedisTreeNode[]>> {
    return httpClient.get<RedisTreeNode[]>(`/api/redis/tree/${nodeId}/children`);
  }

  /**
   * Redis 키 검색
   */
  static async searchKeys(params: RedisSearchParams): Promise<ApiResponse<{ keys: string[]; cursor?: string; total?: number }>> {
    return httpClient.get('/api/redis/keys/search', params);
  }

  /**
   * 특정 키의 데이터 조회
   */
  static async getKeyData(key: string): Promise<ApiResponse<RedisDataPoint>> {
    return httpClient.get<RedisDataPoint>(`/api/redis/keys/${encodeURIComponent(key)}/data`);
  }

  /**
   * 여러 키의 데이터 일괄 조회
   */
  static async getBulkKeyData(keys: string[]): Promise<ApiResponse<RedisDataPoint[]>> {
    return httpClient.post<RedisDataPoint[]>('/api/redis/keys/bulk', { keys });
  }

  /**
   * 실시간 데이터 포인트들 조회 (현재값)
   */
  static async getCurrentValues(params?: {
    device_ids?: number[];
    site_id?: number;
    data_type?: string;
    quality_filter?: string;
    limit?: number;
  }): Promise<ApiResponse<RedisDataPoint[]>> {
    return httpClient.get<RedisDataPoint[]>('/api/realtime/current-values', params);
  }

  /**
   * 특정 디바이스의 데이터 포인트들 조회
   */
  static async getDeviceDataPoints(deviceId: number): Promise<ApiResponse<RedisDataPoint[]>> {
    return httpClient.get<RedisDataPoint[]>(`/api/realtime/device/${deviceId}/values`);
  }

  /**
   * Redis 키 패턴 목록 조회
   */
  static async getKeyPatterns(): Promise<ApiResponse<RedisKeyPattern[]>> {
    return httpClient.get<RedisKeyPattern[]>('/api/redis/patterns');
  }

  /**
   * 테넌트별 키 목록 조회
   */
  static async getTenantKeys(tenantId?: number): Promise<ApiResponse<RedisTreeNode[]>> {
    const params = tenantId ? { tenant_id: tenantId } : {};
    return httpClient.get<RedisTreeNode[]>('/api/redis/tenants/keys', params);
  }

  /**
   * 특정 키 삭제 (관리자 전용)
   */
  static async deleteKey(key: string): Promise<ApiResponse<{ deleted: boolean }>> {
    return httpClient.delete<{ deleted: boolean }>(`/api/redis/keys/${encodeURIComponent(key)}`);
  }

  /**
   * 키 TTL 설정 (관리자 전용)
   */
  static async setKeyTTL(key: string, ttl: number): Promise<ApiResponse<{ success: boolean }>> {
    return httpClient.put<{ success: boolean }>(`/api/redis/keys/${encodeURIComponent(key)}/ttl`, { ttl });
  }

  /**
   * 데이터 내보내기
   */
  static async exportData(keys: string[], format: 'json' | 'csv' = 'json'): Promise<Blob> {
    try {
      const response = await fetch('http://localhost:3000/api/redis/export', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ keys, format })
      });
      
      if (!response.ok) {
        throw new Error('Export failed');
      }
      
      return await response.blob();
    } catch (error) {
      console.error('데이터 내보내기 실패:', error);
      throw error;
    }
  }

  /**
   * 실시간 구독 설정
   */
  static async subscribeToKeys(keys: string[]): Promise<ApiResponse<{ subscription_id: string }>> {
    return httpClient.post<{ subscription_id: string }>('/api/realtime/subscribe', { keys });
  }

  /**
   * 실시간 구독 해제
   */
  static async unsubscribeFromKeys(subscriptionId: string): Promise<ApiResponse<{ success: boolean }>> {
    return httpClient.delete<{ success: boolean }>(`/api/realtime/subscribe/${subscriptionId}`);
  }
}

// Export as default
export default RedisDataApiService;