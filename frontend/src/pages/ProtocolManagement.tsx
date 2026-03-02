// ============================================================================
// frontend/src/pages/ProtocolManagement.tsx
// 프로토콜 관리 메인 페이지 - 공통 UI 시스템 적용 버전
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { useParams, useNavigate } from 'react-router-dom';
import { ProtocolApiService } from '../api/services/protocolApi';
import ProtocolEditor from '../components/modals/ProtocolModal/ProtocolEditor';
import ProtocolDetailModal from '../components/modals/ProtocolModal/ProtocolDetailModal';
import { Pagination } from '../components/common/Pagination';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { ProtocolDevicesModal } from '../components/modals/ProtocolModal/ProtocolDevicesModal';
import ProtocolDashboard from '../components/protocols/ProtocolDashboard';
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
  const { t } = useTranslation(['protocols', 'common']);
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

  // Dashboard View State (URL 기반으로 동기화)
  const { type: urlProtocolType, id: urlProtocolId, tab: urlTab } = useParams();
  const navigate = useNavigate();
  const selectedDashboardId = urlProtocolId ? parseInt(urlProtocolId) : null;

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
      setError(t('protocols:message.loadError'));
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
          setSuccessMessage(t(action === 'enable' ? 'protocols:message.activated' : 'protocols:message.deactivated', { name: protocol.display_name }));
          await loadProtocols();
        }
      } else if (action === 'test') {
        const response = await ProtocolApiService.testProtocolConnection(protocolId, {});
        if (response.success) setSuccessMessage(response.data?.test_successful ? t('protocols:message.testSuccess') : t('protocols:message.testFail'));
      }
    } catch (err) { setError(t('protocols:message.actionError')); }
    finally { setProcessing(null); }
  };

  const openEditor = (mode: 'create' | 'edit', id?: number) => setShowEditor({ mode, protocolId: id });
  const openDetail = (protocol: Protocol) => {
    // 모든 프로토콜에 대해 통합 대시보드로 이동
    navigate(`/protocols/${protocol.protocol_type.toLowerCase()}/${protocol.id}`);
  };
  const openDevicesPopup = (protocol: Protocol) => { setSelectedProtocol(protocol); setShowDevicesModal(true); };

  const handlePageChange = (page: number, size: number) => {
    setCurrentPage(page);
    if (size !== pageSize) { setPageSize(size); setCurrentPage(1); }
  };

  if (loading && protocols.length === 0) {
    return <ManagementLayout><div className="loading-container">{t('protocols:loading')}</div></ManagementLayout>;
  }

  // Dashboard View Render
  if (selectedDashboardId) {
    const protocol = protocols.find(p => p.id === selectedDashboardId);

    // 데이터 로딩 중이거나 프로토콜을 찾지 못한 경우 처리
    if (loading && !protocol) {
      return <ManagementLayout><div className="loading-container">{t('protocols:loadingDashboard')}</div></ManagementLayout>;
    }

    if (!protocol && !loading) {
      return (
        <ManagementLayout>
          <div style={{ textAlign: 'center', padding: '100px' }}>
            <h3>{t('protocols:notFound')}</h3>
            <button className="mgmt-btn" onClick={() => navigate('/protocols')}>{t('protocols:backToList')}</button>
          </div>
        </ManagementLayout>
      );
    }
    return (
      <ManagementLayout>
        <div className="dashboard-view-container" style={{ marginBottom: '24px' }}>
          <div className="mgmt-back-nav" style={{ marginBottom: '16px' }}>
            <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => navigate('/protocols')}>
              <i className="fas fa-arrow-left"></i> {t('protocols:backToProtocolList')}
            </button>
          </div>
          <ProtocolDashboard
            protocol={protocol}
            onRefresh={() => { loadProtocols(); loadStats(); }}
          />
        </div>
      </ManagementLayout>
    );
  }

  return (
    <ManagementLayout>
      <PageHeader
        title={t('protocols:title')}
        description={t('protocols:description')}
        icon="fas fa-microchip"
      />

      {successMessage && <div className="success-banner">{successMessage}</div>}

      <div className="mgmt-stats-panel">
        <StatCard label={t('protocols:stats.totalProtocols')} value={stats?.total_protocols || 0} type="primary" />
        <StatCard label={t('protocols:stats.activeProtocols')} value={stats?.enabled_protocols || 0} type="success" />
        <StatCard label={t('protocols:stats.devicesInUse')} value={protocols.reduce((sum, p) => sum + (p.device_count || 0), 0)} type="neutral" />
        <StatCard label={t('protocols:stats.totalCategories')} value={stats?.categories?.length || 0} type="warning" />
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
            label: t('protocols:filter.category'),
            value: categoryFilter,
            onChange: setCategoryFilter,
            options: [
              { value: 'all', label: t('protocols:filter.all') },
              ...(stats?.categories?.map(c => ({ value: c.category, label: c.category })) || [])
            ]
          },
          {
            label: t('protocols:filter.status'),
            value: statusFilter,
            onChange: setStatusFilter,
            options: [
              { value: 'all', label: t('protocols:filter.all') },
              { value: 'enabled', label: t('protocols:filter.active') },
              { value: 'disabled', label: t('protocols:filter.inactive') }
            ]
          }
        ]}
        rightActions={
          <div className="mgmt-view-toggle">
            <button
              className={`mgmt-btn-icon ${viewMode === 'card' ? 'active' : ''}`}
              onClick={() => setViewMode('card')}
              title={t('protocols:view.card')}
            >
              <i className="fas fa-th-large"></i>
            </button>
            <button
              className={`mgmt-btn-icon ${viewMode === 'table' ? 'active' : ''}`}
              onClick={() => setViewMode('table')}
              title={t('protocols:view.list')}
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
              <div key={protocol.id} className="mgmt-card">
                <div className="mgmt-card-header">
                  <div className="mgmt-card-title">
                    <h4
                      onClick={() => openDetail(protocol)}
                      className="mgmt-card-name"
                    >
                      {protocol.display_name}
                    </h4>
                    <span className="mgmt-badge">{protocol.protocol_type}</span>
                  </div>
                  <div className="mgmt-card-actions">
                    <button className="mgmt-btn-icon" onClick={() => openDetail(protocol)} title={t('protocols:action.detail')}><i className="fas fa-eye"></i></button>
                  </div>
                </div>
                <div className="mgmt-card-body">
                  <p className="mgmt-card-desc">{protocol.description}</p>
                  <div className="mgmt-card-meta">
                    <span><i className="fas fa-layer-group"></i> {protocol.category}</span>
                    <span
                      className="mgmt-table-id-link"
                      onClick={() => openDevicesPopup(protocol)}
                    >
                      <i className="fas fa-network-wired"></i> {protocol.device_count || 0} devices
                    </span>
                  </div>
                </div>
                <div className="mgmt-card-footer">
                  <button
                    className={`mgmt-btn-status ${protocol.is_enabled ? 'active' : 'disabled'}`}
                    onClick={() => handleProtocolAction(protocol.is_enabled ? 'disable' : 'enable', protocol.id)}
                    disabled={processing === protocol.id}
                  >
                    {protocol.is_enabled ? t('protocols:badge.active') : t('protocols:badge.inactive')}
                  </button>
                  <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => handleProtocolAction('test', protocol.id)} disabled={processing === protocol.id}>{t('protocols:action.test')}</button>
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div className="mgmt-table-container">
            <table className="mgmt-table">
              <thead>
                <tr>
                  <th>{t('protocols:table.name')}</th>
                  <th>{t('protocols:table.type')}</th>
                  <th>{t('protocols:table.category')}</th>
                  <th>{t('protocols:table.vendor')}</th>
                  <th>{t('protocols:table.defaultPort')}</th>
                  <th>{t('protocols:table.devicesUsed')}</th>
                  <th>{t('protocols:table.status')}</th>
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
                      <span className="mgmt-badge">{protocol.protocol_type}</span>
                    </td>
                    <td>{protocol.category}</td>
                    <td>
                      <span style={{ fontSize: '13px', color: 'var(--neutral-600)' }}>
                        {protocol.vendor || '-'}
                      </span>
                    </td>
                    <td>
                      <span style={{ fontWeight: '500' }}>
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
                        <span className={`mgmt-badge ${protocol.is_enabled ? 'success' : 'neutral'}`}>
                          {protocol.is_enabled ? t('protocols:badge.active') : t('protocols:badge.inactive')}
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