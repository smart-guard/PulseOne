// ============================================================================
// frontend/src/hooks/usePagination.ts
// 페이지네이션 로직 전용 커스텀 훅 - 상수 업데이트
// ============================================================================

import { useState, useMemo, useCallback } from 'react';
import { 
  PaginationHookReturn, 
  PaginationHookState
} from '../types/common';
import { DEFAULT_PAGINATION_CONFIG } from '../constants/pagination';

interface UsePaginationOptions {
  initialPage?: number;
  initialPageSize?: number;
  totalCount?: number;
  pageSizeOptions?: number[];
  maxVisiblePages?: number;
}

export const usePagination = (options: UsePaginationOptions = {}): PaginationHookReturn => {
  const {
    initialPage = 1,
    initialPageSize = DEFAULT_PAGINATION_CONFIG.defaultPageSize,
    totalCount = 0,
    pageSizeOptions = DEFAULT_PAGINATION_CONFIG.pageSizeOptions,
    maxVisiblePages = DEFAULT_PAGINATION_CONFIG.maxVisiblePages
  } = options;

  // ==========================================================================
  // 상태 관리
  // ==========================================================================

  const [currentPage, setCurrentPage] = useState<number>(initialPage);
  const [pageSize, setPageSize] = useState<number>(initialPageSize);

  // ==========================================================================
  // 계산된 값들
  // ==========================================================================

  const state: PaginationHookState = useMemo(() => {
    const totalPages = Math.ceil(totalCount / pageSize) || 1;
    
    return {
      currentPage: Math.min(currentPage, totalPages), // 현재 페이지가 총 페이지 수를 초과하지 않도록
      pageSize,
      totalCount,
      totalPages
    };
  }, [currentPage, pageSize, totalCount]);

  const hasNext = useMemo(() => {
    return state.currentPage < state.totalPages;
  }, [state.currentPage, state.totalPages]);

  const hasPrev = useMemo(() => {
    return state.currentPage > 1;
  }, [state.currentPage]);

  const startIndex = useMemo(() => {
    return (state.currentPage - 1) * state.pageSize + 1;
  }, [state.currentPage, state.pageSize]);

  const endIndex = useMemo(() => {
    return Math.min(state.currentPage * state.pageSize, state.totalCount);
  }, [state.currentPage, state.pageSize, state.totalCount]);

  // ==========================================================================
  // 페이지 번호 생성 (1 2 3 ... 8 9 10 형태)
  // ==========================================================================

  const getPageNumbers = useCallback((maxVisible: number = maxVisiblePages): number[] => {
    const { currentPage, totalPages } = state;
    
    if (totalPages <= maxVisible) {
      // 총 페이지가 최대 표시 개수보다 적으면 모든 페이지 표시
      return Array.from({ length: totalPages }, (_, i) => i + 1);
    }

    const half = Math.floor(maxVisible / 2);
    let start = Math.max(currentPage - half, 1);
    let end = Math.min(start + maxVisible - 1, totalPages);

    // 끝에서 부족한 부분을 앞에서 채우기
    if (end - start + 1 < maxVisible) {
      start = Math.max(end - maxVisible + 1, 1);
    }

    return Array.from({ length: end - start + 1 }, (_, i) => start + i);
  }, [state, maxVisiblePages]);

  // ==========================================================================
  // 액션 함수들
  // ==========================================================================

  const goToPage = useCallback((page: number) => {
    const validPage = Math.max(1, Math.min(page, state.totalPages));
    setCurrentPage(validPage);
  }, [state.totalPages]);

  const changePageSize = useCallback((size: number) => {
    // 페이지 크기가 변경되면 첫 페이지로 이동
    setPageSize(size);
    setCurrentPage(1);
  }, []);

  const goToFirst = useCallback(() => {
    setCurrentPage(1);
  }, []);

  const goToLast = useCallback(() => {
    setCurrentPage(state.totalPages);
  }, [state.totalPages]);

  const goToNext = useCallback(() => {
    if (hasNext) {
      setCurrentPage(prev => prev + 1);
    }
  }, [hasNext]);

  const goToPrev = useCallback(() => {
    if (hasPrev) {
      setCurrentPage(prev => prev - 1);
    }
  }, [hasPrev]);

  const reset = useCallback(() => {
    setCurrentPage(initialPage);
    setPageSize(initialPageSize);
  }, [initialPage, initialPageSize]);

  // ==========================================================================
  // 반환값
  // ==========================================================================

  return {
    // 상태
    ...state,
    
    // 계산된 값들
    hasNext,
    hasPrev,
    startIndex,
    endIndex,
    
    // 액션 함수들
    goToPage,
    changePageSize,
    goToFirst,
    goToLast,
    goToNext,
    goToPrev,
    reset,
    getPageNumbers
  };
};