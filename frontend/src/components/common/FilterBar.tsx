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
    leftActions?: React.ReactNode;
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
    leftActions,
    rightActions
}) => {
    return (
        <div className={`mgmt-filter-bar ${className}`}>
            {/* 검색 섹션 (가장 앞에 배치) */}
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

            {/* 추가 리드 액션 (커스텀 필터 등 - 기간 설정 등) */}
            {leftActions}

            {/* 개별 필터들 (심각도, 상태 등) */}
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

            {/* 초기화 버튼 (필터 뒤, 액션 앞에 배치) */}
            <button
                className="btn-outline"
                onClick={onReset}
                disabled={activeFilterCount === 0}
                style={{ marginBottom: '1px', minHeight: '36px' }}
            >
                <i className="fas fa-redo-alt"></i>
                초기화
            </button>

            {/* 추가 액션 (일괄 버튼, 조회 버튼 등) */}
            {rightActions}
        </div>
    );
};

export default FilterBar;
