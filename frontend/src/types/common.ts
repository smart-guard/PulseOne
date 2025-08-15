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

export interface PaginatedApiResponse<T> extends ApiResponse<PaginatedResponse<T>> {}

export interface BulkActionResponse {
  success: boolean;
  processed_count: number;
  failed_count: number;
  errors?: Array<{
    item_id: number;
    error: string;
  }>;
}

export const DEFAULT_PAGINATION_CONFIG = {
  defaultPageSize: 50,
  pageSizeOptions: [25, 50, 100, 200],
  showSizeChanger: true,
  showQuickJumper: true,
  showTotal: true
};

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
