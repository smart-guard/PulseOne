// ============================================================================
// frontend/src/pages/VirtualPoints.tsx
// 가상포인트 메인 페이지 - 완전 리팩토링된 버전
// ============================================================================

import React from 'react';
import { FilterPanel, FilterGroup } from '../components/common/FilterPanel';
import { Pagination } from '../components/common/Pagination';
import { useVirtualPoints } from '../hooks/virtualPoints/useVirtualPoints';
import { VirtualPointCard } from '../components/VirtualPoints/VirtualPointCard';
import { VirtualPointTable } from '../components/VirtualPoints/VirtualPointTable';
import { VirtualPointModal } from '../components/VirtualPoints/VirtualPointModal/VirtualPointModal';
import { FormulaHelper } from '../components/VirtualPoints/FormulaHelper/FormulaHelper';
import { useScriptEngine } from '../hooks/shared/useScriptEngine';
import { CATEGORY_OPTIONS, CALCULATION_TRIGGER_OPTIONS, SCOPE_TYPE_OPTIONS } from '../types/virtualPoints';
import '../styles/virtual-points.css';
import '../styles/filter-panel.css';

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
    createVirtualPoint,
    updateVirtualPoint,
    deleteVirtualPoint,
    executeVirtualPoint,
    setFilters,
    setViewMode,
    setCurrentPage,
    clearError,
    refreshData
  } = virtualPointsHook;

  // ========================================================================
  // 설정된 필터 그룹
  // ========================================================================
  const filterGroups: FilterGroup[] = [
    {
      id: 'search',
      label: '검색',
      type: 'search',
      value: filters.search || '',
      placeholder: '이름, 설명, 수식, 태그 검색...',
      className: 'flex-1'
    },
    {
      id: 'category',
      label: '카테고리',
      type: 'select',
      value: filters.category || 'all',
      options: [
        ...CATEGORY_OPTIONS.map(cat => ({
          value: cat,
          label: cat,
          count: categoryStats.find(s => s.category === cat)?.count
        }))
      ]
    },
    {
      id: 'calculation_status',
      label: '상태',
      type: 'select',
      value: filters.calculation_status || 'all',
      options: [
        { value: 'active', label: '활성', count: categoryStats.reduce((sum, s) => sum + s.enabled_count, 0) },
        { value: 'error', label: '오류', count: categoryStats.reduce((sum, s) => sum + s.error_count, 0) },
        { value: 'disabled', label: '비활성', count: categoryStats.reduce((sum, s) => sum + s.disabled_count, 0) }
      ]
    },
    {
      id: 'scope_type',
      label: '범위',
      type: 'select',
      value: filters.scope_type || 'all',
      options: SCOPE_TYPE_OPTIONS.map(option => ({
        value: option.value,
        label: option.label
      }))
    },
    {
      id: 'calculation_trigger',
      label: '실행 방식',
      type: 'select', 
      value: filters.calculation_trigger || 'all',
      options: CALCULATION_TRIGGER_OPTIONS.map(option => ({
        value: option.value,
        label: option.label
      }))
    },
    {
      id: 'is_enabled',
      label: '활성화 상태',
      type: 'toggle',
      value: filters.is_enabled
    }
  ];

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================
  const handleFiltersChange = (newFilters: Record<string, any>) => {
    setFilters(newFilters);
  };

  const handleCreateSuccess = async (formData: any, inputs: any[]) => {
    try {
      await createVirtualPoint(formData, inputs);
    } catch (error) {
      console.error('가상포인트 생성 실패:', error);
    }
  };

  const handleUpdateSuccess = async (formData: any, inputs: any[]) => {
    if (!selectedPoint) return;
    
    try {
      await updateVirtualPoint(selectedPoint.id, formData, inputs);
    } catch (error) {
      console.error('가상포인트 수정 실패:', error);
    }
  };

  const handleDeleteConfirm = async () => {
    if (!selectedPoint) return;
    
    try {
      await deleteVirtualPoint(selectedPoint.id);
    } catch (error) {
      console.error('가상포인트 삭제 실패:', error);
    }
  };

  const handleTestScript = async (expression: string, variables: Record<string, any>) => {
    try {
      return await scriptEngine.testScript(expression, variables);
    } catch (error) {
      console.error('스크립트 테스트 실패:', error);
      throw error;
    }
  };

  const handleExecutePoint = async (pointId: number) => {
    try {
      await executeVirtualPoint(pointId);
    } catch (error) {
      console.error('가상포인트 실행 실패:', error);
    }
  };

  // ========================================================================
  // 렌더링
  // ========================================================================
  return (
    <div className="virtual-points-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-calculator"></i>
            가상포인트 관리
          </h1>
          <p className="page-subtitle">
            스크립트 기반 계산 로직으로 데이터를 가공하고 새로운 포인트를 생성하세요
          </p>
        </div>
        <div className="header-actions">
          <button 
            className="btn btn-outline"
            onClick={() => setShowFormulaHelper(true)}
            disabled={loading}
          >
            <i className="fas fa-question-circle"></i>
            수식 도움말
          </button>
          <button 
            className="btn btn-primary"
            onClick={openCreateModal}
            disabled={loading}
          >
            <i className="fas fa-plus"></i>
            새 가상포인트
          </button>
        </div>
      </div>

      {/* 에러 표시 */}
      {error && (
        <div className="error-banner">
          <div className="error-content">
            <i className="fas fa-exclamation-triangle"></i>
            <span>{error}</span>
            <button onClick={clearError} className="error-close">
              <i className="fas fa-times"></i>
            </button>
          </div>
        </div>
      )}

      {/* 로딩 표시 */}
      {loading && (
        <div className="loading-banner">
          <i className="fas fa-spinner fa-spin"></i>
          데이터를 불러오는 중...
        </div>
      )}

      {/* 통계 카드 */}
      <div className="stats-grid">
        <div className="stat-card total">
          <div className="stat-icon">
            <i className="fas fa-calculator"></i>
          </div>
          <div className="stat-content">
            <div className="stat-value">{virtualPoints.length}</div>
            <div className="stat-label">전체 가상포인트</div>
          </div>
        </div>
        
        <div className="stat-card active">
          <div className="stat-icon">
            <i className="fas fa-play-circle"></i>
          </div>
          <div className="stat-content">
            <div className="stat-value">
              {categoryStats.reduce((sum, stat) => sum + stat.enabled_count, 0)}
            </div>
            <div className="stat-label">활성</div>
          </div>
        </div>
        
        <div className="stat-card error">
          <div className="stat-icon">
            <i className="fas fa-exclamation-triangle"></i>
          </div>
          <div className="stat-content">
            <div className="stat-value">
              {categoryStats.reduce((sum, stat) => sum + stat.error_count, 0)}
            </div>
            <div className="stat-label">오류</div>
          </div>
        </div>
        
        <div className="stat-card performance">
          <div className="stat-icon">
            <i className="fas fa-tachometer-alt"></i>
          </div>
          <div className="stat-content">
            <div className="stat-value">
              {performanceStats?.avg_calculation_time?.toFixed(1) || 'N/A'}ms
            </div>
            <div className="stat-label">평균 계산 시간</div>
          </div>
        </div>
      </div>

      {/* 필터 패널 */}
      <FilterPanel
        filters={filterGroups}
        onFiltersChange={handleFiltersChange}
        showClearAll={true}
        className="virtual-points-filters"
      />

      {/* 뷰 모드 토글 및 정보 */}
      <div className="content-header">
        <div className="view-info">
          <span className="result-count">
            총 <strong>{paginatedData.totalCount}</strong>개 중{' '}
            <strong>{paginatedData.startIndex}-{paginatedData.endIndex}</strong>개 표시
          </span>
        </div>
        
        <div className="view-controls">
          <div className="view-toggle">
            <button
              className={`view-btn ${viewMode === 'card' ? 'active' : ''}`}
              onClick={() => setViewMode('card')}
              title="카드 보기"
            >
              <i className="fas fa-th-large"></i>
            </button>
            <button
              className={`view-btn ${viewMode === 'table' ? 'active' : ''}`}
              onClick={() => setViewMode('table')}
              title="테이블 보기"
            >
              <i className="fas fa-list"></i>
            </button>
          </div>
          
          <button
            className="btn btn-outline btn-sm"
            onClick={refreshData}
            disabled={loading}
            title="새로고침"
          >
            <i className={`fas fa-sync-alt ${loading ? 'fa-spin' : ''}`}></i>
          </button>
        </div>
      </div>

      {/* 가상포인트 목록 */}
      <div className="virtual-points-content">
        {paginatedData.points.length === 0 ? (
          <div className="empty-state">
            <div className="empty-icon">
              <i className="fas fa-calculator"></i>
            </div>
            <h3 className="empty-title">가상포인트가 없습니다</h3>
            <p className="empty-description">
              새로운 가상포인트를 생성하거나 필터 조건을 변경해보세요.
            </p>
            <button
              className="btn btn-primary"
              onClick={openCreateModal}
            >
              <i className="fas fa-plus"></i>
              첫 번째 가상포인트 생성
            </button>
          </div>
        ) : viewMode === 'card' ? (
          <div className="virtual-points-grid">
            {paginatedData.points.map(point => (
              <VirtualPointCard
                key={point.id}
                virtualPoint={point}
                onEdit={() => openEditModal(point)}
                onDelete={() => openDeleteConfirm(point)}
                onTest={() => openTestModal(point)}
                onExecute={() => handleExecutePoint(point.id)}
                isExecuting={testing}
              />
            ))}
          </div>
        ) : (
          <VirtualPointTable
            virtualPoints={paginatedData.points}
            onEdit={openEditModal}
            onDelete={openDeleteConfirm}
            onTest={openTestModal}
            onExecute={handleExecutePoint}
            isExecuting={testing}
            loading={loading}
          />
        )}
      </div>

      {/* 페이징 */}
      {paginatedData.totalCount > 0 && (
        <Pagination
          current={paginatedData.currentPage}
          total={paginatedData.totalCount}
          pageSize={paginatedData.pageSize}
          pageSizeOptions={[6, 12, 24, 48]}
          showSizeChanger={true}
          showTotal={true}
          onChange={(page, pageSize) => {
            setCurrentPage(page);
            if (pageSize !== paginatedData.pageSize) {
              setPageSize(pageSize);
            }
          }}
          onShowSizeChange={(page, pageSize) => {
            setPageSize(pageSize);
            setCurrentPage(1);
          }}
        />
      )}

      {/* 모달들 */}
      {showCreateModal && (
        <VirtualPointModal
          mode="create"
          onClose={closeModals}
          onSave={handleCreateSuccess}
          scriptEngine={scriptEngine}
          loading={saving}
        />
      )}

      {showEditModal && selectedPoint && (
        <VirtualPointModal
          mode="edit"
          virtualPoint={selectedPoint}
          onClose={closeModals}
          onSave={handleUpdateSuccess}
          scriptEngine={scriptEngine}
          loading={saving}
        />
      )}

      {showDeleteConfirm && selectedPoint && (
        <div className="modal-overlay">
          <div className="modal-container small">
            <div className="modal-header">
              <h3>가상포인트 삭제</h3>
              <button onClick={closeModals} className="modal-close">
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div className="modal-content">
              <div className="delete-confirm">
                <div className="delete-icon">
                  <i className="fas fa-exclamation-triangle"></i>
                </div>
                <p>
                  <strong>"{selectedPoint.name}"</strong> 가상포인트를 삭제하시겠습니까?
                </p>
                <p className="delete-warning">
                  이 작업은 되돌릴 수 없으며, 관련된 모든 데이터가 함께 삭제됩니다.
                </p>
              </div>
            </div>
            <div className="modal-footer">
              <button
                className="btn btn-outline"
                onClick={closeModals}
                disabled={saving}
              >
                취소
              </button>
              <button
                className="btn btn-danger"
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

      {showTestModal && selectedPoint && (
        <div className="modal-overlay">
          <div className="modal-container large">
            <div className="modal-header">
              <h3>스크립트 테스트 - {selectedPoint.name}</h3>
              <button onClick={closeModals} className="modal-close">
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div className="modal-content">
              <FormulaHelper
                initialScript={selectedPoint.expression}
                onTest={handleTestScript}
                scriptEngine={scriptEngine}
              />
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default VirtualPoints;