// ============================================================================
// frontend/src/pages/VirtualPoints.tsx
// 가상포인트 메인 페이지 - 리뉴얼 버전
// ============================================================================

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
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
  const { t } = useTranslation(['virtualPoints', 'common']);
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
      setError(err instanceof Error ? err.message : 'Data loading failed');
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
    } catch (err) { setError(err instanceof Error ? err.message : 'Create failed'); }
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
    } catch (err) { setError(err instanceof Error ? err.message : 'Update failed'); }
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
        title: 'Delete Complete',
        message: 'Virtual point deleted successfully.',
        confirmText: 'OK',
        showCancelButton: false
      });
    } catch (err) { setError(err instanceof Error ? err.message : 'Delete failed'); }
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
    } catch (err) { setError(err instanceof Error ? err.message : 'Restore failed'); }
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
        title: 'Status Changed',
        message: `Virtual point has been ${!point.is_enabled ? 'enabled' : 'disabled'}.`,
        confirmText: 'OK',
        showCancelButton: false
      });
    } catch (err) { setError('Status change failed'); }
  };

  const handleExecutePoint = async (pointId: number) => {
    try {
      await virtualPointsApi.executeVirtualPoint(pointId);
      await loadVirtualPoints();

      // 성공 피드백 추가
      await confirm({
        title: 'Execution Complete',
        message: 'Virtual point calculation executed successfully.',
        confirmText: 'OK',
        showCancelButton: false
      });
    } catch (err) { setError('Execution failed'); }
  };

  const handleBulkDelete = async () => {
    if (selectedIds.length === 0) return;
    const confirmed = await confirm({
      title: 'Confirm Bulk Delete',
      message: `Delete ${selectedIds.length} selected virtual point(s)?`,
      confirmText: 'Delete',
      cancelText: 'Cancel'
    });

    if (confirmed) {
      setSaving(true);
      try {
        await Promise.all(selectedIds.map(id => virtualPointsApi.deleteVirtualPoint(id)));
        await loadVirtualPoints();
        setSelectedIds([]);
        await confirm({ title: 'Complete', message: 'Selected items have been deleted.', confirmText: 'OK', showCancelButton: false });
      } catch (e) { setError('Error during bulk delete.'); }
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
      await confirm({ title: 'Complete', message: `Selected items have been ${enable ? 'enabled' : 'disabled'}.`, confirmText: 'OK', showCancelButton: false });
    } catch (e) { setError('Error during bulk status change.'); }
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
        title={t('title')}
        description={t('description')}
        icon="fas fa-function"
        actions={
          <div style={{ display: 'flex', gap: '12px' }}>
            <button className="btn btn-primary" onClick={openCreateModal}><i className="fas fa-plus"></i> {t('new')}</button>
          </div>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label={t('stats.totalPoints')} value={virtualPoints.length} type="neutral" icon="fas fa-layer-group" />
        <StatCard label={t('stats.active')} value={virtualPoints.filter(p => p.is_enabled).length} type="success" icon="fas fa-check-circle" />
        <StatCard label={t('stats.calcError')} value={virtualPoints.filter(p => p.calculation_status === 'error').length} type="error" icon="fas fa-exclamation-triangle" />
      </div>

      <div className="mgmt-tabs-wrapper">
        <div className="tab-navigation">
          <button className={`tab-button ${activeTab === 'list' ? 'active' : ''}`} onClick={() => setActiveTab('list')}>
            <i className="fas fa-list"></i> {t('tabs.list')}
          </button>
          <button className={`tab-button ${activeTab === 'history' ? 'active' : ''}`} onClick={() => setActiveTab('history')}>
            <i className="fas fa-history"></i> {t('tabs.history')}
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
                  label: t('filter.category'),
                  value: categoryFilter,
                  onChange: setCategoryFilter,
                  options: [
                    { value: 'all', label: t('filter.all') },
                    { value: 'temperature', label: t('filter.temperature', '온도') },
                    { value: 'pressure', label: t('filter.pressure', '압력') },
                    { value: 'flow', label: t('filter.flow', '유량') },
                    { value: 'power', label: t('filter.power', '전력') },
                    { value: 'Custom', label: t('filter.custom', '사용자 정의') }
                  ]
                },
                {
                  label: t('filter.status'),
                  value: statusFilter,
                  onChange: setStatusFilter,
                  options: [
                    { value: 'all', label: t('filter.all') },
                    { value: 'active', label: t('filter.active') },
                    { value: 'error', label: t('filter.error') },
                    { value: 'disabled', label: t('filter.disabled') }
                  ]
                }
              ]}
              rightActions={
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px', marginRight: '8px' }}>
                  <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '13px', cursor: 'pointer', color: '#64748b', whiteSpace: 'nowrap' }}>
                    <input type="checkbox" checked={includeDeleted} onChange={(e) => setIncludeDeleted(e.target.checked)} />
                    {t('showDeleted')}
                  </label>
                  <div className="view-toggle" style={{ display: 'flex', flexShrink: 0 }}>
                    <button className={`btn-icon ${viewMode === 'card' ? 'active' : ''}`} onClick={() => setViewMode('card')}><i className="fas fa-th-large"></i></button>
                    <button className={`btn-icon ${viewMode === 'table' ? 'active' : ''}`} onClick={() => setViewMode('table')}><i className="fas fa-list"></i></button>
                  </div>
                </div>
              }
            />

            {/* Bulk Actions 툴바 */}
            {selectedIds.length > 0 && (
              <div style={{
                marginBottom: '12px', padding: '8px 16px', background: '#f0f9ff', border: '1px solid #bae6fd',
                borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'space-between'
              }}>
                <span style={{ fontWeight: 600, color: '#0369a1', fontSize: '14px' }}>
                  <i className="fas fa-check-circle mr-2"></i> {selectedIds.length} item(s) selected
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
                <div style={{ padding: '40px', textAlign: 'center' }}><i className="fas fa-spinner fa-spin"></i> {t('loading')}</div>
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
            <h3>{t('history.empty')}</h3>
            <p>{t('history.emptyDesc')}</p>
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