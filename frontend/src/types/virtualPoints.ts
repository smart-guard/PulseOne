// ============================================================================
// frontend/src/types/virtualPoints.ts
// 가상포인트 관련 타입 정의 - API 응답 기준 (Extended for Renewal)
// ============================================================================

import { ApiResponse } from './common';

export interface VirtualPoint {
  id: number;
  tenant_id: number;
  name: string;
  description?: string;
  category: string;
  data_type: 'number' | 'boolean' | 'string';
  unit?: string;
  expression: string; // 수식/스크립트
  formula?: string;    // Backward compatibility
  scope_type: 'site' | 'device' | 'global';
  site_id?: number;
  device_id?: number;
  calculation_trigger: 'time_based' | 'event_driven' | 'manual';
  calculation_interval?: number; // ms
  is_enabled: boolean;
  is_deleted?: boolean; // Soft-delete flag
  created_at: string;
  updated_at: string;
  created_by?: string;

  // 실행 결과 및 UI 매칭용 필드들
  current_value?: any;
  last_calculated?: string;
  calculation_status: 'active' | 'error' | 'disabled' | 'calculating';
  error_message?: string;

  // UI 변환 대중화 필드 (optional as they are often mapped on frontend)
  execution_type?: 'periodic' | 'on_change' | 'manual';
  execution_interval?: number;

  // 메타데이터
  tags?: string[];
  priority?: number;
  decimal_places?: number;
  timeout_ms?: number;
  error_handling?: string;
  default_value?: any;
  scope_id?: number;
  min_value?: number;
  max_value?: number;

  // 관계 데이터
  inputs?: VirtualPointInput[];
  input_variables?: VirtualPointInput[]; // Alias commonly used
  site_name?: string;
  device_name?: string;
}

export interface VirtualPointInput {
  id: number;
  virtual_point_id: number;
  input_name: string;
  source_type: 'data_point' | 'virtual_point' | 'constant';
  source_id?: number;
  constant_value?: any;
  data_type: 'number' | 'boolean' | 'string';
  description?: string;
  is_required: boolean;
  variable_name?: string; // Alias
  created_at: string;
  source_name?: string;
  current_value?: any;
}

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

export interface VirtualPointInputFormData {
  input_name: string;
  variable_name: string; // required to match virtualPointsApi
  source_type: 'data_point' | 'virtual_point' | 'constant';
  source_id?: number;
  constant_value?: any;
  data_type: 'number' | 'boolean' | 'string' | string;
  description?: string;
  is_required: boolean;
}

export interface VirtualPointFilters {
  tenant_id?: number;
  site_id?: number;
  device_id?: number;
  scope_type?: 'site' | 'device' | 'global' | string;
  is_enabled?: boolean;
  include_deleted?: boolean;
  category?: string;
  calculation_trigger?: 'time_based' | 'event_driven' | 'manual';
  calculation_status?: string;  // 'active' | 'error' | 'disabled' | 'calculating' | 'all'
  search?: string;
  limit?: number;
  offset?: number;
}

/** Full virtual point page/hook state (used by useVirtualPoints) */
export interface VirtualPointPageState {
  // 데이터
  virtualPoints: VirtualPoint[];
  selectedPoint: VirtualPoint | null;
  categoryStats: any[];
  performanceStats: any;

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
