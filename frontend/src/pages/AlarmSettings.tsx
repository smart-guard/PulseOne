import React, { useState, useEffect, useCallback } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { AlarmApiService, AlarmRule } from '../api/services/alarmApi';
import { DataApiService } from '../api/services/dataApi';
import { DeviceApiService } from '../api/services/deviceApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { useTranslation } from 'react-i18next';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import AlarmCreateEditModal from '../components/modals/AlarmCreateEditModal';
import AlarmDetailModal from '../components/modals/AlarmDetailModal';
import '../styles/management.css';
import '../styles/alarm-settings.css';
import '../styles/alarm-management.css';

interface AlarmSettingsProps { }

interface DataPoint {
  id: number;
  name: string;
  device_id: number;
  device_name: string;
  data_type: string;
  unit?: string;
  address?: string;
}

interface Device {
  id: number;
  name: string;
  device_type: string;
  site_name?: string;
}

const AlarmSettings: React.FC<AlarmSettingsProps> = () => {
  const { confirm } = useConfirmContext();
  const { t } = useTranslation('alarms');
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [toggleLoading, setToggleLoading] = useState<Set<number>>(new Set());
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [devices, setDevices] = useState<Device[]>([]);
  const [loadingTargetData, setLoadingTargetData] = useState(false);
  const [showModal, setShowModal] = useState(false);
  const [showDetailModal, setShowDetailModal] = useState(false);
  const [modalMode, setModalMode] = useState<'create' | 'edit'>('create');
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  const { view } = useParams<{ view: string }>();
  const navigate = useNavigate();
  const viewType = (view === 'card' || view === 'table') ? view : 'table';

  const setViewType = (newView: 'card' | 'table') => {
    navigate(`/alarms/settings/${newView}`);
  };

  useEffect(() => {
    if (view && view !== 'card' && view !== 'table') {
      navigate('/alarms/settings/table', { replace: true });
    }
  }, [view, navigate]);
  const [filters, setFilters] = useState<{
    search: string;
    severity: string;
    status: string;
    category: string;
    tag: string;
    deleted?: boolean;
  }>({ search: '', severity: 'all', status: 'all', category: 'all', tag: '', deleted: false });

  // Pagination states
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(12);
  const [totalCount, setTotalCount] = useState(0);

  const loadAlarmRules = useCallback(async () => {
    try {
      setLoading(true);
      const response = await AlarmApiService.getAlarmRules({
        page: currentPage,
        limit: pageSize,
        search: filters.search || undefined,
        severity: filters.severity === 'all' ? undefined : filters.severity,
        enabled: filters.status === 'all' ? undefined : (filters.status === 'enabled'),
        category: filters.category === 'all' ? undefined : filters.category,
        deleted: filters.deleted,
      });
      if (response.success && response.data) {
        setAlarmRules((response.data as any).items || []);
        // FIX: properties are 'total', not 'total_count'
        const pag = (response.data as any).pagination;
        setTotalCount(pag?.total || pag?.totalCount || 0);
      }
    } catch (error) { setError('Data load failed'); }
    finally { setLoading(false); }
  }, [currentPage, pageSize, filters]);

  const loadTargetData = useCallback(async () => {
    try {
      setLoadingTargetData(true);
      const [pts, devs] = await Promise.all([
        DataApiService.searchDataPoints({ page: 1, limit: 1000, include_current_value: false }),
        DeviceApiService.getDevices({ page: 1, limit: 100 })
      ]);
      if (pts.success) setDataPoints((pts.data as any).items || []);
      if (devs.success) setDevices((devs.data as any).items || []);
    } finally { setLoadingTargetData(false); }
  }, []);

  useEffect(() => {
    loadAlarmRules();
    loadTargetData();
  }, [loadAlarmRules, loadTargetData]);

  const handleToggleRule = async (ruleId: number, currentStatus: boolean, ruleName: string) => {
    // Prevent bubbling if called from row click
    const newStatus = !currentStatus;
    setToggleLoading(prev => new Set([...prev, ruleId]));
    try {
      const response = await AlarmApiService.toggleAlarmRule(ruleId, newStatus);
      if (response.success) setAlarmRules(prev => prev.map(r => r.id === ruleId ? { ...r, is_enabled: newStatus } : r));
    } finally {
      setToggleLoading(prev => { const n = new Set(prev); n.delete(ruleId); return n; });
    }
  };

  const handleDeleteRule = async (ruleId: number, ruleName: string) => {
    const confirmed = await confirm({
      title: 'Delete Alarm Rule',
      message: `Really delete alarm rule: ${ruleName}?`,
      confirmText: 'Delete',
      confirmButtonType: 'danger'
    });
    if (confirmed) {
      const response = await AlarmApiService.deleteAlarmRule(ruleId);
      if (response.success) {
        loadAlarmRules();
        setShowModal(false);
        setShowDetailModal(false);
        await confirm({
          title: 'Delete Complete',
          message: 'Alarm rule deleted successfully.',
          confirmText: 'OK',
          showCancelButton: false,
          confirmButtonType: 'primary'
        });
      }
    }
  };

  const handleRestoreRule = async (ruleId: number, ruleName: string) => {
    const confirmed = await confirm({
      title: 'Restore Alarm Rule',
      message: `Restore alarm rule: ${ruleName}?`,
      confirmText: 'Restore',
      confirmButtonType: 'primary'
    });
    if (confirmed) {
      const response = await AlarmApiService.restoreAlarmRule(ruleId);
      if (response.success) {
        loadAlarmRules();
        setShowModal(false);
        setShowDetailModal(false);
        await confirm({
          title: 'Restore Complete',
          message: 'Alarm rule restored successfully.',
          confirmText: 'OK',
          showCancelButton: false,
          confirmButtonType: 'primary'
        });
      }
    }
  };

  const handleCreateRule = () => { setModalMode('create'); setSelectedRule(null); setShowModal(true); };
  const handleEditRule = (rule: AlarmRule) => {
    setSelectedRule(rule);
    setModalMode('edit');
    setShowDetailModal(false);
    setShowModal(true);
  };

  const handleShowDetail = (rule: AlarmRule) => {
    setSelectedRule(rule);
    setShowDetailModal(true);
  };

  const getTargetDisplay = (rule: AlarmRule) => rule.target_display || rule.device_name || `Target #${rule.target_id}`;
  const getSeverityLabel = (s: string) => s.toUpperCase();
  const getSeverityBadgeClass = (s: string) => {
    switch (s) {
      case 'critical': return 'danger';
      case 'high': return 'warning';
      case 'medium': return 'primary';
      case 'low': return 'neutral';
      default: return 'neutral';
    }
  };

  return (
    <ManagementLayout className="page-alarm-settings">
      <PageHeader
        title={t('settings.title')}
        description={t('settings.description')}
        icon="fas fa-bell"
        actions={
          <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreateRule}>
            <i className="fas fa-plus"></i> {t('settings.addRule')}
          </button>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label={t('settings.totalRules')} value={totalCount} icon="fas fa-list-ul" type="primary" />
        <StatCard label={t('settings.activeRules')} value={alarmRules.filter(r => r.is_enabled).length} icon="fas fa-check-circle" type="success" />
        <StatCard label={t('settings.inactiveRules')} value={alarmRules.filter(r => !r.is_enabled).length} icon="fas fa-pause-circle" type="warning" />
        <StatCard label={t('settings.searchResults')} value={alarmRules.length} icon="fas fa-search-dollar" type="neutral" />
      </div>

      <FilterBar
        searchTerm={filters.search}
        onSearchChange={(val) => setFilters(prev => ({ ...prev, search: val }))}
        onReset={() => {
          setFilters({ search: '', severity: 'all', status: 'all', category: 'all', tag: '', deleted: false });
          setCurrentPage(1);
        }}
        activeFilterCount={(filters.search ? 1 : 0) + (filters.severity !== 'all' ? 1 : 0) + (filters.status !== 'all' ? 1 : 0) + (filters.category !== 'all' ? 1 : 0) + (filters.deleted ? 1 : 0)}
        filters={[
          {
            label: t('filter.severity'),
            value: filters.severity,
            onChange: (val) => { setFilters(prev => ({ ...prev, severity: val })); setCurrentPage(1); },
            options: [
              { value: 'all', label: t('filter.all') },
              { value: 'critical', label: 'CRITICAL' },
              { value: 'high', label: 'HIGH' },
              { value: 'medium', label: 'MEDIUM' },
              { value: 'low', label: 'LOW' }
            ]
          },
          {
            label: t('filter.status'),
            value: filters.status,
            onChange: (val) => { setFilters(prev => ({ ...prev, status: val })); setCurrentPage(1); },
            options: [
              { value: 'all', label: t('filter.all') },
              { value: 'enabled', label: t('settings.statusActive') },
              { value: 'disabled', label: t('settings.inactiveRules') }
            ]
          }
        ]}
        rightActions={
          <>
            <div style={{ display: 'flex', alignItems: 'center', height: '32px' }}>
              <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '13px', fontWeight: '500', cursor: 'pointer', whiteSpace: 'nowrap', color: 'var(--neutral-600)', margin: 0 }}>
                <input type="checkbox"
                  checked={filters.deleted || false}
                  onChange={(e) => {
                    setFilters(prev => ({ ...prev, deleted: e.target.checked }));
                    setCurrentPage(1);
                  }}
                />
                {t('settings.viewDeleted')}
              </label>
            </div>
            <div className="mgmt-view-toggle">
              <button
                className={`mgmt-btn-icon ${viewType === 'card' ? 'active' : ''}`}
                onClick={() => setViewType('card')}
                title="Card View"
              >
                <i className="fas fa-th-large"></i>
              </button>
              <button
                className={`mgmt-btn-icon ${viewType === 'table' ? 'active' : ''}`}
                onClick={() => setViewType('table')}
                title="Table View"
              >
                <i className="fas fa-list"></i>
              </button>
            </div>
          </>
        }
      />

      <div className="mgmt-content-area">
        {viewType === 'card' ? (
          <div className="mgmt-grid">
            {alarmRules.map(rule => (
              <div key={rule.id} className="mgmt-card">
                <div className="mgmt-card-header">
                  <div className="mgmt-card-title">
                    <h4 className="mgmt-card-name" onClick={() => handleShowDetail(rule)}>{rule.name}</h4>
                    <div style={{ display: 'flex', gap: '5px' }}>
                      <span className={`mgmt-badge ${getSeverityBadgeClass(rule.severity)}`}>
                        {getSeverityLabel(rule.severity)}
                      </span>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          handleToggleRule(rule.id, rule.is_enabled, rule.name);
                        }}
                        className={`mgmt-badge ${rule.is_enabled ? 'success' : 'neutral'}`}
                        style={{ cursor: 'pointer', border: 'none', display: 'flex', alignItems: 'center', gap: '4px' }}
                      >
                        {rule.is_enabled ? 'ON' : 'OFF'}
                        {toggleLoading.has(rule.id) && <i className="fas fa-spinner fa-spin"></i>}
                      </button>
                    </div>
                  </div>
                </div>
                <div className="mgmt-card-body">
                  <p className="mgmt-card-desc">{rule.description || 'No alarm description.'}</p>
                  <div className="mgmt-card-meta">
                    <span><i className="fas fa-crosshairs"></i> {getTargetDisplay(rule)}</span>
                    <span><i className="fas fa-tag"></i> {rule.category || 'general'}</span>
                  </div>
                </div>
                {/* Remove card actions as per request? Or just list? User said "Manage column not needed" which implies Table. 
                    For Cards, footer is okay? 'Manage' column usually refers to Table. 
                    I'll keep Card actions safe for now, or maybe hide them.
                    But user said "Create/Edit doesn't have Enable toggle".
                    Let's update cards to be consistent. */}
                <div className="mgmt-card-footer">
                  <div className="mgmt-card-actions">
                    <button onClick={() => handleEditRule(rule)} className="mgmt-btn mgmt-btn-outline mgmt-btn-sm">Edit</button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div className="mgmt-table-container">
            <table className="mgmt-table">
              <thead>
                <tr>
                  <th>{t('settings.colRuleName')}</th>
                  <th>{t('settings.colDevicePoint')}</th>
                  <th>{t('settings.colCategory')}</th>
                  <th>{t('settings.colSeverity')}</th>
                  <th>{t('settings.colStatus')}</th>
                  {/* Removed Management Column */}
                </tr>
              </thead>
              <tbody>
                {alarmRules.map(rule => (
                  <tr key={rule.id} onClick={() => handleShowDetail(rule)} style={{ cursor: 'pointer', transition: 'background 0.2s' }} className="hover:bg-neutral-50">
                    <td>
                      <div className="mgmt-table-id-link clickable-name" style={{ fontWeight: '600' }}>
                        {rule.name}
                      </div>
                    </td>
                    <td>{getTargetDisplay(rule)}</td>
                    <td>{rule.category || '-'}</td>
                    <td>
                      <span className={`mgmt-badge ${getSeverityBadgeClass(rule.severity)}`}>
                        {getSeverityLabel(rule.severity)}
                      </span>
                    </td>
                    <td>
                      <div className="status-indicator">
                        <button
                          onClick={(e) => {
                            e.stopPropagation();
                            handleToggleRule(rule.id, rule.is_enabled, rule.name);
                          }}
                          className={`status-pill ${rule.is_enabled ? 'success' : 'neutral'}`}
                          style={{
                            border: 'none',
                            padding: '4px 8px',
                            borderRadius: '12px',
                            fontSize: '11px',
                            fontWeight: 600,
                            cursor: 'pointer',
                            display: 'flex', alignItems: 'center', gap: '6px'
                          }}
                        >
                          <span className={`status-dot ${rule.is_enabled ? 'active' : 'inactive'}`} style={{ width: '6px', height: '6px' }}></span>
                          {rule.is_enabled ? t('settings.statusActive') : t('settings.statusPaused')}
                          {toggleLoading.has(rule.id) && <i className="fas fa-spinner fa-spin ms-1"></i>}
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      <Pagination
        current={currentPage}
        total={totalCount}
        pageSize={pageSize}
        onChange={(page) => setCurrentPage(page)}
      />

      {showDetailModal && (
        <AlarmDetailModal
          rule={selectedRule}
          onClose={() => setShowDetailModal(false)}
          onEdit={handleEditRule}
        />
      )}

      {showModal && (
        <AlarmCreateEditModal
          isOpen={showModal}
          mode={modalMode}
          rule={selectedRule || undefined}
          dataPoints={dataPoints}
          devices={devices}
          loadingTargetData={loadingTargetData}
          onClose={() => setShowModal(false)}
          onSave={loadAlarmRules}
          onDelete={handleDeleteRule}
          onRestore={handleRestoreRule}
        />
      )}
    </ManagementLayout>
  );
};

export default AlarmSettings;