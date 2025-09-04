// ============================================================================
// frontend/src/api/services/protocolApi.ts
// 프로토콜 관리 API 서비스 - 올바른 엔드포인트 경로로 수정
// ============================================================================

import { ApiResponse } from '../../types/common';

// ============================================================================
// 🔧 프로토콜 관련 인터페이스들
// ============================================================================

export interface Protocol {
  id: number;
  protocol_type: string;
  display_name: string;
  description: string;
  default_port?: number;
  uses_serial?: boolean;
  requires_broker?: boolean;
  supported_operations?: string[];
  supported_data_types?: string[];
  connection_params?: Record<string, any>;
  default_polling_interval?: number;
  default_timeout?: number;
  category?: string;
  vendor?: string;
  standard_reference?: string;
  is_enabled?: boolean;
  is_deprecated?: boolean;
  device_count?: number;
  enabled_count?: number;
  connected_count?: number;
  created_at?: string;
  updated_at?: string;
}

export interface ProtocolStats {
  total_protocols: number;
  enabled_protocols: number;
  deprecated_protocols: number;
  categories: Array<{
    category: string;
    count: number;
    percentage: number;
  }>;
  usage_stats: Array<{
    protocol_type: string;
    display_name: string;
    device_count: number;
    enabled_devices: number;
    connected_devices: number;
  }>;
}

export interface ProtocolCreateData {
  protocol_type: string;
  display_name: string;
  description: string;
  default_port?: number;
  uses_serial?: boolean;
  requires_broker?: boolean;
  supported_operations?: string[];
  supported_data_types?: string[];
  connection_params?: Record<string, any>;
  default_polling_interval?: number;
  default_timeout?: number;
  category?: string;
  vendor?: string;
  standard_reference?: string;
  is_enabled?: boolean;
}

export interface ProtocolUpdateData {
  display_name?: string;
  description?: string;
  default_port?: number;
  default_polling_interval?: number;
  default_timeout?: number;
  category?: string;
  vendor?: string;
  standard_reference?: string;
  is_enabled?: boolean;
  is_deprecated?: boolean;
}

// ============================================================================
// ProtocolApiService 클래스 - 올바른 엔드포인트 사용
// ============================================================================

export class ProtocolApiService {
  // 🔥 수정: 올바른 엔드포인트 경로
  private static readonly BASE_URL = '/api/protocols';

  // ========================================================================
  // 📋 기본 CRUD API들
  // ========================================================================

  /**
   * 프로토콜 목록 조회
   */
  static async getProtocols(filters?: {
    category?: string;
    enabled?: string;
    deprecated?: string;
    search?: string;
  }): Promise<ApiResponse<Protocol[]>> {
    try {
      console.log('📋 프로토콜 목록 조회...', filters);

      let url = this.BASE_URL;
      if (filters) {
        const params = new URLSearchParams();
        Object.entries(filters).forEach(([key, value]) => {
          if (value !== undefined && value !== '') {
            params.append(key, value);
          }
        });
        
        if (params.toString()) {
          url += `?${params.toString()}`;
        }
      }

      const response = await fetch(url);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ${result.data?.length || 0}개 조회 완료`);
      
      return result;
      
    } catch (error) {
      console.error('❌ 프로토콜 목록 조회 실패:', error);
      throw error;
    }
  }

  /**
   * 프로토콜 상세 조회
   */
  static async getProtocol(id: number): Promise<ApiResponse<Protocol>> {
    try {
      console.log(`🔍 프로토콜 ID ${id} 상세 조회...`);

      const response = await fetch(`${this.BASE_URL}/${id}`);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ID ${id} 조회 완료`);
      
      return result;
      
    } catch (error) {
      console.error(`❌ 프로토콜 ID ${id} 조회 실패:`, error);
      throw error;
    }
  }

  /**
   * 프로토콜 생성
   */
  static async createProtocol(data: ProtocolCreateData): Promise<ApiResponse<Protocol>> {
    try {
      console.log('➕ 새 프로토콜 생성:', data.protocol_type);

      const response = await fetch(this.BASE_URL, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ${data.protocol_type} 생성 완료`);
      
      return result;
      
    } catch (error) {
      console.error(`❌ 프로토콜 ${data.protocol_type} 생성 실패:`, error);
      throw error;
    }
  }

  /**
   * 프로토콜 수정
   */
  static async updateProtocol(id: number, data: ProtocolUpdateData): Promise<ApiResponse<Protocol>> {
    try {
      console.log(`📝 프로토콜 ID ${id} 수정...`);

      const response = await fetch(`${this.BASE_URL}/${id}`, {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ID ${id} 수정 완료`);
      
      return result;
      
    } catch (error) {
      console.error(`❌ 프로토콜 ID ${id} 수정 실패:`, error);
      throw error;
    }
  }

  /**
   * 프로토콜 삭제
   */
  static async deleteProtocol(id: number, force = false): Promise<ApiResponse<{ deleted: boolean }>> {
    try {
      console.log(`🗑️ 프로토콜 ID ${id} 삭제... (force: ${force})`);

      const url = force ? `${this.BASE_URL}/${id}?force=true` : `${this.BASE_URL}/${id}`;
      const response = await fetch(url, {
        method: 'DELETE',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ID ${id} 삭제 완료`);
      
      return result;
      
    } catch (error) {
      console.error(`❌ 프로토콜 ID ${id} 삭제 실패:`, error);
      throw error;
    }
  }

  // ========================================================================
  // 🔄 프로토콜 제어 API들
  // ========================================================================

  /**
   * 프로토콜 활성화
   */
  static async enableProtocol(id: number): Promise<ApiResponse<Protocol>> {
    try {
      console.log(`🟢 프로토콜 ID ${id} 활성화...`);

      const response = await fetch(`${this.BASE_URL}/${id}/enable`, {
        method: 'POST',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ID ${id} 활성화 완료`);
      
      return result;
      
    } catch (error) {
      console.error(`❌ 프로토콜 ID ${id} 활성화 실패:`, error);
      throw error;
    }
  }

  /**
   * 프로토콜 비활성화
   */
  static async disableProtocol(id: number): Promise<ApiResponse<Protocol>> {
    try {
      console.log(`🔴 프로토콜 ID ${id} 비활성화...`);

      const response = await fetch(`${this.BASE_URL}/${id}/disable`, {
        method: 'POST',
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ID ${id} 비활성화 완료`);
      
      return result;
      
    } catch (error) {
      console.error(`❌ 프로토콜 ID ${id} 비활성화 실패:`, error);
      throw error;
    }
  }

  // ========================================================================
  // 📊 통계 및 분석 API들
  // ========================================================================

  /**
   * 프로토콜 통계 조회
   */
  static async getProtocolStatistics(): Promise<ApiResponse<ProtocolStats>> {
    try {
      console.log('📊 프로토콜 통계 조회...');

      const response = await fetch(`${this.BASE_URL}/statistics`);
      
      if (!response.ok) {
        console.warn(`통계 조회 실패: HTTP ${response.status}`);
        // 통계는 필수가 아니므로 기본값 반환
        return {
          success: true,
          data: {
            total_protocols: 0,
            enabled_protocols: 0,
            deprecated_protocols: 0,
            categories: [],
            usage_stats: []
          }
        };
      }
      
      const result = await response.json();
      console.log('✅ 프로토콜 통계 조회 완료');
      
      return result;
      
    } catch (error) {
      console.warn('⚠️ 프로토콜 통계 조회 실패:', error);
      // 통계는 필수가 아니므로 기본값 반환
      return {
        success: true,
        data: {
          total_protocols: 0,
          enabled_protocols: 0,
          deprecated_protocols: 0,
          categories: [],
          usage_stats: []
        }
      };
    }
  }

  /**
   * 프로토콜 연결 테스트
   */
  static async testProtocolConnection(id: number, params: Record<string, any>): Promise<ApiResponse<{
    protocol_id: number;
    protocol_type: string;
    test_successful: boolean;
    response_time_ms?: number;
    test_timestamp: string;
    error_message?: string;
  }>> {
    try {
      console.log(`🔗 프로토콜 ID ${id} 연결 테스트...`, params);

      const response = await fetch(`${this.BASE_URL}/${id}/test`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(params),
      });
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log(`✅ 프로토콜 ID ${id} 연결 테스트 완료`);
      
      return result;
      
    } catch (error) {
      console.error(`❌ 프로토콜 ID ${id} 연결 테스트 실패:`, error);
      throw error;
    }
  }
}