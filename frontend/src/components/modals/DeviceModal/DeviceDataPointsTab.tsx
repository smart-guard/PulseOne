// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx
// 📊 디바이스 데이터포인트 탭 컴포넌트 - 스크롤 문제 완전 해결
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
  // 필터링된 데이터포인트 + 테스트 데이터
  // ========================================================================
  const filteredDataPoints = React.useMemo(() => {
    let points = [...dataPoints];
    
    // 🔥 테스트용 더미 데이터 추가 (스크롤 확인용)
    if (points.length < 20) {
      const dummyPoints = Array.from({ length: 25 }, (_, i) => ({
        id: 1000 + i,
        device_id: deviceId,
        name: `테스트포인트_${String(i + 1).padStart(2, '0')}`,
        description: `테스트용 데이터포인트 ${i + 1}`,
        address: `4000${i + 1}`,
        data_type: ['number', 'boolean', 'string'][i % 3] as any,
        unit: ['°C', 'bar', 'L/min', 'kW', '%'][i % 5],
        is_enabled: i % 3 !== 0,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        current_value: i % 4 === 0 ? {
          value: (Math.random() * 100).toFixed(1),
          quality: ['good', 'bad', 'uncertain'][i % 3] as any,
          timestamp: new Date().toISOString()
        } : undefined
      }));
      points = [...points, ...dummyPoints];
    }

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
  }, [dataPoints, deviceId, searchTerm, filterEnabled, filterDataType]);

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
    <div className="datapoints-tab-wrapper">
      <div className="datapoints-tab-container">
        
        {/* 헤더 */}
        <div className="datapoints-header">
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
          <div className="error-message">⚠️ {error}</div>
        )}

        {/* 🔥 통계 정보 추가 (페이지 높이 증가) */}
        <div className="datapoints-stats">
          <div className="stats-grid">
            <div className="stat-item">
              <div className="stat-label">총 포인트</div>
              <div className="stat-value">{dataPoints.length}</div>
            </div>
            <div className="stat-item">
              <div className="stat-label">활성화됨</div>
              <div className="stat-value">{dataPoints.filter(dp => dp.is_enabled).length}</div>
            </div>
            <div className="stat-item">
              <div className="stat-label">비활성화됨</div>
              <div className="stat-value">{dataPoints.filter(dp => !dp.is_enabled).length}</div>
            </div>
            <div className="stat-item">
              <div className="stat-label">연결됨</div>
              <div className="stat-value">{dataPoints.filter(dp => dp.current_value).length}</div>
            </div>
          </div>
        </div>

        {/* 🔥 데이터포인트 테이블 */}
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

          {/* 🔥 스크롤 가능한 바디 */}
          <div className="datapoints-table-body">
            {isLoading ? (
              <div className="empty-state">🔄 로딩 중...</div>
            ) : filteredDataPoints.length === 0 ? (
              <div className="empty-state">
                {dataPoints.length === 0 ? '데이터포인트가 없습니다.' : '검색 결과가 없습니다.'}
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

        {/* 🔥 추가 정보 섹션 (페이지 높이 더 증가) */}
        <div className="additional-info">
          <div className="info-sections">
            <div className="info-section">
              <h4>📈 데이터 타입별 분포</h4>
              <div className="type-distribution">
                <div className="type-item">
                  <span className="type-label">숫자형</span>
                  <span className="type-count">{dataPoints.filter(dp => dp.data_type === 'number').length}개</span>
                </div>
                <div className="type-item">
                  <span className="type-label">불린형</span>
                  <span className="type-count">{dataPoints.filter(dp => dp.data_type === 'boolean').length}개</span>
                </div>
                <div className="type-item">
                  <span className="type-label">문자형</span>
                  <span className="type-count">{dataPoints.filter(dp => dp.data_type === 'string').length}개</span>
                </div>
              </div>
            </div>

            <div className="info-section">
              <h4>🔧 빠른 설정</h4>
              <div className="quick-actions">
                <button className="btn btn-outline">모든 포인트 활성화</button>
                <button className="btn btn-outline">모든 포인트 비활성화</button>
                <button className="btn btn-outline">연결 테스트</button>
                <button className="btn btn-outline">설정 내보내기</button>
              </div>
            </div>

            <div className="info-section">
              <h4>📋 최근 활동</h4>
              <div className="recent-activities">
                <div className="activity-item">
                  <span className="activity-time">2분 전</span>
                  <span className="activity-desc">온도센서_01 포인트 추가됨</span>
                </div>
                <div className="activity-item">
                  <span className="activity-time">5분 전</span>
                  <span className="activity-desc">압력센서_02 설정 변경됨</span>
                </div>
                <div className="activity-item">
                  <span className="activity-time">10분 전</span>
                  <span className="activity-desc">유량계_03 연결 테스트 완료</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* 생성 폼 모달 */}
      {showCreateForm && (
        <div className="create-modal-overlay">
          <div className="create-modal-content">
            <div className="create-modal-header">
              <h3>새 데이터포인트 추가</h3>
              <button onClick={() => setShowCreateForm(false)} className="close-btn">✕</button>
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

      {/* 🔥 스타일 - 더 높은 우선순위로 설정 */}
      <style jsx>{`
        /* 🔥 최상위 래퍼 - 다른 탭과 동일한 구조 */
        .datapoints-tab-wrapper {
          height: 100%;
          width: 100%;
          overflow-y: auto; /* 🔥 전체 스크롤 활성화 */
          background: #f8fafc;
          padding: 1rem;
        }

        .datapoints-tab-container {
          display: flex;
          flex-direction: column;
          gap: 1rem;
          min-height: calc(100vh - 200px); /* 🔥 충분한 높이 확보 */
        }

        /* 🔥 헤더 섹션 */
        .datapoints-header {
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

        /* 🔥 필터 섹션 */
        .datapoints-filters {
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

        /* 🔥 일괄 작업 */
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

        /* 🔥 통계 정보 스타일 */
        .datapoints-stats {
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          padding: 1.5rem;
          flex-shrink: 0;
        }

        .stats-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
          gap: 1.5rem;
        }

        .stat-item {
          text-align: center;
        }

        .stat-label {
          font-size: 0.75rem;
          color: #6b7280;
          margin-bottom: 0.5rem;
          text-transform: uppercase;
          letter-spacing: 0.05em;
        }

        .stat-value {
          font-size: 1.5rem;
          font-weight: 600;
          color: #0ea5e9;
        }

        /* 🔥 추가 정보 섹션 스타일 */
        .additional-info {
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          padding: 1.5rem;
          flex-shrink: 0;
        }

        .info-sections {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
          gap: 2rem;
        }

        .info-section h4 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .type-distribution {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
        }

        .type-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 0.5rem;
          background: #f8fafc;
          border-radius: 6px;
        }

        .type-label {
          font-size: 0.875rem;
          color: #374151;
        }

        .type-count {
          font-size: 0.875rem;
          font-weight: 600;
          color: #0ea5e9;
        }

        .quick-actions {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
        }

        .btn-outline {
          background: transparent;
          color: #0ea5e9;
          border: 1px solid #0ea5e9;
        }

        .btn-outline:hover {
          background: #0ea5e9;
          color: white;
        }

        .recent-activities {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
        }

        .activity-item {
          display: flex;
          gap: 1rem;
          padding: 0.75rem;
          background: #f8fafc;
          border-radius: 6px;
          border-left: 3px solid #0ea5e9;
        }

        .activity-time {
          font-size: 0.75rem;
          color: #6b7280;
          min-width: 60px;
          flex-shrink: 0;
        }

        .activity-desc {
          font-size: 0.875rem;
          color: #374151;
        }

        /* 🔥 에러 메시지 */
        .error-message {
          background: #fef2f2;
          color: #dc2626;
          padding: 1rem;
          border-radius: 8px;
          border: 1px solid #fecaca;
          flex-shrink: 0;
        }

        /* 🔥 테이블 컨테이너 - 강제 스크롤 보장 */
        .datapoints-table-container {
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          overflow: hidden;
          display: flex;
          flex-direction: column;
          height: 500px; /* 🔥 고정 높이로 강제 스크롤 */
        }

        /* 🔥 고정 헤더 */
        .datapoints-table-header {
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

        /* 🔥 스크롤 가능한 바디 - 항상 스크롤 보이게 */
        .datapoints-table-body {
          flex: 1;
          overflow-y: scroll !important; /* 🔥 scroll로 변경 (항상 보임) */
          overflow-x: hidden !important;
          scrollbar-width: thin;
          scrollbar-color: #94a3b8 #f1f5f9;
          border: 1px solid #f1f5f9; /* 🔥 스크롤 영역 시각적 구분 */
        }

        /* 🔥 강제 스크롤바 표시 - 더 강력하게 */
        .datapoints-table-body::-webkit-scrollbar {
          width: 14px !important; /* 🔥 더 넓게 */
          background: #f1f5f9 !important;
          border-left: 1px solid #e5e7eb !important;
        }

        .datapoints-table-body::-webkit-scrollbar-track {
          background: #f8fafc !important;
          border-radius: 0 !important;
        }

        .datapoints-table-body::-webkit-scrollbar-thumb {
          background: #94a3b8 !important;
          border-radius: 7px !important;
          border: 2px solid #f8fafc !important;
          min-height: 40px !important; /* 🔥 최소 높이 보장 */
        }

        .datapoints-table-body::-webkit-scrollbar-thumb:hover {
          background: #64748b !important;
        }

        .datapoints-table-body::-webkit-scrollbar-thumb:active {
          background: #475569 !important;
        }

        /* 🔥 Firefox 스크롤바 */
        .datapoints-table-body {
          scrollbar-width: auto !important;
          scrollbar-color: #94a3b8 #f8fafc !important;
        }

        /* 🔥 테이블 행 */
        .datapoints-table-row {
          display: grid;
          grid-template-columns: 50px 2fr 1fr 80px 80px 120px 120px;
          gap: 1rem;
          padding: 1rem;
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

        .datapoints-table-row.selected:hover {
          background: #dbeafe;
        }

        /* 🔥 테이블 셀 */
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
          padding: 0.125rem 0.5rem;
          border-radius: 9999px;
          font-size: 0.625rem;
          flex-shrink: 0;
        }

        /* 🔥 타입 배지 */
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

        /* 🔥 현재값 */
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

        /* 🔥 액션 버튼 */
        .action-buttons {
          display: flex;
          gap: 0.5rem;
        }

        /* 🔥 빈 상태 */
        .empty-state {
          display: flex;
          align-items: center;
          justify-content: center;
          padding: 4rem 2rem;
          color: #6b7280;
          text-align: center;
          font-size: 1rem;
        }

        /* 🔥 버튼 스타일 */
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
          white-space: nowrap;
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

        /* 🔥 생성 모달 스타일 - 고유한 클래스명으로 충돌 방지 */
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
          z-index: 2000; /* 🔥 더 높은 z-index */
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
        }

        .close-btn:hover {
          background: #f3f4f6;
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

        /* 🔥 폼 스타일 */
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
          padding: 0.5rem;
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

        /* 🔥 반응형 */
        @media (max-width: 768px) {
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