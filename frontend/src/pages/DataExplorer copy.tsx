import React, { useState, useEffect, useCallback } from 'react';
import '../styles/base.css';
import '../styles/data-explorer.css';

interface DataPoint {
  id: string;
  key: string;
  name: string;
  value: any;
  dataType: 'string' | 'number' | 'boolean' | 'object' | 'array';
  unit?: string;
  timestamp: Date;
  quality: 'good' | 'bad' | 'uncertain';
  size: number;
  ttl?: number;
}

interface TreeNode {
  id: string;
  name: string;
  path: string;
  type: 'tenant' | 'factory' | 'device' | 'folder' | 'datapoint';
  children?: TreeNode[];
  isExpanded: boolean;
  isLoaded: boolean;
  dataPoint?: DataPoint;
  childCount?: number;
}

const DataExplorer: React.FC = () => {
  const [treeData, setTreeData] = useState<TreeNode[]>([]);
  const [selectedNode, setSelectedNode] = useState<TreeNode | null>(null);
  const [selectedDataPoints, setSelectedDataPoints] = useState<DataPoint[]>([]);
  const [searchTerm, setSearchTerm] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'disconnected' | 'connecting'>('connected');
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5000);

  // 초기 트리 데이터 로드
  useEffect(() => {
    loadInitialTree();
  }, []);

  // 실시간 업데이트
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      if (selectedDataPoints.length > 0) {
        refreshSelectedDataPoints();
      }
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, selectedDataPoints]);

  const loadInitialTree = async () => {
    setIsLoading(true);
    try {
      // 시뮬레이션 데이터
      const initialTree: TreeNode[] = [
        {
          id: 'tenant_samsung',
          name: 'Samsung Electronics',
          path: 'pulseone:samsung',
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 3
        },
        {
          id: 'tenant_lg',
          name: 'LG Electronics',
          path: 'pulseone:lg',
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 2
        },
        {
          id: 'tenant_hyundai',
          name: 'Hyundai Motor',
          path: 'pulseone:hyundai',
          type: 'tenant',
          isExpanded: false,
          isLoaded: false,
          childCount: 4
        }
      ];
      
      setTreeData(initialTree);
    } catch (error) {
      console.error('Failed to load initial tree:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const loadNodeChildren = async (node: TreeNode) => {
    setIsLoading(true);
    try {
      let children: TreeNode[] = [];
      
      // 시뮬레이션 데이터 생성
      if (node.type === 'tenant') {
        children = [
          {
            id: `${node.id}_factory_seoul`,
            name: 'Seoul Factory',
            path: `${node.path}:seoul_fab`,
            type: 'factory',
            isExpanded: false,
            isLoaded: false,
            childCount: 5
          },
          {
            id: `${node.id}_factory_busan`,
            name: 'Busan Factory',
            path: `${node.path}:busan_fab`,
            type: 'factory',
            isExpanded: false,
            isLoaded: false,
            childCount: 3
          }
        ];
      } else if (node.type === 'factory') {
        children = [
          {
            id: `${node.id}_device_plc001`,
            name: 'Main PLC #001',
            path: `${node.path}:plc001`,
            type: 'device',
            isExpanded: false,
            isLoaded: false,
            childCount: 15
          },
          {
            id: `${node.id}_device_plc002`,
            name: 'Backup PLC #002',
            path: `${node.path}:plc002`,
            type: 'device',
            isExpanded: false,
            isLoaded: false,
            childCount: 12
          },
          {
            id: `${node.id}_device_hmi001`,
            name: 'Operator HMI #001',
            path: `${node.path}:hmi001`,
            type: 'device',
            isExpanded: false,
            isLoaded: false,
            childCount: 8
          }
        ];
      } else if (node.type === 'device') {
        // 데이터 포인트들
        const dataPointNames = [
          'temperature_01', 'temperature_02', 'pressure_main', 'pressure_backup',
          'motor_speed', 'motor_current', 'vibration_x', 'vibration_y',
          'flow_rate', 'level_tank1', 'level_tank2', 'ph_value',
          'conductivity', 'turbidity', 'alarm_status'
        ];
        
        children = dataPointNames.slice(0, node.childCount || 10).map((name, index) => ({
          id: `${node.id}_dp_${name}`,
          name: name.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
          path: `${node.path}:${name}`,
          type: 'datapoint',
          isExpanded: false,
          isLoaded: true,
          dataPoint: generateDataPoint(`${node.path}:${name}`, name)
        }));
      }

      // 트리 데이터 업데이트
      setTreeData(prev => updateTreeNode(prev, node.id, { 
        children, 
        isLoaded: true, 
        isExpanded: true 
      }));
      
    } catch (error) {
      console.error('Failed to load node children:', error);
    } finally {
      setIsLoading(false);
    }
  };

  const generateDataPoint = (key: string, name: string): DataPoint => {
    const dataTypes = ['number', 'boolean', 'string'] as const;
    const qualities = ['good', 'bad', 'uncertain'] as const;
    const units = ['°C', 'bar', 'rpm', 'A', 'mm', 'pH', '%', 'L/min'];
    
    const dataType = dataTypes[Math.floor(Math.random() * dataTypes.length)];
    let value: any;
    
    switch (dataType) {
      case 'number':
        value = (Math.random() * 100).toFixed(2);
        break;
      case 'boolean':
        value = Math.random() > 0.5;
        break;
      case 'string':
        value = `Status_${Math.floor(Math.random() * 10)}`;
        break;
    }
    
    return {
      id: key,
      key,
      name,
      value,
      dataType,
      unit: dataType === 'number' ? units[Math.floor(Math.random() * units.length)] : undefined,
      timestamp: new Date(),
      quality: qualities[Math.floor(Math.random() * qualities.length)],
      size: Math.floor(Math.random() * 1024) + 64,
      ttl: Math.random() > 0.7 ? Math.floor(Math.random() * 3600) : undefined
    };
  };

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

  const handleNodeClick = (node: TreeNode) => {
    setSelectedNode(node);
    
    if (node.type === 'datapoint' && node.dataPoint) {
      // 데이터 포인트 선택 시 상세 정보 표시
      setSelectedDataPoints([node.dataPoint]);
    } else if (!node.isLoaded && node.childCount && node.childCount > 0) {
      // 자식 노드 로드
      loadNodeChildren(node);
    } else if (node.isLoaded) {
      // 확장/축소 토글
      setTreeData(prev => updateTreeNode(prev, node.id, { 
        isExpanded: !node.isExpanded 
      }));
    }
  };

  const handleDataPointSelect = (dataPoint: DataPoint) => {
    setSelectedDataPoints(prev => {
      const exists = prev.find(dp => dp.id === dataPoint.id);
      if (exists) {
        return prev.filter(dp => dp.id !== dataPoint.id);
      } else {
        return [...prev, dataPoint];
      }
    });
  };

  const refreshSelectedDataPoints = async () => {
    if (selectedDataPoints.length === 0) return;
    
    setSelectedDataPoints(prev => prev.map(dp => ({
      ...dp,
      value: generateDataPoint(dp.key, dp.name).value,
      timestamp: new Date(),
      quality: Math.random() > 0.9 ? 'uncertain' : 'good'
    })));
  };

  const clearSelection = () => {
    setSelectedDataPoints([]);
  };

  const exportData = () => {
    const data = selectedDataPoints.map(dp => ({
      key: dp.key,
      name: dp.name,
      value: dp.value,
      unit: dp.unit,
      timestamp: dp.timestamp.toISOString(),
      quality: dp.quality
    }));
    
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `redis_data_${new Date().toISOString().split('T')[0]}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const renderTreeNode = (node: TreeNode, level: number = 0): React.ReactNode => {
    const hasChildren = node.childCount && node.childCount > 0;
    const isExpanded = node.isExpanded && node.children;
    
    return (
      <div key={node.id} className="tree-node-container">
        <div 
          className={`tree-node ${selectedNode?.id === node.id ? 'selected' : ''}`}
          style={{ paddingLeft: `${level * 1.5}rem` }}
          onClick={() => handleNodeClick(node)}
        >
          <div className="tree-node-content">
            {hasChildren && (
              <i className={`tree-expand-icon fas ${isExpanded ? 'fa-chevron-down' : 'fa-chevron-right'}`}></i>
            )}
            
            <i className={`tree-node-icon ${getNodeIcon(node.type)}`}></i>
            
            <span className="tree-node-label">{node.name}</span>
            
            {node.type === 'datapoint' && node.dataPoint && (
              <div className="data-point-preview">
                <span className={`data-value ${node.dataPoint.quality}`}>
                  {formatDataValue(node.dataPoint)}
                </span>
                <span className={`quality-indicator ${node.dataPoint.quality}`}></span>
              </div>
            )}
            
            {hasChildren && (
              <span className="child-count">({node.childCount})</span>
            )}
          </div>
        </div>
        
        {isExpanded && node.children && (
          <div className="tree-children">
            {node.children.map(child => renderTreeNode(child, level + 1))}
          </div>
        )}
      </div>
    );
  };

  const getNodeIcon = (type: string): string => {
    switch (type) {
      case 'tenant': return 'fas fa-building';
      case 'factory': return 'fas fa-industry';
      case 'device': return 'fas fa-microchip';
      case 'folder': return 'fas fa-folder';
      case 'datapoint': return 'fas fa-chart-line';
      default: return 'fas fa-file';
    }
  };

  const formatDataValue = (dataPoint: DataPoint): string => {
    if (dataPoint.dataType === 'boolean') {
      return dataPoint.value ? 'TRUE' : 'FALSE';
    }
    if (dataPoint.dataType === 'number') {
      return `${dataPoint.value}${dataPoint.unit || ''}`;
    }
    return String(dataPoint.value);
  };

  const formatTimestamp = (timestamp: Date): string => {
    return timestamp.toLocaleString('ko-KR', {
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  const filteredTreeData = searchTerm 
    ? treeData.filter(node => 
        node.name.toLowerCase().includes(searchTerm.toLowerCase())
      )
    : treeData;

  return (
    <div className="data-explorer-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">데이터 익스플로러</h1>
        <div className="page-actions">
          <div className={`connection-status ${connectionStatus}`}>
            <i className={`fas ${connectionStatus === 'connected' ? 'fa-link' : 'fa-unlink'}`}></i>
            <span className="status-text">
              {connectionStatus === 'connected' ? 'Redis 연결됨' : 'Redis 연결 끊김'}
            </span>
          </div>
          <button 
            className="btn btn-outline"
            onClick={loadInitialTree}
            disabled={isLoading}
          >
            <i className="fas fa-sync-alt"></i>
            새로고침
          </button>
        </div>
      </div>

      <div className="explorer-layout">
        {/* 좌측 트리 패널 */}
        <div className="tree-panel">
          <div className="tree-header">
            <h3>Redis 키 탐색기</h3>
            <div className="tree-controls">
              <div className="search-container">
                <input
                  type="text"
                  placeholder="키 검색..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="search-input"
                />
                <i className="fas fa-search search-icon"></i>
              </div>
            </div>
          </div>
          
          <div className="tree-content">
            {isLoading && <div className="loading-indicator">로딩 중...</div>}
            {filteredTreeData.map(node => renderTreeNode(node))}
          </div>
        </div>

        {/* 우측 상세 패널 */}
        <div className="details-panel">
          <div className="details-header">
            <h3>데이터 상세 정보</h3>
            <div className="details-controls">
              <label className="refresh-control">
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
                  <option value={1000}>1초</option>
                  <option value={5000}>5초</option>
                  <option value={10000}>10초</option>
                  <option value={30000}>30초</option>
                </select>
              )}
              {selectedDataPoints.length > 0 && (
                <>
                  <button className="btn btn-sm btn-outline" onClick={clearSelection}>
                    <i className="fas fa-times"></i>
                    선택 해제
                  </button>
                  <button className="btn btn-sm btn-primary" onClick={exportData}>
                    <i className="fas fa-download"></i>
                    내보내기
                  </button>
                </>
              )}
            </div>
          </div>

          <div className="details-content">
            {selectedNode ? (
              <div className="node-details">
                <div className="node-info">
                  <h4>
                    <i className={`${getNodeIcon(selectedNode.type)} node-type-icon`}></i>
                    {selectedNode.name}
                  </h4>
                  <div className="node-metadata">
                    <div className="metadata-item">
                      <span className="label">경로:</span>
                      <span className="value monospace">{selectedNode.path}</span>
                    </div>
                    <div className="metadata-item">
                      <span className="label">타입:</span>
                      <span className="value">{selectedNode.type}</span>
                    </div>
                    {selectedNode.childCount && (
                      <div className="metadata-item">
                        <span className="label">자식 수:</span>
                        <span className="value">{selectedNode.childCount}</span>
                      </div>
                    )}
                  </div>
                </div>

                {selectedNode.type === 'datapoint' && selectedNode.dataPoint && (
                  <div className="datapoint-details">
                    <div className="datapoint-card">
                      <div className="datapoint-header">
                        <h5>실시간 값</h5>
                        <button
                          className={`watch-btn ${selectedDataPoints.some(dp => dp.id === selectedNode.dataPoint!.id) ? 'active' : ''}`}
                          onClick={() => handleDataPointSelect(selectedNode.dataPoint!)}
                        >
                          <i className="fas fa-eye"></i>
                          감시
                        </button>
                      </div>
                      <div className="datapoint-value">
                        <span className={`value ${selectedNode.dataPoint.quality}`}>
                          {formatDataValue(selectedNode.dataPoint)}
                        </span>
                        <span className={`quality-badge ${selectedNode.dataPoint.quality}`}>
                          {selectedNode.dataPoint.quality}
                        </span>
                      </div>
                      <div className="datapoint-metadata">
                        <div className="meta-row">
                          <span>데이터 타입:</span>
                          <span>{selectedNode.dataPoint.dataType}</span>
                        </div>
                        <div className="meta-row">
                          <span>마지막 업데이트:</span>
                          <span>{formatTimestamp(selectedNode.dataPoint.timestamp)}</span>
                        </div>
                        <div className="meta-row">
                          <span>크기:</span>
                          <span>{selectedNode.dataPoint.size} bytes</span>
                        </div>
                        {selectedNode.dataPoint.ttl && (
                          <div className="meta-row">
                            <span>TTL:</span>
                            <span>{selectedNode.dataPoint.ttl}초</span>
                          </div>
                        )}
                      </div>
                    </div>
                  </div>
                )}
              </div>
            ) : (
              <div className="no-selection">
                <i className="fas fa-mouse-pointer empty-icon"></i>
                <p>탐색기에서 항목을 선택하세요</p>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* 하단 감시 패널 */}
      {selectedDataPoints.length > 0 && (
        <div className="watch-panel">
          <div className="watch-header">
            <h3>실시간 감시 ({selectedDataPoints.length}개)</h3>
            <div className="watch-controls">
              <span className="last-update">
                마지막 업데이트: {formatTimestamp(new Date())}
              </span>
            </div>
          </div>
          <div className="watch-content">
            <div className="watch-table">
              <div className="watch-table-header">
                <div className="watch-cell">키</div>
                <div className="watch-cell">이름</div>
                <div className="watch-cell">값</div>
                <div className="watch-cell">품질</div>
                <div className="watch-cell">타임스탬프</div>
                <div className="watch-cell">동작</div>
              </div>
              {selectedDataPoints.map(dataPoint => (
                <div key={dataPoint.id} className="watch-table-row">
                  <div className="watch-cell monospace">{dataPoint.key}</div>
                  <div className="watch-cell">{dataPoint.name}</div>
                  <div className="watch-cell">
                    <span className={`data-value ${dataPoint.quality}`}>
                      {formatDataValue(dataPoint)}
                    </span>
                  </div>
                  <div className="watch-cell">
                    <span className={`quality-badge ${dataPoint.quality}`}>
                      {dataPoint.quality}
                    </span>
                  </div>
                  <div className="watch-cell monospace">
                    {formatTimestamp(dataPoint.timestamp)}
                  </div>
                  <div className="watch-cell">
                    <button
                      className="btn btn-sm btn-error"
                      onClick={() => handleDataPointSelect(dataPoint)}
                    >
                      <i className="fas fa-times"></i>
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default DataExplorer;

