// ============================================================================
// frontend/src/constants/pagination.ts
// 페이징 관련 상수 정의
// ============================================================================

/**
 * 페이징 기본 설정 상수 - 기존 DEFAULT_PAGINATION_CONFIG와 호환
 */
export const DEFAULT_PAGINATION_CONFIG = {
  defaultPageSize: 10,
  pageSizeOptions: [10, 25, 50, 100],
  maxVisiblePages: 5
} as const;

/**
 * 페이징 기본 설정 상수
 */
export const PAGINATION_CONSTANTS = {
  // 기본 페이지 크기
  DEFAULT_PAGE_SIZE: 10,
  
  // 페이지 크기 옵션들
  PAGE_SIZE_OPTIONS: [10, 25, 50, 100],
  
  // 최대 표시 페이지 번호 개수 (1 2 3 ... 형태)
  MAX_VISIBLE_PAGES: 5,
  
  // 최대 페이지 크기 제한
  MAX_PAGE_SIZE: 200,
  
  // 최소 페이지 크기
  MIN_PAGE_SIZE: 5
} as const;

/**
 * 디바이스 목록 전용 페이징 설정
 */
export const DEVICE_LIST_PAGINATION = {
  DEFAULT_PAGE_SIZE: 10,
  PAGE_SIZE_OPTIONS: [10, 20, 50, 100],
  MAX_VISIBLE_PAGES: 7
} as const;

/**
 * 알람 이력 전용 페이징 설정
 */
export const ALARM_HISTORY_PAGINATION = {
  DEFAULT_PAGE_SIZE: 25,
  PAGE_SIZE_OPTIONS: [25, 50, 100, 200],
  MAX_VISIBLE_PAGES: 5
} as const;

/**
 * 사용자 관리 전용 페이징 설정
 */
export const USER_MANAGEMENT_PAGINATION = {
  DEFAULT_PAGE_SIZE: 15,
  PAGE_SIZE_OPTIONS: [15, 30, 50, 100],
  MAX_VISIBLE_PAGES: 5
} as const;

/**
 * 데이터 탐색 전용 페이징 설정
 */
export const DATA_EXPLORER_PAGINATION = {
  DEFAULT_PAGE_SIZE: 50,
  PAGE_SIZE_OPTIONS: [50, 100, 200, 500],
  MAX_VISIBLE_PAGES: 5
} as const;