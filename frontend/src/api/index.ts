// ============================================================================
// frontend/src/api/index.ts  
// API 서비스 통합 진입점 - 새로운 Backend API 완전 호환
// ============================================================================

// ==========================================================================
// 🔧 기존 시스템 관리 API (유지)
// ==========================================================================
export { default as systemApiService } from '../services/apiService';
export type { 
  ServiceStatus, 
  SystemMetrics, 
  PlatformInfo, 
  HealthStatus 
} from '../services/apiService';

// ==========================================================================
// 🆕 새로 완성된 핵심 비즈니스 API들
// ==========================================================================

// 🏭 디바이스 관리 API
export { DeviceApiService } from './services/deviceApi';
export type { 
  Device,
  DeviceStats,
  DeviceListParams,
  DeviceCreateData,
  DeviceUpdateData,
  ConnectionTestResult,
  BulkActionRequest,
  BulkActionResult
} from './services/deviceApi';

// 🔌 프로토콜 관리 API (새로 추가)
export { ProtocolApiService } from './services/protocolApi';
export type {
  Protocol,
  ProtocolStats,
  ProtocolCreateData,
  ProtocolUpdateData
} from './services/protocolApi';

// 📊 데이터 익스플로러 API
export { DataApiService } from './services/dataApi';
export type {
  DataPoint,
  CurrentValue,
  HistoricalDataPoint,
  DataStatistics,
  DataPointSearchParams,
  CurrentValueParams,
  HistoricalDataParams,
  AdvancedQueryParams,
  ExportParams,
  ExportResult
} from './services/dataApi';

// ⚡ 실시간 데이터 API
export { RealtimeApiService } from './services/realtimeApi';
export type {
  RealtimeValue,
  DeviceRealtimeData,
  SubscriptionRequest,
  SubscriptionInfo,
  SubscriptionSummary,
  PollingData,
  RealtimeStats,
  CurrentValuesParams,
  CurrentValuesResponse
} from './services/realtimeApi';

// 🚨 알람 관리 API (기존 유지)
export { AlarmApiService } from './services/alarmApi';

// ==========================================================================
// 🔗 공통 설정 및 클라이언트
// ==========================================================================
export { apiClient, collectorClient } from './client';
export { ENDPOINTS, API_GROUPS, buildUrlWithParams, buildPaginatedUrl, buildSortedUrl, buildWebSocketUrl } from './endpoints';
export { API_CONFIG } from './config';

// ==========================================================================
// 📄 공통 타입들
// ==========================================================================
export type { 
  ApiResponse, 
  PaginatedApiResponse, 
  PaginationParams,
  PaginationMeta,
  BulkActionRequest,
  BulkActionResponse 
} from '../types/common';

// ==========================================================================
// 🆕 통합 API 서비스 클래스 (옵셔널)
// ==========================================================================

import { DeviceApiService } from './services/deviceApi';
import { DataApiService } from './services/dataApi';
import { RealtimeApiService } from './services/realtimeApi';
import { AlarmApiService } from './services/alarmApi';
import systemApiService from '../services/apiService';

/**
 * 모든 API 서비스를 하나로 통합한 클래스
 * 사용 예시: ApiService.device.getDevices(), ApiService.data.getCurrentValues()
 */
export class ApiService {
  // 🔧 시스템 관리 (기존)
  static system = systemApiService;
  
  // 🏭 디바이스 관리 (신규)
  static device = DeviceApiService;
  
  // 📊 데이터 익스플로러 (신규)
  static data = DataApiService;
  
  // ⚡ 실시간 데이터 (신규)
  static realtime = RealtimeApiService;
  
  // 🚨 알람 관리 (기존)
  static alarm = AlarmApiService;
  
  // =======================================================================
  // 🔧 통합 유틸리티 메서드들
  // =======================================================================
  
  /**
   * 모든 API 서비스의 헬스체크
   */
  static async healthCheck(): Promise<{
    system: boolean;
    device: boolean;
    data: boolean;
    realtime: boolean;
    alarm: boolean;
    overall: boolean;
  }> {
    const results = {
      system: false,
      device: false,
      data: false,
      realtime: false,
      alarm: false,
      overall: false
    };
    
    try {
      // 시스템 API 헬스체크
      const systemHealth = await this.system.getSystemHealth();
      results.system = systemHealth.overall === 'healthy';
    } catch (error) {
      console.warn('System API health check failed:', error);
    }
    
    try {
      // 디바이스 API 헬스체크
      const deviceStats = await this.device.getDeviceStatistics();
      results.device = deviceStats.success;
    } catch (error) {
      console.warn('Device API health check failed:', error);
    }
    
    try {
      // 데이터 API 헬스체크
      const dataStats = await this.data.getDataStatistics();
      results.data = dataStats.success;
    } catch (error) {
      console.warn('Data API health check failed:', error);
    }
    
    try {
      // 실시간 API 헬스체크
      const realtimeStats = await this.realtime.getRealtimeStats();
      results.realtime = realtimeStats.success;
    } catch (error) {
      console.warn('Realtime API health check failed:', error);
    }
    
    try {
      // 알람 API 헬스체크 (테스트 엔드포인트 사용)
      const alarmTest = await fetch('/api/alarms/test');
      results.alarm = alarmTest.ok;
    } catch (error) {
      console.warn('Alarm API health check failed:', error);
    }
    
    // 전체 상태는 중요한 API들이 모두 동작할 때만 true
    results.overall = results.system && results.device && results.data;
    
    return results;
  }
  
  /**
   * 전체 시스템 통계 조회
   */
  static async getSystemOverview(): Promise<{
    devices: any;
    data: any;
    realtime: any;
    alarms: any;
    system: any;
  }> {
    const [deviceStats, dataStats, realtimeStats, alarmStats, systemHealth] = await Promise.allSettled([
      this.device.getDeviceStatistics(),
      this.data.getDataStatistics(),
      this.realtime.getRealtimeStats(),
      fetch('/api/alarms/statistics').then(r => r.json()),
      this.system.getSystemHealth()
    ]);
    
    return {
      devices: deviceStats.status === 'fulfilled' ? deviceStats.value.data : null,
      data: dataStats.status === 'fulfilled' ? dataStats.value.data : null,
      realtime: realtimeStats.status === 'fulfilled' ? realtimeStats.value.data : null,
      alarms: alarmStats.status === 'fulfilled' ? alarmStats.value.data : null,
      system: systemHealth.status === 'fulfilled' ? systemHealth.value : null
    };
  }
  
  /**
   * 통합 검색 (디바이스, 데이터포인트 등)
   */
  static async search(query: string, options: {
    includeDevices?: boolean;
    includeDataPoints?: boolean;
    limit?: number;
  } = {}): Promise<{
    devices: any[];
    dataPoints: any[];
    total: number;
  }> {
    const { includeDevices = true, includeDataPoints = true, limit = 50 } = options;
    const results = {
      devices: [] as any[],
      dataPoints: [] as any[],
      total: 0
    };
    
    const searchPromises: Promise<any>[] = [];
    
    if (includeDevices) {
      searchPromises.push(
        this.device.getDevices({ search: query, limit: Math.floor(limit / 2) })
          .then(response => response.success ? response.data.items : [])
          .catch(() => [])
      );
    } else {
      searchPromises.push(Promise.resolve([]));
    }
    
    if (includeDataPoints) {
      searchPromises.push(
        this.data.searchDataPoints({ search: query, limit: Math.floor(limit / 2) })
          .then(response => response.success ? response.data.items : [])
          .catch(() => [])
      );
    } else {
      searchPromises.push(Promise.resolve([]));
    }
    
    const [devices, dataPoints] = await Promise.all(searchPromises);
    
    results.devices = devices;
    results.dataPoints = dataPoints;
    results.total = devices.length + dataPoints.length;
    
    return results;
  }
  
  /**
   * 실시간 데이터 스트림 시작
   */
  static async startRealtimeStream(config: {
    deviceIds?: number[];
    pointIds?: number[];
    updateInterval?: number;
    onData?: (data: any) => void;
    onError?: (error: any) => void;
  }): Promise<string | null> {
    try {
      const subscriptionResponse = await this.realtime.createSubscription({
        device_ids: config.deviceIds,
        point_ids: config.pointIds,
        update_interval: config.updateInterval || 1000
      });
      
      if (!subscriptionResponse.success || !subscriptionResponse.data) {
        throw new Error('구독 생성 실패');
      }
      
      const subscriptionId = subscriptionResponse.data.subscription_id;
      
      // WebSocket 연결 시작
      this.realtime.connectWebSocket(subscriptionId, {
        onMessage: config.onData,
        onError: config.onError,
        onOpen: () => console.log('실시간 스트림 시작:', subscriptionId),
        onClose: () => console.log('실시간 스트림 종료:', subscriptionId)
      });
      
      return subscriptionId;
    } catch (error) {
      console.error('실시간 스트림 시작 실패:', error);
      config.onError?.(error);
      return null;
    }
  }
  
  /**
   * 실시간 데이터 스트림 중지
   */
  static async stopRealtimeStream(subscriptionId: string): Promise<boolean> {
    try {
      // WebSocket 연결 해제
      this.realtime.disconnectWebSocket();
      
      // 구독 해제
      const response = await this.realtime.unsubscribe(subscriptionId);
      return response.success;
    } catch (error) {
      console.error('실시간 스트림 중지 실패:', error);
      return false;
    }
  }
}

// ==========================================================================
// 🔧 기본 내보내기 (하위 호환성)
// ==========================================================================

export default ApiService;