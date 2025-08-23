// ============================================================================
// frontend/src/components/common/FilterPanel.tsx
// 공통 필터 패널 컴포넌트 (가상포인트 + 알람 공용)
// ============================================================================

import React from 'react';

export interface FilterOption {
  value: string;
  label: string;
  count?: number;
}

export interface FilterGroup {
  id: string;
  label: string;
  type: 'select' | 'search' | 'toggle' | 'multiSelect' | 'dateRange';
  value?: any;
  options?: FilterOption[];
  placeholder?: string;
  className?: string;
  flex?: number;
  onChange?: (value: any) => void;
}

export interface FilterPanelProps {
  filters: FilterGroup[];
  onFiltersChange?: (filters: Record<string, any>) => void;
  className?: string;
  layout?: 'horizontal' | 'vertical';
  showClearAll?: boolean;
  showApply?: boolean;
}

export const FilterPanel: React.FC<FilterPanelProps> = ({
  filters,
  onFiltersChange,
  className = '',
  layout = 'horizontal',
  showClearAll = true,
  showApply = false
}) => {
  // ========================================================================
  // 이벤트 핸들러
  // ========================================================================
  
  const handleFilterChange = (filterId: string, value: any) => {
    const updatedFilters: Record<string, any> = {};
    
    filters.forEach(filter => {
      if (filter.id === filterId) {
        updatedFilters[filter.id] = value;
      } else {
        updatedFilters[filter.id] = filter.value;
      }
    });
    
    onFiltersChange?.(updatedFilters);
  };

  const handleClearAll = () => {
    const clearedFilters: Record<string, any> = {};
    
    filters.forEach(filter => {
      switch (filter.type) {
        case 'search':
          clearedFilters[filter.id] = '';
          break;
        case 'select':
          clearedFilters[filter.id] = 'all';
          break;
        case 'toggle':
          clearedFilters[filter.id] = undefined;
          break;
        case 'multiSelect':
          clearedFilters[filter.id] = [];
          break;
        case 'dateRange':
          clearedFilters[filter.id] = { start: null, end: null };
          break;
        default:
          clearedFilters[filter.id] = null;
      }
    });
    
    onFiltersChange?.(clearedFilters);
  };

  const hasActiveFilters = filters.some(filter => {
    const value = filter.value;
    switch (filter.type) {
      case 'search':
        return value && value.trim() !== '';
      case 'select':
        return value && value !== 'all';
      case 'toggle':
        return value !== undefined;
      case 'multiSelect':
        return Array.isArray(value) && value.length > 0;
      case 'dateRange':
        return value && (value.start || value.end);
      default:
        return value != null;
    }
  });

  // ========================================================================
  // 렌더링 함수들
  // ========================================================================
  
  const renderSearchFilter = (filter: FilterGroup) => (
    <div className="search-container">
      <span className="search-icon">🔍</span>
      <input
        type="text"
        placeholder={filter.placeholder || `${filter.label} 검색...`}
        value={filter.value || ''}
        onChange={(e) => handleFilterChange(filter.id, e.target.value)}
        className="search-input"
      />
    </div>
  );

  const renderSelectFilter = (filter: FilterGroup) => (
    <select
      value={filter.value || 'all'}
      onChange={(e) => handleFilterChange(filter.id, e.target.value)}
      className="filter-select"
    >
      <option value="all">전체 {filter.label}</option>
      {filter.options?.map(option => (
        <option key={option.value} value={option.value}>
          {option.label}
          {option.count !== undefined && ` (${option.count})`}
        </option>
      ))}
    </select>
  );

  const renderToggleFilter = (filter: FilterGroup) => (
    <div className="toggle-group">
      <button
        className={`toggle-btn ${filter.value === true ? 'active' : ''}`}
        onClick={() => handleFilterChange(filter.id, filter.value === true ? undefined : true)}
      >
        활성화
      </button>
      <button
        className={`toggle-btn ${filter.value === false ? 'active' : ''}`}
        onClick={() => handleFilterChange(filter.id, filter.value === false ? undefined : false)}
      >
        비활성화
      </button>
      <button
        className={`toggle-btn ${filter.value === undefined ? 'active' : ''}`}
        onClick={() => handleFilterChange(filter.id, undefined)}
      >
        전체
      </button>
    </div>
  );

  const renderMultiSelectFilter = (filter: FilterGroup) => {
    const selectedValues = Array.isArray(filter.value) ? filter.value : [];
    
    return (
      <div className="multi-select-container">
        <div className="multi-select-header">
          <span>{filter.label}</span>
          {selectedValues.length > 0 && (
            <span className="selected-count">{selectedValues.length}개 선택</span>
          )}
        </div>
        <div className="multi-select-options">
          {filter.options?.map(option => (
            <label key={option.value} className="multi-select-option">
              <input
                type="checkbox"
                checked={selectedValues.includes(option.value)}
                onChange={(e) => {
                  const newValues = e.target.checked
                    ? [...selectedValues, option.value]
                    : selectedValues.filter(v => v !== option.value);
                  handleFilterChange(filter.id, newValues);
                }}
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

  const renderDateRangeFilter = (filter: FilterGroup) => {
    const value = filter.value || { start: '', end: '' };
    
    return (
      <div className="date-range-container">
        <input
          type="datetime-local"
          value={value.start || ''}
          onChange={(e) => handleFilterChange(filter.id, { ...value, start: e.target.value })}
          className="date-input"
          placeholder="시작 날짜"
        />
        <span className="date-separator">~</span>
        <input
          type="datetime-local"
          value={value.end || ''}
          onChange={(e) => handleFilterChange(filter.id, { ...value, end: e.target.value })}
          className="date-input"
          placeholder="종료 날짜"
        />
      </div>
    );
  };

  const renderFilter = (filter: FilterGroup) => {
    let content;
    
    switch (filter.type) {
      case 'search':
        content = renderSearchFilter(filter);
        break;
      case 'select':
        content = renderSelectFilter(filter);
        break;
      case 'toggle':
        content = renderToggleFilter(filter);
        break;
      case 'multiSelect':
        content = renderMultiSelectFilter(filter);
        break;
      case 'dateRange':
        content = renderDateRangeFilter(filter);
        break;
      default:
        content = <div>지원되지 않는 필터 타입</div>;
    }

    return (
      <div 
        key={filter.id}
        className={`filter-group ${filter.className || ''}`}
        style={{ flex: filter.flex || 'none' }}
      >
        <label className="filter-label">{filter.label}</label>
        {content}
      </div>
    );
  };

  // ========================================================================
  // 메인 렌더링
  // ========================================================================
  
  return (
    <div className={`filter-panel ${layout} ${className}`}>
      <div className="filter-row">
        {filters.map(renderFilter)}
        
        {(showClearAll || showApply) && (
          <div className="filter-actions">
            {showClearAll && (
              <button
                className="btn btn-outline btn-sm"
                onClick={handleClearAll}
                disabled={!hasActiveFilters}
              >
                🔄 초기화
              </button>
            )}
            
            {showApply && (
              <button className="btn btn-primary btn-sm">
                ✅ 적용
              </button>
            )}
          </div>
        )}
      </div>
      
      {/* 활성 필터 표시 */}
      {hasActiveFilters && (
        <div className="active-filters">
          <span className="active-filters-label">활성 필터:</span>
          {filters
            .filter(filter => {
              const value = filter.value;
              switch (filter.type) {
                case 'search':
                  return value && value.trim() !== '';
                case 'select':
                  return value && value !== 'all';
                case 'toggle':
                  return value !== undefined;
                case 'multiSelect':
                  return Array.isArray(value) && value.length > 0;
                case 'dateRange':
                  return value && (value.start || value.end);
                default:
                  return value != null;
              }
            })
            .map(filter => (
              <span 
                key={filter.id} 
                className="active-filter-tag"
                onClick={() => handleFilterChange(filter.id, 
                  filter.type === 'search' ? '' :
                  filter.type === 'select' ? 'all' :
                  filter.type === 'toggle' ? undefined :
                  filter.type === 'multiSelect' ? [] :
                  filter.type === 'dateRange' ? { start: null, end: null } :
                  null
                )}
              >
                {filter.label}: {
                  filter.type === 'multiSelect' 
                    ? `${filter.value.length}개 선택`
                    : String(filter.value)
                }
                <span className="remove-filter">×</span>
              </span>
            ))
          }
        </div>
      )}
    </div>
  );
};

export { FilterPanel };
export default FilterPanel;