import React, { useState, useEffect, useCallback, useMemo } from 'react';

// ============================================================================
// 타입 정의
// ============================================================================

interface RedisDataPoint {
  key: string;
  point_id: number;
  device_id: number;
  device_name: string;
  point_name: string;
  value: any;
  timestamp: string;
  quality: 'good' | 'bad' | 'uncertain';
  data_type: string;
  unit?: string;
  value_changed?: boolean;
  ttl?: number;
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
  deviceInfo?: any;
  lastUpdate?: string;
  connectionStatus?: 'connected' | 'disconnected' | 'error';
}

interface FilterState {
  search: string;
  dataType: string;
  quality: string;
  enabled: string;
  device: string;
  realTimeOnly: boolean;
}

const DataExplorer: React.FC = () => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  
  const [treeData, setTreeData] = useState<TreeNode[]>([]);
  const [selectedNode, setSelectedNode] = useState<TreeNode | null>(null);
  const [selectedDataPoints, setSelectedDataPoints] = useState<RedisDataPoint[]>([]);
  const [realtimeData, setRealtimeData] = useState<RedisDataPoint[]>([]);
  const [devices, setDevices] = useState<any[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'connecting' | 'disconnected'>('disconnected');
  const [lastRefresh, setLastRefresh] = useState<Date>(new Date());
  const [filters, setFilters] = useState<FilterState>({
    search: '',
    dataType: 'all',
    quality: 'all',
    enabled: 'all',
    device: 'all',
    realTimeOnly: false
  });
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(3);
  const [showChart, setShowChart] = useState(false);

  // ========================================================================
  // Mock 데이터 생성
  // ========================================================================
  
  const generateMockTreeData = useCallback((): TreeNode[] => {
    return [
      {
        id: 'tenant-1',
        label: 'PulseOne Factory',
        type: 'tenant',
        level: 0,
        isExpanded: true,
        isLoaded: true,
        children: [
          {
            id: 'site-1',
            label: 'Factory A - Building 1',
            type: 'site',
            level: 1,
            isExpanded: true,
            isLoaded: true,
            children: [
              {
                id: 'device-1',
                label: 'PLC-001',
                type: 'device',
                level: 2,
                isExpanded: true,
                isLoaded: true,
                connectionStatus: 'connected',
                lastUpdate: new Date().toISOString(),
                children: [
                  {
                    id: 'point-1',
                    label: 'Temperature',
                    type: 'datapoint',
                    level: 3,
                    isExpanded: false,
                    isLoaded: true,
                    dataPoint: {
                      key: 'pulseone:device:1:temperature',
                      point_id: 1,
                      device_id: 1,
                      device_name: 'PLC-001',
                      point_name: 'Temperature',
                      value: (Math.random() * 50 + 20).toFixed(1),
                      timestamp: new Date().toISOString(),
                      quality: Math.random() > 0.9 ? 'uncertain' : 'good',
                      data_type: 'number',
                      unit: '°C',
                      value_changed: Math.random() > 0.7
                    }
                  },
                  {
                    id: 'point-2',
                    label: 'Pressure',
                    type: 'datapoint',
                    level: 3,
                    isExpanded: false,
                    isLoaded: true,
                    dataPoint: {
                      key: 'pulseone:device:1:pressure',
                      point_id: 2,
                      device_id: 1,
                      device_name: 'PLC-001',
                      point_name: 'Pressure',
                      value: (Math.random() * 5 + 1).toFixed(2),
                      timestamp: new Date().toISOString(),
                      quality: 'good',
                      data_type: 'number',
                      unit: 'bar',
                      value_changed: Math.random() > 0.5
                    }
                  },
                  {
                    id: 'point-3',
                    label: 'Motor Status',
                    type: 'datapoint',
                    level: 3,
                    isExpanded: false,
                    isLoaded: true,
                    dataPoint: {
                      key: 'pulseone:device:1:motor_status',
                      point_id: 3,
                      device_id: 1,
                      device_name: 'PLC-001',
                      point_name: 'Motor Status',
                      value: Math.random() > 0.5 ? 'ON' : 'OFF',
                      timestamp: new Date().toISOString(),
                      quality: 'good',
                      data_type: 'boolean',
                      value_changed: Math.random() > 0.8
                    }
                  }
                ]
              },
              {
                id: 'device-2',
                label: 'HMI-001',
                type: 'device',
                level: 2,
                isExpanded: false,
                isLoaded: true,
                connectionStatus: 'connected',
                lastUpdate: new Date().toISOString(),
                childCount: 3
              },
              {
                id: 'device-3',
                label: 'SENSOR-001',
                type: 'device',
                level: 2,
                isExpanded: false,
                isLoaded: true,
                connectionStatus: 'error',
                lastUpdate: new Date(Date.now() - 300000).toISOString(),
                childCount: 2
              }
            ]
          },
          {
            id: 'site-2',
            label: 'Factory B - Building 2',
            type: 'site',
            level: 1,
            isExpanded: false,
            isLoaded: true,
            childCount: 4
          }
        ]
      }
    ];
  }, []);

  const generateMockRealtimeData = useCallback((): RedisDataPoint[] => {
    const points: RedisDataPoint[] = [];
    for (let i = 1; i <= 15; i++) {
      points.push({
        key: `pulseone:device:${Math.ceil(i/5)}:point_${i}`,
        point_id: i,
        device_id: Math.ceil(i/5),
        device_name: `PLC-00${Math.ceil(i/5)}`,
        point_name: ['Temperature', 'Pressure', 'Flow', 'Level', 'Vibration'][i % 5],
        value: Math.random() > 0.5 ? (Math.random() * 100).toFixed(2) : Math.random() > 0.5 ? 'ON' : 'OFF',
        timestamp: new Date(Date.now() - Math.random() * 60000).toISOString(),
        quality: Math.random() > 0.1 ? 'good' : Math.random() > 0.5 ? 'uncertain' : 'bad',
        data_type: ['number', 'boolean', 'string'][Math.floor(Math.random() * 3)],
        unit: ['°C', 'bar', 'L/min', 'mm', 'Hz'][Math.floor(Math.random() * 5)],
        value_changed: Math.random() > 0.6,
        ttl: Math.floor(Math.random() * 3600)
      });
    }
    return points;
  }, []);

  // ========================================================================
  // 데이터 로딩 함수들
  // ========================================================================

  const loadTreeData = useCallback(async () => {
    setIsLoading(true);
    try {
      await new Promise(resolve => setTimeout(resolve, 500));
      const mockData = generateMockTreeData();
      setTreeData(mockData);
      setConnectionStatus('connected');
      console.log('✅ 트리 데이터 로드 완료');
    } catch (err) {
      setError('트리 데이터 로드 실패');
      setConnectionStatus('disconnected');
    } finally {
      setIsLoading(false);
    }
  }, [generateMockTreeData]);

  const loadRealtimeData = useCallback(async () => {
    try {
      const mockData = generateMockRealtimeData();
      setRealtimeData(mockData);
      setLastRefresh(new Date());
      console.log('✅ 실시간 데이터 갱신');
    } catch (err) {
      console.warn('⚠️ 실시간 데이터 갱신 실패:', err);
    }
  }, [generateMockRealtimeData]);

  const loadNodeChildren = useCallback(async (node: TreeNode) => {
    console.log('🔄 자식 노드 로드:', node.label);
    
    const mockChildren: TreeNode[] = [];
    if (node.type === 'device') {
      for (let i = 1; i <= (node.childCount || 3); i++) {
        mockChildren.push({
          id: `${node.id}-point-${i}`,
          label: `Point ${i}`,
          type: 'datapoint',
          level: node.level + 1,
          isExpanded: false,
          isLoaded: true,
          dataPoint: {
            key: `pulseone:device:${node.id}:point_${i}`,
            point_id: parseInt(node.id.split('-')[1]) * 10 + i,
            device_id: parseInt(node.id.split('-')[1]),
            device_name: node.label,
            point_name: `Point ${i}`,
            value: (Math.random() * 100).toFixed(1),
            timestamp: new Date().toISOString(),
            quality: 'good',
            data_type: 'number'
          }
        });
      }
    }
    
    setTreeData(prev => updateTreeNode(prev, node.id, { 
      children: mockChildren, 
      isLoaded: true, 
      isExpanded: true 
    }));
  }, []);

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
    
    if (filters.realTimeOnly) {
      const fiveMinutesAgo = Date.now() - (5 * 60 * 1000);
      points = points.filter(dp => new Date(dp.timestamp).getTime() > fiveMinutesAgo);
    }
    
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
      const deviceDataPoints = findAllDataPoints([node]);
      setSelectedDataPoints(deviceDataPoints);
      
      if (!node.isLoaded && node.childCount && node.childCount > 0) {
        loadNodeChildren(node);
      }
    } else {
      setTreeData(prev => updateTreeNode(prev, node.id, { 
        isExpanded: !node.isExpanded 
      }));
    }
  }, [findAllDataPoints, loadNodeChildren]);

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
        
        setTreeData(prev => {
          const updateDataPoints = (nodes: TreeNode[]): TreeNode[] => {
            return nodes.map(node => {
              if (node.type === 'datapoint' && node.dataPoint) {
                return {
                  ...node,
                  dataPoint: {
                    ...node.dataPoint,
                    value: node.dataPoint.data_type === 'number' 
                      ? (Math.random() * 100).toFixed(1)
                      : Math.random() > 0.5 ? 'ON' : 'OFF',
                    timestamp: new Date().toISOString(),
                    quality: Math.random() > 0.9 ? 'uncertain' : 'good',
                    value_changed: Math.random() > 0.7
                  }
                };
              }
              if (node.children) {
                return { ...node, children: updateDataPoints(node.children) };
              }
              return node;
            });
          };
          return updateDataPoints(prev);
        });
        
      }, refreshInterval * 1000);
      
      return () => clearInterval(interval);
    }
  }, [autoRefresh, refreshInterval, loadRealtimeData]);

  // ========================================================================
  // 초기 로딩
  // ========================================================================

  useEffect(() => {
    const init = async () => {
      await Promise.all([
        loadTreeData(),
        loadRealtimeData()
      ]);
    };
    init();
  }, [loadTreeData, loadRealtimeData]);

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
              {node.type === 'device' && (
                <span className={`device-status ${node.connectionStatus}`}>
                  📱
                </span>
              )}
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
                {node.dataPoint.value_changed && <span className="change-indicator">🔄</span>}
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
                {connectionStatus === 'connected' && 'Redis 연결됨'}
                {connectionStatus === 'connecting' && 'Redis 연결중'}
                {connectionStatus === 'disconnected' && 'Redis 연결 끊김'}
              </span>
              <span className="stats-info">
                ({filteredDataPoints.length}개 포인트)
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
                <div className="loading-text">트리 로딩 중...</div>
              </div>
            ) : treeData.length === 0 ? (
              <div className="empty-state">
                <div className="empty-state-icon">
                  <i className="fas fa-database"></i>
                </div>
                <h3 className="empty-state-title">데이터가 없습니다</h3>
                <p className="empty-state-description">
                  Redis 연결을 확인하고 새로고침해보세요
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
                </select>

                <label className="realtime-filter">
                  <input
                    type="checkbox"
                    checked={filters.realTimeOnly}
                    onChange={(e) => setFilters(prev => ({ ...prev, realTimeOnly: e.target.checked }))}
                  />
                  실시간만
                </label>
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
                  {selectedNode.type === 'device' && (
                    <>
                      <div className="metadata-item">
                        <span className="label">연결 상태:</span>
                        <span className={`value status-${selectedNode.connectionStatus}`}>
                          {selectedNode.connectionStatus}
                        </span>
                      </div>
                      <div className="metadata-item">
                        <span className="label">마지막 통신:</span>
                        <span className="value">
                          {selectedNode.lastUpdate ? formatTimestamp(selectedNode.lastUpdate) : 'N/A'}
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
                        <span className="label">데이터 타입:</span>
                        <span className="value">{selectedNode.dataPoint.data_type}</span>
                      </div>
                    </>
                  )}
                </div>
              </div>
            )}

            {/* 차트 영역 (InfluxDB 연동) */}
            {showChart && selectedDataPoints.length > 0 && (
              <div className="chart-section">
                <h4>📈 실시간 트렌드 (최근 5분)</h4>
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

            {/* 실시간 데이터 테이블 */}
            <div className="realtime-data">
              <h4>⚡ 실시간 데이터 ({filteredDataPoints.length}개)</h4>
              
              {filteredDataPoints.length === 0 ? (
                <div className="empty-state">
                  <p>표시할 데이터가 없습니다</p>
                </div>
              ) : (
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
                    {filteredDataPoints.map((dataPoint) => (
                      <div key={dataPoint.key} className="data-table-row">
                        <div className="table-cell">
                          <input
                            type="checkbox"
                            checked={selectedDataPoints.some(dp => dp.key === dataPoint.key)}
                            onChange={() => handleDataPointSelect(dataPoint)}
                          />
                        </div>
                        <div className="table-cell">
                          <div className="point-info">
                            <div className="point-name">{dataPoint.point_name}</div>
                            <div className="point-key">{dataPoint.key}</div>
                          </div>
                        </div>
                        <div className="table-cell">
                          <span className="device-name">{dataPoint.device_name}</span>
                        </div>
                        <div className="table-cell">
                          <div className="value-display">
                            <span className={`value ${dataPoint.quality}`}>
                              {dataPoint.value}
                            </span>
                            {dataPoint.unit && <span className="unit">{dataPoint.unit}</span>}
                            {dataPoint.value_changed && <span className="change-indicator">🔄</span>}
                          </div>
                        </div>
                        <div className="table-cell">
                          <span className={`quality-badge ${dataPoint.quality}`}>
                            {dataPoint.quality}
                          </span>
                        </div>
                        <div className="table-cell">
                          <span className="data-type">{dataPoint.data_type}</span>
                        </div>
                        <div className="table-cell">
                          <span className="timestamp">{formatTimestamp(dataPoint.timestamp)}</span>
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

      {/* 스타일 */}
      <style jsx>{`
        .data-explorer-container {
          min-height: 100vh;
          background-color: #f8fafc;
          padding: 0;
          margin: 0;
        }

        .page-header {
          display: flex;
          justify-content: space-between;
          align-items: flex-start;
          padding: 24px 32px;
          background: #ffffff;
          border-bottom: 1px solid #e5e7eb;
          margin-bottom: 24px;
          box-shadow: 0 1px 3px 0 rgba(0, 0, 0, 0.1);
        }

        .header-left {
          display: flex;
          flex-direction: column;
          gap: 8px;
        }

        .page-title {
          font-size: 24px;
          font-weight: 700;
          color: #111827;
          margin: 0;
          display: flex;
          align-items: center;
          gap: 12px;
        }

        .page-title i {
          color: #10b981;
          font-size: 20px;
        }

        .header-meta {
          display: flex;
          align-items: center;
          gap: 16px;
          font-size: 14px;
          color: #6b7280;
        }

        .connection-status {
          display: flex;
          align-items: center;
          gap: 8px;
          padding: 6px 12px;
          border-radius: 6px;
          font-size: 13px;
          font-weight: 500;
        }

        .connection-status.status-connected {
          background-color: #d4edda;
          color: #155724;
          border: 1px solid #c3e6cb;
        }

        .connection-status.status-connecting {
          background-color: #fff3cd;
          color: #856404;
          border: 1px solid #ffeaa7;
        }

        .connection-status.status-disconnected {
          background-color: #f8d7da;
          color: #721c24;
          border: 1px solid #f5c6cb;
        }

        .page-actions {
          display: flex;
          align-items: center;
          gap: 16px;
        }

        .auto-refresh-control {
          display: flex;
          align-items: center;
          gap: 8px;
        }

        .refresh-toggle {
          display: flex;
          align-items: center;
          gap: 6px;
          font-size: 14px;
          cursor: pointer;
        }

        .refresh-interval {
          padding: 4px 8px;
          border: 1px solid #d1d5db;
          border-radius: 4px;
          font-size: 12px;
          background: white;
        }

        .error-banner {
          background-color: #f8d7da;
          border: 1px solid #f5c6cb;
          border-radius: 6px;
          margin: 0 32px 16px 32px;
        }

        .error-content {
          display: flex;
          align-items: center;
          gap: 12px;
          padding: 12px 16px;
        }

        .error-message {
          flex: 1;
          color: #721c24;
          font-size: 14px;
          font-weight: 500;
        }

        .error-retry {
          background-color: #dc3545;
          color: white;
          border: none;
          padding: 6px 12px;
          border-radius: 4px;
          font-size: 12px;
          cursor: pointer;
        }

        .explorer-layout {
          display: grid;
          grid-template-columns: 400px 1fr;
          gap: 24px;
          height: calc(100vh - 200px);
          min-height: 600px;
          padding: 0 32px;
        }

        .tree-panel {
          background: #ffffff;
          border: 1px solid #e5e7eb;
          border-radius: 8px;
          overflow: hidden;
          display: flex;
          flex-direction: column;
          box-shadow: 0 1px 3px 0 rgba(0, 0, 0, 0.1);
        }

        .tree-header {
          padding: 16px 20px;
          background: #f9fafb;
          border-bottom: 1px solid #e5e7eb;
        }

        .tree-header h3 {
          margin: 0 0 12px 0;
          font-size: 16px;
          font-weight: 600;
          color: #111827;
        }

        .search-container {
          display: flex;
          gap: 8px;
        }

        .search-input-wrapper {
          position: relative;
          flex: 1;
        }

        .search-icon {
          position: absolute;
          left: 10px;
          top: 50%;
          transform: translateY(-50%);
          color: #6b7280;
          font-size: 14px;
        }

        .search-input {
          width: 100%;
          padding: 8px 12px 8px 36px;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          font-size: 14px;
        }

        .search-input:focus {
          outline: none;
          border-color: #3b82f6;
          box-shadow: 0 0 0 1px #3b82f6;
        }

        .tree-content {
          flex: 1;
          overflow-y: auto;
          padding: 8px 0;
        }

        .tree-node {
          padding: 8px 12px;
          cursor: pointer;
          transition: all 0.2s ease;
          border-radius: 4px;
          margin: 0 8px;
        }

        .tree-node:hover {
          background: #f3f4f6;
        }

        .tree-node.selected {
          background: #dbeafe;
          color: #1d4ed8;
        }

        .tree-node-content {
          display: flex;
          align-items: center;
          gap: 8px;
        }

        .tree-expand-icon {
          width: 12px;
          color: #6b7280;
          font-size: 10px;
        }

        .tree-node-icon {
          width: 20px;
          font-size: 14px;
        }

        .device-status.connected {
          filter: grayscale(0);
        }

        .device-status.disconnected,
        .device-status.error {
          filter: grayscale(1);
        }

        .tree-node-label {
          flex: 1;
          font-size: 14px;
          font-weight: 500;
        }

        .data-point-preview {
          display: flex;
          align-items: center;
          gap: 6px;
          margin-left: auto;
        }

        .data-value {
          font-size: 12px;
          font-family: 'Courier New', monospace;
          padding: 2px 6px;
          border-radius: 4px;
          background: #f3f4f6;
        }

        .data-value.good {
          background: #dcfce7;
          color: #166534;
        }

        .data-value.bad {
          background: #fee2e2;
          color: #dc2626;
        }

        .data-value.uncertain {
          background: #fef3c7;
          color: #92400e;
        }

        .quality-indicator {
          width: 8px;
          height: 8px;
          border-radius: 50%;
        }

        .quality-indicator.good {
          background: #22c55e;
        }

        .quality-indicator.bad {
          background: #ef4444;
        }

        .quality-indicator.uncertain {
          background: #f59e0b;
        }

        .change-indicator {
          font-size: 10px;
          animation: spin 2s linear infinite;
        }

        .connection-badge {
          font-size: 12px;
        }

        .connection-badge.connected {
          color: #22c55e;
        }

        .connection-badge.disconnected {
          color: #6b7280;
        }

        .connection-badge.error {
          color: #ef4444;
        }

        .child-count {
          font-size: 12px;
          color: #6b7280;
          margin-left: auto;
        }

        .tree-children {
          margin-left: 8px;
        }

        .details-panel {
          background: #ffffff;
          border: 1px solid #e5e7eb;
          border-radius: 8px;
          overflow: hidden;
          display: flex;
          flex-direction: column;
          box-shadow: 0 1px 3px 0 rgba(0, 0, 0, 0.1);
        }

        .details-header {
          padding: 16px 20px;
          background: #f9fafb;
          border-bottom: 1px solid #e5e7eb;
          display: flex;
          justify-content: space-between;
          align-items: center;
          flex-wrap: wrap;
          gap: 12px;
        }

        .details-header h3 {
          margin: 0;
          font-size: 16px;
          font-weight: 600;
          color: #111827;
        }

        .details-controls {
          display: flex;
          gap: 12px;
          align-items: center;
          flex-wrap: wrap;
        }

        .filter-controls {
          display: flex;
          gap: 8px;
          align-items: center;
        }

        .filter-select {
          padding: 4px 8px;
          border: 1px solid #d1d5db;
          border-radius: 4px;
          font-size: 12px;
          background: white;
        }

        .realtime-filter {
          display: flex;
          align-items: center;
          gap: 4px;
          font-size: 12px;
          cursor: pointer;
        }

        .view-controls {
          display: flex;
          gap: 8px;
        }

        .details-content {
          flex: 1;
          overflow-y: auto;
          padding: 20px;
        }

        .node-info {
          margin-bottom: 24px;
        }

        .node-info h4 {
          margin: 0 0 12px 0;
          font-size: 18px;
          font-weight: 600;
          color: #111827;
          display: flex;
          align-items: center;
          gap: 8px;
        }

        .node-metadata {
          display: flex;
          flex-direction: column;
          gap: 8px;
        }

        .metadata-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 8px 0;
          border-bottom: 1px solid #f3f4f6;
        }

        .metadata-item:last-child {
          border-bottom: none;
        }

        .metadata-item .label {
          font-weight: 500;
          color: #374151;
          font-size: 14px;
        }

        .metadata-item .value {
          color: #6b7280;
          font-size: 13px;
        }

        .metadata-item .value.monospace {
          font-family: 'Courier New', monospace;
          background: #f3f4f6;
          padding: 2px 6px;
          border-radius: 4px;
          word-break: break-all;
        }

        .chart-section {
          margin-bottom: 24px;
        }

        .chart-section h4 {
          margin: 0 0 12px 0;
          font-size: 16px;
          font-weight: 600;
          color: #111827;
        }

        .chart-placeholder {
          border: 1px solid #e5e7eb;
          border-radius: 8px;
          overflow: hidden;
        }

        .chart-container {
          height: 200px;
          display: flex;
          align-items: center;
          justify-content: center;
          background: #f9fafb;
        }

        .chart-info {
          text-align: center;
          color: #6b7280;
        }

        .realtime-data h4 {
          margin: 0 0 16px 0;
          font-size: 16px;
          font-weight: 600;
          color: #111827;
        }

        .data-table-container {
          border: 1px solid #e5e7eb;
          border-radius: 8px;
          overflow: hidden;
        }

        .data-table-header {
          display: grid;
          grid-template-columns: 60px 2fr 1fr 1fr 80px 80px 120px;
          gap: 12px;
          padding: 12px 16px;
          background: #f9fafb;
          border-bottom: 1px solid #e5e7eb;
          font-weight: 600;
          font-size: 12px;
          color: #374151;
        }

        .data-table-body {
          max-height: 400px;
          overflow-y: auto;
        }

        .data-table-row {
          display: grid;
          grid-template-columns: 60px 2fr 1fr 1fr 80px 80px 120px;
          gap: 12px;
          padding: 12px 16px;
          border-bottom: 1px solid #f3f4f6;
          align-items: center;
        }

        .data-table-row:hover {
          background: #f9fafb;
        }

        .data-table-row:last-child {
          border-bottom: none;
        }

        .table-cell {
          font-size: 13px;
          overflow: hidden;
        }

        .point-info {
          display: flex;
          flex-direction: column;
          gap: 2px;
        }

        .point-name {
          font-weight: 500;
          color: #111827;
        }

        .point-key {
          font-size: 11px;
          color: #6b7280;
          font-family: 'Courier New', monospace;
        }

        .device-name {
          font-weight: 500;
          color: #374151;
        }

        .value-display {
          display: flex;
          align-items: center;
          gap: 4px;
        }

        .value {
          font-family: 'Courier New', monospace;
          font-weight: 600;
          padding: 2px 6px;
          border-radius: 4px;
        }

        .value.good {
          background: #dcfce7;
          color: #166534;
        }

        .value.bad {
          background: #fee2e2;
          color: #dc2626;
        }

        .value.uncertain {
          background: #fef3c7;
          color: #92400e;
        }

        .unit {
          font-size: 11px;
          color: #6b7280;
        }

        .quality-badge {
          padding: 2px 6px;
          border-radius: 4px;
          font-size: 10px;
          font-weight: 600;
          text-transform: uppercase;
        }

        .quality-badge.good {
          background: #dcfce7;
          color: #166534;
        }

        .quality-badge.bad {
          background: #fee2e2;
          color: #dc2626;
        }

        .quality-badge.uncertain {
          background: #fef3c7;
          color: #92400e;
        }

        .data-type {
          font-size: 11px;
          color: #6b7280;
          text-transform: uppercase;
        }

        .timestamp {
          font-size: 11px;
          color: #6b7280;
          font-family: 'Courier New', monospace;
        }

        .loading-container {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          height: 200px;
          color: #6b7280;
        }

        .loading-spinner {
          width: 32px;
          height: 32px;
          border: 3px solid #e5e7eb;
          border-top: 3px solid #3b82f6;
          border-radius: 50%;
          animation: spin 1s linear infinite;
          margin-bottom: 12px;
        }

        .loading-text {
          font-size: 14px;
          font-weight: 500;
        }

        .empty-state {
          text-align: center;
          padding: 40px 20px;
          color: #6b7280;
        }

        .empty-state-icon {
          font-size: 48px;
          margin-bottom: 16px;
          opacity: 0.5;
        }

        .empty-state-title {
          font-size: 18px;
          font-weight: 600;
          margin-bottom: 8px;
          color: #495057;
        }

        .empty-state-description {
          font-size: 14px;
          line-height: 1.5;
          max-width: 300px;
          margin: 0 auto;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 6px;
          padding: 8px 12px;
          border: none;
          border-radius: 6px;
          font-size: 14px;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
          text-decoration: none;
        }

        .btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        .btn-outline {
          background: #ffffff;
          color: #374151;
          border: 1px solid #d1d5db;
        }

        .btn-outline:hover:not(:disabled) {
          background: #f9fafb;
          border-color: #9ca3af;
        }

        .btn-primary {
          background: #3b82f6;
          color: white;
          border: 1px solid #3b82f6;
        }

        .btn-primary:hover:not(:disabled) {
          background: #2563eb;
          border-color: #2563eb;
        }

        .btn-sm {
          padding: 4px 8px;
          font-size: 12px;
          gap: 4px;
        }

        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }

        /* 스크롤바 스타일링 */
        .tree-content::-webkit-scrollbar,
        .details-content::-webkit-scrollbar,
        .data-table-body::-webkit-scrollbar {
          width: 8px;
        }

        .tree-content::-webkit-scrollbar-track,
        .details-content::-webkit-scrollbar-track,
        .data-table-body::-webkit-scrollbar-track {
          background: #f1f5f9;
        }

        .tree-content::-webkit-scrollbar-thumb,
        .details-content::-webkit-scrollbar-thumb,
        .data-table-body::-webkit-scrollbar-thumb {
          background: #cbd5e1;
          border-radius: 4px;
        }

        .tree-content::-webkit-scrollbar-thumb:hover,
        .details-content::-webkit-scrollbar-thumb:hover,
        .data-table-body::-webkit-scrollbar-thumb:hover {
          background: #94a3b8;
        }

        /* 반응형 */
        @media (max-width: 1200px) {
          .explorer-layout {
            grid-template-columns: 300px 1fr;
          }
        }

        @media (max-width: 968px) {
          .page-header {
            flex-direction: column;
            gap: 16px;
            align-items: stretch;
          }

          .page-actions {
            justify-content: space-between;
          }
        }

        @media (max-width: 768px) {
          .page-header {
            padding: 12px 16px;
          }

          .explorer-layout {
            grid-template-columns: 1fr;
            height: auto;
            padding: 0 16px;
          }

          .tree-panel {
            height: 400px;
          }

          .data-table-header,
          .data-table-row {
            grid-template-columns: 1fr;
            gap: 8px;
          }

          .table-cell {
            padding: 8px 0;
            border-bottom: 1px solid #f3f4f6;
          }

          .table-cell:last-child {
            border-bottom: none;
          }

          .details-controls {
            flex-direction: column;
            align-items: stretch;
            gap: 8px;
          }
        }
      `}</style>
    </div>
  );
};

export default DataExplorer;