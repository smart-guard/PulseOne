// ============================================================================
// frontend/src/components/modals/AlarmBulkUpdateModal.tsx
// 여러 알람 규칙 일괄 업데이트 모달
// ============================================================================

import React, { useState } from 'react';
import { AlarmRuleSettings } from '../../api/services/alarmApi';

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
      alert('변경할 설정을 선택해주세요.');
      return;
    }
    
    const settingsToUpdate: Partial<AlarmRuleSettings> = {};
    selectedFields.forEach(field => {
      if (updateSettings[field as keyof AlarmRuleSettings] !== undefined) {
        settingsToUpdate[field as keyof AlarmRuleSettings] = updateSettings[field as keyof AlarmRuleSettings];
      }
    });
    
    onUpdate(settingsToUpdate);
  };

  return (
    <div className="modal-overlay">
      <div className="modal-container">
        <div className="modal-header">
          <h2>일괄 설정 변경</h2>
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
            <h3>선택된 알람 규칙 ({selectedRules.length}개)</h3>
            <div className="selected-rules-summary">
              {rules.slice(0, 5).map(rule => (
                <div key={rule.id} className="rule-summary-item">
                  <span className="rule-name">{rule.name}</span>
                  <span className="rule-device">{rule.device_name || 'N/A'}</span>
                </div>
              ))}
              {rules.length > 5 && (
                <div className="rule-summary-item more">
                  외 {rules.length - 5}개 규칙...
                </div>
              )}
            </div>
          </div>

          {/* 일괄 변경 설정 */}
          <div className="form-section">
            <h3>변경할 설정 선택</h3>
            <p className="form-help">
              체크한 설정만 선택된 모든 규칙에 일괄 적용됩니다.
            </p>

            {/* 우선순위 설정 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('priority')}
                  onChange={() => handleFieldToggle('priority')}
                />
                <span>우선순위</span>
              </label>
              {selectedFields.has('priority') && (
                <select
                  value={updateSettings.priority || 'medium'}
                  onChange={(e) => handleSettingChange('priority', e.target.value)}
                  className="form-select"
                >
                  <option value="low">Low</option>
                  <option value="medium">Medium</option>
                  <option value="high">High</option>
                  <option value="critical">Critical</option>
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
                <span>심각도</span>
              </label>
              {selectedFields.has('severity') && (
                <select
                  value={updateSettings.severity || 3}
                  onChange={(e) => handleSettingChange('severity', Number(e.target.value))}
                  className="form-select"
                >
                  <option value={1}>1 (가장 낮음)</option>
                  <option value={2}>2 (낮음)</option>
                  <option value={3}>3 (보통)</option>
                  <option value={4}>4 (높음)</option>
                  <option value={5}>5 (가장 높음)</option>
                </select>
              )}
            </div>

            {/* 자동 승인 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('autoAcknowledge')}
                  onChange={() => handleFieldToggle('autoAcknowledge')}
                />
                <span>자동 승인</span>
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
                    활성화
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoAcknowledge"
                      checked={updateSettings.autoAcknowledge === false}
                      onChange={() => handleSettingChange('autoAcknowledge', false)}
                    />
                    비활성화
                  </label>
                </div>
              )}
            </div>

            {/* 자동 리셋 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('autoReset')}
                  onChange={() => handleFieldToggle('autoReset')}
                />
                <span>자동 리셋</span>
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
                    활성화
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="autoReset"
                      checked={updateSettings.autoReset === false}
                      onChange={() => handleSettingChange('autoReset', false)}
                    />
                    비활성화
                  </label>
                </div>
              )}
            </div>

            {/* 이메일 알림 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('emailEnabled')}
                  onChange={() => handleFieldToggle('emailEnabled')}
                />
                <span>이메일 알림</span>
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
                    활성화
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="emailEnabled"
                      checked={updateSettings.emailEnabled === false}
                      onChange={() => handleSettingChange('emailEnabled', false)}
                    />
                    비활성화
                  </label>
                </div>
              )}
            </div>

            {/* SMS 알림 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('smsEnabled')}
                  onChange={() => handleFieldToggle('smsEnabled')}
                />
                <span>SMS 알림</span>
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
                    활성화
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="smsEnabled"
                      checked={updateSettings.smsEnabled === false}
                      onChange={() => handleSettingChange('smsEnabled', false)}
                    />
                    비활성화
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
                <span>중복 억제 시간 (초)</span>
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
                <span>에스컬레이션 시간 (분)</span>
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

            {/* 규칙 활성화/비활성화 */}
            <div className="bulk-setting-group">
              <label className="bulk-setting-label">
                <input
                  type="checkbox"
                  checked={selectedFields.has('isEnabled')}
                  onChange={() => handleFieldToggle('isEnabled')}
                />
                <span>알람 활성화</span>
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
                    활성화
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="isEnabled"
                      checked={updateSettings.isEnabled === false}
                      onChange={() => handleSettingChange('isEnabled', false)}
                    />
                    비활성화
                  </label>
                </div>
              )}
            </div>
          </div>

          {/* 적용 미리보기 */}
          {selectedFields.size > 0 && (
            <div className="form-section">
              <h3>적용될 변경사항</h3>
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
                이 변경사항이 선택된 {selectedRules.length}개 규칙에 모두 적용됩니다.
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
            취소
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

      <style jsx>{`
        .selected-rules-summary {
          max-height: 150px;
          overflow-y: auto;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1rem;
          background: #f9fafb;
        }

        .rule-summary-item {
          display: flex;
          justify-content: space-between;
          padding: 0.5rem 0;
          border-bottom: 1px solid #e5e7eb;
        }

        .rule-summary-item:last-child {
          border-bottom: none;
        }

        .rule-summary-item.more {
          color: #6b7280;
          font-style: italic;
          justify-content: center;
        }

        .rule-name {
          font-weight: 500;
          color: #374151;
        }

        .rule-device {
          color: #6b7280;
          font-size: 0.875rem;
        }

        .bulk-setting-group {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
          padding: 1rem;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          margin-bottom: 1rem;
        }

        .bulk-setting-label {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          font-weight: 500;
          color: #374151;
          cursor: pointer;
        }

        .bulk-setting-label input[type="checkbox"] {
          width: 1.125rem;
          height: 1.125rem;
        }

        .bulk-setting-options {
          display: flex;
          gap: 1rem;
          margin-left: 1.625rem;
        }

        .radio-label {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          cursor: pointer;
          color: #6b7280;
        }

        .preview-changes {
          background: #f3f4f6;
          border-radius: 0.5rem;
          padding: 1rem;
          margin-bottom: 1rem;
        }

        .change-item {
          display: flex;
          justify-content: space-between;
          padding: 0.5rem 0;
          border-bottom: 1px solid #d1d5db;
        }

        .change-item:last-child {
          border-bottom: none;
        }

        .change-field {
          font-weight: 500;
          color: #374151;
        }

        .change-value {
          color: #059669;
          font-weight: 500;
        }

        .preview-warning {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.75rem;
          background: #fef3c7;
          border: 1px solid #f59e0b;
          border-radius: 0.375rem;
          color: #92400e;
          font-size: 0.875rem;
        }
      `}</style>
    </div>
  );
};

// 헬퍼 함수들
const getFieldDisplayName = (field: string): string => {
  const fieldNames: Record<string, string> = {
    priority: '우선순위',
    severity: '심각도',
    autoAcknowledge: '자동 승인',
    autoReset: '자동 리셋',
    emailEnabled: '이메일 알림',
    smsEnabled: 'SMS 알림',
    suppressDuration: '중복 억제 시간',
    escalationTime: '에스컬레이션 시간',
    isEnabled: '알람 활성화'
  };
  return fieldNames[field] || field;
};

const formatFieldValue = (field: string, value: any): string => {
  if (value === undefined || value === null) return 'N/A';
  
  switch (field) {
    case 'priority':
      return value.toString().toUpperCase();
    case 'severity':
      return `${value} (${['', '가장 낮음', '낮음', '보통', '높음', '가장 높음'][value] || ''})`;
    case 'autoAcknowledge':
    case 'autoReset':
    case 'emailEnabled':
    case 'smsEnabled':
    case 'isEnabled':
      return value ? '활성화' : '비활성화';
    case 'suppressDuration':
      return `${value}초`;
    case 'escalationTime':
      return `${value}분`;
    default:
      return value.toString();
  }
};

export default AlarmBulkUpdateModal;