// ============================================================================
// frontend/src/pages/DataExplorer.tsx
// 📝 Redis 데이터 익스플로러 - 완성된 최종 버전
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { RedisDataApiService } from '../api/services/redisDataApi';
import type { 
  RedisTreeNode, 
  RedisDataPoint, 
  RedisStats 
} from '../api/services/redisDataApi';
import { PAGINATION_CONSTANTS } from '../constants/pagination';
import '../styles/base.css';
import '../styles/data-explorer.css';
import '../styles/pagination.css';

const DataExplorer: React.FC = () => {
  // 🔧 기본 상태들
  const [treeData, setTreeData] = useState<RedisTreeNode[]>([]);
  const [selectedNode, setSelectedNode] = useState<RedisTreeNode | null>(null);
  const [selectedDataPoints, setSelectedDataPoints] = useState<RedisDataPoint[]>([]);
  const [searchTerm, setSearchTerm] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // Redis 연결 및 통계
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'disconnected' | 'connecting'>('connecting');
  const [redisStats, setRedisStats] = useState<RedisStats | null>(null);
  
  // 실시간 업데이트
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5000);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // 페이징
  const pagination = usePagination({
    initialPageSize: PAGINATION_CONSTANTS.DEFAULT_PAGE_SIZE,
    totalCount: selectedDataPoints.length,
    pageSizeOptions: PAGINATION_CONSTANTS.PAGE_SIZE_OPTIONS
  });

  // =============================================================================
  // 초기화 및 데이터 로드
  // =============================================================================

  useEffect(() => {
    initializeConnection();
    loadInitialTree();
  }, []);

  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      if (selectedDataPoints.length > 0) {
        refreshSelectedDataPoints();
      }
      updateConnectionStatus();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, selectedDataPoints]);

  // =============================================================================
  // API 호출 함수들 - 완성된 버전
  // =============================================================================

  const initializeConnection = async () => {
    try {
      console.log('🔍 연결 초기화 시작...');
      setConnectionStatus('connecting');
      setError(null); // 기존 에러 클리어
      
      const response = await RedisDataApiService.getConnectionStatus();
      
      console.log('📡 연결 상태 응답 받음:', response);
      
      if (response.success && response.data) {
        console.log('✅ 연결 성공, 상태 설정:', response.data.status);
        setConnectionStatus(response.data.status);
        
        if (response.data.status === 'connected') {
          console.log('🔗 연결됨 - Redis 통계 로드 시작');
          await loadRedisStats();
          setError(null); // 연결 성공 시 에러 클리어
        } else {
          console.log('⚠️ 연결되지 않음, 상태:', response.data.status);
          setError(`Redis 상태: ${response.data.status}`);
        }
      } else {
        console.log('❌ 연결 실패:', response.error || '알 수 없는 오류');
        setConnectionStatus('disconnected');
        setError(response.error || 'Redis 서버에 연결할 수 없습니다.');
      }
    } catch (err) {
      console.error('❌ 연결 초기화 예외:', err);
      setConnectionStatus('disconnected');
      setError('Redis 연결 중 예외가 발생했습니다.');
    }
  };

  const loadRedisStats = async () => {
    try {
      console.log('📊 Redis 통계 로드 시작...');
      const response = await RedisDataApiService.getStats();
      
      if (response.success && response.data) {
        console.log('✅ Redis 통계 로드 성공:', response.data);
        
        // 통계 데이터 유효성 검증
        const validStats = {
          total_keys: typeof response.data.total_keys === 'number' ? response.data.total_keys : 0,
          memory_usage: typeof response.data.memory_usage === 'number' ? response.data.memory_usage : 0,
          connected_clients: typeof response.data.connected_clients === 'number' ? response.data.connected_clients : 0,
          commands_processed: typeof response.data.commands_processed === 'number' ? response.data.commands_processed : 0,
          hits: typeof response.data.hits === 'number' ? response.data.hits : 0,
          misses: typeof response.data.misses === 'number' ? response.data.misses : 0,
          expired_keys: typeof response.data.expired_keys === 'number' ? response.data.expired_keys : 0
        };
        
        console.log('🔍 검증된 통계 데이터:', validStats);
        setRedisStats(validStats);
      } else {
        console.log('⚠️ Redis 통계 로드 실패:', response.error);
        // 통계 로드 실패 시 기본값 설정
        setRedisStats({
          total_keys: 0,
          memory_usage: 0,
          connected_clients: 0,
          commands_processed: 0,
          hits: 0,
          misses: 0,
          expired_keys: 0
        });
      }
    } catch (err) {
      console.error('❌ Redis 통계 로드 예외:', err);
      // 예외 발생 시 기본값 설정
      setRedisStats({
        total_keys: 0,
        memory_usage: 0,
        connected_clients: 0,
        commands_processed: 0,
        hits: 0,
        misses: 0,
        expired_keys: 0
      });
    }
  };

  const loadInitialTree = async () => {
    console.log('🌳 초기 트리 로드 시작...');
    setIsLoading(true);
    
    try {
      const response = await RedisDataApiService.getKeyTree();
      
      console.log('📡 트리 응답 받음:', response);
      
      if (response.success && response.data) {
        console.log('✅ 트리 로드 성공, 노드 수:', response.data.length);
        setTreeData(response.data);
      } else {
        console.log('⚠️ 트리 로드 실패:', response.error);
        
        // 폴백: 연결 끊김 상태에서도 기본 구조 표시
        console.log('🔄 폴백 트리 생성');
        setTreeData(createFallbackTree());
      }
    } catch (err) {
      console.error('❌ 트리 로드 예외:', err);
      setTreeData(createFallbackTree());
    } finally {
      setIsLoading(false);
    }
  };

  const loadNodeChildren = async (node: RedisTreeNode) => {
    setIsLoading(true);
    
    try {
      const response = await RedisDataApiService.getNodeChildren(node.id);
      
      if (response.success) {
        // 트리 데이터 업데이트
        setTreeData(prev => updateTreeNode(prev, node.id, { 
          children: response.data, 
          isLoaded: true, 
          isExpanded: true 
        }));
      } else {
        setError(`노드 ${node.name}의 자식을 로드할 수 없습니다.`);
      }
    } catch (err) {
      console.error('노드 자식 로드 실패:', err);
      setError('하위 노드를 불러올 수 없습니다.');
    } finally {
      setIsLoading(false);
    }
  };

  const loadDataPointDetails = async (node: RedisTreeNode) => {
    if (!node.dataPoint) return;
    
    try {
      const response = await RedisDataApiService.getKeyData(node.dataPoint.key);
      
      if (response.success) {
        // 최신 데이터로 업데이트
        const updatedDataPoint = response.data;
        setSelectedDataPoints(prev => {
          const exists = prev.find(dp => dp.id === updatedDataPoint.id);
          if (exists) {
            return prev.map(dp => dp.id === updatedDataPoint.id ? updatedDataPoint : dp);
          } else {
            return [...prev, updatedDataPoint];
          }
        });
        
        // 트리 노드도 업데이트
        setTreeData(prev => updateTreeNode(prev, node.id, {
          dataPoint: updatedDataPoint
        }));
      }
    } catch (err) {
      console.error('데이터 포인트 상세 로드 실패:', err);
      setError('데이터 포인트 정보를 불러올 수 없습니다.');
    }
  };

  const refreshSelectedDataPoints = async () => {
    if (!Array.isArray(selectedDataPoints) || selectedDataPoints.length === 0) return;
    
    try {
      const keys = selectedDataPoints.map(dp => dp.key);
      const response = await RedisDataApiService.getBulkKeyData(keys);
      
      if (response.success && Array.isArray(response.data)) {
        setSelectedDataPoints(response.data);
        setLastUpdate(new Date());
      }
    } catch (err) {
      console.error('선택된 데이터 포인트 새로고침 실패:', err);
    }
  };

  // 🔄 연결 상태 업데이트 (주기적 호출)
  const updateConnectionStatus = async () => {
    try {
      console.log('🔄 주기적 연결 상태 업데이트...');
      const response = await RedisDataApiService.getConnectionStatus();
      
      if (response.success && response.data) {
        const newStatus = response.data.status;
        
        // 상태가 변경된 경우에만 업데이트
        if (newStatus !== connectionStatus) {
          console.log(`🔄 연결 상태 변경: ${connectionStatus} → ${newStatus}`);
          setConnectionStatus(newStatus);
          
          if (newStatus === 'connected') {
            setError(null); // 연결 복구 시 에러 클리어
            await loadRedisStats();
          } else {
            setError(`Redis 상태: ${newStatus}`);
          }
        }
      }
    } catch (err) {
      console.warn('⚠️ 주기적 연결 상태 확인 실패 (무시):', err);
      // 주기적 확인 실패는 에러 상태 변경하지 않음
    }
  };

  // =============================================================================
  // 이벤트 핸들러들
  // =============================================================================

  const handleNodeClick = async (node: RedisTreeNode) => {
    setSelectedNode(node);
    
    if (node.type === 'datapoint' && node.dataPoint) {
      // 데이터 포인트 선택 시 상세 정보 로드
      await loadDataPointDetails(node);
    } else if (!node.isLoaded && node.childCount && node.childCount > 0) {
      // 자식 노드 로드
      await loadNodeChildren(node);
    } else if (node.isLoaded) {
      // 확장/축소 토글
      setTreeData(prev => updateTreeNode(prev, node.id, { 
        isExpanded: !node.isExpanded 
      }));
    }
  };

  const handleDataPointSelect = (dataPoint: RedisDataPoint) => {
    setSelectedDataPoints(prev => {
      const exists = prev.find(dp => dp.id === dataPoint.id);
      if (exists) {
        return prev.filter(dp => dp.id !== dataPoint.id);
      } else {
        return [...prev, dataPoint];
      }
    });
  };

  const handleSearch = async () => {
    if (!searchTerm.trim()) {
      await loadInitialTree();
      return;
    }
    
    setIsLoading(true);
    try {
      const response = await RedisDataApiService.searchKeys({
        pattern: `*${searchTerm}*`,
        limit: 100
      });
      
      if (response.success && Array.isArray(response.data.keys)) {
        // 검색 결과를 트리 구조로 변환
        const searchResults = createSearchResultTree(response.data.keys);
        setTreeData(searchResults);
      }
    } catch (err) {
      console.error('검색 실패:', err);
      setError('검색 중 오류가 발생했습니다.');
    } finally {
      setIsLoading(false);
    }
  };

  const clearSelection = () => {
    setSelectedDataPoints([]);
  };

  const exportData = async () => {
    if (!Array.isArray(selectedDataPoints) || selectedDataPoints.length === 0) return;
    
    try {
      const keys = selectedDataPoints.map(dp => dp.key);
      const blob = await RedisDataApiService.exportData(keys, 'json');
      
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `redis_data_${new Date().toISOString().split('T')[0]}.json`;
      a.click();
      URL.revokeObjectURL(url);
    } catch (err) {
      console.error('데이터 내보내기 실패:', err);
      setError('데이터 내보내기 중 오류가 발생했습니다.');
    }
  };

  // =============================================================================
  // 유틸리티 함수들
  // =============================================================================

  const createFallbackTree = (): RedisTreeNode[] => {
    return [
      {
        id: 'fallback-root',
        name: 'Redis (연결 없음)',
        path: '',
        type: 'folder',
        isExpanded: false,
        isLoaded: true,
        children: [
          {
            id: 'fallback-message',
            name: '연결을 확인하세요',
            path: 'message',
            type: 'folder',
            isExpanded: false,
            isLoaded: true,
            children: []
          }
        ]
      }
    ];
  };

  const createSearchResultTree = (keys: string[]): RedisTreeNode[] => {
    // keys가 배열인지 확인
    if (!Array.isArray(keys)) {
      console.warn('⚠️ createSearchResultTree: keys is not an array:', keys);
      return [];
    }
    
    return keys.map((key, index) => ({
      id: `search_${index}`,
      name: key,
      path: key,
      type: 'datapoint',
      isExpanded: false,
      isLoaded: true,
      dataPoint: {
        id: key,
        key,
        name: key.split(':').pop() || key,
        value: 'Loading...',
        dataType: 'string',
        timestamp: new Date().toISOString(),
        quality: 'uncertain',
        size: 0
      }
    }));
  };

  const updateTreeNode = (nodes: RedisTreeNode[], nodeId: string, updates: Partial<RedisTreeNode>): RedisTreeNode[] => {
    // nodes가 배열인지 확인
    if (!Array.isArray(nodes)) {
      console.warn('⚠️ updateTreeNode: nodes is not an array:', nodes);
      return [];
    }
    
    return nodes.map(node => {
      if (node.id === nodeId) {
        return { ...node, ...updates };
      }
      if (node.children && Array.isArray(node.children)) {
        return { ...node, children: updateTreeNode(node.children, nodeId, updates) };
      }
      return node;
    });
  };

  const renderTreeNode = (node: RedisTreeNode, level: number = 0): React.ReactNode => {
    // node 유효성 검사
    if (!node || typeof node !== 'object') {
      console.warn('⚠️ renderTreeNode: invalid node:', node);
      return null;
    }
    
    const hasChildren = node.childCount && node.childCount > 0;
    const isExpanded = node.isExpanded && Array.isArray(node.children);
    
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
            
            <span className="tree-node-label">{node.name || 'Unknown'}</span>
            
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
        
        {isExpanded && node.children && Array.isArray(node.children) && (
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
      case 'site': return 'fas fa-industry';
      case 'device': return 'fas fa-microchip';
      case 'folder': return 'fas fa-folder';
      case 'datapoint': return 'fas fa-chart-line';
      default: return 'fas fa-file';
    }
  };

  const formatDataValue = (dataPoint: RedisDataPoint): string => {
    if (!dataPoint || dataPoint.value === undefined || dataPoint.value === null) {
      return 'N/A';
    }
    
    if (dataPoint.dataType === 'boolean') {
      return dataPoint.value ? 'TRUE' : 'FALSE';
    }
    if (dataPoint.dataType === 'number') {
      return `${dataPoint.value}${dataPoint.unit || ''}`;
    }
    return String(dataPoint.value);
  };

  const formatTimestamp = (timestamp: string): string => {
    if (!timestamp) return 'N/A';
    
    try {
      return new Date(timestamp).toLocaleString('ko-KR', {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
      });
    } catch (err) {
      console.warn('⚠️ 타임스탬프 포맷 실패:', timestamp, err);
      return timestamp;
    }
  };

  // =============================================================================
  // 연결 상태 및 에러 렌더링 함수들
  // =============================================================================

  const renderConnectionStatus = () => {
    const statusConfig = {
      connected: { 
        text: 'Redis 연결됨', 
        className: 'status-connected',
        icon: '🟢'
      },
      connecting: { 
        text: 'Redis 연결 중...', 
        className: 'status-connecting',
        icon: '🟡'
      },
      disconnected: { 
        text: 'Redis 연결 끊김', 
        className: 'status-disconnected',
        icon: '🔴'
      }
    };

    const config = statusConfig[connectionStatus] || statusConfig.disconnected;

    return (
      <div className={`connection-status ${config.className}`}>
        <span className="status-icon">{config.icon}</span>
        <span className="status-text">{config.text}</span>
        {connectionStatus === 'connected' && redisStats && redisStats.total_keys !== undefined && (
          <span className="stats-info">
            (키: {redisStats.total_keys.toLocaleString() || 0})
          </span>
        )}
      </div>
    );
  };

  // 에러 표시 개선
  const renderError = () => {
    if (!error) return null;

    return (
      <div className="error-banner">
        <div className="error-content">
          <span className="error-icon">⚠️</span>
          <span className="error-message">{error}</span>
          <button 
            className="error-retry"
            onClick={() => {
              setError(null);
              initializeConnection();
            }}
          >
            다시 시도
          </button>
        </div>
      </div>
    );
  };

  // 🔥 안전한 필터링 - 배열 타입 보장
  const filteredTreeData = useMemo(() => {
    // treeData가 배열인지 확인
    if (!Array.isArray(treeData)) {
      console.warn('⚠️ treeData is not an array:', treeData);
      return [];
    }
    
    if (searchTerm && searchTerm.trim()) {
      return treeData.filter(node => 
        node && node.name && 
        node.name.toLowerCase().includes(searchTerm.toLowerCase())
      );
    }
    
    return treeData;
  }, [treeData, searchTerm]);

  // 🔥 안전한 페이징 데이터 처리
  const paginatedDataPoints = useMemo(() => {
    if (!Array.isArray(selectedDataPoints)) {
      console.warn('⚠️ selectedDataPoints is not an array:', selectedDataPoints);
      return [];
    }
    
    const startIndex = (pagination.currentPage - 1) * pagination.pageSize;
    const endIndex = startIndex + pagination.pageSize;
    return selectedDataPoints.slice(startIndex, endIndex);
  }, [selectedDataPoints, pagination.currentPage, pagination.pageSize]);

  return (
    <div className="data-explorer-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-database"></i>
            데이터 익스플로러
          </h1>
          <div className="header-meta">
            <span className="update-time">
              마지막 업데이트: {lastUpdate.toLocaleTimeString()}
            </span>
            {redisStats && redisStats.total_keys !== undefined && (
              <span className="redis-stats">
                총 키: {redisStats.total_keys.toLocaleString()}개 | 
                메모리: {((redisStats.memory_usage || 0) / 1024 / 1024).toFixed(1)}MB
              </span>
            )}
          </div>
        </div>
        <div className="page-actions">
          {renderConnectionStatus()}
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

      {/* 에러 메시지 */}
      {renderError()}

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
                  onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
                  className="search-input"
                />
                <button onClick={handleSearch} className="search-button">
                  <i className="fas fa-search"></i>
                </button>
              </div>
            </div>
          </div>
          
          <div className="tree-content">
            {isLoading && (
              <div className="loading-container">
                <div className="loading-spinner"></div>
                <div className="loading-text">로딩 중...</div>
              </div>
            )}
            {!isLoading && Array.isArray(filteredTreeData) && filteredTreeData.length > 0 && 
              filteredTreeData.map(node => renderTreeNode(node))
            }
            {!isLoading && (!Array.isArray(filteredTreeData) || filteredTreeData.length === 0) && (
              <div className="empty-state">
                <div className="empty-state-icon">🔍</div>
                <div className="empty-state-title">
                  {searchTerm ? '검색 결과가 없습니다' : '데이터가 없습니다'}
                </div>
                <div className="empty-state-description">
                  {searchTerm ? '다른 검색어를 시도해보세요' : 'Redis 연결을 확인하세요'}
                </div>
              </div>
            )}
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
              {Array.isArray(selectedDataPoints) && selectedDataPoints.length > 0 && (
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
              <div className="empty-state">
                <div className="empty-state-icon">👆</div>
                <div className="empty-state-title">항목을 선택하세요</div>
                <div className="empty-state-description">
                  탐색기에서 항목을 선택하여 상세 정보를 확인하세요
                </div>
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
                마지막 업데이트: {formatTimestamp(lastUpdate.toISOString())}
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
                              {Array.isArray(paginatedDataPoints) && paginatedDataPoints.map(dataPoint => (
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
            
            {/* 페이징 */}
            {selectedDataPoints.length > pagination.pageSize && (
              <Pagination
                className="watch-pagination"
                current={pagination.currentPage}
                total={selectedDataPoints.length}
                pageSize={pagination.pageSize}
                pageSizeOptions={PAGINATION_CONSTANTS.PAGE_SIZE_OPTIONS}
                showSizeChanger={true}
                showQuickJumper={false}
                showTotal={true}
                onChange={pagination.goToPage}
                onShowSizeChange={pagination.changePageSize}
              />
            )}
          </div>
        </div>
      )}
    </div>
  );
};

export default DataExplorer;