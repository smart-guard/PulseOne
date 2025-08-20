// ============================================================================
// frontend/src/api/services/alarmApi.ts
// 🚨 알람 관리 API 서비스 - Backend /api/alarms 완전 호환 + AlarmSettings 지원
// ============================================================================

import { ENDPOINTS } from '../endpoints';

// ============================================================================
// 📋 알람 관련 타입 정의
// ============================================================================

export interface AlarmRule {
  id: number;
  rule_name: string;
  description?: string;
  target_type: 'data_point' | 'device' | 'virtual_point';
  target_id: number;
  device_name?: string;
  data_point_name?: string;
  condition_type: 'high_limit' | 'low_limit' | 'range' | 'status' | 'boolean';
  threshold_high?: number;
  threshold_low?: number;
  threshold_value?: any;
  severity: 'critical' | 'major' | 'minor' | 'warning' | 'info';
  enabled: boolean;
  auto_clear: boolean;
  notification_enabled: boolean;
  email_recipients?: string;
  sms_recipients?: string;
  tenant_id: number;
  created_at: string;
  updated_at: string;
}

export interface AlarmOccurrence {
  id: number;
  rule_id: number;
  rule_name: string;
  target_type: string;
  target_id: number;
  device_name?: string;
  data_point_name?: string;
  message: string;
  description?: string;
  severity: string;
  triggered_value?: any;
  threshold_value?: any;
  state: 'active' | 'acknowledged' | 'cleared';
  triggered_at: string;
  acknowledged_at?: string;
  acknowledged_by?: string;
  acknowledgment_comment?: string;
  cleared_at?: string;
  auto_cleared: boolean;
  tenant_id: number;
}

export interface AlarmStatistics {
  total_active: number;
  total_acknowledged: number;
  total_cleared: number;
  by_severity: {
    critical: number;
    major: number;
    minor: number;
    warning: number;
    info: number;
  };
  recent_24h: number;
  average_response_time: number;
  top_sources: Array<{
    name: string;
    count: number;
  }>;
}

// ========================================================================
// 📋 AlarmSettings 전용 타입 정의
// ========================================================================

export interface AlarmRuleSettings {
  // 임계값 설정
  highHighLimit?: number;
  highLimit?: number;
  lowLimit?: number;
  lowLowLimit?: number;
  deadband?: number;
  targetValue?: number;
  tolerance?: number;
  timeWindow?: number;
  pattern?: string;
  
  // 동작 설정
  priority: 'low' | 'medium' | 'high' | 'critical';
  severity: 1 | 2 | 3 | 4 | 5;
  autoAcknowledge: boolean;
  autoReset: boolean;
  suppressDuration: number; // 중복 억제 시간 (초)
  maxOccurrences: number;   // 최대 발생 횟수
  escalationTime: number;   // 에스컬레이션 시간 (분)
  
  // 알림 설정
  emailEnabled: boolean;
  emailRecipients: string[];
  smsEnabled: boolean;
  smsRecipients: string[];
  soundEnabled: boolean;
  popupEnabled: boolean;
  webhookEnabled: boolean;
  webhookUrl: string;
  
  // 메시지 설정
  messageTemplate: string;
  emailTemplate: string;
  
  // 스케줄 설정
  schedule: {
    type: 'always' | 'business_hours' | 'custom';
    startTime?: string;
    endTime?: string;
    weekdays?: number[];
    holidays?: string[];
  };
  
  // 상태
  isEnabled: boolean;
}

export interface AlarmRuleStatistics {
  rule_info: {
    id: number;
    name: string;
    is_enabled: boolean;
    severity: string;
    created_at: string;
  };
  period: {
    start_date: string;
    end_date: string;
    days: number;
  };
  occurrence_summary: {
    total_occurrences: number;
    acknowledged_count: number;
    cleared_count: number;
    pending_count: number;
    acknowledgment_rate: number;
  };
  performance_metrics: {
    avg_response_time_minutes: number;
    frequency_per_day: number;
    last_triggered: string | null;
  };
  distributions: {
    by_severity: Record<string, number>;
    last_7_days: Array<{ date: string; occurrences: number }>;
    hourly_pattern: Array<{ hour: number; count: number }>;
  };
}

export interface BulkUpdateRequest {
  rule_ids: number[];
  settings: Partial<AlarmRuleSettings>;
}

export interface BulkUpdateResponse {
  total_requested: number;
  successful_updates: number;
  failed_updates: number;
  updated_rules: Array<{ rule_id: number; success: boolean }>;
  failed_rules: Array<{ rule_id: number; error: string }>;
  applied_settings: string[];
}

export interface RuleTestRequest {
  test_value: number;
  test_scenario: 'threshold' | 'range' | 'pattern';
}

export interface RuleTestResponse {
  rule_id: number;
  rule_name: string;
  test_scenario: string;
  test_value: number;
  would_trigger: boolean;
  trigger_reason: string | null;
  recommended_action: string | null;
}

export interface AlarmListParams {
  page?: number;
  limit?: number;
  severity?: string;
  state?: string;
  device_id?: number;
  acknowledged?: boolean;
  search?: string;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
  start_date?: string;
  end_date?: string;
}

export interface AcknowledgeAlarmRequest {
  comment?: string;
  acknowledged_by?: string;
}

export interface AlarmRuleCreateData {
  rule_name: string;
  description?: string;
  target_type: 'data_point' | 'device' | 'virtual_point';
  target_id: number;
  condition_type: 'high_limit' | 'low_limit' | 'range' | 'status' | 'boolean';
  threshold_high?: number;
  threshold_low?: number;
  threshold_value?: any;
  severity: 'critical' | 'major' | 'minor' | 'warning' | 'info';
  enabled?: boolean;
  auto_clear?: boolean;
  notification_enabled?: boolean;
  email_recipients?: string;
  sms_recipients?: string;
}

export interface AlarmRuleUpdateData extends Partial<AlarmRuleCreateData> {}

// ============================================================================
// 🌐 공통 API 응답 타입
// ============================================================================

export interface ApiResponse<T> {
  success: boolean;
  data: T | null;
  message: string;
  error?: string;
  timestamp: string;
}

// ============================================================================
// 🔧 HTTP 클라이언트 클래스 (간단 버전)
// ============================================================================

class HttpClient {
  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    try {
      const response = await fetch(`${endpoint}`, {
        headers: {
          'Content-Type': 'application/json',
          ...options.headers,
        },
        ...options,
      });

      const data = await response.json();
      
      if (!response.ok) {
        return {
          success: false,
          data: null,
          message: data.message || `HTTP ${response.status}`,
          error: data.error || response.statusText,
          timestamp: new Date().toISOString()
        };
      }

      return data;
    } catch (error: any) {
      return {
        success: false,
        message: error.message || 'Network error',
        error: error.message || 'Unknown error',
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

// ============================================================================
// 🚨 알람 API 서비스 클래스
// ============================================================================

export class AlarmApiService {
  private static httpClient = new HttpClient();

  // ========================================================================
  // 📋 활성 알람 관리
  // ========================================================================

  /**
   * 활성 알람 목록 조회
   */
  static async getActiveAlarms(params?: AlarmListParams): Promise<ApiResponse<{
    items: AlarmOccurrence[];
    pagination: {
      page: number;
      limit: number;
      total: number;
      totalPages: number;
      hasNext: boolean;
      hasPrev: boolean;
    };
  }>> {
    console.log('🚨 활성 알람 목록 조회:', params);
    return this.httpClient.get<any>(ENDPOINTS.ALARMS_ACTIVE, params);
  }

  /**
   * 알람 이력 조회
   */
  static async getAlarmHistory(params?: AlarmListParams): Promise<ApiResponse<{
    items: AlarmOccurrence[];
    pagination: any;
  }>> {
    console.log('📊 알람 이력 조회:', params);
    return this.httpClient.get<any>(ENDPOINTS.ALARMS_HISTORY, params);
  }

  /**
   * 특정 알람 발생 조회
   */
  static async getAlarmOccurrence(id: number): Promise<ApiResponse<AlarmOccurrence>> {
    console.log('🔍 알람 발생 상세 조회:', id);
    return this.httpClient.get<AlarmOccurrence>(ENDPOINTS.ALARMS_OCCURRENCE_BY_ID(id));
  }

  /**
   * 알람 확인 처리
   */
  static async acknowledgeAlarm(id: number, data?: AcknowledgeAlarmRequest): Promise<ApiResponse<boolean>> {
    console.log('✅ 알람 확인 처리:', id, data);
    return this.httpClient.post<boolean>(ENDPOINTS.ALARMS_OCCURRENCE_ACKNOWLEDGE(id), data);
  }

  /**
   * 알람 해제 처리
   */
  static async clearAlarm(id: number, comment?: string): Promise<ApiResponse<boolean>> {
    console.log('🔄 알람 해제 처리:', id, comment);
    return this.httpClient.post<boolean>(ENDPOINTS.ALARMS_OCCURRENCE_CLEAR(id), { comment });
  }

  /**
   * 미확인 알람 목록 조회
   */
  static async getUnacknowledgedAlarms(): Promise<ApiResponse<AlarmOccurrence[]>> {
    console.log('⚠️ 미확인 알람 목록 조회');
    return this.httpClient.get<AlarmOccurrence[]>(ENDPOINTS.ALARM_UNACKNOWLEDGED);
  }

  /**
   * 특정 디바이스의 알람 조회
   */
  static async getDeviceAlarms(deviceId: number): Promise<ApiResponse<AlarmOccurrence[]>> {
    console.log('🏭 디바이스 알람 조회:', deviceId);
    return this.httpClient.get<AlarmOccurrence[]>(ENDPOINTS.ALARM_DEVICE(deviceId));
  }

  // ========================================================================
  // 📋 알람 규칙 관리
  // ========================================================================

  /**
   * 알람 규칙 목록 조회
   */
  static async getAlarmRules(params?: {
    page?: number;
    limit?: number;
    search?: string;
    enabled?: boolean;
    severity?: string;
  }): Promise<ApiResponse<{
    items: AlarmRule[];
    pagination: any;
  }>> {
    console.log('📋 알람 규칙 목록 조회:', params);
    return this.httpClient.get<any>(ENDPOINTS.ALARM_RULES, params);
  }

  /**
   * 특정 알람 규칙 조회
   */
  static async getAlarmRule(id: number): Promise<ApiResponse<AlarmRule>> {
    console.log('🔍 알람 규칙 상세 조회:', id);
    return this.httpClient.get<AlarmRule>(ENDPOINTS.ALARM_RULE_BY_ID(id));
  }

  /**
   * 새 알람 규칙 생성
   */
  static async createAlarmRule(data: AlarmRuleCreateData): Promise<ApiResponse<AlarmRule>> {
    console.log('➕ 새 알람 규칙 생성:', data);
    return this.httpClient.post<AlarmRule>(ENDPOINTS.ALARM_RULES, data);
  }

  /**
   * 알람 규칙 수정
   */
  static async updateAlarmRule(id: number, data: AlarmRuleUpdateData): Promise<ApiResponse<AlarmRule>> {
    console.log('✏️ 알람 규칙 수정:', id, data);
    return this.httpClient.put<AlarmRule>(ENDPOINTS.ALARM_RULE_BY_ID(id), data);
  }

  /**
   * 알람 규칙 삭제
   */
  static async deleteAlarmRule(id: number): Promise<ApiResponse<boolean>> {
    console.log('🗑️ 알람 규칙 삭제:', id);
    return this.httpClient.delete<boolean>(ENDPOINTS.ALARM_RULE_BY_ID(id));
  }

  // ========================================================================
  // 🔧 AlarmSettings 전용 API 메서드들
  // ========================================================================

  /**
   * 알람 규칙 설정 부분 업데이트 (AlarmSettings.tsx용)
   */
  static async updateAlarmRuleSettings(
    ruleId: number, 
    settings: Partial<AlarmRuleSettings>
  ): Promise<ApiResponse<AlarmRule>> {
    console.log('🔧 알람 규칙 설정 업데이트:', ruleId, settings);
    return this.httpClient.patch<AlarmRule>(`/api/alarms/rules/${ruleId}/settings`, settings);
  }

  /**
   * 여러 알람 규칙 일괄 설정 업데이트
   */
  static async bulkUpdateAlarmRules(
    request: BulkUpdateRequest
  ): Promise<ApiResponse<BulkUpdateResponse>> {
    console.log('📝 알람 규칙 일괄 업데이트:', request);
    return this.httpClient.patch<BulkUpdateResponse>('/api/alarms/rules/bulk-update', request);
  }

  /**
   * 알람 규칙 상세 통계 조회
   */
  static async getAlarmRuleStatistics(
    ruleId: number, 
    days: number = 30
  ): Promise<ApiResponse<AlarmRuleStatistics>> {
    console.log('📊 알람 규칙 통계 조회:', ruleId, days);
    return this.httpClient.get<AlarmRuleStatistics>(
      `/api/alarms/rules/${ruleId}/statistics`,
      { days }
    );
  }

  /**
   * 알람 규칙 테스트 실행
   */
  static async testAlarmRule(
    ruleId: number, 
    request: RuleTestRequest
  ): Promise<ApiResponse<RuleTestResponse>> {
    console.log('🧪 알람 규칙 테스트:', ruleId, request);
    return this.httpClient.post<RuleTestResponse>(
      `/api/alarms/rules/${ruleId}/test`,
      request
    );
  }

  /**
   * 알람 규칙 설정 변경 이력 조회
   */
  static async getAlarmRuleConfigHistory(
    ruleId: number, 
    limit: number = 20
  ): Promise<ApiResponse<any>> {
    console.log('📜 알람 규칙 설정 이력 조회:', ruleId);
    return this.httpClient.get<any>(
      `/api/alarms/rules/${ruleId}/configuration-history`,
      { limit }
    );
  }

  // ========================================================================
  // 📊 통계 및 기타
  // ========================================================================

  /**
   * 알람 통계 조회
   */
  static async getAlarmStatistics(): Promise<ApiResponse<AlarmStatistics>> {
    console.log('📊 알람 통계 조회');
    return this.httpClient.get<AlarmStatistics>(ENDPOINTS.ALARM_STATISTICS);
  }

  /**
   * 알람 API 테스트
   */
  static async testAlarmApi(): Promise<ApiResponse<any>> {
    console.log('🧪 알람 API 테스트');
    return this.httpClient.get<any>(ENDPOINTS.ALARM_TEST);
  }

  // ========================================================================
  // 🔧 클라이언트 사이드 유틸리티 함수들
  // ========================================================================

  /**
   * 알람 심각도 색상 반환
   */
  static getSeverityColor(severity: string): string {
    const colors = {
      critical: '#dc2626',  // red-600
      major: '#ea580c',     // orange-600
      minor: '#ca8a04',     // yellow-600
      warning: '#0891b2',   // cyan-600
      info: '#0284c7'       // blue-600
    };
    return colors[severity as keyof typeof colors] || colors.info;
  }

  /**
   * 알람 심각도 아이콘 반환
   */
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

  /**
   * 알람 상태 표시 텍스트 반환
   */
  static getStateDisplayText(state: string): string {
    const texts = {
      active: '활성',
      acknowledged: '확인됨',
      cleared: '해제됨'
    };
    return texts[state as keyof typeof texts] || state;
  }

  /**
   * 우선순위별 색상 반환
   */
  static getPriorityColor(priority: string): string {
    const colors = {
      critical: '#dc2626',  // red-600
      high: '#ea580c',      // orange-600
      medium: '#ca8a04',    // yellow-600
      low: '#22c55e'        // green-500
    };
    return colors[priority as keyof typeof colors] || colors.medium;
  }

  /**
   * 우선순위별 CSS 클래스 반환
   */
  static getPriorityClass(priority: string): string {
    return `priority-${priority}`;
  }

  /**
   * 알람 지속 시간 계산 (ms)
   */
  static calculateDuration(triggeredAt: string, clearedAt?: string): number {
    const start = new Date(triggeredAt).getTime();
    const end = clearedAt ? new Date(clearedAt).getTime() : Date.now();
    return end - start;
  }

  /**
   * 지속 시간을 읽기 쉬운 형식으로 변환
   */
  static formatDuration(durationMs: number): string {
    const seconds = Math.floor(durationMs / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) {
      return `${days}일 ${hours % 24}시간`;
    } else if (hours > 0) {
      return `${hours}시간 ${minutes % 60}분`;
    } else if (minutes > 0) {
      return `${minutes}분 ${seconds % 60}초`;
    } else {
      return `${seconds}초`;
    }
  }

  /**
   * 설정 변경사항 비교
   */
  static compareSettings(
    oldSettings: Partial<AlarmRuleSettings>, 
    newSettings: Partial<AlarmRuleSettings>
  ): { changed: string[]; unchanged: string[] } {
    const changed: string[] = [];
    const unchanged: string[] = [];
    
    const allKeys = new Set([...Object.keys(oldSettings), ...Object.keys(newSettings)]);
    
    allKeys.forEach(key => {
      const oldValue = oldSettings[key as keyof AlarmRuleSettings];
      const newValue = newSettings[key as keyof AlarmRuleSettings];
      
      if (JSON.stringify(oldValue) !== JSON.stringify(newValue)) {
        changed.push(key);
      } else {
        unchanged.push(key);
      }
    });
    
    return { changed, unchanged };
  }

  /**
   * 설정 유효성 검증
   */
  static validateSettings(settings: Partial<AlarmRuleSettings>): {
    isValid: boolean;
    errors: string[];
    warnings: string[];
  } {
    const errors: string[] = [];
    const warnings: string[] = [];
    
    // 임계값 검증
    if (settings.highLimit !== undefined && settings.lowLimit !== undefined) {
      if (settings.highLimit <= settings.lowLimit) {
        errors.push('High limit must be greater than low limit');
      }
    }
    
    // 이메일 수신자 검증
    if (settings.emailEnabled && (!settings.emailRecipients || settings.emailRecipients.length === 0)) {
      warnings.push('Email is enabled but no recipients specified');
    }
    
    // SMS 수신자 검증
    if (settings.smsEnabled && (!settings.smsRecipients || settings.smsRecipients.length === 0)) {
      warnings.push('SMS is enabled but no recipients specified');
    }
    
    // 억제 시간 검증
    if (settings.suppressDuration !== undefined && settings.suppressDuration < 0) {
      errors.push('Suppress duration cannot be negative');
    }
    
    return {
      isValid: errors.length === 0,
      errors,
      warnings
    };
  }

  /**
   * 설정을 백엔드 형식으로 변환
   */
  static transformSettingsForBackend(settings: Partial<AlarmRuleSettings>): Record<string, any> {
    const transformed: Record<string, any> = {};
    
    Object.entries(settings).forEach(([key, value]) => {
      // 특별한 변환이 필요한 필드들
      switch (key) {
        case 'emailRecipients':
        case 'smsRecipients':
          transformed[key] = Array.isArray(value) ? value : [];
          break;
        case 'schedule':
          transformed[key] = typeof value === 'object' ? value : {};
          break;
        default:
          transformed[key] = value;
      }
    });
    
    return transformed;
  }

  /**
   * 백엔드 응답을 프론트엔드 형식으로 변환
   */
  static transformSettingsFromBackend(backendData: any): Partial<AlarmRuleSettings> {
    const settings: Partial<AlarmRuleSettings> = {};
    
    // 필드 매핑 (백엔드 -> 프론트엔드)
    const fieldMapping = {
      'high_high_limit': 'highHighLimit',
      'high_limit': 'highLimit',
      'low_limit': 'lowLimit',
      'low_low_limit': 'lowLowLimit',
      'auto_acknowledge': 'autoAcknowledge',
      'auto_clear': 'autoReset',
      'email_notification': 'emailEnabled',
      'sms_notification': 'smsEnabled',
      'message_template': 'messageTemplate',
      'is_enabled': 'isEnabled'
    };
    
    Object.entries(backendData).forEach(([backendKey, value]) => {
      const frontendKey = fieldMapping[backendKey as keyof typeof fieldMapping] || backendKey;
      
      // 특별한 변환이 필요한 필드들
      if (backendKey === 'email_recipients' || backendKey === 'sms_recipients') {
        try {
          settings[frontendKey as keyof AlarmRuleSettings] = typeof value === 'string' ? JSON.parse(value) : value;
        } catch {
          settings[frontendKey as keyof AlarmRuleSettings] = [] as any;
        }
      } else {
        settings[frontendKey as keyof AlarmRuleSettings] = value as any;
      }
    });
    
    return settings;
  }

  /**
   * 알람 목록 필터링 (클라이언트 사이드)
   */
  static filterAlarms(
    alarms: AlarmOccurrence[], 
    filters: {
      search?: string;
      severity?: string;
      state?: string;
    }
  ): AlarmOccurrence[] {
    return alarms.filter(alarm => {
      // 검색어 필터
      if (filters.search) {
        const search = filters.search.toLowerCase();
        const matchesSearch = 
          alarm.rule_name.toLowerCase().includes(search) ||
          alarm.message.toLowerCase().includes(search) ||
          (alarm.device_name && alarm.device_name.toLowerCase().includes(search)) ||
          (alarm.data_point_name && alarm.data_point_name.toLowerCase().includes(search));
        
        if (!matchesSearch) return false;
      }

      // 심각도 필터
      if (filters.severity && filters.severity !== 'all') {
        if (alarm.severity !== filters.severity) return false;
      }

      // 상태 필터
      if (filters.state && filters.state !== 'all') {
        if (alarm.state !== filters.state) return false;
      }

      return true;
    });
  }

  /**
   * 알람 목록 정렬
   */
  static sortAlarms(
    alarms: AlarmOccurrence[], 
    sortBy: 'triggered_at' | 'severity' | 'rule_name' | 'state' = 'triggered_at',
    sortOrder: 'ASC' | 'DESC' = 'DESC'
  ): AlarmOccurrence[] {
    return [...alarms].sort((a, b) => {
      let comparison = 0;

      switch (sortBy) {
        case 'triggered_at':
          comparison = new Date(a.triggered_at).getTime() - new Date(b.triggered_at).getTime();
          break;
        case 'severity':
          const severityOrder = { critical: 5, major: 4, minor: 3, warning: 2, info: 1 };
          comparison = (severityOrder[a.severity as keyof typeof severityOrder] || 0) - 
                      (severityOrder[b.severity as keyof typeof severityOrder] || 0);
          break;
        case 'rule_name':
          comparison = a.rule_name.localeCompare(b.rule_name);
          break;
        case 'state':
          comparison = a.state.localeCompare(b.state);
          break;
      }

      return sortOrder === 'ASC' ? comparison : -comparison;
    });
  }
}