// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceSettingsTab.tsx
// ⚙️ 디바이스 설정 탭 - 3 Column Dense Layout
// ============================================================================

import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { DeviceSettingsTabProps } from './types';
import '../../../styles/management.css';

const DeviceSettingsTab: React.FC<DeviceSettingsTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField,
  onUpdateSettings
}) => {
  const { t } = useTranslation(['devices', 'common']);
  // ========================================================================
  // 로컬 상태로 설정값들 관리
  // ========================================================================
  const [localSettings, setLocalSettings] = useState<any>({});

  const displayData = device || editData;
  const isReadOnly = mode === 'view';
  const isEditable = mode === 'edit' || mode === 'create';

  // ========================================================================
  // 초기화 - 모든 파라미터 포함
  // ========================================================================
  useEffect(() => {
    const settings = displayData?.settings || {};
    setLocalSettings({
      // 기본값들
      polling_interval_ms: 1000,
      connection_timeout_ms: 5000,
      read_timeout_ms: 3000,
      write_timeout_ms: 3000,
      max_retry_count: 3,
      retry_interval_ms: 3000,
      backoff_time_ms: 30000,
      backoff_multiplier: 1.5,
      max_backoff_time_ms: 300000,
      is_keep_alive_enabled: true,
      keep_alive_interval_s: 30,
      keep_alive_timeout_s: 10,
      read_buffer_size: 1024,
      write_buffer_size: 1024,
      queue_size: 100,
      is_data_validation_enabled: true,
      is_performance_monitoring_enabled: false,
      is_detailed_logging_enabled: false,
      is_diagnostic_mode_enabled: false,
      is_communication_logging_enabled: false,
      is_outlier_detection_enabled: false,
      is_deadband_enabled: true,
      ...settings // 실제 설정값으로 덮어쓰기
    });
  }, [displayData]);

  // ========================================================================
  // 토글 업데이트
  // ========================================================================
  const updateSetting = (key: string, value: any) => {
    if (!isEditable) return;

    setLocalSettings((prev: any) => ({
      ...prev,
      [key]: value
    }));

    onUpdateSettings(key, value);
  };

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================

  const renderNumberField = (label: string, key: string, unit: string, min?: number, max?: number, step?: number) => {
    const currentValue = localSettings[key] ?? 0;
    return (
      <div className="st-field">
        <label>{label}</label>
        {isReadOnly ? (
          <div className="form-val">{typeof currentValue === 'number' ? currentValue.toLocaleString() : currentValue} {unit}</div>
        ) : (
          <div className="input-group">
            <input
              type="number"
              className="st-input"
              value={currentValue ?? ''}
              onChange={(e) => {
                const val = e.target.value;
                updateSetting(key, val === '' ? '' : parseFloat(val));
              }}
              min={min}
              max={max}
              step={step || 1}
            />
            <span className="unit-label">{unit}</span>
          </div>
        )}
      </div>
    );
  };

  const renderToggleField = (label: string, key: string, description?: string) => {
    const currentValue = localSettings[key] || false;
    return (
      <div className="st-field-toggle">
        <div className="toggle-header">
          <label>{label}</label>
          {isReadOnly ? (
            <span className={`status-badge-left ${currentValue ? 'enabled' : 'disabled'}`}>
              {currentValue ? t('devices:status.enabled') : t('devices:status.disabled')}
            </span>
          ) : (
            <label className="switch">
              <input
                type="checkbox"
                checked={currentValue}
                onChange={(e) => updateSetting(key, e.target.checked)}
              />
              <span className="slider"></span>
            </label>
          )}
        </div>
        {description && <div className="hint-text">{description}</div>}
      </div>
    );
  };

  // ========================================================================
  // 메인 렌더링
  // ========================================================================
  return (
    <div className="st-container">
      {/* 상단 툴바 */}
      <div className="st-toolbar">
        <div className="mode-tag">
          {mode === 'view' ? 'READ ONLY' : mode === 'edit' ? 'EDIT MODE' : 'CREATE MODE'}
        </div>
        {isEditable && (
          <button
            className="btn-reset"
            onClick={() => {
              const defaultSettings = {
                polling_interval_ms: 1000,
                connection_timeout_ms: 5000,
                read_timeout_ms: 3000,
                write_timeout_ms: 3000,
                max_retry_count: 3,
                retry_interval_ms: 3000,
                backoff_time_ms: 30000,
                backoff_multiplier: 1.5,
                max_backoff_time_ms: 300000,
                is_keep_alive_enabled: true,
                keep_alive_interval_s: 30,
                keep_alive_timeout_s: 10,
                read_buffer_size: 1024,
                write_buffer_size: 1024,
                queue_size: 100,
                is_data_validation_enabled: true,
                is_performance_monitoring_enabled: false,
                is_detailed_logging_enabled: false,
                is_diagnostic_mode_enabled: false,
                is_communication_logging_enabled: false,
                is_outlier_detection_enabled: false,
                is_deadband_enabled: true,
                is_auto_registration_enabled: false,
              };
              setLocalSettings(defaultSettings);
              Object.entries(defaultSettings).forEach(([key, value]) => {
                onUpdateSettings(key, value);
              });
              alert('Reset complete.');
            }}
          >
            <i className="fas fa-undo"></i> {t('devices:settings.reset')}
          </button>
        )}
      </div>

      {/* 3단 그리드 레이아웃 */}
      <div className="st-grid-row">

        {/* 1. 기본 운영 파라미터 */}
        <div className="st-card">
          <h3><i className="fas fa-wifi" style={{ color: '#3b82f6' }}></i> {t('devices:settings.operationParams')}</h3>
          <div className="st-form-stack">
            <div className="st-row">
              {renderNumberField(t('devices:settings.pollingInterval'), 'polling_interval_ms', 'ms', 10, 60000)}
              {renderNumberField(t('devices:settings.connectionTimeout'), 'connection_timeout_ms', 'ms', 100, 30000)}
            </div>
            <div className="st-row">
              {renderNumberField(t('devices:settings.readTimeout'), 'read_timeout_ms', 'ms', 100, 10000)}
              {renderNumberField(t('devices:settings.writeTimeout'), 'write_timeout_ms', 'ms', 100, 10000)}
            </div>
            <div className="st-row">
              {renderNumberField(t('devices:settings.maxRetry'), 'max_retry_count', t('devices:settings.times'), 0, 10)}
              {renderNumberField(t('devices:settings.retryInterval'), 'retry_interval_ms', 'ms', 100, 10000)}
            </div>

            <div className="divider"></div>

            {renderToggleField(t('devices:settings.keepAlive'), 'is_keep_alive_enabled', t('devices:settings.keepAliveDesc'))}
            {localSettings.is_keep_alive_enabled && (
              <div className="st-row">
                {renderNumberField(t('devices:settings.keepAliveInterval'), 'keep_alive_interval_s', t('devices:settings.sec'), 10, 300)}
                {renderNumberField(t('devices:settings.keepAliveTimeout'), 'keep_alive_timeout_s', t('devices:settings.sec'), 1, 60)}
              </div>
            )}
          </div>
        </div>

        {/* 2. 에러 제어 및 백오프 */}
        <div className="st-card">
          <h3><i className="fas fa-undo" style={{ color: '#f59e0b' }}></i> {t('devices:settings.errorControl')}</h3>
          <div className="st-form-stack">
            {renderNumberField(t('devices:settings.backoffInitial'), 'backoff_time_ms', 'ms', 100, 600000)}
            {renderNumberField(t('devices:settings.backoffMax'), 'max_backoff_time_ms', 'ms', 1000, 3600000)}
            {renderNumberField(t('devices:settings.backoffMultiplier'), 'backoff_multiplier', 'x', 1.0, 5.0, 0.1)}

            <div className="info-box">
              <i className="fas fa-info-circle"></i>
              <span>When consecutive errors occur, retry intervals are increased exponentially to reduce system load.</span>
            </div>
          </div>
        </div>

        {/* 3. 리소스 및 데이터 안전 */}
        <div className="st-card">
          <h3><i className="fas fa-microchip" style={{ color: '#10b981' }}></i> {t('devices:settings.resourceAndSafety')}</h3>
          <div className="st-form-stack">
            <div className="st-row">
              {renderNumberField(t('devices:settings.readBuffer'), 'read_buffer_size', 'B', 64, 8192)}
              {renderNumberField(t('devices:settings.writeBuffer'), 'write_buffer_size', 'B', 64, 8192)}
            </div>
            {renderNumberField(t('devices:settings.eventQueueSize'), 'queue_size', t('devices:settings.items'), 10, 1000)}

            <div className="divider"></div>

            {renderToggleField(t('devices:settings.dataValidation'), 'is_data_validation_enabled', t('devices:settings.dataValidationDesc'))}
            {renderToggleField(t('devices:settings.outlierDetection'), 'is_outlier_detection_enabled', t('devices:settings.outlierDetectionDesc'))}
            {renderToggleField(t('devices:settings.deadbandFilter'), 'is_deadband_enabled', t('devices:settings.deadbandFilterDesc'))}
          </div>
        </div>

      </div>

      {/* 하단 전체 너비 섹션: 로깅 및 진단 */}
      <div className="st-grid-row" style={{ marginTop: '4px' }}>
        <div className="st-card" style={{ gridColumn: 'span 3' }}>
          <div className="st-row" style={{ gap: '24px', marginBottom: '8px' }}>
            <div style={{ flex: 1 }}>
              {(editData?.protocol_type === 'MQTT' || device?.protocol_type === 'MQTT') &&
                renderToggleField(t('devices:settings.mqttAutoDiscovery'), 'is_auto_registration_enabled', t('devices:settings.mqttAutoDiscoveryDesc'))
              }
            </div>
            <div style={{ flex: 1 }}></div>
            <div style={{ flex: 1 }}></div>
          </div>
          <div className="divider" style={{ marginBottom: '12px' }}></div>
          <div className="st-row" style={{ gap: '24px' }}>
            <div style={{ flex: 1 }}>{renderToggleField(t('devices:settings.perfMonitoring'), 'is_performance_monitoring_enabled', t('devices:settings.perfMonitoringDesc'))}</div>
            <div style={{ flex: 1 }}>{renderToggleField(t('devices:settings.commLogging'), 'is_communication_logging_enabled', t('devices:settings.commLoggingDesc'))}</div>
            <div style={{ flex: 1 }}>{renderToggleField(t('devices:settings.diagnosticMode'), 'is_diagnostic_mode_enabled', t('devices:settings.diagnosticModeDesc'))}</div>
          </div>
          <div className="warning-box">
            <i className="fas fa-exclamation-triangle"></i>
            <span>Diagnostic and packet logging can significantly increase storage space and CPU usage. Use with caution.</span>
          </div>
        </div>
      </div>

      <style>{`
        .st-container {
          display: flex;
          flex-direction: column;
          gap: 12px;
          height: 100%;
          overflow-y: auto;
          box-sizing: border-box;
          padding: 8px;
          background: #f8fafc;
        }

        .st-toolbar {
           display: flex;
           justify-content: space-between;
           align-items: center;
           margin-bottom: 4px;
        }
        
        .mode-tag {
           font-size: 11px;
           padding: 2px 6px;
           border-radius: 4px;
           background: #e2e8f0;
           color: #475569;
           font-weight: 600;
        }

        .btn-reset {
           background: white;
           border: 1px solid #cbd5e1;
           padding: 4px 8px;
           border-radius: 4px;
           font-size: 11px;
           cursor: pointer;
           color: #475569;
           display: flex;
           align-items: center;
           gap: 4px;
        }
        .btn-reset:hover { background: #f1f5f9; }

        .st-grid-row {
           display: grid;
           grid-template-columns: repeat(3, 1fr);
           gap: 12px;
           flex-shrink: 0;
        }

        .st-card {
           background: white;
           border: 1px solid #e2e8f0;
           border-radius: 8px;
           padding: 16px;
           box-shadow: 0 1px 2px rgba(0,0,0,0.05);
           display: flex;
           flex-direction: column;
           gap: 12px;
        }

        .st-card h3 {
           margin: 0;
           font-size: 13px;
           font-weight: 600;
           color: #1e293b;
           padding-bottom: 8px;
           border-bottom: 1px solid #f1f5f9;
           display: flex;
           align-items: center;
           gap: 8px;
        }

        .st-form-stack {
           display: flex;
           flex-direction: column;
           gap: 12px;
        }

        .st-row {
           display: flex;
           gap: 8px;
        }
        .st-row > * { flex: 1; }

        .st-field {
           display: flex;
           flex-direction: column;
           gap: 4px;
        }
        
        .st-field label, .toggle-header label {
           font-size: 11px;
           font-weight: 500;
           color: #64748b;
        }

        .input-group {
           display: flex;
           align-items: center;
           gap: 6px;
           border: 1px solid #cbd5e1;
           border-radius: 4px;
           padding: 0 8px;
           height: 32px;
           background: white;
        }
        .st-input {
           border: none;
           flex: 1;
           font-size: 13px;
           color: #1e293b;
           min-width: 0;
           outline: none;
        }
        .unit-label {
           font-size: 11px;
           color: #94a3b8;
           white-space: nowrap;
        }

        .form-val {
           font-size: 13px;
           color: #334155;
           font-weight: 600;
           min-height: 1.2em;
        }

        .divider {
           height: 1px;
           background: #f1f5f9;
           margin: 4px 0;
        }

        .st-field-toggle {
           display: flex;
           flex-direction: column;
           gap: 2px;
           padding: 4px 0;
        }
        .toggle-header {
           display: flex;
           justify-content: space-between;
           align-items: center;
           gap: 12px;
        }

        .hint-text {
           font-size: 10px;
           color: #94a3b8;
        }

        /* Status Badge Override for Left Alignment */
        .status-badge-left {
           display: inline-flex;
           align-items: center;
           padding: 2px 8px;
           border-radius: 999px;
           font-size: 10px;
           font-weight: 600;
        }
        .status-badge-left.enabled { background: #dcfce7; color: #166534; }
        .status-badge-left.disabled { background: #f1f5f9; color: #64748b; }

        .info-box {
           background: #eff6ff;
           border: 1px solid #dbeafe;
           border-radius: 4px;
           padding: 8px;
           display: flex;
           gap: 6px;
           font-size: 10px;
           color: #1e40af;
           align-items: flex-start;
           line-height: 1.4;
        }
        .warning-box {
           background: #fff7ed;
           border: 1px solid #ffedd5;
           border-radius: 4px;
           padding: 8px;
           display: flex;
           gap: 6px;
           font-size: 10px;
           color: #9a3412;
           align-items: flex-start;
           line-height: 1.4;
        }

        @media (max-width: 1200px) {
           .st-grid-row {
              grid-template-columns: 1fr;
           }
        }
      `}</style>
    </div>
  );
};


export default DeviceSettingsTab;