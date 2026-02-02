import React, { useState, useEffect } from 'react';
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
  { id: 'daily-prod', title: '일일 생산 데이터', desc: '전일 생산 핵심 지표 및 장치 상태 데이터를 CSV로 추출합니다.', icon: 'fas fa-chart-line', type: 'historical' as ExportType, format: 'csv' as ExportFormat },
  { id: 'weekly-alarm', title: '주간 알람 리포트', desc: '최근 7일간 발생한 모든 알람 및 조치 이력을 PDF로 생성합니다.', icon: 'fas fa-exclamation-triangle', type: 'alarms' as ExportType, format: 'pdf' as ExportFormat },
  { id: 'monthly-status', title: '월간 장치 통계', desc: '한 달간의 장치 가동률 및 주요 상태 변화를 분석 리포트로 내보냅니다.', icon: 'fas fa-microchip', type: 'realtime' as ExportType, format: 'xlsx' as ExportFormat },
  { id: 'ops-log', title: '시스템 운영 로그', desc: '시스템 접속 및 장치 제어 로그를 JSON 형식으로 내보냅니다.', icon: 'fas fa-clipboard-list', type: 'logs' as ExportType, format: 'json' as ExportFormat },
];

const DataExport: React.FC = () => {
  const { confirm } = useConfirmContext();

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
      console.error('장치 로드 실패:', error);
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
      console.error('포인트 로드 실패:', error);
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
        console.error('이력 로드 실패', e);
      }
    }
  };

  const saveHistoryToStorage = (newHistory: ExportTask[]) => {
    localStorage.setItem('export_history', JSON.stringify(newHistory));
  };

  // 내보내기 실행
  const handleStartExport = async () => {
    const isConfirmed = await confirm({
      title: '데이터 내보내기 시작',
      message: `${exportName} 작업을 시작하시겠습니까? 데이터 양에 따라 수 분이 소요될 수 있습니다.`,
      confirmText: '시작',
      cancelText: '취소'
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
      // Step 3: 백엔드 API 호출
      const result = await DataApiService.exportData({
        export_type: exportType,
        point_ids: selectedPointIds,
        device_ids: selectedDeviceId ? [selectedDeviceId] : undefined,
        format: templates.find(t => t.type === exportType)?.format || 'csv',
        start_time: '-24h', // 위저드에서 날짜 선택 기능이 추가될 때까지 기본값 사용
        end_time: 'now'
      });

      if (result.success && result.data) {
        // 성공 시 상태 업데이트
        updateTaskStatus(taskId, 'completed', result.data.total_records * 1024); // 임시 파일 크기 계산
        alert(`내보내기 완료: ${result.data.filename}\n총 ${result.data.total_records}건의 데이터가 추출되었습니다.`);
      } else {
        throw new Error(result.message || '내보내기 실패');
      }
    } catch (error) {
      console.error('Export error:', error);
      updateTaskStatus(taskId, 'failed');
      alert(`내보내기 실패: ${error instanceof Error ? error.message : '알 수 없는 오류'}`);
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
          <h1 className="page-title">데이터 내보내기</h1>
          <p className="page-subtitle">수집된 생산 및 장치 데이터를 다양한 형식으로 추출합니다.</p>
        </div>
        <div className="tab-navigation">
          <button className={`tab-button ${activeTab === 'create' ? 'active' : ''}`} onClick={() => setActiveTab('create')}>
            <i className="fas fa-plus-circle"></i> 신규 작업
          </button>
          <button className={`tab-button ${activeTab === 'history' ? 'active' : ''}`} onClick={() => setActiveTab('history')}>
            <i className="fas fa-history"></i> 내보내기 이력
          </button>
        </div>
      </div>

      {activeTab === 'create' ? (
        <>
          {/* 퀵 템플릿 */}
          {step === 1 && (
            <div className="templates-section">
              <h3 className="section-title"><i className="fas fa-bolt"></i> 빠른 시작 템플릿</h3>
              <div className="templates-grid">
                <div className="template-card" onClick={() => applyTemplate('historical', 'Daily_Production')}>
                  <div className="template-icon"><i className="fas fa-chart-line"></i></div>
                  <div className="template-info">
                    <h4>일일 생산 데이터</h4>
                    <p>전일 생산 핵심 지표 및 장치 상태 데이터를 CSV로 추출합니다.</p>
                  </div>
                </div>
                <div className="template-card" onClick={() => applyTemplate('alarms', 'Alarm_History')}>
                  <div className="template-icon"><i className="fas fa-exclamation-triangle"></i></div>
                  <div className="template-info">
                    <h4>주간 알람 리포트</h4>
                    <p>최근 7일간 발생한 모든 알람 및 조치 이력을 PDF로 생성합니다.</p>
                  </div>
                </div>
                <div className="template-card" onClick={() => applyTemplate('realtime', 'Monthly_Device_Stats')}>
                  <div className="template-icon"><i className="fas fa-microchip"></i></div>
                  <div className="template-info">
                    <h4>월간 장치 통계</h4>
                    <p>한 달간의 장치 가동률 및 주요 상태 변화를 분석 리포트로 내보냅니다.</p>
                  </div>
                </div>
                <div className="template-card" onClick={() => applyTemplate('logs', 'System_Log')}>
                  <div className="template-icon"><i className="fas fa-clipboard-list"></i></div>
                  <div className="template-info">
                    <h4>시스템 운영 로그</h4>
                    <p>시스템 접속 및 장치 제어 로그를 JSON 형식으로 내보냅니다.</p>
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
                <span className="step-label">유형 선택</span>
              </div>
              <div className={`step-item ${step >= 2 ? 'active' : ''} ${step > 2 ? 'completed' : ''}`}>
                <div className="step-number">{step > 2 ? <i className="fas fa-check"></i> : 2}</div>
                <span className="step-label">대상 장치</span>
              </div>
              <div className={`step-item ${step >= 3 ? 'active' : ''} ${step > 3 ? 'completed' : ''}`}>
                <div className="step-number">{step > 3 ? <i className="fas fa-check"></i> : 3}</div>
                <span className="step-label">추출 설정</span>
              </div>
              <div className={`step-item ${step >= 4 ? 'active' : ''} ${step > 4 ? 'completed' : ''}`}>
                <div className="step-number">{step > 4 ? <i className="fas fa-check"></i> : 4}</div>
                <span className="step-label">최종 검토</span>
              </div>
            </div>

            <div className="wizard-content">
              {/* Step 1: Data Type */}
              {step === 1 && (
                <div className="step-container">
                  <h3 className="step-title">추출할 데이터의 유형을 선택하세요.</h3>
                  <div className="type-grid">
                    <div className={`selectable-card ${exportType === 'historical' ? 'selected' : ''}`} onClick={() => setExportType('historical')}>
                      <i className="fas fa-database"></i>
                      <span className="card-title">이력 데이터</span>
                      <span className="card-desc">센서값 등 시계열 데이터</span>
                    </div>
                    <div className={`selectable-card ${exportType === 'realtime' ? 'selected' : ''}`} onClick={() => setExportType('realtime')}>
                      <i className="fas fa-clock"></i>
                      <span className="card-title">실시간 데이터</span>
                      <span className="card-desc">현재 시점의 장치 상태</span>
                    </div>
                    <div className={`selectable-card ${exportType === 'alarms' ? 'selected' : ''}`} onClick={() => setExportType('alarms')}>
                      <i className="fas fa-bell"></i>
                      <span className="card-title">알람 이력</span>
                      <span className="card-desc">발생 및 조치 이력</span>
                    </div>
                    <div className={`selectable-card ${exportType === 'logs' ? 'selected' : ''}`} onClick={() => setExportType('logs')}>
                      <i className="fas fa-list-ul"></i>
                      <span className="card-title">운영 로그</span>
                      <span className="card-desc">시스템 작업 로그</span>
                    </div>
                  </div>

                  <div className="type-details-box">
                    <div className="details-header">
                      <i className="fas fa-info-circle"></i>
                      <span>선택된 유형 상세 정보</span>
                    </div>
                    <div className="details-content">
                      {exportType === 'historical' && (
                        <p><strong>이력 데이터 추출</strong>은 지정된 기간 동안 수집된 모든 센서 및 장치 데이터를 시계열 순으로 추출합니다. 트렌드 분석 및 생산량 집계에 적합한 데이터 세트를 제공합니다.</p>
                      )}
                      {exportType === 'realtime' && (
                        <p><strong>실시간 데이터 스냅샷</strong>은 현재 연결된 장치들의 실시간 통신 상태와 파라미터 값들을 즉시 추출합니다. 현재 설비의 가동 상태를 빠르게 확인하고 기록하는 데 사용됩니다.</p>
                      )}
                      {exportType === 'alarms' && (
                        <p><strong>알람 이력 리포트</strong>는 시스템에서 발생한 중요 알람, 경고 및 사용자의 조치 내역을 추출합니다. 사고 분석 및 유지보수 이력 관리를 위한 상세 보고서를 생성합니다.</p>
                      )}
                      {exportType === 'logs' && (
                        <p><strong>시스템 운영 로그</strong>는 사용자 접속기록, 설정 변경 내역, 제어 명령 실행 이력 등 시스템의 모든 활동 로그를 기록합니다. 보안 감사 및 작업 투명성 확보를 위해 활용됩니다.</p>
                      )}
                      {!exportType && (
                        <p className="placeholder">추출할 데이터 유형을 선택하면 상세 설명이 여기에 표시됩니다.</p>
                      )}
                    </div>
                  </div>
                </div>
              )}

              {/* Step 2: Target Selection */}
              {step === 2 && (
                <div className="step-container">
                  <h3 className="step-title">데이터를 추출할 장치 및 포인트를 선택하세요.</h3>
                  <div className="filter-split">
                    <div className="list-pane">
                      <div className="pane-header">장치 목록 ({devices.length})</div>
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
                      <div className="pane-header">데이터 포인트 ({loading ? '로딩 중...' : points.length})</div>
                      <div className="pane-content">
                        {loading ? (
                          <div className="text-center p-8">데이터를 불러오는 중입니다...</div>
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
                          <div className="text-center p-8 text-neutral-400">장치를 먼저 선택하세요.</div>
                        )}
                      </div>
                    </div>
                  </div>
                </div>
              )}

              {/* Step 3: Config */}
              {step === 3 && (
                <div className="step-container">
                  <h3 className="step-title">추출 조건 및 파일 형식을 설정하세요.</h3>
                  <div className="form-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '2rem' }}>
                    <div className="form-group">
                      <label>작업 이름</label>
                      <input
                        type="text"
                        className="form-input"
                        value={exportName}
                        onChange={(e) => setExportName(e.target.value)}
                        placeholder="예: 2024 하반기 설비 분석"
                      />
                    </div>
                    <div className="form-group">
                      <label>파일 형식</label>
                      <select className="form-select" value={format} onChange={(e) => setFormat(e.target.value as any)}>
                        <option value="csv">CSV (쉼표로 구분)</option>
                        <option value="xlsx">Excel (XLSX)</option>
                        <option value="json">JSON</option>
                        <option value="pdf">PDF 리포트</option>
                      </select>
                    </div>
                    {exportType === 'historical' && (
                      <div className="form-group" style={{ gridColumn: 'span 2' }}>
                        <label>기간 설정</label>
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
                  <h3 className="step-title">설정 내용을 확인하고 내보내기를 시작하세요.</h3>
                  <div className="review-box" style={{ background: '#f8fafc', padding: '1.5rem', borderRadius: '1rem', border: '1px solid #e2e8f0' }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                      <tbody>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>추출 유형</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{exportType.toUpperCase()}</td>
                        </tr>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>대상 장치</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{devices.find(d => d.id === selectedDeviceId)?.name || 'N/A'}</td>
                        </tr>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>포인트 개수</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{selectedPointIds.length}개</td>
                        </tr>
                        <tr style={{ borderBottom: '1px solid #e2e8f0' }}>
                          <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>파일 형식</td>
                          <td style={{ padding: '0.75rem 0', fontWeight: 600 }}>{format.toUpperCase()}</td>
                        </tr>
                        {exportType === 'historical' && (
                          <tr>
                            <td style={{ padding: '0.75rem 0', color: '#64748b', fontWeight: 500 }}>기간</td>
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
              <button className="btn btn-outline" onClick={prevStep} disabled={step === 1}>이전</button>
              {step < 4 ? (
                <button
                  className="btn btn-primary"
                  onClick={nextStep}
                  disabled={(step === 2 && !selectedDeviceId) || (step === 3 && !exportName)}
                >
                  다음
                </button>
              ) : (
                <button className="btn btn-primary" onClick={handleStartExport}>내보내기 시작</button>
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
                    {task.status === 'completed' ? '완료' : task.status === 'running' ? `처리 중 (${task.progress}%)` : '실패'}
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
              <div className="empty-title">내보내기 이력이 없습니다.</div>
              <p>새로운 내보내기 작업을 시작해 보세요.</p>
            </div>
          )}
        </div>
      )
      }
    </div >
  );
};

export default DataExport;

