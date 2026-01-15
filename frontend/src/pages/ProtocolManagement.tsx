// ============================================================================
// frontend/src/pages/ProtocolManagement.tsx
// 프로토콜 관리 메인 페이지 - 공통 UI 시스템 적용 버전
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { ProtocolApiService } from '../api/services/protocolApi';
import ProtocolEditor from '../components/modals/ProtocolModal/ProtocolEditor';
import ProtocolDetailModal from '../components/modals/ProtocolModal/ProtocolDetailModal';
import { Pagination } from '../components/common/Pagination';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { ProtocolDevicesModal } from '../components/modals/ProtocolModal/ProtocolDevicesModal';
import '../styles/management.css';
import '../styles/protocol-management.css';

// 프로토콜 데이터 타입
interface Protocol {
  id: number;
  protocol_type: string;
  display_name: string;
  description: string;
  category?: string;
  default_port?: number;
  uses_serial?: boolean;
  requires_broker?: boolean;
  supported_operations?: string[];
  supported_data_types?: string[];
  connection_params?: Record<string, any>;
  default_polling_interval?: number;
  default_timeout?: number;
  max_concurrent_connections?: number;
  vendor?: string;
  is_enabled: boolean;
  is_deprecated: boolean;
  device_count?: number;
  enabled_count?: number;
  connected_count?: number;
}

interface ProtocolStats {
  total_protocols: number;
  enabled_protocols: number;
  deprecated_protocols: number;
  categories: Array<{
    category: string;
    count: number;
  }>;
}

const ProtocolManagement: React.FC = () => {
  const [protocols, setProtocols] = useState<Protocol[]>([]);
  const [stats, setStats] = useState<ProtocolStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [viewMode, setViewMode] = useState<'card' | 'table'>('table');
  const [categoryFilter, setCategoryFilter] = useState('all');
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState('all');
  const [processing, setProcessing] = useState<number | null>(null);

  // 페이징
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(12);
  const [totalCount, setTotalCount] = useState(0);

  // 모달
  const [selectedProtocol, setSelectedProtocol] = useState<Protocol | null>(null);
  const [showDetailModal, setShowDetailModal] = useState(false);
  const [showEditor, setShowEditor] = useState<{ mode: 'create' | 'edit', protocolId?: number } | null>(null);
  const [showDevicesModal, setShowDevicesModal] = useState(false);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  const loadProtocols = useCallback(async () => {
    try {
      setLoading(true);
      const filters = {
        category: categoryFilter !== 'all' ? categoryFilter : undefined,
        enabled: statusFilter === 'enabled' ? 'true' : statusFilter === 'disabled' ? 'false' : undefined,
        search: searchTerm.trim() || undefined,
        page: currentPage,
        limit: pageSize
      };
      const response = await ProtocolApiService.getProtocols(filters);
      if (response.success && response.data) {
        const protocolsData = Array.isArray(response.data) ? response.data : (response.data as any).items || [];
        setProtocols(protocolsData);
        const pagination = (response as any).pagination;
        setTotalCount(pagination?.total_count || pagination?.totalItems || protocolsData.length);
      }
    } catch (err) {
      setError('프로토콜 데이터를 불러오지 못했습니다.');
    } finally {
      setLoading(false);
    }
  }, [categoryFilter, statusFilter, searchTerm, currentPage, pageSize]);

  const loadStats = useCallback(async () => {
    try {
      const response = await ProtocolApiService.getProtocolStatistics();
      if (response.success) setStats(response.data);
    } catch (err) { console.warn('통계 로드 실패'); }
  }, []);

  useEffect(() => { loadProtocols(); loadStats(); }, [loadProtocols, loadStats]);

  useEffect(() => {
    if (successMessage) {
      const timer = setTimeout(() => setSuccessMessage(null), 3000);
      return () => clearTimeout(timer);
    }
  }, [successMessage]);

  const handleProtocolAction = async (action: string, protocolId: number) => {
    const protocol = protocols.find(p => p.id === protocolId);
    if (!protocol) return;

    try {
      setProcessing(protocolId);
      if (action === 'enable' || action === 'disable') {
        const response = await ProtocolApiService.updateProtocol(protocolId, { is_enabled: action === 'enable' });
        if (response.success) {
          setSuccessMessage(`프로토콜 "${protocol.display_name}"이(가) ${action === 'enable' ? '활성화' : '비활성화'}되었습니다.`);
          await loadProtocols();
        }
      } else if (action === 'test') {
        const response = await ProtocolApiService.testProtocolConnection(protocolId, {});
        if (response.success) setSuccessMessage(response.data?.test_successful ? '연결 성공' : '연결 실패');
      }
    } catch (err) { setError('작업 수행 중 오류가 발생했습니다.'); }
    finally { setProcessing(null); }
  };

  const openEditor = (mode: 'create' | 'edit', id?: number) => setShowEditor({ mode, protocolId: id });
  const openDetail = (protocol: Protocol) => { setSelectedProtocol(protocol); setShowDetailModal(true); };
  const openDevicesPopup = (protocol: Protocol) => { setSelectedProtocol(protocol); setShowDevicesModal(true); };

  const handlePageChange = (page: number, size: number) => {
    setCurrentPage(page);
    if (size !== pageSize) { setPageSize(size); setCurrentPage(1); }
  };

  if (loading && protocols.length === 0) {
    return <ManagementLayout><div className="loading-container">로딩 중...</div></ManagementLayout>;
  }

  return (
    <ManagementLayout>
      <PageHeader
        title="프로토콜 관리"
        description="다양한 산업용 통신 프로토콜을 관리하고 드라이버를 구성합니다."
        icon="fas fa-microchip"
      />

      {successMessage && <div className="success-banner">{successMessage}</div>}

      <div className="mgmt-stats-panel">
        <StatCard label="전체 프로토콜" value={stats?.total_protocols || 0} type="primary" />
        <StatCard label="활성 프로토콜" value={stats?.enabled_protocols || 0} type="success" />
        <StatCard label="사용 중 디바이스" value={protocols.reduce((sum, p) => sum + (p.device_count || 0), 0)} type="neutral" />
        <StatCard label="전체 카테고리" value={stats?.categories?.length || 0} type="warning" />
      </div>

      <FilterBar
        searchTerm={searchTerm}
        onSearchChange={setSearchTerm}
        onReset={() => {
          setSearchTerm('');
          setCategoryFilter('all');
          setStatusFilter('all');
        }}
        activeFilterCount={(searchTerm ? 1 : 0) + (categoryFilter !== 'all' ? 1 : 0) + (statusFilter !== 'all' ? 1 : 0)}
        filters={[
          {
            label: '카테고리',
            value: categoryFilter,
            onChange: setCategoryFilter,
            options: [
              { value: 'all', label: '전체' },
              ...(stats?.categories?.map(c => ({ value: c.category, label: c.category })) || [])
            ]
          },
          {
            label: '상태',
            value: statusFilter,
            onChange: setStatusFilter,
            options: [
              { value: 'all', label: '전체' },
              { value: 'enabled', label: '활성' },
              { value: 'disabled', label: '비활성' }
            ]
          }
        ]}
        rightActions={
          <div className="view-toggle">
            <button
              className={`btn-icon ${viewMode === 'card' ? 'active' : ''}`}
              onClick={() => setViewMode('card')}
              title="카드 보기"
            >
              <i className="fas fa-th-large"></i>
            </button>
            <button
              className={`btn-icon ${viewMode === 'table' ? 'active' : ''}`}
              onClick={() => setViewMode('table')}
              title="리스트 보기"
            >
              <i className="fas fa-list"></i>
            </button>
          </div>
        }
      />

      <div className="mgmt-content-area">
        {viewMode === 'card' ? (
          <div className="mgmt-grid">
            {protocols.map(protocol => (
              <div key={protocol.id} className="mgmt-card protocol-card">
                <div className="card-header">
                  <div className="card-title">
                    <h4
                      onClick={() => openDetail(protocol)}
                      className="clickable-name"
                    >
                      {protocol.display_name}
                    </h4>
                    <span className="badge">{protocol.protocol_type}</span>
                  </div>
                  <div className="card-actions">
                    <button onClick={() => openDetail(protocol)} title="상세보기"><i className="fas fa-eye"></i></button>
                    <button onClick={() => openEditor('edit', protocol.id)} title="편집"><i className="fas fa-edit"></i></button>
                  </div>
                </div>
                <div className="card-body">
                  <p>{protocol.description}</p>
                  <div className="card-meta">
                    <span><i className="fas fa-layer-group"></i> {protocol.category}</span>
                    <span
                      className="mgmt-table-id-link"
                      onClick={() => openDevicesPopup(protocol)}
                    >
                      <i className="fas fa-network-wired"></i> {protocol.device_count || 0} devices
                    </span>
                  </div>
                </div>
                <div className="card-footer">
                  <button
                    className={protocol.is_enabled ? 'btn-status active' : 'btn-status disabled'}
                    onClick={() => handleProtocolAction(protocol.is_enabled ? 'disable' : 'enable', protocol.id)}
                    disabled={processing === protocol.id}
                  >
                    {protocol.is_enabled ? '활성' : '비활성'}
                  </button>
                  <button className="btn-outline" onClick={() => handleProtocolAction('test', protocol.id)} disabled={processing === protocol.id}>테스트</button>
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div className="mgmt-table-container">
            <table className="mgmt-table">
              <thead>
                <tr>
                  <th>프로토콜명</th>
                  <th>타입</th>
                  <th>카테고리</th>
                  <th>제조사/벤더</th>
                  <th>기본 포트</th>
                  <th>사용 디바이스</th>
                  <th>상태</th>
                </tr>
              </thead>
              <tbody>
                {protocols.map(protocol => (
                  <tr key={protocol.id}>
                    <td>
                      <div
                        className="mgmt-table-id-link"
                        onClick={() => openDetail(protocol)}
                        style={{ fontWeight: '600', fontSize: '14px' }}
                      >
                        {protocol.display_name}
                      </div>
                    </td>
                    <td>
                      <span className="badge">{protocol.protocol_type}</span>
                    </td>
                    <td>{protocol.category}</td>
                    <td>
                      <span style={{ fontSize: '13px', color: 'var(--neutral-600)' }}>
                        {protocol.vendor || '-'}
                      </span>
                    </td>
                    <td>
                      <span style={{ fontFamily: 'monospace', fontWeight: '500' }}>
                        {protocol.default_port || '-'}
                      </span>
                    </td>
                    <td>
                      <div
                        className="mgmt-table-id-link"
                        onClick={() => openDevicesPopup(protocol)}
                        style={{ display: 'inline-flex', alignItems: 'center', fontWeight: '600' }}
                      >
                        <i className="fas fa-network-wired" style={{ marginRight: '6px', color: 'var(--primary-400)' }}></i>
                        {protocol.device_count || 0}
                      </div>
                    </td>
                    <td>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <span className={`badge ${protocol.is_enabled ? 'success' : 'neutral'}`}>
                          {protocol.is_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      <Pagination current={currentPage} total={totalCount} pageSize={pageSize} onChange={handlePageChange} />

      {showEditor && (
        <ProtocolEditor
          isOpen={!!showEditor}
          mode={showEditor.mode}
          protocolId={showEditor.protocolId}
          onSave={async () => { await loadProtocols(); setShowEditor(null); }}
          onCancel={() => setShowEditor(null)}
        />
      )}
      {showDetailModal && selectedProtocol && (
        <ProtocolDetailModal
          isOpen={showDetailModal}
          protocol={selectedProtocol as any}
          onClose={() => setShowDetailModal(false)}
          onEdit={() => openEditor('edit', selectedProtocol.id)}
        />
      )}
      {showDevicesModal && selectedProtocol && (
        <ProtocolDevicesModal
          isOpen={showDevicesModal}
          protocol={selectedProtocol as any}
          onClose={() => setShowDevicesModal(false)}
        />
      )}
    </ManagementLayout>
  );
};

export default ProtocolManagement;