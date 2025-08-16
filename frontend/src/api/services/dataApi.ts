// ============================================================================
// frontend/src/api/services/dataApi.ts
// 데이터 익스플로러 API 서비스 - 새로운 Backend API 완전 호환
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
  unit?: string;
  is_enabled: boolean;
  description?: string;
  min_value?: number;
  max_value?: number;
  scaling_factor?: number;
  polling_interval?: number;
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
  source?: 'auto' | 'redis' | 'database';
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
  query_type: 'data_points_search' | 'current_values_aggregate' | 'historical_analysis' | 'device_summary';
  filters?: any;
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
// 🔧 HTTP 클라이언트 클래스 (재사용)
// ============================================================================

class HttpClient {
  private baseUrl: string;

  constructor(baseUrl: string = API_CONFIG.BASE_URL) {
    this.baseUrl = baseUrl;
  }

  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    const url = `${this.baseUrl}${endpoint}`;
    
    console.log('🌐 Data API Request:', {
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
      
      console.log('📡 Data API Response:', {
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
      console.error('❌ Data API Request failed:', {
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
    include_metadata?: boolean;
    include_trends?: boolean;
  }): Promise<ApiResponse<{
    device_id: number;
    device_name: string;
    device_status: string;
    connection_status: string;
    last_communication: string;
    current_values: CurrentValue[];
    total_points: number;
    summary: {
      good_quality: number;
      bad_quality: number;
      uncertain_quality: number;
      last_update: number;
    };
  }>> {
    console.log('⚡ 디바이스 현재값 조회:', { deviceId, params });
    return this.httpClient.get<any>(`/api/data/device/${deviceId}/current-values`, params);
  }

  // ========================================================================
  // 📈 이력 데이터 조회
  // ========================================================================

  /**
   * 이력 데이터 조회 (InfluxDB 기반)
   */
  static async getHistoricalData(params: HistoricalDataParams): Promise<ApiResponse<{
    data_points: DataPoint[];
    historical_data: HistoricalDataPoint[];
    query_info: {
      start_time: string;
      end_time: string;
      interval: string;
      aggregation: string;
      total_points: number;
      simulated?: boolean;
    };
  }>> {
    console.log('📈 이력 데이터 조회:', params);
    return this.httpClient.get<any>(ENDPOINTS.DATA_HISTORICAL, params);
  }

  // ========================================================================
  // 🔍 고급 쿼리 및 분석
  // ========================================================================

  /**
   * 고급 데이터 쿼리 실행
   */
  static async executeAdvancedQuery(params: AdvancedQueryParams): Promise<ApiResponse<{
    query_type: string;
    total_results?: number;
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
    let filtered = [...dataPoints];

    if (filters.search) {
      const term = filters.search.toLowerCase();
      filtered = filtered.filter(dp => 
        dp.name.toLowerCase().includes(term) ||
        dp.description?.toLowerCase().includes(term) ||
        dp.device_name?.toLowerCase().includes(term)
      );
    }

    if (filters.data_type && filters.data_type !== 'all') {
      filtered = filtered.filter(dp => dp.data_type === filters.data_type);
    }

    if (filters.enabled_only) {
      filtered = filtered.filter(dp => dp.is_enabled);
    }

    if (filters.device_id) {
      filtered = filtered.filter(dp => dp.device_id === filters.device_id);
    }

    return filtered;
  }

  /**
   * 현재값 품질별 필터링
   */
  static filterCurrentValuesByQuality(values: CurrentValue[], quality?: string): CurrentValue[] {
    if (!quality || quality === 'all') return values;
    return values.filter(v => v.quality === quality);
  }

  /**
   * 데이터포인트 타입별 그룹화
   */
  static groupDataPointsByType(dataPoints: DataPoint[]): Record<string, DataPoint[]> {
    return dataPoints.reduce((groups, dp) => {
      const type = dp.data_type;
      if (!groups[type]) {
        groups[type] = [];
      }
      groups[type].push(dp);
      return groups;
    }, {} as Record<string, DataPoint[]>);
  }

  /**
   * 디바이스별 데이터포인트 그룹화
   */
  static groupDataPointsByDevice(dataPoints: DataPoint[]): Record<number, DataPoint[]> {
    return dataPoints.reduce((groups, dp) => {
      const deviceId = dp.device_id;
      if (!groups[deviceId]) {
        groups[deviceId] = [];
      }
      groups[deviceId].push(dp);
      return groups;
    }, {} as Record<number, DataPoint[]>);
  }

  /**
   * 현재값 통계 계산
   */
  static calculateValueStatistics(values: CurrentValue[]): {
    total: number;
    good_quality: number;
    bad_quality: number;
    uncertain_quality: number;
    by_data_type: Record<string, number>;
    latest_update: string | null;
  } {
    const stats = {
      total: values.length,
      good_quality: values.filter(v => v.quality === 'good').length,
      bad_quality: values.filter(v => v.quality === 'bad').length,
      uncertain_quality: values.filter(v => v.quality === 'uncertain').length,
      by_data_type: {} as Record<string, number>,
      latest_update: null as string | null
    };

    // 데이터 타입별 통계
    values.forEach(v => {
      const type = v.data_type;
      stats.by_data_type[type] = (stats.by_data_type[type] || 0) + 1;
    });

    // 최신 업데이트 시간
    if (values.length > 0) {
      const timestamps = values.map(v => new Date(v.timestamp).getTime());
      const latestTimestamp = Math.max(...timestamps);
      stats.latest_update = new Date(latestTimestamp).toISOString();
    }

    return stats;
  }

  /**
   * 이력 데이터 시간 범위 검증
   */
  static validateTimeRange(startTime: string, endTime: string): {
    valid: boolean;
    error?: string;
    duration_hours?: number;
  } {
    try {
      const start = new Date(startTime);
      const end = new Date(endTime);

      if (isNaN(start.getTime()) || isNaN(end.getTime())) {
        return { valid: false, error: 'Invalid date format' };
      }

      if (start >= end) {
        return { valid: false, error: 'Start time must be before end time' };
      }

      const durationMs = end.getTime() - start.getTime();
      const durationHours = durationMs / (1000 * 60 * 60);

      // 최대 7일 제한
      if (durationHours > 7 * 24) {
        return { valid: false, error: 'Time range cannot exceed 7 days' };
      }

      return { valid: true, duration_hours: durationHours };
    } catch (error) {
      return { valid: false, error: 'Date parsing error' };
    }
  }

  /**
   * CSV 데이터 변환
   */
  static convertToCSV(data: any[], headers?: string[]): string {
    if (!data.length) return '';

    const csvHeaders = headers || Object.keys(data[0]);
    const csvRows = data.map(row => 
      csvHeaders.map(header => {
        const value = row[header];
        if (typeof value === 'string' && value.includes(',')) {
          return `"${value}"`;
        }
        return value || '';
      }).join(',')
    );

    return [csvHeaders.join(','), ...csvRows].join('\n');
  }
}