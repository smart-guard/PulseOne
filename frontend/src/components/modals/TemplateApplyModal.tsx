import React, { useState, useEffect, useMemo } from 'react';
import alarmTemplatesApi, {
  AlarmTemplate,
  DataPoint,
  ApplyTemplateRequest
} from '../../api/services/alarmTemplatesApi';

interface TemplateApplyModalProps {
  template: AlarmTemplate;
  isOpen: boolean;
  onClose: () => void;
  onSuccess: () => void;
}

const TemplateApplyModal: React.FC<TemplateApplyModalProps> = ({
  template,
  isOpen,
  onClose,
  onSuccess
}) => {
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [loading, setLoading] = useState(false);
  const [applying, setApplying] = useState(false);
  const [selectedIds, setSelectedIds] = useState<number[]>([]);
  const [searchTerm, setSearchTerm] = useState('');
  const [ruleGroupName, setRuleGroupName] = useState('');
  const [selectedDevice, setSelectedDevice] = useState<string>('all');
  const [simulationResult, setSimulationResult] = useState<{ estimated_alarms: number, confidence: string, note: string } | null>(null);
  const [simulating, setSimulating] = useState(false);
  const [appliedDataPointIds, setAppliedDataPointIds] = useState<Set<number>>(new Set());

  // Filtering data points by compatibility, device and search term
  const compatibleDataPoints = useMemo(() => {
    return dataPoints.filter(dp => {
      return template.applicable_data_types.includes('*') ||
        template.applicable_data_types.includes(dp.data_type);
    });
  }, [dataPoints, template.applicable_data_types]);

  const devices = useMemo(() => {
    const devSet = new Set<string>();
    compatibleDataPoints.forEach(dp => devSet.add(dp.device_name));
    return Array.from(devSet).sort();
  }, [compatibleDataPoints]);

  const filteredDataPoints = useMemo(() => {
    return compatibleDataPoints.filter(dp => {
      // Device filter
      if (selectedDevice !== 'all' && dp.device_name !== selectedDevice) return false;

      // Search term check
      if (!searchTerm) return true;
      const term = searchTerm.toLowerCase();
      return dp.name.toLowerCase().includes(term) ||
        dp.device_name.toLowerCase().includes(term) ||
        dp.address?.toLowerCase().includes(term);
    });
  }, [compatibleDataPoints, selectedDevice, searchTerm]);

  useEffect(() => {
    if (isOpen) {
      loadDataPoints();
    }
  }, [isOpen, template]);

  const loadDataPoints = async () => {
    try {
      setLoading(true);
      const [points, appliedRules] = await Promise.all([
        alarmTemplatesApi.getDataPoints(),
        alarmTemplatesApi.getAppliedRules(template.id)
      ]);
      setDataPoints(points);

      const appliedIds = new Set<number>();
      appliedRules.forEach(rule => {
        if (rule.target_id) appliedIds.add(rule.target_id);
      });
      setAppliedDataPointIds(appliedIds);
    } catch (error) {
      console.error('Failed to load data points', error);
    } finally {
      setLoading(false);
    }
  };

  const handleToggleSelect = (id: number) => {
    setSelectedIds(prev =>
      prev.includes(id) ? prev.filter(i => i !== id) : [...prev, id]
    );
  };

  const handleSelectAll = (checked: boolean) => {
    if (checked) {
      const currentIds = filteredDataPoints
        .filter(dp => !appliedDataPointIds.has(dp.id))
        .map(dp => dp.id);
      setSelectedIds(prev => Array.from(new Set([...prev, ...currentIds])));
    } else {
      const currentIds = filteredDataPoints.map(dp => dp.id);
      setSelectedIds(prev => prev.filter(id => !currentIds.includes(id)));
    }
  };

  const handleApply = async () => {
    if (selectedIds.length === 0) return;

    try {
      setApplying(true);
      const request: ApplyTemplateRequest = {
        data_point_ids: selectedIds,
        rule_group_name: ruleGroupName || undefined
      };

      const response = await alarmTemplatesApi.applyTemplate(template.id, request);
      if (response && (response.success || response)) {
        onSuccess();
        onClose();
      }
    } catch (error) {
      alert('템플릿 적용 중 오류가 발생했습니다.');
    } finally {
      setApplying(false);
    }

  };

  const handleSimulate = async () => {
    if (selectedIds.length === 0) return;

    try {
      setSimulating(true);
      const result = await alarmTemplatesApi.simulateTemplate(template.id, {
        data_point_ids: selectedIds,
        duration_days: 7
      });
      setSimulationResult(result);
    } catch (error) {
      console.error('Simulation failed', error);
      alert('시뮬레이션 중 오류가 발생했습니다.');
    } finally {
      setSimulating(false);
    }
  };

  const getSeverityBadgeClass = (s: string) => {
    const severity = s?.toLowerCase();
    if (severity === 'critical') return 'critical';
    if (severity === 'high') return 'high';
    if (severity === 'medium') return 'medium';
    return 'low';
  };

  if (!isOpen) return null;

  return (
    <div className="tam-overlay" onClick={(e) => e.target === e.currentTarget && onClose()}>
      <div className="tam-container">
        {/* Header */}
        <div className="tam-header">
          <div className="tam-header-left">
            <div className="tam-icon-circle">
              <i className="fas fa-magic"></i>
            </div>
            <div className="tam-title-group">
              <h3 className="tam-title">템플릿 적용: <span className="highlight">{template.name}</span></h3>
              <p className="tam-subtitle">이 템플릿을 대량으로 적용할 대상을 선택하고 규칙을 생성합니다.</p>
            </div>
          </div>
          <button className="tam-close-btn" onClick={onClose}>&times;</button>
        </div>

        {/* Content Body */}
        <div className="tam-body">
          {/* Step 1: Device Sidebar */}
          <div className="tam-sidebar">
            <div className="tam-sidebar-header">
              <i className="fas fa-server"></i> 장치 선택
            </div>
            <div className="tam-device-list">
              <button
                className={`tam-device-item ${selectedDevice === 'all' ? 'active' : ''}`}
                onClick={() => setSelectedDevice('all')}
              >
                <span>전체 장치</span>
                <span className="count">{compatibleDataPoints.length}</span>
              </button>
              {devices.map(dev => (
                <button
                  key={dev}
                  className={`tam-device-item ${selectedDevice === dev ? 'active' : ''}`}
                  onClick={() => setSelectedDevice(dev)}
                >
                  <span className="truncate">{dev}</span>
                  <span className="count">
                    {compatibleDataPoints.filter(dp => dp.device_name === dev).length}
                  </span>
                </button>
              ))}
            </div>
          </div>

          {/* Step 2: Data Point Main List */}
          <div className="tam-main">
            <div className="tam-search-bar">
              <i className="fas fa-search"></i>
              <input
                type="text"
                placeholder="데이터포인트명 또는 주소로 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
              />
              {filteredDataPoints.length > 0 && (
                <div className="tam-selection-info">
                  {filteredDataPoints.length}개 항목 발견
                </div>
              )}
            </div>

            <div className="tam-table-container">
              <table className="tam-table">
                <thead>
                  <tr>
                    <th style={{ width: '48px' }}>
                      <input
                        type="checkbox"
                        checked={filteredDataPoints.length > 0 && filteredDataPoints.every(dp => selectedIds.includes(dp.id))}
                        onChange={(e) => handleSelectAll(e.target.checked)}
                      />
                    </th>
                    <th>데이터포인트 상세</th>
                    <th>장비</th>
                    <th>타입</th>
                  </tr>
                </thead>
                <tbody>
                  {loading ? (
                    <tr><td colSpan={4} className="tam-empty">데이터를 불러오는 중...</td></tr>
                  ) : filteredDataPoints.length === 0 ? (
                    <tr><td colSpan={4} className="tam-empty">조건에 맞는 데이터포인트가 없습니다.</td></tr>
                  ) : (
                    filteredDataPoints.map(dp => (
                      <tr
                        key={dp.id}
                        className={`${selectedIds.includes(dp.id) ? 'selected' : ''} ${appliedDataPointIds.has(dp.id) ? 'disabled-row' : ''}`}
                        onClick={() => !appliedDataPointIds.has(dp.id) && handleToggleSelect(dp.id)}
                        style={appliedDataPointIds.has(dp.id) ? { opacity: 0.6, cursor: 'not-allowed', background: '#f8fafc' } : {}}
                      >
                        <td onClick={(e) => e.stopPropagation()}>
                          <input
                            type="checkbox"
                            checked={selectedIds.includes(dp.id) || appliedDataPointIds.has(dp.id)}
                            disabled={appliedDataPointIds.has(dp.id)}
                            onChange={() => handleToggleSelect(dp.id)}
                          />
                        </td>
                        <td>
                          <div className="dp-name">
                            {dp.name}
                            {appliedDataPointIds.has(dp.id) && (
                              <span className="tam-badge low" style={{ marginLeft: '8px', fontSize: '10px' }}>
                                이미 적용됨
                              </span>
                            )}
                          </div>
                          <div className="dp-addr">{dp.address || 'No Address'} | {dp.site_name}</div>
                        </td>
                        <td><span className="tam-device-badge">{dp.device_name}</span></td>
                        <td><span className="tam-type-badge">{dp.data_type}</span></td>
                      </tr>
                    ))
                  )}
                </tbody>
              </table>
            </div>
          </div>

          {/* Step 3: Summary & Action Panel */}
          <div className="tam-panel">
            <div className="tam-panel-content">
              <div className="tam-panel-section">
                <div className="tam-panel-label">선택 요약</div>
                <div className="tam-summary-card">
                  <div className="tam-summary-row">
                    <span>선택된 포인트</span>
                    <span className="tam-count-big">{selectedIds.length}</span>
                  </div>
                  <div className="tam-summary-divider" />
                  <div className="tam-summary-row mini">
                    <span>템플릿 심각도</span>
                    <span className={`tam-badge ${getSeverityBadgeClass(template.severity)}`}>
                      {template.severity.toUpperCase()}
                    </span>
                  </div>
                  <div className="tam-summary-row mini">
                    <span>로직 유형</span>
                    <span className="val">{template.condition_type.toUpperCase()}</span>
                  </div>
                </div>
              </div>

              <div style={{ paddingBottom: '24px', marginBottom: '24px', borderBottom: '1px solid #f1f5f9' }}>
                {!simulationResult ? (
                  <button
                    onClick={handleSimulate}
                    disabled={selectedIds.length === 0 || simulating}
                    style={{
                      width: '100%',
                      padding: '12px',
                      background: '#eff6ff',
                      border: '1px dashed #bfdbfe',
                      borderRadius: '12px',
                      color: '#2563eb',
                      fontSize: '13px',
                      fontWeight: 600,
                      cursor: selectedIds.length > 0 ? 'pointer' : 'not-allowed',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '8px',
                      transition: 'all 0.2s'
                    }}
                  >
                    {simulating ? (
                      <><i className="fas fa-spinner fa-spin"></i> 계산 중...</>
                    ) : (
                      <><i className="fas fa-calculator"></i> 적용 시뮬레이션 (7일 예측)</>
                    )}
                  </button>
                ) : (
                  <div style={{
                    background: '#f0fdf4',
                    border: '1px solid #bbf7d0',
                    borderRadius: '16px',
                    padding: '16px',
                    animation: 'fadeIn 0.3s'
                  }}>
                    <style>{`@keyframes fadeIn { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }`}</style>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
                      <span style={{ fontSize: '11px', fontWeight: 700, color: '#166534', textTransform: 'uppercase' }}>예상 알람 발생 (주간)</span>
                      <button
                        onClick={() => setSimulationResult(null)}
                        style={{ background: 'none', border: 'none', color: '#86efac', cursor: 'pointer', padding: 0 }}
                      >
                        <i className="fas fa-redo"></i>
                      </button>
                    </div>
                    <div style={{ fontSize: '28px', fontWeight: 800, color: '#15803d', marginBottom: '8px' }}>
                      약 {simulationResult.estimated_alarms !== undefined ? simulationResult.estimated_alarms : 0}회
                    </div>
                    {simulationResult.note && (
                      <div style={{ fontSize: '11px', color: '#16a34a', lineHeight: '1.4', background: '#dcfce7', padding: '8px', borderRadius: '8px' }}>
                        <i className="fas fa-info-circle" style={{ marginRight: '4px' }}></i>
                        {simulationResult.note}
                      </div>
                    )}
                  </div>
                )}
              </div>

              <div className="tam-panel-section">
                <div className="tam-panel-label">추가 설정</div>
                <div className="tam-input-group">
                  <label>규칙 그룹명 (선택)</label>
                  <input
                    type="text"
                    placeholder="예: 2024 하반기 정기 점검"
                    value={ruleGroupName}
                    onChange={(e) => setRuleGroupName(e.target.value)}
                  />
                  <div className="help">동일한 그룹명으로 규칙들을 묶어 관리할 수 있습니다.</div>
                </div>
              </div>
            </div>

            <div className="tam-action-area">
              <button
                className="tam-btn-apply"
                disabled={selectedIds.length === 0 || applying}
                onClick={handleApply}
              >
                {applying ? (
                  <><i className="fas fa-circle-notch fa-spin"></i> 생성 중...</>
                ) : (
                  <><i className="fas fa-check-circle"></i> {selectedIds.length}개 규칙 일괄 생성</>
                )}
              </button>
              <button className="tam-btn-cancel" onClick={onClose} disabled={applying}>
                취소
              </button>
            </div>
          </div>


          <style>{`
           .tam-overlay {
              position: fixed !important;
              top: 0 !important;
              left: 0 !important;
              right: 0 !important;
              bottom: 0 !important;
              background: rgba(15, 23, 42, 0.75) !important;
              backdrop-filter: blur(12px) !important;
              display: flex !important;
              align-items: center !important;
              justify-content: center !important;
              z-index: 9000 !important; /* Higher than Detail Modal if needed, but matched detail modal standard */
              padding: 24px !important;
           }

           .tam-container {
              background: white !important;
              width: 1200px !important; /* Slightly wider for 3 columns */
              max-width: 98vw !important;
              height: 750px !important;
              max-height: 90vh !important;
              border-radius: 20px !important;
              box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5) !important;
              display: flex !important;
              flex-direction: column !important;
              overflow: hidden !important;
              animation: tam-slide-up 0.3s ease-out !important;
           }

           @keyframes tam-slide-up {
              from { opacity: 0; transform: translateY(20px); }
              to { opacity: 1; transform: translateY(0); }
           }

           .tam-header {
              padding: 24px 32px !important;
              background: white !important;
              border-bottom: 1px solid #f1f5f9 !important;
              display: flex !important;
              justify-content: space-between !important;
              align-items: center !important;
              flex-shrink: 0 !important;
           }

           .tam-header-left {
              display: flex !important;
              align-items: center !important;
              gap: 20px !important;
           }

           .tam-icon-circle {
              width: 52px !important;
              height: 52px !important;
              background: linear-gradient(135deg, #3b82f6, #2563eb) !important;
              color: white !important;
              border-radius: 14px !important;
              display: flex !important;
              align-items: center !important;
              justify-content: center !important;
              font-size: 22px !important;
              box-shadow: 0 4px 6px -1px rgba(37, 99, 235, 0.2) !important;
           }

           .tam-title {
              margin: 0 !important;
              font-size: 20px !important;
              font-weight: 800 !important;
              color: #0f172a !important;
           }
           .tam-title .highlight { color: #2563eb !important; }

           .tam-subtitle {
              margin: 4px 0 0 0 !important;
              font-size: 14px !important;
              color: #64748b !important;
           }

           .tam-close-btn {
              background: transparent !important;
              border: none !important;
              font-size: 32px !important;
              color: #94a3b8 !important;
              cursor: pointer !important;
              padding: 4px !important;
           }

           .tam-body {
              display: flex !important;
              flex: 1 !important;
              overflow: hidden !important;
              background: #f8fafc !important;
           }

           /* Column 1: Device Sidebar */
           .tam-sidebar {
              width: 240px !important;
              min-width: 240px !important;
              background: white !important;
              border-right: 1px solid #e2e8f0 !important;
              display: flex !important;
              flex-direction: column !important;
           }

           .tam-sidebar-header {
              padding: 20px 24px !important;
              font-size: 13px !important;
              font-weight: 700 !important;
              color: #475569 !important;
              background: #f8fafc !important;
              display: flex !important;
              align-items: center !important;
              gap: 10px !important;
              border-bottom: 1px solid #f1f5f9 !important;
           }

           .tam-device-list {
              flex: 1 !important;
              overflow-y: auto !important;
              padding: 12px !important;
           }

           .tam-device-item {
              width: 100% !important;
              padding: 12px 16px !important;
              border: none !important;
              background: transparent !important;
              border-radius: 10px !important;
              display: flex !important;
              justify-content: space-between !important;
              align-items: center !important;
              cursor: pointer !important;
              margin-bottom: 4px !important;
              transition: all 0.2s !important;
              text-align: left !important;
              color: #475569 !important;
              font-size: 14px !important;
              font-weight: 500 !important;
           }

           .tam-device-item:hover { background: #f1f5f9 !important; }
           .tam-device-item.active {
              background: #eff6ff !important;
              color: #2563eb !important;
              font-weight: 700 !important;
           }

           .tam-device-item .count {
              font-size: 11px !important;
              padding: 2px 8px !important;
              background: #f1f5f9 !important;
              border-radius: 10px !important;
              color: #94a3b8 !important;
           }
           .tam-device-item.active .count {
              background: #2563eb !important;
              color: white !important;
           }

           /* Column 2: Main List */
           .tam-main {
              flex: 1 !important;
              display: flex !important;
              flex-direction: column !important;
              overflow: hidden !important;
              padding: 24px !important;
           }

           .tam-search-bar {
              background: white !important;
              border: 1px solid #e2e8f0 !important;
              border-radius: 12px !important;
              padding: 0 16px !important;
              height: 52px !important;
              display: flex !important;
              align-items: center !important;
              gap: 12px !important;
              margin-bottom: 20px !important;
              box-shadow: 0 1px 2px rgba(0, 0, 0, 0.05) !important;
           }

           .tam-search-bar i { color: #94a3b8 !important; font-size: 16px !important; }
           .tam-search-bar input {
              flex: 1 !important;
              border: none !important;
              outline: none !important;
              font-size: 15px !important;
              background: transparent !important;
           }

           .tam-selection-info {
              font-size: 12px !important;
              font-weight: 600 !important;
              color: #94a3b8 !important;
              background: #f1f5f9 !important;
              padding: 4px 10px !important;
              border-radius: 8px !important;
           }

           .tam-table-container {
              flex: 1 !important;
              background: white !important;
              border: 1px solid #e2e8f0 !important;
              border-radius: 16px !important;
              overflow-y: auto !important;
              box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1) !important;
           }

           .tam-table {
              width: 100% !important;
              border-collapse: collapse !important;
           }

           .tam-table th {
              position: sticky !important;
              top: 0 !important;
              background: #f8fafc !important;
              z-index: 10 !important;
              text-align: left !important;
              padding: 16px !important;
              font-size: 12px !important;
              font-weight: 700 !important;
              color: #475569 !important;
              border-bottom: 1px solid #e2e8f0 !important;
           }

           .tam-table td {
              padding: 14px 16px !important;
              border-bottom: 1px solid #f1f5f9 !important;
              font-size: 14px !important;
              color: #334155 !important;
              transition: background 0.1s !important;
           }

           .tam-table tr:hover td { background: #fcfcfd !important; }
           .tam-table tr.selected td { background: #f0f7ff !important; }

           .dp-name { font-weight: 700 !important; color: #1e293b !important; margin-bottom: 2px !important; }
           .dp-addr { font-size: 12px !important; color: #64748b !important; }

           .tam-device-badge {
              background: #f1f5f9 !important;
              color: #475569 !important;
              padding: 4px 10px !important;
              border-radius: 6px !important;
              font-size: 12px !important;
              font-weight: 600 !important;
           }

           .tam-type-badge {
              font-family: monospace !important;
              color: #2563eb !important;
              font-weight: 700 !important;
              font-size: 11px !important;
           }

           .tam-empty {
              text-align: center !important;
              padding: 60px 0 !important;
              color: #94a3b8 !important;
              font-size: 15px !important;
           }

           /* Column 3: Action Panel */
           .tam-panel {
              width: 300px !important;
              min-width: 300px !important;
              background: white !important;
              border-left: 1px solid #e2e8f0 !important;
              display: flex !important;
              flex-direction: column !important;
              overflow: hidden !important;
              padding: 0 !important;
           }

           .tam-panel-content {
              flex: 1 !important;
              overflow-y: auto !important;
              padding: 32px 24px !important;
              display: flex !important;
              flex-direction: column !important;
              gap: 24px !important;
           }


           .tam-panel-label {
              font-size: 12px !important;
              font-weight: 800 !important;
              color: #94a3b8 !important;
              text-transform: uppercase !important;
              letter-spacing: 0.05em !important;
              margin-bottom: 16px !important;
           }

           .tam-summary-card {
              background: #f8fafc !important;
              padding: 24px !important;
              border-radius: 16px !important;
              border: 1px solid #f1f5f9 !important;
           }

           .tam-summary-row {
              display: flex !important;
              justify-content: space-between !important;
              align-items: center !important;
              font-size: 14px !important;
              font-weight: 600 !important;
              color: #475569 !important;
           }

           .tam-count-big {
              font-size: 32px !important;
              font-weight: 900 !important;
              color: #2563eb !important;
           }

           .tam-summary-divider {
              height: 1px !important;
              background: #e2e8f0 !important;
              margin: 16px 0 !important;
           }

           .tam-summary-row.mini {
              font-size: 12px !important;
              margin-bottom: 8px !important;
           }
           .tam-summary-row.mini .val { color: #0f172a !important; font-weight: 700 !important; }

           .tam-badge {
              padding: 2px 8px !important;
              border-radius: 4px !important;
              font-size: 10px !important;
              font-weight: 800 !important;
           }
           .tam-badge.critical { background: #fee2e2 !important; color: #b91c1c !important; }
           .tam-badge.high { background: #ffedd5 !important; color: #c2410c !important; }
           .tam-badge.medium { background: #dbeafe !important; color: #1e40af !important; }
           .tam-badge.low { background: #f1f5f9 !important; color: #475569 !important; }

           .tam-input-group label {
              display: block !important;
              font-size: 13px !important;
              font-weight: 700 !important;
              color: #475569 !important;
              margin-bottom: 8px !important;
           }

           .tam-input-group input {
              width: 100% !important;
              padding: 12px 14px !important;
              border: 1px solid #e2e8f0 !important;
              border-radius: 10px !important;
              font-size: 14px !important;
              outline: none !important;
              transition: border-color 0.2s !important;
           }
           .tam-input-group input:focus { border-color: #3b82f6 !important; }

           .tam-input-group .help {
              margin-top: 8px !important;
              font-size: 11px !important;
              color: #94a3b8 !important;
              line-height: 1.4 !important;
           }

           .tam-action-area {
              padding: 24px !important;
              background: white !important;
              border-top: 1px solid #f1f5f9 !important;
              flex-shrink: 0 !important;
              display: flex !important;
              flex-direction: column !important;
              gap: 12px !important;
           }

           .tam-btn-apply {
              width: 100% !important;
              height: 54px !important;
              background: #2563eb !important;
              color: white !important;
              border: none !important;
              border-radius: 12px !important;
              font-size: 15px !important;
              font-weight: 700 !important;
              display: flex !important;
              align-items: center !important;
              justify-content: center !important;
              gap: 12px !important;
              cursor: pointer !important;
              transition: all 0.2s !important;
              box-shadow: 0 4px 6px -1px rgba(37, 99, 235, 0.25) !important;
           }

           .tam-btn-apply:hover:not(:disabled) { transform: translateY(-1px) !important; background: #1d4ed8 !important; box-shadow: 0 10px 15px -3px rgba(37, 99, 235, 0.3) !important; }
           .tam-btn-apply:active:not(:disabled) { transform: translateY(0) !important; }
           .tam-btn-apply:disabled { background: #cbd5e1 !important; cursor: not-allowed !important; box-shadow: none !important; }

           .tam-btn-cancel {
              width: 100% !important;
              height: 48px !important;
              background: white !important;
              color: #64748b !important;
              border: 1px solid #e2e8f0 !important;
              border-radius: 12px !important;
              font-size: 14px !important;
              font-weight: 600 !important;
              cursor: pointer !important;
              transition: all 0.2s !important;
           }
           .tam-btn-cancel:hover { background: #f8fafc !important; color: #1e293b !important; }

        `}</style>
        </div>
      </div>
    </div>
  );
};

export default TemplateApplyModal;