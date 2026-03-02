// ============================================================================
// ValidationPanel.tsx - "Ready to Launch" Final Review Dashboard
// ============================================================================

import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import './VirtualPointModal.css';

interface ValidationPanelProps {
  formData: any;
  validationResult?: any;
  onValidate: () => void;
  isValidating: boolean;
  errors?: Record<string, string>;
  warnings?: Record<string, string>;
  simulationResult: number | null;
  isSimulating: boolean;
  onRunSimulation: (inputs?: Record<string, any>) => void;
}

const ValidationPanel: React.FC<ValidationPanelProps> = ({
  formData,
  errors = {},
  warnings = {},
  simulationResult,
  isSimulating,
  onRunSimulation
}) => {
  const [testInputs, setTestInputs] = useState<Record<string, any>>({});
  const { t } = useTranslation(['virtualPoints', 'common']);

  useEffect(() => {
    const inputs: Record<string, any> = {};
    formData.input_variables?.forEach((v: any) => {
      // Default values: 0 for number, false for boolean
      inputs[v.input_name] = v.data_type === 'number' ? 0 : v.data_type === 'boolean' ? false : '';
    });
    setTestInputs(inputs);
  }, [formData.input_variables]);

  const handleInputChange = (name: string, value: any) => {
    setTestInputs(prev => ({ ...prev, [name]: value }));
  };

  // handleRunTest logic moved to onRunSimulation prop

  const hasErrors = Object.keys(errors).length > 0;

  return (
    <div className="wizard-slide-active" style={{ width: '100%' }}>
      <div className="dashboard-grid-v2">

        {/* Left Column: Final Summary Checklist */}
        <div className="dashboard-left-panel">

          {/* Identity & Launch Status */}
          <div className="concept-group-card">
            <div className="concept-title">
              <i className="fas fa-check-double"></i> {t('validation.finalConfigReview', { ns: 'virtualPoints' })}
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', borderBottom: '1px solid #f1f5f9', paddingBottom: '12px' }}>
                <span style={{ color: '#64748b', fontSize: '13px', fontWeight: 700 }}>{t('validation.pointName', { ns: 'virtualPoints' })}</span>
                <span style={{ fontWeight: 800 }}>{formData.name || t('validation.notEntered', { ns: 'virtualPoints' })}</span>
              </div>

              <div style={{ display: 'flex', justifyContent: 'space-between', borderBottom: '1px solid #f1f5f9', paddingBottom: '12px' }}>
                <span style={{ color: '#64748b', fontSize: '13px', fontWeight: 700 }}>{t('validation.schedulingMode', { ns: 'virtualPoints' })}</span>
                <span>{formData.execution_type === 'periodic' ? t('validation.periodic', { ns: 'virtualPoints', ms: formData.execution_interval }) : t('validation.eventBased', { ns: 'virtualPoints' })}</span>
              </div>

              <div style={{ display: 'flex', justifyContent: 'space-between', borderBottom: '1px solid #f1f5f9', paddingBottom: '12px' }}>
                <span style={{ color: '#64748b', fontSize: '13px', fontWeight: 700 }}>{t('validation.dataType', { ns: 'virtualPoints' })}</span>
                <span className="summary-badge active" style={{ textTransform: 'uppercase' }}>{formData.data_type} {formData.unit && `(${formData.unit})`}</span>
              </div>

              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: '#64748b', fontSize: '13px', fontWeight: 700 }}>{t('validation.deployStatus', { ns: 'virtualPoints' })}</span>
                <span style={{ color: formData.is_enabled ? '#10b981' : '#f59e0b', fontWeight: 800 }}>
                  <i className="fas fa-circle" style={{ fontSize: '8px', marginRight: '6px' }}></i>
                  {formData.is_enabled ? t('validation.readyToLive', { ns: 'virtualPoints' }) : t('validation.draftMode', { ns: 'virtualPoints' })}
                </span>
              </div>
            </div>
          </div>

          {/* Logic Preview */}
          <div className="concept-group-card" style={{ borderLeft: '4px solid var(--blueprint-500)' }}>
            <div className="concept-title">
              <i className="fas fa-microchip"></i> {t('validation.calcLogicSummary', { ns: 'virtualPoints' })}
            </div>
            <div style={{ background: '#1e293b', color: '#38bdf8', padding: '20px', borderRadius: '12px', fontFamily: "'JetBrains Mono', monospace", fontSize: '14px', marginBottom: '16px' }}>
              {formData.expression || t('validation.noExpression', { ns: 'virtualPoints' })}
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
              <span style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 800, textTransform: 'uppercase', width: '100%', marginBottom: '4px' }}>{t('validation.variablesUsed', { ns: 'virtualPoints' })}</span>
              {formData.input_variables?.map((v: any) => (
                <span key={v.id} className="preset-chip" style={{ fontSize: '11px', background: '#f8fafc' }}>{v.input_name}</span>
              ))}
              {(!formData.input_variables || formData.input_variables.length === 0) && <span style={{ fontSize: '12px', color: '#94a3b8' }}>{t('validation.none', { ns: 'virtualPoints' })}</span>}
            </div>
          </div>

        </div>

        {/* Right Column: Guidance & Simulation */}
        <div className="dashboard-right-panel">
          <div className="guidance-panel">
            <div className="guidance-header">
              <i className="fas fa-rocket" style={{ color: 'var(--blueprint-500)' }}></i>
              {t('validation.readyToPublish', { ns: 'virtualPoints' })}
            </div>

            <div className="guidance-step">
              <div className="guidance-text">
                <h4>{t('validation.finalReviewStep', { ns: 'virtualPoints' })}</h4>
                <p>{t('validation.finalReviewDesc', { ns: 'virtualPoints' })}</p>
              </div>
            </div>

            {hasErrors ? (
              <div style={{ background: '#fff1f2', border: '1px solid #fecdd3', padding: '16px', borderRadius: '12px', color: '#be123c' }}>
                <div style={{ fontWeight: 800, fontSize: '14px', marginBottom: '4px' }}><i className="fas fa-exclamation-triangle"></i> 주의: 설정 오류</div>
                <div style={{ fontSize: '13px', marginBottom: '8px' }}>필수 항목이 누락되었거나 오류가 있습니다.</div>
                <ul style={{ fontSize: '12px', margin: 0, paddingLeft: '20px' }}>
                  {Object.values(errors).map((err, idx) => <li key={idx}>{err}</li>)}
                </ul>
              </div>
            ) : (
              <div style={{ background: '#f0fdf4', border: '1px solid #dcfce7', padding: '16px', borderRadius: '12px', color: '#15803d' }}>
                <div style={{ fontWeight: 800, fontSize: '14px', marginBottom: '4px' }}><i className="fas fa-check-circle"></i> 성공: 배포 가능</div>
                <div style={{ fontSize: '13px' }}>모든 설정이 완벽합니다. 하단의 <strong>저장하기</strong>를 눌러 완료하세요.</div>
                {Object.keys(warnings).length > 0 && (
                  <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: '1px dashed #dcfce7', fontSize: '12px', opacity: 0.8 }}>
                    <strong>권장사항:</strong> {Object.values(warnings).join(', ')}
                  </div>
                )}
              </div>
            )}

            {/* Sandbox Simulation */}
            <div style={{ marginTop: 'auto', background: '#f8fafc', padding: '20px', borderRadius: '16px', border: '1px solid #e2e8f0' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                <span style={{ fontSize: '13px', fontWeight: 800, color: '#475569' }}>{t('validation.simulationSandbox', { ns: 'virtualPoints' })}</span>
                <button
                  className="btn btn-primary btn-sm"
                  onClick={() => onRunSimulation(testInputs)}
                  disabled={isSimulating || !formData.expression}
                >
                  {isSimulating ? t('validation.running', { ns: 'virtualPoints' }) : t('validation.liveRecalc', { ns: 'virtualPoints' })}
                </button>
              </div>

              {/* Variable Input Table */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', marginBottom: '20px', maxHeight: '180px', overflowY: 'auto', paddingRight: '4px' }}>
                {formData.input_variables?.map((v: any) => (
                  <div key={v.id} style={{ display: 'flex', alignItems: 'center', background: 'white', padding: '8px 12px', borderRadius: '8px', border: '1px solid #f1f5f9' }}>
                    <div style={{ flex: 1, fontSize: '12px', fontWeight: 700, color: '#475569' }}>
                      {v.input_name}
                      <span style={{ fontSize: '10px', color: '#94a3b8', marginLeft: '6px' }}>{v.data_type}</span>
                    </div>
                    {v.data_type === 'number' ? (
                      <input
                        type="number"
                        className="form-input"
                        style={{ width: '80px', height: '32px', textAlign: 'right', fontSize: '12px' }}
                        value={testInputs[v.input_name] ?? 0}
                        onChange={(e) => handleInputChange(v.input_name, parseFloat(e.target.value) || 0)}
                      />
                    ) : (
                      <button
                        className={`choice-btn-v3 ${testInputs[v.input_name] ? 'active' : ''}`}
                        style={{ padding: '4px 12px', fontSize: '10px' }}
                        onClick={() => handleInputChange(v.input_name, !testInputs[v.input_name])}
                      >
                        {testInputs[v.input_name] ? 'TRUE' : 'FALSE'}
                      </button>
                    )}
                  </div>
                ))}
                {(!formData.input_variables || formData.input_variables.length === 0) && (
                  <div style={{ textAlign: 'center', padding: '12px', color: '#94a3b8', fontSize: '11px' }}>입력 변수가 없습니다.</div>
                )}
              </div>

              {simulationResult !== null ? (
                <div style={{ textAlign: 'center', background: 'white', padding: '16px', borderRadius: '12px', border: '1px solid #e2e8f0' }}>
                  <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>{t('validation.expectedResult', { ns: 'virtualPoints' })} ({formData.unit || '-'})</div>
                  <div style={{ fontSize: '32px', fontWeight: 800, color: '#0d9488' }}>
                    {typeof simulationResult === 'number' ? simulationResult.toFixed(formData.decimal_places || 2) : String(simulationResult)}
                  </div>
                </div>
              ) : (
                <div style={{ textAlign: 'center', padding: '12px', color: '#94a3b8', fontSize: '11px', fontStyle: 'italic' }}>
                  {t('validation.simHint', { ns: 'virtualPoints' })}
                </div>
              )}
            </div>
          </div>
        </div>

      </div>
    </div>
  );
};

export default ValidationPanel;