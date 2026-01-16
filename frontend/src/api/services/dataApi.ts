// ============================================================================
// frontend/src/api/services/dataApi.ts
// 데이터 익스플로러 API 서비스 - 최종 완성본
// ============================================================================

import { API_CONFIG } from '../config';
import { ENDPOINTS } from '../endpoints';
import {
  ApiResponse,
  PaginatedApiResponse,
  PaginationParams
} from '../../types/common';

// ============================================================================
// 📊 데이터 관련 인터페이스들
// ============================================================================

export interface DataPoint {
  id: number;
  name: string;
  device_id: number;
  device_name?: string;
  address: string;
  data_type: 'number' | 'boolean' | 'string';
  original_data_type?: string; // 원본 C++ 타입 (FLOAT32, UINT32 등)
  unit?: string;
  is_enabled: boolean;
  description?: string;
  min_value?: number;
  max_value?: number;
  scaling_factor?: number;
  scaling_offset?: number;
  polling_interval?: number;
  access_mode?: 'read' | 'write' | 'read_write';
  is_log_enabled?: boolean;
  log_interval_ms?: number;
  log_deadband?: number;
  is_alarm_enabled?: boolean;
  alarm_priority?: 'low' | 'medium' | 'high' | 'critical';
  high_alarm_limit?: number;
  low_alarm_limit?: number;
  alarm_deadband?: number;
  group_name?: string;
  tags?: string[];
  metadata?: any;
  protocol_params?: any;
  created_at: string;
  updated_at: string;
  tenant_id?: number;
  current_value?: CurrentValue;
  device_info?: {
    id: number;
    name: string;
    protocol_type: string;
    connection_status: string;
  };
}

export interface CurrentValue {
  id: number;
  point_id: number;
  value: any;
  data_type: string;
  unit?: string;
  timestamp: string;
  quality: 'good' | 'bad' | 'uncertain';
  source?: string;
  metadata?: {
    update_count: number;
    last_error?: string;
    protocol?: string;
  };
}

export interface HistoricalDataPoint {
  time: string;
  point_id: number;
  value: any;
  quality: string;
}

export interface DataStatistics {
  data_points: {
    total_data_points: number;
    enabled_data_points: number;
    disabled_data_points: number;
    data_type_distribution: Record<string, number>;
    device_distribution: Array<{
      device_id: number;
      device_name: string;
      data_points_count: number;
    }>;
  };
  current_values: {
    total_values: number;
    good_quality: number;
    bad_quality: number;
    uncertain_quality: number;
    last_update: string;
  };
  device_stats?: {
    device_id: number;
    total_data_points: number;
    enabled_data_points: number;
    data_types: string[];
  };
  site_stats?: {
    site_id: number;
    total_devices: number;
    total_data_points: number;
    data_collection_rate: string;
  };
  system_stats: {
    total_devices: number;
    active_devices: number;
    data_collection_rate: string;
    average_response_time: number;
    last_updated: string;
  };
}

export interface DataPointSearchParams extends PaginationParams {
  search?: string;
  device_id?: number;
  site_id?: number;
  data_type?: string;
  enabled_only?: boolean;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
  include_current_value?: boolean;
}

export interface CurrentValueParams {
  point_ids?: number[];
  device_ids?: number[];
  site_id?: number;
  data_type?: string;
  quality_filter?: string;
  limit?: number;
  source?: string;
  include_metadata?: boolean;
}

export interface HistoricalDataParams {
  point_ids: number[];
  start_time: string;
  end_time: string;
  interval?: string;
  aggregation?: string;
  limit?: number;
}

export interface AdvancedQueryParams {
  query_type: 'aggregation' | 'analysis' | 'correlation';
  filters: any;
  aggregations?: any;
  time_range?: {
    start: string;
    end: string;
  };
  output_format?: 'json' | 'csv' | 'xml';
}

export interface ExportParams {
  export_type: 'current' | 'historical' | 'configuration';
  point_ids?: number[];
  device_ids?: number[];
  start_time?: string;
  end_time?: string;
  format?: 'json' | 'csv' | 'xml';
  include_metadata?: boolean;
}

export interface ExportResult {
  export_id: string;
  filename: string;
  format: string;
  export_type: string;
  total_records: number;
  file_size_mb: string;
  generated_at: string;
  download_url: string;
  data?: any;
}

// ============================================================================
// 🔧 HTTP 클라이언트 클래스 - URL 중복 문제 수정
// ============================================================================

class HttpClient {
  private baseUrl: string;

  constructor(baseUrl: string = API_CONFIG.BASE_URL) {
    this.baseUrl = baseUrl;
  }

  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    // 🔥 URL 중복 방지 로직
    let url: string;

    if (endpoint.startsWith('http://') || endpoint.startsWith('https://')) {
      // 이미 완전한 URL인 경우
      url = endpoint;
    } else if (endpoint.startsWith('/')) {
      // 절대 경로인 경우 (/api/... 등)
      url = `${this.baseUrl}${endpoint}`;
    } else {
      // 상대 경로인 경우
      url = `${this.baseUrl}/${endpoint}`;
    }

    console.log('🌐 Data API Request:', {
      method: options.method || 'GET',
      url: url,
      endpoint: endpoint
    });

    const config: RequestInit = {
      headers: {
        'Content-Type': 'application/json',
        ...API_CONFIG.DEFAULT_HEADERS,
        ...options.headers,
      },
      ...options,
    };

    try {
      const response = await fetch(url, config);

      console.log('📡 Data API Response:', {
        status: response.status,
        ok: response.ok,
        url: response.url
      });

      if (!response.ok) {
        const errorData = await response.json().catch(() => ({}));
        throw new Error(errorData.message || errorData.error || `HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();

      // Backend 응답 형식 보장
      if ('success' in data) {
        return data as ApiResponse<T>;
      }

      // 표준 형식이 아닌 경우 변환
      return {
        success: true,
        data: data as T,
        message: 'Success'
      } as ApiResponse<T>;

    } catch (error) {
      console.error('❌ Data API Request failed:', {
        endpoint,
        url,
        error: error instanceof Error ? error.message : 'Unknown error'
      });

      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error',
        message: error instanceof Error ? error.message : 'Unknown error',
        data: null as any
      };
    }
  }

  async get<T>(endpoint: string, params?: Record<string, any>): Promise<ApiResponse<T>> {
    if (params) {
      const queryParams = new URLSearchParams();

      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          if (Array.isArray(value)) {
            queryParams.append(key, value.join(','));
          } else {
            queryParams.append(key, String(value));
          }
        }
      });

      const queryString = queryParams.toString();
      if (queryString) {
        endpoint = endpoint.includes('?')
          ? `${endpoint}&${queryString}`
          : `${endpoint}?${queryString}`;
      }
    }

    return this.request<T>(endpoint, { method: 'GET' });
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

  async patch<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'PATCH',
      body: data ? JSON.stringify(data) : undefined
    });
  }
}

// ============================================================================
// 📊 데이터 익스플로러 API 서비스 클래스
// ============================================================================

export class DataApiService {
  private static httpClient = new HttpClient();

  // ========================================================================
  // 🔍 데이터포인트 검색 및 조회
  // ========================================================================

  /**
   * 데이터포인트 검색 (페이징, 필터링, 정렬 지원)
   */
  static async searchDataPoints(params?: DataPointSearchParams): Promise<ApiResponse<{
    items: DataPoint[];
    pagination: {
      page: number;
      limit: number;
      total: number;
      totalPages: number;
      hasNext: boolean;
      hasPrev: boolean;
    };
  }>> {
    console.log('🔍 데이터포인트 검색:', params);
    return this.httpClient.get<any>(ENDPOINTS.DATA_POINTS, params);
  }

  /**
   * 특정 데이터포인트 상세 조회
   */
  static async getDataPoint(id: number, options?: {
    include_current_value?: boolean;
    include_device_info?: boolean;
  }): Promise<ApiResponse<DataPoint>> {
    console.log('🔍 데이터포인트 상세 조회:', { id, options });
    return this.httpClient.get<DataPoint>(`/api/data/points/${id}`, options);
  }

  /**
   * 데이터포인트 목록 조회 (InputVariableSourceSelector 전용)
   */
  static async getDataPoints(filters?: {
    search?: string;
    device_id?: number;
    site_id?: number;
    data_type?: 'number' | 'boolean' | 'string';
    enabled_only?: boolean;
    sort_by?: string;
    sort_order?: 'ASC' | 'DESC';
    include_current_value?: boolean;
    page?: number;
    limit?: number;
  }): Promise<{
    points: DataPoint[];
    totalCount: number;
    pagination: {
      page: number;
      limit: number;
      hasNext: boolean;
      hasPrev: boolean;
      total: number;
      totalPages: number;
    };
    transformationInfo?: {
      types_converted: boolean;
      original_count: number;
      filtered_count: number;
    };
  }> {
    console.log('🔄 데이터포인트 목록 조회:', filters);

    const response = await this.searchDataPoints({
      ...filters,
      page: filters?.page || 1,
      limit: filters?.limit || 1000
    });

    if (response.success && response.data) {
      const { items = [], pagination } = response.data;

      return {
        points: items,
        totalCount: pagination?.total || items.length,
        pagination: {
          page: pagination?.page || 1,
          limit: pagination?.limit || 1000,
          hasNext: pagination?.hasNext || false,
          hasPrev: pagination?.hasPrev || false,
          total: pagination?.total || items.length,
          totalPages: pagination?.totalPages || 1
        },
        transformationInfo: {
          types_converted: true,
          original_count: items.length,
          filtered_count: items.length
        }
      };
    } else {
      throw new Error(response.message || 'API 응답 처리 실패');
    }
  }

  /**
   * 디바이스 목록 조회
   */
  static async getDevices(): Promise<Array<{
    id: number;
    name: string;
    protocol_type: string;
    connection_status: string;
  }>> {
    console.log('🔄 디바이스 목록 조회');

    const response = await fetch('/api/devices?limit=100&enabled_only=true');

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }

    const result = await response.json();
    console.log('📦 디바이스 API 응답:', result);

    if (result.success && result.data?.items) {
      return result.data.items;
    } else if (Array.isArray(result.data)) {
      return result.data;
    } else {
      throw new Error(result.message || '디바이스 API 응답 구조 오류');
    }
  }

  // ========================================================================
  // ⚡ 실시간 현재값 조회
  // ========================================================================

  /**
   * 현재값 일괄 조회
   */
  static async getCurrentValues(params?: CurrentValueParams): Promise<ApiResponse<{
    current_values: CurrentValue[];
    total_count: number;
    data_source: string;
    filters_applied: any;
    performance: {
      query_time_ms: number;
      cache_hit_rate: string;
    };
  }>> {
    console.log('⚡ 현재값 일괄 조회:', params);
    return this.httpClient.get<any>(ENDPOINTS.DATA_CURRENT_VALUES, params);
  }

  /**
   * 특정 디바이스의 현재값 조회
   */
  static async getDeviceCurrentValues(deviceId: number, params?: {
    data_type?: string;
    quality_filter?: string;
    limit?: number;
  }): Promise<ApiResponse<{
    device_id: number;
    device_name: string;
    current_values: CurrentValue[];
    total_count: number;
    last_updated: string;
  }>> {
    console.log('⚡ 디바이스 현재값 조회:', { deviceId, params });
    return this.httpClient.get<any>(`/api/data/devices/${deviceId}/current-values`, params);
  }

  /**
   * 여러 디바이스의 상태 및 Redis 데이터 존재 여부 일괄 조회 (추가됨)
   */
  static async getBulkDeviceStatus(deviceIds: number[]): Promise<ApiResponse<Record<number, {
    connection_status: string;
    hasRedisData: boolean;
    last_seen?: string;
  }>>> {
    console.log('⚡ 디바이스 상태 일괄 조회:', deviceIds);
    return this.httpClient.post<any>('/api/data/devices/status', { device_ids: deviceIds });
  }

  // ========================================================================
  // 📊 이력 데이터 조회
  // ========================================================================

  /**
   * 이력 데이터 조회
   */
  static async getHistoricalData(params: HistoricalDataParams): Promise<ApiResponse<{
    data_points: Array<{
      point_id: number;
      point_name: string;
      device_name: string;
      data_type: string;
      unit?: string;
    }>;
    historical_data: HistoricalDataPoint[];
    query_info: {
      start_time: string;
      end_time: string;
      interval?: string;
      aggregation?: string;
      total_points: number;
    };
  }>> {
    console.log('📊 이력 데이터 조회:', params);
    return this.httpClient.get<any>(ENDPOINTS.DATA_HISTORICAL, params);
  }

  // ========================================================================
  // 🔍 고급 검색 및 쿼리
  // ========================================================================

  /**
   * 고급 데이터 쿼리 실행
   */
  static async executeAdvancedQuery(params: AdvancedQueryParams): Promise<ApiResponse<{
    query_type: string;
    execution_time_ms: number;
    results?: any[];
    aggregations?: any;
    analysis_results?: any;
  }>> {
    console.log('🔍 고급 쿼리 실행:', params);
    return this.httpClient.post<any>(ENDPOINTS.DATA_QUERY, params);
  }

  // ========================================================================
  // 📊 데이터 통계 및 분석
  // ========================================================================

  /**
   * 데이터 통계 조회
   */
  static async getDataStatistics(params?: {
    device_id?: number;
    site_id?: number;
    time_range?: string;
  }): Promise<ApiResponse<DataStatistics>> {
    console.log('📊 데이터 통계 조회:', params);
    return this.httpClient.get<DataStatistics>(ENDPOINTS.DATA_STATISTICS, params);
  }

  // ========================================================================
  // 📤 데이터 내보내기
  // ========================================================================

  /**
   * 데이터 내보내기
   */
  static async exportData(params: ExportParams): Promise<ApiResponse<ExportResult>> {
    console.log('📤 데이터 내보내기:', params);
    return this.httpClient.post<ExportResult>(ENDPOINTS.DATA_EXPORT, params);
  }

  /**
   * 현재값 내보내기
   */
  static async exportCurrentValues(params: {
    point_ids?: number[];
    device_ids?: number[];
    format?: 'json' | 'csv' | 'xml';
    include_metadata?: boolean;
  }): Promise<ApiResponse<ExportResult>> {
    console.log('📤 현재값 내보내기:', params);
    return this.exportData({
      export_type: 'current',
      ...params
    });
  }

  /**
   * 이력 데이터 내보내기
   */
  static async exportHistoricalData(params: {
    point_ids: number[];
    start_time: string;
    end_time: string;
    format?: 'json' | 'csv' | 'xml';
    include_metadata?: boolean;
  }): Promise<ApiResponse<ExportResult>> {
    console.log('📤 이력 데이터 내보내기:', params);
    return this.exportData({
      export_type: 'historical',
      ...params
    });
  }

  /**
   * 설정 내보내기
   */
  static async exportConfiguration(params: {
    point_ids?: number[];
    device_ids?: number[];
    format?: 'json' | 'csv' | 'xml';
  }): Promise<ApiResponse<ExportResult>> {
    console.log('📤 설정 내보내기:', params);
    return this.exportData({
      export_type: 'configuration',
      ...params
    });
  }

  // ========================================================================
  // 🔧 유틸리티 메서드들
  // ========================================================================

  /**
   * 데이터포인트 필터링 (클라이언트 사이드)
   */
  static filterDataPoints(dataPoints: DataPoint[], filters: {
    search?: string;
    data_type?: string;
    enabled_only?: boolean;
    device_id?: number;
  }): DataPoint[] {
    return dataPoints.filter(point => {
      if (filters.search) {
        const search = filters.search.toLowerCase();
        const matchesSearch =
          point.name.toLowerCase().includes(search) ||
          point.description?.toLowerCase().includes(search) ||
          point.device_name?.toLowerCase().includes(search);
        if (!matchesSearch) return false;
      }

      if (filters.data_type && point.data_type !== filters.data_type) {
        return false;
      }

      if (filters.enabled_only && !point.is_enabled) {
        return false;
      }

      if (filters.device_id && point.device_id !== filters.device_id) {
        return false;
      }

      return true;
    });
  }

  /**
   * 데이터포인트 정렬
   */
  static sortDataPoints(dataPoints: DataPoint[], sortBy: string = 'name', sortOrder: 'ASC' | 'DESC' = 'ASC'): DataPoint[] {
    return [...dataPoints].sort((a, b) => {
      let aValue: any, bValue: any;

      switch (sortBy) {
        case 'name':
          aValue = a.name;
          bValue = b.name;
          break;
        case 'device_name':
          aValue = a.device_name || '';
          bValue = b.device_name || '';
          break;
        case 'data_type':
          aValue = a.data_type;
          bValue = b.data_type;
          break;
        case 'created_at':
          aValue = new Date(a.created_at);
          bValue = new Date(b.created_at);
          break;
        default:
          aValue = a.name;
          bValue = b.name;
      }

      if (aValue < bValue) {
        return sortOrder === 'ASC' ? -1 : 1;
      }
      if (aValue > bValue) {
        return sortOrder === 'ASC' ? 1 : -1;
      }
      return 0;
    });
  }
}

// ============================================================================
// 📤 Export
// ============================================================================

export default DataApiService;