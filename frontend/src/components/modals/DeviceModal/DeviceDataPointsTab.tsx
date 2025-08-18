// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx
// 📊 디바이스 데이터포인트 탭 컴포넌트
// ============================================================================

import React, { useState } from 'react';
import { DataPoint, DataPointApiService } from '../../../api/services/dataPointApi';
import DataPointModal from '../DataPointModal';
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
  const [isDataPointModalOpen, setIsDataPointModalOpen] = useState(false);
  const [dataPointModalMode, setDataPointModalMode] = useState<'create' | 'edit' | 'view'>('create');
  const [selectedDataPoint, setSelectedDataPoint] = useState<DataPoint | null>(null);
  const [isProcessing, setIsProcessing] = useState(false);

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================

  /**
   * 데이터포인트 선택/해제
   */
  const handleDataPointSelect = (pointId: number, selected: boolean) => {
    setSelectedDataPoints(prev => 
      selected 
        ? [...prev, pointId]
        : prev.filter(id => id !== pointId)
    );
  };

  /**
   * 전체 선택/해제
   */
  const handleSelectAll = (selected: boolean) => {
    setSelectedDataPoints(selected ? dataPoints.map(dp => dp.id) : []);
  };

  /**
   * 새 데이터포인트 추가
   */
  const handleCreateDataPoint = () => {
    setSelectedDataPoint(null);
    setDataPointModalMode('create');
    setIsDataPointModalOpen(true);
  };

  /**
   * 데이터포인트 편집
   */
  const handleEditDataPoint = (dataPoint: DataPoint) => {
    setSelectedDataPoint(dataPoint);
    setDataPointModalMode('edit');
    setIsDataPointModalOpen(true);
  };

  /**
   * 데이터포인트 상세 보기
   */
  const handleViewDataPoint = (dataPoint: DataPoint) => {
    setSelectedDataPoint(dataPoint);
    setDataPointModalMode('view');
    setIsDataPointModalOpen(true);
  };

  /**
   * 데이터포인트 모달 저장
   */
  const handleDataPointSave = (savedDataPoint: DataPoint) => {
    if (dataPointModalMode === 'create') {
      onCreate(savedDataPoint);
    } else if (dataPointModalMode === 'edit') {
      onUpdate(savedDataPoint);
    }
    setIsDataPointModalOpen(false);
  };

  /**
   * 데이터포인트 모달 삭제
   */
  const handleDataPointDelete = (pointId: number) => {
    onDelete(pointId);
    setIsDataPointModalOpen(false);
  };

  /**
   * 데이터포인트 읽기 테스트
   */
  const handleTestRead = async (dataPoint: DataPoint) => {
    try {
      setIsProcessing(true);
      const response = await DataPointApiService.testDataPointRead(dataPoint.id);
      
      if (response.success && response.data) {
        const result = response.data;
        const message = result.test_successful 
          ? `읽기 성공: ${result.current_value} (응답시간: ${result.response_time_ms}ms)`
          : `읽기 실패: ${result.error_message}`;
        alert(message);
      } else {
        alert(`테스트 실패: ${response.error}`);
      }
    } catch (error) {
      console.error('데이터포인트 읽기 테스트 실패:', error);
      alert(`테스트 오류: ${error.message}`);
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 일괄 작업 처리
   */
  const handleBulkAction = async (action: 'enable' | 'disable' | 'delete') => {
    if (selectedDataPoints.length === 0) {
      alert('작업할 데이터포인트를 선택해주세요.');
      return;
    }

    const confirmMessage = `선택된 ${selectedDataPoints.length}개 데이터포인트를 ${
      action === 'enable' ? '활성화' : 
      action === 'disable' ? '비활성화' : '삭제'
    }하시겠습니까?`;
    
    if (!window.confirm(confirmMessage)) return;

    try {
      setIsProcessing(true);
      const response = await DataPointApiService.bulkAction(action, selectedDataPoints);
      
      if (response.success && response.data) {
        const result = response.data;
        alert(`작업 완료: 성공 ${result.successful}개, 실패 ${result.failed}개`);
        
        setSelectedDataPoints([]);
        await onRefresh();
      } else {
        throw new Error(response.error || '일괄 작업 실패');
      }
    } catch (error) {
      console.error('일괄 작업 실패:', error);
      alert(`작업 실패: ${error.message}`);
    } finally {
      setIsProcessing(false);
    }
  };

  /**
   * 시간 포맷 함수
   */
  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    return diff < 1 ? '방금 전' : diff < 60 ? `${diff}분 전` : `${Math.floor(diff/60)}시간 전`;
  };

  // ========================================================================
  // 렌더링
  // ========================================================================

  return (
    <div className="tab-panel">
      {/* 헤더 */}
      <div className="datapoints-header">
        <h3>데이터 포인트 목록</h3>
        <div className="datapoints-actions">
          {mode !== 'create' && (
            <button 
              className="btn btn-sm btn-secondary"
              onClick={onRefresh}
              disabled={isLoading}
            >
              <i className={`fas fa-sync ${isLoading ? 'fa-spin' : ''}`}></i>
              새로고침
            </button>
          )}
          <button 
            className="btn btn-sm btn-primary"
            onClick={handleCreateDataPoint}
          >
            <i className="fas fa-plus"></i>
            포인트 추가
          </button>
        </div>
      </div>

      {/* 일괄 작업 패널 */}
      {selectedDataPoints.length > 0 && (
        <div className="bulk-actions-panel">
          <span className="selected-count">
            {selectedDataPoints.length}개 선택됨
          </span>
          <div className="bulk-actions">
            <button 
              className="btn btn-sm btn-success"
              onClick={() => handleBulkAction('enable')}
              disabled={isProcessing}
            >
              일괄 활성화
            </button>
            <button 
              className="btn btn-sm btn-warning"
              onClick={() => handleBulkAction('disable')}
              disabled={isProcessing}
            >
              일괄 비활성화
            </button>
            <button 
              className="btn btn-sm btn-error"
              onClick={() => handleBulkAction('delete')}
              disabled={isProcessing}
            >
              일괄 삭제
            </button>
          </div>
        </div>
      )}

      {/* 데이터포인트 테이블 */}
      <div className="datapoints-table">
        {isLoading ? (
          <div className="loading-message">
            <i className="fas fa-spinner fa-spin"></i>
            <p>데이터포인트를 불러오는 중...</p>
          </div>
        ) : error ? (
          <div className="error-message">
            <i className="fas fa-exclamation-triangle"></i>
            <p>데이터포인트 로드 실패: {error}</p>
            <button className="btn btn-sm btn-primary" onClick={onRefresh}>
              <i className="fas fa-redo"></i>
              다시 시도
            </button>
          </div>
        ) : dataPoints.length > 0 ? (
          <table>
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
                <th>주소</th>
                <th>타입</th>
                {mode !== 'create' && <th>현재값</th>}
                <th>단위</th>
                <th>상태</th>
                {mode !== 'create' && <th>마지막 읽기</th>}
                <th>액션</th>
              </tr>
            </thead>
            <tbody>
              {dataPoints.map((point) => (
                <tr key={point.id}>
                  <td>
                    <input
                      type="checkbox"
                      checked={selectedDataPoints.includes(point.id)}
                      onChange={(e) => handleDataPointSelect(point.id, e.target.checked)}
                    />
                  </td>
                  <td className="point-name">
                    <span 
                      className="clickable-name"
                      onClick={() => handleViewDataPoint(point)}
                    >
                      {point.name}
                    </span>
                    {point.description && (
                      <div className="point-description">{point.description}</div>
                    )}
                  </td>
                  <td className="point-address">{point.address_string || point.address}</td>
                  <td>
                    <span className="data-type-badge">
                      {point.data_type}
                    </span>
                  </td>
                  {mode !== 'create' && (
                    <td className="point-value">
                      {point.current_value !== null && point.current_value !== undefined 
                        ? (typeof point.current_value === 'object' ? point.current_value.value : point.current_value)
                        : 'N/A'}
                    </td>
                  )}
                  <td>{point.unit || '-'}</td>
                  <td>
                    <span className={`status-badge ${point.is_enabled ? 'enabled' : 'disabled'}`}>
                      {point.is_enabled ? '활성' : '비활성'}
                    </span>
                  </td>
                  {mode !== 'create' && (
                    <td>{formatTimeAgo(point.last_update || point.last_read)}</td>
                  )}
                  <td>
                    <div className="point-actions">
                      <button 
                        className="btn btn-xs btn-info" 
                        title="상세"
                        onClick={() => handleViewDataPoint(point)}
                      >
                        <i className="fas fa-eye"></i>
                      </button>
                      <button 
                        className="btn btn-xs btn-secondary" 
                        title="편집"
                        onClick={() => handleEditDataPoint(point)}
                      >
                        <i className="fas fa-edit"></i>
                      </button>
                      {mode !== 'create' && (
                        <button 
                          className="btn btn-xs btn-warning" 
                          title="읽기 테스트"
                          onClick={() => handleTestRead(point)}
                          disabled={isProcessing}
                        >
                          <i className="fas fa-download"></i>
                        </button>
                      )}
                      {mode !== 'view' && (
                        <button 
                          className="btn btn-xs btn-error" 
                          title="삭제"
                          onClick={() => onDelete(point.id)}
                        >
                          <i className="fas fa-trash"></i>
                        </button>
                      )}
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        ) : (
          <div className="empty-message">
            <i className="fas fa-list"></i>
            <p>
              {mode === 'create' 
                ? '새 디바이스에 데이터 포인트를 추가하세요.' 
                : '등록된 데이터 포인트가 없습니다.'
              }
            </p>
            <button className="btn btn-primary" onClick={handleCreateDataPoint}>
              <i className="fas fa-plus"></i>
              {mode === 'create' ? '데이터 포인트 추가' : '첫 번째 포인트 추가'}
            </button>
          </div>
        )}
      </div>

      {/* 데이터포인트 모달 */}
      <DataPointModal
        isOpen={isDataPointModalOpen}
        mode={dataPointModalMode}
        deviceId={deviceId}
        dataPoint={selectedDataPoint}
        onClose={() => setIsDataPointModalOpen(false)}
        onSave={handleDataPointSave}
        onDelete={handleDataPointDelete}
      />

      <style jsx>{`
        .tab-panel {
          flex: 1;
          overflow-y: auto;
          padding: 2rem;
          height: 100%;
        }

        .datapoints-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1.5rem;
        }

        .datapoints-header h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
          color: #1f2937;
        }

        .datapoints-actions {
          display: flex;
          gap: 0.5rem;
        }

        .bulk-actions-panel {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 0.75rem 1rem;
          background: #f3f4f6;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          margin-bottom: 1rem;
        }

        .selected-count {
          font-size: 0.875rem;
          color: #374151;
          font-weight: 500;
        }

        .bulk-actions {
          display: flex;
          gap: 0.5rem;
        }

        .datapoints-table {
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          overflow: hidden;
        }

        .datapoints-table table {
          width: 100%;
          border-collapse: collapse;
        }

        .datapoints-table th {
          background: #f9fafb;
          padding: 0.75rem;
          text-align: left;
          font-size: 0.75rem;
          font-weight: 600;
          color: #374151;
          border-bottom: 1px solid #e5e7eb;
        }

        .datapoints-table td {
          padding: 0.75rem;
          border-bottom: 1px solid #f3f4f6;
          font-size: 0.875rem;
        }

        .datapoints-table tr:last-child td {
          border-bottom: none;
        }

        .datapoints-table tr:hover {
          background: #f9fafb;
        }

        .point-name {
          font-weight: 500;
          color: #1f2937;
        }

        .clickable-name {
          cursor: pointer;
          color: #0ea5e9;
          text-decoration: underline;
        }

        .clickable-name:hover {
          color: #0284c7;
        }

        .point-description {
          font-size: 0.75rem;
          color: #6b7280;
          margin-top: 0.25rem;
        }

        .point-address {
          font-family: 'Courier New', monospace;
          background: #f0f9ff;
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
          color: #0c4a6e;
          font-size: 0.75rem;
        }

        .data-type-badge {
          display: inline-block;
          padding: 0.25rem 0.5rem;
          background: #e0f2fe;
          color: #0369a1;
          border-radius: 0.25rem;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .point-value {
          font-weight: 500;
          color: #059669;
          font-family: 'Courier New', monospace;
        }

        .status-badge {
          display: inline-flex;
          align-items: center;
          gap: 0.25rem;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .status-badge.enabled {
          background: #dcfce7;
          color: #166534;
        }

        .status-badge.disabled {
          background: #fee2e2;
          color: #991b1b;
        }

        .point-actions {
          display: flex;
          gap: 0.25rem;
        }

        .loading-message,
        .error-message {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          min-height: 200px;
          gap: 1rem;
          padding: 2rem;
          text-align: center;
        }

        .loading-message i {
          font-size: 2rem;
          color: #0ea5e9;
        }

        .error-message i {
          font-size: 2rem;
          color: #dc2626;
        }

        .loading-message p,
        .error-message p {
          margin: 0;
          color: #6b7280;
          font-size: 0.875rem;
        }

        .empty-message {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          min-height: 200px;
          color: #6b7280;
          text-align: center;
          gap: 1rem;
        }

        .empty-message i {
          font-size: 3rem;
          color: #cbd5e1;
        }

        .empty-message p {
          margin: 0;
          font-size: 0.875rem;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
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

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-info {
          background: #0891b2;
          color: white;
        }

        .btn-warning {
          background: #f59e0b;
          color: white;
        }

        .btn-success {
          background: #10b981;
          color: white;
        }

        .btn-error {
          background: #dc2626;
          color: white;
        }

        .btn:hover:not(:disabled) {
          opacity: 0.9;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }
      `}</style>
    </div>
  );
};

export default DeviceDataPointsTab;