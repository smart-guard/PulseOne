// ============================================================================
// frontend/src/components/common/Pagination/Pagination.tsx
// 페이지네이션 UI 컴포넌트
// ============================================================================

import React from 'react';
import { PaginationProps } from '../../../types/common';

export const Pagination: React.FC<PaginationProps> = ({
  current,
  total,
  pageSize,
  pageSizeOptions = [25, 50, 100, 200],
  showSizeChanger = true,
  showQuickJumper = true,
  showTotal = true,
  onChange,
  onShowSizeChange,
  className = '',
  size = 'default'
}) => {
  const totalPages = Math.ceil(total / pageSize);
  const startIndex = (current - 1) * pageSize + 1;
  const endIndex = Math.min(current * pageSize, total);

  // 페이지 번호 생성
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
    <div className={`pagination-container ${className} pagination-${size}`}>
      {/* 정보 표시 */}
      {showTotal && (
        <div className="pagination-info">
          <span>
            {total > 0 ? `${startIndex}-${endIndex}` : '0-0'} / {total}개
          </span>
        </div>
      )}

      {/* 페이지 크기 선택 */}
      {showSizeChanger && (
        <div className="pagination-size-changer">
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

      {/* 페이지 네비게이션 */}
      <div className="pagination-controls">
        {/* 첫 페이지 */}
        <button
          className="pagination-btn"
          onClick={() => handlePageChange(1)}
          disabled={current === 1}
          title="첫 페이지"
        >
          <i className="fas fa-angle-double-left"></i>
        </button>

        {/* 이전 페이지 */}
        <button
          className="pagination-btn"
          onClick={() => handlePageChange(current - 1)}
          disabled={current === 1}
          title="이전 페이지"
        >
          <i className="fas fa-angle-left"></i>
        </button>

        {/* 페이지 번호들 */}
        {pageNumbers.map(page => (
          <button
            key={page}
            className={`pagination-btn ${page === current ? 'active' : ''}`}
            onClick={() => handlePageChange(page)}
          >
            {page}
          </button>
        ))}

        {/* 다음 페이지 */}
        <button
          className="pagination-btn"
          onClick={() => handlePageChange(current + 1)}
          disabled={current === totalPages}
          title="다음 페이지"
        >
          <i className="fas fa-angle-right"></i>
        </button>

        {/* 마지막 페이지 */}
        <button
          className="pagination-btn"
          onClick={() => handlePageChange(totalPages)}
          disabled={current === totalPages}
          title="마지막 페이지"
        >
          <i className="fas fa-angle-double-right"></i>
        </button>
      </div>

      {/* 빠른 이동 */}
      {showQuickJumper && (
        <div className="pagination-jumper">
          <span>페이지 이동:</span>
          <input
            type="number"
            min={1}
            max={totalPages}
            defaultValue={current}
            onKeyPress={(e) => {
              if (e.key === 'Enter') {
                const target = e.target as HTMLInputElement;
                const page = parseInt(target.value);
                handlePageChange(page);
              }
            }}
            className="page-jumper-input"
          />
        </div>
      )}

      {/* 인라인 스타일 */}
      <style jsx>{`
        .pagination-container {
          display: flex;
          align-items: center;
          gap: 1rem;
          padding: 1rem 0;
          border-top: 1px solid #e5e7eb;
        }

        .pagination-info {
          color: #6b7280;
          font-size: 0.875rem;
        }

        .pagination-size-changer {
          display: flex;
          align-items: center;
          gap: 0.5rem;
        }

        .page-size-select {
          padding: 0.25rem 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
        }

        .pagination-controls {
          display: flex;
          gap: 0.25rem;
        }

        .pagination-btn {
          display: flex;
          align-items: center;
          justify-content: center;
          min-width: 2rem;
          height: 2rem;
          padding: 0 0.5rem;
          background: white;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          cursor: pointer;
          transition: all 0.2s ease;
        }

        .pagination-btn:hover:not(:disabled) {
          background: #f3f4f6;
          border-color: #9ca3af;
        }

        .pagination-btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        .pagination-btn.active {
          background: #3b82f6;
          border-color: #3b82f6;
          color: white;
        }

        .pagination-jumper {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          font-size: 0.875rem;
          color: #6b7280;
        }

        .page-jumper-input {
          width: 4rem;
          padding: 0.25rem 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          text-align: center;
          font-size: 0.875rem;
        }

        /* 크기별 스타일 */
        .pagination-small .pagination-btn {
          min-width: 1.5rem;
          height: 1.5rem;
          font-size: 0.75rem;
        }

        .pagination-large .pagination-btn {
          min-width: 2.5rem;
          height: 2.5rem;
          font-size: 1rem;
        }
      `}</style>
    </div>
  );
};

export default Pagination;