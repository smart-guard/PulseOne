// ============================================================================
// frontend/src/api/services/dataPointsApi.ts
// 데이터포인트 API 서비스 - 백엔드 /api/data/points 연동
// ============================================================================

export interface DataPoint {
  id: number;
  device_id: number;
  device_name: string;
  name: string;
  description: string;
  data_type: 'number' | 'boolean' | 'string';
  current_value: any;
  unit?: string;
  address: string;
  is_enabled: boolean;
  min_value?: number;
  max_value?: number;
  created_at: string;
  updated_at: string;
}

export interface Device {
  id: number;
  name: string;
  protocol_type: string;
  connection_status: string;
}

export interface DataPointsResponse {
  success: boolean;
  data: {
    points: DataPoint[];
    total_items: number;
    pagination: {
      page: number;
      limit: number;
      has_next: boolean;
      has_prev: boolean;
    };
  };
  message: string;
  timestamp: string;
}

export interface DataPointFilters {
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
}

class DataPointsApiService {
  private readonly baseUrl = '/api/data';

  /**
   * 데이터포인트 목록 조회
   */
  async getDataPoints(filters?: DataPointFilters): Promise<{
    points: DataPoint[];
    totalCount: number;
    pagination: {
      page: number;
      limit: number;
      hasNext: boolean;
      hasPrev: boolean;
    };
  }> {
    try {
      console.log('데이터포인트 목록 조회:', filters);

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

      const response = await fetch(`${this.baseUrl}/points?${params.toString()}`);
      
      if (response.ok) {
        const result: DataPointsResponse = await response.json();
        if (result.success && result.data?.points) {
          return {
            points: result.data.points,
            totalCount: result.data.total_items || result.data.points.length,
            pagination: {
              page: result.data.pagination?.page || 1,
              limit: result.data.pagination?.limit || 1000,
              hasNext: result.data.pagination?.has_next || false,
              hasPrev: result.data.pagination?.has_prev || false
            }
          };
        } else {
          throw new Error(result.message || 'API 응답 오류');
        }
      } else {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
    } catch (error) {
      console.error('데이터포인트 조회 실패:', error);
      
      // 백엔드 API 실패 시 목 데이터 반환
      return this.getMockDataPoints(filters);
    }
  }

  /**
   * 디바이스 목록 조회 (데이터포인트 선택용)
   */
  async getDevices(): Promise<Device[]> {
    try {
      console.log('디바이스 목록 조회');

      const response = await fetch('/api/devices?limit=100&enabled_only=true');
      
      if (response.ok) {
        const result = await response.json();
        if (result.success && result.data?.devices) {
          return result.data.devices.map((d: any) => ({
            id: d.id,
            name: d.name,
            protocol_type: d.protocol_type || 'unknown',
            connection_status: d.connection_status || 'unknown'
          }));
        }
      }

      // 백엔드 실패 시 목 데이터
      return this.getMockDevices();
    } catch (error) {
      console.error('디바이스 조회 실패:', error);
      return this.getMockDevices();
    }
  }

  // ========================================================================
  // 목 데이터 (백엔드 API가 없을 때)
  // ========================================================================

  private getMockDataPoints(filters?: DataPointFilters): {
    points: DataPoint[];
    totalCount: number;
    pagination: { page: number; limit: number; hasNext: boolean; hasPrev: boolean };
  } {
    console.log('목 데이터포인트 사용');

    const mockData: DataPoint[] = [
      {
        id: 101,
        device_id: 1,
        device_name: 'PLC-001 (보일러)',
        name: 'Temperature_Sensor_01',
        description: '보일러 온도 센서',
        data_type: 'number',
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
        data_type: 'number',
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
        data_type: 'number',
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
        data_type: 'number',
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
        data_type: 'boolean',
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
        data_type: 'string',
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
        data_type: 'number',
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
        data_type: 'number',
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
      filteredData = filteredData.filter(dp =>
        dp.name.toLowerCase().includes(search) ||
        dp.description.toLowerCase().includes(search) ||
        dp.device_name.toLowerCase().includes(search)
      );
    }

    if (filters?.device_id) {
      filteredData = filteredData.filter(dp => dp.device_id === filters.device_id);
    }

    if (filters?.data_type) {
      filteredData = filteredData.filter(dp => dp.data_type === filters.data_type);
    }

    if (filters?.enabled_only) {
      filteredData = filteredData.filter(dp => dp.is_enabled);
    }

    return {
      points: filteredData,
      totalCount: filteredData.length,
      pagination: {
        page: 1,
        limit: 1000,
        hasNext: false,
        hasPrev: false
      }
    };
  }

  private getMockDevices(): Device[] {
    return [
      { id: 1, name: 'PLC-001 (보일러)', protocol_type: 'modbus', connection_status: 'connected' },
      { id: 2, name: 'WEATHER-001 (기상)', protocol_type: 'mqtt', connection_status: 'connected' },
      { id: 3, name: 'HVAC-001 (공조)', protocol_type: 'bacnet', connection_status: 'connected' }
    ];
  }
}

// 싱글톤 인스턴스 생성 및 export
export const dataPointsApi = new DataPointsApiService();