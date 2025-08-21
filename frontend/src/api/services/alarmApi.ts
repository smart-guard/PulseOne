import { ENDPOINTS } from '../endpoints';

// 백엔드 스키마와 정확히 일치하도록 수정된 인터페이스
export interface AlarmRule {
  id: number;
  tenant_id: number;
  
  // 기본 정보
  name: string;
  description?: string;
  
  // 타겟 정보 (실제 스키마)
  target_type: string;          // 'data_point', 'device', 'virtual_point'
  target_id?: number;
  target_group?: string;
  
  // JOIN으로 가져오는 정보들
  device_name?: string;
  device_type?: string;
  manufacturer?: string;
  model?: string;
  site_name?: string;
  site_location?: string;
  site_description?: string;
  
  data_point_name?: string;
  data_point_description?: string;
  unit?: string;
  data_type?: string;
  
  virtual_point_name?: string;
  virtual_point_description?: string;
  calculation_formula?: string;
  
  target_display?: string;      // 백엔드에서 계산된 필드
  condition_display?: string;   // 백엔드에서 계산된 필드
  
  // 알람 타입 및 조건 (실제 스키마)
  alarm_type: string;           // 'analog', 'digital', 'script'
  severity: 'critical' | 'high' | 'medium' | 'low' | 'info';
  priority?: number;
  
  // 개별 임계값들 (실제 스키마)
  high_high_limit?: number;
  high_limit?: number;
  low_limit?: number;
  low_low_limit?: number;
  deadband?: number;
  rate_of_change?: number;
  
  // 디지털 알람 조건
  trigger_condition?: string;
  
  // 스크립트 관련
  condition_script?: string;
  message_script?: string;
  
  // 메시지 관련
  message_config?: any;
  message_template?: string;
  
  // 동작 설정들
  auto_acknowledge?: boolean;
  acknowledge_timeout_min?: number;
  auto_clear?: boolean;
  suppression_rules?: any;
  
  // 알림 설정들
  notification_enabled?: boolean;
  notification_delay_sec?: number;
  notification_repeat_interval_min?: number;
  notification_channels?: any;
  notification_recipients?: any;
  
  // 상태 및 제어
  is_enabled: boolean;
  is_latched?: boolean;
  
  // 템플릿 관련
  template_id?: number;
  rule_group?: string;
  created_by_template?: boolean;
  last_template_update?: string;
  
  // 고급 기능
  escalation_rules?: any;
  tags?: any[];
  
  // 메타데이터
  created_by?: number;
  created_at: string;
  updated_at: string;
}
export interface AlarmOccurrence {
  id: number;
  tenant_id: number;
  alarm_rule_id: number;
  
  // 규칙 정보
  rule_name?: string;
  rule_severity?: string;
  
  // 대상 정보 (백엔드 실제 컬럼명)
  device_id?: number;
  data_point_id?: number;
  virtual_point_id?: number;
  device_name?: string;
  data_point_name?: string;
  virtual_point_name?: string;
  site_location?: string;
  
  // 알람 상세 정보
  severity: string;
  message: string;
  trigger_value?: string;
  condition_details?: string;
  
  // 상태 정보 (백엔드 실제 컬럼명)
  state: 'active' | 'acknowledged' | 'cleared';
  occurrence_time: string;
  
  // 확인 정보 (백엔드 실제 컬럼명)
  acknowledgment_time?: string;
  acknowledged_by?: number;
  acknowledgment_note?: string;
  
  // 해제 정보 (백엔드 실제 컬럼명)
  clear_time?: string;
  cleared_by?: number;
  resolution_note?: string;
  
  // 기타
  escalation_level?: number;
  notification_sent?: boolean;
  
  // 메타데이터
  created_at: string;
  updated_at: string;
}

export interface AlarmTemplate {
  id: number;
  tenant_id: number;
  name: string;
  description?: string;
  category: string;
  condition_type: string;
  condition_template: string;
  default_config: any;              // JSON 객체
  severity: string;
  message_template?: string;
  applicable_data_types: string[];  // JSON 배열
  notification_enabled?: boolean;
  email_notification?: boolean;
  sms_notification?: boolean;
  auto_acknowledge?: boolean;
  auto_clear?: boolean;
  usage_count: number;
  is_active: boolean;
  is_system_template?: boolean;
  created_by?: number;
  created_at: string;
  updated_at: string;
}

export interface AlarmStatistics {
  // 발생 통계
  occurrences: {
    total_occurrences: number;
    active_alarms: number;
    unacknowledged_alarms: number;
    acknowledged_alarms: number;
    cleared_alarms: number;
  };
  
  // 규칙 통계
  rules: {
    total_rules: number;
    enabled_rules: number;
    condition_types: number;
    severity_levels: number;
    devices_with_rules: number;
  };
  
  // 대시보드 요약
  dashboard_summary: {
    total_active: number;
    total_rules: number;
    unacknowledged: number;
    enabled_rules: number;
  };
}

// 백엔드 CREATE 스키마와 정확히 일치
export interface AlarmRuleCreateData {
  name: string;
  description?: string;
  target_type: string;
  target_id?: number;
  target_group?: string;
  alarm_type: string;
  
  // 개별 임계값들
  high_high_limit?: number;
  high_limit?: number;
  low_limit?: number;
  low_low_limit?: number;
  deadband?: number;
  rate_of_change?: number;
  
  trigger_condition?: string;
  condition_script?: string;
  message_script?: string;
  message_config?: any;
  message_template?: string;
  
  severity: 'critical' | 'high' | 'medium' | 'low' | 'info';
  priority?: number;
  
  auto_acknowledge?: boolean;
  acknowledge_timeout_min?: number;
  auto_clear?: boolean;
  suppression_rules?: any;
  
  notification_enabled?: boolean;
  notification_delay_sec?: number;
  notification_repeat_interval_min?: number;
  notification_channels?: any;
  notification_recipients?: any;
  
  is_enabled?: boolean;
  is_latched?: boolean;
  
  template_id?: number;
  rule_group?: string;
  created_by_template?: boolean;
  escalation_rules?: any;
  tags?: any[];
}

export interface AlarmRuleUpdateData extends Partial<AlarmRuleCreateData> {}

// 백엔드 CREATE 스키마와 정확히 일치
export interface AlarmTemplateCreateData {
  name: string;
  description?: string;
  category?: string;
  condition_type: string;
  condition_template: string;
  default_config?: any;            // JSON 객체
  severity?: string;
  message_template?: string;
  applicable_data_types?: string[];
  notification_enabled?: boolean;
  email_notification?: boolean;
  sms_notification?: boolean;
  auto_acknowledge?: boolean;
  auto_clear?: boolean;
  is_active?: boolean;
  is_system_template?: boolean;
}

export interface AlarmListParams {
  page?: number;
  limit?: number;
  severity?: string;
  state?: string;
  device_id?: number;
  rule_id?: number;
  acknowledged?: boolean;
  search?: string;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
  date_from?: string;
  date_to?: string;
  enabled?: boolean;
  condition_type?: string;
}

export interface AcknowledgeAlarmRequest {
  comment?: string;
}

export interface ClearAlarmRequest {
  comment?: string;
  clearedValue?: string;
}

export interface ApiResponse<T> {
  success: boolean;
  data: T | null;
  message: string;
  error_code?: string;
  timestamp: string;
}

export interface PaginatedResponse<T> {
  items: T[];
  pagination: {
    page: number;
    limit: number;
    total: number;
    totalPages: number;
  };
}

class HttpClient {
  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    try {
      console.log(`API 요청: ${options.method || 'GET'} ${endpoint}`);
      
      const response = await fetch(endpoint, {
        headers: {
          'Content-Type': 'application/json',
          ...options.headers,
        },
        ...options,
      });

      const responseText = await response.text();
      
      if (!responseText || responseText.trim() === '') {
        console.warn('빈 응답 수신');
        return {
          success: false,
          data: null,
          message: 'Empty response from server',
          error_code: 'EMPTY_RESPONSE',
          timestamp: new Date().toISOString()
        };
      }

      let data;
      try {
        data = JSON.parse(responseText);
      } catch (parseError) {
        console.error('JSON 파싱 실패:', parseError);
        
        if (responseText.includes('<html>') || responseText.includes('<!DOCTYPE')) {
          return {
            success: false,
            data: null,
            message: `서버가 HTML을 반환했습니다 (Status: ${response.status})`,
            error_code: 'HTML_RESPONSE',
            timestamp: new Date().toISOString()
          };
        }
        
        return {
          success: false,
          data: null,
          message: `JSON 파싱 실패: ${parseError.message}`,
          error_code: 'JSON_PARSE_ERROR',
          timestamp: new Date().toISOString()
        };
      }
      
      if (!response.ok) {
        console.error(`HTTP 에러 ${response.status}:`, data);
        return {
          success: false,
          data: null,
          message: data.message || `HTTP ${response.status} ${response.statusText}`,
          error_code: data.error_code || response.statusText,
          timestamp: new Date().toISOString()
        };
      }

      console.log(`API 성공: ${endpoint}`);
      return data;
    } catch (error: any) {
      console.error('HTTP 요청 실패:', error);
      return {
        success: false,
        message: error.message || 'Network error',
        error_code: 'NETWORK_ERROR',
        data: null as any,
        timestamp: new Date().toISOString()
      };
    }
  }

  async get<T>(endpoint: string, params?: Record<string, any>): Promise<ApiResponse<T>> {
    const queryParams = new URLSearchParams();
    
    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
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

  async put<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'PUT',
      body: data ? JSON.stringify(data) : undefined
    });
  }

  async patch<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'PATCH',
      body: data ? JSON.stringify(data) : undefined
    });
  }

  async delete<T>(endpoint: string): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, { method: 'DELETE' });
  }
}

// 백엔드와 완전 호환되는 AlarmApiService
export class AlarmApiService {
  private static httpClient = new HttpClient();

  // 데이터 변환 없이 백엔드 응답 그대로 사용
  private static parseConditionConfig(configString: string | any): any {
    if (typeof configString === 'string') {
      try {
        return JSON.parse(configString);
      } catch {
        return {};
      }
    }
    return configString || {};
  }

  private static stringifyConditionConfig(config: any): string {
    if (typeof config === 'string') {
      return config;
    }
    return JSON.stringify(config || {});
  }

  // 활성 알람 조회
  static async getActiveAlarms(params?: AlarmListParams): Promise<ApiResponse<PaginatedResponse<AlarmOccurrence>>> {
    try {
      console.log('활성 알람 목록 조회:', params);
      
      const response = await this.httpClient.get<PaginatedResponse<AlarmOccurrence>>(
        ENDPOINTS.ALARMS_ACTIVE, 
        params
      );
      
      return response;
    } catch (error) {
      console.error('활성 알람 조회 실패:', error);
      throw error;
    }
  }

  // 전체 알람 발생 조회
  static async getAlarmOccurrences(params?: AlarmListParams): Promise<ApiResponse<PaginatedResponse<AlarmOccurrence>>> {
    console.log('알람 발생 목록 조회:', params);
    return this.httpClient.get<PaginatedResponse<AlarmOccurrence>>(ENDPOINTS.ALARMS_OCCURRENCES, params);
  }

  // 알람 이력 조회
  static async getAlarmHistory(params?: AlarmListParams): Promise<ApiResponse<PaginatedResponse<AlarmOccurrence>>> {
    console.log('알람 이력 조회:', params);
    return this.httpClient.get<PaginatedResponse<AlarmOccurrence>>(ENDPOINTS.ALARMS_HISTORY, params);
  }

  // 특정 알람 발생 조회
  static async getAlarmOccurrence(id: number): Promise<ApiResponse<AlarmOccurrence>> {
    console.log('알람 발생 상세 조회:', id);
    return this.httpClient.get<AlarmOccurrence>(ENDPOINTS.ALARMS_OCCURRENCE_BY_ID(id));
  }

  // 알람 규칙 목록 조회
  static async getAlarmRules(params?: AlarmListParams): Promise<ApiResponse<PaginatedResponse<AlarmRule>>> {
    try {
      console.log('알람 규칙 목록 조회:', params);
      
      // 백엔드 파라미터 매핑
      const backendParams: any = {};
      if (params?.page) backendParams.page = params.page;
      if (params?.limit) backendParams.limit = params.limit;
      if (params?.search) backendParams.search = params.search;
      if (params?.enabled !== undefined) backendParams.enabled = params.enabled;
      if (params?.severity) backendParams.severity = params.severity;
      if (params?.condition_type) backendParams.condition_type = params.condition_type;
      if (params?.device_id) backendParams.device_id = params.device_id;
      if (params?.data_point_id) backendParams.data_point_id = params.data_point_id;
      if (params?.virtual_point_id) backendParams.virtual_point_id = params.virtual_point_id;
      
      const response = await this.httpClient.get<PaginatedResponse<AlarmRule>>(
        ENDPOINTS.ALARM_RULES, 
        backendParams
      );
      
      // condition_config JSON 파싱
      //if (response.success && response.data?.items) {
      //  response.data.items = response.data.items.map(rule => ({
      //    ...rule,
      //    condition_config: this.parseConditionConfig(rule.condition_config)
      //  }));
      //}
      
      return response;
    } catch (error) {
      console.error('알람 규칙 조회 실패:', error);
      throw error;
    }
  }

  // 특정 알람 규칙 조회
  static async getAlarmRule(id: number): Promise<ApiResponse<AlarmRule>> {
    console.log('알람 규칙 상세 조회:', id);
    const response = await this.httpClient.get<AlarmRule>(ENDPOINTS.ALARM_RULE_BY_ID(id));
    
    if (response.success && response.data) {
      response.data.condition_config = this.parseConditionConfig(response.data.condition_config);
    }
    
    return response;
  }

  // 알람 규칙 생성
  static async createAlarmRule(data: AlarmRuleCreateData): Promise<ApiResponse<AlarmRule>> {
    console.log('새 알람 규칙 생성:', data);
    
    // 백엔드 형식으로 변환
    const backendData = {
      ...data,
      condition_config: this.stringifyConditionConfig(data.condition_config)
    };
    
    const response = await this.httpClient.post<AlarmRule>(ENDPOINTS.ALARM_RULES, backendData);
    
    if (response.success && response.data) {
      response.data.condition_config = this.parseConditionConfig(response.data.condition_config);
    }
    
    return response;
  }

  // 알람 규칙 수정
  static async updateAlarmRule(id: number, data: AlarmRuleUpdateData): Promise<ApiResponse<AlarmRule>> {
    console.log('알람 규칙 수정:', id, data);
    
    // 백엔드 형식으로 변환
    const backendData = {
      ...data,
      condition_config: data.condition_config ? this.stringifyConditionConfig(data.condition_config) : undefined
    };
    
    const response = await this.httpClient.put<AlarmRule>(ENDPOINTS.ALARM_RULE_BY_ID(id), backendData);
    
    if (response.success && response.data) {
      response.data.condition_config = this.parseConditionConfig(response.data.condition_config);
    }
    
    return response;
  }

  // 알람 규칙 삭제
  static async deleteAlarmRule(id: number): Promise<ApiResponse<{ deleted: boolean }>> {
    console.log('알람 규칙 삭제:', id);
    return this.httpClient.delete<{ deleted: boolean }>(ENDPOINTS.ALARM_RULE_BY_ID(id));
  }

  // 알람 확인 처리
  static async acknowledgeAlarm(id: number, data?: AcknowledgeAlarmRequest): Promise<ApiResponse<AlarmOccurrence>> {
    console.log('알람 확인 처리:', id, data);
    return this.httpClient.post<AlarmOccurrence>(ENDPOINTS.ALARMS_OCCURRENCE_ACKNOWLEDGE(id), data);
  }

  // 알람 해제 처리
  static async clearAlarm(id: number, data?: ClearAlarmRequest): Promise<ApiResponse<AlarmOccurrence>> {
    console.log('알람 해제 처리:', id, data);
    return this.httpClient.post<AlarmOccurrence>(ENDPOINTS.ALARMS_OCCURRENCE_CLEAR(id), data);
  }

  // 미확인 알람 조회
  static async getUnacknowledgedAlarms(): Promise<ApiResponse<AlarmOccurrence[]>> {
    console.log('미확인 알람 목록 조회');
    return this.httpClient.get<AlarmOccurrence[]>(ENDPOINTS.ALARM_UNACKNOWLEDGED);
  }

  // 최근 알람 조회
  static async getRecentAlarms(limit: number = 20): Promise<ApiResponse<AlarmOccurrence[]>> {
    console.log('최근 알람 조회:', limit);
    return this.httpClient.get<AlarmOccurrence[]>(ENDPOINTS.ALARM_RECENT, { limit });
  }

  // 디바이스 알람 조회
  static async getDeviceAlarms(deviceId: number): Promise<ApiResponse<AlarmOccurrence[]>> {
    console.log('디바이스 알람 조회:', deviceId);
    return this.httpClient.get<AlarmOccurrence[]>(ENDPOINTS.ALARM_DEVICE(deviceId));
  }

  // 알람 통계 조회
  static async getAlarmStatistics(): Promise<ApiResponse<AlarmStatistics>> {
    console.log('알람 통계 조회');
    return this.httpClient.get<AlarmStatistics>(ENDPOINTS.ALARM_STATISTICS);
  }

  // 알람 규칙 통계 조회
  static async getAlarmRuleStatistics(): Promise<ApiResponse<any>> {
    console.log('알람 규칙 통계 조회');
    return this.httpClient.get<any>(ENDPOINTS.ALARM_RULES_STATISTICS);
  }

  // 템플릿 관련 메서드들
  static async getAlarmTemplates(params?: {
    page?: number;
    limit?: number;
    category?: string;
    is_system_template?: boolean;
    search?: string;
  }): Promise<ApiResponse<PaginatedResponse<AlarmTemplate>>> {
    console.log('알람 템플릿 목록 조회:', params);
    return this.httpClient.get<PaginatedResponse<AlarmTemplate>>(ENDPOINTS.ALARM_TEMPLATES, params);
  }

  static async getAlarmTemplate(id: number): Promise<ApiResponse<AlarmTemplate>> {
    console.log('알람 템플릿 상세 조회:', id);
    const response = await this.httpClient.get<AlarmTemplate>(ENDPOINTS.ALARM_TEMPLATE_BY_ID(id));
    
    if (response.success && response.data) {
      // JSON 필드 파싱
      if (typeof response.data.default_config === 'string') {
        response.data.default_config = JSON.parse(response.data.default_config);
      }
      if (typeof response.data.applicable_data_types === 'string') {
        response.data.applicable_data_types = JSON.parse(response.data.applicable_data_types);
      }
    }
    
    return response;
  }

  static async createAlarmTemplate(data: AlarmTemplateCreateData): Promise<ApiResponse<AlarmTemplate>> {
    console.log('새 알람 템플릿 생성:', data);
    
    // JSON 필드 문자열화
    const backendData = {
      ...data,
      default_config: data.default_config ? JSON.stringify(data.default_config) : '{}',
      applicable_data_types: data.applicable_data_types ? JSON.stringify(data.applicable_data_types) : '["*"]'
    };
    
    return this.httpClient.post<AlarmTemplate>(ENDPOINTS.ALARM_TEMPLATES, backendData);
  }

  static async updateAlarmTemplate(id: number, data: Partial<AlarmTemplateCreateData>): Promise<ApiResponse<AlarmTemplate>> {
    console.log('알람 템플릿 수정:', id, data);
    
    // JSON 필드 문자열화
    const backendData = {
      ...data,
      default_config: data.default_config ? JSON.stringify(data.default_config) : undefined,
      applicable_data_types: data.applicable_data_types ? JSON.stringify(data.applicable_data_types) : undefined
    };
    
    return this.httpClient.put<AlarmTemplate>(ENDPOINTS.ALARM_TEMPLATE_BY_ID(id), backendData);
  }

  static async deleteAlarmTemplate(id: number): Promise<ApiResponse<{ deleted: boolean }>> {
    console.log('알람 템플릿 삭제:', id);
    return this.httpClient.delete<{ deleted: boolean }>(ENDPOINTS.ALARM_TEMPLATE_BY_ID(id));
  }

  static async getSystemTemplates(): Promise<ApiResponse<AlarmTemplate[]>> {
    console.log('시스템 템플릿 조회');
    return this.httpClient.get<AlarmTemplate[]>(ENDPOINTS.ALARM_TEMPLATES_SYSTEM);
  }

  static async getTemplatesByCategory(category: string): Promise<ApiResponse<AlarmTemplate[]>> {
    console.log('카테고리별 템플릿 조회:', category);
    return this.httpClient.get<AlarmTemplate[]>(ENDPOINTS.ALARM_TEMPLATES_CATEGORY(category));
  }

  static async getTemplatesByDataType(dataType: string): Promise<ApiResponse<AlarmTemplate[]>> {
    console.log('데이터 타입별 템플릿 조회:', dataType);
    return this.httpClient.get<AlarmTemplate[]>(ENDPOINTS.ALARM_TEMPLATES_DATA_TYPE(dataType));
  }

  static async applyTemplate(
    templateId: number, 
    data: {
      data_point_ids: number[];
      custom_configs?: Record<number, any>;
      rule_group_name?: string;
    }
  ): Promise<ApiResponse<any>> {
    console.log('템플릿 적용:', templateId, data);
    return this.httpClient.post<any>(ENDPOINTS.ALARM_TEMPLATE_APPLY(templateId), data);
  }

  static async getTemplateAppliedRules(templateId: number): Promise<ApiResponse<AlarmRule[]>> {
    console.log('템플릿 적용 규칙 조회:', templateId);
    return this.httpClient.get<AlarmRule[]>(ENDPOINTS.ALARM_TEMPLATE_APPLIED_RULES(templateId));
  }

  static async getTemplateStatistics(): Promise<ApiResponse<any>> {
    console.log('템플릿 통계 조회');
    return this.httpClient.get<any>(ENDPOINTS.ALARM_TEMPLATES_STATISTICS);
  }

  static async searchTemplates(searchTerm: string, limit: number = 20): Promise<ApiResponse<AlarmTemplate[]>> {
    console.log('템플릿 검색:', searchTerm);
    return this.httpClient.get<AlarmTemplate[]>(ENDPOINTS.ALARM_TEMPLATES_SEARCH, { q: searchTerm, limit });
  }

  static async getMostUsedTemplates(limit: number = 10): Promise<ApiResponse<AlarmTemplate[]>> {
    console.log('인기 템플릿 조회:', limit);
    return this.httpClient.get<AlarmTemplate[]>(ENDPOINTS.ALARM_TEMPLATES_MOST_USED, { limit });
  }

  // 설정 관련 메서드들
  static async updateAlarmRuleSettings(id: number, settings: any): Promise<ApiResponse<AlarmRule>> {
    console.log('알람 규칙 설정 업데이트:', id, settings);
    return this.httpClient.patch<AlarmRule>(ENDPOINTS.ALARM_RULE_SETTINGS(id), settings);
  }

  // API 테스트
  static async testAlarmApi(): Promise<ApiResponse<any>> {
    console.log('알람 API 테스트');
    return this.httpClient.get<any>(ENDPOINTS.ALARM_TEST);
  }

  // 유틸리티 메서드들
  static getSeverityColor(severity: string): string {
    const colors = {
      critical: '#dc2626',   // 빨간색
      major: '#ea580c',      // 주황색
      minor: '#ca8a04',      // 노란색
      warning: '#0891b2',    // 파란색
      info: '#0284c7'        // 연한 파란색
    };
    return colors[severity as keyof typeof colors] || colors.info;
  }

  static getSeverityIcon(severity: string): string {
    const icons = {
      critical: 'fas fa-exclamation-triangle',
      major: 'fas fa-exclamation-circle',
      minor: 'fas fa-info-circle',
      warning: 'fas fa-exclamation',
      info: 'fas fa-info'
    };
    return icons[severity as keyof typeof icons] || icons.info;
  }

  static getStateDisplayText(state: string): string {
    const texts = {
      active: '활성',
      acknowledged: '확인됨',
      cleared: '해제됨'
    };
    return texts[state as keyof typeof texts] || state;
  }

  static getStateColor(state: string): string {
    const colors = {
      active: '#dc2626',      // 빨간색
      acknowledged: '#ca8a04', // 노란색
      cleared: '#16a34a'      // 초록색
    };
    return colors[state as keyof typeof colors] || colors.active;
  }

  static formatDuration(startTime: string, endTime?: string): string {
    const start = new Date(startTime).getTime();
    const end = endTime ? new Date(endTime).getTime() : Date.now();
    const durationMs = end - start;
    
    const seconds = Math.floor(durationMs / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) {
      return `${days}일 ${hours % 24}시간`;
    } else if (hours > 0) {
      return `${hours}시간 ${minutes % 60}분`;
    } else if (minutes > 0) {
      return `${minutes}분`;
    } else {
      return `${seconds}초`;
    }
  }

  // 필터링 및 정렬
  static filterAlarms(
    alarms: AlarmOccurrence[], 
    filters: {
      search?: string;
      severity?: string;
      state?: string;
      device_id?: number;
    }
  ): AlarmOccurrence[] {
    return alarms.filter(alarm => {
      if (filters.search) {
        const search = filters.search.toLowerCase();
        const matchesSearch = 
          alarm.rule_name?.toLowerCase().includes(search) ||
          alarm.message.toLowerCase().includes(search) ||
          alarm.device_name?.toLowerCase().includes(search) ||
          alarm.data_point_name?.toLowerCase().includes(search);
        
        if (!matchesSearch) return false;
      }

      if (filters.severity && filters.severity !== 'all') {
        if (alarm.severity !== filters.severity) return false;
      }

      if (filters.state && filters.state !== 'all') {
        if (alarm.state !== filters.state) return false;
      }

      if (filters.device_id) {
        if (alarm.device_id !== filters.device_id) return false;
      }

      return true;
    });
  }

  static sortAlarms(
    alarms: AlarmOccurrence[], 
    sortBy: keyof AlarmOccurrence = 'occurrence_time',
    sortOrder: 'ASC' | 'DESC' = 'DESC'
  ): AlarmOccurrence[] {
    return [...alarms].sort((a, b) => {
      let comparison = 0;

      switch (sortBy) {
        case 'occurrence_time':
          comparison = new Date(a.occurrence_time).getTime() - new Date(b.occurrence_time).getTime();
          break;
        case 'severity':
          const severityOrder = { critical: 5, major: 4, minor: 3, warning: 2, info: 1 };
          comparison = (severityOrder[a.severity as keyof typeof severityOrder] || 0) - 
                      (severityOrder[b.severity as keyof typeof severityOrder] || 0);
          break;
        case 'rule_name':
          comparison = (a.rule_name || '').localeCompare(b.rule_name || '');
          break;
        case 'state':
          comparison = a.state.localeCompare(b.state);
          break;
        default:
          comparison = 0;
      }

      return sortOrder === 'ASC' ? comparison : -comparison;
    });
  }
}