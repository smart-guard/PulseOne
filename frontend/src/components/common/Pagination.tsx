// ============================================================================
// frontend/src/components/common/Pagination.tsx
// 📝 페이징 UI 컴포넌트 - 완전 수정된 버전
// ============================================================================

import React from 'react';
import { PaginationProps } from '../../types/common';

export const Pagination: React.FC<PaginationProps> = ({
  current,
  total,
  pageSize,
  pageSizeOptions = [10, 25, 50, 100],
  showSizeChanger = true,
  showQuickJumper = false,
  showTotal = true,
  onChange,
  onShowSizeChange,
  className = '',
  size = 'default'
}) => {
  const totalPages = Math.ceil(total / pageSize);
  const startIndex = (current - 1) * pageSize + 1;
  const endIndex = Math.min(current * pageSize, total);

  // 페이지 번호 생성 로직
  const getPageNumbers = (): number[] => {
    const maxVisible = 5;
    
    if (totalPages <= maxVisible) {
      return Array.from({ length: totalPages }, (_, i) => i + 1);
    }

    const half = Math.floor(maxVisible / 2);
    let start = Math.max(current - half, 1);
    let end = Math.min(start + maxVisible - 1, totalPages);

    if (end - start + 1 < maxVisible) {
      start = Math.max(end - maxVisible + 1, 1);
    }

    return Array.from({ length: end - start + 1 }, (_, i) => start + i);
  };

  const handlePageChange = (page: number) => {
    if (page >= 1 && page <= totalPages && page !== current) {
      onChange?.(page, pageSize);
    }
  };

  const handleSizeChange = (newSize: number) => {
    onShowSizeChange?.(1, newSize);
  };

  const pageNumbers = getPageNumbers();

  return (
    <div className={`pagination-wrapper ${className}`}>
      {/* 🔥 페이지 정보 - 왼쪽 정렬 */}
      {showTotal && (
        <div className="pagination-info">
          <span>{startIndex}-{endIndex} / {total}개</span>
        </div>
      )}

      {/* 🔥 페이지 네비게이션 - 중앙 정렬 */}
      <div className="pagination-navigation">
        {/* 맨 처음 */}
        <button
          className="pagination-button"
          onClick={() => handlePageChange(1)}
          disabled={current === 1}
          title="맨 처음"
        >
          ‹‹
        </button>

        {/* 이전 */}
        <button
          className="pagination-button"
          onClick={() => handlePageChange(current - 1)}
          disabled={current === 1}
          title="이전"
        >
          ‹
        </button>

        {/* 페이지 번호들 */}
        {pageNumbers.map(page => (
          <button
            key={page}
            className={`pagination-button ${page === current ? 'active' : ''}`}
            onClick={() => handlePageChange(page)}
          >
            {page}
          </button>
        ))}

        {/* 다음 */}
        <button
          className="pagination-button"
          onClick={() => handlePageChange(current + 1)}
          disabled={current === totalPages}
          title="다음"
        >
          ›
        </button>

        {/* 맨 끝 */}
        <button
          className="pagination-button"
          onClick={() => handlePageChange(totalPages)}
          disabled={current === totalPages}
          title="맨 끝"
        >
          ››
        </button>
      </div>

      {/* 🔥 페이지 크기 선택 - 오른쪽 정렬 */}
      {showSizeChanger && (
        <div className="pagination-size-selector">
          <select
            value={pageSize}
            onChange={(e) => handleSizeChange(Number(e.target.value))}
            className="page-size-select"
          >
            {pageSizeOptions.map(size => (
              <option key={size} value={size}>
                {size}개씩
              </option>
            ))}
          </select>
        </div>
      )}
    </div>
  );
};

export default Pagination;