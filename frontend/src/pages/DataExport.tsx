import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/data-export.css';

interface ExportTask {
  id: string;
  name: string;
  type: 'historical' | 'realtime' | 'virtual' | 'alarms' | 'logs';
  format: 'csv' | 'json' | 'xlsx' | 'pdf';
  dateRange: {
    start: Date;
    end: Date;
  };
  filters: {
    factories: string[];
    devices: string[];
    points: string[];
    categories: string[];
  };
  schedule?: {
    type: 'once' | 'daily' | 'weekly' | 'monthly';
    time?: string;
    day?: number;
    enabled: boolean;
  };
  status: 'pending' | 'running' | 'completed' | 'failed' | 'cancelled';
  progress: number;
  fileSize?: number;
  downloadUrl?: string;
  error?: string;
  createdAt: Date;
  completedAt?: Date;
  createdBy: string;
}

interface ExportTemplate {
  id: string;
  name: string;
  description: string;
  type: ExportTask['type'];
  format: ExportTask['format'];
  defaultFilters: ExportTask['filters'];
  isDefault: boolean;
  usageCount: number;
}

const DataExport: React.FC = () => {
  const [exportTasks, setExportTasks] = useState<ExportTask[]>([]);
  const [templates, setTemplates] = useState<ExportTemplate[]>([]);
  const [currentTask, setCurrentTask] = useState<Partial<ExportTask>>({
    type: 'historical',
    format: 'csv',
    dateRange: {
      start: new Date(Date.now() - 24 * 60 * 60 * 1000),
      end: new Date()
    },
    filters: {
      factories: [],
      devices: [],
      points: [],
      categories: []
    }
  });
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [activeTab, setActiveTab] = useState<'create' | 'history' | 'templates'>('create');

  // 목업 데이터 초기화
  useEffect(() => {
    initializeMockData();
    
    // 실시간 진행률 업데이트 시뮬레이션
    const interval = setInterval(() => {
      setExportTasks(prev => prev.map(task => {
        if (task.status === 'running' && task.progress < 100) {
          const newProgress = Math.min(task.progress + Math.random() * 10, 100);
          return {
            ...task,
            progress: newProgress,
            status: newProgress >= 100 ? 'completed' : 'running',
            completedAt: newProgress >= 100 ? new Date() : undefined,
            fileSize: newProgress >= 100 ? Math.floor(Math.random() * 50000000) + 1000000 : undefined,
            downloadUrl: newProgress >= 100 ? `/downloads/${task.id}.${task.format}` : undefined
          };
        }
        return task;
      }));
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  const initializeMockData = () => {
    // 목업 템플릿
    const mockTemplates: ExportTemplate[] = [
      {
        id: 'tmpl_001',
        name: '일일 생산 데이터',
        description: '매일 생산라인의 주요 데이터를 CSV로 내보내기',
        type: 'historical',
        format: 'csv',
        defaultFilters: {
          factories: ['Factory A'],
          devices: ['PLC-001', 'PLC-002'],
          points: ['Temperature', 'Pressure', 'Production Count'],
          categories: ['Production']
        },
        isDefault: true,
        usageCount: 45
      },
      {
        id: 'tmpl_002',
        name: '주간 알람 리포트',
        description: '주간 알람 발생 현황을 PDF로 생성',
        type: 'alarms',
        format: 'pdf',
        defaultFilters: {
          factories: [],
          devices: [],
          points: [],
          categories: ['Safety', 'Critical']
        },
        isDefault: false,
        usageCount: 12
      },
      {
        id: 'tmpl_003',
        name: '에너지 사용량 분석',
        description: '월간 에너지 사용량 데이터를 Excel로 내보내기',
        type: 'historical',
        format: 'xlsx',
        defaultFilters: {
          factories: [],
          devices: [],
          points: ['Power Consumption', 'Energy Meter'],
          categories: ['Energy']
        },
        isDefault: false,
        usageCount: 8
      }
    ];

    // 목업 내보내기 작업
    const mockTasks: ExportTask[] = [
      {
        id: 'exp_001',
        name: '일일 생산 데이터 - 2024-12-01',
        type: 'historical',
        format: 'csv',
        dateRange: {
          start: new Date('2024-12-01'),
          end: new Date('2024-12-01T23:59:59')
        },
        filters: {
          factories: ['Factory A'],
          devices: ['PLC-001', 'PLC-002'],
          points: ['Temperature', 'Pressure'],
          categories: ['Production']
        },
        status: 'completed',
        progress: 100,
        fileSize: 2457600,
        downloadUrl: '/downloads/exp_001.csv',
        createdAt: new Date(Date.now() - 3600000),
        completedAt: new Date(Date.now() - 3000000),
        createdBy: 'user1'
      },
      {
        id: 'exp_002',
        name: '알람 이력 - 지난주',
        type: 'alarms',
        format: 'xlsx',
        dateRange: {
          start: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000),
          end: new Date()
        },
        filters: {
          factories: [],
          devices: [],
          points: [],
          categories: ['Safety']
        },
        status: 'running',
        progress: 65,
        createdAt: new Date(Date.now() - 300000),
        createdBy: 'user2'
      },
      {
        id: 'exp_003',
        name: '시스템 로그 - 어제',
        type: 'logs',
        format: 'json',
        dateRange: {
          start: new Date(Date.now() - 24 * 60 * 60 * 1000),
          end: new Date()
        },
        filters: {
          factories: [],
          devices: [],
          points: [],
          categories: []
        },
        status: 'failed',
        progress: 45,
        error: '데이터베이스 연결 오류',
        createdAt: new Date(Date.now() - 1800000),
        createdBy: 'admin'
      }
    ];

    setTemplates(mockTemplates);
    setExportTasks(mockTasks);
  };

  // 내보내기 작업 시작
  const handleStartExport = async () => {
    if (!currentTask.name) {
      alert('내보내기 작업 이름을 입력해주세요.');
      return;
    }

    const newTask: ExportTask = {
      id: `exp_${Date.now()}`,
      name: currentTask.name!,
      type: currentTask.type!,
      format: currentTask.format!,
      dateRange: currentTask.dateRange!,
      filters: currentTask.filters!,
      schedule: currentTask.schedule,
      status: 'pending',
      progress: 0,
      createdAt: new Date(),
      createdBy: 'current_user'
    };

    setExportTasks(prev => [newTask, ...prev]);

    // 작업 시작 시뮬레이션
    setTimeout(() => {
      setExportTasks(prev => prev.map(task =>
        task.id === newTask.id
          ? { ...task, status: 'running', progress: 5 }
          : task
      ));
    }, 1000);

    // 폼 초기화
    setCurrentTask({
      type: 'historical',
      format: 'csv',
      dateRange: {
        start: new Date(Date.now() - 24 * 60 * 60 * 1000),
        end: new Date()
      },
      filters: {
        factories: [],
        devices: [],
        points: [],
        categories: []
      }
    });

    alert('내보내기 작업이 시작되었습니다.');
  };

  // 템플릿 적용
  const applyTemplate = (template: ExportTemplate) => {
    setCurrentTask({
      ...currentTask,
      name: `${template.name} - ${new Date().toLocaleDateString()}`,
      type: template.type,
      format: template.format,
      filters: template.defaultFilters
    });

    // 템플릿 사용 횟수 증가
    setTemplates(prev => prev.map(t =>
      t.id === template.id ? { ...t, usageCount: t.usageCount + 1 } : t
    ));

    setActiveTab('create');
  };

  // 작업 취소
  const cancelTask = (taskId: string) => {
    setExportTasks(prev => prev.map(task =>
      task.id === taskId && task.status === 'running'
        ? { ...task, status: 'cancelled' }
        : task
    ));
  };

  // 작업 삭제
  const deleteTask = (taskId: string) => {
    if (confirm('이 내보내기 작업을 삭제하시겠습니까?')) {
      setExportTasks(prev => prev.filter(task => task.id !== taskId));
    }
  };

  // 파일 크기 포맷
  const formatFileSize = (bytes: number): string => {
    const units = ['B', 'KB', 'MB', 'GB'];
    let size = bytes;
    let unitIndex = 0;

    while (size >= 1024 && unitIndex < units.length - 1) {
      size /= 1024;
      unitIndex++;
    }

    return `${size.toFixed(1)} ${units[unitIndex]}`;
  };

  // 빠른 날짜 설정
  const setQuickDateRange = (hours: number) => {
    const end = new Date();
    const start = new Date(end.getTime() - hours * 60 * 60 * 1000);
    setCurrentTask(prev => ({
      ...prev,
      dateRange: { start, end }
    }));
  };

  return (
    <div className="data-export-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">데이터 내보내기</h1>
        <div className="page-actions">
          <span className="text-sm text-neutral-500">
            진행 중: {exportTasks.filter(t => t.status === 'running').length}개
          </span>
        </div>
      </div>

      {/* 탭 네비게이션 */}
      <div className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'create' ? 'active' : ''}`}
          onClick={() => setActiveTab('create')}
        >
          <i className="fas fa-plus"></i>
          새 내보내기
        </button>
        <button
          className={`tab-button ${activeTab === 'history' ? 'active' : ''}`}
          onClick={() => setActiveTab('history')}
        >
          <i className="fas fa-history"></i>
          내보내기 이력
        </button>
        <button
          className={`tab-button ${activeTab === 'templates' ? 'active' : ''}`}
          onClick={() => setActiveTab('templates')}
        >
          <i className="fas fa-bookmark"></i>
          템플릿 관리
        </button>
      </div>

      {/* 새 내보내기 탭 */}
      {activeTab === 'create' && (
        <div className="create-export-panel">
          <div className="export-form">
            <div className="form-section">
              <h3>기본 설정</h3>
              <div className="form-row">
                <div className="form-group">
                  <label>작업 이름 *</label>
                  <input
                    type="text"
                    value={currentTask.name || ''}
                    onChange={(e) => setCurrentTask(prev => ({ ...prev, name: e.target.value }))}
                    className="form-input"
                    placeholder="내보내기 작업 이름"
                  />
                </div>
                <div className="form-group">
                  <label>데이터 타입 *</label>
                  <select
                    value={currentTask.type}
                    onChange={(e) => setCurrentTask(prev => ({ ...prev, type: e.target.value as any }))}
                    className="form-select"
                  >
                    <option value="historical">이력 데이터</option>
                    <option value="realtime">실시간 데이터</option>
                    <option value="virtual">가상포인트 데이터</option>
                    <option value="alarms">알람 이력</option>
                    <option value="logs">시스템 로그</option>
                  </select>
                </div>
              </div>

              <div className="form-row">
                <div className="form-group">
                  <label>파일 형식 *</label>
                  <select
                    value={currentTask.format}
                    onChange={(e) => setCurrentTask(prev => ({ ...prev, format: e.target.value as any }))}
                    className="form-select"
                  >
                    <option value="csv">CSV (쉼표로 구분)</option>
                    <option value="xlsx">Excel (XLSX)</option>
                    <option value="json">JSON</option>
                    <option value="pdf">PDF 리포트</option>
                  </select>
                </div>
                <div className="form-group">
                  <label>기간 설정 *</label>
                  <div className="date-range-inputs">
                    <input
                      type="datetime-local"
                      value={currentTask.dateRange?.start.toISOString().slice(0, 16)}
                      onChange={(e) => setCurrentTask(prev => ({
                        ...prev,
                        dateRange: { ...prev.dateRange!, start: new Date(e.target.value) }
                      }))}
                      className="form-input"
                    />
                    <span>~</span>
                    <input
                      type="datetime-local"
                      value={currentTask.dateRange?.end.toISOString().slice(0, 16)}
                      onChange={(e) => setCurrentTask(prev => ({
                        ...prev,
                        dateRange: { ...prev.dateRange!, end: new Date(e.target.value) }
                      }))}
                      className="form-input"
                    />
                  </div>
                  <div className="quick-dates">
                    <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(1)}>1시간</button>
                    <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24)}>24시간</button>
                    <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 7)}>7일</button>
                    <button className="btn btn-sm btn-outline" onClick={() => setQuickDateRange(24 * 30)}>30일</button>
                  </div>
                </div>
              </div>
            </div>

            {/* 고급 설정 */}
            <div className="form-section">
              <div className="section-header">
                <h3>필터 설정</h3>
                <button
                  className="btn btn-sm btn-outline"
                  onClick={() => setShowAdvanced(!showAdvanced)}
                >
                  <i className={`fas fa-chevron-${showAdvanced ? 'up' : 'down'}`}></i>
                  {showAdvanced ? '간단히' : '고급 설정'}
                </button>
              </div>

              {showAdvanced && (
                <div className="advanced-filters">
                  <div className="form-row">
                    <div className="form-group">
                      <label>공장 선택</label>
                      <select
                        multiple
                        value={currentTask.filters?.factories || []}
                        onChange={(e) => setCurrentTask(prev => ({
                          ...prev,
                          filters: {
                            ...prev.filters!,
                            factories: Array.from(e.target.selectedOptions, option => option.value)
                          }
                        }))}
                        className="form-select multi-select"
                      >
                        <option value="Factory A">Factory A</option>
                        <option value="Factory B">Factory B</option>
                        <option value="Factory C">Factory C</option>
                      </select>
                    </div>
                    <div className="form-group">
                      <label>디바이스 선택</label>
                      <select
                        multiple
                        value={currentTask.filters?.devices || []}
                        onChange={(e) => setCurrentTask(prev => ({
                          ...prev,
                          filters: {
                            ...prev.filters!,
                            devices: Array.from(e.target.selectedOptions, option => option.value)
                          }
                        }))}
                        className="form-select multi-select"
                      >
                        <option value="PLC-001">PLC-001</option>
                        <option value="PLC-002">PLC-002</option>
                        <option value="Sensor-001">Sensor-001</option>
                        <option value="HMI-001">HMI-001</option>
                      </select>
                    </div>
                  </div>

                  <div className="form-row">
                    <div className="form-group">
                      <label>데이터 포인트</label>
                      <select
                        multiple
                        value={currentTask.filters?.points || []}
                        onChange={(e) => setCurrentTask(prev => ({
                          ...prev,
                          filters: {
                            ...prev.filters!,
                            points: Array.from(e.target.selectedOptions, option => option.value)
                          }
                        }))}
                        className="form-select multi-select"
                      >
                        <option value="Temperature">Temperature</option>
                        <option value="Pressure">Pressure</option>
                        <option value="Flow Rate">Flow Rate</option>
                        <option value="Production Count">Production Count</option>
                      </select>
                    </div>
                    <div className="form-group">
                      <label>카테고리</label>
                      <select
                        multiple
                        value={currentTask.filters?.categories || []}
                        onChange={(e) => setCurrentTask(prev => ({
                          ...prev,
                          filters: {
                            ...prev.filters!,
                            categories: Array.from(e.target.selectedOptions, option => option.value)
                          }
                        }))}
                        className="form-select multi-select"
                      >
                        <option value="Production">Production</option>
                        <option value="Safety">Safety</option>
                        <option value="Energy">Energy</option>
                        <option value="Quality">Quality</option>
                      </select>
                    </div>
                  </div>
                </div>
              )}
            </div>

            <div className="form-actions">
              <button
                className="btn btn-primary"
                onClick={handleStartExport}
                disabled={!currentTask.name}
              >
                <i className="fas fa-download"></i>
                내보내기 시작
              </button>
              <button
                className="btn btn-outline"
                onClick={() => setCurrentTask({
                  type: 'historical',
                  format: 'csv',
                  dateRange: {
                    start: new Date(Date.now() - 24 * 60 * 60 * 1000),
                    end: new Date()
                  },
                  filters: { factories: [], devices: [], points: [], categories: [] }
                })}
              >
                초기화
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 내보내기 이력 탭 */}
      {activeTab === 'history' && (
        <div className="export-history">
          <div className="history-filters">
            <div className="filter-row">
              <select className="form-select">
                <option value="all">전체 상태</option>
                <option value="completed">완료</option>
                <option value="running">실행 중</option>
                <option value="failed">실패</option>
              </select>
              <select className="form-select">
                <option value="all">전체 타입</option>
                <option value="historical">이력 데이터</option>
                <option value="alarms">알람</option>
                <option value="logs">로그</option>
              </select>
              <input
                type="text"
                placeholder="작업명 검색..."
                className="form-input"
              />
            </div>
          </div>

          <div className="tasks-list">
            {exportTasks.map(task => (
              <div key={task.id} className={`task-card ${task.status}`}>
                <div className="task-header">
                  <div className="task-info">
                    <h4 className="task-name">{task.name}</h4>
                    <div className="task-meta">
                      <span className="task-type">{task.type}</span>
                      <span className="task-format">{task.format.toUpperCase()}</span>
                      <span className="task-date">
                        {task.dateRange.start.toLocaleDateString()} ~ {task.dateRange.end.toLocaleDateString()}
                      </span>
                    </div>
                  </div>
                  <div className="task-status">
                    <span className={`status-badge ${task.status}`}>
                      {task.status === 'completed' ? '완료' :
                       task.status === 'running' ? '진행 중' :
                       task.status === 'failed' ? '실패' :
                       task.status === 'cancelled' ? '취소됨' : '대기 중'}
                    </span>
                  </div>
                </div>

                <div className="task-progress">
                  <div className="progress-bar">
                    <div 
                      className="progress-fill"
                      style={{ width: `${task.progress}%` }}
                    ></div>
                  </div>
                  <span className="progress-text">{task.progress}%</span>
                </div>

                <div className="task-details">
                  <div className="detail-row">
                    <span>생성일:</span>
                    <span>{task.createdAt.toLocaleString()}</span>
                  </div>
                  {task.completedAt && (
                    <div className="detail-row">
                      <span>완료일:</span>
                      <span>{task.completedAt.toLocaleString()}</span>
                    </div>
                  )}
                  {task.fileSize && (
                    <div className="detail-row">
                      <span>파일 크기:</span>
                      <span>{formatFileSize(task.fileSize)}</span>
                    </div>
                  )}
                  {task.error && (
                    <div className="detail-row error">
                      <span>오류:</span>
                      <span>{task.error}</span>
                    </div>
                  )}
                </div>

                <div className="task-actions">
                  {task.status === 'completed' && task.downloadUrl && (
                    <a
                      href={task.downloadUrl}
                      className="btn btn-sm btn-primary"
                      download
                    >
                      <i className="fas fa-download"></i>
                      다운로드
                    </a>
                  )}
                  {task.status === 'running' && (
                    <button
                      className="btn btn-sm btn-warning"
                      onClick={() => cancelTask(task.id)}
                    >
                      <i className="fas fa-stop"></i>
                      취소
                    </button>
                  )}
                  <button
                    className="btn btn-sm btn-outline"
                    onClick={() => deleteTask(task.id)}
                  >
                    <i className="fas fa-trash"></i>
                    삭제
                  </button>
                </div>
              </div>
            ))}
          </div>

          {exportTasks.length === 0 && (
            <div className="empty-state">
              <i className="fas fa-file-export empty-icon"></i>
              <div className="empty-title">내보내기 이력이 없습니다</div>
              <div className="empty-description">
                새 내보내기 작업을 생성해보세요.
              </div>
            </div>
          )}
        </div>
      )}

      {/* 템플릿 관리 탭 */}
      {activeTab === 'templates' && (
        <div className="templates-panel">
          <div className="templates-header">
            <h3>내보내기 템플릿</h3>
            <button className="btn btn-primary">
              <i className="fas fa-plus"></i>
              새 템플릿
            </button>
          </div>

          <div className="templates-grid">
            {templates.map(template => (
              <div key={template.id} className="template-card">
                <div className="template-header">
                  <h4 className="template-name">{template.name}</h4>
                  {template.isDefault && (
                    <span className="default-badge">기본</span>
                  )}
                </div>
                
                <p className="template-description">{template.description}</p>
                
                <div className="template-details">
                  <div className="template-meta">
                    <span className="type-badge">{template.type}</span>
                    <span className="format-badge">{template.format.toUpperCase()}</span>
                  </div>
                  <div className="usage-count">
                    사용 횟수: {template.usageCount}회
                  </div>
                </div>

                <div className="template-actions">
                  <button
                    className="btn btn-sm btn-primary"
                    onClick={() => applyTemplate(template)}
                  >
                    <i className="fas fa-play"></i>
                    적용
                  </button>
                  <button className="btn btn-sm btn-outline">
                    <i className="fas fa-edit"></i>
                    편집
                  </button>
                  {!template.isDefault && (
                    <button className="btn btn-sm btn-error">
                      <i className="fas fa-trash"></i>
                      삭제
                    </button>
                  )}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};

export default DataExport;

