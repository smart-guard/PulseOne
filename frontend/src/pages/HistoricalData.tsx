import React, { useState, useEffect, useCallback } from 'react';
import '../styles/base.css';
import '../styles/historical-data.css';

interface HistoricalDataPoint {
  id: string;
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

const HistoricalData: React.FC = () => {
  const [historicalData, setHistoricalData] = useState<HistoricalDataPoint[]>([]);
  const [aggregatedData, setAggregatedData] = useState<AggregatedData[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [queryCondition, setQueryCondition] = useState<QueryCondition>({
    deviceIds: [],
    pointNames: [],
    factories: [],
    categories: [],
    qualities: ['good'],
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

  // 목업 데이터 생성
  const generateMockData = useCallback(() => {
    const factories = ['Seoul Factory', 'Busan Factory', 'Daegu Factory'];
    const devices = ['PLC-001', 'PLC-002', 'HMI-001', 'Sensor-Array-01', 'Motor-Controller-01'];
    const pointNames = ['Temperature', 'Pressure', 'Flow Rate', 'Level', 'Speed', 'Current', 'Voltage'];
    const categories = ['Temperature', 'Pressure', 'Flow', 'Level', 'Speed', 'Electrical'];
    const qualities: ('good' | 'bad' | 'uncertain')[] = ['good', 'bad', 'uncertain'];

    const mockData: HistoricalDataPoint[] = [];
    const startTime = queryCondition.dateRange.start.getTime();
    const endTime = queryCondition.dateRange.end.getTime();
    const timeStep = (endTime - startTime) / 500; // 500개 데이터 포인트

    for (let i = 0; i < 500; i++) {
      const timestamp = new Date(startTime + i * timeStep);
      const factory = factories[Math.floor(Math.random() * factories.length)];
      const device = devices[Math.floor(Math.random() * devices.length)];
      const pointName = pointNames[Math.floor(Math.random() * pointNames.length)];
      const category = categories[Math.floor(Math.random() * categories.length)];
      
      let value: number | string | boolean;
      let unit: string | undefined;

      switch (pointName) {
        case 'Temperature':
          value = +(Math.random() * 80 + 20).toFixed(1);
          unit = '°C';
          break;
        case 'Pressure':
          value = +(Math.random() * 5 + 1).toFixed(2);
          unit = 'bar';
          break;
        case 'Flow Rate':
          value = +(Math.random() * 100).toFixed(1);
          unit = 'L/min';
          break;
        case 'Speed':
          value = +(Math.random() * 1500 + 500).toFixed(0);
          unit = 'rpm';
          break;
        case 'Current':
          value = +(Math.random() * 20).toFixed(2);
          unit = 'A';
          break;
        case 'Voltage':
          value = +(Math.random() * 50 + 200).toFixed(1);
          unit = 'V';
          break;
        case 'Level':
          value = +(Math.random() * 100).toFixed(1);
          unit = '%';
          break;
        default:
          value = +(Math.random() * 100).toFixed(2);
      }

      mockData.push({
        id: `hist_${i}`,
        deviceName: device,
        pointName,
        value,
        unit,
        quality: qualities[Math.floor(Math.random() * qualities.length)],
        timestamp,
        factory,
        deviceType: device.startsWith('PLC') ? 'PLC' : device.startsWith('HMI') ? 'HMI' : 'Sensor',
        category
      });
    }

    return mockData.sort((a, b) => b.timestamp.getTime() - a.timestamp.getTime());
  }, [queryCondition.dateRange]);

  // 집계 데이터 생성
  const generateAggregatedData = useCallback(() => {
    if (queryCondition.aggregation === 'none') return [];

    const rawData = generateMockData();
    const intervalMs = getIntervalMs(queryCondition.interval);
    const aggregatedMap = new Map<string, AggregatedData>();

    rawData.forEach(point => {
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
  }, [queryCondition, generateMockData]);

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

  // 데이터 로드
  const loadHistoricalData = async () => {
    setIsLoading(true);
    try {
      // 시뮬레이션 딜레이
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      const mockData = generateMockData();
      const aggregated = generateAggregatedData();
      
      setHistoricalData(mockData);
      setAggregatedData(aggregated);
      setTotalRecords(mockData.length);
      setTotalPages(Math.ceil(mockData.length / pageSize));
    } catch (error) {
      console.error('Failed to load historical data:', error);
    } finally {
      setIsLoading(false);
    }
  };

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

  // 페이지네이션
  const startIndex = (currentPage - 1) * pageSize;
  const endIndex = startIndex + pageSize;
  const currentData = historicalData.slice(startIndex, endIndex);

  // 고유 값들 추출
  const uniqueFactories = [...new Set(historicalData.map(item => item.factory))];
  const uniqueDevices = [...new Set(historicalData.map(item => item.deviceName))];
  const uniquePoints = [...new Set(historicalData.map(item => item.pointName))];
  const uniqueCategories = [...new Set(historicalData.map(item => item.category))];

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
          <h3>조회 조건</h3>
          
          {/* 기본 조건 */}
          <div className="basic-conditions">
            <div className="condition-row">
              <div className="condition-group">
                <label>조회 기간</label>
                <div className="date-range-container">
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
                
                <div className="quick-date-buttons">
                  <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(1)}>1시간</button>
                  <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(6)}>6시간</button>
                  <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24)}>24시간</button>
                  <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 7)}>7일</button>
                  <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 30)}>30일</button>
                </div>
              </div>

              <div className="condition-group">
                <label>집계 방식</label>
                <select
                  value={queryCondition.aggregation}
                  onChange={(e) => setQueryCondition(prev => ({
                    ...prev,
                    aggregation: e.target.value as any
                  }))}
                  className="condition-select"
                >
                  <option value="none">집계 안함 (원본 데이터)</option>
                  <option value="avg">평균값</option>
                  <option value="min">최소값</option>
                  <option value="max">최대값</option>
                  <option value="sum">합계</option>
                  <option value="count">카운트</option>
                </select>
              </div>

              {queryCondition.aggregation !== 'none' && (
                <div className="condition-group">
                  <label>집계 간격</label>
                  <select
                    value={queryCondition.interval}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      interval: e.target.value as any
                    }))}
                    className="condition-select"
                  >
                    <option value="1m">1분</option>
                    <option value="5m">5분</option>
                    <option value="15m">15분</option>
                    <option value="1h">1시간</option>
                    <option value="6h">6시간</option>
                    <option value="1d">1일</option>
                  </select>
                </div>
              )}
            </div>
          </div>

          {/* 고급 필터 */}
          <div className="advanced-filter-toggle">
            <button
              className="btn btn-outline btn-sm"
              onClick={() => setShowAdvancedFilter(!showAdvancedFilter)}
            >
              <i className={`fas fa-chevron-${showAdvancedFilter ? 'up' : 'down'}`}></i>
              고급 필터
            </button>
          </div>

          {showAdvancedFilter && (
            <div className="advanced-conditions">
              <div className="condition-row">
                <div className="condition-group">
                  <label>공장</label>
                  <select
                    multiple
                    value={queryCondition.factories}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      factories: Array.from(e.target.selectedOptions, option => option.value)
                    }))}
                    className="condition-select multi-select"
                  >
                    {uniqueFactories.map(factory => (
                      <option key={factory} value={factory}>{factory}</option>
                    ))}
                  </select>
                </div>

                <div className="condition-group">
                  <label>디바이스</label>
                  <select
                    multiple
                    value={queryCondition.deviceIds}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      deviceIds: Array.from(e.target.selectedOptions, option => option.value)
                    }))}
                    className="condition-select multi-select"
                  >
                    {uniqueDevices.map(device => (
                      <option key={device} value={device}>{device}</option>
                    ))}
                  </select>
                </div>

                <div className="condition-group">
                  <label>데이터 포인트</label>
                  <select
                    multiple
                    value={queryCondition.pointNames}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      pointNames: Array.from(e.target.selectedOptions, option => option.value)
                    }))}
                    className="condition-select multi-select"
                  >
                    {uniquePoints.map(point => (
                      <option key={point} value={point}>{point}</option>
                    ))}
                  </select>
                </div>

                <div className="condition-group">
                  <label>품질</label>
                  <select
                    multiple
                    value={queryCondition.qualities}
                    onChange={(e) => setQueryCondition(prev => ({
                      ...prev,
                      qualities: Array.from(e.target.selectedOptions, option => option.value)
                    }))}
                    className="condition-select multi-select"
                  >
                    <option value="good">양호</option>
                    <option value="uncertain">불확실</option>
                    <option value="bad">불량</option>
                  </select>
                </div>
              </div>
            </div>
          )}

          <div className="query-actions">
            <button 
              className="btn btn-primary"
              onClick={loadHistoricalData}
              disabled={isLoading}
            >
              <i className="fas fa-search"></i>
              조회
            </button>
          </div>
        </div>
      </div>

      {/* 결과 통계 */}
      <div className="result-stats">
        <div className="stats-info">
          <span className="total-records">
            총 {totalRecords.toLocaleString()}개 레코드
          </span>
          <span className="date-range-info">
            ({queryCondition.dateRange.start.toLocaleString()} ~ {queryCondition.dateRange.end.toLocaleString()})
          </span>
          {queryCondition.aggregation !== 'none' && (
            <span className="aggregation-info">
              {queryCondition.aggregation.toUpperCase()} 집계 ({queryCondition.interval} 간격)
            </span>
          )}
        </div>

        <div className="view-controls">
          <div className="view-mode-toggle">
            <button
              className={`view-mode-btn ${viewMode === 'table' ? 'active' : ''}`}
              onClick={() => setViewMode('table')}
            >
              <i className="fas fa-table"></i>
              테이블
            </button>
            <button
              className={`view-mode-btn ${viewMode === 'chart' ? 'active' : ''}`}
              onClick={() => setViewMode('chart')}
            >
              <i className="fas fa-chart-line"></i>
              차트
            </button>
            <button
              className={`view-mode-btn ${viewMode === 'both' ? 'active' : ''}`}
              onClick={() => setViewMode('both')}
            >
              <i className="fas fa-th-large"></i>
              둘다
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
                  <div className="chart-controls">
                    <select
                      multiple
                      value={selectedPoints}
                      onChange={(e) => setSelectedPoints(Array.from(e.target.selectedOptions, option => option.value))}
                      className="point-selector"
                    >
                      {uniquePoints.map(point => (
                        <option key={point} value={point}>{point}</option>
                      ))}
                    </select>
                  </div>
                </div>
                <div className="chart-container">
                  <div className="chart-placeholder">
                    <i className="fas fa-chart-line chart-icon"></i>
                    <p>시계열 차트가 여기에 표시됩니다</p>
                    <p className="text-sm text-neutral-500">
                      {selectedPoints.length > 0 
                        ? `${selectedPoints.length}개 포인트 선택됨` 
                        : '표시할 데이터 포인트를 선택하세요'}
                    </p>
                  </div>
                </div>
              </div>
            )}

            {(viewMode === 'table' || viewMode === 'both') && (
              <div className="table-section">
                <div className="data-table-container">
                  <table className="data-table">
                    <thead>
                      <tr>
                        {queryCondition.aggregation === 'none' ? (
                          <>
                            <th>디바이스</th>
                            <th>포인트명</th>
                            <th>값</th>
                            <th>품질</th>
                            <th>타임스탬프</th>
                            <th>공장</th>
                          </>
                        ) : (
                          <>
                            <th>포인트명</th>
                            <th>타임스탬프</th>
                            <th>집계값</th>
                            <th>데이터 수</th>
                          </>
                        )}
                      </tr>
                    </thead>
                    <tbody>
                      {queryCondition.aggregation === 'none' ? (
                        currentData.map(item => (
                          <tr key={item.id}>
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
                                {typeof item.value === 'number' ? item.value.toFixed(2) : String(item.value)}
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

