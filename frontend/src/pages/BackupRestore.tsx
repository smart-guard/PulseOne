import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { StatCard } from '../components/common/StatCard';
import { Pagination } from '../components/common/Pagination';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import '../styles/base.css';
import '../styles/management.css';

interface BackupRecord {
  id: number;
  name: string;
  filename: string;
  type: string;
  status: 'completed' | 'running' | 'failed' | 'scheduled';
  size: number;
  location: string;
  created_at: string;
  created_by: string;
  description?: string;
  duration?: number;
  fileExists?: boolean;
}

const BackupRestore: React.FC = () => {
  const { confirm } = useConfirmContext();
  const { t } = useTranslation('backup');
  const [activeTab, setActiveTab] = useState<'backups' | 'settings' | 'restore'>('backups');
  const [backupRecords, setBackupRecords] = useState<BackupRecord[]>([]);
  const [loading, setLoading] = useState(true);
  const [totalCount, setTotalCount] = useState(0);
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(20);

  const [showBackupModal, setShowBackupModal] = useState(false);
  const [backupFormData, setBackupFormData] = useState({
    name: '',
    description: ''
  });

  const [settings, setSettings] = useState({
    'backup.auto_enabled': 'false',
    'backup.schedule_time': '02:00',
    'backup.retention_days': '30',
    'backup.include_logs': 'true'
  });
  const [initialSettings, setInitialSettings] = useState<any>(null);
  const [savingSettings, setSavingSettings] = useState(false);

  // 데이터 로드
  const fetchBackups = useCallback(async () => {
    setLoading(true);
    try {
      const response = await fetch(`/api/system/backups/backups?page=${currentPage}&limit=${pageSize}`);
      const result = await response.json();
      if (result.success) {
        setBackupRecords(result.data.items);
        setTotalCount(result.data.total);
      }
    } catch (error) {
      console.error('백업로드 실패:', error);
    } finally {
      setLoading(false);
    }
  }, [currentPage, pageSize]);

  // 설정 로드
  const fetchSettings = useCallback(async () => {
    try {
      const response = await fetch('/api/system/backups/settings');
      const result = await response.json();
      if (result.success) {
        setSettings(result.data);
        setInitialSettings(result.data);
      }
    } catch (error) {
      console.error('설정 로드 실패:', error);
    }
  }, []);

  useEffect(() => {
    fetchBackups();
    if (activeTab === 'settings') {
      fetchSettings();
    }
  }, [fetchBackups, fetchSettings, activeTab]);

  // 설정 저장
  const handleSaveSettings = async () => {
    // 1. 변경 사항 전후 비교
    const isChanged = JSON.stringify(settings) !== JSON.stringify(initialSettings);

    if (!isChanged) {
      await confirm({
        title: t('confirm.noChangesTitle'),
        message: t('confirm.noChangesMsg'),
        confirmText: t('confirm.saveText'),
        showCancelButton: false
      });
      return;
    }

    // 2. 변경 사항이 있을 경우 저장 확인 팝업
    const ok = await confirm({
      title: t('confirm.saveTitle'),
      message: t('confirm.saveMsg'),
      confirmText: t('confirm.saveText'),
      confirmButtonType: 'primary'
    });

    if (!ok) return;

    setSavingSettings(true);
    try {
      const response = await fetch('/api/system/backups/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
      });
      const result = await response.json();
      if (result.success) {
        setInitialSettings(settings); // 최초 상태 동기화
        await confirm({
          title: t('confirm.saveDoneTitle'),
          message: t('confirm.saveDoneMsg'),
          confirmText: t('confirm.saveText'),
          showCancelButton: false
        });
      } else {
        alert(`Save failed: ${result.message}`);
      }
    } catch (error) {
      console.error('설정 저장 오류:', error);
    } finally {
      setSavingSettings(false);
    }
  };

  // 즉시 Run Backup
  const handleCreateBackup = async () => {
    const ok = await confirm({
      title: t('confirm.backupTitle'),
      message: t('confirm.backupMsg'),
      confirmText: t('confirm.backupText'),
      confirmButtonType: 'primary'
    });

    if (!ok) return;

    try {
      const response = await fetch('/api/system/backups/backups', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(backupFormData)
      });
      const result = await response.json();
      if (result.success) {
        setShowBackupModal(false);
        fetchBackups();
        setBackupFormData({ name: '', description: '' });
      } else {
        alert(`Backup failed: ${result.message}`);
      }
    } catch (error) {
      console.error('백업 요청 오류:', error);
    }
  };

  // 백업 삭제
  const handleDeleteBackup = async (id: number) => {
    const ok = await confirm({
      title: t('confirm.deleteTitle'),
      message: t('confirm.deleteMsg'),
      confirmText: t('confirm.deleteText'),
      confirmButtonType: 'danger'
    });

    if (!ok) return;

    try {
      const response = await fetch(`/api/system/backups/backups/${id}`, { method: 'DELETE' });
      const result = await response.json();
      if (result.success) {
        fetchBackups();
      }
    } catch (error) {
      console.error('삭제 오류:', error);
    }
  };

  // 백업에서 복원
  const handleRestoreFromBackup = async (id: number, name: string) => {
    const ok = await confirm({
      title: t('confirm.restoreTitle'),
      message: t('confirm.restoreMsg', { name }),
      confirmText: t('confirm.restoreText'),
      confirmButtonType: 'danger'
    });

    if (!ok) return;

    try {
      const response = await fetch(`/api/system/backups/backups/${id}/restore`, { method: 'POST' });
      const result = await response.json();
      if (result.success) {
        await confirm({
          title: t('confirm.restoreDoneTitle'),
          message: result.message,
          confirmText: 'OK',
          showCancelButton: false
        });
        window.location.reload();
      } else {
        alert(`Restore failed: ${result.message}`);
      }
    } catch (error) {
      console.error('복원 오류:', error);
    }
  };

  // 바이트 포맷
  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  // 기간 포맷
  const formatDuration = (seconds: number): string => {
    if (!seconds) return '-';
    const minutes = Math.floor(seconds / 60);
    const secs = seconds % 60;
    if (minutes > 0) return `${minutes}min ${secs}s`;
    return `${secs}s`;
  };

  return (
    <div className="management-container">
      {/* 페이지 헤더 */}
      <div className="mgmt-header">
        <div className="mgmt-header-info">
          <h1 className="mgmt-title">
            <i className="fas fa-shield-alt text-primary-500"></i> {t('title')}
          </h1>
          <p className="mgmt-subtitle">{t('description')}</p>
        </div>
        <div className="mgmt-header-actions">
          <button
            className="mgmt-btn mgmt-btn-primary"
            onClick={() => setShowBackupModal(true)}
          >
            <i className="fas fa-plus"></i>
            {t('createBackup')}
          </button>
        </div>
      </div>

      {/* 통계 패널 */}
      <div className="mgmt-stats-panel">
        <StatCard title={t('stats.totalBackups')} value={totalCount} icon="fas fa-archive" type="primary" />
        <StatCard title={t('stats.successBackups')} value={backupRecords.filter(b => b.status === 'completed').length} icon="fas fa-check-circle" type="success" />
        <StatCard title={t('stats.totalSize')} value={formatBytes(backupRecords.reduce((sum, b) => sum + (b.size || 0), 0))} icon="fas fa-hdd" type="blueprint" />
        <StatCard
          title={t('stats.scheduleStatus')}
          value={settings['backup.auto_enabled'] === 'true' ? t('stats.active') : t('stats.inactive')}
          icon="fas fa-clock"
          type={settings['backup.auto_enabled'] === 'true' ? 'primary' : 'neutral'}
        />
      </div>

      {/* 탭 네비게이션 */}
      <div className="mgmt-filter-bar" style={{ padding: '0 24px', height: '56px', borderBottom: 'none' }}>
        <div className="mgmt-filter-group" style={{ gap: '2px' }}>
          <button
            className={`mgmt-btn-text premium ${activeTab === 'backups' ? 'active' : ''}`}
            onClick={() => setActiveTab('backups')}
            style={{
              background: activeTab === 'backups' ? 'var(--primary-100)' : 'transparent',
              padding: '8px 20px'
            }}
          >
            <i className="fas fa-list-ul mr-2"></i> {t('tabs.list')}
          </button>
          <button
            className={`mgmt-btn-text premium ${activeTab === 'settings' ? 'active' : ''}`}
            onClick={() => setActiveTab('settings')}
            style={{
              background: activeTab === 'settings' ? 'var(--primary-100)' : 'transparent',
              padding: '8px 20px'
            }}
          >
            <i className="fas fa-cog mr-2"></i> {t('tabs.settings')}
          </button>
        </div>
      </div>

      {/* 콘텐츠 영역 */}
      <div className="mgmt-content-area" style={{ padding: '0 4px', overflow: 'visible' }}>
        {activeTab === 'backups' && (
          <div className="mgmt-table-container" style={{ margin: 0 }}>
            <table className="mgmt-table">
              <thead>
                <tr>
                  <th style={{ width: '40%', paddingLeft: '24px' }}>{t('table.nameFile')}</th>
                  <th style={{ width: '10%' }}>{t('table.status')}</th>
                  <th style={{ width: '12%' }}>{t('table.size')}</th>
                  <th style={{ width: '18%' }}>{t('table.createdBy')}</th>
                  <th style={{ width: '12%' }}>{t('table.fileValidity')}</th>
                  <th style={{ width: '8%', textAlign: 'right', paddingRight: '24px' }}>{t('table.action')}</th>
                </tr>
              </thead>
              <tbody>
                {loading ? (
                  <tr><td colSpan={6} style={{ textAlign: 'center', padding: '100px 0' }}>{t('table.loading')}</td></tr>
                ) : backupRecords.length === 0 ? (
                  <tr><td colSpan={6} style={{ textAlign: 'center', padding: '100px 0' }}>{t('table.noData')}</td></tr>
                ) : (
                  backupRecords.map(backup => (
                    <tr key={backup.id} className="mgmt-table-row">
                      <td style={{ paddingLeft: '24px' }}>
                        <div style={{ fontWeight: 700, color: 'var(--neutral-900)', fontSize: '15px' }}>{backup.name}</div>
                        <div style={{ fontSize: '12px', color: 'var(--neutral-400)', fontFamily: 'monospace', marginTop: '4px' }}>{backup.filename}</div>
                        {backup.description && (
                          <div style={{
                            fontSize: '12px',
                            color: 'var(--neutral-500)',
                            marginTop: '8px',
                            background: 'var(--neutral-50)',
                            padding: '4px 8px',
                            borderRadius: '4px',
                            display: 'inline-block'
                          }}>
                            {backup.description}
                          </div>
                        )}
                      </td>
                      <td>
                        <span className={`mgmt-status-pill ${backup.status === 'completed' ? 'active' : backup.status === 'failed' ? 'error' : 'warning'}`}>
                          {backup.status === 'completed' ? t('status.completed') : backup.status === 'failed' ? t('status.failed') : t('status.running')}
                        </span>
                      </td>
                      <td>
                        <div style={{ fontWeight: 600, fontSize: '14px' }}>{formatBytes(backup.size)}</div>
                        <div style={{ fontSize: '11px', color: 'var(--neutral-400)', marginTop: '2px' }}>{formatDuration(backup.duration!)} {t('table.elapsed')}</div>
                      </td>
                      <td>
                        <div style={{ fontSize: '13px', fontWeight: 500 }}>{new Date(backup.created_at).toLocaleString()}</div>
                        <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '2px' }}>{t('table.by')} {backup.created_by || t('system')}</div>
                      </td>
                      <td>
                        {backup.fileExists ? (
                          <div style={{ color: 'var(--success-600)', fontSize: '12px', fontWeight: 600 }}>
                            <i className="fas fa-check-circle mr-1"></i> {t('table.fileOk')}
                          </div>
                        ) : (
                          <div style={{ color: 'var(--error-500)', fontSize: '12px', fontWeight: 600 }}>
                            <i className="fas fa-exclamation-triangle mr-1"></i> {t('table.fileMissing')}
                          </div>
                        )}
                      </td>
                      <td style={{ textAlign: 'right', paddingRight: '24px' }}>
                        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
                          <button
                            className="mgmt-btn-icon primary"
                            title={t('table.restoreAction')}
                            onClick={() => handleRestoreFromBackup(backup.id, backup.name)}
                            disabled={!backup.fileExists}
                            style={{ width: '36px', height: '36px' }}
                          >
                            <i className="fas fa-history"></i>
                          </button>
                          <button
                            className="mgmt-btn-icon error"
                            title={t('table.deleteAction')}
                            onClick={() => handleDeleteBackup(backup.id)}
                            style={{ width: '36px', height: '36px' }}
                          >
                            <i className="fas fa-trash-alt"></i>
                          </button>
                        </div>
                      </td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
            <Pagination
              current={currentPage}
              total={totalCount}
              pageSize={pageSize}
              onChange={setCurrentPage}
              onShowSizeChange={(_, size) => setPageSize(size)}
            />
          </div>
        )}

        {activeTab === 'settings' && (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(400px, 1fr))', gap: '24px', alignItems: 'stretch' }}>
            {/* Card 1: Schedule Settings */}
            <div className="mgmt-card" style={{ padding: '32px', margin: '0' }}>
              <h3 className="mb-6" style={{ fontSize: '18px', fontWeight: 700 }}>
                <i className="fas fa-calendar-check mr-2 text-primary-500"></i> {t('settings.autoBackupTitle')}
              </h3>

              <div className="mgmt-form-group mb-6">
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', background: 'var(--neutral-50)', padding: '16px 20px', borderRadius: '12px', border: '1px solid var(--neutral-100)' }}>
                  <div>
                    <div style={{ fontWeight: 700, fontSize: '15px' }}>{t('settings.autoBackupLabel')}</div>
                    <div style={{ fontSize: '13px', color: 'var(--neutral-500)', marginTop: '2px' }}>{t('settings.autoBackupDesc')}</div>
                  </div>
                  <div
                    className={`mgmt-toggle ${settings['backup.auto_enabled'] === 'true' ? 'active' : ''}`}
                    onClick={() => setSettings({ ...settings, 'backup.auto_enabled': settings['backup.auto_enabled'] === 'true' ? 'false' : 'true' })}
                    style={{ cursor: 'pointer' }}
                  >
                    <div className="mgmt-toggle-handle"></div>
                  </div>
                </div>
              </div>

              <div className="mgmt-form-group">
                <label style={{ fontWeight: 600, marginBottom: '8px', display: 'block' }}>{t('settings.scheduleTimeLabel')}</label>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <input
                    type="time"
                    className="mgmt-input"
                    style={{ width: '160px', height: '44px', fontSize: '16px', fontWeight: 600 }}
                    value={settings['backup.schedule_time']}
                    onChange={(e) => setSettings({ ...settings, 'backup.schedule_time': e.target.value })}
                    disabled={settings['backup.auto_enabled'] !== 'true'}
                  />
                  <div style={{ color: 'var(--neutral-400)', fontSize: '13px' }}>
                    {t('settings.scheduleTimeDesc')}
                  </div>
                </div>
              </div>
            </div>

            {/* Card 2: Retention & Options */}
            <div className="mgmt-card" style={{ padding: '32px', margin: '0' }}>
              <h3 className="mb-6" style={{ fontSize: '18px', fontWeight: 700 }}>
                <i className="fas fa-database mr-2 text-blueprint-500"></i> {t('settings.retentionTitle')}
              </h3>

              <div className="mgmt-form-group mb-8">
                <label style={{ fontWeight: 600, marginBottom: '8px', display: 'block' }}>{t('settings.retentionLabel')}</label>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <input
                    type="number"
                    className="mgmt-input"
                    style={{ width: '120px' }}
                    min="1"
                    max="365"
                    value={settings['backup.retention_days']}
                    onChange={(e) => setSettings({ ...settings, 'backup.retention_days': e.target.value })}
                  />
                  <span style={{ fontWeight: 600 }}>{t('settings.retentionUnit')}</span>
                  <div style={{ color: 'var(--neutral-400)', fontSize: '13px', marginLeft: '12px' }}>
                    {t('settings.retentionDesc')}
                  </div>
                </div>
              </div>

              <div className="mgmt-form-group">
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <input
                    type="checkbox"
                    id="include-logs"
                    style={{ width: '18px', height: '18px', cursor: 'pointer' }}
                    checked={settings['backup.include_logs'] === 'true'}
                    onChange={(e) => setSettings({ ...settings, 'backup.include_logs': e.target.checked ? 'true' : 'false' })}
                  />
                  <label htmlFor="include-logs" style={{ fontWeight: 600, cursor: 'pointer' }}>{t('settings.includeLogsLabel')}</label>
                </div>
                <div style={{ fontSize: '13px', color: 'var(--neutral-500)', marginLeft: '30px', marginTop: '4px' }}>
                  {t('settings.includeLogsDesc')}
                </div>
              </div>
            </div>

            {/* Grid spanning save button */}
            <div style={{ gridColumn: '1 / -1', display: 'flex', justifyContent: 'center', marginTop: '8px', paddingBottom: '24px' }}>
              <button
                className="mgmt-btn mgmt-btn-primary"
                style={{ width: '240px', height: '48px', fontSize: '16px', fontWeight: 700, borderRadius: '12px', boxShadow: '0 4px 12px rgba(var(--primary-rgb), 0.2)' }}
                onClick={handleSaveSettings}
                disabled={savingSettings}
              >
                {savingSettings ? (
                  <><i className="fas fa-spinner fa-spin mr-2"></i> {t('settings.saving')}</>
                ) : (
                  <><i className="fas fa-save mr-2"></i> {t('settings.saveBtn')}</>
                )}
              </button>
            </div>
          </div>
        )}

        {activeTab === 'restore' && (
          <div className="mgmt-card" style={{ padding: '80px 40px', textAlign: 'center', color: 'var(--neutral-400)', flex: 1 }}>
            <i className="fas fa-tools mb-4" style={{ fontSize: '64px', opacity: 0.3 }}></i>
            <h3 style={{ fontSize: '20px', fontWeight: 700, color: 'var(--neutral-600)' }}>{t('restore.title')}</h3>
            <p style={{ fontSize: '14px' }}>{t('restore.desc')}</p>
          </div>
        )}
      </div>

      {/* 백업 생성 모달 */}
      {showBackupModal && (
        <div className="mgmt-modal-overlay">
          <div className="mgmt-modal" style={{ maxWidth: '500px' }}>
            <div className="mgmt-modal-header">
              <h3>{t('modal.title')}</h3>
              <button className="mgmt-modal-close" onClick={() => setShowBackupModal(false)}>×</button>
            </div>
            <div className="mgmt-modal-body">
              <div className="mgmt-form-group mb-4">
                <label>{t('modal.nameLabel')}</label>
                <input
                  type="text"
                  className="mgmt-input"
                  placeholder={t('modal.namePlaceholder')}
                  value={backupFormData.name}
                  onChange={(e) => setBackupFormData({ ...backupFormData, name: e.target.value })}
                />
              </div>
              <div className="mgmt-form-group">
                <label>{t('modal.descLabel')}</label>
                <textarea
                  className="mgmt-input"
                  style={{ height: '80px', paddingTop: '10px' }}
                  placeholder={t('modal.descPlaceholder')}
                  value={backupFormData.description}
                  onChange={(e) => setBackupFormData({ ...backupFormData, description: e.target.value })}
                />
              </div>
              <div className="mt-4 p-3 bg-primary-50 rounded" style={{ fontSize: '12px', border: '1px solid var(--primary-100)' }}>
                <i className="fas fa-info-circle mr-2 text-primary-500"></i>
                {t('modal.infoMsg')}
              </div>
            </div>
            <div className="mgmt-modal-footer">
              <button className="mgmt-btn mgmt-btn-outline" onClick={() => setShowBackupModal(false)}>{t('modal.cancel')}</button>
              <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreateBackup} disabled={!backupFormData.name}>{t('modal.run')}</button>
            </div>
          </div>
        </div>
      )}

      {/* 모달용 추가 CSS */}
      <style>{`
        .mgmt-modal-overlay {
          position: fixed;
          top: 0; left: 0; right: 0; bottom: 0;
          background: rgba(0,0,0,0.4);
          display: flex; align-items: center; justify-content: center;
          z-index: 1000;
          backdrop-filter: blur(4px);
        }
        .mgmt-modal {
          background: white;
          border-radius: 16px;
          width: 90%;
          box-shadow: 0 20px 25px -5px rgba(0,0,0,0.1), 0 10px 10px -5px rgba(0,0,0,0.04);
          overflow: hidden;
        }
        .mgmt-modal-header {
          padding: 20px 24px;
          border-bottom: 1px solid var(--neutral-100);
          display: flex; justify-content: space-between; align-items: center;
        }
        .mgmt-modal-header h3 { margin: 0; font-size: 18px; font-weight: 700; }
        .mgmt-modal-close { background: none; border: none; font-size: 24px; cursor: pointer; color: var(--neutral-400); }
        .mgmt-modal-body { padding: 24px; }
        .mgmt-modal-footer { padding: 16px 24px; background: var(--neutral-50); display: flex; justify-content: flex-end; gap: 8px; }
        .mgmt-btn-text.premium.active { color: var(--primary-700); font-weight: 700; }
      `}</style>
    </div>
  );
};

export default BackupRestore;