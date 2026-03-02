import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import '../styles/base.css';
import '../styles/data-export.css';
import { DeviceApiService, Device } from '../api/services/deviceApi';
import { DataApiService, DataPoint } from '../api/services/dataApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';

type ExportType = 'historical' | 'realtime' | 'alarms' | 'logs';
type ExportFormat = 'csv' | 'xlsx' | 'json' | 'pdf';

interface ExportTask {
  id: string;
  name: string;
  type: ExportType;
  format: ExportFormat;
  status: 'running' | 'completed' | 'failed';
  progress: number;
  createdAt: Date;
  fileSize?: string;
}

const templates = [
  { id: 'daily-prod', title: 'Daily Production Data', desc: 'Extract previous day core production metrics and device status data to CSV.', icon: 'fas fa-chart-line', type: 'historical' as ExportType, format: 'csv' as ExportFormat },
  { id: 'weekly-alarm', title: 'Weekly Alarm Report', desc: 'Generate a PDF of all alarms and actions from the last 7 days.', icon: 'fas fa-exclamation-triangle', type: 'alarms' as ExportType, format: 'pdf' as ExportFormat },
  { id: 'monthly-status', title: 'Monthly Device Statistics', desc: 'Export monthly device uptime and key status changes as an analysis report.', icon: 'fas fa-microchip', type: 'realtime' as ExportType, format: 'xlsx' as ExportFormat },
  { id: 'ops-log', title: 'System Operation Log', desc: 'Export system access and device control logs in JSON format.', icon: 'fas fa-clipboard-list', type: 'logs' as ExportType, format: 'json' as ExportFormat },
];

const DataExport: React.FC = () => {
  const { confirm } = useConfirmContext();
  const { t } = useTranslation(['dataExport', 'common']);

  // 상태 관리
  const [step, setStep] = useState(1);
  const [activeTab, setActiveTab] = useState<'create' | 'history'>('create');

  // 내보내기 데이터 상태
  const [exportType, setExportType] = useState<ExportType>('historical');
  const [devices, setDevices] = useState<Device[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState<number | null>(null);
  const [points, setPoints] = useState<DataPoint[]>([]);
  const [selectedPointIds, setSelectedPointIds] = useState<number[]>([]);

  const [exportName, setExportName] = useState('');
  const [format, setFormat] = useState<ExportFormat>('csv');
  const [dateRange, setDateRange] = useState({
    start: new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString().slice(0, 16),
    end: new Date().toISOString().slice(0, 16)
  });

  const [loading, setLoading] = useState(false);
  const [history, setHistory] = useState<ExportTask[]>([]);

  // 초기 로드
  useEffect(() => {
    fetchDevices();
    loadHistoryFromStorage();
  }, []);

  // 장치 목록 조회
  const fetchDevices = async () => {
    try {
      const response = await DeviceApiService.getDevices({ limit: 100 });
      if (response.success && response.data?.items) {
        setDevices(response.data.items);
      }
    } catch (error) {
      console.error('장치 로드 Failed:', error);
    }
  };

  // 장치 선택 시 포인트 로드
  useEffect(() => {
    if (selectedDeviceId) {
      fetchPoints(selectedDeviceId);
    } else {
      setPoints([]);
    }
  }, [selectedDeviceId]);

  const fetchPoints = async (deviceId: number) => {
    try {
      setLoading(true);
      const res = await DataApiService.getDataPoints({ device_id: deviceId });
      setPoints(res.points);
    } catch (error) {
      console.error('포인트 로드 Failed:', error);
    } finally {
      setLoading(false);
    }
  };

  // 로컬 스토리지에서 이력 로드 (DB 연동 전까지)
  const loadHistoryFromStorage = () => {
    const saved = localStorage.getItem('export_history');
    if (saved) {
      try {
        const parsed = JSON.parse(saved);
        setHistory(parsed.map((h: any) => ({ ...h, createdAt: new Date(h.createdAt) })));
      } catch (e) {
        console.error('이력 로드 Failed', e);
      }
    }
  };

  const saveHistoryToStorage = (newHistory: ExportTask[]) => {
    localStorage.setItem('export_history', JSON.stringify(newHistory));
  };

  // 내보내기 실행
  const handleStartExport = async () => {
    const isConfirmed = await confirm({
      title: 'Start Data Export',
      message: `Start ${exportName} job? This may take several minutes depending on data size.`,
      confirmText: 'Start',
      cancelText: 'Cancel'
    });

    if (!isConfirmed) return;

    const newTask: ExportTask = {
      id: `EXP-${Date.now()}`,
      name: exportName || `${exportType} Data Export`,
      type: exportType,
      format: format,
      status: 'running',
      progress: 0,
      createdAt: new Date()
    };

    const updatedHistory = [newTask, ...history];
    setHistory(updatedHistory);
    saveHistoryToStorage(updatedHistory);
    setActiveTab('history');

    // 실제 API 호출
    const taskId = newTask.id;
    try {
      const exportTypeMap: Record<ExportType, 'current' | 'historical' | 'configuration'> = {
        'historical': 'historical', 'realtime': 'current', 'alarms': 'historical', 'logs': 'historical'
      };
      const result = await DataApiService.exportData({
        export_type: exportTypeMap[exportType],
        point_ids: selectedPointIds,
        device_ids: selectedDeviceId ? [selectedDeviceId] : undefined,
        format: (templates.find(t => t.type === exportType)?.format === 'pdf' ? 'csv' :
          templates.find(t => t.type === exportType)?.format === 'xlsx' ? 'csv' :
            templates.find(t => t.type === exportType)?.format || 'csv') as 'csv' | 'json' | 'xml',
        start_time: '-24h',
        end_time: 'now'
      });

      if (result.success && result.data) {
        // Success 시 상태 업데이트
        updateTaskStatus(taskId, 'completed', result.data.total_records * 1024); // 임시 파일 크기 계산
        alert(`Export complete: ${result.data.filename}\nTotal ${result.data.total_records} record(s) extracted.`);
      } else {
        throw new Error(result.message || 'Export failed');
      }
    } catch (error) {
      console.error('Export error:', error);
      updateTaskStatus(taskId, 'failed');
      alert(`Export failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }

    // 초기화
    resetWizard();
  };

  const updateTaskProgress = (id: string, progress: number) => {
    setHistory(prev => {
      const updated = prev.map(t => t.id === id ? { ...t, progress } : t);
      saveHistoryToStorage(updated);
      return updated;
    });
  };

  const updateTaskStatus = (id: string, status: 'completed' | 'failed', size?: number) => {
    setHistory(prev => {
      const updated = prev.map(t => t.id === id ? {
        ...t,
        status,
        progress: 100,
        fileSize: size ? (size / 1024 / 1024).toFixed(2) + ' MB' : undefined
      } : t);
      saveHistoryToStorage(updated);
      return updated;
    });
  };

  const resetWizard = () => {
    setStep(1);
    setSelectedDeviceId(null);
    setSelectedPointIds([]);
    setExportName('');
  };

  const nextStep = () => setStep(prev => Math.min(prev + 1, 4));
  const prevStep = () => setStep(prev => Math.max(prev - 1, 1));

  // 퀵 템플릿 적용
  const applyTemplate = (type: ExportType, name: string) => {
    setExportType(type);
    setExportName(`${name}_${new Date().toISOString().slice(0, 10)}`);
    setStep(2);
  };

  return (
    <div className="data-export-container">
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">{t('pageTitle')}</h1>
          <p className="page-subtitle">{t('pageDesc')}</p>
        </div>
        <div className="tab-navigation">
          <button className={`tab-button ${activeTab === 'create' ? 'active' : ''}`} onClick={() => setActiveTab('create')}>
            <i className="fas fa-plus-circle"></i> {t('tab.new')}
          </button>
          <button className={`tab-button ${activeTab === 'history' ? 'active' : ''}`} onClick={() => setActiveTab('history')}>
            <i className="fas fa-history"></i> {t('tab.history')}
          </button>
        </div>
      </div>

      {activeTab === 'create' ? (
        <>
          {/* 퀵 템플릿 */}
          {step === 1 && (
            <div className="templates-section">
              <h3 className="section-title"><i className="fas fa-bolt"></i> {t('template.quickStart')}</h3>
              <div className="templates-grid">
                <div className="template-card" onClick={() => applyTemplate('historical', 'Daily_Production')}>
                  <div className="template-icon"><i className="fas fa-chart-line"></i></div>
                  <div className="template-info">
                    <h4>{t('template.dailyProd')}</h4>
                    <p>{t('template.dailyProdDesc')}</p>
                  </div>
                </div>
                <div className="template-card" onClick={() => applyTemplate('alarms', 'Alarm_History')}>
                  <div className="template-icon"><i className="fas fa-exclamation-triangle"></i></div>
                  <div className="template-info">
                    <h4>{t('template.weeklyAlarm')}</h4>
                    <p>{t('template.weeklyAlarmDesc')}</p>
                  </div>
                </div>
                <div className="template-card" onClick={() => applyTemplate('realtime', 'Monthly_Device_Stats')}>
                  <div className="template-icon"><i className="fas fa-microchip"></i></div>
                  <div className="template-info">
                    <h4>{t('template.monthlyStats')}</h4>
                    <p>{t('template.monthlyStatsDesc')}</p>
                  </div>
                </div>
                <div className="template-card" onClick={() => applyTemplate('logs', 'System_Log')}>
                  <div className="template-icon"><i className="fas fa-clipboard-list"></i></div>
                  <div className="template-info">
                    <h4>{t('template.sysLog')}</h4>
                    <p>{t('template.sysLogDesc')}</p>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* 메인 위저드 */}
          <div className="export-wizard">
            <div className="wizard-stepper">
              <div className={`step-item ${step >= 1 ? 'active' : ''} ${step > 1 ? 'completed' : ''}`}>
                <div className="step-number">{step > 1 ? <i className="fas fa-check"></i> : 1}</div>
                <span className="step-label">{t('wizard.step1')}</span>
              </div>
              <div className={`step-item ${step >= 2 ? 'active' : ''} ${step > 2 ? 'completed' : ''}`}>
                <div className="step-number">{step > 2 ? <i className="fas fa-check"></i> : 2}</div>
                <span className="step-label">{t('wizard.step2')}</span>
              </div>
              <div className={`step-item ${step >= 3 ? 'active' : ''} ${step > 3 ? 'completed' : ''}`}>
                <div className="step-number">{step > 3 ? <i className="fas fa-check"></i> : 3}</div>
                <span className="step-label">{t('wizard.step3')}</span>
              </div>
              <div className={`step-item ${step >= 4 ? 'active' : ''} ${step > 4 ? 'completed' : ''}`}>
                <div className="step-number">{step > 4 ? <i className="fas fa-check"></i> : 4}</div>
                <span className="step-label">{t('wizard.step4')}</span>
              </div>
            </div>

            <div className="wizard-content">
              {/* Step 1: Data Type */}
              {step === 1 && (
                <div className="step-container">
                  <h3 className="step-title">{t('wizard.selectType')}</h3>
                  <div className="type-grid">
                    <div className={`selectable-card ${exportType === 'historical' ? 'selected' : ''}`} onClick={() => setExportType('historical')}>
                      <i className="fas fa-database"></i>
                      <span className="card-title">{t('type.historicalTitle')}</span>
                      <span className="card-desc">{t('type.historicalDesc')}</span>
                    </div>
                    <div className={`selectable-card ${exportType === 'realtime' ? 'selected' : ''}`} onClick={() => setExportType('realtime')}>
                      <i className="fas fa-clock"></i>
                      <span className="card-title">{t('type.realtimeTitle')}</span>
                      <span className="card-desc">{t('type.realtimeDesc')}</span>
                    </div>
                    <div className={`selectable-card ${exportType === 'alarms' ? 'selected' : ''}`} onClick={() => setExportType('alarms')}>
                      <i className="fas fa-bell"></i>
                      <span className="card-title">{t('type.alarmsTitle')}</span>
                      <span className="card-desc">{t('type.alarmsDesc')}</span>
                    </div>
                    <div className={`selectable-card ${exportType === 'logs' ? 'selected' : ''}`} onClick={() => setExportType('logs')}>
                      <i className="fas fa-list-ul"></i>
                      <span className="card-title">{t('type.logsTitle')}</span>
                      <span className="card-desc">{t('type.logsDesc')}</span>
                    </div>
                  </div>

                  <div className="type-details-box">
                    <div className="details-header">
                      <i className="fas fa-info-circle"></i>
                      <span>{t('wizard.selectedTypeDetails')}</span>
                    </div>
                    <div className="details-content">
                      {exportType === 'historical' && (<p>{t('type.historicalDetail')}</p>)}
                      {exportType === 'realtime' && (<p>{t('type.realtimeDetail')}</p>)}
                      {exportType === 'alarms' && (<p>{t('type.alarmsDetail')}</p>)}
                      {exportType === 'logs' && (<p>{t('type.logsDetail')}</p>)}
                      {!exportType && (
                        <p className="placeholder">Select a data type to see a detailed description here.</p>
                      )}
                    </div>
                  </div>
                </div>
              )}

              {/* Step 2: Target Selection */}
              {step === 2 && (
                <div className="step-container">
                  <h3 className="step-title">{t('wizard.selectDevice')}</h3>
                  <div className="filter-split">
                    <div className="list-pane">
                      <div className="pane-header">{t('wizard.deviceList')} ({devices.length})</div>
                      <div className="pane-content">
                        {devices.map(d => (
                          <div
                            key={d.id}
                            className={`list-item ${selectedDeviceId === d.id ? 'active' : ''}`}
                            onClick={() => setSelectedDeviceId(d.id)}
                          >
                            <i className="fas fa-server"></i> {d.name}
                          </div>
                        ))}
                      </div>
                    </div>
                    <div className="list-pane">
                      <div className="pane-header">{t('wizard.dataPoints')} ({loading ? t('wizard.loading') : points.length})</div>
                      <div className="pane-content">
                        {loading ? (
                          <div className="text-center p-8">{t('wizard.loading')}</div>
                        ) : points.length > 0 ? (
                          points.map(p => (
                            <label key={p.id} className="list-item">
                              <input
                                type="checkbox"
                                checked={selectedPointIds.includes(p.id)}
                                onChange={(e) => {
                                  if (e.target.checked) setSelectedPointIds([...selectedPointIds, p.id]);
                                  else setSelectedPointIds(selectedPointIds.filter(id => id !== p.id));
                                }}
                              />
                              {p.name} <span className="text-xs text-neutral-400">({p.address_string})</span>
                            </label>
                          ))
                        ) : (
                          <div className="text-center p-8 text-neutral-400">{t('wizard.selectDeviceFirst')}</div>
                        )}
                      </div>
                    </div>
                  </div>
                </div>
              )}

              {/* Step 3: Config */}
              {step === 3 && (
                <div className="step-container">
                  <h3 className="step-title">{t('wizard.extractionConfig')}</h3>
                  <div className="form-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '2rem' }}>
                    <div className="form-group">
                      <label>{t('wizard.jobName')}</label>
                      <input
                        type="text"
                        className="form-input"
                        value={exportName}
                        onChange={(e) => setExportName(e.target.value)}
                        placeholder="e.g. 2024 H2 Equipment Analysis"
                      />
                    </div>
                    <div className="form-group">
                      <label>{t('wizard.fileFormat')}</label>
                      <select className="form-select" value={format} onChange={(e) => setFormat(e.target.value as any)}>
                        <option value="csv">CSV</option>
                        <option value="xlsx">Excel (XLSX)</option>
                        <option value="json">JSON</option>
                        <option value="pdf">PDF</option>
                      </select>
                    </div>
                    {exportType === 'historical' && (
                      <div className="form-group" style={{ gridColumn: 'span 2' }}>
                        <label>{t('wizard.dateRange')}</label>
                        <div className="date-range-inputs">
                          <input type="datetime-local" className="form-input" value={dateRange.start} onChange={e => setDateRange({ ...dateRange, start: e.target.value })} />
                          <span>~</span>
                          <input type="datetime-local" className="form-input" value={dateRange.end} onChange={e => setDateRange({ ...dateRange, end: e.target.value })} />
                        </div>
                      </div>
                    )}
                  </div>
                </div>
              )}

              {/* Step 4: Review */}
              {step === 4 && (
                <div className="step-container">
                  <h3 className="step-title">{t('wizard.finalReview')}</h3>
                  <div className="review-box" style={{ background: '#f8fafc', padding: '1.5rem', borderRadius: '1rem', border: '1px solid #e2e8f0' }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                      <tbody>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>{t('review.extractionType')}</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{exportType.toUpperCase()}</td>
                        </tr>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>{t('review.targetDevice')}</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{devices.find(d => d.id === selectedDeviceId)?.name || 'N/A'}</td>
                        </tr>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>{t('review.pointCount')}</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{selectedPointIds.length}</td>
                        </tr>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>{t('review.fileFormat')}</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{format.toUpperCase()}</td>
                        </tr>
                        {exportType === 'historical' && (
                          <tr>
                            <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>{t('review.period')}</td>
                            <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{dateRange.start} ~ {dateRange.end}</td>
                          </tr>
                        )}
                      </tbody>
                    </table>
                  </div>
                </div>
              )}
            </div>

            <div className="wizard-footer">
              <button className="btn btn-outline" onClick={prevStep} disabled={step === 1}>{t('wizard.prev')}</button>
              {step < 4 ? (
                <button
                  className="btn btn-primary"
                  onClick={nextStep}
                  disabled={(step === 2 && !selectedDeviceId) || (step === 3 && !exportName)}
                >
                  {t('wizard.next')}
                </button>
              ) : (
                <button className="btn btn-primary" onClick={handleStartExport}>{t('wizard.startExport')}</button>
              )}
            </div>
          </div>
        </>
      ) : (
        /* 이력 탭 */
        <div className="history-section">
          {history.length > 0 ? (
            history.map(task => (
              <div key={task.id} className="export-task-card">
                <div className={`file-icon ${task.format}`}>
                  <i className={`fas fa-file-${task.format === 'pdf' ? 'pdf' : task.format === 'xlsx' ? 'excel' : 'alt'}`}></i>
                </div>
                <div className="task-main">
                  <h4 className="task-title">{task.name}</h4>
                  <div className="task-info-chips">
                    <span className="chip">{task.type}</span>
                    <span className="chip">{task.format}</span>
                    {task.fileSize && <span className="chip">{task.fileSize}</span>}
                  </div>
                </div>
                <div className="export-status-indicator">
                  <span className={`badge-status ${task.status}`}>
                    {task.status === 'completed' ? t('status.completed') : task.status === 'running' ? `${t('status.running')} (${task.progress}%)` : t('status.failed')}
                  </span>
                  <span className="time-stamp">{task.createdAt.toLocaleString()}</span>
                </div>
                <div className="task-actions">
                  {task.status === 'completed' && <button className="btn btn-sm btn-outline"><i className="fas fa-download"></i></button>}
                  <button className="btn btn-sm btn-ghost text-error"><i className="fas fa-trash"></i></button>
                </div>
              </div>
            ))
          ) : (
            <div className="empty-state">
              <i className="fas fa-folder-open empty-icon"></i>
              <div className="empty-title">{t('noHistory')}</div>
              <p>{t('noHistoryDesc')}</p>
            </div>
          )}
        </div>
      )
      }
    </div >
  );
};

export default DataExport;

