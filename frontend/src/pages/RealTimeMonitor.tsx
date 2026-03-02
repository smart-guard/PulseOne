// ============================================================================
// frontend/src/pages/RealTimeMonitor.tsx
// ⚡ Real-time Data Monitoring - 진짜 API 연결 + 부드러운 새로고침
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { RealtimeApiService, RealtimeValue } from '../api/services/realtimeApi';
import { DataApiService } from '../api/services/dataApi';
import { DeviceApiService } from '../api/services/deviceApi';
import '../styles/base.css';
import '../styles/real-time-monitor.css?v=guaranteed_isolation_v2'; // Bump version
import { isBlobValue, getBlobDownloadUrl } from '../utils/dataUtils';

interface RealTimeData {
  id: string;
  key: string;
  displayName: string;
  value: any;
  unit?: string;
  dataType: 'number' | 'boolean' | 'string';
  quality: 'good' | 'bad' | 'uncertain' | 'comm_failure' | 'last_known';
  timestamp: Date;
  trend: 'up' | 'down' | 'stable';
  factory: string;
  customer: string; // 🔥 NEW: 멀티 테넌트용
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
  const { t } = useTranslation(['monitor', 'common']);
  const [filteredData, setFilteredData] = useState<RealTimeData[]>([]);

  // 🔍 Advanced Filter Sate for Drill-down
  const [showAdvancedFilter, setShowAdvancedFilter] = useState(false);
  const [selectedCustomers, setSelectedCustomers] = useState<string[]>([]); // 🔥 NEW
  const [selectedFactories, setSelectedFactories] = useState<string[]>([]);
  const [selectedDeviceIds, setSelectedDeviceIds] = useState<number[]>([]);
  const [selectedPointIds, setSelectedPointIds] = useState<number[]>([]);

  const [selectedQualities, setSelectedQualities] = useState<string[]>([]);
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

  // ✍️ 포인트 제어 state
  const [writeValues, setWriteValues] = useState<Record<number, string>>({}); // point_id → 입력중인 값
  const [writeStatus, setWriteStatus] = useState<Record<number, 'idle' | 'sending' | 'success' | 'error'>>({});
  const [writeMessages, setWriteMessages] = useState<Record<number, string>>({});
  const [writeModal, setWriteModal] = useState<{ item: any; dpMeta: any; confirmValue?: string } | null>(null); // 팝업 제어

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
      case 'Temperature': threshold = { min: 0, max: 80 }; break;
      case 'Pressure': threshold = { min: 0, max: 10 }; break;
      case 'Flow': threshold = { min: 0, max: 150 }; break;
      case 'Level': threshold = { min: 0, max: 100 }; break;
      case 'Current': threshold = { min: 0, max: 25 }; break;
      case 'Voltage': threshold = { min: 180, max: 280 }; break;
      default: return undefined;
    }

    if (threshold.min !== undefined && value < threshold.min) {
      return { level: 'medium' as const, message: `${category} value is below minimum threshold (${threshold.min}).` };
    }

    if (threshold.max !== undefined && value > threshold.max) {
      return { level: 'high' as const, message: `${category} value exceeds maximum threshold (${threshold.max}).` };
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

  const loadMetadata = useCallback(async () => {
    try {
      console.log('📱 메타데이터 로드 시작...');
      const devicesResponse = await DeviceApiService.getDevices({ page: 1, limit: 1000 });
      if (devicesResponse.success && devicesResponse.data) {
        setDevices(devicesResponse.data.items || []);
      }
      const dataPointsResponse = await DataApiService.searchDataPoints({
        page: 1, limit: 1000, enabled_only: true, include_current_value: false
      });
      if (dataPointsResponse.success && dataPointsResponse.data) {
        setDataPoints(dataPointsResponse.data.items || []);
      }
      const statsResponse = await DataApiService.getDataStatistics({ time_range: '1h' });
      if (statsResponse.success && statsResponse.data) {
        setDataStatistics(statsResponse.data);
      }
    } catch (err) {
      console.error('❌ 메타데이터 로드 실패:', err);
    }
  }, []);

  const loadRealtimeData = useCallback(async () => {
    try {
      setError(null);
      setIsLoading(true);
      console.log('⚡ 실시간 데이터 로드 시작...');

      // 🔥 Filter Logic for API Request
      // If we have specific filters, request ONLY those devices to ensure we get their data
      // even if they are not in the default top 1000 keys.
      let targetDeviceIds: number[] = [];
      const hasDeviceFilter = selectedDeviceIds.length > 0;
      const hasFactoryFilter = selectedFactories.length > 0;
      const hasCustomerFilter = selectedCustomers.length > 0;

      if (hasDeviceFilter) {
        targetDeviceIds = selectedDeviceIds;
      } else if (hasFactoryFilter || hasCustomerFilter) {
        // Filter devices based on selected factories/customers
        targetDeviceIds = devices.filter(d => {
          const matchFactory = hasFactoryFilter ? selectedFactories.includes(d.site_name || 'Main Factory') : true;
          const matchCustomer = hasCustomerFilter ? selectedCustomers.includes(d.tenant_name || 'My Company') : true;
          return matchFactory && matchCustomer;
        }).map(d => d.id);
      }

      const params: any = { limit: 1000, quality_filter: 'all' };
      if (targetDeviceIds.length > 0) {
        // 🔥 Fix: Pass array of strings, not joined string
        // Remote service expects string[] and handles joining internally
        params.device_ids = targetDeviceIds.map(String);
      }

      const response = await RealtimeApiService.getCurrentValues(params);

      if (response.success && response.data) {
        // 🔥 Fix: Handle direct array response from backend
        // Backend returns { success: true, data: [...] } where data is the array of values
        const realtimeValues = Array.isArray(response.data)
          ? response.data
          : (response.data['current_values'] || []);

        if (realtimeValues.length === 0) {
          setAllData([]);
          // If we filtered but got no results, it means no data in Redis for these devices
          // But we should still consider it "connected" effectively
          setIsConnected(true);
          return;
        }

        const transformedData: RealTimeData[] = realtimeValues.map((value: RealtimeValue) => {
          const pointId = Number(value.point_id);
          const deviceId = Number(value.device_id); // 🔥 Fix: Ensure number type

          const dataPoint = dataPoints.find(dp => dp.id === pointId);
          const device = devices.find(d => d.id === deviceId || d.id === dataPoint?.device_id);
          const category = inferCategory(dataPoint?.name || value.point_name || 'Unknown');

          return {
            id: `point_${pointId}`,
            key: `pulseone:${device?.name || 'unknown'}:${pointId}`,
            displayName: dataPoint?.name || value.point_name || `Point ${pointId}`,
            value: value.value,
            unit: value.unit || dataPoint?.unit,
            dataType: (value.data_type || dataPoint?.data_type || 'string') as 'number' | 'boolean' | 'string',
            quality: value.quality as any,
            timestamp: new Date(value.timestamp),
            trend: 'stable' as const,
            customer: device?.tenant_name || 'My Company',
            factory: device?.site_name || extractFactory(device?.name || 'Main Factory'),
            device: device?.name || `Device ${deviceId}`,
            category,
            tags: [category.toLowerCase(), device?.protocol_type || 'unknown'],
            alarm: undefined,
            isFavorite: favorites.includes(`point_${pointId}`),
            point_id: pointId,
            device_id: deviceId || dataPoint?.device_id || 0 // 🔥 Ensure number
          };
        });

        // Compute trends based on previous data
        setAllData(prevData => {
          // Optimization: Create map for O(1) lookup
          const prevMap = new Map(prevData.map(p => [p.point_id, p.value]));

          return transformedData.map(newItem => {
            const prevValue = prevMap.get(newItem.point_id);
            if (prevValue !== undefined && newItem.dataType === 'number') {
              const trend: 'up' | 'down' | 'stable' = newItem.value > prevValue ? 'up' :
                newItem.value < prevValue ? 'down' : 'stable';
              return { ...newItem, trend };
            }
            return newItem;
          });
        });

        setIsConnected(true);
        setLastUpdate(new Date());
      } else {
        throw new Error(response.error || 'Backend API response error');
      }
    } catch (err) {
      console.error('❌ 실시간 데이터 로드 실패:', err);
      setError(err instanceof Error ? err.message : 'Backend API connection failed');
      setIsConnected(false);
      setAllData([]);
    } finally {
      setIsLoading(false);
    }
  }, [devices, dataPoints, favorites, selectedDeviceIds, selectedFactories, selectedCustomers]);

  const updateRealTimeData = useCallback(async () => {
    if (!isConnected || allData.length === 0) return;
    try {
      const pointIds = allData.map(item => item.point_id);
      const response = await RealtimeApiService.getCurrentValues({ limit: pointIds.length > 0 ? pointIds.length : 1000 });
      if (response.success && response.data) {
        // 🔥 Fix: Handle direct array response
        const updatedValues = Array.isArray(response.data)
          ? response.data
          : (response.data['current_values'] || []);

        setAllData(prev => prev.map(item => {
          const updated = updatedValues.find(uv => uv.point_id === item.point_id);
          if (updated) {
            const trend = item.dataType === 'number'
              ? (updated.value > item.value ? 'up' : updated.value < item.value ? 'down' : 'stable')
              : 'stable';
            return {
              ...item,
              value: updated.value,
              quality: updated.quality as any,
              timestamp: new Date(updated.timestamp),
              trend: trend as any,
              alarm: (updated as any).alarm || generateAlarmIfNeeded(updated.value, item.category)
            };
          }
          return item;
        }));
        setLastUpdate(new Date());
        selectedData.forEach(item => {
          const updated = updatedValues.find(uv => uv.point_id === item.point_id);
          if (updated && item.dataType === 'number') {
            setChartData(prev => ({
              ...prev,
              [item.id]: [
                ...(prev[item.id] || []).slice(-19),
                { timestamp: new Date(updated.timestamp), value: updated.value as number }
              ]
            }));
          }
        });
      }
    } catch (err) {
      console.error('❌ 실시간 업데이트 실패:', err);
    }
  }, [isConnected, allData, selectedData]);

  // =============================================================================
  // 🔄 Filter Logic & Derivation
  // =============================================================================

  // 0. Available Customers (System Admin Only)
  const availableCustomers = useMemo(() => {
    return [...new Set(devices.map(d => d.tenant_name || 'My Company'))].sort();
  }, [devices]);

  // 1. Available Factories (Sites) - Filtered by Customer
  const availableFactories = useMemo(() => {
    let targetDevices = devices;
    if (selectedCustomers.length > 0) {
      targetDevices = devices.filter(d => selectedCustomers.includes(d.tenant_name || 'My Company'));
    }
    return [...new Set(targetDevices.map(d => d.site_name || 'Main Factory'))].sort();
  }, [devices, selectedCustomers]);

  // 2. Available Devices (Filtered by Selected Factories AND Customers)
  const availableDevices = useMemo(() => {
    let targetDevices = devices;
    if (selectedCustomers.length > 0) {
      targetDevices = targetDevices.filter(d => selectedCustomers.includes(d.tenant_name || 'My Company'));
    }
    if (selectedFactories.length > 0) {
      targetDevices = targetDevices.filter(d => selectedFactories.includes(d.site_name || 'Main Factory'));
    }
    return targetDevices;
  }, [devices, selectedFactories, selectedCustomers]);

  // 3. Available Points (Filtered by Selected Devices)
  const availablePoints = useMemo(() => {
    // If specific devices selected, limit to those. OTHERWISE, limit to available devices (factory-filtered)
    let targetDevices = availableDevices;
    if (selectedDeviceIds.length > 0) {
      targetDevices = devices.filter(d => selectedDeviceIds.includes(d.id));
    }
    const targetDeviceIds = targetDevices.map(d => d.id);
    return dataPoints.filter(dp => targetDeviceIds.includes(dp.device_id));
  }, [dataPoints, availableDevices, selectedDeviceIds, devices]);

  // 🔥 Auto-select Logic
  // 1. If only 1 Customer, select it
  useEffect(() => {
    if (availableCustomers.length === 1 && selectedCustomers.length === 0) {
      setSelectedCustomers([availableCustomers[0]]);
    }
  }, [availableCustomers, selectedCustomers]);

  // 2. If only 1 Factory, select it
  useEffect(() => {
    if (availableFactories.length === 1 && selectedFactories.length === 0) {
      setSelectedFactories([availableFactories[0]]);
    }
  }, [availableFactories, selectedFactories]);

  // Apply Filters
  const applyFiltersAndSort = (resetPage = true) => {
    let filtered = allData;

    // Favorites
    if (showFavoritesOnly) filtered = filtered.filter(item => favorites.includes(item.id));

    // Drill-down 0: Customer (System Admin)
    if (selectedCustomers.length > 0) {
      filtered = filtered.filter(item => selectedCustomers.includes(item.customer));
    }

    // Drill-down 1: Factory
    if (selectedFactories.length > 0) {
      filtered = filtered.filter(item => selectedFactories.includes(item.factory));
    }

    // Drill-down 2: Device
    if (selectedDeviceIds.length > 0) {
      filtered = filtered.filter(item => selectedDeviceIds.includes(item.device_id));
    }

    // Drill-down 3: Point
    if (selectedPointIds.length > 0) {
      filtered = filtered.filter(item => selectedPointIds.includes(item.point_id));
    }

    // Quick Filter: Quality
    if (selectedQualities.length > 0) {
      filtered = filtered.filter(item => selectedQualities.includes(item.quality));
    }

    // Search
    if (searchTerm) {
      filtered = filtered.filter(item =>
        item.displayName.toLowerCase().includes(searchTerm.toLowerCase()) ||
        item.factory.toLowerCase().includes(searchTerm.toLowerCase()) ||
        item.device.toLowerCase().includes(searchTerm.toLowerCase()) ||
        item.tags.some(tag => tag.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    }

    // Sort
    filtered.sort((a, b) => {
      let comparison = 0;
      switch (sortBy) {
        case 'name': comparison = a.displayName.localeCompare(b.displayName); break;
        case 'value':
          if (a.dataType === 'number' && b.dataType === 'number') comparison = (a.value as number) - (b.value as number);
          else comparison = String(a.value).localeCompare(String(b.value));
          break;
        case 'timestamp': comparison = a.timestamp.getTime() - b.timestamp.getTime(); break;
        case 'factory': comparison = a.factory.localeCompare(b.factory); break;
      }
      return sortOrder === 'asc' ? comparison : -comparison;
    });

    setFilteredData(filtered);
    if (resetPage) setCurrentPage(1);
  };

  // =============================================================================
  // 🔄 Efffects
  // =============================================================================

  useEffect(() => { loadMetadata(); }, [loadMetadata]);
  useEffect(() => {
    if (devices.length > 0) {
      setIsLoading(true);
      loadRealtimeData().finally(() => setIsLoading(false));
    }
  }, [devices.length, loadRealtimeData]);

  useEffect(() => {
    if (!autoRefresh || !isConnected) return;
    const interval = setInterval(() => { updateRealTimeData(); }, refreshInterval);
    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, updateRealTimeData, isConnected]);

  useEffect(() => {
    applyFiltersAndSort(false); // data refresh: keep current page
  }, [allData]);

  useEffect(() => {
    applyFiltersAndSort(true); // filter/sort change: reset to page 1
  }, [selectedCustomers, selectedFactories, selectedDeviceIds, selectedPointIds, selectedQualities, searchTerm, sortBy, sortOrder, showFavoritesOnly]);

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
    return { totalCount, filteredCount, selectedCount, favoriteCount, alarmCount, qualityStats, connectionStatus: isConnected ? 'connected' : 'disconnected' };
  }, [allData, filteredData, selectedData, favorites, isConnected]);

  // Helpers UI
  const toggleFavorite = (dataId: string) => { /* ... (Same as before) */
    setFavorites(prev => {
      const newFavorites = prev.includes(dataId) ? prev.filter(id => id !== dataId) : [...prev, dataId];
      setAllData(prevData => prevData.map(item => ({ ...item, isFavorite: newFavorites.includes(item.id) })));
      return newFavorites;
    });
  };
  const toggleDataSelection = (data: RealTimeData) => { /* ... */
    setSelectedData(prev => prev.find(item => item.id === data.id) ? prev.filter(item => item.id !== data.id) : [...prev, data]);
  };
  const formatValue = (data: RealTimeData): string => {
    if (isBlobValue(data.value)) return 'FILE DATA';
    if (data.dataType === 'boolean') return data.value ? 'ON' : 'OFF';
    if (data.dataType === 'number') return `${data.value}${data.unit || ''}`;
    // ISO timestamp 감지
    const str = String(data.value);
    if (/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}/.test(str)) {
      try {
        const d = new Date(str);
        const today = new Date();
        const todayStart = new Date(today.getFullYear(), today.getMonth(), today.getDate());
        const dStart = new Date(d.getFullYear(), d.getMonth(), d.getDate());
        const dayDiff = Math.round((dStart.getTime() - todayStart.getTime()) / 86400000);
        const time = d.toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
        return dayDiff === 0 ? time : `${time} (${dayDiff > 0 ? '+' : ''}${dayDiff}d)`;
      } catch { /* fall through */ }
    }
    return str;
  };
  const formatTimestamp = (timestamp: Date): string => timestamp.toLocaleTimeString('ko-KR');
  const getTrendIcon = (trend: string): string => { /* ... */
    switch (trend) { case 'up': return 'fas fa-arrow-up text-success'; case 'down': return 'fas fa-arrow-down text-error'; default: return 'fas fa-minus text-neutral'; }
  };

  // ✍️ 포인트 제어 함수
  const handleWrite = useCallback(async (item: RealTimeData, value: string) => {
    const pointId = item.point_id;
    const deviceId = item.device_id;
    if (!deviceId || !value.trim()) return;

    setWriteStatus(prev => ({ ...prev, [pointId]: 'sending' }));
    setWriteMessages(prev => ({ ...prev, [pointId]: '' }));

    try {
      const result = await DataApiService.writePoint(deviceId, pointId, value.trim());
      if (result.success) {
        setWriteStatus(prev => ({ ...prev, [pointId]: 'success' }));
        setWriteMessages(prev => ({ ...prev, [pointId]: 'Sent successfully' }));
      } else {
        setWriteStatus(prev => ({ ...prev, [pointId]: 'error' }));
        setWriteMessages(prev => ({ ...prev, [pointId]: result.message || 'Error' }));
      }
    } catch (err) {
      setWriteStatus(prev => ({ ...prev, [pointId]: 'error' }));
      setWriteMessages(prev => ({ ...prev, [pointId]: err instanceof Error ? err.message : 'Error occurred' }));
    }

    // 3초 후 상태 Reset
    setTimeout(() => {
      setWriteStatus(prev => ({ ...prev, [pointId]: 'idle' }));
      setWriteMessages(prev => ({ ...prev, [pointId]: '' }));
    }, 3000);
  }, []);

  // Pagination
  const totalPages = Math.ceil(filteredData.length / itemsPerPage);
  const startIndex = (currentPage - 1) * itemsPerPage;
  const endIndex = startIndex + itemsPerPage;
  const currentData = filteredData.slice(startIndex, endIndex);

  // =============================================================================
  // 🎨 Main Render
  // =============================================================================

  return (
    <div className="realtime-monitor-container">
      {/* 1. Page Header */}
      <div className="page-header">
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <div className="page-title">
            <i className="fas fa-microchip"></i>
            Real-time Data Monitoring
          </div>
          <div className="page-subtitle" style={{ marginLeft: '34px', color: '#666', fontSize: '14px' }}>
            Precisely monitor all sensor data from industrial sites with sub-second latency.
          </div>
        </div>
      </div>

      {/* 2. Query Panel (Drill Down + Controls) */}
      <div className="query-panel">
        <div className="query-section">
          <div className="query-header-row" style={{ display: 'flex', alignItems: 'center', gap: '24px', marginBottom: showAdvancedFilter ? '16px' : '0' }}>
            <h3 style={{ margin: 0, whiteSpace: 'nowrap', fontSize: '15px', fontWeight: 600, color: '#334155' }}>{t('header.monitoringConditions', { ns: 'monitor' })}</h3>

            <div className="query-filter-bar control-strip" style={{ flex: 1, display: 'flex', alignItems: 'center', padding: 0, background: 'transparent', border: 'none', boxShadow: 'none', marginBottom: 0 }}>
              {/* 1. Left: Search (Primary Discovery) */}
              <div className="filter-group search-group expanded" style={{ margin: 0 }}>
                <div className="search-container">
                  <i className="fas fa-search search-icon"></i>
                  <input
                    type="text"
                    className="rt-input"
                    placeholder="Search point, device, site..."
                    value={searchTerm}
                    onChange={(e) => setSearchTerm(e.target.value)}
                  />
                </div>
              </div>

              {/* 2. Right: Operational Controls */}
              <div className="control-actions-group">
                <div className="control-item">
                  <span className="control-label">{t('interval', { ns: 'monitor' })}</span>
                  <select
                    className="rt-select compact"
                    value={refreshInterval}
                    onChange={(e) => setRefreshInterval(Number(e.target.value))}
                    disabled={!autoRefresh}
                  >
                    <option value={1000}>1s</option>
                    <option value={2000}>2s</option>
                    <option value={3000}>3s</option>
                    <option value={5000}>5s</option>
                  </select>
                </div>

                <div className="control-divider"></div>

                <div className="control-item">
                  <span className="control-label">{t('live', { ns: 'monitor' })}</span>
                  <label className="toggle-switch compact">
                    <input
                      type="checkbox"
                      checked={autoRefresh}
                      onChange={(e) => setAutoRefresh(e.target.checked)}
                    />
                    <span className="toggle-slider"></span>
                  </label>
                </div>

                <div className="control-divider"></div>

                <button
                  className="btn-icon-only"
                  onClick={loadRealtimeData}
                  disabled={isLoading}
                  title="Manual Refresh"
                >
                  <i className={`fas fa-sync-alt ${isLoading ? 'fa-spin' : ''}`}></i>
                </button>

                <div className="control-divider"></div>

                {/* ⭐ Favorites Toggle */}
                <button
                  className={`btn-filter-toggle ${showFavoritesOnly ? 'active' : ''}`}
                  onClick={() => setShowFavoritesOnly(!showFavoritesOnly)}
                  title={showFavoritesOnly ? 'Show All' : 'Favorites Only'}
                  style={{ gap: '4px' }}
                >
                  <i className={`fas fa-star`} style={{ color: showFavoritesOnly ? '#f59e0b' : undefined }}></i>
                  <span style={{ fontSize: '12px' }}>{calculatedStats.favoriteCount}</span>
                </button>

                <div className="control-divider"></div>

                <button
                  className={`btn-filter-toggle ${showAdvancedFilter ? 'active' : ''}`}
                  onClick={() => setShowAdvancedFilter(!showAdvancedFilter)}
                  title="Open Structural Filter"
                >
                  <i className="fas fa-filter"></i>
                  <span>{t('filter', { ns: 'monitor' })}</span>
                  <i className={`fas fa-chevron-${showAdvancedFilter ? 'up' : 'down'} arrow`}></i>
                </button>
              </div>
            </div>
          </div>

          {showAdvancedFilter && (
            <div className="advanced-conditions">
              <div className="hierarchical-filter-header">
                <h3>{t('siteHierarchy', { ns: 'monitor' })}</h3>
                <button className="btn-reset" onClick={() => {
                  setSelectedFactories([]);
                  setSelectedDeviceIds([]);
                  setSelectedPointIds([]);
                  setSelectedQualities([]);
                }}>
                  <i className="fas fa-undo"></i> Reset
                </button>
              </div>

              <div className="hierarchical-filter-container">
                {/* 0. Customer (System Admin Only) */}
                {/* 0. Customer (System Admin Only) */}
                {availableCustomers.length > 1 && (
                  <div className="drilldown-column">
                    <div className="column-header">
                      <label>{t('customer', { ns: 'monitor' })}</label>
                      <div className="column-actions">
                        <button onClick={() => setSelectedCustomers(availableCustomers)}>All</button>
                        <button onClick={() => setSelectedCustomers([])}>{t('deselectAll', { ns: 'monitor' })}</button>
                      </div>
                    </div>
                    <div className="drilldown-list">
                      {availableCustomers.map(customer => (
                        <div
                          key={customer}
                          className={`list-item ${selectedCustomers.includes(customer) ? 'selected' : ''}`}
                          onClick={() => {
                            const next = selectedCustomers.includes(customer)
                              ? selectedCustomers.filter(c => c !== customer)
                              : [...selectedCustomers, customer];
                            setSelectedCustomers(next);
                            // Reset lower levels? No, keep logic flexible
                          }}
                        >
                          <div className="checkbox">
                            {selectedCustomers.includes(customer) && <i className="fas fa-check"></i>}
                          </div>
                          <span>{customer}</span>
                        </div>
                      ))}
                    </div>
                  </div>
                )}

                {/* 1. Site/Factory */}
                <div className="drilldown-column">
                  <div className="column-header">
                    <label>{t('site', { ns: 'monitor' })}</label>
                    <div className="column-actions">
                      <button onClick={() => setSelectedFactories(availableFactories)}>All</button>
                      <button onClick={() => setSelectedFactories([])}>{t('deselectAll', { ns: 'monitor' })}</button>
                    </div>
                  </div>
                  <div className="drilldown-list">
                    {availableFactories.map(factory => (
                      <div
                        key={factory}
                        className={`list-item ${selectedFactories.includes(factory) ? 'selected' : ''}`}
                        onClick={() => {
                          const next = selectedFactories.includes(factory)
                            ? selectedFactories.filter(f => f !== factory)
                            : [...selectedFactories, factory];
                          setSelectedFactories(next);
                          // Reset lower levels? No, keep logic flexible to avoid frustration
                        }}
                      >
                        <span className="checkbox"></span>
                        <span className="item-text">{factory}</span>
                      </div>
                    ))}
                  </div>
                </div>

                {/* 2. Device */}
                <div className="drilldown-column">
                  <div className="column-header">
                    <label>Device ({availableDevices.length})</label>
                    <div className="column-actions">
                      <button onClick={() => setSelectedDeviceIds(availableDevices.map(d => d.id))}>All</button>
                      <button onClick={() => setSelectedDeviceIds([])}>{t('deselectAll', { ns: 'monitor' })}</button>
                    </div>
                  </div>
                  <div className="drilldown-list">
                    {availableDevices.map(device => (
                      <div
                        key={device.id}
                        className={`list-item ${selectedDeviceIds.includes(device.id) ? 'selected' : ''}`}
                        onClick={() => {
                          const isSelected = selectedDeviceIds.includes(device.id);
                          if (isSelected) {
                            setSelectedDeviceIds(selectedDeviceIds.filter(id => id !== device.id));
                          } else {
                            setSelectedDeviceIds([...selectedDeviceIds, device.id]);

                            // 🔥 Auto-select Site if not already selected
                            // This ensures logical consistency: Device belongs to a Site
                            const siteName = device.site_name || 'Main Factory';
                            if (!selectedFactories.includes(siteName)) {
                              setSelectedFactories(prev => [...prev, siteName]);
                            }
                          }
                        }}
                      >
                        <span className="checkbox"></span>
                        <span className="item-text">{device.name}</span>
                      </div>
                    ))}
                    {availableDevices.length === 0 && <div className="list-empty">{t('labels.selectASite', { ns: 'monitor' })}</div>}
                  </div>
                </div>


              </div>

              {/* Quality Filter (Moved here) */}
              <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginTop: '16px' }}>
                <label style={{ fontSize: '12px', fontWeight: 700, color: '#6b7280' }}>{t('dataQuality', { ns: 'monitor' })}</label>
                <div style={{ display: 'flex', gap: '8px' }}>
                  {[
                    { val: 'good', label: 'Good' },
                    { val: 'uncertain', label: 'Uncertain' },
                    { val: 'bad', label: 'Bad' }
                  ].map(q => (
                    <button
                      key={q.val}
                      className={`filter-tag ${selectedQualities.includes(q.val) ? 'active' : ''}`}
                      style={{
                        padding: '4px 12px',
                        borderRadius: '16px',
                        border: selectedQualities.includes(q.val) ? '1px solid var(--primary-500)' : '1px solid var(--neutral-300)',
                        background: selectedQualities.includes(q.val) ? 'var(--primary-50)' : 'white',
                        color: selectedQualities.includes(q.val) ? 'var(--primary-700)' : 'var(--neutral-600)',
                        fontSize: '11px',
                        fontWeight: 600,
                        cursor: 'pointer'
                      }}
                      onClick={() => {
                        const next = selectedQualities.includes(q.val)
                          ? selectedQualities.filter(v => v !== q.val)
                          : [...selectedQualities, q.val];
                        setSelectedQualities(next);
                      }}
                    >
                      {q.label}
                    </button>
                  ))}
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* 3. Result Stats Bar */}
      <div className="result-stats">
        <div className="stats-info">
          <div className="stats-item">
            <span className="stats-label">{t('labels.total', { ns: 'monitor' })}</span>
            <span className="stats-value">{calculatedStats.totalCount}</span>
            <span className="stats-unit">{t('labels.pts', { ns: 'monitor' })}</span>
          </div>
          <div className="stats-divider"></div>
          <div className="stats-item">
            <span className="stats-label">{t('labels.selected', { ns: 'monitor' })}</span>
            <span className="stats-value">{selectedData.length}</span>
          </div>
          <div className="stats-divider"></div>
          <div className="stats-item">
            <span className="stats-label">{t('labels.alarms', { ns: 'monitor' })}</span>
            <span className="stats-value" style={{ color: calculatedStats.alarmCount > 0 ? 'var(--error-600)' : 'inherit' }}>
              {calculatedStats.alarmCount}
            </span>
          </div>
          <div className="stats-divider"></div>
          <div className={`status-badge ${isConnected ? 'connected' : 'disconnected'}`}>
            <span className={`pulse-dot-mini ${isConnected ? 'active' : ''}`}></span>
            {isConnected ? 'Server Connected' : 'Disconnected'}
          </div>
        </div>

        <div className="view-mode-toggle-group">
          <button className={`view-toggle-btn ${viewMode === 'list' ? 'active' : ''}`} onClick={() => setViewMode('list')} title="List View">
            <i className="fas fa-list"></i>
          </button>
          <button className={`view-toggle-btn ${viewMode === 'grid' ? 'active' : ''}`} onClick={() => setViewMode('grid')} title="Grid View">
            <i className="fas fa-th-large"></i>
          </button>
          <button className={`view-toggle-btn ${viewMode === 'compact' ? 'active' : ''}`} onClick={() => setViewMode('compact')} title="Compact View">
            <i className="fas fa-align-justify"></i>
          </button>
        </div>
      </div>

      {/* 4. Data Display Area */}
      <div className="data-display-container">
        {isLoading && allData.length === 0 ? (
          <div className="rt-empty-state">
            <i className="fas fa-spinner fa-spin"></i>
            <p>{t('loadingData', { ns: 'monitor' })}</p>
          </div>
        ) : (
          <>
            {viewMode === 'list' && (
              <table className="rt-table">
                <thead>
                  <tr>
                    <th style={{ width: '40px' }}><input type="checkbox" onChange={(e) => { if (e.target.checked) setSelectedData(filteredData); else setSelectedData([]); }} checked={selectedData.length > 0 && selectedData.length === filteredData.length} /></th>
                    <th style={{ width: '80px', whiteSpace: 'normal', lineHeight: '1.3', textAlign: 'center' }}>
                      {(t('statusQuality', { ns: 'monitor' }) as string).split('/').map((s, i) => <span key={i}>{i > 0 && <br />}{s}</span>)}
                    </th>
                    <th style={{ width: '160px' }}>{t('pointName', { ns: 'monitor' })}</th>
                    <th style={{ width: '120px' }}>{t('currentValue', { ns: 'monitor' })}</th>
                    <th style={{ width: '60px' }}>{t('trend', { ns: 'monitor' })}</th>
                    <th>{t('labels.sitelocation', { ns: 'monitor' })}</th>
                    <th>{t('device', { ns: 'monitor' })}</th>
                    <th>{t('labels.lastUpdated', { ns: 'monitor' })}</th>
                    <th style={{ width: '140px' }}>{t('control', { ns: 'monitor' })}</th>
                  </tr>
                </thead>
                <tbody>
                  {currentData.map(item => {
                    const dpMeta = dataPoints.find((dp: any) => dp.id === item.point_id);
                    const isWritable = dpMeta?.access_mode === 'read_write' || dpMeta?.access_mode === 'write';
                    const isBoolType = item.dataType === 'boolean' || dpMeta?.data_type === 'BOOL';
                    const wStatus = writeStatus[item.point_id] || 'idle';
                    return (
                      <tr key={`${item.id}_${item.point_id}`}>
                        <td><input type="checkbox" checked={selectedData.some(d => d.id === item.id)} onChange={() => toggleDataSelection(item)} /></td>
                        <td><span className={`quality-badge ${item.quality}`}>{item.quality}</span></td>
                        <td><div style={{ fontWeight: 600 }}>{item.displayName}</div><div style={{ fontSize: '11px', color: '#9ca3af' }}>{item.category}</div></td>
                        <td style={{ maxWidth: '160px', overflow: 'hidden' }}>
                          <span
                            className={`cell-value ${item.trend === 'up' ? 'value-up' : item.trend === 'down' ? 'value-down' : 'value-stable'}`}
                            style={{ display: 'block', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}
                            title={isBlobValue(item.value) ? 'FILE' : String(formatValue(item))}
                          >
                            {isBlobValue(item.value) ? 'FILE' : formatValue(item)}
                          </span>
                          {item.unit && <span style={{ fontSize: '11px', color: '#6b7280', marginLeft: '4px' }}>{item.unit}</span>}
                        </td>
                        <td>
                          {item.trend === 'up' && <i className="fas fa-arrow-up value-up"></i>}
                          {item.trend === 'down' && <i className="fas fa-arrow-down value-down"></i>}
                          {item.trend === 'stable' && <i className="fas fa-minus value-stable"></i>}
                        </td>
                        <td>{item.factory}</td>
                        <td>{item.device}</td>
                        <td style={{ fontFamily: 'monospace', fontSize: '12px' }}>{formatTimestamp(item.timestamp)}</td>

                        {/* ✍️ 제어 + ★ 한 칸 */}
                        <td style={{ width: '140px' }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', justifyContent: 'space-between' }}>
                            {isWritable ? (
                              <button
                                onClick={() => setWriteModal({ item, dpMeta })}
                                style={{
                                  padding: '4px 10px', fontSize: '11px', fontWeight: 600,
                                  background: '#3b82f6', color: 'white',
                                  border: 'none', borderRadius: '4px', cursor: 'pointer',
                                  display: 'flex', alignItems: 'center', gap: '4px'
                                }}
                              >
                                <i className="fas fa-edit"></i> Write
                              </button>
                            ) : (
                              <span style={{ fontSize: '11px', color: '#9ca3af' }}>{t('labels.readOnly', { ns: 'monitor' })}</span>
                            )}
                            <button style={{ border: 'none', background: 'none', cursor: 'pointer', color: item.isFavorite ? '#f59e0b' : '#d1d5db', fontSize: '14px', flexShrink: 0 }} onClick={() => toggleFavorite(item.id)}><i className="fas fa-star"></i></button>
                          </div>
                        </td>
                      </tr>
                    );
                  })}
                  {currentData.length === 0 && (<tr><td colSpan={10} style={{ textAlign: 'center', padding: '40px', color: '#9ca3af' }}>{t('labels.noSearchResults', { ns: 'monitor' })}</td></tr>)}
                </tbody>
              </table>
            )}
            {viewMode === 'grid' && (
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '16px', padding: '24px' }}>
                {currentData.map(item => (
                  <div key={`${item.id}_${item.point_id}`} style={{ border: '1px solid #e5e7eb', borderRadius: '8px', padding: '16px', background: 'white' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
                      <span className={`quality-badge ${item.quality}`}>{item.quality}</span>
                      <button style={{ border: 'none', background: 'none', cursor: 'pointer', color: item.isFavorite ? '#f59e0b' : '#d1d5db' }} onClick={() => toggleFavorite(item.id)}><i className="fas fa-star"></i></button>
                    </div>
                    <div style={{ marginBottom: '16px' }}>
                      <div style={{ fontSize: '11px', color: '#6b7280', textTransform: 'uppercase' }}>{item.category}</div>
                      <div style={{ fontWeight: 700, fontSize: '16px' }}>{item.displayName}</div>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-end' }}>
                      <div style={{ fontSize: '24px', fontWeight: 700, fontFamily: 'monospace' }}>
                        {isBlobValue(item.value) ? 'FILE' : formatValue(item)}
                        <span style={{ fontSize: '12px', marginLeft: '4px', color: '#6b7280' }}>{item.unit}</span>
                      </div>
                      <div>{item.trend === 'up' && <i className="fas fa-arrow-up value-up"></i>}{item.trend === 'down' && <i className="fas fa-arrow-down value-down"></i>}</div>
                    </div>
                  </div>
                ))}
              </div>
            )}
            {viewMode === 'compact' && (
              <div style={{ padding: '16px' }}>
                {currentData.map(item => (
                  <div key={`${item.id}_${item.point_id}`} style={{ display: 'flex', alignItems: 'center', padding: '8px', borderBottom: '1px solid #f3f4f6' }}>
                    <div style={{ width: '30px' }}><span className={`quality-badge ${item.quality}`} style={{ padding: '2px', width: '8px', height: '8px', borderRadius: '50%', textIndent: '-9999px', display: 'block' }}></span></div>
                    <div style={{ width: '200px', fontWeight: 600 }}>{item.displayName}</div>
                    <div style={{ flex: 1, fontFamily: 'monospace', fontWeight: 700 }}>{isBlobValue(item.value) ? 'FILE' : formatValue(item)}</div>
                    <div style={{ width: '150px', fontSize: '12px', color: '#6b7280' }}>{formatTimestamp(item.timestamp)}</div>
                  </div>
                ))}
              </div>
            )}
          </>
        )}
      </div>

      {/* Pagination */}
      {totalPages > 1 && (
        <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: '16px', gap: '8px' }}>
          <button className="rt-action-btn" disabled={currentPage === 1} onClick={() => setCurrentPage(p => Math.max(1, p - 1))}>{t('labels.prev', { ns: 'monitor' })}</button>
          <span style={{ display: 'flex', alignItems: 'center', fontWeight: 600, color: '#4b5563' }}>{currentPage} / {totalPages}</span>
          <button className="rt-action-btn" disabled={currentPage === totalPages} onClick={() => setCurrentPage(p => Math.min(totalPages, p + 1))}>Next</button>
        </div>
      )}
      {/* ✍️ Write 팝업 모달 (2-Step Confirm) */}
      {writeModal && (() => {
        const { item, dpMeta, confirmValue } = writeModal;
        const rawType = (dpMeta?.data_type || '').toUpperCase();
        const isBool = item.dataType === 'boolean' || rawType === 'BOOL';
        const isInt = ['UINT16', 'UINT32', 'INT16', 'INT32', 'UINT8', 'INT8'].includes(rawType);
        const isFloat = ['FLOAT32', 'FLOAT64', 'DOUBLE', 'REAL'].includes(rawType) || (!isBool && !isInt && item.dataType === 'number');
        const isStr = rawType === 'STRING' || item.dataType === 'string';
        const wStatus = writeStatus[item.point_id] || 'idle';
        const pendingVal = writeValues[item.point_id] ?? '';

        // 제약값
        const intRange: Record<string, [number, number]> = {
          UINT8: [0, 255], INT8: [-128, 127], UINT16: [0, 65535], INT16: [-32768, 32767],
          UINT32: [0, 4294967295], INT32: [-2147483648, 2147483647]
        };
        const [minV, maxV] = intRange[rawType] ?? [undefined, undefined];

        // 현재 사용자 (JWT 디코딩)
        const operator = (() => {
          try {
            const tok = localStorage.getItem('auth_token') || '';
            const payload = JSON.parse(atob(tok.split('.')[1]));
            return payload.username || payload.name || payload.sub || 'Unknown';
          } catch { return 'Unknown'; }
        })();

        const confirmAndSend = (val: string) =>
          setWriteModal(prev => prev ? { ...prev, confirmValue: val } : prev);

        const doSend = () =>
          handleWrite(item, confirmValue!).then(() => setTimeout(() => setWriteModal(null), 1200));

        return (
          <div style={{
            position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.55)',
            display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 9999
          }} onClick={() => setWriteModal(null)}>
            <div style={{
              background: 'white', borderRadius: '14px', padding: '28px 32px',
              width: '420px', boxShadow: '0 20px 60px rgba(0,0,0,0.3)',
              display: 'flex', flexDirection: 'column', gap: '18px',
              transition: 'all 0.2s'
            }} onClick={e => e.stopPropagation()}>

              {/* Header */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                <div>
                  <div style={{ fontWeight: 700, fontSize: '17px', color: '#1e293b' }}>{item.displayName}</div>
                  <div style={{ fontSize: '12px', color: '#94a3b8', marginTop: '2px' }}>
                    {item.device} · {item.factory}
                    {' '}·{' '}
                    <span style={{ background: '#f1f5f9', borderRadius: '4px', padding: '1px 6px', fontSize: '11px', fontWeight: 600 }}>
                      {rawType || item.dataType}
                    </span>
                  </div>
                </div>
                <button onClick={() => setWriteModal(null)} style={{ border: 'none', background: 'none', fontSize: '20px', cursor: 'pointer', color: '#94a3b8' }}>✕</button>
              </div>

              {/* Current Value */}
              <div style={{ background: '#f8fafc', borderRadius: '8px', padding: '12px 16px' }}>
                <div style={{ fontSize: '10px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px', letterSpacing: '0.08em' }}>CURRENT VALUE</div>
                <div style={{ fontSize: '22px', fontWeight: 700, color: '#0f172a', fontFamily: 'monospace' }}>
                  {formatValue(item)}
                  {item.unit && <span style={{ fontSize: '14px', color: '#64748b', marginLeft: '6px' }}>{item.unit}</span>}
                </div>
              </div>

              {/* Operator */}
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '12px', color: '#64748b' }}>
                <i className="fas fa-user-circle" style={{ fontSize: '14px' }} />
                <span>Operator: <strong style={{ color: '#1e293b' }}>{operator}</strong></span>
                <span style={{ marginLeft: 'auto', fontFamily: 'monospace' }}>
                  {new Date().toLocaleTimeString('ko-KR')}
                </span>
              </div>

              {/* ─── Step 1: Input ─── */}
              {!confirmValue && (
                <div>
                  <div style={{ fontSize: '10px', color: '#64748b', fontWeight: 700, marginBottom: '10px', letterSpacing: '0.08em' }}>WRITE VALUE</div>

                  {isBool ? (
                    <div style={{ display: 'flex', gap: '12px' }}>
                      {[{ v: '1', label: 'ON', color: '#22c55e' }, { v: '0', label: 'OFF', color: '#e5e7eb', tc: '#374151' }].map(({ v, label, color, tc }) => (
                        <button key={v} onClick={() => confirmAndSend(v)}
                          style={{
                            flex: 1, padding: '14px', fontWeight: 700, fontSize: '15px',
                            background: color, color: tc || 'white', border: 'none', borderRadius: '8px', cursor: 'pointer'
                          }}>
                          {label}
                        </button>
                      ))}
                    </div>
                  ) : (
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <input
                        type={isStr ? 'text' : 'number'}
                        step={isFloat ? 'any' : '1'}
                        min={minV}
                        max={maxV}
                        value={pendingVal}
                        onChange={e => setWriteValues(prev => ({ ...prev, [item.point_id]: e.target.value }))}
                        onKeyDown={e => e.key === 'Enter' && pendingVal.trim() && confirmAndSend(pendingVal)}
                        placeholder={isStr ? '값 입력…' : (minV !== undefined ? `${minV} ~ ${maxV}` : '숫자 입력…')}
                        autoFocus
                        style={{
                          flex: 1, padding: '10px 14px', fontSize: '14px',
                          border: '1.5px solid #d1d5db', borderRadius: '8px', outline: 'none'
                        }}
                      />
                      <button
                        onClick={() => confirmAndSend(pendingVal)}
                        disabled={!pendingVal.trim()}
                        style={{
                          padding: '10px 18px', fontWeight: 700, fontSize: '13px',
                          background: !pendingVal.trim() ? '#e5e7eb' : '#3b82f6',
                          color: !pendingVal.trim() ? '#9ca3af' : 'white',
                          border: 'none', borderRadius: '8px', cursor: !pendingVal.trim() ? 'default' : 'pointer'
                        }}>
                        Confirm →
                      </button>
                    </div>
                  )}
                  {isInt && minV !== undefined && (
                    <div style={{ marginTop: '6px', fontSize: '11px', color: '#9ca3af' }}>
                      범위: {minV.toLocaleString()} ~ {maxV!.toLocaleString()}
                    </div>
                  )}
                </div>
              )}

              {/* ─── Step 2: Confirmation ─── */}
              {confirmValue !== undefined && (
                <div style={{
                  background: '#fffbeb', border: '1.5px solid #fbbf24',
                  borderRadius: '10px', padding: '16px 20px',
                  display: 'flex', flexDirection: 'column', gap: '12px',
                  animation: 'slideDown 0.2s ease-out'
                }}>
                  <div style={{ fontSize: '12px', fontWeight: 700, color: '#92400e', letterSpacing: '0.06em' }}>⚠️ 전송 전 확인</div>
                  <div style={{ display: 'grid', gridTemplateColumns: 'auto 1fr', gap: '6px 16px', fontSize: '13px' }}>
                    <span style={{ color: '#78716c' }}>포인트</span>
                    <span style={{ fontWeight: 600, color: '#1c1917' }}>{item.displayName}</span>
                    <span style={{ color: '#78716c' }}>장치</span>
                    <span style={{ fontWeight: 600, color: '#1c1917' }}>{item.device}</span>
                    <span style={{ color: '#78716c' }}>현재값</span>
                    <span style={{ fontFamily: 'monospace', color: '#57534e' }}>{formatValue(item)}</span>
                    <span style={{ color: '#78716c' }}>전송값</span>
                    <span style={{ fontFamily: 'monospace', fontWeight: 700, fontSize: '16px', color: '#d97706' }}>
                      {isBool ? (confirmValue === '1' ? 'ON (1)' : 'OFF (0)') : confirmValue}
                      {item.unit && <span style={{ fontSize: '12px', marginLeft: '4px', color: '#a78bfa' }}>{item.unit}</span>}
                    </span>
                    <span style={{ color: '#78716c' }}>조작자</span>
                    <span style={{ fontWeight: 600, color: '#1c1917' }}>{operator}</span>
                    <span style={{ color: '#78716c' }}>시각</span>
                    <span style={{ fontFamily: 'monospace', color: '#57534e' }}>{new Date().toLocaleString('ko-KR')}</span>
                  </div>
                  <div style={{ display: 'flex', gap: '8px', marginTop: '4px' }}>
                    <button onClick={() => setWriteModal(prev => prev ? { ...prev, confirmValue: undefined } : prev)}
                      disabled={wStatus === 'sending'}
                      style={{
                        flex: 1, padding: '10px', fontSize: '13px', fontWeight: 600,
                        background: 'white', color: '#374151',
                        border: '1px solid #d1d5db', borderRadius: '8px', cursor: 'pointer'
                      }}>← 수정</button>
                    <button onClick={doSend}
                      disabled={wStatus === 'sending'}
                      style={{
                        flex: 2, padding: '10px', fontSize: '13px', fontWeight: 700,
                        background: wStatus === 'success' ? '#22c55e' : wStatus === 'error' ? '#ef4444' : '#f59e0b',
                        color: 'white', border: 'none', borderRadius: '8px', cursor: 'pointer'
                      }}>
                      {wStatus === 'sending' ? <><i className="fas fa-spinner fa-spin" /> 전송 중…</> : wStatus === 'success' ? '✓ 전송 완료' : wStatus === 'error' ? '✕ 오류' : '🚀 전송'}
                    </button>
                  </div>
                  {writeMessages[item.point_id] && (
                    <div style={{ fontSize: '12px', color: wStatus === 'error' ? '#ef4444' : '#059669', marginTop: '-4px' }}>
                      {writeMessages[item.point_id]}
                    </div>
                  )}
                </div>
              )}
            </div>
          </div>
        );
      })()}
    </div>
  );
};

export default RealTimeMonitor;