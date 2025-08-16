// ============================================================================
// frontend/src/api/endpoints.ts
// API 엔드포인트 상수 정의 - Redis API 추가
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
  // 🏭 디바이스 관리 API
  // ==========================================================================
  DEVICES: '/api/devices',
  DEVICE_BY_ID: (id: number | string) => `/api/devices/${id}`,
  DEVICE_DATA_POINTS: (id: number | string) => `/api/devices/${id}/data-points`,
  DEVICE_TEST_CONNECTION: (id: number | string) => `/api/devices/${id}/test-connection`,
  DEVICE_ENABLE: (id: number | string) => `/api/devices/${id}/enable`,
  DEVICE_DISABLE: (id: number | string) => `/api/devices/${id}/disable`,
  DEVICE_RESTART: (id: number | string) => `/api/devices/${id}/restart`,
  DEVICE_PROTOCOLS: '/api/devices/protocols',
  
  // ==========================================================================
  // 🚨 알람 관리 API
  // ==========================================================================
  ALARMS_ACTIVE: '/api/alarms/active',
  ALARMS_HISTORY: '/api/alarms/history',
  ALARM_BY_ID: (id: number | string) => `/api/alarms/${id}`,
  ALARM_ACKNOWLEDGE: (id: number | string) => `/api/alarms/${id}/acknowledge`,
  ALARM_CLEAR: (id: number | string) => `/api/alarms/${id}/clear`,
  ALARM_RULES: '/api/alarms/rules',
  ALARM_RULE_BY_ID: (id: number | string) => `/api/alarms/rules/${id}`,
  ALARM_STATISTICS: '/api/alarms/statistics',
  
  // ==========================================================================
  // 👤 사용자 관리 API
  // ==========================================================================
  USERS: '/api/users',
  USER_BY_ID: (id: number | string) => `/api/users/${id}`,
  USER_LOGIN: '/api/users/login',
  USER_LOGOUT: '/api/users/logout',
  USER_PERMISSIONS: (id: number | string) => `/api/users/${id}/permissions`,
  USER_LOGIN_HISTORY: '/api/users/login-history',
  
  // ==========================================================================
  // 📈 데이터 탐색 API
  // ==========================================================================
  DATA_POINTS: '/api/data/points',
  DATA_HISTORICAL: '/api/data/historical',
  DATA_QUERY: '/api/data/query',
  DATA_EXPORT: '/api/data/export',
  DATA_IMPORT: '/api/data/import',
  DATA_STATISTICS: '/api/data/statistics',
  
  // ==========================================================================
  // 🔴 실시간 데이터 API
  // ==========================================================================
  REALTIME_CURRENT_VALUES: '/api/realtime/current-values',
  REALTIME_DEVICE_VALUES: (id: number | string) => `/api/realtime/device/${id}/values`,
  REALTIME_SUBSCRIBE: '/api/realtime/subscribe',
  REALTIME_UNSUBSCRIBE: (id: string) => `/api/realtime/subscribe/${id}`,
  
  // WebSocket
  WS_REALTIME: '/ws/realtime',
  
  // ==========================================================================
  // 🗄️ Redis 데이터 API (새로 추가)
  // ==========================================================================
  REDIS_STATUS: '/api/redis/status',
  REDIS_STATS: '/api/redis/stats',
  REDIS_INFO: '/api/redis/info',
  
  // Redis 키 관리
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
  
  // ==========================================================================
  // 💾 백업/복원 API (향후 구현)
  // ==========================================================================
  BACKUP_LIST: '/api/backup/list',
  BACKUP_CREATE: '/api/backup/create',
  BACKUP_RESTORE: (id: number | string) => `/api/backup/restore/${id}`,
  BACKUP_DELETE: (id: number | string) => `/api/backup/${id}`,
  BACKUP_STATUS: (id: number | string) => `/api/backup/${id}/status`,
  BACKUP_SCHEDULE: '/api/backup/schedule'
};