// ============================================================================
// VirtualPointTable.tsx - 다른 페이지와 동일한 패턴 적용
// ============================================================================

import React, { useEffect, useRef, useState, useCallback } from 'react';
import { VirtualPoint } from '../../types/virtualPoints';

interface VirtualPointTableProps {
  virtualPoints: VirtualPoint[];
  onEdit?: (point: VirtualPoint) => void;
  onDelete?: (point: VirtualPoint) => void;
  onTest?: (point: VirtualPoint) => void;
  onExecute?: (pointId: number) => void;
  onToggleEnabled?: (pointId: number) => void;
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

  // ========================================================================
  // 🔥 스크롤바 동적 감지를 위한 Refs와 State
  // ========================================================================
  const headerRef = useRef<HTMLDivElement>(null);
  const bodyRef = useRef<HTMLDivElement>(null);
  const [scrollbarWidth, setScrollbarWidth] = useState(0);

  // 반응형 그리드 컬럼 - 이름/수식 확대, 나머지 축소
  const gridColumns = `
    clamp(180px, 22%, 300px)     /* 이름 - 크게 확대 */
    clamp(50px, 6%, 80px)        /* 분류 */
    clamp(200px, 28%, 400px)     /* 수식 - 크게 확대 */
    clamp(60px, 7%, 90px)        /* 현재값 - 축소 */
    clamp(45px, 5%, 70px)        /* 상태 - 축소 */
    clamp(40px, 4%, 60px)        /* 범위 - 축소 */
    clamp(45px, 5%, 70px)        /* 실행 - 축소 */
    clamp(60px, 7%, 90px)        /* 마지막 - 축소 */
    clamp(70px, 8%, 90px)        /* 작업 - 축소 */
  `;

  // ========================================================================
  // 🔥 스크롤바 너비 실시간 계산 (알람설정 페이지와 동일한 방식)
  // ========================================================================
  const calculateScrollbarWidth = useCallback(() => {
    if (!bodyRef.current) return 0;
    
    const element = bodyRef.current;
    const hasVerticalScrollbar = element.scrollHeight > element.clientHeight;
    
    if (hasVerticalScrollbar) {
      const scrollbarWidth = element.offsetWidth - element.clientWidth;
      return Math.max(scrollbarWidth, 0);
    }
    
    return 0;
  }, []);

  const updateScrollbarWidth = useCallback(() => {
    const newWidth = calculateScrollbarWidth();
    if (newWidth !== scrollbarWidth) {
      setScrollbarWidth(newWidth);
      
      // 🔥 헤더에 스크롤바 너비만큼 패딩 보정 (다른 페이지와 동일)
      if (headerRef.current) {
        headerRef.current.style.paddingRight = `calc(clamp(4px, 0.5vw, 8px) + ${newWidth}px)`;
      }
    }
  }, [scrollbarWidth, calculateScrollbarWidth]);

  // ResizeObserver로 실시간 감지
  useEffect(() => {
    if (!bodyRef.current) return;

    const resizeObserver = new ResizeObserver(() => {
      setTimeout(() => updateScrollbarWidth(), 10);
    });

    resizeObserver.observe(bodyRef.current);
    setTimeout(() => updateScrollbarWidth(), 100);

    return () => {
      resizeObserver.disconnect();
    };
  }, [updateScrollbarWidth]);

  // 데이터 변경 시에도 재계산
  useEffect(() => {
    setTimeout(() => updateScrollbarWidth(), 100);
  }, [virtualPoints.length, updateScrollbarWidth]);

  // 스크롤바 스타일 추가 (다른 페이지와 동일)
  useEffect(() => {
    const styleId = 'virtual-point-table-style';
    
    const existingStyle = document.getElementById(styleId);
    if (existingStyle) {
      existingStyle.remove();
    }
    
    const style = document.createElement('style');
    style.id = styleId;
    style.textContent = `
      .virtual-point-body::-webkit-scrollbar {
        width: 8px;
        background-color: #f1f5f9;
      }
      .virtual-point-body::-webkit-scrollbar-track {
        background: #f1f5f9;
        border-radius: 4px;
      }
      .virtual-point-body::-webkit-scrollbar-thumb {
        background: #cbd5e1;
        border-radius: 4px;
        border: 1px solid #f1f5f9;
      }
      .virtual-point-body::-webkit-scrollbar-thumb:hover {
        background: #94a3b8;
      }
    `;
    document.head.appendChild(style);
    
    return () => {
      const styleElement = document.getElementById(styleId);
      if (styleElement) {
        styleElement.remove();
      }
    };
  }, []);

  // 유틸리티 함수들
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

  const formatValue = (value: any, unit?: string): string => {
    if (value === null || value === undefined) return 'N/A';
    
    if (typeof value === 'number') {
      const formatted = value.toLocaleString('ko-KR', {
        minimumFractionDigits: 0,
        maximumFractionDigits: 2
      });
      return unit ? `${formatted} ${unit}` : formatted;
    }
    
    return String(value);
  };

  const handleSort = (column: string) => {
    if (!onSort) return;
    
    const newOrder = sortBy === column && sortOrder === 'asc' ? 'desc' : 'asc';
    onSort(column, newOrder);
  };

  const SortIcon: React.FC<{ column: string }> = ({ column }) => {
    if (sortBy !== column) return <i className="fas fa-sort" style={{ color: '#d1d5db', fontSize: '9px', marginLeft: '3px' }}></i>;
    
    return (
      <i 
        className={`fas fa-sort-${sortOrder === 'asc' ? 'up' : 'down'}`} 
        style={{ color: '#3b82f6', fontSize: '9px', marginLeft: '3px' }}
      ></i>
    );
  };

  // ========================================================================
  // 🔥 메인 렌더링 - 알람설정 페이지와 동일한 패턴
  // ========================================================================
  return (
    <div style={{
      background: 'white',
      borderRadius: '12px',
      border: '1px solid #e5e7eb',
      overflow: 'hidden',
      boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)',
      width: '100%',
      display: 'flex',
      flexDirection: 'column',
      height: 'clamp(500px, 70vh, 800px)' // 🔥 전체 높이 고정
    }}>
      
      {/* 🔥 헤더 - sticky로 고정, 스크롤 안됨 */}
      <div 
        ref={headerRef}
        style={{
          display: 'grid',
          gridTemplateColumns: gridColumns,
          gap: 'clamp(3px, 0.4vw, 6px)',
          background: '#f8fafc',
          borderBottom: '2px solid #e5e7eb',
          position: 'sticky',
          top: 0,
          zIndex: 10,
          paddingLeft: 'clamp(4px, 0.5vw, 8px)',
          paddingRight: `calc(clamp(4px, 0.5vw, 8px) + ${scrollbarWidth}px)`, // 🔥 스크롤바 보정
          paddingTop: 'clamp(6px, 0.8vw, 10px)',
          paddingBottom: 'clamp(6px, 0.8vw, 10px)',
          flexShrink: 0 // 🔥 헤더 크기 고정
        }}
      >
        {[
          { key: 'name', label: '이름', sortable: true },
          { key: 'category', label: '분류', sortable: true },
          { key: 'expression', label: '수식', sortable: false },
          { key: 'current_value', label: '현재값', sortable: false },
          { key: 'status', label: '상태', sortable: true },
          { key: 'scope', label: '범위', sortable: true },
          { key: 'execution_type', label: '실행', sortable: true },
          { key: 'last_calculated', label: '마지막', sortable: true },
          { key: 'actions', label: '작업', sortable: false }
        ].map((header, index) => (
          <div
            key={header.key}
            onClick={header.sortable !== false ? () => handleSort(header.key) : undefined}
            style={{
              padding: 'clamp(4px, 0.6vw, 8px)',
              fontSize: 'clamp(10px, 1.1vw, 12px)',
              fontWeight: 700,
              color: '#374151',
              cursor: header.sortable !== false ? 'pointer' : 'default',
              display: 'flex',
              alignItems: 'center',
              justifyContent: index < 2 ? 'flex-start' : 'center',
              textAlign: index < 2 ? 'left' : 'center',
              whiteSpace: 'nowrap',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              background: '#f8fafc',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
              transition: header.sortable !== false ? 'background-color 0.2s ease' : 'none'
            }}
            onMouseEnter={(e) => {
              if (header.sortable !== false) {
                e.currentTarget.style.backgroundColor = '#f3f4f6';
              }
            }}
            onMouseLeave={(e) => {
              if (header.sortable !== false) {
                e.currentTarget.style.backgroundColor = '#f8fafc';
              }
            }}
          >
            {header.label}
            {header.sortable !== false && <SortIcon column={header.key} />}
          </div>
        ))}
      </div>

      {/* 🔥 데이터 영역 - 스크롤 가능, 헤더와 그리드 완벽 정렬 */}
      <div 
        ref={bodyRef}
        className="virtual-point-body"
        style={{
          flex: 1, // 🔥 남은 공간 모두 차지
          overflowY: 'auto', // 🔥 세로 스크롤만
          overflowX: 'hidden',
          minHeight: 0,
          scrollbarWidth: 'thin',
          scrollbarColor: '#cbd5e1 #f1f5f9'
        }}
      >
        <div style={{
          display: 'grid',
          gridTemplateColumns: gridColumns, // 🔥 헤더와 완전히 동일
          gap: 'clamp(3px, 0.4vw, 6px)',   // 🔥 헤더와 완전히 동일
          padding: '0 clamp(4px, 0.5vw, 8px) clamp(8px, 1vw, 12px)' // 🔥 헤더와 동일한 좌우 패딩
        }}>
          {loading ? (
            <div style={{
              gridColumn: '1 / -1',
              textAlign: 'center',
              padding: 'clamp(40px, 8vw, 80px)',
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
              padding: 'clamp(60px, 12vw, 120px)',
              color: '#9ca3af',
              fontSize: 'clamp(13px, 1.5vw, 15px)'
            }}>
              <i className="fas fa-inbox" style={{ 
                fontSize: 'clamp(48px, 6vw, 72px)', 
                marginBottom: '16px', 
                display: 'block',
                color: '#d1d5db'
              }}></i>
              표시할 가상포인트가 없습니다.
            </div>
          ) : (
            // 🔥 데이터 행들 - 헤더와 완벽 정렬
            virtualPoints.map((point) => (
              <React.Fragment key={point.id}>
                {/* 이름 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '2px',
                  minHeight: 'clamp(40px, 5vw, 60px)',
                  justifyContent: 'center'
                }}>
                  <div style={{
                    fontWeight: 600,
                    color: '#111827',
                    fontSize: 'clamp(11px, 1.2vw, 14px)',
                    whiteSpace: 'nowrap',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    lineHeight: 1.2
                  }}>
                    {point.name}
                  </div>
                  {point.description && (
                    <div style={{
                      fontSize: 'clamp(9px, 0.9vw, 11px)',
                      color: '#6b7280',
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      lineHeight: 1.2
                    }}>
                      {point.description}
                    </div>
                  )}
                </div>

                {/* 카테고리 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  <span style={{
                    display: 'inline-flex',
                    alignItems: 'center',
                    padding: '2px 6px',
                    background: getCategoryColor(point.category || 'Custom'),
                    color: 'white',
                    borderRadius: '6px',
                    fontSize: 'clamp(8px, 0.9vw, 10px)',
                    fontWeight: 500,
                    maxWidth: '100%',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap'
                  }}>
                    {(point.category || 'Custom').slice(0, 3)}
                  </span>
                </div>

                {/* 수식 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  <code style={{
                    fontSize: 'clamp(10px, 1vw, 12px)',
                    color: '#374151',
                    background: '#f3f4f6',
                    padding: '2px 6px',
                    borderRadius: '4px',
                    fontFamily: 'JetBrains Mono, Consolas, monospace',
                    display: 'block',
                    whiteSpace: 'nowrap',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    width: '100%',
                    fontWeight: 500
                  }}>
                    {point.formula || point.expression || 'N/A'}
                  </code>
                </div>

                {/* 현재값 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  <div style={{
                    fontFamily: 'JetBrains Mono, Consolas, monospace',
                    fontSize: 'clamp(10px, 1.1vw, 13px)',
                    fontWeight: 500,
                    color: point.calculation_status === 'success' || point.calculation_status === 'active' ?
                      '#059669' : point.calculation_status === 'error' ? '#dc2626' : '#6b7280',
                    textAlign: 'center'
                  }}>
                    {formatValue(point.current_value, point.unit)}
                  </div>
                </div>

                {/* 상태 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  <div style={{
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    gap: '2px',
                    fontSize: 'clamp(9px, 1vw, 11px)',
                    fontWeight: 500,
                    color: getStatusColor(point.calculation_status || 'disabled')
                  }}>
                    <i className={getStatusIcon(point.calculation_status || 'disabled')} style={{fontSize: '12px'}}></i>
                    <span style={{
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      fontSize: 'clamp(8px, 0.9vw, 10px)'
                    }}>
                      {point.calculation_status === 'success' || point.calculation_status === 'active' ? '활성' :
                       point.calculation_status === 'error' ? '오류' :
                       point.calculation_status === 'calculating' ? '계산중' : '비활성'}
                    </span>
                  </div>
                </div>

                {/* 범위 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  <span style={{
                    fontSize: 'clamp(9px, 1vw, 11px)',
                    color: '#6b7280',
                    fontWeight: 500,
                    whiteSpace: 'nowrap'
                  }}>
                    {point.scope_type === 'tenant' ? '전역' :
                     point.scope_type === 'site' ? '사이트' :
                     point.scope_type === 'device' ? '디바이스' : '전역'}
                  </span>
                </div>

                {/* 실행방식 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  <span style={{
                    fontSize: 'clamp(9px, 1vw, 11px)',
                    color: '#6b7280',
                    fontWeight: 500,
                    whiteSpace: 'nowrap'
                  }}>
                    {point.execution_type === 'periodic' ? '시간' :
                     point.execution_type === 'on_change' ? '변화' :
                     point.execution_type === 'manual' ? '수동' : '시간'}
                  </span>
                </div>

                {/* 마지막 계산 */}
                <div style={{
                  padding: 'clamp(8px, 1vw, 12px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  <span style={{
                    fontSize: 'clamp(8px, 0.9vw, 10px)',
                    color: '#6b7280',
                    fontFamily: 'JetBrains Mono, Consolas, monospace',
                    whiteSpace: 'nowrap',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    textAlign: 'center'
                  }}>
                    {point.last_calculated ? 
                      new Date(point.last_calculated).toLocaleString('ko-KR', {
                        month: '2-digit',
                        day: '2-digit',
                        hour: '2-digit',
                        minute: '2-digit'
                      }) : 'N/A'}
                  </span>
                </div>

                {/* 작업 */}
                <div style={{
                  padding: 'clamp(6px, 0.8vw, 10px)',
                  borderBottom: '1px solid #f1f5f9',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '2px',
                  minHeight: 'clamp(40px, 5vw, 60px)'
                }}>
                  {/* 실행 버튼 */}
                  <button
                    onClick={() => onExecute?.(point.id)}
                    title="수동 실행"
                    style={{
                      padding: '4px',
                      border: '1px solid #d1d5db',
                      borderRadius: '4px',
                      background: 'white',
                      color: '#059669',
                      cursor: 'pointer',
                      fontSize: '10px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      transition: 'all 0.2s ease',
                      width: '24px',
                      height: '24px'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#f0fdf4';
                      e.currentTarget.style.borderColor = '#059669';
                      e.currentTarget.style.transform = 'translateY(-1px)';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = 'white';
                      e.currentTarget.style.borderColor = '#d1d5db';
                      e.currentTarget.style.transform = 'translateY(0)';
                    }}
                  >
                    <i className="fas fa-play"></i>
                  </button>

                  {/* 편집 버튼 */}
                  <button
                    onClick={() => onEdit?.(point)}
                    title="편집"
                    style={{
                      padding: '4px',
                      border: '1px solid #d1d5db',
                      borderRadius: '4px',
                      background: 'white',
                      color: '#3b82f6',
                      cursor: 'pointer',
                      fontSize: '10px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      transition: 'all 0.2s ease',
                      width: '24px',
                      height: '24px'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#eff6ff';
                      e.currentTarget.style.borderColor = '#3b82f6';
                      e.currentTarget.style.transform = 'translateY(-1px)';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = 'white';
                      e.currentTarget.style.borderColor = '#d1d5db';
                      e.currentTarget.style.transform = 'translateY(0)';
                    }}
                  >
                    <i className="fas fa-edit"></i>
                  </button>

                  {/* 삭제 버튼 */}
                  <button
                    onClick={() => onDelete?.(point)}
                    title="삭제"
                    style={{
                      padding: '4px',
                      border: '1px solid #d1d5db',
                      borderRadius: '4px',
                      background: 'white',
                      color: '#dc2626',
                      cursor: 'pointer',
                      fontSize: '10px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      transition: 'all 0.2s ease',
                      width: '24px',
                      height: '24px'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.background = '#fef2f2';
                      e.currentTarget.style.borderColor = '#dc2626';
                      e.currentTarget.style.transform = 'translateY(-1px)';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.background = 'white';
                      e.currentTarget.style.borderColor = '#d1d5db';
                      e.currentTarget.style.transform = 'translateY(0)';
                    }}
                  >
                    <i className="fas fa-trash"></i>
                  </button>
                </div>
              </React.Fragment>
            ))
          )}
        </div>
      </div>

      {/* 🔥 디버그 정보 */}
      {process.env.NODE_ENV === 'development' && (
        <div style={{
          fontSize: '10px',
          color: '#6b7280',
          padding: '4px 8px',
          background: '#f8fafc',
          borderTop: '1px solid #e5e7eb',
          display: 'flex',
          justifyContent: 'space-between',
          flexShrink: 0
        }}>
          <span>스크롤바 보정: {scrollbarWidth}px</span>
          <span>데이터: {virtualPoints.length}개</span>
          <span>패턴: AlarmSettings 동일</span>
        </div>
      )}
    </div>
  );
};

// ============================================================================
// 🔥 다른 페이지와 동일한 패턴 적용 완료
// ============================================================================

/*
✅ 알람설정/데이터탐색기와 동일한 패턴:
1. 헤더: sticky로 고정, 스크롤 안됨
2. 데이터 영역: 세로 스크롤만
3. 스크롤바 너비 동적 계산으로 헤더 패딩 보정
4. 동일한 grid 설정으로 완벽 정렬
5. 최소 행 높이로 일관된 레이아웃

🎯 핵심 차이점 수정:
- 헤더가 스크롤되지 않음 (sticky)
- 데이터 영역만 스크롤 
- 스크롤바 보정으로 헤더/데이터 정렬
- 다른 페이지와 동일한 스타일링

이제 알람설정, 데이터탐색기와 완전히 동일한 동작입니다! 🎉
*/