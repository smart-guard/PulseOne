// ============================================================================
// frontend/src/api/services/redisDataApi.ts
// 📝 Redis 데이터 조회 API 서비스 - 완성된 최종 버전
// ============================================================================

// 🔥 API 설정 import 추가
import { API_CONFIG } from '../config';

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

// Backend 실제 응답 구조
interface BackendApiResponse<T = any> {
  status: 'success' | 'error';
  message?: string;
  data: T;
  error?: string;
  timestamp?: string;
}

// Frontend 기대 응답 구조
interface ApiResponse<T = any> {
  success: boolean;
  data: T;
  message?: string;
  error?: string;
  timestamp?: string;
}

// =============================================================================
// 완성된 HTTP 클라이언트 - Backend 응답 변환 포함
// =============================================================================

class SimpleHttpClient {
  private baseURL: string;

  constructor(baseURL?: string) {
    this.baseURL = baseURL || API_CONFIG.BASE_URL;
    console.log('🔗 Redis API Base URL:', this.baseURL);
  }

  // 🔥 Backend 응답을 Frontend 형식으로 변환하는 헬퍼
  private transformResponse<T>(backendResponse: BackendApiResponse<T>): ApiResponse<T> {
    return {
      success: backendResponse.status === 'success',
      data: backendResponse.data,
      message: backendResponse.message,
      error: backendResponse.error,
      timestamp: backendResponse.timestamp
    };
  }

  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    const url = `${this.baseURL}${endpoint}`;
    
    console.log('🌐 API Request:', {
      method: options.method || 'GET',
      url: url,
      baseURL: this.baseURL,
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
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const backendData: BackendApiResponse<T> = await response.json();
      
      // 🔥 Backend 응답을 Frontend 형식으로 변환
      const transformedResponse = this.transformResponse(backendData);
      
      console.log('🔄 Response Transformed:', {
        original: backendData,
        transformed: transformedResponse
      });
      
      return transformedResponse;
      
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
    let url = endpoint;
    if (params) {
      const searchParams = new URLSearchParams();
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          searchParams.append(key, String(value));
        }
      });
      const queryString = searchParams.toString();
      if (queryString) {
        url += `?${queryString}`;
      }
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

// 🔥 완성된 HTTP 클라이언트 인스턴스
const httpClient = new SimpleHttpClient();

// =============================================================================
// 완성된 Redis 데이터 API 서비스 클래스
// =============================================================================

export class RedisDataApiService {
  /**
   * 🔧 디버깅용 - API 설정 정보 출력
   */
  static getApiInfo() {
    console.log('🔍 Redis API 설정 정보:', {
      baseURL: API_CONFIG.BASE_URL,
      timeout: API_CONFIG.TIMEOUT,
      viteApiUrl: import.meta.env.VITE_API_URL,
      currentLocation: window.location.href
    });
  }

  /**
   * Redis 연결 상태 확인 - 완성된 응답 처리
   */
  static async getConnectionStatus(): Promise<ApiResponse<{ status: 'connected' | 'disconnected' | 'connecting'; info?: any }>> {
    try {
      console.log('🔍 Redis 연결 상태 확인 시작...');
      RedisDataApiService.getApiInfo();
      
      const response = await httpClient.get('/api/redis/status');
      
      console.log('✅ Redis 연결 상태 최종 응답:', response);
      
      // 🔥 이제 response.success를 신뢰할 수 있음
      if (response.success && response.data) {
        console.log('🎯 연결 상태 데이터:', response.data);
      }
      
      return response;
    } catch (error) {
      console.error('❌ Redis 연결 상태 확인 실패:', error);
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
    console.log('📊 Redis 통계 조회 시작...');
    return httpClient.get<RedisStats>('/api/redis/stats');
  }

  /**
   * Redis 키 트리 구조 조회 (계층적 표시용)
   */
  static async getKeyTree(parentPath: string = ''): Promise<ApiResponse<RedisTreeNode[]>> {
    console.log('🌳 Redis 트리 조회 시작...', { parentPath });
    const params = parentPath ? { parent_path: parentPath } : {};
    return httpClient.get<RedisTreeNode[]>('/api/redis/tree', params);
  }

  /**
   * 특정 노드의 자식 노드들 조회
   */
  static async getNodeChildren(nodeId: string): Promise<ApiResponse<RedisTreeNode[]>> {
    console.log('👶 노드 자식 조회 시작...', { nodeId });
    return httpClient.get<RedisTreeNode[]>(`/api/redis/tree/${nodeId}/children`);
  }

  /**
   * Redis 키 검색
   */
  static async searchKeys(params: RedisSearchParams): Promise<ApiResponse<{ keys: string[]; cursor?: string; total?: number }>> {
    console.log('🔍 키 검색 시작...', params);
    return httpClient.get('/api/redis/keys/search', params);
  }

  /**
   * 특정 키의 데이터 조회
   */
  static async getKeyData(key: string): Promise<ApiResponse<RedisDataPoint>> {
    console.log('🔑 키 데이터 조회 시작...', { key });
    return httpClient.get<RedisDataPoint>(`/api/redis/keys/${encodeURIComponent(key)}/data`);
  }

  /**
   * 여러 키의 데이터 일괄 조회
   */
  static async getBulkKeyData(keys: string[]): Promise<ApiResponse<RedisDataPoint[]>> {
    console.log('📦 일괄 키 데이터 조회 시작...', { keysCount: keys.length });
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
    console.log('⚡ 실시간 현재값 조회 시작...', params);
    return httpClient.get<RedisDataPoint[]>('/api/realtime/current-values', params);
  }

  /**
   * 특정 디바이스의 데이터 포인트들 조회
   */
  static async getDeviceDataPoints(deviceId: number): Promise<ApiResponse<RedisDataPoint[]>> {
    console.log('📱 디바이스 데이터포인트 조회 시작...', { deviceId });
    return httpClient.get<RedisDataPoint[]>(`/api/realtime/device/${deviceId}/values`);
  }

  /**
   * Redis 키 패턴 목록 조회
   */
  static async getKeyPatterns(): Promise<ApiResponse<RedisKeyPattern[]>> {
    console.log('🔄 키 패턴 조회 시작...');
    return httpClient.get<RedisKeyPattern[]>('/api/redis/patterns');
  }

  /**
   * 테넌트별 키 목록 조회
   */
  static async getTenantKeys(tenantId?: number): Promise<ApiResponse<RedisTreeNode[]>> {
    console.log('🏢 테넌트 키 조회 시작...', { tenantId });
    const params = tenantId ? { tenant_id: tenantId } : {};
    return httpClient.get<RedisTreeNode[]>('/api/redis/tenants/keys', params);
  }

  /**
   * 특정 키 삭제 (관리자 전용)
   */
  static async deleteKey(key: string): Promise<ApiResponse<{ deleted: boolean }>> {
    console.log('🗑️ 키 삭제 시작...', { key });
    return httpClient.delete<{ deleted: boolean }>(`/api/redis/keys/${encodeURIComponent(key)}`);
  }

  /**
   * 키 TTL 설정 (관리자 전용)
   */
  static async setKeyTTL(key: string, ttl: number): Promise<ApiResponse<{ success: boolean }>> {
    console.log('⏰ 키 TTL 설정 시작...', { key, ttl });
    return httpClient.put<{ success: boolean }>(`/api/redis/keys/${encodeURIComponent(key)}/ttl`, { ttl });
  }

  /**
   * 데이터 내보내기
   */
  static async exportData(keys: string[], format: 'json' | 'csv' = 'json'): Promise<Blob> {
    try {
      console.log('💾 데이터 내보내기 시작...', { keysCount: keys.length, format });
      
      const response = await fetch(`${API_CONFIG.BASE_URL}/api/redis/export`, {
        method: 'POST',
        headers: API_CONFIG.DEFAULT_HEADERS,
        body: JSON.stringify({ keys, format })
      });
      
      if (!response.ok) {
        throw new Error(`Export failed: ${response.status} ${response.statusText}`);
      }
      
      return await response.blob();
    } catch (error) {
      console.error('❌ 데이터 내보내기 실패:', error);
      throw error;
    }
  }

  /**
   * 실시간 구독 설정
   */
  static async subscribeToKeys(keys: string[]): Promise<ApiResponse<{ subscription_id: string }>> {
    console.log('📡 실시간 구독 설정 시작...', { keysCount: keys.length });
    return httpClient.post<{ subscription_id: string }>('/api/realtime/subscribe', { keys });
  }

  /**
   * 실시간 구독 해제
   */
  static async unsubscribeFromKeys(subscriptionId: string): Promise<ApiResponse<{ success: boolean }>> {
    console.log('📡 실시간 구독 해제 시작...', { subscriptionId });
    return httpClient.delete<{ success: boolean }>(`/api/realtime/subscribe/${subscriptionId}`);
  }

  /**
   * 🔧 테스트용 - 직접 API 호출
   */
  static async testDirectApiCall(): Promise<void> {
    console.log('🧪 직접 API 호출 테스트 시작...');
    
    try {
      // 1. 기본 API 정보 조회
      const infoResponse = await fetch(`${API_CONFIG.BASE_URL}/api/info`);
      const infoData = await infoResponse.json();
      console.log('📋 API 정보:', infoData);
      
      // 2. Redis 상태 조회
      const statusResponse = await fetch(`${API_CONFIG.BASE_URL}/api/redis/status`);
      const statusData = await statusResponse.json();
      console.log('🔗 Redis 상태:', statusData);
      
    } catch (error) {
      console.error('❌ 직접 API 호출 테스트 실패:', error);
    }
  }
}

// =============================================================================
// 🔧 디버깅용 - Backend 응답 구조 테스트 클래스
// =============================================================================

export class RedisApiDebugger {
  /**
   * Backend 응답 구조 확인
   */
  static async testBackendResponse(): Promise<void> {
    console.log('🧪 Backend 응답 구조 테스트 시작...');
    
    try {
      const response = await fetch(`${API_CONFIG.BASE_URL}/api/redis/status`);
      const rawData = await response.json();
      
      console.log('📋 Backend 원시 응답:', rawData);
      console.log('📊 응답 구조 분석:', {
        hasStatus: 'status' in rawData,
        hasData: 'data' in rawData,
        hasSuccess: 'success' in rawData,
        statusValue: rawData.status,
        dataValue: rawData.data
      });
      
    } catch (error) {
      console.error('❌ Backend 응답 테스트 실패:', error);
    }
  }

  /**
   * API 변환 로직 테스트
   */
  static async testResponseTransformation(): Promise<void> {
    console.log('🔄 응답 변환 로직 테스트 시작...');
    
    try {
      const response = await RedisDataApiService.getConnectionStatus();
      
      console.log('🎯 변환된 응답:', response);
      console.log('✅ 변환 성공 여부:', response.success);
      console.log('📊 응답 데이터:', response.data);
      
    } catch (error) {
      console.error('❌ 응답 변환 테스트 실패:', error);
    }
  }
}

// Export as default
export default RedisDataApiService;