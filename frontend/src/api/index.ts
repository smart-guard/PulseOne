// ============================================================================
// frontend/src/api/index.ts  
// API 서비스 통합 진입점
// ============================================================================

// 🔧 기존 시스템 관리 API (유지)
export { default as systemApiService } from '../services/apiService';
export type { 
  ServiceStatus, 
  SystemMetrics, 
  PlatformInfo, 
  HealthStatus 
} from '../services/apiService';

// 📊 새로운 비즈니스 API들
export { DeviceApiService } from './services/deviceApi';
export { AlarmApiService } from './services/alarmApi';

// 🔗 공통 설정 및 클라이언트
export { apiClient, collectorClient } from './client';
export { ENDPOINTS } from './endpoints';
export { API_CONFIG } from './config';

// 📄 타입들
export type { 
  ApiResponse, 
  PaginatedApiResponse, 
  PaginationParams,
  BulkActionRequest,
  BulkActionResponse 
} from '../types/common';