import React, { useState, useEffect } from 'react';
import { StatCard } from '../components/common/StatCard';
import { Pagination } from '../components/common/Pagination';
import '../styles/base.css';
import '../styles/management.css';

interface BackupRecord {
  id: string;
  name: string;
  type: 'full' | 'incremental' | 'differential';
  status: 'completed' | 'running' | 'failed' | 'scheduled';
  size: number;
  location: string;
  createdAt: Date;
  createdBy: string;
  description?: string;
  duration?: number; // seconds
  includedComponents: string[];
  encryption: boolean;
  compression: boolean;
}

interface ScheduledBackup {
  id: string;
  name: string;
  type: 'full' | 'incremental';
  schedule: {
    frequency: 'daily' | 'weekly' | 'monthly';
    time: string;
    dayOfWeek?: number;
    dayOfMonth?: number;
  };
  enabled: boolean;
  retention: number; // days
  location: string;
  components: string[];
  encryption: boolean;
  compression: boolean;
  lastRun?: Date;
  nextRun: Date;
  createdAt: Date;
}

interface RestoreJob {
  id: string;
  backupId: string;
  backupName: string;
  status: 'running' | 'completed' | 'failed';
  progress: number;
  startedAt: Date;
  completedAt?: Date;
  restoredComponents: string[];
  errors: string[];
}

const BackupRestore: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'backups' | 'schedule' | 'restore' | 'settings'>('backups');
  const [backupRecords, setBackupRecords] = useState<BackupRecord[]>([]);
  const [scheduledBackups, setScheduledBackups] = useState<ScheduledBackup[]>([]);
  const [restoreJobs, setRestoreJobs] = useState<RestoreJob[]>([]);
  const [selectedBackup, setSelectedBackup] = useState<BackupRecord | null>(null);
  const [showBackupModal, setShowBackupModal] = useState(false);
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(20);

  // 백업 생성 폼 데이터
  const [backupFormData, setBackupFormData] = useState({
    name: '',
    type: 'full' as 'full' | 'incremental' | 'differential',
    description: '',
    location: '/backup',
    components: [] as string[],
    encryption: true,
    compression: true
  });

  useEffect(() => {
    initializeMockData();
  }, []);

  const initializeMockData = () => {
    // 백업 기록
    const mockBackupRecords: BackupRecord[] = [];
    const now = new Date();

    for (let i = 0; i < 30; i++) {
      const createdAt = new Date(now.getTime() - i * 24 * 60 * 60 * 1000);
      const status: BackupRecord['status'] = Math.random() > 0.1 ? 'completed' : 'failed';

      const record: BackupRecord = {
        id: `backup_${Date.now() - i * 1000}`,
        name: `백업_${createdAt.toISOString().split('T')[0]}`,
        type: i % 7 === 0 ? 'full' : Math.random() > 0.5 ? 'incremental' : 'differential',
        status,
        size: Math.random() * 5000 * 1024 * 1024, // 0-5GB
        location: '/backup/automated',
        createdAt,
        createdBy: 'system',
        description: i % 7 === 0 ? '주간 전체 백업' : '일일 증분 백업',
        duration: status === 'completed' ? Math.random() * 3600 + 300 : undefined, // 5분-1시간
        includedComponents: ['database', 'configurations', 'logs', 'user_data'],
        encryption: true,
        compression: true
      };

      mockBackupRecords.push(record);
    }

    // 예약된 백업
    const mockScheduledBackups: ScheduledBackup[] = [
      {
        id: 'schedule_1',
        name: '일일 증분 백업',
        type: 'incremental',
        schedule: {
          frequency: 'daily',
          time: '02:00'
        },
        enabled: true,
        retention: 30,
        location: '/backup/daily',
        components: ['database', 'configurations'],
        encryption: true,
        compression: true,
        lastRun: new Date(Date.now() - 6 * 60 * 60 * 1000),
        nextRun: new Date(Date.now() + 18 * 60 * 60 * 1000),
        createdAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000)
      },
      {
        id: 'schedule_2',
        name: '주간 전체 백업',
        type: 'full',
        schedule: {
          frequency: 'weekly',
          time: '01:00',
          dayOfWeek: 0 // Sunday
        },
        enabled: true,
        retention: 90,
        location: '/backup/weekly',
        components: ['database', 'configurations', 'logs', 'user_data', 'system_files'],
        encryption: true,
        compression: true,
        lastRun: new Date(Date.now() - 3 * 24 * 60 * 60 * 1000),
        nextRun: new Date(Date.now() + 4 * 24 * 60 * 60 * 1000),
        createdAt: new Date(Date.now() - 90 * 24 * 60 * 60 * 1000)
      }
    ];

    // 복원 작업
    const mockRestoreJobs: RestoreJob[] = [
      {
        id: 'restore_1',
        backupId: mockBackupRecords[5].id,
        backupName: mockBackupRecords[5].name,
        status: 'completed',
        progress: 100,
        startedAt: new Date(Date.now() - 2 * 60 * 60 * 1000),
        completedAt: new Date(Date.now() - 1.5 * 60 * 60 * 1000),
        restoredComponents: ['database', 'configurations'],
        errors: []
      }
    ];

    setBackupRecords(mockBackupRecords);
    setScheduledBackups(mockScheduledBackups);
    setRestoreJobs(mockRestoreJobs);
  };

  // 즉시 백업 실행
  const handleCreateBackup = async () => {
    if (!backupFormData.name) {
      alert('백업명을 입력해주세요.');
      return;
    }

    const newBackup: BackupRecord = {
      id: `backup_${Date.now()}`,
      name: backupFormData.name,
      type: backupFormData.type,
      status: 'running',
      size: 0,
      location: backupFormData.location,
      createdAt: new Date(),
      createdBy: 'current_user',
      description: backupFormData.description,
      includedComponents: backupFormData.components,
      encryption: backupFormData.encryption,
      compression: backupFormData.compression
    };

    setBackupRecords(prev => [newBackup, ...prev]);
    setShowBackupModal(false);

    // 모의 백업 진행 (실제로는 백엔드에서 처리)
    setTimeout(() => {
      setBackupRecords(prev => prev.map(backup =>
        backup.id === newBackup.id
          ? {
            ...backup,
            status: 'completed',
            size: Math.random() * 1000 * 1024 * 1024, // 0-1GB
            duration: Math.random() * 1800 + 300 // 5-35분
          }
          : backup
      ));
    }, 3000);

    // 폼 초기화
    setBackupFormData({
      name: '',
      type: 'full',
      description: '',
      location: '/backup',
      components: [],
      encryption: true,
      compression: true
    });
  };

  // 백업 삭제
  const handleDeleteBackup = (backupId: string) => {
    if (confirm('정말로 이 백업을 삭제하시겠습니까? 이 작업은 되돌릴 수 없습니다.')) {
      setBackupRecords(prev => prev.filter(backup => backup.id !== backupId));
    }
  };

  // 백업에서 복원
  const handleRestoreFromBackup = (backup: BackupRecord) => {
    if (confirm(`${backup.name} 백업에서 복원하시겠습니까? 현재 데이터가 덮어쓰여질 수 있습니다.`)) {
      const restoreJob: RestoreJob = {
        id: `restore_${Date.now()}`,
        backupId: backup.id,
        backupName: backup.name,
        status: 'running',
        progress: 0,
        startedAt: new Date(),
        restoredComponents: backup.includedComponents,
        errors: []
      };

      setRestoreJobs(prev => [restoreJob, ...prev]);

      // 모의 복원 진행
      const interval = setInterval(() => {
        setRestoreJobs(prev => prev.map(job =>
          job.id === restoreJob.id && job.progress < 100
            ? { ...job, progress: Math.min(job.progress + 10, 100) }
            : job
        ));
      }, 500);

      setTimeout(() => {
        clearInterval(interval);
        setRestoreJobs(prev => prev.map(job =>
          job.id === restoreJob.id
            ? { ...job, status: 'completed', progress: 100, completedAt: new Date() }
            : job
        ));
      }, 5000);
    }
  };

  // 예약 백업 토글
  const handleToggleScheduledBackup = (scheduleId: string) => {
    setScheduledBackups(prev => prev.map(schedule =>
      schedule.id === scheduleId ? { ...schedule, enabled: !schedule.enabled } : schedule
    ));
  };

  // 바이트를 읽기 쉬운 형태로 변환
  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  // 기간 포맷
  const formatDuration = (seconds: number): string => {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    if (hours > 0) return `${hours}시간 ${minutes}분`;
    return `${minutes}분`;
  };

  // 상태 아이콘
  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'completed':
        return 'fa-check-circle text-success-500';
      case 'running':
        return 'fa-spinner fa-spin text-primary-500';
      case 'failed':
        return 'fa-times-circle text-error-500';
      case 'scheduled':
        return 'fa-clock text-warning-500';
      default:
        return 'fa-question-circle text-neutral-400';
    }
  };

  // 페이지네이션
  const startIndex = (currentPage - 1) * pageSize;
  const endIndex = startIndex + pageSize;
  const currentBackups = backupRecords.slice(startIndex, endIndex);

  const availableComponents = [
    { id: 'database', name: '데이터베이스', description: 'PostgreSQL, Redis 데이터' },
    { id: 'configurations', name: '설정 파일', description: '시스템 및 애플리케이션 설정' },
    { id: 'logs', name: '로그 파일', description: '시스템 및 애플리케이션 로그' },
    { id: 'user_data', name: '사용자 데이터', description: '업로드된 파일 및 사용자 컨텐츠' },
    { id: 'system_files', name: '시스템 파일', description: '중요한 시스템 파일 및 라이브러리' }
  ];

  return (
    <div className="management-container">
      {/* 페이지 헤더 */}
      <div className="mgmt-header">
        <h1 className="mgmt-title">
          <i className="fas fa-archive"></i> 백업 및 복원
        </h1>
        <div className="mgmt-header-actions">
          <button
            className="mgmt-btn mgmt-btn-primary"
            onClick={() => setShowBackupModal(true)}
          >
            <i className="fas fa-plus"></i>
            즉시 백업
          </button>
          <button className="mgmt-btn mgmt-btn-outline">
            <i className="fas fa-upload"></i>
            백업 업로드
          </button>
        </div>
      </div>

      {/* 백업 상태 요약 */}
      <div className="mgmt-stats-panel mb-6">
        <StatCard title="총 백업" value={backupRecords.length} icon="fas fa-database" type="primary" />
        <StatCard title="성공" value={backupRecords.filter(b => b.status === 'completed').length} icon="fas fa-check-circle" type="success" />
        <StatCard title="총 용량" value={formatBytes(backupRecords.reduce((sum, b) => sum + b.size, 0))} icon="fas fa-hdd" type="blueprint" />
        <StatCard title="예약된 백업" value={scheduledBackups.filter(s => s.enabled).length} icon="fas fa-clock" type="primary" />
      </div>

      {/* 탭 네비게이션 */}
      <div className="mgmt-tab-navigation">
        <button
          className={`mgmt-tab-button ${activeTab === 'backups' ? 'active' : ''}`}
          onClick={() => setActiveTab('backups')}
        >
          <i className="fas fa-archive"></i>
          백업 목록
          <span className="mgmt-tab-badge">{backupRecords.length}</span>
        </button>
        <button
          className={`mgmt-tab-button ${activeTab === 'schedule' ? 'active' : ''}`}
          onClick={() => setActiveTab('schedule')}
        >
          <i className="fas fa-calendar-alt"></i>
          예약 백업
          <span className="mgmt-tab-badge">{scheduledBackups.length}</span>
        </button>
        <button
          className={`mgmt-tab-button ${activeTab === 'restore' ? 'active' : ''}`}
          onClick={() => setActiveTab('restore')}
        >
          <i className="fas fa-undo"></i>
          복원 작업
          <span className="mgmt-tab-badge">{restoreJobs.length}</span>
        </button>
        <button
          className={`mgmt-tab-button ${activeTab === 'settings' ? 'active' : ''}`}
          onClick={() => setActiveTab('settings')}
        >
          <i className="fas fa-cog"></i>
          백업 설정
        </button>
      </div>

      {/* 백업 목록 탭 */}
      {activeTab === 'backups' && (
        <div className="mgmt-content-area">
          <div className="mgmt-card-header">
            <h2 className="mgmt-card-title">백업 목록</h2>
            <div className="mgmt-card-actions">
              <span className="text-sm text-neutral-600">
                총 {backupRecords.length}개 백업
              </span>
            </div>
          </div>

          <div className="mgmt-table-container">
            <table className="mgmt-table">
              <thead>
                <tr>
                  <th>백업명</th>
                  <th>타입</th>
                  <th>상태</th>
                  <th>크기</th>
                  <th>생성일</th>
                  <th>소요시간</th>
                  <th>위치</th>
                  <th>액션</th>
                </tr>
              </thead>
              <tbody>
                {currentBackups.map(backup => (
                  <tr key={backup.id} className="mgmt-table-row">
                    <td>
                      <div className="mgmt-user-name" style={{ fontWeight: 600 }}>{backup.name}</div>
                      <div className="mgmt-user-email" style={{ fontSize: '12px', color: 'var(--neutral-500)' }}>{backup.description}</div>
                      <div className="mgmt-user-id" style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>
                        {backup.includedComponents.join(', ')}
                      </div>
                    </td>

                    <td>
                      <span className={`mgmt-badge ${backup.type === 'full' ? 'success' :
                        backup.type === 'incremental' ? 'primary' : 'neutral'
                        }`}>
                        {backup.type === 'full' ? '전체' :
                          backup.type === 'incremental' ? '증분' : '차등'}
                      </span>
                    </td>

                    <td>
                      <div className="mgmt-status-indicator">
                        <i className={`fas ${getStatusIcon(backup.status)}`}></i>
                        <span className="mgmt-status-text">
                          {backup.status === 'completed' ? '완료' :
                            backup.status === 'running' ? '진행중' :
                              backup.status === 'failed' ? '실패' : '예약됨'}
                        </span>
                      </div>
                    </td>

                    <td>
                      <div className="text-neutral-700">{formatBytes(backup.size)}</div>
                      <div className="text-xs text-neutral-500">
                        {backup.compression && <span>압축됨</span>}
                        {backup.encryption && <span> • 암호화됨</span>}
                      </div>
                    </td>

                    <td>
                      <div className="text-sm">
                        {backup.createdAt.toLocaleString()}
                      </div>
                      <div className="text-xs text-neutral-400">
                        by {backup.createdBy}
                      </div>
                    </td>

                    <td>
                      <div className="text-neutral-700">
                        {backup.duration ? formatDuration(backup.duration) : '-'}
                      </div>
                    </td>

                    <td>
                      <div className="text-neutral-700 font-mono text-xs">
                        {backup.location}
                      </div>
                    </td>

                    <td className="mgmt-action-cell">
                      <div className="mgmt-card-actions">
                        {backup.status === 'completed' && (
                          <button
                            className="mgmt-btn-icon"
                            onClick={() => handleRestoreFromBackup(backup)}
                            title="복원"
                          >
                            <i className="fas fa-undo"></i>
                          </button>
                        )}
                        <button
                          className="mgmt-btn-icon"
                          onClick={() => {
                            alert('백업 파일을 다운로드합니다.');
                          }}
                          title="다운로드"
                        >
                          <i className="fas fa-download"></i>
                        </button>
                        <button
                          className="mgmt-btn-icon mgmt-btn-error"
                          onClick={() => handleDeleteBackup(backup.id)}
                          title="삭제"
                        >
                          <i className="fas fa-trash"></i>
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <Pagination
            current={currentPage}
            total={backupRecords.length}
            pageSize={pageSize}
            onChange={setCurrentPage}
            onShowSizeChange={(_, size) => setPageSize(size)}
          />
        </div>
      )}

      {/* 예약 백업 탭 */}
      {activeTab === 'schedule' && (
        <div className="mgmt-content-area">
          <div className="mgmt-card-header">
            <h2 className="mgmt-card-title">예약된 백업</h2>
            <div className="mgmt-card-actions">
              <button className="mgmt-btn mgmt-btn-primary">
                <i className="fas fa-plus"></i>
                새 예약 백업
              </button>
            </div>
          </div>

          <div className="mgmt-grid" style={{ gap: '1.5rem', marginTop: '1rem' }}>
            {scheduledBackups.map(schedule => (
              <div key={schedule.id} className="mgmt-card" style={{ padding: '1.5rem' }}>
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '1rem' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
                    <div style={{ width: '48px', height: '48px', background: 'var(--primary-50)', borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                      <i className="fas fa-calendar-alt" style={{ color: 'var(--primary-600)', fontSize: '1.25rem' }}></i>
                    </div>
                    <div>
                      <h3 style={{ fontSize: '1.125rem', fontWeight: 600, color: 'var(--neutral-800)', margin: 0 }}>{schedule.name}</h3>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', marginTop: '0.25rem' }}>
                        <span className={`mgmt-badge ${schedule.type === 'full' ? 'success' : 'primary'}`}>
                          {schedule.type === 'full' ? '전체' : '증분'}
                        </span>
                        <span style={{ fontSize: '0.875rem', color: 'var(--neutral-600)' }}>
                          {schedule.schedule.frequency === 'daily' ? '매일' :
                            schedule.schedule.frequency === 'weekly' ? '매주' : '매월'}
                          {' '}{schedule.schedule.time}
                        </span>
                      </div>
                    </div>
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                    <div className="mgmt-status-indicator">
                      <span style={{ fontSize: '0.875rem', fontWeight: 600, color: schedule.enabled ? 'var(--success-600)' : 'var(--neutral-400)' }}>
                        {schedule.enabled ? '활성' : '비활성'}
                      </span>
                    </div>
                    <button className="mgmt-btn mgmt-btn-outline" style={{ height: '32px', padding: '0 12px' }}>
                      <i className="fas fa-edit"></i>
                      편집
                    </button>
                  </div>
                </div>

                <div className="mgmt-grid" style={{ gridTemplateColumns: 'repeat(2, 1fr)', gap: '1rem', background: 'var(--neutral-50)', padding: '1rem', borderRadius: '8px' }}>
                  <div>
                    <label style={{ fontSize: '0.75rem', fontWeight: 500, color: 'var(--neutral-500)', display: 'block' }}>다음 실행</label>
                    <div style={{ fontSize: '0.875rem', color: 'var(--neutral-800)', marginTop: '2px' }}>
                      {schedule.nextRun.toLocaleString()}
                    </div>
                  </div>
                  <div>
                    <label style={{ fontSize: '0.75rem', fontWeight: 500, color: 'var(--neutral-500)', display: 'block' }}>마지막 실행</label>
                    <div style={{ fontSize: '0.875rem', color: 'var(--neutral-800)', marginTop: '2px' }}>
                      {schedule.lastRun?.toLocaleString() || 'N/A'}
                    </div>
                  </div>
                  <div>
                    <label style={{ fontSize: '0.75rem', fontWeight: 500, color: 'var(--neutral-500)', display: 'block' }}>보관 기간</label>
                    <div style={{ fontSize: '0.875rem', color: 'var(--neutral-800)', marginTop: '2px' }}>{schedule.retention}일</div>
                  </div>
                  <div>
                    <label style={{ fontSize: '0.75rem', fontWeight: 500, color: 'var(--neutral-500)', display: 'block' }}>저장 위치</label>
                    <div style={{ fontSize: '0.875rem', color: 'var(--neutral-800)', marginTop: '2px', fontFamily: 'monospace' }}>{schedule.location}</div>
                  </div>
                </div>

                <div style={{ marginTop: '1rem' }}>
                  <label style={{ fontSize: '0.75rem', fontWeight: 500, color: 'var(--neutral-500)', display: 'block' }}>포함 구성요소</label>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '0.25rem', marginTop: '0.5rem' }}>
                    {schedule.components.map(component => (
                      <span key={component} className="mgmt-badge neutral">
                        {availableComponents.find(c => c.id === component)?.name || component}
                      </span>
                    ))}
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 복원 작업 탭 */}
      {activeTab === 'restore' && (
        <div className="mgmt-content-area">
          <div className="mgmt-card-header">
            <h2 className="mgmt-card-title">복원 작업</h2>
            <div className="mgmt-card-actions">
              <span className="text-sm text-neutral-600">
                최근 복원 작업 목록
              </span>
            </div>
          </div>

          {restoreJobs.length > 0 ? (
            <div className="mgmt-grid" style={{ gap: '1.5rem', marginTop: '1rem' }}>
              {restoreJobs.map(job => (
                <div key={job.id} className="mgmt-card" style={{ padding: '1.5rem' }}>
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '1.5rem' }}>
                    <div>
                      <h3 style={{ fontSize: '1.125rem', fontWeight: 600, color: 'var(--neutral-800)', margin: 0 }}>
                        {job.backupName} 복원
                      </h3>
                      <div className="mgmt-status-indicator" style={{ marginTop: '0.25rem' }}>
                        <i className={`fas ${getStatusIcon(job.status)}`}></i>
                        <span className="mgmt-status-text">
                          {job.status === 'completed' ? '완료' :
                            job.status === 'running' ? '진행중' : '실패'}
                        </span>
                      </div>
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <div style={{ fontSize: '0.875rem', color: 'var(--neutral-500)' }}>
                        시작: {job.startedAt.toLocaleString()}
                      </div>
                      {job.completedAt && (
                        <div style={{ fontSize: '0.875rem', color: 'var(--neutral-500)' }}>
                          완료: {job.completedAt.toLocaleString()}
                        </div>
                      )}
                    </div>
                  </div>

                  {job.status === 'running' && (
                    <div style={{ marginBottom: '1.5rem' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '0.5rem' }}>
                        <span style={{ fontSize: '0.875rem', color: 'var(--neutral-500)' }}>진행률</span>
                        <span style={{ fontSize: '0.875rem', fontWeight: 600, color: 'var(--neutral-800)' }}>{job.progress}%</span>
                      </div>
                      <div style={{ width: '100%', height: '8px', background: 'var(--neutral-100)', borderRadius: '4px' }}>
                        <div
                          style={{
                            width: `${job.progress}%`,
                            height: '100%',
                            background: 'var(--primary-500)',
                            borderRadius: '4px',
                            transition: 'width 0.3s ease'
                          }}
                        ></div>
                      </div>
                    </div>
                  )}

                  <div className="mgmt-grid" style={{ gridTemplateColumns: 'repeat(2, 1fr)', gap: '1rem' }}>
                    <div>
                      <label style={{ fontSize: '0.75rem', fontWeight: 500, color: 'var(--neutral-500)', display: 'block' }}>복원된 구성요소</label>
                      <div style={{ display: 'flex', flexWrap: 'wrap', gap: '0.25rem', marginTop: '0.5rem' }}>
                        {job.restoredComponents.map(component => (
                          <span key={component} className="mgmt-badge neutral">
                            {availableComponents.find(c => c.id === component)?.name || component}
                          </span>
                        ))}
                      </div>
                    </div>
                    {job.errors.length > 0 && (
                      <div>
                        <label style={{ fontSize: '0.75rem', fontWeight: 500, color: 'var(--error-600)', display: 'block' }}>오류</label>
                        <div style={{ fontSize: '0.875rem', color: 'var(--error-700)', marginTop: '0.25rem' }}>
                          {job.errors.join(', ')}
                        </div>
                      </div>
                    )}
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <div className="mgmt-empty-state" style={{ padding: '4rem 2rem', textAlign: 'center', background: 'white', borderRadius: '12px', border: '1px dashed var(--neutral-300)' }}>
              <i className="fas fa-undo" style={{ fontSize: '3rem', color: 'var(--neutral-200)', marginBottom: '1.5rem', display: 'block' }}></i>
              <div style={{ fontSize: '1.25rem', fontWeight: 600, color: 'var(--neutral-800)', marginBottom: '0.5rem' }}>복원 작업이 없습니다</div>
              <div style={{ fontSize: '0.875rem', color: 'var(--neutral-500)' }}>
                백업에서 데이터를 복원한 기록이 없습니다.
              </div>
            </div>
          )}
        </div>
      )}

      {/* 백업 설정 탭 */}
      {activeTab === 'settings' && (
        <div className="mgmt-content-area">
          <div className="mgmt-card-header">
            <h2 className="mgmt-card-title">백업 설정</h2>
          </div>

          <div className="mgmt-grid" style={{ gap: '1.5rem', marginTop: '1rem' }}>
            {/* 기본 설정 */}
            <div className="mgmt-card" style={{ padding: '1.5rem' }}>
              <h3 style={{ fontSize: '1.125rem', fontWeight: 600, color: 'var(--neutral-800)', marginBottom: '1.5rem', borderBottom: '1px solid var(--neutral-100)', paddingBottom: '0.75rem' }}>기본 설정</h3>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
                <div className="mgmt-filter-group">
                  <label>기본 백업 위치</label>
                  <input
                    type="text"
                    defaultValue="/backup"
                    className="mgmt-input"
                  />
                </div>
                <div className="mgmt-filter-group">
                  <label>기본 보관 기간 (일)</label>
                  <input
                    type="number"
                    defaultValue="30"
                    className="mgmt-input"
                  />
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem', marginTop: '0.5rem' }}>
                  <label className="mgmt-checkbox-label">
                    <input type="checkbox" defaultChecked />
                    기본적으로 암호화 사용
                  </label>
                  <label className="mgmt-checkbox-label">
                    <input type="checkbox" defaultChecked />
                    기본적으로 압축 사용
                  </label>
                </div>
              </div>
            </div>

            {/* 알림 설정 */}
            <div className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
              <h3 className="text-lg font-semibold text-neutral-800 mb-4">알림 설정</h3>
              <div className="checkbox-group">
                <div className="checkbox-item">
                  <input type="checkbox" id="notifySuccess" defaultChecked />
                  <label htmlFor="notifySuccess">백업 성공 시 알림</label>
                </div>
                <div className="checkbox-item">
                  <input type="checkbox" id="notifyFailure" defaultChecked />
                  <label htmlFor="notifyFailure">백업 실패 시 알림</label>
                </div>
                <div className="checkbox-item">
                  <input type="checkbox" id="notifyWeekly" />
                  <label htmlFor="notifyWeekly">주간 백업 보고서</label>
                </div>
              </div>
            </div>

            {/* 저장소 설정 */}
            <div className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
              <h3 className="text-lg font-semibold text-neutral-800 mb-4">저장소 설정</h3>
              <div className="space-y-4">
                <div className="form-group">
                  <label>최대 백업 개수</label>
                  <input
                    type="number"
                    defaultValue="50"
                    className="form-input"
                  />
                </div>
                <div className="form-group">
                  <label>최대 저장소 사용량 (GB)</label>
                  <input
                    type="number"
                    defaultValue="100"
                    className="form-input"
                  />
                </div>
                <div className="checkbox-item">
                  <input type="checkbox" id="autoCleanup" defaultChecked />
                  <label htmlFor="autoCleanup">자동 정리 활성화</label>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* 즉시 백업 모달 */}
      {showBackupModal && (
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h2>즉시 백업 생성</h2>
              <button
                className="modal-close-btn"
                onClick={() => setShowBackupModal(false)}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>

            <div className="modal-content">
              <div className="form-section">
                <h3>백업 정보</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>백업명 <span className="required">*</span></label>
                    <input
                      type="text"
                      value={backupFormData.name}
                      onChange={(e) => setBackupFormData(prev => ({ ...prev, name: e.target.value }))}
                      className="form-input"
                      placeholder="백업명을 입력하세요"
                    />
                  </div>

                  <div className="form-group">
                    <label>백업 타입</label>
                    <select
                      value={backupFormData.type}
                      onChange={(e) => setBackupFormData(prev => ({ ...prev, type: e.target.value as any }))}
                      className="form-select"
                    >
                      <option value="full">전체 백업</option>
                      <option value="incremental">증분 백업</option>
                      <option value="differential">차등 백업</option>
                    </select>
                  </div>

                  <div className="form-group full-width">
                    <label>설명</label>
                    <textarea
                      value={backupFormData.description}
                      onChange={(e) => setBackupFormData(prev => ({ ...prev, description: e.target.value }))}
                      className="form-input"
                      rows={3}
                      placeholder="백업에 대한 설명을 입력하세요"
                    />
                  </div>

                  <div className="form-group full-width">
                    <label>저장 위치</label>
                    <input
                      type="text"
                      value={backupFormData.location}
                      onChange={(e) => setBackupFormData(prev => ({ ...prev, location: e.target.value }))}
                      className="form-input"
                    />
                  </div>
                </div>
              </div>

              <div className="form-section">
                <h3>백업 구성요소</h3>
                <div className="permissions-grid">
                  {availableComponents.map(component => (
                    <div key={component.id} className="permission-item">
                      <input
                        type="checkbox"
                        id={`backup-${component.id}`}
                        checked={backupFormData.components.includes(component.id)}
                        onChange={(e) => {
                          const components = backupFormData.components;
                          if (e.target.checked) {
                            setBackupFormData(prev => ({
                              ...prev,
                              components: [...components, component.id]
                            }));
                          } else {
                            setBackupFormData(prev => ({
                              ...prev,
                              components: components.filter(c => c !== component.id)
                            }));
                          }
                        }}
                      />
                      <div>
                        <label htmlFor={`backup-${component.id}`}>{component.name}</label>
                        <div className="permission-description">{component.description}</div>
                      </div>
                    </div>
                  ))}
                </div>
              </div>

              <div className="form-section">
                <h3>백업 옵션</h3>
                <div className="checkbox-group">
                  <div className="checkbox-item">
                    <input
                      type="checkbox"
                      id="encryption"
                      checked={backupFormData.encryption}
                      onChange={(e) => setBackupFormData(prev => ({ ...prev, encryption: e.target.checked }))}
                    />
                    <label htmlFor="encryption">암호화 사용</label>
                  </div>
                  <div className="checkbox-item">
                    <input
                      type="checkbox"
                      id="compression"
                      checked={backupFormData.compression}
                      onChange={(e) => setBackupFormData(prev => ({ ...prev, compression: e.target.checked }))}
                    />
                    <label htmlFor="compression">압축 사용</label>
                  </div>
                </div>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-primary"
                onClick={handleCreateBackup}
              >
                <i className="fas fa-play"></i>
                백업 시작
              </button>
              <button
                className="btn btn-outline"
                onClick={() => setShowBackupModal(false)}
              >
                취소
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default BackupRestore;