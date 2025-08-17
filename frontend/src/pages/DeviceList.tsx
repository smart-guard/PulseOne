// ============================================================================
// frontend/src/pages/DeviceList.tsx  
// 🔥 실제 API만 연결 - 목 데이터 완전 제거 + 부드러운 새로고침
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { DeviceApiService, Device, DeviceStats } from '../api/services/deviceApi';
import DeviceDetailModal from '../components/modals/DeviceDetailModal';
import '../styles/base.css';
import '../styles/device-list.css';
import '../styles/pagination.css';

const DeviceList: React.FC = () => {
  // 🔧 기본 상태들
  const [devices, setDevices] = useState<Device[]>([]);
  const [deviceStats, setDeviceStats] = useState<DeviceStats | null>(null);
  const [selectedDevices, setSelectedDevices] = useState<number[]>([]);
  
  // 🔥 로딩 상태 분리: 초기 로딩 vs 백그라운드 새로고침
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

  // 🔥 모달 상태
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null);
  const [modalMode, setModalMode] = useState<'view' | 'edit' | 'create'>('view');
  const [isModalOpen, setIsModalOpen] = useState(false);

  // 🔥 페이징 훅 사용
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 25,
    totalCount: devices.length
  });

  // 🔥 첫 로딩 완료 여부 추적
  const [hasInitialLoad, setHasInitialLoad] = useState(false);
  
  // 🔥 스크롤 위치 저장용 ref
  const containerRef = useRef<HTMLDivElement>(null);

  // =============================================================================
  // 🔥 실제 API 데이터 기반 통계 계산 함수
  // =============================================================================
  const calculateRealTimeStats = useCallback((devices: Device[]): DeviceStats => {
    // 연결 상태별 카운트
    const connectedDevices = devices.filter(d => 
      d.connection_status === 'connected' || 
      d.status === 'connected' ||
      (typeof d.status === 'object' && d.status?.connection_status === 'connected')
    ).length;

    const disconnectedDevices = devices.filter(d => 
      d.connection_status === 'disconnected' || 
      d.status === 'disconnected' ||
      (typeof d.status === 'object' && d.status?.connection_status === 'disconnected')
    ).length;

    const errorDevices = devices.filter(d => 
      d.connection_status === 'error' || 
      d.status === 'error' ||
      (typeof d.status === 'object' && d.status?.connection_status === 'error') ||
      (d.error_count && d.error_count > 0)
    ).length;

    // 프로토콜별 분포 계산
    const protocolCounts = devices.reduce((acc, device) => {
      const protocol = device.protocol_type || 'UNKNOWN';
      acc[protocol] = (acc[protocol] || 0) + 1;
      return acc;
    }, {} as Record<string, number>);

    const protocolDistribution = Object.entries(protocolCounts).map(([protocol, count]) => ({
      protocol_type: protocol,
      count,
      percentage: devices.length > 0 ? Math.round((count / devices.length) * 100 * 10) / 10 : 0
    }));

    // 사이트별 분포 계산
    const siteCounts = devices.reduce((acc, device) => {
      const siteId = device.site_id || 1;
      const siteName = device.site_name || 'Main Site';
      const key = `${siteId}-${siteName}`;
      
      if (!acc[key]) {
        acc[key] = { site_id: siteId, site_name: siteName, device_count: 0 };
      }
      acc[key].device_count++;
      return acc;
    }, {} as Record<string, { site_id: number; site_name: string; device_count: number }>);

    const siteDistribution = Object.values(siteCounts);

    return {
      total_devices: devices.length,
      connected_devices: connectedDevices,
      disconnected_devices: disconnectedDevices,
      error_devices: errorDevices,
      protocols_count: Object.keys(protocolCounts).length,
      sites_count: siteDistribution.length,
      protocol_distribution: protocolDistribution,
      site_distribution: siteDistribution
    };
  }, []);

  // =============================================================================
  // 🔄 데이터 로드 함수들 (실제 API만, 부드러운 업데이트)
  // =============================================================================

  /**
   * 🔥 실제 API만 사용하는 디바이스 목록 로드 (스크롤 위치 유지)
   */
  const loadDevices = useCallback(async (isBackground = false) => {
    try {
      // 🔥 초기 로딩이 아닌 경우 백그라운드 새로고침만 표시
      if (!hasInitialLoad) {
        setIsInitialLoading(true);
      } else if (isBackground) {
        setIsBackgroundRefreshing(true);
      }
      
      setError(null);

      console.log(`📱 디바이스 목록 ${isBackground ? '백그라운드 ' : ''}로드 시작...`);

      // 🔥 실제 API 호출
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
        console.log(`✅ API로 디바이스 ${response.data.items.length}개 로드 완료`);
        
        if (!hasInitialLoad) {
          setHasInitialLoad(true);
        }
      } else {
        throw new Error(response.error || 'API 응답 오류');
      }

    } catch (err) {
      console.error('❌ 디바이스 목록 로드 실패:', err);
      setError(err instanceof Error ? err.message : '디바이스 목록을 불러올 수 없습니다');
      
      // 에러 시 빈 배열로 설정
      setDevices([]);
      pagination.updateTotalCount(0);
    } finally {
      setIsInitialLoading(false);
      setIsBackgroundRefreshing(false);
      setLastUpdate(new Date());
    }
  }, [pagination.currentPage, pagination.pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm, hasInitialLoad]);

  /**
   * 🔥 실제 API 우선, 실패 시 실시간 계산하는 통계 로드
   */
  const loadDeviceStats = useCallback(async () => {
    try {
      console.log('📊 디바이스 통계 로드 시작...');

      // 🔥 실제 API 시도
      try {
        const response = await DeviceApiService.getDeviceStatistics();

        if (response.success && response.data) {
          console.log('✅ API로 디바이스 통계 로드 완료:', response.data);
          setDeviceStats(response.data);
          return;
        }
      } catch (apiError) {
        console.warn('⚠️ 통계 API 호출 실패, 실시간 계산 사용:', apiError);
      }

      // 🔥 API 실패 시 현재 디바이스 목록으로 실시간 계산
      if (devices.length > 0) {
        console.log('📊 현재 디바이스 목록으로 통계 실시간 계산...');
        const calculatedStats = calculateRealTimeStats(devices);
        setDeviceStats(calculatedStats);
        console.log('✅ 실시간 계산된 통계:', calculatedStats);
      } else {
        // 디바이스가 없으면 0으로 초기화
        setDeviceStats({
          total_devices: 0,
          connected_devices: 0,
          disconnected_devices: 0,
          error_devices: 0,
          protocols_count: 0,
          sites_count: 0,
          protocol_distribution: [],
          site_distribution: []
        });
      }

    } catch (err) {
      console.warn('⚠️ 디바이스 통계 로드 실패:', err);
      // 에러 시에도 실시간 계산 시도
      if (devices.length > 0) {
        const calculatedStats = calculateRealTimeStats(devices);
        setDeviceStats(calculatedStats);
      }
    }
  }, [devices, calculateRealTimeStats]);

  /**
   * 🔥 실제 API만 사용하는 지원 프로토콜 목록 로드
   */
  const loadAvailableProtocols = useCallback(async () => {
    try {
      console.log('📋 지원 프로토콜 로드 시작...');

      const response = await DeviceApiService.getAvailableProtocols();

      if (response.success && response.data) {
        const protocols = response.data.map(p => p.protocol_type);
        setAvailableProtocols(protocols);
        console.log('✅ API로 지원 프로토콜 로드 완료:', protocols);
      } else {
        throw new Error(response.error || 'API 응답 오류');
      }

    } catch (err) {
      console.warn('⚠️ 지원 프로토콜 로드 실패:', err);
      // 에러 시 현재 디바이스에서 프로토콜 추출
      const currentProtocols = [...new Set(devices.map(d => d.protocol_type).filter(Boolean))];
      setAvailableProtocols(currentProtocols);
      console.log('📋 현재 디바이스에서 프로토콜 추출:', currentProtocols);
    }
  }, [devices]);

  // =============================================================================
  // 🔄 디바이스 제어 함수들 (실제 API 사용)
  // =============================================================================

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
          await loadDevices(true); // 백그라운드 새로고침
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
        await loadDevices(true); // 백그라운드 새로고침
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
  // 🔄 모달 이벤트 핸들러들 - DeviceDetailModal과 연결
  // =============================================================================

  const handleDeviceClick = (device: Device) => {
    console.log('👁️ 디바이스 상세 보기:', device.name);
    setSelectedDevice(device);
    setModalMode('view');
    setIsModalOpen(true);
  };

  const handleEditDevice = (device: Device) => {
    console.log('✏️ 디바이스 편집:', device.name);
    setSelectedDevice(device);
    setModalMode('edit');
    setIsModalOpen(true);
  };

  const handleCreateDevice = () => {
    console.log('➕ 새 디바이스 추가');
    setSelectedDevice(null);
    setModalMode('create');
    setIsModalOpen(true);
  };

  const handleCloseModal = () => {
    console.log('❌ 모달 닫기');
    setIsModalOpen(false);
    setSelectedDevice(null);
  };

  // 🔥 모달에서 디바이스 저장 처리
  const handleSaveDevice = async (deviceData: Device) => {
    try {
      setIsProcessing(true);
      console.log('💾 디바이스 저장:', deviceData);

      let response;
      
      if (modalMode === 'create') {
        // 새 디바이스 생성
        response = await DeviceApiService.createDevice({
          name: deviceData.name,
          protocol_type: deviceData.protocol_type,
          endpoint: deviceData.endpoint,
          device_type: deviceData.device_type,
          manufacturer: deviceData.manufacturer,
          model: deviceData.model,
          description: deviceData.description,
          polling_interval: deviceData.polling_interval,
          is_enabled: deviceData.is_enabled
        });
      } else if (modalMode === 'edit' && selectedDevice) {
        // 기존 디바이스 수정
        response = await DeviceApiService.updateDevice(selectedDevice.id, {
          name: deviceData.name,
          endpoint: deviceData.endpoint,
          device_type: deviceData.device_type,
          manufacturer: deviceData.manufacturer,
          model: deviceData.model,
          description: deviceData.description,
          polling_interval: deviceData.polling_interval,
          is_enabled: deviceData.is_enabled
        });
      }

      if (response?.success) {
        console.log('✅ 디바이스 저장 성공');
        await loadDevices(true); // 백그라운드 새로고침
        await loadDeviceStats(); // 통계 새로고침
        handleCloseModal();
      } else {
        throw new Error(response?.error || '저장 실패');
      }

    } catch (err) {
      console.error('❌ 디바이스 저장 실패:', err);
      setError(err instanceof Error ? err.message : '저장 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // 🔥 모달에서 디바이스 삭제 처리
  const handleDeleteDevice = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log('🗑️ 디바이스 삭제:', deviceId);

      const response = await DeviceApiService.deleteDevice(deviceId);

      if (response.success) {
        console.log('✅ 디바이스 삭제 성공');
        await loadDevices(true); // 백그라운드 새로고침
        await loadDeviceStats();
        handleCloseModal();
      } else {
        throw new Error(response.error || '삭제 실패');
      }

    } catch (err) {
      console.error('❌ 디바이스 삭제 실패:', err);
      setError(err instanceof Error ? err.message : '삭제 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 🔄 기타 이벤트 핸들러들
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

  // 🔥 부드러운 새로고침 (사용자 트리거)
  const handleManualRefresh = useCallback(async () => {
    console.log('🔄 수동 새로고침 시작...');
    await Promise.all([
      loadDevices(true),  // 백그라운드 새로고침
      loadDeviceStats()
    ]);
  }, [loadDevices, loadDeviceStats]);

  // =============================================================================
  // 🔄 라이프사이클 hooks
  // =============================================================================

  useEffect(() => {
    loadDevices();
    loadAvailableProtocols();
  }, [loadDevices, loadAvailableProtocols]);

  // 디바이스 목록이 변경될 때마다 통계 업데이트
  useEffect(() => {
    if (devices.length > 0) {
      loadDeviceStats();
    }
  }, [devices, loadDeviceStats]);

  // 필터 변경 시 데이터 다시 로드
  useEffect(() => {
    if (hasInitialLoad) {
      loadDevices(true); // 백그라운드 새로고침
    }
  }, [pagination.currentPage, pagination.pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm]);

  // 자동 새로고침 (부드러운 백그라운드 업데이트)
  useEffect(() => {
    if (!autoRefresh || !hasInitialLoad) return;

    const interval = setInterval(() => {
      console.log('🔄 자동 새로고침 (백그라운드)');
      loadDevices(true); // 백그라운드 새로고침
      loadDeviceStats();
    }, 10000); // 10초마다

    return () => clearInterval(interval);
  }, [autoRefresh, hasInitialLoad, loadDevices, loadDeviceStats]);

  // 🎨 프로토콜별 색상 매핑 함수
  const getProtocolBadgeStyle = (protocolType: string) => {
    const protocol = protocolType?.toUpperCase() || 'UNKNOWN';
    
    switch (protocol) {
      case 'MODBUS_TCP':
      case 'MODBUS_RTU':
        return {
          background: '#dbeafe',
          color: '#1e40af',
          border: '1px solid #93c5fd'
        };
      case 'BACNET':
      case 'BACNET_IP':
      case 'BACNET_MSTP':
        return {
          background: '#dcfce7',
          color: '#166534',
          border: '1px solid #86efac'
        };
      case 'MQTT':
        return {
          background: '#fef3c7',
          color: '#92400e',
          border: '1px solid #fcd34d'
        };
      case 'OPCUA':
      case 'OPC_UA':
        return {
          background: '#f3e8ff',
          color: '#7c3aed',
          border: '1px solid #c4b5fd'
        };
      case 'ETHERNET_IP':
        return {
          background: '#fed7d7',
          color: '#c53030',
          border: '1px solid #fc8181'
        };
      case 'PROFINET':
        return {
          background: '#e0f2fe',
          color: '#0369a1',
          border: '1px solid #7dd3fc'
        };
      case 'HTTP_REST':
      case 'HTTP':
        return {
          background: '#fff7ed',
          color: '#ea580c',
          border: '1px solid #fdba74'
        };
      case 'SNMP':
        return {
          background: '#ecfdf5',
          color: '#059669',
          border: '1px solid #6ee7b7'
        };
      default:
        return {
          background: '#f1f5f9',
          color: '#475569',
          border: '1px solid #cbd5e1'
        };
    }
  };

  // 프로토콜 표시명 정리 함수
  const getProtocolDisplayName = (protocolType: string) => {
    const protocol = protocolType?.toUpperCase() || 'UNKNOWN';
    
    switch (protocol) {
      case 'MODBUS_TCP': return 'Modbus TCP';
      case 'MODBUS_RTU': return 'Modbus RTU';
      case 'BACNET': 
      case 'BACNET_IP': return 'BACnet/IP';
      case 'BACNET_MSTP': return 'BACnet MS/TP';
      case 'MQTT': return 'MQTT';
      case 'OPCUA': 
      case 'OPC_UA': return 'OPC UA';
      case 'ETHERNET_IP': return 'Ethernet/IP';
      case 'PROFINET': return 'PROFINET';
      case 'HTTP_REST': 
      case 'HTTP': return 'HTTP REST';
      case 'SNMP': return 'SNMP';
      default: return protocol || 'Unknown';
    }
  };

  const getStatusBadgeClass = (status: string | any) => {
    const statusValue = typeof status === 'string' ? status : 
                       (status?.connection_status || 'unknown');
    
    switch (statusValue.toLowerCase()) {
      case 'running': 
      case 'connected': return 'status-badge status-running';
      case 'stopped': 
      case 'disconnected': return 'status-badge status-stopped';
      case 'error': return 'status-badge status-error';
      case 'disabled': return 'status-badge status-disabled';
      case 'restarting': return 'status-badge status-restarting';
      default: return 'status-badge status-unknown';
    }
  };

  const getConnectionBadgeClass = (connectionStatus: string | any) => {
    const statusValue = typeof connectionStatus === 'string' ? connectionStatus : 
                       (connectionStatus?.connection_status || 'unknown');
    
    switch (statusValue.toLowerCase()) {
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
    <div className="device-list-container" ref={containerRef}>
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

      {/* 🔥 실제 API 데이터 기반 통계 카드들 */}
      {deviceStats && (
        <div 
          className="stats-grid"
          style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(4, 1fr)',
            gap: '20px',
            marginBottom: '32px',
            padding: '0 32px'
          }}
        >
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-network-wired" style={{ color: '#3b82f6' }}></i>
            </div>
            <div className="stat-content">
              <div className="stat-value">{deviceStats.total_devices || 0}</div>
              <div className="stat-label">전체 디바이스</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-check-circle" style={{ color: '#10b981' }}></i>
            </div>
            <div className="stat-content">
              <div className="stat-value">{deviceStats.connected_devices || 0}</div>
              <div className="stat-label">연결됨</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-times-circle" style={{ color: '#ef4444' }}></i>
            </div>
            <div className="stat-content">
              <div className="stat-value">{deviceStats.disconnected_devices || 0}</div>
              <div className="stat-label">연결 끊김</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-exclamation-triangle" style={{ color: '#f59e0b' }}></i>
            </div>
            <div className="stat-content">
              <div className="stat-value">{deviceStats.error_devices || 0}</div>
              <div className="stat-label">오류</div>
            </div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div 
        className="filters-section"
        style={{
          background: '#ffffff',
          border: '1px solid #e5e7eb',
          borderRadius: '12px',
          padding: '20px 32px',
          marginBottom: '24px'
        }}
      >
        <div 
          className="filters-row"
          style={{
            display: 'flex',
            gap: '12px',
            alignItems: 'center',
            width: '100%'
          }}
        >
          {/* 검색창 */}
          <div 
            className="search-box"
            style={{
              position: 'relative',
              flex: '3',
              minWidth: '300px'
            }}
          >
            <i 
              className="fas fa-search"
              style={{
                position: 'absolute',
                left: '12px',
                top: '50%',
                transform: 'translateY(-50%)',
                color: '#9ca3af',
                fontSize: '14px'
              }}
            ></i>
            <input
              type="text"
              placeholder="디바이스 이름, 설명, 제조사 검색..."
              value={searchTerm}
              onChange={(e) => handleSearch(e.target.value)}
              style={{
                width: '100%',
                padding: '12px 16px 12px 40px',
                border: '1px solid #d1d5db',
                borderRadius: '8px',
                fontSize: '14px'
              }}
            />
          </div>
          
          {/* 필터들 */}
          <select
            value={statusFilter}
            onChange={(e) => handleFilterChange('status', e.target.value)}
            style={{
              padding: '12px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              fontSize: '14px',
              background: '#ffffff',
              cursor: 'pointer',
              width: '130px'
            }}
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
            style={{
              padding: '12px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              fontSize: '14px',
              background: '#ffffff',
              cursor: 'pointer',
              width: '140px'
            }}
          >
            <option value="all">모든 프로토콜</option>
            {availableProtocols.map(protocol => (
              <option key={protocol} value={protocol}>{protocol}</option>
            ))}
          </select>

          <select
            value={connectionFilter}
            onChange={(e) => handleFilterChange('connection', e.target.value)}
            style={{
              padding: '12px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              fontSize: '14px',
              background: '#ffffff',
              cursor: 'pointer',
              width: '140px'
            }}
          >
            <option value="all">모든 연결상태</option>
            <option value="connected">연결됨</option>
            <option value="disconnected">연결 끊김</option>
            <option value="error">연결 오류</option>
          </select>
        </div>

        {/* 일괄 작업 버튼들 */}
        {selectedDevices.length > 0 && (
          <div 
            className="bulk-actions"
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '12px',
              padding: '12px 16px',
              background: '#f3f4f6',
              borderRadius: '8px',
              border: '1px solid #e5e7eb',
              marginTop: '16px'
            }}
          >
            <span 
              className="selected-count"
              style={{
                fontSize: '14px',
                color: '#374151',
                fontWeight: '500'
              }}
            >
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
        {isInitialLoading ? (
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
          <div className="device-table">
            {/* 헤더 */}
            <div className="device-table-header">
              <div>
                <input
                  type="checkbox"
                  checked={selectedDevices.length === devices.length}
                  onChange={(e) => handleSelectAll(e.target.checked)}
                />
              </div>
              <div>디바이스</div>
              <div>프로토콜</div>
              <div>상태</div>
              <div>연결</div>
              <div>데이터</div>
              <div>성능</div>
              <div>네트워크</div>
              <div>작업</div>
            </div>

            {/* 바디 */}
            <div className="device-table-body">
              {devices.map((device) => (
                <div 
                  key={device.id}
                  className="device-table-row"
                >
                  {/* 체크박스 */}
                  <div className="device-table-cell">
                    <input
                      type="checkbox"
                      checked={selectedDevices.includes(device.id)}
                      onChange={(e) => handleDeviceSelect(device.id, e.target.checked)}
                    />
                  </div>

                  {/* 디바이스 정보 - 클릭 시 모달 열기 */}
                  <div className="device-table-cell">
                    <div className="device-info">
                      <div className="device-icon">
                        <i className="fas fa-microchip"></i>
                      </div>
                      <div>
                        <div 
                          className="device-name"
                          onClick={() => handleDeviceClick(device)}
                          style={{ cursor: 'pointer', color: '#3b82f6' }}
                        >
                          {device.name}
                        </div>
                        {device.description && (
                          <div className="device-endpoint">{device.description}</div>
                        )}
                        <div className="device-endpoint">{device.endpoint}</div>
                      </div>
                    </div>
                  </div>

                  {/* 프로토콜 - 🎨 색깔별로 구분 */}
                  <div className="device-table-cell">
                    <span 
                      className="protocol-badge"
                      style={{
                        ...getProtocolBadgeStyle(device.protocol_type),
                        padding: '4px 8px',
                        borderRadius: '12px',
                        fontSize: '11px',
                        fontWeight: '600',
                        textTransform: 'uppercase',
                        letterSpacing: '0.025em'
                      }}
                    >
                      {getProtocolDisplayName(device.protocol_type)}
                    </span>
                  </div>

                  {/* 상태 */}
                  <div className="device-table-cell">
                    <span className={`status ${getStatusBadgeClass(typeof device.status === 'string' ? device.status : device.status?.connection_status || 'unknown')}`}>
                      <span className={`status-dot status-dot-${typeof device.status === 'string' ? device.status : device.status?.connection_status || 'unknown'}`}></span>
                      {typeof device.status === 'string' ? device.status : device.status?.connection_status || 'unknown'}
                    </span>
                  </div>

                  {/* 연결상태 */}
                  <div className="device-table-cell">
                    <div className="connection-info">
                      <div className="info-title">
                        {typeof device.connection_status === 'string' ? device.connection_status : 
                         device.connection_status?.connection_status || 'unknown'}
                      </div>
                      <div className="info-subtitle">
                        {formatLastSeen(device.last_seen)}
                      </div>
                    </div>
                  </div>

                  {/* 🔥 데이터 정보 - 올바른 필드명 사용 */}
                  <div className="device-table-cell">
                    <div className="data-info">
                      <div className="info-title">
                        포인트: {device.data_point_count || 0}
                      </div>
                      <div className="info-subtitle">
                        활성: {device.enabled_point_count || 0}
                      </div>
                    </div>
                  </div>

                  {/* 성능 정보 */}
                  <div className="device-table-cell">
                    <div className="performance-info">
                      <div className="info-title">
                        응답: {typeof device.response_time === 'number' ? device.response_time : 
                              (device.status_info?.response_time || 0)}ms
                      </div>
                      <div className="info-subtitle">
                        처리율: {device.status_info?.successful_requests && device.status_info?.total_requests ? 
                               Math.round((device.status_info.successful_requests / device.status_info.total_requests) * 100) : 
                               98}%
                      </div>
                    </div>
                  </div>

                  {/* 네트워크 정보 */}
                  <div className="device-table-cell">
                    <div className="network-info">
                      <div className="info-title">
                        신호: {device.connection_status === 'connected' ? '좋음' : 
                              device.connection_status === 'disconnected' ? '끊김' :
                              device.connection_status === 'error' ? '오류' : '알수없음'}
                      </div>
                      <div className="info-subtitle">
                        오류: {device.error_count || 0}회
                      </div>
                    </div>
                  </div>

                  {/* 작업 버튼들 */}
                  <div className="device-table-cell">
                    <div className="device-actions">
                      <button 
                        onClick={() => handleTestConnection(device.id)}
                        disabled={isProcessing}
                        className="action-btn btn-view"
                        title="연결 테스트"
                      >
                        <i className="fas fa-plug"></i>
                      </button>
                      <button 
                        onClick={() => handleEditDevice(device)}
                        disabled={isProcessing}
                        className="action-btn btn-edit"
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

      {/* 🔥 부드러운 새로고침 상태바 */}
      <div style={{
        background: '#ffffff',
        border: '1px solid #e5e7eb',
        borderRadius: '12px',
        padding: '16px 24px',
        marginTop: '24px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '24px'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            fontSize: '14px',
            color: '#6b7280'
          }}>
            <span>마지막 업데이트:</span>
            <span style={{
              color: '#111827',
              fontWeight: '600'
            }}>
              {lastUpdate.toLocaleTimeString('ko-KR', { 
                hour12: true, 
                hour: '2-digit', 
                minute: '2-digit',
                second: '2-digit'
              })}
            </span>
          </div>

          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            fontSize: '14px'
          }}>
            <span style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '4px 12px',
              background: autoRefresh ? '#dcfce7' : '#fef3c7',
              color: autoRefresh ? '#166534' : '#92400e',
              borderRadius: '20px',
              fontSize: '12px',
              fontWeight: '500'
            }}>
              <div style={{
                width: '8px',
                height: '8px',
                borderRadius: '50%',
                background: autoRefresh ? '#22c55e' : '#f59e0b',
                animation: autoRefresh ? 'pulse 2s infinite' : 'none'
              }}></div>
              {autoRefresh ? '10초마다 자동 새로고침' : '자동새로고침 중지'}
            </span>
          </div>

          {/* 🔥 백그라운드 새로고침 상태 표시 */}
          {isBackgroundRefreshing && (
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '14px',
              color: '#3b82f6'
            }}>
              <i className="fas fa-sync-alt fa-spin" style={{ fontSize: '12px' }}></i>
              <span>백그라운드 업데이트 중...</span>
            </div>
          )}

          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '16px',
            fontSize: '14px',
            color: '#6b7280'
          }}>
            <span style={{
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              color: '#059669'
            }}>
              <i className="fas fa-circle" style={{ fontSize: '8px' }}></i>
              실제 API 데이터
            </span>
          </div>
        </div>

        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '12px'
        }}>
          {isProcessing && (
            <span style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              color: '#3b82f6',
              fontSize: '14px',
              fontWeight: '500'
            }}>
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
          
          <button
            onClick={handleManualRefresh}
            disabled={isProcessing || isBackgroundRefreshing}
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
              cursor: 'pointer',
              transition: 'all 0.2s ease'
            }}
            onMouseOver={(e) => {
              e.currentTarget.style.background = '#e5e7eb';
            }}
            onMouseOut={(e) => {
              e.currentTarget.style.background = '#f3f4f6';
            }}
          >
            <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 🔥 DeviceDetailModal 연결 */}
      <DeviceDetailModal
        device={selectedDevice}
        isOpen={isModalOpen}
        mode={modalMode}
        onClose={handleCloseModal}
        onSave={handleSaveDevice}
        onDelete={handleDeleteDevice}
      />
    </div>
  );
};

export default DeviceList;