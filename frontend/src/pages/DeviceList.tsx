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

  // 🔥 공통 페이징 훅 사용
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
        return `${days}일 ${hours}시간`;
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
      
      const response = await DeviceApiService.getDevices({
        page: 1,
        limit: 1000
      });
      
      if (response.success && Array.isArray(response.data)) {
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
        
        setDevices(transformedDevices);
        setLastUpdate(new Date());
      } else {
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

  useEffect(() => {
    fetchDevices();
    
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

    if (searchTerm) {
      filtered = filtered.filter(device => 
        device.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        device.endpoint.toLowerCase().includes(searchTerm.toLowerCase()) ||
        (device.description && device.description.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    }

    if (statusFilter !== 'all') {
      filtered = filtered.filter(device => device.status === statusFilter);
    }

    if (protocolFilter !== 'all') {
      filtered = filtered.filter(device => device.protocol_type === protocolFilter);
    }

    if (connectionFilter !== 'all') {
      filtered = filtered.filter(device => device.connection_status === connectionFilter);
    }

    setFilteredDevices(filtered);
  }, [devices, searchTerm, statusFilter, protocolFilter, connectionFilter]);

  // 🔥 디바이스 액션 처리
  const handleDeviceAction = async (device: Device, action: string) => {
    setIsProcessing(true);
    try {
      switch (action) {
        case 'start':
        case 'pause':
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
      
      await fetchDevices();
    } catch (err) {
      console.error(`❌ 디바이스 ${action} 오류:`, err);
      setError(`디바이스 ${action} 중 오류가 발생했습니다.`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 🔥 일괄 액션 처리
  const handleBulkAction = async (action: string) => {
    if (selectedDevices.length === 0) return;
    
    setIsProcessing(true);
    try {
      const promises = selectedDevices.map(deviceId => {
        switch (action) {
          case 'start':
            return DeviceApiService.enableDevice(deviceId);
          case 'stop':
            return DeviceApiService.disableDevice(deviceId);
          case 'restart':
            return DeviceApiService.restartDevice(deviceId);
          default:
            return Promise.resolve();
        }
      });
      
      await Promise.all(promises);
      setSelectedDevices([]);
      await fetchDevices();
    } catch (err) {
      console.error(`❌ 일괄 ${action} 오류:`, err);
      setError(`일괄 ${action} 중 오류가 발생했습니다.`);
    } finally {
      setIsProcessing(false);
    }
  };

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
    if (selectedDevices.length === paginatedDevices.length && paginatedDevices.length > 0) {
      setSelectedDevices([]);
    } else {
      setSelectedDevices(paginatedDevices.map(device => device.id));
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

  const protocols = devices && devices.length > 0 
    ? [...new Set(devices.map(device => device.protocol_type))]
    : [];

  const paginatedDevices = filteredDevices.slice(
    (pagination.currentPage - 1) * pagination.pageSize,
    pagination.currentPage * pagination.pageSize
  );

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

      {/* 통계 패널 */}
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

      {/* 필터 패널 */}
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

      {/* 🔥 간소화된 제어 패널 - 한 줄로 */}
      <div className="compact-control-panel">
        <div className="control-left">
          <div className="selected-info">
            <input
              type="checkbox"
              checked={selectedDevices.length === paginatedDevices.length && paginatedDevices.length > 0}
              onChange={handleSelectAll}
            />
            <span>{selectedDevices.length}개 선택됨</span>
          </div>
          {selectedDevices.length > 0 && (
            <div className="control-buttons">
              <button 
                className="btn btn-success btn-sm" 
                disabled={isProcessing}
                onClick={() => handleBulkAction('start')}
              >
                <i className="fas fa-play"></i> 일괄 시작
              </button>
              <button 
                className="btn btn-warning btn-sm" 
                disabled={isProcessing}
                onClick={() => handleBulkAction('stop')}
              >
                <i className="fas fa-pause"></i> 일괄 중지
              </button>
            </div>
          )}
        </div>
      </div>

      {/* 🔥 디바이스 목록 - 헤더에 자동새로고침 통합 */}
      <div className="device-list">
        <div className="device-list-header">
          <div className="device-list-title">
            <h3>디바이스 목록</h3>
            <div className="header-controls">
              <span className="device-count">{filteredDevices.length}개</span>
              <div className="auto-refresh-control">
                <label className="refresh-label">
                  <input
                    type="checkbox"
                    checked={autoRefresh}
                    onChange={(e) => setAutoRefresh(e.target.checked)}
                  />
                  <i className="fas fa-sync-alt"></i>
                  <span>30초 자동새로고침</span>
                </label>
              </div>
            </div>
          </div>
        </div>

        {/* 🚨 완전히 수정된 테이블 구조 */}
        <div className="device-table">
          {/* 헤더 */}
          <div className="device-table-header">
            <div>
              <input
                type="checkbox"
                checked={selectedDevices.length === paginatedDevices.length && paginatedDevices.length > 0}
                onChange={handleSelectAll}
              />
            </div>
            <div>디바이스 정보</div>
            <div>프로토콜</div>
            <div>상태</div>
            <div>연결 정보</div>
            <div>데이터 정보</div>
            <div>성능 정보</div>
            <div>네트워크 정보</div>
            <div>작업</div>
          </div>

          {/* 바디 */}
          <div className="device-table-body">
            {paginatedDevices.map((device) => (
              <div key={device.id} className="device-table-row">
                {/* 체크박스 */}
                <div className="device-table-cell">
                  <input
                    type="checkbox"
                    checked={selectedDevices.includes(device.id)}
                    onChange={() => handleDeviceSelect(device.id)}
                  />
                </div>
                
                {/* 🚨 디바이스 정보 - 완전히 수정된 구조 */}
                <div className="device-table-cell">
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
                      <div className="device-endpoint">
                        {device.endpoint}
                      </div>
                    </div>
                  </div>
                </div>
                
                {/* 🚨 프로토콜 */}
                <div className="device-table-cell">
                  <span className={`protocol-badge ${
                    device.protocol_type === 'MODBUS_TCP' ? 'bg-blue-100' :
                    device.protocol_type === 'MODBUS_RTU' ? 'bg-orange-100' :
                    device.protocol_type === 'MQTT' ? 'bg-green-100' :
                    device.protocol_type === 'BACNET' ? 'bg-purple-100' :
                    'bg-orange-100'
                  }`}>
                    {device.protocol_type}
                  </span>
                </div>
                
                {/* 🚨 상태 */}
                <div className="device-table-cell">
                  <span className={`status status-${device.connection_status || 'unknown'}`}>
                    <span className={`status-dot status-dot-${device.connection_status || 'unknown'}`}></span>
                    {device.connection_status === 'connected' ? '연결됨' : 
                    device.connection_status === 'disconnected' ? '연결 안됨' : '알 수 없음'}
                  </span>
                </div>
                
                {/* 연결 정보 */}
                <div className="device-table-cell">
                  <div>
                    <div className="info-title">
                      {device.site_name || 'Unknown Site'}
                    </div>
                    <div className="info-subtitle">
                      마지막: {device.last_seen ? new Date(device.last_seen).toLocaleDateString() : 'N/A'}
                    </div>
                  </div>
                </div>
                
                {/* 데이터 정보 */}
                <div className="device-table-cell data-info">
                  <div>
                    <div className="info-title">
                      {device.data_points_count || 0}개 포인트
                    </div>
                    <div className="info-subtitle">
                      폴링: {device.polling_interval || 1000}ms
                    </div>
                  </div>
                </div>
                
                {/* 성능 정보 */}
                <div className="device-table-cell performance-info">
                  <div>
                    <div className="info-title">
                      응답: {device.response_time || 0}ms
                    </div>
                    <div className="info-subtitle">
                      오류: {device.error_count || 0}회
                    </div>
                  </div>
                </div>
                
                {/* 네트워크 정보 */}
                <div className="device-table-cell network-info">
                  <div>
                    <div className="info-title">
                      가동시간
                    </div>
                    <div className="info-subtitle">
                      {device.uptime || 'N/A'}
                    </div>
                  </div>
                </div>
                
                {/* 🔥 수정된 작업 버튼 */}
                <div className="device-table-cell">
                  <div className="device-actions">
                    {/* 시작/일시정지 버튼 (상태에 따라 토글) */}
                    {device.connection_status === 'connected' ? (
                      <button 
                        className="action-btn btn-pause"
                        onClick={() => handleDeviceAction(device, 'pause')}
                        disabled={isProcessing}
                        title="일시정지"
                      >
                        <i className="fas fa-pause"></i>
                      </button>
                    ) : (
                      <button 
                        className="action-btn btn-start"
                        onClick={() => handleDeviceAction(device, 'start')}
                        disabled={isProcessing}
                        title="시작"
                      >
                        <i className="fas fa-play"></i>
                      </button>
                    )}
                    
                    {/* 정지 버튼 */}
                    <button 
                      className="action-btn btn-stop"
                      onClick={() => handleDeviceAction(device, 'stop')}
                      disabled={isProcessing}
                      title="정지"
                    >
                      <i className="fas fa-stop"></i>
                    </button>
                    
                    {/* 보기 버튼 */}
                    <button 
                      className="action-btn btn-view"
                      onClick={() => handleModalOpen(device, 'view')}
                      title="상세보기"
                    >
                      <i className="fas fa-eye"></i>
                    </button>
                    
                    {/* 편집 버튼 */}
                    <button 
                      className="action-btn btn-edit"
                      onClick={() => handleModalOpen(device, 'edit')}
                      title="편집"
                    >
                      <i className="fas fa-edit"></i>
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* 빈 상태 */}
      {filteredDevices.length === 0 && !isLoading && (
        <div className="empty-state">
          <i className="fas fa-network-wired empty-icon"></i>
          <h3 className="empty-title">디바이스를 찾을 수 없습니다</h3>
          <p className="empty-description">필터 조건을 변경하거나 새로운 디바이스를 추가해보세요.</p>
        </div>
      )}

      {/* 🔥 페이징에 className 추가 */}
      {filteredDevices.length > 0 && (
        <Pagination
          className="device-pagination"
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