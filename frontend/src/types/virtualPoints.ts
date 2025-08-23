// ============================================================================
// frontend/src/types/virtualPoints.ts
// 가상포인트 관련 타입 정의 - API 응답 기준
// ============================================================================

import { ApiResponse, PaginationMeta } from './common';

// 기본 가상포인트 인터페이스 - API 응답과 일치
export interface VirtualPoint {
  id: number;
  tenant_id: number;
  name: string;
  description?: string;
  category: string;
  data_type: 'number' | 'boolean' | 'string';
  unit?: string;
  expression: string; // 수식/스크립트
  scope_type: 'site' | 'device' | 'global';
  site_id?: number;
  device_id?: number;
  calculation_trigger: 'time_based' | 'event_driven' | 'manual';
  calculation_interval?: number; // ms
  is_enabled: boolean;
  created_at: string;
  updated_at: string;
  created_by?: string;
  
  // 실행 결과 관련
  current_value?: any;
  last_calculated?: string;
  calculation_status: 'active' | 'error' | 'disabled' | 'calculating';
  error_message?: string;
  
  // 메타데이터
  tags?: string[];
  priority?: number;
  
  // 관계 데이터
  inputs?: VirtualPointInput[];
  site_name?: string;
  device_name?: string;
}

// 가상포인트 입력 변수
export interface VirtualPointInput {
  id: number;
  virtual_point_id: number;
  input_name: string; // 스크립트에서 사용할 변수명
  source_type: 'data_point' | 'virtual_point' | 'constant';
  source_id?: number;
  constant_value?: any;
  data_type: 'number' | 'boolean' | 'string';
  description?: string;
  is_required: boolean;
  created_at: string;
  
  // 관계 데이터
  source_name?: string;
  current_value?: any;
}

// 가상포인트 생성/수정용 폼 데이터
export interface VirtualPointFormData {
  name: string;
  description?: string;
  category: string;
  data_type: 'number' | 'boolean' | 'string';
  unit?: string;
  expression: string;
  scope_type: 'site' | 'device' | 'global';
  site_id?: number;
  device_id?: number;
  calculation_trigger: 'time_based' | 'event_driven' | 'manual';
  calculation_interval?: number;
  is_enabled: boolean;
  tags?: string[];
  priority?: number;
}

// 가상포인트 입력 변수 폼 데이터
export interface VirtualPointInputFormData {
  input_name: string;
  source_type: 'data_point' | 'virtual_point' | 'constant';
  source_id?: number;
  constant_value?: any;
  data_type: 'number' | 'boolean' | 'string';
  description?: string;
  is_required: boolean;
}

// 가상포인트 필터
export interface VirtualPointFilters {
  tenant_id?: number;
  site_id?: number;
  device_id?: number;
  scope_type?: 'site' | 'device' | 'global';
  is_enabled?: boolean;
  category?: string;
  calculation_trigger?: 'time_based' | 'event_driven' | 'manual';
  search?: string;
  limit?: number;
  offset?: number;
}

// 가상포인트 통계
export interface VirtualPointCategoryStats {
  category: string;
  count: number;
  enabled_count: number;
  disabled_count: number;
  error_count: number;
}

export interface VirtualPointPerformanceStats {
  total_count: number;
  active_count: number;
  error_count: number;
  avg_calculation_time?: number;
  max_calculation_time?: number;
  calculations_per_minute?: number;
  memory_usage?: number;
}

// 스크립트 테스트 관련
export interface ScriptTestRequest {
  expression: string;
  variables: Record<string, any>;
  data_type: 'number' | 'boolean' | 'string';
}

export interface ScriptTestResult {
  success: boolean;
  result?: any;
  error_message?: string;
  execution_time?: number;
  warnings?: string[];
}

// 가상포인트 실행 요청
export interface VirtualPointExecuteRequest {
  force_recalculation?: boolean;
  include_inputs?: boolean;
}

export interface VirtualPointExecuteResult {
  success: boolean;
  value?: any;
  calculation_time?: number;
  error_message?: string;
  inputs_used?: Record<string, any>;
}

// API 응답 타입들
export type VirtualPointsApiResponse = ApiResponse<VirtualPoint[]>;
export type VirtualPointApiResponse = ApiResponse<VirtualPoint>;
export type VirtualPointCategoryStatsApiResponse = ApiResponse<VirtualPointCategoryStats[]>;
export type VirtualPointPerformanceStatsApiResponse = ApiResponse<VirtualPointPerformanceStats>;
export type ScriptTestApiResponse = ApiResponse<ScriptTestResult>;
export type VirtualPointExecuteApiResponse = ApiResponse<VirtualPointExecuteResult>;

// 페이지 상태 관리용 인터페이스
export interface VirtualPointPageState {
  // 데이터
  virtualPoints: VirtualPoint[];
  selectedPoint: VirtualPoint | null;
  categoryStats: VirtualPointCategoryStats[];
  performanceStats: VirtualPointPerformanceStats | null;
  
  // UI 상태  
  loading: boolean;
  saving: boolean;
  testing: boolean;
  error: string | null;
  
  // 모달 상태
  showCreateModal: boolean;
  showEditModal: boolean;
  showDeleteConfirm: boolean;
  showTestModal: boolean;
  
  // 필터 상태
  filters: VirtualPointFilters;
  viewMode: 'card' | 'table';
  sortBy: string;
  sortOrder: 'asc' | 'desc';
  
  // 페이징
  currentPage: number;
  pageSize: number;
  totalCount: number;
}

// 뷰 모드 및 정렬 옵션
export const VIEW_MODES = ['card', 'table'] as const;
export const SORT_OPTIONS = [
  { value: 'name', label: '이름순' },
  { value: 'category', label: '카테고리순' },
  { value: 'updated_at', label: '수정일순' },
  { value: 'calculation_status', label: '상태순' },
  { value: 'priority', label: '우선순위순' }
] as const;

// 카테고리 옵션 (실제로는 API에서 가져올 수 있음)
export const CATEGORY_OPTIONS = [
  'Temperature',
  'Pressure', 
  'Flow',
  'Power',
  'Production',
  'Quality',
  'Safety',
  'Maintenance',
  'Energy',
  'Custom'
] as const;

// 계산 트리거 옵션
export const CALCULATION_TRIGGER_OPTIONS = [
  { value: 'time_based', label: '시간 기반' },
  { value: 'event_driven', label: '이벤트 기반' }, 
  { value: 'manual', label: '수동 실행' }
] as const;

// 데이터 타입 옵션
export const DATA_TYPE_OPTIONS = [
  { value: 'number', label: '숫자' },
  { value: 'boolean', label: '불린' },
  { value: 'string', label: '문자열' }
] as const;

// 스코프 타입 옵션
export const SCOPE_TYPE_OPTIONS = [
  { value: 'global', label: '전역' },
  { value: 'site', label: '사이트' },
  { value: 'device', label: '디바이스' }
] as const;