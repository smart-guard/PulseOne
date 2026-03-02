import React, { useState, useEffect, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { AlarmApiService, AlarmRule } from '../../api/services/alarmApi';
import { useConfirmContext } from '../common/ConfirmProvider';
import '../../styles/alarm-settings.css';
import '../../styles/notification-grid.css';
import '../../styles/script-patterns.css';
import '../../styles/limits-grid.css';

// --- Constants ---
const SCRIPT_PATTERNS = [
  { id: 'threshold_above', label: 'Simple Upper Limit', icon: '📈', script: 'return value > 100;' },
  { id: 'threshold_below', label: 'Simple Lower Limit', icon: '📉', script: 'return value < 0;' },
  { id: 'moving_avg', label: 'Moving Average', icon: '📊', script: '// moving average\nconst avg = (value + prev_value) / 2;\nreturn avg > 80;' },
  { id: 'hysteresis', label: 'Hysteresis', icon: '➰', script: 'if (!is_active) return value > 90;\nelse return value > 80;' },
  { id: 'rate_of_change', label: 'Rapid Rate of Change', icon: '⚡', script: 'return Math.abs(value - prev_value) > 10;' }
];

const ALARM_PRESETS = [
  {
    id: 'high_temp', icon: '🔥', title: 'High Temperature Alarm', apply: {
      name: 'System High Temp Alarm', category: 'temperature', alarm_type: 'analog' as const, high_limit: '80', high_high_limit: '90', severity: 'high' as const,
      tags: ['#high_temp', '#safety'], description: 'Detects internal temperature rise due to cooling system issues or increased load. May cause hardware damage if sustained.'
    }
  },
  {
    id: 'comm_loss', icon: '🔌', title: 'Communication Lost', apply: {
      name: 'Connection Loss Detection', category: 'system', target_type: 'device' as const, alarm_type: 'digital' as const, trigger_condition: 'connection_lost', severity: 'critical' as const,
      tags: ['#network', '#critical'], description: 'Device connection has been lost. Check network status or power supply.'
    }
  },
  {
    id: 'delayed_trigger', icon: '⌛', title: 'Delayed Trigger', apply: {
      name: 'Delay-Based Alarm', category: 'general', alarm_type: 'analog' as const, high_limit: '100', deadband: '5.0', severity: 'medium' as const,
      description: 'An alarm with delay to prevent false triggers from temporary noise or spikes.'
    }
  },
  {
    id: 'complex_cond', icon: '🔴', title: 'Complex Condition', apply: {
      name: 'Complex Condition Alarm', category: 'safety', target_type: 'data_point' as const, alarm_type: 'script' as const, condition_script: 'return value > 50 && prev_value < 50;', severity: 'high' as const,
      description: 'Precise alarm using complex logic (e.g., change detection) that cannot be expressed with simple thresholds.'
    }
  }
];

const CATEGORY_DISPLAY_NAMES: Record<string, string> = {
  'temperature': 'Temperature', 'pressure': 'Pressure', 'flow': 'Flow', 'level': 'Level', 'vibration': 'Vibration', 'electrical': 'Electrical', 'safety': 'Safety', 'general': 'General'
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
    const { t } = useTranslation(['alarms', 'common']);
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
        if (unit.includes('c') || unit.includes('f') || name.includes('temp') || name.includes('temperature')) {
          setFormData(prev => ({
            ...prev,
            category: 'temperature',
            alarm_type: 'analog',
            description: prev.description || `Detects temperature anomaly in [${dp.name}] data point.`
          }));
        }
        // Pressure inference
        else if (unit.includes('bar') || unit.includes('pa') || name.includes('press') || name.includes('pressure')) {
          setFormData(prev => ({
            ...prev,
            category: 'pressure',
            alarm_type: 'analog',
            description: prev.description || `Monitors pressure changes in [${dp.name}] data point.`
          }));
        }
      }
    }
  };

  const getDeviceOptions = () => devices.map(d => ({ value: d.id.toString(), label: d.name }));
  const getDataPointOptions = () => dataPoints.filter(dp => dp.device_id.toString() === formData.selected_device_id).map(dp => ({ value: dp.id.toString(), label: dp.name }));

  const getSelectedTargetName = () => {
    if (formData.target_type === 'device') return devices.find(d => d.id.toString() === formData.target_id)?.name || 'Device';
    if (formData.target_type === 'data_point') return dataPoints.find(p => p.id.toString() === formData.target_id)?.name || 'Data Point';
    return 'Virtual Point';
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
      await confirm({ title: 'Input Required', message: 'Please enter a rule name.', confirmText: 'OK', showCancelButton: false, confirmButtonType: 'primary' });
      return;
    }
    if (!formData.target_id) {
      await confirm({ title: 'Input Required', message: 'Please select a target.', confirmText: 'OK', showCancelButton: false, confirmButtonType: 'primary' });
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
          title: mode === 'create' ? 'Created' : 'Updated',
          message: mode === 'create' ? 'New alarm rule created.' : 'Alarm rule updated.',
          confirmText: 'OK',
          showCancelButton: false,
          confirmButtonType: 'primary'
        });
        onSave();
        onClose();
      } else {
        await confirm({
          title: 'Save Failed',
          message: `Save failed: ${response?.message || 'Unknown error'}`,
          confirmText: 'OK',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      }
    } catch (error: any) {
      console.error(error);
      await confirm({
        title: 'Error',
        message: `Error occurred while saving: ${error.message || 'Unknown error'}`,
        confirmText: 'OK',
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
    pills.push({ text: "If" });
    pills.push({ text: `[${targetName}]`, highlight: true });
    pills.push({ text: "of" });
    pills.push({ text: formData.alarm_type === 'analog' ? 'Analog' : 'Status', highlight: true });
    pills.push({ text: "value is" });

    if (formData.alarm_type === 'analog') {
      const val = formData.high_limit || formData.trigger_condition || "...";
      pills.push({ text: `[${val}]`, highlight: true });
    } else {
      pills.push({ text: `[${formData.trigger_condition || '...'}]`, highlight: true });
    }

    pills.push({ text: "then trigger alarm." });
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
          <h2 className="modal-title">{mode === 'create' ? 'Create New Alarm Rule:' : `Edit Alarm Rule: ${rule?.name}`}</h2>
          <button className="close-button" onClick={onClose}><i className="fas fa-times"></i></button>
        </div>
        <div className="modal-content">
          <div className="form-section-header" style={{ padding: '24px 32px 0 32px', display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', color: 'var(--neutral-600)' }}>
            <i className="fas fa-pencil-alt"></i> Quick Start (Presets & Templates)
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
                    <label className="form-label" style={{ marginBottom: 0 }}>Rule Name *</label>
                    <label className="toggle-switch-label" style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', fontSize: '13px', fontWeight: 600 }}>
                      <input
                        type="checkbox"
                        checked={formData.is_enabled}
                        onChange={e => setFormData(prev => ({ ...prev, is_enabled: e.target.checked }))}
                        style={{ accentColor: 'var(--success-500)', transform: 'scale(1.2)' }}
                      />
                      <span style={{ color: formData.is_enabled ? 'var(--success-600)' : 'var(--neutral-400)' }}>
                        {formData.is_enabled ? 'Enabled' : 'Disabled'}
                      </span>
                    </label>
                  </div>
                  <input type="text" className="form-input" placeholder="Enter alarm rule name" value={formData.name} onChange={e => setFormData(prev => ({ ...prev, name: e.target.value }))} />
                </div>
                <div className="form-group">
                  <label className="form-label">Category</label>
                  <select className="form-select" value={formData.category} onChange={e => setFormData(prev => ({ ...prev, category: e.target.value }))}>
                    <option value="">Select</option>
                    {Object.entries(CATEGORY_DISPLAY_NAMES).map(([val, label]) => <option key={val} value={val}>{label}</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label">Description</label>
                  <textarea className="form-input" placeholder="Enter a description for this alarm rule" rows={2} value={formData.description} onChange={e => setFormData(prev => ({ ...prev, description: e.target.value }))} />
                </div>
                <div className="form-group">
                  <label className="form-label">Tags</label>
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
                      placeholder="Enter tag then press Enter or comma"
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
                  <label className="form-label">Target Type *</label>
                  <div className="logic-pill-container" style={{ display: 'flex', gap: '8px' }}>
                    {['data_point', 'device', 'virtual_point'].map(t => (
                      <button key={t} type="button" className={`logic-pill ${formData.target_type === t ? 'active' : ''}`} onClick={() => handleTargetTypeChange(t)}>
                        {t === 'data_point' ? 'Data Point' : t === 'device' ? 'Device' : 'Virtual Point'}
                      </button>
                    ))}
                  </div>
                </div>
                <div className="form-group">
                  <label className="form-label">Target Group</label>
                  <input type="text" className="form-input" placeholder="Target group (optional)" value={formData.target_group} onChange={e => setFormData(prev => ({ ...prev, target_group: e.target.value }))} />
                </div>
                <div className="form-group">
                  <label className="form-label">Select Device *</label>
                  <select className="form-select" value={formData.selected_device_id} onChange={e => handleDeviceChange(e.target.value)}>
                    <option value="">Select a device</option>
                    {getDeviceOptions().map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label">Select Data Point *</label>
                  <select className="form-select" value={formData.target_id} onChange={e => handleTargetChange(e.target.value)} disabled={!formData.selected_device_id}>
                    <option value="">Select a data point</option>
                    {getDataPointOptions().map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
                  </select>
                </div>
              </div>

              {/* --- Section 3: Condition Settings --- */}
              <div className="form-section">
                <div className="section-title">Condition Settings</div>
                <div className="form-group">
                  <div className="logic-pill-container" style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
                    {['analog', 'digital', 'script'].map(t => (
                      <button key={t} type="button" className={`logic-pill ${formData.alarm_type === t ? 'active' : ''}`} onClick={() => setFormData(prev => ({ ...prev, alarm_type: t as any }))}>
                        {t === 'analog' ? 'Analog' : t === 'digital' ? 'Digital' : 'Script'}
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
                      <label className="form-label" title="Alarm clear hysteresis. Value must drop this amount below threshold before alarm clears. Required to prevent chattering.">
                        DEADBAND &nbsp;<i className="fas fa-info-circle" style={{ color: 'var(--primary-400)', fontSize: '11px' }} title="Chattering prevention: Alarm will not trigger if value fluctuates within threshold ± Deadband range."></i>
                      </label>
                      <input type="number" className="form-input" placeholder="e.g. 5.0" value={formData.deadband} onChange={e => setFormData(prev => ({ ...prev, deadband: e.target.value }))} />
                    </div>
                    <div className="form-group">
                      <label className="form-label">ROC LIMIT</label>
                      <input type="number" className="form-input" value={formData.rate_of_change} onChange={e => setFormData(prev => ({ ...prev, rate_of_change: e.target.value }))} />
                    </div>
                  </div>
                )}
                {formData.alarm_type === 'digital' && (
                  <div className="form-group">
                    <label className="form-label">Trigger Condition</label>
                    <select className="form-select" value={formData.trigger_condition} onChange={e => setFormData(prev => ({ ...prev, trigger_condition: e.target.value }))}>
                      <option value="on_true">When value is True (1)</option>
                      <option value="on_false">When value is False (0)</option>
                      <option value="on_change">On State Change</option>
                      <option value="connection_lost">Connection Lost</option>
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
                <div className="section-title">Notifications & Actions</div>

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
                        Latching
                        {formData.is_latched && <span style={{ marginLeft: '8px', fontSize: '11px', background: 'var(--warning-200)', color: 'var(--warning-800)', padding: '1px 6px', borderRadius: '10px' }}>Enabled</span>}
                      </div>
                      <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '2px', lineHeight: 1.4 }}>
                        Even after the state returns to normal, the operator must press <strong>the Acknowledge button</strong> to clear the alarm.
                        Recommended for Critical alarms.
                      </div>
                    </div>
                  </label>
                </div>

                <div className="notification-grid">
                  <div className="checkbox-group">
                    <label className="checkbox-label" title="Automatically acknowledges alarm on trigger. Disabling is generally recommended.">
                      <input type="checkbox" checked={formData.auto_acknowledge} onChange={e => setFormData(prev => ({ ...prev, auto_acknowledge: e.target.checked }))} />
                      Auto Acknowledge
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
            <button type="button" className="btn btn-secondary" style={{ minWidth: '100px' }} onClick={onClose}>{t('cancel', {ns: 'common'})}</button>
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