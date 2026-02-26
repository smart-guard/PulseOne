// =============================================================================
// frontend/src/api/services/virtualPointsApi.ts
// 가상포인트 API 서비스 완성본 - 모든 기능 통합
// =============================================================================

interface VirtualPointFormData {
  name: string;
  description?: string;
  formula?: string;
  expression?: string;
  data_type?: string;
  unit?: string;
  calculation_trigger?: string;
  calculation_interval?: number;
  is_enabled?: boolean;
  category?: string;
  scope_type?: string;
  site_id?: number;
  device_id?: number;
  tenant_id?: number;
  priority?: number;
  cache_duration_ms?: number;
  error_handling?: string;
}

interface VirtualPointInputFormData {
  variable_name: string;
  source_type: 'data_point' | 'virtual_point' | 'constant' | 'formula';
  source_id?: number;
  constant_value?: number;
  source_formula?: string;
  data_processing?: 'current' | 'average' | 'min' | 'max' | 'sum';
  time_window_seconds?: number;
}

interface VirtualPointFilters {
  tenant_id?: number;
  site_id?: number;
  device_id?: number;
  scope_type?: string;
  is_enabled?: boolean;
  include_deleted?: boolean;
  category?: string;
  calculation_trigger?: string;
  search?: string;
  limit?: number;
  page?: number;
}

interface ApiResponse<T = any> {
  success: boolean;
  data: T;
  message: string;
  error_code?: string;
  timestamp: string;
}

class VirtualPointsApiService {
  private baseUrl: string;

  constructor() {
    this.baseUrl = '/api/virtual-points';
  }

  // ==========================================================================
  // CRUD 기본 기능들
  // ==========================================================================

  /**
   * 가상포인트 목록 조회 (필터링 및 페이징 지원)
   */
  async getVirtualPoints(filters: VirtualPointFilters = {}): Promise<any[]> {
    console.log('가상포인트 목록 조회:', { filters });

    const queryParams = new URLSearchParams();

    // 필터 추가
    Object.entries(filters).forEach(([key, value]) => {
      if (value !== undefined && value !== null && value !== '') {
        queryParams.append(key, String(value));
      }
    });

    const url = queryParams.toString() ? `${this.baseUrl}?${queryParams}` : this.baseUrl;

    try {
      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 목록 조회 실패`);
      }

      const result: ApiResponse<any[]> = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 목록 조회 실패');
      }

      // 배열 형태로 반환 (백엔드에서 result.data가 이미 배열)
      return Array.isArray(result.data) ? result.data : [];

    } catch (error) {
      console.error('가상포인트 목록 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 상세 조회
   */
  async getVirtualPoint(id: number): Promise<any> {
    console.log(`가상포인트 상세 조회: ID ${id}`);

    try {
      const response = await fetch(`${this.baseUrl}/${id}`, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 조회 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 조회 실패');
      }

      return result.data;

    } catch (error) {
      console.error(`가상포인트 ${id} 조회 실패:`, error);
      throw error;
    }
  }

  /**
   * 가상포인트 생성
   */
  async createVirtualPoint(
    formData: VirtualPointFormData,
    inputs: VirtualPointInputFormData[] = []
  ): Promise<any> {
    console.log('가상포인트 생성:', { formData, inputsCount: inputs.length });

    const requestData = {
      virtualPoint: {
        ...formData,
        formula: formData.expression || formData.formula || 'return 0;'
      },
      inputs: inputs || []
    };

    try {
      const response = await fetch(this.baseUrl, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(requestData)
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 생성 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 생성 실패');
      }

      return result.data;

    } catch (error) {
      console.error('가상포인트 생성 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 전체 업데이트 (기존 유지 - 전체 업데이트용)
   */
  async updateVirtualPoint(
    id: number,
    data: Partial<VirtualPointFormData>,
    inputs?: VirtualPointInputFormData[]
  ): Promise<any> {
    console.log('가상포인트 전체 수정:', { id, data });

    // 백엔드 형식에 맞게 변환
    const requestData = {
      virtualPoint: {
        ...data,
        formula: data.expression || data.formula
      },
      inputs: inputs || null
    };

    try {
      const response = await fetch(`${this.baseUrl}/${id}`, {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(requestData)
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 수정 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 수정 실패');
      }

      return result.data;

    } catch (error) {
      console.error(`가상포인트 ${id} 수정 실패:`, error);
      throw error;
    }
  }

  /**
   * 가상포인트 삭제
   */
  async deleteVirtualPoint(id: number): Promise<{ deleted: boolean }> {
    console.log(`가상포인트 삭제 (Soft-delete): ID ${id}`);

    try {
      const response = await fetch(`${this.baseUrl}/${id}`, {
        method: 'DELETE',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        console.error('삭제 에러 응답:', errorResult);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 삭제 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 삭제 실패');
      }

      return { deleted: true };

    } catch (error) {
      console.error(`가상포인트 ${id} 삭제 실패:`, error);
      throw error;
    }
  }

  /**
   * 가상포인트 복원
   */
  async restoreVirtualPoint(id: number): Promise<{ restored: boolean }> {
    console.log(`가상포인트 복원: ID ${id}`);

    try {
      const response = await fetch(`${this.baseUrl}/${id}/restore`, {
        method: 'PATCH',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 복원 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 복원 실패');
      }

      return { restored: true };

    } catch (error) {
      console.error(`가상포인트 ${id} 복원 실패:`, error);
      throw error;
    }
  }

  // ==========================================================================
  // PATCH API들 (부분 업데이트)
  // ==========================================================================

  /**
   * 가상포인트 활성화/비활성화 토글 (PATCH 사용!)
   */
  async toggleVirtualPoint(id: number, isEnabled: boolean): Promise<any> {
    console.log(`가상포인트 ${id} 활성화 토글: ${isEnabled}`);

    try {
      const response = await fetch(`${this.baseUrl}/${id}/toggle`, {
        method: 'PATCH',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          is_enabled: isEnabled
        })
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 토글 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 토글 실패');
      }

      return result.data;

    } catch (error) {
      console.error(`가상포인트 ${id} 토글 실패:`, error);
      throw error;
    }
  }

  /**
   * 가상포인트 설정만 업데이트 (이름 등 필수 필드 건드리지 않음)
   */
  async updateVirtualPointSettings(
    id: number,
    settings: Partial<Pick<VirtualPointFormData,
      'is_enabled' | 'calculation_interval' | 'calculation_trigger' |
      'priority' | 'description' | 'unit' | 'data_type' | 'category'>>
  ): Promise<any> {
    console.log(`가상포인트 ${id} 설정 업데이트:`, settings);

    try {
      const response = await fetch(`${this.baseUrl}/${id}/settings`, {
        method: 'PATCH',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 설정 업데이트 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 설정 업데이트 실패');
      }

      return result.data;

    } catch (error) {
      console.error(`가상포인트 ${id} 설정 업데이트 실패:`, error);
      throw error;
    }
  }

  // ==========================================================================
  // 실행 및 테스트 관련
  // ==========================================================================

  /**
   * 가상포인트 실행
   */
  async executeVirtualPoint(id: number, options: { timeout?: number, context?: any } = {}): Promise<any> {
    console.log(`가상포인트 실행: ID ${id}`, options);

    try {
      const response = await fetch(`${this.baseUrl}/${id}/execute`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          timeout: options.timeout || 10000,
          context: options.context || {}
        })
      });

      if (response.status === 404) {
        throw new Error('실행 기능이 아직 구현되지 않았습니다');
      }

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 가상포인트 실행 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '가상포인트 실행 실패');
      }

      return result.data;

    } catch (error) {
      console.error(`가상포인트 ${id} 실행 실패:`, error);
      throw error;
    }
  }

  /**
   * 스크립트 테스트 (실제 백엔드: POST /api/script-engine/test)
   */
  async testScript(testData: {
    expression: string;
    variables?: Record<string, any>;
    data_type?: string;
  }): Promise<any> {
    console.log('스크립트 테스트:', testData);

    try {
      // 백엔드 script-engine 라우트는 /api/script-engine/test 사용
      // 파라미터: { script: string, context: object }
      const response = await fetch('/api/script-engine/test', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          script: testData.expression,
          context: testData.variables || {}
        })
      });

      if (response.status === 404) {
        throw new Error('테스트 기능이 아직 구현되지 않았습니다');
      }

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 스크립트 테스트 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '스크립트 테스트 실패');
      }

      return result.data;

    } catch (error) {
      console.error('스크립트 테스트 실패:', error);
      throw error;
    }
  }

  // ==========================================================================
  // 통계 및 분석
  // ==========================================================================

  /**
   * 카테고리별 통계
   */
  async getCategoryStats(tenantId?: number): Promise<any[]> {
    console.log('카테고리별 통계 조회', tenantId ? `(테넌트: ${tenantId})` : '');

    try {
      const queryParams = new URLSearchParams();
      if (tenantId) {
        queryParams.append('tenant_id', String(tenantId));
      }

      const url = queryParams.toString() ?
        `${this.baseUrl}/stats/category?${queryParams}` :
        `${this.baseUrl}/stats/category`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 카테고리 통계 조회 실패`);
      }

      const result: ApiResponse<any[]> = await response.json();

      if (!result.success) {
        throw new Error(result.message || '카테고리 통계 조회 실패');
      }

      return result.data || [];

    } catch (error) {
      console.error('카테고리별 통계 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 성능 통계
   */
  async getPerformanceStats(tenantId?: number): Promise<any> {
    console.log('성능 통계 조회', tenantId ? `(테넌트: ${tenantId})` : '');

    try {
      const queryParams = new URLSearchParams();
      if (tenantId) {
        queryParams.append('tenant_id', String(tenantId));
      }

      const url = queryParams.toString() ?
        `${this.baseUrl}/stats/performance?${queryParams}` :
        `${this.baseUrl}/stats/performance`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 성능 통계 조회 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '성능 통계 조회 실패');
      }

      return result.data || { total_points: 0, enabled_points: 0 };

    } catch (error) {
      console.error('성능 통계 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 요약 통계
   */
  async getSummaryStats(tenantId?: number): Promise<any> {
    console.log('요약 통계 조회', tenantId ? `(테넌트: ${tenantId})` : '');

    try {
      const queryParams = new URLSearchParams();
      if (tenantId) {
        queryParams.append('tenant_id', String(tenantId));
      }

      const url = queryParams.toString() ?
        `${this.baseUrl}/stats/summary?${queryParams}` :
        `${this.baseUrl}/stats/summary`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 요약 통계 조회 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '요약 통계 조회 실패');
      }

      return result.data || {
        total_virtual_points: 0,
        enabled_virtual_points: 0,
        disabled_virtual_points: 0,
        category_distribution: [],
        last_updated: new Date().toISOString()
      };

    } catch (error) {
      console.error('요약 통계 조회 실패:', error);
      throw error;
    }
  }

  // ==========================================================================
  // 유틸리티 및 관리 기능
  // ==========================================================================

  /**
   * 고아 레코드 정리 (관리자 기능)
   */
  async cleanupOrphanedRecords(): Promise<any> {
    console.log('고아 레코드 정리 요청');

    try {
      const response = await fetch(`${this.baseUrl}/admin/cleanup-orphaned`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        }
      });

      if (!response.ok) {
        const errorResult = await response.json().catch(() => null);
        throw new Error(errorResult?.message || `HTTP ${response.status}: 고아 레코드 정리 실패`);
      }

      const result: ApiResponse = await response.json();

      if (!result.success) {
        throw new Error(result.message || '고아 레코드 정리 실패');
      }

      return result.data;

    } catch (error) {
      console.error('고아 레코드 정리 실패:', error);
      throw error;
    }
  }

  // ==========================================================================
  // 헬퍼 메소드들
  // ==========================================================================

  /**
   * API 응답 형식 검증
   */
  private validateApiResponse(response: any): boolean {
    return response && typeof response.success === 'boolean';
  }

  /**
   * 에러 메시지 추출
   */
  private extractErrorMessage(error: any, defaultMessage: string): string {
    if (error instanceof Error) {
      return error.message;
    }
    if (error && typeof error.message === 'string') {
      return error.message;
    }
    return defaultMessage;
  }

  /**
   * 안전한 JSON 파싱
   */
  private async safeJsonParse(response: Response): Promise<any | null> {
    try {
      return await response.json();
    } catch {
      return null;
    }
  }

  /**
   * 감사 로그 조회
   */
  async getLogs(pointId: number): Promise<any[]> {
    try {
      const response = await fetch(`${this.baseUrl}/${pointId}/logs`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
      });
      if (!response.ok) throw new Error('로그 조회 실패');
      const result = await response.json();
      return result.success ? result.data : [];
    } catch (error) {
      console.error('로그 조회 오류:', error);
      return [];
    }
  }
}

// 싱글톤 인스턴스 생성 및 export
export const virtualPointsApi = new VirtualPointsApiService();

// 타입들도 export
export type {
  VirtualPointFormData,
  VirtualPointInputFormData,
  VirtualPointFilters,
  ApiResponse
};