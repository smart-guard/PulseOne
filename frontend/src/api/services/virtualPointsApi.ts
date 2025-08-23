// ============================================================================
// frontend/src/api/services/virtualPointsApi.ts
// 가상포인트 API 서비스 - 실제 백엔드 연동
// ============================================================================

import { httpClient } from './httpClient';
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
  async getVirtualPoints(filters?: VirtualPointFilters): Promise<VirtualPoint[]> {
    try {
      console.log('🔍 가상포인트 목록 조회:', filters);
      
      const response = await httpClient.get<VirtualPointsApiResponse>(this.baseUrl, {
        params: this.buildQueryParams(filters)
      });

      if (response.success && Array.isArray(response.data)) {
        console.log(`✅ ${response.data.length}개 가상포인트 로드 완료`);
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 목록 조회 실패');
      }
    } catch (error) {
      console.error('❌ 가상포인트 목록 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 상세 조회
   */
  async getVirtualPoint(id: number): Promise<VirtualPoint> {
    try {
      console.log('🔍 가상포인트 상세 조회:', id);
      
      const response = await httpClient.get<VirtualPointApiResponse>(`${this.baseUrl}/${id}`);

      if (response.success && response.data) {
        console.log('✅ 가상포인트 상세 정보 로드 완료');
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 조회 실패');
      }
    } catch (error) {
      console.error('❌ 가상포인트 상세 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 생성
   */
  async createVirtualPoint(
    formData: VirtualPointFormData, 
    inputs: VirtualPointInputFormData[] = []
  ): Promise<VirtualPoint> {
    try {
      console.log('🆕 가상포인트 생성 시작:', formData);
      
      const requestData = {
        virtualPoint: {
          tenant_id: 1, // 기본값
          ...formData,
          created_at: new Date().toISOString(),
          updated_at: new Date().toISOString()
        },
        inputs: inputs.map((input, index) => ({
          ...input,
          id: Date.now() + index, // 임시 ID
          created_at: new Date().toISOString()
        }))
      };

      const response = await httpClient.post<VirtualPointApiResponse>(
        this.baseUrl, 
        requestData
      );

      if (response.success && response.data) {
        console.log('✅ 가상포인트 생성 완료:', response.data.id);
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 생성 실패');
      }
    } catch (error) {
      console.error('❌ 가상포인트 생성 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 수정
   */
  async updateVirtualPoint(
    id: number, 
    formData: Partial<VirtualPointFormData>, 
    inputs?: VirtualPointInputFormData[]
  ): Promise<VirtualPoint> {
    try {
      console.log('📝 가상포인트 수정 시작:', id, formData);
      
      const requestData = {
        virtualPoint: {
          ...formData,
          updated_at: new Date().toISOString()
        },
        ...(inputs && { 
          inputs: inputs.map((input, index) => ({
            ...input,
            virtual_point_id: id,
            id: input.id || Date.now() + index
          }))
        })
      };

      const response = await httpClient.put<VirtualPointApiResponse>(
        `${this.baseUrl}/${id}`, 
        requestData
      );

      if (response.success && response.data) {
        console.log('✅ 가상포인트 수정 완료');
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 수정 실패');
      }
    } catch (error) {
      console.error('❌ 가상포인트 수정 실패:', error);
      throw error;
    }
  }

  /**
   * 가상포인트 삭제
   */
  async deleteVirtualPoint(id: number): Promise<boolean> {
    try {
      console.log('🗑️ 가상포인트 삭제 시작:', id);
      
      const response = await httpClient.delete(`${this.baseUrl}/${id}`);

      if (response.success) {
        console.log('✅ 가상포인트 삭제 완료');
        return true;
      } else {
        throw new Error(response.message || '가상포인트 삭제 실패');
      }
    } catch (error) {
      console.error('❌ 가상포인트 삭제 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 실행 및 테스트
  // ========================================================================

  /**
   * 스크립트 테스트 실행
   */
  async testScript(request: ScriptTestRequest): Promise<any> {
    try {
      console.log('🧪 스크립트 테스트 시작:', request);
      
      // 임시 스크립트 테스트 엔드포인트 (실제 구현 필요)
      const response = await httpClient.post<ScriptTestApiResponse>(
        `${this.baseUrl}/test-script`, 
        request
      );

      if (response.success) {
        console.log('✅ 스크립트 테스트 완료:', response.data);
        return response.data;
      } else {
        throw new Error(response.message || '스크립트 테스트 실패');
      }
    } catch (error) {
      console.error('❌ 스크립트 테스트 실패:', error);
      
      // 백엔드 API가 없는 경우 목업 응답
      return this.mockScriptTest(request);
    }
  }

  /**
   * 가상포인트 수동 실행
   */
  async executeVirtualPoint(id: number, request?: VirtualPointExecuteRequest): Promise<any> {
    try {
      console.log('▶️ 가상포인트 실행 시작:', id);
      
      const response = await httpClient.post<VirtualPointExecuteApiResponse>(
        `${this.baseUrl}/${id}/execute`,
        request || {}
      );

      if (response.success) {
        console.log('✅ 가상포인트 실행 완료:', response.data);
        return response.data;
      } else {
        throw new Error(response.message || '가상포인트 실행 실패');
      }
    } catch (error) {
      console.error('❌ 가상포인트 실행 실패:', error);
      throw error;
    }
  }

  // ========================================================================
  // 통계 및 메타데이터
  // ========================================================================

  /**
   * 카테고리별 통계 조회
   */
  async getCategoryStats(tenantId: number = 1): Promise<any[]> {
    try {
      console.log('📊 카테고리 통계 조회');
      
      const response = await httpClient.get<VirtualPointCategoryStatsApiResponse>(
        `${this.baseUrl}/stats/category`,
        { params: { tenant_id: tenantId } }
      );

      if (response.success && Array.isArray(response.data)) {
        console.log('✅ 카테고리 통계 로드 완료');
        return response.data;
      } else {
        throw new Error(response.message || '카테고리 통계 조회 실패');
      }
    } catch (error) {
      console.error('❌ 카테고리 통계 조회 실패:', error);
      return []; // 빈 배열 반환
    }
  }

  /**
   * 성능 통계 조회
   */
  async getPerformanceStats(tenantId: number = 1): Promise<any> {
    try {
      console.log('⚡ 성능 통계 조회');
      
      const response = await httpClient.get<VirtualPointPerformanceStatsApiResponse>(
        `${this.baseUrl}/stats/performance`,
        { params: { tenant_id: tenantId } }
      );

      if (response.success && response.data) {
        console.log('✅ 성능 통계 로드 완료');
        return response.data;
      } else {
        throw new Error(response.message || '성능 통계 조회 실패');
      }
    } catch (error) {
      console.error('❌ 성능 통계 조회 실패:', error);
      return null;
    }
  }

  /**
   * 사용 가능한 카테고리 목록 조회
   */
  async getCategories(): Promise<string[]> {
    try {
      const stats = await this.getCategoryStats();
      return stats.map(stat => stat.category).filter(Boolean);
    } catch (error) {
      console.error('❌ 카테고리 목록 조회 실패:', error);
      return [
        'Temperature', 'Pressure', 'Flow', 'Power', 
        'Production', 'Quality', 'Safety', 'Maintenance', 
        'Energy', 'Custom'
      ];
    }
  }

  /**
   * 의존성 조회
   */
  async getDependencies(id: number): Promise<any> {
    try {
      console.log('🔗 가상포인트 의존성 조회:', id);
      
      const response = await httpClient.get(`${this.baseUrl}/${id}/dependencies`);
      
      if (response.success) {
        return response.data || [];
      } else {
        throw new Error(response.message || '의존성 조회 실패');
      }
    } catch (error) {
      console.error('❌ 의존성 조회 실패:', error);
      return [];
    }
  }

  // ========================================================================
  // 유틸리티 메서드
  // ========================================================================

  /**
   * 쿼리 파라미터 빌드
   */
  private buildQueryParams(filters?: VirtualPointFilters): Record<string, any> {
    if (!filters) return {};

    const params: Record<string, any> = {};

    // 각 필터를 쿼리 파라미터로 변환
    Object.entries(filters).forEach(([key, value]) => {
      if (value !== undefined && value !== null && value !== '') {
        params[key] = value;
      }
    });

    return params;
  }

  /**
   * 목업 스크립트 테스트 (백엔드 API 미구현 시 사용)
   */
  private mockScriptTest(request: ScriptTestRequest): any {
    const { expression, variables } = request;
    
    try {
      // 간단한 수식 평가 시뮬레이션
      if (expression.includes('temp1 + temp2 + temp3')) {
        const temp1 = variables.temp1 || 25;
        const temp2 = variables.temp2 || 26;  
        const temp3 = variables.temp3 || 24;
        return {
          success: true,
          result: temp1 + temp2 + temp3,
          execution_time: Math.random() * 10 + 5,
          warnings: []
        };
      }
      
      if (expression.includes('avg(')) {
        return {
          success: true,
          result: 25.4,
          execution_time: Math.random() * 10 + 5,
          warnings: []
        };
      }
      
      return {
        success: true,
        result: '테스트 결과 (목업)',
        execution_time: Math.random() * 10 + 5,
        warnings: ['백엔드 스크립트 테스트 API 미구현']
      };
      
    } catch (error) {
      return {
        success: false,
        error_message: '스크립트 테스트 실패',
        warnings: []
      };
    }
  }

  /**
   * 상태별 색상 가져오기
   */
  static getStatusColor(status: string): string {
    const colors = {
      active: '#10b981',    // 초록색
      error: '#ef4444',     // 빨간색
      disabled: '#6b7280',  // 회색
      calculating: '#f59e0b' // 주황색
    };
    return colors[status as keyof typeof colors] || colors.disabled;
  }

  /**
   * 상태별 아이콘 가져오기
   */
  static getStatusIcon(status: string): string {
    const icons = {
      active: 'fas fa-check-circle',
      error: 'fas fa-exclamation-triangle',
      disabled: 'fas fa-pause-circle',
      calculating: 'fas fa-spinner fa-spin'
    };
    return icons[status as keyof typeof icons] || icons.disabled;
  }

  /**
   * 데이터 타입별 아이콘 가져오기
   */
  static getDataTypeIcon(dataType: string): string {
    const icons = {
      number: 'fas fa-hashtag',
      boolean: 'fas fa-toggle-on',
      string: 'fas fa-quote-right'
    };
    return icons[dataType as keyof typeof icons] || icons.string;
  }

  /**
   * 카테고리별 색상 가져기
   */
  static getCategoryColor(category: string): string {
    const colors = {
      Temperature: '#ef4444',
      Pressure: '#3b82f6', 
      Flow: '#06b6d4',
      Power: '#f59e0b',
      Production: '#10b981',
      Quality: '#8b5cf6',
      Safety: '#f97316',
      Maintenance: '#84cc16',
      Energy: '#eab308',
      Custom: '#6b7280'
    };
    return colors[category as keyof typeof colors] || colors.Custom;
  }
}

// 싱글톤 인스턴스 생성 및 내보내기
export const virtualPointsApi = new VirtualPointsApiService();
export default virtualPointsApi;