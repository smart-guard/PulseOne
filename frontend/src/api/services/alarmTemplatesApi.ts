// frontend/src/api/services/alarmTemplatesApi.ts
// 알람 템플릿 관리 API 서비스

const BASE_URL = '/api';

export interface AlarmTemplate {
  id: number;
  name: string;
  description: string;
  category: string;
  template_type: 'simple' | 'advanced' | 'script';
  condition_type: 'threshold' | 'range' | 'pattern' | 'script';
  default_config: any;
  severity: string;
  message_template: string;
  usage_count: number;
  is_active: boolean;
  supports_hh_ll: boolean;
  supports_script: boolean;
  applicable_data_types: string[];
  created_at: string;
  updated_at: string;
}

export interface DataPoint {
  id: number;
  name: string;
  device_name: string;
  site_name: string;
  data_type: string;
  unit: string;
  current_value: number;
  last_updated: string;
  supports_analog: boolean;
  supports_digital: boolean;
}

export interface CreatedAlarmRule {
  id: number;
  name: string;
  template_name: string;
  data_point_name: string;
  device_name: string;
  site_name: string;
  severity: string;
  enabled: boolean;
  created_at: string;
  threshold_config: any;
}

export interface TemplateListParams {
  page?: number;
  limit?: number;
  category?: string;
  template_type?: string;
  search?: string;
  is_active?: boolean;
}

export interface ApplyTemplateRequest {
  data_point_ids: number[];
  custom_configs?: Record<number, any>;
  rule_group_name?: string;
}

export interface ApplyTemplateResponse {
  success: boolean;
  data: {
    rules_created: number;
    rule_group_id: string;
    created_rules: CreatedAlarmRule[];
  };
  message: string;
}

class AlarmTemplatesApi {
  
  // 알람 템플릿 목록 조회
  async getTemplates(params?: TemplateListParams): Promise<AlarmTemplate[]> {
    const queryParams = new URLSearchParams();
    
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });
    }
    
    const url = `${BASE_URL}/alarms/templates${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
    
    const response = await fetch(url, {
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1', // 기본 테넌트 ID
      },
    });

    if (!response.ok) {
      throw new Error(`템플릿 목록 조회 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data || [];
  }

  // 특정 알람 템플릿 조회
  async getTemplate(id: number): Promise<AlarmTemplate | null> {
    const response = await fetch(`${BASE_URL}/alarms/templates/${id}`, {
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
    });

    if (!response.ok) {
      if (response.status === 404) {
        return null;
      }
      throw new Error(`템플릿 조회 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data;
  }

  // 카테고리별 템플릿 조회
  async getTemplatesByCategory(category: string): Promise<AlarmTemplate[]> {
    return this.getTemplates({ category });
  }

  // 데이터 타입별 호환 템플릿 조회
  async getCompatibleTemplates(dataType: string): Promise<AlarmTemplate[]> {
    const response = await fetch(`${BASE_URL}/alarms/templates/compatible/${dataType}`, {
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
    });

    if (!response.ok) {
      throw new Error(`호환 템플릿 조회 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data || [];
  }

  // 템플릿 적용
  async applyTemplate(templateId: number, request: ApplyTemplateRequest): Promise<ApplyTemplateResponse> {
    const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}/apply`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
      body: JSON.stringify(request),
    });

    if (!response.ok) {
      const error = await response.json();
      throw new Error(error.message || `템플릿 적용 실패: ${response.statusText}`);
    }

    return await response.json();
  }

  // 템플릿으로 생성된 규칙 조회
  async getAppliedRules(templateId: number): Promise<CreatedAlarmRule[]> {
    const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}/applied-rules`, {
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
    });

    if (!response.ok) {
      throw new Error(`적용된 규칙 조회 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data || [];
  }

  // 데이터포인트 목록 조회 - 모든 디바이스의 데이터포인트를 수집
  async getDataPoints(filters?: {
    site_name?: string;
    device_name?: string;
    data_type?: string;
    search?: string;
  }): Promise<DataPoint[]> {
    try {
      // 1. 먼저 모든 디바이스 목록을 조회
      const devicesResponse = await fetch('/api/devices', {
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!devicesResponse.ok) {
        throw new Error(`디바이스 목록 조회 실패: ${devicesResponse.statusText}`);
      }

      const devicesResult = await devicesResponse.json();
      const devices = devicesResult.data?.items || [];

      // 2. 각 디바이스의 데이터포인트를 조회하여 합침
      const allDataPoints: DataPoint[] = [];
      
      for (const device of devices) {
        try {
          const response = await fetch(`/api/devices/${device.id}/data-points`, {
            headers: {
              'Content-Type': 'application/json',
              'x-tenant-id': '1',
            },
          });

          if (response.ok) {
            const result = await response.json();
            const deviceDataPoints = result.data?.items || [];
            
            // 데이터포인트에 디바이스 정보 추가
            deviceDataPoints.forEach((dp: any) => {
              allDataPoints.push({
                id: dp.id,
                name: dp.name,
                device_name: device.name,
                site_name: device.site_name || '기본 사이트',
                data_type: dp.data_type,
                unit: dp.unit || '',
                current_value: dp.current_value || 0,
                last_updated: dp.last_updated || new Date().toISOString(),
                supports_analog: dp.data_type !== 'boolean' && dp.data_type !== 'digital',
                supports_digital: dp.data_type === 'boolean' || dp.data_type === 'digital'
              });
            });
          }
        } catch (error) {
          console.warn(`디바이스 ${device.id} 데이터포인트 조회 실패:`, error);
        }
      }

      // 3. 필터 적용
      let filteredDataPoints = allDataPoints;

      if (filters?.site_name && filters.site_name !== 'all') {
        filteredDataPoints = filteredDataPoints.filter(dp => dp.site_name === filters.site_name);
      }

      if (filters?.device_name && filters.device_name !== 'all') {
        filteredDataPoints = filteredDataPoints.filter(dp => dp.device_name === filters.device_name);
      }

      if (filters?.data_type && filters.data_type !== 'all') {
        filteredDataPoints = filteredDataPoints.filter(dp => dp.data_type === filters.data_type);
      }

      if (filters?.search) {
        const searchTerm = filters.search.toLowerCase();
        filteredDataPoints = filteredDataPoints.filter(dp => 
          dp.name.toLowerCase().includes(searchTerm) ||
          dp.device_name.toLowerCase().includes(searchTerm) ||
          dp.site_name.toLowerCase().includes(searchTerm)
        );
      }

      return filteredDataPoints;

    } catch (error) {
      console.error('데이터포인트 조회 실패:', error);
      throw error;
    }
  }

  // 생성된 모든 알람 규칙 조회
  async getCreatedRules(params?: {
    page?: number;
    limit?: number;
    template_id?: number;
    search?: string;
    enabled?: boolean;
  }): Promise<CreatedAlarmRule[]> {
    const queryParams = new URLSearchParams();
    
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });
    }

    const url = `${BASE_URL}/alarms/rules${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
    
    const response = await fetch(url, {
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
    });

    if (!response.ok) {
      throw new Error(`생성된 규칙 조회 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data || [];
  }

  // 알람 규칙 활성화/비활성화
  async toggleRule(ruleId: number, enabled: boolean): Promise<boolean> {
    const response = await fetch(`${BASE_URL}/alarms/rules/${ruleId}/toggle`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
      body: JSON.stringify({ enabled }),
    });

    if (!response.ok) {
      throw new Error(`규칙 상태 변경 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.success;
  }

  // 알람 규칙 삭제
  async deleteRule(ruleId: number): Promise<boolean> {
    const response = await fetch(`${BASE_URL}/alarms/rules/${ruleId}`, {
      method: 'DELETE',
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
    });

    if (!response.ok) {
      throw new Error(`규칙 삭제 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.success;
  }

  // 템플릿 사용 통계 조회
  async getTemplateStats(templateId: number): Promise<{
    usage_count: number;
    active_rules: number;
    last_used: string | null;
  }> {
    const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}/stats`, {
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
    });

    if (!response.ok) {
      throw new Error(`템플릿 통계 조회 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data;
  }

  // 새 템플릿 생성
  async createTemplate(templateData: Omit<AlarmTemplate, 'id' | 'usage_count' | 'created_at' | 'updated_at'>): Promise<AlarmTemplate> {
    const response = await fetch(`${BASE_URL}/alarms/templates`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
      body: JSON.stringify(templateData),
    });

    if (!response.ok) {
      const error = await response.json();
      throw new Error(error.message || `템플릿 생성 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data;
  }

  // 템플릿 수정
  async updateTemplate(templateId: number, updates: Partial<AlarmTemplate>): Promise<AlarmTemplate> {
    const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
      body: JSON.stringify(updates),
    });

    if (!response.ok) {
      const error = await response.json();
      throw new Error(error.message || `템플릿 수정 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.data;
  }

  // 템플릿 삭제
  async deleteTemplate(templateId: number): Promise<boolean> {
    const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}`, {
      method: 'DELETE',
      headers: {
        'Content-Type': 'application/json',
        'x-tenant-id': '1',
      },
    });

    if (!response.ok) {
      throw new Error(`템플릿 삭제 실패: ${response.statusText}`);
    }

    const result = await response.json();
    return result.success;
  }
}

export const alarmTemplatesApi = new AlarmTemplatesApi();
export default alarmTemplatesApi;