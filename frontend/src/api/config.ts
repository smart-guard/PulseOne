// ============================================================================
// frontend/src/api/config.ts
// Vite 환경변수 호환 버전
// ============================================================================

export const API_CONFIG = {
  // 🔧 Vite에서는 import.meta.env 사용
  // Proxy 사용을 위해 기본값을 빈 문자열로 설정 (absolute URL 대신 relative URL 사용)
  BASE_URL: import.meta.env.VITE_API_URL || '',
  COLLECTOR_URL: import.meta.env.VITE_COLLECTOR_URL || 'http://localhost:8080',

  // ⚙️ 기본 설정들
  TIMEOUT: 10000, // 10초
  DEFAULT_PAGE_SIZE: 50,
  MAX_PAGE_SIZE: 200,

  // 📄 페이지 크기 옵션들
  PAGE_SIZE_OPTIONS: [25, 50, 100, 200],

  // 🔄 폴링 설정
  POLLING_INTERVAL: 5000, // 5초

  // 📱 헤더 설정
  DEFAULT_HEADERS: {
    'Content-Type': 'application/json',
    'Accept': 'application/json'
  }
};

// TypeScript를 위한 환경변수 타입 확장
declare global {
  interface ImportMetaEnv {
    readonly VITE_API_URL?: string;
    readonly VITE_COLLECTOR_URL?: string;
    readonly MODE: string;
    readonly DEV: boolean;
    readonly PROD: boolean;
  }

  interface ImportMeta {
    readonly env: ImportMetaEnv;
  }
}