import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/user-management.css';

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
    <div className="user-management-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">백업 및 복원</h1>
        <div className="page-actions">
          <button
            className="btn btn-primary"
            onClick={() => setShowBackupModal(true)}
          >
            <i className="fas fa-plus"></i>
            즉시 백업
          </button>
          <button className="btn btn-outline">
            <i className="fas fa-upload"></i>
            백업 업로드
          </button>
        </div>
      </div>

      {/* 백업 상태 요약 */}
      <div className="user-stats-panel mb-6">
        <div className="stat-card">
          <div className="stat-icon">
            <i className="fas fa-database text-primary-600"></i>
          </div>
          <div className="stat-value">{backupRecords.length}</div>
          <div className="stat-label">총 백업</div>
        </div>
        <div className="stat-card">
          <div className="stat-icon">
            <i className="fas fa-check-circle text-success-600"></i>
          </div>
          <div className="stat-value">{backupRecords.filter(b => b.status === 'completed').length}</div>
          <div className="stat-label">성공</div>
        </div>
        <div className="stat-card">
          <div className="stat-icon">
            <i className="fas fa-hdd text-warning-600"></i>
          </div>
          <div className="stat-value">
            {formatBytes(backupRecords.reduce((sum, b) => sum + b.size, 0))}
          </div>
          <div className="stat-label">총 용량</div>
        </div>
        <div className="stat-card">
          <div className="stat-icon">
            <i className="fas fa-clock text-primary-600"></i>
          </div>
          <div className="stat-value">{scheduledBackups.filter(s => s.enabled).length}</div>
          <div className="stat-label">예약된 백업</div>
        </div>
      </div>

      {/* 탭 네비게이션 */}
      <div className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'backups' ? 'active' : ''}`}
          onClick={() => setActiveTab('backups')}
        >
          <i className="fas fa-archive"></i>
          백업 목록
          <span className="tab-badge">{backupRecords.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'schedule' ? 'active' : ''}`}
          onClick={() => setActiveTab('schedule')}
        >
          <i className="fas fa-calendar-alt"></i>
          예약 백업
          <span className="tab-badge">{scheduledBackups.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'restore' ? 'active' : ''}`}
          onClick={() => setActiveTab('restore')}
        >
          <i className="fas fa-undo"></i>
          복원 작업
          <span className="tab-badge">{restoreJobs.length}</span>
        </button>
        <button
          className={`tab-button ${activeTab === 'settings' ? 'active' : ''}`}
          onClick={() => setActiveTab('settings')}
        >
          <i className="fas fa-cog"></i>
          백업 설정
        </button>
      </div>

      {/* 백업 목록 탭 */}
      {activeTab === 'backups' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">백업 목록</h2>
            <div className="users-actions">
              <span className="text-sm text-neutral-600">
                총 {backupRecords.length}개 백업
              </span>
            </div>
          </div>

          <div className="users-table">
            <div className="table-header">
              <div className="header-cell">백업명</div>
              <div className="header-cell">타입</div>
              <div className="header-cell">상태</div>
              <div className="header-cell">크기</div>
              <div className="header-cell">생성일</div>
              <div className="header-cell">소요시간</div>
              <div className="header-cell">위치</div>
              <div className="header-cell">액션</div>
            </div>

            {currentBackups.map(backup => (
              <div key={backup.id} className="table-row">
                <div className="table-cell" data-label="백업명">
                  <div className="user-name">{backup.name}</div>
                  <div className="user-email">{backup.description}</div>
                  <div className="user-id">
                    {backup.includedComponents.join(', ')}
                  </div>
                </div>

                <div className="table-cell" data-label="타입">
                  <span className={`role-badge ${
                    backup.type === 'full' ? 'role-admin' : 
                    backup.type === 'incremental' ? 'role-engineer' : 'role-manager'
                  }`}>
                    {backup.type === 'full' ? '전체' :
                     backup.type === 'incremental' ? '증분' : '차등'}
                  </span>
                </div>

                <div className="table-cell" data-label="상태">
                  <div className="status-indicator">
                    <i className={`fas ${getStatusIcon(backup.status)}`}></i>
                    <span className="status-text">
                      {backup.status === 'completed' ? '완료' :
                       backup.status === 'running' ? '진행중' :
                       backup.status === 'failed' ? '실패' : '예약됨'}
                    </span>
                  </div>
                </div>

                <div className="table-cell" data-label="크기">
                  <div className="text-neutral-700">{formatBytes(backup.size)}</div>
                  <div className="text-xs text-neutral-500">
                    {backup.compression && <span>압축됨</span>}
                    {backup.encryption && <span> • 암호화됨</span>}
                  </div>
                </div>

                <div className="table-cell" data-label="생성일">
                  <div className="last-login-time">
                    {backup.createdAt.toLocaleString()}
                  </div>
                  <div className="login-count">
                    by {backup.createdBy}
                  </div>
                </div>

                <div className="table-cell" data-label="소요시간">
                  <div className="text-neutral-700">
                    {backup.duration ? formatDuration(backup.duration) : '-'}
                  </div>
                </div>

                <div className="table-cell" data-label="위치">
                  <div className="text-neutral-700 font-mono text-xs">
                    {backup.location}
                  </div>
                </div>

                <div className="table-cell action-cell" data-label="액션">
                  <div className="action-buttons">
                    {backup.status === 'completed' && (
                      <button
                        className="btn btn-sm btn-success"
                        onClick={() => handleRestoreFromBackup(backup)}
                        title="복원"
                      >
                        <i className="fas fa-undo"></i>
                      </button>
                    )}
                    <button
                      className="btn btn-sm btn-outline"
                      onClick={() => {
                        alert('백업 파일을 다운로드합니다.');
                      }}
                      title="다운로드"
                    >
                      <i className="fas fa-download"></i>
                    </button>
                    <button
                      className="btn btn-sm btn-error"
                      onClick={() => handleDeleteBackup(backup.id)}
                      title="삭제"
                    >
                      <i className="fas fa-trash"></i>
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>

          {/* 페이지네이션 */}
          {backupRecords.length > 0 && (
            <div className="pagination-container">
              <div className="pagination-info">
                {startIndex + 1}-{Math.min(endIndex, backupRecords.length)} / {backupRecords.length} 항목
              </div>
              
              <div className="pagination-controls">
                <select
                  value={pageSize}
                  onChange={(e) => {
                    setPageSize(Number(e.target.value));
                    setCurrentPage(1);
                  }}
                  className="page-size-select"
                >
                  <option value={10}>10개씩</option>
                  <option value={20}>20개씩</option>
                  <option value={50}>50개씩</option>
                </select>

                <button
                  className="btn btn-sm"
                  disabled={currentPage === 1}
                  onClick={() => setCurrentPage(1)}
                >
                  <i className="fas fa-angle-double-left"></i>
                </button>
                <button
                  className="btn btn-sm"
                  disabled={currentPage === 1}
                  onClick={() => setCurrentPage(prev => prev - 1)}
                >
                  <i className="fas fa-angle-left"></i>
                </button>
                
                <span className="page-info">
                  {currentPage} / {Math.ceil(backupRecords.length / pageSize)}
                </span>
                
                <button
                  className="btn btn-sm"
                  disabled={currentPage === Math.ceil(backupRecords.length / pageSize)}
                  onClick={() => setCurrentPage(prev => prev + 1)}
                >
                  <i className="fas fa-angle-right"></i>
                </button>
                <button
                  className="btn btn-sm"
                  disabled={currentPage === Math.ceil(backupRecords.length / pageSize)}
                  onClick={() => setCurrentPage(Math.ceil(backupRecords.length / pageSize))}
                >
                  <i className="fas fa-angle-double-right"></i>
                </button>
              </div>
            </div>
          )}
        </div>
      )}

      {/* 예약 백업 탭 */}
      {activeTab === 'schedule' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">예약된 백업</h2>
            <div className="users-actions">
              <button className="btn btn-primary">
                <i className="fas fa-plus"></i>
                새 예약 백업
              </button>
            </div>
          </div>

          <div className="space-y-4">
            {scheduledBackups.map(schedule => (
              <div key={schedule.id} className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 bg-primary-100 rounded-lg flex items-center justify-center">
                      <i className="fas fa-calendar-alt text-primary-600"></i>
                    </div>
                    <div>
                      <h3 className="text-lg font-semibold text-neutral-800">{schedule.name}</h3>
                      <div className="flex items-center gap-2">
                        <span className={`role-badge ${schedule.type === 'full' ? 'role-admin' : 'role-engineer'}`}>
                          {schedule.type === 'full' ? '전체' : '증분'}
                        </span>
                        <span className="text-sm text-neutral-600">
                          {schedule.schedule.frequency === 'daily' ? '매일' :
                           schedule.schedule.frequency === 'weekly' ? '매주' : '매월'} 
                          {' '}{schedule.schedule.time}
                        </span>
                      </div>
                    </div>
                  </div>
                  <div className="flex items-center gap-2">
                    <label className="status-toggle">
                      <input
                        type="checkbox"
                        checked={schedule.enabled}
                        onChange={() => handleToggleScheduledBackup(schedule.id)}
                      />
                      <span className="toggle-slider"></span>
                    </label>
                    <button className="btn btn-sm btn-outline">
                      <i className="fas fa-edit"></i>
                      편집
                    </button>
                  </div>
                </div>

                <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
                  <div>
                    <label className="text-xs font-medium text-neutral-500">다음 실행</label>
                    <div className="text-sm text-neutral-800">
                      {schedule.nextRun.toLocaleString()}
                    </div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">마지막 실행</label>
                    <div className="text-sm text-neutral-800">
                      {schedule.lastRun?.toLocaleString() || 'N/A'}
                    </div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">보관 기간</label>
                    <div className="text-sm text-neutral-800">{schedule.retention}일</div>
                  </div>
                  <div>
                    <label className="text-xs font-medium text-neutral-500">저장 위치</label>
                    <div className="text-sm text-neutral-800 font-mono">{schedule.location}</div>
                  </div>
                </div>

                <div className="mt-4">
                  <label className="text-xs font-medium text-neutral-500">포함 구성요소</label>
                  <div className="flex flex-wrap gap-1 mt-1">
                    {schedule.components.map(component => (
                      <span key={component} className="permission-tag">
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
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">복원 작업</h2>
            <div className="users-actions">
              <span className="text-sm text-neutral-600">
                최근 복원 작업 목록
              </span>
            </div>
          </div>

          {restoreJobs.length > 0 ? (
            <div className="space-y-4">
              {restoreJobs.map(job => (
                <div key={job.id} className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
                  <div className="flex items-center justify-between mb-4">
                    <div>
                      <h3 className="text-lg font-semibold text-neutral-800">
                        {job.backupName} 복원
                      </h3>
                      <div className="flex items-center gap-2">
                        <i className={`fas ${getStatusIcon(job.status)}`}></i>
                        <span className="text-sm text-neutral-600">
                          {job.status === 'completed' ? '완료' :
                           job.status === 'running' ? '진행중' : '실패'}
                        </span>
                      </div>
                    </div>
                    <div className="text-right">
                      <div className="text-sm text-neutral-600">
                        시작: {job.startedAt.toLocaleString()}
                      </div>
                      {job.completedAt && (
                        <div className="text-sm text-neutral-600">
                          완료: {job.completedAt.toLocaleString()}
                        </div>
                      )}
                    </div>
                  </div>

                  {job.status === 'running' && (
                    <div className="mb-4">
                      <div className="flex justify-between items-center mb-2">
                        <span className="text-sm text-neutral-600">진행률</span>
                        <span className="text-sm text-neutral-800">{job.progress}%</span>
                      </div>
                      <div className="w-full bg-neutral-200 rounded-full h-2">
                        <div 
                          className="bg-primary-500 h-2 rounded-full transition-all duration-300"
                          style={{ width: `${job.progress}%` }}
                        ></div>
                      </div>
                    </div>
                  )}

                  <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
                    <div>
                      <label className="text-xs font-medium text-neutral-500">복원된 구성요소</label>
                      <div className="flex flex-wrap gap-1 mt-1">
                        {job.restoredComponents.map(component => (
                          <span key={component} className="permission-tag">
                            {availableComponents.find(c => c.id === component)?.name || component}
                          </span>
                        ))}
                      </div>
                    </div>
                    {job.errors.length > 0 && (
                      <div>
                        <label className="text-xs font-medium text-error-600">오류</label>
                        <div className="text-sm text-error-700 mt-1">
                          {job.errors.join(', ')}
                        </div>
                      </div>
                    )}
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <div className="empty-state">
              <i className="fas fa-undo empty-icon"></i>
              <div className="empty-title">복원 작업이 없습니다</div>
              <div className="empty-description">
                백업에서 데이터를 복원한 기록이 없습니다.
              </div>
            </div>
          )}
        </div>
      )}

      {/* 백업 설정 탭 */}
      {activeTab === 'settings' && (
        <div className="users-management">
          <div className="users-header">
            <h2 className="users-title">백업 설정</h2>
          </div>

          <div className="space-y-6">
            {/* 기본 설정 */}
            <div className="bg-white rounded-lg shadow-sm border border-neutral-200 p-6">
              <h3 className="text-lg font-semibold text-neutral-800 mb-4">기본 설정</h3>
              <div className="space-y-4">
                <div className="form-group">
                  <label>기본 백업 위치</label>
                  <input
                    type="text"
                    defaultValue="/backup"
                    className="form-input"
                  />
                </div>
                <div className="form-group">
                  <label>기본 보관 기간 (일)</label>
                  <input
                    type="number"
                    defaultValue="30"
                    className="form-input"
                  />
                </div>
                <div className="checkbox-group">
                  <div className="checkbox-item">
                    <input type="checkbox" id="defaultEncryption" defaultChecked />
                    <label htmlFor="defaultEncryption">기본적으로 암호화 사용</label>
                  </div>
                  <div className="checkbox-item">
                    <input type="checkbox" id="defaultCompression" defaultChecked />
                    <label htmlFor="defaultCompression">기본적으로 압축 사용</label>
                  </div>
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