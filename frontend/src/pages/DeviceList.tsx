import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Pagination } from '../components/common/Pagination';
import { DeviceApiService, Device, DeviceStats } from '../api/services/deviceApi';
import DeviceDetailModal from '../components/modals/DeviceDetailModal';

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

  // VirtualPoints 방식: 직접 state로 페이징 관리
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

  // 반응형 Grid 컬럼 정의 - 전체 폭 활용하면서 버튼 보호
  const getGridColumns = () => {
    if (screenWidth < 1400) {
      // 작은 화면: 작업 열만 고정, 디바이스 열이 남은 공간 차지
      return '40px 1fr 80px 60px 80px 90px 80px 140px';
    } else {
      // 큰 화면: 작업 열을 더 넓게, 디바이스 열이 남은 공간 차지
      return '50px 1fr 100px 80px 100px 120px 100px 160px';
    }
  };

  const getGap = () => {
    return screenWidth < 1400 ? '4px' : '8px';
  };

  const getPadding = () => {
    return screenWidth < 1400 ? '8px 8px' : '12px 16px';
  };

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

  const getProtocolBadgeStyle = (protocolType: string) => {
    const protocol = protocolType?.toUpperCase() || 'UNKNOWN';
    
    switch (protocol) {
      case 'MODBUS_TCP':
      case 'MODBUS_RTU':
        return {
          background: '#dbeafe',
          color: '#1e40af',
          border: '1px solid #93c5fd',
          padding: '4px 8px',
          borderRadius: '4px',
          fontSize: '12px',
          fontWeight: '600'
        };
      case 'BACNET':
        return {
          background: '#dcfce7',
          color: '#166534',
          border: '1px solid #86efac',
          padding: '4px 8px',
          borderRadius: '4px',
          fontSize: '12px',
          fontWeight: '600'
        };
      default:
        return {
          background: '#f1f5f9',
          color: '#475569',
          border: '1px solid #cbd5e1',
          padding: '4px 8px',
          borderRadius: '4px',
          fontSize: '12px',
          fontWeight: '600'
        };
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

  // =============================================================================
  // 스타일 상수들
  // =============================================================================

  const SCROLLBAR_WIDTH = 15;

  const containerStyle = {
    width: '100%',
    background: '#f8fafc',
    minHeight: '100vh',
    padding: '0',
    margin: '0'
  };

  const headerStyle = {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: '24px',
    background: '#ffffff',
    borderBottom: '1px solid #e5e7eb',
    marginBottom: '24px'
  };

  const statsGridStyle = {
    display: 'grid',
    gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))',
    gap: '16px',
    marginBottom: '32px',
    padding: '0 24px'
  };

  const tableContainerStyle = {
    background: '#ffffff',
    border: '1px solid #e5e7eb',
    borderRadius: '12px',
    overflow: 'hidden',
    margin: '0 24px',
    boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
  };

  const tableHeaderStyle = {
    display: 'grid',
    gridTemplateColumns: getGridColumns(),
    gap: getGap(),
    padding: getPadding(),
    paddingRight: hasScrollbar ? 
      (screenWidth < 1400 ? `${8 + SCROLLBAR_WIDTH}px` : `${16 + SCROLLBAR_WIDTH}px`) : 
      (screenWidth < 1400 ? '8px' : '16px'),
    background: '#f8fafc',
    borderBottom: '2px solid #e5e7eb',
    fontSize: screenWidth < 1400 ? '11px' : '12px',
    fontWeight: '700',
    color: '#374151',
    textTransform: 'uppercase' as const,
    letterSpacing: '0.025em',
    alignItems: 'center'
  };

  const tableBodyStyle = {
    maxHeight: '65vh',
    overflowY: 'auto' as const,
    overflowX: 'hidden' as const
  };

  const getRowStyle = (index: number) => ({
    display: 'grid',
    gridTemplateColumns: getGridColumns(),
    gap: getGap(),
    padding: getPadding(),
    borderBottom: '1px solid #f1f5f9',
    alignItems: 'center',
    backgroundColor: index % 2 === 0 ? '#ffffff' : '#fafafa',
    transition: 'background-color 0.15s ease',
    cursor: 'pointer'
  });

  const actionButtonStyle = {
    width: screenWidth < 1400 ? '24px' : '28px',
    height: screenWidth < 1400 ? '24px' : '28px',
    border: 'none',
    borderRadius: '4px',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    cursor: 'pointer',
    fontSize: screenWidth < 1400 ? '9px' : '11px',
    margin: '0 1px',
    transition: 'all 0.15s ease'
  };

  const editButtonStyle = { ...actionButtonStyle, background: '#8b5cf6', color: 'white' };
  const startButtonStyle = { ...actionButtonStyle, background: '#10b981', color: 'white' };
  const stopButtonStyle = { ...actionButtonStyle, background: '#ef4444', color: 'white' };
  const restartButtonStyle = { ...actionButtonStyle, background: '#f59e0b', color: 'white' };

  const totalPages = Math.ceil(totalCount / pageSize) || 1;

  return (
    <div style={containerStyle} key={`container-${renderKey}`}>
      {/* 페이지 헤더 */}
      <div style={headerStyle}>
        <div>
          <h1 style={{fontSize: '28px', fontWeight: '700', color: '#111827', margin: '0', display: 'flex', alignItems: 'center', gap: '12px'}}>
            <i className="fas fa-network-wired" style={{color: '#3b82f6'}}></i>
            디바이스 관리
          </h1>
          <div style={{fontSize: '16px', color: '#6b7280', margin: '8px 0 0 0'}}>
            연결된 디바이스 목록을 관리하고 모니터링합니다
          </div>
        </div>
        <div style={{display: 'flex', gap: '12px'}}>
          <button 
            style={{
              padding: '12px 20px',
              background: '#3b82f6',
              color: 'white',
              border: 'none',
              borderRadius: '8px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '14px',
              fontWeight: '500',
              transition: 'background-color 0.15s ease'
            }}
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
        <div style={statsGridStyle}>
          <div style={{background: '#ffffff', border: '1px solid #e5e7eb', borderRadius: '12px', padding: '24px', textAlign: 'center', boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'}}>
            <i className="fas fa-network-wired" style={{fontSize: '32px', color: '#3b82f6', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.total_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>전체 디바이스</div>
          </div>
          <div style={{background: '#ffffff', border: '1px solid #e5e7eb', borderRadius: '12px', padding: '24px', textAlign: 'center', boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'}}>
            <i className="fas fa-check-circle" style={{fontSize: '32px', color: '#10b981', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.connected_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>연결됨</div>
          </div>
          <div style={{background: '#ffffff', border: '1px solid #e5e7eb', borderRadius: '12px', padding: '24px', textAlign: 'center', boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'}}>
            <i className="fas fa-times-circle" style={{fontSize: '32px', color: '#ef4444', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.disconnected_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>연결 끊김</div>
          </div>
          <div style={{background: '#ffffff', border: '1px solid #e5e7eb', borderRadius: '12px', padding: '24px', textAlign: 'center', boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'}}>
            <i className="fas fa-exclamation-triangle" style={{fontSize: '32px', color: '#f59e0b', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.error_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>오류</div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div style={{background: '#ffffff', border: '1px solid #e5e7eb', borderRadius: '12px', padding: '20px 24px', margin: '0 24px 24px', display: 'flex', gap: '12px', alignItems: 'center', flexWrap: 'wrap'}}>
        <div style={{position: 'relative', flex: '1', minWidth: '200px'}}>
          <i className="fas fa-search" style={{position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)', color: '#9ca3af'}}></i>
          <input
            type="text"
            placeholder="디바이스 이름, 설명, 제조사 검색..."
            value={searchTerm}
            onChange={(e) => handleSearch(e.target.value)}
            style={{width: '100%', padding: '12px 16px 12px 40px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px'}}
          />
        </div>
        
        <select value={statusFilter} onChange={(e) => handleFilterChange('status', e.target.value)} style={{padding: '12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px', background: '#ffffff', minWidth: '120px'}}>
          <option value="all">모든 상태</option>
          <option value="running">실행 중</option>
          <option value="stopped">중지됨</option>
          <option value="error">오류</option>
          <option value="disabled">비활성화</option>
        </select>

        <select value={protocolFilter} onChange={(e) => handleFilterChange('protocol', e.target.value)} style={{padding: '12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px', background: '#ffffff', minWidth: '120px'}}>
          <option value="all">모든 프로토콜</option>
          {availableProtocols.map(protocol => (
            <option key={protocol} value={protocol}>{protocol}</option>
          ))}
        </select>

        <select value={connectionFilter} onChange={(e) => handleFilterChange('connection', e.target.value)} style={{padding: '12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px', background: '#ffffff', minWidth: '120px'}}>
          <option value="all">모든 연결상태</option>
          <option value="connected">연결됨</option>
          <option value="disconnected">연결 끊김</option>
          <option value="error">연결 오류</option>
        </select>
      </div>

      {/* 에러 표시 */}
      {error && (
        <div style={{display: 'flex', alignItems: 'center', gap: '12px', padding: '12px 16px', background: '#fef2f2', border: '1px solid #fecaca', borderRadius: '8px', color: '#dc2626', margin: '0 24px 16px'}}>
          <i className="fas fa-exclamation-circle"></i>
          {error}
          <button onClick={() => setError(null)} style={{marginLeft: 'auto', background: 'none', border: 'none', color: '#dc2626', cursor: 'pointer'}}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 디바이스 테이블 */}
      <div style={tableContainerStyle}>
        {isInitialLoading ? (
          <div style={{display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '80px', color: '#6b7280'}}>
            <i className="fas fa-spinner fa-spin" style={{fontSize: '32px', color: '#3b82f6', marginBottom: '16px'}}></i>
            <span>디바이스 목록을 불러오는 중...</span>
          </div>
        ) : devices.length === 0 ? (
          <div style={{textAlign: 'center', padding: '80px', color: '#6b7280'}}>
            <i className="fas fa-network-wired" style={{fontSize: '48px', color: '#d1d5db', marginBottom: '16px'}}></i>
            <h3 style={{fontSize: '18px', fontWeight: '600', marginBottom: '8px', color: '#374151'}}>등록된 디바이스가 없습니다</h3>
            <p style={{fontSize: '14px', color: '#6b7280', marginBottom: '24px'}}>새 디바이스를 추가하여 시작하세요</p>
            <button style={{padding: '12px 20px', background: '#3b82f6', color: 'white', border: 'none', borderRadius: '8px', cursor: 'pointer', display: 'inline-flex', alignItems: 'center', gap: '8px', fontSize: '14px'}} onClick={handleCreateDevice}>
              <i className="fas fa-plus"></i>
              첫 번째 디바이스 추가
            </button>
          </div>
        ) : (
          <div key={`table-${renderKey}`}>
            {/* 헤더 */}
            <div style={tableHeaderStyle}>
              <div style={{textAlign: 'center', display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                <input type="checkbox" checked={selectedDevices.length === devices.length && devices.length > 0} onChange={(e) => handleSelectAll(e.target.checked)} style={{cursor: 'pointer'}} />
              </div>
              <div style={{textAlign: 'left'}}>디바이스</div>
              <div style={{textAlign: 'center'}}>프로토콜</div>
              <div style={{textAlign: 'center'}}>상태</div>
              <div style={{textAlign: 'center'}}>연결</div>
              <div style={{textAlign: 'center'}}>데이터</div>
              <div style={{textAlign: 'center'}}>성능</div>
              <div style={{textAlign: 'center'}}>작업</div>
            </div>

            {/* 바디 */}
            <div style={tableBodyStyle} ref={tableBodyRef} key={`tbody-${renderKey}`}>
              {devices.map((device, index) => (
                <div key={`device-${device.id}-${renderKey}-${currentPage}-${index}`} style={getRowStyle(index)}>
                  {/* 체크박스 */}
                  <div style={{textAlign: 'center', display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                    <input type="checkbox" checked={selectedDevices.includes(device.id)} onChange={(e) => handleDeviceSelect(device.id, e.target.checked)} style={{cursor: 'pointer'}} />
                  </div>

                  {/* 디바이스 정보 */}
                  <div style={{textAlign: 'left'}}>
                    <div style={{display: 'flex', alignItems: 'center', gap: '12px', cursor: 'pointer', padding: '4px 8px', borderRadius: '8px', transition: 'background-color 0.15s ease'}} onClick={() => handleDeviceClick(device)}>
                      <div style={{width: '32px', height: '32px', background: 'linear-gradient(135deg, #3b82f6, #2563eb)', borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'white', fontSize: '14px', flexShrink: 0}}>
                        <i className="fas fa-microchip"></i>
                      </div>
                      <div style={{minWidth: 0, flex: 1}}>
                        <div style={{fontWeight: '600', color: '#3b82f6', fontSize: '14px', marginBottom: '2px', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis'}}>
                          {device.name}
                        </div>
                        {device.manufacturer && (
                          <div style={{fontSize: '12px', color: '#6b7280', margin: '1px 0', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis'}}>
                            {device.manufacturer} {device.model}
                          </div>
                        )}
                        {device.description && (
                          <div style={{fontSize: '12px', color: '#6b7280', margin: '1px 0', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis'}}>
                            {device.description}
                          </div>
                        )}
                        <div style={{fontSize: '11px', color: '#9ca3af', fontFamily: 'monospace', margin: '2px 0 0 0', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis'}}>
                          {device.endpoint}
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* 프로토콜 */}
                  <div style={{textAlign: 'center'}}>
                    <span style={{...getProtocolBadgeStyle(device.protocol_type), fontSize: '11px', padding: '4px 8px', display: 'inline-block'}}>
                      {getProtocolDisplayName(device.protocol_type)}
                    </span>
                  </div>

                  {/* 상태 */}
                  <div style={{textAlign: 'center'}}>
                    <span style={{padding: '3px 8px', borderRadius: '4px', fontSize: '11px', fontWeight: '600', background: device.connection_status === 'connected' ? '#dcfce7' : device.connection_status === 'disconnected' ? '#fee2e2' : '#f3f4f6', color: device.connection_status === 'connected' ? '#166534' : device.connection_status === 'disconnected' ? '#dc2626' : '#4b5563', display: 'inline-block'}}>
                      {device.connection_status === 'connected' ? '연결' : device.connection_status === 'disconnected' ? '끊김' : '알수없음'}
                    </span>
                  </div>

                  {/* 연결 */}
                  <div style={{textAlign: 'center'}}>
                    <div style={{fontSize: '12px', fontWeight: '600', margin: '0', color: '#374151'}}>
                      {device.connection_status === 'connected' ? '정상' : device.connection_status === 'disconnected' ? '끊김' : '알수없음'}
                    </div>
                    <div style={{fontSize: '10px', color: '#9ca3af', margin: '2px 0 0 0'}}>
                      {device.last_seen ? new Date(device.last_seen).getMonth() + 1 + '/' + new Date(device.last_seen).getDate() : '없음'}
                    </div>
                  </div>

                  {/* 데이터 */}
                  <div style={{textAlign: 'center'}}>
                    <div style={{fontSize: '12px', fontWeight: '600', margin: '0', color: '#374151'}}>
                      {device.data_point_count || 0}개
                    </div>
                    <div style={{fontSize: '10px', color: '#9ca3af', margin: '2px 0 0 0'}}>
                      활성: {device.enabled_point_count || 0}개
                    </div>
                  </div>

                  {/* 성능 */}
                  <div style={{textAlign: 'center'}}>
                    <div style={{fontSize: '12px', fontWeight: '600', margin: '0', color: '#374151'}}>
                      {device.response_time || 0}ms
                    </div>
                    <div style={{fontSize: '10px', color: '#9ca3af', margin: '2px 0 0 0'}}>
                      98% OK
                    </div>
                  </div>

                  {/* 작업 버튼들 - 수직 정렬 완전 수정 */}
                  <div style={{
                    display: 'flex', 
                    flexDirection: 'column',
                    justifyContent: 'center', 
                    alignItems: 'center', 
                    height: '100%',
                    minHeight: screenWidth < 1400 ? '60px' : '70px',
                    width: '100%'
                  }}>
                    <div style={{
                      display: 'flex', 
                      gap: screenWidth < 1400 ? '1px' : '2px', 
                      justifyContent: 'center', 
                      alignItems: 'center'
                    }}>
                      <button onClick={() => handleEditDevice(device)} disabled={isProcessing} style={editButtonStyle} title="편집">
                        <i className="fas fa-edit"></i>
                      </button>
                      <button onClick={() => handleStartWorker(device.id)} disabled={isProcessing} style={startButtonStyle} title="워커 시작">
                        <i className="fas fa-play"></i>
                      </button>
                      <button onClick={() => handleStopWorker(device.id)} disabled={isProcessing} style={stopButtonStyle} title="워커 정지">
                        <i className="fas fa-stop"></i>
                      </button>
                      <button onClick={() => handleRestartWorker(device.id)} disabled={isProcessing} style={restartButtonStyle} title="워커 재시작">
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
      <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '16px 24px', background: '#ffffff', borderTop: '1px solid #e5e7eb', borderRadius: '0 0 12px 12px', margin: '0 24px 24px'}}>
        <div style={{flex: 1, textAlign: 'left', color: '#6b7280', fontSize: '14px', fontWeight: '500'}}>
          {totalCount > 0 ? (
            <span>{Math.min(((currentPage - 1) * pageSize) + 1, totalCount)}-{Math.min(currentPage * pageSize, totalCount)} / {totalCount}개</span>
          ) : (
            <span>데이터 없음</span>
          )}
        </div>

        <div style={{flex: '0 0 auto', display: 'flex', alignItems: 'center', gap: '4px'}}>
          <button onClick={() => handlePageChange(1)} disabled={currentPage === 1} style={{minWidth: '32px', height: '32px', padding: '0 8px', background: '#ffffff', border: '1px solid #d1d5db', borderRadius: '6px', color: currentPage === 1 ? '#d1d5db' : '#374151', fontSize: '14px', fontWeight: '500', cursor: currentPage === 1 ? 'not-allowed' : 'pointer', transition: 'all 0.2s ease'}}>««</button>
          <button onClick={() => handlePageChange(Math.max(1, currentPage - 1))} disabled={currentPage <= 1} style={{minWidth: '32px', height: '32px', padding: '0 8px', background: '#ffffff', border: '1px solid #d1d5db', borderRadius: '6px', color: currentPage <= 1 ? '#d1d5db' : '#374151', fontSize: '14px', fontWeight: '500', cursor: currentPage <= 1 ? 'not-allowed' : 'pointer', transition: 'all 0.2s ease'}}>‹</button>

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
              <button key={page} onClick={() => handlePageChange(page)} style={{minWidth: '32px', height: '32px', padding: '0 8px', background: page === currentPage ? '#3b82f6' : '#ffffff', border: page === currentPage ? '1px solid #3b82f6' : '1px solid #d1d5db', borderRadius: '6px', color: page === currentPage ? '#ffffff' : '#374151', fontSize: '14px', fontWeight: page === currentPage ? '600' : '500', cursor: 'pointer', transition: 'all 0.2s ease'}}>{page}</button>
            ));
          })()}

          <button onClick={() => handlePageChange(Math.min(totalPages, currentPage + 1))} disabled={currentPage >= totalPages} style={{minWidth: '32px', height: '32px', padding: '0 8px', background: '#ffffff', border: '1px solid #d1d5db', borderRadius: '6px', color: currentPage >= totalPages ? '#d1d5db' : '#374151', fontSize: '14px', fontWeight: '500', cursor: currentPage >= totalPages ? 'not-allowed' : 'pointer', transition: 'all 0.2s ease'}}>›</button>
          <button onClick={() => handlePageChange(totalPages)} disabled={currentPage === totalPages} style={{minWidth: '32px', height: '32px', padding: '0 8px', background: '#ffffff', border: '1px solid #d1d5db', borderRadius: '6px', color: currentPage === totalPages ? '#d1d5db' : '#374151', fontSize: '14px', fontWeight: '500', cursor: currentPage === totalPages ? 'not-allowed' : 'pointer', transition: 'all 0.2s ease'}}>»»</button>
        </div>

        <div style={{flex: 1, textAlign: 'right'}}>
          <select value={pageSize} onChange={(e) => handlePageSizeChange(Number(e.target.value))} style={{padding: '6px 8px', border: '1px solid #d1d5db', borderRadius: '6px', background: '#ffffff', color: '#374151', fontSize: '14px', cursor: 'pointer', minWidth: '100px'}}>
            <option value="10">10개씩</option>
            <option value="25">25개씩</option>
            <option value="50">50개씩</option>
            <option value="100">100개씩</option>
          </select>
        </div>
      </div>

      {/* 상태바 */}
      <div style={{background: '#ffffff', border: '1px solid #e5e7eb', borderRadius: '12px', padding: '16px 24px', margin: '24px 24px 0', display: 'flex', justifyContent: 'space-between', alignItems: 'center'}}>
        <div style={{display: 'flex', alignItems: 'center', gap: '24px'}}>
          <div style={{display: 'flex', alignItems: 'center', gap: '8px', fontSize: '14px', color: '#6b7280'}}>
            <span>마지막 업데이트:</span>
            <span style={{color: '#111827', fontWeight: '600'}}>
              {lastUpdate.toLocaleTimeString('ko-KR', { hour12: true, hour: '2-digit', minute: '2-digit', second: '2-digit' })}
            </span>
          </div>
          {isBackgroundRefreshing && (
            <div style={{display: 'flex', alignItems: 'center', gap: '8px', color: '#3b82f6'}}>
              <i className="fas fa-sync-alt fa-spin"></i>
              <span>백그라운드 업데이트 중...</span>
            </div>
          )}
        </div>
        <div style={{display: 'flex', alignItems: 'center', gap: '12px'}}>
          {isProcessing && (
            <span style={{display: 'flex', alignItems: 'center', gap: '8px', color: '#3b82f6'}}>
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
          <button style={{display: 'flex', alignItems: 'center', gap: '6px', padding: '8px 16px', background: '#f3f4f6', border: '1px solid #d1d5db', borderRadius: '8px', color: '#374151', fontSize: '14px', cursor: 'pointer', transition: 'all 0.15s ease'}} onClick={() => loadDevices(true)} disabled={isProcessing || isBackgroundRefreshing}>
            <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 확인 모달 */}
      {confirmModal.isOpen && (
        <div style={{position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, backgroundColor: 'rgba(0, 0, 0, 0.5)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000}}>
          <div style={{background: '#ffffff', borderRadius: '12px', padding: '32px', maxWidth: '500px', width: '90%', boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.25)', border: '1px solid #e5e7eb'}}>
            <div style={{display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '24px'}}>
              <div style={{width: '48px', height: '48px', borderRadius: '50%', display: 'flex', alignItems: 'center', justifyContent: 'center', background: confirmModal.type === 'danger' ? '#fee2e2' : confirmModal.type === 'warning' ? '#fef3c7' : '#eff6ff', color: confirmModal.type === 'danger' ? '#dc2626' : confirmModal.type === 'warning' ? '#d97706' : '#3b82f6'}}>
                <i className={`fas ${confirmModal.type === 'danger' ? 'fa-exclamation-triangle' : confirmModal.type === 'warning' ? 'fa-exclamation-circle' : 'fa-info-circle'}`} style={{fontSize: '20px'}}></i>
              </div>
              <h3 style={{fontSize: '20px', fontWeight: '700', color: '#111827', margin: 0}}>{confirmModal.title}</h3>
            </div>
            <div style={{fontSize: '14px', color: '#4b5563', lineHeight: '1.6', marginBottom: '32px', whiteSpace: 'pre-line'}}>{confirmModal.message}</div>
            <div style={{display: 'flex', gap: '12px', justifyContent: 'flex-end'}}>
              <button onClick={confirmModal.onCancel} style={{padding: '12px 24px', border: '1px solid #d1d5db', background: '#ffffff', color: '#374151', borderRadius: '8px', fontSize: '14px', fontWeight: '500', cursor: 'pointer', transition: 'all 0.2s ease'}}>{confirmModal.cancelText}</button>
              <button onClick={confirmModal.onConfirm} style={{padding: '12px 24px', border: 'none', background: confirmModal.type === 'danger' ? '#dc2626' : confirmModal.type === 'warning' ? '#d97706' : '#3b82f6', color: 'white', borderRadius: '8px', fontSize: '14px', fontWeight: '600', cursor: 'pointer', transition: 'all 0.2s ease'}}>{confirmModal.confirmText}</button>
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