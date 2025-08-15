import React, { useState, useEffect } from 'react';
import DeviceDetailModal from '../components/modals/DeviceDetailModal';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { DeviceApiService } from '../api/services/deviceApi';
import { DEVICE_LIST_PAGINATION } from '../constants/pagination';
import '../styles/base.css';
import '../styles/device-list.css';

interface Device {
  id: number;
  name: string;
  protocol_type: string;
  device_type?: string;
  endpoint: string;
  is_enabled: boolean;
  connection_status: string;
  status: string;
  last_seen?: string;
  site_name?: string;
  data_points_count?: number;
  description?: string;
  manufacturer?: string;
  model?: string;
  response_time?: number;
  error_count?: number;
  polling_interval?: number;
  created_at?: string;
  uptime?: string;
}

interface DeviceStats {
  total: number;
  running: number;
  stopped: number;
  error: number;
  connected: number;
  disconnected: number;
}

const DeviceList: React.FC = () => {
  // 🔧 기본 상태들
  const [devices, setDevices] = useState<Device[]>([]);
  const [filteredDevices, setFilteredDevices] = useState<Device[]>([]);
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

  // 🔥 공통 페이징 훅 사용 - 상수에서 설정값 가져오기
  const pagination = usePagination({
    initialPageSize: DEVICE_LIST_PAGINATION.DEFAULT_PAGE_SIZE,
    totalCount: filteredDevices.length,
    pageSizeOptions: DEVICE_LIST_PAGINATION.PAGE_SIZE_OPTIONS,
    maxVisiblePages: DEVICE_LIST_PAGINATION.MAX_VISIBLE_PAGES
  });

  // 🔧 가동시간 계산 유틸리티 함수
  const calculateUptime = (createdAt: string): string => {
    try {
      const created = new Date(createdAt);
      const now = new Date();
      const diffMs = now.getTime() - created.getTime();
      
      const days = Math.floor(diffMs / (1000 * 60 * 60 * 24));
      const hours = Math.floor((diffMs % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60));
      const minutes = Math.floor((diffMs % (1000 * 60 * 60)) / (1000 * 60));
      
      if (days > 0) {
        return `${days}일 ${hours}시간 ${minutes}분`;
      } else if (hours > 0) {
        return `${hours}시간 ${minutes}분`;
      } else {
        return `${minutes}분`;
      }
    } catch (error) {
      return '알 수 없음';
    }
  };

  // 🔥 API 호출 - DeviceApiService 사용
  const fetchDevices = async () => {
    try {
      setIsLoading(true);
      setError(null);
      
      console.log('🔍 디바이스 목록 조회 시작...');
      
      // 🔥 공통 API 서비스 사용
      const response = await DeviceApiService.getDevices({
        page: 1,
        limit: 1000 // 일단 전체 조회 후 프론트에서 페이징
      });
      
      console.log('🔍 API 응답:', response);
      
      if (response.success && Array.isArray(response.data)) {
        // 백엔드 필드명을 프론트엔드 필드명으로 매핑
        const transformedDevices = response.data.map(device => ({
          id: device.id,
          name: device.name,
          protocol_type: device.protocol_type,
          device_type: device.device_type,
          endpoint: device.endpoint,
          is_enabled: device.is_enabled,
          connection_status: device.connection_status || 'unknown',
          status: device.status?.status || device.connection_status || 'unknown',
          last_seen: device.last_communication || device.created_at,
          site_name: device.site_name,
          data_points_count: device.data_point_count || 0,
          description: device.description,
          manufacturer: device.manufacturer,
          model: device.model,
          response_time: device.response_time || 0,
          error_count: device.error_count || 0,
          polling_interval: device.polling_interval_ms || device.settings?.polling_interval_ms || 1000,
          created_at: device.created_at,
          uptime: calculateUptime(device.created_at)
        }));
        
        console.log('✅ 변환된 디바이스 데이터:', transformedDevices);
        setDevices(transformedDevices);
        setLastUpdate(new Date());
      } else {
        console.warn('⚠️ 예상과 다른 API 응답:', response);
        setDevices([]);
        setError('올바른 형식의 데이터를 받지 못했습니다.');
      }
      
    } catch (err) {
      console.error('❌ 디바이스 목록 조회 실패:', err);
      const errorMessage = err instanceof Error ? err.message : '알 수 없는 오류가 발생했습니다.';
      setError(errorMessage);
      setDevices([]);
    } finally {
      setIsLoading(false);
    }
  };

  // 컴포넌트 마운트 시 디바이스 목록 조회
  useEffect(() => {
    fetchDevices();
    
    // 자동 새로고침 설정 (30초마다)
    const interval = setInterval(() => {
      if (autoRefresh && !isModalOpen) {
        fetchDevices();
      }
    }, 30000);

    return () => clearInterval(interval);
  }, [autoRefresh, isModalOpen]);

  // 필터링 로직
  useEffect(() => {
    let filtered = [...devices];

    // 검색어 필터
    if (searchTerm) {
      filtered = filtered.filter(device => 
        device.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        device.endpoint.toLowerCase().includes(searchTerm.toLowerCase()) ||
        (device.description && device.description.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    }

    // 상태 필터
    if (statusFilter !== 'all') {
      filtered = filtered.filter(device => device.status === statusFilter);
    }

    // 프로토콜 필터
    if (protocolFilter !== 'all') {
      filtered = filtered.filter(device => device.protocol_type === protocolFilter);
    }

    // 연결 상태 필터
    if (connectionFilter !== 'all') {
      filtered = filtered.filter(device => device.connection_status === connectionFilter);
    }

    setFilteredDevices(filtered);
  }, [devices, searchTerm, statusFilter, protocolFilter, connectionFilter]);

  // 🔥 디바이스 액션 처리 - API 서비스 사용
  const handleDeviceAction = async (device: Device, action: string) => {
    setIsProcessing(true);
    try {
      console.log(`🔧 디바이스 ${action} 실행:`, device.name);
      
      switch (action) {
        case 'start':
          await DeviceApiService.enableDevice(device.id);
          break;
        case 'stop':
          await DeviceApiService.disableDevice(device.id);
          break;
        case 'restart':
          await DeviceApiService.restartDevice(device.id);
          break;
        case 'test':
          await DeviceApiService.testConnection(device.id);
          break;
        default:
          throw new Error(`알 수 없는 액션: ${action}`);
      }
      
      console.log(`✅ 디바이스 ${action} 성공`);
      await fetchDevices(); // 상태 새로고침
    } catch (err) {
      console.error(`❌ 디바이스 ${action} 오류:`, err);
      setError(`디바이스 ${action} 중 오류가 발생했습니다.`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 모달 관련 핸들러
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

  // 🔧 안전한 통계 계산
  const stats: DeviceStats = {
    total: devices?.length || 0,
    running: devices?.filter(d => d.status === 'running')?.length || 0,
    stopped: devices?.filter(d => d.status === 'stopped')?.length || 0,
    error: devices?.filter(d => d.status === 'error')?.length || 0,
    connected: devices?.filter(d => d.connection_status === 'connected')?.length || 0,
    disconnected: devices?.filter(d => d.connection_status === 'disconnected')?.length || 0
  };

  // 고유 프로토콜 목록
  const protocols = devices && devices.length > 0 
    ? [...new Set(devices.map(device => device.protocol_type))]
    : [];

  // 🔥 페이징된 디바이스 목록
  const paginatedDevices = filteredDevices.slice(
    (pagination.currentPage - 1) * pagination.pageSize,
    pagination.currentPage * pagination.pageSize
  );

  // 로딩 상태
  if (isLoading && devices.length === 0) {
    return (
      <div className="loading-container">
        <div className="loading-spinner"></div>
        <p>디바이스 목록을 불러오는 중...</p>
      </div>
    );
  }

  return (
    <div className="device-management-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-network-wired"></i>
            디바이스 관리
          </h1>
          <p className="subtitle">마지막 업데이트: {lastUpdate.toLocaleTimeString()}</p>
        </div>
        <div className="page-actions">
          <button 
            className="btn btn-primary"
            onClick={() => handleModalOpen(null, 'create')}
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

      {/* 🔥 통계 패널 - 올바른 CSS 클래스 사용 */}
      <div className="stats-panel">
        <div className="stat-card status-running">
          <div className="stat-value">{stats.total}</div>
          <div className="stat-label">총 디바이스</div>
        </div>
        <div className="stat-card status-running">
          <div className="stat-value">{stats.connected}</div>
          <div className="stat-label">연결됨</div>
        </div>
        <div className="stat-card status-paused">
          <div className="stat-value">{stats.running}</div>
          <div className="stat-label">실행 중</div>
        </div>
        <div className="stat-card status-error">
          <div className="stat-value">{stats.error}</div>
          <div className="stat-label">오류</div>
        </div>
      </div>

      {/* 🔥 필터 패널 - device-list.css 스타일 사용 */}
      <div className="filter-panel">
        <div className="filter-row">
          <div className="filter-group flex-1">
            <label>검색</label>
            <div className="search-input-container">
              <i className="fas fa-search search-icon"></i>
              <input
                type="text"
                className="search-input"
                placeholder="디바이스 이름, 엔드포인트, 설명 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
              />
            </div>
          </div>
          <div className="filter-group">
            <label>상태</label>
            <select 
              className="filter-select" 
              value={statusFilter} 
              onChange={(e) => setStatusFilter(e.target.value)}
            >
              <option value="all">모든 상태</option>
              <option value="running">실행 중</option>
              <option value="stopped">중지됨</option>
              <option value="error">오류</option>
            </select>
          </div>
          <div className="filter-group">
            <label>프로토콜</label>
            <select 
              className="filter-select" 
              value={protocolFilter} 
              onChange={(e) => setProtocolFilter(e.target.value)}
            >
              <option value="all">모든 프로토콜</option>
              {protocols.map(protocol => (
                <option key={protocol} value={protocol}>{protocol}</option>
              ))}
            </select>
          </div>
          <div className="filter-group">
            <label>연결상태</label>
            <select 
              className="filter-select" 
              value={connectionFilter} 
              onChange={(e) => setConnectionFilter(e.target.value)}
            >
              <option value="all">모든 연결상태</option>
              <option value="connected">연결됨</option>
              <option value="disconnected">연결 안됨</option>
            </select>
          </div>
        </div>
      </div>

      {/* 🔥 공통 제어 패널 - base.css 스타일 사용 */}
      <div className="control-panel">
        <div className="control-section">
          <div className="selected-info">
            <input
              type="checkbox"
              checked={selectedDevices.length === paginatedDevices.length && paginatedDevices.length > 0}
              onChange={handleSelectAll}
            />
            <span>{selectedDevices.length}개 선택됨</span>
          </div>
          <div className="control-buttons">
            {selectedDevices.length > 0 && (
              <>
                <button className="btn btn-success btn-sm" disabled={isProcessing}>
                  <i className="fas fa-play"></i> 일괄 시작
                </button>
                <button className="btn btn-warning btn-sm" disabled={isProcessing}>
                  <i className="fas fa-pause"></i> 일괄 중지
                </button>
              </>
            )}
          </div>
        </div>
        <div className="control-section">
          <div className="section-title">자동 새로고침</div>
          <label className="flex items-center gap-2">
            <input
              type="checkbox"
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
            />
            <span className="text-sm">30초마다 자동 업데이트</span>
          </label>
        </div>
      </div>

      {/* 🔥 디바이스 목록 - device-list.css 스타일 사용 */}
      <div className="device-list">
        <div className="device-list-header">
          <div className="device-list-title">
            <h3>디바이스 목록</h3>
            <span className="device-count">{filteredDevices.length}개</span>
          </div>
        </div>

        <table className="device-table">
          <thead className="device-table-header">
            <tr>
              <th>
                <input
                  type="checkbox"
                  checked={selectedDevices.length === paginatedDevices.length && paginatedDevices.length > 0}
                  onChange={handleSelectAll}
                />
              </th>
              <th>디바이스 정보</th>
              <th>프로토콜</th>
              <th>상태</th>
              <th>연결 정보</th>
              <th>데이터 정보</th>
              <th>성능 정보</th>
              <th>네트워크 정보</th>
              <th>작업</th>
            </tr>
          </thead>
          <tbody>
            {paginatedDevices.map((device) => (
              <tr key={device.id} className="device-table-row">
                <td className="device-table-cell">
                  <input
                    type="checkbox"
                    checked={selectedDevices.includes(device.id)}
                    onChange={() => handleDeviceSelect(device.id)}
                  />
                </td>
                <td className="device-table-cell">
                  <div className="device-info">
                    <div className="device-icon">
                      <i className="fas fa-microchip"></i>
                    </div>
                    <div>
                      <div className="device-name">{device.name}</div>
                      <div className="device-details">
                        <span className="device-type">{device.device_type || 'DEVICE'}</span>
                        <span className="device-manufacturer">{device.manufacturer || 'Unknown'}</span>
                      </div>
                      <div style={{ fontSize: '0.75rem', color: '#6b7280', marginTop: '4px' }}>
                        {device.endpoint}
                      </div>
                    </div>
                  </div>
                </td>
                <td className="device-table-cell">
                  <span className={`protocol-badge ${
                    device.protocol_type === 'MODBUS_TCP' ? 'bg-blue-100 text-blue-800' :
                    device.protocol_type === 'MQTT' ? 'bg-green-100 text-green-800' :
                    device.protocol_type === 'BACNET' ? 'bg-purple-100 text-purple-800' :
                    'bg-orange-100 text-orange-800'
                  }`}>
                    {device.protocol_type}
                  </span>
                </td>
                <td className="device-table-cell">
                  <span className={`status status-${device.connection_status || 'unknown'}`}>
                    <span className={`status-dot status-dot-${device.connection_status || 'unknown'}`}></span>
                    {device.connection_status === 'connected' ? '연결됨' : 
                     device.connection_status === 'disconnected' ? '연결 안됨' : '알 수 없음'}
                  </span>
                </td>
                <td className="device-table-cell">
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#374151', fontWeight: '500' }}>
                      {device.site_name || 'Unknown Site'}
                    </div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                      마지막 통신: {device.last_seen ? new Date(device.last_seen).toLocaleString() : 'N/A'}
                    </div>
                  </div>
                </td>
                <td className="device-table-cell data-info">
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#374151', fontWeight: '500' }}>
                      {device.data_points_count || 0}개 포인트
                    </div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                      폴링: {device.polling_interval || 1000}ms
                    </div>
                  </div>
                </td>
                <td className="device-table-cell performance-info">
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#374151', fontWeight: '500' }}>
                      응답: {device.response_time || 0}ms
                    </div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                      오류: {device.error_count || 0}회
                    </div>
                  </div>
                </td>
                <td className="device-table-cell network-info">
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#374151', fontWeight: '500' }}>
                      가동시간
                    </div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                      {device.uptime || 'N/A'}
                    </div>
                  </div>
                </td>
                <td className="device-table-cell">
                  <div style={{ display: 'flex', gap: '0.25rem' }}>
                    <button 
                      className="btn btn-sm btn-outline"
                      onClick={() => handleModalOpen(device, 'view')}
                      title="보기"
                    >
                      <i className="fas fa-eye"></i>
                    </button>
                    <button 
                      className="btn btn-sm btn-outline"
                      onClick={() => handleModalOpen(device, 'edit')}
                      title="편집"
                    >
                      <i className="fas fa-edit"></i>
                    </button>
                    {device.status === 'running' ? (
                      <button 
                        className="btn btn-sm btn-warning"
                        onClick={() => handleDeviceAction(device, 'stop')}
                        disabled={isProcessing}
                        title="정지"
                      >
                        <i className="fas fa-stop"></i>
                      </button>
                    ) : (
                      <button 
                        className="btn btn-sm btn-success"
                        onClick={() => handleDeviceAction(device, 'start')}
                        disabled={isProcessing}
                        title="시작"
                      >
                        <i className="fas fa-play"></i>
                      </button>
                    )}
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* 빈 상태 */}
      {filteredDevices.length === 0 && !isLoading && (
        <div className="empty-state">
          <i className="fas fa-network-wired empty-icon"></i>
          <h3 className="empty-title">디바이스를 찾을 수 없습니다</h3>
          <p className="empty-description">필터 조건을 변경하거나 새로운 디바이스를 추가해보세요.</p>
        </div>
      )}

      {/* 🔥 공통 페이지네이션 컴포넌트 사용 - 상수 활용 */}
      {filteredDevices.length > 0 && (
        <Pagination
          current={pagination.currentPage}
          total={filteredDevices.length}
          pageSize={pagination.pageSize}
          pageSizeOptions={DEVICE_LIST_PAGINATION.PAGE_SIZE_OPTIONS}
          showSizeChanger={true}
          showQuickJumper={true}
          showTotal={true}
          onChange={pagination.goToPage}
          onShowSizeChange={pagination.changePageSize}
        />
      )}

      {/* DeviceDetailModal */}
      <DeviceDetailModal
        device={selectedDevice}
        mode={modalMode}
        isOpen={isModalOpen}
        onClose={handleModalClose}
        onSave={fetchDevices}
      />
    </div>
  );
};

export default DeviceList;