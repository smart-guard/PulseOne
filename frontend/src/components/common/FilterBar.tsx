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
}

interface FilterBarProps {
    searchPlaceholder?: string;
    searchTerm: string;
    onSearchChange: (value: string) => void;
    filters: FilterConfig[];
    onReset: () => void;
    activeFilterCount: number;
    className?: string;
    rightActions?: React.ReactNode;
}

export const FilterBar: React.FC<FilterBarProps> = ({
    searchPlaceholder = '검색...',
    searchTerm,
    onSearchChange,
    filters,
    onReset,
    activeFilterCount,
    className = '',
    rightActions
}) => {
    return (
        <div className={`mgmt-filter-bar ${className}`}>
            <div style={{ display: 'flex', gap: '16px', flex: 1, alignItems: 'flex-end', flexWrap: 'wrap' }}>
                {/* 검색 섹션 */}
                <div className="mgmt-filter-group search">
                    <label>검색</label>
                    <div className="mgmt-search-wrapper">
                        <i className="fas fa-search mgmt-search-icon"></i>
                        <input
                            type="text"
                            className="mgmt-input mgmt-input-with-icon"
                            placeholder={searchPlaceholder}
                            value={searchTerm}
                            onChange={(e) => onSearchChange(e.target.value)}
                        />
                    </div>
                </div>

                {/* 개별 필터들 */}
                {filters.map((filter, index) => (
                    <div key={index} className="mgmt-filter-group">
                        <label>{filter.label}</label>
                        <select
                            className="mgmt-select"
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

                {/* 초기화 버튼 */}
                <button
                    className="btn-outline"
                    onClick={onReset}
                    disabled={activeFilterCount === 0}
                    style={{ marginBottom: '1px' }} // Align with inputs
                >
                    <i className="fas fa-redo-alt"></i>
                    초기화 {activeFilterCount > 0 && `(${activeFilterCount})`}
                </button>
            </div>

            {rightActions && (
                <div className="mgmt-filter-actions-right">
                    {rightActions}
                </div>
            )}
        </div>
    );
};

export default FilterBar;
