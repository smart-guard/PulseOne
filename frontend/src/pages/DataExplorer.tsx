// ============================================================================
// frontend/src/pages/DataExplorer.tsx
// 📋 데이터 익스플로러 - 관리자용 데이터포인트 설정 관리 도구
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import { DataApiService, DataPoint } from '../api/services/dataApi';
import { DeviceApiService } from '../api/services/deviceApi';
import '../styles/base.css';
import '../styles/data-explorer.css';
import '../styles/pagination.css';

interface DataPointWithStats extends DataPoint {
  last_value?: any;
  last_update?: string;
  update_count?: number;
  error_count?: number;
  data_size_bytes?: number;
}

const DataExplorer: React.FC = () => {
  // =============================================================================
  // 🔧 State 관리 (실시간 기능 모두 제거)
  // =============================================================================
  
  // 데이터 상태
  const [dataPoints, setDataPoints] = useState<DataPointWithStats[]>([]);
  const [selectedDataPoints, setSelectedDataPoints] = useState<DataPointWithStats[]>([]);
  const [devices, setDevices] = useState<any[]>([]);
  const [dataStatistics, setDataStatistics] = useState<any>(null);
  
  // 로딩 및 에러 상태
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 검색 및 필터 상태
  const [searchTerm, setSearchTerm] = useState('');
  const [deviceFilter, setDeviceFilter] = useState<number | 'all'>('all');
  const [dataTypeFilter, setDataTypeFilter] = useState<string>('all');
  const [enabledFilter, setEnabledFilter] = useState<boolean | 'all'>('all');
  
  // UI 상태
  const [viewMode, setViewMode] = useState<'table' | 'card'>('table');
  const [sortBy, setSortBy] = useState<'name' | 'device' | 'type' | 'address'>('name');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('asc');
  
  // 관리 기능 상태
  const [showConfigPanel, setShowConfigPanel] = useState(false);
  const [editingDataPoint, setEditingDataPoint] = useState<DataPointWithStats | null>(null);
  const [bulkAction, setBulkAction] = useState<'enable' | 'disable' | 'delete' | ''>('');

  // 페이징
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 25,
    totalCount: 0
  });

  // =============================================================================
  // 🔄 데이터 로드 함수들 (관리 기능에 특화)
  // =============================================================================

  /**
   * 📊 데이터포인트 메타데이터 로드 (실시간 값 제외)
   */
  const loadDataPoints = useCallback(async () => {
    try {
      setIsLoading(true);
      setError(null);

      console.log('📊 데이터포인트 메타데이터 로드 시작...');

      const response = await DataApiService.searchDataPoints({
        page: pagination.currentPage,
        limit: pagination.pageSize,
        search: searchTerm || undefined,
        device_id: deviceFilter !== 'all' ? Number(deviceFilter) : undefined,
        data_type: dataTypeFilter !== 'all' ? dataTypeFilter : undefined,
        enabled_only: enabledFilter !== 'all' ? Boolean(enabledFilter) : undefined,
        sort_by: sortBy,
        sort_order: sortOrder.toUpperCase() as 'ASC' | 'DESC',
        include_current_value: false, // 실시간 값 제외
        include_metadata: true        // 메타데이터 포함
      });

      if (response.success && response.data) {
        setDataPoints(response.data.items || []);
        pagination.updateTotalCount(response.data.pagination?.total || 0);
        
        console.log(`✅ 데이터포인트 ${response.data.items?.length || 0}개 로드 완료`);
      } else {
        throw new Error(response.error || '데이터포인트 로드 실패');
      }

    } catch (err) {
      console.error('❌ 데이터포인트 로드 실패:', err);
      setError(err instanceof Error ? err.message : '데이터 로드 중 오류가 발생했습니다');
      setDataPoints([]);
    } finally {
      setIsLoading(false);
    }
  }, [
    pagination.currentPage, 
    pagination.pageSize, 
    searchTerm, 
    deviceFilter, 
    dataTypeFilter, 
    enabledFilter,
    sortBy,
    sortOrder
  ]);

  /**
   * 📈 데이터 통계 로드 (관리용)
   */
  const loadDataStatistics = useCallback(async () => {
    try {
      console.log('📈 데이터 통계 로드 시작...');

      const response = await DataApiService.getDataStatistics({
        device_id: deviceFilter !== 'all' ? Number(deviceFilter) : undefined,
        include_performance: true
      });

      if (response.success && response.data) {
        setDataStatistics(response.data);
        console.log('✅ 데이터 통계 로드 완료');
      }
    } catch (err) {
      console.warn('⚠️ 데이터 통계 로드 실패:', err);
    }
  }, [deviceFilter]);

  /**
   * 📱 디바이스 목록 로드
   */
  const loadDevices = useCallback(async () => {
    try {
      const response = await DeviceApiService.getDevices({
        page: 1,
        limit: 1000
      });

      if (response.success && response.data) {
        setDevices(response.data.items || []);
      }
    } catch (err) {
      console.warn('⚠️ 디바이스 목록 로드 실패:', err);
    }
  }, []);

  // =============================================================================
  // 🎛️ 관리 액션 함수들
  // =============================================================================

  /**
   * 데이터포인트 선택/해제
   */
  const handleDataPointSelect = useCallback((dataPoint: DataPointWithStats, selected: boolean) => {
    setSelectedDataPoints(prev => 
      selected 
        ? [...prev, dataPoint]
        : prev.filter(dp => dp.id !== dataPoint.id)
    );
  }, []);

  /**
   * 전체 선택/해제
   */
  const handleSelectAll = useCallback((selected: boolean) => {
    setSelectedDataPoints(selected ? [...dataPoints] : []);
  }, [dataPoints]);

  /**
   * 📤 설정 데이터 내보내기
   */
  const handleExportConfig = useCallback(async (format: 'json' | 'csv' | 'xml') => {
    if (selectedDataPoints.length === 0) {
      alert('내보낼 데이터포인트를 선택해주세요.');
      return;
    }

    try {
      console.log(`📤 설정 데이터 내보내기 시작 (${format}):`, selectedDataPoints.length);

      const exportData = selectedDataPoints.map(dp => ({
        id: dp.id,
        name: dp.name,
        device_id: dp.device_id,
        device_name: dp.device_name,
        address: dp.address,
        data_type: dp.data_type,
        unit: dp.unit,
        is_enabled: dp.is_enabled,
        description: dp.description,
        min_value: dp.min_value,
        max_value: dp.max_value,
        scaling_factor: dp.scaling_factor,
        polling_interval: dp.polling_interval,
        created_at: dp.created_at,
        updated_at: dp.updated_at
      }));

      let content: string;
      let filename: string;
      let mimeType: string;

      switch (format) {
        case 'json':
          content = JSON.stringify(exportData, null, 2);
          filename = `datapoints_config_${Date.now()}.json`;
          mimeType = 'application/json';
          break;
        case 'csv':
          const headers = Object.keys(exportData[0]).join(',');
          const rows = exportData.map(row => Object.values(row).join(','));
          content = [headers, ...rows].join('\n');
          filename = `datapoints_config_${Date.now()}.csv`;
          mimeType = 'text/csv';
          break;
        case 'xml':
          content = `<?xml version="1.0" encoding="UTF-8"?>
<datapoints>
${exportData.map(dp => `  <datapoint>
    ${Object.entries(dp).map(([key, value]) => `<${key}>${value}</${key}>`).join('\n    ')}
  </datapoint>`).join('\n')}
</datapoints>`;
          filename = `datapoints_config_${Date.now()}.xml`;
          mimeType = 'application/xml';
          break;
      }

      const blob = new Blob([content], { type: mimeType });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      
      console.log(`✅ 설정 데이터 내보내기 완료: ${exportData.length}개 항목`);

    } catch (err) {
      console.error('❌ 설정 데이터 내보내기 실패:', err);
      setError(err instanceof Error ? err.message : '설정 데이터 내보내기 실패');
    }
  }, [selectedDataPoints]);

  /**
   * 🔧 일괄 작업 실행
   */
  const handleBulkAction = useCallback(async () => {
    if (selectedDataPoints.length === 0 || !bulkAction) {
      return;
    }

    const confirmMessage = {
      enable: `선택된 ${selectedDataPoints.length}개 데이터포인트를 활성화하시겠습니까?`,
      disable: `선택된 ${selectedDataPoints.length}개 데이터포인트를 비활성화하시겠습니까?`,
      delete: `선택된 ${selectedDataPoints.length}개 데이터포인트를 삭제하시겠습니까? (복구할 수 없습니다)`
    };

    if (!confirm(confirmMessage[bulkAction])) {
      return;
    }

    try {
      console.log(`🔧 일괄 작업 실행: ${bulkAction}`, selectedDataPoints.length);

      const pointIds = selectedDataPoints.map(dp => dp.id);
      
      // TODO: 실제 API 호출로 교체
      // const response = await DataApiService.bulkUpdateDataPoints({
      //   point_ids: pointIds,
      //   action: bulkAction,
      //   data: bulkAction === 'enable' ? { is_enabled: true } : 
      //         bulkAction === 'disable' ? { is_enabled: false } : undefined
      // });

      // 시뮬레이션
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      if (bulkAction === 'delete') {
        setDataPoints(prev => prev.filter(dp => !pointIds.includes(dp.id)));
        setSelectedDataPoints([]);
      } else {
        setDataPoints(prev => prev.map(dp => 
          pointIds.includes(dp.id) 
            ? { ...dp, is_enabled: bulkAction === 'enable' }
            : dp
        ));
      }

      setBulkAction('');
      console.log(`✅ 일괄 작업 완료: ${bulkAction}`);

    } catch (err) {
      console.error('❌ 일괄 작업 실패:', err);
      setError(err instanceof Error ? err.message : '일괄 작업 실패');
    }
  }, [selectedDataPoints, bulkAction]);

  /**
   * 필터 초기화
   */
  const handleResetFilters = useCallback(() => {
    setSearchTerm('');
    setDeviceFilter('all');
    setDataTypeFilter('all');
    setEnabledFilter('all');
    setSortBy('name');
    setSortOrder('asc');
    pagination.goToFirst();
  }, [pagination]);

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
    }
    pagination.goToFirst();
  }, [pagination]);

  const handleSort = useCallback((field: string) => {
    if (sortBy === field) {
      setSortOrder(prev => prev === 'asc' ? 'desc' : 'asc');
    } else {
      setSortBy(field as any);
      setSortOrder('asc');
    }
  }, [sortBy]);

  // =============================================================================
  // 🔄 라이프사이클 Hooks
  // =============================================================================

  useEffect(() => {
    const init = async () => {
      await Promise.all([
        loadDevices(),
        loadDataStatistics()
      ]);
    };
    init();
  }, [loadDevices, loadDataStatistics]);

  useEffect(() => {
    loadDataPoints();
  }, [loadDataPoints]);

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

  const getDataTypeColor = (dataType: string) => {
    switch (dataType) {
      case 'number': return 'text-blue-600';
      case 'boolean': return 'text-green-600';
      case 'string': return 'text-purple-600';
      default: return 'text-gray-600';
    }
  };

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  const formatTimestamp = (timestamp: string) => {
    return new Date(timestamp).toLocaleString();
  };

  // =============================================================================
  // 🎨 메인 렌더링
  // =============================================================================

  return (
    <div className="data-explorer-container">
      {/* 📋 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-cogs"></i>
            데이터포인트 관리
          </h1>
          <div className="page-subtitle">
            데이터포인트 설정을 탐색하고 관리합니다
          </div>
        </div>

        <div className="header-right">
          <div className="header-actions">
            <button 
              className="btn btn-outline"
              onClick={() => setShowConfigPanel(!showConfigPanel)}
            >
              <i className="fas fa-cog"></i>
              설정 패널
            </button>
            <button 
              className="btn btn-outline"
              onClick={loadDataPoints}
              disabled={isLoading}
            >
              <i className={`fas fa-sync-alt ${isLoading ? 'fa-spin' : ''}`}></i>
              새로고침
            </button>
            <button 
              className="btn btn-primary"
              onClick={() => handleExportConfig('json')}
              disabled={selectedDataPoints.length === 0}
            >
              <i className="fas fa-download"></i>
              설정 내보내기
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
              onClick={() => setError(null)}
            >
              닫기
            </button>
          </div>
        </div>
      )}

      {/* 📊 통계 대시보드 */}
      {dataStatistics && (
        <div className="stats-grid">
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-database text-primary"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">전체 데이터포인트</div>
              <div className="stat-value">{dataStatistics.data_points?.total_data_points || 0}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-check-circle text-success"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">활성화됨</div>
              <div className="stat-value">{dataStatistics.data_points?.enabled_data_points || 0}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-times-circle text-error"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">비활성화됨</div>
              <div className="stat-value">{dataStatistics.data_points?.disabled_data_points || 0}</div>
            </div>
          </div>
          <div className="stat-card">
            <div className="stat-icon">
              <i className="fas fa-network-wired text-info"></i>
            </div>
            <div className="stat-content">
              <div className="stat-label">연결된 디바이스</div>
              <div className="stat-value">{devices.length}</div>
            </div>
          </div>
        </div>
      )}

      {/* 🔍 필터 및 관리 섹션 */}
      <div className="filters-section">
        <div className="search-box">
          <i className="fas fa-search"></i>
          <input
            type="text"
            placeholder="데이터포인트 이름, 주소, 설명 검색..."
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

          <button 
            className="btn btn-outline btn-sm"
            onClick={handleResetFilters}
          >
            <i className="fas fa-undo"></i>
            필터 초기화
          </button>
        </div>

        <div className="view-controls">
          <div className="view-mode-toggle">
            <button 
              className={`btn btn-sm ${viewMode === 'table' ? 'btn-primary' : 'btn-outline'}`}
              onClick={() => setViewMode('table')}
            >
              <i className="fas fa-table"></i>
              테이블
            </button>
            <button 
              className={`btn btn-sm ${viewMode === 'card' ? 'btn-primary' : 'btn-outline'}`}
              onClick={() => setViewMode('card')}
            >
              <i className="fas fa-th"></i>
              카드
            </button>
          </div>
          
          <div className="sort-controls">
            <select
              value={sortBy}
              onChange={(e) => setSortBy(e.target.value as any)}
            >
              <option value="name">이름순</option>
              <option value="device">디바이스순</option>
              <option value="type">타입순</option>
              <option value="address">주소순</option>
            </select>
            <button 
              className="btn btn-sm btn-outline"
              onClick={() => setSortOrder(prev => prev === 'asc' ? 'desc' : 'asc')}
            >
              <i className={`fas fa-sort-${sortOrder === 'asc' ? 'up' : 'down'}`}></i>
            </button>
          </div>
        </div>

        {/* 🔧 일괄 작업 패널 */}
        {selectedDataPoints.length > 0 && (
          <div className="bulk-actions">
            <span className="selected-count">
              {selectedDataPoints.length}개 선택됨
            </span>
            <div className="bulk-action-controls">
              <select
                value={bulkAction}
                onChange={(e) => setBulkAction(e.target.value as any)}
              >
                <option value="">일괄 작업 선택</option>
                <option value="enable">활성화</option>
                <option value="disable">비활성화</option>
                <option value="delete">삭제</option>
              </select>
              <button 
                className="btn btn-sm btn-primary"
                onClick={handleBulkAction}
                disabled={!bulkAction}
              >
                <i className="fas fa-play"></i>
                실행
              </button>
            </div>
            <div className="export-actions">
              <button 
                onClick={() => handleExportConfig('json')}
                className="btn btn-sm btn-outline"
              >
                JSON
              </button>
              <button 
                onClick={() => handleExportConfig('csv')}
                className="btn btn-sm btn-outline"
              >
                CSV
              </button>
              <button 
                onClick={() => handleExportConfig('xml')}
                className="btn btn-sm btn-outline"
              >
                XML
              </button>
            </div>
            <button 
              onClick={() => setSelectedDataPoints([])}
              className="btn btn-sm btn-outline"
            >
              <i className="fas fa-times"></i>
              선택 해제
            </button>
          </div>
        )}
      </div>

      {/* 📋 데이터 컨텐츠 */}
      <div className="data-content">
        {isLoading ? (
          <div className="loading-container">
            <div className="loading-spinner"></div>
            <div className="loading-text">데이터포인트를 불러오는 중...</div>
          </div>
        ) : dataPoints.length === 0 ? (
          <div className="empty-state">
            <div className="empty-state-icon">
              <i className="fas fa-database"></i>
            </div>
            <h3 className="empty-state-title">데이터포인트가 없습니다</h3>
            <p className="empty-state-description">
              검색 조건을 변경하거나 필터를 초기화해보세요
            </p>
            <button className="btn btn-outline" onClick={handleResetFilters}>
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
                      checked={selectedDataPoints.length === dataPoints.length && dataPoints.length > 0}
                      onChange={(e) => handleSelectAll(e.target.checked)}
                    />
                  </th>
                  <th 
                    className="sortable"
                    onClick={() => handleSort('name')}
                  >
                    이름 
                    {sortBy === 'name' && <i className={`fas fa-sort-${sortOrder}`}></i>}
                  </th>
                  <th 
                    className="sortable"
                    onClick={() => handleSort('device')}
                  >
                    디바이스
                    {sortBy === 'device' && <i className={`fas fa-sort-${sortOrder}`}></i>}
                  </th>
                  <th 
                    className="sortable"
                    onClick={() => handleSort('type')}
                  >
                    타입
                    {sortBy === 'type' && <i className={`fas fa-sort-${sortOrder}`}></i>}
                  </th>
                  <th 
                    className="sortable"
                    onClick={() => handleSort('address')}
                  >
                    주소
                    {sortBy === 'address' && <i className={`fas fa-sort-${sortOrder}`}></i>}
                  </th>
                  <th>단위</th>
                  <th>범위</th>
                  <th>상태</th>
                  <th>생성일</th>
                  <th>액션</th>
                </tr>
              </thead>
              <tbody>
                {dataPoints.map((dataPoint) => {
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
                        <span className="device-name">
                          {dataPoint.device_info?.name || dataPoint.device_name || `Device ${dataPoint.device_id}`}
                        </span>
                      </td>
                      <td>
                        <span className={`data-type-badge ${getDataTypeColor(dataPoint.data_type)}`}>
                          {dataPoint.data_type}
                        </span>
                      </td>
                      <td>
                        <span className="address monospace">{dataPoint.address}</span>
                      </td>
                      <td>
                        <span className="unit">{dataPoint.unit || '-'}</span>
                      </td>
                      <td>
                        <div className="value-range">
                          {dataPoint.min_value !== null && dataPoint.max_value !== null ? (
                            <span className="range">{dataPoint.min_value} ~ {dataPoint.max_value}</span>
                          ) : '-'}
                        </div>
                      </td>
                      <td>
                        <span className={`status-badge ${dataPoint.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                          {dataPoint.is_enabled ? '활성' : '비활성'}
                        </span>
                      </td>
                      <td>
                        <span className="timestamp monospace">
                          {formatTimestamp(dataPoint.created_at)}
                        </span>
                      </td>
                      <td>
                        <div className="action-buttons">
                          <button 
                            className="btn btn-sm btn-outline"
                            onClick={() => setEditingDataPoint(dataPoint)}
                          >
                            <i className="fas fa-edit"></i>
                          </button>
                          <button 
                            className="btn btn-sm btn-outline"
                            onClick={() => console.log('Copy config:', dataPoint)}
                          >
                            <i className="fas fa-copy"></i>
                          </button>
                        </div>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        ) : (
          <div className="data-cards-container">
            {dataPoints.map((dataPoint) => {
              const isSelected = selectedDataPoints.some(dp => dp.id === dataPoint.id);
              
              return (
                <div 
                  key={dataPoint.id}
                  className={`data-card ${isSelected ? 'selected' : ''}`}
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
                        onChange={(e) => handleDataPointSelect(dataPoint, e.target.checked)}
                      />
                    </div>
                  </div>
                  <div className="card-body">
                    <div className="card-info">
                      <div className="info-item">
                        <span className="label">디바이스:</span>
                        <span className="value">
                          {dataPoint.device_info?.name || dataPoint.device_name || `Device ${dataPoint.device_id}`}
                        </span>
                      </div>
                      <div className="info-item">
                        <span className="label">타입:</span>
                        <span className={`value ${getDataTypeColor(dataPoint.data_type)}`}>{dataPoint.data_type}</span>
                      </div>
                      <div className="info-item">
                        <span className="label">주소:</span>
                        <span className="value monospace">{dataPoint.address}</span>
                      </div>
                      <div className="info-item">
                        <span className="label">단위:</span>
                        <span className="value">{dataPoint.unit || '-'}</span>
                      </div>
                      <div className="info-item">
                        <span className="label">범위:</span>
                        <span className="value">
                          {dataPoint.min_value !== null && dataPoint.max_value !== null 
                            ? `${dataPoint.min_value} ~ ${dataPoint.max_value}` 
                            : '-'
                          }
                        </span>
                      </div>
                      <div className="info-item">
                        <span className="label">상태:</span>
                        <span className={`status-badge ${dataPoint.is_enabled ? 'status-enabled' : 'status-disabled'}`}>
                          {dataPoint.is_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    </div>
                    <div className="card-footer">
                      <div className="card-meta">
                        <span className="created-date">
                          생성: {formatTimestamp(dataPoint.created_at)}
                        </span>
                      </div>
                      <div className="card-actions">
                        <button 
                          className="btn btn-sm btn-outline"
                          onClick={() => setEditingDataPoint(dataPoint)}
                        >
                          <i className="fas fa-edit"></i>
                          편집
                        </button>
                      </div>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* 📄 페이징 */}
      {dataPoints.length > 0 && (
        <div className="pagination-section">
          <Pagination
            current={pagination.currentPage}
            total={pagination.totalCount}
            pageSize={pagination.pageSize}
            pageSizeOptions={[10, 25, 50, 100]}
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

      {/* 📊 상태 바 */}
      <div className="status-bar">
        <div className="status-info">
          <span>총 {pagination.totalCount}개 데이터포인트</span>
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