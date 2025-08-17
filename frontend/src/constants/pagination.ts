// ============================================================================
// frontend/src/constants/pagination.ts
// 📄 페이징 관련 상수 정의 - 완성본
// ============================================================================

/**
 * 🎯 기본 페이징 설정 상수 (기존 호환성 유지)
 */
export const DEFAULT_PAGINATION_CONFIG = {
  defaultPageSize: 25,
  pageSizeOptions: [10, 25, 50, 100],
  maxVisiblePages: 5
} as const;

/**
 * 🔧 전역 페이징 상수들
 */
export const PAGINATION_CONSTANTS = {
  // 기본 페이지 크기
  DEFAULT_PAGE_SIZE: 25,
  
  // 페이지 크기 옵션들
  PAGE_SIZE_OPTIONS: [10, 25, 50, 100],
  
  // 최대 표시 페이지 번호 개수 (1 2 3 4 5 ... 형태)
  MAX_VISIBLE_PAGES: 5,
  
  // 페이지 크기 제한
  MAX_PAGE_SIZE: 1000,
  MIN_PAGE_SIZE: 5,
  
  // 점프 페이지 설정
  ENABLE_QUICK_JUMPER: true,
  ENABLE_SIZE_CHANGER: true,
  SHOW_TOTAL_COUNT: true,
  
  // 모바일 대응
  MOBILE_MAX_VISIBLE_PAGES: 3,
  MOBILE_PAGE_SIZE_OPTIONS: [10, 25, 50]
} as const;

// ============================================================================
// 📱 페이지별 전용 페이징 설정
// ============================================================================

/**
 * 🏭 디바이스 목록 전용 페이징 설정
 */
export const DEVICE_LIST_PAGINATION = {
  DEFAULT_PAGE_SIZE: 25,
  PAGE_SIZE_OPTIONS: [10, 25, 50, 100],
  MAX_VISIBLE_PAGES: 7,
  SHOW_TOTAL_COUNT: true,
  ENABLE_SIZE_CHANGER: true,
  ENABLE_QUICK_JUMPER: false
} as const;

/**
 * 🚨 알람 관리 전용 페이징 설정
 */
export const ALARM_PAGINATION = {
  // 활성 알람
  ACTIVE_ALARMS: {
    DEFAULT_PAGE_SIZE: 20,
    PAGE_SIZE_OPTIONS: [10, 20, 50, 100],
    MAX_VISIBLE_PAGES: 5,
    AUTO_REFRESH_INTERVAL: 10000 // 10초
  },
  
  // 알람 이력
  ALARM_HISTORY: {
    DEFAULT_PAGE_SIZE: 50,
    PAGE_SIZE_OPTIONS: [25, 50, 100, 200],
    MAX_VISIBLE_PAGES: 5,
    ENABLE_DATE_FILTER: true
  },
  
  // 알람 규칙
  ALARM_RULES: {
    DEFAULT_PAGE_SIZE: 15,
    PAGE_SIZE_OPTIONS: [10, 15, 30, 50],
    MAX_VISIBLE_PAGES: 5
  }
} as const;

/**
 * 📊 데이터 관련 페이징 설정
 */
export const DATA_PAGINATION = {
  // 데이터 탐색
  DATA_EXPLORER: {
    DEFAULT_PAGE_SIZE: 50,
    PAGE_SIZE_OPTIONS: [25, 50, 100, 200, 500],
    MAX_VISIBLE_PAGES: 5,
    ENABLE_EXPORT: true
  },
  
  // 실시간 데이터
  REALTIME_DATA: {
    DEFAULT_PAGE_SIZE: 100,
    PAGE_SIZE_OPTIONS: [50, 100, 200, 500],
    MAX_VISIBLE_PAGES: 3,
    AUTO_REFRESH_INTERVAL: 5000 // 5초
  },
  
  // 이력 데이터
  HISTORICAL_DATA: {
    DEFAULT_PAGE_SIZE: 100,
    PAGE_SIZE_OPTIONS: [50, 100, 200, 500, 1000],
    MAX_VISIBLE_PAGES: 5,
    ENABLE_TIME_RANGE: true
  }
} as const;

/**
 * 👤 사용자 관리 전용 페이징 설정
 */
export const USER_MANAGEMENT_PAGINATION = {
  DEFAULT_PAGE_SIZE: 20,
  PAGE_SIZE_OPTIONS: [10, 20, 50, 100],
  MAX_VISIBLE_PAGES: 5,
  SHOW_TOTAL_COUNT: true,
  ENABLE_SEARCH: true
} as const;

/**
 * 🔮 가상포인트 전용 페이징 설정
 */
export const VIRTUAL_POINTS_PAGINATION = {
  DEFAULT_PAGE_SIZE: 30,
  PAGE_SIZE_OPTIONS: [15, 30, 50, 100],
  MAX_VISIBLE_PAGES: 5,
  ENABLE_CATEGORY_FILTER: true
} as const;

/**
 * 🏢 사이트 관리 전용 페이징 설정
 */
export const SITE_MANAGEMENT_PAGINATION = {
  DEFAULT_PAGE_SIZE: 15,
  PAGE_SIZE_OPTIONS: [10, 15, 30, 50],
  MAX_VISIBLE_PAGES: 5,
  ENABLE_HIERARCHY_VIEW: true
} as const;

/**
 * 📈 시스템 모니터링 전용 페이징 설정
 */
export const MONITORING_PAGINATION = {
  // 시스템 로그
  SYSTEM_LOGS: {
    DEFAULT_PAGE_SIZE: 100,
    PAGE_SIZE_OPTIONS: [50, 100, 200, 500],
    MAX_VISIBLE_PAGES: 5,
    AUTO_REFRESH_INTERVAL: 15000 // 15초
  },
  
  // 성능 메트릭
  PERFORMANCE_METRICS: {
    DEFAULT_PAGE_SIZE: 50,
    PAGE_SIZE_OPTIONS: [25, 50, 100, 200],
    MAX_VISIBLE_PAGES: 3
  }
} as const;

// ============================================================================
// 🎨 UI 스타일 관련 상수
// ============================================================================

/**
 * 페이징 UI 스타일 상수
 */
export const PAGINATION_UI = {
  // 크기 옵션
  SIZES: {
    SMALL: 'small',
    DEFAULT: 'default', 
    LARGE: 'large'
  } as const,
  
  // 테마
  THEMES: {
    DEFAULT: 'default',
    COMPACT: 'compact',
    MODERN: 'modern'
  } as const,
  
  // 아이콘
  ICONS: {
    FIRST: '««',
    PREV: '‹',
    NEXT: '›',
    LAST: '»»',
    ELLIPSIS: '...'
  } as const,
  
  // CSS 클래스
  CSS_CLASSES: {
    WRAPPER: 'pagination-wrapper',
    NAVIGATION: 'pagination-navigation',
    BUTTON: 'pagination-button',
    CURRENT: 'current',
    DISABLED: 'disabled',
    INFO: 'pagination-info',
    SIZE_CHANGER: 'pagination-size-changer',
    QUICK_JUMPER: 'pagination-quick-jumper'
  } as const
} as const;

// ============================================================================
// 🔧 헬퍼 함수들
// ============================================================================

/**
 * 페이지별 설정 가져오기
 */
export const getPaginationConfig = (pageType: keyof typeof PAGE_CONFIGS) => {
  return PAGE_CONFIGS[pageType] || DEFAULT_PAGINATION_CONFIG;
};

/**
 * 모바일 여부에 따른 설정 조정
 */
export const getResponsivePaginationConfig = (isMobile: boolean, baseConfig: any) => {
  if (!isMobile) return baseConfig;
  
  return {
    ...baseConfig,
    maxVisiblePages: PAGINATION_CONSTANTS.MOBILE_MAX_VISIBLE_PAGES,
    pageSizeOptions: PAGINATION_CONSTANTS.MOBILE_PAGE_SIZE_OPTIONS,
    enableQuickJumper: false
  };
};

/**
 * 페이지 설정 통합 맵
 */
export const PAGE_CONFIGS = {
  // 디바이스 관련
  'device-list': DEVICE_LIST_PAGINATION,
  'device-data-points': DATA_PAGINATION.DATA_EXPLORER,
  
  // 알람 관련  
  'active-alarms': ALARM_PAGINATION.ACTIVE_ALARMS,
  'alarm-history': ALARM_PAGINATION.ALARM_HISTORY,
  'alarm-rules': ALARM_PAGINATION.ALARM_RULES,
  
  // 데이터 관련
  'data-explorer': DATA_PAGINATION.DATA_EXPLORER,
  'realtime-monitor': DATA_PAGINATION.REALTIME_DATA,
  'historical-data': DATA_PAGINATION.HISTORICAL_DATA,
  
  // 가상포인트
  'virtual-points': VIRTUAL_POINTS_PAGINATION,
  
  // 시스템 관리
  'user-management': USER_MANAGEMENT_PAGINATION,
  'site-management': SITE_MANAGEMENT_PAGINATION,
  'system-logs': MONITORING_PAGINATION.SYSTEM_LOGS,
  'performance-metrics': MONITORING_PAGINATION.PERFORMANCE_METRICS,
  
  // 기본값
  'default': DEFAULT_PAGINATION_CONFIG
} as const;

// ============================================================================
// 📊 페이징 통계 및 유틸리티
// ============================================================================

/**
 * 페이징 성능 메트릭
 */
export const PAGINATION_METRICS = {
  // 권장 최대 항목 수 (성능 고려)
  RECOMMENDED_MAX_ITEMS: 10000,
  
  // 가상 스크롤링 임계값
  VIRTUAL_SCROLL_THRESHOLD: 1000,
  
  // 서버 사이드 페이징 임계값
  SERVER_SIDE_THRESHOLD: 500,
  
  // 무한 스크롤 임계값
  INFINITE_SCROLL_THRESHOLD: 200
} as const;

/**
 * 타입 정의 (재사용)
 */
export type PaginationConfigKey = keyof typeof PAGE_CONFIGS;
export type PaginationSize = keyof typeof PAGINATION_UI.SIZES;
export type PaginationTheme = keyof typeof PAGINATION_UI.THEMES;

// ============================================================================
// 🔗 내보내기 통합
// ============================================================================

/**
 * 모든 페이징 관련 상수를 하나로 통합
 */
export const PAGINATION_CONFIG = {
  DEFAULT: DEFAULT_PAGINATION_CONFIG,
  CONSTANTS: PAGINATION_CONSTANTS,
  PAGES: PAGE_CONFIGS,
  UI: PAGINATION_UI,
  METRICS: PAGINATION_METRICS,
  
  // 헬퍼 함수들
  getConfig: getPaginationConfig,
  getResponsiveConfig: getResponsivePaginationConfig
} as const;

// 기본 내보내기
export default PAGINATION_CONFIG;