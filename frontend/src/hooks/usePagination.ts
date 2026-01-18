// ============================================================================
// frontend/src/hooks/usePagination.ts
// 📄 페이지네이션 로직 전용 커스텀 훅 - 완성본
// ============================================================================

import { useState, useMemo, useCallback, useEffect } from 'react';
import {
  PaginationHookReturn,
  PaginationHookState
} from '../types/common';
import { DEFAULT_PAGINATION_CONFIG } from '../constants/pagination';

// ============================================================================
// 📋 인터페이스 정의
// ============================================================================

interface UsePaginationOptions {
  initialPage?: number;
  initialPageSize?: number;
  totalCount?: number;
  pageSizeOptions?: number[];
  maxVisiblePages?: number;
  enableUrlSync?: boolean; // URL 동기화 지원
  enableLocalStorage?: boolean; // 로컬 스토리지 저장
  storageKey?: string; // 로컬 스토리지 키
  onPageChange?: (page: number, pageSize: number) => void; // 페이지 변경 콜백
  onPageSizeChange?: (pageSize: number, page: number) => void; // 페이지 크기 변경 콜백
}

interface PaginationStorage {
  currentPage: number;
  pageSize: number;
  timestamp: number;
}

// ============================================================================
// 🎯 메인 훅 구현
// ============================================================================

export const usePagination = (options: UsePaginationOptions = {}): PaginationHookReturn => {
  const {
    initialPage = 1,
    initialPageSize = DEFAULT_PAGINATION_CONFIG.defaultPageSize,
    totalCount = 0,
    pageSizeOptions = DEFAULT_PAGINATION_CONFIG.pageSizeOptions,
    maxVisiblePages = DEFAULT_PAGINATION_CONFIG.maxVisiblePages,
    enableUrlSync = false,
    enableLocalStorage = false,
    storageKey = 'pagination',
    onPageChange,
    onPageSizeChange
  } = options;

  // ==========================================================================
  // 🔧 로컬 스토리지 헬퍼 함수들
  // ==========================================================================

  const getStoredPagination = useCallback((): PaginationStorage | null => {
    if (!enableLocalStorage || typeof window === 'undefined') return null;

    try {
      const stored = localStorage.getItem(storageKey);
      if (!stored) return null;

      const parsed = JSON.parse(stored) as PaginationStorage;

      // 24시간 이내 데이터만 유효
      const isValid = (Date.now() - parsed.timestamp) < 24 * 60 * 60 * 1000;

      return isValid ? parsed : null;
    } catch {
      return null;
    }
  }, [enableLocalStorage, storageKey]);

  const savePaginationToStorage = useCallback((page: number, size: number) => {
    if (!enableLocalStorage || typeof window === 'undefined') return;

    try {
      const data: PaginationStorage = {
        currentPage: page,
        pageSize: size,
        timestamp: Date.now()
      };

      localStorage.setItem(storageKey, JSON.stringify(data));
    } catch {
      // 로컬 스토리지 저장 실패 시 무시
    }
  }, [enableLocalStorage, storageKey]);

  // ==========================================================================
  // 📊 상태 관리 (로컬 스토리지 복원 포함)
  // ==========================================================================

  const storedData = getStoredPagination();

  const [currentPage, setCurrentPage] = useState<number>(
    storedData?.currentPage ?? initialPage
  );
  const [pageSize, setPageSize] = useState<number>(
    storedData?.pageSize ?? initialPageSize
  );
  const [totalCountState, setTotalCountState] = useState<number>(totalCount);

  // ==========================================================================
  // 🧮 계산된 값들
  // ==========================================================================

  const state: PaginationHookState = useMemo(() => {
    const totalPages = Math.ceil(totalCountState / pageSize) || 1;

    return {
      currentPage: Math.min(Math.max(currentPage, 1), totalPages),
      pageSize,
      totalCount: totalCountState,
      totalPages
    };
  }, [currentPage, pageSize, totalCountState]);

  const hasNext = useMemo(() => {
    return state.currentPage < state.totalPages;
  }, [state.currentPage, state.totalPages]);

  const hasPrev = useMemo(() => {
    return state.currentPage > 1;
  }, [state.currentPage]);

  const startIndex = useMemo(() => {
    if (state.totalCount === 0) return 0;
    return (state.currentPage - 1) * state.pageSize + 1;
  }, [state.currentPage, state.pageSize, state.totalCount]);

  const endIndex = useMemo(() => {
    if (state.totalCount === 0) return 0;
    return Math.min(state.currentPage * state.pageSize, state.totalCount);
  }, [state.currentPage, state.pageSize, state.totalCount]);

  // ==========================================================================
  // 📄 페이지 번호 생성 (개선된 로직)
  // ==========================================================================

  const getPageNumbers = useCallback((maxVisible: number = maxVisiblePages): number[] => {
    const { currentPage, totalPages } = state;

    if (totalPages <= 1) return [1];

    if (totalPages <= maxVisible) {
      return Array.from({ length: totalPages }, (_, i) => i + 1);
    }

    const half = Math.floor(maxVisible / 2);
    let start = Math.max(currentPage - half, 1);
    let end = Math.min(start + maxVisible - 1, totalPages);

    // 끝쪽에서 조정
    if (end - start + 1 < maxVisible) {
      start = Math.max(end - maxVisible + 1, 1);
    }

    return Array.from({ length: end - start + 1 }, (_, i) => start + i);
  }, [state, maxVisiblePages]);

  // 페이지 정보 생성 (ellipsis 포함)
  const getPageInfo = useCallback(() => {
    const { currentPage, totalPages } = state;
    const pageNumbers = getPageNumbers();

    const showFirstEllipsis = pageNumbers[0] > 2;
    const showLastEllipsis = pageNumbers[pageNumbers.length - 1] < totalPages - 1;

    return {
      pages: pageNumbers,
      showFirstEllipsis,
      showLastEllipsis,
      showFirstPage: pageNumbers[0] > 1,
      showLastPage: pageNumbers[pageNumbers.length - 1] < totalPages
    };
  }, [state, getPageNumbers]);

  // ==========================================================================
  // 🎮 액션 함수들 (개선된 버전)
  // ==========================================================================

  const goToPage = useCallback((page: number) => {
    const validPage = Math.max(1, Math.min(page, state.totalPages));

    if (validPage === currentPage) return;

    setCurrentPage(validPage);
    savePaginationToStorage(validPage, pageSize);
    onPageChange?.(validPage, pageSize);
  }, [state.totalPages, currentPage, pageSize, savePaginationToStorage, onPageChange]);

  const changePageSize = useCallback((size: number) => {
    // 페이지 크기 유효성 검사
    const validSize = Math.max(1, Math.min(size, 1000));

    if (validSize === pageSize) return;

    // 현재 항목의 인덱스를 유지하면서 페이지 이동
    const currentItemIndex = (currentPage - 1) * pageSize;
    const newPage = Math.floor(currentItemIndex / validSize) + 1;
    const finalPage = Math.max(1, Math.min(newPage, Math.ceil(totalCountState / validSize) || 1));

    setPageSize(validSize);
    setCurrentPage(finalPage);
    savePaginationToStorage(finalPage, validSize);
    onPageSizeChange?.(validSize, finalPage);
  }, [pageSize, currentPage, totalCountState, savePaginationToStorage, onPageSizeChange]);

  // 🔥 핵심: updateTotalCount 메서드 추가
  const updateTotalCount = useCallback((newTotal: number) => {
    const validTotal = Math.max(0, newTotal);
    setTotalCountState(validTotal);

    // totalCount 변경으로 인해 현재 페이지가 범위를 벗어나는 경우 조정
    const newTotalPages = Math.ceil(validTotal / pageSize) || 1;
    if (currentPage > newTotalPages) {
      const adjustedPage = Math.max(1, newTotalPages);
      setCurrentPage(adjustedPage);
      savePaginationToStorage(adjustedPage, pageSize);
      onPageChange?.(adjustedPage, pageSize);
    }
  }, [pageSize, currentPage, savePaginationToStorage, onPageChange]);

  const goToFirst = useCallback(() => {
    goToPage(1);
  }, [goToPage]);

  const goToLast = useCallback(() => {
    goToPage(state.totalPages);
  }, [goToPage, state.totalPages]);

  const goToNext = useCallback(() => {
    if (hasNext) {
      goToPage(currentPage + 1);
    }
  }, [hasNext, currentPage, goToPage]);

  const goToPrev = useCallback(() => {
    if (hasPrev) {
      goToPage(currentPage - 1);
    }
  }, [hasPrev, currentPage, goToPage]);

  const reset = useCallback(() => {
    setCurrentPage(initialPage);
    setPageSize(initialPageSize);
    setTotalCountState(totalCount);

    // 로컬 스토리지도 초기화
    if (enableLocalStorage && typeof window !== 'undefined') {
      try {
        localStorage.removeItem(storageKey);
      } catch {
        // 무시
      }
    }

    onPageChange?.(initialPage, initialPageSize);
  }, [initialPage, initialPageSize, totalCount, enableLocalStorage, storageKey, onPageChange]);

  // ==========================================================================
  // 🔄 사이드 이펙트 (URL 동기화 등)
  // ==========================================================================

  // URL 동기화 (옵션)
  useEffect(() => {
    if (!enableUrlSync || typeof window === 'undefined') return;

    const url = new URL(window.location.href);
    url.searchParams.set('page', currentPage.toString());
    url.searchParams.set('pageSize', pageSize.toString());

    window.history.replaceState({}, '', url.toString());
  }, [enableUrlSync, currentPage, pageSize]);

  // totalCount 초기값 변경 감지 - 제거 (updateTotalCount가 주된 업데이트 수단임)
  /*
  useEffect(() => {
    if (totalCount !== totalCountState) {
      updateTotalCount(totalCount);
    }
  }, [totalCount, totalCountState, updateTotalCount]);
  */

  // ==========================================================================
  // 📤 반환값 (확장된 버전)
  // ==========================================================================

  return {
    // 기본 상태
    ...state,

    // 계산된 값들
    hasNext,
    hasPrev,
    startIndex,
    endIndex,

    // 기본 액션들
    goToPage,
    changePageSize,
    updateTotalCount, // 🔥 핵심 메서드
    goToFirst,
    goToLast,
    goToNext,
    goToPrev,
    reset,

    // 고급 기능들
    getPageNumbers,
    getPageInfo,

    // 메타 정보
    isEmpty: state.totalCount === 0,
    isFirstPage: state.currentPage === 1,
    isLastPage: state.currentPage === state.totalPages,

    // 설정값들
    pageSizeOptions: pageSizeOptions as number[],
    maxVisiblePages
  };
};

// ============================================================================
// 🎯 편의 훅들
// ============================================================================

/**
 * 디바이스 목록 전용 페이징 훅
 */
export const useDevicePagination = (totalCount: number = 0) => {
  return usePagination({
    initialPageSize: 25,
    totalCount,
    pageSizeOptions: [10, 25, 50, 100],
    maxVisiblePages: 7,
    enableLocalStorage: true,
    storageKey: 'device-pagination'
  });
};

/**
 * 알람 목록 전용 페이징 훅
 */
export const useAlarmPagination = (totalCount: number = 0) => {
  return usePagination({
    initialPageSize: 10,
    totalCount,
    pageSizeOptions: [10, 25, 50, 100],
    maxVisiblePages: 5,
    enableLocalStorage: true,
    storageKey: 'alarm-pagination'
  });
};

/**
 * 데이터 탐색 전용 페이징 훅
 */
export const useDataExplorerPagination = (totalCount: number = 0) => {
  return usePagination({
    initialPageSize: 50,
    totalCount,
    pageSizeOptions: [25, 50, 100, 200, 500],
    maxVisiblePages: 5,
    enableLocalStorage: true,
    storageKey: 'data-explorer-pagination'
  });
};


/**
 * 사용자 관리 전용 페이징 훅
 */
export const useUserPagination = (totalCount: number = 0) => {
  return usePagination({
    initialPageSize: 20,
    totalCount,
    pageSizeOptions: [10, 20, 50, 100],
    maxVisiblePages: 5,
    enableLocalStorage: true,
    storageKey: 'user-pagination'
  });
};

// ============================================================================
// 📊 타입 정의 확장 (PaginationHookReturn에 추가될 속성들)
// ============================================================================

declare module '../types/common' {
  interface PaginationHookReturn {
    updateTotalCount: (newTotal: number) => void;
    getPageInfo: () => {
      pages: number[];
      showFirstEllipsis: boolean;
      showLastEllipsis: boolean;
      showFirstPage: boolean;
      showLastPage: boolean;
    };
    isEmpty: boolean;
    isFirstPage: boolean;
    isLastPage: boolean;
    pageSizeOptions: number[];
    maxVisiblePages: number;
  }
}

export default usePagination;