// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointTable.tsx
// 가상포인트 테이블 컴포넌트
// ============================================================================

import React from 'react';
import { VirtualPoint } from '../../types/virtualPoints';
import { virtualPointsApi } from '../../api/services/virtualPointsApi';

export interface VirtualPointTableProps {
  virtualPoints: VirtualPoint[];
  onEdit: (point: VirtualPoint) => void;
  onDelete: (point: VirtualPoint) => void;
  onTest: (point: VirtualPoint) => void;
  onExecute: (pointId: number) => void;
  isExecuting?: boolean;
  loading?: boolean;
}

export const VirtualPointTable: React.FC<VirtualPointTableProps> = ({
  virtualPoints,
  onEdit,
  onDelete,
  onTest,
  onExecute,
  isExecuting = false,
  loading = false
}) => {
  const formatValue = (value: any, unit?: string) => {
    if (value === null || value === undefined) return 'N/A';
    
    let formattedValue = value;
    if (typeof value === 'number') {
      formattedValue = Number.isInteger(value) ? value.toString() : value.toFixed(2);
    }
    
    return unit ? `${formattedValue} ${unit}` : formattedValue.toString();
  };

  return (
    <div className="virtual-points-table-container">
      <table className="virtual-points-table">
        <thead>
          <tr>
            <th style={{ fontSize: '13px' }}>이름</th>
            <th style={{ fontSize: '13px' }}>카테고리</th>
            <th style={{ fontSize: '13px' }}>수식</th>
            <th style={{ fontSize: '13px' }}>현재값</th>
            <th style={{ fontSize: '13px' }}>상태</th>
            <th style={{ fontSize: '13px' }}>범위</th>
            <th style={{ fontSize: '13px' }}>실행방식</th>
            <th style={{ fontSize: '13px' }}>마지막 계산</th>
            <th style={{ fontSize: '13px' }}>작업</th>
          </tr>
        </thead>
        <tbody>
          {loading ? (
            <tr>
              <td colSpan={9} style={{ textAlign: 'center', padding: '40px' }}>
                <i className="fas fa-spinner fa-spin"></i>
                로딩 중...
              </td>
            </tr>
          ) : virtualPoints.length === 0 ? (
            <tr>
              <td colSpan={9} style={{ textAlign: 'center', padding: '40px', color: '#6b7280' }}>
                가상포인트가 없습니다
              </td>
            </tr>
          ) : (
            virtualPoints.map(point => (
              <tr key={point.id} className="virtual-point-row">
                <td>
                  <div className="point-name">
                    <i className={virtualPointsApi.getDataTypeIcon(point.data_type)}></i>
                    <div>
                      <div className="name">{point.name}</div>
                      {point.description && (
                        <div className="description">{point.description}</div>
                      )}
                    </div>
                  </div>
                </td>
                <td>
                  <span 
                    className="category-badge"
                    style={{ 
                      backgroundColor: virtualPointsApi.getCategoryColor(point.category),
                      color: 'white',
                      padding: '4px 8px',
                      borderRadius: '4px',
                      fontSize: '11px'
                    }}
                  >
                    {point.category}
                  </span>
                </td>
                <td>
                  <code className="formula-code">
                    {point.expression.length > 50 
                      ? point.expression.substring(0, 50) + '...'
                      : point.expression
                    }
                  </code>
                </td>
                <td>
                  <div className={`current-value ${point.calculation_status}`}>
                    {formatValue(point.current_value, point.unit)}
                  </div>
                </td>
                <td>
                  <span 
                    className="status-badge"
                    style={{ 
                      backgroundColor: virtualPointsApi.getStatusColor(point.calculation_status),
                      color: 'white',
                      padding: '4px 8px',
                      borderRadius: '4px',
                      fontSize: '11px'
                    }}
                  >
                    <i className={virtualPointsApi.getStatusIcon(point.calculation_status)}></i>
                    {point.calculation_status === 'active' ? '활성' : 
                     point.calculation_status === 'error' ? '오류' :
                     point.calculation_status === 'disabled' ? '비활성' : '계산중'}
                  </span>
                </td>
                <td>
                  <span className="scope-type">
                    {point.scope_type === 'global' ? '전역' :
                     point.scope_type === 'site' ? '사이트' : '디바이스'}
                  </span>
                </td>
                <td>
                  <span className="calc-trigger">
                    {point.calculation_trigger === 'time_based' ? '시간기반' :
                     point.calculation_trigger === 'event_driven' ? '이벤트' : '수동'}
                  </span>
                </td>
                <td>
                  <span className="last-calc">
                    {point.last_calculated 
                      ? new Date(point.last_calculated).toLocaleString('ko-KR')
                      : 'N/A'}
                  </span>
                </td>
                <td>
                  <div className="table-actions">
                    <button
                      className="action-btn execute-btn"
                      onClick={() => onExecute(point.id)}
                      disabled={isExecuting || !point.is_enabled}
                      title="실행"
                    >
                      <i className={`fas ${isExecuting ? 'fa-spinner fa-spin' : 'fa-play'}`}></i>
                    </button>
                    <button
                      className="action-btn test-btn"
                      onClick={() => onTest(point)}
                      title="테스트"
                    >
                      <i className="fas fa-vial"></i>
                    </button>
                    <button
                      className="action-btn edit-btn"
                      onClick={() => onEdit(point)}
                      title="편집"
                    >
                      <i className="fas fa-edit"></i>
                    </button>
                    <button
                      className="action-btn delete-btn"
                      onClick={() => onDelete(point)}
                      title="삭제"
                    >
                      <i className="fas fa-trash"></i>
                    </button>
                  </div>
                </td>
              </tr>
            ))
          )}
        </tbody>
      </table>
    </div>
  );
};