// ============================================================================
// frontend/src/api/config.ts
// 🔧 수정된 Frontend API 설정 - 개발환경 최적화
// ============================================================================

// 🔥 환경 감지 및 동적 URL 설정
function getApiBaseUrl(): string {
  // 1. 환경변수에서 확인
  if (import.meta.env.VITE_API_URL) {
    return import.meta.env.VITE_API_URL;
  }
  
  // 2. 현재 호스트 기반으로 자동 설정
  if (typeof window !== 'undefined') {
    const hostname = window.location.hostname;
    const protocol = window.location.protocol;
    
    // 개발 환경 감지
    if (hostname === 'localhost' || hostname === '127.0.0.1' || hostname === '0.0.0.0') {
      return `${protocol}//${hostname}:3000`;
    }
    
    // 프로덕션 환경에서는 같은 호스트 사용
    return `${protocol}//${hostname}:3000`;
  }
  
  // 3. 기본값
  return 'http://localhost:3000';
}

function getCollectorUrl(): string {
  if (import.meta.env.VITE_COLLECTOR_URL) {
    return import.meta.env.VITE_COLLECTOR_URL;
  }
  
  if (typeof window !== 'undefined') {
    const hostname = window.location.hostname;
    const protocol = window.location.protocol;
    return `${protocol}//${hostname}:8080`;
  }
  
  return 'http://localhost:8080';
}

function getWebSocketUrl(): string {
  const baseUrl = getApiBaseUrl();
  return baseUrl.replace(/^http/, 'ws');
}

// ============================================================================
// 🔧 API 설정 객체
// ============================================================================

export const API_CONFIG = {
  // 🌐 기본 URL들
  BASE_URL: getApiBaseUrl(),
  COLLECTOR_URL: getCollectorUrl(), 
  WS_URL: getWebSocketUrl(),
  
  // ⚙️ 타임아웃 및 재시도 설정
  TIMEOUT: 30000, // 30초 (개발환경에서 더 길게)
  RETRY_COUNT: 3,
  RETRY_DELAY: 1000, // 1초
  
  // 📄 페이지네이션 설정
  DEFAULT_PAGE_SIZE: 25,
  MAX_PAGE_SIZE: 200,
  PAGE_SIZE_OPTIONS: [10, 25, 50, 100, 200],
  
  // 🔄 실시간 데이터 설정
  POLLING_INTERVAL: 5000, // 5초
  WEBSOCKET_RECONNECT_DELAY: 2000, // 2초
  MAX_RECONNECT_ATTEMPTS: 5,
  
  // 📱 HTTP 헤더 설정
  DEFAULT_HEADERS: {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
    'X-Requested-With': 'XMLHttpRequest'
  },
  
  // 🏷️ 캐시 설정
  CACHE_TTL: 300000, // 5분 (ms)
  ENABLE_CACHE: true,
  
  // 🔍 디버그 설정
  DEBUG_API_CALLS: import.meta.env.DEV, // Vite의 개발 모드 감지
  LOG_LEVEL: import.meta.env.DEV ? 'debug' : 'error'
};

// ============================================================================
// 🧪 환경별 설정 오버라이드
// ============================================================================

// 개발 환경 특화 설정
if (import.meta.env.DEV) {
  Object.assign(API_CONFIG, {
    TIMEOUT: 60000, // 개발환경에서는 더 긴 타임아웃
    DEBUG_API_CALLS: true,
    LOG_LEVEL: 'debug'
  });
  
  console.log('🔧 개발 환경 API 설정:', {
    BASE_URL: API_CONFIG.BASE_URL,
    COLLECTOR_URL: API_CONFIG.COLLECTOR_URL,
    WS_URL: API_CONFIG.WS_URL,
    TIMEOUT: API_CONFIG.TIMEOUT
  });
}

// 프로덕션 환경 특화 설정
if (import.meta.env.PROD) {
  Object.assign(API_CONFIG, {
    TIMEOUT: 10000, // 프로덕션에서는 더 짧은 타임아웃
    DEBUG_API_CALLS: false,
    LOG_LEVEL: 'error',
    RETRY_COUNT: 2 // 프로덕션에서는 더 적은 재시도
  });
}

// ============================================================================
// 🌍 환경변수 타입 정의 (TypeScript)
// ============================================================================

declare global {
  interface ImportMetaEnv {
    readonly VITE_API_URL?: string;
    readonly VITE_COLLECTOR_URL?: string;
    readonly VITE_WS_URL?: string;
    readonly VITE_DEBUG_API?: string;
    readonly VITE_ENABLE_MOCK?: string;
    readonly DEV: boolean;
    readonly PROD: boolean;
  }

  interface ImportMeta {
    readonly env: ImportMetaEnv;
  }
}

// ============================================================================
// 🔧 유틸리티 함수들
// ============================================================================

/**
 * API URL 유효성 검사
 */
export function validateApiConfig(): {
  isValid: boolean;
  errors: string[];
  warnings: string[];
} {
  const errors: string[] = [];
  const warnings: string[] = [];
  
  // BASE_URL 검사
  try {
    new URL(API_CONFIG.BASE_URL);
  } catch {
    errors.push(`Invalid BASE_URL: ${API_CONFIG.BASE_URL}`);
  }
  
  // COLLECTOR_URL 검사
  try {
    new URL(API_CONFIG.COLLECTOR_URL);
  } catch {
    warnings.push(`Invalid COLLECTOR_URL: ${API_CONFIG.COLLECTOR_URL}`);
  }
  
  // 타임아웃 검사
  if (API_CONFIG.TIMEOUT < 1000) {
    warnings.push(`Timeout is very short: ${API_CONFIG.TIMEOUT}ms`);
  }
  
  // 개발환경에서 localhost 체크
  if (import.meta.env.DEV) {
    if (!API_CONFIG.BASE_URL.includes('localhost') && !API_CONFIG.BASE_URL.includes('127.0.0.1')) {
      warnings.push('Development mode but not using localhost');
    }
  }
  
  return {
    isValid: errors.length === 0,
    errors,
    warnings
  };
}

/**
 * 동적 API URL 생성
 */
export function buildApiUrl(endpoint: string, params?: Record<string, any>): string {
  let url = `${API_CONFIG.BASE_URL}${endpoint}`;
  
  if (params) {
    const searchParams = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined && value !== null) {
        searchParams.append(key, String(value));
      }
    });
    
    const queryString = searchParams.toString();
    if (queryString) {
      url += `?${queryString}`;
    }
  }
  
  return url;
}

/**
 * WebSocket URL 생성
 */
export function buildWebSocketUrl(endpoint: string): string {
  return `${API_CONFIG.WS_URL}${endpoint}`;
}

/**
 * 네트워크 연결 테스트
 */
export async function testNetworkConnection(): Promise<{
  backend: boolean;
  collector: boolean;
  latency: number;
}> {
  const startTime = performance.now();
  
  try {
    // Backend 연결 테스트
    const backendResponse = await fetch(`${API_CONFIG.BASE_URL}/health`, {
      method: 'GET',
      timeout: 5000
    });
    const backendOk = backendResponse.ok;
    
    // Collector 연결 테스트 (옵셔널)
    let collectorOk = false;
    try {
      const collectorResponse = await fetch(`${API_CONFIG.COLLECTOR_URL}/health`, {
        method: 'GET',
        timeout: 3000
      });
      collectorOk = collectorResponse.ok;
    } catch {
      collectorOk = false;
    }
    
    const endTime = performance.now();
    const latency = endTime - startTime;
    
    return {
      backend: backendOk,
      collector: collectorOk,
      latency: Math.round(latency)
    };
    
  } catch (error) {
    console.error('Network connection test failed:', error);
    return {
      backend: false,
      collector: false,
      latency: -1
    };
  }
}

/**
 * 개발 환경에서 설정 정보 출력
 */
export function logApiConfig(): void {
  if (!import.meta.env.DEV) return;
  
  console.group('🔧 PulseOne API Configuration');
  console.log('Environment:', import.meta.env.MODE);
  console.log('Backend URL:', API_CONFIG.BASE_URL);
  console.log('Collector URL:', API_CONFIG.COLLECTOR_URL);
  console.log('WebSocket URL:', API_CONFIG.WS_URL);
  console.log('Timeout:', `${API_CONFIG.TIMEOUT}ms`);
  console.log('Debug Mode:', API_CONFIG.DEBUG_API_CALLS);
  
  const validation = validateApiConfig();
  if (validation.errors.length > 0) {
    console.error('❌ Configuration errors:', validation.errors);
  }
  if (validation.warnings.length > 0) {
    console.warn('⚠️ Configuration warnings:', validation.warnings);
  }
  console.groupEnd();
}

// 개발 환경에서 자동으로 설정 정보 출력
if (import.meta.env.DEV) {
  // 약간의 지연 후 출력 (다른 초기화 로그와 겹치지 않게)
  setTimeout(logApiConfig, 100);
}

// ============================================================================
// 🔄 핫 리로드 지원 (Vite HMR)
// ============================================================================

if (import.meta.hot) {
  import.meta.hot.accept(() => {
    console.log('🔄 API Config hot reloaded');
    logApiConfig();
  });
}

export default API_CONFIG;