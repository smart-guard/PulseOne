// ============================================================================
// frontend/src/api/services/deviceApi.ts
// 개선된 Device API - protocol_id 직접 관리
// ============================================================================

import { API_CONFIG } from '../config';

// ============================================================================
// 프로토콜 정보 인터페이스 - ID 포함
// ============================================================================

export interface ProtocolInfo {
  id: number;                    // 🔥 추가: database ID
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
  edge_server_id?: number;
  
  // 디바이스 기본 속성
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  serial_number?: string;
  
  // 🔥 프로토콜 정보 - ID와 타입 모두 관리
  protocol_id: number;           // 데이터베이스 ID (실제 저장값)
  protocol_type: string;         // 타입 문자열 (표시용)
  endpoint: string;
  config?: any;
  
  // 🔥 프로토콜 상세 정보 (JOIN된 데이터)
  protocol?: {
    id: number;
    type: string;
    name: string;
    display_name?: string;
    default_port?: number;
    category?: string;
  };
  
  // 운영 설정
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled: boolean;
  
  // 상태 정보
  connection_status?: string;
  status?: string | any;
  last_seen?: string;
  last_communication?: string;
  
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
  created_at: string;
  updated_at: string;
  
  // 조인된 정보
  site_name?: string;
  site_code?: string;
  group_name?: string;
  group_type?: string;
}

// ============================================================================
// 요청 인터페이스 - protocol_id 사용
// ============================================================================

export interface CreateDeviceRequest {
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  protocol_id: number;           // 🔥 변경: protocol_type → protocol_id
  endpoint: string;
  config?: any;
  site_id?: number;
  device_group_id?: number;
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled: boolean;
}

export interface UpdateDeviceRequest {
  name?: string;
  description?: string;
  device_type?: string;
  manufacturer?: string;
  model?: string;
  protocol_id?: number;          // 🔥 변경: protocol_type → protocol_id
  endpoint?: string;
  config?: any;
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled?: boolean;
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

export interface GetDevicesParams {
  page?: number;
  limit?: number;
  search?: string;
  protocol_type?: string;        // 필터링용 (호환성)
  protocol_id?: number;          // 🔥 추가: ID로 필터링
  device_type?: string;
  connection_status?: string;
  status?: string;
  site_id?: number;
  device_group_id?: number;
  sort_by?: string;
  sort_order?: 'ASC' | 'DESC';
  include_rtu_relations?: boolean;
}

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  message?: string;
  error_code?: string;
  timestamp?: string;
}

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
      const response = await fetch('/api/devices/protocols');
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const apiResponse: ApiResponse<ProtocolInfo[]> = await response.json();
      
      if (apiResponse.success && apiResponse.data) {
        this.protocols = apiResponse.data;
        
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
// DeviceApiService 클래스 - protocol_id 지원
// ============================================================================

export class DeviceApiService {
  private static readonly BASE_URL = '/api/devices';
  
  // 초기화 (프로토콜 로드)
  static async initialize(): Promise<void> {
    await ProtocolManager.loadProtocols();
  }
  
  // 프로토콜 관련 메서드들
  static getProtocolManager() {
    return ProtocolManager;
  }
  
  // 디바이스 목록 조회
  static async getDevices(params?: GetDevicesParams): Promise<ApiResponse<DevicesResponse>> {
    try {
      const queryParams = new URLSearchParams();
      
      if (params) {
        Object.entries(params).forEach(([key, value]) => {
          if (value !== undefined && value !== null) {
            queryParams.append(key, value.toString());
          }
        });
      }
      
      const url = `${this.BASE_URL}?${queryParams.toString()}`;
      const response = await fetch(url);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
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
    }
  ): Promise<ApiResponse<Device>> {
    try {
      const queryParams = new URLSearchParams();
      
      if (options?.include_data_points) {
        queryParams.append('include_data_points', 'true');
      }
      
      if (options?.include_rtu_network) {
        queryParams.append('include_rtu_network', 'true');
      }
      
      const url = `${this.BASE_URL}/${id}?${queryParams.toString()}`;
      const response = await fetch(url);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
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
      
      const response = await fetch(this.BASE_URL, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
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
      
      const response = await fetch(`${this.BASE_URL}/${id}`, {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 수정 실패:`, error);
      throw error;
    }
  }

  // 디바이스 삭제
  static async deleteDevice(id: number, force?: boolean): Promise<ApiResponse<void>> {
    try {
      const url = force ? `${this.BASE_URL}/${id}?force=true` : `${this.BASE_URL}/${id}`;
      const response = await fetch(url, {
        method: 'DELETE',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 삭제 실패:`, error);
      throw error;
    }
  }

  // 디바이스 활성화/비활성화
  static async enableDevice(id: number): Promise<ApiResponse<Device>> {
    try {
      const response = await fetch(`${this.BASE_URL}/${id}/enable`, {
        method: 'POST',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 활성화 실패:`, error);
      throw error;
    }
  }

  static async disableDevice(id: number): Promise<ApiResponse<Device>> {
    try {
      const response = await fetch(`${this.BASE_URL}/${id}/disable`, {
        method: 'POST',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 비활성화 실패:`, error);
      throw error;
    }
  }

  // 연결 테스트
  static async testDeviceConnection(id: number): Promise<ApiResponse<ConnectionTestResult>> {
    try {
      const response = await fetch(`${this.BASE_URL}/${id}/test-connection`, {
        method: 'POST',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${id} 연결 테스트 실패:`, error);
      throw error;
    }
  }

  // 일괄 작업
  static async bulkAction(data: BulkActionRequest): Promise<ApiResponse<BulkActionResult>> {
    try {
      const response = await fetch(`${this.BASE_URL}/bulk-action`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('디바이스 일괄 작업 실패:', error);
      throw error;
    }
  }

  // 통계 조회
  static async getDeviceStatistics(): Promise<ApiResponse<DeviceStats>> {
    try {
      const response = await fetch(`${this.BASE_URL}/statistics`);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('디바이스 통계 조회 실패:', error);
      throw error;
    }
  }

  // 지원 프로토콜 목록 조회
  static async getAvailableProtocols(): Promise<ApiResponse<ProtocolInfo[]>> {
    try {
      const response = await fetch(`${this.BASE_URL}/protocols`);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
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
      const queryParams = new URLSearchParams();
      
      if (params) {
        Object.entries(params).forEach(([key, value]) => {
          if (value !== undefined && value !== null) {
            queryParams.append(key, value.toString());
          }
        });
      }
      
      const url = `${this.BASE_URL}/${deviceId}/data-points?${queryParams.toString()}`;
      const response = await fetch(url);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error(`디바이스 ${deviceId} 데이터포인트 조회 실패:`, error);
      throw error;
    }
  }

  // RTU 네트워크 정보 조회
  static async getRtuNetworks(): Promise<ApiResponse<any>> {
    try {
      const response = await fetch(`${this.BASE_URL}/rtu/networks`);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      return await response.json();
    } catch (error) {
      console.error('RTU 네트워크 조회 실패:', error);
      throw error;
    }
  }

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
}

// Export 기본값
export default DeviceApiService;