// ============================================================================
// frontend/src/pages/DeviceList.tsx 
// 🔥 완전한 최종 버전 - 기존 기능 + 새로운 Collector 제어 기능 모두 통합
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

  // 페이징 훅
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 25,
    totalCount: 0
  });

  // 첫 로딩 완료 여부
  const [hasInitialLoad, setHasInitialLoad] = useState(false);
  
  // 스크롤 위치 저장용 ref
  const containerRef = useRef<HTMLDivElement>(null);
  
  // 자동새로고침 타이머 ref
  const autoRefreshRef = useRef<NodeJS.Timeout | null>(null);

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
  // 데이터 로드 함수들 (실제 API만, 부드러운 업데이트)
  // =============================================================================

  // 디바이스 목록 로드 - 의존성 제거하여 무한호출 방지
  const loadDevices = useCallback(async (isBackground = false) => {
    try {
      if (!hasInitialLoad) {
        setIsInitialLoading(true);
      } else if (isBackground) {
        setIsBackgroundRefreshing(true);
      }
      
      setError(null);

      console.log(`📱 디바이스 목록 ${isBackground ? '백그라운드 ' : ''}로드 시작...`);

      const response = await DeviceApiService.getDevices({
        page: pagination.currentPage,
        limit: pagination.pageSize,
        protocol_type: protocolFilter !== 'all' ? protocolFilter : undefined,
        connection_status: connectionFilter !== 'all' ? connectionFilter : undefined,
        status: statusFilter !== 'all' ? statusFilter : undefined,
        search: searchTerm || undefined,
        sort_by: 'name',
        sort_order: 'ASC',
        include_collector_status: true // 실시간 워커 상태 포함
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
      setDevices([]);
      pagination.updateTotalCount(0);
    } finally {
      setIsInitialLoading(false);
      setIsBackgroundRefreshing(false);
      setLastUpdate(new Date());
    }
  }, []); // 의존성 완전 제거

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
  // 🔥 기존 디바이스 제어 함수들 (DB 레벨) - 그대로 유지
  // =============================================================================

  // 디바이스 연결 테스트
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
          await loadDevices(true);
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

  // 일괄 작업 처리
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
        
        setSelectedDevices([]);
        await loadDevices(true);
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
  // 🔥 신규 추가: Collector 워커 제어 함수들 (실시간 제어)
  // =============================================================================

  /**
   * 워커 시작 (Collector 레벨) - 확인 팝업 추가
   */
  const handleStartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    // 확인 팝업
    const confirmed = window.confirm(
      `워커를 시작하시겠습니까?\n\n` +
      `디바이스: ${deviceName}\n` +
      `엔드포인트: ${device?.endpoint || 'N/A'}\n` +
      `프로토콜: ${device?.protocol_type || 'N/A'}\n\n` +
      `이 작업은 데이터 수집을 시작합니다.`
    );
    
    if (!confirmed) {
      console.log(`🚫 워커 시작 취소: ${deviceId}`);
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`🚀 워커 시작: ${deviceId}`);

      const response = await DeviceApiService.startDeviceWorker(deviceId);

      if (response.success) {
        console.log(`✅ 워커 시작 완료: ${deviceId}`);
        
        // 성공 메시지 표시
        const workerInfo = response.data;
        const message = workerInfo?.worker_pid 
          ? `워커가 시작되었습니다 (PID: ${workerInfo.worker_pid})`
          : '워커가 시작되었습니다';
        
        alert(message);
        
        // 상태 새로고침
        await loadDevices(true);
      } else {
        throw new Error(response.error || '워커 시작 실패');
      }
    } catch (err) {
      console.error(`❌ 워커 시작 실패: ${deviceId}`, err);
      setError(err instanceof Error ? err.message : '워커 시작 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 워커 정지 (Collector 레벨) - 확인 팝업 추가
   */
  const handleStopWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    // 확인 팝업
    const confirmed = window.confirm(
      `워커를 정지하시겠습니까?\n\n` +
      `디바이스: ${deviceName}\n` +
      `현재 상태: ${device?.collector_status?.status || '알 수 없음'}\n\n` +
      `⚠️ 주의: 데이터 수집이 중단됩니다.`
    );
    
    if (!confirmed) {
      console.log(`🚫 워커 정지 취소: ${deviceId}`);
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`🛑 워커 정지: ${deviceId}`);

      const response = await DeviceApiService.stopDeviceWorker(deviceId, { graceful: true });

      if (response.success) {
        console.log(`✅ 워커 정지 완료: ${deviceId}`);
        
        alert('워커가 안전하게 정지되었습니다');
        
        // 상태 새로고침
        await loadDevices(true);
      } else {
        throw new Error(response.error || '워커 정지 실패');
      }
    } catch (err) {
      console.error(`❌ 워커 정지 실패: ${deviceId}`, err);
      setError(err instanceof Error ? err.message : '워커 정지 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 워커 재시작 (Collector 레벨) - 확인 팝업 추가
   */
  const handleRestartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    // 확인 팝업
    const confirmed = window.confirm(
      `워커를 재시작하시겠습니까?\n\n` +
      `디바이스: ${deviceName}\n` +
      `현재 상태: ${device?.collector_status?.status || '알 수 없음'}\n\n` +
      `재시작하면 데이터 수집이 일시적으로 중단됩니다.`
    );
    
    if (!confirmed) {
      console.log(`🚫 워커 재시작 취소: ${deviceId}`);
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`🔄 워커 재시작: ${deviceId}`);

      const response = await DeviceApiService.restartDeviceWorker(deviceId);

      if (response.success) {
        console.log(`✅ 워커 재시작 완료: ${deviceId}`);
        
        const workerInfo = response.data;
        const message = workerInfo?.worker_pid 
          ? `워커가 재시작되었습니다 (PID: ${workerInfo.worker_pid})`
          : '워커가 재시작되었습니다';
        
        alert(message);
        
        // 상태 새로고침
        await loadDevices(true);
      } else {
        throw new Error(response.error || '워커 재시작 실패');
      }
    } catch (err) {
      console.error(`❌ 워커 재시작 실패: ${deviceId}`, err);
      setError(err instanceof Error ? err.message : '워커 재시작 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 워커 일시정지 (새로운 기능) - 확인 팝업 추가
   */
  const handlePauseWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    // 확인 팝업
    const confirmed = window.confirm(
      `워커를 일시정지하시겠습니까?\n\n` +
      `디바이스: ${deviceName}\n` +
      `데이터 수집이 일시적으로 중단됩니다.\n` +
      `나중에 재개할 수 있습니다.`
    );
    
    if (!confirmed) {
      console.log(`🚫 워커 일시정지 취소: ${deviceId}`);
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`⏸️ 워커 일시정지: ${deviceId}`);

      const response = await DeviceApiService.pauseDeviceWorker(deviceId);

      if (response.success) {
        console.log(`✅ 워커 일시정지 완료: ${deviceId}`);
        alert('워커가 일시정지되었습니다');
        await loadDevices(true);
      } else {
        throw new Error(response.error || '워커 일시정지 실패');
      }
    } catch (err) {
      console.error(`❌ 워커 일시정지 실패: ${deviceId}`, err);
      setError(err instanceof Error ? err.message : '워커 일시정지 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 워커 재개 (새로운 기능) - 확인 팝업 추가
   */
  const handleResumeWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const deviceName = device?.name || `Device ${deviceId}`;
    
    // 확인 팝업
    const confirmed = window.confirm(
      `워커를 재개하시겠습니까?\n\n` +
      `디바이스: ${deviceName}\n` +
      `데이터 수집을 다시 시작합니다.`
    );
    
    if (!confirmed) {
      console.log(`🚫 워커 재개 취소: ${deviceId}`);
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`▶️ 워커 재개: ${deviceId}`);

      const response = await DeviceApiService.resumeDeviceWorker(deviceId);

      if (response.success) {
        console.log(`✅ 워커 재개 완료: ${deviceId}`);
        alert('워커가 재개되었습니다');
        await loadDevices(true);
      } else {
        throw new Error(response.error || '워커 재개 실패');
      }
    } catch (err) {
      console.error(`❌ 워커 재개 실패: ${deviceId}`, err);
      setError(err instanceof Error ? err.message : '워커 재개 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 실시간 워커 상태 조회 (새로운 기능)
   */
  const handleCheckWorkerStatus = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log(`📊 워커 상태 조회: ${deviceId}`);

      const response = await DeviceApiService.getDeviceWorkerStatus(deviceId);

      if (response.success && response.data) {
        const status = response.data;
        
        const statusInfo = `
워커 상태: ${status.worker_status || 'unknown'}
PID: ${status.worker_pid || 'N/A'}
업타임: ${DeviceApiService.formatDeviceUptime(status.uptime_seconds)}
마지막 활동: ${status.last_activity || 'N/A'}
처리된 요청: ${status.performance_metrics?.requests_processed || 0}개
평균 응답시간: ${status.performance_metrics?.avg_response_time_ms || 0}ms
        `.trim();
        
        alert(statusInfo);
        console.log(`✅ 워커 상태 조회 완료: ${deviceId}`, status);
      } else {
        throw new Error(response.error || '워커 상태 조회 실패');
      }
    } catch (err) {
      console.error(`❌ 워커 상태 조회 실패: ${deviceId}`, err);
      setError(err instanceof Error ? err.message : '워커 상태 조회 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 🔥 신규 추가: 워커 상태 기반 UI 제어 함수들
  // =============================================================================

  /**
   * 워커 상태 확인 유틸리티
   */
  const getWorkerStatus = (device: Device): string => {
    return device.collector_status?.status || 'unknown';
  };

  /**
   * 워커 상태별 버튼 활성화 체크
   */
  const shouldShowStartButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return status === 'stopped' || status === 'error' || status === 'unknown';
  };

  const shouldShowStopButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return status === 'running' || status === 'paused';
  };

  const shouldShowPauseButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return status === 'running';
  };

  const shouldShowResumeButton = (device: Device): boolean => {
    const status = getWorkerStatus(device);
    return status === 'paused';
  };

  /**
   * 워커 상태 표시 텍스트
   */
  const getWorkerStatusText = (device: Device): string => {
    const status = getWorkerStatus(device);
    switch (status) {
      case 'running': return '실행 중';
      case 'stopped': return '정지됨';
      case 'paused': return '일시정지';
      case 'starting': return '시작 중';
      case 'stopping': return '정지 중';
      case 'error': return '오류';
      default: return '알 수 없음';
    }
  };

  /**
   * 워커 상태별 CSS 클래스
   */
  const getWorkerStatusClass = (device: Device): string => {
    const status = getWorkerStatus(device);
    switch (status) {
      case 'running': return 'worker-status-running';
      case 'stopped': return 'worker-status-stopped';
      case 'paused': return 'worker-status-paused';
      case 'starting': return 'worker-status-starting';
      case 'stopping': return 'worker-status-stopping';
      case 'error': return 'worker-status-error';
      default: return 'worker-status-unknown';
    }
  };

  /**
   * 디바이스 설정 재로드
   */
  const handleReloadDeviceConfig = async (deviceId: number) => {
    try {
      setIsProcessing(true);
      console.log(`🔄 설정 재로드: ${deviceId}`);

      const response = await DeviceApiService.reloadDeviceConfig(deviceId);

      if (response.success && response.data) {
        const result = response.data;
        const message = `설정 재로드 완료\n적용된 변경사항: ${result.changes_applied || 0}개\n경고: ${result.warnings?.length || 0}개`;
        alert(message);
        console.log(`✅ 설정 재로드 완료:`, result);
        await loadDevices(true);
      } else {
        throw new Error(response.error || '설정 재로드 실패');
      }
    } catch (err) {
      console.error(`❌ 설정 재로드 실패: ${deviceId}`, err);
      setError(err instanceof Error ? err.message : '설정 재로드 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 🔥 신규 추가: 일괄 워커 제어 함수들
  // =============================================================================

  /**
   * 워커 일괄 제어
   */
  const handleBulkWorkerAction = async (action: 'start' | 'stop' | 'restart') => {
    if (selectedDevices.length === 0) {
      alert('작업할 디바이스를 선택해주세요.');
      return;
    }

    const actionText = action === 'start' ? '시작' : action === 'stop' ? '정지' : '재시작';
    const confirmMessage = `선택된 ${selectedDevices.length}개 디바이스의 워커를 ${actionText}하시겠습니까?`;
    
    if (!window.confirm(confirmMessage)) {
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`🔄 워커 일괄 ${action}:`, selectedDevices);

      let response;
      
      if (action === 'start') {
        response = await DeviceApiService.startMultipleDeviceWorkers(selectedDevices);
      } else if (action === 'stop') {
        response = await DeviceApiService.stopMultipleDeviceWorkers(selectedDevices, { graceful: true });
      } else {
        // 재시작은 개별적으로 처리 (배치 API가 없을 경우)
        const results = await Promise.allSettled(
          selectedDevices.map(deviceId => 
            DeviceApiService.restartDeviceWorker(deviceId)
          )
        );
        
        const successful = results.filter(r => r.status === 'fulfilled').length;
        const failed = results.length - successful;
        
        response = {
          success: true,
          data: { total_processed: results.length, successful, failed }
        };
      }

      if (response.success && response.data) {
        const result = response.data;
        const message = `워커 ${actionText} 완료\n성공: ${result.successful}개\n실패: ${result.failed}개`;
        alert(message);
        
        console.log(`✅ 워커 일괄 ${action} 완료:`, result);
        
        setSelectedDevices([]);
        await loadDevices(true);
      } else {
        throw new Error(response.error || `워커 일괄 ${actionText} 실패`);
      }
    } catch (err) {
      console.error(`❌ 워커 일괄 ${action} 실패:`, err);
      setError(err instanceof Error ? err.message : `워커 일괄 ${actionText} 실패`);
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 설정 관리 일괄 작업
   */
  const handleBulkConfigAction = async (action: 'reload' | 'test') => {
    if (selectedDevices.length === 0) {
      alert('작업할 디바이스를 선택해주세요.');
      return;
    }

    const actionText = action === 'reload' ? '설정 재로드' : '연결 테스트';
    const confirmMessage = `선택된 ${selectedDevices.length}개 디바이스에 ${actionText}를 수행하시겠습니까?`;
    
    if (!window.confirm(confirmMessage)) {
      return;
    }

    try {
      setIsProcessing(true);
      console.log(`🔄 ${actionText} 일괄 실행:`, selectedDevices);

      const results = await Promise.allSettled(
        selectedDevices.map(async (deviceId) => {
          if (action === 'reload') {
            return await DeviceApiService.reloadDeviceConfig(deviceId);
          } else {
            return await DeviceApiService.testDeviceConnection(deviceId);
          }
        })
      );

      const successful = results.filter(r => r.status === 'fulfilled' && r.value.success).length;
      const failed = results.length - successful;
      
      const message = `${actionText} 완료\n성공: ${successful}개\n실패: ${failed}개`;
      alert(message);
      
      console.log(`✅ ${actionText} 일괄 실행 완료: 성공 ${successful}개, 실패 ${failed}개`);
      
      setSelectedDevices([]);
      await loadDevices(true);
      
    } catch (err) {
      console.error(`❌ ${actionText} 일괄 실행 실패:`, err);
      setError(err instanceof Error ? err.message : `${actionText} 일괄 실행 실패`);
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 모달 이벤트 핸들러들 - 그대로 유지
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

  // 모달에서 디바이스 저장 처리
  const handleSaveDevice = async (deviceData: Device) => {
    try {
      setIsProcessing(true);
      console.log('💾 디바이스 저장:', deviceData);

      let response;
      
      if (modalMode === 'create') {
        response = await DeviceApiService.createDevice({
          name: deviceData.name,
          protocol_id: deviceData.protocol_id, // 수정: protocol_type → protocol_id
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
          protocol_id: deviceData.protocol_id, // 수정: protocol_type → protocol_id
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
      setError(err instanceof Error ? err.message : '저장 실패');
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
      setError(err instanceof Error ? err.message : '삭제 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // =============================================================================
  // 기타 이벤트 핸들러들 - 그대로 유지
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

  // =============================================================================
  // 라이프사이클 hooks - 무한호출 방지
  // =============================================================================

  // 초기 로딩 (한 번만)
  useEffect(() => {
    console.log('🚀 DeviceList 초기 로딩');
    loadDevices();
    loadAvailableProtocols();
  }, []); // 빈 배열로 한 번만 실행

  // 디바이스 목록이 변경될 때마다 통계 업데이트
  useEffect(() => {
    if (devices.length > 0) {
      loadDeviceStats();
    }
  }, [devices.length, loadDeviceStats]); // devices 전체가 아닌 length만 의존

  // 필터 변경 시 데이터 다시 로드
  useEffect(() => {
    if (hasInitialLoad) {
      console.log('🔄 필터 변경으로 인한 재로드');
      loadDevices(true);
    }
  }, [pagination.currentPage, pagination.pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm, hasInitialLoad]);

  // 자동 새로고침 (부드러운 백그라운드 업데이트) - cleanup 추가
  useEffect(() => {
    if (!autoRefresh || !hasInitialLoad) {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
      return;
    }

    console.log('⏰ 자동 새로고침 타이머 시작 (30초)');
    
    autoRefreshRef.current = setInterval(() => {
      console.log('🔄 자동 새로고침 (백그라운드)');
      loadDevices(true);
      loadDeviceStats();
    }, 30000); // 30초로 늘림

    return () => {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
    };
  }, [autoRefresh, hasInitialLoad]); // loadDevices, loadDeviceStats 제거

  // 컴포넌트 언마운트 시 정리
  useEffect(() => {
    return () => {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
      }
    };
  }, []);

  // =============================================================================
  // 스타일링 함수들 - 그대로 유지
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
  // UI 렌더링 - 모든 UI 컴포넌트 + 새로운 버튼들
  // =============================================================================

  return (
    <div className="device-list-container" ref={containerRef}>
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

        {/* 🔥 일괄 작업 버튼들 - 확장된 버전 */}
        {selectedDevices.length > 0 && (
          <div className="bulk-actions">
            <span className="selected-count">
              {selectedDevices.length}개 선택됨
            </span>
            
            {/* 기존 DB 레벨 일괄 작업 */}
            <div className="bulk-group">
              <label className="bulk-group-label">DB 설정:</label>
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
            
            {/* 🔥 새로운 워커 제어 일괄 작업 */}
            <div className="bulk-group">
              <label className="bulk-group-label">워커 제어:</label>
              <button 
                onClick={() => handleBulkWorkerAction('start')}
                disabled={isProcessing}
                className="btn btn-sm btn-primary"
              >
                <i className="fas fa-rocket"></i>
                일괄 시작
              </button>
              <button 
                onClick={() => handleBulkWorkerAction('stop')}
                disabled={isProcessing}
                className="btn btn-sm btn-danger"
              >
                <i className="fas fa-stop"></i>
                일괄 정지
              </button>
              <button 
                onClick={() => handleBulkWorkerAction('restart')}
                disabled={isProcessing}
                className="btn btn-sm btn-secondary"
              >
                <i className="fas fa-redo"></i>
                일괄 재시작
              </button>
            </div>
            
            {/* 🔥 새로운 설정 관리 일괄 작업 */}
            <div className="bulk-group">
              <label className="bulk-group-label">설정:</label>
              <button 
                onClick={() => handleBulkConfigAction('reload')}
                disabled={isProcessing}
                className="btn btn-sm btn-info"
              >
                <i className="fas fa-sync-alt"></i>
                설정 재로드
              </button>
              <button 
                onClick={() => handleBulkConfigAction('test')}
                disabled={isProcessing}
                className="btn btn-sm btn-outline"
              >
                <i className="fas fa-plug"></i>
                연결 테스트
              </button>
            </div>
            
            {/* 선택 해제 버튼 */}
            <button 
              onClick={() => setSelectedDevices([])}
              className="btn btn-sm btn-ghost"
              title="선택 해제"
            >
              <i className="fas fa-times"></i>
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
            {/* 헤더 - 체크박스 포함 */}
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

                  {/* 디바이스 정보 */}
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
                        {device.description && (
                          <div className="device-endpoint">{device.description}</div>
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

                  {/* 🔥 작업 버튼들 - 완전히 새로운 확장 버전 */}
                  <div className="device-table-cell">
                    <div className="device-actions">
                      
                      {/* 🔥 기존 버튼들 - 패턴 유지 */}
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
                      
                      {/* 🔥 새로운 구분선 */}
                      <div className="action-divider"></div>
                      
                      {/* 🔥 신규 추가: Collector 워커 제어 버튼들 - 상태 기반 동적 표시 */}
                      
                      {/* 워커 상태 표시 */}
                      <div className="worker-status-indicator">
                        <span className={`worker-status-badge ${getWorkerStatusClass(device)}`}>
                          {getWorkerStatusText(device)}
                        </span>
                      </div>
                      
                      {/* 워커 시작 버튼 (정지/오류/알수없음 상태일 때만) */}
                      {shouldShowStartButton(device) && (
                        <button 
                          onClick={() => handleStartWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-start-worker"
                          title="워커 시작"
                        >
                          <i className="fas fa-rocket"></i>
                        </button>
                      )}
                      
                      {/* 워커 정지 버튼 (실행 중이거나 일시정지 중일 때) */}
                      {shouldShowStopButton(device) && (
                        <button 
                          onClick={() => handleStopWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-stop"
                          title="워커 정지"
                        >
                          <i className="fas fa-stop"></i>
                        </button>
                      )}
                      
                      {/* 워커 일시정지 버튼 (실행 중일 때만) */}
                      {shouldShowPauseButton(device) && (
                        <button 
                          onClick={() => handlePauseWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-pause-worker"
                          title="워커 일시정지"
                        >
                          <i className="fas fa-pause-circle"></i>
                        </button>
                      )}
                      
                      {/* 워커 재개 버튼 (일시정지 중일 때만) */}
                      {shouldShowResumeButton(device) && (
                        <button 
                          onClick={() => handleResumeWorker(device.id)}
                          disabled={isProcessing}
                          className="action-btn btn-resume"
                          title="워커 재개"
                        >
                          <i className="fas fa-play-circle"></i>
                        </button>
                      )}
                      
                      {/* 워커 재시작 (항상 표시, 단 정지 중에는 비활성화) */}
                      <button 
                        onClick={() => handleRestartWorker(device.id)}
                        disabled={isProcessing || getWorkerStatus(device) === 'stopped'}
                        className="action-btn btn-restart"
                        title={getWorkerStatus(device) === 'stopped' ? '워커를 먼저 시작하세요' : '워커 재시작'}
                      >
                        <i className="fas fa-redo"></i>
                      </button>
                      
                      {/* 🔥 새로운 구분선 */}
                      <div className="action-divider"></div>
                      
                      {/* 🔥 신규 추가: 유틸리티 버튼들 */}
                      
                      {/* 워커 상태 조회 */}
                      <button 
                        onClick={() => handleCheckWorkerStatus(device.id)}
                        disabled={isProcessing}
                        className="action-btn btn-status"
                        title="워커 상태 확인"
                      >
                        <i className="fas fa-info-circle"></i>
                      </button>
                      
                      {/* 설정 재로드 */}
                      <button 
                        onClick={() => handleReloadDeviceConfig(device.id)}
                        disabled={isProcessing}
                        className="action-btn btn-config"
                        title="설정 재로드"
                      >
                        <i className="fas fa-sync-alt"></i>
                      </button>
                      
                      {/* 드롭다운 메뉴 (더 많은 옵션) */}
                      <div className="dropdown">
                        <button 
                          className="action-btn btn-more"
                          title="더 많은 옵션"
                        >
                          <i className="fas fa-ellipsis-v"></i>
                        </button>
                        <div className="dropdown-menu">
                          <button onClick={() => handleCheckWorkerStatus(device.id)}>
                            <i className="fas fa-info-circle"></i> 워커 상태 상세
                          </button>
                          <button onClick={() => handleReloadDeviceConfig(device.id)}>
                            <i className="fas fa-sync"></i> 설정 동기화
                          </button>
                          <button onClick={() => console.log('실시간 데이터 보기')}>
                            <i className="fas fa-chart-line"></i> 실시간 데이터
                          </button>
                          <button onClick={() => console.log('데이터포인트 관리')}>
                            <i className="fas fa-list"></i> 데이터포인트
                          </button>
                          <div className="dropdown-divider"></div>
                          <button 
                            onClick={() => handleDeleteDevice?.(device.id)}
                            className="danger"
                          >
                            <i className="fas fa-trash"></i> 삭제
                          </button>
                        </div>
                      </div>
                      
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

      {/* 상태바 */}
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

          {isBackgroundRefreshing && (
            <div className="background-refresh">
              <i className="fas fa-sync-alt fa-spin"></i>
              <span>백그라운드 업데이트 중...</span>
            </div>
          )}

          <div className="api-status">
            <span className="api-indicator">
              <i className="fas fa-circle"></i>
              실제 API 데이터
            </span>
          </div>
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