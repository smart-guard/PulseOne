import React, { useState, useEffect, useCallback, useMemo } from 'react';
import '../styles/data-explorer.css';

// ============================================================================
// 🔥 실제 API 응답에 맞춘 타입 정의
// ============================================================================

interface RedisDataPoint {
  id: string;
  key: string;
  point_id: number;
  device_id: string;
  device_name: string;
  point_name: string;
  value: string;
  timestamp: string;
  quality: 'good' | 'bad' | 'uncertain' | 'comm_failure' | 'last_known';
  data_type: 'number' | 'boolean' | 'string' | 'integer';
  unit?: string;
  changed?: boolean;
  source: string;
}

interface DeviceInfo {
  device_id: string;
  device_name: string;
  device_type?: string;
  point_count: number;
  status: string;
  last_seen?: string;
}

interface TreeNode {
  id: string;
  label: string;
  type: 'tenant' | 'site' | 'device' | 'datapoint';
  level: number;
  isExpanded: boolean;
  isLoaded: boolean;
  children?: TreeNode[];
  childCount?: number;
  dataPoint?: RedisDataPoint;
  deviceInfo?: DeviceInfo;
  lastUpdate?: string;
  connectionStatus?: 'connected' | 'disconnected' | 'error';
}

interface FilterState {
  search: string;
  dataType: string;
  quality: string;
  device: string;
}

interface APIResponse {
  success: boolean;
  data: any;
  message: string;
  error_code?: string;
  timestamp: string;
}

const DataExplorer: React.FC = () => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  
  const [treeData, setTreeData] = useState<TreeNode[]>([]);
  const [selectedNode, setSelectedNode] = useState<TreeNode | null>(null);
  const [selectedDataPoints, setSelectedDataPoints] = useState<RedisDataPoint[]>([]);
  const [realtimeData, setRealtimeData] = useState<RedisDataPoint[]>([]);
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
  // 🔥 실제 API 연동 함수들
  // ========================================================================
  
  const API_BASE = process.env.NODE_ENV === 'development' ? 'http://localhost:3000' : '';

  const apiCall = async (endpoint: string, options: RequestInit = {}): Promise<APIResponse> => {
    try {
      const response = await fetch(`${API_BASE}${endpoint}`, {
        headers: {
          'Content-Type': 'application/json',
          ...options.headers,
        },
        ...options,
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      return data;
    } catch (error) {
      console.error(`API 호출 실패 [${endpoint}]:`, error);
      throw error;
    }
  };

  // 🔥 디바이스 목록 로드 (실제 API)
  const loadDevices = useCallback(async () => {
    try {
      console.log('🔄 디바이스 목록 로드 시작...');
      
      const response = await apiCall('/api/realtime/devices');
      
      if (response.success && response.data?.devices) {
        const deviceList: DeviceInfo[] = response.data.devices.map((device: any) => ({
          device_id: device.device_id,
          device_name: device.device_name || `Device ${device.device_id}`,
          device_type: device.device_type || 'Unknown',
          point_count: device.point_count || 0,
          status: device.status || 'unknown',
          last_seen: device.last_seen
        }));
        
        setDevices(deviceList);
        console.log(`✅ ${deviceList.length}개 디바이스 로드 완료`);
        return deviceList;
      } else {
        console.warn('⚠️ 디바이스 API 응답 이상:', response);
        return [];
      }
    } catch (error: any) {
      console.error('❌ 디바이스 로드 실패:', error);
      throw error;
    }
  }, []);

  // 🔥 실시간 데이터 로드 (실제 API)
  const loadRealtimeData = useCallback(async () => {
    try {
      console.log('🔄 실시간 데이터 로드 시작...');
      
      const queryParams = new URLSearchParams({
        limit: '100'
      });

      // 필터 적용
      if (filters.quality !== 'all') {
        queryParams.append('quality_filter', filters.quality);
      }
      
      if (filters.device !== 'all') {
        queryParams.append('device_ids', filters.device);
      }

      const response = await apiCall(`/api/realtime/current-values?${queryParams.toString()}`);
      
      if (response.success && response.data?.current_values) {
        const dataPoints: RedisDataPoint[] = response.data.current_values.map((item: any) => ({
          id: item.id,
          key: item.key,
          point_id: item.point_id,
          device_id: item.device_id,
          device_name: item.device_name,
          point_name: item.point_name,
          value: item.value,
          timestamp: item.timestamp,
          quality: item.quality,
          data_type: item.data_type,
          unit: item.unit,
          changed: item.changed,
          source: item.source
        }));

        setRealtimeData(dataPoints);
        setLastRefresh(new Date());
        console.log(`✅ ${dataPoints.length}개 실시간 데이터 로드 완료`);
        return dataPoints;
      } else {
        console.warn('⚠️ 실시간 데이터 API 응답 이상:', response);
        return [];
      }
    } catch (error: any) {
      console.error('❌ 실시간 데이터 로드 실패:', error);
      throw error;
    }
  }, [filters.quality, filters.device]);

  // 🔥 특정 디바이스 데이터 로드
  const loadDeviceData = useCallback(async (deviceId: string) => {
    try {
      console.log(`🔄 디바이스 ${deviceId} 데이터 로드...`);
      
      const response = await apiCall(`/api/realtime/device/${deviceId}/values`);
      
      if (response.success && response.data?.data_points) {
        const dataPoints: RedisDataPoint[] = response.data.data_points.map((item: any) => ({
          id: item.id,
          key: item.key,
          point_id: item.point_id,
          device_id: item.device_id,
          device_name: item.device_name,
          point_name: item.point_name,
          value: item.value,
          timestamp: item.timestamp,
          quality: item.quality,
          data_type: item.data_type,
          unit: item.unit,
          changed: item.changed,
          source: item.source
        }));

        console.log(`✅ 디바이스 ${deviceId}: ${dataPoints.length}개 포인트 로드 완료`);
        return dataPoints;
      } else {
        console.warn(`⚠️ 디바이스 ${deviceId} API 응답 이상:`, response);
        return [];
      }
    } catch (error: any) {
      console.error(`❌ 디바이스 ${deviceId} 데이터 로드 실패:`, error);
      return [];
    }
  }, []);

  // 🔥 트리 데이터 생성 (실제 디바이스 기반)
  const generateTreeData = useCallback((devices: DeviceInfo[]): TreeNode[] => {
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

    // 디바이스들을 사이트별로 그룹화 (여기서는 단순하게 하나의 사이트로)
    const deviceNodes: TreeNode[] = devices.map(device => ({
      id: `device-${device.device_id}`,
      label: device.device_name,
      type: 'device',
      level: 2,
      isExpanded: false,
      isLoaded: false, // 처음에는 자식 로드 안됨
      deviceInfo: device,
      connectionStatus: device.status === 'connected' ? 'connected' : 'disconnected',
      lastUpdate: device.last_seen,
      childCount: device.point_count
    }));

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
  }, []);

  // 🔥 디바이스 자식 노드 로드 (실제 API 호출)
  const loadDeviceChildren = useCallback(async (deviceNode: TreeNode) => {
    if (deviceNode.type !== 'device') return;
    
    const deviceId = deviceNode.id.replace('device-', '');
    console.log(`🔄 디바이스 ${deviceId} 자식 노드 로드...`);
    
    try {
      const dataPoints = await loadDeviceData(deviceId);
      
      // 데이터 포인트를 트리 노드로 변환
      const pointNodes: TreeNode[] = dataPoints.map((point, index) => ({
        id: `${deviceNode.id}-point-${point.point_id}`,
        label: point.point_name,
        type: 'datapoint',
        level: deviceNode.level + 1,
        isExpanded: false,
        isLoaded: true,
        dataPoint: point
      }));

      // 트리 업데이트
      setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
        children: pointNodes,
        isLoaded: true,
        isExpanded: true,
        childCount: pointNodes.length
      }));

      console.log(`✅ 디바이스 ${deviceId} 자식 노드 ${pointNodes.length}개 로드 완료`);

    } catch (error) {
      console.error(`❌ 디바이스 ${deviceId} 자식 노드 로드 실패:`, error);
      
      // 에러 시 빈 노드로 표시
      setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
        children: [],
        isLoaded: true,
        isExpanded: true,
        childCount: 0
      }));
    }
  }, [loadDeviceData]);

  // 🔥 트리 초기화 (디바이스 + 실시간 데이터 로드)
  const initializeData = useCallback(async () => {
    setIsLoading(true);
    setConnectionStatus('connecting');
    
    try {
      console.log('🚀 데이터 초기화 시작...');
      
      // 1. 디바이스 목록 로드
      const deviceList = await loadDevices();
      
      // 2. 실시간 데이터 로드
      const realtimeDataPoints = await loadRealtimeData();
      
      // 3. 트리 구조 생성
      const treeStructure = generateTreeData(deviceList);
      setTreeData(treeStructure);
      
      setConnectionStatus('connected');
      setError(null);
      
      console.log('✅ 데이터 초기화 완료');
      console.log(`📊 디바이스: ${deviceList.length}개, 실시간 포인트: ${realtimeDataPoints.length}개`);
      
    } catch (error: any) {
      console.error('❌ 데이터 초기화 실패:', error);
      setError(`데이터 로드 실패: ${error.message}`);
      setConnectionStatus('disconnected');
      
      // 에러 시 기본 트리 구조라도 표시
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
  }, [loadDevices, loadRealtimeData, generateTreeData]);

  // ========================================================================
  // 유틸리티 함수들
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

  const findAllDataPoints = (nodes: TreeNode[]): RedisDataPoint[] => {
    const dataPoints: RedisDataPoint[] = [];
    
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
  // 필터링된 데이터
  // ========================================================================

  const filteredDataPoints = useMemo(() => {
    let points = selectedDataPoints.length > 0 ? selectedDataPoints : realtimeData;
    
    if (filters.search) {
      points = points.filter(dp => 
        dp.point_name.toLowerCase().includes(filters.search.toLowerCase()) ||
        dp.device_name.toLowerCase().includes(filters.search.toLowerCase()) ||
        dp.key.toLowerCase().includes(filters.search.toLowerCase())
      );
    }
    
    if (filters.dataType !== 'all') {
      points = points.filter(dp => dp.data_type === filters.dataType);
    }
    
    if (filters.quality !== 'all') {
      points = points.filter(dp => dp.quality === filters.quality);
    }
    
    if (filters.device !== 'all') {
      points = points.filter(dp => dp.device_id === filters.device);
    }
    
    return points;
  }, [selectedDataPoints, realtimeData, filters]);

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================

  const handleNodeClick = useCallback((node: TreeNode) => {
    setSelectedNode(node);
    
    if (node.type === 'datapoint' && node.dataPoint) {
      // 데이터 포인트 선택
      setSelectedDataPoints([node.dataPoint]);
      
    } else if (node.type === 'device') {
      // 디바이스 선택 - 바로 API 호출로 최신 데이터 가져오기
      const deviceId = node.id.replace('device-', '');
      
      // 기존 자식 노드의 데이터포인트도 선택에 포함
      const existingDataPoints = findAllDataPoints([node]);
      if (existingDataPoints.length > 0) {
        setSelectedDataPoints(existingDataPoints);
      }
      
      // 실시간으로 해당 디바이스 데이터 로드
      loadDeviceData(deviceId).then(dataPoints => {
        if (dataPoints.length > 0) {
          setSelectedDataPoints(dataPoints);
        }
      });
      
      // 자식 노드가 로드되지 않았으면 로드
      if (!node.isLoaded && node.childCount && node.childCount > 0) {
        loadDeviceChildren(node);
      } else {
        // 이미 로드된 경우 토글
        setTreeData(prev => updateTreeNode(prev, node.id, { 
          isExpanded: !node.isExpanded 
        }));
      }
      
    } else {
      // 사이트나 테넌트 토글
      setTreeData(prev => updateTreeNode(prev, node.id, { 
        isExpanded: !node.isExpanded 
      }));
    }
  }, [findAllDataPoints, loadDeviceChildren, loadDeviceData]);

  const handleDataPointSelect = useCallback((dataPoint: RedisDataPoint) => {
    setSelectedDataPoints(prev => {
      const exists = prev.find(dp => dp.key === dataPoint.key);
      if (exists) {
        return prev.filter(dp => dp.key !== dataPoint.key);
      } else {
        return [...prev, dataPoint];
      }
    });
  }, []);

  const handleRefresh = useCallback(() => {
    loadRealtimeData();
  }, [loadRealtimeData]);

  const handleExportData = useCallback(() => {
    const data = filteredDataPoints.map(dp => ({
      key: dp.key,
      point_name: dp.point_name,
      device_name: dp.device_name,
      value: dp.value,
      unit: dp.unit,
      timestamp: dp.timestamp,
      quality: dp.quality,
      data_type: dp.data_type
    }));
    
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `redis_data_${new Date().toISOString().split('T')[0]}.json`;
    a.click();
    URL.revokeObjectURL(url);
  }, [filteredDataPoints]);

  const clearSelection = useCallback(() => {
    setSelectedDataPoints([]);
    setSelectedNode(null);
  }, []);

  // ========================================================================
  // 자동 새로고침
  // ========================================================================

  useEffect(() => {
    if (autoRefresh && refreshInterval > 0) {
      const interval = setInterval(() => {
        loadRealtimeData();
      }, refreshInterval * 1000);
      
      return () => clearInterval(interval);
    }
  }, [autoRefresh, refreshInterval, loadRealtimeData]);

  // ========================================================================
  // 초기 로딩
  // ========================================================================

  useEffect(() => {
    initializeData();
  }, []); // 한 번만 실행

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================

  const renderTreeNode = (node: TreeNode): React.ReactNode => {
    const hasChildren = (node.children && node.children.length > 0) || (node.childCount && node.childCount > 0);
    const isExpanded = node.isExpanded && node.children;
    
    return (
      <div key={node.id} className="tree-node-container">
        <div 
          className={`tree-node ${selectedNode?.id === node.id ? 'selected' : ''}`}
          style={{ paddingLeft: `${node.level * 20 + 12}px` }}
          onClick={() => handleNodeClick(node)}
        >
          <div className="tree-node-content">
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
            
            <span className="tree-node-label">{node.label}</span>
            
            {node.type === 'datapoint' && node.dataPoint && (
              <div className="data-point-preview">
                <span className={`data-value ${node.dataPoint.quality}`}>
                  {node.dataPoint.value}
                  {node.dataPoint.unit && ` ${node.dataPoint.unit}`}
                </span>
                <div className={`quality-indicator ${node.dataPoint.quality}`}></div>
                {node.dataPoint.changed && <span className="change-indicator">🔄</span>}
              </div>
            )}
            
            {node.type === 'device' && node.connectionStatus && (
              <div className={`connection-badge ${node.connectionStatus}`}>
                {node.connectionStatus === 'connected' && '✅'}
                {node.connectionStatus === 'disconnected' && '⚪'}
                {node.connectionStatus === 'error' && '❌'}
              </div>
            )}
            
            {!hasChildren && node.childCount && (
              <span className="child-count">{node.childCount}</span>
            )}
          </div>
        </div>
        
        {isExpanded && node.children && (
          <div className="tree-children">
            {node.children.map(child => renderTreeNode(child))}
          </div>
        )}
      </div>
    );
  };

  const formatTimestamp = (timestamp: string) => {
    return new Date(timestamp).toLocaleString('ko-KR', {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  // ========================================================================
  // 메인 렌더링
  // ========================================================================

  return (
    <div className="data-explorer-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-database"></i>
            Redis 실시간 데이터 탐색기
          </h1>
          <div className="header-meta">
            <div className={`connection-status status-${connectionStatus}`}>
              <span className="status-icon">
                {connectionStatus === 'connected' && <i className="fas fa-check-circle"></i>}
                {connectionStatus === 'connecting' && <i className="fas fa-clock"></i>}
                {connectionStatus === 'disconnected' && <i className="fas fa-exclamation-circle"></i>}
              </span>
              <span className="status-text">
                {connectionStatus === 'connected' && 'API 연결됨'}
                {connectionStatus === 'connecting' && 'API 연결중'}
                {connectionStatus === 'disconnected' && 'API 연결 끊김'}
              </span>
              <span className="stats-info">
                ({devices.length}개 디바이스, {filteredDataPoints.length}개 포인트)
              </span>
            </div>
            <div className="last-update">
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
            className="btn btn-outline"
            onClick={handleRefresh}
            disabled={isLoading}
          >
            <i className={`fas fa-sync-alt ${isLoading ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
          
          <button 
            className="btn btn-primary"
            onClick={handleExportData}
            disabled={filteredDataPoints.length === 0}
          >
            <i className="fas fa-download"></i>
            데이터 내보내기
          </button>
        </div>
      </div>

      {/* 에러 배너 */}
      {error && (
        <div className="error-banner">
          <div className="error-content">
            <i className="fas fa-exclamation-triangle error-icon"></i>
            <span className="error-message">{error}</span>
            <button 
              className="error-retry"
              onClick={() => setError(null)}
            >
              닫기
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
                <i className="fas fa-search search-icon"></i>
                <input
                  type="text"
                  placeholder="검색..."
                  value={filters.search}
                  onChange={(e) => setFilters(prev => ({ ...prev, search: e.target.value }))}
                  className="search-input"
                />
              </div>
            </div>
          </div>
          
          <div className="tree-content">
            {isLoading ? (
              <div className="loading-container">
                <div className="loading-spinner"></div>
                <div className="loading-text">API에서 데이터 로딩 중...</div>
              </div>
            ) : treeData.length === 0 ? (
              <div className="empty-state">
                <div className="empty-state-icon">
                  <i className="fas fa-database"></i>
                </div>
                <h3 className="empty-state-title">데이터가 없습니다</h3>
                <p className="empty-state-description">
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
                  className={`btn btn-sm ${showChart ? 'btn-primary' : 'btn-outline'}`}
                  onClick={() => setShowChart(!showChart)}
                >
                  📈 차트 {showChart ? '숨기기' : '보기'}
                </button>
                
                {selectedDataPoints.length > 0 && (
                  <button
                    className="btn btn-sm btn-outline"
                    onClick={clearSelection}
                  >
                    선택 해제 ({selectedDataPoints.length})
                  </button>
                )}
              </div>
            </div>
          </div>

          <div className="details-content">
            {/* 선택된 노드 정보 */}
            {selectedNode && (
              <div className="node-info">
                <h4>
                  <span className="node-type-icon">
                    {selectedNode.type === 'tenant' && '🏢'}
                    {selectedNode.type === 'site' && '🏭'}
                    {selectedNode.type === 'device' && '📱'}
                    {selectedNode.type === 'datapoint' && '📊'}
                  </span>
                  {selectedNode.label}
                </h4>
                
                <div className="node-metadata">
                  <div className="metadata-item">
                    <span className="label">타입:</span>
                    <span className="value">{selectedNode.type}</span>
                  </div>
                  <div className="metadata-item">
                    <span className="label">레벨:</span>
                    <span className="value">{selectedNode.level}</span>
                  </div>
                  {selectedNode.type === 'device' && selectedNode.deviceInfo && (
                    <>
                      <div className="metadata-item">
                        <span className="label">디바이스 ID:</span>
                        <span className="value">{selectedNode.deviceInfo.device_id}</span>
                      </div>
                      <div className="metadata-item">
                        <span className="label">디바이스 타입:</span>
                        <span className="value">{selectedNode.deviceInfo.device_type}</span>
                      </div>
                      <div className="metadata-item">
                        <span className="label">포인트 개수:</span>
                        <span className="value">{selectedNode.deviceInfo.point_count}개</span>
                      </div>
                      <div className="metadata-item">
                        <span className="label">연결 상태:</span>
                        <span className={`value status-${selectedNode.connectionStatus}`}>
                          {selectedNode.connectionStatus}
                        </span>
                      </div>
                    </>
                  )}
                  {selectedNode.type === 'datapoint' && selectedNode.dataPoint && (
                    <>
                      <div className="metadata-item">
                        <span className="label">Redis 키:</span>
                        <span className="value monospace">{selectedNode.dataPoint.key}</span>
                      </div>
                      <div className="metadata-item">
                        <span className="label">포인트 ID:</span>
                        <span className="value">{selectedNode.dataPoint.point_id}</span>
                      </div>
                      <div className="metadata-item">
                        <span className="label">데이터 타입:</span>
                        <span className="value">{selectedNode.dataPoint.data_type}</span>
                      </div>
                      <div className="metadata-item">
                        <span className="label">현재 품질:</span>
                        <span className={`value quality-${selectedNode.dataPoint.quality}`}>
                          {selectedNode.dataPoint.quality}
                        </span>
                      </div>
                    </>
                  )}
                </div>
              </div>
            )}

            {/* 차트 영역 */}
            {showChart && selectedDataPoints.length > 0 && (
              <div className="chart-section">
                <h4>📈 실시간 트렌드</h4>
                <div className="chart-placeholder">
                  <div className="chart-container">
                    <div className="chart-info">
                      <p>InfluxDB 연동 차트가 여기에 표시됩니다</p>
                      <p>선택된 {selectedDataPoints.length}개 포인트의 트렌드</p>
                      <small>API: /api/realtime/historical?point_ids={selectedDataPoints.map(p => p.point_id).join(',')}</small>
                    </div>
                  </div>
                </div>
              </div>
            )}

{/* 실시간 데이터 테이블 */}
<div className="realtime-data">
  <h4>⚡ 실시간 데이터 ({filteredDataPoints.length}개)</h4>
  
  {/* 🔥 1단계: 디버깅 정보 표시 */}
  <div style={{background: '#fff3cd', padding: '15px', margin: '10px 0', border: '1px solid #ffeaa7', borderRadius: '8px'}}>
    <h5 style={{margin: '0 0 10px 0', color: '#856404'}}>🔍 디버깅 정보</h5>
    <div style={{fontSize: '14px', fontFamily: 'monospace'}}>
      <div><strong>filteredDataPoints.length:</strong> {filteredDataPoints.length}</div>
      <div><strong>realtimeData.length:</strong> {realtimeData.length}</div>
      <div><strong>selectedDataPoints.length:</strong> {selectedDataPoints.length}</div>
      
      {filteredDataPoints.length > 0 && (
        <div style={{marginTop: '10px'}}>
          <strong>첫 번째 데이터:</strong>
          <pre style={{background: '#f8f9fa', padding: '10px', borderRadius: '4px', fontSize: '12px', overflow: 'auto'}}>
            {JSON.stringify(filteredDataPoints[0], null, 2)}
          </pre>
        </div>
      )}
      
      {realtimeData.length > 0 && filteredDataPoints.length === 0 && (
        <div style={{marginTop: '10px', color: '#dc3545'}}>
          <strong>⚠️ 주의:</strong> realtimeData는 있지만 filteredDataPoints가 비어있습니다!
          <pre style={{background: '#f8f9fa', padding: '10px', borderRadius: '4px', fontSize: '12px', overflow: 'auto'}}>
            첫 번째 realtimeData: {JSON.stringify(realtimeData[0], null, 2)}
          </pre>
        </div>
      )}
    </div>
  </div>
  
  {/* 🔥 2단계: 강제 하드코딩 테스트 테이블 */}
  <div style={{background: '#d1ecf1', padding: '15px', margin: '10px 0', border: '1px solid #bee5eb', borderRadius: '8px'}}>
    <h5 style={{margin: '0 0 10px 0', color: '#0c5460'}}>🧪 하드코딩 테스트 테이블</h5>
    <div className="data-table-container">
      <div className="data-table-header">
        <div className="header-cell">선택</div>
        <div className="header-cell">포인트명</div>
        <div className="header-cell">디바이스</div>
        <div className="header-cell">현재값</div>
        <div className="header-cell">품질</div>
        <div className="header-cell">타입</div>
        <div className="header-cell">업데이트</div>
      </div>
      <div className="data-table-body">
        <div className="data-table-row">
          <div className="table-cell cell-checkbox">✅</div>
          <div className="table-cell cell-point">
            <div className="point-info">
              <div className="point-name">하드코딩_포인트</div>
              <div className="point-key">test:hardcoded:key</div>
            </div>
          </div>
          <div className="table-cell cell-device">
            <span className="device-name">하드코딩_디바이스</span>
          </div>
          <div className="table-cell cell-value">
            <span className="value good">999.99</span>
          </div>
          <div className="table-cell cell-quality">
            <span className="quality-badge good">TEST</span>
          </div>
          <div className="table-cell cell-type">
            <span className="data-type">hardcoded</span>
          </div>
          <div className="table-cell cell-time">
            <span className="timestamp">지금</span>
          </div>
        </div>
      </div>
    </div>
  </div>
  
  {/* 🔥 3단계: 실제 데이터 테이블 */}
  {filteredDataPoints.length === 0 ? (
    <div className="empty-state">
      <p>표시할 데이터가 없습니다</p>
      <small>필터를 조정하거나 API 연결을 확인해보세요</small>
    </div>
  ) : (
    <div style={{background: '#d4edda', padding: '15px', margin: '10px 0', border: '1px solid #c3e6cb', borderRadius: '8px'}}>
      <h5 style={{margin: '0 0 10px 0', color: '#155724'}}>📊 실제 데이터 테이블</h5>
      <div className="data-table-container">
        <div className="data-table-header">
          <div className="header-cell">선택</div>
          <div className="header-cell">포인트명</div>
          <div className="header-cell">디바이스</div>
          <div className="header-cell">현재값</div>
          <div className="header-cell">품질</div>
          <div className="header-cell">타입</div>
          <div className="header-cell">업데이트</div>
        </div>
        <div className="data-table-body">
          {filteredDataPoints.map((dataPoint, index) => {
            // 🔥 각 행별로 콘솔에 로그 출력
            console.log(`🔍 Row ${index}:`, {
              key: dataPoint.key,
              point_name: dataPoint.point_name,
              device_name: dataPoint.device_name,
              value: dataPoint.value,
              quality: dataPoint.quality,
              data_type: dataPoint.data_type,
              timestamp: dataPoint.timestamp
            });
            
            return (
              <div key={dataPoint.key || `row-${index}`} className="data-table-row">
                <div className="table-cell cell-checkbox">
                  <input
                    type="checkbox"
                    checked={selectedDataPoints.some(dp => dp.key === dataPoint.key)}
                    onChange={() => handleDataPointSelect(dataPoint)}
                  />
                </div>
                <div className="table-cell cell-point">
                  <div className="point-info">
                    <div className="point-name" title={dataPoint.point_name || 'undefined'}>
                      {dataPoint.point_name || '[포인트명 없음]'}
                    </div>
                    <div className="point-key" title={dataPoint.key || 'undefined'}>
                      {dataPoint.key || '[키 없음]'}
                    </div>
                  </div>
                </div>
                <div className="table-cell cell-device">
                  <span className="device-name" title={dataPoint.device_name || 'undefined'}>
                    {dataPoint.device_name || '[디바이스명 없음]'}
                  </span>
                </div>
                <div className="table-cell cell-value">
                  <div className="value-display">
                    <span className={`value ${dataPoint.quality || 'unknown'}`}>
                      {dataPoint.value !== undefined ? String(dataPoint.value) : '[값 없음]'}
                    </span>
                    {dataPoint.unit && <span className="unit">{dataPoint.unit}</span>}
                    {dataPoint.changed && <span className="change-indicator">🔄</span>}
                  </div>
                </div>
                <div className="table-cell cell-quality">
                  <span className={`quality-badge ${dataPoint.quality || 'unknown'}`}>
                    {dataPoint.quality || '[품질 없음]'}
                  </span>
                </div>
                <div className="table-cell cell-type">
                  <span className="data-type">
                    {dataPoint.data_type || '[타입 없음]'}
                  </span>
                </div>
                <div className="table-cell cell-time">
                  <span className="timestamp">
                    {dataPoint.timestamp ? formatTimestamp(dataPoint.timestamp) : '[시간 없음]'}
                  </span>
                </div>
              </div>
            );
          })}
        </div>
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