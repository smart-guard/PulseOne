import React, { useState, useEffect, useCallback, useMemo } from 'react';
import {
  RealtimeApiService,
  RealtimeValue
} from '../api/services/realtimeApi';
import {
  DeviceApiService
} from '../api/services/deviceApi';
import DataApiService from '../api/services/dataApi';
import { useDataExplorerPagination } from '../hooks/usePagination';
import '../styles/data-explorer.css';

// 모듈화된 컴포넌트 임포트
import ExplorerHeader from '../components/explorer/ExplorerHeader';
import DeviceTree, { TreeNode } from '../components/explorer/DeviceTree';
import TrendChart from '../components/explorer/TrendChart';
import RealtimeDataTable from '../components/explorer/RealtimeDataTable';

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
  const [autoRefresh, setAutoRefresh] = useState(false);
  const [refreshInterval, setRefreshInterval] = useState(10);
  const [showChart, setShowChart] = useState(false);
  const [historicalData, setHistoricalData] = useState<any[]>([]);
  const [timeRange, setTimeRange] = useState('1h');
  const [isChartLoading, setIsChartLoading] = useState(false);

  // 트리 확장 상태와 Redis 데이터 상태 분리 관리
  const [expandedNodes, setExpandedNodes] = useState<Set<string>>(new Set(['root-system', 'tenant-1']));
  const [redisDeviceStatuses, setRedisDeviceStatuses] = useState<Map<string, { connection_status: string; hasRedisData: boolean }>>(new Map());

  const pagination = useDataExplorerPagination(0);

  // ========================================================================
  // 🚀 Bulk 상태 체크 함수 (N+1 문제 해결)
  // ========================================================================
  const checkBulkDeviceStatus = useCallback(async (deviceIds: number[]) => {
    if (deviceIds.length === 0) return;

    try {
      const response = await DataApiService.getBulkDeviceStatus(deviceIds);
      if (response.success && response.data) {
        setRedisDeviceStatuses(prev => {
          const next = new Map(prev);
          Object.entries(response.data).forEach(([id, status]) => {
            next.set(id, status as any);
          });
          return next;
        });
      }
    } catch (err) {
      console.warn('Bulk status check failed:', err);
    }
  }, []);

  // ========================================================================
  // 트리 노드에 Redis 상태 업데이트
  // ========================================================================
  const updateTreeWithRedisStatus = useCallback((nodes: TreeNode[], statusesMap?: Map<string, { connection_status: string; hasRedisData: boolean }>): TreeNode[] => {
    const currentStatusesMap = statusesMap || redisDeviceStatuses;
    return nodes.map(node => {
      const updatedNode = { ...node };
      const isDevice = node.type === 'master' || node.id.startsWith('dev-');

      if (isDevice && node.device_info?.device_id) {
        const deviceId = node.device_info.device_id.toString();
        const status = currentStatusesMap.get(deviceId);

        if (status) {
          updatedNode.hasRedisData = status.hasRedisData;
          updatedNode.connection_status = status.connection_status;
        } else {
          updatedNode.hasRedisData = false;
          updatedNode.connection_status = undefined;
        }
      }

      if (node.children) {
        updatedNode.children = updateTreeWithRedisStatus(node.children, currentStatusesMap);
      }

      return updatedNode;
    });
  }, [redisDeviceStatuses]);

  // ========================================================================
  // 트리 데이터 로드
  // ========================================================================
  const loadTreeStructure = useCallback(async () => {
    setIsLoading(true);
    setConnectionStatus('connecting');
    setError(null);

    try {
      const response = await DeviceApiService.getDeviceTreeStructure();

      if (response.success && response.data) {
        const rawTree = response.data.tree || [];
        setStatistics(response.data.statistics || {});

        // 장치 ID들 추출하여 일괄 상태 체크 시동
        const deviceIds: number[] = [];
        const findDevices = (nodes: any[]) => {
          nodes.forEach(n => {
            if ((n.type === 'master' || n.id?.startsWith('dev-')) && n.device_info?.device_id) {
              deviceIds.push(Number(n.device_info.device_id));
            }
            if (n.children) findDevices(n.children);
          });
        };
        // 트리 데이터가 객체인 경우 배열로 변환하여 장치 검색 및 상태 저장
        const processedTree = Array.isArray(rawTree) ? rawTree : [rawTree];
        findDevices(processedTree);

        // Bulk 갱신 시작
        await checkBulkDeviceStatus(deviceIds);

        setTreeData(processedTree);
        setConnectionStatus('connected');
      } else {
        throw new Error(response.message || '네트워크 구조를 불러오지 못했습니다.');
      }
    } catch (err: any) {
      setError(err.message);
      setConnectionStatus('disconnected');
    } finally {
      setIsLoading(false);
    }
  }, [checkBulkDeviceStatus]);

  // 장치 상세 데이터 로드
  const loadDeviceData = useCallback(async (deviceId: string) => {
    try {
      const response = await RealtimeApiService.getDeviceValues(deviceId);
      if (response.success && response.data) {
        const mappedData = response.data.data_points.map(dp => ({
          ...dp,
          key: dp.key || `point:${dp.point_id}`,
          point_name: dp.point_name || `Point ${dp.point_id}`,
          device_name: response.data.device_name
        }));
        return mappedData;
      }
      return [];
    } catch (err) {
      console.error('Failed to load device data:', err);
      return [];
    }
  }, []);

  // 이력 데이터 조회 (차트용)
  const fetchHistoricalData = useCallback(async (pointIds: number[]) => {
    if (pointIds.length === 0) {
      setHistoricalData([]);
      return;
    }

    setIsChartLoading(true);
    try {
      const now = new Date();
      let startTime = new Date();
      let interval = '1m';

      switch (timeRange) {
        case '1h': startTime.setHours(now.getHours() - 1); interval = '1m'; break;
        case '6h': startTime.setHours(now.getHours() - 6); interval = '5m'; break;
        case '12h': startTime.setHours(now.getHours() - 12); interval = '10m'; break;
        case '24h': startTime.setHours(now.getHours() - 24); interval = '15m'; break;
      }

      const response = await DataApiService.getHistoricalData({
        point_ids: pointIds,
        start_time: startTime.toISOString(),
        end_time: now.toISOString(),
        interval,
        aggregation: 'mean'
      });

      if (response.success && response.data) {
        const groupedData = response.data.historical_data.reduce((acc: any, curr: any) => {
          const time = new Date(curr.time).toLocaleTimeString('ko-KR', { hour12: false, hour: '2-digit', minute: '2-digit' });
          if (!acc[time]) acc[time] = { time };
          acc[time][`p_${curr.point_id}`] = Number(curr.value.toFixed(2));
          return acc;
        }, {});

        const sortedData = Object.values(groupedData).sort((a: any, b: any) => {
          const [aH, aM] = a.time.split(':').map(Number);
          const [bH, bM] = b.time.split(':').map(Number);
          return (aH * 60 + aM) - (bH * 60 + bM);
        });

        setHistoricalData(sortedData);
      }
    } catch (err) {
      console.error('Historical data fetch failed:', err);
    } finally {
      setIsChartLoading(false);
    }
  }, [timeRange]);

  // 초기 로드
  useEffect(() => {
    loadTreeStructure();
  }, [loadTreeStructure]);

  // 차트 데이터 갱신
  useEffect(() => {
    if (showChart && selectedDataPoints.length > 0) {
      const ids = selectedDataPoints.map(p => p.point_id).filter((id): id is number => !!id);
      fetchHistoricalData(ids);
    }
  }, [showChart, selectedDataPoints, timeRange, fetchHistoricalData]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh || refreshInterval <= 0) return;
    const interval = setInterval(() => handleRefresh(), refreshInterval * 1000);
    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval]);

  const handleRefresh = useCallback(async () => {
    setLastRefresh(new Date());
    await loadTreeStructure();

    if (selectedNode && (selectedNode.type === 'master' || selectedNode.id.startsWith('dev-'))) {
      const deviceId = selectedNode.device_info?.device_id || selectedNode.id.replace('dev-', '');
      if (deviceId) {
        const data = await loadDeviceData(deviceId.toString());
        setRealtimeData(data);
      }
    }
  }, [loadTreeStructure, selectedNode, loadDeviceData]);

  const handleNodeClick = useCallback(async (node: TreeNode) => {
    if (node.children && node.children.length > 0) {
      setExpandedNodes(prev => {
        const next = new Set(prev);
        if (next.has(node.id)) next.delete(node.id);
        else next.add(node.id);
        return next;
      });
    }

    setSelectedNode(node);

    if (node.type === 'master' || node.id.startsWith('dev-')) {
      const deviceId = node.device_info?.device_id || node.id.replace('dev-', '');
      if (deviceId) {
        setIsLoading(true);
        const dataPoints = await loadDeviceData(deviceId.toString());
        setRealtimeData(dataPoints);
        setSelectedDataPoints(dataPoints);
        setIsLoading(false);
        pagination.updateTotalCount(dataPoints.length);
        pagination.goToFirst();
      }
    }
  }, [loadDeviceData, pagination]);

  // 필터링 및 페이징 로직
  const filteredDataPoints = useMemo(() => {
    return realtimeData.filter(dp => {
      const matchesSearch = filters.search === '' ||
        dp.point_name.toLowerCase().includes(filters.search.toLowerCase()) ||
        dp.key.toLowerCase().includes(filters.search.toLowerCase());
      const matchesType = filters.dataType === 'all' || dp.data_type === filters.dataType;
      const matchesQuality = filters.quality === 'all' || dp.quality === filters.quality;
      return matchesSearch && matchesType && matchesQuality;
    });
  }, [realtimeData, filters]);

  const paginatedDataPoints = useMemo(() => {
    const start = (pagination.currentPage - 1) * pagination.pageSize;
    return filteredDataPoints.slice(start, start + pagination.pageSize);
  }, [filteredDataPoints, pagination]);

  const handleToggleSelectAll = () => {
    if (selectedDataPoints.length === filteredDataPoints.length) setSelectedDataPoints([]);
    else setSelectedDataPoints(filteredDataPoints);
  };

  const handleDataPointSelect = (dp: RealtimeValue) => {
    setSelectedDataPoints(prev => {
      const exists = prev.find(p => p.key === dp.key);
      if (exists) return prev.filter(p => p.key !== dp.key);
      return [...prev, dp];
    });
  };

  const handleExportData = () => {
    const headers = ['Point Name', 'Device', 'Value', 'Unit', 'Quality', 'Type', 'Timestamp'];
    const rows = filteredDataPoints.map(dp => [
      dp.point_name,
      dp.device_name,
      dp.value,
      dp.unit || '',
      dp.quality,
      dp.data_type,
      dp.timestamp
    ]);
    const csvContent = "\uFEFF" + [headers, ...rows].map(e => e.join(",")).join("\n");
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.setAttribute("href", url);
    link.setAttribute("download", `pulseone_data_${new Date().getTime()}.csv`);
    link.click();
  };

  const processedTreeData = useMemo(() => updateTreeWithRedisStatus(treeData), [treeData, updateTreeWithRedisStatus]);

  return (
    <div className="data-explorer-container">
      <ExplorerHeader
        connectionStatus={connectionStatus}
        statistics={statistics}
        activeDevicesCount={redisDeviceStatuses.size}
        lastRefresh={lastRefresh}
        autoRefresh={autoRefresh}
        setAutoRefresh={setAutoRefresh}
        refreshInterval={refreshInterval}
        setRefreshInterval={setRefreshInterval}
        handleRefresh={handleRefresh}
        handleExportData={handleExportData}
        isLoading={isLoading}
        exportDisabled={filteredDataPoints.length === 0}
      />

      {error && <div className="error-banner"><div className="error-content"><span>⚠️ {error}</span></div></div>}

      <div className="explorer-layout">
        <div className="tree-panel">
          <div className="tree-header">
            <h3>📋 장치 계층 구조</h3>
            <div className="search-container">
              <input
                type="text" placeholder="트리 검색..."
                value={filters.search}
                onChange={e => setFilters(f => ({ ...f, search: e.target.value }))}
                className="search-input"
              />
            </div>
          </div>
          <DeviceTree
            treeData={processedTreeData}
            expandedNodes={expandedNodes}
            selectedNode={selectedNode}
            onNodeClick={handleNodeClick}
            isLoading={isLoading}
            searchTerm={filters.search}
          />
        </div>

        <div className="details-panel">
          <div className="details-header">
            <h3>📊 실시간 데이터 {selectedNode && ` - ${selectedNode.label}`}</h3>
            <div className="view-controls">
              <button
                onClick={() => setShowChart(!showChart)}
                className={`btn btn-sm ${showChart ? 'btn-primary' : 'btn-outline'}`}
              >
                📈 차트 {showChart ? '숨기기' : '보기'}
              </button>
            </div>
          </div>

          <div className="details-content">
            {showChart && selectedDataPoints.length > 0 && (
              <TrendChart
                historicalData={historicalData}
                selectedDataPoints={selectedDataPoints}
                timeRange={timeRange}
                setTimeRange={setTimeRange}
                isChartLoading={isChartLoading}
              />
            )}

            <RealtimeDataTable
              filteredDataPoints={filteredDataPoints}
              selectedDataPoints={selectedDataPoints}
              paginatedDataPoints={paginatedDataPoints}
              handleToggleSelectAll={handleToggleSelectAll}
              handleDataPointSelect={handleDataPointSelect}
              pagination={{
                currentPage: pagination.currentPage,
                totalCount: filteredDataPoints.length,
                pageSize: pagination.pageSize,
                pageSizeOptions: [10, 20, 50, 100],
                goToPage: pagination.goToPage,
                changePageSize: pagination.changePageSize
              }}
              selectedNode={selectedNode}
              renderEmptyDeviceMessage={(node) => (
                <div className="empty-state">
                  <p>{node.label}에 연결된 포인트가 없거나 데이터를 불러올 수 없습니다.</p>
                </div>
              )}
            />
          </div>
        </div>
      </div>
    </div>
  );
};

export default DataExplorer;