import React, { useState, useEffect, useCallback } from 'react';
import '../styles/base.css';
import '../styles/real-time-monitor.css';

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
}

interface ChartData {
  timestamp: Date;
  value: number;
}

const RealTimeMonitor: React.FC = () => {
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
  const [chartData, setChartData] = useState<{[key: string]: ChartData[]}>({});
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(2000);
  const [currentPage, setCurrentPage] = useState(1);
  const [itemsPerPage, setItemsPerPage] = useState(20);
  const [favorites, setFavorites] = useState<string[]>([]);
  const [showFavoritesOnly, setShowFavoritesOnly] = useState(false);

  // 초기 데이터 로드
  useEffect(() => {
    loadInitialData();
  }, []);

  // 실시간 데이터 업데이트
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      updateRealTimeData();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval]);

  // 필터링 및 정렬
  useEffect(() => {
    applyFiltersAndSort();
  }, [allData, selectedFactories, selectedCategories, selectedQualities, searchTerm, sortBy, sortOrder, showFavoritesOnly]);

  const loadInitialData = async () => {
    // 시뮬레이션 데이터 생성
    const factories = ['Seoul Factory', 'Busan Factory', 'Daegu Factory'];
    const devices = ['PLC-001', 'PLC-002', 'HMI-001', 'Sensor-Array-01', 'Motor-Controller-01'];
    const categories = ['Temperature', 'Pressure', 'Flow', 'Level', 'Speed', 'Current', 'Voltage', 'Status'];
    const dataTypes: ('number' | 'boolean' | 'string')[] = ['number', 'boolean', 'string'];
    
    const mockData: RealTimeData[] = [];
    
    for (let i = 0; i < 100; i++) {
      const factory = factories[Math.floor(Math.random() * factories.length)];
      const device = devices[Math.floor(Math.random() * devices.length)];
      const category = categories[Math.floor(Math.random() * categories.length)];
      const dataType = dataTypes[Math.floor(Math.random() * dataTypes.length)];
      
      let value: any;
      let unit: string | undefined;
      
      switch (category) {
        case 'Temperature':
          value = (Math.random() * 80 + 20).toFixed(1);
          unit = '°C';
          break;
        case 'Pressure':
          value = (Math.random() * 5 + 1).toFixed(2);
          unit = 'bar';
          break;
        case 'Flow':
          value = (Math.random() * 100).toFixed(1);
          unit = 'L/min';
          break;
        case 'Speed':
          value = (Math.random() * 1500 + 500).toFixed(0);
          unit = 'rpm';
          break;
        case 'Current':
          value = (Math.random() * 20).toFixed(2);
          unit = 'A';
          break;
        case 'Voltage':
          value = (Math.random() * 50 + 200).toFixed(1);
          unit = 'V';
          break;
        case 'Level':
          value = (Math.random() * 100).toFixed(1);
          unit = '%';
          break;
        case 'Status':
          value = Math.random() > 0.8 ? 'Alarm' : 'Normal';
          dataType === 'boolean' ? Math.random() > 0.5 : value;
          break;
        default:
          value = (Math.random() * 100).toFixed(2);
      }

      const hasAlarm = Math.random() > 0.9;
      
      mockData.push({
        id: `data_${i}`,
        key: `pulseone:${factory.toLowerCase().replace(' ', '_')}:${device.toLowerCase()}:${category.toLowerCase()}_${i}`,
        displayName: `${category} Sensor ${String.fromCharCode(65 + (i % 26))}`,
        value: dataType === 'number' ? parseFloat(value) : value,
        unit,
        dataType,
        quality: Math.random() > 0.95 ? 'uncertain' : Math.random() > 0.98 ? 'bad' : 'good',
        timestamp: new Date(Date.now() - Math.random() * 60000),
        trend: Math.random() > 0.6 ? 'up' : Math.random() > 0.3 ? 'down' : 'stable',
        factory,
        device,
        category,
        tags: [category.toLowerCase(), device.toLowerCase(), factory.toLowerCase().split(' ')[0]],
        alarm: hasAlarm ? {
          level: ['low', 'medium', 'high', 'critical'][Math.floor(Math.random() * 4)] as any,
          message: `${category} value out of range`
        } : undefined,
        isFavorite: Math.random() > 0.8
      });
    }
    
    setAllData(mockData);
  };

  const updateRealTimeData = () => {
    setAllData(prev => prev.map(item => {
      let newValue = item.value;
      
      if (item.dataType === 'number') {
        const change = (Math.random() - 0.5) * 0.1 * item.value;
        newValue = Math.max(0, item.value + change);
        if (item.unit === '°C') newValue = Math.min(100, newValue);
        if (item.unit === 'bar') newValue = Math.min(10, newValue);
        if (item.unit === '%') newValue = Math.min(100, newValue);
        newValue = parseFloat(newValue.toFixed(2));
      } else if (item.dataType === 'boolean') {
        newValue = Math.random() > 0.95 ? !item.value : item.value;
      } else {
        newValue = Math.random() > 0.99 ? (item.value === 'Normal' ? 'Alarm' : 'Normal') : item.value;
      }

      const trend = item.dataType === 'number' 
        ? (newValue > item.value ? 'up' : newValue < item.value ? 'down' : 'stable')
        : 'stable';

      return {
        ...item,
        value: newValue,
        timestamp: new Date(),
        trend,
        quality: Math.random() > 0.98 ? 'uncertain' : 'good'
      };
    }));

    // 차트 데이터 업데이트
    selectedData.forEach(item => {
      if (item.dataType === 'number') {
        setChartData(prev => ({
          ...prev,
          [item.id]: [
            ...(prev[item.id] || []).slice(-19), // 최근 20개 포인트만 유지
            { timestamp: new Date(), value: item.value as number }
          ]
        }));
      }
    });
  };

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
    setCurrentPage(1); // 필터 변경 시 첫 페이지로
  };

  const toggleFavorite = (dataId: string) => {
    setFavorites(prev => {
      const newFavorites = prev.includes(dataId)
        ? prev.filter(id => id !== dataId)
        : [...prev, dataId];
      
      // 즐겨찾기 상태 업데이트
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

  return (
    <div className="realtime-monitor-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">실시간 데이터 모니터링</h1>
        <div className="page-actions">
          <div className="live-indicator">
            <span className="pulse-dot"></span>
            <span>실시간</span>
          </div>
          <button 
            className="btn btn-outline"
            onClick={loadInitialData}
          >
            <i className="fas fa-sync-alt"></i>
            새로고침
          </button>
        </div>
      </div>

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

      {/* 통계 정보 */}
      <div className="stats-bar">
        <div className="stat-item">
          <span className="stat-label">전체:</span>
          <span className="stat-value">{allData.length}</span>
        </div>
        <div className="stat-item">
          <span className="stat-label">필터됨:</span>
          <span className="stat-value">{filteredData.length}</span>
        </div>
        <div className="stat-item">
          <span className="stat-label">선택됨:</span>
          <span className="stat-value">{selectedData.length}</span>
        </div>
        <div className="stat-item">
          <span className="stat-label">즐겨찾기:</span>
          <span className="stat-value">{favorites.length}</span>
        </div>
        <div className="stat-item">
          <span className="stat-label">알람:</span>
          <span className="stat-value text-error">
            {allData.filter(item => item.alarm).length}
          </span>
        </div>
      </div>

      {/* 데이터 표시 영역 */}
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
              <div key={item.id} className={`data-table-row ${item.alarm ? 'has-alarm' : ''}`}>
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
                    {formatValue(item)}
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
              <div key={item.id} className={`data-card ${item.alarm ? 'has-alarm' : ''}`}>
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
                      {formatValue(item)}
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
              <div key={item.id} className={`compact-item ${item.alarm ? 'has-alarm' : ''}`}>
                <input
                  type="checkbox"
                  checked={selectedData.some(d => d.id === item.id)}
                  onChange={() => toggleDataSelection(item)}
                />
                <span className="compact-name">{item.displayName}</span>
                <span className={`compact-value ${item.quality}`}>
                  {formatValue(item)}
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
      </div>

      {/* 페이지네이션 */}
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

      {/* 선택된 데이터 차트 (옵션) */}
      {selectedData.length > 0 && showChart && (
        <div className="chart-panel">
          <div className="chart-header">
            <h3>실시간 차트 ({selectedData.length}개)</h3>
            <button
              className="btn btn-sm btn-outline"
              onClick={() => setShowChart(false)}
            >
              <i className="fas fa-times"></i>
              닫기
            </button>
          </div>
          <div className="chart-content">
            <div className="chart-placeholder">
              <i className="fas fa-chart-line chart-icon"></i>
              <p>실시간 차트가 여기에 표시됩니다</p>
              <p className="text-sm text-neutral-500">
                선택된 {selectedData.filter(d => d.dataType === 'number').length}개의 숫자 데이터 포인트
              </p>
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

