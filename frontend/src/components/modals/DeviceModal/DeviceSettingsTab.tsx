// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceSettingsTab.tsx
// ⚙️ 디바이스 설정 탭 - 3 Column Dense Layout
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceSettingsTabProps } from './types';
import '../../../styles/management.css';

const DeviceSettingsTab: React.FC<DeviceSettingsTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField,
  onUpdateSettings
}) => {
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
              value={currentValue}
              onChange={(e) => updateSetting(key, parseFloat(e.target.value) || 0)}
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
              {currentValue ? '활성화' : '비활성화'}
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
              };
              setLocalSettings(defaultSettings);
              Object.entries(defaultSettings).forEach(([key, value]) => {
                onUpdateSettings(key, value);
              });
              alert('초기화 완료');
            }}
          >
            <i className="fas fa-undo"></i> 기본값
          </button>
        )}
      </div>

      {/* 3단 그리드 레이아웃 */}
      <div className="st-grid-row">

        {/* 1. 기본 운영 파라미터 */}
        <div className="st-card">
          <h3><i className="fas fa-wifi" style={{ color: '#3b82f6' }}></i> 기본 운영 파라미터</h3>
          <div className="st-form-stack">
            <div className="st-row">
              {renderNumberField('폴링 간격', 'polling_interval_ms', 'ms', 10, 60000)}
              {renderNumberField('연결 타임아웃', 'connection_timeout_ms', 'ms', 100, 30000)}
            </div>
            <div className="st-row">
              {renderNumberField('읽기 타임아웃', 'read_timeout_ms', 'ms', 100, 10000)}
              {renderNumberField('쓰기 타임아웃', 'write_timeout_ms', 'ms', 100, 10000)}
            </div>
            <div className="st-row">
              {renderNumberField('최대 재시도', 'max_retry_count', '회', 0, 10)}
              {renderNumberField('재시도 간격', 'retry_interval_ms', 'ms', 100, 10000)}
            </div>

            <div className="divider"></div>

            {renderToggleField('Keep-Alive 세션 유지', 'is_keep_alive_enabled', '접속 세션 지속 유지 활성화')}
            {localSettings.is_keep_alive_enabled && (
              <div className="st-row">
                {renderNumberField('KA 간격', 'keep_alive_interval_s', '초', 10, 300)}
                {renderNumberField('KA 타임아웃', 'keep_alive_timeout_s', '초', 1, 60)}
              </div>
            )}
          </div>
        </div>

        {/* 2. 에러 제어 및 백오프 */}
        <div className="st-card">
          <h3><i className="fas fa-undo" style={{ color: '#f59e0b' }}></i> 에러 제어 및 지수 백오프</h3>
          <div className="st-form-stack">
            {renderNumberField('초기 백오프 시간', 'backoff_time_ms', 'ms', 100, 600000)}
            {renderNumberField('최대 백오프 시간', 'max_backoff_time_ms', 'ms', 1000, 3600000)}
            {renderNumberField('지수 증폭 배율', 'backoff_multiplier', '배', 1.0, 5.0, 0.1)}

            <div className="info-box">
              <i className="fas fa-info-circle"></i>
              <span>연속된 에러 발생 시 재시도 간격을 지수적으로 늘려 시스템 부하를 방지합니다.</span>
            </div>
          </div>
        </div>

        {/* 3. 리소스 및 데이터 안전 */}
        <div className="st-card">
          <h3><i className="fas fa-microchip" style={{ color: '#10b981' }}></i> 리소스 및 데이터 안전</h3>
          <div className="st-form-stack">
            <div className="st-row">
              {renderNumberField('읽기 버퍼', 'read_buffer_size', 'B', 64, 8192)}
              {renderNumberField('쓰기 버퍼', 'write_buffer_size', 'B', 64, 8192)}
            </div>
            {renderNumberField('이벤트 큐 사이즈', 'queue_size', '개', 10, 1000)}

            <div className="divider"></div>

            {renderToggleField('데이터 유효성 검증', 'is_data_validation_enabled', '수신 데이터 패킷 구조 검사')}
            {renderToggleField('이상치 탐지', 'is_outlier_detection_enabled', '범위 밖 데이터 필터링')}
            {renderToggleField('데드밴드 필터', 'is_deadband_enabled', '미세 변화 무시 (Deadband)')}
          </div>
        </div>

      </div>

      {/* 하단 전체 너비 섹션: 로깅 및 진단 */}
      <div className="st-grid-row" style={{ marginTop: '4px' }}>
        <div className="st-card" style={{ gridColumn: 'span 3' }}>
          <h3><i className="fas fa-shield-alt" style={{ color: '#64748b' }}></i> 로깅 및 진단 시스템</h3>
          <div className="st-row" style={{ gap: '24px' }}>
            <div style={{ flex: 1 }}>{renderToggleField('성능 모니터링', 'is_performance_monitoring_enabled', '응답시간 및 처리량 통계 수집')}</div>
            <div style={{ flex: 1 }}>{renderToggleField('통신 패킷 로깅', 'is_communication_logging_enabled', 'TX/RX 원시 패킷 기록 (디버깅용)')}</div>
            <div style={{ flex: 1 }}>{renderToggleField('정밀 진단 모드', 'is_diagnostic_mode_enabled', '문제 해결을 위한 상세 내부 로그')}</div>
          </div>
          <div className="warning-box">
            <i className="fas fa-exclamation-triangle"></i>
            <span>진단 및 패킷 로깅은 저장 공간과 CPU 사용량을 크게 높일 수 있으므로 주의바랍니다.</span>
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