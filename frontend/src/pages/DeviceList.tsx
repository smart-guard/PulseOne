import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Pagination } from '../components/common/Pagination';
import { DeviceApiService, Device, DeviceStats } from '../api/services/deviceApi';
import DeviceDetailModal from '../components/modals/DeviceDetailModal';
import '../styles/device-list.css';

const DeviceList: React.FC = () => {
  console.log('💡 DeviceList 컴포넌트 렌더링 시작');
  
  // 기본 상태들
  const [devices, setDevices] = useState<Device[]>([]);
  const [deviceStats, setDeviceStats] = useState<DeviceStats | null>(null);
  const [selectedDevices, setSelectedDevices] = useState<number[]>([]);
  
  // 강제 리렌더링을 위한 키 추가
  const [renderKey, setRenderKey] = useState(0);
  
  // 화면 크기 상태 추가
  const [screenWidth, setScreenWidth] = useState(typeof window !== 'undefined' ? window.innerWidth : 1400);
  
  // 로딩 상태 분리
  const [isInitialLoading, setIsInitialLoading] = useState(true);
  const [isBackgroundRefreshing, setIsBackgroundRefreshing] = useState(false);
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

  // 페이징 관리
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(10);
  const [totalCount, setTotalCount] = useState(0);

  // 첫 로딩 완료 여부
  const [hasInitialLoad, setHasInitialLoad] = useState(false);
  
  // 자동새로고침 타이머 ref
  const autoRefreshRef = useRef<NodeJS.Timeout | null>(null);

  // 스크롤바 감지를 위한 ref
  const tableBodyRef = useRef<HTMLDivElement>(null);
  const [hasScrollbar, setHasScrollbar] = useState(false);

  // 확인 모달 상태
  const [confirmModal, setConfirmModal] = useState<{
    isOpen: boolean;
    title: string;
    message: string;
    confirmText: string;
    cancelText: string;
    onConfirm: () => void;
    onCancel: () => void;
    type: 'warning' | 'danger' | 'info';
  }>({
    isOpen: false,
    title: '',
    message: '',
    confirmText: '확인',
    cancelText: '취소',
    onConfirm: () => {},
    onCancel: () => {},
    type: 'info'
  });

  // 스크롤바 감지
  const checkScrollbar = useCallback(() => {
    const tableBody = tableBodyRef.current;
    if (tableBody) {
      const hasVerticalScrollbar = tableBody.scrollHeight > tableBody.clientHeight;
      setHasScrollbar(hasVerticalScrollbar);
    }
  }, []);

  // 커스텀 확인 모달 표시
  const showConfirmModal = (config: {
    title: string;
    message: string;
    confirmText?: string;
    cancelText?: string;
    onConfirm: () => void;
    type?: 'warning' | 'danger' | 'info';
  }) => {
    setConfirmModal({
      isOpen: true,
      title: config.title,
      message: config.message,
      confirmText: config.confirmText || '확인',
      cancelText: config.cancelText || '취소',
      onConfirm: () => {
        config.onConfirm();
        setConfirmModal(prev => ({ ...prev, isOpen: false }));
      },
      onCancel: () => {
        setConfirmModal(prev => ({ ...prev, isOpen: false }));
      },
      type: config.type || 'info'
    });
  };

  // =============================================================================
  // 데이터 로드 함수들
  // =============================================================================

  const loadDevices = useCallback(async (isBackground = false) => {
    try {
      if (!hasInitialLoad) {
        setIsInitialLoading(true);
      } else if (isBackground) {
        setIsBackgroundRefreshing(true);
      }
      
      setError(null);

      const response = await DeviceApiService.getDevices({
        page: currentPage,
        limit: pageSize,
        protocol_type: protocolFilter !== 'all' ? protocolFilter : undefined,
        connection_status: connectionFilter !== 'all' ? connectionFilter : undefined,
        status: statusFilter !== 'all' ? statusFilter : undefined,
        search: searchTerm || undefined,
        sort_by: 'name',
        sort_order: 'ASC',
        include_collector_status: true
      });

      if (response.success && response.data) {
        const newDevices = response.data.items || [];
        setDevices([...newDevices]);
        setRenderKey(prev => prev + 1);
        
        const apiTotal = response.data.pagination?.total || 0;
        setTotalCount(apiTotal);
        
        if (!hasInitialLoad) {
          setHasInitialLoad(true);
        }

        setTimeout(checkScrollbar, 100);
      } else {
        throw new Error(response.error || 'API 응답 오류');
      }

    } catch (err) {
      console.error('❌ 디바이스 목록 로드 실패:', err);
      setError(err instanceof Error ? err.message : '디바이스 목록을 불러올 수 없습니다');
      setDevices([]);
      setTotalCount(0);
    } finally {
      setIsInitialLoading(false);
      setIsBackgroundRefreshing(false);
      setLastUpdate(new Date());
    }
  }, [currentPage, pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm, hasInitialLoad, checkScrollbar]);

  const loadDeviceStats = useCallback(async () => {
    try {
      const response = await DeviceApiService.getDeviceStatistics();
      if (response.success && response.data) {
        setDeviceStats(response.data);
      }
    } catch (err) {
      console.warn('통계 로드 실패:', err);
    }
  }, []);

  const loadAvailableProtocols = useCallback(async () => {
    try {
      const response = await DeviceApiService.getAvailableProtocols();
      if (response.success && response.data) {
        const protocols = response.data.map(p => p.protocol_type);
        setAvailableProtocols(protocols);
      }
    } catch (err) {
      console.warn('프로토콜 로드 실패:', err);
    }
  }, []);

  // =============================================================================
  // 워커 제어 함수들
  // =============================================================================

  const handleStartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    showConfirmModal({
      title: '워커 시작 확인',
      message: `워커를 시작하시겠습니까?\n\n디바이스: ${deviceName}`,
      confirmText: '시작',
      cancelText: '취소',
      type: 'info',
      onConfirm: async () => {
        try {
          setIsProcessing(true);
          const response = await DeviceApiService.startDeviceWorker(deviceId);
          if (response.success) {
            alert('워커가 시작되었습니다');
            await loadDevices(true);
          } else {
            throw new Error(response.error || '워커 시작 실패');
          }
        } catch (err) {
          alert(`워커 시작 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
        } finally {
          setIsProcessing(false);
        }
      }
    });
  };

  const handleStopWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    showConfirmModal({
      title: '워커 정지 확인',
      message: `워커를 정지하시겠습니까?\n\n디바이스: ${deviceName}`,
      confirmText: '정지',
      cancelText: '취소',
      type: 'danger',
      onConfirm: async () => {
        try {
          setIsProcessing(true);
          const response = await DeviceApiService.stopDeviceWorker(deviceId, { graceful: true });
          if (response.success) {
            alert('워커가 정지되었습니다');
            await loadDevices(true);
          } else {
            throw new Error(response.error || '워커 정지 실패');
          }
        } catch (err) {
          alert(`워커 정지 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
        } finally {
          setIsProcessing(false);
        }
      }
    });
  };

  const handleRestartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    showConfirmModal({
      title: '워커 재시작 확인',
      message: `워커를 재시작하시겠습니까?\n\n디바이스: ${deviceName}`,
      confirmText: '재시작',
      cancelText: '취소',
      type: 'warning',
      onConfirm: async () => {
        try {
          setIsProcessing(true);
          const response = await DeviceApiService.restartDeviceWorker(deviceId);
          if (response.success) {
            alert('워커가 재시작되었습니다');
            await loadDevices(true);
          } else {
            throw new Error(response.error || '워커 재시작 실패');
          }
        } catch (err) {
          alert(`워커 재시작 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
        } finally {
          setIsProcessing(false);
        }
      }
    });
  };

  // =============================================================================
  // 이벤트 핸들러들
  // =============================================================================

  const handleSearch = (term: string) => {
    setSearchTerm(term);
    setCurrentPage(1);
  };

  const handleFilterChange = (filterType: string, value: string) => {
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
    setCurrentPage(1);
  };

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

  const handleCloseModal = () => {
    setIsModalOpen(false);
    setSelectedDevice(null);
    loadDevices(true);
  };

  const handlePageSizeChange = useCallback((newPageSize: number) => {
    setPageSize(newPageSize);
    setCurrentPage(1);
  }, []);

  const handlePageChange = useCallback((page: number, newPageSize?: number) => {
    if (newPageSize && newPageSize !== pageSize) {
      setPageSize(newPageSize);
      setCurrentPage(1);
    } else {
      setCurrentPage(page);
    }
  }, [pageSize]);

  // =============================================================================
  // 스타일링 함수들
  // =============================================================================

  const getProtocolBadgeClass = (protocolType: string) => {
    const protocol = protocolType?.toUpperCase() || 'UNKNOWN';
    
    switch (protocol) {
      case 'MODBUS_TCP':
      case 'MODBUS_RTU':
        return 'device-list-protocol-badge device-list-protocol-modbus';
      case 'BACNET':
        return 'device-list-protocol-badge device-list-protocol-bacnet';
      default:
        return 'device-list-protocol-badge device-list-protocol-unknown';
    }
  };

  const getProtocolDisplayName = (protocolType: string) => {
    const protocol = protocolType?.toUpperCase() || 'UNKNOWN';
    switch (protocol) {
      case 'MODBUS_TCP': return 'MODBUS TCP';
      case 'MODBUS_RTU': return 'MODBUS RTU';
      case 'BACNET': return 'BACnet/IP';
      default: return protocol || 'Unknown';
    }
  };

  const getStatusBadgeClass = (status: string) => {
    switch (status) {
      case 'connected': return 'device-list-status-badge device-list-status-connected';
      case 'disconnected': return 'device-list-status-badge device-list-status-disconnected';
      case 'error': return 'device-list-status-badge device-list-status-disconnected';
      default: return 'device-list-status-badge device-list-status-unknown';
    }
  };

  // =============================================================================
  // 라이프사이클 hooks
  // =============================================================================

  useEffect(() => {
    loadDevices();
    loadAvailableProtocols();
  }, []);

  useEffect(() => {
    if (devices.length > 0) {
      loadDeviceStats();
    }
  }, [devices.length, loadDeviceStats]);

  useEffect(() => {
    if (hasInitialLoad) {
      loadDevices(true);
    }
  }, [currentPage, pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm, hasInitialLoad]);

  useEffect(() => {
    if (!autoRefresh || !hasInitialLoad) {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
      return;
    }

    autoRefreshRef.current = setInterval(() => {
      loadDevices(true);
    }, 60000);

    return () => {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
    };
  }, [autoRefresh, hasInitialLoad, loadDevices]);

  // 창 리사이즈 시 화면 크기 업데이트
  useEffect(() => {
    const handleResize = () => {
      setScreenWidth(window.innerWidth);
      checkScrollbar();
      setRenderKey(prev => prev + 1);
    };
    
    setScreenWidth(window.innerWidth);
    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, [checkScrollbar]);

  const totalPages = Math.ceil(totalCount / pageSize) || 1;

  return (
    <div className="device-list-container" key={`container-${renderKey}`}>
      {/* 페이지 헤더 */}
      <div className="device-list-header">
        <div>
          <h1>
            <i className="fas fa-network-wired"></i>
            디바이스 관리
          </h1>
          <div className="subtitle">
            연결된 디바이스 목록을 관리하고 모니터링합니다
          </div>
        </div>
        <div className="actions">
          <button 
            className="btn-primary"
            onClick={handleCreateDevice}
            disabled={isProcessing}
          >
            <i className="fas fa-plus"></i>
            디바이스 추가
          </button>
        </div>
      </div>

      {/* 통계 카드들 */}
      {deviceStats && (
        <div className="device-list-stats">
          <div className="device-list-stat-card">
            <i className="fas fa-network-wired" style={{color: '#3b82f6'}}></i>
            <div className="value">{deviceStats.total_devices || 0}</div>
            <div className="label">전체 디바이스</div>
          </div>
          <div className="device-list-stat-card">
            <i className="fas fa-check-circle" style={{color: '#10b981'}}></i>
            <div className="value">{deviceStats.connected_devices || 0}</div>
            <div className="label">연결됨</div>
          </div>
          <div className="device-list-stat-card">
            <i className="fas fa-times-circle" style={{color: '#ef4444'}}></i>
            <div className="value">{deviceStats.disconnected_devices || 0}</div>
            <div className="label">연결 끊김</div>
          </div>
          <div className="device-list-stat-card">
            <i className="fas fa-exclamation-triangle" style={{color: '#f59e0b'}}></i>
            <div className="value">{deviceStats.error_devices || 0}</div>
            <div className="label">오류</div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div className="device-list-filters">
        <div className="device-list-search">
          <i className="fas fa-search"></i>
          <input
            type="text"
            placeholder="디바이스 이름, 설명, 제조사 검색..."
            value={searchTerm}
            onChange={(e) => handleSearch(e.target.value)}
          />
        </div>
        
        <select value={statusFilter} onChange={(e) => handleFilterChange('status', e.target.value)}>
          <option value="all">모든 상태</option>
          <option value="running">실행 중</option>
          <option value="stopped">중지됨</option>
          <option value="error">오류</option>
          <option value="disabled">비활성화</option>
        </select>

        <select value={protocolFilter} onChange={(e) => handleFilterChange('protocol', e.target.value)}>
          <option value="all">모든 프로토콜</option>
          {availableProtocols.map(protocol => (
            <option key={protocol} value={protocol}>{protocol}</option>
          ))}
        </select>

        <select value={connectionFilter} onChange={(e) => handleFilterChange('connection', e.target.value)}>
          <option value="all">모든 연결상태</option>
          <option value="connected">연결됨</option>
          <option value="disconnected">연결 끊김</option>
          <option value="error">연결 오류</option>
        </select>
      </div>

      {/* 에러 표시 */}
      {error && (
        <div className="device-list-error">
          <i className="fas fa-exclamation-circle"></i>
          {error}
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 디바이스 테이블 */}
      <div className="device-list-table-container">
        {isInitialLoading ? (
          <div className="device-list-loading">
            <i className="fas fa-spinner fa-spin"></i>
            <span>디바이스 목록을 불러오는 중...</span>
          </div>
        ) : devices.length === 0 ? (
          <div className="device-list-empty">
            <i className="fas fa-network-wired"></i>
            <h3>등록된 디바이스가 없습니다</h3>
            <p>새 디바이스를 추가하여 시작하세요</p>
            <button onClick={handleCreateDevice}>
              <i className="fas fa-plus"></i>
              첫 번째 디바이스 추가
            </button>
          </div>
        ) : (
          <div key={`table-${renderKey}`}>
            {/* 헤더 */}
            <div className="device-list-table-header">
              <div>
                <input
                  type="checkbox"
                  checked={selectedDevices.length === devices.length && devices.length > 0}
                  onChange={(e) => handleSelectAll(e.target.checked)}
                />
              </div>
              <div>디바이스</div>
              <div>프로토콜</div>
              <div>상태</div>
              <div>연결</div>
              <div>데이터</div>
              <div>성능</div>
              <div>작업</div>
            </div>

            {/* 바디 */}
            <div className="device-list-table-body" ref={tableBodyRef} key={`tbody-${renderKey}`}>
              {devices.map((device, index) => (
                <div key={`device-${device.id}-${renderKey}-${currentPage}-${index}`} className="device-list-table-row">
                  {/* 체크박스 */}
                  <div className="device-list-checkbox-cell">
                    <input
                      type="checkbox"
                      checked={selectedDevices.includes(device.id)}
                      onChange={(e) => handleDeviceSelect(device.id, e.target.checked)}
                    />
                  </div>

                  {/* 디바이스 정보 */}
                  <div className="device-list-device-cell">
                    <div className="device-list-device-info" onClick={() => handleDeviceClick(device)}>
                      <div className="device-list-device-icon">
                        <i className="fas fa-microchip"></i>
                      </div>
                      <div className="device-list-device-details">
                        <div className="device-list-device-name">
                          {device.name}
                        </div>
                        {device.manufacturer && (
                          <div className="device-list-device-meta">
                            {device.manufacturer} {device.model}
                          </div>
                        )}
                        {device.description && (
                          <div className="device-list-device-meta">
                            {device.description}
                          </div>
                        )}
                        <div className="device-list-device-endpoint">
                          {device.endpoint}
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* 프로토콜 */}
                  <div className="device-list-protocol-cell">
                    <span className={getProtocolBadgeClass(device.protocol_type)}>
                      {getProtocolDisplayName(device.protocol_type)}
                    </span>
                  </div>

                  {/* 상태 */}
                  <div className="device-list-status-cell">
                    <span className={getStatusBadgeClass(device.connection_status)}>
                      {device.connection_status === 'connected' ? '연결' : device.connection_status === 'disconnected' ? '끊김' : '알수없음'}
                    </span>
                  </div>

                  {/* 연결 */}
                  <div className="device-list-info-cell">
                    <div className="device-list-info-title">
                      {device.connection_status === 'connected' ? '정상' : device.connection_status === 'disconnected' ? '끊김' : '알수없음'}
                    </div>
                    <div className="device-list-info-subtitle">
                      {device.last_seen ? new Date(device.last_seen).getMonth() + 1 + '/' + new Date(device.last_seen).getDate() : '없음'}
                    </div>
                  </div>

                  {/* 데이터 */}
                  <div className="device-list-info-cell">
                    <div className="device-list-info-title">{device.data_point_count || 0}개</div>
                    <div className="device-list-info-subtitle">활성: {device.enabled_point_count || 0}개</div>
                  </div>

                  {/* 성능 */}
                  <div className="device-list-info-cell">
                    <div className="device-list-info-title">{device.response_time || 0}ms</div>
                    <div className="device-list-info-subtitle">98% OK</div>
                  </div>

                  {/* 작업 버튼들 */}
                  <div className="device-list-actions-cell">
                    <div className="device-list-actions">
                      <button 
                        onClick={() => handleEditDevice(device)} 
                        disabled={isProcessing} 
                        className="device-list-action-btn device-list-btn-edit"
                        title="편집"
                      >
                        <i className="fas fa-edit"></i>
                      </button>
                      <button 
                        onClick={() => handleStartWorker(device.id)} 
                        disabled={isProcessing} 
                        className="device-list-action-btn device-list-btn-start"
                        title="워커 시작"
                      >
                        <i className="fas fa-play"></i>
                      </button>
                      <button 
                        onClick={() => handleStopWorker(device.id)} 
                        disabled={isProcessing} 
                        className="device-list-action-btn device-list-btn-stop"
                        title="워커 정지"
                      >
                        <i className="fas fa-stop"></i>
                      </button>
                      <button 
                        onClick={() => handleRestartWorker(device.id)} 
                        disabled={isProcessing} 
                        className="device-list-action-btn device-list-btn-restart"
                        title="워커 재시작"
                      >
                        <i className="fas fa-redo"></i>
                      </button>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* 페이징 */}
      <div className="device-list-pagination">
        <div className="device-list-pagination-info">
          {totalCount > 0 ? (
            <span>{Math.min(((currentPage - 1) * pageSize) + 1, totalCount)}-{Math.min(currentPage * pageSize, totalCount)} / {totalCount}개</span>
          ) : (
            <span>데이터 없음</span>
          )}
        </div>

        <div className="device-list-pagination-controls">
          <button onClick={() => handlePageChange(1)} disabled={currentPage === 1} className="device-list-pagination-btn">««</button>
          <button onClick={() => handlePageChange(Math.max(1, currentPage - 1))} disabled={currentPage <= 1} className="device-list-pagination-btn">‹</button>

          {(() => {
            const maxVisible = 5;
            let pages: number[] = [];
            if (totalPages <= maxVisible) {
              pages = Array.from({ length: totalPages }, (_, i) => i + 1);
            } else {
              const half = Math.floor(maxVisible / 2);
              let start = Math.max(currentPage - half, 1);
              let end = Math.min(start + maxVisible - 1, totalPages);
              if (end - start + 1 < maxVisible) {
                start = Math.max(end - maxVisible + 1, 1);
              }
              pages = Array.from({ length: end - start + 1 }, (_, i) => start + i);
            }
            return pages.map(page => (
              <button 
                key={page} 
                onClick={() => handlePageChange(page)} 
                className={`device-list-pagination-btn ${page === currentPage ? 'active' : ''}`}
              >
                {page}
              </button>
            ));
          })()}

          <button onClick={() => handlePageChange(Math.min(totalPages, currentPage + 1))} disabled={currentPage >= totalPages} className="device-list-pagination-btn">›</button>
          <button onClick={() => handlePageChange(totalPages)} disabled={currentPage === totalPages} className="device-list-pagination-btn">»»</button>
        </div>

        <div className="device-list-pagination-size">
          <select value={pageSize} onChange={(e) => handlePageSizeChange(Number(e.target.value))} className="device-list-pagination-select">
            <option value="10">10개씩</option>
            <option value="25">25개씩</option>
            <option value="50">50개씩</option>
            <option value="100">100개씩</option>
          </select>
        </div>
      </div>

      {/* 상태바 */}
      <div className="device-list-status-bar">
        <div className="device-list-status-bar-left">
          <div className="device-list-last-update">
            <span>마지막 업데이트:</span>
            <span className="device-list-update-time">
              {lastUpdate.toLocaleTimeString('ko-KR', { hour12: true, hour: '2-digit', minute: '2-digit', second: '2-digit' })}
            </span>
          </div>
          {isBackgroundRefreshing && (
            <div className="device-list-background-refresh">
              <i className="fas fa-sync-alt fa-spin"></i>
              <span>백그라운드 업데이트 중...</span>
            </div>
          )}
        </div>
        <div className="device-list-status-bar-right">
          {isProcessing && (
            <span className="device-list-processing">
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
          <button 
            className="device-list-refresh-btn"
            onClick={() => loadDevices(true)} 
            disabled={isProcessing || isBackgroundRefreshing}
          >
            <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 확인 모달 */}
      {confirmModal.isOpen && (
        <div className="device-list-modal-overlay">
          <div className="device-list-modal-content">
            <div className="device-list-modal-header">
              <div className={`device-list-modal-icon ${confirmModal.type}`}>
                <i className={`fas ${confirmModal.type === 'danger' ? 'fa-exclamation-triangle' : confirmModal.type === 'warning' ? 'fa-exclamation-circle' : 'fa-info-circle'}`}></i>
              </div>
              <h3>{confirmModal.title}</h3>
            </div>
            <div className="device-list-modal-body">{confirmModal.message}</div>
            <div className="device-list-modal-footer">
              <button onClick={confirmModal.onCancel} className="device-list-modal-btn device-list-modal-btn-cancel">{confirmModal.cancelText}</button>
              <button onClick={confirmModal.onConfirm} className={`device-list-modal-btn device-list-modal-btn-confirm device-list-modal-btn-${confirmModal.type}`}>{confirmModal.confirmText}</button>
            </div>
          </div>
        </div>
      )}

      {/* 모달 */}
      <DeviceDetailModal device={selectedDevice} isOpen={isModalOpen} mode={modalMode} onClose={handleCloseModal} onSave={async () => {}} onDelete={async () => {}} />
    </div>
  );
};

export default DeviceList;