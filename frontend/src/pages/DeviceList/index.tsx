import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useSearchParams } from 'react-router-dom';
import { Pagination } from '../../components/common/Pagination';
import { PageHeader } from '../../components/common/PageHeader';
import { DeviceApiService, Device, DeviceStats as DeviceStatsType } from '../../api/services/deviceApi';
import DeviceDetailModal from '../../components/modals/DeviceDetailModal';
import DeviceTable from './components/DeviceTable';
import { useConfirmContext } from '../../components/common/ConfirmProvider';
import { ManagementLayout } from '../../components/common/ManagementLayout';
import { StatCard } from '../../components/common/StatCard';
import { FilterBar } from '../../components/common/FilterBar';
import '../../styles/management.css';
import '../../styles/device-list.css';
import DeviceTemplateWizard from '../../components/modals/DeviceTemplateWizard';
import GroupSidePanel from './components/GroupSidePanel';
import MoveGroupModal from '../../components/modals/MoveGroupModal';
import NetworkScanModal from '../../components/modals/NetworkScanModal';

const DeviceList: React.FC = () => {
  // URL Params 관리
  const [searchParams, setSearchParams] = useSearchParams();
  const { confirm } = useConfirmContext();

  // 상태 관리
  // 기본 상태들
  const [devices, setDevices] = useState<Device[]>([]);
  const [deviceStats, setDeviceStats] = useState<DeviceStatsType | null>(null);
  const [selectedDevices, setSelectedDevices] = useState<number[]>([]);

  // 강제 리렌더링을 위한 키 추가
  const [renderKey, setRenderKey] = useState(0);

  // 로딩 상태 분리
  const [isInitialLoading, setIsInitialLoading] = useState(true);
  const [isProcessing, setIsProcessing] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // 필터 상태
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [protocolFilter, setProtocolFilter] = useState<string>('all');
  const [connectionFilter, setConnectionFilter] = useState<string>('all');
  const [availableProtocols, setAvailableProtocols] = useState<string[]>([]);

  // 정렬 상태
  const [sortField, setSortField] = useState<string>('name');
  const [sortOrder, setSortOrder] = useState<'ASC' | 'DESC'>('ASC');
  const [includeDeleted, setIncludeDeleted] = useState(false);

  // 실시간 업데이트
  const [autoRefresh, setAutoRefresh] = useState(true);

  // 모달 상태
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null);
  const [modalMode, setModalMode] = useState<'view' | 'edit' | 'create'>('view');
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [isWizardOpen, setIsWizardOpen] = useState(false);
  const [isMoveGroupModalOpen, setIsMoveGroupModalOpen] = useState(false);
  const [isScanModalOpen, setIsScanModalOpen] = useState(false);

  // 페이징 관리
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(10);
  const [totalCount, setTotalCount] = useState(0);

  // 그룹 필터 상태 추가
  const [selectedGroupId, setSelectedGroupId] = useState<number | 'all'>('all');

  // 첫 로딩 완료 여부
  const [hasInitialLoad, setHasInitialLoad] = useState(false);

  // 사이드바 접힘 상태
  const [isSidebarCollapsed, setIsSidebarCollapsed] = useState(true);

  // 자동새로고침 타이머 ref
  const autoRefreshRef = useRef<NodeJS.Timeout | null>(null);

  // 스크롤바 감지를 위한 ref
  const tableBodyRef = useRef<HTMLDivElement>(null);

  // URL 기반 모달 상태 복원 (딥링크 & 새로고침 유지)
  useEffect(() => {
    const deviceIdParam = searchParams.get('deviceId');
    const modeParam = searchParams.get('mode');

    if (modeParam === 'create' && !isModalOpen) {
      // 생성 모드 복원
      setSelectedDevice(null);
      setModalMode('create');
      setIsModalOpen(true);
    } else if (deviceIdParam) {
      const id = parseInt(deviceIdParam, 10);

      // 1. 이미 로드된 리스트에서 찾기
      const found = devices.find(d => d.id === id);
      if (found) {
        // 편집 모드인 경우, 백그라운드 리스트 갱신 때문에 선택된 기기를 덮어쓰지 않음 (수정 중 데이터 손실 방지)
        if (!(isModalOpen && modalMode === 'edit' && selectedDevice?.id === id)) {
          setSelectedDevice(found);
        }
        setModalMode(modeParam === 'edit' ? 'edit' : 'view'); // mode 파라미터가 있으면 반영
        setIsModalOpen(true);
      } else {
        // 2. 리스트에 없으면(페이지 다름 등) 개별 조회
        DeviceApiService.getDevice(id)
          .then(res => {
            if (res.success && res.data) {
              setSelectedDevice(res.data);
              setModalMode(modeParam === 'edit' ? 'edit' : 'view');
              setIsModalOpen(true);
            }
          })
          .catch(err => console.error('URL 디바이스 로드 실패:', err));
      }
    } else if (!deviceIdParam && !modeParam && isModalOpen) {
      // 뒤로가기 등으로 URL 파라미터가 사라지면 모달 닫기
      setIsModalOpen(false);
      setSelectedDevice(null);
    }
  }, [searchParams, devices, isModalOpen]);


  // 정렬 핸들러
  const handleSort = useCallback((field: string) => {
    console.log(`Sorting by ${field}...`);
    if (sortField === field) {
      setSortOrder(prev => prev === 'ASC' ? 'DESC' : 'ASC');
    } else {
      setSortField(field);
      setSortOrder('ASC');
    }
    setCurrentPage(1);
  }, [sortField]);

  // 정렬 아이콘 표시
  const getSortIcon = (field: string) => {
    if (sortField !== field) {
      return <i className="fas fa-sort" style={{ opacity: 0.3, marginLeft: '4px', fontSize: '10px' }}></i>;
    }
    return sortOrder === 'ASC'
      ? <i className="fas fa-sort-up" style={{ color: '#3b82f6', marginLeft: '4px', fontSize: '10px' }}></i>
      : <i className="fas fa-sort-down" style={{ color: '#3b82f6', marginLeft: '4px', fontSize: '10px' }}></i>;
  };

  // 정렬 가능한 헤더 스타일
  const getSortableHeaderStyle = (field: string) => ({
    cursor: 'pointer',
    userSelect: 'none' as const,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'flex-start', // Forced left-align for all headers
    padding: '4px 8px',
    borderRadius: '4px',
    transition: 'background-color 0.2s ease',
    backgroundColor: sortField === field ? 'rgba(59, 130, 246, 0.1)' : 'transparent',
  });

  const loadDevices = useCallback(async (isBackground = false) => {
    try {
      if (!isBackground && !hasInitialLoad) {
        setIsInitialLoading(true);
      }

      setError(null);

      const apiParams = {
        page: currentPage,
        limit: pageSize,
        protocol_type: protocolFilter !== 'all' ? protocolFilter : undefined,
        connection_status: connectionFilter !== 'all' ? connectionFilter : undefined,
        search: searchTerm || undefined,
        sort_by: sortField,
        sort_order: sortOrder,
        include_collector_status: true,
        device_group_id: selectedGroupId === 'all' ? undefined : selectedGroupId,
        onlyDeleted: includeDeleted,
        includeCount: true // backend standard for returning total
      };

      const response = await DeviceApiService.getDevices(apiParams);

      if (response.success && response.data) {
        // 백엔드에서 페이징 처리된 결과를 반환함
        const { items, pagination } = response.data;

        let processedItems = items || [];

        // collector_status 필터링 (아직 백엔드에서 지원하지 않는 경우 클라이언트에서 수행)
        // 만약 데이터가 너무 많다면 이 부분도 백엔드 필터 레이어로 옮겨야 함
        if (statusFilter !== 'all') {
          processedItems = processedItems.filter(device => {
            const statusValue = (device.collector_status?.status || 'unknown').toLowerCase();
            return statusValue === statusFilter;
          });
        }

        setDevices(processedItems);

        if (pagination) {
          setTotalCount(pagination.total || 0);
        } else {
          // Fallback if pagination object is missing
          setTotalCount(processedItems.length);
        }

        setRenderKey(prev => prev + 1);

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
      setTotalCount(0);
    } finally {
      setIsInitialLoading(false);
    }
  }, [currentPage, pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm, sortField, sortOrder, hasInitialLoad, selectedGroupId, includeDeleted]);

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

  // 모달 콜백 함수들
  const handleDeviceSave = useCallback(async () => {
    await loadDevices(true);
    await loadDeviceStats();
  }, [loadDevices, loadDeviceStats]);

  const handleDeviceDelete = useCallback(async (deletedDeviceId: number) => {
    setSelectedDevices(prev => prev.filter(id => id !== deletedDeviceId));
    await loadDevices(true);
    await loadDeviceStats();
  }, [loadDevices, loadDeviceStats]);

  const handleCloseModal = useCallback(() => {
    // 로컬 상태를 직접 변경하지 않고 URL 파라미터만 제거
    // useEffect에서 URL 변경을 감지하여 모달을 닫음 (경쟁 조건 해결)
    setSearchParams(prev => {
      const newParams = new URLSearchParams(prev);
      newParams.delete('deviceId');
      newParams.delete('tab');
      newParams.delete('mode');
      newParams.delete('bulk');
      return newParams;
    }, { replace: true });
  }, [setSearchParams]);

  // 워커 제어 함수들
  const handleStartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const confirmed = await confirm({
      title: '워커 시작 확인',
      message: `워커를 시작하시겠습니까?\n\n디바이스: ${device?.name || deviceId}`,
      confirmText: '시작',
      confirmButtonType: 'primary'
    });

    if (confirmed) {
      try {
        setIsProcessing(true);
        const response = await DeviceApiService.startDeviceWorker(deviceId);
        if (response.success) {
          await loadDevices(true);
          // Optional: Add success message here if desired
        } else {
          throw new Error(response.error || '워커 시작 실패');
        }
      } catch (err: any) {
        setIsProcessing(false);
        await confirm({
          title: '워커 시작 실패',
          message: `오류가 발생했습니다: ${err.message}`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      } finally {
        setIsProcessing(false);
      }
    }
  };

  const handleStopWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const confirmed = await confirm({
      title: '워커 정지 확인',
      message: `워커를 정지하시겠습니까?\n\n디바이스: ${device?.name || deviceId}`,
      confirmText: '정지',
      confirmButtonType: 'danger'
    });

    if (confirmed) {
      try {
        setIsProcessing(true);
        const response = await DeviceApiService.stopDeviceWorker(deviceId, { graceful: true });
        if (response.success) {
          await loadDevices(true);
        } else {
          throw new Error(response.error || '워커 정지 실패');
        }
      } catch (err: any) {
        setIsProcessing(false);
        await confirm({
          title: '워커 정지 실패',
          message: `오류가 발생했습니다: ${err.message}`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      } finally {
        setIsProcessing(false);
      }
    }
  };

  const handleRestartWorker = async (deviceId: number) => {
    const device = devices.find(d => d.id === deviceId);
    const confirmed = await confirm({
      title: '워커 재시작 확인',
      message: `워커를 재시작하시겠습니까?\n\n디바이스: ${device?.name || deviceId}`,
      confirmText: '재시작',
      confirmButtonType: 'warning'
    });

    if (confirmed) {
      try {
        setIsProcessing(true);
        const response = await DeviceApiService.restartDeviceWorker(deviceId);
        if (response.success) {
          await loadDevices(true);
        } else {
          throw new Error(response.error || '워커 재시작 실패');
        }
      } catch (err: any) {
        setIsProcessing(false);
        await confirm({
          title: '워커 재시작 실패',
          message: `오류가 발생했습니다: ${err.message}`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      } finally {
        setIsProcessing(false);
      }
    }
  };

  const handleRestoreDevice = async (device: Device) => {
    const confirmed = await confirm({
      title: '디바이스 복구 확인',
      message: `장치명: ${device.name}\n모델: ${device.model || 'N/A'}\n제조사: ${device.manufacturer || 'N/A'}\n종류: ${device.device_type || 'N/A'}\n\n이 디바이스를 정말로 복구하시겠습니까?`,
      confirmText: '복구',
      confirmButtonType: 'primary'
    });

    if (confirmed) {
      try {
        setIsProcessing(true);
        const response = await DeviceApiService.restoreDevice(device.id);
        if (response.success) {
          setIncludeDeleted(false);
          await loadDevices(true);
          await loadDeviceStats();

          setIsProcessing(false); // Hide spinner
          // 성공 알림 추가
          await confirm({
            title: '복구 완료',
            message: `"${device.name}" 디바이스가 성공적으로 복구되었습니다.`,
            confirmText: '확인',
            showCancelButton: false,
            confirmButtonType: 'success'
          });
        } else {
          throw new Error(response.error || '복구 실패');
        }
      } catch (err: any) {
        setIsProcessing(false);
        await confirm({
          title: '복구 실패',
          message: `오류가 발생했습니다: ${err.message}`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      } finally {
        setIsProcessing(false);
      }
    }
  };

  // 이벤트 핸들러들
  const handleDeviceSelect = useCallback((deviceId: number, selected: boolean) => {
    setSelectedDevices(prev =>
      selected
        ? [...prev, deviceId]
        : prev.filter(id => id !== deviceId)
    );
  }, []);

  const handleSelectAll = useCallback((selected: boolean) => {
    setSelectedDevices(selected ? devices.map(d => d.id) : []);
  }, [devices]);

  const handleDeviceClick = useCallback((device: Device) => {
    // Navigate only - let useEffect handle state
    setSearchParams(prev => {
      const newParams = new URLSearchParams(prev);
      newParams.set('deviceId', device.id.toString());
      newParams.set('mode', 'view'); // 명시적으로 view 모드 설정
      if (!newParams.get('tab')) {
        newParams.set('tab', 'basic');
      }
      return newParams;
    });
  }, [setSearchParams]);

  const handleEditDevice = useCallback((device: Device) => {
    // Navigate only - let useEffect handle state
    setSearchParams(prev => {
      const newParams = new URLSearchParams(prev);
      newParams.set('deviceId', device.id.toString());
      if (!newParams.get('tab')) {
        newParams.set('tab', 'basic');
      }
      return newParams;
    });
  }, [setSearchParams]);

  const handleBulkMoveToGroup = async (targetGroupId: number | null) => {
    if (selectedDevices.length === 0) return;

    try {
      setIsProcessing(true);
      const moveCount = selectedDevices.length;
      const response = await DeviceApiService.bulkUpdateDevices(selectedDevices, {
        device_group_id: targetGroupId as any // null works for clearing
      });

      if (response.success) {
        setIsMoveGroupModalOpen(false);
        setSelectedDevices([]);
        await loadDevices(true);
        await loadDeviceStats();

        setIsProcessing(false);
        // 성공 알림 추가
        await confirm({
          title: '그룹 이동 완료',
          message: `${moveCount}개의 디바이스가 새로운 그룹으로 이동되었습니다.`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'success'
        });
      } else {
        setIsProcessing(false);
        await confirm({
          title: '그룹 이동 실패',
          message: `오류가 발생했습니다: ${response.error || '알 수 없는 오류'}`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      }
    } catch (err: any) {
      setIsProcessing(false);
      await confirm({
        title: '오류 발생',
        message: err.message,
        confirmText: '확인',
        showCancelButton: false,
        confirmButtonType: 'danger'
      });
    } finally {
      setIsProcessing(false);
    }
  };

  const handleBulkDelete = async () => {
    if (selectedDevices.length === 0) return;

    const confirmed = await confirm({
      title: '대량 삭제 확인',
      message: `선택한 ${selectedDevices.length}개의 디바이스를 정말 삭제하시겠습니까?`,
      confirmText: '삭제',
      confirmButtonType: 'danger'
    });

    if (confirmed) {
      try {
        setIsProcessing(true);
        const deletedCount = selectedDevices.length;
        const response = await DeviceApiService.bulkDeleteDevices(selectedDevices);

        if (response.success) {
          setSelectedDevices([]);
          await loadDevices(true);
          await loadDeviceStats();

          setIsProcessing(false);
          // 성공 알림 추가
          await confirm({
            title: '삭제 완료',
            message: `${deletedCount}개의 디바이스가 성공적으로 삭제되었습니다.`,
            confirmText: '확인',
            showCancelButton: false,
            confirmButtonType: 'success'
          });
        } else {
          setIsProcessing(false);
          await confirm({
            title: '장치 삭제 실패',
            message: `오류가 발생했습니다: ${response.error || '알 수 없는 오류'}`,
            confirmText: '확인',
            showCancelButton: false,
            confirmButtonType: 'danger'
          });
        }
      } catch (err: any) {
        setIsProcessing(false);
        await confirm({
          title: '오류 발생',
          message: err.message,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      } finally {
        setIsProcessing(false);
      }
    }
  };

  const handleCreateDevice = useCallback(() => {
    // Navigate only - let useEffect handle state
    setSearchParams(prev => {
      const newParams = new URLSearchParams(prev);
      newParams.set('mode', 'create');
      newParams.set('tab', 'basic');
      newParams.delete('deviceId'); // ID는 없으므로 제거
      return newParams;
    }, { replace: true });
  }, [setSearchParams]);

  // 라이프사이클 hooks
  useEffect(() => {
    loadDevices();
    loadAvailableProtocols();
  }, []);

  useEffect(() => {
    if (devices.length >= 0) {
      loadDeviceStats();
    }
  }, [devices.length]);

  useEffect(() => {
    if (hasInitialLoad) {
      loadDevices(true);
    }
  }, [currentPage, pageSize, protocolFilter, connectionFilter, statusFilter, searchTerm, sortField, sortOrder, hasInitialLoad, selectedGroupId, includeDeleted]);

  useEffect(() => {
    if (!autoRefresh || !hasInitialLoad || isModalOpen) {
      if (autoRefreshRef.current) {
        clearInterval(autoRefreshRef.current);
        autoRefreshRef.current = null;
      }
      return;
    }

    const intervalId = setInterval(() => {
      loadDevices(true);
    }, 10000);

    autoRefreshRef.current = intervalId;

    return () => clearInterval(intervalId);
  }, [autoRefresh, hasInitialLoad, isModalOpen, loadDevices]);

  return (
    <>
      <ManagementLayout className="page-devices">
        <PageHeader
          title="디바이스 관리"
          description="산업 현장의 디바이스를 마스터 모델 기반으로 관리하고 모니터링합니다."
          icon="fas fa-network-wired"
          actions={
            <div style={{ display: 'flex', gap: '8px' }}>
              <button className="mgmt-btn mgmt-btn-outline primary" onClick={() => setIsWizardOpen(true)} disabled={isProcessing}>
                <i className="fas fa-magic"></i> 마스터 모델로 추가
              </button>
              <button className="mgmt-btn mgmt-btn-outline" onClick={() => setIsScanModalOpen(true)} disabled={isProcessing}>
                <i className="fas fa-search-location"></i> 네트워크 스캔
              </button>
              <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreateDevice} disabled={isProcessing}>
                <i className="fas fa-plus"></i> 수동 등록
              </button>
            </div>
          }
        />

        {deviceStats && (
          <div className="mgmt-stats-panel">
            <StatCard title="전체 디바이스" value={deviceStats.total_devices || 0} icon="fas fa-network-wired" type="primary" />
            <StatCard title="활성" value={deviceStats.active_devices || 0} icon="fas fa-check-circle" type="success" />
            <StatCard title="사용 중" value={deviceStats.enabled_devices || 0} icon="fas fa-toggle-on" type="neutral" />
            <StatCard title="프로토콜 수" value={Object.keys(deviceStats.by_protocol || {}).length} icon="fas fa-plug" type="warning" />
          </div>
        )}

        <FilterBar
          searchTerm={searchTerm}
          onSearchChange={setSearchTerm}
          onReset={() => {
            setSearchTerm('');
            setStatusFilter('all');
            setProtocolFilter('all');
            setConnectionFilter('all');
            setIncludeDeleted(false);
          }}
          activeFilterCount={(searchTerm ? 1 : 0) + (statusFilter !== 'all' ? 1 : 0) + (protocolFilter !== 'all' ? 1 : 0) + (connectionFilter !== 'all' ? 1 : 0) + (includeDeleted ? 1 : 0)}
          filters={[
            {
              label: '상태',
              value: statusFilter,
              onChange: setStatusFilter,
              options: [
                { value: 'all', label: '전체 상태' },
                { value: 'online', label: '온라인' },
                { value: 'offline', label: '오프라인' },
                { value: 'error', label: '오류' }
              ]
            },
            {
              label: '프로토콜',
              value: protocolFilter,
              onChange: setProtocolFilter,
              options: [
                { value: 'all', label: '전체 프로토콜' },
                ...availableProtocols.map(p => ({ value: p, label: p }))
              ]
            },
            {
              label: '연결 확인',
              value: connectionFilter,
              onChange: setConnectionFilter,
              options: [
                { value: 'all', label: '전체' },
                { value: 'connected', label: '연결됨' },
                { value: 'disconnected', label: '끊김' }
              ]
            }
          ]}
          rightActions={
            <div className="mgmt-filter-group" style={{ flexDirection: 'row', alignItems: 'center', height: '100%', paddingBottom: '2px' }}>
              <label className="mgmt-checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', marginBottom: 0 }}>
                <input
                  type="checkbox"
                  checked={includeDeleted}
                  onChange={(e) => {
                    setIncludeDeleted(e.target.checked);
                    setCurrentPage(1);
                  }}
                />
                <span style={{ fontSize: '13px', fontWeight: 500, color: 'var(--neutral-700)' }}>삭제된 디바이스 보기</span>
              </label>
            </div>
          }
        />

        <div className={`device-list-main-layout ${isSidebarCollapsed ? 'sidebar-collapsed' : ''}`}>
          <GroupSidePanel
            selectedGroupId={selectedGroupId}
            onGroupSelect={(id) => {
              setSelectedGroupId(id);
              setCurrentPage(1);
            }}
            selectedDevicesCount={selectedDevices.length}
            onMoveSelectedToGroup={(groupId) => {
              if (window.confirm(`${selectedDevices.length}개의 장치를 이 그룹으로 이동하시겠습니까?`)) {
                handleBulkMoveToGroup(groupId);
              }
            }}
            isCollapsed={isSidebarCollapsed}
            onToggleCollapse={() => setIsSidebarCollapsed(prev => !prev)}
          />

          <div className="device-list-table-wrapper" style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
            {error && (
              <div className="device-list-error" style={{ margin: 'var(--space-4)' }}>
                <i className="fas fa-exclamation-circle"></i> {error}
                <button onClick={() => setError(null)}><i className="fas fa-times"></i></button>
              </div>
            )}

            {isInitialLoading ? (
              <div className="device-list-loading">
                <i className="fas fa-spinner fa-spin"></i>
                <span>디바이스 목록을 불러오는 중...</span>
              </div>
            ) : devices.length === 0 ? (
              <div className="device-list-empty">
                <i className="fas fa-network-wired"></i>
                <h3>등록된 디바이스가 없습니다</h3>
                <p>{selectedGroupId === 'all' ? '새 디바이스를 추가하여 시작하세요' : '이 그룹에 등록된 장치가 없습니다'}</p>
                {selectedGroupId === 'all' && (
                  <button onClick={handleCreateDevice}><i className="fas fa-plus"></i> 첫 번째 디바이스 추가</button>
                )}
              </div>
            ) : (
              <>
                <div className="device-list-table-container">
                  <DeviceTable
                    devices={devices}
                    selectedDevices={selectedDevices}
                    onDeviceSelect={handleDeviceSelect}
                    onSelectAll={handleSelectAll}
                    onDeviceClick={handleDeviceClick}
                    onEditDevice={handleEditDevice}
                    onStartWorker={handleStartWorker}
                    onStopWorker={handleStopWorker}
                    onRestartWorker={handleRestartWorker}
                    onRestoreDevice={handleRestoreDevice}
                    onMoveDevice={(device) => {
                      setSelectedDevices([device.id]);
                      setIsMoveGroupModalOpen(true);
                    }}
                    sortField={sortField}
                    sortOrder={sortOrder}
                    onSort={handleSort}
                    getSortIcon={getSortIcon}
                    getSortableHeaderStyle={getSortableHeaderStyle}
                    tableBodyRef={tableBodyRef}
                    renderKey={renderKey}
                  />
                </div>

                <Pagination
                  current={currentPage}
                  total={totalCount}
                  pageSize={pageSize}
                  onChange={(page) => setCurrentPage(page)}
                  onShowSizeChange={(_, size) => {
                    setPageSize(size);
                    setCurrentPage(1);
                  }}
                />
              </>
            )}
          </div>
        </div>

        {/* 대량 작업 바 */}
        {selectedDevices.length > 0 && (
          <div className="bulk-action-bar active">
            <div className="bulk-info">
              <span className="count">{selectedDevices.length}</span>
              <span>개의 디바이스 선택됨</span>
            </div>
            <div className="bulk-actions">
              <button className="btn-bulk" onClick={() => setIsMoveGroupModalOpen(true)}>
                <i className="fas fa-exchange-alt"></i> 그룹 이동
              </button>
              <button className="btn-bulk danger" onClick={handleBulkDelete}>
                <i className="fas fa-trash"></i> 삭제
              </button>
              <div className="separator"></div>
              <button className="btn-close-bulk" onClick={() => setSelectedDevices([])}>
                <i className="fas fa-times"></i>
              </button>
            </div>
          </div>
        )}

        {isScanModalOpen && (
          <NetworkScanModal
            isOpen={isScanModalOpen}
            onClose={() => setIsScanModalOpen(false)}
            onSuccess={() => {
              loadDevices(true);
            }}
          />
        )}

        {isModalOpen && (
          <DeviceDetailModal
            isOpen={isModalOpen}
            onClose={handleCloseModal}
            onSave={handleDeviceSave}
            onDelete={handleDeviceDelete}
            onEdit={() => {
              setSearchParams(prev => {
                const newParams = new URLSearchParams(prev);
                newParams.set('mode', 'edit');
                return newParams;
              });
            }}
            device={selectedDevice as any}
            mode={modalMode}
            initialTab={searchParams.get('tab') || undefined}
            onTabChange={(tab) => {
              setSearchParams(prev => {
                const newParams = new URLSearchParams(prev);
                newParams.set('tab', tab);
                return newParams;
              }, { replace: true });
            }}
          />
        )}

        <DeviceTemplateWizard
          isOpen={isWizardOpen}
          onClose={() => setIsWizardOpen(false)}
          onSuccess={(deviceId) => {
            loadDevices(true);
            loadDeviceStats();
            // 새로 생성된 디바이스로 상세 이동 가능
            setSearchParams(prev => {
              const newParams = new URLSearchParams(prev);
              newParams.set('deviceId', deviceId.toString());
              newParams.set('mode', 'view');
              newParams.set('tab', 'basic');
              return newParams;
            });
          }}
        />

        {isMoveGroupModalOpen && (
          <MoveGroupModal
            selectedDevices={devices.filter(d => selectedDevices.includes(d.id))}
            loading={isProcessing}
            onClose={() => setIsMoveGroupModalOpen(false)}
            onConfirm={handleBulkMoveToGroup}
          />
        )}

      </ManagementLayout>

      {/* 처리 중 오버레이 */}
      {isProcessing && (
        <div className="processing-overlay">
          <div className="processing-content">
            <i className="fas fa-spinner fa-spin"></i>
            <span>처리 중...</span>
          </div>
        </div>
      )}
    </>
  );
};

export default DeviceList;