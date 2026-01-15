// ============================================================================
// frontend/src/pages/VirtualPoints.tsx
// 가상포인트 메인 페이지 - 공통 UI 시스템 적용 버전
// ============================================================================

import React, { useState, useEffect } from 'react';
import { Pagination } from '../components/common/Pagination';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { FilterBar } from '../components/common/FilterBar';
import { virtualPointsApi } from '../api/services/virtualPointsApi';

// 필수 컴포넌트 추가
import { VirtualPointCard } from '../components/VirtualPoints/VirtualPointCard';
import { VirtualPointTable } from '../components/VirtualPoints/VirtualPointTable';
import VirtualPointModal from '../components/VirtualPoints/VirtualPointModal/VirtualPointModal';
import { FormulaHelper } from '../components/VirtualPoints/FormulaHelper/FormulaHelper';

import { useScriptEngine } from '../hooks/shared/useScriptEngine';
import '../styles/virtual-points.css';
import '../styles/management.css';
import '../styles/protocol-management.css';
// 기존 스타일 유지 (카드/테이블용)

// 실제 DB 스키마에 맞는 VirtualPoint 인터페이스
interface VirtualPointDB {
  id: number;
  tenant_id: number;
  name: string;
  description?: string;
  category: string;
  formula: string;
  data_type: 'bool' | 'int' | 'float' | 'double' | 'string';
  unit?: string;
  scope_type: 'tenant' | 'site' | 'device';
  site_id?: number;
  device_id?: number;
  calculation_trigger: 'timer' | 'onchange' | 'manual' | 'event';
  calculation_interval: number;
  is_enabled: boolean;
  created_at: string;
  updated_at: string;
  created_by?: string;
  current_value?: any;
  last_calculated?: string;
  calculation_status?: 'active' | 'error' | 'disabled' | 'calculating';
  error_message?: string;
  tags?: string;
  priority?: number;
  expression?: string;
  execution_type?: 'periodic' | 'on_change' | 'manual';
  execution_interval?: number;
  input_variables?: any[];
}

const VirtualPoints: React.FC = () => {
  // 데이터 상태
  const [virtualPoints, setVirtualPoints] = useState<VirtualPointDB[]>([]);
  const [selectedPoint, setSelectedPoint] = useState<VirtualPointDB | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // 모달 상태
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showEditModal, setShowEditModal] = useState(false);
  const [showDeleteConfirm, setShowDeleteConfirm] = useState(false);
  const [showTestModal, setShowTestModal] = useState(false);
  const [showFormulaHelper, setShowFormulaHelper] = useState(false);

  // UI 상태
  const [viewMode, setViewMode] = useState<'card' | 'table'>('card');

  // 필터 상태
  const [searchFilter, setSearchFilter] = useState('');
  const [categoryFilter, setCategoryFilter] = useState('all');
  const [statusFilter, setStatusFilter] = useState('all');
  const [scopeFilter, setScopeFilter] = useState('all');
  const [enabledFilter, setEnabledFilter] = useState('all');

  // 페이징 상태
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(12);

  const scriptEngine = useScriptEngine({
    autoLoadFunctions: false
  });

  useEffect(() => {
    loadVirtualPoints();
  }, []);

  const loadVirtualPoints = async () => {
    setLoading(true);
    setError(null);
    try {
      const response = await virtualPointsApi.getVirtualPoints({});
      let data: any[] = [];
      if (Array.isArray(response)) data = response;
      else if (response && Array.isArray((response as any).data)) data = (response as any).data;
      else if (response && Array.isArray((response as any).items)) data = (response as any).items;
      else if (response && (response as any).success) {
        const responseData = (response as any).data;
        data = responseData?.items || responseData || [];
      }

      const processedData: VirtualPointDB[] = data.map((point: any) => ({
        ...point,
        expression: point.formula || point.expression || '',
        execution_type: mapTriggerToExecutionType(point.calculation_trigger),
        execution_interval: point.calculation_interval || 5000,
        category: point.category || 'Custom',
        calculation_status: point.calculation_status || (point.is_enabled ? 'active' : 'disabled'),
        tags: typeof point.tags === 'string' ? JSON.parse(point.tags || '[]') : (point.tags || []),
        input_variables: point.input_variables || [],
        data_type: mapDataType(point.data_type),
        scope_type: point.scope_type || 'device'
      }));
      setVirtualPoints(processedData);
    } catch (err) {
      setError(err instanceof Error ? err.message : '데이터 로딩 실패');
    } finally {
      setLoading(false);
    }
  };

  const mapTriggerToExecutionType = (trigger?: string): 'periodic' | 'on_change' | 'manual' => {
    switch (trigger) {
      case 'timer': return 'periodic';
      case 'onchange': return 'on_change';
      case 'manual': return 'manual';
      default: return 'periodic';
    }
  };

  const mapExecutionTypeToTrigger = (executionType?: string): string => {
    switch (executionType) {
      case 'periodic': return 'timer';
      case 'on_change': return 'onchange';
      case 'manual': return 'manual';
      default: return 'timer';
    }
  };

  const mapDataType = (dbType?: string): 'number' | 'boolean' | 'string' => {
    switch (dbType) {
      case 'bool': return 'boolean';
      case 'int':
      case 'float':
      case 'double': return 'number';
      case 'string': return 'string';
      default: return 'number';
    }
  };

  const filteredPoints = React.useMemo(() => {
    let points = [...virtualPoints];
    if (searchFilter.trim()) {
      const searchLower = searchFilter.toLowerCase();
      points = points.filter(point =>
        point.name.toLowerCase().includes(searchLower) ||
        (point.description && point.description.toLowerCase().includes(searchLower))
      );
    }
    if (categoryFilter !== 'all') points = points.filter(p => p.category === categoryFilter);
    if (statusFilter !== 'all') points = points.filter(p => p.calculation_status === statusFilter);
    if (enabledFilter !== 'all') points = points.filter(p => p.is_enabled === (enabledFilter === 'true'));
    return points;
  }, [virtualPoints, searchFilter, categoryFilter, statusFilter, enabledFilter]);

  const paginatedData = React.useMemo(() => {
    const totalCount = filteredPoints.length;
    const startIndex = (currentPage - 1) * pageSize;
    return {
      points: filteredPoints.slice(startIndex, startIndex + pageSize),
      totalCount,
      currentPage,
      pageSize
    };
  }, [filteredPoints, currentPage, pageSize]);

  const handleCreateVirtualPoint = async (data: any) => {
    setSaving(true);
    try {
      await virtualPointsApi.createVirtualPoint({ ...data, formula: data.expression, calculation_trigger: mapExecutionTypeToTrigger(data.execution_type) });
      await loadVirtualPoints();
      setShowCreateModal(false);
    } catch (err) { setError(err instanceof Error ? err.message : '생성 실패'); }
    finally { setSaving(false); }
  };

  const handleUpdateVirtualPoint = async (data: any) => {
    if (!selectedPoint) return;
    setSaving(true);
    try {
      await virtualPointsApi.updateVirtualPoint(selectedPoint.id, { ...data, formula: data.expression, calculation_trigger: mapExecutionTypeToTrigger(data.execution_type) });
      await loadVirtualPoints();
      setShowEditModal(false);
    } catch (err) { setError(err instanceof Error ? err.message : '수정 실패'); }
    finally { setSaving(false); }
  };

  const handleDeleteConfirm = async () => {
    if (!selectedPoint) return;
    setSaving(true);
    try {
      await virtualPointsApi.deleteVirtualPoint(selectedPoint.id);
      await loadVirtualPoints();
      setShowDeleteConfirm(false);
    } catch (err) { setError(err instanceof Error ? err.message : '삭제 실패'); }
    finally { setSaving(false); }
  };

  const handleToggleEnabled = async (pointId: number) => {
    const point = virtualPoints.find(p => p.id === pointId);
    if (!point) return;
    try {
      await virtualPointsApi.updateVirtualPoint(pointId, { is_enabled: !point.is_enabled });
      await loadVirtualPoints();
    } catch (err) { setError('상태 변경 실패'); }
  };

  const handleExecutePoint = async (pointId: number) => {
    try { await virtualPointsApi.executeVirtualPoint(pointId); await loadVirtualPoints(); }
    catch (err) { setError('실행 실패'); }
  };

  const openCreateModal = () => { setSelectedPoint(null); setShowCreateModal(true); };
  const openEditModal = (point: VirtualPointDB) => { setSelectedPoint(point); setShowEditModal(true); };
  const openDeleteConfirm = (point: VirtualPointDB) => { setSelectedPoint(point); setShowDeleteConfirm(true); };
  const openTestModal = (point: VirtualPointDB) => { setSelectedPoint(point); setShowTestModal(true); };
  const closeModals = () => { setShowCreateModal(false); setShowEditModal(false); setShowDeleteConfirm(false); setShowTestModal(false); };

  const handleShowFormulaHelper = () => setShowFormulaHelper(true);
  const handleInsertFunction = (code: string) => { console.log(code); setShowFormulaHelper(false); };
  const handleClearFilters = () => { setSearchFilter(''); setCategoryFilter('all'); setStatusFilter('all'); setEnabledFilter('all'); setCurrentPage(1); };
  const getActiveFilterCount = () => (searchFilter ? 1 : 0) + (categoryFilter !== 'all' ? 1 : 0) + (statusFilter !== 'all' ? 1 : 0) + (enabledFilter !== 'all' ? 1 : 0);
  const handlePageChange = (page: number, size: number) => { setCurrentPage(page); setPageSize(size); };

  if (loading && virtualPoints.length === 0) {
    return <ManagementLayout><div className="loading-container">가상포인트를 불러오는 중...</div></ManagementLayout>;
  }

  return (
    <ManagementLayout>
      <PageHeader
        title="가상포인트 관리"
        description="실시간 데이터를 기반으로 계산되는 가상포인트를 관리합니다."
        icon="fas fa-function"
        actions={
          <>
            <button className="btn-secondary" onClick={handleShowFormulaHelper}><i className="fas fa-calculator"></i> 함수 도우미</button>
            <button className="btn-primary" onClick={openCreateModal}><i className="fas fa-plus"></i> 새 가상포인트</button>
          </>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label="전체" value={virtualPoints.length} type="neutral" />
        <StatCard label="활성" value={virtualPoints.filter(p => p.is_enabled).length} type="success" />
        <StatCard label="오류" value={virtualPoints.filter(p => p.calculation_status === 'error').length} type="error" />
        <StatCard label="결과" value={paginatedData.totalCount} type="primary" />
      </div>

      <FilterBar
        searchTerm={searchFilter}
        onSearchChange={setSearchFilter}
        onReset={handleClearFilters}
        activeFilterCount={getActiveFilterCount()}
        filters={[
          {
            label: '카테고리',
            value: categoryFilter,
            onChange: setCategoryFilter,
            options: [
              { value: 'all', label: '전체' },
              { value: '온도', label: '온도' },
              { value: '압력', label: '압력' },
              { value: '유량', label: '유량' },
              { value: '전력', label: '전력' },
              { value: 'Custom', label: 'Custom' }
            ]
          },
          {
            label: '계산 상태',
            value: statusFilter,
            onChange: setStatusFilter,
            options: [
              { value: 'all', label: '전체' },
              { value: 'active', label: '활성' },
              { value: 'error', label: '오류' },
              { value: 'disabled', label: '비활성' }
            ]
          }
        ]}
      />

      <div style={{ display: 'flex', justifyContent: 'flex-end', marginBottom: '16px' }}>
        <div className="view-toggle">
          <button onClick={() => setViewMode('card')} className={viewMode === 'card' ? 'active' : ''}><i className="fas fa-th-large"></i> 카드</button>
          <button onClick={() => setViewMode('table')} className={viewMode === 'table' ? 'active' : ''}><i className="fas fa-table"></i> 테이블</button>
        </div>
      </div>

      {error && <div className="error-banner"><span>{error}</span><button onClick={() => setError(null)}>X</button></div>}

      <div className="points-list">
        {viewMode === 'card' ? (
          <div className="points-grid">
            {paginatedData.points.map(point => (
              <VirtualPointCard
                key={point.id}
                virtualPoint={point as any}
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
            virtualPoints={paginatedData.points as any}
            onEdit={(p) => openEditModal(p as any)}
            onDelete={(p) => openDeleteConfirm(p as any)}
            onTest={(p) => openTestModal(p as any)}
            onExecute={handleExecutePoint}
            onToggleEnabled={handleToggleEnabled}
          />
        )}
      </div>

      <Pagination
        current={paginatedData.currentPage}
        total={paginatedData.totalCount}
        pageSize={paginatedData.pageSize}
        onChange={handlePageChange}
      />

      {showCreateModal && <VirtualPointModal isOpen={showCreateModal} mode="create" onSave={handleCreateVirtualPoint} onClose={closeModals} loading={saving} />}
      {showEditModal && selectedPoint && <VirtualPointModal isOpen={showEditModal} mode="edit" virtualPoint={selectedPoint as any} onSave={handleUpdateVirtualPoint} onClose={closeModals} loading={saving} />}
      {showFormulaHelper && <FormulaHelper isOpen={showFormulaHelper} onClose={() => setShowFormulaHelper(false)} onInsertFunction={handleInsertFunction} functions={scriptEngine.functions || []} />}
    </ManagementLayout>
  );
};

export default VirtualPoints;