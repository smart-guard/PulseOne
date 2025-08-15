// ============================================================================
// frontend/src/api/endpoints.ts  
// 모든 API 엔드포인트 중앙 관리
// ============================================================================

export const ENDPOINTS = {
  // ==========================================================================
  // 🏭 디바이스 관리 API
  // ==========================================================================
  DEVICES: '/api/devices',
  DEVICE_BY_ID: (id: number | string) => `/api/devices/${id}`,
  DEVICE_SETTINGS: (id: number | string) => `/api/devices/${id}/settings`,
  DEVICE_DATA_POINTS: (id: number | string) => `/api/devices/${id}/data-points`,
  DEVICE_STATUS: (id: number | string) => `/api/devices/${id}/status`,
  
  // 디바이스 제어 (Node.js Backend)
  DEVICE_ENABLE: (id: number | string) => `/api/devices/${id}/enable`,
  DEVICE_DISABLE: (id: number | string) => `/api/devices/${id}/disable`,
  DEVICE_RESTART: (id: number | string) => `/api/devices/${id}/restart`,
  DEVICE_TEST_CONNECTION: (id: number | string) => `/api/devices/${id}/test-connection`,
  
  // 일괄 작업
  DEVICES_BULK_ENABLE: '/api/devices/batch/enable',
  DEVICES_BULK_DISABLE: '/api/devices/batch/disable',
  DEVICES_BULK_DELETE: '/api/devices/bulk-delete',
  DEVICES_BULK_UPDATE: '/api/devices/bulk-update',
  
  // ==========================================================================
  // ⚙️ C++ Collector 제어 API (향후 구현)
  // ==========================================================================
  COLLECTOR_DEVICE_START: (id: number | string) => `/api/collector/devices/${id}/start`,
  COLLECTOR_DEVICE_STOP: (id: number | string) => `/api/collector/devices/${id}/stop`,
  COLLECTOR_DEVICE_PAUSE: (id: number | string) => `/api/collector/devices/${id}/pause`,
  COLLECTOR_DEVICE_RESUME: (id: number | string) => `/api/collector/devices/${id}/resume`,
  
  // ==========================================================================
  // 🚨 알람 관리 API
  // ==========================================================================
  ALARMS_ACTIVE: '/api/alarms/active',
  ALARMS_HISTORY: '/api/alarms/history',
  ALARMS_STATISTICS: '/api/alarms/statistics',
  ALARM_BY_ID: (id: number | string) => `/api/alarms/${id}`,
  ALARM_ACKNOWLEDGE: (id: number | string) => `/api/alarms/${id}/acknowledge`,
  ALARM_CLEAR: (id: number | string) => `/api/alarms/${id}/clear`,
  
  // 알람 규칙
  ALARM_RULES: '/api/alarms/rules',
  ALARM_RULE_BY_ID: (id: number | string) => `/api/alarms/rules/${id}`,
  
  // 알람 일괄 작업
  ALARMS_BULK_ACKNOWLEDGE: '/api/alarms/bulk-acknowledge',
  ALARMS_BULK_CLEAR: '/api/alarms/bulk-clear',
  
  // ==========================================================================
  // 📊 실시간 데이터 API
  // ==========================================================================
  REALTIME_CURRENT_VALUES: '/api/realtime/current-values',
  REALTIME_DEVICE_VALUES: (id: number | string) => `/api/realtime/device/${id}/values`,
  REALTIME_SUBSCRIBE: '/api/realtime/subscribe',
  REALTIME_UNSUBSCRIBE: (id: number | string) => `/api/realtime/subscribe/${id}`,
  
  // WebSocket
  WEBSOCKET_REALTIME: '/ws/realtime',
  
  // ==========================================================================
  // 🔧 가상포인트 API
  // ==========================================================================
  VIRTUAL_POINTS: '/api/virtual-points',
  VIRTUAL_POINT_BY_ID: (id: number | string) => `/api/virtual-points/${id}`,
  VIRTUAL_POINT_TEST: (id: number | string) => `/api/virtual-points/${id}/test`,
  VIRTUAL_POINT_DEPENDENCIES: (id: number | string) => `/api/virtual-points/${id}/dependencies`,
  VIRTUAL_POINTS_BULK_TOGGLE: '/api/virtual-points/bulk-toggle',
  
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
  // 🖥️ 시스템 관리 API
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
  // 💾 백업/복원 API (향후 구현)
  // ==========================================================================
  BACKUP_LIST: '/api/backup/list',
  BACKUP_CREATE: '/api/backup/create',
  BACKUP_RESTORE: (id: number | string) => `/api/backup/restore/${id}`,
  BACKUP_DELETE: (id: number | string) => `/api/backup/${id}`,
  BACKUP_STATUS: (id: number | string) => `/api/backup/${id}/status`,
  BACKUP_SCHEDULE: '/api/backup/schedule'
};