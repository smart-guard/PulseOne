// ============================================================================
// frontend/src/pages/RealTimeMonitor.tsx
// ⚡ 실시간 데이터 모니터링 - 진짜 API 연결 + 부드러운 새로고침
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { RealtimeApiService, RealtimeValue } from '../api/services/realtimeApi';
import { DataApiService } from '../api/services/dataApi';
import { DeviceApiService } from '../api/services/deviceApi';
import '../styles/base.css';
import '../styles/real-time-monitor.css';
import { isBlobValue, getBlobDownloadUrl } from '../utils/dataUtils';

interface RealTimeData {
  id: string;
  key: string;
  displayName: string;
  value: any;
  unit?: string;
  dataType: 'number' | 'boolean' | 'string';
  quality: 'good' | 'bad' | 'uncertain';
  timestamp: Date;
  trend: 'up' | 'down' | 'stable';
  factory: string;
  device: string;
  category: string;
  tags: string[];
  alarm?: {
    level: 'low' | 'medium' | 'high' | 'critical';
    message: string;
  };
  isFavorite: boolean;
  point_id: number;
  device_id: number;
}

interface ChartData {
  timestamp: Date;
  value: number;
}

const RealTimeMonitor: React.FC = () => {
  // =============================================================================
  // 🔧 State 관리
  // =============================================================================

  const [allData, setAllData] = useState<RealTimeData[]>([]);
  const [filteredData, setFilteredData] = useState<RealTimeData[]>([]);
  const [selectedFactories, setSelectedFactories] = useState<string[]>(['all']);
  const [selectedCategories, setSelectedCategories] = useState<string[]>(['all']);
  const [selectedQualities, setSelectedQualities] = useState<string[]>(['all']);
  const [searchTerm, setSearchTerm] = useState('');
  const [viewMode, setViewMode] = useState<'list' | 'grid' | 'compact'>('list');
  const [sortBy, setSortBy] = useState<'name' | 'value' | 'timestamp' | 'factory'>('name');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('asc');
  const [selectedData, setSelectedData] = useState<RealTimeData[]>([]);
  const [showChart, setShowChart] = useState(false);
  const [chartData, setChartData] = useState<{ [key: string]: ChartData[] }>({});
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(2000);
  const [currentPage, setCurrentPage] = useState(1);
  const [itemsPerPage, setItemsPerPage] = useState(20);
  const [favorites, setFavorites] = useState<string[]>([]);
  const [showFavoritesOnly, setShowFavoritesOnly] = useState(false);

  // 로딩 및 연결 상태
  const [isLoading, setIsLoading] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // 디바이스 및 데이터포인트 정보 (한 번만 로드)
  const [devices, setDevices] = useState<any[]>([]);
  const [dataPoints, setDataPoints] = useState<any[]>([]);
  const [dataStatistics, setDataStatistics] = useState<any>(null);

  // =============================================================================
  // 🛠️ 헬퍼 함수들
  // =============================================================================

  /**
   * 🚨 간단한 알람 생성 함수 (임시)
   */
  const generateAlarmIfNeeded = (value: any, category: string) => {
    // 간단한 임계값 기반 알람 생성
    if (typeof value !== 'number') return undefined;

    let threshold: { min?: number; max?: number } = {};

    switch (category) {
      case 'Temperature':
        threshold = { min: 0, max: 80 };
        break;
      case 'Pressure':
        threshold = { min: 0, max: 10 };
        break;
      case 'Flow':
        threshold = { min: 0, max: 150 };
        break;
      case 'Level':
        threshold = { min: 0, max: 100 };
        break;
      case 'Current':
        threshold = { min: 0, max: 25 };
        break;
      case 'Voltage':
        threshold = { min: 180, max: 280 };
        break;
      default:
        return undefined;
    }

    if (threshold.min !== undefined && value < threshold.min) {
      return {
        level: 'medium' as const,
        message: `${category} 값이 최소 임계값(${threshold.min}) 미만입니다.`
      };
    }

    if (threshold.max !== undefined && value > threshold.max) {
      return {
        level: 'high' as const,
        message: `${category} 값이 최대 임계값(${threshold.max})을 초과했습니다.`
      };
    }

    return undefined;
  };

  const inferCategory = (name: string): string => {
    const lowerName = name.toLowerCase();
    if (lowerName.includes('temp')) return 'Temperature';
    if (lowerName.includes('press')) return 'Pressure';
    if (lowerName.includes('flow')) return 'Flow';
    if (lowerName.includes('level')) return 'Level';
    if (lowerName.includes('speed') || lowerName.includes('rpm')) return 'Speed';
    if (lowerName.includes('current') || lowerName.includes('amp')) return 'Current';
    if (lowerName.includes('voltage') || lowerName.includes('volt')) return 'Voltage';
    if (lowerName.includes('status') || lowerName.includes('state')) return 'Status';
    return 'Sensor';
  };

  const extractFactory = (deviceName: string): string => {
    if (deviceName.toLowerCase().includes('seoul')) return 'Seoul Factory';
    if (deviceName.toLowerCase().includes('busan')) return 'Busan Factory';
    if (deviceName.toLowerCase().includes('daegu')) return 'Daegu Factory';
    return 'Main Factory';
  };

  // =============================================================================
  // 🔄 실제 API 데이터 로드 함수들
  // =============================================================================

  /**
   * 📱 메타데이터 로드 (한 번만)
   */
  const loadMetadata = useCallback(async () => {
    try {
      console.log('📱 메타데이터 로드 시작...');

      // 디바이스 목록 로드
      const devicesResponse = await DeviceApiService.getDevices({
        page: 1,
        limit: 1000,
        enabled_only: true
      });

      if (devicesResponse.success && devicesResponse.data) {
        setDevices(devicesResponse.data.items || []);
        console.log(`✅ 디바이스 ${devicesResponse.data.items?.length || 0}개 로드`);
      }

      // 데이터포인트 목록 로드
      const dataPointsResponse = await DataApiService.searchDataPoints({
        page: 1,
        limit: 1000,
        enabled_only: true,
        include_current_value: false
      });

      if (dataPointsResponse.success && dataPointsResponse.data) {
        setDataPoints(dataPointsResponse.data.items || []);
        console.log(`✅ 데이터포인트 ${dataPointsResponse.data.items?.length || 0}개 로드`);
      }

      // 데이터 통계 로드
      const statsResponse = await DataApiService.getDataStatistics({
        time_range: '1h'
      });

      if (statsResponse.success && statsResponse.data) {
        setDataStatistics(statsResponse.data);
        console.log('✅ 데이터 통계 로드 완료');
      }

    } catch (err) {
      console.error('❌ 메타데이터 로드 실패:', err);
    }
  }, []);

  /**
   * ⚡ 실시간 현재값 로드 (진짜 API 연결 - 목 데이터 제거)
   */
  const loadRealtimeData = useCallback(async () => {
    try {
      setError(null);
      setIsLoading(true);

      console.log('⚡ 실시간 데이터 로드 시작...');

      const response = await RealtimeApiService.getCurrentValues({
        limit: 1000,
        quality_filter: 'all',
        include_metadata: true
      });

      if (response.success && response.data) {
        const realtimeValues = response.data.current_values || [];

        console.log(`📡 백엔드에서 ${realtimeValues.length}개 데이터 수신`);

        if (realtimeValues.length === 0) {
          console.warn('⚠️ 백엔드에서 데이터가 없습니다');
          setAllData([]);
          setIsConnected(true);
          return;
        }

        // 실시간 값을 UI 형식으로 변환
        const transformedData: RealTimeData[] = realtimeValues.map((value: RealtimeValue) => {
          // 메타데이터에서 디바이스 정보 찾기
          const dataPoint = dataPoints.find(dp => dp.id === value.point_id);
          const device = devices.find(d => d.id === value.device_id || d.id === dataPoint?.device_id);

          // 카테고리 추론
          const category = inferCategory(dataPoint?.name || value.point_name || 'Unknown');

          return {
            id: `point_${value.point_id}`,
            key: `pulseone:${device?.name || 'unknown'}:${value.point_id}`,
            displayName: dataPoint?.name || value.point_name || `Point ${value.point_id}`,
            value: value.value,
            unit: value.unit || dataPoint?.unit,
            dataType: (value.data_type || dataPoint?.data_type || 'string') as 'number' | 'boolean' | 'string',
            quality: value.quality,
            timestamp: new Date(value.timestamp),
            trend: 'stable', // 초기에는 stable
            factory: device?.site_name || extractFactory(device?.name || 'Main Factory'),
            device: device?.name || `Device ${value.device_id}`,
            category,
            tags: [category.toLowerCase(), device?.protocol_type || 'unknown'],
            alarm: undefined, // 백엔드에서 알람 정보 제공 시에만 표시
            isFavorite: favorites.includes(`point_${value.point_id}`),
            point_id: value.point_id,
            device_id: value.device_id || dataPoint?.device_id || 0
          };
        });

        // 🔄 부드러운 업데이트 (이전 데이터와 비교)
        setAllData(prevData => {
          const updatedData = transformedData.map(newItem => {
            const prevItem = prevData.find(p => p.point_id === newItem.point_id);
            if (prevItem && newItem.dataType === 'number') {
              // 트렌드 계산
              const trend = newItem.value > prevItem.value ? 'up' :
                newItem.value < prevItem.value ? 'down' : 'stable';
              return { ...newItem, trend };
            }
            return newItem;
          });
          return updatedData;
        });

        setIsConnected(true);
        setLastUpdate(new Date());

        console.log(`✅ 실시간 데이터 ${transformedData.length}개 변환 완료`);

      } else {
        throw new Error(response.error || '백엔드 API 응답 오류');
      }

    } catch (err) {
      console.error('❌ 실시간 데이터 로드 실패:', err);
      setError(err instanceof Error ? err.message : '백엔드 API 연결 실패');
      setIsConnected(false);

      // 에러 시 빈 배열로 설정
      setAllData([]);
    } finally {
      setIsLoading(false);
    }
  }, [devices, dataPoints, favorites]);

  /**
   * 🔄 실시간 데이터 부드러운 업데이트
   */
  const updateRealTimeData = useCallback(async () => {
    if (!isConnected || allData.length === 0) {
      return;
    }

    try {
      // 모든 포인트의 현재값을 가져와서 부드럽게 업데이트
      const pointIds = allData.map(item => item.point_id);

      const response = await RealtimeApiService.getCurrentValues({
        point_ids: pointIds,
        limit: pointIds.length,
        include_metadata: false
      });

      if (response.success && response.data) {
        const updatedValues = response.data.current_values || [];

        // 🔄 부드러운 상태 업데이트 (깜빡임 방지)
        setAllData(prev => prev.map(item => {
          const updated = updatedValues.find(uv => uv.point_id === item.point_id);
          if (updated) {
            const trend = item.dataType === 'number'
              ? (updated.value > item.value ? 'up' : updated.value < item.value ? 'down' : 'stable')
              : 'stable';

            return {
              ...item,
              value: updated.value,
              quality: updated.quality,
              timestamp: new Date(updated.timestamp),
              trend,
              alarm: updated.alarm || generateAlarmIfNeeded(updated.value, item.category)
            };
          }
          return item;
        }));

        setLastUpdate(new Date());

        // 차트 데이터 업데이트 (선택된 항목만)
        selectedData.forEach(item => {
          const updated = updatedValues.find(uv => uv.point_id === item.point_id);
          if (updated && item.dataType === 'number') {
            setChartData(prev => ({
              ...prev,
              [item.id]: [
                ...(prev[item.id] || []).slice(-19), // 최근 20개 포인트만 유지
                { timestamp: new Date(updated.timestamp), value: updated.value as number }
              ]
            }));
          }
        });
      }

    } catch (err) {
      console.error('❌ 실시간 업데이트 실패:', err);
      // 네트워크 오류 시 연결 상태 유지 (일시적 오류일 수 있음)
    }
  }, [isConnected, allData, selectedData]);

  // =============================================================================
  // 🎨 계산된 통계 (진짜 데이터 기반)
  // =============================================================================

  const calculatedStats = useMemo(() => {
    const totalCount = allData.length;
    const filteredCount = filteredData.length;
    const selectedCount = selectedData.length;
    const favoriteCount = favorites.length;
    const alarmCount = allData.filter(item => item.alarm).length;
    const qualityStats = {
      good: allData.filter(item => item.quality === 'good').length,
      uncertain: allData.filter(item => item.quality === 'uncertain').length,
      bad: allData.filter(item => item.quality === 'bad').length
    };

    return {
      totalCount,
      filteredCount,
      selectedCount,
      favoriteCount,
      alarmCount,
      qualityStats,
      connectionStatus: isConnected ? 'connected' : 'disconnected'
    };
  }, [allData, filteredData, selectedData, favorites, isConnected]);

  // =============================================================================
  // 🔄 기존 필터링 및 UI 로직
  // =============================================================================

  useEffect(() => {
    loadMetadata();
  }, [loadMetadata]);

  useEffect(() => {
    if (devices.length > 0 && dataPoints.length > 0) {
      setIsLoading(true);
      loadRealtimeData().finally(() => setIsLoading(false));
    }
  }, [devices.length, dataPoints.length, loadRealtimeData]);

  // 실시간 데이터 업데이트
  useEffect(() => {
    if (!autoRefresh || !isConnected) return;

    const interval = setInterval(() => {
      updateRealTimeData();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, updateRealTimeData, isConnected]);

  // 필터링 및 정렬
  useEffect(() => {
    applyFiltersAndSort();
  }, [allData, selectedFactories, selectedCategories, selectedQualities, searchTerm, sortBy, sortOrder, showFavoritesOnly]);

  const applyFiltersAndSort = () => {
    let filtered = allData;

    // 즐겨찾기 필터
    if (showFavoritesOnly) {
      filtered = filtered.filter(item => favorites.includes(item.id));
    }

    // 공장 필터
    if (!selectedFactories.includes('all')) {
      filtered = filtered.filter(item => selectedFactories.includes(item.factory));
    }

    // 카테고리 필터
    if (!selectedCategories.includes('all')) {
      filtered = filtered.filter(item => selectedCategories.includes(item.category));
    }

    // 품질 필터
    if (!selectedQualities.includes('all')) {
      filtered = filtered.filter(item => selectedQualities.includes(item.quality));
    }

    // 검색 필터
    if (searchTerm) {
      filtered = filtered.filter(item =>
        item.displayName.toLowerCase().includes(searchTerm.toLowerCase()) ||
        item.factory.toLowerCase().includes(searchTerm.toLowerCase()) ||
        item.device.toLowerCase().includes(searchTerm.toLowerCase()) ||
        item.category.toLowerCase().includes(searchTerm.toLowerCase()) ||
        item.tags.some(tag => tag.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    }

    // 정렬
    filtered.sort((a, b) => {
      let comparison = 0;

      switch (sortBy) {
        case 'name':
          comparison = a.displayName.localeCompare(b.displayName);
          break;
        case 'value':
          if (a.dataType === 'number' && b.dataType === 'number') {
            comparison = (a.value as number) - (b.value as number);
          } else {
            comparison = String(a.value).localeCompare(String(b.value));
          }
          break;
        case 'timestamp':
          comparison = a.timestamp.getTime() - b.timestamp.getTime();
          break;
        case 'factory':
          comparison = a.factory.localeCompare(b.factory);
          break;
      }

      return sortOrder === 'asc' ? comparison : -comparison;
    });

    setFilteredData(filtered);
    setCurrentPage(1);
  };

  const toggleFavorite = (dataId: string) => {
    setFavorites(prev => {
      const newFavorites = prev.includes(dataId)
        ? prev.filter(id => id !== dataId)
        : [...prev, dataId];

      setAllData(prevData => prevData.map(item => ({
        ...item,
        isFavorite: newFavorites.includes(item.id)
      })));

      return newFavorites;
    });
  };

  const toggleDataSelection = (data: RealTimeData) => {
    setSelectedData(prev => {
      const exists = prev.find(item => item.id === data.id);
      if (exists) {
        return prev.filter(item => item.id !== data.id);
      } else {
        return [...prev, data];
      }
    });
  };

  const formatValue = (data: RealTimeData): string => {
    if (isBlobValue(data.value)) {
      return 'FILE DATA';
    }
    if (data.dataType === 'boolean') {
      return data.value ? 'ON' : 'OFF';
    }
    if (data.dataType === 'number') {
      return `${data.value}${data.unit || ''}`;
    }
    return String(data.value);
  };

  const formatTimestamp = (timestamp: Date): string => {
    return timestamp.toLocaleTimeString('ko-KR');
  };

  const getTrendIcon = (trend: string): string => {
    switch (trend) {
      case 'up': return 'fas fa-arrow-up text-success';
      case 'down': return 'fas fa-arrow-down text-error';
      case 'stable': return 'fas fa-minus text-neutral';
      default: return 'fas fa-minus text-neutral';
    }
  };

  // 페이지네이션
  const totalPages = Math.ceil(filteredData.length / itemsPerPage);
  const startIndex = (currentPage - 1) * itemsPerPage;
  const endIndex = startIndex + itemsPerPage;
  const currentData = filteredData.slice(startIndex, endIndex);

  // 고유 값들 추출
  const uniqueFactories = [...new Set(allData.map(item => item.factory))];
  const uniqueCategories = [...new Set(allData.map(item => item.category))];

  // =============================================================================
  // 🎨 메인 렌더링 (DeviceList 스타일 적용)
  // =============================================================================

  return (
    <div className="realtime-monitor-container">
      {/* 📊 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-chart-line"></i>
            실시간 데이터 모니터링
          </h1>
          <div className="page-subtitle">
            실시간 데이터를 모니터링하고 분석합니다
          </div>
        </div>

        <div className="header-right">
          <div className="header-actions">
            <div className={`live-indicator ${isConnected ? 'connected' : 'disconnected'}`}>
              <span className={`pulse-dot ${isConnected ? 'active' : ''}`}></span>
              <span>{isConnected ? '실시간 연결됨' : '연결 끊어짐'}</span>
              {isConnected && <span className="data-count">({allData.length}개)</span>}
            </div>
            <button
              className="btn btn-outline"
              onClick={loadRealtimeData}
              disabled={isLoading}
            >
              <i className={`fas fa-sync-alt ${isLoading ? 'fa-spin' : ''}`}></i>
              새로고침
            </button>
          </div>
        </div>
      </div>

      {/* ⚠️ 에러 배너 */}
      {error && (
        <div className="error-banner">
          <div className="error-content">
            <i className="error-icon fas fa-exclamation-triangle"></i>
            <span className="error-message">{error}</span>
            <button
              className="error-retry"
              onClick={() => {
                setError(null);
                loadRealtimeData();
              }}
            >
              재시도
            </button>
          </div>
        </div>
      )}

      {/* 📊 통계 대시보드 - 가로 1줄 전체 폭 배치 */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(6, 1fr)',
        gap: '16px',
        marginBottom: '24px',
        padding: '16px 0'
      }}>
        <div style={{
          background: 'white',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          padding: '16px 20px',
          textAlign: 'center',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <div style={{ fontSize: '24px', fontWeight: '700', color: '#111827', marginBottom: '4px' }}>
            {calculatedStats.totalCount}
          </div>
          <div style={{ fontSize: '12px', color: '#6b7280', fontWeight: '500' }}>전체</div>
        </div>

        <div style={{
          background: 'white',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          padding: '16px 20px',
          textAlign: 'center',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <div style={{ fontSize: '24px', fontWeight: '700', color: '#111827', marginBottom: '4px' }}>
            {calculatedStats.filteredCount}
          </div>
          <div style={{ fontSize: '12px', color: '#6b7280', fontWeight: '500' }}>필터됨</div>
        </div>

        <div style={{
          background: 'white',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          padding: '16px 20px',
          textAlign: 'center',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <div style={{ fontSize: '24px', fontWeight: '700', color: '#111827', marginBottom: '4px' }}>
            {calculatedStats.selectedCount}
          </div>
          <div style={{ fontSize: '12px', color: '#6b7280', fontWeight: '500' }}>선택됨</div>
        </div>

        <div style={{
          background: 'white',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          padding: '16px 20px',
          textAlign: 'center',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <div style={{ fontSize: '24px', fontWeight: '700', color: '#f59e0b', marginBottom: '4px' }}>
            {calculatedStats.favoriteCount}
          </div>
          <div style={{ fontSize: '12px', color: '#6b7280', fontWeight: '500' }}>즐겨찾기</div>
        </div>

        <div style={{
          background: 'white',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          padding: '16px 20px',
          textAlign: 'center',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <div style={{ fontSize: '24px', fontWeight: '700', color: calculatedStats.alarmCount > 0 ? '#ef4444' : '#111827', marginBottom: '4px' }}>
            {calculatedStats.alarmCount}
          </div>
          <div style={{ fontSize: '12px', color: '#6b7280', fontWeight: '500' }}>알람</div>
        </div>

        <div style={{
          background: 'white',
          border: '1px solid #e5e7eb',
          borderRadius: '8px',
          padding: '16px 20px',
          textAlign: 'center',
          boxShadow: '0 1px 3px rgba(0,0,0,0.1)'
        }}>
          <div style={{
            fontSize: '14px',
            fontWeight: '600',
            color: isConnected ? '#10b981' : '#ef4444',
            marginBottom: '4px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px'
          }}>
            <i className={`fas fa-circle`} style={{ fontSize: '8px' }}></i>
            {isConnected ? '온라인' : '오프라인'}
          </div>
          <div style={{ fontSize: '12px', color: '#6b7280', fontWeight: '500' }}>연결 상태</div>
        </div>
      </div>

      {/* 로딩 상태 */}
      {isLoading && (
        <div className="loading-banner">
          <i className="fas fa-spinner fa-spin"></i>
          <span>실시간 데이터를 불러오는 중...</span>
        </div>
      )}

      {/* 필터 및 제어 패널 */}
      <div className="filter-control-panel">
        <div className="filter-section">
          <div className="filter-row">
            <div className="filter-group">
              <label>공장</label>
              <select
                multiple
                value={selectedFactories}
                onChange={(e) => setSelectedFactories(Array.from(e.target.selectedOptions, option => option.value))}
                className="filter-select"
              >
                <option value="all">전체 공장</option>
                {uniqueFactories.map(factory => (
                  <option key={factory} value={factory}>{factory}</option>
                ))}
              </select>
            </div>

            <div className="filter-group">
              <label>카테고리</label>
              <select
                multiple
                value={selectedCategories}
                onChange={(e) => setSelectedCategories(Array.from(e.target.selectedOptions, option => option.value))}
                className="filter-select"
              >
                <option value="all">전체 카테고리</option>
                {uniqueCategories.map(category => (
                  <option key={category} value={category}>{category}</option>
                ))}
              </select>
            </div>

            <div className="filter-group">
              <label>품질</label>
              <select
                multiple
                value={selectedQualities}
                onChange={(e) => setSelectedQualities(Array.from(e.target.selectedOptions, option => option.value))}
                className="filter-select"
              >
                <option value="all">전체 품질</option>
                <option value="good">양호</option>
                <option value="uncertain">불확실</option>
                <option value="bad">불량</option>
              </select>
            </div>

            <div className="filter-group flex-1">
              <label>검색</label>
              <div className="search-container">
                <input
                  type="text"
                  placeholder="이름, 공장, 디바이스, 태그 검색..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="search-input"
                />
                <i className="fas fa-search search-icon"></i>
              </div>
            </div>
          </div>

          <div className="filter-row">
            <div className="view-controls">
              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={showFavoritesOnly}
                  onChange={(e) => setShowFavoritesOnly(e.target.checked)}
                />
                즐겨찾기만 표시
              </label>

              <label className="checkbox-label">
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
                  className="refresh-select"
                >
                  <option value={1000}>1초</option>
                  <option value={2000}>2초</option>
                  <option value={5000}>5초</option>
                  <option value={10000}>10초</option>
                </select>
              )}
            </div>

            <div className="sort-controls">
              <select
                value={sortBy}
                onChange={(e) => setSortBy(e.target.value as any)}
                className="sort-select"
              >
                <option value="name">이름순</option>
                <option value="value">값순</option>
                <option value="timestamp">시간순</option>
                <option value="factory">공장순</option>
              </select>

              <button
                className={`sort-order-btn ${sortOrder}`}
                onClick={() => setSortOrder(prev => prev === 'asc' ? 'desc' : 'asc')}
              >
                <i className={`fas fa-sort-${sortOrder === 'asc' ? 'up' : 'down'}`}></i>
              </button>
            </div>

            <div className="view-mode-controls">
              <button
                className={`view-mode-btn ${viewMode === 'list' ? 'active' : ''}`}
                onClick={() => setViewMode('list')}
              >
                <i className="fas fa-list"></i>
              </button>
              <button
                className={`view-mode-btn ${viewMode === 'grid' ? 'active' : ''}`}
                onClick={() => setViewMode('grid')}
              >
                <i className="fas fa-th"></i>
              </button>
              <button
                className={`view-mode-btn ${viewMode === 'compact' ? 'active' : ''}`}
                onClick={() => setViewMode('compact')}
              >
                <i className="fas fa-bars"></i>
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* 📋 데이터 표시 영역 */}
      <div className="data-display-area">
        {viewMode === 'list' && (
          <div className="data-table">
            <div className="data-table-header">
              <div className="data-cell">선택</div>
              <div className="data-cell">이름</div>
              <div className="data-cell">값</div>
              <div className="data-cell">트렌드</div>
              <div className="data-cell">품질</div>
              <div className="data-cell">공장</div>
              <div className="data-cell">디바이스</div>
              <div className="data-cell">시간</div>
              <div className="data-cell">동작</div>
            </div>

            {currentData.map(item => (
              <div key={`${item.id}_${item.point_id}`} className={`data-table-row ${item.alarm ? 'has-alarm' : ''}`}>
                <div className="data-cell">
                  <input
                    type="checkbox"
                    checked={selectedData.some(d => d.id === item.id)}
                    onChange={() => toggleDataSelection(item)}
                  />
                </div>
                <div className="data-cell">
                  <div className="data-name">
                    {item.alarm && (
                      <i className={`alarm-icon fas fa-exclamation-triangle ${item.alarm.level}`}></i>
                    )}
                    {item.displayName}
                    <div className="data-tags">
                      {item.tags.slice(0, 2).map(tag => (
                        <span key={tag} className="tag">{tag}</span>
                      ))}
                    </div>
                  </div>
                </div>
                <div className="data-cell">
                  <span className={`data-value ${item.quality}`}>
                    {isBlobValue(item.value) ? (
                      <a href={getBlobDownloadUrl(item.value as string)} className="blob-download-link" title="Download File">
                        <i className="fas fa-file-download"></i> {formatValue(item)}
                      </a>
                    ) : (
                      formatValue(item)
                    )}
                  </span>
                </div>
                <div className="data-cell">
                  <i className={getTrendIcon(item.trend)}></i>
                </div>
                <div className="data-cell">
                  <span className={`quality-badge ${item.quality}`}>
                    {item.quality}
                  </span>
                </div>
                <div className="data-cell">{item.factory}</div>
                <div className="data-cell">{item.device}</div>
                <div className="data-cell monospace">{formatTimestamp(item.timestamp)}</div>
                <div className="data-cell">
                  <button
                    className={`favorite-btn ${item.isFavorite ? 'active' : ''}`}
                    onClick={() => toggleFavorite(item.id)}
                  >
                    <i className="fas fa-star"></i>
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}

        {viewMode === 'grid' && (
          <div className="data-grid">
            {currentData.map(item => (
              <div key={`${item.id}_${item.point_id}`} className={`data-card ${item.alarm ? 'has-alarm' : ''}`}>
                <div className="card-header">
                  <input
                    type="checkbox"
                    checked={selectedData.some(d => d.id === item.id)}
                    onChange={() => toggleDataSelection(item)}
                  />
                  <button
                    className={`favorite-btn ${item.isFavorite ? 'active' : ''}`}
                    onClick={() => toggleFavorite(item.id)}
                  >
                    <i className="fas fa-star"></i>
                  </button>
                </div>
                <div className="card-content">
                  <h4 className="card-title">
                    {item.alarm && (
                      <i className={`alarm-icon fas fa-exclamation-triangle ${item.alarm.level}`}></i>
                    )}
                    {item.displayName}
                  </h4>
                  <div className="card-value">
                    <span className={`value ${item.quality}`}>
                      {isBlobValue(item.value) ? (
                        <a href={getBlobDownloadUrl(item.value as string)} className="blob-download-link" title="Download File">
                          <i className="fas fa-file-download"></i> {formatValue(item)}
                        </a>
                      ) : (
                        formatValue(item)
                      )}
                    </span>
                    <i className={getTrendIcon(item.trend)}></i>
                  </div>
                  <div className="card-meta">
                    <div className="meta-row">
                      <span>공장:</span>
                      <span>{item.factory}</span>
                    </div>
                    <div className="meta-row">
                      <span>디바이스:</span>
                      <span>{item.device}</span>
                    </div>
                    <div className="meta-row">
                      <span>품질:</span>
                      <span className={`quality-badge ${item.quality}`}>
                        {item.quality}
                      </span>
                    </div>
                  </div>
                  <div className="card-footer">
                    <span className="timestamp">{formatTimestamp(item.timestamp)}</span>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}

        {viewMode === 'compact' && (
          <div className="data-compact">
            {currentData.map(item => (
              <div key={`${item.id}_${item.point_id}`} className={`compact-item ${item.alarm ? 'has-alarm' : ''}`}>
                <input
                  type="checkbox"
                  checked={selectedData.some(d => d.id === item.id)}
                  onChange={() => toggleDataSelection(item)}
                />
                <span className="compact-name">{item.displayName}</span>
                <span className={`compact-value ${item.quality}`}>
                  {isBlobValue(item.value) ? (
                    <a href={getBlobDownloadUrl(item.value as string)} className="blob-download-link" title="Download File">
                      <i className="fas fa-file-download"></i>
                    </a>
                  ) : (
                    formatValue(item)
                  )}
                </span>
                <i className={getTrendIcon(item.trend)}></i>
                <span className="compact-time">{formatTimestamp(item.timestamp)}</span>
                <button
                  className={`favorite-btn ${item.isFavorite ? 'active' : ''}`}
                  onClick={() => toggleFavorite(item.id)}
                >
                  <i className="fas fa-star"></i>
                </button>
              </div>
            ))}
          </div>
        )}

        {/* 빈 상태 표시 */}
        {!isLoading && currentData.length === 0 && (
          <div className="empty-state">
            <div className="empty-state-icon">
              <i className="fas fa-chart-line"></i>
            </div>
            <h3 className="empty-state-title">표시할 데이터가 없습니다</h3>
            <p className="empty-state-description">
              {filteredData.length === 0
                ? '필터 조건을 변경하거나 실시간 연결을 확인해주세요'
                : '다른 페이지를 확인해보세요'
              }
            </p>
            {!isConnected && (
              <button className="btn btn-primary" onClick={loadRealtimeData}>
                <i className="fas fa-plug"></i>
                재연결 시도
              </button>
            )}
          </div>
        )}
      </div>

      {/* 📄 페이지네이션 */}
      {filteredData.length > 0 && (
        <div className="pagination-container">
          <div className="pagination-info">
            {startIndex + 1}-{Math.min(endIndex, filteredData.length)} / {filteredData.length} 항목
          </div>

          <div className="pagination-controls">
            <select
              value={itemsPerPage}
              onChange={(e) => setItemsPerPage(Number(e.target.value))}
              className="items-per-page"
            >
              <option value={10}>10개씩</option>
              <option value={20}>20개씩</option>
              <option value={50}>50개씩</option>
              <option value={100}>100개씩</option>
            </select>

            <button
              className="btn btn-sm"
              disabled={currentPage === 1}
              onClick={() => setCurrentPage(1)}
            >
              <i className="fas fa-angle-double-left"></i>
            </button>
            <button
              className="btn btn-sm"
              disabled={currentPage === 1}
              onClick={() => setCurrentPage(prev => prev - 1)}
            >
              <i className="fas fa-angle-left"></i>
            </button>

            <span className="page-info">
              {currentPage} / {totalPages}
            </span>

            <button
              className="btn btn-sm"
              disabled={currentPage === totalPages}
              onClick={() => setCurrentPage(prev => prev + 1)}
            >
              <i className="fas fa-angle-right"></i>
            </button>
            <button
              className="btn btn-sm"
              disabled={currentPage === totalPages}
              onClick={() => setCurrentPage(totalPages)}
            >
              <i className="fas fa-angle-double-right"></i>
            </button>
          </div>
        </div>
      )}

      {/* 📊 상태 바 */}
      <div className="status-bar">
        <div className="status-info">
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          {autoRefresh && isConnected && (
            <span className="auto-refresh-indicator">
              <i className="fas fa-sync-alt fa-spin"></i>
              자동 새로고침 활성 ({refreshInterval / 1000}초)
            </span>
          )}
        </div>
      </div>

      {/* 선택된 데이터 차트 (옵션) */}
      {selectedData.length > 0 && showChart && (
        <div className="chart-panel">
          <div className="chart-header">
            <h3>실시간 차트 ({selectedData.length}개)</h3>
            <div className="chart-controls">
              <button
                className="btn btn-sm btn-outline"
                onClick={() => setChartData({})}
              >
                <i className="fas fa-trash"></i>
                데이터 초기화
              </button>
              <button
                className="btn btn-sm btn-outline"
                onClick={() => setShowChart(false)}
              >
                <i className="fas fa-times"></i>
                닫기
              </button>
            </div>
          </div>
          <div className="chart-content">
            <div className="chart-placeholder">
              <i className="fas fa-chart-line chart-icon"></i>
              <p>실시간 차트가 여기에 표시됩니다</p>
              <p className="text-sm text-neutral-500">
                선택된 {selectedData.filter(d => d.dataType === 'number').length}개의 숫자 데이터 포인트
              </p>
              <div className="chart-legend">
                {selectedData.filter(d => d.dataType === 'number').map(item => (
                  <div key={item.id} className="legend-item">
                    <span className="legend-color" style={{ backgroundColor: `hsl(${item.id.charCodeAt(0) * 137.5 % 360}, 70%, 50%)` }}></span>
                    <span className="legend-label">{item.displayName}</span>
                    <span className="legend-value">{formatValue(item)}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* 플로팅 차트 버튼 */}
      {selectedData.length > 0 && !showChart && (
        <button
          className="floating-chart-btn"
          onClick={() => setShowChart(true)}
        >
          <i className="fas fa-chart-line"></i>
          차트 보기 ({selectedData.filter(d => d.dataType === 'number').length})
        </button>
      )}
    </div>
  );
};

export default RealTimeMonitor;