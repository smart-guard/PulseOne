// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx
// 📊 디바이스 데이터포인트 탭 컴포넌트 - 강제 스크롤 구현
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
  // 필터링된 데이터포인트
  // ========================================================================
  const filteredDataPoints = dataPoints.filter(dp => {
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
      setNewPoint({ name: '', description: '', address: '', data_type: 'number', unit: '', is_enabled: true });
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
    <div className="tab-panel">
      <div className="datapoints-container">
        
        {/* 헤더 */}
        <div className="header-section">
          <div className="header-left">
            <h3>📊 데이터포인트 관리</h3>
            <span className="count-badge">{filteredDataPoints.length}개</span>
          </div>
          <div className="header-right">
            <button className="btn btn-secondary" onClick={onRefresh} disabled={isLoading}>
              {isLoading ? '🔄' : '새로고침'}
            </button>
            {mode !== 'view' && (
              <button className="btn btn-primary" onClick={() => setShowCreateForm(true)}>
                ➕ 추가
              </button>
            )}
          </div>
        </div>

        {/* 필터 */}
        <div className="filter-section">
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
          <div className="error-message">⚠️ {error}</div>
        )}

        {/* 🔥 데이터포인트 테이블 */}
        <div className="table-section">
          
          {/* 고정 헤더 */}
          <div className="table-header">
            <div className="header-cell checkbox-cell">
              <input
                type="checkbox"
                checked={filteredDataPoints.length > 0 && 
                         filteredDataPoints.every(dp => selectedDataPoints.includes(dp.id))}
                onChange={(e) => handleSelectAll(e.target.checked)}
                disabled={mode === 'view' || filteredDataPoints.length === 0}
              />
            </div>
            <div className="header-cell name-cell">포인트명</div>
            <div className="header-cell address-cell">주소</div>
            <div className="header-cell type-cell">타입</div>
            <div className="header-cell unit-cell">단위</div>
            <div className="header-cell value-cell">현재값</div>
            <div className="header-cell action-cell">작업</div>
          </div>

          {/* 🔥 스크롤 가능한 바디 */}
          <div className="table-body">
            {isLoading ? (
              <div className="empty-state">🔄 로딩 중...</div>
            ) : filteredDataPoints.length === 0 ? (
              <div className="empty-state">
                {dataPoints.length === 0 ? '데이터포인트가 없습니다.' : '검색 결과가 없습니다.'}
              </div>
            ) : (
              filteredDataPoints.map((dataPoint) => (
                <div key={dataPoint.id} className={`table-row ${selectedDataPoints.includes(dataPoint.id) ? 'selected' : ''}`}>
                  
                  {/* 체크박스 */}
                  <div className="table-cell checkbox-cell">
                    <input
                      type="checkbox"
                      checked={selectedDataPoints.includes(dataPoint.id)}
                      onChange={(e) => handleDataPointSelect(dataPoint.id, e.target.checked)}
                      disabled={mode === 'view'}
                    />
                  </div>

                  {/* 포인트명 */}
                  <div className="table-cell name-cell">
                    <div className="point-name">
                      {dataPoint.name}
                      {!dataPoint.is_enabled && <span className="disabled-badge">비활성</span>}
                    </div>
                    <div className="point-description">{dataPoint.description || 'N/A'}</div>
                  </div>

                  {/* 주소 */}
                  <div className="table-cell address-cell" title={dataPoint.address}>
                    {dataPoint.address}
                  </div>

                  {/* 타입 */}
                  <div className="table-cell type-cell">
                    <span className={`type-badge ${dataPoint.data_type}`}>
                      {dataPoint.data_type}
                    </span>
                  </div>

                  {/* 단위 */}
                  <div className="table-cell unit-cell">{dataPoint.unit || 'N/A'}</div>

                  {/* 현재값 */}
                  <div className="table-cell value-cell">
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
                  <div className="table-cell action-cell">
                    <div className="action-buttons">
                      <button
                        className="btn btn-info btn-xs"
                        onClick={() => handleTestRead(dataPoint)}
                        disabled={isProcessing}
                        title="읽기 테스트"
                      >
                        ▶️
                      </button>
                      {mode !== 'view' && (
                        <>
                          <button className="btn btn-secondary btn-xs" title="편집">✏️</button>
                          <button
                            className="btn btn-error btn-xs"
                            onClick={() => {
                              if (confirm(`"${dataPoint.name}" 삭제하시겠습니까?`)) {
                                onDelete(dataPoint.id);
                              }
                            }}
                            title="삭제"
                          >
                            🗑️
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
        <div className="modal-overlay">
          <div className="modal-content">
            <div className="modal-header">
              <h3>새 데이터포인트 추가</h3>
              <button onClick={() => setShowCreateForm(false)} className="close-btn">✕</button>
            </div>
            
            <div className="modal-body">
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

            <div className="modal-footer">
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
        .tab-panel {
          height: 100%;
          padding: 1rem;
          overflow-y: auto;
          background: #f8fafc;
        }

        .datapoints-container {
          height: 100%;
          display: flex;
          flex-direction: column;
          gap: 1rem;
        }

        .header-section {
          display: flex;
          justify-content: space-between;
          align-items: center;
          background: white;
          padding: 1rem;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          flex-shrink: 0;
        }

        .header-left {
          display: flex;
          align-items: center;
          gap: 1rem;
        }

        .header-left h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
        }

        .count-badge {
          background: #0ea5e9;
          color: white;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
        }

        .header-right {
          display: flex;
          gap: 0.75rem;
        }

        .filter-section {
          display: flex;
          gap: 1rem;
          background: white;
          padding: 1rem;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          flex-shrink: 0;
        }

        .search-input {
          flex: 1;
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
        }

        .filter-select {
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          background: white;
        }

        .bulk-actions {
          display: flex;
          align-items: center;
          gap: 1rem;
          background: #fef3c7;
          padding: 0.75rem 1rem;
          border-radius: 8px;
          border: 1px solid #f59e0b;
          flex-shrink: 0;
        }

        .error-message {
          background: #fef2f2;
          color: #dc2626;
          padding: 1rem;
          border-radius: 8px;
          border: 1px solid #fecaca;
          flex-shrink: 0;
        }

        .table-section {
          flex: 1;
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          overflow: hidden;
          display: flex;
          flex-direction: column;
          min-height: 400px;
        }

        .table-header {
          display: grid;
          grid-template-columns: 50px 2fr 1fr 80px 80px 120px 120px;
          gap: 1rem;
          padding: 1rem;
          background: #f9fafb;
          border-bottom: 2px solid #e5e7eb;
          font-weight: 600;
          font-size: 0.875rem;
          color: #374151;
          flex-shrink: 0;
        }

        .header-cell {
          display: flex;
          align-items: center;
        }

        .header-cell.checkbox-cell,
        .header-cell.action-cell {
          justify-content: center;
        }

        .table-body {
          flex: 1;
          overflow-y: scroll !important;
          overflow-x: hidden;
          max-height: 500px;
          min-height: 300px;
        }

        .table-row {
          display: grid;
          grid-template-columns: 50px 2fr 1fr 80px 80px 120px 120px;
          gap: 1rem;
          padding: 1rem;
          border-bottom: 1px solid #e5e7eb;
          font-size: 0.875rem;
          align-items: center;
          transition: background-color 0.2s;
        }

        .table-row:hover {
          background: #f9fafb;
        }

        .table-row.selected {
          background: #eff6ff;
        }

        .table-row.selected:hover {
          background: #dbeafe;
        }

        .table-cell {
          display: flex;
          align-items: center;
          overflow: hidden;
        }

        .table-cell.checkbox-cell,
        .table-cell.action-cell {
          justify-content: center;
        }

        .table-cell.name-cell {
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
          padding: 0.125rem 0.5rem;
          border-radius: 9999px;
          font-size: 0.625rem;
          flex-shrink: 0;
        }

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

        .current-value {
          display: flex;
          flex-direction: column;
          gap: 0.25rem;
        }

        .value {
          font-weight: 500;
          overflow: hidden;
          text-overflow: ellipsis;
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

        .action-buttons {
          display: flex;
          gap: 0.5rem;
        }

        .empty-state {
          display: flex;
          align-items: center;
          justify-content: center;
          padding: 2rem;
          color: #6b7280;
          text-align: center;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 6px;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-xs {
          padding: 0.25rem 0.5rem;
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

        /* 강제 스크롤바 표시 */
        .table-body::-webkit-scrollbar {
          width: 12px;
          display: block !important;
        }

        .table-body::-webkit-scrollbar-track {
          background: #f1f5f9;
        }

        .table-body::-webkit-scrollbar-thumb {
          background: #94a3b8;
          border-radius: 6px;
        }

        .table-body::-webkit-scrollbar-thumb:hover {
          background: #64748b;
        }

        /* 모달 스타일 */
        .modal-overlay {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: rgba(0, 0, 0, 0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 1001;
        }

        .modal-content {
          background: white;
          border-radius: 8px;
          width: 600px;
          max-width: 90vw;
          max-height: 80vh;
          display: flex;
          flex-direction: column;
          box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
        }

        .modal-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1.5rem;
          border-bottom: 1px solid #e5e7eb;
        }

        .modal-header h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
        }

        .close-btn {
          background: none;
          border: none;
          font-size: 1.5rem;
          cursor: pointer;
        }

        .modal-body {
          flex: 1;
          padding: 1.5rem;
          overflow-y: auto;
        }

        .modal-footer {
          display: flex;
          justify-content: flex-end;
          gap: 0.75rem;
          padding: 1.5rem;
          border-top: 1px solid #e5e7eb;
        }

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
        }

        .form-group input,
        .form-group select,
        .form-group textarea {
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
        }

        .checkbox-label {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          cursor: pointer;
        }

        .checkbox-label input {
          margin: 0;
        }
      `}</style>
    </div>
  );
};

export default DeviceDataPointsTab;