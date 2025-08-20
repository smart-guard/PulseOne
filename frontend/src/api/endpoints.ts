// ============================================================================
// frontend/src/api/endpoints.ts
// API 엔드포인트 상수 정의 - 완전한 백엔드 호환 버전
// ============================================================================

// React 환경에서 process.env 안전하게 접근
const getApiBase = () => {
  // 개발 환경에서는 기본값 사용
  if (typeof window !== 'undefined') {
    // 브라우저 환경
    return window.location.hostname === 'localhost' 
      ? 'http://localhost:3000'
      : 'https://api.pulseone.com';
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
  HEALTH: `${API_BASE}/health`,
  API_INFO: `${API_BASE}/api/info`,
  
  // ==========================================================================
  // 시스템 관리 API
  // ==========================================================================
  SYSTEM_STATUS: `${API_BASE}/api/system/status`,
  SYSTEM_INFO: `${API_BASE}/api/system/info`, 
  SYSTEM_DATABASES: `${API_BASE}/api/system/databases`,
  SYSTEM_HEALTH: `${API_BASE}/api/system/health`,
  
  // 서비스 제어
  SERVICES: `${API_BASE}/api/services`,
  SERVICE_BY_NAME: (name: string) => `${API_BASE}/api/services/${name}`,
  SERVICE_START: (name: string) => `${API_BASE}/api/services/${name}/start`,
  SERVICE_STOP: (name: string) => `${API_BASE}/api/services/${name}/stop`,
  SERVICE_RESTART: (name: string) => `${API_BASE}/api/services/${name}/restart`,
  
  // 프로세스 관리
  PROCESSES: `${API_BASE}/api/processes`,
  PROCESS_BY_ID: (id: number | string) => `${API_BASE}/api/processes/${id}`,
  
  // ==========================================================================
  // 디바이스 관리 API
  // ==========================================================================
  DEVICES: `${API_BASE}/api/devices`,
  DEVICE_BY_ID: (id: number | string) => `${API_BASE}/api/devices/${id}`,
  DEVICE_DATA_POINTS: (id: number | string) => `${API_BASE}/api/devices/${id}/data-points`,
  DEVICE_TEST_CONNECTION: (id: number | string) => `${API_BASE}/api/devices/${id}/test-connection`,
  DEVICE_ENABLE: (id: number | string) => `${API_BASE}/api/devices/${id}/enable`,
  DEVICE_DISABLE: (id: number | string) => `${API_BASE}/api/devices/${id}/disable`,
  DEVICE_RESTART: (id: number | string) => `${API_BASE}/api/devices/${id}/restart`,
  DEVICE_PROTOCOLS: `${API_BASE}/api/devices/protocols`,
  DEVICE_STATISTICS: `${API_BASE}/api/devices/statistics`,
  DEVICE_BULK_ACTION: `${API_BASE}/api/devices/bulk-action`,
  
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
  // 알람 관리 API - 백엔드 라우트와 완전 일치
  // ==========================================================================
  
  // 알람 발생 관련
  ALARMS_ACTIVE: `${API_BASE}/api/alarms/active`,
  ALARMS_OCCURRENCES: `${API_BASE}/api/alarms/occurrences`,
  ALARMS_OCCURRENCE_BY_ID: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}`,
  ALARMS_OCCURRENCE_ACKNOWLEDGE: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}/acknowledge`,
  ALARMS_OCCURRENCE_CLEAR: (id: number | string) => `${API_BASE}/api/alarms/occurrences/${id}/clear`,
  ALARMS_HISTORY: `${API_BASE}/api/alarms/history`,
  ALARM_UNACKNOWLEDGED: `${API_BASE}/api/alarms/unacknowledged`,
  ALARM_RECENT: `${API_BASE}/api/alarms/recent`,
  ALARM_DEVICE: (deviceId: number | string) => `${API_BASE}/api/alarms/device/${deviceId}`,
  ALARM_STATISTICS: `${API_BASE}/api/alarms/statistics`,
  
  // 알람 규칙 관련
  ALARM_RULES: `${API_BASE}/api/alarms/rules`,
  ALARM_RULE_BY_ID: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}`,
  ALARM_RULES_STATISTICS: `${API_BASE}/api/alarms/rules/statistics`,
  ALARM_RULE_SETTINGS: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/settings`,
  ALARM_RULES_BULK_UPDATE: `${API_BASE}/api/alarms/rules/bulk-update`,
  ALARM_RULE_STATISTICS: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/statistics`,
  ALARM_RULE_CONFIG_HISTORY: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/configuration-history`,
  ALARM_RULE_TEST: (id: number | string) => `${API_BASE}/api/alarms/rules/${id}/test`,
  
  // 알람 템플릿 관련
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
  
  // 호환성을 위한 기존 엔드포인트들
  ALARM_BY_ID: (id: number | string) => `${API_BASE}/api/alarms/${id}`,
  ALARM_ACKNOWLEDGE: (id: number | string) => `${API_BASE}/api/alarms/${id}/acknowledge`,
  ALARM_CLEAR: (id: number | string) => `${API_BASE}/api/alarms/${id}/clear`,
  ALARM_TEST: `${API_BASE}/api/alarms/test`,
  
  // ==========================================================================
  // 대시보드 API
  // ==========================================================================
  DASHBOARD_OVERVIEW: `${API_BASE}/api/dashboard/overview`,
  DASHBOARD_TENANT_STATS: `${API_BASE}/api/dashboard/tenant-stats`,
  DASHBOARD_RECENT_DEVICES: `${API_BASE}/api/dashboard/recent-devices`,
  DASHBOARD_SYSTEM_HEALTH: `${API_BASE}/api/dashboard/system-health`,
  DASHBOARD_SERVICE_CONTROL: (name: string) => `${API_BASE}/api/dashboard/service/${name}/control`,
  
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
  
  // ==========================================================================
  // 향후 추가될 API들
  // ==========================================================================
  MONITORING_SERVICE_HEALTH: `${API_BASE}/api/monitoring/service-health`,
  MONITORING_SYSTEM_METRICS: `${API_BASE}/api/monitoring/system-metrics`,
  MONITORING_DATABASE_STATS: `${API_BASE}/api/monitoring/database-stats`,
  MONITORING_PERFORMANCE: `${API_BASE}/api/monitoring/performance`,
  MONITORING_LOGS: `${API_BASE}/api/monitoring/logs`,
  
  // 가상포인트 관리
  VIRTUAL_POINTS: `${API_BASE}/api/virtual-points`,
  VIRTUAL_POINT_BY_ID: (id: number | string) => `${API_BASE}/api/virtual-points/${id}`,
  VIRTUAL_POINT_TEST: (id: number | string) => `${API_BASE}/api/virtual-points/${id}/test`,
  VIRTUAL_POINT_DEPENDENCIES: (id: number | string) => `${API_BASE}/api/virtual-points/${id}/dependencies`,
  
  // 사이트 관리
  SITES: `${API_BASE}/api/sites`,
  SITE_BY_ID: (id: number | string) => `${API_BASE}/api/sites/${id}`,
  SITE_DEVICES: (id: number | string) => `${API_BASE}/api/sites/${id}/devices`,
  SITE_STATISTICS: (id: number | string) => `${API_BASE}/api/sites/${id}/statistics`,
  
  // 백업/복원
  BACKUP_LIST: `${API_BASE}/api/backup/list`,
  BACKUP_CREATE: `${API_BASE}/api/backup/create`,
  BACKUP_RESTORE: (id: number | string) => `${API_BASE}/api/backup/restore/${id}`,
  BACKUP_DELETE: (id: number | string) => `${API_BASE}/api/backup/${id}`,
  BACKUP_STATUS: (id: number | string) => `${API_BASE}/api/backup/${id}/status`,
  BACKUP_SCHEDULE: `${API_BASE}/api/backup/schedule`,
  
  // 네트워크 설정
  NETWORK_SETTINGS: `${API_BASE}/api/network/settings`,
  NETWORK_INTERFACES: `${API_BASE}/api/network/interfaces`,
  NETWORK_TEST: `${API_BASE}/api/network/test`,
  
  // 권한 관리
  PERMISSIONS: `${API_BASE}/api/permissions`,
  PERMISSION_ROLES: `${API_BASE}/api/permissions/roles`,
  PERMISSION_USERS: `${API_BASE}/api/permissions/users`,
  PERMISSION_ASSIGN: `${API_BASE}/api/permissions/assign`,
  
  // 로그 관리
  LOGS: `${API_BASE}/api/logs`,
  LOGS_DOWNLOAD: `${API_BASE}/api/logs/download`,
  LOGS_CLEAR: `${API_BASE}/api/logs/clear`,
  
  // 설정 관리
  CONFIG: `${API_BASE}/api/config`,
  CONFIG_UPDATE: `${API_BASE}/api/config/update`,
  CONFIG_RESET: `${API_BASE}/api/config/reset`,
  CONFIG_EXPORT: `${API_BASE}/api/config/export`,
  CONFIG_IMPORT: `${API_BASE}/api/config/import`
} as const;

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
  
  DEVICES: {
    LIST: ENDPOINTS.DEVICES,
    DETAIL: ENDPOINTS.DEVICE_BY_ID,
    DATA_POINTS: ENDPOINTS.DEVICE_DATA_POINTS,
    TEST_CONNECTION: ENDPOINTS.DEVICE_TEST_CONNECTION,
    ENABLE: ENDPOINTS.DEVICE_ENABLE,
    DISABLE: ENDPOINTS.DEVICE_DISABLE,
    RESTART: ENDPOINTS.DEVICE_RESTART,
    PROTOCOLS: ENDPOINTS.DEVICE_PROTOCOLS,
    STATISTICS: ENDPOINTS.DEVICE_STATISTICS,
    BULK_ACTION: ENDPOINTS.DEVICE_BULK_ACTION
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
    ACTIVE: ENDPOINTS.ALARMS_ACTIVE,
    OCCURRENCES: ENDPOINTS.ALARMS_OCCURRENCES,
    OCCURRENCE_DETAIL: ENDPOINTS.ALARMS_OCCURRENCE_BY_ID,
    ACKNOWLEDGE: ENDPOINTS.ALARMS_OCCURRENCE_ACKNOWLEDGE,
    CLEAR: ENDPOINTS.ALARMS_OCCURRENCE_CLEAR,
    HISTORY: ENDPOINTS.ALARMS_HISTORY,
    RULES: ENDPOINTS.ALARM_RULES,
    RULE_DETAIL: ENDPOINTS.ALARM_RULE_BY_ID,
    TEMPLATES: ENDPOINTS.ALARM_TEMPLATES,
    TEMPLATE_DETAIL: ENDPOINTS.ALARM_TEMPLATE_BY_ID,
    STATISTICS: ENDPOINTS.ALARM_STATISTICS,
    UNACKNOWLEDGED: ENDPOINTS.ALARM_UNACKNOWLEDGED,
    DEVICE_ALARMS: ENDPOINTS.ALARM_DEVICE,
    TEST: ENDPOINTS.ALARM_TEST
  },
  
  DASHBOARD: {
    OVERVIEW: ENDPOINTS.DASHBOARD_OVERVIEW,
    TENANT_STATS: ENDPOINTS.DASHBOARD_TENANT_STATS,
    RECENT_DEVICES: ENDPOINTS.DASHBOARD_RECENT_DEVICES,
    SYSTEM_HEALTH: ENDPOINTS.DASHBOARD_SYSTEM_HEALTH,
    SERVICE_CONTROL: ENDPOINTS.DASHBOARD_SERVICE_CONTROL
  }
} as const;

// ==========================================================================
// URL 빌더 유틸리티 함수들
// ==========================================================================

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
 * WebSocket URL 생성 헬퍼 함수
 */
export function buildWebSocketUrl(baseUrl: string, endpoint: string, params?: Record<string, any>): string {
  const wsBaseUrl = baseUrl.replace(/^http/, 'ws');
  const fullEndpoint = buildUrlWithParams(endpoint, params);
  return `${wsBaseUrl}${fullEndpoint}`;
}

// ==========================================================================
// 엔드포인트 검증 유틸리티
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
 * API 그룹별로 엔드포인트를 가져오는 함수
 */
export function getEndpointsByGroup(group: keyof typeof API_GROUPS): Record<string, string | Function> {
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
    baseUrl: 'https://api.pulseone.com',
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

// 에러 코드 상수
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
  
  // 알람 관련
  ALARM_NOT_FOUND: 'ALARM_NOT_FOUND',
  ALARM_RULE_NOT_FOUND: 'ALARM_RULE_NOT_FOUND',
  ALARM_TEMPLATE_NOT_FOUND: 'ALARM_TEMPLATE_NOT_FOUND',
  ALARM_ALREADY_ACKNOWLEDGED: 'ALARM_ALREADY_ACKNOWLEDGED',
  ALARM_ALREADY_CLEARED: 'ALARM_ALREADY_CLEARED',
  
  // 서버 관련
  INTERNAL_SERVER_ERROR: 'INTERNAL_SERVER_ERROR',
  SERVICE_UNAVAILABLE: 'SERVICE_UNAVAILABLE'
} as const;

export default ENDPOINTS;