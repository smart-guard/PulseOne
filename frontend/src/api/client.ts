// ============================================================================
// frontend/src/api/client.ts
// Axios 클라이언트 설정
// ============================================================================

import axios, { AxiosInstance, AxiosRequestConfig, AxiosResponse } from 'axios';
import { API_CONFIG } from './config';

// 기본 Axios 인스턴스 생성
export const apiClient: AxiosInstance = axios.create({
  baseURL: API_CONFIG.BASE_URL,
  timeout: API_CONFIG.TIMEOUT,
  headers: API_CONFIG.DEFAULT_HEADERS
});

// Collector 전용 Axios 인스턴스 (향후 사용)
export const collectorClient: AxiosInstance = axios.create({
  baseURL: API_CONFIG.COLLECTOR_URL,
  timeout: API_CONFIG.TIMEOUT,
  headers: API_CONFIG.DEFAULT_HEADERS
});

// ==========================================================================
// 요청 인터셉터 (공통 처리)
// ==========================================================================

apiClient.interceptors.request.use(
  (config: AxiosRequestConfig) => {
    // 요청 시작 시 로딩 상태 설정 가능
    console.log(`🔄 API 요청: ${config.method?.toUpperCase()} ${config.url}`);
    
    // 인증 토큰이 있다면 헤더에 추가
    const token = localStorage.getItem('auth_token');
    if (token && config.headers) {
      config.headers.Authorization = `Bearer ${token}`;
    }
    
    return config;
  },
  (error) => {
    console.error('❌ 요청 에러:', error);
    return Promise.reject(error);
  }
);

// ==========================================================================
// 응답 인터셉터 (에러 처리 및 공통 처리)
// ==========================================================================

apiClient.interceptors.response.use(
  (response: AxiosResponse) => {
    console.log(`✅ API 응답: ${response.status} ${response.config.url}`);
    return response;
  },
  (error) => {
    const { response, config } = error;
    
    console.error(`❌ API 에러: ${response?.status} ${config?.url}`, error);
    
    // 에러 타입별 처리
    switch (response?.status) {
      case 401:
        // 인증 만료 - 로그인 페이지로 리다이렉트
        console.warn('🔐 인증이 만료되었습니다. 다시 로그인해주세요.');
        localStorage.removeItem('auth_token');
        window.location.href = '/login';
        break;
        
      case 403:
        // 권한 없음
        console.warn('🚫 접근 권한이 없습니다.');
        break;
        
      case 404:
        // 리소스 없음
        console.warn('🔍 요청한 리소스를 찾을 수 없습니다.');
        break;
        
      case 500:
        // 서버 에러
        console.error('🔥 서버에서 오류가 발생했습니다.');
        break;
        
      default:
        // 기타 에러
        console.error('⚠️ 알 수 없는 오류가 발생했습니다.');
    }
    
    return Promise.reject(error);
  }
);

// ==========================================================================
// 유틸리티 함수들
// ==========================================================================

// 쿼리 파라미터 빌더
export const buildQueryParams = (params: Record<string, any>): string => {
  const searchParams = new URLSearchParams();
  
  Object.entries(params).forEach(([key, value]) => {
    if (value !== undefined && value !== null && value !== '') {
      searchParams.append(key, String(value));
    }
  });
  
  return searchParams.toString();
};

// 페이지네이션 파라미터 빌더
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

// 에러 메시지 추출
export const extractErrorMessage = (error: any): string => {
  if (error.response?.data?.error) {
    return error.response.data.error;
  }
  if (error.response?.data?.message) {
    return error.response.data.message;
  }
  if (error.message) {
    return error.message;
  }
  return '알 수 없는 오류가 발생했습니다.';
};

// 재시도 함수
export const retryRequest = async <T>(
  requestFn: () => Promise<T>,
  maxRetries: number = API_CONFIG.RETRY_COUNT,
  delay: number = API_CONFIG.RETRY_DELAY
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

export default apiClient;