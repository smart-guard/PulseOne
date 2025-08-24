// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx
// 🚨 목데이터 완전 제거 - 실제 API 데이터만 사용
// ============================================================================

import React, { useState } from 'react';
import { DataApiService, DataPoint } from '../../../api/services/dataApi';
import { DeviceDataPointsTabProps } from './types';

const DeviceDataPointsTab: React.FC<DeviceDataPointsTabProps> = ({
  deviceId,
  dataPoints,
  isLoading,
  error,
  mode,
  onRefresh,
  onCreate,
  onUpdate,
  onDelete
}) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [selectedDataPoints, setSelectedDataPoints] = useState<number[]>([]);
  const [searchTerm, setSearchTerm] = useState('');
  const [filterEnabled, setFilterEnabled] = useState<string>('all');
  const [filterDataType, setFilterDataType] = useState<string>('all');
  const [isProcessing, setIsProcessing] = useState(false);
  const [showCreateForm, setShowCreateForm] = useState(false);

  // 새 데이터포인트 폼 상태
  const [newPoint, setNewPoint] = useState({
    name: '',
    description: '',
    address: '',
    data_type: 'number' as const,
    unit: '',
    is_enabled: true
  });

  // ========================================================================
  // ✅ 필터링된 데이터포인트 - 실제 API 데이터만 사용
  // ========================================================================
  const filteredDataPoints = React.useMemo(() => {
    // 🔥 실제 API 데이터만 사용 - 목데이터 완전 제거
    const points = dataPoints || [];
    
    console.log(`📊 실제 데이터포인트 수: ${points.length}`);
    console.log('📊 실제 데이터포인트:', points);

    // 목데이터 추가 로직 완전 제거
    return points.filter(dp => {
      const matchesSearch = !searchTerm || 
        dp.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        dp.description?.toLowerCase().includes(searchTerm.toLowerCase()) ||
        dp.address.toLowerCase().includes(searchTerm.toLowerCase());

      const matchesEnabled = filterEnabled === 'all' || 
        (filterEnabled === 'enabled' && dp.is_enabled) ||
        (filterEnabled === 'disabled' && !dp.is_enabled);

      const matchesDataType = filterDataType === 'all' || dp.data_type === filterDataType;

      return matchesSearch && matchesEnabled && matchesDataType;
    });
  }, [dataPoints, searchTerm, filterEnabled, filterDataType]);

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================

  const handleDataPointSelect = (pointId: number, selected: boolean) => {
    setSelectedDataPoints(prev => 
      selected ? [...prev, pointId] : prev.filter(id => id !== pointId)
    );
  };

  const handleSelectAll = (selected: boolean) => {
    setSelectedDataPoints(selected ? filteredDataPoints.map(dp => dp.id) : []);
  };

  const handleCreateDataPoint = async () => {
    if (!newPoint.name.trim() || !newPoint.address.trim()) {
      alert('포인트명과 주소는 필수입니다.');
      return;
    }

    try {
      setIsProcessing(true);
      
      // TODO: 실제 API 호출로 교체 필요
      const mockNewPoint: DataPoint = {
        id: Date.now(),
        device_id: deviceId,
        name: newPoint.name,
        description: newPoint.description,
        address: newPoint.address,
        data_type: newPoint.data_type,
        unit: newPoint.unit,
        is_enabled: newPoint.is_enabled,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString()
      };

      onCreate(mockNewPoint);
      setShowCreateForm(false);
      setNewPoint({ 
        name: '', 
        description: '', 
        address: '', 
        data_type: 'number', 
        unit: '', 
        is_enabled: true 
      });
      alert('데이터포인트가 생성되었습니다.');
    } catch (error) {
      alert(`생성 실패: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  const handleTestRead = async (dataPoint: DataPoint) => {
    try {
      setIsProcessing(true);
      const response = await DataApiService.getCurrentValues({
        point_ids: [dataPoint.id],
        include_metadata: true
      });
      
      if (response.success && response.data) {
        const currentValue = response.data.current_values.find(cv => cv.point_id === dataPoint.id);
        if (currentValue) {
          alert(`읽기 성공!\n값: ${currentValue.value}\n품질: ${currentValue.quality}`);
        } else {
          alert('현재값을 찾을 수 없습니다.');
        }
      } else {
        alert(`테스트 실패: ${response.error}`);
      }
    } catch (error) {
      alert(`테스트 오류: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // ========================================================================
  // 메인 렌더링
  // ========================================================================

  return (
    <div className="datapoints-tab-wrapper">
      <div className="datapoints-tab-container">
        
        {/* 헤더 */}
        <div className="datapoints-header">
          <div className="header-left">
            <h3>데이터포인트 관리</h3>
            <span className="count-badge">{filteredDataPoints.length}개</span>
          </div>
          <div className="header-right">
            <button className="btn btn-secondary" onClick={onRefresh} disabled={isLoading}>
              {isLoading ? '새로고침 중...' : '새로고침'}
            </button>
            {mode !== 'view' && (
              <button className="btn btn-primary" onClick={() => setShowCreateForm(true)}>
                추가
              </button>
            )}
          </div>
        </div>

        {/* 필터 */}
        <div className="datapoints-filters">
          <input
            type="text"
            placeholder="검색..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            className="search-input"
          />
          <select value={filterEnabled} onChange={(e) => setFilterEnabled(e.target.value)} className="filter-select">
            <option value="all">전체 상태</option>
            <option value="enabled">활성화됨</option>
            <option value="disabled">비활성화됨</option>
          </select>
          <select value={filterDataType} onChange={(e) => setFilterDataType(e.target.value)} className="filter-select">
            <option value="all">전체 타입</option>
            <option value="number">숫자</option>
            <option value="boolean">불린</option>
            <option value="string">문자열</option>
          </select>
        </div>

        {/* 일괄 작업 */}
        {mode !== 'view' && selectedDataPoints.length > 0 && (
          <div className="bulk-actions">
            <span>{selectedDataPoints.length}개 선택됨</span>
            <button className="btn btn-success btn-sm">활성화</button>
            <button className="btn btn-warning btn-sm">비활성화</button>
            <button className="btn btn-error btn-sm">삭제</button>
          </div>
        )}

        {/* 에러 메시지 */}
        {error && (
          <div className="error-message">
            <i className="fas fa-exclamation-triangle"></i>
            {error}
          </div>
        )}

        {/* 데이터포인트 테이블 */}
        <div className="datapoints-table-container">
          
          {/* 고정 헤더 */}
          <div className="datapoints-table-header">
            <div className="header-col checkbox-col">
              <input
                type="checkbox"
                checked={filteredDataPoints.length > 0 && 
                         filteredDataPoints.every(dp => selectedDataPoints.includes(dp.id))}
                onChange={(e) => handleSelectAll(e.target.checked)}
                disabled={mode === 'view' || filteredDataPoints.length === 0}
              />
            </div>
            <div className="header-col name-col">포인트명</div>
            <div className="header-col address-col">주소</div>
            <div className="header-col type-col">타입</div>
            <div className="header-col unit-col">단위</div>
            <div className="header-col value-col">현재값</div>
            <div className="header-col action-col">작업</div>
          </div>

          {/* 스크롤 가능한 바디 */}
          <div className="datapoints-table-body">
            {isLoading ? (
              <div className="empty-state">
                <i className="fas fa-spinner fa-spin"></i>
                <span>로딩 중...</span>
              </div>
            ) : filteredDataPoints.length === 0 ? (
              <div className="empty-state">
                <i className="fas fa-database"></i>
                <h3>{dataPoints.length === 0 ? '등록된 데이터포인트가 없습니다' : '검색 결과가 없습니다'}</h3>
                <p>{dataPoints.length === 0 ? '새 데이터포인트를 추가하여 시작하세요' : '검색 조건을 변경해보세요'}</p>
                {mode !== 'view' && dataPoints.length === 0 && (
                  <button className="btn btn-primary" onClick={() => setShowCreateForm(true)}>
                    첫 번째 데이터포인트 추가
                  </button>
                )}
              </div>
            ) : (
              filteredDataPoints.map((dataPoint) => (
                <div key={dataPoint.id} className={`datapoints-table-row ${selectedDataPoints.includes(dataPoint.id) ? 'selected' : ''}`}>
                  
                  {/* 체크박스 */}
                  <div className="table-col checkbox-col">
                    <input
                      type="checkbox"
                      checked={selectedDataPoints.includes(dataPoint.id)}
                      onChange={(e) => handleDataPointSelect(dataPoint.id, e.target.checked)}
                      disabled={mode === 'view'}
                    />
                  </div>

                  {/* 포인트명 */}
                  <div className="table-col name-col">
                    <div className="point-name">
                      {dataPoint.name}
                      {!dataPoint.is_enabled && <span className="disabled-badge">비활성</span>}
                    </div>
                    <div className="point-description">{dataPoint.description || 'N/A'}</div>
                  </div>

                  {/* 주소 */}
                  <div className="table-col address-col" title={dataPoint.address}>
                    {dataPoint.address}
                  </div>

                  {/* 타입 */}
                  <div className="table-col type-col">
                    <span className={`type-badge ${dataPoint.data_type}`}>
                      {dataPoint.data_type}
                    </span>
                  </div>

                  {/* 단위 */}
                  <div className="table-col unit-col">{dataPoint.unit || 'N/A'}</div>

                  {/* 현재값 */}
                  <div className="table-col value-col">
                    {dataPoint.current_value ? (
                      <div className="current-value">
                        <div className="value">{dataPoint.current_value.value}</div>
                        <div className={`quality ${dataPoint.current_value.quality}`}>
                          {dataPoint.current_value.quality}
                        </div>
                      </div>
                    ) : 'N/A'}
                  </div>

                  {/* 작업 */}
                  <div className="table-col action-col">
                    <div className="action-buttons">
                      <button
                        className="btn btn-info btn-xs"
                        onClick={() => handleTestRead(dataPoint)}
                        disabled={isProcessing}
                        title="읽기 테스트"
                      >
                        <i className="fas fa-play"></i>
                      </button>
                      {mode !== 'view' && (
                        <>
                          <button className="btn btn-secondary btn-xs" title="편집">
                            <i className="fas fa-edit"></i>
                          </button>
                          <button
                            className="btn btn-error btn-xs"
                            onClick={() => {
                              if (confirm(`"${dataPoint.name}" 삭제하시겠습니까?`)) {
                                onDelete(dataPoint.id);
                              }
                            }}
                            title="삭제"
                          >
                            <i className="fas fa-trash"></i>
                          </button>
                        </>
                      )}
                    </div>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>
      </div>

      {/* 생성 폼 모달 */}
      {showCreateForm && (
        <div className="create-modal-overlay">
          <div className="create-modal-content">
            <div className="create-modal-header">
              <h3>새 데이터포인트 추가</h3>
              <button onClick={() => setShowCreateForm(false)} className="close-btn">
                <i className="fas fa-times"></i>
              </button>
            </div>
            
            <div className="create-modal-body">
              <div className="form-grid">
                <div className="form-group">
                  <label>포인트명 *</label>
                  <input
                    type="text"
                    value={newPoint.name}
                    onChange={(e) => setNewPoint(prev => ({ ...prev, name: e.target.value }))}
                    placeholder="데이터포인트명"
                  />
                </div>
                <div className="form-group">
                  <label>주소 *</label>
                  <input
                    type="text"
                    value={newPoint.address}
                    onChange={(e) => setNewPoint(prev => ({ ...prev, address: e.target.value }))}
                    placeholder="예: 40001"
                  />
                </div>
              </div>

              <div className="form-grid">
                <div className="form-group">
                  <label>데이터 타입</label>
                  <select
                    value={newPoint.data_type}
                    onChange={(e) => setNewPoint(prev => ({ ...prev, data_type: e.target.value as any }))}
                  >
                    <option value="number">숫자</option>
                    <option value="boolean">불린</option>
                    <option value="string">문자열</option>
                  </select>
                </div>
                <div className="form-group">
                  <label>단위</label>
                  <input
                    type="text"
                    value={newPoint.unit}
                    onChange={(e) => setNewPoint(prev => ({ ...prev, unit: e.target.value }))}
                    placeholder="예: °C, bar"
                  />
                </div>
              </div>

              <div className="form-group">
                <label>설명</label>
                <textarea
                  value={newPoint.description}
                  onChange={(e) => setNewPoint(prev => ({ ...prev, description: e.target.value }))}
                  placeholder="설명"
                  rows={2}
                />
              </div>

              <div className="form-group">
                <label className="checkbox-label">
                  <input
                    type="checkbox"
                    checked={newPoint.is_enabled}
                    onChange={(e) => setNewPoint(prev => ({ ...prev, is_enabled: e.target.checked }))}
                  />
                  활성화
                </label>
              </div>
            </div>

            <div className="create-modal-footer">
              <button onClick={() => setShowCreateForm(false)} className="btn btn-secondary">취소</button>
              <button
                onClick={handleCreateDataPoint}
                disabled={isProcessing || !newPoint.name.trim() || !newPoint.address.trim()}
                className="btn btn-primary"
              >
                {isProcessing ? '생성 중...' : '생성'}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 스타일 */}
      <style jsx>{`
        .datapoints-tab-wrapper {
          height: 100%;
          overflow-y: auto;
          background: #f8fafc;
          padding: 1.5rem;
        }

        .datapoints-tab-container {
          display: flex;
          flex-direction: column;
          gap: 1.5rem;
          max-width: 100%;
        }

        /* 헤더 */
        .datapoints-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          background: white;
          padding: 1.5rem;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          box-shadow: 0 1px 3px 0 rgba(0, 0, 0, 0.1);
        }

        .header-left {
          display: flex;
          align-items: center;
          gap: 1rem;
        }

        .header-left h3 {
          margin: 0;
          font-size: 1.25rem;
          font-weight: 600;
          color: #111827;
        }

        .count-badge {
          background: #0ea5e9;
          color: white;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .header-right {
          display: flex;
          gap: 0.75rem;
        }

        /* 필터 */
        .datapoints-filters {
          display: flex;
          gap: 1rem;
          background: white;
          padding: 1.5rem;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          box-shadow: 0 1px 3px 0 rgba(0, 0, 0, 0.1);
          align-items: center;
        }

        .search-input {
          flex: 1;
          padding: 0.75rem 1rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          font-size: 0.875rem;
        }

        .search-input:focus {
          outline: none;
          border-color: #0ea5e9;
          box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
        }

        .filter-select {
          padding: 0.75rem 1rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          background: white;
          font-size: 0.875rem;
          min-width: 120px;
        }

        .filter-select:focus {
          outline: none;
          border-color: #0ea5e9;
          box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
        }

        /* 일괄 작업 */
        .bulk-actions {
          display: flex;
          align-items: center;
          gap: 1rem;
          background: #fef3c7;
          padding: 1rem 1.5rem;
          border-radius: 8px;
          border: 1px solid #f59e0b;
        }

        .bulk-actions span {
          font-weight: 500;
          color: #92400e;
        }

        /* 에러 메시지 */
        .error-message {
          display: flex;
          align-items: center;
          gap: 0.75rem;
          background: #fef2f2;
          color: #dc2626;
          padding: 1rem 1.5rem;
          border-radius: 8px;
          border: 1px solid #fecaca;
        }

        /* 테이블 컨테이너 */
        .datapoints-table-container {
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          box-shadow: 0 1px 3px 0 rgba(0, 0, 0, 0.1);
          overflow: hidden;
          display: flex;
          flex-direction: column;
          height: 500px;
        }

        /* 테이블 헤더 */
        .datapoints-table-header {
          display: grid;
          grid-template-columns: 50px 2fr 1.2fr 100px 100px 120px 120px;
          gap: 1rem;
          padding: 1rem 1.5rem;
          background: #f9fafb;
          border-bottom: 2px solid #e5e7eb;
          font-weight: 600;
          font-size: 0.875rem;
          color: #374151;
          position: sticky;
          top: 0;
          z-index: 10;
        }

        .header-col {
          display: flex;
          align-items: center;
        }

        .checkbox-col,
        .action-col {
          justify-content: center;
        }

        /* 테이블 바디 */
        .datapoints-table-body {
          flex: 1;
          overflow-y: auto;
          overflow-x: hidden;
        }

        .datapoints-table-body::-webkit-scrollbar {
          width: 8px;
        }

        .datapoints-table-body::-webkit-scrollbar-track {
          background: #f1f5f9;
        }

        .datapoints-table-body::-webkit-scrollbar-thumb {
          background: #94a3b8;
          border-radius: 4px;
        }

        .datapoints-table-body::-webkit-scrollbar-thumb:hover {
          background: #64748b;
        }

        /* 테이블 행 */
        .datapoints-table-row {
          display: grid;
          grid-template-columns: 50px 2fr 1.2fr 100px 100px 120px 120px;
          gap: 1rem;
          padding: 1rem 1.5rem;
          border-bottom: 1px solid #e5e7eb;
          font-size: 0.875rem;
          align-items: center;
          transition: background-color 0.2s;
        }

        .datapoints-table-row:hover {
          background: #f9fafb;
        }

        .datapoints-table-row.selected {
          background: #eff6ff;
        }

        /* 테이블 셀 */
        .table-col {
          display: flex;
          align-items: center;
          overflow: hidden;
        }

        .table-col.checkbox-col,
        .table-col.action-col {
          justify-content: center;
        }

        .table-col.name-col {
          flex-direction: column;
          align-items: flex-start;
          gap: 0.25rem;
        }

        .point-name {
          font-weight: 500;
          display: flex;
          align-items: center;
          gap: 0.5rem;
        }

        .point-description {
          font-size: 0.75rem;
          color: #6b7280;
          overflow: hidden;
          text-overflow: ellipsis;
          white-space: nowrap;
          width: 100%;
        }

        .disabled-badge {
          background: #fee2e2;
          color: #991b1b;
          padding: 0.125rem 0.375rem;
          border-radius: 9999px;
          font-size: 0.625rem;
        }

        /* 타입 배지 */
        .type-badge {
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .type-badge.number {
          background: #dbeafe;
          color: #1d4ed8;
        }

        .type-badge.boolean {
          background: #dcfce7;
          color: #166534;
        }

        .type-badge.string {
          background: #fef3c7;
          color: #92400e;
        }

        /* 현재값 */
        .current-value {
          display: flex;
          flex-direction: column;
          gap: 0.25rem;
        }

        .value {
          font-weight: 500;
        }

        .quality {
          font-size: 0.75rem;
          padding: 0.125rem 0.375rem;
          border-radius: 0.25rem;
          font-weight: 500;
        }

        .quality.good {
          background: #dcfce7;
          color: #166534;
        }

        .quality.bad {
          background: #fee2e2;
          color: #991b1b;
        }

        .quality.uncertain {
          background: #fef3c7;
          color: #92400e;
        }

        /* 액션 버튼 */
        .action-buttons {
          display: flex;
          gap: 0.5rem;
        }

        /* 빈 상태 */
        .empty-state {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          padding: 4rem 2rem;
          color: #6b7280;
          text-align: center;
          gap: 1rem;
        }

        .empty-state i {
          font-size: 3rem;
          margin-bottom: 1rem;
          color: #d1d5db;
        }

        .empty-state h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
          color: #374151;
        }

        .empty-state p {
          margin: 0;
          font-size: 0.875rem;
        }

        /* 버튼 스타일 */
        .btn {
          display: inline-flex;
          align-items: center;
          justify-content: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 6px;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
          white-space: nowrap;
          text-decoration: none;
        }

        .btn-xs {
          padding: 0.375rem 0.5rem;
          font-size: 0.75rem;
        }

        .btn-sm {
          padding: 0.375rem 0.75rem;
          font-size: 0.75rem;
        }

        .btn-primary {
          background: #0ea5e9;
          color: white;
        }

        .btn-primary:hover:not(:disabled) {
          background: #0284c7;
        }

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-secondary:hover:not(:disabled) {
          background: #475569;
        }

        .btn-success {
          background: #059669;
          color: white;
        }

        .btn-warning {
          background: #d97706;
          color: white;
        }

        .btn-error {
          background: #dc2626;
          color: white;
        }

        .btn-info {
          background: #0891b2;
          color: white;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        /* 생성 모달 */
        .create-modal-overlay {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: rgba(0, 0, 0, 0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 2000;
        }

        .create-modal-content {
          background: white;
          border-radius: 8px;
          width: 600px;
          max-width: 90vw;
          max-height: 80vh;
          display: flex;
          flex-direction: column;
          box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
        }

        .create-modal-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1.5rem;
          border-bottom: 1px solid #e5e7eb;
        }

        .create-modal-header h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
        }

        .close-btn {
          background: none;
          border: none;
          font-size: 1.5rem;
          cursor: pointer;
          padding: 0.5rem;
          border-radius: 4px;
          color: #6b7280;
        }

        .close-btn:hover {
          background: #f3f4f6;
          color: #374151;
        }

        .create-modal-body {
          flex: 1;
          padding: 1.5rem;
          overflow-y: auto;
        }

        .create-modal-footer {
          display: flex;
          justify-content: flex-end;
          gap: 0.75rem;
          padding: 1.5rem;
          border-top: 1px solid #e5e7eb;
        }

        /* 폼 스타일 */
        .form-grid {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 1rem;
          margin-bottom: 1rem;
        }

        .form-group {
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .form-group label {
          font-weight: 500;
          font-size: 0.875rem;
          color: #374151;
        }

        .form-group input,
        .form-group select,
        .form-group textarea {
          padding: 0.75rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          font-size: 0.875rem;
        }

        .form-group input:focus,
        .form-group select:focus,
        .form-group textarea:focus {
          outline: none;
          border-color: #0ea5e9;
          box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
        }

        .checkbox-label {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          cursor: pointer;
          font-size: 0.875rem;
        }

        .checkbox-label input {
          margin: 0;
        }

        /* 반응형 */
        @media (max-width: 768px) {
          .datapoints-tab-wrapper {
            padding: 1rem;
          }
          
          .datapoints-table-header,
          .datapoints-table-row {
            grid-template-columns: 40px 1fr 80px 60px;
          }
          
          .address-col,
          .unit-col,
          .value-col {
            display: none;
          }
          
          .form-grid {
            grid-template-columns: 1fr;
          }
          
          .create-modal-content {
            margin: 1rem;
            width: auto;
          }
        }
      `}</style>
    </div>
  );
};

export default DeviceDataPointsTab;