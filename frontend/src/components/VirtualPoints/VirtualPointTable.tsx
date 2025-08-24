// ============================================================================
// VirtualPointTable.tsx - 완전 수정된 버전 (테이블 정렬, UI 개선)
// ============================================================================

import React from 'react';

export interface VirtualPointTableProps {
  virtualPoints: any[];
  onEdit: (point: any) => void;
  onDelete: (point: any) => void;
  onTest: (point: any) => void;
  onExecute: (pointId: number) => void;
  onToggleEnabled: (pointId: number) => void;
  sortBy?: string;
  sortOrder?: 'asc' | 'desc';
  onSort?: (sortBy: string, sortOrder: 'asc' | 'desc') => void;
  loading?: boolean;
}

export const VirtualPointTable: React.FC<VirtualPointTableProps> = ({
  virtualPoints,
  onEdit,
  onDelete,
  onTest,
  onExecute,
  onToggleEnabled,
  sortBy = 'name',
  sortOrder = 'asc',
  onSort,
  loading = false
}) => {

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'active':
      case 'success': return '#10b981';
      case 'error': return '#ef4444';
      case 'calculating': return '#f59e0b';
      case 'disabled': return '#6b7280';
      default: return '#6b7280';
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'active':
      case 'success': return 'fas fa-check-circle';
      case 'error': return 'fas fa-times-circle';
      case 'calculating': return 'fas fa-spinner fa-spin';
      case 'disabled': return 'fas fa-pause-circle';
      default: return 'fas fa-question-circle';
    }
  };

  const getCategoryColor = (category: string) => {
    const colors: Record<string, string> = {
      '온도': '#ef4444',
      '압력': '#3b82f6',
      '유량': '#10b981',
      '전력': '#f59e0b',
      '생산': '#8b5cf6',
      '품질': '#ec4899',
      '안전': '#f97316',
      '유지보수': '#6b7280',
      '에너지': '#84cc16',
      'Custom': '#14b8a6',
      'calculation': '#8b5cf6'
    };
    return colors[category] || '#6b7280';
  };

  const formatValue = (value: any, unit?: string) => {
    if (value === null || value === undefined) return 'N/A';
    
    let formattedValue = value;
    if (typeof value === 'number') {
      formattedValue = Number.isInteger(value) ? value.toString() : value.toFixed(2);
    }
    
    return unit ? `${formattedValue} ${unit}` : formattedValue.toString();
  };

  const handleSort = (column: string) => {
    if (onSort) {
      const newOrder = sortBy === column && sortOrder === 'asc' ? 'desc' : 'asc';
      onSort(column, newOrder);
    }
  };

  const SortIcon = ({ column }: { column: string }) => {
    if (sortBy !== column) return null;
    return (
      <i className={`fas fa-sort-${sortOrder === 'asc' ? 'up' : 'down'}`} 
         style={{ marginLeft: '4px', fontSize: '10px' }}></i>
    );
  };

  return (
    <div style={{
      background: 'white',
      borderRadius: '12px',
      border: '1px solid #e5e7eb',
      overflow: 'hidden',
      boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
    }}>
      <div style={{ overflowX: 'auto' }}>
        <table style={{ 
          width: '100%', 
          borderCollapse: 'collapse',
          minWidth: '1000px' // 최소 너비 보장
        }}>
          <thead>
            <tr style={{ background: '#f9fafb' }}>
              {[
                { key: 'name', label: '이름', width: '200px' },
                { key: 'category', label: '카테고리', width: '120px' },
                { key: 'formula', label: '수식', width: '250px' },
                { key: 'current_value', label: '현재값', width: '120px' },
                { key: 'calculation_status', label: '상태', width: '100px' },
                { key: 'scope_type', label: '범위', width: '80px' },
                { key: 'calculation_trigger', label: '실행방식', width: '100px' },
                { key: 'last_calculated', label: '마지막 계산', width: '120px' },
                { key: 'actions', label: '작업', width: '120px', sortable: false }
              ].map((header) => (
                <th 
                  key={header.key}
                  onClick={header.sortable !== false ? () => handleSort(header.key) : undefined}
                  style={{
                    padding: '12px',
                    textAlign: 'left',
                    fontSize: '12px',
                    fontWeight: 600,
                    color: '#374151',
                    borderBottom: '1px solid #e5e7eb',
                    cursor: header.sortable !== false ? 'pointer' : 'default',
                    width: header.width,
                    minWidth: header.width,
                    whiteSpace: 'nowrap'
                  }}
                >
                  {header.label}
                  {header.sortable !== false && <SortIcon column={header.key} />}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {loading ? (
              <tr>
                <td colSpan={9} style={{ 
                  textAlign: 'center', 
                  padding: '40px',
                  color: '#6b7280'
                }}>
                  <i className="fas fa-spinner fa-spin" style={{ marginRight: '8px' }}></i>
                  로딩 중...
                </td>
              </tr>
            ) : virtualPoints.length === 0 ? (
              <tr>
                <td colSpan={9} style={{ 
                  textAlign: 'center', 
                  padding: '40px', 
                  color: '#6b7280' 
                }}>
                  가상포인트가 없습니다
                </td>
              </tr>
            ) : (
              virtualPoints.map((point, index) => (
                <tr key={point.id || index} 
                    style={{ 
                      borderBottom: index === virtualPoints.length - 1 ? 'none' : '1px solid #f3f4f6',
                      transition: 'background-color 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#f9fafb';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = 'white';
                    }}
                >
                  {/* 이름 */}
                  <td style={{ padding: '12px', width: '200px' }}>
                    <div>
                      <div style={{ 
                        fontSize: '13px', 
                        fontWeight: 500, 
                        color: '#111827',
                        marginBottom: '2px',
                        whiteSpace: 'nowrap',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis'
                      }}>
                        {point.name}
                      </div>
                      {point.description && (
                        <div style={{ 
                          fontSize: '11px', 
                          color: '#6b7280',
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis'
                        }}>
                          {point.description}
                        </div>
                      )}
                    </div>
                  </td>

                  {/* 카테고리 */}
                  <td style={{ padding: '12px', width: '120px' }}>
                    <span style={{
                      display: 'inline-flex',
                      alignItems: 'center',
                      padding: '4px 8px',
                      background: getCategoryColor(point.category || 'Custom'),
                      color: 'white',
                      borderRadius: '12px',
                      fontSize: '10px',
                      fontWeight: 500,
                      height: '20px',
                      minWidth: 'fit-content'
                    }}>
                      {point.category || 'Custom'}
                    </span>
                  </td>

                  {/* 수식 */}
                  <td style={{ padding: '12px', width: '250px' }}>
                    <code style={{
                      fontSize: '11px',
                      color: '#374151',
                      background: '#f3f4f6',
                      padding: '2px 4px',
                      borderRadius: '3px',
                      fontFamily: 'JetBrains Mono, Consolas, monospace',
                      display: 'block',
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis'
                    }}>
                      {point.formula || point.expression || 'N/A'}
                    </code>
                  </td>

                  {/* 현재값 */}
                  <td style={{ padding: '12px', width: '120px' }}>
                    <div style={{
                      fontFamily: 'JetBrains Mono, Consolas, monospace',
                      fontSize: '12px',
                      fontWeight: 500,
                      color: point.calculation_status === 'success' || point.calculation_status === 'active' ? '#166534' : '#6b7280',
                      background: point.calculation_status === 'success' || point.calculation_status === 'active' ? '#f0fdf4' : '#f9fafb',
                      padding: '4px 8px',
                      borderRadius: '4px',
                      border: '1px solid #e5e7eb',
                      textAlign: 'center'
                    }}>
                      {formatValue(point.current_value, point.unit)}
                    </div>
                  </td>

                  {/* 상태 */}
                  <td style={{ padding: '12px', width: '100px' }}>
                    <span style={{
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: '4px',
                      padding: '4px 8px',
                      background: getStatusColor(point.calculation_status || 'disabled'),
                      color: 'white',
                      borderRadius: '12px',
                      fontSize: '10px',
                      fontWeight: 500,
                      height: '20px',
                      minWidth: 'fit-content'
                    }}>
                      <i className={getStatusIcon(point.calculation_status || 'disabled').replace('fa-spin', '')} 
                         style={{ fontSize: '9px' }}></i>
                      {point.calculation_status === 'success' || point.calculation_status === 'active' ? '활성' : 
                       point.calculation_status === 'error' ? '오류' :
                       point.calculation_status === 'calculating' ? '계산중' : '비활성'}
                    </span>
                  </td>

                  {/* 범위 */}
                  <td style={{ padding: '12px', width: '80px', fontSize: '12px', color: '#6b7280' }}>
                    {point.scope_type === 'global' || point.scope_type === 'tenant' ? '전역' : 
                     point.scope_type === 'site' ? '사이트' : '디바이스'}
                  </td>

                  {/* 실행방식 */}
                  <td style={{ padding: '12px', width: '100px', fontSize: '12px', color: '#6b7280' }}>
                    {point.calculation_trigger === 'time_based' || point.calculation_trigger === 'timer' ? '시간기반' : 
                     point.calculation_trigger === 'event_driven' || point.calculation_trigger === 'onchange' ? '이벤트' : '수동'}
                  </td>

                  {/* 마지막 계산 */}
                  <td style={{ padding: '12px', width: '120px', fontSize: '12px', color: '#6b7280' }}>
                    {point.last_calculated 
                      ? new Date(point.last_calculated).toLocaleDateString('ko-KR')
                      : 'N/A'}
                  </td>

                  {/* 작업 */}
                  <td style={{ padding: '12px', width: '120px' }}>
                    <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
                      <button
                        onClick={() => onExecute(point.id)}
                        disabled={!point.is_enabled}
                        style={{
                          padding: '4px 6px',
                          background: 'none',
                          border: '1px solid #d1d5db',
                          borderRadius: '4px',
                          color: point.is_enabled ? '#374151' : '#9ca3af',
                          cursor: point.is_enabled ? 'pointer' : 'not-allowed',
                          opacity: point.is_enabled ? 1 : 0.5,
                          fontSize: '11px'
                        }}
                        title="실행"
                      >
                        <i className="fas fa-play"></i>
                      </button>
                      
                      <button
                        onClick={() => onTest(point)}
                        style={{
                          padding: '4px 6px',
                          background: 'none',
                          border: '1px solid #d1d5db',
                          borderRadius: '4px',
                          color: '#374151',
                          cursor: 'pointer',
                          fontSize: '11px'
                        }}
                        title="테스트"
                      >
                        <i className="fas fa-vial"></i>
                      </button>
                      
                      <button
                        onClick={() => onEdit(point)}
                        style={{
                          padding: '4px 6px',
                          background: 'none',
                          border: '1px solid #d1d5db',
                          borderRadius: '4px',
                          color: '#374151',
                          cursor: 'pointer',
                          fontSize: '11px'
                        }}
                        title="편집"
                      >
                        <i className="fas fa-edit"></i>
                      </button>
                      
                      <button
                        onClick={() => onDelete(point)}
                        style={{
                          padding: '4px 6px',
                          background: 'none',
                          border: '1px solid #d1d5db',
                          borderRadius: '4px',
                          color: '#dc2626',
                          cursor: 'pointer',
                          fontSize: '11px'
                        }}
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
    </div>
  );
};