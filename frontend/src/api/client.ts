// ============================================================================
// frontend/src/api/client.ts
// 🔧 완전 통일된 Fetch 기반 API 클라이언트 (axios 완전 대체)
// ============================================================================

import { API_CONFIG } from './config';

// ============================================================================
// 🔧 공통 타입 정의
// ============================================================================

export interface ApiResponse<T> {
  success: boolean;
  data: T | null;
  message: string;
  error?: string;
  timestamp: string;
}

export interface RequestConfig {
  method?: 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH';
  headers?: Record<string, string>;
  timeout?: number;
  baseUrl?: string;
  body?: any;
}

// ============================================================================
// 🌐 통합 HTTP 클라이언트 클래스 (모든 API 서비스에서 공용)
// ============================================================================

class UnifiedHttpClient {
  private baseUrl: string;
  private defaultTimeout: number;
  private defaultHeaders: Record<string, string>;

  constructor(
    baseUrl: string = API_CONFIG.BASE_URL,
    timeout: number = API_CONFIG.TIMEOUT,
    headers: Record<string, string> = API_CONFIG.DEFAULT_HEADERS
  ) {
    this.baseUrl = baseUrl;
    this.defaultTimeout = timeout;
    this.defaultHeaders = headers;
  }

  // ========================================================================
  // 🔧 요청 전처리 (axios interceptors.request 대체)
  // ========================================================================

  private preprocessRequest(endpoint: string, config: RequestConfig = {}): [string, RequestInit] {
    const url = config.baseUrl ?
      `${config.baseUrl}${endpoint}` :
      `${this.baseUrl}${endpoint}`;

    // 🔄 API 요청 로깅 (기존 axios 패턴과 동일)
    console.log(`🔄 API 요청: ${(config.method || 'GET').toUpperCase()} ${url}`);

    // 🔐 인증 토큰 자동 추가 (기존 axios 인터셉터와 동일)
    let token = localStorage.getItem('auth_token');

    // 🛠️ 개발 환경에서 토큰이 없으면 더미 토큰 추가
    if (!token && import.meta.env.MODE === 'development') {
      token = 'dev-dummy-token';
    }

    const headers = {
      ...this.defaultHeaders,
      ...config.headers,
      ...(token && { Authorization: `Bearer ${token}` })
    };

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), config.timeout || this.defaultTimeout);

    const fetchConfig: RequestInit = {
      method: config.method || 'GET',
      headers,
      signal: controller.signal,
      ...(config.body && {
        body: typeof config.body === 'string' ? config.body : JSON.stringify(config.body)
      })
    };

    // 타임아웃 관리를 위해 저장
    (fetchConfig as any)._timeoutId = timeoutId;

    return [url, fetchConfig];
  }

  // ========================================================================
  // 🔧 응답 후처리 (axios interceptors.response 대체)
  // ========================================================================

  private async processResponse<T>(response: Response, url: string, fetchConfig: RequestInit): Promise<ApiResponse<T>> {
    // 타임아웃 정리
    if ((fetchConfig as any)._timeoutId) {
      clearTimeout((fetchConfig as any)._timeoutId);
    }

    console.log(`✅ API 응답: ${response.status} ${url}`);

    try {
      // 🚨 에러 상태 처리 (기존 axios 인터셉터와 동일)
      if (!response.ok) {
        const errorData = await response.json().catch(() => ({}));

        // 기존 axios 에러 처리 로직과 동일
        switch (response.status) {
          case 401:
            console.warn('🔐 인증이 만료되었습니다. 다시 로그인해주세요.');
            localStorage.removeItem('auth_token');
            // 🛠️ 개발 환경에서는 404/401 에러 시 /login으로 튕기지 않도록 함
            if (import.meta.env.MODE !== 'development') {
              window.location.href = '/login';
            } else {
              console.warn('🛠️ [DEV] 401 에러 발생 - 로그인 페이지 리다이렉트 중단됨');
            }
            break;
          case 403:
            console.warn('🚫 접근 권한이 없습니다.');
            break;
          case 404:
            console.warn('🔍 요청한 리소스를 찾을 수 없습니다.');
            break;
          case 500:
            console.error('🔥 서버에서 오류가 발생했습니다.');
            break;
          default:
            console.error('⚠️ 알 수 없는 오류가 발생했습니다.');
        }

        return {
          success: false,
          data: null,
          message: errorData.message || `HTTP ${response.status}`,
          error: errorData.error || response.statusText,
          timestamp: new Date().toISOString()
        };
      }

      // ✅ 성공 응답 처리
      const data = await response.json();

      // Backend 응답이 이미 ApiResponse 형식인 경우
      if ('success' in data && typeof data.success === 'boolean') {
        return data;
      }

      // 일반 데이터를 ApiResponse로 래핑
      return {
        success: true,
        data: data,
        message: 'Request successful',
        timestamp: new Date().toISOString()
      };

    } catch (parseError) {
      // 타임아웃 정리
      if ((fetchConfig as any)._timeoutId) {
        clearTimeout((fetchConfig as any)._timeoutId);
      }

      console.error('❌ 응답 파싱 실패:', parseError);

      return {
        success: false,
        data: null,
        message: 'Failed to parse response',
        error: parseError instanceof Error ? parseError.message : 'Parse error',
        timestamp: new Date().toISOString()
      };
    }
  }

  // ========================================================================
  // 🔧 HTTP 메서드들 (axios API 완전 호환)
  // ========================================================================

  async request<T>(config: RequestConfig & { url: string }): Promise<ApiResponse<T>> {
    try {
      const [url, fetchConfig] = this.preprocessRequest(config.url, config);
      const response = await fetch(url, fetchConfig);
      return await this.processResponse<T>(response, url, fetchConfig);

    } catch (error) {
      console.error(`❌ API 요청 실패: ${config.url}`, error);

      // AbortError (타임아웃) 처리
      if (error instanceof Error && error.name === 'AbortError') {
        return {
          success: false,
          data: null,
          message: 'Request timeout',
          error: 'The request took too long to complete',
          timestamp: new Date().toISOString()
        };
      }

      return {
        success: false,
        data: null,
        message: 'Request failed',
        error: error instanceof Error ? error.message : 'Unknown error',
        timestamp: new Date().toISOString()
      };
    }
  }

  async get<T>(url: string, params?: Record<string, any>): Promise<ApiResponse<T>> {
    // 쿼리 파라미터 처리
    let finalUrl = url;
    if (params) {
      const queryParams = new URLSearchParams();
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });

      if (queryParams.toString()) {
        finalUrl += `?${queryParams.toString()}`;
      }
    }

    return this.request<T>({ url: finalUrl, method: 'GET' });
  }

  async post<T>(url: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>({
      url,
      method: 'POST',
      body: data,
      headers: data ? { 'Content-Type': 'application/json' } : {}
    });
  }

  async put<T>(url: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>({
      url,
      method: 'PUT',
      body: data,
      headers: data ? { 'Content-Type': 'application/json' } : {}
    });
  }

  async delete<T>(url: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>({
      url,
      method: 'DELETE',
      body: data,
      headers: data ? { 'Content-Type': 'application/json' } : {}
    });
  }

  async patch<T>(url: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>({
      url,
      method: 'PATCH',
      body: data,
      headers: data ? { 'Content-Type': 'application/json' } : {}
    });
  }

  // ========================================================================
  // 🔧 Axios 호환 속성들
  // ========================================================================

  get defaults() {
    return {
      baseURL: this.baseUrl,
      timeout: this.defaultTimeout,
      headers: this.defaultHeaders
    };
  }

  // interceptors 속성 (호환성을 위해)
  interceptors = {
    request: {
      use: (onFulfilled?: any, onRejected?: any) => {
        console.log('🔧 Request interceptor registered (compatibility mode)');
        // 실제 구현은 preprocessRequest에서 처리됨
      }
    },
    response: {
      use: (onFulfilled?: any, onRejected?: any) => {
        console.log('🔧 Response interceptor registered (compatibility mode)');
        // 실제 구현은 processResponse에서 처리됨
      }
    }
  };
}

// ============================================================================
// 🏭 클라이언트 인스턴스 생성 (기존 axios.create 패턴과 동일)
// ============================================================================

// 기본 API 클라이언트 생성
export const apiClient = new UnifiedHttpClient(
  API_CONFIG.BASE_URL,
  API_CONFIG.TIMEOUT,
  API_CONFIG.DEFAULT_HEADERS
);

// Collector 전용 클라이언트 생성 (향후 사용)
export const collectorClient = new UnifiedHttpClient(
  API_CONFIG.COLLECTOR_URL,
  API_CONFIG.TIMEOUT,
  API_CONFIG.DEFAULT_HEADERS
);

// ============================================================================
// 🔧 기존 axios 유틸리티 함수들 (완전 호환)
// ============================================================================

/**
 * 쿼리 파라미터 빌더 (기존과 동일)
 */
export const buildQueryParams = (params: Record<string, any>): string => {
  const searchParams = new URLSearchParams();

  Object.entries(params).forEach(([key, value]) => {
    if (value !== undefined && value !== null && value !== '') {
      searchParams.append(key, String(value));
    }
  });

  return searchParams.toString();
};

/**
 * 페이지네이션 파라미터 빌더 (기존과 동일)
 */
export const buildPaginationParams = (
  page: number = 1,
  limit: number = API_CONFIG.DEFAULT_PAGE_SIZE,
  additionalParams: Record<string, any> = {}
) => {
  return {
    page,
    limit: Math.min(limit, API_CONFIG.MAX_PAGE_SIZE),
    ...additionalParams
  };
};

/**
 * 에러 메시지 추출 (기존 axios 버전과 호환)
 */
export const extractErrorMessage = (error: any): string => {
  // ApiResponse 에러 형식
  if (error.error) {
    return error.error;
  }
  if (error.message) {
    return error.message;
  }

  // 기존 axios 에러 형식 호환
  if (error.response?.data?.error) {
    return error.response.data.error;
  }
  if (error.response?.data?.message) {
    return error.response.data.message;
  }

  return '알 수 없는 오류가 발생했습니다.';
};

/**
 * 재시도 함수 (기존과 동일)
 */
export const retryRequest = async <T>(
  requestFn: () => Promise<T>,
  maxRetries: number = 3,
  delay: number = 1000
): Promise<T> => {
  let lastError: any;

  for (let i = 0; i < maxRetries; i++) {
    try {
      return await requestFn();
    } catch (error) {
      lastError = error;

      // 마지막 시도가 아니라면 지연 후 재시도
      if (i < maxRetries - 1) {
        console.warn(`🔄 재시도 ${i + 1}/${maxRetries} (${delay}ms 후)`);
        await new Promise(resolve => setTimeout(resolve, delay));
      }
    }
  }

  throw lastError;
};

// ============================================================================
// 기본 내보내기 (기존 axios 호환)
// ============================================================================

export default apiClient;