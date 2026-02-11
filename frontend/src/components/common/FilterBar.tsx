import React from 'react';
import '../../styles/management.css';

export interface FilterOption {
    label: string;
    value: string;
}

export interface FilterConfig {
    label: string;
    value: string;
    options: FilterOption[];
    onChange: (value: string) => void;
    flexWeight?: number;
    minWidth?: string;
}

interface FilterBarProps {
    searchPlaceholder?: string;
    searchTerm?: string;
    onSearchChange?: (value: string) => void;
    filters: FilterConfig[];
    onReset: () => void;
    activeFilterCount: number;
    className?: string;
    leftActions?: React.ReactNode;
    rightActions?: React.ReactNode;
}

export const FilterBar: React.FC<FilterBarProps> = ({
    searchPlaceholder = '검색...',
    searchTerm = '',
    onSearchChange,
    filters,
    onReset,
    activeFilterCount,
    className = '',
    leftActions,
    rightActions
}) => {
    return (
        <div className={`mgmt-filter-bar ${className}`}>
            {/* 필터 섹션: 검색 + 옵션들 */}
            <div className="mgmt-filter-bar-inner">
                {onSearchChange && (
                    <div className="mgmt-filter-group search">
                        <div className="mgmt-search-wrapper">
                            <i className="fas fa-search mgmt-search-icon"></i>
                            <input
                                type="text"
                                className="mgmt-input sm mgmt-input-with-icon"
                                placeholder={searchPlaceholder}
                                value={searchTerm}
                                onChange={(e) => onSearchChange(e.target.value)}
                            />
                        </div>
                    </div>
                )}

                {leftActions}

                {filters.map((filter, index) => (
                    <div
                        key={index}
                        className="mgmt-filter-group"
                    >
                        <label>{filter.label}</label>
                        <select
                            className="mgmt-select sm"
                            value={filter.value}
                            onChange={(e) => filter.onChange(e.target.value)}
                        >
                            {filter.options.map((option) => (
                                <option key={option.value} value={option.value}>
                                    {option.label}
                                </option>
                            ))}
                        </select>
                    </div>
                ))}
            </div>

            {/* 액션 섹션: 초기화 + 추가 버튼들 */}
            <div className="mgmt-filter-actions">
                <button
                    className="mgmt-btn mgmt-btn-outline"
                    onClick={onReset}
                    disabled={activeFilterCount === 0}
                >
                    <i className="fas fa-redo-alt"></i>
                    초기화
                </button>
                {rightActions}
            </div>
        </div>
    );
};

export default FilterBar;
