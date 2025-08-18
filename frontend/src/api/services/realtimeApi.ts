// ============================================================================
// frontend/src/api/services/realtimeApi.ts
// 실시간 데이터 API 서비스 - 백엔드 realtime.js 완전 호환 버전
// ============================================================================

import { API_CONFIG } from '../config';
import { ENDPOINTS } from '../endpoints';
import { ApiResponse } from '../../types/common';

// ============================================================================
// 🔥 백엔드 realtime.js와 완전 일치하는 인터페이스들
// ============================================================================

export interface RealtimeValue {
  id: string;
  key: string;
  point_id?: number;
  device_id: string;                              // 🔥 수정: number → string
  device_name: string;                            // 🔥 추가
  point_name: string;                             // 🔥 수정: name → point_name
  value: any;
  data_type: 'number' | 'boolean' | 'string' | 'integer';  // 🔥 수정: dataType → data_type
  unit?: string;
  timestamp: string;
  quality: 'good' | 'bad' | 'uncertain' | 'comm_failure' | 'last_known';  // 🔥 추가: comm_failure, last_known
  changed?: boolean;                              // 🔥 추가
  source: 'redis' | 'database' | 'simulation';
  metadata?: {
    update_count: number;
    last_error?: string;
    protocol?: string;
    device_id?: number;
    data_point_id?: number;
    last_communication?: string;
    error_count?: number;
  };
  trend?: {
    direction: 'increasing' | 'decreasing' | 'stable';
    rate: string;
    prediction: {
      next_5min: string;
      next_15min: string;
      confidence: string;
    };
  };
}

export interface DeviceRealtimeData {
  device_id: number;
  device_name: string;
  device_type: string;                            // 🔥 추가
  connection_status: string;
  ip_address?: string;                            // 🔥 추가
  last_communication: string;
  response_time?: number;                         // 🔥 추가
  error_count: number;                            // 🔥 추가
  data_points: RealtimeValue[];
  total_points: number;
  summary: {
    good_quality: number;
    bad_quality: number;
    uncertain_quality: number;
    comm_failure: number;                         // 🔥 추가
    last_known: number;                           // 🔥 추가
    last_update: string;                          // 🔥 수정: number → string
  };
}

export interface SubscriptionRequest {
  keys?: string[];
  point_ids?: number[];
  device_ids?: string[];                          // 🔥 수정: number[] → string[]
  update_interval?: number;
  filters?: {
    data_type?: string;
    quality_filter?: string;
    device_filter?: string[];                     // 🔥 수정: number[] → string[]
  };
  callback_url?: string;
}

export interface SubscriptionInfo {
  subscription_id: string;
  websocket_url: string;
  http_polling_url: string;
  subscription_info: {
    total_keys: number;
    update_interval: number;
    estimated_updates_per_minute: number;
    expires_at: string;
  };
  preview_keys: string[];                         // 🔥 추가
  subscription: {
    id: string;
    tenant_id: number;
    user_id: number;
    keys: string[];
    point_ids: number[];
    device_ids: string[];                         // 🔥 수정: number[] → string[]
    update_interval: number;
    filters: any;
    callback_url?: string;
    created_at: string;
    status: 'active' | 'inactive';
    client_info: {
      ip: string;
      user_agent: string;
    };
  };
}

export interface SubscriptionSummary {
  id: string;
  keys_count: number;
  status: 'active' | 'inactive';
  created_at: string;
  last_update: string;
  update_interval: number;
  client_ip: string;
  expires_at: string;
}

export interface PollingData {
  subscription_id: string;
  updates: Array<RealtimeValue & {
    subscription_id: string;
    update_type: string;
    previous_value?: any;
  }>;
  total_updates: number;
  polling_info: {
    next_poll_after: string;
    recommended_interval: number;
    since: string;
  };
  subscription_status: string;
}

export interface RealtimeStats {
  tenant_id: number;
  system_status: string;
  redis: {
    connected: boolean;
    memory_usage?: string;
    total_keys?: number;
    error?: string;
  };
  subscriptions: {
    total_subscriptions: number;
    active_subscriptions: number;
    total_keys_monitored: number;
  };
  performance: {
    realtime_connections: number;
    messages_per_second: number;
    data_points_monitored: number;
    average_latency_ms: number;
    uptime_seconds: number;
    last_restart: string;
    error_rate: string;
    cache_hit_rate: string;
  };
  last_updated: string;
}

export interface CurrentValuesParams {
  device_ids?: string[];                          // 🔥 수정: number[] → string[]
  point_names?: string[];                         // 🔥 수정: point_ids → point_names
  site_id?: number;
  data_type?: string;
  quality_filter?: string;
  limit?: number;
  sort_by?: string;                               // 🔥 추가
  source?: 'auto' | 'redis' | 'database';
}

export interface CurrentValuesResponse {
  current_values: RealtimeValue[];
  total_count: number;
  data_source: string;
  filters_applied: {
    device_ids: string;
    point_names: string;
    quality_filter: string;
  };
  performance: {
    query_time_ms: number;
    redis_keys_scanned: number;                   // 🔥 수정: cache_hit_rate → redis_keys_scanned
  };
}

// 🔥 추가: 디바이스 목록 응답
export interface DevicesResponse {
  devices: Array<{
    device_id: string;
    device_name: string;
    point_count: number;
    status: string;
    last_seen?: string;
  }>;
  total_count: number;
  total_points: number;
}

// 🔥 추가: 개별 포인트 조회 응답
export interface PointsResponse {
  points: RealtimeValue[];
  requested_count: number;
  found_count: number;
  errors?: string[];
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
    
    console.log('🌐 Realtime API Request:', {
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
      
      console.log('📡 Realtime API Response:', {
        status: response.status,
        ok: response.ok,
        url: response.url
      });
      
      if (!response.ok) {
        const errorData = await response.json().catch(() => ({}));
        throw new Error(errorData.message || errorData.error || `HTTP ${response.status}: ${response.statusText}`);
      }

      const data: ApiResponse<T> = await response.json();
      
      // Backend 응답 변환
      if ('success' in data) {
        return {
          success: data.success,
          data: data.data,
          message: data.message,
          error: data.error,
          timestamp: data.timestamp
        };
      }
      
      return data;
      
    } catch (error) {
      console.error('❌ Realtime API Request failed:', {
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
          if (Array.isArray(value)) {
            queryParams.append(key, value.join(','));
          } else {
            queryParams.append(key, String(value));
          }
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

  async delete<T>(endpoint: string): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, { method: 'DELETE' });
  }
}

// ============================================================================
// 🔄 실시간 데이터 API 서비스 클래스
// ============================================================================

export class RealtimeApiService {
  private static httpClient = new HttpClient();
  private static webSocket: WebSocket | null = null;
  private static subscriptions: Map<string, SubscriptionInfo> = new Map();

  // ========================================================================
  // ⚡ 실시간 현재값 조회
  // ========================================================================

  /**
   * 현재값 일괄 조회 (Redis + Database 통합)
   */
  static async getCurrentValues(params?: CurrentValuesParams): Promise<ApiResponse<CurrentValuesResponse>> {
    console.log('⚡ 실시간 현재값 조회:', params);
    
    // 🔥 백엔드 realtime.js가 기대하는 파라미터 형식으로 변환
    const queryParams: Record<string, any> = {};
    
    if (params?.device_ids && params.device_ids.length > 0) {
      queryParams.device_ids = params.device_ids.join(',');  // string 배열을 쉼표로 연결
    }
    
    if (params?.point_names && params.point_names.length > 0) {
      queryParams.point_names = params.point_names.join(',');
    }
    
    if (params?.quality_filter && params.quality_filter !== 'all') {
      queryParams.quality_filter = params.quality_filter;
    }
    
    if (params?.limit) {
      queryParams.limit = params.limit;
    }
    
    if (params?.sort_by) {
      queryParams.sort_by = params.sort_by;
    }

    return this.httpClient.get<CurrentValuesResponse>('/api/realtime/current-values', queryParams);
  }

  /**
   * 특정 디바이스의 실시간 값 조회
   */
  static async getDeviceValues(deviceId: string, params?: {  // 🔥 수정: number → string
    data_type?: string;
    include_metadata?: boolean;
    include_trends?: boolean;
  }): Promise<ApiResponse<DeviceRealtimeData>> {
    console.log('⚡ 디바이스 실시간 값 조회:', { deviceId, params });
    return this.httpClient.get<DeviceRealtimeData>(`/api/realtime/device/${deviceId}/values`, params);
  }

  /**
   * 🔥 추가: 모든 디바이스 목록 조회
   */
  static async getDevices(): Promise<ApiResponse<DevicesResponse>> {
    console.log('📋 디바이스 목록 조회');
    return this.httpClient.get<DevicesResponse>('/api/realtime/devices');
  }

  /**
   * 🔥 추가: 개별 포인트들 조회 (키 이름으로)
   */
  static async getPoints(keys: string[]): Promise<ApiResponse<PointsResponse>> {
    console.log('🔍 개별 포인트 조회:', keys);
    
    return this.httpClient.get<PointsResponse>('/api/realtime/points', {
      keys: keys.join(',')
    });
  }

  // ========================================================================
  // 🔄 구독 관리
  // ========================================================================

  /**
   * 실시간 데이터 구독 생성
   */
  static async createSubscription(request: SubscriptionRequest): Promise<ApiResponse<SubscriptionInfo>> {
    console.log('🔄 구독 생성:', request);
    const response = await this.httpClient.post<SubscriptionInfo>('/api/realtime/subscribe', request);
    
    if (response.success && response.data) {
      this.subscriptions.set(response.data.subscription_id, response.data);
    }
    
    return response;
  }

  /**
   * 구독 해제
   */
  static async unsubscribe(subscriptionId: string): Promise<ApiResponse<{
    subscription_id: string;
    unsubscribed_at: string;
    was_active: boolean;
    cleanup_completed: boolean;
  }>> {
    console.log('🔄 구독 해제:', subscriptionId);
    const response = await this.httpClient.delete<any>(`/api/realtime/subscribe/${subscriptionId}`);
    
    if (response.success) {
      this.subscriptions.delete(subscriptionId);
    }
    
    return response;
  }

  /**
   * 활성 구독 목록 조회
   */
  static async getSubscriptions(params?: {
    status?: 'all' | 'active' | 'inactive';
    limit?: number;
  }): Promise<ApiResponse<{
    subscriptions: SubscriptionSummary[];
    total_count: number;
    active_count: number;
    inactive_count: number;
    summary: {
      total_keys_monitored: number;
      average_update_interval: number;
    };
  }>> {
    console.log('🔄 구독 목록 조회:', params);
    return this.httpClient.get<any>('/api/realtime/subscriptions', params);
  }

  /**
   * HTTP 폴링을 통한 구독 데이터 조회
   */
  static async pollSubscription(subscriptionId: string, since?: string): Promise<ApiResponse<PollingData>> {
    console.log('📡 구독 폴링:', { subscriptionId, since });
    const params = since ? { since } : undefined;
    return this.httpClient.get<PollingData>(`/api/realtime/poll/${subscriptionId}`, params);
  }

  // ========================================================================
  // 📊 실시간 통계 및 모니터링
  // ========================================================================

  /**
   * 실시간 시스템 통계 조회
   */
  static async getRealtimeStats(): Promise<ApiResponse<RealtimeStats>> {
    console.log('📊 실시간 시스템 통계 조회');
    return this.httpClient.get<RealtimeStats>('/api/realtime/stats');
  }

  // ========================================================================
  // 🌐 WebSocket 연결 관리
  // ========================================================================

  /**
   * WebSocket 연결 생성
   */
  static connectWebSocket(subscriptionId: string, callbacks: {
    onOpen?: () => void;
    onMessage?: (data: RealtimeValue) => void;
    onError?: (error: Event) => void;
    onClose?: () => void;
  }): WebSocket | null {
    try {
      const subscription = this.subscriptions.get(subscriptionId);
      if (!subscription) {
        console.error('❌ 구독 정보를 찾을 수 없음:', subscriptionId);
        return null;
      }

      const wsUrl = `${API_CONFIG.BASE_URL.replace('http', 'ws')}${subscription.websocket_url}`;
      console.log('🌐 WebSocket 연결 시도:', wsUrl);

      this.webSocket = new WebSocket(wsUrl);

      this.webSocket.onopen = () => {
        console.log('✅ WebSocket 연결 성공');
        callbacks.onOpen?.();
      };

      this.webSocket.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          console.log('📨 WebSocket 메시지 수신:', data);
          callbacks.onMessage?.(data);
        } catch (error) {
          console.error('❌ WebSocket 메시지 파싱 실패:', error);
        }
      };

      this.webSocket.onerror = (error) => {
        console.error('❌ WebSocket 에러:', error);
        callbacks.onError?.(error);
      };

      this.webSocket.onclose = () => {
        console.log('🔌 WebSocket 연결 종료');
        this.webSocket = null;
        callbacks.onClose?.();
      };

      return this.webSocket;
    } catch (error) {
      console.error('❌ WebSocket 연결 실패:', error);
      return null;
    }
  }

  /**
   * WebSocket 연결 해제
   */
  static disconnectWebSocket(): void {
    if (this.webSocket) {
      console.log('🔌 WebSocket 연결 해제');
      this.webSocket.close();
      this.webSocket = null;
    }
  }

  /**
   * WebSocket 상태 확인
   */
  static getWebSocketState(): {
    connected: boolean;
    readyState?: number;
    url?: string;
  } {
    if (!this.webSocket) {
      return { connected: false };
    }

    return {
      connected: this.webSocket.readyState === WebSocket.OPEN,
      readyState: this.webSocket.readyState,
      url: this.webSocket.url
    };
  }

  // ========================================================================
  // 🔧 유틸리티 메서드들
  // ========================================================================

  /**
   * 구독 ID에서 구독 정보 조회
   */
  static getSubscriptionInfo(subscriptionId: string): SubscriptionInfo | null {
    return this.subscriptions.get(subscriptionId) || null;
  }

  /**
   * 모든 구독 정보 조회
   */
  static getAllSubscriptions(): SubscriptionInfo[] {
    return Array.from(this.subscriptions.values());
  }

  /**
   * 실시간 값 품질별 필터링
   */
  static filterValuesByQuality(values: RealtimeValue[], quality?: string): RealtimeValue[] {
    if (!quality || quality === 'all') return values;
    return values.filter(v => v.quality === quality);
  }

  /**
   * 디바이스별 실시간 값 그룹화
   */
  static groupValuesByDevice(values: RealtimeValue[]): Record<string, RealtimeValue[]> {  // 🔥 수정: number → string
    return values.reduce((groups, value) => {
      const deviceId = value.device_id;
      if (!groups[deviceId]) {
        groups[deviceId] = [];
      }
      groups[deviceId].push(value);
      return groups;
    }, {} as Record<string, RealtimeValue[]>);
  }

  /**
   * 데이터 타입별 실시간 값 그룹화
   */
  static groupValuesByDataType(values: RealtimeValue[]): Record<string, RealtimeValue[]> {
    return values.reduce((groups, value) => {
      const dataType = value.data_type;              // 🔥 수정: dataType → data_type
      if (!groups[dataType]) {
        groups[dataType] = [];
      }
      groups[dataType].push(value);
      return groups;
    }, {} as Record<string, RealtimeValue[]>);
  }

  /**
   * 실시간 값 통계 계산
   */
  static calculateRealtimeStatistics(values: RealtimeValue[]): {
    total: number;
    by_quality: Record<string, number>;
    by_data_type: Record<string, number>;
    by_device: Record<string, number>;              // 🔥 수정: number → string
    latest_timestamp: string | null;
    value_range: {
      numeric_values: number[];
      min?: number;
      max?: number;
      avg?: number;
    };
  } {
    const stats = {
      total: values.length,
      by_quality: {} as Record<string, number>,
      by_data_type: {} as Record<string, number>,
      by_device: {} as Record<string, number>,      // 🔥 수정: number → string
      latest_timestamp: null as string | null,
      value_range: {
        numeric_values: [] as number[]
      }
    };

    values.forEach(value => {
      // 품질별 카운트
      stats.by_quality[value.quality] = (stats.by_quality[value.quality] || 0) + 1;
      
      // 데이터 타입별 카운트
      stats.by_data_type[value.data_type] = (stats.by_data_type[value.data_type] || 0) + 1;  // 🔥 수정: dataType → data_type
      
      // 디바이스별 카운트
      stats.by_device[value.device_id] = (stats.by_device[value.device_id] || 0) + 1;
      
      // 숫자 값 수집
      if (value.data_type === 'number' && typeof value.value === 'number') {  // 🔥 수정: dataType → data_type
        stats.value_range.numeric_values.push(value.value);
      }
    });

    // 숫자 값 통계
    if (stats.value_range.numeric_values.length > 0) {
      const numericValues = stats.value_range.numeric_values;
      stats.value_range.min = Math.min(...numericValues);
      stats.value_range.max = Math.max(...numericValues);
      stats.value_range.avg = numericValues.reduce((sum, val) => sum + val, 0) / numericValues.length;
    }

    // 최신 타임스탬프
    if (values.length > 0) {
      const timestamps = values.map(v => new Date(v.timestamp).getTime());
      const latestTimestamp = Math.max(...timestamps);
      stats.latest_timestamp = new Date(latestTimestamp).toISOString();
    }

    return stats;
  }

  /**
   * 구독 요청 유효성 검사
   */
  static validateSubscriptionRequest(request: SubscriptionRequest): {
    valid: boolean;
    errors: string[];
  } {
    const errors: string[] = [];

    // 최소 하나의 키/포인트/디바이스 필요
    if (!request.keys?.length && !request.point_ids?.length && !request.device_ids?.length) {
      errors.push('At least one of keys, point_ids, or device_ids is required');
    }

    // 업데이트 간격 제한 (100ms ~ 60초)
    if (request.update_interval !== undefined) {
      if (request.update_interval < 100 || request.update_interval > 60000) {
        errors.push('Update interval must be between 100ms and 60000ms');
      }
    }

    // 키 개수 제한 (최대 1000개)
    const totalKeys = (request.keys?.length || 0) + 
                     (request.point_ids?.length || 0) + 
                     (request.device_ids?.length || 0);
    if (totalKeys > 1000) {
      errors.push('Total number of keys/points/devices cannot exceed 1000');
    }

    return {
      valid: errors.length === 0,
      errors
    };
  }

  /**
   * 자동 재연결 설정
   */
  static setupAutoReconnect(subscriptionId: string, callbacks: {
    onOpen?: () => void;
    onMessage?: (data: RealtimeValue) => void;
    onError?: (error: Event) => void;
    onClose?: () => void;
  }, options: {
    maxRetries?: number;
    retryDelay?: number;
  } = {}): void {
    const maxRetries = options.maxRetries || 5;
    const retryDelay = options.retryDelay || 3000;
    let retryCount = 0;

    const connect = () => {
      const ws = this.connectWebSocket(subscriptionId, {
        ...callbacks,
        onClose: () => {
          callbacks.onClose?.();
          
          // 자동 재연결 시도
          if (retryCount < maxRetries) {
            retryCount++;
            console.log(`🔄 WebSocket 재연결 시도 ${retryCount}/${maxRetries} (${retryDelay}ms 후)`);
            setTimeout(connect, retryDelay);
          } else {
            console.error('❌ WebSocket 최대 재연결 시도 횟수 초과');
          }
        }
      });

      if (ws && ws.readyState === WebSocket.OPEN) {
        retryCount = 0; // 연결 성공 시 재시도 카운트 리셋
      }
    };

    connect();
  }

  /**
   * 실시간 값 변화율 계산
   */
  static calculateValueTrend(currentValue: any, previousValue: any, timeInterval: number): {
    direction: 'increasing' | 'decreasing' | 'stable';
    rate: number;
    percentage: number;
  } {
    const current = parseFloat(currentValue) || 0;
    const previous = parseFloat(previousValue) || 0;
    
    const difference = current - previous;
    const rate = timeInterval > 0 ? difference / timeInterval : 0;
    const percentage = previous !== 0 ? (difference / previous) * 100 : 0;
    
    let direction: 'increasing' | 'decreasing' | 'stable' = 'stable';
    if (Math.abs(difference) > 0.001) {
      direction = difference > 0 ? 'increasing' : 'decreasing';
    }
    
    return {
      direction,
      rate: Math.abs(rate),
      percentage: Math.abs(percentage)
    };
  }

  /**
   * 폴링 간격 최적화 계산
   */
  static optimizePollingInterval(values: RealtimeValue[], options: {
    maxInterval?: number;
    minInterval?: number;
    targetUpdateRate?: number;
  } = {}): number {
    const maxInterval = options.maxInterval || 10000; // 10초
    const minInterval = options.minInterval || 500;   // 0.5초
    const targetUpdateRate = options.targetUpdateRate || 0.1; // 10% 변화율
    
    if (values.length === 0) return maxInterval;
    
    // 최근 값들의 변화율 분석
    const recentChanges = values
      .filter(v => v.data_type === 'number')          // 🔥 수정: dataType → data_type
      .map(v => parseFloat(v.value as string) || 0);
    
    if (recentChanges.length < 2) return maxInterval;
    
    // 변화율 계산
    let totalChange = 0;
    for (let i = 1; i < recentChanges.length; i++) {
      const change = Math.abs(recentChanges[i] - recentChanges[i-1]);
      const average = (recentChanges[i] + recentChanges[i-1]) / 2;
      if (average > 0) {
        totalChange += change / average;
      }
    }
    
    const averageChangeRate = totalChange / (recentChanges.length - 1);
    
    // 변화율에 따른 간격 조정
    if (averageChangeRate > targetUpdateRate) {
      return Math.max(minInterval, maxInterval * 0.3); // 빠른 폴링
    } else if (averageChangeRate > targetUpdateRate * 0.5) {
      return Math.max(minInterval, maxInterval * 0.6); // 중간 폴링
    } else {
      return maxInterval; // 느린 폴링
    }
  }
}