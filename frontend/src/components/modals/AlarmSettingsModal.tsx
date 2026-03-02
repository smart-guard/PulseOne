import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { AlarmRuleSettings, AlarmStatistics } from '../../api/services/alarmApi';
import '../../styles/alarm-settings.css';

interface AlarmRule {
  id: number;
  name: string;
  description: string;
  target_type: string;
  device_name?: string;
  data_point_name?: string;
  category: string;
  condition_type: string;
  created_at: string;
  updated_at: string;
}

interface AlarmSettingsModalProps {
  rule: AlarmRule;
  currentSettings: AlarmRuleSettings;
  statistics?: AlarmStatistics;
  onClose: () => void;
  onSave: (settings: Partial<AlarmRuleSettings>) => void;
  onSaveAndClose: (settings: Partial<AlarmRuleSettings>) => void;
  hasChanges: boolean;
  loading: boolean;
}

const AlarmSettingsModal: React.FC<AlarmSettingsModalProps> = ({
  rule,
  currentSettings,
  statistics,
  onClose,
  onSave,
  onSaveAndClose,
  hasChanges,
  loading
}) => {
  const [localSettings, setLocalSettings] = useState<AlarmRuleSettings>(currentSettings);
    const { t } = useTranslation(['alarms', 'common']);
  const [activeTab, setActiveTab] = useState<'thresholds' | 'behavior' | 'notifications' | 'schedule'>('thresholds');

  useEffect(() => {
    setLocalSettings(currentSettings);
  }, [currentSettings]);

  const handleSettingChange = (key: keyof AlarmRuleSettings, value: any) => {
    const newSettings = { ...localSettings, [key]: value };
    setLocalSettings(newSettings);
    onSave({ [key]: value });
  };

  const handleScheduleChange = (scheduleUpdate: Partial<AlarmRuleSettings['schedule']>) => {
    const newSchedule = { ...localSettings.schedule, ...scheduleUpdate };
    handleSettingChange('schedule', newSchedule);
  };

  const handleSaveAndClose = () => {
    onSaveAndClose(localSettings);
  };

  return (
    <div className="modal-overlay">
      <div className="modal-container large">
        <div className="modal-header">
          <h2 className="modal-title">Alarm Settings: {rule.name}</h2>
          <button
            className="modal-close-btn"
            onClick={onClose}
            disabled={loading}
          >
            <i className="fas fa-times"></i>
          </button>
        </div>

        <div className="modal-content">
          {/* 읽기전용 정보 */}
          <div className="form-section readonly-section">
            <h3>{t('settingsModal.basicInfoReadonly', {ns: 'alarms'})}</h3>
            <div className="readonly-grid">
              <div className="readonly-item">
                <label>{t('settingsModal.dataSource', {ns: 'alarms'})}</label>
                <span>{rule.data_point_name || `${rule.target_type} ${rule.id}`}</span>
              </div>
              <div className="readonly-item">
                <label>{t('columns.device', {ns: 'alarms'})}</label>
                <span>{rule.device_name || 'N/A'}</span>
              </div>
              <div className="readonly-item">
                <label>{t('modals.category', {ns: 'alarms'})}</label>
                <span>{rule.category || 'General'}</span>
              </div>
              <div className="readonly-item">
                <label>{t('settingsModal.conditionType', {ns: 'alarms'})}</label>
                <span>{rule.condition_type}</span>
              </div>
              {statistics && (
                <>
                  <div className="readonly-item">
                    <label>{t('settingsModal.occurrenceCount', {ns: 'alarms'})}</label>
                    <span>{statistics.occurrence_summary.total_occurrences} times</span>
                  </div>
                  <div className="readonly-item">
                    <label>{t('settingsModal.avgResponseTime', {ns: 'alarms'})}</label>
                    <span>{statistics.performance_metrics.avg_response_time_minutes?.toFixed(1) || 'N/A'}min</span>
                  </div>
                  <div className="readonly-item">
                    <label>{t('settingsModal.lastOccurrence', {ns: 'alarms'})}</label>
                    <span>{statistics.performance_metrics.last_triggered
                      ? new Date(statistics.performance_metrics.last_triggered).toLocaleString()
                      : 'N/A'}</span>
                  </div>
                </>
              )}
              <div className="readonly-item">
                <label>{t('templates.lastModified', {ns: 'alarms'})}</label>
                <span>{new Date(rule.updated_at).toLocaleString()}</span>
              </div>
            </div>
          </div>

          {/* 탭 네비게이션 */}
          <div className="tab-navigation">
            <button
              className={`tab-button ${activeTab === 'thresholds' ? 'active' : ''}`}
              onClick={() => setActiveTab('thresholds')}
            >
              <i className="fas fa-sliders-h"></i> Threshold
            </button>
            <button
              className={`tab-button ${activeTab === 'behavior' ? 'active' : ''}`}
              onClick={() => setActiveTab('behavior')}
            >
              <i className="fas fa-cogs"></i> Actions
            </button>
            <button
              className={`tab-button ${activeTab === 'notifications' ? 'active' : ''}`}
              onClick={() => setActiveTab('notifications')}
            >
              <i className="fas fa-bell"></i> Notifications
            </button>
            <button
              className={`tab-button ${activeTab === 'schedule' ? 'active' : ''}`}
              onClick={() => setActiveTab('schedule')}
            >
              <i className="fas fa-clock"></i> Schedule
            </button>
          </div>

          {/* 탭 컨텐츠 */}
          <div className="tab-content">
            {/* Threshold Settings 탭 */}
            {activeTab === 'thresholds' && (
              <div className="form-section">
                <h3>{t('settingsModal.thresholdConfig', {ns: 'alarms'})}</h3>
                {(rule.condition_type === 'threshold' || rule.condition_type === 'range') && (
                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">{t('settingsModal.veryHigh', {ns: 'alarms'})}</label>
                      <input
                        type="number"
                        value={localSettings.highHighLimit || ''}
                        onChange={(e) => handleSettingChange('highHighLimit', Number(e.target.value) || undefined)}
                        className="form-input"
                        step="0.1"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">{t('settingsModal.high', {ns: 'alarms'})}</label>
                      <input
                        type="number"
                        value={localSettings.highLimit || ''}
                        onChange={(e) => handleSettingChange('highLimit', Number(e.target.value) || undefined)}
                        className="form-input"
                        step="0.1"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">{t('settingsModal.low', {ns: 'alarms'})}</label>
                      <input
                        type="number"
                        value={localSettings.lowLimit || ''}
                        onChange={(e) => handleSettingChange('lowLimit', Number(e.target.value) || undefined)}
                        className="form-input"
                        step="0.1"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">{t('settingsModal.veryLow', {ns: 'alarms'})}</label>
                      <input
                        type="number"
                        value={localSettings.lowLowLimit || ''}
                        onChange={(e) => handleSettingChange('lowLowLimit', Number(e.target.value) || undefined)}
                        className="form-input"
                        step="0.1"
                      />
                    </div>
                  </div>
                )}

                <div className="form-group">
                  <label className="form-label">{t('settingsModal.hysteresis', {ns: 'alarms'})}</label>
                  <input
                    type="number"
                    value={localSettings.deadband === 0 ? '0' : (localSettings.deadband || '')}
                    onChange={(e) => {
                      const value = e.target.value;
                      handleSettingChange('deadband', value === '' ? undefined : Number(value));
                    }}
                    className="form-input"
                    step="0.1"
                    min="0"
                    placeholder="Deadband value"
                  />
                  <small className="form-help">Deadband to prevent alarm chattering (0 = no deadband)</small>
                </div>
              </div>
            )}

            {/* Actions 설정 탭 */}
            {activeTab === 'behavior' && (
              <div className="form-section">
                <h3>{t('labels.priorityActions', {ns: 'alarms'})}</h3>
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">{t('bulk.priority', {ns: 'alarms'})}</label>
                    <select
                      value={localSettings.priority}
                      onChange={(e) => handleSettingChange('priority', e.target.value as any)}
                      className="form-select"
                    >
                      <option value="low">{t('severity.low', {ns: 'alarms'})}</option>
                      <option value="medium">{t('severity.medium', {ns: 'alarms'})}</option>
                      <option value="high">{t('severity.high', {ns: 'alarms'})}</option>
                      <option value="critical">{t('severity.critical', {ns: 'alarms'})}</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label className="form-label">Severity (1-5)</label>
                    <select
                      value={localSettings.severity}
                      onChange={(e) => handleSettingChange('severity', Number(e.target.value) as any)}
                      className="form-select"
                    >
                      <option value={1}>1 (Lowest)</option>
                      <option value={2}>2 (Low)</option>
                      <option value={3}>3 (Normal)</option>
                      <option value={4}>4 (High)</option>
                      <option value={5}>5 (Highest)</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label className="form-label">{t('bulk.escalationTime', {ns: 'alarms'})}</label>
                    <input
                      type="number"
                      value={localSettings.escalationTime}
                      onChange={(e) => handleSettingChange('escalationTime', Number(e.target.value) || 0)}
                      className="form-input"
                      min="0"
                    />
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">{t('labels.dedupSuppressionTimeS', {ns: 'alarms'})}</label>
                    <input
                      type="number"
                      value={localSettings.suppressDuration}
                      onChange={(e) => handleSettingChange('suppressDuration', Number(e.target.value) || 0)}
                      className="form-input"
                      min="0"
                    />
                    <small className="form-help">{t('settingsModal.suppressHelp', {ns: 'alarms'})}</small>
                  </div>
                  <div className="form-group">
                    <label className="form-label">{t('settingsModal.maxOccurrences', {ns: 'alarms'})}</label>
                    <input
                      type="number"
                      value={localSettings.maxOccurrences}
                      onChange={(e) => handleSettingChange('maxOccurrences', Number(e.target.value) || 0)}
                      className="form-input"
                      min="0"
                    />
                    <small className="form-help">0 = unlimited</small>
                  </div>
                </div>

                <div className="checkbox-options">
                  <div className="checkbox-group">
                    <input
                      type="checkbox"
                      className="checkbox"
                      checked={localSettings.autoAcknowledge}
                      onChange={(e) => handleSettingChange('autoAcknowledge', e.target.checked)}
                    />
                    <label className="checkbox-label">{t('labels.autoApprove', {ns: 'alarms'})}</label>
                  </div>
                  <div className="checkbox-group">
                    <input
                      type="checkbox"
                      className="checkbox"
                      checked={localSettings.autoReset}
                      onChange={(e) => handleSettingChange('autoReset', e.target.checked)}
                    />
                    <label className="checkbox-label">{t('bulk.autoReset', {ns: 'alarms'})}</label>
                  </div>
                  <div className="checkbox-group">
                    <input
                      type="checkbox"
                      className="checkbox"
                      checked={localSettings.isEnabled}
                      onChange={(e) => handleSettingChange('isEnabled', e.target.checked)}
                    />
                    <label className="checkbox-label">{t('bulk.isEnabled', {ns: 'alarms'})}</label>
                  </div>
                </div>
              </div>
            )}

            {/* Notification Settings 탭 */}
            {activeTab === 'notifications' && (
              <div className="form-section">
                <h3>{t('settingsModal.notificationConfig', {ns: 'alarms'})}</h3>
                <div className="notification-options">
                  <div className="checkbox-options">
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.emailEnabled}
                        onChange={(e) => handleSettingChange('emailEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">{t('labels.emailNotifications', {ns: 'alarms'})}</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.smsEnabled}
                        onChange={(e) => handleSettingChange('smsEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">{t('bulk.smsEnabled', {ns: 'alarms'})}</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.soundEnabled}
                        onChange={(e) => handleSettingChange('soundEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">{t('settingsModal.soundEnabled', {ns: 'alarms'})}</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.popupEnabled}
                        onChange={(e) => handleSettingChange('popupEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">{t('settingsModal.popupEnabled', {ns: 'alarms'})}</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.webhookEnabled}
                        onChange={(e) => handleSettingChange('webhookEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">{t('settingsModal.webhookEnabled', {ns: 'alarms'})}</label>
                    </div>
                  </div>
                </div>

                {localSettings.emailEnabled && (
                  <div className="form-group">
                    <label className="form-label">{t('settingsModal.emailRecipients', {ns: 'alarms'})}</label>
                    <input
                      type="text"
                      value={localSettings.emailRecipients.join(', ')}
                      onChange={(e) => handleSettingChange('emailRecipients',
                        e.target.value.split(',').map(email => email.trim()).filter(email => email)
                      )}
                      className="form-input"
                      placeholder="Separate email addresses with commas"
                    />
                  </div>
                )}

                {localSettings.smsEnabled && (
                  <div className="form-group">
                    <label className="form-label">{t('settingsModal.smsRecipients', {ns: 'alarms'})}</label>
                    <input
                      type="text"
                      value={localSettings.smsRecipients.join(', ')}
                      onChange={(e) => handleSettingChange('smsRecipients',
                        e.target.value.split(',').map(phone => phone.trim()).filter(phone => phone)
                      )}
                      className="form-input"
                      placeholder="Separate phone numbers with commas"
                    />
                  </div>
                )}

                {localSettings.webhookEnabled && (
                  <div className="form-group">
                    <label className="form-label">{t('settingsModal.webhookUrl', {ns: 'alarms'})}</label>
                    <input
                      type="url"
                      value={localSettings.webhookUrl}
                      onChange={(e) => handleSettingChange('webhookUrl', e.target.value)}
                      className="form-input"
                      placeholder="https://hooks.slack.com/services/..."
                    />
                  </div>
                )}

                <div className="form-group">
                  <label className="form-label">{t('settingsModal.messageTemplate', {ns: 'alarms'})}</label>
                  <textarea
                    value={localSettings.messageTemplate}
                    onChange={(e) => handleSettingChange('messageTemplate', e.target.value)}
                    className="form-textarea"
                    placeholder="Alarm Message Template"
                    rows={3}
                  />
                  <small className="form-help">
                    Available variables: {'{sourceName}'}, {'{currentValue}'}, {'{threshold}'}, {'{severity}'}, {'{timestamp}'}
                  </small>
                </div>

                {localSettings.emailEnabled && (
                  <div className="form-group">
                    <label className="form-label">{t('settingsModal.emailTemplate', {ns: 'alarms'})}</label>
                    <textarea
                      value={localSettings.emailTemplate}
                      onChange={(e) => handleSettingChange('emailTemplate', e.target.value)}
                      className="form-textarea"
                      placeholder="Detailed message template for email"
                      rows={5}
                    />
                  </div>
                )}
              </div>
            )}

            {/* Schedule Settings 탭 */}
            {activeTab === 'schedule' && (
              <div className="form-section">
                <h3>{t('settingsModal.scheduleConfig', {ns: 'alarms'})}</h3>
                <div className="form-group">
                  <label className="form-label">{t('labels.executionTime', {ns: 'alarms'})}</label>
                  <select
                    value={localSettings.schedule.type}
                    onChange={(e) => handleScheduleChange({ type: e.target.value as any })}
                    className="form-select"
                  >
                    <option value="always">{t('settingsModal.always', {ns: 'alarms'})}</option>
                    <option value="business_hours">{t('settingsModal.businessHours', {ns: 'alarms'})}</option>
                    <option value="custom">{t('settingsModal.custom', {ns: 'alarms'})}</option>
                  </select>
                </div>

                {localSettings.schedule.type === 'business_hours' && (
                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">{t('columns.startTime', {ns: 'alarms'})}</label>
                      <input
                        type="time"
                        value={localSettings.schedule.startTime || '08:00'}
                        onChange={(e) => handleScheduleChange({ startTime: e.target.value })}
                        className="form-input"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">{t('settingsModal.endTime', {ns: 'alarms'})}</label>
                      <input
                        type="time"
                        value={localSettings.schedule.endTime || '18:00'}
                        onChange={(e) => handleScheduleChange({ endTime: e.target.value })}
                        className="form-input"
                      />
                    </div>
                  </div>
                )}

                {localSettings.schedule.type === 'custom' && (
                  <div className="form-group">
                    <label className="form-label">요일 선택</label>
                    <div className="weekday-selector">
                      {['월', '화', '수', '목', '금', '토', '일'].map((day, index) => (
                        <label key={index} className="weekday-label">
                          <input
                            type="checkbox"
                            checked={localSettings.schedule.weekdays?.includes(index + 1) || false}
                            onChange={(e) => {
                              const weekdays = localSettings.schedule.weekdays || [];
                              if (e.target.checked) {
                                handleScheduleChange({ weekdays: [...weekdays, index + 1] });
                              } else {
                                handleScheduleChange({ weekdays: weekdays.filter(d => d !== index + 1) });
                              }
                            }}
                          />
                          {day}
                        </label>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>
        </div>

        <div className="modal-footer">
          <button
            className="btn btn-outline"
            onClick={onClose}
            disabled={loading}
          >
            닫기
          </button>
          <button
            className="btn btn-primary"
            onClick={handleSaveAndClose}
            disabled={!hasChanges || loading}
          >
            {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-save"></i>}
            설정 저장
          </button>
        </div>
      </div>
    </div>
  );
};

export default AlarmSettingsModal;