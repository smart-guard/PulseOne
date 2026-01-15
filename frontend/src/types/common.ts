// ============================================================================
// frontend/src/types/common.ts - 중복 상수 제거, 인터페이스만 유지
// ============================================================================

export interface ApiResponse<T = any> {
  success: boolean;
  data: T;
  message?: string;
  error?: string;
  timestamp?: string;
}

export interface PaginationParams {
  page: number;
  limit: number;
}

export interface PaginationMeta {
  current_page: number;
  total_pages: number;
  total_count: number;
  limit: number;
  has_next: boolean;
  has_prev: boolean;
}

export interface PaginatedResponse<T> {
  items: T[];
  pagination: PaginationMeta;
}

export interface PaginatedApiResponse<T> extends ApiResponse<PaginatedResponse<T>> { }

export interface BulkActionRequest<T = any> {
  action: string;
  ids: string[];
  data?: T;
}

export interface BulkActionResponse {
  success: boolean;
  processed_count: number;
  failed_count: number;
  errors?: Array<{
    item_id: number;
    error: string;
  }>;
}

export interface Tenant {
  id: number;
  company_name: string;
  company_code: string;
  domain?: string;
  contact_name?: string;
  contact_email?: string;
  contact_phone?: string;
  subscription_plan: 'starter' | 'professional' | 'enterprise';
  subscription_status: 'active' | 'trial' | 'suspended' | 'cancelled';
  max_edge_servers: number;
  max_data_points: number;
  max_users: number;
  is_active: boolean;
  trial_end_date?: string;
  created_at: string;
  updated_at: string;
}

export interface Site {
  id: number;
  tenant_id: number;
  parent_site_id?: number;
  name: string;
  code: string;
  site_type: string;
  description?: string;
  location?: string;
  address?: string;
  timezone: string;
  is_active: boolean;
  is_deleted: boolean;
  created_at: string;
  updated_at: string;
  children?: Site[];
}

// ============================================================================
// 🆕 페이지네이션 훅 관련 인터페이스 (누락된 부분 추가)
// ============================================================================

/**
 * 페이지네이션 훅 상태
 */
export interface PaginationHookState {
  currentPage: number;
  pageSize: number;
  totalCount: number;
  totalPages: number;
}

/**
 * 페이지네이션 훅 반환값
 */
export interface PaginationHookReturn extends PaginationHookState {
  hasNext: boolean;
  hasPrev: boolean;
  startIndex: number;
  endIndex: number;
  goToPage: (page: number) => void;
  changePageSize: (size: number) => void;
  goToFirst: () => void;
  goToLast: () => void;
  goToNext: () => void;
  goToPrev: () => void;
  reset: () => void;
  getPageNumbers: (maxVisible?: number) => number[];
}

/**
 * 페이지네이션 컴포넌트 Props
 */
export interface PaginationProps {
  current: number;
  total: number;
  pageSize: number;
  pageSizeOptions?: number[];
  showSizeChanger?: boolean;
  showQuickJumper?: boolean;
  showTotal?: boolean;
  onChange?: (page: number, pageSize: number) => void;
  onShowSizeChange?: (page: number, pageSize: number) => void;
  className?: string;
  size?: 'small' | 'default' | 'large';
}

// ============================================================================
// 🗑️ 중복 제거: DEFAULT_PAGINATION_CONFIG는 constants/pagination.ts에서만 관리
// ============================================================================

export const LOADING_STATES = {
  IDLE: 'idle' as const,
  LOADING: 'loading' as const,
  SUCCESS: 'success' as const,
  ERROR: 'error' as const
};

export const MODAL_MODES = {
  VIEW: 'view' as const,
  CREATE: 'create' as const,
  EDIT: 'edit' as const,
  DELETE: 'delete' as const
} as const;

/**
 * 페이지네이션 훅 반환값 (updateTotalCount 메서드 추가)
 */
export interface PaginationHookReturn extends PaginationHookState {
  hasNext: boolean;
  hasPrev: boolean;
  startIndex: number;
  endIndex: number;
  goToPage: (page: number) => void;
  changePageSize: (size: number) => void;
  updateTotalCount: (newTotal: number) => void; // 🔥 추가된 메서드
  goToFirst: () => void;
  goToLast: () => void;
  goToNext: () => void;
  goToPrev: () => void;
  reset: () => void;
  getPageNumbers: (maxVisible?: number) => number[];
}