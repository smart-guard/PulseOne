// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceSettingsTab.tsx
// ⚙️ 디바이스 설정 탭 - 완전히 동작하는 토글 버튼 구현
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService } from '../../../api/services/deviceApi';
import { DeviceSettingsTabProps } from './types';

const DeviceSettingsTab: React.FC<DeviceSettingsTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField,
  onUpdateSettings
}) => {
  // ========================================================================
  // 🔥 로컬 상태로 설정값들 관리 (핵심 수정)
  // ========================================================================
  const [localSettings, setLocalSettings] = useState<any>({});
  const [expandedSections, setExpandedSections] = useState<Set<string>>(
    new Set(['communication', 'performance'])
  );

  const displayData = device || editData;
  const isReadOnly = mode === 'view';
  const isEditable = mode === 'edit' || mode === 'create';

  // ========================================================================
  // 초기화 - displayData가 변경될 때마다 로컬 상태 동기화
  // ========================================================================
  useEffect(() => {
    const settings = displayData?.settings || {};
    console.log('🔄 설정 초기화:', settings);
    setLocalSettings({
      // 기본값들
      polling_interval_ms: 1000,
      connection_timeout_ms: 5000,
      read_timeout_ms: 3000,
      write_timeout_ms: 3000,
      max_retry_count: 3,
      retry_interval_ms: 1000,
      backoff_time_ms: 2000,
      keep_alive_enabled: true,
      keep_alive_interval_s: 30,
      data_validation_enabled: true,
      performance_monitoring_enabled: true,
      detailed_logging_enabled: false,
      diagnostic_mode_enabled: false,
      communication_logging_enabled: false,
      ...settings // 실제 설정값으로 덮어쓰기
    });
  }, [displayData]);

  // ========================================================================
  // 🔥 토글 업데이트 함수 - 즉시 로컬 상태 변경
  // ========================================================================
  const updateSetting = (key: string, value: any) => {
    if (!isEditable) {
      console.warn(`❌ 읽기 전용 모드 - 설정 변경 불가: ${key}`);
      return;
    }

    console.log(`🔥 설정 변경: ${key} = ${value}`);
    
    // 1. 즉시 로컬 상태 업데이트 (화면 반영)
    setLocalSettings(prev => ({
      ...prev,
      [key]: value
    }));

    // 2. 상위 컴포넌트에도 알림
    onUpdateSettings(key, value);
  };

  const toggleSection = (sectionId: string) => {
    setExpandedSections(prev => {
      const newSet = new Set(prev);
      if (newSet.has(sectionId)) {
        newSet.delete(sectionId);
      } else {
        newSet.add(sectionId);
      }
      return newSet;
    });
  };

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================
  const renderSectionHeader = (id: string, title: string, icon: string) => (
    <div className="section-header" onClick={() => toggleSection(id)}>
      <div className="section-title">
        <i className={`fas ${icon}`}></i>
        <h3>{title}</h3>
      </div>
      <i className={`fas ${expandedSections.has(id) ? 'fa-chevron-up' : 'fa-chevron-down'}`}></i>
    </div>
  );

  const renderToggleField = (label: string, key: string, description?: string) => {
    const currentValue = localSettings[key] || false;
    
    return (
      <div className="form-group">
        <div className="toggle-header">
          <label>{label}</label>
          {isReadOnly ? (
            <span className={`status-badge ${currentValue ? 'enabled' : 'disabled'}`}>
              {currentValue ? '활성화' : '비활성화'}
            </span>
          ) : (
            <label className="switch">
              <input
                type="checkbox"
                checked={currentValue}
                onChange={(e) => {
                  console.log(`🎯 토글 클릭: ${key} = ${e.target.checked}`);
                  updateSetting(key, e.target.checked);
                }}
              />
              <span className="slider"></span>
            </label>
          )}
        </div>
        {description && <div className="form-description">{description}</div>}
      </div>
    );
  };

  const renderNumberField = (label: string, key: string, unit: string, min?: number, max?: number) => {
    const currentValue = localSettings[key] || 0;
    
    return (
      <div className="form-group">
        <label>{label}</label>
        {isReadOnly ? (
          <div className="form-value">{currentValue.toLocaleString()} {unit}</div>
        ) : (
          <div className="input-with-unit">
            <input
              type="number"
              value={currentValue}
              onChange={(e) => updateSetting(key, parseInt(e.target.value) || 0)}
              min={min}
              max={max}
            />
            <span className="unit">{unit}</span>
          </div>
        )}
      </div>
    );
  };

  // ========================================================================
  // 메인 렌더링
  // ========================================================================
  return (
    <div className="tab-panel">
      <div className="settings-container">
        
        {/* 헤더 */}
        <div className="settings-header">
          <div className="header-left">
            <h2>⚙️ 디바이스 설정</h2>
            <div className="mode-indicator">
              <span className={`mode-badge ${mode}`}>
                {mode === 'view' ? '보기 모드' : mode === 'edit' ? '편집 모드' : '생성 모드'}
              </span>
            </div>
          </div>
          {isEditable && (
            <button 
              className="btn btn-secondary btn-sm" 
              onClick={() => {
                const defaultSettings = {
                  polling_interval_ms: 1000,
                  connection_timeout_ms: 5000,
                  read_timeout_ms: 3000,
                  write_timeout_ms: 3000,
                  max_retry_count: 3,
                  retry_interval_ms: 1000,
                  backoff_time_ms: 2000,
                  keep_alive_enabled: true,
                  keep_alive_interval_s: 30,
                  data_validation_enabled: true,
                  performance_monitoring_enabled: true,
                  detailed_logging_enabled: false,
                  diagnostic_mode_enabled: false,
                  communication_logging_enabled: false
                };
                setLocalSettings(defaultSettings);
                Object.entries(defaultSettings).forEach(([key, value]) => {
                  onUpdateSettings(key, value);
                });
                alert('기본값으로 초기화되었습니다.');
              }}
            >
              <i className="fas fa-undo"></i>
              기본값 복원
            </button>
          )}
        </div>

        {/* 설정 섹션들 */}
        <div className="settings-grid">
          
          {/* 왼쪽 열 - 통신 설정 */}
          <div className="settings-column">
            <div className="settings-section">
              {renderSectionHeader('communication', '통신 설정', 'fa-wifi')}
              
              {expandedSections.has('communication') && (
                <div className="section-content">
                  <div className="form-row">
                    {renderNumberField('폴링 간격', 'polling_interval_ms', 'ms', 100, 60000)}
                    {renderNumberField('연결 타임아웃', 'connection_timeout_ms', 'ms', 1000, 30000)}
                  </div>
                  
                  <div className="form-row">
                    {renderNumberField('읽기 타임아웃', 'read_timeout_ms', 'ms', 1000, 10000)}
                    {renderNumberField('쓰기 타임아웃', 'write_timeout_ms', 'ms', 1000, 10000)}
                  </div>

                  <div className="form-row">
                    {renderNumberField('최대 재시도', 'max_retry_count', '회', 0, 10)}
                    {renderNumberField('재시도 간격', 'retry_interval_ms', 'ms', 500, 10000)}
                  </div>

                  {renderToggleField(
                    'Keep-Alive 활성화',
                    'keep_alive_enabled',
                    '연결 유지를 위한 주기적 통신 활성화'
                  )}

                  {localSettings.keep_alive_enabled && (
                    <div className="form-row">
                      {renderNumberField('Keep-Alive 간격', 'keep_alive_interval_s', '초', 10, 300)}
                    </div>
                  )}
                </div>
              )}
            </div>
          </div>

          {/* 오른쪽 열 - 성능 및 고급 설정 */}
          <div className="settings-column">
            
            {/* 성능 설정 */}
            <div className="settings-section">
              {renderSectionHeader('performance', '성능 및 최적화', 'fa-tachometer-alt')}
              
              {expandedSections.has('performance') && (
                <div className="section-content">
                  {renderToggleField(
                    '데이터 검증',
                    'data_validation_enabled',
                    '수신된 데이터의 유효성 검사 수행'
                  )}

                  {renderToggleField(
                    '성능 모니터링',
                    'performance_monitoring_enabled',
                    '응답시간, 처리량 등 성능 지표 수집'
                  )}
                </div>
              )}
            </div>

            {/* 고급 설정 */}
            <div className="settings-section">
              {renderSectionHeader('advanced', '고급 설정', 'fa-cogs')}
              
              {expandedSections.has('advanced') && (
                <div className="section-content">
                  {renderToggleField(
                    '상세 로깅',
                    'detailed_logging_enabled',
                    '디버깅을 위한 상세한 로그 기록 (성능에 영향을 줄 수 있음)'
                  )}

                  {renderToggleField(
                    '진단 모드',
                    'diagnostic_mode_enabled',
                    '문제 해결을 위한 진단 정보 수집'
                  )}

                  {renderToggleField(
                    '통신 로깅',
                    'communication_logging_enabled',
                    '모든 통신 내용을 로그로 기록'
                  )}
                </div>
              )}
            </div>
          </div>
        </div>

        {/* 🔥 디버깅 패널 - 현재 설정값 실시간 표시 */}
        {process.env.NODE_ENV === 'development' && (
          <div className="debug-panel">
            <h4>🐛 실시간 설정값 (개발용)</h4>
            <pre>{JSON.stringify(localSettings, null, 2)}</pre>
          </div>
        )}
      </div>

      {/* 스타일 */}
      <style jsx>{`
        .tab-panel {
          flex: 1;
          padding: 1.5rem;
          overflow-y: auto;
          background: #f8fafc;
        }

        .settings-container {
          display: flex;
          flex-direction: column;
          gap: 1.5rem;
        }

        .settings-grid {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 1.5rem;
          align-items: start;
        }

        @media (max-width: 1024px) {
          .settings-grid {
            grid-template-columns: 1fr;
          }
        }

        .settings-header {
          display: flex;
          justify-content: space-between;
          align-items: flex-start;
          margin-bottom: 1rem;
        }

        .header-left {
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .settings-header h2 {
          margin: 0;
          font-size: 1.25rem;
          font-weight: 600;
          color: #1f2937;
        }

        .mode-indicator {
          margin-top: 0.5rem;
        }

        .mode-badge {
          font-size: 0.75rem;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-weight: 500;
        }

        .mode-badge.view {
          background: #e5e7eb;
          color: #374151;
        }

        .mode-badge.edit {
          background: #dbeafe;
          color: #1d4ed8;
        }

        .mode-badge.create {
          background: #dcfce7;
          color: #166534;
        }

        .settings-column {
          display: flex;
          flex-direction: column;
          gap: 1.5rem;
        }

        .settings-section {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          overflow: hidden;
        }

        .section-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1rem 1.5rem;
          background: #f9fafb;
          border-bottom: 1px solid #e5e7eb;
          cursor: pointer;
          transition: background-color 0.2s ease;
        }

        .section-header:hover {
          background: #f3f4f6;
        }

        .section-title {
          display: flex;
          align-items: center;
          gap: 0.75rem;
          flex: 1;
        }

        .section-title i {
          color: #6b7280;
          width: 1.25rem;
        }

        .section-title h3 {
          margin: 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .section-content {
          padding: 1.5rem;
          display: flex;
          flex-direction: column;
          gap: 1.5rem;
        }

        .form-row {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
          gap: 1rem;
        }

        .form-group {
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .form-group label {
          font-size: 0.875rem;
          font-weight: 500;
          color: #374151;
        }

        .form-group input {
          padding: 0.5rem 0.75rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          transition: border-color 0.2s ease;
        }

        .form-group input:focus {
          outline: none;
          border-color: #0ea5e9;
          box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
        }

        .form-value {
          padding: 0.5rem 0.75rem;
          background: #f9fafb;
          border: 1px solid #e5e7eb;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          color: #374151;
        }

        .form-description {
          font-size: 0.75rem;
          color: #6b7280;
          line-height: 1.4;
        }

        .input-with-unit {
          display: flex;
          align-items: center;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          overflow: hidden;
        }

        .input-with-unit input {
          flex: 1;
          border: none;
          outline: none;
          padding: 0.5rem 0.75rem;
        }

        .input-with-unit .unit {
          padding: 0.5rem 0.75rem;
          background: #f3f4f6;
          border-left: 1px solid #d1d5db;
          font-size: 0.75rem;
          font-weight: 500;
          color: #6b7280;
        }

        .toggle-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
        }

        .status-badge {
          display: inline-flex;
          align-items: center;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .status-badge.enabled {
          background: #dcfce7;
          color: #166534;
        }

        .status-badge.disabled {
          background: #fee2e2;
          color: #991b1b;
        }

        /* 🔥 토글 스위치 스타일 */
        .switch {
          position: relative;
          display: inline-block;
          width: 3rem;
          height: 1.5rem;
        }

        .switch input {
          opacity: 0;
          width: 0;
          height: 0;
        }

        .slider {
          position: absolute;
          cursor: pointer;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: #cbd5e1;
          transition: 0.2s;
          border-radius: 1.5rem;
        }

        .slider:before {
          position: absolute;
          content: "";
          height: 1.125rem;
          width: 1.125rem;
          left: 0.1875rem;
          bottom: 0.1875rem;
          background: white;
          transition: 0.2s;
          border-radius: 50%;
        }

        input:checked + .slider {
          background: #0ea5e9;
        }

        input:checked + .slider:before {
          transform: translateX(1.5rem);
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
        }

        .btn-sm {
          padding: 0.375rem 0.75rem;
          font-size: 0.75rem;
        }

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-secondary:hover {
          background: #475569;
        }

        /* 🔥 디버깅 패널 */
        .debug-panel {
          background: #1f2937;
          color: #f3f4f6;
          padding: 1rem;
          border-radius: 0.5rem;
          margin-top: 1rem;
        }

        .debug-panel h4 {
          margin: 0 0 0.5rem 0;
          color: #fbbf24;
        }

        .debug-panel pre {
          font-size: 0.75rem;
          margin: 0;
          max-height: 200px;
          overflow-y: auto;
          background: #111827;
          padding: 0.5rem;
          border-radius: 0.25rem;
        }
      `}</style>
    </div>
  );
};

export default DeviceSettingsTab;