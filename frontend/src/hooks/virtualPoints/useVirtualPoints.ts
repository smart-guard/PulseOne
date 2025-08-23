// ============================================================================
// frontend/src/hooks/virtualPoints/useVirtualPoints.ts
// 가상포인트 관련 상태 및 로직 관리 훅
// ============================================================================

import { useState, useEffect, useCallback } from 'react';
import { virtualPointsApi } from '../../api/services/virtualPointsApi';
import {
  VirtualPoint,
  VirtualPointFormData,
  VirtualPointInputFormData,
  VirtualPointFilters,
  VirtualPointPageState
} from '../../types/virtualPoints';

interface UseVirtualPointsOptions {
  autoLoad?: boolean;
  initialFilters?: VirtualPointFilters;
  pageSize?: number;
}

export const useVirtualPoints = (options: UseVirtualPointsOptions = {}) => {
  const { autoLoad = true, initialFilters = {}, pageSize = 10 } = options;

  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [state, setState] = useState<VirtualPointPageState>({
    // 데이터
    virtualPoints: [],
    selectedPoint: null,
    categoryStats: [],
    performanceStats: null,
    
    // UI 상태
    loading: false,
    saving: false,
    testing: false,
    error: null,
    
    // 모달 상태
    showCreateModal: false,
    showEditModal: false,
    showDeleteConfirm: false,
    showTestModal: false,
    
    // 필터 상태
    filters: initialFilters,
    viewMode: 'card',
    sortBy: 'name',
    sortOrder: 'asc',
    
    // 페이징
    currentPage: 1,
    pageSize,
    totalCount: 0
  });

  // ========================================================================
  // 데이터 로딩 함수들
  // ========================================================================
  
  const loadVirtualPoints = useCallback(async (filters?: VirtualPointFilters) => {
    setState(prev => ({ ...prev, loading: true, error: null }));
    
    try {
      console.log('가상포인트 목록 로딩 시작');
      
      const mergedFilters = { ...state.filters, ...filters };
      const data = await virtualPointsApi.getVirtualPoints(mergedFilters);
      
      setState(prev => ({
        ...prev,
        virtualPoints: data,
        totalCount: data.length,
        loading: false,
        filters: mergedFilters
      }));
      
      console.log(`${data.length}개 가상포인트 로드 완료`);
      
    } catch (error) {
      console.error('가상포인트 로딩 실패:', error);
      setState(prev => ({
        ...prev,
        loading: false,
        error: error instanceof Error ? error.message : '데이터 로딩 실패'
      }));
    }
  }, [state.filters]);

  const loadCategoryStats = useCallback(async () => {
    try {
      const stats = await virtualPointsApi.getCategoryStats();
      setState(prev => ({ ...prev, categoryStats: stats }));
    } catch (error) {
      console.error('카테고리 통계 로딩 실패:', error);
    }
  }, []);

  const loadPerformanceStats = useCallback(async () => {
    try {
      const stats = await virtualPointsApi.getPerformanceStats();
      setState(prev => ({ ...prev, performanceStats: stats }));
    } catch (error) {
      console.error('성능 통계 로딩 실패:', error);
    }
  }, []);

  const refreshData = useCallback(async () => {
    await Promise.all([
      loadVirtualPoints(),
      loadCategoryStats(),
      loadPerformanceStats()
    ]);
  }, [loadVirtualPoints, loadCategoryStats, loadPerformanceStats]);

  // ========================================================================
  // CRUD 작업
  // ========================================================================
  
  const createVirtualPoint = useCallback(async (
    formData: VirtualPointFormData,
    inputs: VirtualPointInputFormData[] = []
  ) => {
    setState(prev => ({ ...prev, saving: true, error: null }));
    
    try {
      console.log('가상포인트 생성 시작:', formData);
      
      const newPoint = await virtualPointsApi.createVirtualPoint(formData, inputs);
      
      // 목록에 추가
      setState(prev => ({
        ...prev,
        virtualPoints: [...prev.virtualPoints, newPoint],
        totalCount: prev.totalCount + 1,
        saving: false,
        showCreateModal: false
      }));
      
      console.log('가상포인트 생성 완료:', newPoint.id);
      return newPoint;
      
    } catch (error) {
      console.error('가상포인트 생성 실패:', error);
      setState(prev => ({
        ...prev,
        saving: false,
        error: error instanceof Error ? error.message : '생성 실패'
      }));
      throw error;
    }
  }, []);

  const updateVirtualPoint = useCallback(async (
    id: number,
    formData: Partial<VirtualPointFormData>,
    inputs?: VirtualPointInputFormData[]
  ) => {
    setState(prev => ({ ...prev, saving: true, error: null }));
    
    try {
      console.log('가상포인트 수정 시작:', id);
      
      const updatedPoint = await virtualPointsApi.updateVirtualPoint(id, formData, inputs);
      
      // 목록에서 업데이트
      setState(prev => ({
        ...prev,
        virtualPoints: prev.virtualPoints.map(point => 
          point.id === id ? updatedPoint : point
        ),
        selectedPoint: prev.selectedPoint?.id === id ? updatedPoint : prev.selectedPoint,
        saving: false,
        showEditModal: false
      }));
      
      console.log('가상포인트 수정 완료');
      return updatedPoint;
      
    } catch (error) {
      console.error('가상포인트 수정 실패:', error);
      setState(prev => ({
        ...prev,
        saving: false,
        error: error instanceof Error ? error.message : '수정 실패'
      }));
      throw error;
    }
  }, []);

  const deleteVirtualPoint = useCallback(async (id: number) => {
    setState(prev => ({ ...prev, saving: true, error: null }));
    
    try {
      console.log('가상포인트 삭제 시작:', id);
      
      await virtualPointsApi.deleteVirtualPoint(id);
      
      // 목록에서 제거
      setState(prev => ({
        ...prev,
        virtualPoints: prev.virtualPoints.filter(point => point.id !== id),
        totalCount: prev.totalCount - 1,
        selectedPoint: prev.selectedPoint?.id === id ? null : prev.selectedPoint,
        saving: false,
        showDeleteConfirm: false
      }));
      
      console.log('가상포인트 삭제 완료');
      return true;
      
    } catch (error) {
      console.error('가상포인트 삭제 실패:', error);
      setState(prev => ({
        ...prev,
        saving: false,
        error: error instanceof Error ? error.message : '삭제 실패'
      }));
      throw error;
    }
  }, []);

  // ========================================================================
  // 테스트 및 실행
  // ========================================================================
  
  const testScript = useCallback(async (expression: string, variables: Record<string, any>) => {
    setState(prev => ({ ...prev, testing: true, error: null }));
    
    try {
      console.log('스크립트 테스트 시작');
      
      const result = await virtualPointsApi.testScript({
        expression,
        variables,
        data_type: 'number' // 기본값
      });
      
      setState(prev => ({ ...prev, testing: false }));
      
      console.log('스크립트 테스트 완료:', result);
      return result;
      
    } catch (error) {
      console.error('스크립트 테스트 실패:', error);
      setState(prev => ({
        ...prev,
        testing: false,
        error: error instanceof Error ? error.message : '테스트 실패'
      }));
      throw error;
    }
  }, []);

  const executeVirtualPoint = useCallback(async (id: number) => {
    setState(prev => ({ ...prev, testing: true, error: null }));
    
    try {
      console.log('가상포인트 실행 시작:', id);
      
      const result = await virtualPointsApi.executeVirtualPoint(id);
      
      // 해당 포인트의 값 업데이트
      if (result.success && result.value !== undefined) {
        setState(prev => ({
          ...prev,
          virtualPoints: prev.virtualPoints.map(point =>
            point.id === id 
              ? { ...point, current_value: result.value, last_calculated: new Date().toISOString() }
              : point
          ),
          testing: false
        }));
      } else {
        setState(prev => ({ ...prev, testing: false }));
      }
      
      console.log('가상포인트 실행 완료');
      return result;
      
    } catch (error) {
      console.error('가상포인트 실행 실패:', error);
      setState(prev => ({
        ...prev,
        testing: false,
        error: error instanceof Error ? error.message : '실행 실패'
      }));
      throw error;
    }
  }, []);

  // ========================================================================
  // UI 상태 관리
  // ========================================================================
  
  const setFilters = useCallback((filters: Partial<VirtualPointFilters>) => {
    setState(prev => ({
      ...prev,
      filters: { ...prev.filters, ...filters },
      currentPage: 1 // 필터 변경 시 첫 페이지로 이동
    }));
  }, []);

  const setViewMode = useCallback((viewMode: 'card' | 'table') => {
    setState(prev => ({ ...prev, viewMode }));
  }, []);

  const setSorting = useCallback((sortBy: string, sortOrder: 'asc' | 'desc') => {
    setState(prev => ({ ...prev, sortBy, sortOrder }));
  }, []);

  const setSelectedPoint = useCallback((point: VirtualPoint | null) => {
    setState(prev => ({ ...prev, selectedPoint: point }));
  }, []);

  const setCurrentPage = useCallback((page: number) => {
    setState(prev => ({ ...prev, currentPage: page }));
  }, []);

  const setPageSize = useCallback((size: number) => {
    setState(prev => ({ 
      ...prev, 
      pageSize: size,
      currentPage: 1 // 페이지 크기 변경 시 첫 페이지로 이동
    }));
  }, []);

  // ========================================================================
  // 모달 상태 관리
  // ========================================================================
  
  const openCreateModal = useCallback(() => {
    setState(prev => ({ ...prev, showCreateModal: true, selectedPoint: null }));
  }, []);

  const openEditModal = useCallback((point: VirtualPoint) => {
    setState(prev => ({ ...prev, showEditModal: true, selectedPoint: point }));
  }, []);

  const openDeleteConfirm = useCallback((point: VirtualPoint) => {
    setState(prev => ({ ...prev, showDeleteConfirm: true, selectedPoint: point }));
  }, []);

  const openTestModal = useCallback((point: VirtualPoint) => {
    setState(prev => ({ ...prev, showTestModal: true, selectedPoint: point }));
  }, []);

  const closeModals = useCallback(() => {
    setState(prev => ({
      ...prev,
      showCreateModal: false,
      showEditModal: false,
      showDeleteConfirm: false,
      showTestModal: false,
      selectedPoint: null
    }));
  }, []);

  const clearError = useCallback(() => {
    setState(prev => ({ ...prev, error: null }));
  }, []);

  // ========================================================================
  // 계산된 값들
  // ========================================================================
  
  const filteredAndSortedPoints = useCallback(() => {
    let points = [...state.virtualPoints];

    // 필터 적용
    const { search, category, calculation_status, scope_type, is_enabled } = state.filters;

    if (search) {
      const searchLower = search.toLowerCase();
      points = points.filter(point =>
        point.name.toLowerCase().includes(searchLower) ||
        point.description?.toLowerCase().includes(searchLower) ||
        point.expression.toLowerCase().includes(searchLower) ||
        point.tags?.some(tag => tag.toLowerCase().includes(searchLower))
      );
    }

    if (category && category !== 'all') {
      points = points.filter(point => point.category === category);
    }

    if (calculation_status && calculation_status !== 'all') {
      points = points.filter(point => point.calculation_status === calculation_status);
    }

    if (scope_type && scope_type !== 'all') {
      points = points.filter(point => point.scope_type === scope_type);
    }

    if (is_enabled !== undefined) {
      points = points.filter(point => point.is_enabled === is_enabled);
    }

    // 정렬 적용
    points.sort((a, b) => {
      let comparison = 0;
      
      switch (state.sortBy) {
        case 'name':
          comparison = a.name.localeCompare(b.name);
          break;
        case 'category':
          comparison = a.category.localeCompare(b.category);
          break;
        case 'updated_at':
          comparison = new Date(a.updated_at).getTime() - new Date(b.updated_at).getTime();
          break;
        case 'calculation_status':
          comparison = a.calculation_status.localeCompare(b.calculation_status);
          break;
        case 'priority':
          comparison = (a.priority || 0) - (b.priority || 0);
          break;
        default:
          comparison = 0;
      }
      
      return state.sortOrder === 'asc' ? comparison : -comparison;
    });

    return points;
  }, [state.virtualPoints, state.filters, state.sortBy, state.sortOrder]);

  const paginatedPoints = useCallback(() => {
    const allPoints = filteredAndSortedPoints();
    const startIndex = (state.currentPage - 1) * state.pageSize;
    const endIndex = startIndex + state.pageSize;
    
    return {
      points: allPoints.slice(startIndex, endIndex),
      totalCount: allPoints.length,
      totalPages: Math.ceil(allPoints.length / state.pageSize),
      startIndex: startIndex + 1,
      endIndex: Math.min(endIndex, allPoints.length)
    };
  }, [filteredAndSortedPoints, state.currentPage, state.pageSize]);

  // ========================================================================
  // 초기 로딩
  // ========================================================================
  
  useEffect(() => {
    if (autoLoad) {
      refreshData();
    }
  }, [autoLoad, refreshData]);

  // 필터 변경 시 데이터 다시 로드
  useEffect(() => {
    if (autoLoad) {
      loadVirtualPoints();
    }
  }, [state.filters, autoLoad, loadVirtualPoints]);

  // ========================================================================
  // 반환값
  // ========================================================================
  
  return {
    // 상태
    ...state,
    
    // 계산된 값들
    filteredPoints: filteredAndSortedPoints(),
    paginatedData: paginatedPoints(),
    
    // 데이터 로딩
    loadVirtualPoints,
    loadCategoryStats, 
    loadPerformanceStats,
    refreshData,
    
    // CRUD 작업
    createVirtualPoint,
    updateVirtualPoint,
    deleteVirtualPoint,
    
    // 테스트 및 실행
    testScript,
    executeVirtualPoint,
    
    // UI 상태 관리
    setFilters,
    setViewMode,
    setSorting,
    setSelectedPoint,
    setCurrentPage,
    setPageSize,
    
    // 모달 관리
    openCreateModal,
    openEditModal,
    openDeleteConfirm,
    openTestModal,
    closeModals,
    clearError
  };
};