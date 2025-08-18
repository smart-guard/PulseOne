import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { 
  RealtimeApiService, 
  RealtimeValue, 
  DevicesResponse,
  CurrentValuesResponse,
  ApiResponse 
} from '../api/services/realtimeApi';
import '../styles/data-explorer.css';

// ============================================================================
// 🔥 수정된 API 서비스와 완전 연동된 DataExplorer
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
  // 상태 관리
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
  // 🔥 수정된 API 서비스 사용
  // ========================================================================

  // 🔥 디바이스 목록 로드 (수정된 API 서비스 사용)
  const loadDevices = useCallback(async () => {
    try {
      console.log('🔄 디바이스 목록 로드 시작...');
      
      const response: ApiResponse<DevicesResponse> = await RealtimeApiService.getDevices();
      
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
        console.log(`✅ ${deviceList.length}개 디바이스 로드 완료:`, deviceList);
        return deviceList;
      } else {
        console.warn('⚠️ 디바이스 API 응답 이상:', response);
        // 🔥 디바이스 API 실패 시 실시간 데이터에서 디바이스 추출
        if (realtimeData.length > 0) {
          const uniqueDevices = Array.from(new Set(realtimeData.map(dp => dp.device_id)))
            .map(deviceId => {
              const devicePoints = realtimeData.filter(dp => dp.device_id === deviceId);
              return {
                device_id: deviceId,
                device_name: devicePoints[0]?.device_name || `Device ${deviceId}`,
                device_type: 'Unknown',
                point_count: devicePoints.length,
                status: 'connected',
                last_seen: new Date().toISOString()
              };
            });
          
          setDevices(uniqueDevices);
          console.log(`✅ 실시간 데이터에서 ${uniqueDevices.length}개 디바이스 추출:`, uniqueDevices);
          return uniqueDevices;
        }
        return [];
      }
    } catch (error: any) {
      console.error('❌ 디바이스 로드 실패:', error);
      // 🔥 에러 시에도 실시간 데이터에서 디바이스 추출 시도
      if (realtimeData.length > 0) {
        const uniqueDevices = Array.from(new Set(realtimeData.map(dp => dp.device_id)))
          .map(deviceId => {
            const devicePoints = realtimeData.filter(dp => dp.device_id === deviceId);
            return {
              device_id: deviceId,
              device_name: devicePoints[0]?.device_name || `Device ${deviceId}`,
              device_type: 'Unknown',
              point_count: devicePoints.length,
              status: 'connected',
              last_seen: new Date().toISOString()
            };
          });
        
        setDevices(uniqueDevices);
        console.log(`✅ 에러 복구: 실시간 데이터에서 ${uniqueDevices.length}개 디바이스 추출`);
        return uniqueDevices;
      }
      throw error;
    }
  }, [realtimeData]);

  // 🔥 실시간 데이터 로드 (수정된 API 서비스 사용)
  const loadRealtimeData = useCallback(async () => {
    try {
      console.log('🔄 실시간 데이터 로드 시작...');
      
      // 🔥 수정된 API 파라미터 형식 사용
      const queryParams: any = {
        limit: 100
      };

      // 필터 적용
      if (filters.quality !== 'all') {
        queryParams.quality_filter = filters.quality;
      }
      
      if (filters.device !== 'all') {
        queryParams.device_ids = [filters.device];  // string 배열
      }

      const response: ApiResponse<CurrentValuesResponse> = await RealtimeApiService.getCurrentValues(queryParams);
      
      console.log('🔍 실시간 데이터 API 응답:', response);
      
      if (response.success && response.data?.current_values) {
        const dataPoints: RealtimeValue[] = response.data.current_values;

        console.log(`🔍 API에서 받은 데이터포인트 ${dataPoints.length}개:`, dataPoints);

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
      
      // 🔥 에러 시 시뮬레이션 데이터 생성 (디버깅용)
      console.log('🔧 시뮬레이션 데이터 생성 중...');
      const simulationData: RealtimeValue[] = Array.from({ length: 5 }, (_, i) => ({
        id: `sim_${i}`,
        key: `device:${i+1}:temp_${i+1}`,
        point_id: i + 1,
        device_id: String(i + 1),
        device_name: `시뮬레이션 Device ${i + 1}`,
        point_name: `temperature_sensor_${String(i + 1).padStart(2, '0')}`,
        value: (20 + Math.random() * 10).toFixed(2),
        timestamp: new Date().toISOString(),
        quality: ['good', 'bad', 'uncertain'][i % 3] as any,
        data_type: 'number',
        unit: '°C',
        changed: Math.random() > 0.5,
        source: 'simulation'
      }));
      
      setRealtimeData(simulationData);
      console.log(`🔧 시뮬레이션 데이터 ${simulationData.length}개 생성 완료`);
      return simulationData;
    }
  }, [filters.quality, filters.device]);

  // 🔥 특정 디바이스 데이터 로드 (수정된 API 서비스 사용)
  const loadDeviceData = useCallback(async (deviceId: string) => {
    try {
      console.log(`🔄 디바이스 ${deviceId} 데이터 로드...`);
      
      const response = await RealtimeApiService.getDeviceValues(deviceId);
      
      if (response.success && response.data?.data_points) {
        const dataPoints: RealtimeValue[] = response.data.data_points;

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

    // 디바이스들을 사이트별로 그룹화
    const deviceNodes: TreeNode[] = devices.map(device => ({
      id: `device-${device.device_id}`,
      label: device.device_name,
      type: 'device',
      level: 2,
      isExpanded: false,
      isLoaded: false,
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

  // 🔥 디바이스 자식 노드 로드
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
      
      setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
        children: [],
        isLoaded: true,
        isExpanded: true,
        childCount: 0
      }));
    }
  }, [loadDeviceData]);

  // 🔥 트리 초기화 (수정: 실시간 데이터 로드 후 디바이스 추출)
  const initializeData = useCallback(async () => {
    setIsLoading(true);
    setConnectionStatus('connecting');
    
    try {
      console.log('🚀 데이터 초기화 시작...');
      
      // 1. 실시간 데이터 먼저 로드
      const realtimeDataPoints = await loadRealtimeData();
      
      // 2. 디바이스 목록 로드 (실시간 데이터 기반으로 폴백)
      const deviceList = await loadDevices();
      
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
  // 🔥 필터링된 데이터 - 수정된 필드명 사용
  // ========================================================================

  const filteredDataPoints = useMemo(() => {
    let points = selectedDataPoints.length > 0 ? selectedDataPoints : realtimeData;
    
    console.log('🔍 필터링 시작:', {
      selectedDataPoints: selectedDataPoints.length,
      realtimeData: realtimeData.length,
      sourcePick: selectedDataPoints.length > 0 ? 'selected' : 'realtime',
      initialPoints: points.length
    });
    
    // 🔥 검색 필터 적용 (수정된 필드명 사용)
    if (filters.search) {
      const searchTerm = filters.search.toLowerCase();
      points = points.filter(dp => 
        (dp.point_name && dp.point_name.toLowerCase().includes(searchTerm)) ||
        (dp.device_name && dp.device_name.toLowerCase().includes(searchTerm)) ||
        (dp.key && dp.key.toLowerCase().includes(searchTerm))
      );
      console.log(`🔍 검색 필터 "${filters.search}" 적용 후: ${points.length}개`);
    }
    
    // 🔥 데이터 타입 필터 적용 (data_type 사용)
    if (filters.dataType !== 'all') {
      points = points.filter(dp => dp.data_type === filters.dataType);
      console.log(`🔍 데이터타입 필터 "${filters.dataType}" 적용 후: ${points.length}개`);
    }
    
    // 🔥 품질 필터 적용
    if (filters.quality !== 'all') {
      points = points.filter(dp => dp.quality === filters.quality);
      console.log(`🔍 품질 필터 "${filters.quality}" 적용 후: ${points.length}개`);
    }
    
    // 🔥 디바이스 필터 적용 (device_id는 string)
    if (filters.device !== 'all') {
      points = points.filter(dp => dp.device_id === filters.device);
      console.log(`🔍 디바이스 필터 "${filters.device}" 적용 후: ${points.length}개`);
    }
    
    console.log('✅ 필터링 완료:', points.length + '개');
    return points;
  }, [selectedDataPoints, realtimeData, filters]);

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================

  const handleNodeClick = useCallback((node: TreeNode) => {
    setSelectedNode(node);
    
    if (node.type === 'datapoint' && node.dataPoint) {
      setSelectedDataPoints([node.dataPoint]);
      
    } else if (node.type === 'device') {
      const deviceId = node.id.replace('device-', '');
      
      const existingDataPoints = findAllDataPoints([node]);
      if (existingDataPoints.length > 0) {
        setSelectedDataPoints(existingDataPoints);
      }
      
      loadDeviceData(deviceId).then(dataPoints => {
        if (dataPoints.length > 0) {
          setSelectedDataPoints(dataPoints);
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

  const handleDataPointSelect = useCallback((dataPoint: RealtimeValue) => {
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
  }, []);

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
  // 메인 렌더링 - 제공된 CSS 클래스 사용
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
            {/* 차트 영역 */}
            {showChart && selectedDataPoints.length > 0 && (
              <div className="chart-section">
                <h4>📈 실시간 트렌드</h4>
                <div className="chart-placeholder">
                  <div className="chart-container">
                    <div className="chart-info">
                      <p>InfluxDB 연동 차트가 여기에 표시됩니다</p>
                      <p>선택된 {selectedDataPoints.length}개 포인트의 트렌드</p>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* 🔥 실시간 데이터 테이블 - 제공된 CSS 클래스 사용 */}
            <div className="realtime-data">
              <h4>⚡ 실시간 데이터 ({filteredDataPoints.length}개)</h4>
              
              {filteredDataPoints.length === 0 ? (
                <div className="empty-state">
                  <p>표시할 데이터가 없습니다</p>
                  <small>필터를 조정하거나 API 연결을 확인해보세요</small>
                  {realtimeData.length > 0 && (
                    <div style={{marginTop: '10px', fontSize: '12px', color: '#6c757d'}}>
                      <p>원본 데이터는 {realtimeData.length}개가 있지만 필터 조건에 맞지 않습니다.</p>
                    </div>
                  )}
                </div>
              ) : (
                <div className="data-table-container">
                  {/* 🔥 컴팩트한 테이블 헤더 */}
                  <div className="data-table-header" style={{
                    display: 'grid',
                    gridTemplateColumns: '40px 200px 120px 100px 70px 60px 80px',
                    gap: '1px',
                    background: '#f9fafb',
                    borderBottom: '2px solid #e5e7eb',
                    position: 'sticky',
                    top: '0',
                    zIndex: 10
                  }}>
                    <div className="header-cell" style={{padding: '8px 4px', fontSize: '10px', fontWeight: '700', textAlign: 'center'}}>✓</div>
                    <div className="header-cell" style={{padding: '8px 6px', fontSize: '10px', fontWeight: '700', textAlign: 'left'}}>포인트명</div>
                    <div className="header-cell" style={{padding: '8px 4px', fontSize: '10px', fontWeight: '700', textAlign: 'center'}}>디바이스</div>
                    <div className="header-cell" style={{padding: '8px 4px', fontSize: '10px', fontWeight: '700', textAlign: 'center'}}>현재값</div>
                    <div className="header-cell" style={{padding: '8px 4px', fontSize: '10px', fontWeight: '700', textAlign: 'center'}}>품질</div>
                    <div className="header-cell" style={{padding: '8px 4px', fontSize: '10px', fontWeight: '700', textAlign: 'center'}}>타입</div>
                    <div className="header-cell" style={{padding: '8px 4px', fontSize: '10px', fontWeight: '700', textAlign: 'center'}}>시간</div>
                  </div>
                  <div className="data-table-body">
                    {filteredDataPoints.map((dataPoint, index) => (
                      <div key={dataPoint.key || `row-${index}`} className="data-table-row" style={{
                        display: 'grid',
                        gridTemplateColumns: '40px 200px 120px 100px 70px 60px 80px',
                        gap: '1px',
                        borderBottom: '1px solid #f1f5f9',
                        alignItems: 'center',
                        minHeight: '44px',
                        fontSize: '12px'
                      }}>
                        <div className="table-cell" style={{padding: '4px', textAlign: 'center'}}>
                          <input
                            type="checkbox"
                            checked={selectedDataPoints.some(dp => dp.key === dataPoint.key)}
                            onChange={() => handleDataPointSelect(dataPoint)}
                            style={{width: '12px', height: '12px'}}
                          />
                        </div>
                        <div className="table-cell" style={{padding: '6px', textAlign: 'left', overflow: 'hidden'}}>
                          <div style={{fontWeight: '600', fontSize: '11px', color: '#111827', marginBottom: '1px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap'}}>
                            {dataPoint.point_name || '[포인트명 없음]'}
                          </div>
                          <div style={{fontSize: '8px', color: '#6b7280', fontFamily: 'monospace', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap'}}>
                            {(dataPoint.key || '').replace('device:', '').replace(/:/g, '/')}
                          </div>
                        </div>
                        <div className="table-cell" style={{padding: '4px', textAlign: 'center', fontSize: '10px', color: '#374151'}}>
                          <div style={{overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap'}}>
                            {(dataPoint.device_name || '').replace('-CTRL-', '').replace('-01', '')}
                          </div>
                        </div>
                        <div className="table-cell" style={{padding: '4px', textAlign: 'right'}}>
                          <span style={{
                            fontFamily: 'monospace',
                            fontWeight: '600',
                            fontSize: '11px',
                            padding: '2px 6px',
                            borderRadius: '3px',
                            backgroundColor: dataPoint.quality === 'good' ? '#dcfce7' : 
                                            dataPoint.quality === 'bad' ? '#fee2e2' : 
                                            dataPoint.quality === 'uncertain' ? '#fef3c7' : 
                                            dataPoint.quality === 'comm_failure' ? '#fee2e2' : '#f3f4f6',
                            color: dataPoint.quality === 'good' ? '#166534' : 
                                   dataPoint.quality === 'bad' ? '#dc2626' : 
                                   dataPoint.quality === 'uncertain' ? '#92400e' : 
                                   dataPoint.quality === 'comm_failure' ? '#dc2626' : '#374151',
                            display: 'block'
                          }}>
                            {String(dataPoint.value || '—')}
                            {dataPoint.unit && <span style={{fontSize: '9px', marginLeft: '1px'}}>{dataPoint.unit}</span>}
                          </span>
                        </div>
                        <div className="table-cell" style={{padding: '4px', textAlign: 'center'}}>
                          <span style={{
                            padding: '2px 4px',
                            borderRadius: '8px',
                            fontSize: '8px',
                            fontWeight: '700',
                            textTransform: 'uppercase',
                            backgroundColor: dataPoint.quality === 'good' ? '#dcfce7' : 
                                            dataPoint.quality === 'bad' ? '#fee2e2' : 
                                            dataPoint.quality === 'uncertain' ? '#fef3c7' : 
                                            dataPoint.quality === 'comm_failure' ? '#fee2e2' : 
                                            dataPoint.quality === 'last_known' ? '#f3f4f6' : '#f3f4f6',
                            color: dataPoint.quality === 'good' ? '#166534' : 
                                   dataPoint.quality === 'bad' ? '#dc2626' : 
                                   dataPoint.quality === 'uncertain' ? '#92400e' : 
                                   dataPoint.quality === 'comm_failure' ? '#dc2626' : '#374151'
                          }}>
                            {dataPoint.quality === 'good' ? 'OK' :
                             dataPoint.quality === 'comm_failure' ? 'ERR' : 
                             dataPoint.quality === 'last_known' ? 'OLD' : 
                             dataPoint.quality === 'uncertain' ? '?' : 
                             dataPoint.quality || '—'}
                          </span>
                        </div>
                        <div className="table-cell" style={{padding: '4px', textAlign: 'center'}}>
                          <span style={{
                            fontSize: '8px',
                            color: '#6b7280',
                            textTransform: 'uppercase',
                            fontWeight: '600',
                            background: '#f3f4f6',
                            padding: '2px 4px',
                            borderRadius: '2px'
                          }}>
                            {dataPoint.data_type === 'number' ? 'NUM' :
                             dataPoint.data_type === 'boolean' ? 'BOOL' :
                             dataPoint.data_type === 'integer' ? 'INT' :
                             dataPoint.data_type === 'string' ? 'STR' : 'UNK'}
                          </span>
                        </div>
                        <div className="table-cell" style={{padding: '4px', textAlign: 'center'}}>
                          <span style={{
                            fontSize: '8px',
                            color: '#6b7280',
                            fontFamily: 'monospace'
                          }}>
                            {dataPoint.timestamp ? 
                              new Date(dataPoint.timestamp).toLocaleTimeString('ko-KR', {
                                hour: '2-digit',
                                minute: '2-digit'
                              }) : '—'}
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