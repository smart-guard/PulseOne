// ============================================================================
// 수정된 VirtualPointsApiService - 목 데이터 완전 제거
// ============================================================================

class VirtualPointsApiService {
  private readonly baseUrl = '/api/virtual-points';

  /**
   * 가상포인트 목록 조회 - 목 데이터 제거
   */
  async getVirtualPoints(filters?: any, pagination?: {
    page: number;
    pageSize: number;
    sortBy?: string;
    sortOrder?: 'asc' | 'desc';
  }): Promise<{
    items: any[];
    totalCount: number;
    pageInfo: {
      currentPage: number;
      pageSize: number;
      totalPages: number;
      hasNext: boolean;
      hasPrev: boolean;
    };
  }> {
    console.log('가상포인트 목록 조회:', { filters, pagination });

    const params = {
      ...filters,
      ...pagination
    };

    const response = await fetch(`${this.baseUrl}?${new URLSearchParams(params)}`, {
      method: 'GET',
      headers: {
        'Content-Type': 'application/json'
      }
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: 가상포인트 목록 조회 실패`);
    }

    const result = await response.json();

    if (!result.success) {
      throw new Error(result.message || '가상포인트 조회 실패');
    }

    // 백엔드 응답 형식에 맞게 처리
    const data = result.data || [];
    
    return {
      items: Array.isArray(data) ? data : [],
      totalCount: Array.isArray(data) ? data.length : 0,
      pageInfo: {
        currentPage: pagination?.page || 1,
        pageSize: pagination?.pageSize || 10,
        totalPages: Math.ceil((Array.isArray(data) ? data.length : 0) / (pagination?.pageSize || 10)),
        hasNext: false,
        hasPrev: false
      }
    };
  }

  /**
   * 가상포인트 생성
   */
  async createVirtualPoint(data: any): Promise<any> {
    console.log('가상포인트 생성:', data);

    // 백엔드 형식에 맞게 변환
    const requestData = {
      virtualPoint: {
        ...data,
        formula: data.expression || data.formula,
        tenant_id: data.tenant_id || 1
      },
      inputs: data.input_variables || []
    };

    const response = await fetch(this.baseUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(requestData)
    });

    if (!response.ok) {
      const errorResult = await response.json();
      throw new Error(errorResult.message || `HTTP ${response.status}: 가상포인트 생성 실패`);
    }

    const result = await response.json();

    if (!result.success) {
      throw new Error(result.message || '가상포인트 생성 실패');
    }

    return result.data;
  }

  /**
   * 가상포인트 삭제
   */
  async deleteVirtualPoint(id: number): Promise<{ deleted: boolean }> {
    console.log('가상포인트 삭제:', id);

    const response = await fetch(`/api/virtual-points/${id}`, {
        method: 'DELETE',
        headers: {
        'Content-Type': 'application/json'
        }
    });

    if (!response.ok) {
        const errorResult = await response.json();
        console.error('삭제 에러 응답:', errorResult);
        throw new Error(errorResult.message || `HTTP ${response.status}: 가상포인트 삭제 실패`);
    }

    const result = await response.json();

    if (!result.success) {
        throw new Error(result.message || '가상포인트 삭제 실패');
    }

    return { deleted: true };
    }

  /**
   * 가상포인트 수정
   */
  async updateVirtualPoint(id: number, data: any): Promise<any> {
    console.log('가상포인트 수정:', { id, data });

    // 백엔드 형식에 맞게 변환
    const requestData = {
      virtualPoint: {
        ...data,
        formula: data.expression || data.formula
      },
      inputs: data.input_variables || null
    };

    const response = await fetch(`${this.baseUrl}/${id}`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(requestData)
    });

    if (!response.ok) {
      const errorResult = await response.json();
      throw new Error(errorResult.message || `HTTP ${response.status}: 가상포인트 수정 실패`);
    }

    const result = await response.json();

    if (!result.success) {
      throw new Error(result.message || '가상포인트 수정 실패');
    }

    return result.data;
  }

  /**
   * 가상포인트 실행 - 404 대응
   */
  async executeVirtualPoint(id: number): Promise<any> {
    console.log('가상포인트 실행:', id);

    try {
      const response = await fetch(`${this.baseUrl}/${id}/execute`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          timeout: 10000,
          context: {}
        })
      });

      if (response.status === 404) {
        throw new Error('실행 기능이 아직 구현되지 않았습니다');
      }

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: 가상포인트 실행 실패`);
      }

      const result = await response.json();
      return result.data || result;

    } catch (error) {
      console.error('가상포인트 실행 실패:', error);
      throw error;
    }
  }
}

// 싱글톤 인스턴스 생성 및 export
export const virtualPointsApi = new VirtualPointsApiService();