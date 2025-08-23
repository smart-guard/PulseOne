// ============================================================================
// frontend/src/components/common/FilterPanel.tsx
// 공통 필터 패널 컴포넌트 (VirtualPoints + Alarm 공용)
// ============================================================================

import React from 'react';

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
}

export interface FilterPanelProps {
  filterGroups: FilterGroup[];
  onFiltersChange: (filters: Record<string, any>) => void;
  onClear?: () => void;
  layout?: 'horizontal' | 'vertical';
  showActiveFilters?: boolean;
  className?: string;
}

// ============================================================================
// FilterPanel 메인 컴포넌트
// ============================================================================

export const FilterPanel: React.FC<FilterPanelProps> = ({
  filterGroups,
  onFiltersChange,
  onClear,
  layout = 'horizontal',
  showActiveFilters = true,
  className = ''
}) => {
  // ========================================================================
  // 필터 변경 핸들러
  // ========================================================================
  
  const handleFilterChange = (filterId: string, value: any) => {
    const currentFilters = filterGroups.reduce((acc, group) => {
      acc[group.id] = group.value;
      return acc;
    }, {} as Record<string, any>);

    const newFilters = {
      ...currentFilters,
      [filterId]: value
    };

    onFiltersChange(newFilters);
  };

  const handleClear = () => {
    const clearedFilters = filterGroups.reduce((acc, group) => {
      acc[group.id] = group.type === 'search' ? '' : 
                     group.type === 'multi-select' ? [] :
                     group.type === 'toggle' ? false :
                     'all';
      return acc;
    }, {} as Record<string, any>);

    onFiltersChange(clearedFilters);
    onClear?.();
  };

  // ========================================================================
  // 활성 필터 계산
  // ========================================================================
  
  const getActiveFilters = () => {
    return filterGroups.filter(group => {
      if (group.type === 'search') return group.value && group.value.trim() !== '';
      if (group.type === 'multi-select') return group.value && group.value.length > 0;
      if (group.type === 'toggle') return group.value === true;
      return group.value && group.value !== 'all' && group.value !== '';
    });
  };

  const activeFilters = getActiveFilters();

  // ========================================================================
  // 필터 입력 렌더링 함수들
  // ========================================================================
  
  const renderSearchFilter = (group: FilterGroup) => (
    <div className="search-container">
      <input
        type="text"
        placeholder={group.placeholder || '검색...'}
        value={group.value || ''}
        onChange={(e) => handleFilterChange(group.id, e.target.value)}
        className="search-input"
        disabled={group.disabled}
      />
      <i className="fas fa-search search-icon"></i>
    </div>
  );

  const renderSelectFilter = (group: FilterGroup) => (
    <select
      value={group.value || 'all'}
      onChange={(e) => handleFilterChange(group.id, e.target.value)}
      className="filter-select"
      disabled={group.disabled}
    >
      <option key="all" value="all">전체</option>
      {group.options?.map((option) => (
        <option key={option.value} value={option.value}>
          {option.label}
          {option.count !== undefined && ` (${option.count})`}
        </option>
      ))}
    </select>
  );

  const renderMultiSelectFilter = (group: FilterGroup) => {
    const selectedValues = group.value || [];
    
    return (
      <div className="multi-select-container">
        <div className="multi-select-trigger">
          <span>
            {selectedValues.length === 0 
              ? '선택하세요' 
              : `${selectedValues.length}개 선택됨`
            }
          </span>
          <i className="fas fa-chevron-down"></i>
        </div>
        <div className="multi-select-options">
          {group.options?.map((option) => (
            <label key={option.value} className="multi-select-option">
              <input
                type="checkbox"
                checked={selectedValues.includes(option.value)}
                onChange={(e) => {
                  const newSelected = e.target.checked
                    ? [...selectedValues, option.value]
                    : selectedValues.filter((v: string) => v !== option.value);
                  handleFilterChange(group.id, newSelected);
                }}
                disabled={group.disabled}
              />
              <span>{option.label}</span>
              {option.count !== undefined && (
                <span className="option-count">({option.count})</span>
              )}
            </label>
          ))}
        </div>
      </div>
    );
  };

  const renderToggleFilter = (group: FilterGroup) => (
    <label className="toggle-container">
      <input
        type="checkbox"
        checked={group.value || false}
        onChange={(e) => handleFilterChange(group.id, e.target.checked)}
        className="toggle-input"
        disabled={group.disabled}
      />
      <span className="toggle-slider"></span>
    </label>
  );

  const renderToggleGroupFilter = (group: FilterGroup) => (
    <div className="toggle-group">
      {group.options?.map((option) => (
        <button
          key={option.value}
          className={`toggle-btn ${group.value === option.value ? 'active' : ''}`}
          onClick={() => handleFilterChange(group.id, option.value)}
          disabled={group.disabled}
        >
          {option.label}
          {option.count !== undefined && (
            <span className="toggle-count">({option.count})</span>
          )}
        </button>
      ))}
    </div>
  );

  const renderDateRangeFilter = (group: FilterGroup) => {
    const { startDate = '', endDate = '' } = group.value || {};
    
    return (
      <div className="date-range-container">
        <input
          type="datetime-local"
          value={startDate}
          onChange={(e) => handleFilterChange(group.id, { 
            ...group.value, 
            startDate: e.target.value 
          })}
          className="date-input"
          disabled={group.disabled}
        />
        <span className="date-separator">~</span>
        <input
          type="datetime-local"
          value={endDate}
          onChange={(e) => handleFilterChange(group.id, { 
            ...group.value, 
            endDate: e.target.value 
          })}
          className="date-input"
          disabled={group.disabled}
        />
      </div>
    );
  };

  const renderFilterInput = (group: FilterGroup) => {
    switch (group.type) {
      case 'search':
        return renderSearchFilter(group);
      case 'select':
        return renderSelectFilter(group);
      case 'multi-select':
        return renderMultiSelectFilter(group);
      case 'toggle':
        return renderToggleFilter(group);
      case 'toggle-group':
        return renderToggleGroupFilter(group);
      case 'date-range':
        return renderDateRangeFilter(group);
      default:
        return null;
    }
  };

  // ========================================================================
  // 렌더링
  // ========================================================================
  
  return (
    <div className={`filter-panel ${layout} ${className}`}>
      <div className="filter-row">
        {filterGroups.map((group) => (
          <div
            key={group.id}
            className={`filter-group ${group.className || ''}`}
          >
            <label className="filter-label">
              {group.label}
            </label>
            {renderFilterInput(group)}
          </div>
        ))}
        
        {/* 액션 버튼들 */}
        <div className="filter-actions">
          <button
            type="button"
            onClick={handleClear}
            className="btn btn-outline btn-sm"
            disabled={activeFilters.length === 0}
          >
            <i className="fas fa-refresh"></i>
            초기화
          </button>
        </div>
      </div>

      {/* 활성 필터 표시 */}
      {showActiveFilters && activeFilters.length > 0 && (
        <div className="active-filters">
          <span className="active-filters-label">활성 필터:</span>
          {activeFilters.map((filter) => (
            <span
              key={filter.id}
              className="active-filter-tag"
              onClick={() => {
                const clearValue = filter.type === 'search' ? '' :
                                 filter.type === 'multi-select' ? [] :
                                 filter.type === 'toggle' ? false :
                                 'all';
                handleFilterChange(filter.id, clearValue);
              }}
            >
              {filter.label}: {
                filter.type === 'multi-select' 
                  ? `${filter.value.length}개 선택됨`
                  : filter.type === 'toggle'
                    ? '활성'
                    : Array.isArray(filter.value) 
                      ? filter.value.join(', ')
                      : filter.value
              }
              <span className="remove-filter">×</span>
            </span>
          ))}
        </div>
      )}
    </div>
  );
};