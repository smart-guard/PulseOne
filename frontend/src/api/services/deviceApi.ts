// ============================================================================
// frontend/src/api/services/deviceApi.ts
// 완성된 Device API - 모든 Collector 제어 API 통합
// ============================================================================

import { API_CONFIG } from '../config';
import { apiClient, ApiResponse } from '../client';
import { ENDPOINTS } from '../endpoints';

// ============================================================================
// 프로토콜 정보 인터페이스 - ID 포함
// ============================================================================

export interface ProtocolInfo {
  id: number;                    // 데이터베이스 ID
  protocol_type: string;         // 백엔드 호환성
  name: string;                  // 표시명 (display_name)
  value: string;                 // 호환성 (protocol_type와 동일)
  description: string;
  display_name: string;
  default_port?: number;
  uses_serial?: boolean;
  requires_broker?: boolean;
  supported_operations?: string[];
  supported_data_types?: string[];
  connection_params_schema?: any;
  default_polling_interval?: number;
  default_timeout?: number;
  category?: string;
  device_count?: number;
  enabled_count?: number;
  connected_count?: number;
}

// ============================================================================
// 디바이스 인터페이스 - protocol_id 추가
// ============================================================================

export interface Device {
  // 기본 정보
  id: number;
  tenant_id?: number;
  site_id?: number;
  device_group_id?: number;
  device_group_name?: string;
  edge_server_id?: number;

  // 디바이스 기본 속성
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  serial_number?: string;
  template_device_id?: number;
  template_name?: string;

  // 프로토콜 정보 - ID와 타입 모두 관리
  protocol_id: number;           // 데이터베이스 ID (실제 저장값)
  protocol_instance_id?: number; // 🔥 NEW: 프로토콜 인스턴스 ID (멀티 인스턴스/VHost 지원)
  instance_name?: string;        // 🔥 NEW: 인스턴스 이름 (표시용)
  protocol_type: string;         // 타입 문자열 (표시용)
  endpoint: string;
  config?: any;

  // 프로토콜 상세 정보 (JOIN된 데이터)
  protocol?: {
    id: number;
    type: string;
    name: string;
    display_name?: string;
    default_port?: number;
    category?: string;
  };

  settings?: {
    polling_interval_ms?: number;
    connection_timeout_ms?: number;
    read_timeout_ms?: number;
    write_timeout_ms?: number;
    max_retry_count?: number;
    retry_interval_ms?: number;
    backoff_time_ms?: number;
    is_keep_alive_enabled?: boolean;
    keep_alive_interval_s?: number;
    is_data_validation_enabled?: boolean;
    is_performance_monitoring_enabled?: boolean;
    is_detailed_logging_enabled?: boolean;
    is_diagnostic_mode_enabled?: boolean;
    is_communication_logging_enabled?: boolean;
    [key: string]: any;
  };

  // 운영 설정
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled: boolean;

  // 상태 정보
  connection_status?: string;
  status?: string | any;
  tags?: string[] | string; // Added/Moved as per instruction
  metadata?: any; // Added as per instruction
  custom_fields?: any; // Added as per instruction
  created_at?: string;
  last_seen?: string;
  last_communication?: string;

  // Collector 상태 (실시간)
  collector_status?: {
    status?: string;
    message?: string;
    worker_pid?: number;
    uptime?: number;
    last_error?: string;
    performance?: {
      requests_processed?: number;
      errors?: number;
      avg_response_time?: number;
    };
  };

  // 데이터포인트 정보
  data_point_count?: number;
  enabled_point_count?: number;
  data_points_count?: number;

  // 성능 정보
  response_time?: number;
  error_count?: number;
  last_error?: string;
  firmware_version?: string;
  hardware_info?: string;
  diagnostic_data?: any;

  // RTU 특화 정보
  rtu_info?: RtuInfo | null;
  rtu_network?: RtuNetwork | null;

  // 시간 정보
  installation_date?: string;
  last_maintenance?: string;
  updated_at?: string;
  is_deleted?: number;

  site_name?: string;
  site_code?: string;
  protocol_name?: string; // 🔥 추가: 스캔 결과 등에서 표시용 프로토콜명
  group_name?: string;
  group_type?: string;
  groups?: DeviceGroupAssignment[];
  group_ids?: number[];
  status_info?: {
    connection_status: string;
    last_communication?: string;
    error_count: number;
    last_error?: string;
    response_time?: number;
    throughput_ops_per_sec: number;
    total_requests: number;
    successful_requests: number;
    failed_requests: number;
    firmware_version?: string;
    hardware_info?: any;
    diagnostic_data?: any;
    cpu_usage?: number;
    memory_usage?: number;
    uptime_percentage: number;
  };
}

export interface DeviceGroupAssignment {
  id: number;
  name: string;
  is_primary?: boolean;
}

// ============================================================================
// Collector 제어 관련 인터페이스들
// ============================================================================

export interface CollectorDeviceStatus {
  device_id: string;
  worker_status: 'running' | 'stopped' | 'paused' | 'error' | 'starting' | 'stopping';
  worker_pid?: number;
  uptime_seconds?: number;
  last_activity?: string;
  performance_metrics?: {
    requests_processed: number;
    successful_requests: number;
    failed_requests: number;
    avg_response_time_ms: number;
    last_response_time_ms?: number;
  };
  connection_info?: {
    protocol: string;
    endpoint: string;
    connected: boolean;
    last_connected?: string;
    connection_attempts?: number;
  };
  error_info?: {
    last_error?: string;
    error_count?: number;
    consecutive_errors?: number;
  };
  data_points?: {
    total: number;
    active: number;
    last_update?: string;
  };
}

export interface WorkerBatchResult {
  total_processed: number;
  successful: number;
  failed: number;
  results: Array<{
    device_id: number;
    success: boolean;
    data?: any;
    error?: string;
  }>;
}

export interface HardwareControlResult {
  device_id: string;
  output_id: string;
  action: string;
  success: boolean;
  previous_state?: any;
  new_state?: any;
  timestamp: string;
  response_time_ms?: number;
}

export interface ConfigSyncResult {
  device_id: string;
  sync_type: 'reload' | 'sync' | 'notify';
  success: boolean;
  changes_applied?: number;
  warnings?: string[];
  timestamp: string;
}

// ============================================================================
// 요청 인터페이스들
// ============================================================================

export interface CreateDeviceRequest {
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  protocol_id: number;           // protocol_type → protocol_id
  protocol_instance_id?: number; // 🔥 NEW: 프로토콜 인스턴스 ID (선택 사항)
  endpoint: string;
  config?: any;
  site_id?: number;
  device_group_id?: number;
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled: boolean;
  group_ids?: number[];
  data_points?: any[]; // 🔥 NEW: 일괄 생성용 데이터포인트
}

export interface UpdateDeviceRequest {
  name?: string;
  endpoint?: string;
  device_type?: string;
  site_id?: number;
  manufacturer?: string;
  model?: string;
  description?: string;
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled?: boolean;
  config?: any;
  device_group_id?: number;
  protocol_id?: number;
  protocol_instance_id?: number; // 🔥 NEW
  tags?: string[] | string;
  metadata?: any;
  custom_fields?: any;
  // 🔥 핵심 추가: settings 필드
  settings?: {
    polling_interval_ms?: number;
    connection_timeout_ms?: number;
    read_timeout_ms?: number;
    write_timeout_ms?: number;
    max_retry_count?: number;
    retry_interval_ms?: number;
    backoff_time_ms?: number;
    is_keep_alive_enabled?: boolean;
    keep_alive_interval_s?: number;
    is_data_validation_enabled?: boolean;
    is_performance_monitoring_enabled?: boolean;
    is_detailed_logging_enabled?: boolean;
    is_diagnostic_mode_enabled?: boolean;
    is_communication_logging_enabled?: boolean;
    [key: string]: any; // 추가 설정 필드를 위한 인덱스 시그니처
  };
  group_ids?: number[];
  data_points?: any[]; // 🔥 NEW: 일괄 업데이트용 데이터포인트
}

export interface GetDevicesParams {
  page?: number;
  limit?: number;
  search?: string;
  protocol_type?: string;        // 필터링용 (호환성)
  protocol_id?: number;          // ID로 필터링
  protocol_instance_id?: number; // 🔥 NEW: 인스턴스 ID로 필터링
  device_type?: string;
  connection_status?: string;
  status?: string;
  site_id?: number;
  device_group_id?: number;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
  include_rtu_relations?: boolean;
  include_collector_status?: boolean; // 실시간 상태 포함
  includeDeleted?: boolean;           // 삭제된 장치 포함 여부
  onlyDeleted?: boolean;              // 삭제된 장치만 조회 여부
}

export interface DigitalControlRequest {
  state: boolean;
  duration?: number;  // 지속시간 (밀리초)
  force?: boolean;    // 강제 실행
}

export interface AnalogControlRequest {
  value: number;
  unit?: string;
  ramp_time?: number; // 램프 시간 (밀리초)
}

export interface PumpControlRequest {
  enable: boolean;
  speed?: number;     // 0-100%
  duration?: number;  // 지속시간 (밀리초)
}

// ============================================================================
// RTU 관련 인터페이스들 (기존 유지)
// ============================================================================

export interface RtuInfo {
  slave_id: number | null;
  master_device_id: number | null;
  baud_rate: number | null;
  data_bits: number;
  stop_bits: number;
  parity: string;
  frame_delay_ms: number | null;
  response_timeout_ms: number | null;
  is_master: boolean;
  is_slave: boolean;
  serial_port: string;
  network_info: {
    protocol: string;
    connection_type: string;
    port: string;
  };
  slave_count?: number;
  slaves?: Array<{
    device_id: number;
    device_name: string;
    slave_id: number | null;
    device_type: string;
    connection_status: string;
    manufacturer?: string;
    model?: string;
  }>;
}

export interface RtuNetwork {
  role: 'master' | 'slave';
  master?: Device;
  slaves?: Device[];
  slave_count?: number;
  network_status?: string;
  serial_port?: string;
  slave_id?: number;
  communication_settings?: {
    baud_rate: number;
    data_bits: number;
    stop_bits: number;
    parity: string;
  };
  error?: string;
}

// 기타 인터페이스들 (기존 유지)
export interface DeviceStats {
  total_devices: number;
  active_devices: number;
  enabled_devices: number;
  by_protocol: { [key: string]: number };
  by_connection: { [key: string]: number };
  rtu_statistics?: {
    total_rtu_devices: number;
    rtu_masters: number;
    rtu_slaves: number;
    rtu_networks: Array<{
      master_id: number;
      master_name: string;
      serial_port: string;
      baud_rate: number | null;
      connection_status: string;
    }>;
  };
  last_updated: string;
}

// ApiResponse interface removed as it is now imported from ../client

export interface PaginationInfo {
  page: number;
  limit: number;
  total: number;
  totalPages: number;
  hasNext: boolean;
  hasPrev: boolean;
}

export interface DevicesResponse {
  items: Device[];
  pagination: PaginationInfo;
  rtu_summary?: {
    total_rtu_devices: number;
    rtu_masters: number;
    rtu_slaves: number;
    rtu_networks: Array<{
      master_id: number;
      master_name: string;
      serial_port: string;
      baud_rate: number | null;
      slave_count: number;
      connection_status: string;
    }>;
  };
}

export interface ConnectionTestResult {
  device_id: number;
  device_name: string;
  endpoint: string;
  protocol_type: string;
  test_successful: boolean;
  response_time_ms?: number;
  test_timestamp: string;
  error_message?: string;
  rtu_info?: RtuInfo;
}

export interface BulkActionRequest {
  action: 'enable' | 'disable' | 'delete';
  device_ids: number[];
}

export interface BulkActionResult {
  total_processed: number;
  successful: number;
  failed: number;
  errors?: Array<{
    device_id: number;
    error: string;
  }>;
}

// ============================================================================
// 프로토콜 관리 클래스
// ============================================================================

class ProtocolManager {
  private static protocols: ProtocolInfo[] = [];
  private static protocolMap: Map<number, ProtocolInfo> = new Map();
  private static typeToIdMap: Map<string, number> = new Map();

  // 프로토콜 목록 로드
  static async loadProtocols(): Promise<ProtocolInfo[]> {
    try {
      // API 응답 타입 수정 (items가 포함된 목록이라고 가정 - ProtocolService.js 확인 결과)
      // ProtocolService.getProtocols returns { items: [], total_count: ... }
      const response = await apiClient.get<any>('/api/protocols');

      if (response.success && response.data) {
        // 응답 구조가 { items: [...] } 인 경우와 [...] 인 경우 모두 처리
        const items = Array.isArray(response.data) ? response.data : (response.data.items || []);

        this.protocols = items;

        // Map 생성 (빠른 조회용)
        this.protocolMap.clear();
        this.typeToIdMap.clear();

        this.protocols.forEach(protocol => {
          this.protocolMap.set(protocol.id, protocol);
          this.typeToIdMap.set(protocol.protocol_type, protocol.id);
        });

        console.log(`프로토콜 ${this.protocols.length}개 로드 완료`);
        return this.protocols;
      }

      throw new Error('프로토콜 데이터 로드 실패');

    } catch (error) {
      console.error('프로토콜 로드 실패:', error);
      return this.getDefaultProtocols();
    }
  }

  // 기본 프로토콜 목록 (로드 실패 시)
  static getDefaultProtocols(): ProtocolInfo[] {
    return [
      {
        id: 1,
        protocol_type: 'MODBUS_TCP',
        name: 'Modbus TCP',
        value: 'MODBUS_TCP',
        description: 'Modbus TCP/IP Protocol',
        display_name: 'Modbus TCP',
        default_port: 502,
        device_count: 0,
        enabled_count: 0,
        connected_count: 0
      },
      {
        id: 2,
        protocol_type: 'MODBUS_RTU',
        name: 'Modbus RTU',
        value: 'MODBUS_RTU',
        description: 'Modbus RTU Serial Protocol',
        display_name: 'Modbus RTU',
        uses_serial: true,
        device_count: 0,
        enabled_count: 0,
        connected_count: 0
      },
      {
        id: 3,
        protocol_type: 'MQTT',
        name: 'MQTT',
        value: 'MQTT',
        description: 'Message Queuing Telemetry Transport',
        display_name: 'MQTT',
        default_port: 1883,
        requires_broker: true,
        device_count: 0,
        enabled_count: 0,
        connected_count: 0
      },
      {
        id: 4,
        protocol_type: 'BACNET',
        name: 'BACnet',
        value: 'BACNET',
        description: 'Building Automation and Control Networks',
        display_name: 'BACnet',
        default_port: 47808,
        device_count: 0,
        enabled_count: 0,
        connected_count: 0
      }
    ];
  }

  // ID로 프로토콜 조회
  static getProtocolById(id: number): ProtocolInfo | undefined {
    return this.protocolMap.get(id);
  }

  // 타입으로 ID 조회
  static getProtocolIdByType(type: string): number | undefined {
    return this.typeToIdMap.get(type);
  }

  // 타입으로 프로토콜 조회
  static getProtocolByType(type: string): ProtocolInfo | undefined {
    const id = this.typeToIdMap.get(type);
    return id ? this.protocolMap.get(id) : undefined;
  }

  // 모든 프로토콜 반환
  static getAllProtocols(): ProtocolInfo[] {
    return [...this.protocols];
  }

  // 프로토콜 이름 조회
  static getProtocolName(id: number): string {
    const protocol = this.protocolMap.get(id);
    return protocol?.display_name || protocol?.name || `Protocol ${id}`;
  }
}

// ============================================================================
// DeviceApiService 클래스 - 모든 Collector 제어 API 통합
// ============================================================================

export class DeviceApiService {
  private static readonly BASE_URL = '/api/devices';
  private static readonly COLLECTOR_URL = '/api/collector';

  // 초기화 (프로토콜 로드)
  static async initialize(): Promise<void> {
    await ProtocolManager.loadProtocols();
  }

  // 프로토콜 관련 메서드들
  static getProtocolManager() {
    return ProtocolManager;
  }

  // ========================================================================
  // 기본 CRUD API들
  // ========================================================================

  // 디바이스 목록 조회
  static async getDevices(params?: GetDevicesParams): Promise<ApiResponse<DevicesResponse>> {
    try {
      return await apiClient.get<DevicesResponse>(this.BASE_URL, params);
    } catch (error) {
      console.error('디바이스 목록 조회 실패:', error);
      throw error;
    }
  }

  // 디바이스 상세 조회
  static async getDevice(
    id: number,
    options?: {
      include_data_points?: boolean;
      include_rtu_network?: boolean;
      include_collector_status?: boolean;
    }
  ): Promise<ApiResponse<Device>> {
    try {
      return await apiClient.get<Device>(`${this.BASE_URL}/${id}`, options);
    } catch (error) {
      console.error(`디바이스 ${id} 조회 실패:`, error);
      throw error;
    }
  }

  // 디바이스 생성 - protocol_id 사용
  static async createDevice(data: CreateDeviceRequest): Promise<ApiResponse<Device>> {
    try {
      // protocol_id 유효성 검사
      const protocol = ProtocolManager.getProtocolById(data.protocol_id);
      if (!protocol) {
        throw new Error(`유효하지 않은 프로토콜 ID: ${data.protocol_id}`);
      }

      return await apiClient.post<Device>(this.BASE_URL, data);
    } catch (error) {
      console.error('디바이스 생성 실패:', error);
      throw error;
    }
  }

  // 디바이스 수정 - protocol_id 사용
  static async updateDevice(id: number, data: UpdateDeviceRequest): Promise<ApiResponse<Device>> {
    try {
      // protocol_id 유효성 검사 (변경 시)
      if (data.protocol_id !== undefined) {
        const protocol = ProtocolManager.getProtocolById(data.protocol_id);
        if (!protocol) {
          throw new Error(`유효하지 않은 프로토콜 ID: ${data.protocol_id}`);
        }
      }

      return await apiClient.put<Device>(`${this.BASE_URL}/${id}`, data);
    } catch (error) {
      console.error(`디바이스 ${id} 수정 실패:`, error);
      throw error;
    }
  }

  // 디바이스 삭제
  static async deleteDevice(id: number, force?: boolean): Promise<ApiResponse<any>> {
    try {
      const endpoint = `${this.BASE_URL}/${id}` + (force ? '?force=true' : '');
      return await apiClient.delete<any>(endpoint);
    } catch (error) {
      console.error(`디바이스 ${id} 삭제 실패:`, error);
      throw error;
    }
  }

  // 디바이스 복구
  static async restoreDevice(id: number): Promise<ApiResponse<any>> {
    try {
      return await apiClient.post<any>(`${this.BASE_URL}/${id}/restore`);
    } catch (error) {
      console.error(`디바이스 ${id} 복구 실패:`, error);
      throw error;
    }
  }

  // 디바이스 대량 업데이트 (벌크 수정)
  static async bulkUpdateDevices(ids: number[], data: Partial<Device>): Promise<ApiResponse<number>> {
    try {
      return await apiClient.put<number>(`${this.BASE_URL}/bulk`, { ids, data });
    } catch (error) {
      console.error('디바이스 대량 업데이트 실패:', error);
      throw error;
    }
  }

  // 디바이스 대량 삭제 (벌크 삭제)
  static async bulkDeleteDevices(ids: number[]): Promise<ApiResponse<number>> {
    try {
      return await apiClient.delete<number>(`${this.BASE_URL}/bulk`, { ids });
    } catch (error) {
      console.error('디바이스 대량 삭제 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 기본 디바이스 제어 API들 (DB 상태 변경)
  // ========================================================================

  // 디바이스 활성화
  static async enableDevice(id: number): Promise<ApiResponse<Device>> {
    try {
      return await apiClient.post<Device>(`${this.BASE_URL}/${id}/enable`);
    } catch (error) {
      console.error(`디바이스 ${id} 활성화 실패:`, error);
      throw error;
    }
  }

  // 디바이스 비활성화
  static async disableDevice(id: number): Promise<ApiResponse<Device>> {
    try {
      return await apiClient.post<Device>(`${this.BASE_URL}/${id}/disable`);
    } catch (error) {
      console.error(`디바이스 ${id} 비활성화 실패:`, error);
      throw error;
    }
  }

  // 연결 테스트 (기본 DB 연결성)
  static async testDeviceConnection(id: number): Promise<ApiResponse<ConnectionTestResult>> {
    try {
      return await apiClient.post<ConnectionTestResult>(`${this.BASE_URL}/${id}/test-connection`);
    } catch (error) {
      console.error(`디바이스 ${id} 연결 테스트 실패:`, error);
      throw error;
    }
  }

  // 연결 진단 (Collector 실시간 패킷 테스트)
  static async diagnoseDevice(id: number): Promise<ApiResponse<any>> {
    try {
      return await apiClient.post<any>(`${this.BASE_URL}/${id}/diagnose`);
    } catch (error) {
      console.error(`디바이스 ${id} 연결 진단 실패:`, error);
      throw error;
    }
  }

  // ========================================================================
  // 신규 추가: 네트워크 스캔 (Collector Discovery)
  // ========================================================================
  static async scanNetwork(params: {
    protocol: string;
    range?: string;
    timeout?: number;
    edgeServerId?: number;
    tenantId?: number;
  }): Promise<ApiResponse<any>> {
    try {
      return await apiClient.post<any>(ENDPOINTS.NETWORK_SCAN, params);
    } catch (error) {
      console.error('네트워크 스캔 실패:', error);
      throw error;
    }
  }

  // 네트워크 스캔 결과 조회
  static async getScanResults(params: {
    since?: string;
    protocol?: string;
  }): Promise<ApiResponse<Device[]>> {
    try {
      return await apiClient.get<Device[]>(`${this.BASE_URL}/scan/results`, params);
    } catch (error) {
      console.error('네트워크 스캔 결과 조회 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 신규 추가: Collector 워커 제어 API들 (실시간 제어)
  // ========================================================================

  // 워커 시작 (Collector 레벨)
  static async startDeviceWorker(id: number, options?: { forceRestart?: boolean }): Promise<ApiResponse<CollectorDeviceStatus>> {
    try {
      return await apiClient.post<CollectorDeviceStatus>(`${this.BASE_URL}/${id}/start`, options);
    } catch (error) {
      console.error(`디바이스 워커 ${id} 시작 실패:`, error);
      throw error;
    }
  }

  // 워커 정지 (Collector 레벨)
  static async stopDeviceWorker(id: number, options?: { graceful?: boolean }): Promise<ApiResponse<CollectorDeviceStatus>> {
    try {
      return await apiClient.post<CollectorDeviceStatus>(`${this.BASE_URL}/${id}/stop`, options || { graceful: true });
    } catch (error) {
      console.error(`디바이스 워커 ${id} 정지 실패:`, error);
      throw error;
    }
  }

  // 워커 재시작 (Collector 레벨)
  static async restartDeviceWorker(id: number, options?: { wait?: number }): Promise<ApiResponse<CollectorDeviceStatus>> {
    try {
      return await apiClient.post<CollectorDeviceStatus>(`${this.BASE_URL}/${id}/restart`, options);
    } catch (error) {
      console.error(`디바이스 워커 ${id} 재시작 실패:`, error);
      throw error;
    }
  }

  // 워커 일시정지
  static async pauseDeviceWorker(id: number): Promise<ApiResponse<CollectorDeviceStatus>> {
    try {
      return await apiClient.post<CollectorDeviceStatus>(`${this.COLLECTOR_URL.replace('/api', '')}/devices/${id}/pause`);
    } catch (error) {
      console.error(`디바이스 워커 ${id} 일시정지 실패:`, error);
      throw error;
    }
  }

  // 워커 재개
  static async resumeDeviceWorker(id: number): Promise<ApiResponse<CollectorDeviceStatus>> {
    try {
      return await apiClient.post<CollectorDeviceStatus>(`${this.COLLECTOR_URL.replace('/api', '')}/devices/${id}/resume`);
    } catch (error) {
      console.error(`디바이스 워커 ${id} 재개 실패:`, error);
      throw error;
    }
  }

  // 워커 실시간 상태 조회
  static async getDeviceWorkerStatus(id: number): Promise<ApiResponse<CollectorDeviceStatus>> {
    try {
      return await apiClient.get<CollectorDeviceStatus>(`${this.BASE_URL}/${id}/status`);
    } catch (error) {
      console.error(`디바이스 워커 ${id} 상태 조회 실패:`, error);
      throw error;
    }
  }

  // 실시간 데이터 조회
  static async getCurrentDeviceData(id: number, pointIds?: string[]): Promise<ApiResponse<any>> {
    try {
      return await apiClient.get<any>(`${this.BASE_URL}/${id}/data/current`, { point_ids: pointIds?.join(',') });
    } catch (error) {
      console.error(`디바이스 ${id} 실시간 데이터 조회 실패:`, error);
      throw error;
    }
  }

  // ========================================================================
  // 신규 추가: 하드웨어 직접 제어 API들
  // ========================================================================

  // 디지털 출력 제어 (릴레이, 솔레노이드 등)
  static async controlDigitalOutput(
    deviceId: number,
    outputId: string,
    request: DigitalControlRequest
  ): Promise<ApiResponse<HardwareControlResult>> {
    try {
      return await apiClient.post<HardwareControlResult>(`${this.BASE_URL}/${deviceId}/digital/${outputId}/control`, request);
    } catch (error) {
      console.error(`디바이스 ${deviceId} 디지털 출력 ${outputId} 제어 실패:`, error);
      throw error;
    }
  }

  // 아날로그 출력 제어 (VFD 속도, 밸브 개도 등)
  static async controlAnalogOutput(
    deviceId: number,
    outputId: string,
    request: AnalogControlRequest
  ): Promise<ApiResponse<HardwareControlResult>> {
    try {
      return await apiClient.post<HardwareControlResult>(`${this.BASE_URL}/${deviceId}/analog/${outputId}/control`, request);
    } catch (error) {
      console.error(`디바이스 ${deviceId} 아날로그 출력 ${outputId} 제어 실패:`, error);
      throw error;
    }
  }

  // 펌프 제어
  static async controlPump(
    deviceId: number,
    pumpId: string,
    request: PumpControlRequest
  ): Promise<ApiResponse<HardwareControlResult>> {
    try {
      return await apiClient.post<HardwareControlResult>(`${this.BASE_URL}/${deviceId}/pump/${pumpId}/control`, request);
    } catch (error) {
      console.error(`디바이스 ${deviceId} 펌프 ${pumpId} 제어 실패:`, error);
      throw error;
    }
  }

  // ========================================================================
  // 신규 추가: 배치 작업 API들
  // ========================================================================

  // 배치 워커 시작
  static async startMultipleDeviceWorkers(deviceIds: number[]): Promise<ApiResponse<WorkerBatchResult>> {
    try {
      return await apiClient.post<WorkerBatchResult>(`${this.BASE_URL}/batch/start`, { device_ids: deviceIds });
    } catch (error) {
      console.error('배치 워커 시작 실패:', error);
      throw error;
    }
  }

  // 배치 워커 정지
  static async stopMultipleDeviceWorkers(
    deviceIds: number[],
    options?: { graceful?: boolean }
  ): Promise<ApiResponse<WorkerBatchResult>> {
    try {
      return await apiClient.post<WorkerBatchResult>(`${this.BASE_URL}/batch/stop`, { device_ids: deviceIds, ...options });
    } catch (error) {
      console.error('배치 워커 정지 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 신규 추가: 설정 동기화 API들
  // ========================================================================

  // 디바이스 설정 재로드
  static async reloadDeviceConfig(id: number): Promise<ApiResponse<ConfigSyncResult>> {
    try {
      return await apiClient.post<ConfigSyncResult>(`${this.BASE_URL}/${id}/config/reload`);
    } catch (error) {
      console.error(`디바이스 ${id} 설정 재로드 실패:`, error);
      throw error;
    }
  }

  // 전체 설정 재로드
  static async reloadAllConfigs(): Promise<ApiResponse<ConfigSyncResult>> {
    try {
      return await apiClient.post<ConfigSyncResult>(`${this.COLLECTOR_URL}/config/reload`);
    } catch (error) {
      console.error('전체 설정 재로드 실패:', error);
      throw error;
    }
  }

  // 디바이스 설정 동기화
  static async syncDeviceSettings(id: number, settings: any): Promise<ApiResponse<ConfigSyncResult>> {
    try {
      return await apiClient.post<ConfigSyncResult>(`${this.COLLECTOR_URL}/devices/${id}/sync`, settings);
    } catch (error) {
      console.error(`디바이스 ${id} 설정 동기화 실패:`, error);
      throw error;
    }
  }

  // 설정 변경 알림
  static async notifyConfigChange(
    type: string,
    entityId: number,
    changes?: any
  ): Promise<ApiResponse<ConfigSyncResult>> {
    try {
      return await apiClient.post<ConfigSyncResult>(`${this.COLLECTOR_URL}/config/notify-change`, { type, entity_id: entityId, changes: changes || {} });
    } catch (error) {
      console.error(`설정 변경 알림 실패 (${type} ${entityId}):`, error);
      throw error;
    }
  }

  // ========================================================================
  // 기존 API들 (유지)
  // ========================================================================

  // 일괄 작업 (DB 레벨)
  static async bulkAction(data: BulkActionRequest): Promise<ApiResponse<BulkActionResult>> {
    try {
      return await apiClient.post<BulkActionResult>(`${this.BASE_URL}/bulk-action`, data);
    } catch (error) {
      console.error('디바이스 일괄 작업 실패:', error);
      throw error;
    }
  }

  // 통계 조회
  static async getDeviceStatistics(): Promise<ApiResponse<DeviceStats>> {
    try {
      return await apiClient.get<DeviceStats>(`${this.BASE_URL}/statistics`);
    } catch (error) {
      console.error('디바이스 통계 조회 실패:', error);
      throw error;
    }
  }

  // 지원 프로토콜 목록 조회
  static async getAvailableProtocols(): Promise<ApiResponse<ProtocolInfo[]>> {
    try {
      return await apiClient.get<ProtocolInfo[]>(`${this.BASE_URL}/protocols`);
    } catch (error) {
      console.error('지원 프로토콜 조회 실패:', error);
      throw error;
    }
  }

  // 데이터포인트 조회
  static async getDeviceDataPoints(
    deviceId: number,
    params?: {
      page?: number;
      limit?: number;
      data_type?: string;
      enabled_only?: boolean;
    }
  ): Promise<ApiResponse<any>> {
    try {
      return await apiClient.get<any>(`${this.BASE_URL}/${deviceId}/data-points`, params);
    } catch (error) {
      console.error(`디바이스 ${deviceId} 데이터포인트 조회 실패:`, error);
      throw error;
    }
  }

  // RTU 네트워크 정보 조회
  static async getRtuNetworks(): Promise<ApiResponse<any>> {
    try {
      return await apiClient.get<any>(`${this.BASE_URL}/rtu/networks`);
    } catch (error) {
      console.error('RTU 네트워크 조회 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 유틸리티 메서드들
  // ========================================================================

  // RTU 관련 유틸리티 메서드들
  static isRtuDevice(device: Device): boolean {
    return device.protocol_type === 'MODBUS_RTU';
  }

  static isRtuMaster(device: Device): boolean {
    return this.isRtuDevice(device) && device.rtu_info?.is_master === true;
  }

  static isRtuSlave(device: Device): boolean {
    return this.isRtuDevice(device) && device.rtu_info?.is_slave === true;
  }

  static getRtuSlaveId(device: Device): number | null {
    if (!this.isRtuSlave(device)) return null;
    return device.rtu_info?.slave_id || null;
  }

  static getRtuMasterDeviceId(device: Device): number | null {
    if (!this.isRtuSlave(device)) return null;
    return device.rtu_info?.master_device_id || null;
  }

  static getRtuSerialPort(device: Device): string | null {
    if (!this.isRtuDevice(device)) return null;
    return device.rtu_info?.serial_port || device.endpoint;
  }

  static getRtuCommunicationSettings(device: Device): {
    baud_rate: number | null;
    data_bits: number;
    stop_bits: number;
    parity: string;
  } | null {
    if (!this.isRtuDevice(device) || !device.rtu_info) return null;

    return {
      baud_rate: device.rtu_info.baud_rate,
      data_bits: device.rtu_info.data_bits,
      stop_bits: device.rtu_info.stop_bits,
      parity: device.rtu_info.parity
    };
  }

  // RTU 네트워크별 디바이스 그룹화
  static groupRtuDevicesByNetwork(devices: Device[]): { [serialPort: string]: { master: Device; slaves: Device[] } } {
    const networks: { [serialPort: string]: { master: Device; slaves: Device[] } } = {};

    const rtuMasters = devices.filter(d => this.isRtuMaster(d));

    rtuMasters.forEach(master => {
      const serialPort = this.getRtuSerialPort(master);
      if (serialPort) {
        networks[serialPort] = {
          master,
          slaves: []
        };
      }
    });

    const rtuSlaves = devices.filter(d => this.isRtuSlave(d));

    rtuSlaves.forEach(slave => {
      const masterId = this.getRtuMasterDeviceId(slave);
      if (masterId) {
        Object.values(networks).forEach(network => {
          if (network.master.id === masterId) {
            network.slaves.push(slave);
          }
        });
      }
    });

    return networks;
  }

  // 디바이스 상태 체크 유틸리티
  static isDeviceOnline(device: Device): boolean {
    return device.connection_status === 'connected' ||
      device.connection_status === 'online';
  }

  static isDeviceRunning(device: Device): boolean {
    return device.collector_status?.status === 'running' ||
      device.status === 'running';
  }

  static isDeviceEnabled(device: Device): boolean {
    return device.is_enabled;
  }

  static getDeviceLastSeen(device: Device): Date | null {
    if (!device.last_seen) return null;
    return new Date(device.last_seen);
  }

  static formatDeviceUptime(uptimeSeconds: number | undefined): string {
    if (!uptimeSeconds) return '알 수 없음';

    const hours = Math.floor(uptimeSeconds / 3600);
    const minutes = Math.floor((uptimeSeconds % 3600) / 60);
    const seconds = Math.floor(uptimeSeconds % 60);

    if (hours > 0) {
      return `${hours}시간 ${minutes}분`;
    } else if (minutes > 0) {
      return `${minutes}분 ${seconds}초`;
    } else {
      return `${seconds}초`;
    }
  }

  /**
   * 🌳 디바이스 트리 구조 조회
   * RTU Master/Slave 계층구조를 포함한 완전한 트리 데이터를 반환
   */
  static async getDeviceTreeStructure(options?: {
    include_data_points?: boolean;
    include_realtime?: boolean;
  }): Promise<ApiResponse<{
    tree: any;
    statistics: any;
    options: any;
  }>> {
    try {
      return await apiClient.get<any>('/api/devices/tree-structure', options);
    } catch (error) {
      console.error('디바이스 트리 구조 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 🔍 디바이스 트리 구조 검색
   */
  static async searchDeviceTree(criteria: {
    search?: string;
    protocol_type?: string;
    connection_status?: string;
    device_type?: string;
    include_realtime?: boolean;
  }): Promise<ApiResponse<{
    tree: any;
    total_found: number;
    search_criteria: any;
  }>> {
    try {
      return await apiClient.get<any>(`${this.BASE_URL}/tree-structure/search`, criteria);
    } catch (error) {
      console.error('디바이스 트리 검색 실패:', error);
      throw error;
    }
  }
}



// Export 기본값
export default DeviceApiService;