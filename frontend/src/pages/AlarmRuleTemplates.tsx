import React, { useState, useEffect, useCallback } from 'react';
import alarmTemplatesApi, {
  AlarmTemplate,
  DataPoint,
  CreatedAlarmRule
} from '../api/services/alarmTemplatesApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { useTranslation } from 'react-i18next';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { Pagination } from '../components/common/Pagination';
import AlarmTemplateCreateEditModal from '../components/modals/AlarmTemplateCreateEditModal';
import AlarmTemplateDetailModal from '../components/modals/AlarmTemplateDetailModal';
import TemplateApplyModal from '../components/modals/TemplateApplyModal';
import '../styles/management.css';
import '../styles/alarm-rule-templates.css';

const AlarmRuleTemplates: React.FC = () => {
  const { confirm } = useConfirmContext();
  const { t } = useTranslation('alarms');

  // Data States
  const [templates, setTemplates] = useState<AlarmTemplate[]>([]);
  const [createdRules, setCreatedRules] = useState<CreatedAlarmRule[]>([]);
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);

  // UI States
  const [loading, setLoading] = useState(false);
  const [activeTab, setActiveTab] = useState<'browse' | 'created'>('browse');
  const [viewMode, setViewMode] = useState<'card' | 'table'>('table');
  const [showModal, setShowModal] = useState(false);
  const [showDetailModal, setShowDetailModal] = useState(false);
  const [showApplyModal, setShowApplyModal] = useState(false);
  const [modalMode, setModalMode] = useState<'create' | 'edit'>('create');
  const [selectedTemplate, setSelectedTemplate] = useState<AlarmTemplate | null>(null);

  // Filter States
  const [filters, setFilters] = useState({
    search: '',
    type: 'all',
    category: 'all',
    sort: 'newest'
  });

  // Pagination States
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(12);
  const [totalCount, setTotalCount] = useState(0);

  // ===================================================================
  // Data Loading
  // ===================================================================
  const loadTemplates = useCallback(async () => {
    try {
      setLoading(true);
      const params = {
        is_active: true,
        page: currentPage,
        limit: pageSize,
        search: filters.search || undefined,
        ...(filters.type !== 'all' && { template_type: filters.type as any }),
        ...(filters.category !== 'all' && { category: filters.category })
      };

      const response = await alarmTemplatesApi.getTemplates(params);

      if (response && response.success && response.data) {
        const items = response.data.items || [];
        setTemplates(items.map((t: any) => ({
          ...t,
          default_config: typeof t.default_config === 'string' ? JSON.parse(t.default_config) : (t.default_config || {})
        })));
        setTotalCount(response.data.pagination?.total || 0);
      }
    } catch (error) {
      console.error('Failed to load templates', error);
    } finally {
      setLoading(false);
    }
  }, [currentPage, pageSize, filters]);

  const loadCreatedRules = useCallback(async () => {
    try {
      const response = await alarmTemplatesApi.getCreatedRules({
        search: filters.search || undefined
      });
      if (response && response.success && response.data) {
        setCreatedRules(response.data.items || response.data || []);
      }
    } catch (error) {
      console.error('Failed to load rules', error);
    }
  }, [filters.search]);

  // Load initial data
  useEffect(() => {
    loadTemplates();
    loadCreatedRules();
  }, []);

  // Load data when tab changes
  useEffect(() => {
    if (activeTab === 'browse') {
      loadTemplates();
    } else {
      loadCreatedRules();
    }
  }, [activeTab, loadTemplates, loadCreatedRules]);

  // ===================================================================
  // Handlers
  // ===================================================================
  const handleCreateTemplate = () => {
    setModalMode('create');
    setSelectedTemplate(null);
    setShowModal(true);
  };

  const handleEditTemplate = (template: AlarmTemplate) => {
    setSelectedTemplate(template);
    setModalMode('edit');
    setShowDetailModal(false); // Close detail if editing
    setShowModal(true);
  };

  const handleViewTemplate = (template: AlarmTemplate) => {
    setSelectedTemplate(template);
    setShowDetailModal(true);
  };

  const handleApplyTemplate = (template: AlarmTemplate) => {
    setSelectedTemplate(template);
    setShowApplyModal(true);
  };

  const handleDeleteTemplate = async (id: number, name: string) => {
    const isConfirmed = await confirm({
      title: 'Delete Template',
      message: `Delete template "${name}"?`,
      confirmText: 'Delete',
      confirmButtonType: 'danger'
    });

    if (isConfirmed) {
      try {
        await alarmTemplatesApi.deleteTemplate(id);
        loadTemplates();
        setShowModal(false);
      } catch (error) {
        alert('Delete failed.');
      }
    }
  };

  const handleExportTemplates = () => {
    try {
      const exportData = {
        version: '1.0',
        exportDate: new Date().toISOString(),
        templates: templates
      };

      const dataStr = JSON.stringify(exportData, null, 2);
      const dataBlob = new Blob([dataStr], { type: 'application/json' });
      const url = URL.createObjectURL(dataBlob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `alarm-templates-${new Date().toISOString().split('T')[0]}.json`;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      URL.revokeObjectURL(url);
    } catch (error) {
      console.error('Export failed:', error);
      alert('Export failed.');
    }
  };

  const handleImportTemplates = () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = async (e: any) => {
      const file = e.target.files?.[0];
      if (!file) return;

      try {
        const text = await file.text();
        const importData = JSON.parse(text);

        if (!importData.templates || !Array.isArray(importData.templates)) {
          alert('Invalid template file format.');
          return;
        }

        const isConfirmed = await confirm({
          title: 'Import Template',
          message: `Import ${importData.templates.length} template(s)?`,
          confirmText: 'Import',
          confirmButtonType: 'primary'
        });

        if (isConfirmed) {
          // 여기서는 각 템플릿을 생성하는 API를 호출해야 합니다
          // 현재는 알림만 표시
          alert(`Imported ${importData.templates.length} template(s).\n(Full implementation requires backend API integration.)`);
          loadTemplates();
        }
      } catch (error) {
        console.error('Import failed:', error);
        alert('Import failed. Please check the file format.');
      }
    };
    input.click();
  };

  const renderTemplateConfig = (template: AlarmTemplate) => {
    const { condition_type, default_config: config } = template;
    if (!config) return '-';

    switch (condition_type) {
      case 'threshold':
        return `value > ${config.high_limit || '?'}`;
      case 'range':
        return `${config.low_limit || '?'} < value < ${config.high_limit || '?'}`;
      case 'digital':
        return `value == ${config.trigger_condition || '?'}`;
      case 'script':
        return 'Custom Script';
      default:
        return 'Complex Logic';
    }
  };

  const getSeverityBadgeClass = (s: string) => {
    const severity = s?.toLowerCase();
    if (severity === 'critical') return 'danger';
    if (severity === 'high') return 'warning';
    if (severity === 'medium') return 'primary';
    return 'neutral';
  };

  // ===================================================================
  // Render
  // ===================================================================
  return (
    <ManagementLayout>
      <PageHeader
        title={t('templates.title')}
        description={t('templates.description')}
        icon="fas fa-magic"
        actions={
          <div className="mgmt-page-actions" style={{ display: 'flex', flexDirection: 'row', gap: '8px' }}>
            <button className="mgmt-btn mgmt-btn-outline" onClick={handleExportTemplates}>
              <i className="fas fa-file-export"></i> {t('templates.export')}
            </button>
            <button className="mgmt-btn mgmt-btn-outline" onClick={handleImportTemplates}>
              <i className="fas fa-file-import"></i> {t('templates.import')}
            </button>
            <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreateTemplate}>
              <i className="fas fa-plus"></i> {t('templates.createNew')}
            </button>
          </div>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard label={t('templates.totalTemplates')} value={activeTab === 'browse' ? totalCount : templates.length} icon="fas fa-layer-group" type="blueprint" />
        <StatCard label={t('templates.appliedRules')} value={createdRules.length} icon="fas fa-check-double" type="success" />
        <StatCard label={t('templates.mostUsed')} value="High Temp Alert" icon="fas fa-star" type="warning" />
        <StatCard label={t('templates.activeStatus')} value="Excellent" icon="fas fa-heartbeat" type="neutral" />
      </div>

      <div className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'browse' ? 'active' : ''}`}
          onClick={() => setActiveTab('browse')}
        >
          <i className="fas fa-search"></i> {t('templates.tabLibrary')}
          <span className="tab-badge">{totalCount}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'created' ? 'active' : ''}`}
          onClick={() => setActiveTab('created')}
        >
          <i className="fas fa-clipboard-list"></i> {t('templates.tabApplied')}
          <span className="tab-badge">{createdRules.length}</span>
        </button>
      </div>

      <FilterBar
        searchTerm={filters.search}
        onSearchChange={(val) => setFilters(prev => ({ ...prev, search: val }))}
        onReset={() => {
          setFilters({ search: '', type: 'all', category: 'all', sort: 'newest' });
          setCurrentPage(1);
        }}
        activeFilterCount={(filters.search ? 1 : 0) + (filters.type !== 'all' ? 1 : 0) + (filters.category !== 'all' ? 1 : 0)}
        filters={activeTab === 'browse' ? [
          {
            label: t('templates.filterType'),
            value: filters.type,
            onChange: (val) => { setFilters(prev => ({ ...prev, type: val })); setCurrentPage(1); },
            options: [
              { value: 'all', label: t('templates.allTypes') },
              { value: 'simple', label: 'Simple' },
              { value: 'advanced', label: 'Advanced' },
              { value: 'script', label: 'Script' }
            ]
          },
          {
            label: t('templates.filterCategory'),
            value: filters.category,
            onChange: (val) => { setFilters(prev => ({ ...prev, category: val })); setCurrentPage(1); },
            options: [
              { value: 'all', label: t('templates.allCategories') },
              { value: 'temperature', label: 'Temperature' },
              { value: 'pressure', label: 'Pressure' },
              { value: 'flow', label: 'Flow Rate' },
              { value: 'electrical', label: 'Electrical' }
            ]
          }
        ] : []}
        rightActions={
          <div className="mgmt-view-toggle">
            <button className={`mgmt-btn-icon ${viewMode === 'card' ? 'active' : ''}`} onClick={() => setViewMode('card')}>
              <i className="fas fa-th-large"></i>
            </button>
            <button className={`mgmt-btn-icon ${viewMode === 'table' ? 'active' : ''}`} onClick={() => setViewMode('table')}>
              <i className="fas fa-list"></i>
            </button>
          </div>
        }
      />

      <div className="mgmt-content-area">
        {activeTab === 'browse' ? (
          viewMode === 'card' ? (
            <div className="mgmt-grid">
              {templates.map(template => (
                <div key={template.id} className="mgmt-card">
                  <div className="mgmt-card-body">
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
                      <h4 className="mgmt-card-name" onClick={() => handleViewTemplate(template)} style={{ margin: 0, fontSize: '1.1rem', fontWeight: 700 }}>
                        {template.name}
                      </h4>
                      <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', alignItems: 'flex-end', flexShrink: 0 }}>
                        <span className={`mgmt-badge ${getSeverityBadgeClass(template.severity)}`}>
                          {template.severity.toUpperCase()}
                        </span>
                        <span className="mgmt-badge neutral">
                          {template.category}
                        </span>
                        {template.is_system_template && (
                          <span className="mgmt-badge" style={{ backgroundColor: '#1e293b', color: '#f8fafc', border: '1px solid #334155' }}>SYSTEM</span>
                        )}
                      </div>
                    </div>
                    <p className="mgmt-card-desc">{template.description || 'No description available.'}</p>
                    <div className="mgmt-card-logic-preview">
                      <code>{renderTemplateConfig(template)}</code>
                    </div>
                    <div className="mgmt-card-meta">
                      <span><i className="fas fa-code-branch"></i> {template.template_type}</span>
                      <span><i className="fas fa-history"></i> {t('templates.usedTimes', { count: template.usage_count || 0 })}</span>
                    </div>
                  </div>
                  <div className="mgmt-card-footer">
                    <div className="mgmt-card-actions">
                      <button className="mgmt-btn mgmt-btn-primary mgmt-btn-sm" onClick={() => handleApplyTemplate(template)}>{t('templates.applyBtn')}</button>
                      {template.is_system_template ? (
                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" disabled style={{ opacity: 0.5, cursor: 'not-allowed' }}>
                          <i className="fas fa-lock" style={{ marginRight: '4px' }}></i>SYSTEM
                        </button>
                      ) : (
                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => handleEditTemplate(template)}>{t('templates.editBtn')}</button>
                      )}
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
                    <th>{t('templates.colName')}</th>
                    <th>{t('templates.colType')}</th>
                    <th>{t('templates.colCategory')}</th>
                    <th>{t('templates.colLogic')}</th>
                    <th>{t('templates.colSeverity')}</th>
                    <th>{t('templates.colUsage')}</th>
                  </tr>
                </thead>
                <tbody>
                  {templates.map(template => (
                    <tr key={template.id} onClick={() => handleViewTemplate(template)} style={{ cursor: 'pointer' }}>
                      <td className="clickable-name" style={{ fontWeight: 600 }}>{template.name}</td>
                      <td>{template.template_type}</td>
                      <td>{template.category}</td>
                      <td><code>{renderTemplateConfig(template)}</code></td>
                      <td>
                        <span className={`mgmt-badge ${getSeverityBadgeClass(template.severity)}`}>
                          {template.severity.toUpperCase()}
                        </span>
                      </td>
                      <td>{template.usage_count || 0}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )
        ) : (
          <div className="mgmt-table-container">
            <table className="mgmt-table">
              <thead>
                <tr>
                  <th>{t('templates.colRuleName')}</th>
                  <th>{t('templates.colTemplate')}</th>
                  <th>{t('templates.colTarget')}</th>
                  <th>{t('templates.colSeverity')}</th>
                  <th>{t('templates.colStatus')}</th>
                </tr>
              </thead>
              <tbody>
                {createdRules.map(rule => (
                  <tr key={rule.id}>
                    <td style={{ fontWeight: 600 }}>{rule.name}</td>
                    <td><span className="mgmt-badge neutral">{rule.template_name || '기본 템플릿'}</span></td>
                    <td>{rule.data_point_name || `Target #${rule.target_id}`}</td>
                    <td>
                      <span className={`mgmt-badge ${getSeverityBadgeClass(rule.severity)}`}>
                        {rule.severity.toUpperCase()}
                      </span>
                    </td>
                    <td>
                      <span className={`status-pill ${rule.enabled || rule.is_enabled ? 'success' : 'neutral'}`}>
                        {rule.enabled || rule.is_enabled ? t('templates.statusActive') : t('templates.statusPaused')}
                      </span>
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
        onChange={setCurrentPage}
      />

      {showModal && (
        <AlarmTemplateCreateEditModal
          isOpen={showModal}
          onClose={() => {
            setShowModal(false);
            setSelectedTemplate(null);
          }}
          onSuccess={() => {
            loadTemplates();
            setShowModal(false);
            setSelectedTemplate(null);
          }}
          mode={selectedTemplate ? 'edit' : 'create'}
          template={selectedTemplate}
          onDelete={handleDeleteTemplate}
        />
      )}

      {showDetailModal && selectedTemplate && (
        <AlarmTemplateDetailModal
          isOpen={showDetailModal}
          template={selectedTemplate}
          onClose={() => setShowDetailModal(false)}
          onEdit={handleEditTemplate}
          onApply={handleApplyTemplate}
        />
      )}

      {showApplyModal && selectedTemplate && (
        <TemplateApplyModal
          template={selectedTemplate}
          isOpen={showApplyModal}
          onClose={() => setShowApplyModal(false)}
          onSuccess={() => {
            loadCreatedRules();
            setActiveTab('created');
          }}
        />
      )}
    </ManagementLayout>
  );
};

export default AlarmRuleTemplates;