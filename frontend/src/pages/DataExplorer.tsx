import React, { useState, useEffect, useCallback, useMemo } from 'react';
import {
  RealtimeApiService,
  RealtimeValue,
  DevicesResponse,
  CurrentValuesResponse,
  ApiResponse
} from '../api/services/realtimeApi';
import { 
  DeviceApiService, 
  Device 
} from '../api/services/deviceApi';
import '../styles/data-explorer.css';

// ============================================================================
// 🔥 완성된 PulseOne 실시간 데이터 탐색기 - 반응형 버전
// ============================================================================

interface TreeNode {
  id: string;
  label: string;
  type: 'tenant' | 'site' | 'device' | 'datapoint';
  level: number;
  isExpanded: boolean;
  isLoaded: boolean;
  children?: TreeNode[];
  childCount?: number;
  dataPoint?: RealtimeValue;
  deviceInfo?: DeviceInfo;
  lastUpdate?: string;
  connectionStatus?: 'connected' | 'disconnected' | 'error';
}

interface DeviceInfo {
  device_id: string;
  device_name: string;
  device_type?: string;
  point_count: number;
  status: string;
  last_seen?: string;
}

interface FilterState {
  search: string;
  dataType: string;
  quality: string;
  device: string;
}

const DataExplorer: React.FC = () => {
  // ========================================================================
  // 상태 관리 (기존과 동일)
  // ========================================================================
  
  const [treeData, setTreeData] = useState<TreeNode[]>([]);
  const [selectedNode, setSelectedNode] = useState<TreeNode | null>(null);
  const [selectedDataPoints, setSelectedDataPoints] = useState<RealtimeValue[]>([]);
  const [realtimeData, setRealtimeData] = useState<RealtimeValue[]>([]);
  const [devices, setDevices] = useState<DeviceInfo[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'connecting' | 'disconnected'>('disconnected');
  const [lastRefresh, setLastRefresh] = useState<Date>(new Date());
  const [filters, setFilters] = useState<FilterState>({
    search: '',
    dataType: 'all',
    quality: 'all',
    device: 'all'
  });
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(3);
  const [showChart, setShowChart] = useState(false);

  // ========================================================================
  // 🔥 API 서비스 연동 (기존과 동일)
  // ========================================================================

  const loadDevices = useCallback(async () => {
    try {
      console.log('🔄 데이터베이스에서 디바이스 목록 로드 시작...');
      
      const response: ApiResponse<any> = await DeviceApiService.getDevices({
        page: 1,
        limit: 1000,
        sort_by: 'name',
        sort_order: 'ASC'
      });

      if (response.success && response.data?.items) {
        const deviceList: DeviceInfo[] = response.data.items.map((device: Device) => ({
          device_id: device.id.toString(),
          device_name: device.name,
          device_type: device.device_type || 'Unknown',
          point_count: device.data_point_count || device.data_points_count || 0,
          status: device.status || 'unknown',
          last_seen: device.last_seen
        }));
        
        setDevices(deviceList);
        console.log(`✅ 데이터베이스에서 ${deviceList.length}개 디바이스 로드 완료:`, deviceList);
        
        return deviceList;
      } else {
        console.warn('⚠️ 디바이스 API 응답 이상:', response);
        throw new Error(response.error || '디바이스 목록 조회 실패');
      }
    } catch (error: any) {
      console.error('❌ 데이터베이스 디바이스 로드 실패:', error);
      throw error;
    }
  }, []);

  const loadRealtimeData = useCallback(async (deviceList?: DeviceInfo[]) => {
    try {
      console.log('🔄 Redis에서 실시간 데이터 로드 시작...');
      
      let response;
      
      if (deviceList && deviceList.length > 0) {
        const deviceIds = deviceList.map(d => d.device_id);
        
        response = await RealtimeApiService.getCurrentValues({
          device_ids: deviceIds,
          quality_filter: 'all',
          limit: 5000
        });
      } else {
        response = await RealtimeApiService.getCurrentValues({
          quality_filter: 'all',
          limit: 5000
        });
      }

      if (response.success && response.data?.current_values) {
        setRealtimeData(response.data.current_values);
        console.log(`✅ Redis에서 ${response.data.current_values.length}개 실시간 값 로드 완료`);
        return response.data.current_values;
      } else {
        console.warn('⚠️ 실시간 데이터 로드 실패:', response);
        setRealtimeData([]);
        return [];
      }
    } catch (error: any) {
      console.error('❌ Redis 실시간 데이터 로드 실패:', error);
      setRealtimeData([]);
      return [];
    }
  }, []);

  const loadDeviceData = useCallback(async (deviceId: string) => {
    try {
      console.log(`🔄 디바이스 ${deviceId} Redis 데이터 확인...`);
      
      const response = await RealtimeApiService.getDeviceValues(deviceId);
      
      if (response.success && response.data?.data_points) {
        const dataPoints: RealtimeValue[] = response.data.data_points;
        console.log(`✅ 디바이스 ${deviceId}: Redis에서 ${dataPoints.length}개 포인트 발견`);
        return dataPoints;
      } else {
        console.warn(`⚠️ 디바이스 ${deviceId} Redis에 데이터 없음`);
        return [];
      }
    } catch (error: any) {
      console.error(`❌ 디바이스 ${deviceId} Redis 확인 실패:`, error);
      return [];
    }
  }, []);

  const generateTreeData = useCallback(async (devices: DeviceInfo[]): Promise<TreeNode[]> => {
    if (!devices || devices.length === 0) {
      return [{
        id: 'tenant-1',
        label: 'PulseOne Factory (데이터 없음)',
        type: 'tenant',
        level: 0,
        isExpanded: true,
        isLoaded: true,
        children: []
      }];
    }

    const deviceNodesPromises = devices.map(async (device) => {
      let connectionStatus: 'connected' | 'disconnected' | 'error' = 'disconnected';
      let realDataCount = 0;

      try {
        const deviceValues = await loadDeviceData(device.device_id);
        if (deviceValues.length > 0) {
          connectionStatus = 'connected';
          realDataCount = deviceValues.length;
        }
      } catch (error) {
        console.warn(`⚠️ 디바이스 ${device.device_id} 연결 상태 확인 실패:`, error);
        connectionStatus = 'error';
      }

      const pointCount = connectionStatus === 'connected' ? realDataCount : device.point_count;

      return {
        id: `device-${device.device_id}`,
        label: `${device.device_name} (포인트: ${pointCount})`,
        type: 'device' as const,
        level: 2,
        isExpanded: false,
        isLoaded: false,
        deviceInfo: device,
        connectionStatus,
        lastUpdate: device.last_seen,
        childCount: pointCount
      };
    });

    const deviceNodes = await Promise.all(deviceNodesPromises);

    return [{
      id: 'tenant-1',
      label: 'PulseOne Factory',
      type: 'tenant',
      level: 0,
      isExpanded: true,
      isLoaded: true,
      children: [{
        id: 'site-1',
        label: 'Factory A - Production Line',
        type: 'site',
        level: 1,
        isExpanded: true,
        isLoaded: true,
        children: deviceNodes
      }]
    }];
  }, [loadDeviceData]);

  const loadDeviceChildren = useCallback(async (deviceNode: TreeNode) => {
    if (deviceNode.type !== 'device') return;
    
    const deviceId = deviceNode.id.replace('device-', '');
    console.log(`🔄 디바이스 ${deviceId} 자식 노드 로드...`);
    
    try {
      const dataPoints = await loadDeviceData(deviceId);
      
      if (dataPoints.length === 0) {
        console.log(`⚠️ 디바이스 ${deviceId}: Redis에 데이터 없음 - 트리 확장 안함`);
        
        setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
          isLoaded: true,
          isExpanded: false,
          childCount: 0
        }));
        
        return;
      }
      
      const pointNodes: TreeNode[] = dataPoints.map((point: any, index: number) => ({
        id: `${deviceNode.id}-point-${point.point_id}`,
        label: point.point_name,
        type: 'datapoint',
        level: deviceNode.level + 1,
        isExpanded: false,
        isLoaded: true,
        dataPoint: point
      }));

      setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
        children: pointNodes,
        isLoaded: true,
        isExpanded: true,
        childCount: pointNodes.length
      }));

      console.log(`✅ 디바이스 ${deviceId} 자식 노드 ${pointNodes.length}개 로드 완료`);

    } catch (error) {
      console.error(`❌ 디바이스 ${deviceId} 자식 노드 로드 실패:`, error);
      
      setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
        children: [],
        isLoaded: true,
        isExpanded: false,
        childCount: 0
      }));
    }
  }, [loadDeviceData]);

  const renderEmptyDeviceMessage = (selectedNode: TreeNode | null) => {
    if (!selectedNode || selectedNode.type !== 'device') return null;
    
    const device = selectedNode.deviceInfo;
    const connectionStatus = selectedNode.connectionStatus;
    
    return (
      <div className="empty-state">
        <div style={{fontSize: '48px', marginBottom: '16px'}}>
          {connectionStatus === 'disconnected' ? '🔴' : '⚠️'}
        </div>
        <h3 style={{margin: '0 0 8px 0', fontSize: '18px', color: '#374151'}}>
          {device?.device_name} 연결 안됨
        </h3>
        <p style={{margin: '0 0 8px 0', fontSize: '14px'}}>
          이 디바이스는 현재 Redis에 실시간 데이터가 없습니다.
        </p>
        <div style={{
          marginTop: '16px',
          padding: '12px 16px',
          backgroundColor: '#f3f4f6',
          borderRadius: '6px',
          fontSize: '12px'
        }}>
          <div><strong>디바이스 정보:</strong></div>
          <div>타입: {device?.device_type || 'Unknown'}</div>
          <div>설정된 포인트: {device?.point_count || 0}개</div>
          <div>마지막 연결: {device?.last_seen || '없음'}</div>
        </div>
        <div style={{
          marginTop: '16px',
          fontSize: '12px',
          color: '#9ca3af'
        }}>
          연결이 복구되면 실시간 데이터가 표시됩니다.
        </div>
      </div>
    );
  };

  const initializeData = useCallback(async () => {
    setIsLoading(true);
    setConnectionStatus('connecting');
    
    try {
      console.log('🚀 데이터 초기화 시작...');
      
      const realtimeDataPoints = await loadRealtimeData();
      const deviceList = await loadDevices();
      const treeStructure = await generateTreeData(deviceList);
      setTreeData(treeStructure);
      
      setConnectionStatus('connected');
      setError(null);
      
      console.log('✅ 데이터 초기화 완료');
      console.log(`📊 DB 디바이스: ${deviceList.length}개, Redis 실시간 포인트: ${realtimeDataPoints.length}개`);
      
    } catch (error: any) {
      console.error('❌ 데이터 초기화 실패:', error);
      setError(`데이터 로드 실패: ${error.message}`);
      setConnectionStatus('disconnected');
      
      setTreeData([{
        id: 'tenant-1',
        label: 'PulseOne Factory (연결 실패)',
        type: 'tenant',
        level: 0,
        isExpanded: true,
        isLoaded: true,
        children: []
      }]);
      
    } finally {
      setIsLoading(false);
    }
  }, [loadRealtimeData, loadDevices, generateTreeData]);

  // ========================================================================
  // 유틸리티 함수들 (기존과 동일)
  // ========================================================================

  const updateTreeNode = (nodes: TreeNode[], nodeId: string, updates: Partial<TreeNode>): TreeNode[] => {
    return nodes.map(node => {
      if (node.id === nodeId) {
        return { ...node, ...updates };
      }
      if (node.children) {
        return { ...node, children: updateTreeNode(node.children, nodeId, updates) };
      }
      return node;
    });
  };

  const findAllDataPoints = (nodes: TreeNode[]): RealtimeValue[] => {
    const dataPoints: RealtimeValue[] = [];
    
    const traverse = (nodeArray: TreeNode[]) => {
      nodeArray.forEach(node => {
        if (node.type === 'datapoint' && node.dataPoint) {
          dataPoints.push(node.dataPoint);
        }
        if (node.children) {
          traverse(node.children);
        }
      });
    };
    
    traverse(nodes);
    return dataPoints;
  };

  // ========================================================================
  // 🔥 필터링된 데이터 (기존과 동일)
  // ========================================================================

  const filteredDataPoints = useMemo(() => {
    if (selectedNode && selectedNode.type === 'device' && 
        (selectedNode.connectionStatus === 'disconnected' || selectedNode.childCount === 0)) {
      console.log('🔍 필터링: 연결 안된 디바이스 선택됨 - 빈 배열 반환');
      return [];
    }

    let points = selectedDataPoints.length > 0 ? selectedDataPoints : realtimeData;
    
    console.log('🔍 필터링 시작:', {
      selectedDataPoints: selectedDataPoints.length,
      realtimeData: realtimeData.length,
      sourcePick: selectedDataPoints.length > 0 ? 'selected' : 'realtime',
      initialPoints: points.length
    });
    
    if (filters.search) {
      const searchTerm = filters.search.toLowerCase();
      points = points.filter((dp: RealtimeValue) => 
        (dp.point_name && dp.point_name.toLowerCase().includes(searchTerm)) ||
        (dp.device_name && dp.device_name.toLowerCase().includes(searchTerm)) ||
        (dp.key && dp.key.toLowerCase().includes(searchTerm))
      );
      console.log(`🔍 검색 필터 "${filters.search}" 적용 후: ${points.length}개`);
    }
    
    if (filters.dataType !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.data_type === filters.dataType);
      console.log(`🔍 데이터타입 필터 "${filters.dataType}" 적용 후: ${points.length}개`);
    }
    
    if (filters.quality !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.quality === filters.quality);
      console.log(`🔍 품질 필터 "${filters.quality}" 적용 후: ${points.length}개`);
    }
    
    if (filters.device !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.device_id === filters.device);
      console.log(`🔍 디바이스 필터 "${filters.device}" 적용 후: ${points.length}개`);
    }
    
    console.log('✅ 필터링 완료:', points.length + '개');
    return points;
  }, [selectedDataPoints, realtimeData, filters, selectedNode]); 

  // ========================================================================
  // 이벤트 핸들러들 (기존과 동일하지만 간소화)
  // ========================================================================

  const handleDataPointSelect = useCallback((dataPoint: RealtimeValue) => {
    setSelectedDataPoints(prev => {
      const exists = prev.find((dp: RealtimeValue) => dp.key === dataPoint.key);
      if (exists) {
        return prev.filter((dp: RealtimeValue) => dp.key !== dataPoint.key);
      } else {
        return [...prev, dataPoint];
      }
    });
  }, []);

  const handleRefresh = useCallback(() => {
    console.log('🔄 수동 새로고침 시작...');
    setLastRefresh(new Date());
    
    if (devices.length > 0) {
      loadRealtimeData(devices);
    } else {
      initializeData();
    }
  }, [devices, loadRealtimeData, initializeData]);

  const handleExportData = useCallback(() => {
    console.log('📥 CSV 데이터 내보내기 시작...');
    
    if (filteredDataPoints.length === 0) {
      console.warn('⚠️ 내보낼 데이터가 없습니다.');
      alert('내보낼 데이터가 없습니다.');
      return;
    }
    
    const csvHeaders = [
      'Device Name (디바이스명)',
      'Point Name (포인트명)', 
      'Point ID (포인트 ID)',
      'Current Value (현재값)',
      'Unit (단위)',
      'Data Type (데이터타입)',
      'Quality (품질)',
      'Timestamp (시간)'
    ];
    
    const csvRows = filteredDataPoints.map((dp) => {
      return [
        `"${dp.device_name || 'Unknown'}"`,
        `"${dp.point_name || 'Unknown'}"`,
        `"${dp.point_id || ''}"`,
        `"${dp.value !== undefined && dp.value !== null ? dp.value : ''}"`,
        `"${dp.unit || ''}"`,
        `"${dp.data_type || 'unknown'}"`,
        `"${dp.quality || 'unknown'}"`,
        `"${dp.timestamp ? new Date(dp.timestamp).toLocaleString('ko-KR') : ''}"`
      ].join(',');
    });
    
    const csvContent = '\uFEFF' + [csvHeaders.join(','), ...csvRows].join('\n');
    
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    
    const link = document.createElement('a');
    link.href = url;
    link.download = `pulseone_realtime_data_${new Date().toISOString().split('T')[0]}.csv`;
    link.style.display = 'none';
    
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
    
    console.log(`✅ ${filteredDataPoints.length}개 데이터 CSV 내보내기 완료`);
    
  }, [filteredDataPoints]);

  const clearSelection = useCallback(() => {
    console.log('🔄 선택 초기화');
    setSelectedDataPoints([]);
    setSelectedNode(null);
  }, []);

  const handleNodeClick = useCallback((node: TreeNode) => {
    setSelectedNode(node);
    
    if (node.type === 'datapoint' && node.dataPoint) {
      setSelectedDataPoints([node.dataPoint]);
      
    } else if (node.type === 'device') {
      const deviceId = node.id.replace('device-', '');
      
      if (node.connectionStatus === 'disconnected' || node.childCount === 0) {
        console.log(`⚠️ 디바이스 ${deviceId}: Redis 데이터 없음 - 강제 초기화`);
        
        setSelectedDataPoints([]);
        setRealtimeData([]);
        
        return;
      }
      
      const existingDataPoints = findAllDataPoints([node]);
      if (existingDataPoints.length > 0) {
        setSelectedDataPoints(existingDataPoints);
      }
      
      loadDeviceData(deviceId).then(dataPoints => {
        if (dataPoints.length > 0) {
          setSelectedDataPoints(dataPoints);
          setRealtimeData(dataPoints);
        }
      });
      
      if (!node.isLoaded && node.childCount && node.childCount > 0) {
        loadDeviceChildren(node);
      } else {
        setTreeData(prev => updateTreeNode(prev, node.id, { 
          isExpanded: !node.isExpanded 
        }));
      }
      
    } else {
      setTreeData(prev => updateTreeNode(prev, node.id, { 
        isExpanded: !node.isExpanded 
      }));
    }
  }, [findAllDataPoints, loadDeviceChildren, loadDeviceData]);

  // ========================================================================
  // 초기 로딩 및 자동 새로고침
  // ========================================================================

  useEffect(() => {
    initializeData();
  }, [initializeData]);

  useEffect(() => {
    if (!autoRefresh || refreshInterval <= 0) return;

    console.log(`🔄 자동 새로고침 설정: ${refreshInterval}초 간격`);
    
    const interval = setInterval(() => {
      console.log('🔄 자동 새로고침 실행...');
      setLastRefresh(new Date());
      
      if (devices.length > 0) {
        loadRealtimeData(devices);
      }
    }, refreshInterval * 1000);

    return () => {
      console.log('🔄 자동 새로고침 정리');
      clearInterval(interval);
    };
  }, [autoRefresh, refreshInterval, devices, loadRealtimeData]);

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================

  const renderTreeNode = (node: TreeNode): React.ReactNode => {
    const hasChildren = (node.children && node.children.length > 0) || (node.childCount && node.childCount > 0);
    const isExpanded = node.isExpanded && node.children;
    
    return (
      <div key={node.id} className="tree-node">
        <div 
          className={`tree-node-content ${selectedNode?.id === node.id ? 'selected' : ''}`}
          onClick={() => handleNodeClick(node)}
        >
          {hasChildren && (
            <span className="tree-expand-icon">
              {isExpanded ? '▼' : '▶'}
            </span>
          )}
          
          <span className="tree-node-icon">
            {node.type === 'tenant' && '🏢'}
            {node.type === 'site' && '🏭'}
            {node.type === 'device' && '📱'}
            {node.type === 'datapoint' && '📊'}
          </span>
          
          <span className="tree-node-label">
            {node.label}
          </span>
          
          {node.type === 'datapoint' && node.dataPoint && (
            <div className="data-point-preview">
              <span className={`data-value ${node.dataPoint.quality || 'unknown'}`}>
                {node.dataPoint.value}
                {node.dataPoint.unit && ` ${node.dataPoint.unit}`}
              </span>
            </div>
          )}
          
          {node.type === 'device' && node.connectionStatus && (
            <span className={`connection-badge ${node.connectionStatus}`}>
              {node.connectionStatus === 'connected' && '🟢'}
              {node.connectionStatus === 'disconnected' && '⚪'}
              {node.connectionStatus === 'error' && '❌'}
            </span>
          )}
          
          {!hasChildren && node.childCount && (
            <span className="child-count">
              {node.childCount}
            </span>
          )}
        </div>
        
        {isExpanded && node.children && (
          <div className="tree-children">
            {node.children.map(child => renderTreeNode(child))}
          </div>
        )}
      </div>
    );
  };

  // ========================================================================
  // 🔥 메인 렌더링 - CSS 클래스 기반으로 완전 수정
  // ========================================================================

  return (
    <div className="data-explorer-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            📊 PulseOne 실시간 데이터 탐색기
          </h1>
          <div className="header-meta">
            <div className={`connection-status status-${connectionStatus}`}>
              <span>
                {connectionStatus === 'connected' && '✅'}
                {connectionStatus === 'connecting' && '🔄'}
                {connectionStatus === 'disconnected' && '❌'}
              </span>
              <span>
                {connectionStatus === 'connected' && 'API 연결됨'}
                {connectionStatus === 'connecting' && 'API 연결중'}
                {connectionStatus === 'disconnected' && 'API 연결 끊김'}
              </span>
              <span>
                ({devices.length}개 디바이스, {filteredDataPoints.length}개 포인트)
              </span>
            </div>
            <div>
              마지막 업데이트: {lastRefresh.toLocaleTimeString()}
            </div>
          </div>
        </div>

        <div className="page-actions">
          <div className="auto-refresh-control">
            <label className="refresh-toggle">
              <input
                type="checkbox"
                checked={autoRefresh}
                onChange={(e) => setAutoRefresh(e.target.checked)}
              />
              자동 새로고침
            </label>
            {autoRefresh && (
              <select
                value={refreshInterval}
                onChange={(e) => setRefreshInterval(Number(e.target.value))}
                className="refresh-interval"
              >
                <option value={1}>1초</option>
                <option value={3}>3초</option>
                <option value={5}>5초</option>
                <option value={10}>10초</option>
              </select>
            )}
          </div>
          
          <button 
            onClick={handleRefresh}
            disabled={isLoading}
            className="btn btn-outline"
          >
            <span style={{transform: isLoading ? 'rotate(360deg)' : 'none', transition: 'transform 1s linear'}}>🔄</span>
            새로고침
          </button>
          
          <button 
            onClick={handleExportData}
            disabled={filteredDataPoints.length === 0}
            className="btn btn-primary"
          >
            📥 데이터 내보내기
          </button>
        </div>
      </div>

      {/* 에러 배너 */}
      {error && (
        <div className="error-banner">
          <div className="error-content">
            <div className="error-message">
              <span>⚠️</span>
              <span>{error}</span>
            </div>
            <button 
              onClick={() => setError(null)}
              className="error-retry"
            >
              ×
            </button>
          </div>
        </div>
      )}

      {/* 메인 레이아웃 */}
      <div className="explorer-layout">
        
        {/* 왼쪽: 트리 패널 */}
        <div className="tree-panel">
          <div className="tree-header">
            <h3>📋 데이터 구조</h3>
            <div className="search-container">
              <div className="search-input-wrapper">
                <input
                  type="text"
                  placeholder="검색..."
                  value={filters.search}
                  onChange={(e) => setFilters(prev => ({ ...prev, search: e.target.value }))}
                  className="search-input"
                />
                <span className="search-icon">🔍</span>
              </div>
            </div>
          </div>
          
          <div className="tree-content">
            {isLoading ? (
              <div className="loading-container">
                <div className="loading-spinner"></div>
                <div className="loading-text">DB/Redis에서 데이터 로딩 중...</div>
              </div>
            ) : treeData.length === 0 ? (
              <div className="empty-state">
                <div style={{fontSize: '48px', marginBottom: '16px'}}>📊</div>
                <h3 style={{margin: '0 0 8px 0', fontSize: '16px'}}>데이터가 없습니다</h3>
                <p style={{margin: 0, fontSize: '14px', textAlign: 'center'}}>
                  API 연결을 확인하고 새로고침해보세요
                </p>
              </div>
            ) : (
              treeData.map(node => renderTreeNode(node))
            )}
          </div>
        </div>

        {/* 오른쪽: 상세 정보 패널 */}
        <div className="details-panel">
          <div className="details-header">
            <h3>
              📊 실시간 데이터 
              {selectedNode && ` - ${selectedNode.label}`}
            </h3>
            <div className="details-controls">
              <div className="filter-controls">
                <select
                  value={filters.dataType}
                  onChange={(e) => setFilters(prev => ({ ...prev, dataType: e.target.value }))}
                  className="filter-select"
                >
                  <option value="all">모든 타입</option>
                  <option value="number">숫자</option>
                  <option value="boolean">불린</option>
                  <option value="string">문자열</option>
                  <option value="integer">정수</option>
                </select>

                <select
                  value={filters.quality}
                  onChange={(e) => setFilters(prev => ({ ...prev, quality: e.target.value }))}
                  className="filter-select"
                >
                  <option value="all">모든 품질</option>
                  <option value="good">Good</option>
                  <option value="uncertain">Uncertain</option>
                  <option value="bad">Bad</option>
                  <option value="comm_failure">Comm Failure</option>
                  <option value="last_known">Last Known</option>
                </select>

                <select
                  value={filters.device}
                  onChange={(e) => setFilters(prev => ({ ...prev, device: e.target.value }))}
                  className="filter-select"
                >
                  <option value="all">모든 디바이스</option>
                  {devices.map(device => (
                    <option key={device.device_id} value={device.device_id}>
                      {device.device_name}
                    </option>
                  ))}
                </select>
              </div>

              <div className="view-controls">
                <button
                  onClick={() => setShowChart(!showChart)}
                  className={`btn btn-sm ${showChart ? 'btn-primary' : 'btn-outline'}`}
                >
                  📈 차트 {showChart ? '숨기기' : '보기'}
                </button>
                
                {selectedDataPoints.length > 0 && (
                  <button
                    onClick={clearSelection}
                    className="btn btn-sm btn-outline"
                  >
                    선택 해제 ({selectedDataPoints.length})
                  </button>
                )}
              </div>
            </div>
          </div>

          <div className="details-content">
            {/* 차트 영역 */}
            {showChart && selectedDataPoints.length > 0 && (
              <div style={{
                marginBottom: '20px',
                padding: '16px',
                border: '1px solid #e5e7eb',
                borderRadius: '6px',
                backgroundColor: '#f9fafb'
              }}>
                <h4 style={{margin: '0 0 12px 0', fontSize: '14px', fontWeight: 600}}>📈 실시간 트렌드</h4>
                <div style={{
                  height: '200px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  backgroundColor: '#ffffff',
                  border: '2px dashed #d1d5db',
                  borderRadius: '6px',
                  color: '#6b7280'
                }}>
                  <div style={{textAlign: 'center'}}>
                    <p style={{margin: '0 0 8px 0'}}>InfluxDB 연동 차트가 여기에 표시됩니다</p>
                    <p style={{margin: 0, fontSize: '12px'}}>선택된 {selectedDataPoints.length}개 포인트의 트렌드</p>
                  </div>
                </div>
              </div>
            )}

            {/* 🔥 완전히 CSS 클래스 기반으로 변경된 실시간 데이터 테이블 */}
            <div className="realtime-data">
              <h4>
                ⚡ 실시간 데이터 ({filteredDataPoints.length}개)
              </h4>
              
              {filteredDataPoints.length === 0 ? (
                selectedNode && selectedNode.type === 'device' && 
                (selectedNode.connectionStatus === 'disconnected' || selectedNode.childCount === 0) ? 
                  renderEmptyDeviceMessage(selectedNode) : (
                  <div className="empty-state">
                    <p style={{margin: '0 0 8px 0'}}>표시할 데이터가 없습니다</p>
                    <small>필터를 조정하거나 API 연결을 확인해보세요</small>
                    {realtimeData.length > 0 && (
                      <div style={{marginTop: '10px', fontSize: '12px', color: '#6c757d'}}>
                        <p style={{margin: 0}}>원본 데이터는 {realtimeData.length}개가 있지만 필터 조건에 맞지 않습니다.</p>
                      </div>
                    )}
                  </div>
                )
              ) : (
                <div className="data-table-container">
                  {/* 🔥 CSS 클래스 기반 헤더 */}
                  <div className="data-table-header">
                    <div className="header-cell">✓</div>
                    <div className="header-cell">포인트명</div>
                    <div className="header-cell">디바이스</div>
                    <div className="header-cell">현재값</div>
                    <div className="header-cell">품질</div>
                    <div className="header-cell">타입</div>
                    <div className="header-cell">시간</div>
                  </div>
                  
                  {/* 🔥 CSS 클래스 기반 바디 */}
                  <div className="data-table-body">
                    {filteredDataPoints.map((dataPoint: RealtimeValue, index: number) => (
                      <div key={dataPoint.key || `row-${index}`} className="data-table-row">
                        
                        {/* 체크박스 */}
                        <div className="table-cell cell-checkbox" data-label="선택">
                          <input
                            type="checkbox"
                            checked={selectedDataPoints.some((dp: RealtimeValue) => dp.key === dataPoint.key)}
                            onChange={() => handleDataPointSelect(dataPoint)}
                          />
                        </div>
                        
                        {/* 포인트명 */}
                        <div className="table-cell cell-point" data-label="포인트명">
                          <div className="point-info">
                            <div className="point-name">
                              {dataPoint.point_name || '[포인트명 없음]'}
                            </div>
                            <div className="point-key">
                              {(dataPoint.key || '').replace('device:', '').replace(/:/g, '/')}
                            </div>
                          </div>
                        </div>
                        
                        {/* 디바이스 */}
                        <div className="table-cell cell-device" data-label="디바이스">
                          <div className="device-name">
                            {(dataPoint.device_name || '').replace('-CTRL-', '').replace('-01', '')}
                          </div>
                        </div>
                        
                        {/* 현재값 */}
                        <div className="table-cell cell-value" data-label="현재값">
                          <div className="value-display">
                            <span className={`value ${dataPoint.quality || 'unknown'}`}>
                              {String(dataPoint.value || '—')}
                              {dataPoint.unit && <span className="unit">{dataPoint.unit}</span>}
                            </span>
                          </div>
                        </div>
                        
                        {/* 품질 */}
                        <div className="table-cell cell-quality" data-label="품질">
                          <span className={`quality-badge ${dataPoint.quality || 'unknown'}`}>
                            {dataPoint.quality === 'good' ? 'OK' :
                             dataPoint.quality === 'comm_failure' ? 'ERR' : 
                             dataPoint.quality === 'last_known' ? 'OLD' : 
                             dataPoint.quality === 'uncertain' ? '?' : 
                             dataPoint.quality || '—'}
                          </span>
                        </div>
                        
                        {/* 타입 */}
                        <div className="table-cell cell-type" data-label="타입">
                          <span className="data-type">
                            {dataPoint.data_type === 'number' ? 'NUM' :
                             dataPoint.data_type === 'boolean' ? 'BOOL' :
                             dataPoint.data_type === 'integer' ? 'INT' :
                             dataPoint.data_type === 'string' ? 'STR' : 'UNK'}
                          </span>
                        </div>
                        
                        {/* 시간 - 밀리초 포함 */}
                        <div className="table-cell cell-time" data-label="업데이트">
                          <span className="timestamp">
                            {dataPoint.timestamp ? 
                              new Date(dataPoint.timestamp).toLocaleTimeString('ko-KR', {
                                hour12: false,
                                hour: '2-digit',
                                minute: '2-digit',
                                second: '2-digit'
                              }) + '.' + String(new Date(dataPoint.timestamp).getMilliseconds()).padStart(3, '0') : '—'}
                          </span>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default DataExplorer;