import React, { useState, useEffect, useCallback, useMemo } from 'react';
import {
  RealtimeApiService,
  RealtimeValue,
  DevicesResponse,
  CurrentValuesResponse,
  ApiResponse
} from '../api/services/realtimeApi';           // ✅ 한 단계 위로
import { 
  DeviceApiService, 
  Device 
} from '../api/services/deviceApi';             // ✅ 한 단계 위로
import '../styles/data-explorer.css';           // ✅ 스타일도 한 단계 위로

// ============================================================================
// 🔥 완성된 PulseOne 실시간 데이터 탐색기 - DB+Redis 하이브리드
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
  // 🔥 API 서비스 연동
  // ========================================================================

  // 🔥 디바이스 목록 로드 (DB에서 모든 설정된 디바이스)
  const loadDevices = useCallback(async () => {
    try {
      console.log('🔄 데이터베이스에서 디바이스 목록 로드 시작...');
      
      const response: ApiResponse<any> = await DeviceApiService.getDevices({
        page: 1,
        limit: 1000,  // 모든 디바이스 가져오기
        sort_by: 'name',
        sort_order: 'ASC'
      });

      if (response.success && response.data?.items) {
        const deviceList: DeviceInfo[] = response.data.items.map((device: Device) => ({
          device_id: device.id.toString(), // 🔥 수정: number → string 변환
          device_name: device.name,
          device_type: device.device_type || 'Unknown',
          point_count: device.data_point_count || device.data_points_count || 0, // 🔥 두 필드명 모두 체크
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



  // 🔥 실시간 데이터 로드 (Redis에서 연결된 디바이스만)
  const loadRealtimeData = useCallback(async (deviceList?: DeviceInfo[]) => {
    try {
      console.log('🔄 Redis에서 실시간 데이터 로드 시작...');
      
      let response;
      
      if (deviceList && deviceList.length > 0) {
        // 특정 디바이스들의 실시간 데이터
        const deviceIds = deviceList.map(d => d.device_id); // 이미 string 배열
        
        response = await RealtimeApiService.getCurrentValues({
          device_ids: deviceIds,
          quality_filter: 'all',
          limit: 5000
        });
      } else {
        // 모든 실시간 데이터
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

  // 🔥 특정 디바이스 데이터 로드 (Redis 확인)
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

  // 🔥 트리 데이터 생성 (DB 디바이스 + Redis 연결 상태 확인)
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

    // 🎯 각 디바이스의 Redis 연결 상태 확인
    const deviceNodesPromises = devices.map(async (device) => {
      let connectionStatus: 'connected' | 'disconnected' | 'error' = 'disconnected';
      let realDataCount = 0;

      try {
        // Redis에서 해당 디바이스의 실시간 데이터 확인
        const deviceValues = await loadDeviceData(device.device_id);
        if (deviceValues.length > 0) {
          connectionStatus = 'connected';
          realDataCount = deviceValues.length;
        }
      } catch (error) {
        console.warn(`⚠️ 디바이스 ${device.device_id} 연결 상태 확인 실패:`, error);
        connectionStatus = 'error';
      }

      // 🔥 포인트 수 표시 - 연결되면 실시간 포인트 수, 아니면 DB 포인트 수
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

  // 🔥 디바이스 자식 노드 로드
  const loadDeviceChildren = useCallback(async (deviceNode: TreeNode) => {
    if (deviceNode.type !== 'device') return;
    
    const deviceId = deviceNode.id.replace('device-', '');
    console.log(`🔄 디바이스 ${deviceId} 자식 노드 로드...`);
    
    try {
      const dataPoints = await loadDeviceData(deviceId);
      
      // 🔥 수정: Redis에 데이터가 없으면 확장하지 않음
      if (dataPoints.length === 0) {
        console.log(`⚠️ 디바이스 ${deviceId}: Redis에 데이터 없음 - 트리 확장 안함`);
        
        // 트리 업데이트하지 않고 그대로 유지
        setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
          isLoaded: true,
          isExpanded: false,  // 🔥 확장하지 않음
          childCount: 0
        }));
        
        return;
      }
      
      // 데이터 포인트를 트리 노드로 변환
      const pointNodes: TreeNode[] = dataPoints.map((point: any, index: number) => ({
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
      
      // 에러 시에도 확장하지 않음
      setTreeData(prev => updateTreeNode(prev, deviceNode.id, {
        children: [],
        isLoaded: true,
        isExpanded: false,  // 🔥 확장하지 않음
        childCount: 0
      }));
    }
  }, [loadDeviceData]);

  const renderEmptyDeviceMessage = (selectedNode: TreeNode | null) => {
    if (!selectedNode || selectedNode.type !== 'device') return null;
    
    const device = selectedNode.deviceInfo;
    const connectionStatus = selectedNode.connectionStatus;
    
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '300px',
        color: '#6b7280',
        textAlign: 'center',
        padding: '40px'
      }}>
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

  // 🔥 시스템 초기화 (DB + Redis 하이브리드)
  const initializeData = useCallback(async () => {
    setIsLoading(true);
    setConnectionStatus('connecting');
    
    try {
      console.log('🚀 데이터 초기화 시작...');
      
      // 1. 실시간 데이터 먼저 로드 (Redis)
      const realtimeDataPoints = await loadRealtimeData();
      
      // 2. 🎯 디바이스 목록을 DB에서 정확히 가져오기
      const deviceList = await loadDevices();
      
      // 3. 트리 구조 생성 (DB 디바이스 + Redis 연결 상태)
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
  // 🔥 필터링된 데이터
  // ========================================================================

  const filteredDataPoints = useMemo(() => {
    // 🔥 수정: selectedNode가 연결 안된 디바이스면 강제로 빈 배열 반환
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
    
    // 🔥 검색 필터 적용
    if (filters.search) {
      const searchTerm = filters.search.toLowerCase();
      points = points.filter((dp: RealtimeValue) => 
        (dp.point_name && dp.point_name.toLowerCase().includes(searchTerm)) ||
        (dp.device_name && dp.device_name.toLowerCase().includes(searchTerm)) ||
        (dp.key && dp.key.toLowerCase().includes(searchTerm))
      );
      console.log(`🔍 검색 필터 "${filters.search}" 적용 후: ${points.length}개`);
    }
    
    // 🔥 데이터 타입 필터 적용
    if (filters.dataType !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.data_type === filters.dataType);
      console.log(`🔍 데이터타입 필터 "${filters.dataType}" 적용 후: ${points.length}개`);
    }
    
    // 🔥 품질 필터 적용
    if (filters.quality !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.quality === filters.quality);
      console.log(`🔍 품질 필터 "${filters.quality}" 적용 후: ${points.length}개`);
    }
    
    // 🔥 디바이스 필터 적용
    if (filters.device !== 'all') {
      points = points.filter((dp: RealtimeValue) => dp.device_id === filters.device);
      console.log(`🔍 디바이스 필터 "${filters.device}" 적용 후: ${points.length}개`);
    }
    
    console.log('✅ 필터링 완료:', points.length + '개');
    return points;
  }, [selectedDataPoints, realtimeData, filters, selectedNode]); 

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
    
    // 현재 로드된 디바이스들의 실시간 데이터 갱신
    if (devices.length > 0) {
      loadRealtimeData(devices);
    } else {
      // 디바이스가 없으면 전체 초기화
      initializeData();
    }
  }, [devices, loadRealtimeData, initializeData]);

  const handleExportData = useCallback(() => {
    console.log('📥 데이터 내보내기 시작...');
    
    const exportData = filteredDataPoints.map((dp: RealtimeValue) => ({
      key: dp.key,
      point_name: dp.point_name,
      device_name: dp.device_name,
      device_id: dp.device_id,
      value: dp.value,
      unit: dp.unit,
      data_type: dp.data_type,
      quality: dp.quality,
      timestamp: dp.timestamp,
      source: dp.source
    }));
    
    // JSON 파일로 다운로드
    const jsonData = JSON.stringify(exportData, null, 2);
    const blob = new Blob([jsonData], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const link = document.createElement('a');
    link.href = url;
    link.download = `pulseone_realtime_data_${new Date().toISOString().split('T')[0]}.json`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
    
    console.log(`✅ ${exportData.length}개 데이터 내보내기 완료`);
  }, [filteredDataPoints]);

  const clearSelection = useCallback(() => {
    console.log('🔄 선택 초기화');
    setSelectedDataPoints([]);
    setSelectedNode(null);
  }, []);

// ============================================================================
// 🔥 자동 새로고침 Effect 추가 (useEffect 섹션에 추가)
// ============================================================================

// 원본 파일의 useEffect 섹션에 다음 코드를 추가하세요:

  // 자동 새로고침 효과
  useEffect(() => {
    if (!autoRefresh || refreshInterval <= 0) return;

    console.log(`🔄 자동 새로고침 설정: ${refreshInterval}초 간격`);
    
    const interval = setInterval(() => {
      console.log('🔄 자동 새로고침 실행...');
      setLastRefresh(new Date());
      
      // 현재 로드된 디바이스들의 실시간 데이터만 갱신
      if (devices.length > 0) {
        loadRealtimeData(devices);
      }
    }, refreshInterval * 1000);

    return () => {
      console.log('🔄 자동 새로고침 정리');
      clearInterval(interval);
    };
  }, [autoRefresh, refreshInterval, devices, loadRealtimeData]);
  // ============================================================================
// 🔥 Redis 없는 디바이스 클릭 방지 - 수정이 필요한 함수들만
// ============================================================================

  // 🔥 1. handleNodeClick 함수 (수정 버전) - 빈 디바이스도 선택 표시
  const handleNodeClick = useCallback((node: TreeNode) => {
    setSelectedNode(node);
    
    if (node.type === 'datapoint' && node.dataPoint) {
      setSelectedDataPoints([node.dataPoint]);
      
    } else if (node.type === 'device') {
      const deviceId = node.id.replace('device-', '');
      
      // 🔥 수정: Redis 데이터가 없는 디바이스는 강제로 빈 배열 설정
      if (node.connectionStatus === 'disconnected' || node.childCount === 0) {
        console.log(`⚠️ 디바이스 ${deviceId}: Redis 데이터 없음 - 강제 초기화`);
        
        // 🔥 핵심: 모든 관련 상태를 초기화
        setSelectedDataPoints([]); // 선택된 데이터포인트 초기화
        setRealtimeData([]);        // 🔥 추가: 실시간 데이터도 초기화 
        
        return; // 확장은 하지 않되, 선택 상태는 유지
      }
      
      const existingDataPoints = findAllDataPoints([node]);
      if (existingDataPoints.length > 0) {
        setSelectedDataPoints(existingDataPoints);
      }
      
      loadDeviceData(deviceId).then(dataPoints => {
        if (dataPoints.length > 0) {
          setSelectedDataPoints(dataPoints);
          setRealtimeData(dataPoints); // 🔥 실시간 데이터도 업데이트
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
  // 초기 로딩
  // ========================================================================

  useEffect(() => {
    initializeData();
  }, [initializeData]);

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================

  const renderTreeNode = (node: TreeNode): React.ReactNode => {
    const hasChildren = (node.children && node.children.length > 0) || (node.childCount && node.childCount > 0);
    const isExpanded = node.isExpanded && node.children;
    
    return (
      <div key={node.id} style={{marginBottom: '2px'}}>
        <div 
          style={{
            padding: '8px 12px',
            paddingLeft: `${node.level * 20 + 12}px`,
            cursor: 'pointer',
            backgroundColor: selectedNode?.id === node.id ? '#e3f2fd' : 'transparent',
            borderRadius: '4px',
            transition: 'background-color 0.2s ease',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            fontSize: '14px'
          }}
          onClick={() => handleNodeClick(node)}
          onMouseEnter={(e) => {
            if (selectedNode?.id !== node.id) {
              e.currentTarget.style.backgroundColor = '#f5f5f5';
            }
          }}
          onMouseLeave={(e) => {
            if (selectedNode?.id !== node.id) {
              e.currentTarget.style.backgroundColor = 'transparent';
            }
          }}
        >
          {hasChildren && (
            <span style={{fontSize: '12px', color: '#666'}}>
              {isExpanded ? '▼' : '▶'}
            </span>
          )}
          
          <span style={{fontSize: '16px'}}>
            {node.type === 'tenant' && '🏢'}
            {node.type === 'site' && '🏭'}
            {node.type === 'device' && '📱'}
            {node.type === 'datapoint' && '📊'}
          </span>
          
          <span style={{fontWeight: node.type === 'device' ? 600 : 400}}>
            {node.label}
          </span>
          
          {node.type === 'datapoint' && node.dataPoint && (
            <div style={{display: 'flex', alignItems: 'center', gap: '8px', marginLeft: 'auto'}}>
              <span style={{
                fontFamily: 'monospace',
                fontSize: '12px',
                padding: '2px 6px',
                borderRadius: '3px',
                backgroundColor: node.dataPoint.quality === 'good' ? '#d4edda' : '#f8d7da',
                color: node.dataPoint.quality === 'good' ? '#155724' : '#721c24'
              }}>
                {node.dataPoint.value}
                {node.dataPoint.unit && ` ${node.dataPoint.unit}`}
              </span>
            </div>
          )}
          
          {node.type === 'device' && node.connectionStatus && (
            <span style={{marginLeft: 'auto', fontSize: '16px'}}>
              {node.connectionStatus === 'connected' && '🟢'}
              {node.connectionStatus === 'disconnected' && '⚪'}
              {node.connectionStatus === 'error' && '❌'}
            </span>
          )}
          
          {!hasChildren && node.childCount && (
            <span style={{
              marginLeft: 'auto',
              fontSize: '11px',
              color: '#666',
              background: '#f0f0f0',
              padding: '2px 6px',
              borderRadius: '10px'
            }}>
              {node.childCount}
            </span>
          )}
        </div>
        
        {isExpanded && node.children && (
          <div>
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
    <div style={{
      minHeight: '100vh',
      backgroundColor: '#f8fafc',
      fontFamily: 'system-ui, -apple-system, sans-serif'
    }}>
      {/* 페이지 헤더 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'flex-start',
        padding: '24px 32px',
        background: '#ffffff',
        borderBottom: '1px solid #e5e7eb',
        marginBottom: '24px',
        boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)'
      }}>
        <div>
          <h1 style={{
            fontSize: '24px',
            fontWeight: 700,
            color: '#111827',
            margin: 0,
            display: 'flex',
            alignItems: 'center',
            gap: '12px'
          }}>
            📊 PulseOne 실시간 데이터 탐색기
          </h1>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '16px',
            fontSize: '14px',
            color: '#6b7280',
            marginTop: '8px'
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              padding: '6px 12px',
              borderRadius: '6px',
              fontSize: '13px',
              fontWeight: 500,
              backgroundColor: connectionStatus === 'connected' ? '#d4edda' : 
                              connectionStatus === 'connecting' ? '#fff3cd' : '#f8d7da',
              color: connectionStatus === 'connected' ? '#155724' : 
                     connectionStatus === 'connecting' ? '#856404' : '#721c24'
            }}>
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

        <div style={{display: 'flex', alignItems: 'center', gap: '12px'}}>
          <div style={{display: 'flex', alignItems: 'center', gap: '8px'}}>
            <label style={{display: 'flex', alignItems: 'center', gap: '6px', fontSize: '14px'}}>
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
                style={{
                  padding: '4px 8px',
                  borderRadius: '4px',
                  border: '1px solid #d1d5db',
                  fontSize: '12px'
                }}
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
            style={{
              padding: '8px 16px',
              backgroundColor: '#ffffff',
              color: '#374151',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              fontSize: '14px',
              fontWeight: 500,
              cursor: isLoading ? 'not-allowed' : 'pointer',
              opacity: isLoading ? 0.6 : 1,
              display: 'flex',
              alignItems: 'center',
              gap: '6px'
            }}
          >
            <span style={{transform: isLoading ? 'rotate(360deg)' : 'none', transition: 'transform 1s linear'}}>🔄</span>
            새로고침
          </button>
          
          <button 
            onClick={handleExportData}
            disabled={filteredDataPoints.length === 0}
            style={{
              padding: '8px 16px',
              backgroundColor: '#3b82f6',
              color: 'white',
              border: '1px solid #3b82f6',
              borderRadius: '6px',
              fontSize: '14px',
              fontWeight: 500,
              cursor: filteredDataPoints.length === 0 ? 'not-allowed' : 'pointer',
              opacity: filteredDataPoints.length === 0 ? 0.6 : 1,
              display: 'flex',
              alignItems: 'center',
              gap: '6px'
            }}
          >
            📥 데이터 내보내기
          </button>
        </div>
      </div>

      {/* 에러 배너 */}
      {error && (
        <div style={{
          backgroundColor: '#fee2e2',
          border: '1px solid #fecaca',
          color: '#991b1b',
          padding: '12px 24px',
          margin: '0 24px 20px 24px',
          borderRadius: '6px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}>
          <div style={{display: 'flex', alignItems: 'center', gap: '8px'}}>
            <span>⚠️</span>
            <span>{error}</span>
          </div>
          <button 
            onClick={() => setError(null)}
            style={{
              background: 'none',
              border: 'none',
              color: '#991b1b',
              cursor: 'pointer',
              fontSize: '16px'
            }}
          >
            ×
          </button>
        </div>
      )}

      {/* 메인 레이아웃 */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '350px 1fr',
        gap: '20px',
        height: 'calc(100vh - 160px)',
        minHeight: '700px',
        padding: '0 24px',
        maxWidth: '100vw',
        overflow: 'auto'
      }}>
        
        {/* 왼쪽: 트리 패널 */}
        <div style={{
          background: '#ffffff',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column',
          boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
          minWidth: '300px',
          maxWidth: '350px'
        }}>
          <div style={{
            padding: '16px',
            borderBottom: '1px solid #e5e7eb',
            backgroundColor: '#f9fafb'
          }}>
            <h3 style={{margin: '0 0 12px 0', fontSize: '16px', fontWeight: 600}}>📋 데이터 구조</h3>
            <div style={{position: 'relative'}}>
              <input
                type="text"
                placeholder="검색..."
                value={filters.search}
                onChange={(e) => setFilters(prev => ({ ...prev, search: e.target.value }))}
                style={{
                  width: '100%',
                  padding: '8px 12px 8px 32px',
                  border: '1px solid #d1d5db',
                  borderRadius: '6px',
                  fontSize: '14px'
                }}
              />
              <span style={{
                position: 'absolute',
                left: '10px',
                top: '50%',
                transform: 'translateY(-50%)',
                color: '#9ca3af'
              }}>🔍</span>
            </div>
          </div>
          
          <div style={{
            flex: 1,
            overflow: 'auto',
            padding: '12px'
          }}>
            {isLoading ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '200px',
                color: '#6b7280'
              }}>
                <div style={{fontSize: '24px', marginBottom: '8px'}}>⏳</div>
                <div>DB/Redis에서 데이터 로딩 중...</div>
              </div>
            ) : treeData.length === 0 ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '200px',
                color: '#6b7280'
              }}>
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
        <div style={{
          background: '#ffffff',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          overflow: 'auto',
          display: 'flex',
          flexDirection: 'column',
          boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
          minWidth: 0
        }}>
          <div style={{
            padding: '16px',
            borderBottom: '1px solid #e5e7eb',
            backgroundColor: '#f9fafb'
          }}>
            <h3 style={{margin: '0 0 12px 0', fontSize: '16px', fontWeight: 600}}>
              📊 실시간 데이터 
              {selectedNode && ` - ${selectedNode.label}`}
            </h3>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              gap: '12px',
              flexWrap: 'wrap'
            }}>
              <div style={{display: 'flex', gap: '8px', flexWrap: 'wrap'}}>
                <select
                  value={filters.dataType}
                  onChange={(e) => setFilters(prev => ({ ...prev, dataType: e.target.value }))}
                  style={{
                    padding: '4px 8px',
                    borderRadius: '4px',
                    border: '1px solid #d1d5db',
                    fontSize: '12px'
                  }}
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
                  style={{
                    padding: '4px 8px',
                    borderRadius: '4px',
                    border: '1px solid #d1d5db',
                    fontSize: '12px'
                  }}
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
                  style={{
                    padding: '4px 8px',
                    borderRadius: '4px',
                    border: '1px solid #d1d5db',
                    fontSize: '12px'
                  }}
                >
                  <option value="all">모든 디바이스</option>
                  {devices.map(device => (
                    <option key={device.device_id} value={device.device_id}>
                      {device.device_name}
                    </option>
                  ))}
                </select>
              </div>

              <div style={{display: 'flex', gap: '8px'}}>
                <button
                  onClick={() => setShowChart(!showChart)}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: showChart ? '#3b82f6' : '#ffffff',
                    color: showChart ? 'white' : '#374151',
                    border: '1px solid #d1d5db',
                    borderRadius: '4px',
                    fontSize: '12px',
                    cursor: 'pointer'
                  }}
                >
                  📈 차트 {showChart ? '숨기기' : '보기'}
                </button>
                
                {selectedDataPoints.length > 0 && (
                  <button
                    onClick={clearSelection}
                    style={{
                      padding: '4px 8px',
                      backgroundColor: '#ffffff',
                      color: '#374151',
                      border: '1px solid #d1d5db',
                      borderRadius: '4px',
                      fontSize: '12px',
                      cursor: 'pointer'
                    }}
                  >
                    선택 해제 ({selectedDataPoints.length})
                  </button>
                )}
              </div>
            </div>
          </div>

          <div style={{
            flex: 1,
            overflow: 'auto',
            padding: '16px',
            display: 'flex',
            flexDirection: 'column',
            minWidth: 0
          }}>
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

            {/* 🔥 완벽한 실시간 데이터 테이블 */}
            <div style={{
              flex: 1,
              display: 'flex',
              flexDirection: 'column',
              overflow: 'visible',
              minHeight: 0,
              width: '100%'
            }}>
              <h4 style={{margin: '0 0 12px 0', fontSize: '14px', fontWeight: 600}}>
                ⚡ 실시간 데이터 ({filteredDataPoints.length}개)
              </h4>
              
              {filteredDataPoints.length === 0 ? (
                // 🔥 선택된 노드가 연결 안된 디바이스인 경우 특별 메시지 표시
                selectedNode && selectedNode.type === 'device' && 
                (selectedNode.connectionStatus === 'disconnected' || selectedNode.childCount === 0) ? 
                  renderEmptyDeviceMessage(selectedNode) : (
                  // 🔥 기존 빈 데이터 메시지
                  <div style={{
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    justifyContent: 'center',
                    height: '200px',
                    color: '#6b7280',
                    textAlign: 'center'
                  }}>
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
                <div style={{
                  border: '1px solid #e5e7eb',
                  borderRadius: '6px',
                  overflowX: 'auto',
                  overflowY: 'hidden',
                  background: 'white',
                  flex: 1,
                  display: 'flex',
                  flexDirection: 'column',
                  boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
                  minHeight: '400px',
                  maxHeight: 'calc(100vh - 280px)',
                  width: '100%'
                }}>
                  {/* 🔥 가로 스크롤용 테이블 래퍼 */}
                  <div style={{
                    minWidth: '950px',
                    width: '100%',
                    display: 'flex',
                    flexDirection: 'column',
                    paddingBottom: '16px'
                  }}>
                    {/* 🔥 완벽한 정렬 고정 너비 헤더 */}
                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: '60px 300px 140px 160px 90px 80px 120px',
                      gap: 0,
                      background: '#f8fafc',
                      borderBottom: '2px solid #e5e7eb',
                      position: 'sticky',
                      top: 0,
                      zIndex: 10
                    }}>
                      <div style={{padding: '10px 8px', fontSize: '11px', fontWeight: 700, textAlign: 'center', borderRight: '1px solid #e5e7eb', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '40px'}}>✓</div>
                      <div style={{padding: '10px 12px', fontSize: '11px', fontWeight: 700, textAlign: 'left', borderRight: '1px solid #e5e7eb', display: 'flex', alignItems: 'center', justifyContent: 'flex-start', minHeight: '40px'}}>포인트명</div>
                      <div style={{padding: '10px 8px', fontSize: '11px', fontWeight: 700, textAlign: 'center', borderRight: '1px solid #e5e7eb', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '40px'}}>디바이스</div>
                      <div style={{padding: '10px 8px', fontSize: '11px', fontWeight: 700, textAlign: 'center', borderRight: '1px solid #e5e7eb', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '40px'}}>현재값</div>
                      <div style={{padding: '10px 8px', fontSize: '11px', fontWeight: 700, textAlign: 'center', borderRight: '1px solid #e5e7eb', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '40px'}}>품질</div>
                      <div style={{padding: '10px 8px', fontSize: '11px', fontWeight: 700, textAlign: 'center', borderRight: '1px solid #e5e7eb', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '40px'}}>타입</div>
                      <div style={{padding: '10px 8px', fontSize: '11px', fontWeight: 700, textAlign: 'center', borderRight: 'none', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '40px'}}>시간</div>
                    </div>
                    
                    {/* 🔥 자동 높이 조정 데이터 영역 */}
                    <div style={{
                      flex: 1,
                      overflowY: 'auto',
                      overflowX: 'visible',
                      minHeight: 0,
                      maxHeight: 'calc(100vh - 320px)',
                      paddingBottom: '20px'
                    }}>
                    {filteredDataPoints.map((dataPoint: RealtimeValue, index: number) => (
                        <div key={dataPoint.key || `row-${index}`} style={{
                          display: 'grid',
                          gridTemplateColumns: '60px 300px 140px 160px 90px 80px 120px',
                          gap: 0,
                          borderBottom: '1px solid #f1f5f9',
                          alignItems: 'center',
                          minHeight: '46px',
                          fontSize: '13px',
                          transition: 'background-color 0.15s ease'
                        }}
                        onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#f8fafc'}
                        onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}>
                          
                          {/* 체크박스 */}
                          <div style={{padding: '8px', textAlign: 'center', borderRight: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '46px'}}>
                            <input
                              type="checkbox"
                              checked={selectedDataPoints.some((dp: RealtimeValue) => dp.key === dataPoint.key)}
                              onChange={() => handleDataPointSelect(dataPoint)}
                              style={{width: '16px', height: '16px', cursor: 'pointer'}}
                            />
                          </div>
                          
                          {/* 포인트명 */}
                          <div style={{padding: '8px 12px', textAlign: 'left', overflow: 'hidden', borderRight: '1px solid #f1f5f9', minHeight: '46px', display: 'flex', flexDirection: 'column', justifyContent: 'center'}}>
                            <div style={{fontWeight: 600, fontSize: '12px', color: '#111827', marginBottom: '2px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap'}}>
                              {dataPoint.point_name || '[포인트명 없음]'}
                            </div>
                            <div style={{fontSize: '9px', color: '#6b7280', fontFamily: 'monospace', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap'}}>
                              {(dataPoint.key || '').replace('device:', '').replace(/:/g, '/')}
                            </div>
                          </div>
                          
                          {/* 디바이스 */}
                          <div style={{padding: '8px', textAlign: 'center', fontSize: '11px', color: '#374151', borderRight: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '46px'}}>
                            <div style={{overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', width: '100%'}}>
                              {(dataPoint.device_name || '').replace('-CTRL-', '').replace('-01', '')}
                            </div>
                          </div>
                          
                          {/* 현재값 */}
                          <div style={{padding: '8px 10px', textAlign: 'right', borderRight: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'flex-end', minHeight: '46px'}}>
                            <span style={{
                              fontFamily: 'monospace',
                              fontWeight: 600,
                              fontSize: '12px',
                              padding: '4px 8px',
                              borderRadius: '5px',
                              backgroundColor: dataPoint.quality === 'good' ? '#dcfce7' : 
                                              dataPoint.quality === 'bad' ? '#fee2e2' : 
                                              dataPoint.quality === 'uncertain' ? '#fef3c7' : 
                                              dataPoint.quality === 'comm_failure' ? '#fee2e2' : '#f3f4f6',
                              color: dataPoint.quality === 'good' ? '#166534' : 
                                     dataPoint.quality === 'bad' ? '#dc2626' : 
                                     dataPoint.quality === 'uncertain' ? '#92400e' : 
                                     dataPoint.quality === 'comm_failure' ? '#dc2626' : '#374151'
                            }}>
                              {String(dataPoint.value || '—')}
                              {dataPoint.unit && <span style={{fontSize: '10px', marginLeft: '2px'}}>{dataPoint.unit}</span>}
                            </span>
                          </div>
                          
                          {/* 품질 */}
                          <div style={{padding: '8px', textAlign: 'center', borderRight: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '46px'}}>
                            <span style={{
                              padding: '4px 8px',
                              borderRadius: '12px',
                              fontSize: '10px',
                              fontWeight: 700,
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
                          
                          {/* 타입 */}
                          <div style={{padding: '8px', textAlign: 'center', borderRight: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '46px'}}>
                            <span style={{
                              fontSize: '10px',
                              color: '#6b7280',
                              textTransform: 'uppercase',
                              fontWeight: 600,
                              background: '#f3f4f6',
                              padding: '3px 8px',
                              borderRadius: '4px'
                            }}>
                              {dataPoint.data_type === 'number' ? 'NUM' :
                               dataPoint.data_type === 'boolean' ? 'BOOL' :
                               dataPoint.data_type === 'integer' ? 'INT' :
                               dataPoint.data_type === 'string' ? 'STR' : 'UNK'}
                            </span>
                          </div>
                          
                          {/* 시간 */}
                          <div style={{padding: '8px', textAlign: 'center', borderRight: 'none', display: 'flex', alignItems: 'center', justifyContent: 'center', minHeight: '46px'}}>
                            <span style={{
                              fontSize: '11px',
                              color: '#6b7280',
                              fontFamily: 'monospace'
                            }}>
                              {dataPoint.timestamp ? 
                                new Date(dataPoint.timestamp).toLocaleTimeString('ko-KR', {
                                  hour: '2-digit',
                                  minute: '2-digit',
                                  second: '2-digit'
                                }) : '—'}
                            </span>
                          </div>
                        </div>
                      ))}
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