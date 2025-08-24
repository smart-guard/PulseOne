// ============================================================================
// frontend/src/api/services/dataApi.ts
// 데이터 익스플로러 API 서비스 - dataPointsApi 완전 통합 버전
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
  source?: string;
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
// 📊 데이터 익스플로러 API 서비스 클래스 (완전 통합 버전)
// ============================================================================

export class DataApiService {
  private static httpClient = new HttpClient();

  // ========================================================================
  // 🔍 데이터포인트 검색 및 조회 (기존 메소드들)
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
  // 🆕 InputVariableSourceSelector용 메소드들 (dataPointsApi 통합)
  // ========================================================================

  /**
   * 데이터포인트 목록 조회 (InputVariableSourceSelector 전용)
   * 기존 dataPointsApi.getDataPoints()를 완전 대체
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
    console.log('🔄 데이터포인트 목록 조회 (통합 API):', filters);

    try {
      // ✅ 직접 fetch로 호출 (URL 중복 문제 해결)
      const params = new URLSearchParams();
      
      // 기본값 설정
      params.append('limit', (filters?.limit || 1000).toString());
      params.append('page', (filters?.page || 1).toString());
      params.append('sort_by', filters?.sort_by || 'name');
      params.append('sort_order', filters?.sort_order || 'ASC');
      
      // 선택적 필터들
      if (filters?.search) params.append('search', filters.search);
      if (filters?.device_id) params.append('device_id', filters.device_id.toString());
      if (filters?.site_id) params.append('site_id', filters.site_id.toString());
      if (filters?.data_type) params.append('data_type', filters.data_type);
      if (filters?.enabled_only !== undefined) params.append('enabled_only', filters.enabled_only.toString());
      if (filters?.include_current_value !== undefined) params.append('include_current_value', filters.include_current_value.toString());

      // ✅ URL 올바르게 구성
      const url = `/api/data/points?${params.toString()}`;
      console.log('🔗 API 요청 URL:', url);

      const response = await fetch(url);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const result = await response.json();
      console.log('📦 API 응답:', result);
      
      // 백엔드 응답 처리 (타입 변환 지원)
      if (result.success && result.data) {
        const points = result.data.points || [];
        return {
          points: points,
          totalCount: result.data.pagination?.total || points.length,
          pagination: {
            page: result.data.pagination?.page || 1,
            limit: result.data.pagination?.limit || 1000,
            hasNext: result.data.pagination?.has_next || false,
            hasPrev: result.data.pagination?.has_prev || false,
            total: result.data.pagination?.total || points.length,
            totalPages: result.data.pagination?.total_pages || 1
          },
          transformationInfo: result.data.data_transformation || {
            types_converted: true,
            original_count: points.length,
            filtered_count: points.length
          }
        };
      } else {
        throw new Error(result.message || 'API 응답 처리 실패');
      }

    } catch (error) {
      console.warn('⚠️ API 호출 실패, 목 데이터 사용:', error);
      return this.getMockDataPoints(filters);
    }
  }

  /**
   * 디바이스 목록 조회 (InputVariableSourceSelector 전용)
   */
  static async getDevices(): Promise<Array<{
    id: number;
    name: string;
    protocol_type: string;
    connection_status: string;
  }>> {
    try {
      console.log('🔄 디바이스 목록 조회');
      
      const response = await fetch('/api/devices?limit=100&enabled_only=true');
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const result = await response.json();
      console.log('📦 디바이스 API 응답:', result);
      
      if (result.success && result.data?.items) {
        return result.data.items.map((d: any) => ({
          id: d.id,
          name: d.name,
          protocol_type: d.protocol_type || 'unknown',
          connection_status: d.connection_status || 'unknown'
        }));
      } else if (result.success && Array.isArray(result.data)) {
        return result.data.map((d: any) => ({
          id: d.id,
          name: d.name,
          protocol_type: d.protocol_type || 'unknown',
          connection_status: d.connection_status || 'unknown'
        }));
      } else {
        throw new Error(result.message || '디바이스 API 응답 구조 오류');
      }
    } catch (error) {
      console.warn('⚠️ 디바이스 API 호출 실패, 목 데이터 사용:', error);
      return [
        { id: 1, name: 'PLC-001 (보일러)', protocol_type: 'modbus', connection_status: 'connected' },
        { id: 2, name: 'WEATHER-001 (기상)', protocol_type: 'mqtt', connection_status: 'connected' },
        { id: 3, name: 'HVAC-001 (공조)', protocol_type: 'bacnet', connection_status: 'connected' }
      ];
    }
  }

  // ========================================================================
  // ⚡ 실시간 현재값 조회 (기존 메소드들)
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

  // ========================================================================
  // 📊 이력 데이터 조회 (기존 메소드들)
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
  // 🔍 고급 검색 및 쿼리 (기존 메소드들)
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
  // 📊 데이터 통계 및 분석 (기존 메소드들)
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
  // 📤 데이터 내보내기 (기존 메소드들)
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
  // 🔧 유틸리티 메서드들 (기존 메소드들)
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

  // ========================================================================
  // 🎭 목 데이터 (API 실패 시)
  // ========================================================================

  /**
   * 목 데이터포인트 (InputVariableSourceSelector용)
   */
  private static getMockDataPoints(filters?: any): {
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
    transformationInfo: {
      types_converted: boolean;
      original_count: number;
      filtered_count: number;
    };
  } {
    console.log('🎭 목 데이터포인트 사용 (타입 변환 적용됨)');

    // ✅ 웹 표준 타입으로 변환된 목 데이터
    const mockData: DataPoint[] = [
      {
        id: 101,
        device_id: 1,
        device_name: 'PLC-001 (보일러)',
        name: 'Temperature_Sensor_01',
        description: '보일러 온도 센서',
        data_type: 'number', // ✅ 변환됨
        original_data_type: 'FLOAT32', // 원본 타입 보존
        current_value: 85.3,
        unit: '°C',
        address: '40001',
        is_enabled: true,
        min_value: 0,
        max_value: 200,
        created_at: '2024-08-20T09:00:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      },
      {
        id: 102,
        device_id: 1,
        device_name: 'PLC-001 (보일러)',
        name: 'Pressure_Sensor_01',
        description: '보일러 압력 센서',
        data_type: 'number', // ✅ 변환됨
        original_data_type: 'UINT32', // 원본 타입 보존
        current_value: 2.4,
        unit: 'bar',
        address: '40002',
        is_enabled: true,
        min_value: 0,
        max_value: 10,
        created_at: '2024-08-20T09:05:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      },
      {
        id: 103,
        device_id: 2,
        device_name: 'WEATHER-001 (기상)',
        name: 'External_Temperature',
        description: '외부 온도 센서',
        data_type: 'number', // ✅ 변환됨
        original_data_type: 'FLOAT64', // 원본 타입 보존
        current_value: 24.5,
        unit: '°C',
        address: 'temp',
        is_enabled: true,
        min_value: -20,
        max_value: 50,
        created_at: '2024-08-20T09:10:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      },
      {
        id: 104,
        device_id: 2,
        device_name: 'WEATHER-001 (기상)',
        name: 'Humidity_Sensor',
        description: '습도 센서',
        data_type: 'number', // ✅ 변환됨
        original_data_type: 'UINT16', // 원본 타입 보존
        current_value: 65.2,
        unit: '%',
        address: 'humidity',
        is_enabled: true,
        min_value: 0,
        max_value: 100,
        created_at: '2024-08-20T09:15:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      },
      {
        id: 105,
        device_id: 3,
        device_name: 'HVAC-001 (공조)',
        name: 'Fan_Status',
        description: '환풍기 동작 상태',
        data_type: 'boolean', // ✅ 변환됨
        original_data_type: 'BOOL', // 원본 타입 보존
        current_value: true,
        address: 'coil_001',
        is_enabled: true,
        created_at: '2024-08-20T09:20:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      },
      {
        id: 106,
        device_id: 3,
        device_name: 'HVAC-001 (공조)',
        name: 'System_Mode',
        description: '시스템 동작 모드',
        data_type: 'string', // ✅ 변환됨
        original_data_type: 'STRING', // 원본 타입 보존
        current_value: '냉방',
        address: 'mode',
        is_enabled: true,
        created_at: '2024-08-20T09:25:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      },
      {
        id: 107,
        device_id: 1,
        device_name: 'PLC-001 (보일러)',
        name: 'Flow_Rate',
        description: '유량 센서',
        data_type: 'number', // ✅ 변환됨
        original_data_type: 'DECIMAL', // 원본 타입 보존
        current_value: 15.7,
        unit: 'L/min',
        address: '40003',
        is_enabled: true,
        min_value: 0,
        max_value: 100,
        created_at: '2024-08-20T09:30:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      },
      {
        id: 108,
        device_id: 2,
        device_name: 'WEATHER-001 (기상)',
        name: 'Wind_Speed',
        description: '풍속 센서',
        data_type: 'number', // ✅ 변환됨
        original_data_type: 'REAL', // 원본 타입 보존
        current_value: 3.2,
        unit: 'm/s',
        address: 'wind',
        is_enabled: true,
        min_value: 0,
        max_value: 50,
        created_at: '2024-08-20T09:35:00Z',
        updated_at: '2024-08-23T10:30:00Z'
      }
    ];

    // 필터 적용
    let filteredData = mockData;
    
    if (filters?.search) {
      const search = filters.search.toLowerCase();
      filteredData = filteredData.filter((dp: any) =>
        dp.name.toLowerCase().includes(search) ||
        dp.description.toLowerCase().includes(search) ||
        dp.device_name.toLowerCase().includes(search)
      );
    }

    if (filters?.device_id) {
      filteredData = filteredData.filter((dp: any) => dp.device_id === filters.device_id);
    }

    if (filters?.data_type) {
      // ✅ 이제 웹 표준 타입으로 정확히 필터링 가능
      filteredData = filteredData.filter((dp: any) => dp.data_type === filters.data_type);
    }

    if (filters?.enabled_only) {
      filteredData = filteredData.filter((dp: any) => dp.is_enabled);
    }

    return {
      points: filteredData,
      totalCount: filteredData.length,
      pagination: {
        page: 1,
        limit: 1000,
        hasNext: false,
        hasPrev: false,
        total: filteredData.length,
        totalPages: 1
      },
      transformationInfo: {
        types_converted: true,
        original_count: mockData.length,
        filtered_count: filteredData.length
      }
    };
  }
}

// ============================================================================
// 📤 Export - 기본 및 명명된 export 모두 제공
// ============================================================================

export default DataApiService;