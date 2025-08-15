// ============================================================================
// frontend/src/api/config.ts
// API 기본 설정 중앙화
// ============================================================================

export const API_CONFIG = {
  // 기본 URL 설정 (환경변수 우선)
  BASE_URL: process.env.REACT_APP_API_URL || 'http://localhost:3000',
  COLLECTOR_URL: process.env.REACT_APP_COLLECTOR_URL || 'http://localhost:8080',
  
  // 타임아웃 설정
  TIMEOUT: 10000, // 10초
  
  // 재시도 설정
  RETRY_COUNT: 3,
  RETRY_DELAY: 1000, // 1초
  
  // 페이지네이션 기본값
  DEFAULT_PAGE_SIZE: 50,
  MAX_PAGE_SIZE: 200,
  
  // 헤더 설정
  DEFAULT_HEADERS: {
    'Content-Type': 'application/json',
    'Accept': 'application/json'
  }
};