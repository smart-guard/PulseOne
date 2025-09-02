// ============================================================================
// frontend/src/pages/DeviceList.tsx 
// 🔥 완전 복원 + 문제 수정 버전 
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

  // 페이징 훅 - 문제 수정
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 25,
    totalCount: 0
  });

  // 첫 로딩 완료 여부
  const [hasInitialLoad, setHasInitialLoad] = useState(false);
  
  // 자동새로고침 타이머 ref
  const autoRefreshRef = useRef<NodeJS.Timeout | null>(null);
  
  // 🔥 스피너 강제 고정을 위한 DOM 조작 useEffect
  useEffect(() => {
    const fixSpinnerPosition = () => {
      const container = document.querySelector('.device-list-container');
      if (!container) return;
      
      // 모든 스피너 아이콘 찾기
      const spinners = container.querySelectorAll('.fa-spin');
      spinners.forEach((spinner) => {
        const element = spinner as HTMLElement;
        element.style.position = 'relative';
        element.style.display = 'inline-block';
        element.style.margin = '0';
        element.style.padding = '0';
        element.style.float = 'none';
        element.style.clear = 'none';
        element.style.top = 'auto';
        element.style.left = 'auto';
        element.style.right = 'auto';
        element.style.bottom = 'auto';
        element.style.animation = 'spin 1s linear infinite';
        element.style.transformOrigin = 'center center';
      });
      
      // 백그라운드 새로고침 인디케이터 강제 고정
      const bgRefresh = container.querySelector('.background-refresh-indicator');
      if (bgRefresh) {
        const element = bgRefresh as HTMLElement;
        element.style.position = 'relative';
        element.style.display = 'inline-flex';
        element.style.alignItems = 'center';
        element.style.gap = '8px';
        element.style.margin = '0';
        element.style.float = 'none';
        element.style.clear = 'none';
      }
    };
    
    // 즉시 실행
    fixSpinnerPosition();
    
    // MutationObserver로 DOM 변경 감지하여 계속 적용
    const observer = new MutationObserver(fixSpinnerPosition);
    const container = document.querySelector('.device-list-container');
    if (container) {
      observer.observe(container, {
        childList: true,
        subtree: true,
        attributes: true,
        attributeFilter: ['class', 'style']
      });
    }
    
    return () => {
      observer.disconnect();
    };
  }, [isBackgroundRefreshing, isProcessing]); // 로딩 상태 변경 시마다 적용

  // =============================================================================
  // 실제 API 데이터 기반 통계 계산 함수
  // =============================================================================
  const calculateRealTimeStats = useCallback((devices: Device[]): DeviceStats => {
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

    return {
      total_devices: devices.length,
      connected_devices: connectedDevices,
      disconnected_devices: disconnectedDevices,
      error_devices: errorDevices,
      protocols_count: Object.keys(protocolCounts).length,
      sites_count: Object.values(siteCounts).length,
      protocol_distribution: protocolDistribution,
      site_distribution: Object.values(siteCounts)
    };
  }, []);

  // =============================================================================
  // 🔥 데이터 로드 함수들 - 페이징 수정
  // =============================================================================

  // 디바이스 목록 로드 - 페이징 수정
  const loadDevices = useCallback(async (isBackground = false) => {
    try {
      if (!hasInitialLoad) {
        setIsInitialLoading(true);
      } else if (isBackground) {
        setIsBackgroundRefreshing(true);
      }
      
      setError(null);

      console.log(`📱 디바이스 목록 로드 - 페이지: ${pagination.currentPage}, 크기: ${pagination.pageSize}`);

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

      console.log('📡 API 응답:', response);

      if (response.success && response.data) {
        setDevices(response.data.items);
        
        // 페이징 정보 설정
        const totalCount = response.data.pagination?.total || response.data.pagination?.totalCount || 0;
        
        console.log('페이징 정보:', {
          총개수: totalCount,
          현재페이지: pagination.currentPage,
          페이지크기: pagination.pageSize,
          아이템수: response.data.items?.length
        });
        
        pagination.updateTotalCount(totalCount);
        
        if (!hasInitialLoad) {
          setHasInitialLoad(true);
        }
      } else {
        console.error('❌ API 응답 실패:', response);
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
  }, [pagination, protocolFilter, connectionFilter, statusFilter, searchTerm, hasInitialLoad]); // 🔥 의존성 수정

  // 디바이스 통계 로드
  const loadDeviceStats = useCallback(async () => {
    try {
      console.log('📊 디바이스 통계 로드 시작...');

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

      if (devices.length > 0) {
        console.log('📊 현재 디바이스 목록으로 통계 실시간 계산...');
        const calculatedStats = calculateRealTimeStats(devices);
        setDeviceStats(calculatedStats);
        console.log('✅ 실시간 계산된 통계:', calculatedStats);
      } else {
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
      if (devices.length > 0) {
        const calculatedStats = calculateRealTimeStats(devices);
        setDeviceStats(calculatedStats);
      }
    }
  }, [devices, calculateRealTimeStats]);

  // 지원 프로토콜 목록 로드
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
      const currentProtocols = [...new Set(devices.map(d => d.protocol_type).filter(Boolean))];
      setAvailableProtocols(currentProtocols);
      console.log('📋 현재 디바이스에서 프로토콜 추출:', currentProtocols);
    }
  }, [devices]);

  // =============================================================================
  // 워커 제어 함수들 - 팝업과 툴팁 포함
  // =============================================================================

  // 워커 상태 확인 유틸리티
  const getWorkerStatus = (device: Device): string => {
    return device.collector_status?.status || 'unknown';
  };

  // 워커 상태별 버튼 표시 로직
  const shouldShowStartButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return ['stopped', 'error', 'unknown'].includes(status);
  };

  const shouldShowStopButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return ['running', 'paused'].includes(status);
  };

  const shouldShowPauseButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return status === 'running';
  };

  const shouldShowResumeButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return status === 'paused';
  };

  const getWorkerStatusText = (device: Device): string => {
    const status = getWorkerStatus(device);
    const statusMap: Record<string, string> = {
      'running': '실행중',
      'stopped': '정지됨',
      'paused': '일시정지',
      'starting': '시작중',
      'stopping': '정지중',
      'error': '오류',
      'unknown': '알수없음'
    };
    return statusMap[status] || '알수없음';
  };

  const getWorkerStatusClass = (device: Device): string => {
    const status = getWorkerStatus(device);
    return `worker-status-${status}`;
  };

  // 워커 제어 함수들 - 팝업 포함
  const handleStartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    if (!window.confirm(`워커를 시작하시겠습니까?\n\n디바이스: ${deviceName}\n엔드포인트: ${device?.endpoint || 'N/A'}\n프로토콜: ${device?.protocol_type || 'N/A'}`)) {
      return;
    }

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
      console.error(`❌ 워커 시작 실패: ${deviceId}`, err);
      alert(`워커 시작 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  const handleStopWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    if (!window.confirm(`워커를 정지하시겠습니까?\n\n디바이스: ${deviceName}\n⚠️ 주의: 데이터 수집이 중단됩니다.`)) {
      return;
    }

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
      console.error(`❌ 워커 정지 실패: ${deviceId}`, err);
      alert(`워커 정지 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  const handleRestartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    if (!window.confirm(`워커를 재시작하시겠습니까?\n\n디바이스: ${deviceName}`)) {
      return;
    }

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
      console.error(`❌ 워커 재시작 실패: ${deviceId}`, err);
      alert(`워커 재시작 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  const handlePauseWorker = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      const response = await DeviceApiService.pauseDeviceWorker(deviceId);
      if (response.success) {
        alert('워커가 일시정지되었습니다');
        await loadDevices(true);
      }
    } catch (err) {
      alert(`워커 일시정지 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  const handleResumeWorker = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      const response = await DeviceApiService.resumeDeviceWorker(deviceId);
      if (response.success) {
        alert('워커가 재개되었습니다');
        await loadDevices(true);
      }
    } catch (err) {
      alert(`워커 재개 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 연결 테스트
  const handleTestConnection = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      const response = await DeviceApiService.testDeviceConnection(deviceId);
      if (response.success && response.data) {
        const result = response.data;
        const message = result.test_successful 
          ? `연결 성공 (응답시간: ${result.response_time_ms}ms)`
          : `연결 실패: ${result.error_message}`;
        alert(message);
        if (result.test_successful) {
          await loadDevices(true);
        }
      }
    } catch (err) {
      alert(`연결 테스트 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 일괄 작업
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
      const response = await DeviceApiService.bulkAction({
        action,
        device_ids: selectedDevices
      });

      if (response.success && response.data) {
        const result = response.data;
        alert(`작업 완료: 성공 ${result.successful}개, 실패 ${result.failed}개`);
        setSelectedDevices([]);
        await loadDevices(true);
        await loadDeviceStats();
      }
    } catch (err) {
      alert(`일괄 작업 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 이벤트 핸들러들
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

  const handleManualRefresh = useCallback(async () => {
    console.log('🔄 수동 새로고침 시작...');
    await Promise.all([
      loadDevices(true),
      loadDeviceStats()
    ]);
  }, [loadDevices, loadDeviceStats]);

  // 모달 핸들러들
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

  // 모달에서 디바이스 저장 처리
  const handleSaveDevice = async (deviceData: Device) => {
    try {
      setIsProcessing(true);
      console.log('💾 디바이스 저장:', deviceData);

      let response;
      
      if (modalMode === 'create') {
        response = await DeviceApiService.createDevice({
          name: deviceData.name,
          protocol_id: deviceData.protocol_id, // protocol_type → protocol_id
          endpoint: deviceData.endpoint,
          device_type: deviceData.device_type,
          manufacturer: deviceData.manufacturer,
          model: deviceData.model,
          description: deviceData.description,
          polling_interval: deviceData.polling_interval,
          is_enabled: deviceData.is_enabled
        });
      } else if (modalMode === 'edit' && selectedDevice) {
        response = await DeviceApiService.updateDevice(selectedDevice.id, {
          name: deviceData.name,
          protocol_id: deviceData.protocol_id, // protocol_type → protocol_id
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
        await loadDevices(true);
        await loadDeviceStats();
        handleCloseModal();
      } else {
        throw new Error(response?.error || '저장 실패');
      }

    } catch (err) {
      console.error('❌ 디바이스 저장 실패:', err);
      alert(`저장 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 모달에서 디바이스 삭제 처리
  const handleDeleteDevice = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log('🗑️ 디바이스 삭제:', deviceId);

      const response = await DeviceApiService.deleteDevice(deviceId);

      if (response.success) {
        console.log('✅ 디바이스 삭제 성공');
        await loadDevices(true);
        await loadDeviceStats();
        handleCloseModal();
      } else {
        throw new Error(response.error || '삭제 실패');
      }

    } catch (err) {
      console.error('❌ 디바이스 삭제 실패:', err);
      alert(`삭제 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 라이프사이클 hooks
  // =============================================================================

  // 초기 로딩
  useEffect(() => {
    console.log('🚀 DeviceList 초기 로딩');
    loadDevices();
    loadAvailableProtocols();
  }, []);

  // 디바이스 목록이 변경될 때마다 통계 업데이트
  useEffect(() => {
    if (devices.length > 0) {
      loadDeviceStats();
    }
  }, [devices.length, loadDeviceStats]);

  // 필터 변경 시 데이터 다시 로드 - 무한호출 방지
  useEffect(() => {
    if (hasInitialLoad) {
      console.log('필터 변경으로 인한 재로드');
      loadDevices(true);
    }
  }, [
    pagination.currentPage, 
    pagination.pageSize, 
    protocolFilter, 
    connectionFilter, 
    statusFilter, 
    searchTerm, 
    hasInitialLoad
  ]); // pagination 객체 대신 개별 속성으로 분리

  // 자동 새로고침 - 깜빡임 방지
  useEffect(() => {
    if (!autoRefresh || !hasInitialLoad) {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
      return;
    }

    // 자동 새로고침 간격을 60초로 늘려서 깜빡임 줄이기
    autoRefreshRef.current = setInterval(() => {
      console.log('자동 새로고침 (백그라운드)');
      loadDevices(true); // 백그라운드 로딩으로 깜빡임 방지
    }, 60000); // 30초 → 60초로 변경

    return () => {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
    };
  }, [autoRefresh, hasInitialLoad]); // loadDevices 의존성 제거로 무한호출 방지) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
    };
  }, [autoRefresh, hasInitialLoad]);

  // =============================================================================
  // 스타일링 함수들 - 원본 복원
  // =============================================================================

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
  // 🔥 UI 렌더링 - 완전한 원본 데이터 표시
  // =============================================================================

  return (
    <div className="device-list-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-network-wired"></i>
            디바이스 관리
          </h1>
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
              <div className="stat-value">{deviceStats.total_devices || 0}</div>
              <div className="stat-label">전체 디바이스</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-check-circle text-success"></i>
            </div>
            <div className="stat-content">
              <div className="stat-value">{deviceStats.connected_devices || 0}</div>
              <div className="stat-label">연결됨</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-times-circle text-error"></i>
            </div>
            <div className="stat-content">
              <div className="stat-value">{deviceStats.disconnected_devices || 0}</div>
              <div className="stat-label">연결 끊김</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-exclamation-triangle text-warning"></i>
            </div>
            <div className="stat-content">
              <div className="stat-value">{deviceStats.error_devices || 0}</div>
              <div className="stat-label">오류</div>
            </div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div className="filters-section">
        <div className="filters-row">
          <div className="search-box">
            <i className="fas fa-search"></i>
            <input
              type="text"
              placeholder="디바이스 이름, 설명, 제조사 검색..."
              value={searchTerm}
              onChange={(e) => handleSearch(e.target.value)}
            />
          </div>
          
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

        {/* 일괄 작업 버튼들 */}
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
              <i className="fas fa-check"></i>
              일괄 활성화
            </button>
            <button 
              onClick={() => handleBulkAction('disable')}
              disabled={isProcessing}
              className="btn btn-sm btn-warning"
            >
              <i className="fas fa-pause"></i>
              일괄 비활성화
            </button>
            <button 
              onClick={() => handleBulkAction('delete')}
              disabled={isProcessing}
              className="btn btn-sm btn-danger"
            >
              <i className="fas fa-trash"></i>
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
              <div>워커상태</div>
              <div>작업</div>
            </div>

            {/* 바디 */}
            <div className="device-table-body">
              {devices.map((device) => (
                <div key={device.id} className="device-table-row">
                  {/* 체크박스 */}
                  <div className="device-table-cell">
                    <input
                      type="checkbox"
                      checked={selectedDevices.includes(device.id)}
                      onChange={(e) => handleDeviceSelect(device.id, e.target.checked)}
                    />
                  </div>

                  {/* 디바이스 정보 - 원본 풍부한 데이터 복원 */}
                  <div className="device-table-cell">
                    <div className="device-info">
                      <div className="device-icon">
                        <i className="fas fa-microchip"></i>
                      </div>
                      <div>
                        <div 
                          className="device-name"
                          onClick={() => handleDeviceClick(device)}
                        >
                          {device.name}
                        </div>
                        {device.manufacturer && device.model && (
                          <div className="device-details">
                            <span className="device-manufacturer">{device.manufacturer}</span>
                            <span className="device-model">{device.model}</span>
                          </div>
                        )}
                        {device.description && (
                          <div className="device-description">{device.description}</div>
                        )}
                        <div className="device-endpoint">{device.endpoint}</div>
                      </div>
                    </div>
                  </div>

                  {/* 프로토콜 */}
                  <div className="device-table-cell">
                    <span 
                      className="protocol-badge"
                      style={getProtocolBadgeStyle(device.protocol_type)}
                    >
                      {getProtocolDisplayName(device.protocol_type)}
                    </span>
                  </div>

                  {/* 상태 */}
                  <div className="device-table-cell">
                    <span className={getStatusBadgeClass(typeof device.status === 'string' ? device.status : device.status?.connection_status || 'unknown')}>
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

                  {/* 데이터 정보 */}
                  <div className="device-table-cell">
                    <div className="data-info">
                      <div className="info-title">
                        포인트: {device.data_point_count || device.data_points_count || 0}
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
                        응답: {device.response_time || device.status_info?.response_time || 0}ms
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

                  {/* 워커 상태 */}
                  <div className="device-table-cell">
                    <span className={`worker-status-badge ${getWorkerStatusClass(device)}`}>
                      {getWorkerStatusText(device)}
                    </span>
                  </div>

                  {/* 🔥 작업 버튼들 - 툴팁 포함 */}
                  <div className="device-table-cell">
                    <div className="device-actions">
                      
                      {/* 연결 테스트 */}
                      <button 
                        onClick={() => handleTestConnection(device.id)}
                        disabled={isProcessing}
                        className="action-btn btn-view"
                        title="연결 테스트"
                      >
                        <i className="fas fa-plug"></i>
                      </button>
                      
                      {/* 편집 */}
                      <button 
                        onClick={() => handleEditDevice(device)}
                        disabled={isProcessing}
                        className="action-btn btn-edit"
                        title="편집"
                      >
                        <i className="fas fa-edit"></i>
                      </button>
                      
                      <div className="action-divider"></div>
                      
                      {/* 워커 시작 */}
                      {shouldShowStartButton(device) && (
                        <button 
                          onClick={() => handleStartWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-start"
                          title="워커 시작 - 데이터 수집을 시작합니다"
                        >
                          <i className="fas fa-play"></i>
                        </button>
                      )}
                      
                      {/* 워커 일시정지 */}
                      {shouldShowPauseButton(device) && (
                        <button 
                          onClick={() => handlePauseWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-pause"
                          title="워커 일시정지 - 데이터 수집을 일시적으로 중단합니다"
                        >
                          <i className="fas fa-pause"></i>
                        </button>
                      )}
                      
                      {/* 워커 재개 */}
                      {shouldShowResumeButton(device) && (
                        <button 
                          onClick={() => handleResumeWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-resume"
                          title="워커 재개 - 일시정지된 데이터 수집을 다시 시작합니다"
                        >
                          <i className="fas fa-play"></i>
                        </button>
                      )}
                      
                      {/* 워커 정지 */}
                      {shouldShowStopButton(device) && (
                        <button 
                          onClick={() => handleStopWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-stop"
                          title="워커 정지 - 데이터 수집을 완전히 중단합니다"
                        >
                          <i className="fas fa-stop"></i>
                        </button>
                      )}
                      
                      {/* 워커 재시작 */}
                      <button 
                        onClick={() => handleRestartWorker(device.id)}
                        disabled={isProcessing}
                        className="action-btn btn-restart"
                        title="워커 재시작 - 워커를 다시 시작합니다"
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

      {/* 🔥 상태바 - 로딩 인디케이터 위치 완전 수정 */}
      <div className="status-bar">
        <div className="status-bar-left">
          <div className="last-update">
            <span>마지막 업데이트:</span>
            <span className="update-time">
              {lastUpdate.toLocaleTimeString('ko-KR', { 
                hour12: true, 
                hour: '2-digit', 
                minute: '2-digit',
                second: '2-digit'
              })}
            </span>
          </div>

          <div className="auto-refresh-status">
            <span className={`refresh-indicator ${autoRefresh ? 'active' : 'inactive'}`}>
              <div className="refresh-dot"></div>
              {autoRefresh ? '30초마다 자동 새로고침' : '자동새로고침 중지'}
            </span>
          </div>

          {/* 🔥 백그라운드 새로고침 인디케이터 - 인라인 스타일로 강제 위치 고정 */}
          {isBackgroundRefreshing && (
            <div 
              className="background-refresh-indicator"
              style={{
                display: 'inline-flex',
                alignItems: 'center',
                gap: '8px',
                fontSize: '14px',
                color: '#3b82f6',
                background: '#eff6ff',
                padding: '6px 12px',
                borderRadius: '6px',
                border: '1px solid #dbeafe',
                whiteSpace: 'nowrap',
                flexShrink: 0,
                position: 'relative',
                margin: 0,
                float: 'none'
              }}
            >
              <i 
                className="fas fa-sync-alt fa-spin"
                style={{
                  fontSize: '12px',
                  color: '#3b82f6',
                  display: 'inline-block',
                  margin: 0,
                  padding: 0,
                  animation: 'spin 1s linear infinite'
                }}
              ></i>
              <span>백그라운드 업데이트 중...</span>
            </div>
          )}

        </div>

        <div className="status-bar-right">
          {isProcessing && (
            <span className="processing-indicator">
              <i className="fas fa-spinner fa-spin"></i>
              처리 중...
            </span>
          )}
          
          <button
            className="refresh-button"
            onClick={handleManualRefresh}
            disabled={isProcessing || isBackgroundRefreshing}
          >
            <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
        </div>
      </div>

      {/* DeviceDetailModal 연결 */}
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