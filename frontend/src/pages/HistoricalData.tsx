import React, { useState, useEffect, useCallback } from 'react';
import '../styles/base.css';
import '../styles/historical-data.css';
import { isBlobValue, getBlobDownloadUrl } from '../utils/dataUtils';
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend,
  ResponsiveContainer, YAxisProps
} from 'recharts';

interface HistoricalDataPoint {
  id: string;
  realPointId?: number; // DB의 실제 ID
  deviceName: string;
  pointName: string;
  value: number | string | boolean;
  unit?: string;
  quality: 'good' | 'bad' | 'uncertain';
  timestamp: Date;
  factory: string;
  deviceType: string;
  category: string;
}

interface QueryCondition {
  deviceIds: string[];
  pointNames: string[];
  factories: string[];
  categories: string[];
  qualities: string[];
  dateRange: {
    start: Date;
    end: Date;
  };
  aggregation: 'none' | 'avg' | 'min' | 'max' | 'sum' | 'count';
  interval: '1m' | '5m' | '15m' | '1h' | '6h' | '1d';
}

interface AggregatedData {
  timestamp: Date;
  value: number;
  count: number;
  pointName: string;
}

// 메타데이터 인터페이스
interface PointMetadata {
  id: number;
  name: string;
  deviceId: number;
  deviceName: string;
  siteId: number;
  siteName: string;
}

interface DeviceMetadata {
  id: string; // deviceName
  realId: number; // device_id
  points: { id: number; name: string }[];
}

interface SiteMetadata {
  id: string; // siteName
  realId: number; // site_id
  devices: DeviceMetadata[];
}

const HistoricalData: React.FC = () => {
  const [historicalData, setHistoricalData] = useState<HistoricalDataPoint[]>([]);
  const [aggregatedData, setAggregatedData] = useState<AggregatedData[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [metadata, setMetadata] = useState<{ factories: SiteMetadata[] }>({ factories: [] });
  const [queryCondition, setQueryCondition] = useState<QueryCondition>({
    deviceIds: [],
    pointNames: [], // This will now store point IDs as strings for consistency with the filter UI
    factories: [],
    categories: [],
    qualities: ['good', 'uncertain', 'bad'],
    dateRange: {
      start: new Date(Date.now() - 24 * 60 * 60 * 1000), // 24시간 전
      end: new Date()
    },
    aggregation: 'none',
    interval: '1h'
  });

  const [currentPage, setCurrentPage] = useState(1);
  const [totalPages, setTotalPages] = useState(1);
  const [totalRecords, setTotalRecords] = useState(0);
  const [pageSize, setPageSize] = useState(100);
  const [viewMode, setViewMode] = useState<'table' | 'chart' | 'both'>('table');
  const [selectedPoints, setSelectedPoints] = useState<string[]>([]);
  const [showAdvancedFilter, setShowAdvancedFilter] = useState(false);


  const getIntervalMs = (interval: string): number => {
    switch (interval) {
      case '1m': return 60 * 1000;
      case '5m': return 5 * 60 * 1000;
      case '15m': return 15 * 60 * 1000;
      case '1h': return 60 * 60 * 1000;
      case '6h': return 6 * 60 * 60 * 1000;
      case '1d': return 24 * 60 * 60 * 1000;
      default: return 60 * 60 * 1000;
    }
  };

  // 포인트 메타데이터 로드
  useEffect(() => {
    const fetchMetadata = async () => {
      try {
        const response = await fetch('/api/data/points?limit=1000');
        const result = await response.json();

        if (result.success && result.data && result.data.items) {
          const items = result.data.items;
          const sitesMap = new Map<number, SiteMetadata>();

          items.forEach((item: any) => {
            const siteId = item.site_id || 1;
            const siteName = item.site_name || `Site ${siteId}`;
            const deviceId = item.device_id;
            const deviceName = item.device_name;

            if (!sitesMap.has(siteId)) {
              sitesMap.set(siteId, {
                id: siteName,
                realId: siteId,
                devices: []
              });
            }

            const site = sitesMap.get(siteId)!;
            let device = site.devices.find(d => d.realId === deviceId);

            if (!device) {
              device = {
                id: deviceName,
                realId: deviceId,
                points: []
              };
              site.devices.push(device);
            }

            device.points.push({
              id: item.id,
              name: item.name
            });
          });

          setMetadata({ factories: Array.from(sitesMap.values()) });
        }
      } catch (error) {
        console.error('Failed to fetch metadata:', error);
      }
    };

    fetchMetadata();
  }, []);

  // 데이터 로드
  const loadHistoricalData = async () => {
    setIsLoading(true);
    setCurrentPage(1);

    try {
      const { pointNames, dateRange, interval, aggregation } = queryCondition;

      // pointNames에 선택된 포인트 ID들이 들어있음
      if (pointNames.length === 0) {
        setHistoricalData([]);
        setAggregatedData([]);
        setTotalRecords(0);
        return;
      }

      const params = new URLSearchParams({
        point_ids: pointNames.join(','),
        start_time: dateRange.start.toISOString(),
        end_time: dateRange.end.toISOString(),
        interval: (interval as string) === 'none' ? '' : interval,
        aggregation: aggregation === 'none' ? 'mean' : aggregation
      });

      const response = await fetch(`/api/data/historical?${params.toString()}`);
      const result = await response.json();

      if (result.success && result.data) {
        const { historical_data, data_points } = result.data;

        // 데이터 매핑
        const mappedData: HistoricalDataPoint[] = historical_data.map((item: any, index: number) => {
          const pointDetail = data_points.find((p: any) => p.point_id === item.point_id);
          return {
            id: `hist_${index}`,
            realPointId: pointDetail?.point_id,
            timestamp: new Date(item.time),
            deviceName: pointDetail?.device_name || 'Unknown',
            deviceType: pointDetail?.device_name ? `ID: ${item.point_id}` : 'N/A',
            pointName: pointDetail?.point_name || `Point ${item.point_id}`,
            category: pointDetail?.point_name ? `ID: ${item.point_id}` : 'N/A',
            value: item.value,
            unit: pointDetail?.unit || '',
            quality: item.quality || 'good',
            factory: pointDetail?.site_name || 'N/A'
          };
        });

        // 집계 모드인 경우 aggregatedData 설정
        if (aggregation !== 'none') {
          const mappedAggregated: AggregatedData[] = historical_data.map((item: any) => ({
            timestamp: new Date(item.time),
            value: item.value,
            count: 1, // 백엔드에서 count를 주지 않으므로 1로 설정 (또는 백엔드 수정 필요)
            pointName: data_points.find((p: any) => p.point_id === item.point_id)?.point_name || `Point ${item.point_id}`
          }));
          setAggregatedData(mappedAggregated);
        } else {
          setAggregatedData([]);
        }

        setHistoricalData(mappedData);
        setTotalRecords(mappedData.length);
        setTotalPages(Math.ceil(mappedData.length / pageSize));
      }
    } catch (error) {
      console.error('Failed to load historical data:', error);
    } finally {
      setIsLoading(false);
    }
  };

  // 집계 데이터 생성 보조 함수 (필터링된 데이터 기반)
  const generateAggregatedFromData = (data: HistoricalDataPoint[]) => {
    if (queryCondition.aggregation === 'none') return [];

    const intervalMs = getIntervalMs(queryCondition.interval);
    const aggregatedMap = new Map<string, AggregatedData>();

    data.forEach(point => {
      if (typeof point.value !== 'number') return;

      const intervalStart = Math.floor(point.timestamp.getTime() / intervalMs) * intervalMs;
      const key = `${point.pointName}_${intervalStart}`;

      if (!aggregatedMap.has(key)) {
        aggregatedMap.set(key, {
          timestamp: new Date(intervalStart),
          value: point.value,
          count: 1,
          pointName: point.pointName
        });
      } else {
        const existing = aggregatedMap.get(key)!;
        switch (queryCondition.aggregation) {
          case 'avg':
            existing.value = (existing.value * existing.count + point.value) / (existing.count + 1);
            break;
          case 'min':
            existing.value = Math.min(existing.value, point.value);
            break;
          case 'max':
            existing.value = Math.max(existing.value, point.value);
            break;
          case 'sum':
            existing.value += point.value;
            break;
        }
        existing.count++;
      }
    });

    return Array.from(aggregatedMap.values()).sort((a, b) =>
      a.timestamp.getTime() - b.timestamp.getTime()
    );
  };

  // 차트 데이터 변환
  const getChartData = () => {
    const dataToUse = queryCondition.aggregation === 'none' ? historicalData : aggregatedData;
    if (dataToUse.length === 0) return [];

    // 타임스탬프별로 그룹화
    const timeMap = new Map<number, any>();

    dataToUse.forEach(item => {
      const time = item.timestamp.getTime();
      if (!timeMap.has(time)) {
        timeMap.set(time, {
          timestamp: time,
          displayTime: item.timestamp.toLocaleString('ko-KR', {
            month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit'
          })
        });
      }
      const entry = timeMap.get(time);
      entry[item.pointName] = item.value;
    });

    return Array.from(timeMap.values()).sort((a, b) => a.timestamp - b.timestamp);
  };

  const chartColors = [
    '#3b82f6', '#ef4444', '#10b981', '#f59e0b', '#8b5cf6',
    '#ec4899', '#06b6d4', '#84cc16', '#6366f1', '#f43f5e'
  ];

  // 초기 로드
  useEffect(() => {
    loadHistoricalData();
  }, [queryCondition.dateRange, queryCondition.aggregation, queryCondition.interval]);

  // 빠른 날짜 설정
  const setQuickDateRange = (hours: number) => {
    const end = new Date();
    const start = new Date(end.getTime() - hours * 60 * 60 * 1000);
    setQueryCondition(prev => ({
      ...prev,
      dateRange: { start, end }
    }));
  };

  // 데이터 내보내기
  const exportData = (format: 'csv' | 'json') => {
    const dataToExport = queryCondition.aggregation === 'none' ? historicalData : aggregatedData;

    if (format === 'csv') {
      const headers = queryCondition.aggregation === 'none'
        ? ['Device', 'Point', 'Value', 'Unit', 'Quality', 'Timestamp', 'Factory']
        : ['Point', 'Timestamp', 'Value', 'Count'];

      const csvContent = [
        headers.join(','),
        ...dataToExport.map(item => {
          if ('deviceName' in item) {
            return [
              item.deviceName,
              item.pointName,
              item.value,
              item.unit || '',
              item.quality,
              item.timestamp.toISOString(),
              item.factory
            ].join(',');
          } else {
            return [
              item.pointName,
              item.timestamp.toISOString(),
              item.value,
              item.count
            ].join(',');
          }
        })
      ].join('\n');

      const blob = new Blob([csvContent], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `historical_data_${new Date().toISOString().split('T')[0]}.csv`;
      a.click();
      URL.revokeObjectURL(url);
    } else {
      const blob = new Blob([JSON.stringify(dataToExport, null, 2)], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `historical_data_${new Date().toISOString().split('T')[0]}.json`;
      a.click();
      URL.revokeObjectURL(url);
    }
  };

  const handleResetFilters = () => {
    setQueryCondition(prev => ({
      ...prev,
      factories: [],
      deviceIds: [],
      pointNames: [],
      qualities: ['good', 'uncertain', 'bad']
    }));
  };

  // 필터링 가능한 메타데이터 계산 (계층적/Cascading)
  const availableFactories = metadata.factories.map(f => f.id);

  const availableDevices = React.useMemo(() => {
    if (queryCondition.factories.length === 0) {
      return [];
    }
    return metadata.factories
      .filter(f => queryCondition.factories.includes(f.id))
      .flatMap(f => f.devices.map(d => d.id));
  }, [metadata, queryCondition.factories]);

  const availablePoints = React.useMemo(() => {
    if (queryCondition.deviceIds.length === 0) {
      return [];
    }

    const points = metadata.factories
      .flatMap(f => f.devices)
      .filter(d => queryCondition.deviceIds.includes(d.id))
      .flatMap(d => d.points);

    // 중복 제거 및 형식 변환 (ID를 문자열로)
    const uniquePoints = new Map<string, string>();
    points.forEach(p => {
      uniquePoints.set(String(p.id), p.name);
    });

    return Array.from(uniquePoints.entries()).map(([id, name]) => ({ id, name }));
  }, [metadata, queryCondition.deviceIds]);

  // 페이지네이션
  const startIndex = (currentPage - 1) * pageSize;
  const endIndex = startIndex + pageSize;
  const currentData = historicalData.slice(startIndex, endIndex);

  return (
    <div className="historical-data-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">이력 데이터 조회</h1>
        <div className="page-actions">
          <button
            className="btn btn-outline"
            onClick={loadHistoricalData}
            disabled={isLoading}
          >
            <i className="fas fa-sync-alt"></i>
            새로고침
          </button>
          <div className="export-buttons">
            <button
              className="btn btn-primary"
              onClick={() => exportData('csv')}
              disabled={historicalData.length === 0}
            >
              <i className="fas fa-file-csv"></i>
              CSV 내보내기
            </button>
            <button
              className="btn btn-primary"
              onClick={() => exportData('json')}
              disabled={historicalData.length === 0}
            >
              <i className="fas fa-file-code"></i>
              JSON 내보내기
            </button>
          </div>
        </div>
      </div>

      {/* 검색 조건 패널 */}
      <div className="query-panel">
        <div className="query-section">
          <div className="query-header-row" style={{ display: 'flex', alignItems: 'center', gap: '24px', marginBottom: showAdvancedFilter ? '16px' : '0' }}>
            <h3 style={{ margin: 0, whiteSpace: 'nowrap', fontSize: '15px', fontWeight: 600, color: '#334155' }}>조회 조건</h3>

            {/* 기본 조건 */}
            <div className="query-filter-bar single-line" style={{ marginBottom: 0 }}>
              <div className="filter-group date-range">
                <label>조회 기간</label>
                <div className="date-range-wrapper">
                  <input
                    type="datetime-local"
                    value={queryCondition.dateRange.start.toISOString().slice(0, 16)}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      dateRange: { ...prev.dateRange, start: new Date(e.target.value) }
                    }))}
                    className="date-input"
                  />
                  <span className="date-separator">~</span>
                  <input
                    type="datetime-local"
                    value={queryCondition.dateRange.end.toISOString().slice(0, 16)}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      dateRange: { ...prev.dateRange, end: new Date(e.target.value) }
                    }))}
                    className="date-input"
                  />
                </div>
              </div>

              <div className="filter-group quick-ranges">
                <label>빠른 설정</label>
                <div className="quick-date-buttons">
                  <button className="btn btn-xs btn-outline" onClick={() => setQuickDateRange(1)}>1H</button>
                  <button className="btn btn-xs btn-outline" onClick={() => setQuickDateRange(6)}>6H</button>
                  <button className="btn btn-xs btn-outline" onClick={() => setQuickDateRange(24)}>1D</button>
                  <button className="btn btn-xs btn-outline" onClick={() => setQuickDateRange(24 * 7)}>7D</button>
                  <button className="btn btn-xs btn-outline" onClick={() => setQuickDateRange(24 * 30)}>30D</button>
                </div>
              </div>

              <div className="filter-group">
                <label>집계 방식</label>
                <select
                  value={queryCondition.aggregation}
                  onChange={(e) => setQueryCondition(prev => ({
                    ...prev,
                    aggregation: e.target.value as any
                  }))}
                  className="condition-select"
                >
                  <option value="none">원본</option>
                  <option value="avg">평균</option>
                  <option value="min">최소</option>
                  <option value="max">최대</option>
                </select>
              </div>

              {queryCondition.aggregation !== 'none' && (
                <div className="filter-group">
                  <label>간격</label>
                  <select
                    value={queryCondition.interval}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      interval: e.target.value as any
                    }))}
                    className="condition-select interval-select"
                  >
                    <option value="1m">1분</option>
                    <option value="1h">1시간</option>
                    <option value="1d">1일</option>
                  </select>
                </div>
              )}

              <div className="filter-group search-action" style={{ flexDirection: 'row', alignItems: 'flex-end', gap: '8px' }}>
                <div>
                  <label>&nbsp;</label>
                  <button
                    className="btn btn-primary search-btn"
                    onClick={loadHistoricalData}
                    disabled={isLoading}
                  >
                    <i className="fas fa-search"></i>
                    조회
                  </button>
                </div>

                {/* 고급 필터 버튼을 여기로 이동 */}
                <div>
                  <label>&nbsp;</label>
                  <button
                    className="btn btn-outline btn-sm search-btn" // search-btn 클래스 재사용하여 높이 맞춤
                    onClick={() => setShowAdvancedFilter(!showAdvancedFilter)}
                    style={{ background: 'white' }}
                  >
                    <i className={`fas fa-chevron-${showAdvancedFilter ? 'up' : 'down'}`}></i>
                    고급 필터
                  </button>
                </div>
              </div>
            </div>
          </div>

          {showAdvancedFilter && (
            <div className="advanced-conditions">
              <div className="hierarchical-filter-header">
                <h3>고급 검색 조건</h3>
                <button className="btn btn-xs btn-outline btn-reset" onClick={handleResetFilters}>
                  <i className="fas fa-undo"></i> 전체 초기화
                </button>
              </div>

              <div className="hierarchical-filter-container">
                {/* 사이트 선택 */}
                <div className="drilldown-column">
                  <div className="column-header">
                    <label>사이트 (공장)</label>
                    <div className="column-actions">
                      <button onClick={() => setQueryCondition(prev => ({ ...prev, factories: availableFactories, deviceIds: [], pointNames: [] }))}>전체</button>
                      <button onClick={() => setQueryCondition(prev => ({ ...prev, factories: [], deviceIds: [], pointNames: [] }))}>해제</button>
                    </div>
                  </div>
                  <div className="drilldown-list">
                    {availableFactories.map(factory => (
                      <div
                        key={factory}
                        className={`list-item ${queryCondition.factories.includes(factory) ? 'selected' : ''}`}
                        onClick={() => {
                          const next = queryCondition.factories.includes(factory)
                            ? queryCondition.factories.filter(f => f !== factory)
                            : [...queryCondition.factories, factory];
                          setQueryCondition(prev => ({ ...prev, factories: next, deviceIds: [], pointNames: [] }));
                        }}
                      >
                        <span className="checkbox"></span>
                        <span className="item-text">{factory}</span>
                      </div>
                    ))}
                  </div>
                </div>

                {/* 디바이스 선택 */}
                <div className="drilldown-column">
                  <div className="column-header">
                    <label>디바이스</label>
                    <div className="column-actions">
                      <button onClick={() => setQueryCondition(prev => ({ ...prev, deviceIds: availableDevices, pointNames: [] }))}>전체</button>
                      <button onClick={() => setQueryCondition(prev => ({ ...prev, deviceIds: [], pointNames: [] }))}>해제</button>
                    </div>
                  </div>
                  <div className="drilldown-list">
                    {availableDevices.map(device => (
                      <div
                        key={device}
                        className={`list-item ${queryCondition.deviceIds.includes(device) ? 'selected' : ''}`}
                        onClick={() => {
                          const next = queryCondition.deviceIds.includes(device)
                            ? queryCondition.deviceIds.filter(d => d !== device)
                            : [...queryCondition.deviceIds, device];
                          setQueryCondition(prev => ({ ...prev, deviceIds: next, pointNames: [] }));
                        }}
                      >
                        <span className="checkbox"></span>
                        <span className="item-text">{device}</span>
                      </div>
                    ))}
                    {availableDevices.length === 0 && <div className="list-empty">공장을 먼저 선택하세요</div>}
                  </div>
                </div>

                {/* 데이터 포인트 선택 */}
                <div className="drilldown-column">
                  <div className="column-header">
                    <label>데이터 포인트</label>
                    <div className="column-actions">
                      <button onClick={() => setQueryCondition(prev => ({ ...prev, pointNames: availablePoints.map(p => p.id) }))}>전체</button>
                      <button onClick={() => setQueryCondition(prev => ({ ...prev, pointNames: [] }))}>해제</button>
                    </div>
                  </div>
                  <div className="drilldown-list">
                    {availablePoints.map(point => (
                      <div
                        key={point.id}
                        className={`list-item ${queryCondition.pointNames.includes(point.id) ? 'selected' : ''}`}
                        onClick={() => {
                          const next = queryCondition.pointNames.includes(point.id)
                            ? queryCondition.pointNames.filter(p => p !== point.id)
                            : [...queryCondition.pointNames, point.id];
                          setQueryCondition(prev => ({ ...prev, pointNames: next }));
                        }}
                      >
                        <span className="checkbox"></span>
                        <span className="item-text">{point.name}</span>
                      </div>
                    ))}
                    {availablePoints.length === 0 && <div className="list-empty">디바이스를 먼저 선택하세요</div>}
                  </div>
                </div>
              </div>

              <div className="quality-filter-row">
                <label>데이터 품질</label>
                <div className="filter-tags">
                  {[
                    { val: 'good', label: '양호' },
                    { val: 'uncertain', label: '불확실' },
                    { val: 'bad', label: '불량' }
                  ].map(quality => (
                    <button
                      key={quality.val}
                      className={`filter-tag ${queryCondition.qualities.includes(quality.val) ? 'active' : ''}`}
                      onClick={() => {
                        const next = queryCondition.qualities.includes(quality.val)
                          ? queryCondition.qualities.filter(q => q !== quality.val)
                          : [...queryCondition.qualities, quality.val];
                        setQueryCondition(prev => ({ ...prev, qualities: next }));
                      }}
                    >
                      {quality.label}
                    </button>
                  ))}
                </div>
              </div>
            </div>
          )}

        </div>
      </div>

      {/* 결과 통계 */}
      <div className="result-stats">
        <div className="stats-info">
          <div className="stats-item">
            <span className="stats-label">TOTAL</span>
            <span className="total-records">{totalRecords.toLocaleString()}</span>
            <span className="stats-unit">REC</span>
          </div>
          <div className="stats-divider"></div>
          <div className="stats-item date-range-display">
            <i className="far fa-calendar-alt"></i>
            <span className="date-text">{queryCondition.dateRange.start.toLocaleString('ko-KR', { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false })}</span>
            <span className="date-sep">~</span>
            <span className="date-text">{queryCondition.dateRange.end.toLocaleString('ko-KR', { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false })}</span>
          </div>
          {queryCondition.aggregation !== 'none' && (
            <>
              <div className="stats-divider"></div>
              <div className="stats-item aggregation-tag">
                <span className="agg-mode">{queryCondition.aggregation.toUpperCase()}</span>
                <span className="agg-interval">{queryCondition.interval}</span>
              </div>
            </>
          )}
        </div>

        <div className="view-controls">
          <div className="view-mode-toggle-group">
            <button
              className={`view-toggle-btn ${viewMode === 'table' ? 'active' : ''}`}
              onClick={() => setViewMode('table')}
            >
              <i className="fas fa-table"></i>
              <span>테이블</span>
            </button>
            <button
              className={`view-toggle-btn ${viewMode === 'chart' ? 'active' : ''}`}
              onClick={() => setViewMode('chart')}
            >
              <i className="fas fa-chart-line"></i>
              <span>차트</span>
            </button>
            <button
              className={`view-toggle-btn ${viewMode === 'both' ? 'active' : ''}`}
              onClick={() => setViewMode('both')}
            >
              <i className="fas fa-columns"></i>
              <span>전체보기</span>
            </button>
          </div>
        </div>
      </div>

      {/* 데이터 표시 영역 */}
      <div className="data-display-container">
        {isLoading ? (
          <div className="loading-container">
            <div className="loading-spinner"></div>
            <span>데이터를 조회하고 있습니다...</span>
          </div>
        ) : (
          <>
            {(viewMode === 'chart' || viewMode === 'both') && (
              <div className="chart-section">
                <div className="chart-header">
                  <h3>시계열 차트</h3>
                  <div className="point-tag-selector">
                    {availablePoints.map(point => (
                      <button
                        key={point.id}
                        className={`point-tag ${selectedPoints.includes(point.id) ? 'active' : ''}`}
                        onClick={() => {
                          if (selectedPoints.includes(point.id)) {
                            setSelectedPoints(selectedPoints.filter(p => p !== point.id));
                          } else {
                            setSelectedPoints([...selectedPoints, point.id]);
                          }
                        }}
                      >
                        {point.name}
                      </button>
                    ))}
                  </div>
                  <div className="chart-selection-actions">
                    <button
                      className="btn-chart-action"
                      onClick={() => setSelectedPoints(availablePoints.map(p => p.id))}
                    >전체 선택</button>
                    <button
                      className="btn-chart-action"
                      onClick={() => setSelectedPoints([])}
                    >선택 해제</button>
                  </div>
                </div>
                <div className="chart-container">
                  {selectedPoints.length > 0 && (queryCondition.aggregation === 'none' ? historicalData : aggregatedData).length > 0 ? (
                    <ResponsiveContainer width="100%" height="100%">
                      <LineChart data={getChartData()} margin={{ top: 10, right: 30, left: 0, bottom: 0 }}>
                        <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#f0f0f0" />
                        <XAxis
                          dataKey="displayTime"
                          tick={{ fontSize: 11, fill: '#94a3b8' }}
                          axisLine={{ stroke: '#e2e8f0' }}
                          tickLine={false}
                        />
                        <YAxis
                          tick={{ fontSize: 11, fill: '#94a3b8' }}
                          axisLine={{ stroke: '#e2e8f0' }}
                          tickLine={false}
                          domain={['auto', 'auto']}
                        />
                        <Tooltip
                          contentStyle={{
                            borderRadius: '8px',
                            border: 'none',
                            boxShadow: '0 4px 12px rgba(0,0,0,0.1)',
                            fontSize: '12px'
                          }}
                        />
                        <Legend iconType="circle" />
                        {selectedPoints.map((point, index) => (
                          <Line
                            key={point}
                            type="monotone"
                            dataKey={point}
                            stroke={chartColors[index % chartColors.length]}
                            strokeWidth={2}
                            dot={{ r: 2, fill: chartColors[index % chartColors.length] }}
                            activeDot={{ r: 5 }}
                            animationDuration={300}
                          />
                        ))}
                      </LineChart>
                    </ResponsiveContainer>
                  ) : (
                    <div className="chart-placeholder">
                      <i className="fas fa-chart-line chart-icon"></i>
                      <p>시계열 차트가 여기에 표시됩니다</p>
                      <p className="text-sm text-neutral-500">
                        {selectedPoints.length > 0
                          ? '표시할 데이터가 없습니다 (조회 조건을 확인하세요)'
                          : '표시할 데이터 포인트를 선택하세요'}
                      </p>
                    </div>
                  )}
                </div>
              </div>
            )}

            {(viewMode === 'table' || viewMode === 'both') && (
              <div className="table-section">
                {viewMode === 'both' && (
                  <div className="section-divider-header">
                    <h3>상세 데이터 명세</h3>
                  </div>
                )}
                <div className="data-table-container">
                  <table className="data-table">
                    <thead>
                      <tr>
                        {queryCondition.aggregation === 'none' ? (
                          <>
                            <th className="num-cell">No.</th>
                            <th className="device-cell">디바이스</th>
                            <th className="point-cell">포인트명</th>
                            <th className="value-cell">값</th>
                            <th className="quality-cell">품질</th>
                            <th className="timestamp-cell">타임스탬프</th>
                            <th className="factory-cell">공장</th>
                          </>
                        ) : (
                          <>
                            <th className="num-cell">No.</th>
                            <th className="point-cell">포인트명</th>
                            <th className="timestamp-cell">타임스탬프</th>
                            <th className="value-cell">집계값</th>
                            <th className="count-cell">데이터 수</th>
                          </>
                        )}
                      </tr>
                    </thead>
                    <tbody>
                      {queryCondition.aggregation === 'none' ? (
                        currentData.map((item, index) => (
                          <tr key={item.id}>
                            <td className="num-cell">
                              {startIndex + index + 1}
                            </td>
                            <td className="device-cell">
                              <div className="device-info">
                                <span className="device-name">{item.deviceName}</span>
                                <span className="device-type">{item.deviceType}</span>
                              </div>
                            </td>
                            <td className="point-cell">
                              <div className="point-info">
                                <span className="point-name">{item.pointName}</span>
                                <span className="point-category">{item.category}</span>
                              </div>
                            </td>
                            <td className="value-cell">
                              <span className="data-value">
                                {isBlobValue(item.value) ? (
                                  <a href={getBlobDownloadUrl(item.value as string)} className="blob-download-link" title="Download File">
                                    <i className="fas fa-file-download"></i> FILE DATA
                                  </a>
                                ) : (
                                  typeof item.value === 'number' ? item.value.toFixed(2) : String(item.value)
                                )}
                                {item.unit && <span className="unit">{item.unit}</span>}
                              </span>
                            </td>
                            <td className="quality-cell">
                              <span className={`quality-badge ${item.quality}`}>
                                {item.quality}
                              </span>
                            </td>
                            <td className="timestamp-cell monospace">
                              {item.timestamp.toLocaleString()}
                            </td>
                            <td className="factory-cell">{item.factory}</td>
                          </tr>
                        ))
                      ) : (
                        aggregatedData.slice(startIndex, endIndex).map((item, index) => (
                          <tr key={`agg_${index}`}>
                            <td className="num-cell">
                              {startIndex + index + 1}
                            </td>
                            <td className="point-cell">{item.pointName}</td>
                            <td className="timestamp-cell monospace">
                              {item.timestamp.toLocaleString()}
                            </td>
                            <td className="value-cell">
                              <span className="data-value">
                                {item.value.toFixed(2)}
                              </span>
                            </td>
                            <td className="count-cell">{item.count}</td>
                          </tr>
                        ))
                      )}
                    </tbody>
                  </table>

                  {(queryCondition.aggregation === 'none' ? currentData : aggregatedData).length === 0 && (
                    <div className="empty-state">
                      <i className="fas fa-database empty-icon"></i>
                      <div className="empty-title">조회 결과가 없습니다</div>
                      <div className="empty-description">
                        조회 조건을 변경하여 다시 시도해보세요.
                      </div>
                    </div>
                  )}
                </div>

                {/* 페이지네이션 */}
                {totalPages > 1 && (
                  <div className="pagination-container">
                    <div className="pagination-info">
                      {startIndex + 1}-{Math.min(endIndex, totalRecords)} / {totalRecords} 항목
                    </div>

                    <div className="pagination-controls">
                      <select
                        value={pageSize}
                        onChange={(e) => {
                          setPageSize(Number(e.target.value));
                          setCurrentPage(1);
                        }}
                        className="page-size-select"
                      >
                        <option value={50}>50개씩</option>
                        <option value={100}>100개씩</option>
                        <option value={200}>200개씩</option>
                        <option value={500}>500개씩</option>
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
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );
};

export default HistoricalData;

