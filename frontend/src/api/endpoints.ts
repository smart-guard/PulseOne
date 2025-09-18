// ============================================================================
// frontend/src/api/endpoints.ts
// API 엔드포인트 상수 정의 - 완성본 (전체)
// ============================================================================

// React 환경에서 process.env 안전하게 접근
const getApiBase = () => {
  // 브라우저 환경
  if (typeof window !== 'undefined') {
    return window.location.hostname === 'localhost' 
      ? 'http://localhost:3000'
      : window.location.origin;
  }
  
  // 빌드 시점에 환경변수가 있다면 사용
  try {
    return process?.env?.REACT_APP_API_BASE || 'http://localhost:3000';
  } catch {
    return 'http://localhost:3000';
  }
};

const API_BASE = getApiBase();

/**
 * API 엔드포인트 상수들
 * 모든 API URL을 중앙에서 관리
 */
export const ENDPOINTS = {
  // ==========================================================================
  // 헬스체크 및 기본 정보
  // ==========================================================================
  HEALTH: `${API_BASE}/api/health`,
  API_INFO: `${API_BASE}/api`,
  
  // ==========================================================================
  // 시스템 관리 API
  // ==========================================================================
  SYSTEM_STATUS: `${API_BASE}/api/system/status`,
  SYSTEM_INFO: `${API_BASE}/api/system/info`, 
  SYSTEM_DATABASES: `${API_BASE}/api/system/databases`,
  SYSTEM_HEALTH: `${API_BASE}/api/system/health`,
  
  // ==========================================================================
  // 서비스 제어 - 백엔드와 완벽 일치
  // ==========================================================================
  SERVICES: `${API_BASE}/api/services`,
  SERVICE_BY_NAME: (name: string) => `${API_BASE}/api/services/${name}`,
  
  // Collector 서비스 제어
  COLLECTOR_START: `${API_BASE}/api/services/collector/start`,
  COLLECTOR_STOP: `${API_BASE}/api/services/collector/stop`,
  COLLECTOR_RESTART: `${API_BASE}/api/services/collector/restart`,
  COLLECTOR_STATUS: `${API_BASE}/api/services/collector`,
  
  // Redis 서비스 제어
  REDIS_START: `${API_BASE}/api/services/redis/start`,
  REDIS_STOP: `${API_BASE}/api/services/redis/stop`,
  REDIS_RESTART: `${API_BASE}/api/services/redis/restart`,
  REDIS_SERVICE_STATUS: `${API_BASE}/api/services/redis/status`,
  
  // 범용 서비스 제어 함수
  SERVICE_START: (name: string) => `${API_BASE}/api/services/${name}/start`,
  SERVICE_STOP: (name: string) => `${API_BASE}/api/services/${name}/stop`,
  SERVICE_RESTART: (name: string) => `${API_BASE}/api/services/${name}/restart`,
  SERVICE_STATUS: (name: string) => `${API_BASE}/api/services/${name}`,
  
  // 서비스 헬스체크 및 시스템 정보
  SERVICES_HEALTH_CHECK: `${API_BASE}/api/services/health/check`,
  SERVICES_SYSTEM_INFO: `${API_BASE}/api/services/system/info`,
  SERVICES_PROCESSES: `${API_BASE}/api/services/processes`,
  SERVICES_PLATFORM_INFO: `${API_BASE}/api/services/platform/info`,
  
  // 프로세스 관리
  PROCESSES: `${API_BASE}/api/processes`,
  PROCESS_BY_ID: (id: number | string) => `${API_BASE}/api/processes/${id}`,
  
  // ==========================================================================
  // 디바이스 관리 API (protocol_id 직접 처리 지원)
  // ==========================================================================
  DEVICES: `${API_BASE}/api/devices`,
  DEVICE_BY_ID: (id: number | string) => `${API_BASE}/api/devices/${id}`,
  DEVICE_DATA_POINTS: (id: number | string) => `${API_BASE}/api/devices/${id}/data-points`,
  DEVICE_TEST_CONNECTION: (id: number | string) => `${API_BASE}/api/devices/${id}/test-connection`,
  DEVICE_ENABLE: (id: number | string) => `${API_BASE}/api/devices/${id}/enable`,
  DEVICE_DISABLE: (id: number | string) => `${API_BASE}/api/devices/${id}/disable`,
  DEVICE_RESTART: (id: number | string) => `${API_BASE}/api/devices/${id}/restart`,
  DEVICE_PROTOCOLS: `${API_BASE}/api/devices/protocols`,  // ID 정보 포함하여 반환
  DEVICE_STATISTICS: `${API_BASE}/api/devices/statistics`,
  DEVICE_BULK_ACTION: `${API_BASE}/api/devices/bulk-action`,
  
  // RTU 전용 엔드포인트들
  RTU_NETWORKS: `${API_BASE}/api/devices/rtu/networks`,
  RTU_NETWORK_BY_ID: (id: number | string) => `${API_BASE}/api/devices/rtu/networks/${id}`,
  RTU_MASTER_SLAVES: (masterId: number | string) => `${API_BASE}/api/devices/rtu/master/${masterId}/slaves`,
  RTU_SLAVE_STATUS: (slaveId: number | string) => `${API_BASE}/api/devices/rtu/slave/${slaveId}/status`,
  RTU_NETWORK_SCAN: `${API_BASE}/api/devices/rtu/scan`,
  RTU_NETWORK_STATUS: (port: string) => `${API_BASE}/api/devices/rtu/port/${encodeURIComponent(port)}/status`,
  
  // 디버깅 API (개발용)
  DEBUG_DEVICES_DIRECT: `${API_BASE}/api/devices/debug/direct`,
  DEBUG_REPOSITORY: `${API_BASE}/api/devices/debug/repository`,
  
  // ==========================================================================
  // 데이터 탐색 API
  // ==========================================================================
  DATA_POINTS: `${API_BASE}/api/data/points`,
  DATA_POINT_BY_ID: (id: number | string) => `${API_BASE}/api/data/points/${id}`,
  DATA_CURRENT_VALUES: `${API_BASE}/api/data/current-values`,
  DATA_DEVICE_VALUES: (id: number | string) => `${API_BASE}/api/data/device/${id}/current-values`,
  DATA_HISTORICAL: `${API_BASE}/api/data/historical`,
  DATA_QUERY: `${API_BASE}/api/data/query`,
  DATA_EXPORT: `${API_BASE}/api/data/export`,
  DATA_STATISTICS: `${API_BASE}/api/data/statistics`,
  
  // ==========================================================================
  // 실시간 데이터 API
  // ==========================================================================
  REALTIME_CURRENT_VALUES: `${API_BASE}/api/realtime/current-values`,
  REALTIME_DEVICE_VALUES: (id: number | string) => `${API_BASE}/api/realtime/device/${id}/values`,
  REALTIME_SUBSCRIBE: `${API_BASE}/api/realtime/subscribe`,
  REALTIME_UNSUBSCRIBE: (id: string) => `${API_BASE}/api/realtime/subscribe/${id}`,
  REALTIME_SUBSCRIPTIONS: `${API_BASE}/api/realtime/subscriptions`,
  REALTIME_POLL: (id: string) => `${API_BASE}/api/realtime/poll/${id}`,
  REALTIME_STATS: `${API_BASE}/api/realtime/stats`,
  
  // ==========================================================================
  // 완전한 알람 관리 API
  // ==========================================================================
  
  // ---- 알람 발생 (Alarm Occurrences) ----
  ALARMS_ACTIVE: `${API_BASE}/api/alarms/active`,
  ALARMS_OCCURRENCES: `${API_BASE}/api/alarms/occurrences`,
  ALARMS_OCCURRENCE_BY_ID: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}`,
  ALARMS_OCCURRENCE_ACKNOWLEDGE: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}/acknowledge`,
  ALARMS_OCCURRENCE_CLEAR: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}/clear`,
  ALARMS_HISTORY: `${API_BASE}/api/alarms/history`,
  ALARM_TODAY: `${API_BASE}/api/alarms/today`,
  ALARM_TODAY_STATISTICS: `${API_BASE}/api/alarms/statistics/today`,
  
  // ---- 카테고리/태그별 알람 발생 ----
  ALARMS_OCCURRENCES_CATEGORY: (category: string) => `${API_BASE}/api/alarms/occurrences/category/${category}`,
  ALARMS_OCCURRENCES_TAG: (tag: string) => `${API_BASE}/api/alarms/occurrences/tag/${tag}`,
  
  // ---- 알람 규칙 (Alarm Rules) ----
  ALARM_RULES: `${API_BASE}/api/alarms/rules`,
  ALARM_RULE_BY_ID: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}`,
  ALARM_RULES_STATISTICS: `${API_BASE}/api/alarms/rules/statistics`,
  ALARM_RULE_SETTINGS: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/settings`,
  
  // 간단한 업데이트 엔드포인트들
  ALARM_RULE_TOGGLE: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/toggle`,
  ALARM_RULE_NAME: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/name`,
  ALARM_RULE_SEVERITY: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/severity`,
  
  // ---- 카테고리/태그별 알람 규칙 ----
  ALARM_RULES_CATEGORY: (category: string) => `${API_BASE}/api/alarms/rules/category/${category}`,
  ALARM_RULES_TAG: (tag: string) => `${API_BASE}/api/alarms/rules/tag/${tag}`,
  
  // ---- 알람 템플릿 (Alarm Templates) ----
  ALARM_TEMPLATES: `${API_BASE}/api/alarms/templates`,
  ALARM_TEMPLATE_BY_ID: (id: number | string) => `${API_BASE}/api/alarms/templates/${id}`,
  ALARM_TEMPLATES_CATEGORY: (category: string) => `${API_BASE}/api/alarms/templates/category/${category}`,
  ALARM_TEMPLATES_SYSTEM: `${API_BASE}/api/alarms/templates/system`,
  ALARM_TEMPLATES_DATA_TYPE: (dataType: string) => `${API_BASE}/api/alarms/templates/data-type/${dataType}`,
  ALARM_TEMPLATE_APPLY: (id: number | string) => `${API_BASE}/api/alarms/templates/${id}/apply`,
  ALARM_TEMPLATE_APPLIED_RULES: (id: number | string) => `${API_BASE}/api/alarms/templates/${id}/applied-rules`,
  ALARM_TEMPLATES_STATISTICS: `${API_BASE}/api/alarms/templates/statistics`,
  ALARM_TEMPLATES_SEARCH: `${API_BASE}/api/alarms/templates/search`,
  ALARM_TEMPLATES_MOST_USED: `${API_BASE}/api/alarms/templates/most-used`,
  
  // ---- 태그별 알람 템플릿 ----
  ALARM_TEMPLATES_TAG: (tag: string) => `${API_BASE}/api/alarms/templates/tag/${tag}`,
  
  // ---- 특화 알람 엔드포인트들 ----
  ALARM_STATISTICS: `${API_BASE}/api/alarms/statistics`,
  ALARM_UNACKNOWLEDGED: `${API_BASE}/api/alarms/unacknowledged`,
  ALARM_RECENT: `${API_BASE}/api/alarms/recent`,
  ALARM_DEVICE: (deviceId: number | string) => `${API_BASE}/api/alarms/device/${deviceId}`,
  ALARM_TEST: `${API_BASE}/api/alarms/test`,
  
  // ---- 기존 호환성 엔드포인트들 ----
  ALARM_BY_ID: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}`,
  ALARM_ACKNOWLEDGE: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}/acknowledge`,
  ALARM_CLEAR: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}/clear`,
  
  // ==========================================================================
  // 대시보드 API
  // ==========================================================================
  DASHBOARD_OVERVIEW: `${API_BASE}/api/dashboard/overview`,
  DASHBOARD_TENANT_STATS: `${API_BASE}/api/dashboard/tenant-stats`,
  DASHBOARD_RECENT_DEVICES: `${API_BASE}/api/dashboard/recent-devices`,
  DASHBOARD_SYSTEM_HEALTH: `${API_BASE}/api/dashboard/system-health`,
  DASHBOARD_SERVICE_CONTROL: (name: string, action: string) => 
    `${API_BASE}/api/services/${name}/${action}`,  // 수정됨
  DASHBOARD_SERVICES_STATUS: `${API_BASE}/api/dashboard/services/status`,
  
  // ==========================================================================
  // 사용자 관리 API
  // ==========================================================================
  USERS: `${API_BASE}/api/users`,
  USER_BY_ID: (id: number | string) => `${API_BASE}/api/users/${id}`,
  USER_LOGIN: `${API_BASE}/api/users/login`,
  USER_LOGOUT: `${API_BASE}/api/users/logout`,
  USER_PERMISSIONS: (id: number | string) => `${API_BASE}/api/users/${id}/permissions`,
  USER_LOGIN_HISTORY: `${API_BASE}/api/users/login-history`,
  
  // ==========================================================================
  // 가상포인트 관리 API
  // ==========================================================================
  VIRTUAL_POINTS: `${API_BASE}/api/virtual-points`,
  VIRTUAL_POINT_BY_ID: (id: number | string) => `${API_BASE}/api/virtual-points/${id}`,
  VIRTUAL_POINT_TEST: (id: number | string) => `${API_BASE}/api/virtual-points/${id}/test`,
  VIRTUAL_POINT_EXECUTE: (id: number | string) => `${API_BASE}/api/virtual-points/${id}/execute`,
  VIRTUAL_POINT_DEPENDENCIES: (id: number | string) => `${API_BASE}/api/virtual-points/${id}/dependencies`,
  VIRTUAL_POINT_HISTORY: (id: number | string) => `${API_BASE}/api/virtual-points/${id}/history`,
  VIRTUAL_POINT_VALUE: (id: number | string) => `${API_BASE}/api/virtual-points/${id}/value`,
  VIRTUAL_POINTS_STATS_CATEGORY: `${API_BASE}/api/virtual-points/stats/category`,
  VIRTUAL_POINTS_STATS_PERFORMANCE: `${API_BASE}/api/virtual-points/stats/performance`,
  
  // ==========================================================================
  // WebSocket 엔드포인트
  // ==========================================================================
  WS_REALTIME: '/ws/realtime',
  
  // ==========================================================================
  // Redis 데이터 API
  // ==========================================================================
  REDIS_STATUS: `${API_BASE}/api/redis/status`,
  REDIS_STATS: `${API_BASE}/api/redis/stats`,
  REDIS_INFO: `${API_BASE}/api/redis/info`,
  REDIS_TREE: `${API_BASE}/api/redis/tree`,
  REDIS_TREE_CHILDREN: (nodeId: string) => `${API_BASE}/api/redis/tree/${nodeId}/children`,
  REDIS_KEYS_SEARCH: `${API_BASE}/api/redis/keys/search`,
  REDIS_KEY_DATA: (key: string) => `${API_BASE}/api/redis/keys/${encodeURIComponent(key)}/data`,
  REDIS_KEYS_BULK: `${API_BASE}/api/redis/keys/bulk`,
  REDIS_KEY_DELETE: (key: string) => `${API_BASE}/api/redis/keys/${encodeURIComponent(key)}`,
  REDIS_KEY_TTL: (key: string) => `${API_BASE}/api/redis/keys/${encodeURIComponent(key)}/ttl`,
  REDIS_PATTERNS: `${API_BASE}/api/redis/patterns`,
  REDIS_TENANT_KEYS: `${API_BASE}/api/redis/tenants/keys`,
  REDIS_EXPORT: `${API_BASE}/api/redis/export`,
  REDIS_TEST: `${API_BASE}/api/redis/test`,
  REDIS_PING: `${API_BASE}/api/redis/ping`,
  REDIS_CONNECTION_CHECK: `${API_BASE}/api/redis-connection-check`,
  
  // ==========================================================================
  // 초기화 및 설정 API
  // ==========================================================================
  INIT_STATUS: `${API_BASE}/api/init/status`,
  INIT_TRIGGER: `${API_BASE}/api/init/trigger`,
  INIT_MANUAL: `${API_BASE}/api/init/manual`,
  
  // ==========================================================================
  // 모니터링 API
  // ==========================================================================
  MONITORING_SERVICE_HEALTH: `${API_BASE}/api/monitoring/service-health`,
  MONITORING_SYSTEM_METRICS: `${API_BASE}/api/monitoring/system-metrics`,
  MONITORING_DATABASE_STATS: `${API_BASE}/api/monitoring/database-stats`,
  MONITORING_PERFORMANCE: `${API_BASE}/api/monitoring/performance`,
  MONITORING_LOGS: `${API_BASE}/api/monitoring/logs`,
  
  // ==========================================================================
  // 사이트 관리
  // ==========================================================================
  SITES: `${API_BASE}/api/sites`,
  SITE_BY_ID: (id: number | string) => `${API_BASE}/api/sites/${id}`,
  SITE_DEVICES: (id: number | string) => `${API_BASE}/api/sites/${id}/devices`,
  SITE_STATISTICS: (id: number | string) => `${API_BASE}/api/sites/${id}/statistics`,
  
  // ==========================================================================
  // 백업/복원
  // ==========================================================================
  BACKUP_LIST: `${API_BASE}/api/backup/list`,
  BACKUP_CREATE: `${API_BASE}/api/backup/create`,
  BACKUP_RESTORE: (id: number | string) => `${API_BASE}/api/backup/restore/${id}`,
  BACKUP_DELETE: (id: number | string) => `${API_BASE}/api/backup/${id}`,
  BACKUP_STATUS: (id: number | string) => `${API_BASE}/api/backup/${id}/status`,
  BACKUP_SCHEDULE: `${API_BASE}/api/backup/schedule`,
  
  // ==========================================================================
  // 네트워크 설정
  // ==========================================================================
  NETWORK_SETTINGS: `${API_BASE}/api/network/settings`,
  NETWORK_INTERFACES: `${API_BASE}/api/network/interfaces`,
  NETWORK_TEST: `${API_BASE}/api/network/test`,
  
  // ==========================================================================
  // 권한 관리
  // ==========================================================================
  PERMISSIONS: `${API_BASE}/api/permissions`,
  PERMISSION_ROLES: `${API_BASE}/api/permissions/roles`,
  PERMISSION_USERS: `${API_BASE}/api/permissions/users`,
  PERMISSION_ASSIGN: `${API_BASE}/api/permissions/assign`,
  
  // ==========================================================================
  // 로그 관리
  // ==========================================================================
  LOGS: `${API_BASE}/api/logs`,
  LOGS_DOWNLOAD: `${API_BASE}/api/logs/download`,
  LOGS_CLEAR: `${API_BASE}/api/logs/clear`,
  
  // ==========================================================================
  // 설정 관리
  // ==========================================================================
  CONFIG: `${API_BASE}/api/config`,
  CONFIG_UPDATE: `${API_BASE}/api/config/update`,
  CONFIG_RESET: `${API_BASE}/api/config/reset`,
  CONFIG_EXPORT: `${API_BASE}/api/config/export`,
  CONFIG_IMPORT: `${API_BASE}/api/config/import`
} as const;

// ==========================================================================
// URL 빌더 유틸리티 함수들 - protocol_id 지원 추가
// ==========================================================================

/**
 * 서비스 제어 URL 빌더
 */
export function buildServiceControlUrl(serviceName: string, action: 'start' | 'stop' | 'restart'): string {
  return `${API_BASE}/api/services/${serviceName}/${action}`;
}

/**
 * 디바이스 목록 조회 URL - protocol_id 필터 지원
 */
export function buildDevicesListUrl(params?: {
  page?: number;
  limit?: number;
  search?: string;
  protocol_type?: string;    // 백워드 호환성
  protocol_id?: number;      // 새로 추가 - ID로 필터링
  device_type?: string;
  connection_status?: string;
  status?: string;
  site_id?: number;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
  include_rtu_relations?: boolean;
}): string {
  return buildUrlWithParams(ENDPOINTS.DEVICES, params);
}

/**
 * 디바이스 생성/수정용 데이터 유효성 검사
 */
export function validateDeviceData(data: {
  name?: string;
  protocol_id?: number;      // protocol_type 대신 protocol_id 사용
  endpoint?: string;
  [key: string]: any;
}): { isValid: boolean; errors: string[] } {
  const errors: string[] = [];
  
  if (data.name !== undefined && (!data.name || data.name.trim().length === 0)) {
    errors.push('Device name is required');
  }
  
  if (data.protocol_id !== undefined && (!data.protocol_id || typeof data.protocol_id !== 'number' || data.protocol_id < 1)) {
    errors.push('Valid protocol_id is required');
  }
  
  if (data.endpoint !== undefined && (!data.endpoint || data.endpoint.trim().length === 0)) {
    errors.push('Endpoint is required');
  }
  
  return {
    isValid: errors.length === 0,
    errors
  };
}

/**
 * protocol_type을 protocol_id로 변환하는 헬퍼 (클라이언트 사이드)
 * ProtocolManager에서 사용
 */
export function convertProtocolTypeToId(protocolType: string, protocolList: Array<{ id: number; protocol_type: string }>): number | null {
  const protocol = protocolList.find(p => p.protocol_type === protocolType);
  return protocol ? protocol.id : null;
}

/**
 * protocol_id를 protocol_type으로 변환하는 헬퍼 (클라이언트 사이드)
 */
export function convertProtocolIdToType(protocolId: number, protocolList: Array<{ id: number; protocol_type: string }>): string | null {
  const protocol = protocolList.find(p => p.id === protocolId);
  return protocol ? protocol.protocol_type : null;
}

/**
 * RTU 네트워크 상태 조회 URL 빌더
 */
export function buildRtuNetworkStatusUrl(serialPort: string): string {
  return ENDPOINTS.RTU_NETWORK_STATUS(serialPort);
}

/**
 * RTU 마스터의 슬래이브 목록 URL 빌더
 */
export function buildRtuMasterSlavesUrl(masterId: number | string, params?: {
  include_status?: boolean;
  include_data_points?: boolean;
}): string {
  return buildUrlWithParams(ENDPOINTS.RTU_MASTER_SLAVES(masterId), params);
}

/**
 * RTU 슬래이브 상태 조회 URL 빌더
 */
export function buildRtuSlaveStatusUrl(slaveId: number | string, params?: {
  include_master_info?: boolean;
  include_communication_stats?: boolean;
}): string {
  return buildUrlWithParams(ENDPOINTS.RTU_SLAVE_STATUS(slaveId), params);
}

/**
 * RTU 네트워크 스캔 URL 빌더
 */
export function buildRtuNetworkScanUrl(params: {
  serial_port: string;
  start_slave_id?: number;
  end_slave_id?: number;
  timeout_ms?: number;
  baud_rate?: number;
}): string {
  return buildUrlWithParams(ENDPOINTS.RTU_NETWORK_SCAN, params);
}

/**
 * 디바이스 상세 조회 URL (RTU 네트워크 정보 포함 옵션)
 */
export function buildDeviceDetailUrl(deviceId: number | string, params?: {
  include_data_points?: boolean;
  include_rtu_network?: boolean;
}): string {
  return buildUrlWithParams(ENDPOINTS.DEVICE_BY_ID(deviceId), params);
}

/**
 * 쿼리 파라미터를 URL에 추가하는 헬퍼 함수
 */
export function buildUrlWithParams(endpoint: string, params?: Record<string, any>): string {
  if (!params || Object.keys(params).length === 0) {
    return endpoint;
  }
  
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
  return queryString ? `${endpoint}?${queryString}` : endpoint;
}

/**
 * 페이징 파라미터를 URL에 추가하는 헬퍼 함수
 */
export function buildPaginatedUrl(endpoint: string, params: {
  page?: number;
  limit?: number;
  [key: string]: any;
}): string {
  const { page = 1, limit = 25, ...otherParams } = params;
  
  return buildUrlWithParams(endpoint, {
    page,
    limit,
    ...otherParams
  });
}

/**
 * 정렬 파라미터를 URL에 추가하는 헬퍼 함수
 */
export function buildSortedUrl(endpoint: string, params: {
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
  [key: string]: any;
}): string {
  const { sort_by = 'id', sort_order = 'ASC', ...otherParams } = params;
  
  return buildUrlWithParams(endpoint, {
    sort_by,
    sort_order,
    ...otherParams
  });
}

/**
 * 알람 관련 검색 URL 빌더
 */
export function buildAlarmSearchUrl(baseEndpoint: string, params: {
  search?: string;
  severity?: string;
  state?: string;
  device_id?: number;
  date_from?: string;
  date_to?: string;
  page?: number;
  limit?: number;
  category?: string;
  tag?: string;
}): string {
  return buildUrlWithParams(baseEndpoint, params);
}

/**
 * 템플릿 적용 URL 빌더
 */
export function buildTemplateApplyUrl(templateId: number | string, params: {
  target_ids: number[];
  target_type?: string;
  custom_configs?: Record<string, any>;
  rule_group_name?: string;
}): string {
  return ENDPOINTS.ALARM_TEMPLATE_APPLY(templateId);
}

/**
 * 카테고리별 알람 URL 빌더
 */
export function buildAlarmCategoryUrl(category: string, type: 'rules' | 'occurrences' | 'templates'): string {
  switch (type) {
    case 'rules':
      return ENDPOINTS.ALARM_RULES_CATEGORY(category);
    case 'occurrences':
      return ENDPOINTS.ALARMS_OCCURRENCES_CATEGORY(category);
    case 'templates':
      return ENDPOINTS.ALARM_TEMPLATES_CATEGORY(category);
    default:
      throw new Error(`Unknown alarm type: ${type}`);
  }
}

/**
 * 태그별 알람 URL 빌더
 */
export function buildAlarmTagUrl(tag: string, type: 'rules' | 'occurrences' | 'templates'): string {
  switch (type) {
    case 'rules':
      return ENDPOINTS.ALARM_RULES_TAG(tag);
    case 'occurrences':
      return ENDPOINTS.ALARMS_OCCURRENCES_TAG(tag);
    case 'templates':
      return ENDPOINTS.ALARM_TEMPLATES_TAG(tag);
    default:
      throw new Error(`Unknown alarm type: ${type}`);
  }
}

/**
 * 알람 규칙 토글 URL 빌더
 */
export function buildAlarmToggleUrl(ruleId: number | string): string {
  return ENDPOINTS.ALARM_RULE_TOGGLE(ruleId);
}

/**
 * 알람 규칙 간단 업데이트 URL 빌더
 */
export function buildAlarmSimpleUpdateUrl(ruleId: number | string, updateType: 'settings' | 'name' | 'severity'): string {
  switch (updateType) {
    case 'settings':
      return ENDPOINTS.ALARM_RULE_SETTINGS(ruleId);
    case 'name':
      return ENDPOINTS.ALARM_RULE_NAME(ruleId);
    case 'severity':
      return ENDPOINTS.ALARM_RULE_SEVERITY(ruleId);
    default:
      throw new Error(`Unknown update type: ${updateType}`);
  }
}

/**
 * WebSocket URL 생성 헬퍼 함수
 */
export function buildWebSocketUrl(baseUrl: string, endpoint: string, params?: Record<string, any>): string {
  const wsBaseUrl = baseUrl.replace(/^http/, 'ws');
  const fullEndpoint = buildUrlWithParams(endpoint, params);
  return `${wsBaseUrl}${fullEndpoint}`;
}

// ==========================================================================
// 타입 안전성을 위한 엔드포인트 그룹화
// ==========================================================================

export const API_GROUPS = {
  SYSTEM: {
    STATUS: ENDPOINTS.SYSTEM_STATUS,
    INFO: ENDPOINTS.SYSTEM_INFO,
    DATABASES: ENDPOINTS.SYSTEM_DATABASES,
    HEALTH: ENDPOINTS.SYSTEM_HEALTH
  },
  
  SERVICES: {
    LIST: ENDPOINTS.SERVICES,
    DETAIL: ENDPOINTS.SERVICE_BY_NAME,
    COLLECTOR: {
      START: ENDPOINTS.COLLECTOR_START,
      STOP: ENDPOINTS.COLLECTOR_STOP,
      RESTART: ENDPOINTS.COLLECTOR_RESTART,
      STATUS: ENDPOINTS.COLLECTOR_STATUS
    },
    REDIS: {
      START: ENDPOINTS.REDIS_START,
      STOP: ENDPOINTS.REDIS_STOP,
      RESTART: ENDPOINTS.REDIS_RESTART,
      STATUS: ENDPOINTS.REDIS_SERVICE_STATUS
    },
    HEALTH_CHECK: ENDPOINTS.SERVICES_HEALTH_CHECK,
    SYSTEM_INFO: ENDPOINTS.SERVICES_SYSTEM_INFO,
    PROCESSES: ENDPOINTS.SERVICES_PROCESSES,
    PLATFORM: ENDPOINTS.SERVICES_PLATFORM_INFO
  },
  
  DEVICES: {
    LIST: ENDPOINTS.DEVICES,
    DETAIL: ENDPOINTS.DEVICE_BY_ID,
    DATA_POINTS: ENDPOINTS.DEVICE_DATA_POINTS,
    TEST_CONNECTION: ENDPOINTS.DEVICE_TEST_CONNECTION,
    ENABLE: ENDPOINTS.DEVICE_ENABLE,
    DISABLE: ENDPOINTS.DEVICE_DISABLE,
    RESTART: ENDPOINTS.DEVICE_RESTART,
    PROTOCOLS: ENDPOINTS.DEVICE_PROTOCOLS,  // ID 정보 포함
    STATISTICS: ENDPOINTS.DEVICE_STATISTICS,
    BULK_ACTION: ENDPOINTS.DEVICE_BULK_ACTION,
    
    // RTU 전용 엔드포인트 그룹
    RTU: {
      NETWORKS: ENDPOINTS.RTU_NETWORKS,
      NETWORK_BY_ID: ENDPOINTS.RTU_NETWORK_BY_ID,
      MASTER_SLAVES: ENDPOINTS.RTU_MASTER_SLAVES,
      SLAVE_STATUS: ENDPOINTS.RTU_SLAVE_STATUS,
      NETWORK_SCAN: ENDPOINTS.RTU_NETWORK_SCAN,
      NETWORK_STATUS: ENDPOINTS.RTU_NETWORK_STATUS
    },
    
    // 디버깅 API
    DEBUG: {
      DIRECT: ENDPOINTS.DEBUG_DEVICES_DIRECT,
      REPOSITORY: ENDPOINTS.DEBUG_REPOSITORY
    }
  },
  
  DATA: {
    POINTS: ENDPOINTS.DATA_POINTS,
    POINT_DETAIL: ENDPOINTS.DATA_POINT_BY_ID,
    CURRENT_VALUES: ENDPOINTS.DATA_CURRENT_VALUES,
    DEVICE_VALUES: ENDPOINTS.DATA_DEVICE_VALUES,
    HISTORICAL: ENDPOINTS.DATA_HISTORICAL,
    QUERY: ENDPOINTS.DATA_QUERY,
    EXPORT: ENDPOINTS.DATA_EXPORT,
    STATISTICS: ENDPOINTS.DATA_STATISTICS
  },
  
  REALTIME: {
    CURRENT_VALUES: ENDPOINTS.REALTIME_CURRENT_VALUES,
    DEVICE_VALUES: ENDPOINTS.REALTIME_DEVICE_VALUES,
    SUBSCRIBE: ENDPOINTS.REALTIME_SUBSCRIBE,
    UNSUBSCRIBE: ENDPOINTS.REALTIME_UNSUBSCRIBE,
    SUBSCRIPTIONS: ENDPOINTS.REALTIME_SUBSCRIPTIONS,
    POLL: ENDPOINTS.REALTIME_POLL,
    STATS: ENDPOINTS.REALTIME_STATS
  },
  
  ALARMS: {
    // 알람 발생 관련
    ACTIVE: ENDPOINTS.ALARMS_ACTIVE,
    OCCURRENCES: ENDPOINTS.ALARMS_OCCURRENCES,
    OCCURRENCE_DETAIL: ENDPOINTS.ALARMS_OCCURRENCE_BY_ID,
    ACKNOWLEDGE: ENDPOINTS.ALARMS_OCCURRENCE_ACKNOWLEDGE,
    CLEAR: ENDPOINTS.ALARMS_OCCURRENCE_CLEAR,
    HISTORY: ENDPOINTS.ALARMS_HISTORY,
    UNACKNOWLEDGED: ENDPOINTS.ALARM_UNACKNOWLEDGED,
    RECENT: ENDPOINTS.ALARM_RECENT,
    DEVICE_ALARMS: ENDPOINTS.ALARM_DEVICE,
    
    // 카테고리/태그별 알람 발생
    OCCURRENCES_BY_CATEGORY: ENDPOINTS.ALARMS_OCCURRENCES_CATEGORY,
    OCCURRENCES_BY_TAG: ENDPOINTS.ALARMS_OCCURRENCES_TAG,
    
    // 알람 규칙 관련
    RULES: ENDPOINTS.ALARM_RULES,
    RULE_DETAIL: ENDPOINTS.ALARM_RULE_BY_ID,
    RULES_STATISTICS: ENDPOINTS.ALARM_RULES_STATISTICS,
    RULE_SETTINGS: ENDPOINTS.ALARM_RULE_SETTINGS,
    
    // 간단한 업데이트 엔드포인트들
    RULE_TOGGLE: ENDPOINTS.ALARM_RULE_TOGGLE,
    RULE_NAME: ENDPOINTS.ALARM_RULE_NAME,
    RULE_SEVERITY: ENDPOINTS.ALARM_RULE_SEVERITY,
    
    // 카테고리/태그별 알람 규칙
    RULES_BY_CATEGORY: ENDPOINTS.ALARM_RULES_CATEGORY,
    RULES_BY_TAG: ENDPOINTS.ALARM_RULES_TAG,
    
    // 알람 템플릿 관련
    TEMPLATES: ENDPOINTS.ALARM_TEMPLATES,
    TEMPLATE_DETAIL: ENDPOINTS.ALARM_TEMPLATE_BY_ID,
    TEMPLATES_CATEGORY: ENDPOINTS.ALARM_TEMPLATES_CATEGORY,
    TEMPLATES_SYSTEM: ENDPOINTS.ALARM_TEMPLATES_SYSTEM,
    TEMPLATES_DATA_TYPE: ENDPOINTS.ALARM_TEMPLATES_DATA_TYPE,
    TEMPLATE_APPLY: ENDPOINTS.ALARM_TEMPLATE_APPLY,
    TEMPLATE_APPLIED_RULES: ENDPOINTS.ALARM_TEMPLATE_APPLIED_RULES,
    TEMPLATES_STATISTICS: ENDPOINTS.ALARM_TEMPLATES_STATISTICS,
    TEMPLATES_SEARCH: ENDPOINTS.ALARM_TEMPLATES_SEARCH,
    TEMPLATES_MOST_USED: ENDPOINTS.ALARM_TEMPLATES_MOST_USED,
    TEMPLATES_BY_TAG: ENDPOINTS.ALARM_TEMPLATES_TAG,
    
    // 통계 및 기타
    STATISTICS: ENDPOINTS.ALARM_STATISTICS,
    TEST: ENDPOINTS.ALARM_TEST,
    TODAY: ENDPOINTS.ALARM_TODAY,
    TODAY_STATISTICS: ENDPOINTS.ALARM_TODAY_STATISTICS
  },
  
  VIRTUAL_POINTS: {
    LIST: ENDPOINTS.VIRTUAL_POINTS,
    DETAIL: ENDPOINTS.VIRTUAL_POINT_BY_ID,
    TEST: ENDPOINTS.VIRTUAL_POINT_TEST,
    EXECUTE: ENDPOINTS.VIRTUAL_POINT_EXECUTE,
    DEPENDENCIES: ENDPOINTS.VIRTUAL_POINT_DEPENDENCIES,
    HISTORY: ENDPOINTS.VIRTUAL_POINT_HISTORY,
    VALUE: ENDPOINTS.VIRTUAL_POINT_VALUE,
    STATS_CATEGORY: ENDPOINTS.VIRTUAL_POINTS_STATS_CATEGORY,
    STATS_PERFORMANCE: ENDPOINTS.VIRTUAL_POINTS_STATS_PERFORMANCE
  },
  
  DASHBOARD: {
    OVERVIEW: ENDPOINTS.DASHBOARD_OVERVIEW,
    TENANT_STATS: ENDPOINTS.DASHBOARD_TENANT_STATS,
    RECENT_DEVICES: ENDPOINTS.DASHBOARD_RECENT_DEVICES,
    SYSTEM_HEALTH: ENDPOINTS.DASHBOARD_SYSTEM_HEALTH,
    SERVICE_CONTROL: ENDPOINTS.DASHBOARD_SERVICE_CONTROL,
    SERVICES_STATUS: ENDPOINTS.DASHBOARD_SERVICES_STATUS
  }
} as const;

// ==========================================================================
// 엔드포인트 검증 및 유틸리티
// ==========================================================================

/**
 * 엔드포인트가 유효한지 확인하는 함수
 */
export function isValidEndpoint(endpoint: string): boolean {
  return Object.values(ENDPOINTS).includes(endpoint as any) ||
         Object.values(ENDPOINTS).some(value => 
           typeof value === 'function' || endpoint.startsWith('/api/')
         );
}

/**
 * RTU 관련 엔드포인트만 가져오는 함수
 */
export function getRtuEndpoints(): Record<string, string | Function> {
  return API_GROUPS.DEVICES.RTU;
}

/**
 * API 그룹별로 엔드포인트를 가져오는 함수
 */
export function getEndpointsByGroup(group: keyof typeof API_GROUPS): Record<string, any> {
  return API_GROUPS[group];
}

/**
 * 모든 엔드포인트 목록을 가져오는 함수
 */
export function getAllEndpoints(): string[] {
  return Object.values(ENDPOINTS).filter(value => typeof value === 'string');
}

// 환경별 설정
export const API_CONFIG = {
  development: {
    baseUrl: 'http://localhost:3000',
    timeout: 10000,
    retries: 3
  },
  production: {
    baseUrl: window?.location?.origin || 'https://api.pulseone.com',
    timeout: 15000,
    retries: 2
  },
  test: {
    baseUrl: 'http://localhost:3000',
    timeout: 5000,
    retries: 1
  }
};

// 현재 환경 설정
const getCurrentConfig = () => {
  const env = (typeof process !== 'undefined' ? process.env.NODE_ENV : 'development') as keyof typeof API_CONFIG;
  return API_CONFIG[env] || API_CONFIG.development;
};

export const currentApiConfig = getCurrentConfig();

// HTTP 상태 코드 상수
export const HTTP_STATUS = {
  OK: 200,
  CREATED: 201,
  NO_CONTENT: 204,
  BAD_REQUEST: 400,
  UNAUTHORIZED: 401,
  FORBIDDEN: 403,
  NOT_FOUND: 404,
  CONFLICT: 409,
  INTERNAL_SERVER_ERROR: 500,
  SERVICE_UNAVAILABLE: 503
} as const;

// 에러 코드 상수 - protocol_id 관련 추가
export const ERROR_CODES = {
  // 네트워크 관련
  NETWORK_ERROR: 'NETWORK_ERROR',
  TIMEOUT_ERROR: 'TIMEOUT_ERROR',
  
  // 인증 관련
  UNAUTHORIZED: 'UNAUTHORIZED',
  FORBIDDEN: 'FORBIDDEN',
  
  // 데이터 관련
  NOT_FOUND: 'NOT_FOUND',
  VALIDATION_ERROR: 'VALIDATION_ERROR',
  DUPLICATE_ERROR: 'DUPLICATE_ERROR',
  
  // 서비스 관련
  SERVICE_CONTROL_ERROR: 'SERVICE_CONTROL_ERROR',
  SERVICE_NOT_FOUND: 'SERVICE_NOT_FOUND',
  SERVICE_ALREADY_RUNNING: 'SERVICE_ALREADY_RUNNING',
  SERVICE_NOT_RUNNING: 'SERVICE_NOT_RUNNING',
  
  // 디바이스 및 프로토콜 관련 에러 코드들
  DEVICE_NOT_FOUND: 'DEVICE_NOT_FOUND',
  DEVICE_NAME_CONFLICT: 'DEVICE_NAME_CONFLICT',
  DEVICE_CREATE_ERROR: 'DEVICE_CREATE_ERROR',
  DEVICE_UPDATE_ERROR: 'DEVICE_UPDATE_ERROR',
  DEVICE_DELETE_ERROR: 'DEVICE_DELETE_ERROR',
  PROTOCOL_ID_REQUIRED: 'PROTOCOL_ID_REQUIRED',          // 새로 추가
  INVALID_PROTOCOL_ID: 'INVALID_PROTOCOL_ID',            // 새로 추가
  PROTOCOL_ID_NOT_FOUND: 'PROTOCOL_ID_NOT_FOUND',        // 새로 추가
  UNSUPPORTED_PROTOCOL: 'UNSUPPORTED_PROTOCOL',
  
  // RTU 관련 에러 코드들
  RTU_NETWORK_ERROR: 'RTU_NETWORK_ERROR',
  RTU_MASTER_NOT_FOUND: 'RTU_MASTER_NOT_FOUND',
  RTU_SLAVE_NOT_FOUND: 'RTU_SLAVE_NOT_FOUND',
  RTU_COMMUNICATION_ERROR: 'RTU_COMMUNICATION_ERROR',
  RTU_SCAN_TIMEOUT: 'RTU_SCAN_TIMEOUT',
  RTU_SERIAL_PORT_ERROR: 'RTU_SERIAL_PORT_ERROR',
  RTU_BAUD_RATE_ERROR: 'RTU_BAUD_RATE_ERROR',
  RTU_SLAVE_ID_CONFLICT: 'RTU_SLAVE_ID_CONFLICT',
  RTU_NETWORK_STATUS_ERROR: 'RTU_NETWORK_STATUS_ERROR',
  
  // 알람 관련 에러 코드들
  ALARM_NOT_FOUND: 'ALARM_NOT_FOUND',
  ALARM_RULE_NOT_FOUND: 'ALARM_RULE_NOT_FOUND',
  ALARM_TEMPLATE_NOT_FOUND: 'ALARM_TEMPLATE_NOT_FOUND',
  ALARM_ALREADY_ACKNOWLEDGED: 'ALARM_ALREADY_ACKNOWLEDGED',
  ALARM_ALREADY_CLEARED: 'ALARM_ALREADY_CLEARED',
  ALARM_RULES_ERROR: 'ALARM_RULES_ERROR',
  ALARM_RULE_CREATE_ERROR: 'ALARM_RULE_CREATE_ERROR',
  ALARM_RULE_UPDATE_ERROR: 'ALARM_RULE_UPDATE_ERROR',
  ALARM_RULE_DELETE_ERROR: 'ALARM_RULE_DELETE_ERROR',
  ALARM_RULE_DETAIL_ERROR: 'ALARM_RULE_DETAIL_ERROR',
  ALARM_RULE_TOGGLE_ERROR: 'ALARM_RULE_TOGGLE_ERROR',
  ALARM_RULE_SETTINGS_ERROR: 'ALARM_RULE_SETTINGS_ERROR',
  ALARM_OCCURRENCE_ERROR: 'ALARM_OCCURRENCE_ERROR',
  ALARM_ACKNOWLEDGE_ERROR: 'ALARM_ACKNOWLEDGE_ERROR',
  ALARM_CLEAR_ERROR: 'ALARM_CLEAR_ERROR',
  ALARM_TEMPLATE_CREATE_ERROR: 'ALARM_TEMPLATE_CREATE_ERROR',
  ALARM_TEMPLATE_UPDATE_ERROR: 'ALARM_TEMPLATE_UPDATE_ERROR',
  ALARM_TEMPLATE_DELETE_ERROR: 'ALARM_TEMPLATE_DELETE_ERROR',
  TEMPLATE_APPLY_ERROR: 'TEMPLATE_APPLY_ERROR',
  TEMPLATE_NOT_FOUND: 'TEMPLATE_NOT_FOUND',
  SEARCH_TERM_REQUIRED: 'SEARCH_TERM_REQUIRED',
  SETTINGS_UPDATE_ERROR: 'SETTINGS_UPDATE_ERROR',
  ACTIVE_ALARMS_ERROR: 'ACTIVE_ALARMS_ERROR',
  ALARM_STATS_ERROR: 'ALARM_STATS_ERROR',
  CATEGORY_ALARM_RULES_ERROR: 'CATEGORY_ALARM_RULES_ERROR',
  TAG_ALARM_RULES_ERROR: 'TAG_ALARM_RULES_ERROR',
  CATEGORY_ALARM_OCCURRENCES_ERROR: 'CATEGORY_ALARM_OCCURRENCES_ERROR',
  TAG_ALARM_OCCURRENCES_ERROR: 'TAG_ALARM_OCCURRENCES_ERROR',
  CATEGORY_TEMPLATES_ERROR: 'CATEGORY_TEMPLATES_ERROR',
  TAG_TEMPLATES_ERROR: 'TAG_TEMPLATES_ERROR',
  TEST_ERROR: 'TEST_ERROR',
  
  // 서버 관련
  INTERNAL_SERVER_ERROR: 'INTERNAL_SERVER_ERROR',
  SERVICE_UNAVAILABLE: 'SERVICE_UNAVAILABLE'
} as const;

// 알람 관련 상수들
export const ALARM_CONSTANTS = {
  SEVERITIES: ['critical', 'high', 'medium', 'low', 'info'] as const,
  STATES: ['active', 'acknowledged', 'cleared'] as const,
  ALARM_TYPES: ['analog', 'digital', 'script'] as const,
  TARGET_TYPES: ['device', 'data_point', 'virtual_point'] as const,
  TEMPLATE_CATEGORIES: ['general', 'temperature', 'pressure', 'flow', 'level', 'vibration', 'electrical', 'safety'] as const,
  DATA_TYPES: ['number', 'boolean', 'string', 'object'] as const,
  
  DEFAULT_CATEGORIES: [
    'temperature',   'pressure',      'flow',          'level',
    'vibration',     'electrical',    'safety',        'general'
  ] as const,
  
  COMMON_TAGS: [
    'critical',      'maintenance',   'production',    'quality',
    'energy',        'efficiency',    'compliance',    'monitoring'
  ] as const,
  
  UPDATE_TYPES: [
    'toggle',        'settings',      'name',          'severity'
  ] as const
} as const;

// RTU 관련 상수들
export const RTU_CONSTANTS = {
  BAUD_RATES: [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200] as const,
  PARITY_OPTIONS: ['N', 'E', 'O'] as const, // None, Even, Odd
  DATA_BITS: [5, 6, 7, 8] as const,
  STOP_BITS: [1, 2] as const,
  SLAVE_ID_RANGE: { min: 1, max: 247 } as const,
  DEFAULT_TIMEOUTS: {
    response: 1000,    // ms
    byte: 100,         // ms
    frame_delay: 50    // ms
  } as const,
  SCAN_DEFAULTS: {
    start_slave_id: 1,
    end_slave_id: 247,
    timeout_ms: 2000
  } as const
} as const;

export type AlarmSeverity = typeof ALARM_CONSTANTS.SEVERITIES[number];
export type AlarmState = typeof ALARM_CONSTANTS.STATES[number];
export type AlarmType = typeof ALARM_CONSTANTS.ALARM_TYPES[number];
export type AlarmTargetType = typeof ALARM_CONSTANTS.TARGET_TYPES[number];
export type AlarmTemplateCategory = typeof ALARM_CONSTANTS.TEMPLATE_CATEGORIES[number];
export type AlarmDataType = typeof ALARM_CONSTANTS.DATA_TYPES[number];
export type AlarmDefaultCategory = typeof ALARM_CONSTANTS.DEFAULT_CATEGORIES[number];
export type AlarmCommonTag = typeof ALARM_CONSTANTS.COMMON_TAGS[number];
export type AlarmUpdateType = typeof ALARM_CONSTANTS.UPDATE_TYPES[number];

// RTU 관련 타입들
export type RtuBaudRate = typeof RTU_CONSTANTS.BAUD_RATES[number];
export type RtuParity = typeof RTU_CONSTANTS.PARITY_OPTIONS[number];
export type RtuDataBits = typeof RTU_CONSTANTS.DATA_BITS[number];
export type RtuStopBits = typeof RTU_CONSTANTS.STOP_BITS[number];

export default ENDPOINTS;