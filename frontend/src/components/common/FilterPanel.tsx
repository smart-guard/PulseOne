import React, { useState, useEffect, useRef } from 'react';

// ============================================================================
// 타입 정의
// ============================================================================

export interface FilterOption {
  value: string;
  label: string;
  count?: number;
}

export interface FilterGroup {
  id: string;
  label: string;
  type: 'search' | 'select' | 'multi-select' | 'date-range' | 'toggle' | 'toggle-group';
  value?: any;
  placeholder?: string;
  options?: FilterOption[];
  className?: string;
  multiple?: boolean;
  disabled?: boolean;
  priority?: number;
  width?: 'auto' | 'small' | 'medium' | 'large' | 'flex';
}

export interface FilterPanelProps {
  filterGroups: FilterGroup[];
  onFiltersChange: (filters: Record<string, any>) => void;
  onClear?: () => void;
  showActiveFilters?: boolean;
  className?: string;
  style?: 'clean' | 'card';
}

// ============================================================================
// FilterPanel - 완전 수정된 버전
// ============================================================================

export const FilterPanel: React.FC<FilterPanelProps> = ({
  filterGroups,
  onFiltersChange,
  onClear,
  showActiveFilters = false,
  className = '',
  style = 'clean'
}) => {
  const [openDropdown, setOpenDropdown] = useState<string | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  // ========================================================================
  // 필터 변경 핸들러 - 직접적으로 props 값 사용
  // ========================================================================
  
  const handleFilterChange = (filterId: string, value: any) => {
    console.log('Filter changed:', filterId, value);
    
    const currentFilters = filterGroups.reduce((acc, group) => {
      acc[group.id] = group.value;
      return acc;
    }, {} as Record<string, any>);

    const newFilters = {
      ...currentFilters,
      [filterId]: value
    };
    
    onFiltersChange(newFilters);
    
    // 드롭다운 닫기 (select 타입 제외)
    if (openDropdown && filterGroups.find(g => g.id === filterId)?.type !== 'select') {
      setOpenDropdown(null);
    }
  };

  const handleClear = () => {
    console.log('Clear filters called');
    
    const clearedFilters = filterGroups.reduce((acc, group) => {
      acc[group.id] = group.type === 'search' ? '' : 
                     group.type === 'multi-select' ? [] :
                     group.type === 'toggle' ? false :
                     group.type === 'date-range' ? { startDate: '', endDate: '' } :
                     'all';
      return acc;
    }, {} as Record<string, any>);

    onFiltersChange(clearedFilters);
    onClear?.();
    setOpenDropdown(null);
  };

  // 외부 클릭시 드롭다운 닫기
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (containerRef.current && !containerRef.current.contains(event.target as Node)) {
        setOpenDropdown(null);
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, []);

  // ========================================================================
  // 활성 필터 계산 - props 값 직접 사용
  // ========================================================================
  
  const getActiveFilters = () => {
    return filterGroups.filter(group => {
      const value = group.value;
      
      if (group.type === 'search') return value && value.trim() !== '';
      if (group.type === 'multi-select') return value && Array.isArray(value) && value.length > 0;
      if (group.type === 'toggle') return value === true;
      if (group.type === 'date-range') {
        const dateValue = value || {};
        return dateValue.startDate || dateValue.endDate;
      }
      return value && value !== 'all' && value !== '';
    });
  };

  const activeFilters = getActiveFilters();

  // ========================================================================
  // 필터 너비 계산
  // ========================================================================
  
  const getFilterWidth = (group: FilterGroup) => {
    if (group.width) {
      switch (group.width) {
        case 'small': return '120px';
        case 'medium': return '180px';
        case 'large': return '240px';
        case 'flex': return 'minmax(200px, 1fr)';
        case 'auto': 
        default:
          return 'auto';
      }
    }
    
    // 기본 너비 (타입별)
    switch (group.type) {
      case 'search': return 'minmax(200px, 1fr)';
      case 'select': return '140px';
      case 'multi-select': return '160px';
      case 'toggle': return 'auto';
      case 'toggle-group': return 'auto';
      case 'date-range': return '320px';
      default: return 'auto';
    }
  };

  // ========================================================================
  // 필터 입력 렌더링 함수들
  // ========================================================================
  
  const renderSearchFilter = (group: FilterGroup) => (
    <div 
      style={{ 
        position: 'relative',
        width: getFilterWidth(group),
        minWidth: group.type === 'search' ? '200px' : 'auto'
      }}
    >
      <i 
        className="fas fa-search"
        style={{
          position: 'absolute',
          left: '12px',
          top: '50%',
          transform: 'translateY(-50%)',
          color: '#9ca3af',
          fontSize: '14px',
          pointerEvents: 'none',
          zIndex: 1
        }}
      />
      <input
        type="text"
        placeholder={group.placeholder || '검색...'}
        value={group.value || ''}
        onChange={(e) => handleFilterChange(group.id, e.target.value)}
        disabled={group.disabled}
        style={{
          width: '100%',
          height: '38px',
          paddingLeft: '36px',
          paddingRight: '12px',
          border: '1px solid #d1d5db',
          borderRadius: '8px',
          fontSize: '14px',
          backgroundColor: group.disabled ? '#f9fafb' : '#ffffff',
          color: group.disabled ? '#9ca3af' : '#374151',
          outline: 'none',
          transition: 'border-color 0.2s ease, box-shadow 0.2s ease',
          boxSizing: 'border-box'
        }}
        onFocus={(e) => {
          e.target.style.borderColor = '#3b82f6';
          e.target.style.boxShadow = '0 0 0 3px rgba(59, 130, 246, 0.1)';
        }}
        onBlur={(e) => {
          e.target.style.borderColor = '#d1d5db';
          e.target.style.boxShadow = 'none';
        }}
      />
    </div>
  );

  const renderSelectFilter = (group: FilterGroup) => (
    <div style={{ position: 'relative', width: getFilterWidth(group) }}>
      <select
        value={group.value || 'all'}
        onChange={(e) => {
          e.stopPropagation();
          console.log('Select changed:', group.id, e.target.value);
          handleFilterChange(group.id, e.target.value);
        }}
        disabled={group.disabled}
        style={{
          width: '100%',
          height: '38px',
          paddingLeft: '12px',
          paddingRight: '32px',
          border: '1px solid #d1d5db',
          borderRadius: '8px',
          fontSize: '14px',
          backgroundColor: group.disabled ? '#f9fafb' : '#ffffff',
          color: group.disabled ? '#9ca3af' : '#374151',
          outline: 'none',
          appearance: 'none',
          cursor: group.disabled ? 'not-allowed' : 'pointer',
          boxSizing: 'border-box',
          transition: 'border-color 0.2s ease, box-shadow 0.2s ease'
        }}
        onFocus={(e) => {
          if (!group.disabled) {
            e.target.style.borderColor = '#3b82f6';
            e.target.style.boxShadow = '0 0 0 3px rgba(59, 130, 246, 0.1)';
          }
        }}
        onBlur={(e) => {
          e.target.style.borderColor = '#d1d5db';
          e.target.style.boxShadow = 'none';
        }}
      >
        <option value="all">전체</option>
        {group.options?.map((option) => (
          <option key={option.value} value={option.value}>
            {option.label}
            {option.count !== undefined && ` (${option.count})`}
          </option>
        ))}
      </select>
      <i 
        className="fas fa-chevron-down"
        style={{
          position: 'absolute',
          right: '12px',
          top: '50%',
          transform: 'translateY(-50%)',
          color: '#9ca3af',
          fontSize: '12px',
          pointerEvents: 'none'
        }}
      />
    </div>
  );

  const renderMultiSelectFilter = (group: FilterGroup) => {
    const selectedValues = group.value || [];
    const isOpen = openDropdown === group.id;
    
    return (
      <div style={{ position: 'relative', width: getFilterWidth(group) }}>
        <button
          type="button"
          onClick={(e) => {
            e.stopPropagation();
            console.log('Multi-select clicked:', group.id, isOpen);
            setOpenDropdown(isOpen ? null : group.id);
          }}
          disabled={group.disabled}
          style={{
            width: '100%',
            height: '38px',
            paddingLeft: '12px',
            paddingRight: '32px',
            border: '1px solid #d1d5db',
            borderRadius: '8px',
            fontSize: '14px',
            backgroundColor: group.disabled ? '#f9fafb' : '#ffffff',
            color: group.disabled ? '#9ca3af' : '#374151',
            textAlign: 'left',
            cursor: group.disabled ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            outline: 'none',
            boxSizing: 'border-box',
            transition: 'border-color 0.2s ease, box-shadow 0.2s ease'
          }}
          onFocus={(e) => {
            if (!group.disabled) {
              e.target.style.borderColor = '#3b82f6';
              e.target.style.boxShadow = '0 0 0 3px rgba(59, 130, 246, 0.1)';
            }
          }}
          onBlur={(e) => {
            e.target.style.borderColor = '#d1d5db';
            e.target.style.boxShadow = 'none';
          }}
        >
          <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {selectedValues.length === 0 
              ? '선택하세요'
              : `${selectedValues.length}개 선택됨`
            }
          </span>
          <i 
            className={`fas fa-chevron-down`}
            style={{
              color: '#9ca3af',
              fontSize: '12px',
              transform: isOpen ? 'rotate(180deg)' : 'none',
              transition: 'transform 0.2s ease'
            }}
          />
        </button>
        
        {isOpen && (
          <div
            style={{
              position: 'absolute',
              top: '100%',
              left: 0,
              right: 0,
              zIndex: 50,
              marginTop: '4px',
              backgroundColor: '#ffffff',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              boxShadow: '0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05)',
              maxHeight: '200px',
              overflowY: 'auto'
            }}
          >
            {group.options?.map((option) => (
              <label
                key={option.value}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  padding: '10px 12px',
                  cursor: 'pointer',
                  fontSize: '14px',
                  color: '#374151',
                  transition: 'background-color 0.2s ease',
                  borderBottom: '1px solid #f3f4f6'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = '#f9fafb';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }}
                onClick={(e) => {
                  e.stopPropagation();
                }}
              >
                <input
                  type="checkbox"
                  checked={selectedValues.includes(option.value)}
                  onChange={(e) => {
                    e.stopPropagation();
                    const newSelected = e.target.checked
                      ? [...selectedValues, option.value]
                      : selectedValues.filter((v: string) => v !== option.value);
                    console.log('Checkbox changed:', group.id, newSelected);
                    handleFilterChange(group.id, newSelected);
                  }}
                  style={{
                    marginRight: '8px',
                    accentColor: '#3b82f6'
                  }}
                />
                <span style={{ flex: 1 }}>{option.label}</span>
                {option.count !== undefined && (
                  <span style={{ fontSize: '12px', color: '#9ca3af' }}>({option.count})</span>
                )}
              </label>
            ))}
          </div>
        )}
      </div>
    );
  };

  const renderDateRangeFilter = (group: FilterGroup) => {
    const currentValue = group.value || {};
    const { startDate = '', endDate = '' } = currentValue;
    
    return (
      <div 
        style={{ 
          display: 'flex', 
          alignItems: 'center', 
          gap: '8px',
          width: getFilterWidth(group)
        }}
      >
        <input
          type="datetime-local"
          value={startDate}
          onChange={(e) => handleFilterChange(group.id, { 
            ...currentValue, 
            startDate: e.target.value 
          })}
          disabled={group.disabled}
          style={{
            height: '38px',
            padding: '0 12px',
            border: '1px solid #d1d5db',
            borderRadius: '8px',
            fontSize: '13px',
            backgroundColor: group.disabled ? '#f9fafb' : '#ffffff',
            color: group.disabled ? '#9ca3af' : '#374151',
            outline: 'none',
            flex: 1,
            minWidth: '150px',
            transition: 'border-color 0.2s ease, box-shadow 0.2s ease'
          }}
          onFocus={(e) => {
            if (!group.disabled) {
              e.target.style.borderColor = '#3b82f6';
              e.target.style.boxShadow = '0 0 0 3px rgba(59, 130, 246, 0.1)';
            }
          }}
          onBlur={(e) => {
            e.target.style.borderColor = '#d1d5db';
            e.target.style.boxShadow = 'none';
          }}
        />
        <span style={{ color: '#9ca3af', fontSize: '14px', userSelect: 'none' }}>~</span>
        <input
          type="datetime-local"
          value={endDate}
          onChange={(e) => handleFilterChange(group.id, { 
            ...currentValue, 
            endDate: e.target.value 
          })}
          disabled={group.disabled}
          style={{
            height: '38px',
            padding: '0 12px',
            border: '1px solid #d1d5db',
            borderRadius: '8px',
            fontSize: '13px',
            backgroundColor: group.disabled ? '#f9fafb' : '#ffffff',
            color: group.disabled ? '#9ca3af' : '#374151',
            outline: 'none',
            flex: 1,
            minWidth: '150px',
            transition: 'border-color 0.2s ease, box-shadow 0.2s ease'
          }}
          onFocus={(e) => {
            if (!group.disabled) {
              e.target.style.borderColor = '#3b82f6';
              e.target.style.boxShadow = '0 0 0 3px rgba(59, 130, 246, 0.1)';
            }
          }}
          onBlur={(e) => {
            e.target.style.borderColor = '#d1d5db';
            e.target.style.boxShadow = 'none';
          }}
        />
      </div>
    );
  };

  const renderToggleFilter = (group: FilterGroup) => (
    <label 
      style={{ 
        display: 'flex', 
        alignItems: 'center', 
        gap: '8px', 
        cursor: group.disabled ? 'not-allowed' : 'pointer',
        opacity: group.disabled ? 0.6 : 1,
        userSelect: 'none'
      }}
    >
      <div style={{ position: 'relative' }}>
        <input
          type="checkbox"
          checked={group.value || false}
          onChange={(e) => {
            console.log('Toggle changed:', group.id, e.target.checked);
            handleFilterChange(group.id, e.target.checked);
          }}
          disabled={group.disabled}
          style={{ display: 'none' }}
        />
        <div
          style={{
            width: '44px',
            height: '24px',
            borderRadius: '12px',
            backgroundColor: group.value ? '#3b82f6' : '#e5e7eb',
            position: 'relative',
            transition: 'background-color 0.2s ease',
            cursor: group.disabled ? 'not-allowed' : 'pointer'
          }}
          onClick={() => {
            if (!group.disabled) {
              handleFilterChange(group.id, !group.value);
            }
          }}
        >
          <div
            style={{
              width: '20px',
              height: '20px',
              borderRadius: '10px',
              backgroundColor: '#ffffff',
              position: 'absolute',
              top: '2px',
              left: group.value ? '22px' : '2px',
              transition: 'left 0.2s ease',
              boxShadow: '0 2px 4px rgba(0, 0, 0, 0.1)'
            }}
          />
        </div>
      </div>
      <span style={{ fontSize: '14px', color: '#374151' }}>{group.label}</span>
    </label>
  );

  const renderToggleGroupFilter = (group: FilterGroup) => (
    <div 
      style={{ 
        display: 'flex', 
        backgroundColor: '#f3f4f6', 
        borderRadius: '8px', 
        padding: '4px',
        gap: '2px'
      }}
    >
      {group.options?.map((option) => (
        <button
          key={option.value}
          onClick={() => {
            console.log('Toggle group changed:', group.id, option.value);
            handleFilterChange(group.id, option.value);
          }}
          disabled={group.disabled}
          style={{
            padding: '6px 12px',
            fontSize: '14px',
            fontWeight: '500',
            borderRadius: '6px',
            border: 'none',
            cursor: group.disabled ? 'not-allowed' : 'pointer',
            transition: 'all 0.2s ease',
            backgroundColor: group.value === option.value ? '#ffffff' : 'transparent',
            color: group.value === option.value ? '#3b82f6' : '#6b7280',
            boxShadow: group.value === option.value ? '0 1px 3px rgba(0, 0, 0, 0.1)' : 'none',
            opacity: group.disabled ? 0.6 : 1
          }}
        >
          {option.label}
          {option.count !== undefined && (
            <span style={{ marginLeft: '4px', fontSize: '12px', opacity: 0.7 }}>
              ({option.count})
            </span>
          )}
        </button>
      ))}
    </div>
  );

  const renderFilterInput = (group: FilterGroup) => {
    switch (group.type) {
      case 'search': return renderSearchFilter(group);
      case 'select': return renderSelectFilter(group);
      case 'multi-select': return renderMultiSelectFilter(group);
      case 'date-range': return renderDateRangeFilter(group);
      case 'toggle': return renderToggleFilter(group);
      case 'toggle-group': return renderToggleGroupFilter(group);
      default: return null;
    }
  };

  // ========================================================================
  // 스타일 변수 - 간격 개선
  // ========================================================================
  
  const containerStyle = style === 'card' ? {
    background: '#ffffff',
    border: '1px solid #e5e7eb',
    borderRadius: '12px',
    boxShadow: '0 1px 3px 0 rgba(0, 0, 0, 0.1)',
    padding: '20px',
    marginBottom: '24px'
  } : {
    background: '#ffffff',
    borderBottom: '1px solid #e5e7eb',
    padding: '20px',
    marginBottom: '24px'
  };

  // ========================================================================
  // 디버깅 정보
  // ========================================================================
  
  console.log('FilterPanel render:', {
    filterValues: filterGroups.map(g => ({ id: g.id, value: g.value })),
    activeFilters: activeFilters.length,
    filterGroups: filterGroups.length
  });

  // ========================================================================
  // 렌더링 - 수정된 버전
  // ========================================================================
  
  return (
    <div 
      ref={containerRef}
      className={className}
      style={{
        ...containerStyle,
        width: '100%'
      }}
    >
      {/* 메인 필터 영역 - 깔끔한 한 줄 배치 */}
      <div style={{ 
        display: 'flex', 
        alignItems: 'end', 
        gap: '20px', 
        flexWrap: 'wrap'
      }}>
        {filterGroups.map((group) => (
          <div 
            key={group.id} 
            className={group.className || ''}
            style={{ 
              display: 'flex', 
              flexDirection: 'column', 
              gap: '8px',
              minWidth: group.type === 'toggle' ? 'auto' : '80px'
            }}
          >
            {/* 라벨 (토글 타입은 제외) */}
            {group.type !== 'toggle' && (
              <label style={{ 
                fontSize: '14px', 
                fontWeight: '500', 
                color: '#374151',
                whiteSpace: 'nowrap',
                userSelect: 'none',
                marginBottom: '2px'
              }}>
                {group.label}
              </label>
            )}
            
            {/* 필터 입력 */}
            {renderFilterInput(group)}
          </div>
        ))}
        
        {/* 초기화 버튼 */}
        <div style={{ display: 'flex', alignSelf: 'end' }}>
          <button
            type="button"
            onClick={handleClear}
            disabled={activeFilters.length === 0}
            style={{
              height: '38px',
              padding: '0 16px',
              fontSize: '14px',
              fontWeight: '500',
              color: activeFilters.length === 0 ? '#9ca3af' : '#6b7280',
              backgroundColor: activeFilters.length === 0 ? '#f3f4f6' : '#f9fafb',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              cursor: activeFilters.length === 0 ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              transition: 'all 0.2s ease',
              opacity: activeFilters.length === 0 ? 0.6 : 1
            }}
            onMouseEnter={(e) => {
              if (activeFilters.length > 0) {
                e.currentTarget.style.backgroundColor = '#f3f4f6';
                e.currentTarget.style.borderColor = '#9ca3af';
              }
            }}
            onMouseLeave={(e) => {
              if (activeFilters.length > 0) {
                e.currentTarget.style.backgroundColor = '#f9fafb';
                e.currentTarget.style.borderColor = '#d1d5db';
              }
            }}
          >
            <i className="fas fa-redo-alt" style={{ fontSize: '12px' }}></i>
            <span>초기화</span>
          </button>
        </div>
      </div>

      {/* 활성 필터 표시 (옵션) */}
      {showActiveFilters && activeFilters.length > 0 && (
        <div style={{ 
          marginTop: '20px',
          paddingTop: '16px',
          borderTop: '1px solid #f3f4f6',
          display: 'flex',
          flexWrap: 'wrap',
          alignItems: 'center',
          gap: '8px'
        }}>
          <span style={{ 
            fontSize: '13px', 
            fontWeight: '500', 
            color: '#6b7280' 
          }}>
            활성 필터:
          </span>
          {activeFilters.map((filter) => {
            const value = filter.value;
            return (
              <span
                key={filter.id}
                onClick={() => {
                  const clearValue = filter.type === 'search' ? '' :
                                   filter.type === 'multi-select' ? [] :
                                   filter.type === 'toggle' ? false :
                                   filter.type === 'date-range' ? { startDate: '', endDate: '' } :
                                   'all';
                  handleFilterChange(filter.id, clearValue);
                }}
                style={{
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '4px',
                  padding: '4px 8px',
                  backgroundColor: '#dbeafe',
                  color: '#1e40af',
                  fontSize: '12px',
                  borderRadius: '6px',
                  cursor: 'pointer',
                  transition: 'background-color 0.2s ease',
                  maxWidth: '150px'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = '#bfdbfe';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = '#dbeafe';
                }}
              >
                <span style={{ 
                  overflow: 'hidden', 
                  textOverflow: 'ellipsis', 
                  whiteSpace: 'nowrap' 
                }}>
                  {filter.label}: {
                    filter.type === 'multi-select' 
                      ? `${value.length}개`
                      : filter.type === 'toggle'
                        ? '활성'
                        : filter.type === 'date-range'
                          ? '설정됨'
                          : String(value)
                  }
                </span>
                <i 
                  className="fas fa-times" 
                  style={{ 
                    fontSize: '10px',
                    opacity: 0.8
                  }}
                />
              </span>
            );
          })}
        </div>
      )}
    </div>
  );
};