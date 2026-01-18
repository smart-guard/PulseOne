// ============================================================================
// VirtualPointTable.tsx - 다른 페이지와 동일한 패턴 적용
// ============================================================================

import React, { useEffect, useRef, useState, useCallback } from 'react';
import { VirtualPoint } from '../../types/virtualPoints';

interface VirtualPointTableProps {
  virtualPoints: VirtualPoint[];
  onEdit?: (point: VirtualPoint) => void;
  onDelete?: (pointId: number) => void;
  onTest?: (point: VirtualPoint) => void;
  onExecute?: (pointId: number) => void;
  onToggleEnabled?: (pointId: number) => void;
  onRestore?: (pointId: number) => void;
  onRowClick?: (point: VirtualPoint) => void;
  selectedIds?: number[];
  onSelectionChange?: (ids: number[]) => void;
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
  onRestore,
  onRowClick,
  selectedIds = [],
  onSelectionChange,
  sortBy = 'name',
  sortOrder = 'asc',
  onSort,
  loading = false
}) => {

  const headerRef = useRef<HTMLDivElement>(null);
  const bodyRef = useRef<HTMLDivElement>(null);
  const [scrollbarWidth, setScrollbarWidth] = useState(0);

  // 반응형 그리드 컬럼 (이름/수식 위주)
  const gridColumns = `
    40px                         /* 체크박스 */
    minmax(220px, 1.2fr)         /* 이름 */
    80px                         /* 분류 */
    minmax(200px, 2fr)           /* 수식 */
    100px                        /* 현재값 */
    90px                         /* 활성상태 */
    110px                        /* 계산상태 */
    70px                         /* 범위 */
    70px                         /* 실행 */
    100px                        /* 마지막 */
  `;

  const calculateScrollbarWidth = useCallback(() => {
    if (!bodyRef.current) return 0;
    const element = bodyRef.current;
    const hasVerticalScrollbar = element.scrollHeight > element.clientHeight;
    if (hasVerticalScrollbar) {
      return element.offsetWidth - element.clientWidth;
    }
    return 0;
  }, []);

  const updateScrollbarWidth = useCallback(() => {
    const newWidth = calculateScrollbarWidth();
    if (newWidth !== scrollbarWidth) {
      setScrollbarWidth(newWidth);
      if (headerRef.current) {
        headerRef.current.style.paddingRight = `calc(clamp(4px, 0.5vw, 8px) + ${newWidth}px)`;
      }
    }
  }, [scrollbarWidth, calculateScrollbarWidth]);

  useEffect(() => {
    if (!bodyRef.current) return;
    const resizeObserver = new ResizeObserver(() => {
      setTimeout(() => updateScrollbarWidth(), 10);
    });
    resizeObserver.observe(bodyRef.current);
    setTimeout(() => updateScrollbarWidth(), 100);
    return () => resizeObserver.disconnect();
  }, [updateScrollbarWidth]);

  useEffect(() => {
    setTimeout(() => updateScrollbarWidth(), 100);
  }, [virtualPoints.length, updateScrollbarWidth]);

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
      '온도': '#ef4444', '압력': '#3b82f6', '유량': '#10b981', '전력': '#f59e0b',
      '생산': '#8b5cf6', '품질': '#ec4899', '안전': '#f97316', '유지보수': '#6b7280',
      '에너지': '#84cc16', 'Custom': '#14b8a6', 'calculation': '#8b5cf6'
    };
    return colors[category] || '#6b7280';
  };

  const formatValue = (value: any, unit?: string): string => {
    if (value === null || value === undefined) return 'N/A';
    if (typeof value === 'number') {
      const formatted = value.toLocaleString('ko-KR', { minimumFractionDigits: 0, maximumFractionDigits: 2 });
      return unit ? `${formatted} ${unit}` : formatted;
    }
    return String(value);
  };

  const handleSort = (column: string) => {
    if (!onSort) return;
    const newOrder = sortBy === column && sortOrder === 'asc' ? 'desc' : 'asc';
    onSort(column, newOrder);
  };

  return (
    <div style={{
      background: 'white', borderRadius: '12px', border: '1px solid #e5e7eb',
      overflow: 'hidden', boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)',
      width: '100%', display: 'flex', flexDirection: 'column', height: 'clamp(500px, 70vh, 800px)'
    }}>
      <div
        ref={headerRef}
        style={{
          display: 'grid', gridTemplateColumns: gridColumns,
          gap: 'clamp(3px, 0.4vw, 6px)', background: '#f8fafc',
          borderBottom: '2px solid #e5e7eb', position: 'sticky', top: 0, zIndex: 10,
          paddingLeft: 'clamp(4px, 0.5vw, 8px)',
          paddingRight: `calc(clamp(4px, 0.5vw, 8px) + ${scrollbarWidth}px)`,
          paddingTop: '10px', paddingBottom: '10px', flexShrink: 0
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
          <input
            type="checkbox"
            checked={virtualPoints.length > 0 && selectedIds.length === virtualPoints.length}
            onChange={(e) => {
              const checked = e.target.checked;
              onSelectionChange?.(checked ? virtualPoints.map(p => p.id) : []);
            }}
            style={{ cursor: 'pointer', width: '16px', height: '16px' }}
          />
        </div>
        {[
          { key: 'name', label: '이름', sortable: true },
          { key: 'category', label: '분류', sortable: true },
          { key: 'expression', label: '수식', sortable: false },
          { key: 'current_value', label: '현재값', sortable: false },
          { key: 'is_enabled', label: '활성상태', sortable: true },
          { key: 'status', label: '계산상태', sortable: true },
          { key: 'scope', label: '범위', sortable: true },
          { key: 'execution_type', label: '실행', sortable: true },
          { key: 'last_calculated', label: '마지막', sortable: true }
        ].map((header, index) => (
          <div
            key={header.key}
            onClick={header.sortable ? () => handleSort(header.key) : undefined}
            style={{
              padding: '8px', fontSize: '13px', fontWeight: 800, color: '#475569',
              cursor: header.sortable ? 'pointer' : 'default',
              display: 'flex', alignItems: 'center',
              justifyContent: index === 0 || index === 2 ? 'flex-start' : 'center',
              textAlign: index === 0 || index === 2 ? 'left' : 'center',
              whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis'
            }}
          >
            {header.label}
            {header.sortable && (
              <i className={`fas fa-sort${sortBy === header.key ? (sortOrder === 'asc' ? '-up' : '-down') : ''}`}
                style={{ marginLeft: '4px', fontSize: '10px', color: sortBy === header.key ? '#3b82f6' : '#d1d5db' }}></i>
            )}
          </div>
        ))}
      </div>

      <div
        ref={bodyRef}
        style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden', minHeight: 0 }}
      >
        <div style={{
          display: 'grid', gridTemplateColumns: gridColumns,
          gap: 'clamp(3px, 0.4vw, 6px)', padding: '0 clamp(4px, 0.5vw, 8px) 12px'
        }}>
          {loading ? (
            <div style={{ gridColumn: '1 / -1', textAlign: 'center', padding: '40px', color: '#6b7280' }}>
              <i className="fas fa-spinner fa-spin mr-2"></i> 로딩 중...
            </div>
          ) : virtualPoints.length === 0 ? (
            <div style={{ gridColumn: '1 / -1', textAlign: 'center', padding: '100px', color: '#9ca3af' }}>
              <i className="fas fa-inbox fa-3x mb-4 block opacity-20"></i> 표시할 데이터가 없습니다.
            </div>
          ) : (
            virtualPoints.map((point) => {
              const isSelected = selectedIds.includes(point.id);
              return (
                <React.Fragment key={point.id}>
                  {/* 체크박스 */}
                  <div style={{
                    padding: '12px 8px', borderBottom: '1px solid #f1f5f9',
                    display: 'flex', alignItems: 'center', justifyContent: 'center',
                    backgroundColor: isSelected ? '#eff6ff' : 'transparent'
                  }}>
                    <input
                      type="checkbox"
                      checked={isSelected}
                      onChange={(e) => {
                        e.stopPropagation();
                        onSelectionChange?.(isSelected
                          ? selectedIds.filter(id => id !== point.id)
                          : [...selectedIds, point.id]
                        );
                      }}
                      style={{ cursor: 'pointer', width: '16px', height: '16px' }}
                    />
                  </div>

                  {/* 이름 (클릭 가능) */}
                  <div
                    onClick={() => onRowClick?.(point)}
                    style={{
                      padding: '12px 8px', borderBottom: '1px solid #f1f5f9', cursor: 'pointer',
                      display: 'flex', flexDirection: 'column', gap: '2px', justifyContent: 'center',
                      opacity: point.is_deleted ? 0.6 : 1,
                      filter: point.is_deleted ? 'grayscale(0.5)' : 'none',
                      backgroundColor: isSelected ? '#eff6ff' : (point.is_deleted ? '#f8fafc' : 'transparent')
                    }}
                    onMouseEnter={(e) => { if (!point.is_deleted && !isSelected) e.currentTarget.style.backgroundColor = '#f8fafc'; }}
                    onMouseLeave={(e) => { if (!point.is_deleted && !isSelected) e.currentTarget.style.backgroundColor = 'transparent'; }}
                  >
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <div style={{ fontWeight: 700, color: '#0f172a', fontSize: '14px' }}>{point.name}</div>
                      {!!point.is_deleted && (
                        <span style={{ fontSize: '10px', background: '#e2e8f0', color: '#64748b', padding: '1px 5px', borderRadius: '4px', fontWeight: 600 }}>
                          <i className="fas fa-trash-alt" style={{ fontSize: '9px' }}></i> 삭제됨
                        </span>
                      )}
                    </div>
                    {point.description && <div style={{ fontSize: '11px', color: '#6b7280' }}>{point.description}</div>}
                  </div>

                  {/* 분류 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <span style={{
                      padding: '2px 6px', background: getCategoryColor(point.category || 'Custom'),
                      color: 'white', borderRadius: '4px', fontSize: '10px', fontWeight: 500
                    }}>
                      {(point.category || 'Custom').slice(0, 4)}
                    </span>
                  </div>

                  {/* 수식 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center' }}>
                    <code style={{ fontSize: '12px', background: '#f3f4f6', padding: '2px 6px', borderRadius: '4px', width: '100%', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                      {point.expression || point.formula}
                    </code>
                  </div>

                  {/* 현재값 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <span style={{ fontWeight: 600, color: '#0f172a', fontSize: '13px' }}>{formatValue(point.current_value, point.unit)}</span>
                  </div>

                  {/* 활성상태 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <span style={{
                      padding: '2px 8px',
                      borderRadius: '12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      background: point.is_enabled ? '#ecfdf5' : '#f1f5f9',
                      color: point.is_enabled ? '#10b981' : '#94a3b8',
                      border: `1px solid ${point.is_enabled ? '#10b981' : '#e2e8f0'}`
                    }}>
                      {point.is_enabled ? 'ON' : 'OFF'}
                    </span>
                  </div>

                  {/* 계산상태 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '4px', color: getStatusColor(point.calculation_status || 'disabled'), fontSize: '12px', fontWeight: 600 }}>
                      <i className={getStatusIcon(point.calculation_status || 'disabled')}></i>
                      {point.calculation_status === 'active' ? '정상' : point.calculation_status === 'error' ? '오류' : '중지'}
                    </div>
                  </div>

                  {/* 범위 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <span style={{ fontSize: '12px', color: '#6b7280' }}>{point.scope_type === 'global' ? '전역' : point.scope_type === 'site' ? '사이트' : '디바이스'}</span>
                  </div>

                  {/* 실행 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <span style={{ fontSize: '12px', color: '#6b7280' }}>{point.execution_type === 'periodic' ? '주기' : '이벤트'}</span>
                  </div>

                  {/* 마지막 계산 */}
                  <div style={{ padding: '12px 8px', borderBottom: '1px solid #f1f5f9', display: 'flex', alignItems: 'center', justifyContent: 'center', opacity: point.is_deleted ? 0.6 : 1 }}>
                    <span style={{ fontSize: '10px', color: '#94a3b8' }}>
                      {point.last_calculated ? new Date(point.last_calculated).toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit' }) : '-'}
                    </span>
                  </div>

                </React.Fragment>
              );
            })
          )}
        </div>
      </div>
    </div >
  );
};