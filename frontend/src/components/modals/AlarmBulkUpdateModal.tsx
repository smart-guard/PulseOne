import React, { useState } from 'react';
import { useTranslation } from 'react-i18next';
import { AlarmRuleSettings } from '../../api/services/alarmApi';
import '../../styles/alarm-settings.css';

interface AlarmRule {
  id: number;
  name: string;
  description: string;
  target_type: string;
  device_name?: string;
  severity: string;
  is_enabled: boolean;
}

interface AlarmBulkUpdateModalProps {
  selectedRules: number[];
  rules: AlarmRule[];
  onClose: () => void;
  onUpdate: (settings: Partial<AlarmRuleSettings>) => void;
  loading: boolean;
}

const AlarmBulkUpdateModal: React.FC<AlarmBulkUpdateModalProps> = ({
  selectedRules,
  rules,
  onClose,
  onUpdate,
  loading
}) => {
  const [updateSettings, setUpdateSettings] = useState<Partial<AlarmRuleSettings>>({});
  const { t } = useTranslation(['alarms', 'common']);
  const [selectedFields, setSelectedFields] = useState<Set<string>>(new Set());

  const handleFieldToggle = (field: string) => {
    const newSelectedFields = new Set(selectedFields);
    if (newSelectedFields.has(field)) {
      newSelectedFields.delete(field);
      const newSettings = { ...updateSettings };
      delete newSettings[field as keyof AlarmRuleSettings];
      setUpdateSettings(newSettings);
    } else {
      newSelectedFields.add(field);
    }
    setSelectedFields(newSelectedFields);
  };

  const handleSettingChange = (field: keyof AlarmRuleSettings, value: any) => {
    setUpdateSettings(prev => ({
      ...prev,
      [field]: value
    }));
  };

  const handleSubmit = () => {
    if (selectedFields.size === 0) {
      alert(t('bulk.selectFieldAlert', { defaultValue: '변경할 설정을 선택해주세요.' }));
      return;
    }

    const settingsToUpdate: Partial<AlarmRuleSettings> = {};
    selectedFields.forEach(field => {
      if (updateSettings[field as keyof AlarmRuleSettings] !== undefined) {
        (settingsToUpdate as any)[field] = (updateSettings as any)[field];
      }
    });

    onUpdate(settingsToUpdate);
  };

  // 헬퍼 함수들 (컴포넌트 내부에서 t() 사용)
  const getFieldDisplayName = (field: string): string => {
    const fieldNames: Record<string, string> = {
      priority: t('bulk.priority', { ns: 'alarms', defaultValue: '우선순위' }),
      severity: t('severity.label', { ns: 'alarms', defaultValue: '심각도' }),
      autoAcknowledge: t('labels.autoApprove', { ns: 'alarms', defaultValue: '자동 승인' }),
      autoReset: t('bulk.autoReset', { ns: 'alarms', defaultValue: '자동 초기화' }),
      emailEnabled: t('labels.emailNotifications', { ns: 'alarms', defaultValue: '이메일 알림' }),
      smsEnabled: t('bulk.smsEnabled', { ns: 'alarms', defaultValue: 'SMS 알림' }),
      suppressDuration: t('labels.duplicateSuppressionTimeS', { ns: 'alarms', defaultValue: '중복 억제 시간' }),
      escalationTime: t('bulk.escalationTime', { ns: 'alarms', defaultValue: '에스컬레이션 시간' }),
      isEnabled: t('bulk.isEnabled', { ns: 'alarms', defaultValue: '알람 활성화' })
    };
    return fieldNames[field] || field;
  };

  const formatFieldValue = (field: string, value: any): string => {
    if (value === undefined || value === null) return 'N/A';
    switch (field) {
      case 'priority': return value.toString().toUpperCase();
      case 'severity':
        return `${value} (${['', t('severity.lowest', { defaultValue: '최저' }), t('severity.low', { defaultValue: '낮음' }), t('severity.medium', { defaultValue: '중간' }), t('severity.high', { defaultValue: '높음' }), t('severity.highest', { defaultValue: '최고' })][value] || ''})`;
      case 'autoAcknowledge': case 'autoReset': case 'emailEnabled': case 'smsEnabled': case 'isEnabled':
        return value ? t('common:enabled', { defaultValue: '활성' }) : t('common:disabled', { defaultValue: '비활성' });
      case 'suppressDuration': return `${value}s`;
      case 'escalationTime': return `${value}분`;
      default: return value.toString();
    }
  };

  return (
    <div className="modal-overlay">
      <div className="modal-container">
        <div className="modal-header">
          <h2 className="modal-title">{t('labels.bulkSettingsChange', { ns: 'alarms' })}</h2>
          <button
            className="modal-close-btn"
            onClick={onClose}
            disabled={loading}
          >
            <i className="fas fa-times"></i>
          </button>
        </div>

        <div className="modal-content">
          {/* 선택된 규칙 요약 */}
          <div className="form-section">
            <h3>{t('bulk.selectedRules', { defaultValue: '선택된 알람 규칙' })} ({selectedRules.length})</h3>
            <div className="selected-rules-summary">
              {rules.slice(0, 5).map(rule => (
                <div key={rule.id} className="rule-summary-item">
                  <span className="rule-name">{rule.name}</span>
                  <span className="rule-device">{rule.device_name || 'N/A'}</span>
                </div>
              ))}
              {rules.length > 5 && (
                <div className="rule-summary-item more">
                  {t('bulk.andMoreRules', { count: rules.length - 5, defaultValue: '외 {{count}}개 규칙 더...' })}
                </div>
              )}
            </div>
          </div>

          {/* 일괄 변경 설정 */}
          <div className="form-section">
            <h3>{t('bulk.selectSettings', { ns: 'alarms' })}</h3>
            <p className="form-help">
              {t('bulk.helpText', { defaultValue: '체크한 설정만 선택된 전체 규칙에 적용됩니다.' })}
            </p>

            {/* 우선순위 설정 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('priority')}
                  onChange={() => handleFieldToggle('priority')}
                />
                <span>{t('bulk.priority', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('priority') && (
                <select
                  value={updateSettings.priority || 'medium'}
                  onChange={(e) => handleSettingChange('priority', e.target.value)}
                  className="form-select"
                >
                  <option value="low">{t('severity.low', { ns: 'alarms' })}</option>
                  <option value="medium">{t('severity.medium', { ns: 'alarms' })}</option>
                  <option value="high">{t('severity.high', { ns: 'alarms' })}</option>
                  <option value="critical">{t('severity.critical', { ns: 'alarms' })}</option>
                </select>
              )}
            </div>

            {/* 심각도 설정 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('severity')}
                  onChange={() => handleFieldToggle('severity')}
                />
                <span>{t('severity.label', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('severity') && (
                <select
                  value={updateSettings.severity || 3}
                  onChange={(e) => handleSettingChange('severity', Number(e.target.value))}
                  className="form-select"
                >
                  <option value={1}>1 (Lowest)</option>
                  <option value={2}>2 (Low)</option>
                  <option value={3}>3 (Normal)</option>
                  <option value={4}>4 (High)</option>
                  <option value={5}>5 (Highest)</option>
                </select>
              )}
            </div>

            {/* Auto Approve */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('autoAcknowledge')}
                  onChange={() => handleFieldToggle('autoAcknowledge')}
                />
                <span>{t('labels.autoApprove', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('autoAcknowledge') && (
                <div className="bulk-setting-options">
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoAcknowledge"
                      checked={updateSettings.autoAcknowledge === true}
                      onChange={() => handleSettingChange('autoAcknowledge', true)}
                    />
                    {t('common:enabled', { defaultValue: '활성' })}
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoAcknowledge"
                      checked={updateSettings.autoAcknowledge === false}
                      onChange={() => handleSettingChange('autoAcknowledge', false)}
                    />
                    {t('common:disabled', { defaultValue: '비활성' })}
                  </label>
                </div>
              )}
            </div>

            {/* Auto Reset */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('autoReset')}
                  onChange={() => handleFieldToggle('autoReset')}
                />
                <span>{t('bulk.autoReset', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('autoReset') && (
                <div className="bulk-setting-options">
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoReset"
                      checked={updateSettings.autoReset === true}
                      onChange={() => handleSettingChange('autoReset', true)}
                    />
                    {t('common:enabled', { defaultValue: '활성' })}
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoReset"
                      checked={updateSettings.autoReset === false}
                      onChange={() => handleSettingChange('autoReset', false)}
                    />
                    {t('common:disabled', { defaultValue: '비활성' })}
                  </label>
                </div>
              )}
            </div>

            {/* Email Notifications */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('emailEnabled')}
                  onChange={() => handleFieldToggle('emailEnabled')}
                />
                <span>{t('labels.emailNotifications', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('emailEnabled') && (
                <div className="bulk-setting-options">
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="emailEnabled"
                      checked={updateSettings.emailEnabled === true}
                      onChange={() => handleSettingChange('emailEnabled', true)}
                    />
                    {t('common:enabled', { defaultValue: '활성' })}
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="emailEnabled"
                      checked={updateSettings.emailEnabled === false}
                      onChange={() => handleSettingChange('emailEnabled', false)}
                    />
                    {t('common:disabled', { defaultValue: '비활성' })}
                  </label>
                </div>
              )}
            </div>

            {/* SMS Notification */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('smsEnabled')}
                  onChange={() => handleFieldToggle('smsEnabled')}
                />
                <span>{t('bulk.smsEnabled', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('smsEnabled') && (
                <div className="bulk-setting-options">
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="smsEnabled"
                      checked={updateSettings.smsEnabled === true}
                      onChange={() => handleSettingChange('smsEnabled', true)}
                    />
                    {t('common:enabled', { defaultValue: '활성' })}
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="smsEnabled"
                      checked={updateSettings.smsEnabled === false}
                      onChange={() => handleSettingChange('smsEnabled', false)}
                    />
                    {t('common:disabled', { defaultValue: '비활성' })}
                  </label>
                </div>
              )}
            </div>

            {/* 억제 시간 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('suppressDuration')}
                  onChange={() => handleFieldToggle('suppressDuration')}
                />
                <span>{t('labels.duplicateSuppressionTimeS', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('suppressDuration') && (
                <input
                  type="number"
                  value={updateSettings.suppressDuration || 300}
                  onChange={(e) => handleSettingChange('suppressDuration', Number(e.target.value) || 0)}
                  className="form-input"
                  min="0"
                  placeholder="300"
                />
              )}
            </div>

            {/* 에스컬레이션 시간 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('escalationTime')}
                  onChange={() => handleFieldToggle('escalationTime')}
                />
                <span>{t('bulk.escalationTime', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('escalationTime') && (
                <input
                  type="number"
                  value={updateSettings.escalationTime || 15}
                  onChange={(e) => handleSettingChange('escalationTime', Number(e.target.value) || 0)}
                  className="form-input"
                  min="0"
                  placeholder="15"
                />
              )}
            </div>

            {/* 규칙 Enabled/Disabled */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('isEnabled')}
                  onChange={() => handleFieldToggle('isEnabled')}
                />
                <span>{t('bulk.isEnabled', { ns: 'alarms' })}</span>
              </label>
              {selectedFields.has('isEnabled') && (
                <div className="bulk-setting-options">
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="isEnabled"
                      checked={updateSettings.isEnabled === true}
                      onChange={() => handleSettingChange('isEnabled', true)}
                    />
                    {t('common:enabled', { defaultValue: '활성' })}
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="isEnabled"
                      checked={updateSettings.isEnabled === false}
                      onChange={() => handleSettingChange('isEnabled', false)}
                    />
                    {t('common:disabled', { defaultValue: '비활성' })}
                  </label>
                </div>
              )}
            </div>
          </div>

          {/* Apply 미리보기 */}
          {selectedFields.size > 0 && (
            <div className="form-section">
              <h3>{t('bulk.previewTitle', { ns: 'alarms' })}</h3>
              <div className="preview-changes">
                {Array.from(selectedFields).map(field => (
                  <div key={field} className="change-item">
                    <span className="change-field">{getFieldDisplayName(field)}:</span>
                    <span className="change-value">{formatFieldValue(field, updateSettings[field as keyof AlarmRuleSettings])}</span>
                  </div>
                ))}
              </div>
              <div className="preview-warning">
                <i className="fas fa-exclamation-triangle"></i>
                {t('bulk.previewWarning', { count: selectedRules.length, defaultValue: '이 변경 사항은 선택된 {{count}}개 전체 규칙에 적용됩니다.' })}
              </div>
            </div>
          )}
        </div>

        <div className="modal-footer">
          <button
            className="btn btn-outline"
            onClick={onClose}
            disabled={loading}
          >
            {t('cancel', { ns: 'common' })}
          </button>
          <button
            className="btn btn-primary"
            onClick={handleSubmit}
            disabled={selectedFields.size === 0 || loading}
          >
            {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-edit"></i>}
            {selectedRules.length}개 규칙 일괄 변경
          </button>
        </div>
      </div>
    </div>
  );
};

export default AlarmBulkUpdateModal;