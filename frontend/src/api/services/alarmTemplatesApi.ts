// frontend/src/api/services/alarmTemplatesApi.ts
// 알람 템플릿 관리 API 서비스 - 완전히 수정된 버전

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
  async getTemplates(params?: TemplateListParams): Promise<any> {
    const queryParams = new URLSearchParams();
    
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });
    }
    
    const url = `${BASE_URL}/alarms/templates${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
    
    try {
      const response = await fetch(url, {
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!response.ok) {
        throw new Error(`템플릿 조회 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result;
    } catch (error) {
      console.error('알람 템플릿 조회 실패:', error);
      throw error;
    }
  }

  // 특정 알람 템플릿 조회
  async getTemplate(id: number): Promise<AlarmTemplate | null> {
    try {
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
        throw new Error(`템플릿 조회 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success ? result.data : result;
    } catch (error) {
      console.error('템플릿 조회 실패:', error);
      throw error;
    }
  }

  // 카테고리별 템플릿 조회
  async getTemplatesByCategory(category: string): Promise<AlarmTemplate[]> {
    return this.getTemplates({ category });
  }

  // 데이터 타입별 호환 템플릿 조회
  async getCompatibleTemplates(dataType: string): Promise<AlarmTemplate[]> {
    try {
      const response = await fetch(`${BASE_URL}/alarms/templates/data-type/${dataType}`, {
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!response.ok) {
        throw new Error(`호환 템플릿 조회 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success ? result.data : result;
    } catch (error) {
      console.error('호환 템플릿 조회 실패:', error);
      return [];
    }
  }

  // 템플릿 적용
  async applyTemplate(templateId: number, request: ApplyTemplateRequest): Promise<ApplyTemplateResponse> {
    try {
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
        throw new Error(error.message || `템플릿 적용 실패: ${response.status} ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      console.error('템플릿 적용 실패:', error);
      throw error;
    }
  }

  // 템플릿으로 생성된 규칙 조회
  async getAppliedRules(templateId: number): Promise<CreatedAlarmRule[]> {
    try {
      const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}/applied-rules`, {
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!response.ok) {
        throw new Error(`적용된 규칙 조회 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success ? result.data : result;
    } catch (error) {
      console.error('적용된 규칙 조회 실패:', error);
      return [];
    }
  }

  // 데이터포인트 목록 조회 - 백엔드 API와 100% 일치하도록 수정
  async getDataPoints(filters?: {
    site_name?: string;
    device_name?: string;
    data_type?: string;
    search?: string;
  }): Promise<DataPoint[]> {
    try {
      console.log('🔍 데이터포인트 조회 시작:', filters);
      
      // 모든 디바이스에서 데이터포인트 수집하는 방법 사용 (안정적)
      return await this.getDataPointsFromDevices(filters);

    } catch (error) {
      console.error('데이터포인트 조회 실패:', error);
      return [];
    }
  }

  // 디바이스 기반 데이터포인트 수집 (실제 백엔드 API 사용)
  private async getDataPointsFromDevices(filters?: {
    site_name?: string;
    device_name?: string;
    data_type?: string;
    search?: string;
  }): Promise<DataPoint[]> {
    try {
      console.log('📱 디바이스 기반 데이터포인트 수집 시작');
      
      // 1. 디바이스 목록 조회 - 실제 백엔드 API 사용
      const devicesResponse = await fetch(`${BASE_URL}/devices?limit=100`, {
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!devicesResponse.ok) {
        throw new Error(`디바이스 목록 조회 실패: ${devicesResponse.status} ${devicesResponse.statusText}`);
      }

      const devicesResult = await devicesResponse.json();
      let devices = [];
      
      if (devicesResult.success && devicesResult.data) {
        devices = devicesResult.data.items || [];
      } else if (Array.isArray(devicesResult)) {
        devices = devicesResult;
      }

      console.log(`📱 찾은 디바이스 수: ${devices.length}`);

      // 2. 각 디바이스의 데이터포인트 조회
      const allDataPoints: DataPoint[] = [];
      
      for (const device of devices) {
        try {
          console.log(`📊 디바이스 ${device.name} (ID: ${device.id}) 데이터포인트 조회`);
          
          const response = await fetch(`${BASE_URL}/devices/${device.id}/data-points?limit=100`, {
            headers: {
              'Content-Type': 'application/json',
              'x-tenant-id': '1',
            },
          });

          if (response.ok) {
            const result = await response.json();
            let deviceDataPoints = [];
            
            if (result.success && result.data) {
              deviceDataPoints = result.data.items || [];
            } else if (Array.isArray(result)) {
              deviceDataPoints = result;
            }
            
            console.log(`📊 디바이스 ${device.name}에서 ${deviceDataPoints.length}개 데이터포인트 발견`);
            
            // 데이터포인트에 디바이스 정보 추가
            deviceDataPoints.forEach((dp: any) => {
              allDataPoints.push({
                id: dp.id,
                name: dp.name || `점_${dp.id}`,
                device_name: device.name || '알 수 없는 장치',
                site_name: device.site_name || '기본 사이트',
                data_type: dp.data_type || 'unknown',
                unit: dp.unit || '',
                current_value: dp.current_value || 0,
                last_updated: dp.last_updated || new Date().toISOString(),
                supports_analog: dp.data_type !== 'boolean' && dp.data_type !== 'digital',
                supports_digital: dp.data_type === 'boolean' || dp.data_type === 'digital'
              });
            });
          } else {
            console.warn(`⚠️ 디바이스 ${device.id} 데이터포인트 조회 실패: ${response.status}`);
          }
        } catch (error) {
          console.warn(`⚠️ 디바이스 ${device.id} 데이터포인트 조회 중 오류:`, error);
        }
      }

      console.log(`📊 총 수집된 데이터포인트 수: ${allDataPoints.length}`);

      // 3. 필터 적용
      let filteredDataPoints = allDataPoints;

      if (filters?.site_name && filters.site_name !== 'all') {
        filteredDataPoints = filteredDataPoints.filter(dp => dp.site_name === filters.site_name);
        console.log(`🔍 사이트 필터 적용 후: ${filteredDataPoints.length}개`);
      }

      if (filters?.device_name && filters.device_name !== 'all') {
        filteredDataPoints = filteredDataPoints.filter(dp => dp.device_name === filters.device_name);
        console.log(`🔍 디바이스 필터 적용 후: ${filteredDataPoints.length}개`);
      }

      if (filters?.data_type && filters.data_type !== 'all') {
        filteredDataPoints = filteredDataPoints.filter(dp => dp.data_type === filters.data_type);
        console.log(`🔍 데이터 타입 필터 적용 후: ${filteredDataPoints.length}개`);
      }

      if (filters?.search) {
        const searchTerm = filters.search.toLowerCase();
        filteredDataPoints = filteredDataPoints.filter(dp => 
          dp.name.toLowerCase().includes(searchTerm) ||
          dp.device_name.toLowerCase().includes(searchTerm) ||
          dp.site_name.toLowerCase().includes(searchTerm)
        );
        console.log(`🔍 검색 필터 적용 후: ${filteredDataPoints.length}개`);
      }

      console.log(`✅ 최종 데이터포인트 수: ${filteredDataPoints.length}`);
      return filteredDataPoints;

    } catch (error) {
      console.error('❌ 디바이스 기반 데이터포인트 조회 실패:', error);
      return [];
    }
  }

  // 생성된 모든 알람 규칙 조회
  async getCreatedRules(params?: {
    page?: number;
    limit?: number;
    template_id?: number;
    search?: string;
    enabled?: boolean;
  }): Promise<any> {
    const queryParams = new URLSearchParams();
    
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });
    }

    const url = `${BASE_URL}/alarms/rules${queryParams.toString() ? '?' + queryParams.toString() : ''}`;
    
    try {
      const response = await fetch(url, {
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!response.ok) {
        throw new Error(`생성된 규칙 조회 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result;
    } catch (error) {
      console.error('생성된 규칙 조회 실패:', error);
      return { success: true, data: { items: [] } };
    }
  }

  // 알람 규칙 활성화/비활성화
  async toggleRule(ruleId: number, enabled: boolean): Promise<boolean> {
    try {
      const response = await fetch(`${BASE_URL}/alarms/rules/${ruleId}`, {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
        body: JSON.stringify({ is_enabled: enabled }),
      });

      if (!response.ok) {
        throw new Error(`규칙 상태 변경 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success;
    } catch (error) {
      console.error('규칙 상태 변경 실패:', error);
      return false;
    }
  }

  // 알람 규칙 삭제
  async deleteRule(ruleId: number): Promise<boolean> {
    try {
      const response = await fetch(`${BASE_URL}/alarms/rules/${ruleId}`, {
        method: 'DELETE',
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!response.ok) {
        throw new Error(`규칙 삭제 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success;
    } catch (error) {
      console.error('규칙 삭제 실패:', error);
      return false;
    }
  }

  // 템플릿 사용 통계 조회
  async getTemplateStats(templateId: number): Promise<{
    usage_count: number;
    active_rules: number;
    last_used: string | null;
  }> {
    try {
      const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}/stats`, {
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!response.ok) {
        throw new Error(`템플릿 통계 조회 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success ? result.data : {
        usage_count: 0,
        active_rules: 0,
        last_used: null
      };
    } catch (error) {
      console.error('템플릿 통계 조회 실패:', error);
      return {
        usage_count: 0,
        active_rules: 0,
        last_used: null
      };
    }
  }

  // 새 템플릿 생성
  async createTemplate(templateData: Omit<AlarmTemplate, 'id' | 'usage_count' | 'created_at' | 'updated_at'>): Promise<AlarmTemplate> {
    try {
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
        throw new Error(error.message || `템플릿 생성 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success ? result.data : result;
    } catch (error) {
      console.error('템플릿 생성 실패:', error);
      throw error;
    }
  }

  // 템플릿 수정
  async updateTemplate(templateId: number, updates: Partial<AlarmTemplate>): Promise<AlarmTemplate> {
    try {
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
        throw new Error(error.message || `템플릿 수정 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success ? result.data : result;
    } catch (error) {
      console.error('템플릿 수정 실패:', error);
      throw error;
    }
  }

  // 템플릿 삭제
  async deleteTemplate(templateId: number): Promise<boolean> {
    try {
      const response = await fetch(`${BASE_URL}/alarms/templates/${templateId}`, {
        method: 'DELETE',
        headers: {
          'Content-Type': 'application/json',
          'x-tenant-id': '1',
        },
      });

      if (!response.ok) {
        throw new Error(`템플릿 삭제 실패: ${response.status} ${response.statusText}`);
      }

      const result = await response.json();
      return result.success;
    } catch (error) {
      console.error('템플릿 삭제 실패:', error);
      return false;
    }
  }
}

export const alarmTemplatesApi = new AlarmTemplatesApi();
export default alarmTemplatesApi;