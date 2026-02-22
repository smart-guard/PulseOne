import React, { useState, useEffect, useRef } from 'react';
import { AlarmApiService, AlarmRule } from '../../api/services/alarmApi';
import { useConfirmContext } from '../common/ConfirmProvider';
import '../../styles/alarm-settings.css';
import '../../styles/notification-grid.css';
import '../../styles/script-patterns.css';
import '../../styles/limits-grid.css';

// --- Constants ---
const SCRIPT_PATTERNS = [
  { id: 'threshold_above', label: '단순 상한값', icon: '📈', script: 'return value > 100;' },
  { id: 'threshold_below', label: '단순 하한값', icon: '📉', script: 'return value < 0;' },
  { id: 'moving_avg', label: '이동 평균', icon: '📊', script: '// moving average\nconst avg = (value + prev_value) / 2;\nreturn avg > 80;' },
  { id: 'hysteresis', label: '히스테리시스', icon: '➰', script: 'if (!is_active) return value > 90;\nelse return value > 80;' },
  { id: 'rate_of_change', label: '급격한 변화량', icon: '⚡', script: 'return Math.abs(value - prev_value) > 10;' }
];

const ALARM_PRESETS = [
  {
    id: 'high_temp', icon: '🔥', title: '고온 경보', apply: {
      name: '시스템 고온 경보', category: 'temperature', alarm_type: 'analog' as const, high_limit: '80', high_high_limit: '90', severity: 'high' as const,
      tags: ['#high_temp', '#safety'], description: '냉각 시스템 이상 또는 부하 증가로 인한 내부 온도 상승을 감지합니다. 지속될 경우 하드웨어 손상이 발생할 수 있습니다.'
    }
  },
  {
    id: 'comm_loss', icon: '🔌', title: '통신 끊김', apply: {
      name: '통신 끊김 감지', category: 'system', target_type: 'device' as const, alarm_type: 'digital' as const, trigger_condition: 'connection_lost', severity: 'critical' as const,
      tags: ['#network', '#critical'], description: '디바이스와의 연결이 끊겼습니다. 네트워크 상태 또는 전원 공급 여부를 확인해야 합니다.'
    }
  },
  {
    id: 'delayed_trigger', icon: '⌛', title: '지연 발생', apply: {
      name: '지연 발생 경보', category: 'general', alarm_type: 'analog' as const, high_limit: '100', deadband: '5.0', severity: 'medium' as const,
      description: '일시적인 노이즈나 튀는 값에 의한 알람 오동작을 방지하기 위해 지연 시간을 적용한 경보입니다.'
    }
  },
  {
    id: 'complex_cond', icon: '🔴', title: '복합 조건', apply: {
      name: '복합 조건 알람', category: 'safety', target_type: 'data_point' as const, alarm_type: 'script' as const, condition_script: 'return value > 50 && prev_value < 50;', severity: 'high' as const,
      description: '단순 임계값만으로 설명하기 어려운 복잡한 로직(변화 감지 등)을 활용한 정밀 알람 설정입니다.'
    }
  }
];

const CATEGORY_DISPLAY_NAMES: Record<string, string> = {
  'temperature': '온도', 'pressure': '압력', 'flow': '유량', 'level': '레벨', 'vibration': '진동', 'electrical': '전기', 'safety': '안전', 'general': '일반'
};

// --- Interfaces ---
interface DataPoint { id: number; name: string; device_id: number; device_name: string; unit?: string; address?: string; }
interface Device { id: number; name: string; device_type: string; site_name?: string; }

interface AlarmCreateEditModalProps {
  isOpen: boolean;
  mode: 'create' | 'edit';
  rule?: AlarmRule;
  onClose: () => void;
  onSave: () => void;
  dataPoints: DataPoint[];
  devices: Device[];
  loadingTargetData: boolean;
  onDelete?: (id: number, name: string) => void;
  onRestore?: (id: number, name: string) => void;
}

interface AlarmRuleFormData {
  name: string;
  description: string;
  target_type: 'data_point' | 'device' | 'virtual_point';
  target_id: string;
  selected_device_id: string;
  target_group: string;
  alarm_type: 'analog' | 'digital' | 'script';
  high_high_limit: string;
  high_limit: string;
  low_limit: string;
  low_low_limit: string;
  deadband: string;
  rate_of_change: string;
  trigger_condition: string;
  condition_script: string;
  message_template: string;
  severity: 'critical' | 'high' | 'medium' | 'low' | 'info';
  priority: number;
  auto_acknowledge: boolean;
  auto_clear: boolean;
  is_latched: boolean;
  is_enabled: boolean;
  category: string;
  tags: string[];
}

const AlarmCreateEditModal: React.FC<AlarmCreateEditModalProps> = ({
  isOpen, mode, rule, onClose, onSave, onDelete, onRestore, dataPoints, devices
}) => {
  const tagInputRef = useRef<HTMLInputElement>(null);
  const [loading, setLoading] = useState(false);
  const [formData, setFormData] = useState<AlarmRuleFormData>({
    name: '', description: '', target_type: 'data_point', target_id: '', selected_device_id: '', target_group: '',
    alarm_type: 'analog', high_high_limit: '', high_limit: '', low_limit: '', low_low_limit: '', deadband: '2.0',
    rate_of_change: '', trigger_condition: '', condition_script: '', message_template: '', severity: 'medium',
    priority: 100, auto_acknowledge: false, auto_clear: true, is_latched: false, is_enabled: true, category: '', tags: []
  });

  useEffect(() => {
    if (isOpen && mode === 'edit' && rule) {
      setFormData({
        name: rule.name, description: rule.description || '', target_type: rule.target_type as any,
        target_id: rule.target_id?.toString() || '', selected_device_id: (rule as any).device_id?.toString() || '',
        target_group: rule.target_group || '', alarm_type: rule.alarm_type as any,
        high_high_limit: rule.high_high_limit?.toString() || '', high_limit: rule.high_limit?.toString() || '',
        low_limit: rule.low_limit?.toString() || '', low_low_limit: rule.low_low_limit?.toString() || '',
        deadband: rule.deadband?.toString() || '2.0', rate_of_change: rule.rate_of_change?.toString() || '',
        trigger_condition: rule.trigger_condition || '', condition_script: rule.condition_script || '',
        message_template: rule.message_template || '', severity: rule.severity as any, priority: rule.priority || 100,
        auto_acknowledge: rule.auto_acknowledge || false, auto_clear: rule.auto_clear || true,
        is_latched: (rule as any).is_latched || false,
        is_enabled: rule.is_enabled,
        category: rule.category || '', tags: rule.tags || []
      });
    } else if (isOpen && mode === 'create') {
      resetForm();
    }
  }, [mode, rule, isOpen]);

  const resetForm = () => {
    setFormData({
      name: '', description: '', target_type: 'data_point', target_id: '', selected_device_id: '', target_group: '',
      alarm_type: 'analog', high_high_limit: '', high_limit: '', low_limit: '', low_low_limit: '', deadband: '2.0',
      rate_of_change: '', trigger_condition: '', condition_script: '', message_template: '', severity: 'medium',
      priority: 100, auto_acknowledge: false, auto_clear: true, is_latched: false, is_enabled: true, category: '', tags: []
    });
  };

  const handleTargetTypeChange = (type: string) => {
    setFormData(prev => ({ ...prev, target_type: type as any, target_id: '', selected_device_id: '', alarm_type: type === 'device' ? 'digital' : 'analog' }));
  };

  const handleDeviceChange = (deviceId: string) => {
    setFormData(prev => ({ ...prev, selected_device_id: deviceId, target_id: '' }));
  };

  const handleTargetChange = (targetId: string) => {
    setFormData(prev => ({ ...prev, target_id: targetId }));

    // 🧠 Premium Feature: Smart Target Inference
    if (formData.target_type === 'data_point') {
      const dp = dataPoints.find(p => p.id.toString() === targetId);
      if (dp) {
        const name = (dp.name || '').toLowerCase();
        const unit = (dp.unit || '').toLowerCase();

        // Temperature inference
        if (unit.includes('c') || unit.includes('f') || name.includes('temp') || name.includes('온도')) {
          setFormData(prev => ({
            ...prev,
            category: 'temperature',
            alarm_type: 'analog',
            description: prev.description || `[${dp.name}] 데이터포인트의 온도 이상을 감지하는 규칙입니다.`
          }));
        }
        // Pressure inference
        else if (unit.includes('bar') || unit.includes('pa') || name.includes('press') || name.includes('압력')) {
          setFormData(prev => ({
            ...prev,
            category: 'pressure',
            alarm_type: 'analog',
            description: prev.description || `[${dp.name}] 데이터포인트의 압력 변화를 모니터링합니다.`
          }));
        }
      }
    }
  };

  const getDeviceOptions = () => devices.map(d => ({ value: d.id.toString(), label: d.name }));
  const getDataPointOptions = () => dataPoints.filter(dp => dp.device_id.toString() === formData.selected_device_id).map(dp => ({ value: dp.id.toString(), label: dp.name }));

  const getSelectedTargetName = () => {
    if (formData.target_type === 'device') return devices.find(d => d.id.toString() === formData.target_id)?.name || '디바이스';
    if (formData.target_type === 'data_point') return dataPoints.find(p => p.id.toString() === formData.target_id)?.name || '데이터포인트';
    return '가상포인트';
  };

  const addTag = (tag: string) => {
    if (tag.trim() && !formData.tags.includes(tag.trim())) {
      setFormData(prev => ({ ...prev, tags: [...prev.tags, tag.trim()] }));
    }
  };

  const removeTag = (tagToRemove: string) => {
    setFormData(prev => ({ ...prev, tags: prev.tags.filter(tag => tag !== tagToRemove) }));
  };

  const handlePresetSelect = (preset: any) => {
    setFormData(prev => ({ ...prev, ...preset.apply }));
  };

  const handlePatternSelect = (pattern: typeof SCRIPT_PATTERNS[0]) => {
    setFormData(prev => ({ ...prev, alarm_type: 'script', condition_script: pattern.script }));
  };

  const { confirm } = useConfirmContext(); // Hook usage

  const handleSubmit = async () => {
    if (!formData.name) {
      await confirm({ title: '입력 확인', message: '규칙 이름을 입력하세요.', confirmText: '확인', showCancelButton: false, confirmButtonType: 'primary' });
      return;
    }
    if (!formData.target_id) {
      await confirm({ title: '입력 확인', message: '타겟을 선택하세요.', confirmText: '확인', showCancelButton: false, confirmButtonType: 'primary' });
      return;
    }

    setLoading(true);
    try {
      const submitData = {
        ...formData,
        target_id: parseInt(formData.target_id),
        high_high_limit: formData.high_high_limit ? parseFloat(formData.high_high_limit) : undefined,
        high_limit: formData.high_limit ? parseFloat(formData.high_limit) : undefined,
        low_limit: formData.low_limit ? parseFloat(formData.low_limit) : undefined,
        low_low_limit: formData.low_low_limit ? parseFloat(formData.low_low_limit) : undefined,
        deadband: formData.deadband ? parseFloat(formData.deadband) : undefined,
        rate_of_change: formData.rate_of_change ? parseFloat(formData.rate_of_change) : undefined,
      };

      let response;
      if (mode === 'create') {
        response = await AlarmApiService.createAlarmRule(submitData as any);
      } else if (rule) {
        response = await AlarmApiService.updateAlarmRule(rule.id, submitData as any);
      }

      if (response && response.success) {
        await confirm({
          title: mode === 'create' ? '생성 완료' : '수정 완료',
          message: mode === 'create' ? '새로운 알람 규칙이 생성되었습니다.' : '알람 규칙이 수정되었습니다.',
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'primary'
        });
        onSave();
        onClose();
      } else {
        await confirm({
          title: '저장 실패',
          message: `저장에 실패했습니다: ${response?.message || '알 수 없는 오류'}`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      }
    } catch (error: any) {
      console.error(error);
      await confirm({
        title: '오류 발생',
        message: `저장 도중 오류가 발생했습니다: ${error.message || 'Unknown error'}`,
        confirmText: '확인',
        showCancelButton: false,
        confirmButtonType: 'danger'
      });
    } finally {
      setLoading(false);
    }
  };

  const generateSentence = () => {
    const targetName = getSelectedTargetName();
    const pills: { text: string; highlight?: boolean }[] = [];
    pills.push({ text: "만약" });
    pills.push({ text: `[${targetName}]`, highlight: true });
    pills.push({ text: "의" });
    pills.push({ text: formData.alarm_type === 'analog' ? '아날로그' : '상태', highlight: true });
    pills.push({ text: "값이" });

    if (formData.alarm_type === 'analog') {
      const val = formData.high_limit || formData.trigger_condition || "...";
      pills.push({ text: `[${val}]`, highlight: true });
    } else {
      pills.push({ text: `[${formData.trigger_condition || '...'}]`, highlight: true });
    }

    pills.push({ text: "이면 알람을 발생합니다." });
    return pills;
  };

  const renderSentencePills = (pills: { text: string; highlight?: boolean }[]) => (
    <div className="sentence-content" style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
      {pills.map((pill, idx) => (
        <span key={idx} className={`sentence-pill ${pill.highlight ? 'highlight' : ''}`} style={{
          background: pill.highlight ? 'var(--primary-600)' : 'transparent',
          padding: pill.highlight ? '2px 6px' : '0',
          borderRadius: '4px',
          color: '#fff'
        }}>{pill.text}</span>
      ))}
    </div>
  );

  if (!isOpen) return null;

  return (
    <div className="modal-overlay">
      <div className="modal modal-xl">
        <div className="modal-header">
          <h2 className="modal-title">{mode === 'create' ? '새 알람 규칙 생성:' : `알람 규칙 수정: ${rule?.name}`}</h2>
          <button className="close-button" onClick={onClose}><i className="fas fa-times"></i></button>
        </div>
        <div className="modal-content">
          <div className="form-section-header" style={{ padding: '24px 32px 0 32px', display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', color: 'var(--neutral-600)' }}>
            <i className="fas fa-pencil-alt"></i> 빠른 시작 (프리셋 & 템플릿)
          </div>
          <div className="preset-horizontal-scroll-container" style={{ padding: '12px 32px 24px 32px' }}>
            <div className="preset-horizontal-list">
              {ALARM_PRESETS.map(p => (
                <button key={p.id} type="button" className="preset-chip-btn preset-type" onClick={() => handlePresetSelect(p)}>
                  <span className="preset-chip-icon">{p.icon}</span>
                  <span className="preset-chip-title">{p.title}</span>
                </button>
              ))}
            </div>
          </div>

          <form onSubmit={e => e.preventDefault()} style={{ padding: '0 32px 32px 32px' }}>
            <div className="modal-form-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '32px 48px' }}>

              {/* --- Section 1: Basic Information --- */}
              <div className="form-section">
                <div className="form-group">
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                    <label className="form-label" style={{ marginBottom: 0 }}>규칙 이름 *</label>
                    <label className="toggle-switch-label" style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', fontSize: '13px', fontWeight: 600 }}>
                      <input
                        type="checkbox"
                        checked={formData.is_enabled}
                        onChange={e => setFormData(prev => ({ ...prev, is_enabled: e.target.checked }))}
                        style={{ accentColor: 'var(--success-500)', transform: 'scale(1.2)' }}
                      />
                      <span style={{ color: formData.is_enabled ? 'var(--success-600)' : 'var(--neutral-400)' }}>
                        {formData.is_enabled ? '활성화됨' : '비활성'}
                      </span>
                    </label>
                  </div>
                  <input type="text" className="form-input" placeholder="알람 규칙 이름을 입력하세요" value={formData.name} onChange={e => setFormData(prev => ({ ...prev, name: e.target.value }))} />
                </div>
                <div className="form-group">
                  <label className="form-label">카테고리</label>
                  <select className="form-select" value={formData.category} onChange={e => setFormData(prev => ({ ...prev, category: e.target.value }))}>
                    <option value="">선택하세요</option>
                    {Object.entries(CATEGORY_DISPLAY_NAMES).map(([val, label]) => <option key={val} value={val}>{label}</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label">설명</label>
                  <textarea className="form-input" placeholder="알람 규칙에 대한 설명을 입력하세요" rows={2} value={formData.description} onChange={e => setFormData(prev => ({ ...prev, description: e.target.value }))} />
                </div>
                <div className="form-group">
                  <label className="form-label">태그</label>
                  <div className="tags-input" onClick={() => tagInputRef.current?.focus()}>
                    {formData.tags.map(t => (
                      <span key={t} className="tag-item">
                        {t}
                        <button type="button" onClick={(e) => { e.stopPropagation(); removeTag(t); }}>
                          <i className="fas fa-times"></i>
                        </button>
                      </span>
                    ))}
                    <input
                      ref={tagInputRef}
                      type="text"
                      className="tags-input-field"
                      placeholder="태그 입력 후 Enter 또는 콤마 (예: #핵심)"
                      onKeyDown={e => {
                        if (e.key === 'Enter' || e.key === ',') {
                          e.preventDefault();
                          addTag(e.currentTarget.value);
                          e.currentTarget.value = '';
                        }
                      }}
                    />
                  </div>
                </div>
              </div>

              {/* --- Section 2: Target Selection --- */}
              <div className="form-section">
                <div className="form-group">
                  <label className="form-label">타겟 타입 *</label>
                  <div className="logic-pill-container" style={{ display: 'flex', gap: '8px' }}>
                    {['data_point', 'device', 'virtual_point'].map(t => (
                      <button key={t} type="button" className={`logic-pill ${formData.target_type === t ? 'active' : ''}`} onClick={() => handleTargetTypeChange(t)}>
                        {t === 'data_point' ? '데이터포인트' : t === 'device' ? '디바이스' : '가상포인트'}
                      </button>
                    ))}
                  </div>
                </div>
                <div className="form-group">
                  <label className="form-label">타겟 그룹</label>
                  <input type="text" className="form-input" placeholder="타겟 그룹 (선택사항)" value={formData.target_group} onChange={e => setFormData(prev => ({ ...prev, target_group: e.target.value }))} />
                </div>
                <div className="form-group">
                  <label className="form-label">디바이스 선택 *</label>
                  <select className="form-select" value={formData.selected_device_id} onChange={e => handleDeviceChange(e.target.value)}>
                    <option value="">디바이스를 선택하세요</option>
                    {getDeviceOptions().map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label">데이터포인트 선택 *</label>
                  <select className="form-select" value={formData.target_id} onChange={e => handleTargetChange(e.target.value)} disabled={!formData.selected_device_id}>
                    <option value="">데이터포인트를 선택하세요</option>
                    {getDataPointOptions().map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
                  </select>
                </div>
              </div>

              {/* --- Section 3: Condition Settings --- */}
              <div className="form-section">
                <div className="section-title">조건 설정</div>
                <div className="form-group">
                  <div className="logic-pill-container" style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
                    {['analog', 'digital', 'script'].map(t => (
                      <button key={t} type="button" className={`logic-pill ${formData.alarm_type === t ? 'active' : ''}`} onClick={() => setFormData(prev => ({ ...prev, alarm_type: t as any }))}>
                        {t === 'analog' ? '아날로그' : t === 'digital' ? '디지털' : '스크립트'}
                      </button>
                    ))}
                  </div>
                </div>
                {formData.alarm_type === 'analog' && (
                  <div className="limits-grid">
                    {['HH', 'H', 'L', 'LL'].map(l => (
                      <div key={l} className="form-group">
                        <label className="form-label">{l} LIMIT</label>
                        <input type="number" className="form-input"
                          value={(formData as any)[l === 'HH' ? 'high_high_limit' : l === 'H' ? 'high_limit' : l === 'L' ? 'low_limit' : 'low_low_limit']}
                          onChange={e => setFormData(prev => ({ ...prev, [l === 'HH' ? 'high_high_limit' : l === 'H' ? 'high_limit' : l === 'L' ? 'low_limit' : 'low_low_limit']: e.target.value }))} />
                      </div>
                    ))}
                    <div className="form-group">
                      <label className="form-label" title="알람 해제 히스테리시스. 값이 임계값에서 이 값만큼 더 내려가야 알람이 해제됩니다. 채터링 방지에 필수입니다.">
                        DEADBAND &nbsp;<i className="fas fa-info-circle" style={{ color: 'var(--primary-400)', fontSize: '11px' }} title="채터링 방지: 임계값 ± Deadband 범위 안에서 값이 오락가락해도 알람이 뜨지 않습니다."></i>
                      </label>
                      <input type="number" className="form-input" placeholder="예: 5.0" value={formData.deadband} onChange={e => setFormData(prev => ({ ...prev, deadband: e.target.value }))} />
                    </div>
                    <div className="form-group">
                      <label className="form-label">ROC LIMIT</label>
                      <input type="number" className="form-input" value={formData.rate_of_change} onChange={e => setFormData(prev => ({ ...prev, rate_of_change: e.target.value }))} />
                    </div>
                  </div>
                )}
                {formData.alarm_type === 'digital' && (
                  <div className="form-group">
                    <label className="form-label">트리거 조건</label>
                    <select className="form-select" value={formData.trigger_condition} onChange={e => setFormData(prev => ({ ...prev, trigger_condition: e.target.value }))}>
                      <option value="on_true">값이 True일 때 (1)</option>
                      <option value="on_false">값이 False일 때 (0)</option>
                      <option value="on_change">상태가 변할 때</option>
                      <option value="connection_lost">연결 끊김</option>
                    </select>
                  </div>
                )}
                {formData.alarm_type === 'script' && (
                  <div className="form-group">
                    <div className="script-patterns-grid" style={{ display: 'flex', flexWrap: 'wrap', gap: '8px', marginBottom: '16px' }}>
                      {SCRIPT_PATTERNS.map(p => (
                        <button key={p.id} type="button" className={`pattern-chip ${formData.condition_script === p.script ? 'active' : ''}`} onClick={() => handlePatternSelect(p)}>
                          {p.icon} {p.label}
                        </button>
                      ))}
                    </div>
                    <textarea className="form-input script-editor" rows={6} value={formData.condition_script} onChange={e => setFormData(prev => ({ ...prev, condition_script: e.target.value }))} style={{ fontFamily: 'monospace', fontSize: '13px', background: 'var(--neutral-50)' }} />
                  </div>
                )}
              </div>

              {/* --- Section 4: Notifications & Actions --- */}
              <div className="form-section">
                <div className="section-title">알림 및 조치</div>

                {/* Latching - 가장 중요하므로 최상단 강조 박스 */}
                <div style={{
                  background: formData.is_latched ? 'var(--warning-50, #fffbeb)' : 'var(--neutral-50)',
                  border: `1px solid ${formData.is_latched ? 'var(--warning-300, #fcd34d)' : 'var(--neutral-200)'}`,
                  borderRadius: '8px', padding: '12px 14px', marginBottom: '12px',
                  transition: 'all 0.2s ease',
                }}>
                  <label style={{ display: 'flex', alignItems: 'flex-start', gap: '10px', cursor: 'pointer' }}>
                    <input
                      type="checkbox"
                      checked={formData.is_latched}
                      onChange={e => setFormData(prev => ({ ...prev, is_latched: e.target.checked }))}
                      style={{ marginTop: '2px', accentColor: 'var(--warning-500)', transform: 'scale(1.2)', flexShrink: 0 }}
                    />
                    <div>
                      <div style={{ fontWeight: 600, fontSize: '13px', color: formData.is_latched ? 'var(--warning-700, #b45309)' : 'var(--neutral-700)' }}>
                        발보 잘츠 (Latching)
                        {formData.is_latched && <span style={{ marginLeft: '8px', fontSize: '11px', background: 'var(--warning-200)', color: 'var(--warning-800)', padding: '1px 6px', borderRadius: '10px' }}>활성화됨</span>}
                      </div>
                      <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '2px', lineHeight: 1.4 }}>
                        상태가 정상으로 복귀돼도 운전원이 <strong>직접 확인 버튼</strong>을 눏러야 알람이 해제됩니다.
                        Critical 알람에 권장합니다.
                      </div>
                    </div>
                  </label>
                </div>

                <div className="notification-grid">
                  <div className="checkbox-group">
                    <label className="checkbox-label" title="알람 발생 시 자동으로 확인 처리. 일반적으로 비활성화 권장.">
                      <input type="checkbox" checked={formData.auto_acknowledge} onChange={e => setFormData(prev => ({ ...prev, auto_acknowledge: e.target.checked }))} />
                      자동 확인 (Auto Ack)
                    </label>
                  </div>
                  <div className="checkbox-group">
                    <label className="checkbox-label" style={{ color: !formData.auto_clear ? 'var(--primary-600)' : undefined }}>
                      <input
                        type="checkbox"
                        checked={formData.auto_clear}
                        onChange={e => setFormData(prev => ({ ...prev, auto_clear: e.target.checked }))}
                        disabled={formData.is_latched}
                      />
                      <span>자동 해제 (Auto Clear)
                        {formData.is_latched && <span style={{ fontSize: '10px', color: 'var(--neutral-400)', marginLeft: '4px' }}>— Latching 시 무시됨</span>}
                      </span>
                    </label>
                  </div>
                  <div className="priority-group" style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                    <label className="form-label" style={{ whiteSpace: 'nowrap', marginBottom: 0 }}>우선순위 (1-1000)</label>
                    <input type="number" className="form-input" style={{ width: '80px' }} value={formData.priority} onChange={e => setFormData(prev => ({ ...prev, priority: parseInt(e.target.value) || 100 }))} />
                  </div>
                </div>
              </div>
            </div>

            <div className="sentence-builder-bar" style={{ marginTop: '32px', marginBottom: '12px', borderRadius: '8px' }}>
              <i className="fas fa-robot"></i>
              <div className="sentence-label" style={{ marginLeft: '12px' }}>Live Preview:</div>
              {renderSentencePills(generateSentence())}
            </div>
          </form>
        </div>
        <div className="modal-footer" style={{ borderTop: '1px solid var(--neutral-100)', padding: '20px 32px', display: 'flex', justifyContent: 'space-between', width: '100%' }}>
          <div className="footer-left" style={{ display: 'flex', gap: '8px', marginRight: 'auto' }}>
            {mode === 'edit' && (
              rule?.is_deleted ? (
                onRestore && (
                  <button type="button" className="btn btn-primary" onClick={() => onRestore(rule.id, rule.name)} style={{ backgroundColor: 'var(--primary-600)', borderColor: 'var(--primary-600)' }}>
                    <i className="fas fa-undo"></i> 복원
                  </button>
                )
              ) : (
                onDelete && (
                  <button type="button" className="btn btn-danger" onClick={() => onDelete(rule!.id, rule!.name)}>
                    <i className="fas fa-trash-alt"></i> 삭제
                  </button>
                )
              )
            )}
            {/* If there's nothing here, justify-between will push right section to the right. 
                If delete button is here, it stays on the left. */}
          </div>
          <div className="footer-right" style={{ display: 'flex', gap: '12px' }}>
            <button type="button" className="btn btn-secondary" style={{ minWidth: '100px' }} onClick={onClose}>취소</button>
            <button type="button" className="btn btn-primary" style={{ minWidth: '100px' }} onClick={handleSubmit}>
              {mode === 'create' ? '생성' : '수정'}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AlarmCreateEditModal;