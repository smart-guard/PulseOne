import React, { useState, useEffect, useCallback } from 'react';
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
        title: '변경 사항 없음',
        message: '수정된 설정 내용이 없습니다.',
        confirmText: '확인',
        showCancelButton: false
      });
      return;
    }

    // 2. 변경 사항이 있을 경우 저장 확인 팝업
    const ok = await confirm({
      title: '설정 저장 확인',
      message: '변경된 백업 설정을 저장하시겠습니까?',
      confirmText: '저장',
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
          title: '설정 저장 완료',
          message: '백업 설정이 성공적으로 저장되었습니다.',
          confirmText: '확인',
          showCancelButton: false
        });
      } else {
        alert(`저장 실패: ${result.message}`);
      }
    } catch (error) {
      console.error('설정 저장 오류:', error);
    } finally {
      setSavingSettings(false);
    }
  };

  // 즉시 백업 실행
  const handleCreateBackup = async () => {
    const ok = await confirm({
      title: '즉시 백업 실행',
      message: '현재 데이터베이스 상태를 즉시 백업하시겠습니까? 데이터 양에 따라 다소 시간이 걸릴 수 있습니다.',
      confirmText: '백업 시작',
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
        alert(`백업 실패: ${result.message}`);
      }
    } catch (error) {
      console.error('백업 요청 오류:', error);
    }
  };

  // 백업 삭제
  const handleDeleteBackup = async (id: number) => {
    const ok = await confirm({
      title: '백업 삭제 확인',
      message: '정말로 이 백업 파일을 삭제하시겠습니까? 파일과 DB 기록이 모두 영구적으로 제거됩니다.',
      confirmText: '삭제',
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
      title: '시스템 복원 주의',
      message: `[${name}] 백업 시점으로 시스템을 복원하시겠습니까? 현재 데이터베이스가 백업 파일로 교체되며, 작업 중 시스템 접근이 일시 중단될 수 있습니다.`,
      confirmText: '복원 실행',
      confirmButtonType: 'danger'
    });

    if (!ok) return;

    try {
      const response = await fetch(`/api/system/backups/backups/${id}/restore`, { method: 'POST' });
      const result = await response.json();
      if (result.success) {
        await confirm({
          title: '복원 완료',
          message: result.message,
          confirmText: '확인',
          showCancelButton: false
        });
        window.location.reload();
      } else {
        alert(`복원 실패: ${result.message}`);
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
    if (minutes > 0) return `${minutes}분 ${secs}초`;
    return `${secs}초`;
  };

  return (
    <div className="management-container">
      {/* 페이지 헤더 */}
      <div className="mgmt-header">
        <div className="mgmt-header-info">
          <h1 className="mgmt-title">
            <i className="fas fa-shield-alt text-primary-500"></i> 백업 및 시스템 복구
          </h1>
          <p className="mgmt-subtitle">중요 데이터의 안전한 보호 및 비상시 신속한 복구를 관리합니다.</p>
        </div>
        <div className="mgmt-header-actions">
          <button
            className="mgmt-btn mgmt-btn-primary"
            onClick={() => setShowBackupModal(true)}
          >
            <i className="fas fa-plus"></i>
            즉시 백업 생성
          </button>
        </div>
      </div>

      {/* 통계 패널 */}
      <div className="mgmt-stats-panel">
        <StatCard title="총 백업 기록" value={totalCount} icon="fas fa-archive" type="primary" />
        <StatCard title="정상 백업" value={backupRecords.filter(b => b.status === 'completed').length} icon="fas fa-check-circle" type="success" />
        <StatCard title="누적 사용량" value={formatBytes(backupRecords.reduce((sum, b) => sum + (b.size || 0), 0))} icon="fas fa-hdd" type="blueprint" />
        <StatCard
          title="예약 백업 상태"
          value={settings['backup.auto_enabled'] === 'true' ? '활성' : '비활성'}
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
            <i className="fas fa-list-ul mr-2"></i> 백업 목록
          </button>
          <button
            className={`mgmt-btn-text premium ${activeTab === 'settings' ? 'active' : ''}`}
            onClick={() => setActiveTab('settings')}
            style={{
              background: activeTab === 'settings' ? 'var(--primary-100)' : 'transparent',
              padding: '8px 20px'
            }}
          >
            <i className="fas fa-cog mr-2"></i> 백업 설정
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
                  <th style={{ width: '40%', paddingLeft: '24px' }}>백업명 / 파일명</th>
                  <th style={{ width: '10%' }}>상태</th>
                  <th style={{ width: '12%' }}>크기</th>
                  <th style={{ width: '18%' }}>생성 정보</th>
                  <th style={{ width: '12%' }}>파일 유효성</th>
                  <th style={{ width: '8%', textAlign: 'right', paddingRight: '24px' }}>액션</th>
                </tr>
              </thead>
              <tbody>
                {loading ? (
                  <tr><td colSpan={6} style={{ textAlign: 'center', padding: '100px 0' }}>데이터 로딩 중...</td></tr>
                ) : backupRecords.length === 0 ? (
                  <tr><td colSpan={6} style={{ textAlign: 'center', padding: '100px 0' }}>저장된 백업 기록이 없습니다.</td></tr>
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
                          {backup.status === 'completed' ? '완료' : backup.status === 'failed' ? '실패' : '진행중'}
                        </span>
                      </td>
                      <td>
                        <div style={{ fontWeight: 600, fontSize: '14px' }}>{formatBytes(backup.size)}</div>
                        <div style={{ fontSize: '11px', color: 'var(--neutral-400)', marginTop: '2px' }}>{formatDuration(backup.duration!)} 소요</div>
                      </td>
                      <td>
                        <div style={{ fontSize: '13px', fontWeight: 500 }}>{new Date(backup.created_at).toLocaleString()}</div>
                        <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '2px' }}>by {backup.created_by || 'system'}</div>
                      </td>
                      <td>
                        {backup.fileExists ? (
                          <div style={{ color: 'var(--success-600)', fontSize: '12px', fontWeight: 600 }}>
                            <i className="fas fa-check-circle mr-1"></i> 파일 정상
                          </div>
                        ) : (
                          <div style={{ color: 'var(--error-500)', fontSize: '12px', fontWeight: 600 }}>
                            <i className="fas fa-exclamation-triangle mr-1"></i> 파일 소실
                          </div>
                        )}
                      </td>
                      <td style={{ textAlign: 'right', paddingRight: '24px' }}>
                        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
                          <button
                            className="mgmt-btn-icon primary"
                            title="복원"
                            onClick={() => handleRestoreFromBackup(backup.id, backup.name)}
                            disabled={!backup.fileExists}
                            style={{ width: '36px', height: '36px' }}
                          >
                            <i className="fas fa-history"></i>
                          </button>
                          <button
                            className="mgmt-btn-icon error"
                            title="삭제"
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
                <i className="fas fa-calendar-check mr-2 text-primary-500"></i> 자동 백업 예약 설정
              </h3>

              <div className="mgmt-form-group mb-6">
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', background: 'var(--neutral-50)', padding: '16px 20px', borderRadius: '12px', border: '1px solid var(--neutral-100)' }}>
                  <div>
                    <div style={{ fontWeight: 700, fontSize: '15px' }}>자동 백업 활성화</div>
                    <div style={{ fontSize: '13px', color: 'var(--neutral-500)', marginTop: '2px' }}>정해진 주기마다 시스템을 자동으로 백업합니다.</div>
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
                <label style={{ fontWeight: 600, marginBottom: '8px', display: 'block' }}>백업 수행 시간 (KST)</label>
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
                    매일 정해진 시간에 전체 백업을 수행합니다.
                  </div>
                </div>
              </div>
            </div>

            {/* Card 2: Retention & Options */}
            <div className="mgmt-card" style={{ padding: '32px', margin: '0' }}>
              <h3 className="mb-6" style={{ fontSize: '18px', fontWeight: 700 }}>
                <i className="fas fa-database mr-2 text-blueprint-500"></i> 백업 보관 및 옵션
              </h3>

              <div className="mgmt-form-group mb-8">
                <label style={{ fontWeight: 600, marginBottom: '8px', display: 'block' }}>백업 파일 보관 기간</label>
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
                  <span style={{ fontWeight: 600 }}>일</span>
                  <div style={{ color: 'var(--neutral-400)', fontSize: '13px', marginLeft: '12px' }}>
                    보관 기간 경과 시 자동 삭제됩니다.
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
                  <label htmlFor="include-logs" style={{ fontWeight: 600, cursor: 'pointer' }}>시스템 이벤트 로그 포함</label>
                </div>
                <div style={{ fontSize: '13px', color: 'var(--neutral-500)', marginLeft: '30px', marginTop: '4px' }}>
                  활성화 시 DB 외에 이벤트 로그도 함께 백업됩니다.
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
                  <><i className="fas fa-spinner fa-spin mr-2"></i> 저장 중...</>
                ) : (
                  <><i className="fas fa-save mr-2"></i> 백업 설정 저장</>
                )}
              </button>
            </div>
          </div>
        )}

        {activeTab === 'restore' && (
          <div className="mgmt-card" style={{ padding: '80px 40px', textAlign: 'center', color: 'var(--neutral-400)', flex: 1 }}>
            <i className="fas fa-tools mb-4" style={{ fontSize: '64px', opacity: 0.3 }}></i>
            <h3 style={{ fontSize: '20px', fontWeight: 700, color: 'var(--neutral-600)' }}>준비 중인 기능입니다</h3>
            <p style={{ fontSize: '14px' }}>외부 백업 파일 업로드 및 특정 시점 복구(PITR) 기능은 차기 업데이트에서 제공됩니다.</p>
          </div>
        )}
      </div>

      {/* 백업 생성 모달 */}
      {showBackupModal && (
        <div className="mgmt-modal-overlay">
          <div className="mgmt-modal" style={{ maxWidth: '500px' }}>
            <div className="mgmt-modal-header">
              <h3>신규 백업 생성</h3>
              <button className="mgmt-modal-close" onClick={() => setShowBackupModal(false)}>×</button>
            </div>
            <div className="mgmt-modal-body">
              <div className="mgmt-form-group mb-4">
                <label>백업 명칭</label>
                <input
                  type="text"
                  className="mgmt-input"
                  placeholder="예: 엔진 정기 점검 전 백업"
                  value={backupFormData.name}
                  onChange={(e) => setBackupFormData({ ...backupFormData, name: e.target.value })}
                />
              </div>
              <div className="mgmt-form-group">
                <label>상세 설명</label>
                <textarea
                  className="mgmt-input"
                  style={{ height: '80px', paddingTop: '10px' }}
                  placeholder="백업 사유나 특이사항을 입력하세요."
                  value={backupFormData.description}
                  onChange={(e) => setBackupFormData({ ...backupFormData, description: e.target.value })}
                />
              </div>
              <div className="mt-4 p-3 bg-primary-50 rounded" style={{ fontSize: '12px', border: '1px solid var(--primary-100)' }}>
                <i className="fas fa-info-circle mr-2 text-primary-500"></i>
                현재 시스템 데이터베이스의 전체 스냅샷을 생성합니다.
              </div>
            </div>
            <div className="mgmt-modal-footer">
              <button className="mgmt-btn mgmt-btn-outline" onClick={() => setShowBackupModal(false)}>취소</button>
              <button className="mgmt-btn mgmt-btn-primary" onClick={handleCreateBackup} disabled={!backupFormData.name}>백업 실행</button>
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