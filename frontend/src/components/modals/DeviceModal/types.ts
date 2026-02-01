// ============================================================================
// frontend/src/components/modals/DeviceModal/types.ts
// 🎯 디바이스 모달 관련 타입 정의 - 올바른 import 경로
// ============================================================================

import { DataPoint } from '../../../api/services/dataApi';

// ============================================================================
// 🏭 디바이스 타입 정의
// ============================================================================

export interface Device {
  id: number;
  tenant_id?: number;
  site_id?: number;
  device_group_id?: number;
  edge_server_id?: number;
  tags?: string[] | string;
  metadata?: any;
  custom_fields?: any;
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  serial_number?: string;
  protocol_type: string;
  protocol_id: number;
  protocol?: {
    id: number;
    name: string;
    type: string;
    category?: string;
    default_port?: number;
  };
  endpoint: string;
  config?: any;
  polling_interval?: number;
  timeout?: number;
  retry_count?: number;
  is_enabled: boolean;
  installation_date?: string;
  last_maintenance?: string;
  created_at?: string;
  updated_at?: string;

  // 상태 정보
  connection_status?: string;
  status?: string | any;
  last_seen?: string;
  last_communication?: string;

  // 확장 정보
  settings?: DeviceSettings;
  status_info?: DeviceStatus;

  site_name?: string;
  group_name?: string;
  groups?: DeviceGroupAssignment[];
  group_ids?: number[];
  data_points?: DataPoint[];
  data_points_count?: number;

  // 🔥 동기화 정보 (Backend partial success 대응)
  sync_warning?: string;
  sync_error?: string;
}

export interface DeviceGroupAssignment {
  id: number;
  name: string;
  is_primary?: boolean;
}

export interface DeviceSettings {
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
  scan_rate_override?: number;
  scan_group?: number;
  inter_frame_delay_ms?: number;
  read_buffer_size?: number;
  write_buffer_size?: number;
  queue_size?: number;
  backoff_multiplier?: number;
  max_backoff_time_ms?: number;
  keep_alive_timeout_s?: number;
  is_outlier_detection_enabled?: boolean;
  is_deadband_enabled?: boolean;
  created_at?: string;
  updated_at?: string;
  updated_by?: string;
}

export interface DeviceStatus {
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
}

// ============================================================================
// 🎯 모달 Props 타입
// ============================================================================

export interface DeviceModalProps {
  device: Device | null;
  isOpen: boolean;
  mode: 'view' | 'edit' | 'create';
  onClose: () => void;
  onSave?: (device: Device) => void;
  onDelete?: (deviceId: number) => void;
  onEdit?: () => void;
  initialTab?: string;
  onTabChange?: (tab: string) => void;
}

// ============================================================================
// 📊 탭 컴포넌트 Props 타입들
// ============================================================================

export interface DeviceBasicInfoTabProps {
  device?: Device | null;
  editData?: Device | null;
  mode: 'view' | 'edit' | 'create';
  onUpdateField: (field: string, value: any) => void;
  showModal?: (config: {
    type: 'confirm' | 'success' | 'error';
    title: string;
    message: string;
    confirmText?: string;
    cancelText?: string;
    onConfirm: () => void | Promise<void>;
    onCancel?: () => void;
    showCancel?: boolean;
  }) => void;
}

export interface DeviceSettingsTabProps {
  device?: Device | null;
  editData?: Device | null;
  mode: 'view' | 'edit' | 'create';
  onUpdateField: (field: string, value: any) => void;
  onUpdateSettings: (field: string, value: any) => void;
}

export interface DeviceDataPointsTabProps {
  deviceId: number;
  dataPoints: DataPoint[];
  isLoading: boolean;
  error: string | null;
  mode: 'view' | 'edit' | 'create';
  onRefresh: () => Promise<void>;
  onCreate: (dataPoint: DataPoint) => void;
  onUpdate: (dataPoint: DataPoint) => void;
  onDelete: (pointId: number) => void;
  showModal?: (config: {
    type: 'confirm' | 'success' | 'error';
    title: string;
    message: string;
    confirmText?: string;
    cancelText?: string;
    onConfirm: () => void | Promise<void>;
    onCancel?: () => void;
    showCancel?: boolean;
  }) => void;
}

export interface DeviceStatusTabProps {
  device?: Device | null;
  dataPoints?: DataPoint[];
}

export interface DeviceLogsTabProps {
  deviceId: number;
}

// ============================================================================
// 🎨 유틸리티 타입들
// ============================================================================

export type DeviceType = 'PLC' | 'HMI' | 'SENSOR' | 'ACTUATOR' | 'METER' | 'CONTROLLER' | 'GATEWAY' | 'OTHER';

export type ProtocolType =
  | 'MODBUS_TCP'
  | 'MODBUS_RTU'
  | 'MQTT'
  | 'BACNET'
  | 'OPCUA'
  | 'ETHERNET_IP'
  | 'PROFINET'
  | 'HTTP_REST';

export type ConnectionStatus = 'connected' | 'disconnected' | 'connecting' | 'error' | 'unknown';

export type DeviceMode = 'view' | 'edit' | 'create';

// ============================================================================
// 📋 폼 데이터 타입들
// ============================================================================

export interface DeviceFormData {
  name: string;
  description?: string;
  device_type: DeviceType;
  manufacturer?: string;
  model?: string;
  serial_number?: string;
  protocol_type: ProtocolType;
  endpoint: string;
  polling_interval: number;
  timeout: number;
  retry_count: number;
  is_enabled: boolean;
  installation_date?: string;
  last_maintenance?: string;
}

export interface DeviceSettingsFormData {
  polling_interval_ms: number;
  connection_timeout_ms: number;
  read_timeout_ms: number;
  write_timeout_ms: number;
  max_retry_count: number;
  retry_interval_ms: number;
  backoff_time_ms: number;
  is_keep_alive_enabled: boolean;
  keep_alive_interval_s: number;
  is_data_validation_enabled: boolean;
  is_performance_monitoring_enabled: boolean;
  is_detailed_logging_enabled: boolean;
  is_diagnostic_mode_enabled: boolean;
}

// ============================================================================
// 🔧 API 관련 타입들
// ============================================================================

export interface DeviceTestConnectionResult {
  device_id: number;
  device_name: string;
  endpoint: string;
  protocol_type: string;
  test_successful: boolean;
  response_time_ms: number;
  test_timestamp: string;
  error_message?: string;
}

export interface DeviceLogEntry {
  id: number;
  device_id: number;
  timestamp: string;
  level: 'DEBUG' | 'INFO' | 'WARN' | 'ERROR';
  category: string;
  message: string;
  details?: any;
}

export interface PacketLogEntry {
  id: number;
  device_id: number;
  timestamp: string;
  direction: 'TX' | 'RX';
  protocol: string;
  length: number;
  data_hex: string;
  data_ascii?: string;
}

export interface DeviceBulkActionResult {
  action: string;
  total: number;
  successful: number;
  failed: number;
  results: Array<{
    device_id: number;
    success: boolean;
    error?: string;
  }>;
}