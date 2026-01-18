import React, { useState, useEffect } from 'react';
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
          <h2 className="modal-title">알람 설정 조정: {rule.name}</h2>
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
            <h3>기본 정보 (읽기전용)</h3>
            <div className="readonly-grid">
              <div className="readonly-item">
                <label>데이터 소스</label>
                <span>{rule.data_point_name || `${rule.target_type} ${rule.id}`}</span>
              </div>
              <div className="readonly-item">
                <label>디바이스</label>
                <span>{rule.device_name || 'N/A'}</span>
              </div>
              <div className="readonly-item">
                <label>카테고리</label>
                <span>{rule.category || 'General'}</span>
              </div>
              <div className="readonly-item">
                <label>조건 타입</label>
                <span>{rule.condition_type}</span>
              </div>
              {statistics && (
                <>
                  <div className="readonly-item">
                    <label>발생 횟수</label>
                    <span>{statistics.occurrence_summary.total_occurrences}회</span>
                  </div>
                  <div className="readonly-item">
                    <label>평균 대응시간</label>
                    <span>{statistics.performance_metrics.avg_response_time_minutes?.toFixed(1) || 'N/A'}분</span>
                  </div>
                  <div className="readonly-item">
                    <label>마지막 발생</label>
                    <span>{statistics.performance_metrics.last_triggered
                      ? new Date(statistics.performance_metrics.last_triggered).toLocaleString()
                      : 'N/A'}</span>
                  </div>
                </>
              )}
              <div className="readonly-item">
                <label>최근 수정</label>
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
              <i className="fas fa-sliders-h"></i> 임계값
            </button>
            <button
              className={`tab-button ${activeTab === 'behavior' ? 'active' : ''}`}
              onClick={() => setActiveTab('behavior')}
            >
              <i className="fas fa-cogs"></i> 동작
            </button>
            <button
              className={`tab-button ${activeTab === 'notifications' ? 'active' : ''}`}
              onClick={() => setActiveTab('notifications')}
            >
              <i className="fas fa-bell"></i> 알림
            </button>
            <button
              className={`tab-button ${activeTab === 'schedule' ? 'active' : ''}`}
              onClick={() => setActiveTab('schedule')}
            >
              <i className="fas fa-clock"></i> 스케줄
            </button>
          </div>

          {/* 탭 컨텐츠 */}
          <div className="tab-content">
            {/* 임계값 설정 탭 */}
            {activeTab === 'thresholds' && (
              <div className="form-section">
                <h3>임계값 설정</h3>
                {(rule.condition_type === 'threshold' || rule.condition_type === 'range') && (
                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">매우 높음 (HH)</label>
                      <input
                        type="number"
                        value={localSettings.highHighLimit || ''}
                        onChange={(e) => handleSettingChange('highHighLimit', Number(e.target.value) || undefined)}
                        className="form-input"
                        step="0.1"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">높음 (H)</label>
                      <input
                        type="number"
                        value={localSettings.highLimit || ''}
                        onChange={(e) => handleSettingChange('highLimit', Number(e.target.value) || undefined)}
                        className="form-input"
                        step="0.1"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">낮음 (L)</label>
                      <input
                        type="number"
                        value={localSettings.lowLimit || ''}
                        onChange={(e) => handleSettingChange('lowLimit', Number(e.target.value) || undefined)}
                        className="form-input"
                        step="0.1"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">매우 낮음 (LL)</label>
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
                  <label className="form-label">히스테리시스 (Deadband)</label>
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
                    placeholder="데드밴드 값"
                  />
                  <small className="form-help">알람 chattering 방지를 위한 데드밴드 (0은 데드밴드 없음)</small>
                </div>
              </div>
            )}

            {/* 동작 설정 탭 */}
            {activeTab === 'behavior' && (
              <div className="form-section">
                <h3>우선순위 및 동작</h3>
                <div className="form-row">
                  <div className="form-group">
                    <label className="form-label">우선순위</label>
                    <select
                      value={localSettings.priority}
                      onChange={(e) => handleSettingChange('priority', e.target.value as any)}
                      className="form-select"
                    >
                      <option value="low">Low</option>
                      <option value="medium">Medium</option>
                      <option value="high">High</option>
                      <option value="critical">Critical</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label className="form-label">심각도 (1-5)</label>
                    <select
                      value={localSettings.severity}
                      onChange={(e) => handleSettingChange('severity', Number(e.target.value) as any)}
                      className="form-select"
                    >
                      <option value={1}>1 (가장 낮음)</option>
                      <option value={2}>2 (낮음)</option>
                      <option value={3}>3 (보통)</option>
                      <option value={4}>4 (높음)</option>
                      <option value={5}>5 (가장 높음)</option>
                    </select>
                  </div>
                  <div className="form-group">
                    <label className="form-label">에스컬레이션 시간 (분)</label>
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
                    <label className="form-label">중복 억제 시간 (초)</label>
                    <input
                      type="number"
                      value={localSettings.suppressDuration}
                      onChange={(e) => handleSettingChange('suppressDuration', Number(e.target.value) || 0)}
                      className="form-input"
                      min="0"
                    />
                    <small className="form-help">동일한 알람의 중복 발생을 억제할 시간</small>
                  </div>
                  <div className="form-group">
                    <label className="form-label">최대 발생 횟수</label>
                    <input
                      type="number"
                      value={localSettings.maxOccurrences}
                      onChange={(e) => handleSettingChange('maxOccurrences', Number(e.target.value) || 0)}
                      className="form-input"
                      min="0"
                    />
                    <small className="form-help">0 = 무제한</small>
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
                    <label className="checkbox-label">자동 승인</label>
                  </div>
                  <div className="checkbox-group">
                    <input
                      type="checkbox"
                      className="checkbox"
                      checked={localSettings.autoReset}
                      onChange={(e) => handleSettingChange('autoReset', e.target.checked)}
                    />
                    <label className="checkbox-label">자동 리셋</label>
                  </div>
                  <div className="checkbox-group">
                    <input
                      type="checkbox"
                      className="checkbox"
                      checked={localSettings.isEnabled}
                      onChange={(e) => handleSettingChange('isEnabled', e.target.checked)}
                    />
                    <label className="checkbox-label">알람 활성화</label>
                  </div>
                </div>
              </div>
            )}

            {/* 알림 설정 탭 */}
            {activeTab === 'notifications' && (
              <div className="form-section">
                <h3>알림 설정</h3>
                <div className="notification-options">
                  <div className="checkbox-options">
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.emailEnabled}
                        onChange={(e) => handleSettingChange('emailEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">이메일 알림</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.smsEnabled}
                        onChange={(e) => handleSettingChange('smsEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">SMS 알림</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.soundEnabled}
                        onChange={(e) => handleSettingChange('soundEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">소리 알림</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.popupEnabled}
                        onChange={(e) => handleSettingChange('popupEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">팝업 알림</label>
                    </div>
                    <div className="checkbox-group">
                      <input
                        type="checkbox"
                        className="checkbox"
                        checked={localSettings.webhookEnabled}
                        onChange={(e) => handleSettingChange('webhookEnabled', e.target.checked)}
                      />
                      <label className="checkbox-label">웹훅 알림</label>
                    </div>
                  </div>
                </div>

                {localSettings.emailEnabled && (
                  <div className="form-group">
                    <label className="form-label">이메일 수신자</label>
                    <input
                      type="text"
                      value={localSettings.emailRecipients.join(', ')}
                      onChange={(e) => handleSettingChange('emailRecipients',
                        e.target.value.split(',').map(email => email.trim()).filter(email => email)
                      )}
                      className="form-input"
                      placeholder="이메일 주소들을 쉼표로 구분"
                    />
                  </div>
                )}

                {localSettings.smsEnabled && (
                  <div className="form-group">
                    <label className="form-label">SMS 수신자</label>
                    <input
                      type="text"
                      value={localSettings.smsRecipients.join(', ')}
                      onChange={(e) => handleSettingChange('smsRecipients',
                        e.target.value.split(',').map(phone => phone.trim()).filter(phone => phone)
                      )}
                      className="form-input"
                      placeholder="전화번호들을 쉼표로 구분"
                    />
                  </div>
                )}

                {localSettings.webhookEnabled && (
                  <div className="form-group">
                    <label className="form-label">웹훅 URL</label>
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
                  <label className="form-label">알람 메시지 템플릿</label>
                  <textarea
                    value={localSettings.messageTemplate}
                    onChange={(e) => handleSettingChange('messageTemplate', e.target.value)}
                    className="form-textarea"
                    placeholder="알람 메시지 템플릿"
                    rows={3}
                  />
                  <small className="form-help">
                    사용 가능한 변수: {'{sourceName}'}, {'{currentValue}'}, {'{threshold}'}, {'{severity}'}, {'{timestamp}'}
                  </small>
                </div>

                {localSettings.emailEnabled && (
                  <div className="form-group">
                    <label className="form-label">이메일 메시지 템플릿</label>
                    <textarea
                      value={localSettings.emailTemplate}
                      onChange={(e) => handleSettingChange('emailTemplate', e.target.value)}
                      className="form-textarea"
                      placeholder="이메일용 상세 메시지 템플릿"
                      rows={5}
                    />
                  </div>
                )}
              </div>
            )}

            {/* 스케줄 설정 탭 */}
            {activeTab === 'schedule' && (
              <div className="form-section">
                <h3>스케줄 설정</h3>
                <div className="form-group">
                  <label className="form-label">실행 시간</label>
                  <select
                    value={localSettings.schedule.type}
                    onChange={(e) => handleScheduleChange({ type: e.target.value as any })}
                    className="form-select"
                  >
                    <option value="always">항상</option>
                    <option value="business_hours">업무시간</option>
                    <option value="custom">사용자 정의</option>
                  </select>
                </div>

                {localSettings.schedule.type === 'business_hours' && (
                  <div className="form-row">
                    <div className="form-group">
                      <label className="form-label">시작 시간</label>
                      <input
                        type="time"
                        value={localSettings.schedule.startTime || '08:00'}
                        onChange={(e) => handleScheduleChange({ startTime: e.target.value })}
                        className="form-input"
                      />
                    </div>
                    <div className="form-group">
                      <label className="form-label">종료 시간</label>
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