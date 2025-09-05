import React, { useState, useEffect, useCallback, useMemo } from 'react';
import {
  RealtimeApiService,
  RealtimeValue,
  CurrentValuesResponse,
  ApiResponse
} from '../api/services/realtimeApi';
import {
  DeviceApiService,
  Device
} from '../api/services/deviceApi';
import { Pagination } from '../components/common/Pagination';
import { useDataExplorerPagination } from '../hooks/usePagination';
import '../styles/data-explorer.css';

// ============================================================================
// 인터페이스 - 백엔드 API 응답에 맞게 단순화
// ============================================================================

interface TreeNode {
  id: string;
  label: string;
  type: 'tenant' | 'site' | 'device' | 'master' | 'slave' | 'datapoint' | 'search';
  level: number;
  children?: TreeNode[];
  child_count?: number;
  point_count?: number;
  device_info?: {
    device_id: string;
    device_name: string;
    device_type?: string;
    protocol_type?: string;
    endpoint?: string;
    connection_status?: string;
    status?: string;
    last_seen?: string;
    is_enabled?: boolean;
  };
  connection_status?: 'connected' | 'disconnected' | 'error';
  rtu_info?: any;
  data_points?: any[];
  value?: any;
  unit?: string;
  quality?: string;
  timestamp?: string;
}

interface FilterState {
  search: string;
  dataType: string;
  quality: string;
  device: string;
}

const DataExplorer: React.FC = () => {
  // 상태 관리
  const [treeData, setTreeData] = useState<TreeNode[]>([]);
  const [selectedNode, setSelectedNode] = useState<TreeNode | null>(null);
  const [selectedDataPoints, setSelectedDataPoints] = useState<RealtimeValue[]>([]);
  const [realtimeData, setRealtimeData] = useState<RealtimeValue[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'connecting' | 'disconnected'>('disconnected');
  const [lastRefresh, setLastRefresh] = useState<Date>(new Date());
  const [statistics, setStatistics] = useState<any>({});
  const [filters, setFilters] = useState<FilterState>({
    search: '',
    dataType: 'all',
    quality: 'all',
    device: 'all'
  });
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(3);
  const [showChart, setShowChart] = useState(false);

  const pagination = useDataExplorerPagination(0);

  // ========================================================================
  // 새로운 API를 사용한 트리 데이터 로드
  // ========================================================================
  const loadTreeStructure = useCallback(async () => {
    try {
      console.log('🌳 백엔드 API에서 트리 구조 로드 시작...');
      setIsLoading(true);
      setConnectionStatus('connecting');
      
      const response = await DeviceApiService.getDeviceTreeStructure({
        include_realtime: true,
        include_data_points: false
      });
      
      if (response.success && response.data) {
        console.log('✅ 트리 구조 로드 완료:', response.data.statistics);
        
        // 백엔드에서 받은 트리 구조를 그대로 사용
        setTreeData([response.data.tree]);
        setStatistics(response.data.statistics);
        setConnectionStatus('connected');
        setError(null);
        
        // 실시간 데이터 추출
        const allDataPoints: RealtimeValue[] = [];
        const extractDataPoints = (node: TreeNode) => {
          if (node.data_points) {
            allDataPoints.push(...node.data_points);
          }
          if (node.children) {
            node.children.forEach(extractDataPoints);
          }
        };
        extractDataPoints(response.data.tree);
        setRealtimeData(allDataPoints);
        
      } else {
        throw new Error(response.error || '트리 구조 로드 실패');
      }
    } catch (error: any) {
      console.error('❌ 트리 구조 로드 실패:', error);
      setError(`트리 구조 로드 실패: ${error.message}`);
      setConnectionStatus('disconnected');
      setTreeData([{
        id: 'error-1',
        label: 'PulseOne Factory (연결 실패)',
        type: 'tenant',
        level: 0,
        children: []
      }]);
    } finally {
      setIsLoading(false);
    }
  }, []);

  // ========================================================================
  // 디바이스 세부 데이터 로드 (기존 로직 유지)
  // ========================================================================
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

  // ========================================================================
  // 트리 노드 확장 로직
  // ========================================================================
  const loadDeviceChildren = useCallback(async (deviceNode: TreeNode) => {
    if (!['device', 'master', 'slave'].includes(deviceNode.type)) return;
    
    const deviceId = deviceNode.device_info?.device_id;
    if (!deviceId) return;
    
    console.log(`🔄 디바이스 ${deviceId} 자식 노드 로드...`);

    try {
      const dataPoints = await loadDeviceData(deviceId);
      if (dataPoints.length === 0) {
        console.log(`⚠️ 디바이스 ${deviceId}: Redis에 데이터 없음`);
        return;
      }

      const pointNodes: TreeNode[] = dataPoints.map((point: any) => ({
        id: `${deviceNode.id}-point-${point.point_id}`,
        label: point.point_name,
        type: 'datapoint',
        level: deviceNode.level + 1,
        value: point.value,
        unit: point.unit,
        quality: point.quality,
        timestamp: point.timestamp
      }));

      // 트리 데이터 업데이트
      setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
        children: pointNodes,
        child_count: pointNodes.length
      }));

      console.log(`✅ 디바이스 ${deviceId} 자식 노드 ${pointNodes.length}개 로드 완료`);
    } catch (error) {
      console.error(`❌ 디바이스 ${deviceId} 자식 노드 로드 실패:`, error);
    }
  }, [loadDeviceData]);

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
        if (node.type === 'datapoint') {
          dataPoints.push({
            key: node.id,
            point_name: node.label,
            point_id: node.id.split('-').pop() || '',
            device_id: node.device_info?.device_id || '',
            device_name: node.device_info?.device_name || '',
            value: node.value,
            unit: node.unit,
            quality: node.quality,
            timestamp: node.timestamp,
            data_type: typeof node.value,
            source: 'redis'
          } as RealtimeValue);
        }
        if (node.children) {
          traverse(node.children);
        }
      });
    };
    traverse(nodes);
    return dataPoints;
  };

  const renderEmptyDeviceMessage = (selectedNode: TreeNode | null) => {
    if (!selectedNode || !['device', 'master', 'slave'].includes(selectedNode.type)) return null;
    
    const device = selectedNode.device_info;
    const connectionStatus = selectedNode.connection_status;

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
          <div>프로토콜: {device?.protocol_type || 'Unknown'}</div>
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

  // ========================================================================
  // 필터링된 데이터
  // ========================================================================
  const filteredDataPoints = useMemo(() => {
    if (selectedNode && ['device', 'master', 'slave'].includes(selectedNode.type) &&
      (selectedNode.connection_status === 'disconnected' || !selectedNode.point_count)) {
      console.log('🔍 필터링: 연결 안된 디바이스 선택됨 - 빈 배열 반환');
      return [];
    }

    let points = selectedDataPoints.length > 0 ? selectedDataPoints : realtimeData;

    if (filters.search) {
      const searchTerm = filters.search.toLowerCase();
      points = points.filter((dp: RealtimeValue) =>
        (dp.point_name && dp.point_name.toLowerCase().includes(searchTerm)) ||
        (dp.device_name && dp.device_name.toLowerCase().includes(searchTerm)) ||
        (dp.key && dp.key.toLowerCase().includes(searchTerm))
      );
    }

    if (filters.dataType !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.data_type === filters.dataType);
    }

    if (filters.quality !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.quality === filters.quality);
    }

    return points;
  }, [selectedDataPoints, realtimeData, filters, selectedNode]);

  const paginatedDataPoints = useMemo(() => {
    const startIndex = (pagination.currentPage - 1) * pagination.pageSize;
    const endIndex = startIndex + pagination.pageSize;
    return filteredDataPoints.slice(startIndex, endIndex);
  }, [filteredDataPoints, pagination.currentPage, pagination.pageSize]);

  useEffect(() => {
    pagination.updateTotalCount(filteredDataPoints.length);
  }, [filteredDataPoints.length, pagination.updateTotalCount]);

  // ========================================================================
  // 이벤트 핸들러들
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
    loadTreeStructure();
  }, [loadTreeStructure]);

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
    
    if (node.type === 'datapoint') {
      const dataPoint: RealtimeValue = {
        key: node.id,
        point_name: node.label,
        point_id: node.id.split('-').pop() || '',
        device_id: node.device_info?.device_id || '',
        device_name: node.device_info?.device_name || '',
        value: node.value,
        unit: node.unit,
        quality: node.quality,
        timestamp: node.timestamp,
        data_type: typeof node.value,
        source: 'redis'
      };
      setSelectedDataPoints([dataPoint]);
    } else if (['device', 'master', 'slave'].includes(node.type)) {
      const deviceId = node.device_info?.device_id;
      
      if (!deviceId || node.connection_status === 'disconnected' || !node.point_count) {
        console.log(`⚠️ 디바이스 ${deviceId}: Redis 데이터 없음`);
        setSelectedDataPoints([]);
        return;
      }

      // RTU 마스터인 경우, 자신과 모든 슬레이브의 데이터를 로드
      if (node.type === 'master' && node.children) {
        Promise.all([
          loadDeviceData(deviceId),
          ...node.children.map(child => {
            const slaveId = child.device_info?.device_id;
            return slaveId ? loadDeviceData(slaveId) : Promise.resolve([]);
          })
        ]).then(results => {
          const allDataPoints = results.flat();
          if (allDataPoints.length > 0) {
            setSelectedDataPoints(allDataPoints);
            setRealtimeData(allDataPoints);
          }
        });
      } else {
        // 일반 디바이스나 슬레이브인 경우
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
      }

      // 자식 노드 로드 (데이터포인트)
      if (!node.children && node.point_count && node.point_count > 0) {
        loadDeviceChildren(node);
      }
    }
  }, [findAllDataPoints, loadDeviceChildren, loadDeviceData]);

  // ========================================================================
  // 초기화 및 자동 새로고침
  // ========================================================================
  useEffect(() => {
    loadTreeStructure();
  }, [loadTreeStructure]);

  useEffect(() => {
    if (!autoRefresh || refreshInterval <= 0) return;

    const interval = setInterval(() => {
      setLastRefresh(new Date());
      loadTreeStructure();
    }, refreshInterval * 1000);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, loadTreeStructure]);

  // ========================================================================
  // 렌더링 함수
  // ========================================================================
  const renderTreeNode = (node: TreeNode): React.ReactNode => {
    const hasChildren = (node.children && node.children.length > 0) || (node.child_count && node.child_count > 0);
    const isExpanded = node.children && node.children.length > 0;

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
            {node.type === 'master' && '🔌'}
            {node.type === 'slave' && '📊'}
            {node.type === 'datapoint' && '📊'}
          </span>
          <span className="tree-node-label">
            {node.label}
          </span>
          {node.type === 'datapoint' && (
            <div className="data-point-preview">
              <span className={`data-value ${node.quality || 'unknown'}`}>
                {node.value}
                {node.unit && ` ${node.unit}`}
              </span>
            </div>
          )}
          {(['device', 'master', 'slave'].includes(node.type)) && node.connection_status && (
            <span className={`connection-badge ${node.connection_status}`}>
              {node.connection_status === 'connected' && '🟢'}
              {node.connection_status === 'disconnected' && '⚪'}
              {node.connection_status === 'error' && '❌'}
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
  // 메인 렌더링
  // ========================================================================
  return (
    <div className="data-explorer-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            📊 PulseOne 실시간 데이터 탐색기 (RTU 계층구조)
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
                ({statistics.total_devices || 0}개 디바이스, {filteredDataPoints.length}개 포인트)
              </span>
            </div>
            <div>
              마지막 업데이트: {lastRefresh.toLocaleTimeString()}
            </div>
            {statistics.rtu_masters && (
              <div style={{fontSize: '12px', color: '#6b7280'}}>
                RTU: 마스터 {statistics.rtu_masters}개, 슬레이브 {statistics.rtu_slaves}개
              </div>
            )}
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
            <h3>📋 데이터 구조 (RTU 네트워크)</h3>
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
                <div className="loading-text">RTU 네트워크 구조 로드 중...</div>
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

            {/* 실시간 데이터 테이블 */}
            <div className="realtime-data">
              <h4>
                ⚡ 실시간 데이터 ({filteredDataPoints.length}개)
              </h4>

              {filteredDataPoints.length === 0 ? (
                selectedNode && ['device', 'master', 'slave'].includes(selectedNode.type) &&
                (selectedNode.connection_status === 'disconnected' || !selectedNode.point_count) ?
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
                  {/* 테이블 헤더 */}
                  <div className="data-table-header">
                    <div className="header-cell">✓</div>
                    <div className="header-cell">포인트명</div>
                    <div className="header-cell">디바이스</div>
                    <div className="header-cell">현재값</div>
                    <div className="header-cell">품질</div>
                    <div className="header-cell">타입</div>
                    <div className="header-cell">시간</div>
                  </div>

                  {/* 테이블 바디 */}
                  <div className="data-table-body">
                    {paginatedDataPoints.map((dataPoint: RealtimeValue, index: number) => (
                      <div key={dataPoint.key || `row-${index}`} className="data-table-row">
                        <div className="table-cell cell-checkbox" data-label="선택">
                          <input
                            type="checkbox"
                            checked={selectedDataPoints.some((dp: RealtimeValue) => dp.key === dataPoint.key)}
                            onChange={() => handleDataPointSelect(dataPoint)}
                          />
                        </div>
                        
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
                        
                        <div className="table-cell cell-device" data-label="디바이스">
                          <div className="device-name">
                            {(dataPoint.device_name || '').replace('-CTRL-', '').replace('-01', '')}
                          </div>
                        </div>
                        
                        <div className="table-cell cell-value" data-label="현재값">
                          <div className="value-display">
                            <span className={`value ${dataPoint.quality || 'unknown'}`}>
                              {String(dataPoint.value || '—')}
                              {dataPoint.unit && <span className="unit">{dataPoint.unit}</span>}
                            </span>
                          </div>
                        </div>
                        
                        <div className="table-cell cell-quality" data-label="품질">
                          <span className={`quality-badge ${dataPoint.quality || 'unknown'}`}>
                            {dataPoint.quality === 'good' ? 'OK' :
                            dataPoint.quality === 'comm_failure' ? 'ERR' :
                            dataPoint.quality === 'last_known' ? 'OLD' :
                            dataPoint.quality === 'uncertain' ? '?' :
                            dataPoint.quality || '—'}
                          </span>
                        </div>
                        
                        <div className="table-cell cell-type" data-label="타입">
                          <span className="data-type">
                            {dataPoint.data_type === 'number' ? 'NUM' :
                            dataPoint.data_type === 'boolean' ? 'BOOL' :
                            dataPoint.data_type === 'integer' ? 'INT' :
                            dataPoint.data_type === 'string' ? 'STR' : 'UNK'}
                          </span>
                        </div>
                        
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

                  {/* 페이징 컴포넌트 */}
                  {filteredDataPoints.length > 0 && (
                    <Pagination
                      current={pagination.currentPage}
                      total={pagination.totalCount}
                      pageSize={pagination.pageSize}
                      pageSizeOptions={pagination.pageSizeOptions}
                      showSizeChanger={true}
                      showTotal={true}
                      onChange={(page) => pagination.goToPage(page)}
                      onShowSizeChange={(page, pageSize) => pagination.changePageSize(pageSize)}
                      className="pagination-wrapper"
                    />
                  )}
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