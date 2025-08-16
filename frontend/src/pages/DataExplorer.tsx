// ============================================================================
// frontend/src/pages/DataExplorer.tsx
// 📝 데이터 익스플로러 - 새로운 DataApiService 완전 연결
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { DataApiService, DataPoint, CurrentValue, DataStatistics } from '../api/services/dataApi';
import { DeviceApiService } from '../api/services/deviceApi';
import '../styles/base.css';
import '../styles/data-explorer.css';
import '../styles/pagination.css';

const DataExplorer: React.FC = () => {
  // 🔧 기본 상태들
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [selectedDataPoints, setSelectedDataPoints] = useState<DataPoint[]>([]);
  const [currentValues, setCurrentValues] = useState<CurrentValue[]>([]);
  const [dataStatistics, setDataStatistics] = useState<DataStatistics | null>(null);
  const [devices, setDevices] = useState<any[]>([]);
  
  // 로딩 및 에러 상태
  const [isLoading, setIsLoading] = useState(false);
  const [isLoadingValues, setIsLoadingValues] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 검색 및 필터 상태
  const [searchTerm, setSearchTerm] = useState('');
  const [deviceFilter, setDeviceFilter] = useState<number | 'all'>('all');
  const [dataTypeFilter, setDataTypeFilter] = useState<string>('all');
  const [enabledFilter, setEnabledFilter] = useState<boolean | 'all'>('all');
  const [qualityFilter, setQualityFilter] = useState<string>('all');
  
  // 뷰 모드
  const [viewMode, setViewMode] = useState<'table' | 'card' | 'tree'>('table');
  const [showCurrentValues, setShowCurrentValues] = useState(true);
  
  // 실시간 업데이트
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5000);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // 페이징
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 50,
    totalCount: 0
  });

  // =============================================================================
  // 🔄 데이터 로드 함수들 (새로운 API 사용)
  // =============================================================================

  /**
   * 데이터포인트 목록 로드
   */
  const loadDataPoints = useCallback(async () => {
    try {
      setIsLoading(true);
      setError(null);

      console.log('📊 데이터포인트 목록 로드 시작...');

      const response = await DataApiService.searchDataPoints({
        page: pagination.currentPage,
        limit: pagination.pageSize,
        search: searchTerm || undefined,
        device_id: deviceFilter !== 'all' ? Number(deviceFilter) : undefined,
        data_type: dataTypeFilter !== 'all' ? dataTypeFilter : undefined,
        enabled_only: enabledFilter !== 'all' ? Boolean(enabledFilter) : undefined,
        sort_by: 'name',
        sort_order: 'ASC',
        include_current_value: showCurrentValues
      });

      if (response.success && response.data) {
        setDataPoints(response.data.items);
        pagination.updateTotalCount(response.data.pagination.total);
        
        console.log(`✅ 데이터포인트 ${response.data.items.length}개 로드 완료`);
      } else {
        throw new Error(response.error || '데이터포인트 로드 실패');
      }

    } catch (err) {
      console.error('❌ 데이터포인트 로드 실패:', err);
      setError(err instanceof Error ? err.message : '알 수 없는 오류');
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  }, [pagination.currentPage, pagination.pageSize, searchTerm, deviceFilter, dataTypeFilter, enabledFilter, showCurrentValues]);

  /**
   * 선택된 데이터포인트들의 현재값 로드
   */
  const loadCurrentValues = useCallback(async () => {
    if (selectedDataPoints.length === 0) {
      setCurrentValues([]);
      return;
    }

    try {
      setIsLoadingValues(true);
      console.log('⚡ 현재값 로드 시작...', selectedDataPoints.length);

      const pointIds = selectedDataPoints.map(dp => dp.id);
      const response = await DataApiService.getCurrentValues({
        point_ids: pointIds,
        quality_filter: qualityFilter !== 'all' ? qualityFilter : undefined,
        limit: 1000
      });

      if (response.success && response.data) {
        setCurrentValues(response.data.current_values);
        console.log(`✅ 현재값 ${response.data.current_values.length}개 로드 완료`);
      } else {
        throw new Error(response.error || '현재값 로드 실패');
      }

    } catch (err) {
      console.error('❌ 현재값 로드 실패:', err);
      setCurrentValues([]);
    } finally {
      setIsLoadingValues(false);
    }
  }, [selectedDataPoints, qualityFilter]);

  /**
   * 데이터 통계 로드
   */
  const loadDataStatistics = useCallback(async () => {
    try {
      console.log('📊 데이터 통계 로드 시작...');

      const response = await DataApiService.getDataStatistics({
        device_id: deviceFilter !== 'all' ? Number(deviceFilter) : undefined,
        time_range: '24h'
      });

      if (response.success && response.data) {
        setDataStatistics(response.data);
        console.log('✅ 데이터 통계 로드 완료');
      } else {
        console.warn('⚠️ 데이터 통계 로드 실패:', response.error);
      }
    } catch (err) {
      console.warn('⚠️ 데이터 통계 로드 실패:', err);
    }
  }, [deviceFilter]);

  /**
   * 디바이스 목록 로드 (필터용)
   */
  const loadDevices = useCallback(async () => {
    try {
      console.log('📱 디바이스 목록 로드 시작...');

      const response = await DeviceApiService.getDevices({
        page: 1,
        limit: 1000 // 모든 디바이스 가져오기
      });

      if (response.success && response.data) {
        setDevices(response.data.items);
        console.log(`✅ 디바이스 ${response.data.items.length}개 로드 완료`);
      } else {
        console.warn('⚠️ 디바이스 목록 로드 실패:', response.error);
      }
    } catch (err) {
      console.warn('⚠️ 디바이스 목록 로드 실패:', err);
    }
  }, []);

  // =============================================================================
  // 🔄 액션 함수들
  // =============================================================================

  /**
   * 데이터포인트 선택/해제
   */
  const handleDataPointSelect = (dataPoint: DataPoint, selected: boolean) => {
    setSelectedDataPoints(prev => 
      selected 
        ? [...prev, dataPoint]
        : prev.filter(dp => dp.id !== dataPoint.id)
    );
  };

  /**
   * 전체 선택/해제
   */
  const handleSelectAll = (selected: boolean) => {
    setSelectedDataPoints(selected ? [...dataPoints] : []);
  };

  /**
   * 데이터 내보내기
   */
  const handleExportData = async (format: 'json' | 'csv' | 'xml') => {
    if (selectedDataPoints.length === 0) {
      alert('내보낼 데이터포인트를 선택해주세요.');
      return;
    }

    try {
      console.log(`📤 데이터 내보내기 시작 (${format}):`, selectedDataPoints.length);

      const pointIds = selectedDataPoints.map(dp => dp.id);
      const response = await DataApiService.exportCurrentValues({
        point_ids: pointIds,
        format,
        include_metadata: true
      });

      if (response.success && response.data) {
        const result = response.data;
        
        // 파일 다운로드 시뮬레이션
        if (format === 'json') {
          const blob = new Blob([JSON.stringify(result.data, null, 2)], { type: 'application/json' });
          const url = URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          a.download = result.filename;
          a.click();
          URL.revokeObjectURL(url);
        }
        
        console.log(`✅ 데이터 내보내기 완료: ${result.total_records}개 레코드`);
        alert(`데이터 내보내기 완료: ${result.total_records}개 레코드`);
      } else {
        throw new Error(response.error || '데이터 내보내기 실패');
      }

    } catch (err) {
      console.error('❌ 데이터 내보내기 실패:', err);
      setError(err instanceof Error ? err.message : '데이터 내보내기 실패');
    }
  };

  /**
   * 필터 초기화
   */
  const handleResetFilters = () => {
    setSearchTerm('');
    setDeviceFilter('all');
    setDataTypeFilter('all');
    setEnabledFilter('all');
    setQualityFilter('all');
    pagination.goToFirst();
  };

  // =============================================================================
  // 🔄 이벤트 핸들러들
  // =============================================================================

  const handleSearch = useCallback((term: string) => {
    setSearchTerm(term);
    pagination.goToFirst();
  }, [pagination]);

  const handleFilterChange = useCallback((filterType: string, value: any) => {
    switch (filterType) {
      case 'device':
        setDeviceFilter(value);
        break;
      case 'dataType':
        setDataTypeFilter(value);
        break;
      case 'enabled':
        setEnabledFilter(value);
        break;
      case 'quality':
        setQualityFilter(value);
        break;
    }
    pagination.goToFirst();
  }, [pagination]);

  // =============================================================================
  // 🔄 라이프사이클 hooks
  // =============================================================================

  useEffect(() => {
    loadDevices();
    loadDataStatistics();
  }, [loadDevices, loadDataStatistics]);

  useEffect(() => {
    loadDataPoints();
  }, [loadDataPoints]);

  useEffect(() => {
    if (showCurrentValues) {
      loadCurrentValues();
    }
  }, [loadCurrentValues, showCurrentValues]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      if (showCurrentValues) {
        loadCurrentValues();
      }
      loadDataStatistics();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, loadCurrentValues, loadDataStatistics, showCurrentValues]);

  // =============================================================================
  // 🎨 렌더링 헬퍼 함수들
  // =============================================================================

  const getDataTypeIcon = (dataType: string) => {
    switch (dataType) {
      case 'number': return 'fas fa-hashtag';
      case 'boolean': return 'fas fa-toggle-on';
      case 'string': return 'fas fa-font';
      default: return 'fas fa-question';
    }
  };

  const getQualityBadgeClass = (quality: string) => {
    switch (quality) {
      case 'good': return 'quality-badge quality-good';
      case 'bad': return 'quality-badge quality-bad';
      case 'uncertain': return 'quality-badge quality-uncertain';
      default: return 'quality-badge quality-unknown';
    }
  };

  const formatValue = (value: any, dataType: string, unit?: string) => {
    if (value === null || value === undefined) return '-';
    
    let formattedValue = String(value);
    
    if (dataType === 'number') {
      const num = parseFloat(value);
      if (!isNaN(num)) {
        formattedValue = num.toFixed(2);
      }
    }
    
    return unit ? `${formattedValue} ${unit}` : formattedValue;
  };

  const formatTimestamp = (timestamp: string) => {
    const date = new Date(timestamp);
    return date.toLocaleString();
  };

  // 필터링된 현재값들
  const filteredCurrentValues = useMemo(() => {
    return currentValues.filter(cv => {
      if (qualityFilter !== 'all' && cv.quality !== qualityFilter) return false;
      return true;
    });
  }, [currentValues, qualityFilter]);

  // =============================================================================
  // 🎨 UI 렌더링
  // =============================================================================

  return (
    <div className="data-explorer-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">데이터 익스플로러</h1>
          <div className="page-subtitle">
            실시간 데이터포인트와 현재값을 탐색하고 분석합니다
          </div>
        </div>
        <div className="header-right">
          <div className="header-actions">
            <button 
              className="btn btn-secondary"
              onClick={() => setAutoRefresh(!autoRefresh)}
            >
              <i className={`fas fa-${autoRefresh ? 'pause' : 'play'}`}></i>
              {autoRefresh ? '자동새로고침 중지' : '자동새로고침 시작'}
            </button>
            <button 
              className="btn btn-primary"
              onClick={() => handleExportData('json')}
              disabled={selectedDataPoints.length === 0}
            >
              <i className="fas fa-download"></i>
              데이터 내보내기
            </button>
          </div>
        </div>
      </div>

      {/* 통계 대시보드 */}
      {dataStatistics && (
        <div className="stats-grid">
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-database text-primary"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">전체 데이터포인트</div>
              <div className="stat-value">{dataStatistics.data_points.total_data_points}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-check-circle text-success"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">활성화됨</div>
              <div className="stat-value">{dataStatistics.data_points.enabled_data_points}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-clock text-info"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">현재값 (양호)</div>
              <div className="stat-value">{dataStatistics.current_values.good_quality}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-network-wired text-warning"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">활성 디바이스</div>
              <div className="stat-value">{dataStatistics.system_stats.active_devices}</div>
            </div>
          </div>
        </div>
      )}

      {/* 필터 및 검색 */}
      <div className="filters-section">
        <div className="search-box">
          <i className="fas fa-search"></i>
          <input
            type="text"
            placeholder="데이터포인트 이름 또는 설명 검색..."
            value={searchTerm}
            onChange={(e) => handleSearch(e.target.value)}
          />
        </div>
        
        <div className="filter-group">
          <select
            value={deviceFilter}
            onChange={(e) => handleFilterChange('device', e.target.value === 'all' ? 'all' : parseInt(e.target.value))}
          >
            <option value="all">모든 디바이스</option>
            {devices.map(device => (
              <option key={device.id} value={device.id}>{device.name}</option>
            ))}
          </select>

          <select
            value={dataTypeFilter}
            onChange={(e) => handleFilterChange('dataType', e.target.value)}
          >
            <option value="all">모든 데이터 타입</option>
            <option value="number">숫자</option>
            <option value="boolean">불린</option>
            <option value="string">문자열</option>
          </select>

          <select
            value={enabledFilter}
            onChange={(e) => handleFilterChange('enabled', e.target.value === 'all' ? 'all' : e.target.value === 'true')}
          >
            <option value="all">모든 상태</option>
            <option value="true">활성화됨</option>
            <option value="false">비활성화됨</option>
          </select>

          {showCurrentValues && (
            <select
              value={qualityFilter}
              onChange={(e) => handleFilterChange('quality', e.target.value)}
            >
              <option value="all">모든 품질</option>
              <option value="good">양호</option>
              <option value="bad">불량</option>
              <option value="uncertain">불확실</option>
            </select>
          )}

          <button 
            className="btn btn-secondary btn-sm"
            onClick={handleResetFilters}
          >
            <i className="fas fa-undo"></i>
            필터 초기화
          </button>
        </div>

        <div className="view-controls">
          <div className="view-mode-toggle">
            <button 
              className={`btn btn-sm ${viewMode === 'table' ? 'btn-primary' : 'btn-secondary'}`}
              onClick={() => setViewMode('table')}
            >
              <i className="fas fa-table"></i>
              테이블
            </button>
            <button 
              className={`btn btn-sm ${viewMode === 'card' ? 'btn-primary' : 'btn-secondary'}`}
              onClick={() => setViewMode('card')}
            >
              <i className="fas fa-th"></i>
              카드
            </button>
          </div>
          
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={showCurrentValues}
              onChange={(e) => setShowCurrentValues(e.target.checked)}
            />
            현재값 표시
          </label>
        </div>

        {selectedDataPoints.length > 0 && (
          <div className="selection-actions">
            <span className="selected-count">
              {selectedDataPoints.length}개 선택됨
            </span>
            <button 
              onClick={() => handleExportData('json')}
              className="btn btn-sm btn-primary"
            >
              JSON 내보내기
            </button>
            <button 
              onClick={() => handleExportData('csv')}
              className="btn btn-sm btn-primary"
            >
              CSV 내보내기
            </button>
            <button 
              onClick={() => setSelectedDataPoints([])}
              className="btn btn-sm btn-secondary"
            >
              선택 해제
            </button>
          </div>
        )}
      </div>

      {/* 에러 표시 */}
      {error && (
        <div className="error-message">
          <i className="fas fa-exclamation-circle"></i>
          {error}
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 데이터포인트 목록 */}
      <div className="data-content">
        {isLoading ? (
          <div className="loading-spinner">
            <i className="fas fa-spinner fa-spin"></i>
            <span>데이터포인트를 불러오는 중...</span>
          </div>
        ) : dataPoints.length === 0 ? (
          <div className="empty-state">
            <i className="fas fa-database"></i>
            <h3>데이터포인트가 없습니다</h3>
            <p>검색 조건을 변경하거나 필터를 초기화해보세요</p>
            <button className="btn btn-secondary" onClick={handleResetFilters}>
              <i className="fas fa-undo"></i>
              필터 초기화
            </button>
          </div>
        ) : viewMode === 'table' ? (
          <div className="data-table-container">
            <table className="data-table">
              <thead>
                <tr>
                  <th>
                    <input
                      type="checkbox"
                      checked={selectedDataPoints.length === dataPoints.length}
                      onChange={(e) => handleSelectAll(e.target.checked)}
                    />
                  </th>
                  <th>이름</th>
                  <th>디바이스</th>
                  <th>데이터 타입</th>
                  <th>주소</th>
                  <th>상태</th>
                  {showCurrentValues && (
                    <>
                      <th>현재값</th>
                      <th>품질</th>
                      <th>업데이트 시간</th>
                    </>
                  )}
                </tr>
              </thead>
              <tbody>
                {dataPoints.map((dataPoint) => {
                  const currentValue = currentValues.find(cv => cv.point_id === dataPoint.id);
                  const isSelected = selectedDataPoints.some(dp => dp.id === dataPoint.id);
                  
                  return (
                    <tr 
                      key={dataPoint.id}
                      className={isSelected ? 'selected' : ''}
                    >
                      <td>
                        <input
                          type="checkbox"
                          checked={isSelected}
                          onChange={(e) => handleDataPointSelect(dataPoint, e.target.checked)}
                        />
                      </td>
                      <td>
                        <div className="data-point-info">
                          <div className="data-point-name">
                            <i className={getDataTypeIcon(dataPoint.data_type)}></i>
                            {dataPoint.name}
                          </div>
                          {dataPoint.description && (
                            <div className="data-point-description">{dataPoint.description}</div>
                          )}
                        </div>
                      </td>
                      <td>
                        <span className="device-name">{dataPoint.device_info?.name || `Device ${dataPoint.device_id}`}</span>
                      </td>
                      <td>
                        <span className="data-type-badge">{dataPoint.data_type}</span>
                      </td>
                      <td>
                        <span className="address">{dataPoint.address}</span>
                      </td>
                      <td>
                        <span className={`status-badge ${dataPoint.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                          {dataPoint.is_enabled ? '활성' : '비활성'}
                        </span>
                      </td>
                      {showCurrentValues && (
                        <>
                          <td>
                            <div className="current-value">
                              {currentValue ? (
                                formatValue(currentValue.value, currentValue.data_type, currentValue.unit)
                              ) : (
                                <span className="no-value">-</span>
                              )}
                            </div>
                          </td>
                          <td>
                            {currentValue && (
                              <span className={getQualityBadgeClass(currentValue.quality)}>
                                {currentValue.quality}
                              </span>
                            )}
                          </td>
                          <td>
                            {currentValue && (
                              <span className="timestamp">
                                {formatTimestamp(currentValue.timestamp)}
                              </span>
                            )}
                          </td>
                        </>
                      )}
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        ) : (
          <div className="data-cards-container">
            {dataPoints.map((dataPoint) => {
              const currentValue = currentValues.find(cv => cv.point_id === dataPoint.id);
              const isSelected = selectedDataPoints.some(dp => dp.id === dataPoint.id);
              
              return (
                <div 
                  key={dataPoint.id}
                  className={`data-card ${isSelected ? 'selected' : ''}`}
                  onClick={() => handleDataPointSelect(dataPoint, !isSelected)}
                >
                  <div className="card-header">
                    <div className="card-title">
                      <i className={getDataTypeIcon(dataPoint.data_type)}></i>
                      {dataPoint.name}
                    </div>
                    <div className="card-actions">
                      <input
                        type="checkbox"
                        checked={isSelected}
                        onChange={(e) => {
                          e.stopPropagation();
                          handleDataPointSelect(dataPoint, e.target.checked);
                        }}
                      />
                    </div>
                  </div>
                  <div className="card-body">
                    <div className="card-info">
                      <div className="info-item">
                        <span className="label">디바이스:</span>
                        <span className="value">{dataPoint.device_info?.name || `Device ${dataPoint.device_id}`}</span>
                      </div>
                      <div className="info-item">
                        <span className="label">타입:</span>
                        <span className="value">{dataPoint.data_type}</span>
                      </div>
                      <div className="info-item">
                        <span className="label">주소:</span>
                        <span className="value">{dataPoint.address}</span>
                      </div>
                      <div className="info-item">
                        <span className="label">상태:</span>
                        <span className={`status-badge ${dataPoint.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                          {dataPoint.is_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    </div>
                    {showCurrentValues && currentValue && (
                      <div className="current-value-section">
                        <div className="current-value-label">현재값</div>
                        <div className="current-value-display">
                          <span className="value">
                            {formatValue(currentValue.value, currentValue.data_type, currentValue.unit)}
                          </span>
                          <span className={getQualityBadgeClass(currentValue.quality)}>
                            {currentValue.quality}
                          </span>
                        </div>
                        <div className="update-time">
                          {formatTimestamp(currentValue.timestamp)}
                        </div>
                      </div>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* 페이징 */}
      {dataPoints.length > 0 && (
        <div className="pagination-section">
          <Pagination
            current={pagination.currentPage}
            total={pagination.totalCount}
            pageSize={pagination.pageSize}
            pageSizeOptions={[25, 50, 100, 200]}
            showSizeChanger={true}
            showTotal={true}
            onChange={(page, pageSize) => {
              pagination.goToPage(page);
              if (pageSize !== pagination.pageSize) {
                pagination.changePageSize(pageSize);
              }
            }}
            onShowSizeChange={(page, pageSize) => {
              pagination.changePageSize(pageSize);
              pagination.goToPage(1);
            }}
          />
        </div>
      )}

      {/* 상태 정보 */}
      <div className="status-bar">
        <div className="status-info">
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          {isLoadingValues && (
            <span className="processing-indicator">
              <i className="fas fa-spinner fa-spin"></i>
              현재값 업데이트 중...
            </span>
          )}
          {selectedDataPoints.length > 0 && (
            <span className="selection-info">
              {selectedDataPoints.length}개 선택됨
            </span>
          )}
        </div>
      </div>
    </div>
  );
};

export default DataExplorer;