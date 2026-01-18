// ============================================================================
// frontend/src/pages/VirtualPoints.tsx
// 가상포인트 메인 페이지 - 리뉴얼 버전
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { Pagination } from '../components/common/Pagination';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { FilterBar } from '../components/common/FilterBar';
import { virtualPointsApi } from '../api/services/virtualPointsApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { VirtualPoint } from '../types/virtualPoints';

// 컴포넌트 임포트
import { VirtualPointCard } from '../components/VirtualPoints/VirtualPointCard';
import { VirtualPointTable } from '../components/VirtualPoints/VirtualPointTable';
import VirtualPointModal from '../components/VirtualPoints/VirtualPointModal/VirtualPointModal';
import { VirtualPointDetailModal } from '../components/VirtualPoints/VirtualPointModal/VirtualPointDetailModal';
import { FormulaHelper } from '../components/VirtualPoints/FormulaHelper/FormulaHelper';

import { useScriptEngine } from '../hooks/shared/useScriptEngine';
import '../styles/virtual-points.css';
import '../styles/management.css';

const VirtualPoints: React.FC = () => {
  const { confirm } = useConfirmContext();
  const scriptEngine = useScriptEngine({ autoLoadFunctions: false });

  // 데이터 상태
  const [virtualPoints, setVirtualPoints] = useState<VirtualPoint[]>([]);
  const [selectedPoint, setSelectedPoint] = useState<VirtualPoint | null>(null);
  const [selectedIds, setSelectedIds] = useState<number[]>([]);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // UI 상태
  const [viewMode, setViewMode] = useState<'card' | 'table'>('table');
  const [activeTab, setActiveTab] = useState<'list' | 'history'>('list');
  const [includeDeleted, setIncludeDeleted] = useState(false);

  // 모달 상태
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [showDetailModal, setShowDetailModal] = useState(false);
  const [showFormulaHelper, setShowFormulaHelper] = useState(false);

  // 필터 상태
  const [searchFilter, setSearchFilter] = useState('');
  const [categoryFilter, setCategoryFilter] = useState('all');
  const [statusFilter, setStatusFilter] = useState('all');

  // 페이징 상태
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(12);

  const loadVirtualPoints = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const response = await virtualPointsApi.getVirtualPoints({
        search: searchFilter,
        include_deleted: includeDeleted
      } as any);

      let data: any[] = Array.isArray(response) ? response : [];

      const processedData: VirtualPoint[] = data.map((point: any) => ({
        ...point,
        expression: point.formula || point.expression || '',
        execution_type: point.calculation_trigger === 'timer' ? 'periodic' :
          point.calculation_trigger === 'onchange' ? 'on_change' : 'manual',
        execution_interval: point.calculation_interval || 5000,
        category: point.category || 'Custom',
        calculation_status: point.calculation_status || (point.is_enabled ? 'active' : 'disabled')
      }));
      setVirtualPoints(processedData);
      setSelectedIds([]); // 데이터 로드 시 선택 초기화
    } catch (err) {
      setError(err instanceof Error ? err.message : '데이터 로딩 실패');
    } finally {
      setLoading(false);
    }
  }, [searchFilter, includeDeleted]);

  useEffect(() => {
    loadVirtualPoints();
  }, [loadVirtualPoints]);

  const filteredPoints = useMemo(() => {
    let points = [...virtualPoints];
    if (categoryFilter !== 'all') points = points.filter(p => p.category === categoryFilter);
    if (statusFilter !== 'all') points = points.filter(p => p.calculation_status === statusFilter);
    return points;
  }, [virtualPoints, categoryFilter, statusFilter]);

  const paginatedPoints = useMemo(() => {
    const startIndex = (currentPage - 1) * pageSize;
    return filteredPoints.slice(startIndex, startIndex + pageSize);
  }, [filteredPoints, currentPage, pageSize]);

  const handleCreateVirtualPoint = async (data: any) => {
    setSaving(true);
    try {
      await virtualPointsApi.createVirtualPoint({
        ...data,
        formula: data.expression,
        calculation_trigger: data.execution_type === 'periodic' ? 'timer' : 'onchange'
      });
      await loadVirtualPoints();
      setShowCreateModal(false);
    } catch (err) { setError(err instanceof Error ? err.message : '생성 실패'); }
    finally { setSaving(false); }
  };

  const handleUpdateVirtualPoint = async (data: any) => {
    if (!selectedPoint) return;
    setSaving(true);
    try {
      await virtualPointsApi.updateVirtualPoint(selectedPoint.id, {
        ...data,
        formula: data.expression,
        calculation_trigger: data.execution_type === 'periodic' ? 'timer' : 'onchange'
      });
      await loadVirtualPoints();
      setShowEditModal(false);
    } catch (err) { setError(err instanceof Error ? err.message : '수정 실패'); }
    finally { setSaving(false); }
  };

  const handleDeletePoint = async (pointId: number) => {
    setSaving(true);
    try {
      await virtualPointsApi.deleteVirtualPoint(pointId);
      await loadVirtualPoints();
      setShowDetailModal(false);
      setShowEditModal(false);

      // 성공 피드백 추가
      await confirm({
        title: '삭제 완료',
        message: '가상포인트가 성공적으로 삭제되었습니다.',
        confirmText: '확인',
        showCancelButton: false
      });
    } catch (err) { setError(err instanceof Error ? err.message : '삭제 실패'); }
    finally { setSaving(false); }
  };

  const handleRestorePoint = async (pointId: number) => {
    setSaving(true);
    try {
      await virtualPointsApi.restoreVirtualPoint(pointId);
      await loadVirtualPoints();
      if (selectedPoint && selectedPoint.id === pointId) {
        // Update selected point if it was restored while open
        setSelectedPoint({ ...selectedPoint, is_deleted: false });
      }
    } catch (err) { setError(err instanceof Error ? err.message : '복원 실패'); }
    finally { setSaving(false); }
  };

  const handleToggleEnabled = async (pointId: number) => {
    const point = virtualPoints.find(p => p.id === pointId);
    if (!point) return;
    try {
      await virtualPointsApi.updateVirtualPoint(pointId, { is_enabled: !point.is_enabled });
      await loadVirtualPoints();

      // 성공 피드백 추가
      await confirm({
        title: '상태 변경 완료',
        message: `가상포인트가 ${!point.is_enabled ? '활성화' : '비활성화'} 되었습니다.`,
        confirmText: '확인',
        showCancelButton: false
      });
    } catch (err) { setError('상태 변경 실패'); }
  };

  const handleExecutePoint = async (pointId: number) => {
    try {
      await virtualPointsApi.executeVirtualPoint(pointId);
      await loadVirtualPoints();

      // 성공 피드백 추가
      await confirm({
        title: '실행 완료',
        message: '가상포인트 연산이 성공적으로 실행되었습니다.',
        confirmText: '확인',
        showCancelButton: false
      });
    } catch (err) { setError('실행 실패'); }
  };

  const handleBulkDelete = async () => {
    if (selectedIds.length === 0) return;
    const confirmed = await confirm({
      title: '일괄 삭제 확인',
      message: `선택한 ${selectedIds.length}개의 가상포인트를 삭제하시겠습니까?`,
      confirmText: '삭제',
      cancelText: '취소'
    });

    if (confirmed) {
      setSaving(true);
      try {
        await Promise.all(selectedIds.map(id => virtualPointsApi.deleteVirtualPoint(id)));
        await loadVirtualPoints();
        setSelectedIds([]);
        await confirm({ title: '완료', message: '선택한 항목이 삭제되었습니다.', confirmText: '확인', showCancelButton: false });
      } catch (e) { setError('일괄 삭제 중 오류가 발생했습니다.'); }
      finally { setSaving(false); }
    }
  };

  const handleBulkToggle = async (enable: boolean) => {
    if (selectedIds.length === 0) return;
    setSaving(true);
    try {
      await Promise.all(selectedIds.map(id => virtualPointsApi.updateVirtualPoint(id, { is_enabled: enable })));
      await loadVirtualPoints();
      setSelectedIds([]);
      await confirm({ title: '완료', message: `선택한 항목이 ${enable ? '활성화' : '비활성화'} 되었습니다.`, confirmText: '확인', showCancelButton: false });
    } catch (e) { setError('일괄 상태 변경 중 오류가 발생했습니다.'); }
    finally { setSaving(false); }
  };


  const openCreateModal = () => { setSelectedPoint(null); setShowCreateModal(true); };
  const openEditModal = (point: VirtualPoint) => { setSelectedPoint(point); setShowEditModal(true); setShowDetailModal(false); };
  const openDetailModal = (point: VirtualPoint) => { setSelectedPoint(point); setShowDetailModal(true); };
  const closeModals = () => {
    setShowCreateModal(false);
    setShowEditModal(false);
    setShowDetailModal(false);
  };

  return (
    <ManagementLayout>
      <PageHeader
        title="가상포인트 관리"
        description="실시간 데이터를 기반으로 계산되는 가상포인트를 관리합니다. 이름 클릭 시 상세 정보를 확인 수 있습니다."
        icon="fas fa-function"
        actions={
          <div style={{ display: 'flex', gap: '12px' }}>
            <button className="btn btn-primary" onClick={openCreateModal}><i className="fas fa-plus"></i> 새 가상포인트</button>
          </div>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label="전체 포인트" value={virtualPoints.length} type="neutral" icon="fas fa-layer-group" />
        <StatCard label="활성 상태" value={virtualPoints.filter(p => p.is_enabled).length} type="success" icon="fas fa-check-circle" />
        <StatCard label="계산 오류" value={virtualPoints.filter(p => p.calculation_status === 'error').length} type="error" icon="fas fa-exclamation-triangle" />
      </div>

      <div className="mgmt-tabs-wrapper">
        <div className="tab-navigation">
          <button className={`tab-button ${activeTab === 'list' ? 'active' : ''}`} onClick={() => setActiveTab('list')}>
            <i className="fas fa-list"></i> 가상 포인트 목록
          </button>
          <button className={`tab-button ${activeTab === 'history' ? 'active' : ''}`} onClick={() => setActiveTab('history')}>
            <i className="fas fa-history"></i> 실행 이력
          </button>
        </div>
      </div>

      <div className="mgmt-content-area">
        {activeTab === 'list' ? (
          <div style={{ display: 'flex', flexDirection: 'column', height: '100%', minHeight: 0 }}>
            <FilterBar
              searchTerm={searchFilter}
              onSearchChange={setSearchFilter}
              onReset={() => { setSearchFilter(''); setCategoryFilter('all'); setStatusFilter('all'); setIncludeDeleted(false); }}
              activeFilterCount={(searchFilter ? 1 : 0) + (categoryFilter !== 'all' ? 1 : 0) + (statusFilter !== 'all' ? 1 : 0) + (includeDeleted ? 1 : 0)}
              filters={[
                {
                  label: '카테고리',
                  value: categoryFilter,
                  onChange: setCategoryFilter,
                  options: [
                    { value: 'all', label: '전체' },
                    { value: '온도', label: '온도' }, { value: '압력', label: '압력' },
                    { value: '유량', label: '유량' }, { value: '전력', label: '전력' },
                    { value: 'Custom', label: 'Custom' }
                  ]
                },
                {
                  label: '상태',
                  value: statusFilter,
                  onChange: setStatusFilter,
                  options: [
                    { value: 'all', label: '전체' },
                    { value: 'active', label: '활성' }, { value: 'error', label: '오류' }, { value: 'disabled', label: '비활성' }
                  ]
                }
              ]}
              rightActions={
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px', marginRight: '8px' }}>
                  <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '13px', cursor: 'pointer', color: '#64748b', whiteSpace: 'nowrap' }}>
                    <input type="checkbox" checked={includeDeleted} onChange={(e) => setIncludeDeleted(e.target.checked)} />
                    삭제된 항목 보기
                  </label>
                  <div className="view-toggle" style={{ display: 'flex', flexShrink: 0 }}>
                    <button className={`btn-icon ${viewMode === 'card' ? 'active' : ''}`} onClick={() => setViewMode('card')}><i className="fas fa-th-large"></i></button>
                    <button className={`btn-icon ${viewMode === 'table' ? 'active' : ''}`} onClick={() => setViewMode('table')}><i className="fas fa-list"></i></button>
                  </div>
                </div>
              }
            />

            {/* 일괄 작업 툴바 */}
            {selectedIds.length > 0 && (
              <div style={{
                marginBottom: '12px', padding: '8px 16px', background: '#f0f9ff', border: '1px solid #bae6fd',
                borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'space-between'
              }}>
                <span style={{ fontWeight: 600, color: '#0369a1', fontSize: '14px' }}>
                  <i className="fas fa-check-circle mr-2"></i> {selectedIds.length}개 항목 선택됨
                </span>
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button onClick={() => handleBulkToggle(true)} style={{
                    padding: '6px 12px', borderRadius: '6px', border: '1px solid #bbf7d0',
                    background: '#dcfce7', color: '#166534', cursor: 'pointer', fontSize: '13px', fontWeight: 600
                  }}>
                    <i className="fas fa-play mr-1"></i> 활성화
                  </button>
                  <button onClick={() => handleBulkToggle(false)} style={{
                    padding: '6px 12px', borderRadius: '6px', border: '1px solid #e2e8f0',
                    background: 'white', color: '#475569', cursor: 'pointer', fontSize: '13px', fontWeight: 600
                  }}>
                    <i className="fas fa-pause mr-1"></i> 비활성화
                  </button>
                  <button onClick={handleBulkDelete} style={{
                    padding: '6px 12px', borderRadius: '6px', border: '1px solid #fecaca',
                    background: '#fee2e2', color: '#991b1b', cursor: 'pointer', fontSize: '13px', fontWeight: 600
                  }}>
                    <i className="fas fa-trash-alt mr-1"></i> 삭제
                  </button>
                  <button onClick={() => setSelectedIds([])} style={{
                    padding: '6px 12px', borderRadius: '6px', border: 'none',
                    background: 'transparent', color: '#64748b', cursor: 'pointer', fontSize: '13px'
                  }}>
                    취소
                  </button>
                </div>
              </div>
            )}

            <div className="mgmt-content-area" style={{ flex: 1, overflow: 'auto', padding: '8px 4px' }}>
              {loading ? (
                <div style={{ padding: '40px', textAlign: 'center' }}><i className="fas fa-spinner fa-spin"></i> 로딩 중...</div>
              ) : viewMode === 'card' ? (
                <div className="mgmt-grid">
                  {paginatedPoints.map(point => (
                    <VirtualPointCard
                      key={point.id}
                      virtualPoint={point}
                      onEdit={openEditModal}
                      onExecute={handleExecutePoint}
                      onToggleEnabled={handleToggleEnabled}
                      onRestore={handleRestorePoint}
                      onRefresh={loadVirtualPoints}
                      onRowClick={openDetailModal}
                    />
                  ))}
                </div>
              ) : (
                <div className="mgmt-table-wrapper" style={{ height: 'calc(100% - 16px)' }}>
                  <VirtualPointTable
                    virtualPoints={paginatedPoints}
                    selectedIds={selectedIds}
                    onSelectionChange={setSelectedIds}
                    onEdit={openEditModal}
                    onExecute={handleExecutePoint}
                    onToggleEnabled={handleToggleEnabled}
                    onRestore={handleRestorePoint}
                    onDelete={handleDeletePoint}
                    onRowClick={openDetailModal}
                  />
                </div>
              )}
            </div>

            <div style={{ padding: '12px 24px', background: 'white', borderTop: '1px solid #e2e8f0', marginTop: 'auto' }}>
              <Pagination
                current={currentPage}
                total={filteredPoints.length}
                pageSize={pageSize}
                onChange={(page) => setCurrentPage(page)}
              />
            </div>
          </div>
        ) : (
          <div className="empty-state-notice">
            <i className="fas fa-clock-rotate-left"></i>
            <h3>실행 이력이 없습니다</h3>
            <p>최근 실행된 가상 포인트의 계산 결과 및 성능 정보가 여기에 표시됩니다.</p>
          </div>
        )}
      </div>

      <VirtualPointModal isOpen={showCreateModal} mode="create" onSave={handleCreateVirtualPoint} onClose={closeModals} loading={saving} />
      {selectedPoint && <VirtualPointModal isOpen={showEditModal} mode="edit" virtualPoint={selectedPoint} onSave={handleUpdateVirtualPoint} onDelete={handleDeletePoint} onRestore={handleRestorePoint} onClose={closeModals} loading={saving} />}
      {selectedPoint && <VirtualPointDetailModal isOpen={showDetailModal} virtualPoint={selectedPoint} onClose={closeModals} onEdit={openEditModal} onExecute={handleExecutePoint} onToggleEnabled={handleToggleEnabled} onRestore={handleRestorePoint} onDelete={handleDeletePoint} />}
    </ManagementLayout>
  );
};

export default VirtualPoints;