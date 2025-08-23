// ============================================================================
// frontend/src/pages/VirtualPoints.tsx
// 가상포인트 메인 페이지 - 수정된 버전 (FormulaHelper 오류 해결)
// ============================================================================

import React, { useState } from 'react';
import { FilterPanel, FilterGroup } from '../components/common/FilterPanel';
import { Pagination } from '../components/common/Pagination';
import { useVirtualPoints } from '../hooks/virtualPoints/useVirtualPoints';

// ✅ named export로 되어 있는 컴포넌트들
import { VirtualPointCard } from '../components/VirtualPoints/VirtualPointCard';
import { VirtualPointTable } from '../components/VirtualPoints/VirtualPointTable';

// ✅ default export로 되어 있는 컴포넌트들
import VirtualPointModal from '../components/VirtualPoints/VirtualPointModal/VirtualPointModal';
import { FormulaHelper } from '../components/VirtualPoints/FormulaHelper/FormulaHelper';

import { useScriptEngine } from '../hooks/shared/useScriptEngine';
import { CATEGORY_OPTIONS, CALCULATION_TRIGGER_OPTIONS, SCOPE_TYPE_OPTIONS } from '../types/virtualPoints';
import '../styles/virtual-points.css';
import '../styles/filter-panel.css';
import '../components/VirtualPoints/FormulaHelper/FormulaHelper.css';


const VirtualPoints: React.FC = () => {
  // ========================================================================
  // 훅 사용
  // ========================================================================
  const virtualPointsHook = useVirtualPoints({
    autoLoad: true,
    pageSize: 12
  });

  const scriptEngine = useScriptEngine({
    autoLoadFunctions: true
  });

  const {
    // 상태
    virtualPoints,
    selectedPoint,
    categoryStats,
    performanceStats,
    loading,
    saving,
    testing,
    error,
    
    // 모달 상태
    showCreateModal,
    showEditModal,
    showDeleteConfirm,
    showTestModal,
    
    // UI 상태
    viewMode,
    filters,
    
    // 페이징
    paginatedData,
    
    // 액션들
    openCreateModal,
    openEditModal,
    openDeleteConfirm,
    openTestModal,
    closeModals,
    setViewMode,
    setFilters,
    setSorting,
    setCurrentPage,
    setPageSize,
    
    // CRUD 액션들
    createVirtualPoint,
    updateVirtualPoint,
    deleteVirtualPoint,
    toggleVirtualPointEnabled,
    executeVirtualPoint,
    testVirtualPoint
  } = virtualPointsHook;

  // FormulaHelper 상태 추가
  const [showFormulaHelper, setShowFormulaHelper] = useState(false);

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================

  const handleCreateVirtualPoint = async (data: any) => {
    try {
      await createVirtualPoint(data);
    } catch (err) {
      console.error('가상포인트 생성 실패:', err);
    }
  };

  const handleUpdateVirtualPoint = async (data: any) => {
    try {
      if (selectedPoint) {
        await updateVirtualPoint(selectedPoint.id, data);
      }
    } catch (err) {
      console.error('가상포인트 수정 실패:', err);
    }
  };

  const handleDeleteConfirm = async () => {
    try {
      if (selectedPoint) {
        await deleteVirtualPoint(selectedPoint.id);
        closeModals();
      }
    } catch (err) {
      console.error('가상포인트 삭제 실패:', err);
    }
  };

  const handleToggleEnabled = async (pointId: number) => {
    try {
      await toggleVirtualPointEnabled(pointId);
    } catch (err) {
      console.error('가상포인트 활성화 토글 실패:', err);
    }
  };

  const handleExecutePoint = async (pointId: number) => {
    try {
      await executeVirtualPoint(pointId);
    } catch (err) {
      console.error('가상포인트 실행 실패:', err);
    }
  };

  const handleTestScript = async (pointId: number, testData: any) => {
    try {
      const result = await testVirtualPoint(pointId, testData);
      console.log('테스트 결과:', result);
      return result;
    } catch (err) {
      console.error('스크립트 테스트 실패:', err);
      throw err;
    }
  };

  // FormulaHelper 핸들러들
  const handleShowFormulaHelper = () => {
    setShowFormulaHelper(true);
  };

  const handleInsertFunction = (functionCode: string) => {
    // 실제로는 현재 편집 중인 가상포인트의 수식에 함수를 삽입해야 함
    // 여기서는 단순히 콘솔에 로그만 출력
    console.log('함수 삽입:', functionCode);
    setShowFormulaHelper(false);
  };

  const handleFilterChange = (newFilters: any) => {
    setFilters(newFilters);
  };

  const handleSortChange = (sortBy: string, sortOrder: 'asc' | 'desc') => {
    setSorting(sortBy, sortOrder);
  };

  const handleViewModeChange = (mode: 'card' | 'table') => {
    setViewMode(mode);
  };

  const handlePageChange = (page: number, pageSize: number) => {
    setCurrentPage(page);
    if (pageSize !== paginatedData.pageSize) {
      setPageSize(pageSize);
    }
  };

  // ========================================================================
  // 필터 설정
  // ========================================================================

  const filterGroups: FilterGroup[] = [
    {
      key: 'search',
      type: 'search',
      label: '검색',
      placeholder: '이름, 설명, 수식 검색...',
      value: filters.search || '',
      className: 'flex-1'
    },
    {
      key: 'category',
      type: 'select',
      label: '카테고리',
      value: filters.category || 'all',
      options: [
        { value: 'all', label: '전체' },
        ...CATEGORY_OPTIONS
      ]
    },
    {
      key: 'calculation_status',
      type: 'select',
      label: '계산 상태',
      value: filters.calculation_status || 'all',
      options: [
        { value: 'all', label: '전체' },
        { value: 'success', label: '성공' },
        { value: 'error', label: '오류' },
        { value: 'timeout', label: '타임아웃' }
      ]
    },
    {
      key: 'scope_type',
      type: 'select',
      label: '범위',
      value: filters.scope_type || 'all',
      options: [
        { value: 'all', label: '전체' },
        ...SCOPE_TYPE_OPTIONS
      ]
    },
    {
      key: 'is_enabled',
      type: 'select',
      label: '상태',
      value: filters.is_enabled !== undefined ? String(filters.is_enabled) : 'all',
      options: [
        { value: 'all', label: '전체' },
        { value: 'true', label: '활성' },
        { value: 'false', label: '비활성' }
      ]
    }
  ];

  // ========================================================================
  // 정렬 옵션
  // ========================================================================

  const sortOptions = [
    { value: 'name', label: '이름' },
    { value: 'category', label: '카테고리' },
    { value: 'updated_at', label: '수정일' },
    { value: 'calculation_status', label: '계산 상태' },
    { value: 'priority', label: '우선순위' }
  ];

  // ========================================================================
  // 렌더링
  // ========================================================================

  if (loading && virtualPoints.length === 0) {
    return (
      <div className="virtual-points-container">
        <div className="loading-container">
          <i className="fas fa-spinner fa-spin"></i>
          <p>가상포인트를 불러오는 중...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="virtual-points-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-function"></i>
            가상포인트 관리
          </h1>
          <p className="page-subtitle">
            실시간 데이터를 기반으로 계산되는 가상포인트를 관리합니다.
          </p>
        </div>
        
        <div className="header-actions">
          <button
            className="btn-secondary"
            onClick={handleShowFormulaHelper}
          >
            <i className="fas fa-calculator"></i>
            함수 도우미
          </button>
          <button
            className="btn-primary"
            onClick={openCreateModal}
          >
            <i className="fas fa-plus"></i>
            새 가상포인트
          </button>
        </div>
      </div>

      {/* 필터 패널 */}
      <FilterPanel
        filterGroups={filterGroups}
        onFiltersChange={handleFilterChange}
        sortOptions={sortOptions}
        currentSort={{
          sortBy: paginatedData.sortBy,
          sortOrder: paginatedData.sortOrder
        }}
        onSortChange={handleSortChange}
        viewMode={viewMode}
        onViewModeChange={handleViewModeChange}
      />

      {/* 통계 정보 */}
      <div className="stats-panel">
        <div className="stat-card">
          <div className="stat-value">{categoryStats?.total || 0}</div>
          <div className="stat-label">전체 가상포인트</div>
        </div>
        <div className="stat-card status-active">
          <div className="stat-value">{categoryStats?.active || 0}</div>
          <div className="stat-label">활성</div>
        </div>
        <div className="stat-card status-disabled">
          <div className="stat-value">{categoryStats?.disabled || 0}</div>
          <div className="stat-label">비활성</div>
        </div>
        <div className="stat-card status-error">
          <div className="stat-value">{categoryStats?.error || 0}</div>
          <div className="stat-label">오류</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{paginatedData.items.length}</div>
          <div className="stat-label">필터 결과</div>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="error-banner">
          <i className="fas fa-exclamation-triangle"></i>
          <span>{error}</span>
          <button onClick={() => window.location.reload()}>
            <i className="fas fa-sync"></i>
            새로고침
          </button>
        </div>
      )}

      {/* 가상포인트 목록 */}
      <div className="points-list">
        {paginatedData.items.length === 0 ? (
          <div className="empty-state">
            <i className="fas fa-function"></i>
            <h3>가상포인트가 없습니다</h3>
            <p>첫 번째 가상포인트를 생성해보세요.</p>
            <button className="btn-primary" onClick={openCreateModal}>
              <i className="fas fa-plus"></i>
              가상포인트 생성
            </button>
          </div>
        ) : viewMode === 'card' ? (
          <div className="points-grid">
            {paginatedData.items.map(point => (
              <VirtualPointCard
                key={point.id}
                virtualPoint={point}
                onEdit={openEditModal}
                onDelete={openDeleteConfirm}
                onTest={openTestModal}
                onExecute={handleExecutePoint}
                onToggleEnabled={handleToggleEnabled}
              />
            ))}
          </div>
        ) : (
          <VirtualPointTable
            virtualPoints={paginatedData.items}
            onEdit={openEditModal}
            onDelete={openDeleteConfirm}
            onTest={openTestModal}
            onExecute={handleExecutePoint}
            onToggleEnabled={handleToggleEnabled}
            sortBy={paginatedData.sortBy}
            sortOrder={paginatedData.sortOrder}
            onSort={handleSortChange}
          />
        )}
      </div>

      {/* 페이징 */}
      {paginatedData.totalCount > 0 && (
        <div className="pagination-section">
          <Pagination
            current={paginatedData.currentPage}
            total={paginatedData.totalCount}
            pageSize={paginatedData.pageSize}
            pageSizeOptions={[6, 12, 24, 48]}
            showSizeChanger={true}
            showTotal={true}
            onChange={handlePageChange}
            onShowSizeChange={handlePageChange}
          />
        </div>
      )}

      {/* 가상포인트 생성 모달 */}
      {showCreateModal && (
        <VirtualPointModal
          isOpen={showCreateModal}
          mode="create"
          onSave={handleCreateVirtualPoint}
          onClose={closeModals}
          loading={saving}
        />
      )}

      {/* 가상포인트 편집 모달 */}
      {showEditModal && selectedPoint && (
        <VirtualPointModal
          isOpen={showEditModal}
          mode="edit"
          virtualPoint={selectedPoint}
          onSave={handleUpdateVirtualPoint}
          onClose={closeModals}
          loading={saving}
        />
      )}

      {/* 삭제 확인 모달 */}
      {showDeleteConfirm && selectedPoint && (
        <div className="modal-overlay" onClick={closeModals}>
          <div className="modal-container" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h3>
                <i className="fas fa-trash"></i>
                가상포인트 삭제
              </h3>
              <button className="modal-close-btn" onClick={closeModals}>
                <i className="fas fa-times"></i>
              </button>
            </div>
            
            <div className="modal-content">
              <div className="delete-confirmation">
                <div className="warning-icon">
                  <i className="fas fa-exclamation-triangle"></i>
                </div>
                <h4>정말로 이 가상포인트를 삭제하시겠습니까?</h4>
                <p><strong>{selectedPoint.name}</strong></p>
                <p className="delete-warning">
                  이 작업은 되돌릴 수 없으며, 관련된 모든 데이터가 함께 삭제됩니다.
                </p>
              </div>
            </div>
            
            <div className="modal-footer">
              <button
                className="btn-secondary"
                onClick={closeModals}
                disabled={saving}
              >
                취소
              </button>
              <button
                className="btn-danger"
                onClick={handleDeleteConfirm}
                disabled={saving}
              >
                {saving ? (
                  <>
                    <i className="fas fa-spinner fa-spin"></i>
                    삭제 중...
                  </>
                ) : (
                  <>
                    <i className="fas fa-trash"></i>
                    삭제
                  </>
                )}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* FormulaHelper 모달 */}
      {showFormulaHelper && (
        <FormulaHelper
          isOpen={showFormulaHelper}
          onClose={() => setShowFormulaHelper(false)}
          onInsertFunction={handleInsertFunction}
          functions={scriptEngine.functions}
        />
      )}
    </div>
  );
};

export default VirtualPoints;