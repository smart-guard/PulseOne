// ============================================================================
// frontend/src/pages/VirtualPoints.tsx
// 가상포인트 메인 페이지 - 분리된 컴포넌트 사용 + API 필드 수정
// ============================================================================

import React, { useState, useEffect } from 'react';
import { Pagination } from '../components/common/Pagination';
import { virtualPointsApi } from '../api/services/virtualPointsApi';

// 분리된 컴포넌트들 사용
import { VirtualPointCard } from '../components/VirtualPoints/VirtualPointCard';
import { VirtualPointTable } from '../components/VirtualPoints/VirtualPointTable';
import VirtualPointModal from '../components/VirtualPoints/VirtualPointModal/VirtualPointModal';
import { FormulaHelper } from '../components/VirtualPoints/FormulaHelper/FormulaHelper';

import { useScriptEngine } from '../hooks/shared/useScriptEngine';
import { VirtualPoint } from '../types/virtualPoints';
import '../styles/virtual-points.css';

// 실제 DB 스키마에 맞는 VirtualPoint 인터페이스
interface VirtualPointDB {
  id: number;
  tenant_id: number;
  name: string;
  description?: string;
  category: string;
  formula: string; // DB에서는 formula 필드 사용
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
  
  // 실행 결과 관련 (virtual_point_values에서 조인)
  current_value?: any;
  last_calculated?: string;
  calculation_status?: 'active' | 'error' | 'disabled' | 'calculating';
  error_message?: string;
  
  // 메타데이터
  tags?: string;
  priority?: number;
}

const VirtualPoints: React.FC = () => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  
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

  // 스크립트 엔진은 선택적으로 로딩
  const scriptEngine = useScriptEngine({
    autoLoadFunctions: false // API 404 에러 방지
  });

  // ========================================================================
  // 데이터 로딩
  // ========================================================================
  
  useEffect(() => {
    loadVirtualPoints();
  }, []);

  const loadVirtualPoints = async () => {
    setLoading(true);
    setError(null);
    
    try {
      console.log('가상포인트 목록 로딩 시작');
      const data = await virtualPointsApi.getVirtualPoints({});
      
      // DB 응답 데이터 검증 및 변환
      const processedData = data.map((point: any) => ({
        ...point,
        // formula -> expression 필드 매핑 (하위 호환성)
        expression: point.formula || point.expression,
        // 기본값 설정
        category: point.category || 'Custom',
        calculation_status: point.calculation_status || 'disabled',
        tags: typeof point.tags === 'string' ? JSON.parse(point.tags || '[]') : (point.tags || [])
      }));
      
      setVirtualPoints(processedData);
      console.log(`${processedData.length}개 가상포인트 로드 완료`);
    } catch (err) {
      console.error('가상포인트 로딩 실패:', err);
      setError(err instanceof Error ? err.message : '데이터 로딩 실패');
    } finally {
      setLoading(false);
    }
  };

  // ========================================================================
  // 클라이언트 사이드 필터링 및 페이징
  // ========================================================================
  
  const filteredPoints = React.useMemo(() => {
    let points = [...virtualPoints];

    if (searchFilter.trim()) {
      const searchLower = searchFilter.toLowerCase();
      points = points.filter(point =>
        point.name.toLowerCase().includes(searchLower) ||
        (point.description && point.description.toLowerCase().includes(searchLower)) ||
        (point.formula && point.formula.toLowerCase().includes(searchLower)) ||
        (Array.isArray(point.tags) && point.tags.some(tag => tag.toLowerCase().includes(searchLower)))
      );
    }

    if (categoryFilter !== 'all') {
      points = points.filter(point => point.category === categoryFilter);
    }

    if (statusFilter !== 'all') {
      points = points.filter(point => point.calculation_status === statusFilter);
    }

    if (scopeFilter !== 'all') {
      points = points.filter(point => point.scope_type === scopeFilter);
    }

    if (enabledFilter !== 'all') {
      const isEnabled = enabledFilter === 'true';
      points = points.filter(point => point.is_enabled === isEnabled);
    }

    return points;
  }, [virtualPoints, searchFilter, categoryFilter, statusFilter, scopeFilter, enabledFilter]);

  const paginatedData = React.useMemo(() => {
    const totalCount = filteredPoints.length;
    const totalPages = Math.ceil(totalCount / pageSize);
    const startIndex = (currentPage - 1) * pageSize;
    const endIndex = startIndex + pageSize;
    const points = filteredPoints.slice(startIndex, endIndex);
    
    return {
      points,
      totalCount,
      totalPages,
      currentPage,
      pageSize,
      startIndex: startIndex + 1,
      endIndex: Math.min(endIndex, totalCount)
    };
  }, [filteredPoints, currentPage, pageSize]);

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================

  const handleCreateVirtualPoint = async (data: any) => {
    setSaving(true);
    try {
      // expression -> formula 매핑
      const dbData = {
        ...data,
        formula: data.expression || data.formula
      };
      
      await virtualPointsApi.createVirtualPoint(dbData);
      await loadVirtualPoints();
      setShowCreateModal(false);
    } catch (err) {
      console.error('가상포인트 생성 실패:', err);
      setError(err instanceof Error ? err.message : '생성 실패');
    } finally {
      setSaving(false);
    }
  };

  const handleUpdateVirtualPoint = async (data: any) => {
    if (!selectedPoint) return;
    
    setSaving(true);
    try {
      // expression -> formula 매핑
      const dbData = {
        ...data,
        formula: data.expression || data.formula
      };
      
      await virtualPointsApi.updateVirtualPoint(selectedPoint.id, dbData);
      await loadVirtualPoints();
      setShowEditModal(false);
      setSelectedPoint(null);
    } catch (err) {
      console.error('가상포인트 수정 실패:', err);
      setError(err instanceof Error ? err.message : '수정 실패');
    } finally {
      setSaving(false);
    }
  };

  const handleDeleteConfirm = async () => {
    if (!selectedPoint) return;
    
    setSaving(true);
    try {
      await virtualPointsApi.deleteVirtualPoint(selectedPoint.id);
      await loadVirtualPoints();
      setShowDeleteConfirm(false);
      setSelectedPoint(null);
    } catch (err) {
      console.error('가상포인트 삭제 실패:', err);
      setError(err instanceof Error ? err.message : '삭제 실패');
    } finally {
      setSaving(false);
    }
  };

  const handleToggleEnabled = async (pointId: number) => {
    try {
      const point = virtualPoints.find(p => p.id === pointId);
      if (!point) return;
      
      await virtualPointsApi.updateVirtualPoint(pointId, { 
        is_enabled: !point.is_enabled 
      });
      await loadVirtualPoints();
    } catch (err) {
      console.error('가상포인트 활성화 토글 실패:', err);
      setError(err instanceof Error ? err.message : '상태 변경 실패');
    }
  };

  const handleExecutePoint = async (pointId: number) => {
    try {
      await virtualPointsApi.executeVirtualPoint(pointId);
      await loadVirtualPoints();
    } catch (err) {
      console.error('가상포인트 실행 실패:', err);
      setError(err instanceof Error ? err.message : '실행 실패');
    }
  };

  const handleTestScript = async (pointId: number, testData: any) => {
    try {
      const result = await virtualPointsApi.testVirtualPoint(pointId, testData);
      console.log('테스트 결과:', result);
      return result;
    } catch (err) {
      console.error('스크립트 테스트 실패:', err);
      throw err;
    }
  };

  // 모달 핸들러들
  const openCreateModal = () => {
    setSelectedPoint(null);
    setShowCreateModal(true);
  };

  const openEditModal = (point: VirtualPointDB) => {
    setSelectedPoint(point);
    setShowEditModal(true);
  };

  const openDeleteConfirm = (point: VirtualPointDB) => {
    setSelectedPoint(point);
    setShowDeleteConfirm(true);
  };

  const openTestModal = (point: VirtualPointDB) => {
    setSelectedPoint(point);
    setShowTestModal(true);
  };

  const closeModals = () => {
    setShowCreateModal(false);
    setShowEditModal(false);
    setShowDeleteConfirm(false);
    setShowTestModal(false);
    setSelectedPoint(null);
  };

  // FormulaHelper 핸들러들
  const handleShowFormulaHelper = () => {
    setShowFormulaHelper(true);
  };

  const handleInsertFunction = (functionCode: string) => {
    console.log('함수 삽입:', functionCode);
    setShowFormulaHelper(false);
  };

  // 필터 관련 핸들러들
  const handleClearFilters = () => {
    setSearchFilter('');
    setCategoryFilter('all');
    setStatusFilter('all');
    setScopeFilter('all');
    setEnabledFilter('all');
    setCurrentPage(1);
  };

  const getActiveFilterCount = () => {
    let count = 0;
    if (searchFilter.trim()) count++;
    if (categoryFilter !== 'all') count++;
    if (statusFilter !== 'all') count++;
    if (scopeFilter !== 'all') count++;
    if (enabledFilter !== 'all') count++;
    return count;
  };

  const handlePageChange = (page: number, newPageSize: number) => {
    setCurrentPage(page);
    if (newPageSize !== pageSize) {
      setPageSize(newPageSize);
      setCurrentPage(1);
    }
  };

  // 정렬 핸들러
  const handleSortChange = (sortBy: string, sortOrder: 'asc' | 'desc') => {
    console.log('정렬 변경:', sortBy, sortOrder);
    // TODO: 정렬 기능 구현
  };

  // ========================================================================
  // 렌더링
  // ========================================================================

  if (loading && virtualPoints.length === 0) {
    return (
      <div className="virtual-points-container">
        <div style={{ 
          display: 'flex', 
          justifyContent: 'center', 
          alignItems: 'center', 
          height: '400px', 
          gap: '12px' 
        }}>
          <i className="fas fa-spinner fa-spin" style={{ fontSize: '24px', color: '#3b82f6' }}></i>
          <p style={{ fontSize: '16px', color: '#6b7280', margin: 0 }}>
            가상포인트를 불러오는 중...
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="virtual-points-container" style={{ 
      padding: '24px', 
      background: '#f8fafc', 
      minHeight: '100vh' 
    }}>
      {/* 페이지 헤더 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '24px',
        padding: '24px',
        background: 'white',
        borderRadius: '12px',
        boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)',
        border: '1px solid #e5e7eb'
      }}>
        <div>
          <h1 className="page-title">
            <i className="fas fa-function" style={{ color: '#3b82f6' }}></i>
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

      {/* 통계 정보 */}
      <div className="stats-panel">
        <div className="stat-card">
          <div className="stat-value">{virtualPoints.length}</div>
          <div className="stat-label">전체 가상포인트</div>
        </div>
        <div className="stat-card status-active">
          <div className="stat-value">{virtualPoints.filter(p => p.is_enabled).length}</div>
          <div className="stat-label">활성</div>
        </div>
        <div className="stat-card status-disabled">
          <div className="stat-value">{virtualPoints.filter(p => !p.is_enabled).length}</div>
          <div className="stat-label">비활성</div>
        </div>
        <div className="stat-card status-error">
          <div className="stat-value">{virtualPoints.filter(p => p.calculation_status === 'error').length}</div>
          <div className="stat-label">오류</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{paginatedData.totalCount}</div>
          <div className="stat-label">필터 결과</div>
        </div>
      </div>
      
      {/* 필터 패널 및 뷰 토글 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'end',
        padding: '20px',
        background: 'white',
        borderRadius: '12px',
        marginBottom: '20px',
        boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)',
        border: '1px solid #e5e7eb',
        flexWrap: 'wrap',
        gap: '16px'
      }}>
        {/* 왼쪽 필터들 */}
        <div style={{ 
          display: 'flex', 
          gap: '16px', 
          alignItems: 'end', 
          flex: 1, 
          minWidth: '600px' 
        }}>
          {/* 검색 필터 */}
          <div style={{ 
            display: 'flex', 
            flexDirection: 'column', 
            gap: '8px', 
            flex: 1, 
            minWidth: '200px' 
          }}>
            <label style={{ fontSize: '12px', fontWeight: 600, color: '#334155' }}>검색</label>
            <div style={{ position: 'relative' }}>
              <input
                type="text"
                placeholder="이름, 설명, 수식 검색..."
                value={searchFilter}
                onChange={(e) => {
                  setSearchFilter(e.target.value);
                  setCurrentPage(1);
                }}
                style={{ 
                  padding: '8px 36px 8px 12px', 
                  border: '1px solid #d1d5db', 
                  borderRadius: '6px', 
                  background: 'white', 
                  fontSize: '12px',
                  width: '100%'
                }}
              />
              <i className="fas fa-search" style={{
                position: 'absolute',
                right: '12px',
                top: '50%',
                transform: 'translateY(-50%)',
                color: '#9ca3af',
                fontSize: '14px'
              }}></i>
            </div>
          </div>

          {/* 카테고리, 상태, 범위 필터들 */}
          {[
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
                { value: 'calculating', label: '계산중' },
                { value: 'disabled', label: '비활성' }
              ]
            },
            {
              label: '상태',
              value: enabledFilter,
              onChange: setEnabledFilter,
              options: [
                { value: 'all', label: '전체' },
                { value: 'true', label: '활성' },
                { value: 'false', label: '비활성' }
              ]
            }
          ].map((filter, index) => (
            <div key={index} style={{ 
              display: 'flex', 
              flexDirection: 'column', 
              gap: '8px', 
              minWidth: '120px' 
            }}>
              <label style={{ fontSize: '12px', fontWeight: 600, color: '#334155' }}>
                {filter.label}
              </label>
              <select
                value={filter.value}
                onChange={(e) => {
                  filter.onChange(e.target.value);
                  setCurrentPage(1);
                }}
                style={{ 
                  padding: '8px 12px', 
                  border: '1px solid #d1d5db', 
                  borderRadius: '6px', 
                  background: 'white', 
                  fontSize: '12px' 
                }}
              >
                {filter.options.map(option => (
                  <option key={option.value} value={option.value}>
                    {option.label}
                  </option>
                ))}
              </select>
            </div>
          ))}
          
          {/* 초기화 버튼 */}
          <button
            onClick={handleClearFilters}
            disabled={getActiveFilterCount() === 0}
            className="btn-outline"
            style={{
              opacity: getActiveFilterCount() === 0 ? 0.6 : 1,
            }}
          >
            <i className="fas fa-redo-alt"></i>
            초기화 {getActiveFilterCount() > 0 && `(${getActiveFilterCount()})`}
          </button>
        </div>

        {/* 오른쪽 뷰 토글 */}
        <div className="view-toggle">
          <button
            onClick={() => setViewMode('card')}
            className={viewMode === 'card' ? 'active' : ''}
          >
            <i className="fas fa-th-large"></i>
            카드
          </button>
          <button
            onClick={() => setViewMode('table')}
            className={viewMode === 'table' ? 'active' : ''}
          >
            <i className="fas fa-table"></i>
            테이블
          </button>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="error-banner">
          <i className="fas fa-exclamation-triangle"></i>
          <span>{error}</span>
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 가상포인트 목록 */}
      <div className="points-list">
        {paginatedData.points.length === 0 ? (
          <div className="empty-state">
            <i className="fas fa-function"></i>
            <h3>
              {getActiveFilterCount() > 0 
                ? '필터 조건에 맞는 가상포인트가 없습니다' 
                : '가상포인트가 없습니다'}
            </h3>
            <p>
              {getActiveFilterCount() > 0 
                ? '필터 조건을 변경하거나 초기화해보세요.' 
                : '첫 번째 가상포인트를 생성해보세요.'}
            </p>
            {getActiveFilterCount() > 0 ? (
              <button className="btn-secondary" onClick={handleClearFilters}>
                <i className="fas fa-redo"></i>
                필터 초기화
              </button>
            ) : (
              <button className="btn-primary" onClick={openCreateModal}>
                <i className="fas fa-plus"></i>
                가상포인트 생성
              </button>
            )}
          </div>
        ) : viewMode === 'card' ? (
          <div className="points-grid">
            {paginatedData.points.map(point => (
              <VirtualPointCard
                key={point.id}
                virtualPoint={point as any} // 임시 타입 변환
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
            virtualPoints={paginatedData.points as any} // 임시 타입 변환
            onEdit={openEditModal}
            onDelete={openDeleteConfirm}
            onTest={openTestModal}
            onExecute={handleExecutePoint}
            onToggleEnabled={handleToggleEnabled}
            sortBy="name"
            sortOrder="asc"
            onSort={handleSortChange}
          />
        )}
      </div>

      {/* 페이징 - 항상 표시 (1개 이상이면) */}
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

      {/* 모달들 */}
      {showCreateModal && (
        <VirtualPointModal
          isOpen={showCreateModal}
          mode="create"
          onSave={handleCreateVirtualPoint}
          onClose={closeModals}
          loading={saving}
        />
      )}

      {showEditModal && selectedPoint && (
        <VirtualPointModal
          isOpen={showEditModal}
          mode="edit"
          virtualPoint={selectedPoint as any} // 임시 타입 변환
          onSave={handleUpdateVirtualPoint}
          onClose={closeModals}
          loading={saving}
        />
      )}

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

      {/* FormulaHelper 모달 - 스크립트 엔진 함수가 로드된 경우에만 표시 */}
      {showFormulaHelper && (
        <FormulaHelper
          isOpen={showFormulaHelper}
          onClose={() => setShowFormulaHelper(false)}
          onInsertFunction={handleInsertFunction}
          functions={scriptEngine.functions || []}
        />
      )}
    </div>
  );
};

export default VirtualPoints;