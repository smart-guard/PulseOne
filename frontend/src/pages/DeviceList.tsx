// ============================================================================
// frontend/src/pages/DeviceList.tsx 
// 🔥 인라인 스타일로 완전 수정 - CSS 문제 해결
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { DeviceApiService, Device, DeviceStats } from '../api/services/deviceApi';
import DeviceDetailModal from '../components/modals/DeviceDetailModal';

const DeviceList: React.FC = () => {
  console.log('💡 DeviceList 컴포넌트 렌더링 시작');
  
  // 기본 상태들
  const [devices, setDevices] = useState<Device[]>([]);
  const [deviceStats, setDeviceStats] = useState<DeviceStats | null>(null);
  const [selectedDevices, setSelectedDevices] = useState<number[]>([]);
  
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

  // 페이징 훅
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 25,
    totalCount: 0
  });

  // 첫 로딩 완료 여부
  const [hasInitialLoad, setHasInitialLoad] = useState(false);
  
  // 자동새로고침 타이머 ref
  const autoRefreshRef = useRef<NodeJS.Timeout | null>(null);

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
        page: pagination.currentPage,
        limit: pagination.pageSize,
        protocol_type: protocolFilter !== 'all' ? protocolFilter : undefined,
        connection_status: connectionFilter !== 'all' ? connectionFilter : undefined,
        status: statusFilter !== 'all' ? statusFilter : undefined,
        search: searchTerm || undefined,
        sort_by: 'name',
        sort_order: 'ASC',
        include_collector_status: true
      });

      if (response.success && response.data) {
        setDevices(response.data.items || []);
        
        const totalCount = response.data.pagination?.total || response.data.pagination?.totalCount || 0;
        pagination.updateTotalCount(totalCount);
        
        if (!hasInitialLoad) {
          setHasInitialLoad(true);
        }
      } else {
        throw new Error(response.error || 'API 응답 오류');
      }

    } catch (err) {
      console.error('❌ 디바이스 목록 로드 실패:', err);
      setError(err instanceof Error ? err.message : '디바이스 목록을 불러올 수 없습니다');
      setDevices([]);
      pagination.updateTotalCount(0);
    } finally {
      setIsInitialLoading(false);
      setIsBackgroundRefreshing(false);
      setLastUpdate(new Date());
    }
  }, [pagination.currentPage, pagination.pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm, hasInitialLoad]);

  const loadDeviceStats = useCallback(async () => {
    try {
      const response = await DeviceApiService.getDeviceStatistics();
      if (response.success && response.data) {
        setDeviceStats(response.data);
      } else {
        // 간단한 통계 계산
        setDeviceStats({
          total_devices: devices.length,
          connected_devices: devices.filter(d => d.connection_status === 'connected').length,
          disconnected_devices: devices.filter(d => d.connection_status === 'disconnected').length,
          error_devices: devices.filter(d => d.connection_status === 'error').length,
          protocols_count: [...new Set(devices.map(d => d.protocol_type))].length,
          sites_count: 1,
          protocol_distribution: [],
          site_distribution: []
        });
      }
    } catch (err) {
      console.warn('통계 로드 실패:', err);
    }
  }, [devices]);

  const loadAvailableProtocols = useCallback(async () => {
    try {
      const response = await DeviceApiService.getAvailableProtocols();
      if (response.success && response.data) {
        const protocols = response.data.map(p => p.protocol_type);
        setAvailableProtocols(protocols);
      } else {
        const currentProtocols = [...new Set(devices.map(d => d.protocol_type).filter(Boolean))];
        setAvailableProtocols(currentProtocols);
      }
    } catch (err) {
      console.warn('프로토콜 로드 실패:', err);
    }
  }, [devices]);

  // =============================================================================
  // 워커 제어 함수들
  // =============================================================================

  const handleStartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    showConfirmModal({
      title: '워커 시작 확인',
      message: `워커를 시작하시겠습니까?\n\n디바이스: ${deviceName}\n엔드포인트: ${device?.endpoint || 'N/A'}\n프로토콜: ${device?.protocol_type || 'N/A'}\n\n⚠️ 데이터 수집이 시작됩니다.`,
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
      message: `워커를 정지하시겠습니까?\n\n디바이스: ${deviceName}\n현재 상태: 실행중\n\n⚠️ 주의: 데이터 수집이 중단됩니다.\n이 작업은 신중하게 수행해주세요.`,
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
      message: `워커를 재시작하시겠습니까?\n\n디바이스: ${deviceName}\n현재 상태: 실행중\n\n⚠️ 워커가 일시적으로 중단된 후 다시 시작됩니다.\n데이터 수집에 짧은 중단이 발생할 수 있습니다.`,
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
    pagination.goToFirst();
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
    pagination.goToFirst();
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
  };

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
  }, [devices.length]);

  useEffect(() => {
    if (hasInitialLoad) {
      loadDevices(true);
    }
  }, [pagination.currentPage, pagination.pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm]);

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
  }, [autoRefresh, hasInitialLoad]);

  // =============================================================================
  // UI 렌더링 - 완전 인라인 스타일
  // =============================================================================

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

  const titleStyle = {
    fontSize: '28px',
    fontWeight: '700',
    color: '#111827',
    margin: '0',
    display: 'flex',
    alignItems: 'center',
    gap: '12px'
  };

  const subtitleStyle = {
    fontSize: '16px',
    color: '#6b7280',
    margin: '8px 0 0 0'
  };

  const statsGridStyle = {
    display: 'grid',
    gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))',
    gap: '16px',
    marginBottom: '32px',
    padding: '0 24px'
  };

  const statCardStyle = {
    background: '#ffffff',
    border: '1px solid #e5e7eb',
    borderRadius: '12px',
    padding: '24px',
    textAlign: 'center' as const,
    boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
  };

  const filtersStyle = {
    background: '#ffffff',
    border: '1px solid #e5e7eb',
    borderRadius: '12px',
    padding: '20px 24px',
    margin: '0 24px 24px',
    display: 'flex',
    gap: '12px',
    alignItems: 'center',
    flexWrap: 'wrap' as const
  };

  const searchBoxStyle = {
    position: 'relative' as const,
    flex: '1',
    minWidth: '200px'
  };

  const searchInputStyle = {
    width: '100%',
    padding: '12px 16px 12px 40px',
    border: '1px solid #d1d5db',
    borderRadius: '8px',
    fontSize: '14px'
  };

  const selectStyle = {
    padding: '12px',
    border: '1px solid #d1d5db',
    borderRadius: '8px',
    fontSize: '14px',
    background: '#ffffff',
    minWidth: '120px'
  };

  const tableContainerStyle = {
    background: '#ffffff',
    border: '1px solid #e5e7eb',
    borderRadius: '12px',
    overflow: 'hidden',
    margin: '0 24px',
    boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
  };

  const tableStyle = {
    width: '100%',
    borderCollapse: 'collapse' as const
  };

  const tableHeaderStyle = {
    background: '#f3f4f6',
    borderBottom: '2px solid #e5e7eb'
  };

  const headerCellStyle = {
    padding: '16px 12px',
    fontSize: '12px',
    fontWeight: '700',
    color: '#374151',
    textTransform: 'uppercase' as const,
    letterSpacing: '0.025em',
    textAlign: 'center' as const,
    borderRight: '1px solid #e5e7eb'
  };

  const headerCellFirstStyle = {
    ...headerCellStyle,
    textAlign: 'left' as const,
    width: '50px'
  };

  const headerCellDeviceStyle = {
    ...headerCellStyle,
    textAlign: 'left' as const,
    width: '300px'
  };

  const tableCellStyle = {
    padding: '12px',
    fontSize: '14px',
    textAlign: 'center' as const,
    borderRight: '1px solid #e5e7eb',
    borderBottom: '1px solid #e5e7eb',
    verticalAlign: 'middle' as const
  };

  const tableCellFirstStyle = {
    ...tableCellStyle,
    textAlign: 'left' as const,
    width: '50px'
  };

  const tableCellDeviceStyle = {
    ...tableCellStyle,
    textAlign: 'left' as const,
    width: '300px'
  };

  const deviceInfoStyle = {
    display: 'flex',
    alignItems: 'center',
    gap: '12px'
  };

  const deviceIconStyle = {
    width: '32px',
    height: '32px',
    background: 'linear-gradient(135deg, #3b82f6, #2563eb)',
    borderRadius: '8px',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    color: 'white',
    fontSize: '14px',
    flexShrink: 0
  };

  const deviceNameStyle = {
    fontWeight: '600',
    color: '#111827',
    fontSize: '14px',
    marginBottom: '2px',
    cursor: 'pointer'
  };

  const deviceDetailStyle = {
    fontSize: '12px',
    color: '#6b7280',
    margin: '1px 0'
  };

  const actionButtonStyle = {
    width: '32px',
    height: '32px',
    border: 'none',
    borderRadius: '6px',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    cursor: 'pointer',
    fontSize: '12px',
    margin: '0 2px'
  };

  const testButtonStyle = {
    ...actionButtonStyle,
    background: '#3b82f6',
    color: 'white'
  };

  const editButtonStyle = {
    ...actionButtonStyle,
    background: '#8b5cf6',
    color: 'white'
  };

  const startButtonStyle = {
    ...actionButtonStyle,
    background: '#10b981',
    color: 'white'
  };

  const stopButtonStyle = {
    ...actionButtonStyle,
    background: '#ef4444',
    color: 'white'
  };

  const restartButtonStyle = {
    ...actionButtonStyle,
    background: '#f59e0b',
    color: 'white'
  };

  const statusBarStyle = {
    background: '#ffffff',
    border: '1px solid #e5e7eb',
    borderRadius: '12px',
    padding: '16px 24px',
    margin: '24px 24px 0',
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center'
  };

  const spinnerStyle = {
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
    color: '#3b82f6'
  };

  return (
    <div style={containerStyle}>
      {/* 페이지 헤더 */}
      <div style={headerStyle}>
        <div>
          <h1 style={titleStyle}>
            <i className="fas fa-network-wired" style={{color: '#3b82f6'}}></i>
            디바이스 관리
          </h1>
          <div style={subtitleStyle}>
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
              gap: '8px'
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
          <div style={statCardStyle}>
            <i className="fas fa-network-wired" style={{fontSize: '32px', color: '#3b82f6', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.total_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>전체 디바이스</div>
          </div>
          <div style={statCardStyle}>
            <i className="fas fa-check-circle" style={{fontSize: '32px', color: '#10b981', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.connected_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>연결됨</div>
          </div>
          <div style={statCardStyle}>
            <i className="fas fa-times-circle" style={{fontSize: '32px', color: '#ef4444', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.disconnected_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>연결 끊김</div>
          </div>
          <div style={statCardStyle}>
            <i className="fas fa-exclamation-triangle" style={{fontSize: '32px', color: '#f59e0b', marginBottom: '12px'}}></i>
            <div style={{fontSize: '32px', fontWeight: '700', color: '#111827'}}>{deviceStats.error_devices || 0}</div>
            <div style={{fontSize: '14px', color: '#6b7280'}}>오류</div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div style={filtersStyle}>
        <div style={searchBoxStyle}>
          <i className="fas fa-search" style={{position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)', color: '#9ca3af'}}></i>
          <input
            type="text"
            placeholder="디바이스 이름, 설명, 제조사 검색..."
            value={searchTerm}
            onChange={(e) => handleSearch(e.target.value)}
            style={searchInputStyle}
          />
        </div>
        
        <select
          value={statusFilter}
          onChange={(e) => handleFilterChange('status', e.target.value)}
          style={selectStyle}
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
          style={selectStyle}
        >
          <option value="all">모든 프로토콜</option>
          {availableProtocols.map(protocol => (
            <option key={protocol} value={protocol}>{protocol}</option>
          ))}
        </select>

        <select
          value={connectionFilter}
          onChange={(e) => handleFilterChange('connection', e.target.value)}
          style={selectStyle}
        >
          <option value="all">모든 연결상태</option>
          <option value="connected">연결됨</option>
          <option value="disconnected">연결 끊김</option>
          <option value="error">연결 오류</option>
        </select>
      </div>

      {/* 에러 표시 */}
      {error && (
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          padding: '12px 16px',
          background: '#fef2f2',
          border: '1px solid #fecaca',
          borderRadius: '8px',
          color: '#dc2626',
          margin: '0 24px 16px'
        }}>
          <i className="fas fa-exclamation-circle"></i>
          {error}
          <button 
            onClick={() => setError(null)}
            style={{marginLeft: 'auto', background: 'none', border: 'none', color: '#dc2626', cursor: 'pointer'}}
          >
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 디바이스 테이블 - CSS Grid */}
      <div style={tableContainerStyle}>
        {isInitialLoading ? (
          <div style={{display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '60px', color: '#6b7280'}}>
            <i className="fas fa-spinner fa-spin" style={{fontSize: '32px', color: '#3b82f6', marginBottom: '16px'}}></i>
            <span>디바이스 목록을 불러오는 중...</span>
          </div>
        ) : devices.length === 0 ? (
          <div style={{textAlign: 'center', padding: '60px', color: '#6b7280'}}>
            <i className="fas fa-network-wired" style={{fontSize: '48px', color: '#d1d5db', marginBottom: '16px'}}></i>
            <h3 style={{fontSize: '18px', fontWeight: '600', marginBottom: '8px', color: '#374151'}}>등록된 디바이스가 없습니다</h3>
            <p style={{fontSize: '14px', color: '#6b7280', marginBottom: '24px'}}>새 디바이스를 추가하여 시작하세요</p>
            <button 
              style={{
                padding: '12px 20px',
                background: '#3b82f6',
                color: 'white',
                border: 'none',
                borderRadius: '8px',
                cursor: 'pointer',
                display: 'inline-flex',
                alignItems: 'center',
                gap: '8px'
              }}
              onClick={handleCreateDevice}
            >
              <i className="fas fa-plus"></i>
              첫 번째 디바이스 추가
            </button>
          </div>
        ) : (
          <div>
            {/* 헤더 - CSS Grid */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '50px 400px 100px 70px 80px 100px 90px 140px',
              gap: '2px',
              padding: '12px 8px',
              background: '#f3f4f6',
              borderBottom: '2px solid #e5e7eb',
              fontSize: '12px',
              fontWeight: '700',
              color: '#374151',
              textTransform: 'uppercase',
              letterSpacing: '0.025em',
              alignItems: 'center'
            }}>
              <div style={{textAlign: 'center'}}>
                <input
                  type="checkbox"
                  checked={selectedDevices.length === devices.length && devices.length > 0}
                  onChange={(e) => handleSelectAll(e.target.checked)}
                />
              </div>
              <div style={{textAlign: 'center'}}>디바이스</div>
              <div style={{textAlign: 'center'}}>프로토콜</div>
              <div style={{textAlign: 'center'}}>상태</div>
              <div style={{textAlign: 'center'}}>연결</div>
              <div style={{textAlign: 'center'}}>데이터</div>
              <div style={{textAlign: 'center'}}>성능</div>
              <div style={{textAlign: 'center'}}>작업</div>
            </div>

            {/* 바디 - CSS Grid */}
            <div style={{maxHeight: '70vh', overflowY: 'auto'}}>
              {devices.map((device, index) => (
                <div 
                  key={device.id} 
                  style={{
                    display: 'grid',
                    gridTemplateColumns: '50px 400px 100px 70px 80px 100px 90px 140px',
                    gap: '2px',
                    padding: '8px',
                    borderBottom: '1px solid #e5e7eb',
                    alignItems: 'center',
                    backgroundColor: index % 2 === 0 ? '#ffffff' : '#fafafa',
                    ':hover': {backgroundColor: '#f9fafb'}
                  }}
                >
                  {/* 체크박스 */}
                  <div style={{textAlign: 'center'}}>
                    <input
                      type="checkbox"
                      checked={selectedDevices.includes(device.id)}
                      onChange={(e) => handleDeviceSelect(device.id, e.target.checked)}
                    />
                  </div>

                  {/* 디바이스 정보 - 클릭 시 상세 모달 */}
                  <div style={{textAlign: 'left'}}>
                    <div 
                      style={{
                        ...deviceInfoStyle,
                        cursor: 'pointer',
                        padding: '4px',
                        ':hover': {backgroundColor: '#f3f4f6'}
                      }}
                      onClick={() => handleDeviceClick(device)}
                    >
                      <div style={{...deviceIconStyle, width: '28px', height: '28px', marginRight: '8px'}}>
                        <i className="fas fa-microchip"></i>
                      </div>
                      <div>
                        <div style={{
                          ...deviceNameStyle,
                          color: '#3b82f6',
                          margin: '0 0 2px 0',
                          ':hover': {color: '#2563eb', textDecoration: 'underline'}
                        }}>
                          {device.name}
                        </div>
                        {device.manufacturer && (
                          <div style={{...deviceDetailStyle, margin: '1px 0'}}>
                            {device.manufacturer} {device.model}
                          </div>
                        )}
                        {device.description && (
                          <div style={{...deviceDetailStyle, margin: '1px 0'}}>{device.description}</div>
                        )}
                        <div style={{...deviceDetailStyle, fontFamily: 'monospace', margin: '1px 0'}}>{device.endpoint}</div>
                      </div>
                    </div>
                  </div>

                  {/* 프로토콜 */}
                  <div style={{textAlign: 'center'}}>
                    <span style={{
                      ...getProtocolBadgeStyle(device.protocol_type),
                      fontSize: '11px',
                      padding: '2px 4px'
                    }}>
                      {getProtocolDisplayName(device.protocol_type)}
                    </span>
                  </div>

                  {/* 상태 */}
                  <div style={{textAlign: 'center'}}>
                    <span style={{
                      padding: '2px 4px',
                      borderRadius: '3px',
                      fontSize: '11px',
                      fontWeight: '600',
                      background: device.connection_status === 'connected' ? '#dcfce7' : 
                                 device.connection_status === 'disconnected' ? '#fee2e2' : '#f3f4f6',
                      color: device.connection_status === 'connected' ? '#166534' : 
                             device.connection_status === 'disconnected' ? '#dc2626' : '#4b5563'
                    }}>
                      {device.connection_status === 'connected' ? '연결' : 
                       device.connection_status === 'disconnected' ? '끊김' : '알수없음'}
                    </span>
                  </div>

                  {/* 연결 */}
                  <div style={{textAlign: 'center'}}>
                    <div>
                      <div style={{fontSize: '12px', fontWeight: '600', margin: '0'}}>
                        {device.connection_status === 'connected' ? '정상' : 
                         device.connection_status === 'disconnected' ? '끊김' : '알수없음'}
                      </div>
                      <div style={{fontSize: '10px', color: '#6b7280', margin: '1px 0 0 0'}}>
                        {device.last_seen ? new Date(device.last_seen).getMonth() + 1 + '/' + new Date(device.last_seen).getDate() : '없음'}
                      </div>
                    </div>
                  </div>

                  {/* 데이터 */}
                  <div style={{textAlign: 'center'}}>
                    <div>
                      <div style={{fontSize: '12px', fontWeight: '600', margin: '0'}}>
                        포인트: {device.data_point_count || 0}
                      </div>
                      <div style={{fontSize: '10px', color: '#6b7280', margin: '1px 0 0 0'}}>
                        활성: {device.enabled_point_count || 0}
                      </div>
                    </div>
                  </div>

                  {/* 성능 */}
                  <div style={{textAlign: 'center'}}>
                    <div>
                      <div style={{fontSize: '12px', fontWeight: '600', margin: '0'}}>
                        응답: {device.response_time || 0}ms
                      </div>
                      <div style={{fontSize: '10px', color: '#6b7280', margin: '1px 0 0 0'}}>
                        처리율: 98%
                      </div>
                    </div>
                  </div>

                  {/* 작업 버튼들 - 공간을 더 활용하여 분산 배치 */}
                  <div style={{textAlign: 'center'}}>
                    <div style={{display: 'flex', gap: '4px', justifyContent: 'space-between', width: '100%', padding: '0 4px'}}>
                      <button 
                        onClick={() => handleEditDevice(device)}
                        disabled={isProcessing}
                        style={{...editButtonStyle, margin: '0'}}
                        title="편집"
                      >
                        <i className="fas fa-edit"></i>
                      </button>
                      
                      <button 
                        onClick={() => handleStartWorker(device.id)}
                        disabled={isProcessing}
                        style={{...startButtonStyle, margin: '0'}}
                        title="워커 시작"
                      >
                        <i className="fas fa-play"></i>
                      </button>
                      
                      <button 
                        onClick={() => handleStopWorker(device.id)}
                        disabled={isProcessing}
                        style={{...stopButtonStyle, margin: '0'}}
                        title="워커 정지"
                      >
                        <i className="fas fa-stop"></i>
                      </button>
                      
                      <button 
                        onClick={() => handleRestartWorker(device.id)}
                        disabled={isProcessing}
                        style={{...restartButtonStyle, margin: '0'}}
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
      {devices.length > 0 && (
        <div style={{padding: '24px'}}>
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

      {/* 상태바 */}
      <div style={statusBarStyle}>
        <div style={{display: 'flex', alignItems: 'center', gap: '24px'}}>
          <div style={{display: 'flex', alignItems: 'center', gap: '8px', fontSize: '14px', color: '#6b7280'}}>
            <span>마지막 업데이트:</span>
            <span style={{color: '#111827', fontWeight: '600'}}>
              {lastUpdate.toLocaleTimeString('ko-KR', { 
                hour12: true, 
                hour: '2-digit', 
                minute: '2-digit',
                second: '2-digit'
              })}
            </span>
          </div>

          {isBackgroundRefreshing && (
            <div style={spinnerStyle}>
              <i className="fas fa-sync-alt fa-spin"></i>
              <span>백그라운드 업데이트 중...</span>
            </div>
          )}
        </div>

        <div style={{display: 'flex', alignItems: 'center', gap: '12px'}}>
          {isProcessing && (
            <span style={spinnerStyle}>
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
          
          <button
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '8px 16px',
              background: '#f3f4f6',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              color: '#374151',
              fontSize: '14px',
              cursor: 'pointer'
            }}
            onClick={async () => {
              await Promise.all([loadDevices(true), loadDeviceStats()]);
            }}
            disabled={isProcessing || isBackgroundRefreshing}
          >
            <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 커스텀 확인 모달 */}
      {confirmModal.isOpen && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.5)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            background: '#ffffff',
            borderRadius: '12px',
            padding: '32px',
            maxWidth: '500px',
            width: '90%',
            boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.25)',
            border: '1px solid #e5e7eb'
          }}>
            {/* 모달 헤더 */}
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '12px',
              marginBottom: '24px'
            }}>
              <div style={{
                width: '48px',
                height: '48px',
                borderRadius: '50%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                background: confirmModal.type === 'danger' ? '#fee2e2' : 
                           confirmModal.type === 'warning' ? '#fef3c7' : '#eff6ff',
                color: confirmModal.type === 'danger' ? '#dc2626' :
                       confirmModal.type === 'warning' ? '#d97706' : '#3b82f6'
              }}>
                <i className={`fas ${
                  confirmModal.type === 'danger' ? 'fa-exclamation-triangle' :
                  confirmModal.type === 'warning' ? 'fa-exclamation-circle' : 'fa-info-circle'
                }`} style={{fontSize: '20px'}}></i>
              </div>
              <h3 style={{
                fontSize: '20px',
                fontWeight: '700',
                color: '#111827',
                margin: 0
              }}>
                {confirmModal.title}
              </h3>
            </div>

            {/* 모달 내용 */}
            <div style={{
              fontSize: '14px',
              color: '#4b5563',
              lineHeight: '1.6',
              marginBottom: '32px',
              whiteSpace: 'pre-line'
            }}>
              {confirmModal.message}
            </div>

            {/* 모달 버튼들 */}
            <div style={{
              display: 'flex',
              gap: '12px',
              justifyContent: 'flex-end'
            }}>
              <button
                onClick={confirmModal.onCancel}
                style={{
                  padding: '12px 24px',
                  border: '1px solid #d1d5db',
                  background: '#ffffff',
                  color: '#374151',
                  borderRadius: '8px',
                  fontSize: '14px',
                  fontWeight: '500',
                  cursor: 'pointer',
                  transition: 'all 0.2s ease'
                }}
                onMouseOver={(e) => {
                  e.currentTarget.style.background = '#f9fafb';
                }}
                onMouseOut={(e) => {
                  e.currentTarget.style.background = '#ffffff';
                }}
              >
                {confirmModal.cancelText}
              </button>
              <button
                onClick={confirmModal.onConfirm}
                style={{
                  padding: '12px 24px',
                  border: 'none',
                  background: confirmModal.type === 'danger' ? '#dc2626' :
                             confirmModal.type === 'warning' ? '#d97706' : '#3b82f6',
                  color: 'white',
                  borderRadius: '8px',
                  fontSize: '14px',
                  fontWeight: '600',
                  cursor: 'pointer',
                  transition: 'all 0.2s ease'
                }}
                onMouseOver={(e) => {
                  const currentBg = e.currentTarget.style.background;
                  if (currentBg.includes('#dc2626')) {
                    e.currentTarget.style.background = '#b91c1c';
                  } else if (currentBg.includes('#d97706')) {
                    e.currentTarget.style.background = '#b45309';
                  } else {
                    e.currentTarget.style.background = '#2563eb';
                  }
                }}
                onMouseOut={(e) => {
                  e.currentTarget.style.background = confirmModal.type === 'danger' ? '#dc2626' :
                                                     confirmModal.type === 'warning' ? '#d97706' : '#3b82f6';
                }}
              >
                {confirmModal.confirmText}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* DeviceDetailModal */}
      <DeviceDetailModal
        device={selectedDevice}
        isOpen={isModalOpen}
        mode={modalMode}
        onClose={handleCloseModal}
        onSave={async () => {}}
        onDelete={async () => {}}
      />
    </div>
  );
};

export default DeviceList;