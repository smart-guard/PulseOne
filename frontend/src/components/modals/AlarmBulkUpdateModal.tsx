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
      alert('Please select a setting to change.');
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

  return (
    <div className="modal-overlay">
      <div className="modal-container">
        <div className="modal-header">
          <h2 className="modal-title">{t('labels.bulkSettingsChange', {ns: 'alarms'})}</h2>
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
            <h3>Selected Alarm Rules ({selectedRules.length})</h3>
            <div className="selected-rules-summary">
              {rules.slice(0, 5).map(rule => (
                <div key={rule.id} className="rule-summary-item">
                  <span className="rule-name">{rule.name}</span>
                  <span className="rule-device">{rule.device_name || 'N/A'}</span>
                </div>
              ))}
              {rules.length > 5 && (
                <div className="rule-summary-item more">
                  and {rules.length - 5} more rule(s)...
                </div>
              )}
            </div>
          </div>

          {/* 일괄 변경 설정 */}
          <div className="form-section">
            <h3>{t('bulk.selectSettings', {ns: 'alarms'})}</h3>
            <p className="form-help">
              Only checked settings will be applied to all selected rules.
            </p>

            {/* 우선순위 설정 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('priority')}
                  onChange={() => handleFieldToggle('priority')}
                />
                <span>{t('bulk.priority', {ns: 'alarms'})}</span>
              </label>
              {selectedFields.has('priority') && (
                <select
                  value={updateSettings.priority || 'medium'}
                  onChange={(e) => handleSettingChange('priority', e.target.value)}
                  className="form-select"
                >
                  <option value="low">{t('severity.low', {ns: 'alarms'})}</option>
                  <option value="medium">{t('severity.medium', {ns: 'alarms'})}</option>
                  <option value="high">{t('severity.high', {ns: 'alarms'})}</option>
                  <option value="critical">{t('severity.critical', {ns: 'alarms'})}</option>
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
                <span>{t('severity.label', {ns: 'alarms'})}</span>
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
                <span>{t('labels.autoApprove', {ns: 'alarms'})}</span>
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
                    Enabled
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoAcknowledge"
                      checked={updateSettings.autoAcknowledge === false}
                      onChange={() => handleSettingChange('autoAcknowledge', false)}
                    />
                    Disabled
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
                <span>{t('bulk.autoReset', {ns: 'alarms'})}</span>
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
                    Enabled
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoReset"
                      checked={updateSettings.autoReset === false}
                      onChange={() => handleSettingChange('autoReset', false)}
                    />
                    Disabled
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
                <span>{t('labels.emailNotifications', {ns: 'alarms'})}</span>
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
                    Enabled
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="emailEnabled"
                      checked={updateSettings.emailEnabled === false}
                      onChange={() => handleSettingChange('emailEnabled', false)}
                    />
                    Disabled
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
                <span>{t('bulk.smsEnabled', {ns: 'alarms'})}</span>
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
                    Enabled
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="smsEnabled"
                      checked={updateSettings.smsEnabled === false}
                      onChange={() => handleSettingChange('smsEnabled', false)}
                    />
                    Disabled
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
                <span>{t('labels.duplicateSuppressionTimeS', {ns: 'alarms'})}</span>
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
                <span>{t('bulk.escalationTime', {ns: 'alarms'})}</span>
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
                <span>{t('bulk.isEnabled', {ns: 'alarms'})}</span>
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
                    Enabled
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="isEnabled"
                      checked={updateSettings.isEnabled === false}
                      onChange={() => handleSettingChange('isEnabled', false)}
                    />
                    Disabled
                  </label>
                </div>
              )}
            </div>
          </div>

          {/* Apply 미리보기 */}
          {selectedFields.size > 0 && (
            <div className="form-section">
              <h3>{t('bulk.previewTitle', {ns: 'alarms'})}</h3>
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
                These changes will apply to all {selectedRules.length} selected rules.
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
            Cancel
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

// 헬퍼 함수들
const getFieldDisplayName = (field: string): string => {
  const fieldNames: Record<string, string> = {
    priority: 'Priority',
    severity: 'Severity',
    autoAcknowledge: 'Auto Approve',
    autoReset: 'Auto Reset',
    emailEnabled: 'Email Notifications',
    smsEnabled: 'SMS Notification',
    suppressDuration: 'Duplicate Suppression Time',
    escalationTime: 'Escalation Time',
    isEnabled: 'Alarm Enabled'
  };
  return fieldNames[field] || field;
};

const formatFieldValue = (field: string, value: any): string => {
  if (value === undefined || value === null) return 'N/A';

  switch (field) {
    case 'priority':
      return value.toString().toUpperCase();
    case 'severity':
      return `${value} (${['', 'Lowest', 'Low', 'Medium', 'High', 'Highest'][value] || ''})`;
    case 'autoAcknowledge':
    case 'autoReset':
    case 'emailEnabled':
    case 'smsEnabled':
    case 'isEnabled':
      return value ? 'Enabled' : 'Disabled';
    case 'suppressDuration':
      return `${value}s`;
    case 'escalationTime':
      return `${value}min`;
    default:
      return value.toString();
  }
};

export default AlarmBulkUpdateModal;