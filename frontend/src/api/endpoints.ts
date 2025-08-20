// ============================================================================
// frontend/src/api/endpoints.ts
// API 엔드포인트 상수 정의 - 새로운 Backend API 완전 호환
// ============================================================================

/**
 * 🌐 API 엔드포인트 상수들
 * 모든 API URL을 중앙에서 관리
 */
export const ENDPOINTS = {
  // ==========================================================================
  // 🏥 헬스체크 및 기본 정보
  // ==========================================================================
  HEALTH: '/health',
  API_INFO: '/api/info',
  
  // ==========================================================================
  // 🔧 시스템 관리 API (기존)
  // ==========================================================================
  SYSTEM_STATUS: '/api/system/status',
  SYSTEM_INFO: '/api/system/info', 
  SYSTEM_DATABASES: '/api/system/databases',
  SYSTEM_HEALTH: '/api/system/health',
  
  // 서비스 제어
  SERVICES: '/api/services',
  SERVICE_BY_NAME: (name: string) => `/api/services/${name}`,
  SERVICE_START: (name: string) => `/api/services/${name}/start`,
  SERVICE_STOP: (name: string) => `/api/services/${name}/stop`,
  SERVICE_RESTART: (name: string) => `/api/services/${name}/restart`,
  
  // 프로세스 관리
  PROCESSES: '/api/processes',
  PROCESS_BY_ID: (id: number | string) => `/api/processes/${id}`,
  
  // ==========================================================================
  // 🏭 디바이스 관리 API (새로 완성됨)
  // ==========================================================================
  DEVICES: '/api/devices',
  DEVICE_BY_ID: (id: number | string) => `/api/devices/${id}`,
  DEVICE_DATA_POINTS: (id: number | string) => `/api/devices/${id}/data-points`,
  DEVICE_TEST_CONNECTION: (id: number | string) => `/api/devices/${id}/test-connection`,
  DEVICE_ENABLE: (id: number | string) => `/api/devices/${id}/enable`,
  DEVICE_DISABLE: (id: number | string) => `/api/devices/${id}/disable`,
  DEVICE_RESTART: (id: number | string) => `/api/devices/${id}/restart`,
  DEVICE_PROTOCOLS: '/api/devices/protocols',
  DEVICE_STATISTICS: '/api/devices/statistics',
  DEVICE_BULK_ACTION: '/api/devices/bulk-action',
  
  // ==========================================================================
  // 📊 데이터 탐색 API (새로 완성됨)
  // ==========================================================================
  DATA_POINTS: '/api/data/points',
  DATA_POINT_BY_ID: (id: number | string) => `/api/data/points/${id}`,
  DATA_CURRENT_VALUES: '/api/data/current-values',
  DATA_DEVICE_VALUES: (id: number | string) => `/api/data/device/${id}/current-values`,
  DATA_HISTORICAL: '/api/data/historical',
  DATA_QUERY: '/api/data/query',
  DATA_EXPORT: '/api/data/export',
  DATA_STATISTICS: '/api/data/statistics',
  
  // ==========================================================================
  // ⚡ 실시간 데이터 API (새로 완성됨)
  // ==========================================================================
  REALTIME_CURRENT_VALUES: '/api/realtime/current-values',
  REALTIME_DEVICE_VALUES: (id: number | string) => `/api/realtime/device/${id}/values`,
  REALTIME_SUBSCRIBE: '/api/realtime/subscribe',
  REALTIME_UNSUBSCRIBE: (id: string) => `/api/realtime/subscribe/${id}`,
  REALTIME_SUBSCRIPTIONS: '/api/realtime/subscriptions',
  REALTIME_POLL: (id: string) => `/api/realtime/poll/${id}`,
  REALTIME_STATS: '/api/realtime/stats',
  
  // ==========================================================================
  // 🚨 알람 관리 API (기존 완성됨)
  // ==========================================================================
  ALARMS_ACTIVE: '/api/alarms/active',
  ALARMS_OCCURRENCES: '/api/alarms/occurrences',
  ALARMS_OCCURRENCE_BY_ID: (id: number | string) => `/api/alarms/occurrences/${id}`,
  ALARMS_OCCURRENCE_ACKNOWLEDGE: (id: number | string) => `/api/alarms/occurrences/${id}/acknowledge`,
  ALARMS_OCCURRENCE_CLEAR: (id: number | string) => `/api/alarms/occurrences/${id}/clear`,
  ALARMS_HISTORY: '/api/alarms/history',
  ALARM_BY_ID: (id: number | string) => `/api/alarms/${id}`,
  ALARM_ACKNOWLEDGE: (id: number | string) => `/api/alarms/${id}/acknowledge`,
  ALARM_CLEAR: (id: number | string) => `/api/alarms/${id}/clear`,
  ALARM_RULE_TEMPLATES: '/api/alarms/rules',
  ALARM_RULE_BY_ID: (id: number | string) => `/api/alarms/rules/${id}`,
  ALARM_STATISTICS: '/api/alarms/statistics',
  ALARM_UNACKNOWLEDGED: '/api/alarms/unacknowledged',
  ALARM_DEVICE: (deviceId: number | string) => `/api/alarms/device/${deviceId}`,
  ALARM_TEST: '/api/alarms/test',
  
  // ==========================================================================
  // 📈 대시보드 API (기존 완성됨)
  // ==========================================================================
  DASHBOARD_OVERVIEW: '/api/dashboard/overview',
  DASHBOARD_TENANT_STATS: '/api/dashboard/tenant-stats',
  DASHBOARD_RECENT_DEVICES: '/api/dashboard/recent-devices',
  DASHBOARD_SYSTEM_HEALTH: '/api/dashboard/system-health',
  DASHBOARD_SERVICE_CONTROL: (name: string) => `/api/dashboard/service/${name}/control`,
  
  // ==========================================================================
  // 👤 사용자 관리 API (기존)
  // ==========================================================================
  USERS: '/api/users',
  USER_BY_ID: (id: number | string) => `/api/users/${id}`,
  USER_LOGIN: '/api/users/login',
  USER_LOGOUT: '/api/users/logout',
  USER_PERMISSIONS: (id: number | string) => `/api/users/${id}/permissions`,
  USER_LOGIN_HISTORY: '/api/users/login-history',
  
  // ==========================================================================
  // 🌐 WebSocket 엔드포인트
  // ==========================================================================
  WS_REALTIME: '/ws/realtime',
  
  // ==========================================================================
  // 🗄️ Redis 데이터 API (기존 - 통합됨)
  // ==========================================================================
  REDIS_STATUS: '/api/redis/status',
  REDIS_STATS: '/api/redis/stats',
  REDIS_INFO: '/api/redis/info',
  
  // Redis 키 관리 (일부 realtime API로 통합됨)
  REDIS_TREE: '/api/redis/tree',
  REDIS_TREE_CHILDREN: (nodeId: string) => `/api/redis/tree/${nodeId}/children`,
  REDIS_KEYS_SEARCH: '/api/redis/keys/search',
  REDIS_KEY_DATA: (key: string) => `/api/redis/keys/${encodeURIComponent(key)}/data`,
  REDIS_KEYS_BULK: '/api/redis/keys/bulk',
  REDIS_KEY_DELETE: (key: string) => `/api/redis/keys/${encodeURIComponent(key)}`,
  REDIS_KEY_TTL: (key: string) => `/api/redis/keys/${encodeURIComponent(key)}/ttl`,
  
  // Redis 패턴 및 구조
  REDIS_PATTERNS: '/api/redis/patterns',
  REDIS_TENANT_KEYS: '/api/redis/tenants/keys',
  REDIS_EXPORT: '/api/redis/export',
  
  // Redis 테스트
  REDIS_TEST: '/api/redis/test',
  REDIS_PING: '/api/redis/ping',
  REDIS_CONNECTION_CHECK: '/api/redis-connection-check',
  
  // ==========================================================================
  // 🔧 초기화 및 설정 API
  // ==========================================================================
  INIT_STATUS: '/api/init/status',
  INIT_TRIGGER: '/api/init/trigger',
  
  // ==========================================================================
  // 🆕 향후 추가될 API들 (플레이스홀더)
  // ==========================================================================
  MONITORING_SERVICE_HEALTH: '/api/monitoring/service-health',
  MONITORING_SYSTEM_METRICS: '/api/monitoring/system-metrics',
  MONITORING_DATABASE_STATS: '/api/monitoring/database-stats',
  MONITORING_PERFORMANCE: '/api/monitoring/performance',
  MONITORING_LOGS: '/api/monitoring/logs',
  
  // 가상포인트 관리
  VIRTUAL_POINTS: '/api/virtual-points',
  VIRTUAL_POINT_BY_ID: (id: number | string) => `/api/virtual-points/${id}`,
  VIRTUAL_POINT_TEST: (id: number | string) => `/api/virtual-points/${id}/test`,
  VIRTUAL_POINT_DEPENDENCIES: (id: number | string) => `/api/virtual-points/${id}/dependencies`,
  
  // 사이트 관리
  SITES: '/api/sites',
  SITE_BY_ID: (id: number | string) => `/api/sites/${id}`,
  SITE_DEVICES: (id: number | string) => `/api/sites/${id}/devices`,
  SITE_STATISTICS: (id: number | string) => `/api/sites/${id}/statistics`,
  
  // 백업/복원
  BACKUP_LIST: '/api/backup/list',
  BACKUP_CREATE: '/api/backup/create',
  BACKUP_RESTORE: (id: number | string) => `/api/backup/restore/${id}`,
  BACKUP_DELETE: (id: number | string) => `/api/backup/${id}`,
  BACKUP_STATUS: (id: number | string) => `/api/backup/${id}/status`,
  BACKUP_SCHEDULE: '/api/backup/schedule',
  
  // 네트워크 설정
  NETWORK_SETTINGS: '/api/network/settings',
  NETWORK_INTERFACES: '/api/network/interfaces',
  NETWORK_TEST: '/api/network/test',
  
  // 권한 관리
  PERMISSIONS: '/api/permissions',
  PERMISSION_ROLES: '/api/permissions/roles',
  PERMISSION_USERS: '/api/permissions/users',
  PERMISSION_ASSIGN: '/api/permissions/assign',
  
  // 로그 관리
  LOGS: '/api/logs',
  LOGS_DOWNLOAD: '/api/logs/download',
  LOGS_CLEAR: '/api/logs/clear',
  
  // 설정 관리
  CONFIG: '/api/config',
  CONFIG_UPDATE: '/api/config/update',
  CONFIG_RESET: '/api/config/reset',
  CONFIG_EXPORT: '/api/config/export',
  CONFIG_IMPORT: '/api/config/import'
} as const;

// ==========================================================================
// 🔧 타입 안전성을 위한 엔드포인트 그룹화
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
// 🔧 URL 빌더 유틸리티 함수들
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
// 🔧 엔드포인트 검증 유틸리티
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

export default ENDPOINTS;