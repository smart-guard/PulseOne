// ============================================================================
// VirtualPointTable.tsx - 완전 반응형 버전 (구조 수정)
// ============================================================================

import React, { useEffect } from 'react';

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

  // 스크롤바 스타일을 동적으로 추가
  useEffect(() => {
    const styleId = 'virtual-point-table-scrollbar';
    
    // 기존 스타일 제거
    const existingStyle = document.getElementById(styleId);
    if (existingStyle) {
      existingStyle.remove();
    }
    
    // 새 스타일 추가
    const style = document.createElement('style');
    style.id = styleId;
    style.textContent = `
      .virtual-point-table-scroll::-webkit-scrollbar {
        height: 8px;
        background-color: #f1f5f9;
      }
      .virtual-point-table-scroll::-webkit-scrollbar-track {
        background: #f1f5f9;
        border-radius: 4px;
      }
      .virtual-point-table-scroll::-webkit-scrollbar-thumb {
        background: #cbd5e1;
        border-radius: 4px;
        border: 1px solid #f1f5f9;
      }
      .virtual-point-table-scroll::-webkit-scrollbar-thumb:hover {
        background: #94a3b8;
      }
    `;
    document.head.appendChild(style);
    
    // 클린업
    return () => {
      const styleElement = document.getElementById(styleId);
      if (styleElement) {
        styleElement.remove();
      }
    };
  }, []);

  // 디버깅용: 헤더 개수 확인
  const headerCount = 9; // 이름, 카테고리, 수식, 현재값, 상태, 범위, 실행방식, 마지막계산, 작업
  
  const gridColumns = `
    clamp(150px, 20%, 250px)     
    clamp(60px, 8%, 90px)        
    clamp(200px, 25%, 350px)     
    clamp(100px, 12%, 150px)     
    clamp(80px, 10%, 120px)      
    clamp(70px, 8%, 100px)       
    clamp(90px, 10%, 130px)      
    clamp(100px, 12%, 140px)     
    clamp(120px, 15%, 160px)     
  `;

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
      boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)',
      width: '100%'
    }}>
      <div 
        style={{ 
          overflowX: 'auto',
          scrollbarWidth: 'thin',
          scrollbarColor: '#cbd5e1 #f1f5f9'
        }} 
        className="virtual-point-table-scroll"
      >
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          minWidth: '800px',
          width: '100%'
        }}>
          
          {/* 헤더 - 정확한 9개 컬럼 */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: gridColumns,
            gap: 'clamp(4px, 0.5vw, 8px)',
            background: '#f9fafb',
            borderBottom: '2px solid #e5e7eb',
            position: 'sticky',
            top: 0,
            zIndex: 10,
            padding: '0 clamp(4px, 0.5vw, 8px)',
            // 🔥 Grid 컨테이너 전체가 회색 배경
            width: '100%'
          }}>
            {[
              { key: 'name', label: '이름' },
              { key: 'category', label: '카테고리' },
              { key: 'formula', label: '수식' },
              { key: 'current_value', label: '현재값' },
              { key: 'calculation_status', label: '상태' },
              { key: 'scope_type', label: '범위' },
              { key: 'calculation_trigger', label: '실행방식' },
              { key: 'last_calculated', label: '마지막 계산' },
              { key: 'actions', label: '작업', sortable: false }
            ].map((header, index) => (
              <div 
                key={header.key}
                onClick={header.sortable !== false ? () => handleSort(header.key) : undefined}
                style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  fontSize: 'clamp(11px, 1.2vw, 13px)',
                  fontWeight: 600,
                  color: '#374151',
                  cursor: header.sortable !== false ? 'pointer' : 'default',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: index < 2 ? 'flex-start' : 'center',
                  textAlign: index < 2 ? 'left' : 'center',
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  background: '#f9fafb', // 🔥 각 헤더 셀에도 배경색 명시
                  border: 'none' // 🔥 보더 제거
                }}
              >
                {header.label}
                {header.sortable !== false && <SortIcon column={header.key} />}
              </div>
            ))}
          </div>

          {/* 바디 - 헤더와 동일한 Grid 구조 */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: gridColumns, // 🔥 헤더와 동일한 컬럼 정의
            gap: 'clamp(4px, 0.5vw, 8px)',
            padding: '0 clamp(4px, 0.5vw, 8px)'
          }}>
            {loading ? (
              <div style={{
                gridColumn: '1 / -1',
                textAlign: 'center',
                padding: 'clamp(20px, 4vw, 40px)',
                color: '#6b7280',
                fontSize: 'clamp(12px, 1.4vw, 14px)'
              }}>
                <i className="fas fa-spinner fa-spin" style={{ marginRight: '8px' }}></i>
                로딩 중...
              </div>
            ) : virtualPoints.length === 0 ? (
              <div style={{
                gridColumn: '1 / -1',
                textAlign: 'center',
                padding: 'clamp(20px, 4vw, 40px)',
                color: '#6b7280',
                fontSize: 'clamp(12px, 1.4vw, 14px)'
              }}>
                가상포인트가 없습니다
              </div>
            ) : (
              virtualPoints.map((point, index) => (
                <React.Fragment key={point.id || index}>
                  {/* 이름 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '2px',
                    justifyContent: 'center'
                  }}>
                    <div style={{
                      fontSize: 'clamp(12px, 1.3vw, 14px)',
                      fontWeight: 500,
                      color: '#111827',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap'
                    }}>
                      {point.name}
                    </div>
                    {point.description && (
                      <div style={{
                        fontSize: 'clamp(10px, 1.1vw, 12px)',
                        color: '#6b7280',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                        whiteSpace: 'nowrap'
                      }}>
                        {point.description}
                      </div>
                    )}
                  </div>

                  {/* 카테고리 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
                  }}>
                    <span style={{
                      display: 'inline-flex',
                      alignItems: 'center',
                      padding: 'clamp(2px, 0.3vw, 4px) clamp(4px, 0.6vw, 8px)',
                      background: getCategoryColor(point.category || 'Custom'),
                      color: 'white',
                      borderRadius: 'clamp(8px, 1vw, 12px)',
                      fontSize: 'clamp(8px, 0.9vw, 10px)',
                      fontWeight: 500,
                      maxWidth: '100%',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap'
                    }}>
                      {point.category || 'Custom'}
                    </span>
                  </div>

                  {/* 수식 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    display: 'flex',
                    alignItems: 'center'
                  }}>
                    <code style={{
                      fontSize: 'clamp(10px, 1.1vw, 12px)',
                      color: '#374151',
                      background: '#f3f4f6',
                      padding: 'clamp(2px, 0.3vw, 4px) clamp(3px, 0.5vw, 6px)',
                      borderRadius: '3px',
                      fontFamily: 'JetBrains Mono, Consolas, monospace',
                      display: 'block',
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      width: '100%'
                    }}>
                      {point.formula || point.expression || 'N/A'}
                    </code>
                  </div>

                  {/* 현재값 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
                  }}>
                    <div style={{
                      fontFamily: 'JetBrains Mono, Consolas, monospace',
                      fontSize: 'clamp(11px, 1.2vw, 13px)',
                      fontWeight: 500,
                      color: point.calculation_status === 'success' || point.calculation_status === 'active' ? '#166534' : '#6b7280',
                      background: point.calculation_status === 'success' || point.calculation_status === 'active' ? '#f0fdf4' : '#f9fafb',
                      padding: 'clamp(3px, 0.4vw, 6px) clamp(6px, 0.8vw, 10px)',
                      borderRadius: '4px',
                      border: '1px solid #e5e7eb',
                      textAlign: 'center',
                      minWidth: '60px'
                    }}>
                      {formatValue(point.current_value, point.unit)}
                    </div>
                  </div>

                  {/* 상태 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
                  }}>
                    <span style={{
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: 'clamp(2px, 0.3vw, 4px)',
                      padding: 'clamp(3px, 0.4vw, 5px) clamp(6px, 0.8vw, 10px)',
                      background: getStatusColor(point.calculation_status || 'disabled'),
                      color: 'white',
                      borderRadius: 'clamp(8px, 1vw, 12px)',
                      fontSize: 'clamp(8px, 0.9vw, 10px)',
                      fontWeight: 500,
                      minWidth: 'fit-content'
                    }}>
                      <i className={getStatusIcon(point.calculation_status || 'disabled').replace('fa-spin', '')}
                         style={{ fontSize: 'clamp(7px, 0.8vw, 9px)' }}></i>
                      {point.calculation_status === 'success' || point.calculation_status === 'active' ? '활성' :
                       point.calculation_status === 'error' ? '오류' :
                       point.calculation_status === 'calculating' ? '계산중' : '비활성'}
                    </span>
                  </div>

                  {/* 범위 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    fontSize: 'clamp(10px, 1.1vw, 12px)',
                    color: '#6b7280',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center'
                  }}>
                    {point.scope_type === 'global' || point.scope_type === 'tenant' ? '전역' :
                     point.scope_type === 'site' ? '사이트' : '디바이스'}
                  </div>

                  {/* 실행방식 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    fontSize: 'clamp(10px, 1.1vw, 12px)',
                    color: '#6b7280',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center'
                  }}>
                    {point.calculation_trigger === 'time_based' || point.calculation_trigger === 'timer' ? '시간기반' :
                     point.calculation_trigger === 'event_driven' || point.calculation_trigger === 'onchange' ? '이벤트' : '수동'}
                  </div>

                  {/* 마지막 계산 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    fontSize: 'clamp(10px, 1.1vw, 12px)',
                    color: '#6b7280',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center'
                  }}>
                    {point.last_calculated
                      ? new Date(point.last_calculated).toLocaleDateString('ko-KR')
                      : 'N/A'}
                  </div>

                  {/* 작업 */}
                  <div style={{
                    padding: 'clamp(8px, 1vw, 12px)',
                    borderBottom: '1px solid #f3f4f6',
                    display: 'flex',
                    gap: 'clamp(2px, 0.4vw, 4px)',
                    alignItems: 'center',
                    justifyContent: 'center'
                  }}>
                    <button
                      onClick={() => onExecute(point.id)}
                      disabled={!point.is_enabled}
                      style={{
                        padding: 'clamp(3px, 0.4vw, 5px) clamp(4px, 0.6vw, 8px)',
                        background: 'none',
                        border: '1px solid #d1d5db',
                        borderRadius: '4px',
                        color: point.is_enabled ? '#374151' : '#9ca3af',
                        cursor: point.is_enabled ? 'pointer' : 'not-allowed',
                        opacity: point.is_enabled ? 1 : 0.5,
                        fontSize: 'clamp(9px, 1vw, 11px)'
                      }}
                      title="실행"
                    >
                      <i className="fas fa-play"></i>
                    </button>

                    <button
                      onClick={() => onTest(point)}
                      style={{
                        padding: 'clamp(3px, 0.4vw, 5px) clamp(4px, 0.6vw, 8px)',
                        background: 'none',
                        border: '1px solid #d1d5db',
                        borderRadius: '4px',
                        color: '#374151',
                        cursor: 'pointer',
                        fontSize: 'clamp(9px, 1vw, 11px)'
                      }}
                      title="테스트"
                    >
                      <i className="fas fa-vial"></i>
                    </button>

                    <button
                      onClick={() => onEdit(point)}
                      style={{
                        padding: 'clamp(3px, 0.4vw, 5px) clamp(4px, 0.6vw, 8px)',
                        background: 'none',
                        border: '1px solid #d1d5db',
                        borderRadius: '4px',
                        color: '#374151',
                        cursor: 'pointer',
                        fontSize: 'clamp(9px, 1vw, 11px)'
                      }}
                      title="편집"
                    >
                      <i className="fas fa-edit"></i>
                    </button>

                    <button
                      onClick={() => onDelete(point)}
                      style={{
                        padding: 'clamp(3px, 0.4vw, 5px) clamp(4px, 0.6vw, 8px)',
                        background: 'none',
                        border: '1px solid #d1d5db',
                        borderRadius: '4px',
                        color: '#dc2626',
                        cursor: 'pointer',
                        fontSize: 'clamp(9px, 1vw, 11px)'
                      }}
                      title="삭제"
                    >
                      <i className="fas fa-trash"></i>
                    </button>
                  </div>
                </React.Fragment>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  );
};