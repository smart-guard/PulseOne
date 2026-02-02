// ============================================================================
// frontend/src/api/services/protocolApi.ts
// 기존 DeviceApi 패턴 100% 동일하게 적용한 프로토콜 API 서비스
// ============================================================================

import { apiClient } from '../client';
import { ApiResponse, PaginatedApiResponse, PaginatedResponse } from '../../types/common';
export type { ApiResponse, PaginatedApiResponse, PaginatedResponse };

// ============================================================================
// 프로토콜 관련 인터페이스들
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
  capabilities?: {
    serial?: 'supported' | 'unsupported' | 'required';
    broker?: 'supported' | 'unsupported' | 'required';
  };
  default_polling_interval?: number;
  default_timeout?: number;
  max_concurrent_connections?: number;
  min_firmware_version?: string;
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

export interface ProtocolInstance {
  id: number;
  protocol_id: number;
  instance_name: string;
  description?: string;
  vhost?: string;
  api_key?: string;
  api_key_updated_at?: string;
  connection_params?: Record<string, any>;
  is_enabled: boolean;
  status: 'RUNNING' | 'STOPPED' | 'ERROR';
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
// ProtocolApiService 클래스
// ============================================================================

export class ProtocolApiService {
  private static readonly BASE_URL = '/api/protocols';

  /**
   * 프로토콜 목록 조회 - apiClient 사용 (PaginatedResponse 반환 가능성 대응)
   */
  static async getProtocols(filters?: {
    category?: string;
    enabled?: string;
    deprecated?: string;
    search?: string;
    limit?: number;
    offset?: number;
    sortBy?: string;
    sortOrder?: string;
  }): Promise<PaginatedApiResponse<Protocol>> {
    console.log('📋 프로토콜 목록 조회...', filters);
    return await apiClient.get<any>(this.BASE_URL, filters);
  }

  /**
   * 프로토콜 상세 조회
   */
  static async getProtocol(id: number): Promise<ApiResponse<Protocol>> {
    return await apiClient.get<Protocol>(`${this.BASE_URL}/${id}`);
  }

  /**
   * 프로토콜 생성
   */
  static async createProtocol(data: ProtocolCreateData): Promise<ApiResponse<Protocol>> {
    return await apiClient.post<Protocol>(this.BASE_URL, data);
  }

  /**
   * 프로토콜 수정
   */
  static async updateProtocol(id: number, data: ProtocolUpdateData): Promise<ApiResponse<Protocol>> {
    return await apiClient.put<Protocol>(`${this.BASE_URL}/${id}`, data);
  }

  /**
   * 프로토콜 삭제
   */
  static async deleteProtocol(id: number, force = false): Promise<ApiResponse<{ deleted: boolean }>> {
    const url = force ? `${this.BASE_URL}/${id}?force=true` : `${this.BASE_URL}/${id}`;
    return await apiClient.delete<{ deleted: boolean }>(url);
  }

  /**
   * 프로토콜 활성화
   */
  static async enableProtocol(id: number): Promise<ApiResponse<Protocol>> {
    return await apiClient.post<Protocol>(`${this.BASE_URL}/${id}/enable`);
  }

  /**
   * 프로토콜 비활성화
   */
  static async disableProtocol(id: number): Promise<ApiResponse<Protocol>> {
    return await apiClient.post<Protocol>(`${this.BASE_URL}/${id}/disable`);
  }

  /**
   * 프로토콜 통계 조회
   */
  static async getProtocolStatistics(): Promise<ApiResponse<ProtocolStats>> {
    return await apiClient.get<ProtocolStats>(`${this.BASE_URL}/statistics`);
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
    return await apiClient.post<any>(`${this.BASE_URL}/${id}/test`, params);
  }

  /**
   * MQTT 브로커 상태 조회
   */
  static async getBrokerStatus(): Promise<ApiResponse<{
    is_healthy: boolean;
    stats: any;
    health_details: any;
    connection_list: any[];
    timestamp: string;
  }>> {
    return await apiClient.get<any>(`${this.BASE_URL}/broker/status`);
  }

  /**
   * 프로토콜 인스턴스 목록 조회
   */
  static async getProtocolInstances(protocolId: number, page: number = 1, limit: number = 20): Promise<PaginatedApiResponse<ProtocolInstance>> {
    return await apiClient.get<PaginatedResponse<ProtocolInstance>>(`${this.BASE_URL}/${protocolId}/instances?page=${page}&limit=${limit}`);
  }

  /**
   * 프로토콜 인스턴스 생성
   */
  static async createProtocolInstance(protocolId: number, data: Partial<ProtocolInstance>): Promise<ApiResponse<ProtocolInstance>> {
    return await apiClient.post<ProtocolInstance>(this.BASE_URL + "/" + protocolId + "/instances", data);
  }

  /**
   * 프로토콜 인스턴스 수정
   */
  static async updateProtocolInstance(instanceId: number, data: Partial<ProtocolInstance>): Promise<ApiResponse<ProtocolInstance>> {
    return await apiClient.put<ProtocolInstance>(this.BASE_URL + "/instances/" + instanceId, data);
  }

  /**
   * 프로토콜 인스턴스 삭제
   */
  static async deleteProtocolInstance(instanceId: number): Promise<ApiResponse<{ deleted: boolean }>> {
    return await apiClient.delete<{ deleted: boolean }>(this.BASE_URL + "/instances/" + instanceId);
  }

  /**
   * 특정 프로토콜을 사용하는 디바이스 목록 조회
   */
  static async getProtocolDevices(protocolId: number, params?: { limit?: number; offset?: number }): Promise<ApiResponse<any[]>> {
    return await apiClient.get<any[]>(`${this.BASE_URL}/${protocolId}/devices`, params);
  }
}
