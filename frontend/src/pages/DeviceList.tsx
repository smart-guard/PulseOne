// ============================================================================
// frontend/src/pages/DeviceList.tsx
// 리팩토링된 디바이스 목록 페이지 (새 API 구조 적용)
// ============================================================================

import React, { useState, useEffect } from 'react';
import DeviceDetailModal from '../components/modals/DeviceDetailModal';

// 🆕 새로운 API 구조 사용
import { DeviceApiService } from '../api/services/deviceApi';
import { usePagination } from '../hooks/usePagination';
import type { Device, DeviceListParams } from '../api/services/deviceApi';
import type { ApiResponse, PaginatedApiResponse } from '../types/common';

interface DeviceStats {
  total: number;
  running: number;
  stopped: number;
  error: number;
  connected: number;
  disconnected: number;
}

const DeviceList: React.FC = () => {
  // ==========================================================================
  // 상태 관리 (기존 유지)
  // ==========================================================================
  const [devices, setDevices] = useState<Device[]>([]);
  const [selectedDevices, setSelectedDevices] = useState<number[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [isProcessing, setIsProcessing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 필터 상태
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [protocolFilter, setProtocolFilter] = useState<string>('all');
  const [connectionFilter, setConnectionFilter] = useState<string>('all');

  // 실시간 업데이트
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [autoRefresh, setAutoRefresh] = useState(true);

  // 모달 상태
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null);
  const [modalMode, setModalMode] = useState<'view' | 'edit' | 'create'>('view');
  const [isModalOpen, setIsModalOpen] = useState(false);

  // 🆕 페이지네이션 훅 사용
  const pagination = usePagination({
    initialPageSize: 10,
    totalCount: devices.length
  });

  // ==========================================================================
  // 🚀 새로운 API 호출 함수들
  // ==========================================================================

  /**
   * 디바이스 목록 조회 (새 API 사용)
   */
  const fetchDevices = async () => {
    try {
      setIsLoading(true);
      setError(null);
      
      console.log('🔍 디바이스 목록 조회 시작... (새 API)');
      
      // 🆕 필터 파라미터 구성
      const params: DeviceListParams = {
        page: pagination.currentPage,
        limit: pagination.pageSize,
        ...(protocolFilter !== 'all' && { protocol_type: protocolFilter }),
        ...(connectionFilter !== 'all' && { connection_status: connectionFilter }),
        ...(searchTerm && { search: searchTerm })
      };
      
      // 🆕 새로운 API 서비스 사용
      const response: PaginatedApiResponse<Device> = await DeviceApiService.getDevices(params);
      
      if (response.success) {
        console.log('📋 디바이스 목록 조회 성공:', response.data);
        setDevices(response.data.items);
        
        // 페이지네이션 정보 업데이트 (향후 구현)
        // setPagination(response.data.pagination);
      } else {
        throw new Error(response.error || '디바이스 목록 조회 실패');
      }
    } catch (error) {
      console.error('❌ 디바이스 목록 조회 실패:', error);
      setError(error instanceof Error ? error.message : '알 수 없는 오류가 발생했습니다.');
      
      // 🔄 기존 방식으로 폴백 (개발 중)
      await fetchDevicesFallback();
    } finally {
      setIsLoading(false);
    }
  };

  /**
   * 폴백: 기존 방식으로 디바이스 조회
   */
  const fetchDevicesFallback = async () => {
    try {
      console.log('🔄 기존 API로 폴백...');
      const response = await fetch('http://localhost:3000/api/devices');
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      
      const result = await response.json();
      
      if (result.success && Array.isArray(result.data)) {
        setDevices(result.data);
      } else {
        setDevices([]);
      }
    } catch (error) {
      console.error('❌ 폴백 API도 실패:', error);
      setDevices([]);
    }
  };

  // ==========================================================================
  // 🎮 디바이스 제어 함수들 (새 API 사용)
  // ==========================================================================

  /**
   * 디바이스 활성화/비활성화
   */
  const handleDeviceToggle = async (device: Device) => {
    try {
      setIsProcessing(true);
      
      if (device.is_enabled) {
        console.log('🔄 디바이스 비활성화:', device.name);
        await DeviceApiService.disableDevice(device.id);
      } else {
        console.log('🔄 디바이스 활성화:', device.name);
        await DeviceApiService.enableDevice(device.id);
      }
      
      console.log('✅ 디바이스 상태 변경 완료');
      await fetchDevices(); // 목록 새로고침
      
    } catch (error) {
      console.error('❌ 디바이스 상태 변경 실패:', error);
      setError(error instanceof Error ? error.message : '디바이스 상태 변경에 실패했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 디바이스 재시작
   */
  const handleDeviceRestart = async (device: Device) => {
    try {
      setIsProcessing(true);
      console.log('🔄 디바이스 재시작:', device.name);
      
      await DeviceApiService.restartDevice(device.id);
      
      console.log('✅ 디바이스 재시작 완료');
      await fetchDevices();
      
    } catch (error) {
      console.error('❌ 디바이스 재시작 실패:', error);
      setError(error instanceof Error ? error.message : '디바이스 재시작에 실패했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 디바이스 연결 테스트
   */
  const handleConnectionTest = async (device: Device) => {
    try {
      setIsProcessing(true);
      console.log('🔄 연결 테스트:', device.name);
      
      const response = await DeviceApiService.testDeviceConnection(device.id);
      
      if (response.success) {
        console.log('✅ 연결 테스트 성공:', response.data);
        alert(`연결 테스트 성공!\n응답 시간: ${response.data?.response_time || 'N/A'}ms`);
      } else {
        console.log('❌ 연결 테스트 실패:', response.data);
        alert(`연결 테스트 실패!\n오류: ${response.data?.error || 'Unknown error'}`);
      }
      
      await fetchDevices();
      
    } catch (error) {
      console.error('❌ 연결 테스트 오류:', error);
      alert('연결 테스트 중 오류가 발생했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  // ==========================================================================
  // 🔄 일괄 작업 함수들 (새 API 사용)
  // ==========================================================================

  /**
   * 선택된 디바이스들 일괄 활성화
   */
  const handleBulkEnable = async () => {
    if (selectedDevices.length === 0) return;
    
    try {
      setIsProcessing(true);
      console.log('🔄 일괄 활성화:', selectedDevices);
      
      const response = await DeviceApiService.bulkEnableDevices(selectedDevices);
      
      if (response.success) {
        console.log('✅ 일괄 활성화 완료:', response);
        alert(`${response.processed_count}개 디바이스가 활성화되었습니다.`);
        setSelectedDevices([]);
        await fetchDevices();
      }
      
    } catch (error) {
      console.error('❌ 일괄 활성화 실패:', error);
      setError(error instanceof Error ? error.message : '일괄 활성화에 실패했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 선택된 디바이스들 일괄 비활성화
   */
  const handleBulkDisable = async () => {
    if (selectedDevices.length === 0) return;
    
    try {
      setIsProcessing(true);
      console.log('🔄 일괄 비활성화:', selectedDevices);
      
      const response = await DeviceApiService.bulkDisableDevices(selectedDevices);
      
      if (response.success) {
        console.log('✅ 일괄 비활성화 완료:', response);
        alert(`${response.processed_count}개 디바이스가 비활성화되었습니다.`);
        setSelectedDevices([]);
        await fetchDevices();
      }
      
    } catch (error) {
      console.error('❌ 일괄 비활성화 실패:', error);
      setError(error instanceof Error ? error.message : '일괄 비활성화에 실패했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 선택된 디바이스들 일괄 삭제
   */
  const handleBulkDelete = async () => {
    if (selectedDevices.length === 0) return;
    
    const confirmed = window.confirm(
      `선택된 ${selectedDevices.length}개 디바이스를 삭제하시겠습니까?\n이 작업은 되돌릴 수 없습니다.`
    );
    
    if (!confirmed) return;
    
    try {
      setIsProcessing(true);
      console.log('🔄 일괄 삭제:', selectedDevices);
      
      const response = await DeviceApiService.bulkDeleteDevices(selectedDevices);
      
      if (response.success) {
        console.log('✅ 일괄 삭제 완료:', response);
        alert(`${response.processed_count}개 디바이스가 삭제되었습니다.`);
        setSelectedDevices([]);
        await fetchDevices();
      }
      
    } catch (error) {
      console.error('❌ 일괄 삭제 실패:', error);
      setError(error instanceof Error ? error.message : '일괄 삭제에 실패했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  // ==========================================================================
  // 📝 CRUD 작업 함수들 (모달용)
  // ==========================================================================

  /**
   * 디바이스 저장/수정
   */
  const handleDeviceSave = async (device: Device) => {
    try {
      setIsProcessing(true);
      console.log('💾 디바이스 저장:', device);
      
      if (modalMode === 'create') {
        // 새 디바이스 생성
        const createData = {
          name: device.name,
          protocol_type: device.protocol_type,
          endpoint: device.endpoint,
          device_type: device.device_type,
          manufacturer: device.manufacturer,
          model: device.model,
          description: device.description,
          is_enabled: device.is_enabled
        };
        
        await DeviceApiService.createDevice(createData);
        console.log('✅ 새 디바이스 생성 완료');
        
      } else if (modalMode === 'edit') {
        // 기존 디바이스 수정
        const updateData = {
          name: device.name,
          protocol_type: device.protocol_type,
          endpoint: device.endpoint,
          device_type: device.device_type,
          manufacturer: device.manufacturer,
          model: device.model,
          description: device.description,
          is_enabled: device.is_enabled
        };
        
        await DeviceApiService.updateDevice(device.id, updateData);
        console.log('✅ 디바이스 수정 완료');
      }
      
      await fetchDevices();
      handleModalClose();
      
    } catch (error) {
      console.error('❌ 디바이스 저장 실패:', error);
      throw error; // 모달에서 에러 처리하도록 전파
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 디바이스 삭제
   */
  const handleDeviceDelete = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log('🗑️ 디바이스 삭제:', deviceId);
      
      await DeviceApiService.deleteDevice(deviceId);
      
      console.log('✅ 디바이스 삭제 완료');
      await fetchDevices();
      handleModalClose();
      
    } catch (error) {
      console.error('❌ 디바이스 삭제 실패:', error);
      throw error;
    } finally {
      setIsProcessing(false);
    }
  };

  // ==========================================================================
  // 기존 로직들 (UI 관련, 변경 없음)
  // ==========================================================================

  // 체크박스 선택 핸들러
  const handleDeviceSelect = (deviceId: number) => {
    setSelectedDevices(prev => 
      prev.includes(deviceId) 
        ? prev.filter(id => id !== deviceId)
        : [...prev, deviceId]
    );
  };

  const handleSelectAll = () => {
    if (selectedDevices.length === filteredDevices.length) {
      setSelectedDevices([]);
    } else {
      setSelectedDevices(filteredDevices.map(device => device.id));
    }
  };

  // 모달 핸들러들
  const handleModalOpen = (device: Device | null, mode: 'view' | 'edit' | 'create') => {
    setSelectedDevice(device);
    setModalMode(mode);
    setIsModalOpen(true);
    setAutoRefresh(false);
  };

  const handleModalClose = () => {
    setIsModalOpen(false);
    setSelectedDevice(null);
    setAutoRefresh(true);
  };

  // 필터링된 디바이스 목록
  const filteredDevices = devices.filter(device => {
    const matchesSearch = !searchTerm || 
      device.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      device.endpoint.toLowerCase().includes(searchTerm.toLowerCase()) ||
      device.protocol_type.toLowerCase().includes(searchTerm.toLowerCase()) ||
      (device.manufacturer?.toLowerCase().includes(searchTerm.toLowerCase())) ||
      (device.model?.toLowerCase().includes(searchTerm.toLowerCase()));
    
    const matchesStatus = statusFilter === 'all' || device.status === statusFilter;
    const matchesProtocol = protocolFilter === 'all' || device.protocol_type === protocolFilter;
    const matchesConnection = connectionFilter === 'all' || device.connection_status === connectionFilter;
    
    return matchesSearch && matchesStatus && matchesProtocol && matchesConnection;
  });

  // 통계 계산
  const stats: DeviceStats = {
    total: devices.length,
    running: devices.filter(d => d.status === 'running').length,
    stopped: devices.filter(d => d.status === 'stopped').length,
    error: devices.filter(d => d.status === 'error').length,
    connected: devices.filter(d => d.connection_status === 'connected').length,
    disconnected: devices.filter(d => d.connection_status === 'disconnected').length
  };

  // 고유 프로토콜 목록
  const protocols = [...new Set(devices.map(device => device.protocol_type))];

  // ==========================================================================
  // 생명주기 훅들
  // ==========================================================================

  // 실시간 업데이트 제어
  useEffect(() => {
    if (!autoRefresh || isModalOpen) return;

    const interval = setInterval(() => {
      if (!isModalOpen) {
        fetchDevices();
        setLastUpdate(new Date());
      }
    }, 30000); // 30초 간격

    return () => clearInterval(interval);
  }, [autoRefresh, isModalOpen]);

  // 컴포넌트 마운트 시 초기 데이터 로드
  useEffect(() => {
    fetchDevices();
  }, []);

  // 페이지네이션 변경 시 데이터 새로고침
  useEffect(() => {
    if (pagination.currentPage > 1) {
      fetchDevices();
    }
  }, [pagination.currentPage, pagination.pageSize]);

  // ==========================================================================
  // 렌더링 (기존 UI 유지)
  // ==========================================================================

  if (isLoading && devices.length === 0) {
    return (
      <div className="loading-container">
        <div className="loading-spinner"></div>
        <p>디바이스 목록을 불러오는 중... (새 API)</p>
      </div>
    );
  }

  return (
    <div className="device-list-container">
      {/* 헤더 */}
      <div className="device-list-header">
        <div className="header-left">
          <h1>
            <i className="fas fa-network-wired"></i>
            디바이스 관리
          </h1>
          <p className="subtitle">
            마지막 업데이트: {lastUpdate.toLocaleTimeString()} 
            <span className="api-indicator">(새 API 🚀)</span>
          </p>
        </div>
        <div className="header-actions">
          <button 
            className="btn btn-primary"
            onClick={() => handleModalOpen(null, 'create')}
            disabled={isProcessing}
          >
            <i className="fas fa-plus"></i>
            디바이스 추가
          </button>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="error-banner">
          <i className="fas fa-exclamation-triangle"></i>
          <span>{error}</span>
          <button onClick={() => setError(null)} className="error-close">
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 통계 카드들 */}
      <div className="stats-grid">
        <div className="stat-card">
          <div className="stat-icon total">
            <i className="fas fa-server"></i>
          </div>
          <div className="stat-content">
            <div className="stat-number">{stats.total}</div>
            <div className="stat-label">총 디바이스</div>
          </div>
        </div>

        <div className="stat-card">
          <div className="stat-icon running">
            <i className="fas fa-play-circle"></i>
          </div>
          <div className="stat-content">
            <div className="stat-number">{stats.running}</div>
            <div className="stat-label">실행 중</div>
          </div>
        </div>

        <div className="stat-card">
          <div className="stat-icon connected">
            <i className="fas fa-wifi"></i>
          </div>
          <div className="stat-content">
            <div className="stat-number">{stats.connected}</div>
            <div className="stat-label">연결됨</div>
          </div>
        </div>

        <div className="stat-card">
          <div className="stat-icon error">
            <i className="fas fa-exclamation-triangle"></i>
          </div>
          <div className="stat-content">
            <div className="stat-number">{stats.error}</div>
            <div className="stat-label">오류</div>
          </div>
        </div>
      </div>

      {/* 필터 및 검색 바 */}
      <div className="controls-panel">
        <div className="search-section">
          <div className="search-box">
            <i className="fas fa-search"></i>
            <input
              type="text"
              placeholder="디바이스명, 엔드포인트, 프로토콜로 검색..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
            />
          </div>
        </div>

        <div className="filter-section">
          <select
            value={statusFilter}
            onChange={(e) => setStatusFilter(e.target.value)}
          >
            <option value="all">모든 상태</option>
            <option value="running">실행 중</option>
            <option value="stopped">정지</option>
            <option value="error">오류</option>
          </select>

          <select
            value={protocolFilter}
            onChange={(e) => setProtocolFilter(e.target.value)}
          >
            <option value="all">모든 프로토콜</option>
            {protocols.map(protocol => (
              <option key={protocol} value={protocol}>
                {protocol.toUpperCase()}
              </option>
            ))}
          </select>

          <select
            value={connectionFilter}
            onChange={(e) => setConnectionFilter(e.target.value)}
          >
            <option value="all">모든 연결 상태</option>
            <option value="connected">연결됨</option>
            <option value="disconnected">연결끊김</option>
            <option value="connecting">연결 중</option>
          </select>
        </div>

        {/* 일괄 작업 버튼들 */}
        {selectedDevices.length > 0 && (
          <div className="bulk-actions">
            <span className="selected-count">
              {selectedDevices.length}개 선택됨
            </span>
            <button
              className="btn btn-sm btn-success"
              onClick={handleBulkEnable}
              disabled={isProcessing}
            >
              <i className="fas fa-play"></i>
              일괄 활성화
            </button>
            <button
              className="btn btn-sm btn-warning"
              onClick={handleBulkDisable}
              disabled={isProcessing}
            >
              <i className="fas fa-pause"></i>
              일괄 비활성화
            </button>
            <button
              className="btn btn-sm btn-danger"
              onClick={handleBulkDelete}
              disabled={isProcessing}
            >
              <i className="fas fa-trash"></i>
              일괄 삭제
            </button>
          </div>
        )}
      </div>

      {/* 디바이스 테이블 */}
      <div className="devices-table-container">
        <table className="devices-table">
          <thead>
            <tr>
              <th className="checkbox-col">
                <input
                  type="checkbox"
                  checked={selectedDevices.length === filteredDevices.length && filteredDevices.length > 0}
                  onChange={handleSelectAll}
                />
              </th>
              <th>디바이스명</th>
              <th>프로토콜</th>
              <th>엔드포인트</th>
              <th>상태</th>
              <th>연결</th>
              <th>마지막 통신</th>
              <th>액션</th>
            </tr>
          </thead>
          <tbody>
            {filteredDevices.map((device) => (
              <tr
                key={device.id}
                className={`device-row ${selectedDevices.includes(device.id) ? 'selected' : ''}`}
              >
                <td>
                  <input
                    type="checkbox"
                    checked={selectedDevices.includes(device.id)}
                    onChange={() => handleDeviceSelect(device.id)}
                  />
                </td>
                
                <td>
                  <div className="device-info">
                    <div className="device-name">
                      {device.name}
                      {!device.is_enabled && (
                        <span className="disabled-badge">비활성</span>
                      )}
                    </div>
                    <div className="device-details">
                      {device.manufacturer} {device.model}
                    </div>
                  </div>
                </td>
                
                <td>
                  <span className={`protocol-badge ${device.protocol_type}`}>
                    {device.protocol_type.toUpperCase()}
                  </span>
                </td>
                
                <td>
                  <code className="endpoint">{device.endpoint}</code>
                </td>
                
                <td>
                  <span className={`status-badge ${device.status}`}>
                    <i className={`fas fa-circle`}></i>
                    {device.status === 'running' ? '실행 중' :
                     device.status === 'stopped' ? '정지' :
                     device.status === 'error' ? '오류' : device.status}
                  </span>
                </td>
                
                <td>
                  <span className={`connection-badge ${device.connection_status}`}>
                    <i className={`fas ${
                      device.connection_status === 'connected' ? 'fa-wifi' :
                      device.connection_status === 'disconnected' ? 'fa-wifi' :
                      'fa-circle-notch fa-spin'
                    }`}></i>
                    {device.connection_status === 'connected' ? '연결됨' :
                     device.connection_status === 'disconnected' ? '연결끊김' :
                     device.connection_status === 'connecting' ? '연결 중' : '알수없음'}
                  </span>
                </td>
                
                <td>
                  <div className="last-seen">
                    {device.last_seen ? new Date(device.last_seen).toLocaleString('ko-KR') : 'N/A'}
                  </div>
                </td>
                
                <td>
                  <div className="action-buttons">
                    {/* 활성화/비활성화 토글 */}
                    <button
                      className={`btn btn-xs ${device.is_enabled ? 'btn-warning' : 'btn-success'}`}
                      onClick={() => handleDeviceToggle(device)}
                      disabled={isProcessing}
                      title={device.is_enabled ? '비활성화' : '활성화'}
                    >
                      <i className={`fas ${device.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>
                    </button>
                    
                    {/* 재시작 */}
                    <button
                      className="btn btn-xs btn-info"
                      onClick={() => handleDeviceRestart(device)}
                      disabled={isProcessing}
                      title="재시작"
                    >
                      <i className="fas fa-redo"></i>
                    </button>
                    
                    {/* 연결 테스트 */}
                    <button
                      className="btn btn-xs btn-secondary"
                      onClick={() => handleConnectionTest(device)}
                      disabled={isProcessing}
                      title="연결 테스트"
                    >
                      <i className="fas fa-plug"></i>
                    </button>
                    
                    {/* 상세보기 */}
                    <button
                      className="btn btn-xs btn-primary"
                      onClick={() => handleModalOpen(device, 'view')}
                      title="상세보기"
                    >
                      <i className="fas fa-eye"></i>
                    </button>
                    
                    {/* 편집 */}
                    <button
                      className="btn btn-xs btn-warning"
                      onClick={() => handleModalOpen(device, 'edit')}
                      title="편집"
                    >
                      <i className="fas fa-edit"></i>
                    </button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>

        {filteredDevices.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-server"></i>
            <h3>디바이스가 없습니다</h3>
            <p>조건에 맞는 디바이스를 찾을 수 없습니다. 필터를 확인하거나 새 디바이스를 추가해보세요.</p>
            <button
              className="btn btn-primary"
              onClick={() => handleModalOpen(null, 'create')}
            >
              <i className="fas fa-plus"></i>
              첫 번째 디바이스 추가
            </button>
          </div>
        )}
      </div>

      {/* 🆕 페이지네이션 (향후 활성화) */}
      {/*
      <Pagination
        current={pagination.currentPage}
        total={pagination.totalCount}
        pageSize={pagination.pageSize}
        onChange={(page, pageSize) => {
          pagination.goToPage(page);
          pagination.changePageSize(pageSize);
        }}
        showSizeChanger
        showQuickJumper
        showTotal
      />
      */}

      {/* 디바이스 상세 모달 */}
      <DeviceDetailModal
        device={selectedDevice}
        isOpen={isModalOpen}
        mode={modalMode}
        onClose={handleModalClose}
        onSave={handleDeviceSave}
        onDelete={handleDeviceDelete}
        onTestConnection={handleConnectionTest}
      />

      {/* 로딩 오버레이 */}
      {isProcessing && (
        <div className="processing-overlay">
          <div className="processing-spinner">
            <div className="loading-spinner"></div>
            <p>처리 중...</p>
          </div>
        </div>
      )}

      {/* CSS는 기존 그대로 사용 */}
    </div>
  );
};

export default DeviceList;