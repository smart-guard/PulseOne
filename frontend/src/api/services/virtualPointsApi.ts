// ============================================================================
// frontend/src/api/services/virtualPointsApi.ts
// 가상포인트 API 서비스 (수정됨 - 통합 클라이언트 사용)
// ============================================================================

import { apiClient } from '../client';
import {
  VirtualPoint,
  VirtualPointFormData,
  VirtualPointInputFormData,
  VirtualPointFilters,
  VirtualPointsApiResponse,
  VirtualPointApiResponse,
  VirtualPointCategoryStatsApiResponse,
  VirtualPointPerformanceStatsApiResponse,
  ScriptTestRequest,
  ScriptTestApiResponse,
  VirtualPointExecuteRequest,
  VirtualPointExecuteApiResponse
} from '../../types/virtualPoints';

class VirtualPointsApiService {
  private readonly baseUrl = '/api/virtual-points';

  // ========================================================================
  // CRUD 작업
  // ========================================================================

  /**
   * 가상포인트 목록 조회
   */
  async getVirtualPoints(filters?: VirtualPointFilters, pagination?: {
    page: number;
    pageSize: number;
    sortBy?: string;
    sortOrder?: 'asc' | 'desc';
  }): Promise<{
    items: VirtualPoint[];
    totalCount: number;
    pageInfo: {
      currentPage: number;
      pageSize: number;
      totalPages: number;
      hasNext: boolean;
      hasPrev: boolean;
    };
  }> {
    try {
      console.log('가상포인트 목록 조회:', { filters, pagination });

      const params = {
        ...filters,
        ...pagination
      };

      const response = await apiClient.get<VirtualPointsApiResponse>(this.baseUrl, params);

      if (response.success && response.data) {
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 조회 실패');
      }
    } catch (error) {
      console.error('가상포인트 조회 실패:', error);
      
      // 백엔드 API가 없는 경우 더미 데이터 반환
      return this.getMockVirtualPoints(filters, pagination);
    }
  }

  /**
   * 특정 가상포인트 조회
   */
  async getVirtualPoint(id: number): Promise<VirtualPoint> {
    try {
      console.log('가상포인트 상세 조회:', id);

      const response = await apiClient.get<VirtualPointApiResponse>(`${this.baseUrl}/${id}`);

      if (response.success && response.data) {
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트를 찾을 수 없습니다');
      }
    } catch (error) {
      console.error('가상포인트 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 새 가상포인트 생성
   */
  async createVirtualPoint(data: VirtualPointFormData): Promise<VirtualPoint> {
    try {
      console.log('가상포인트 생성:', data);

      const response = await apiClient.post<VirtualPointApiResponse>(this.baseUrl, data);

      if (response.success && response.data) {
        console.log('가상포인트 생성 완료:', response.data.id);
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 생성 실패');
      }
    } catch (error) {
      console.error('가상포인트 생성 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 수정
   */
  async updateVirtualPoint(id: number, data: VirtualPointFormData): Promise<VirtualPoint> {
    try {
      console.log('가상포인트 수정:', { id, data });

      const response = await apiClient.put<VirtualPointApiResponse>(`${this.baseUrl}/${id}`, data);

      if (response.success && response.data) {
        console.log('가상포인트 수정 완료:', id);
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 수정 실패');
      }
    } catch (error) {
      console.error('가상포인트 수정 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 삭제
   */
  async deleteVirtualPoint(id: number): Promise<{ deleted: boolean }> {
    try {
      console.log('가상포인트 삭제:', id);

      const response = await apiClient.delete<{ deleted: boolean }>(`${this.baseUrl}/${id}`);

      if (response.success && response.data) {
        console.log('가상포인트 삭제 완료:', id);
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 삭제 실패');
      }
    } catch (error) {
      console.error('가상포인트 삭제 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 가상포인트 제어 및 실행
  // ========================================================================

  /**
   * 가상포인트 활성화/비활성화 토글
   */
  async toggleVirtualPointEnabled(id: number): Promise<VirtualPoint> {
    try {
      console.log('가상포인트 상태 토글:', id);

      const response = await apiClient.post<VirtualPointApiResponse>(`${this.baseUrl}/${id}/toggle`);

      if (response.success && response.data) {
        console.log('가상포인트 상태 변경 완료:', response.data.is_enabled ? '활성화' : '비활성화');
        return response.data;
      } else {
        throw new Error(response.message || '상태 변경 실패');
      }
    } catch (error) {
      console.error('가상포인트 상태 토글 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 수동 실행
   */
  async executeVirtualPoint(id: number, context?: Record<string, any>): Promise<{
    success: boolean;
    value: any;
    executionTime: number;
    timestamp: string;
  }> {
    try {
      console.log('가상포인트 실행:', { id, context });

      const request: VirtualPointExecuteRequest = {
        id,
        context: context || {},
        timeout: 10000
      };

      const response = await apiClient.post<VirtualPointExecuteApiResponse>(
        `${this.baseUrl}/${id}/execute`, 
        request
      );

      if (response.success && response.data) {
        console.log('가상포인트 실행 완료:', response.data.success ? '성공' : '실패');
        return response.data;
      } else {
        throw new Error(response.message || '실행 실패');
      }
    } catch (error) {
      console.error('가상포인트 실행 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 스크립트 테스트
   */
  async testVirtualPointScript(id: number, testData: Record<string, any>): Promise<{
    success: boolean;
    result: any;
    executionTime: number;
    error?: string;
    logs: string[];
  }> {
    try {
      console.log('가상포인트 스크립트 테스트:', { id, testData });

      const request: ScriptTestRequest = {
        script: '', // 실제로는 백엔드에서 ID로 스크립트를 찾아서 테스트
        context: testData,
        timeout: 5000,
        includeDebugInfo: true
      };

      const response = await apiClient.post<ScriptTestApiResponse>(
        `${this.baseUrl}/${id}/test`, 
        request
      );

      if (response.success && response.data) {
        console.log('스크립트 테스트 완료:', response.data.success ? '성공' : '실패');
        return response.data;
      } else {
        throw new Error(response.message || '테스트 실패');
      }
    } catch (error) {
      console.error('스크립트 테스트 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 통계 및 분석
  // ========================================================================

  /**
   * 카테고리별 통계 조회
   */
  async getCategoryStats(): Promise<{
    total: number;
    active: number;
    disabled: number;
    error: number;
    byCategory: Record<string, number>;
    byExecutionType: Record<string, number>;
  }> {
    try {
      console.log('가상포인트 카테고리 통계 조회');

      const response = await apiClient.get<VirtualPointCategoryStatsApiResponse>(
        `${this.baseUrl}/stats/category`
      );

      if (response.success && response.data) {
        return response.data;
      } else {
        throw new Error(response.message || '통계 조회 실패');
      }
    } catch (error) {
      console.error('카테고리 통계 조회 실패:', error);
      
      // 기본값 반환
      return {
        total: 0,
        active: 0,
        disabled: 0,
        error: 0,
        byCategory: {},
        byExecutionType: {}
      };
    }
  }

  /**
   * 성능 통계 조회
   */
  async getPerformanceStats(): Promise<{
    averageExecutionTime: number;
    successRate: number;
    totalExecutions: number;
    errorCount: number;
    topPerformers: Array<{
      id: number;
      name: string;
      avgTime: number;
      successRate: number;
    }>;
  }> {
    try {
      console.log('가상포인트 성능 통계 조회');

      const response = await apiClient.get<VirtualPointPerformanceStatsApiResponse>(
        `${this.baseUrl}/stats/performance`
      );

      if (response.success && response.data) {
        return response.data;
      } else {
        throw new Error(response.message || '성능 통계 조회 실패');
      }
    } catch (error) {
      console.error('성능 통계 조회 실패:', error);
      
      // 기본값 반환
      return {
        averageExecutionTime: 0,
        successRate: 0,
        totalExecutions: 0,
        errorCount: 0,
        topPerformers: []
      };
    }
  }

  // ========================================================================
  // 유틸리티 메서드들
  // ========================================================================

  /**
   * 상태별 색상 반환
   */
  getStatusColor(status: string): string {
    switch (status) {
      case 'active': return '#10b981'; // green-500
      case 'disabled': return '#6b7280'; // gray-500
      case 'error': return '#ef4444'; // red-500
      default: return '#6b7280';
    }
  }

  /**
   * 상태별 아이콘 반환
   */
  getStatusIcon(status: string): string {
    switch (status) {
      case 'active': return 'fas fa-check-circle';
      case 'disabled': return 'fas fa-pause-circle';
      case 'error': return 'fas fa-exclamation-triangle';
      default: return 'fas fa-question-circle';
    }
  }

  /**
   * 카테고리별 색상 반환
   */
  getCategoryColor(category: string): string {
    const colors: Record<string, string> = {
      '온도': '#f59e0b', // amber-500
      '압력': '#3b82f6', // blue-500
      '유량': '#10b981', // green-500
      '전력': '#ef4444', // red-500
      '계산': '#8b5cf6', // violet-500
      '기타': '#6b7280'  // gray-500
    };
    return colors[category] || colors['기타'];
  }

  /**
   * 데이터 타입별 아이콘 반환
   */
  getDataTypeIcon(dataType: string): string {
    switch (dataType) {
      case 'number': return 'fas fa-calculator';
      case 'boolean': return 'fas fa-toggle-on';
      case 'string': return 'fas fa-font';
      default: return 'fas fa-question';
    }
  }

  // ========================================================================
  // 목업 데이터 (백엔드 API가 없을 때 사용)
  // ========================================================================

  private getMockVirtualPoints(
    filters?: VirtualPointFilters,
    pagination?: {
      page: number;
      pageSize: number;
      sortBy?: string;
      sortOrder?: 'asc' | 'desc';
    }
  ): {
    items: VirtualPoint[];
    totalCount: number;
    pageInfo: {
      currentPage: number;
      pageSize: number;
      totalPages: number;
      hasNext: boolean;
      hasPrev: boolean;
    };
  } {
    console.log('목업 가상포인트 데이터 사용');

    const mockData: VirtualPoint[] = [
      {
        id: 1,
        name: '평균 온도',
        description: '3개 센서의 평균 온도를 계산합니다',
        expression: '(temp1 + temp2 + temp3) / 3',
        category: '온도',
        tags: ['온도', '평균', '센서'],
        data_type: 'number',
        unit: '°C',
        decimal_places: 1,
        execution_type: 'periodic',
        execution_interval: 5000,
        priority: 0,
        timeout_ms: 10000,
        error_handling: 'propagate',
        default_value: 0,
        is_enabled: true,
        scope_type: 'device',
        scope_id: 1,
        calculation_status: 'success',
        current_value: 24.5,
        last_calculated: '2025-08-23T10:30:00Z',
        created_at: '2025-08-20T09:00:00Z',
        updated_at: '2025-08-23T10:30:00Z',
        input_variables: [
          {
            id: 1,
            variable_name: 'temp1',
            source_type: 'data_point',
            source_id: 101,
            data_type: 'number',
            description: '센서 1 온도',
            is_required: true
          },
          {
            id: 2,
            variable_name: 'temp2',
            source_type: 'data_point',
            source_id: 102,
            data_type: 'number',
            description: '센서 2 온도',
            is_required: true
          },
          {
            id: 3,
            variable_name: 'temp3',
            source_type: 'data_point',
            source_id: 103,
            data_type: 'number',
            description: '센서 3 온도',
            is_required: true
          }
        ]
      },
      {
        id: 2,
        name: '압력 상태',
        description: '압력이 정상 범위인지 확인합니다',
        expression: 'pressure > 1.0 && pressure < 5.0 ? "정상" : "이상"',
        category: '압력',
        tags: ['압력', '상태', '모니터링'],
        data_type: 'string',
        unit: '',
        decimal_places: 0,
        execution_type: 'on_change',
        execution_interval: 0,
        priority: 1,
        timeout_ms: 5000,
        error_handling: 'default_value',
        default_value: '알 수 없음',
        is_enabled: true,
        scope_type: 'site',
        scope_id: 1,
        min_value: 0,
        max_value: 10,
        calculation_status: 'success',
        current_value: '정상',
        last_calculated: '2025-08-23T10:29:30Z',
        created_at: '2025-08-21T14:30:00Z',
        updated_at: '2025-08-23T10:29:30Z',
        input_variables: [
          {
            id: 4,
            variable_name: 'pressure',
            source_type: 'data_point',
            source_id: 201,
            data_type: 'number',
            description: '압력 센서 값',
            is_required: true
          }
        ]
      }
    ];

    // 필터 적용 (간단한 구현)
    let filteredData = mockData;
    if (filters?.search) {
      const searchLower = filters.search.toLowerCase();
      filteredData = filteredData.filter(item =>
        item.name.toLowerCase().includes(searchLower) ||
        item.description?.toLowerCase().includes(searchLower)
      );
    }

    // 페이징 적용
    const page = pagination?.page || 1;
    const pageSize = pagination?.pageSize || 10;
    const startIndex = (page - 1) * pageSize;
    const endIndex = startIndex + pageSize;
    const items = filteredData.slice(startIndex, endIndex);
    const totalCount = filteredData.length;
    const totalPages = Math.ceil(totalCount / pageSize);

    return {
      items,
      totalCount,
      pageInfo: {
        currentPage: page,
        pageSize,
        totalPages,
        hasNext: page < totalPages,
        hasPrev: page > 1
      }
    };
  }
}

// 싱글톤 인스턴스 생성 및 export
export const virtualPointsApi = new VirtualPointsApiService();