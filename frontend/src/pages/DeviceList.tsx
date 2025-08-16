// ============================================================================
// frontend/src/pages/DeviceList.tsx 
// 📝 디바이스 목록 페이지 - 새로운 DeviceApiService 완전 연결
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { DeviceApiService, Device, DeviceStats } from '../api/services/deviceApi';
import '../styles/base.css';
import '../styles/device-list.css';
import '../styles/pagination.css';

const DeviceList: React.FC = () => {
  // 🔧 기본 상태들
  const [devices, setDevices] = useState<Device[]>([]);
  const [deviceStats, setDeviceStats] = useState<DeviceStats | null>(null);
  const [selectedDevices, setSelectedDevices] = useState<number[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [isProcessing, setIsProcessing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 필터 상태
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [protocolFilter, setProtocolFilter] = useState<string>('all');
  const [connectionFilter, setConnectionFilter] = useState<string>('all');
  const [availableProtocols, setAvailableProtocols] = useState<string[]>([]);

  // 실시간 업데이트
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [autoRefresh, setAutoRefresh] = useState(true);

  // 모달 상태
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null);
  const [modalMode, setModalMode] = useState<'view' | 'edit' | 'create'>('view');
  const [isModalOpen, setIsModalOpen] = useState(false);

  // 🔥 페이징 훅 사용
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 25,
    totalCount: devices.length
  });

  // =============================================================================
  // 🔄 데이터 로드 함수들 (새로운 API 사용)
  // =============================================================================

  /**
   * 디바이스 목록 로드
   */
  const loadDevices = useCallback(async () => {
    try {
      setIsLoading(true);
      setError(null);

      console.log('📱 디바이스 목록 로드 시작...');

      const response = await DeviceApiService.getDevices({
        page: pagination.currentPage,
        limit: pagination.pageSize,
        protocol_type: protocolFilter !== 'all' ? protocolFilter : undefined,
        connection_status: connectionFilter !== 'all' ? connectionFilter : undefined,
        status: statusFilter !== 'all' ? statusFilter : undefined,
        search: searchTerm || undefined,
        sort_by: 'name',
        sort_order: 'ASC'
      });

      if (response.success && response.data) {
        setDevices(response.data.items);
        pagination.updateTotalCount(response.data.pagination.total);
        
        console.log(`✅ 디바이스 ${response.data.items.length}개 로드 완료`);
      } else {
        throw new Error(response.error || '디바이스 목록 로드 실패');
      }

    } catch (err) {
      console.error('❌ 디바이스 목록 로드 실패:', err);
      setError(err instanceof Error ? err.message : '알 수 없는 오류');
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  }, [pagination.currentPage, pagination.pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm]);

  /**
   * 디바이스 통계 로드
   */
  const loadDeviceStats = useCallback(async () => {
    try {
      console.log('📊 디바이스 통계 로드 시작...');

      const response = await DeviceApiService.getDeviceStatistics();

      if (response.success && response.data) {
        setDeviceStats(response.data);
        console.log('✅ 디바이스 통계 로드 완료');
      } else {
        console.warn('⚠️ 디바이스 통계 로드 실패:', response.error);
      }
    } catch (err) {
      console.warn('⚠️ 디바이스 통계 로드 실패:', err);
    }
  }, []);

  /**
   * 지원 프로토콜 목록 로드
   */
  const loadAvailableProtocols = useCallback(async () => {
    try {
      console.log('📋 지원 프로토콜 로드 시작...');

      const response = await DeviceApiService.getAvailableProtocols();

      if (response.success && response.data) {
        const protocols = response.data.map(p => p.protocol_type);
        setAvailableProtocols(protocols);
        console.log('✅ 지원 프로토콜 로드 완료:', protocols);
      } else {
        console.warn('⚠️ 지원 프로토콜 로드 실패:', response.error);
      }
    } catch (err) {
      console.warn('⚠️ 지원 프로토콜 로드 실패:', err);
    }
  }, []);

  // =============================================================================
  // 🔄 디바이스 제어 함수들 (새로운 API 사용)
  // =============================================================================

  /**
   * 디바이스 활성화
   */
  const handleEnableDevice = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log(`🟢 디바이스 ${deviceId} 활성화 시작...`);

      const response = await DeviceApiService.enableDevice(deviceId);

      if (response.success) {
        console.log(`✅ 디바이스 ${deviceId} 활성화 완료`);
        await loadDevices(); // 목록 새로고침
        await loadDeviceStats(); // 통계 새로고침
      } else {
        throw new Error(response.error || '디바이스 활성화 실패');
      }
    } catch (err) {
      console.error(`❌ 디바이스 ${deviceId} 활성화 실패:`, err);
      setError(err instanceof Error ? err.message : '디바이스 활성화 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 디바이스 비활성화
   */
  const handleDisableDevice = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log(`🔴 디바이스 ${deviceId} 비활성화 시작...`);

      const response = await DeviceApiService.disableDevice(deviceId);

      if (response.success) {
        console.log(`✅ 디바이스 ${deviceId} 비활성화 완료`);
        await loadDevices();
        await loadDeviceStats();
      } else {
        throw new Error(response.error || '디바이스 비활성화 실패');
      }
    } catch (err) {
      console.error(`❌ 디바이스 ${deviceId} 비활성화 실패:`, err);
      setError(err instanceof Error ? err.message : '디바이스 비활성화 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 디바이스 재시작
   */
  const handleRestartDevice = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log(`🔄 디바이스 ${deviceId} 재시작 시작...`);

      const response = await DeviceApiService.restartDevice(deviceId);

      if (response.success) {
        console.log(`✅ 디바이스 ${deviceId} 재시작 완료`);
        await loadDevices();
      } else {
        throw new Error(response.error || '디바이스 재시작 실패');
      }
    } catch (err) {
      console.error(`❌ 디바이스 ${deviceId} 재시작 실패:`, err);
      setError(err instanceof Error ? err.message : '디바이스 재시작 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 디바이스 연결 테스트
   */
  const handleTestConnection = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log(`🔗 디바이스 ${deviceId} 연결 테스트 시작...`);

      const response = await DeviceApiService.testDeviceConnection(deviceId);

      if (response.success && response.data) {
        const result = response.data;
        const message = result.test_successful 
          ? `연결 성공 (응답시간: ${result.response_time_ms}ms)`
          : `연결 실패: ${result.error_message}`;
        
        alert(message);
        console.log(`✅ 디바이스 ${deviceId} 연결 테스트 완료:`, result);
        
        if (result.test_successful) {
          await loadDevices(); // 연결 상태 업데이트
        }
      } else {
        throw new Error(response.error || '연결 테스트 실패');
      }
    } catch (err) {
      console.error(`❌ 디바이스 ${deviceId} 연결 테스트 실패:`, err);
      setError(err instanceof Error ? err.message : '연결 테스트 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 일괄 작업 처리
   */
  const handleBulkAction = async (action: 'enable' | 'disable' | 'delete') => {
    if (selectedDevices.length === 0) {
      alert('작업할 디바이스를 선택해주세요.');
      return;
    }

    const confirmMessage = `선택된 ${selectedDevices.length}개 디바이스를 ${action === 'enable' ? '활성화' : action === 'disable' ? '비활성화' : '삭제'}하시겠습니까?`;
    
    if (!window.confirm(confirmMessage)) {
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`🔄 일괄 ${action} 시작:`, selectedDevices);

      const response = await DeviceApiService.bulkAction({
        action,
        device_ids: selectedDevices
      });

      if (response.success && response.data) {
        const result = response.data;
        const message = `작업 완료: 성공 ${result.successful}개, 실패 ${result.failed}개`;
        alert(message);
        
        console.log(`✅ 일괄 ${action} 완료:`, result);
        
        setSelectedDevices([]); // 선택 해제
        await loadDevices();
        await loadDeviceStats();
      } else {
        throw new Error(response.error || '일괄 작업 실패');
      }
    } catch (err) {
      console.error(`❌ 일괄 ${action} 실패:`, err);
      setError(err instanceof Error ? err.message : '일괄 작업 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 🔄 이벤트 핸들러들
  // =============================================================================

  const handleSearch = useCallback((term: string) => {
    setSearchTerm(term);
    pagination.goToFirst();
  }, [pagination]);

  const handleFilterChange = useCallback((filterType: string, value: string) => {
    switch (filterType) {
      case 'status':
        setStatusFilter(value);
        break;
      case 'protocol':
        setProtocolFilter(value);
        break;
      case 'connection':
        setConnectionFilter(value);
        break;
    }
    pagination.goToFirst();
  }, [pagination]);

  const handleDeviceSelect = (deviceId: number, selected: boolean) => {
    setSelectedDevices(prev => 
      selected 
        ? [...prev, deviceId]
        : prev.filter(id => id !== deviceId)
    );
  };

  const handleSelectAll = (selected: boolean) => {
    setSelectedDevices(selected ? devices.map(d => d.id) : []);
  };

  const handleDeviceClick = (device: Device) => {
    setSelectedDevice(device);
    setModalMode('view');
    setIsModalOpen(true);
  };

  const handleEditDevice = (device: Device) => {
    setSelectedDevice(device);
    setModalMode('edit');
    setIsModalOpen(true);
  };

  const handleCreateDevice = () => {
    setSelectedDevice(null);
    setModalMode('create');
    setIsModalOpen(true);
  };

  // =============================================================================
  // 🔄 라이프사이클 hooks
  // =============================================================================

  useEffect(() => {
    loadDevices();
    loadDeviceStats();
    loadAvailableProtocols();
  }, [loadDevices, loadDeviceStats, loadAvailableProtocols]);

  // 필터 변경 시 데이터 다시 로드
  useEffect(() => {
    loadDevices();
  }, [pagination.currentPage, pagination.pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      loadDevices();
      loadDeviceStats();
    }, 30000); // 30초마다

    return () => clearInterval(interval);
  }, [autoRefresh, loadDevices, loadDeviceStats]);

  // =============================================================================
  // 🎨 렌더링 헬퍼 함수들
  // =============================================================================

  const getStatusBadgeClass = (status: string) => {
    switch (status.toLowerCase()) {
      case 'running': return 'status-badge status-running';
      case 'stopped': return 'status-badge status-stopped';
      case 'error': return 'status-badge status-error';
      case 'disabled': return 'status-badge status-disabled';
      case 'restarting': return 'status-badge status-restarting';
      default: return 'status-badge status-unknown';
    }
  };

  const getConnectionBadgeClass = (connectionStatus: string) => {
    switch (connectionStatus.toLowerCase()) {
      case 'connected': return 'connection-badge connection-connected';
      case 'disconnected': return 'connection-badge connection-disconnected';
      case 'error': return 'connection-badge connection-error';
      default: return 'connection-badge connection-unknown';
    }
  };

  const formatLastSeen = (lastSeen?: string) => {
    if (!lastSeen) return '없음';
    
    const date = new Date(lastSeen);
    const now = new Date();
    const diffMs = now.getTime() - date.getTime();
    const diffMinutes = Math.floor(diffMs / (1000 * 60));
    
    if (diffMinutes < 1) return '방금 전';
    if (diffMinutes < 60) return `${diffMinutes}분 전`;
    if (diffMinutes < 1440) return `${Math.floor(diffMinutes / 60)}시간 전`;
    return date.toLocaleDateString();
  };

  // =============================================================================
  // 🎨 UI 렌더링
  // =============================================================================

  return (
    <div className="device-list-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">디바이스 관리</h1>
          <div className="page-subtitle">
            연결된 디바이스 목록을 관리하고 모니터링합니다
          </div>
        </div>
        <div className="header-right">
          <div className="header-actions">
            <button 
              className="btn btn-primary"
              onClick={handleCreateDevice}
              disabled={isProcessing}
            >
              <i className="fas fa-plus"></i>
              디바이스 추가
            </button>
            <button 
              className="btn btn-secondary"
              onClick={() => setAutoRefresh(!autoRefresh)}
            >
              <i className={`fas fa-${autoRefresh ? 'pause' : 'play'}`}></i>
              {autoRefresh ? '자동새로고침 중지' : '자동새로고침 시작'}
            </button>
          </div>
        </div>
      </div>

      {/* 통계 카드들 */}
      {deviceStats && (
        <div className="stats-grid">
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-network-wired text-primary"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">전체 디바이스</div>
              <div className="stat-value">{deviceStats.total_devices}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-check-circle text-success"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">연결됨</div>
              <div className="stat-value">{deviceStats.connected_devices}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-times-circle text-danger"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">연결 끊김</div>
              <div className="stat-value">{deviceStats.disconnected_devices}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-exclamation-triangle text-warning"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">오류</div>
              <div className="stat-value">{deviceStats.error_devices}</div>
            </div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div className="filters-section">
        <div className="search-box">
          <i className="fas fa-search"></i>
          <input
            type="text"
            placeholder="디바이스 이름, 설명, 제조사 검색..."
            value={searchTerm}
            onChange={(e) => handleSearch(e.target.value)}
          />
        </div>
        
        <div className="filter-group">
          <select
            value={statusFilter}
            onChange={(e) => handleFilterChange('status', e.target.value)}
          >
            <option value="all">모든 상태</option>
            <option value="running">실행 중</option>
            <option value="stopped">중지됨</option>
            <option value="error">오류</option>
            <option value="disabled">비활성화</option>
          </select>

          <select
            value={protocolFilter}
            onChange={(e) => handleFilterChange('protocol', e.target.value)}
          >
            <option value="all">모든 프로토콜</option>
            {availableProtocols.map(protocol => (
              <option key={protocol} value={protocol}>{protocol}</option>
            ))}
          </select>

          <select
            value={connectionFilter}
            onChange={(e) => handleFilterChange('connection', e.target.value)}
          >
            <option value="all">모든 연결상태</option>
            <option value="connected">연결됨</option>
            <option value="disconnected">연결 끊김</option>
            <option value="error">연결 오류</option>
          </select>
        </div>

        {selectedDevices.length > 0 && (
          <div className="bulk-actions">
            <span className="selected-count">
              {selectedDevices.length}개 선택됨
            </span>
            <button 
              onClick={() => handleBulkAction('enable')}
              disabled={isProcessing}
              className="btn btn-sm btn-success"
            >
              일괄 활성화
            </button>
            <button 
              onClick={() => handleBulkAction('disable')}
              disabled={isProcessing}
              className="btn btn-sm btn-warning"
            >
              일괄 비활성화
            </button>
            <button 
              onClick={() => handleBulkAction('delete')}
              disabled={isProcessing}
              className="btn btn-sm btn-danger"
            >
              일괄 삭제
            </button>
          </div>
        )}
      </div>

      {/* 에러 표시 */}
      {error && (
        <div className="error-message">
          <i className="fas fa-exclamation-circle"></i>
          {error}
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 디바이스 테이블 */}
      <div className="devices-table-container">
        {isLoading ? (
          <div className="loading-spinner">
            <i className="fas fa-spinner fa-spin"></i>
            <span>디바이스 목록을 불러오는 중...</span>
          </div>
        ) : devices.length === 0 ? (
          <div className="empty-state">
            <i className="fas fa-network-wired"></i>
            <h3>등록된 디바이스가 없습니다</h3>
            <p>새 디바이스를 추가하여 시작하세요</p>
            <button className="btn btn-primary" onClick={handleCreateDevice}>
              <i className="fas fa-plus"></i>
              첫 번째 디바이스 추가
            </button>
          </div>
        ) : (
          <table className="devices-table">
            <thead>
              <tr>
                <th>
                  <input
                    type="checkbox"
                    checked={selectedDevices.length === devices.length}
                    onChange={(e) => handleSelectAll(e.target.checked)}
                  />
                </th>
                <th>이름</th>
                <th>프로토콜</th>
                <th>엔드포인트</th>
                <th>상태</th>
                <th>연결상태</th>
                <th>최종 통신</th>
                <th>작업</th>
              </tr>
            </thead>
            <tbody>
              {devices.map((device) => (
                <tr 
                  key={device.id}
                  className={selectedDevices.includes(device.id) ? 'selected' : ''}
                >
                  <td>
                    <input
                      type="checkbox"
                      checked={selectedDevices.includes(device.id)}
                      onChange={(e) => handleDeviceSelect(device.id, e.target.checked)}
                    />
                  </td>
                  <td>
                    <div className="device-info">
                      <div 
                        className="device-name"
                        onClick={() => handleDeviceClick(device)}
                      >
                        {device.name}
                      </div>
                      {device.description && (
                        <div className="device-description">{device.description}</div>
                      )}
                    </div>
                  </td>
                  <td>
                    <span className="protocol-badge">{device.protocol_type}</span>
                  </td>
                  <td>
                    <span className="endpoint">{device.endpoint}</span>
                  </td>
                  <td>
                    <span className={getStatusBadgeClass(device.status)}>
                      {device.status}
                    </span>
                  </td>
                  <td>
                    <span className={getConnectionBadgeClass(device.connection_status)}>
                      {device.connection_status}
                    </span>
                  </td>
                  <td>
                    <span className="last-seen">
                      {formatLastSeen(device.last_seen)}
                    </span>
                  </td>
                  <td>
                    <div className="device-actions">
                      {device.is_enabled ? (
                        <button 
                          onClick={() => handleDisableDevice(device.id)}
                          disabled={isProcessing}
                          className="btn btn-sm btn-warning"
                          title="비활성화"
                        >
                          <i className="fas fa-pause"></i>
                        </button>
                      ) : (
                        <button 
                          onClick={() => handleEnableDevice(device.id)}
                          disabled={isProcessing}
                          className="btn btn-sm btn-success"
                          title="활성화"
                        >
                          <i className="fas fa-play"></i>
                        </button>
                      )}
                      <button 
                        onClick={() => handleRestartDevice(device.id)}
                        disabled={isProcessing}
                        className="btn btn-sm btn-secondary"
                        title="재시작"
                      >
                        <i className="fas fa-redo"></i>
                      </button>
                      <button 
                        onClick={() => handleTestConnection(device.id)}
                        disabled={isProcessing}
                        className="btn btn-sm btn-info"
                        title="연결 테스트"
                      >
                        <i className="fas fa-plug"></i>
                      </button>
                      <button 
                        onClick={() => handleEditDevice(device)}
                        disabled={isProcessing}
                        className="btn btn-sm btn-primary"
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
        )}
      </div>

      {/* 페이징 */}
      {devices.length > 0 && (
        <div className="pagination-section">
          <Pagination
            current={pagination.currentPage}
            total={pagination.totalCount}
            pageSize={pagination.pageSize}
            pageSizeOptions={[10, 25, 50, 100]}
            showSizeChanger={true}
            showTotal={true}
            onChange={(page, pageSize) => {
              pagination.goToPage(page);
              if (pageSize !== pagination.pageSize) {
                pagination.changePageSize(pageSize);
              }
            }}
            onShowSizeChange={(page, pageSize) => {
              pagination.changePageSize(pageSize);
              pagination.goToPage(1);
            }}
          />
        </div>
      )}

      {/* 상태 정보 */}
      <div className="status-bar">
        <div className="status-info">
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          {isProcessing && (
            <span className="processing-indicator">
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
        </div>
      </div>

      {/* 디바이스 상세/편집 모달 */}
      {isModalOpen && (
        <div className="modal-overlay" onClick={() => setIsModalOpen(false)}>
          <div className="modal-content" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h3>
                {modalMode === 'create' ? '새 디바이스 추가' : 
                 modalMode === 'edit' ? '디바이스 편집' : '디바이스 상세'}
              </h3>
              <button onClick={() => setIsModalOpen(false)}>
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div className="modal-body">
              {/* TODO: DeviceDetailModal 컴포넌트 구현 */}
              <p>디바이스 상세 정보 모달 (구현 예정)</p>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default DeviceList;