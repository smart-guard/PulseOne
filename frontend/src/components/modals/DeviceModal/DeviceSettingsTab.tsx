// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceSettingsTab.tsx
// ⚙️ 디바이스 설정 탭 컴포넌트 - 완전 구현
// ============================================================================

import React, { useState } from 'react';
import { DeviceSettingsTabProps } from './types';

const DeviceSettingsTab: React.FC<DeviceSettingsTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField,
  onUpdateSettings
}) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [expandedSections, setExpandedSections] = useState<Set<string>>(
    new Set(['communication', 'performance'])
  );

  const displayData = device || editData;
  const settings = displayData?.settings || {};

  // ========================================================================
  // 헬퍼 함수들
  // ========================================================================

  /**
   * 섹션 펼치기/접기
   */
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

  /**
   * 설정값 안전하게 가져오기
   */
  const getSettingValue = (key: string, defaultValue: any) => {
    return settings[key] !== undefined ? settings[key] : defaultValue;
  };

  /**
   * 설정값 업데이트
   */
  const updateSettingValue = (key: string, value: any) => {
    if (mode !== 'view') {
      onUpdateSettings(key, value);
    }
  };

  /**
   * 기본값으로 초기화
   */
  const resetToDefaults = () => {
    if (mode === 'view') return;

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
      diagnostic_mode_enabled: false
    };

    Object.entries(defaultSettings).forEach(([key, value]) => {
      onUpdateSettings(key, value);
    });

    alert('기본값으로 초기화되었습니다.');
  };

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================

  /**
   * 섹션 헤더 렌더링
   */
  const renderSectionHeader = (id: string, title: string, icon: string) => (
    <div 
      className="section-header"
      onClick={() => toggleSection(id)}
    >
      <div className="section-title">
        <i className={`fas ${icon}`}></i>
        <h3>{title}</h3>
      </div>
      <i className={`fas ${expandedSections.has(id) ? 'fa-chevron-up' : 'fa-chevron-down'}`}></i>
    </div>
  );

  /**
   * 숫자 입력 필드 렌더링
   */
  const renderNumberField = (
    label: string, 
    key: string, 
    defaultValue: number, 
    unit: string,
    min?: number,
    max?: number,
    step?: number
  ) => (
    <div className="form-group">
      <label>{label}</label>
      {mode === 'view' ? (
        <div className="form-value">
          {getSettingValue(key, defaultValue).toLocaleString()} {unit}
        </div>
      ) : (
        <div className="input-with-unit">
          <input
            type="number"
            value={getSettingValue(key, defaultValue)}
            onChange={(e) => updateSettingValue(key, parseInt(e.target.value))}
            min={min}
            max={max}
            step={step}
          />
          <span className="unit">{unit}</span>
        </div>
      )}
    </div>
  );

  /**
   * 토글 스위치 렌더링
   */
  const renderToggleField = (label: string, key: string, defaultValue: boolean, description?: string) => (
    <div className="form-group">
      <div className="toggle-header">
        <label>{label}</label>
        {mode === 'view' ? (
          <span className={`status-badge ${getSettingValue(key, defaultValue) ? 'enabled' : 'disabled'}`}>
            {getSettingValue(key, defaultValue) ? '활성화' : '비활성화'}
          </span>
        ) : (
          <label className="switch">
            <input
              type="checkbox"
              checked={getSettingValue(key, defaultValue)}
              onChange={(e) => updateSettingValue(key, e.target.checked)}
            />
            <span className="slider"></span>
          </label>
        )}
      </div>
      {description && <div className="form-description">{description}</div>}
    </div>
  );

  // ========================================================================
  // 메인 렌더링
  // ========================================================================

  return (
    <div className="tab-panel">
      <div className="settings-container">
        
        {/* 헤더 */}
        <div className="settings-header">
          <h2>⚙️ 디바이스 설정</h2>
          {mode !== 'view' && (
            <button className="btn btn-secondary btn-sm" onClick={resetToDefaults}>
              <i className="fas fa-undo"></i>
              기본값 복원
            </button>
          )}
        </div>

        {/* 통신 설정 섹션 */}
        <div className="settings-section">
          {renderSectionHeader('communication', '통신 설정', 'fa-wifi')}
          
          {expandedSections.has('communication') && (
            <div className="section-content">
              <div className="form-row">
                {renderNumberField('폴링 간격', 'polling_interval_ms', 1000, 'ms', 100, 60000, 100)}
                {renderNumberField('연결 타임아웃', 'connection_timeout_ms', 5000, 'ms', 1000, 30000, 1000)}
              </div>
              
              <div className="form-row">
                {renderNumberField('읽기 타임아웃', 'read_timeout_ms', 3000, 'ms', 1000, 10000, 500)}
                {renderNumberField('쓰기 타임아웃', 'write_timeout_ms', 3000, 'ms', 1000, 10000, 500)}
              </div>

              <div className="form-row">
                {renderNumberField('최대 재시도', 'max_retry_count', 3, '회', 0, 10, 1)}
                {renderNumberField('재시도 간격', 'retry_interval_ms', 1000, 'ms', 500, 10000, 500)}
              </div>

              <div className="form-row">
                {renderNumberField('백오프 시간', 'backoff_time_ms', 2000, 'ms', 1000, 30000, 1000)}
              </div>

              {renderToggleField(
                'Keep-Alive 활성화',
                'keep_alive_enabled',
                true,
                '연결 유지를 위한 주기적 통신 활성화'
              )}

              {getSettingValue('keep_alive_enabled', true) && (
                <div className="form-row">
                  {renderNumberField('Keep-Alive 간격', 'keep_alive_interval_s', 30, '초', 10, 300, 10)}
                </div>
              )}
            </div>
          )}
        </div>

        {/* 성능 및 최적화 섹션 */}
        <div className="settings-section">
          {renderSectionHeader('performance', '성능 및 최적화', 'fa-tachometer-alt')}
          
          {expandedSections.has('performance') && (
            <div className="section-content">
              {renderToggleField(
                '데이터 검증',
                'data_validation_enabled',
                true,
                '수신된 데이터의 유효성 검사 수행'
              )}

              {renderToggleField(
                '성능 모니터링',
                'performance_monitoring_enabled',
                true,
                '응답시간, 처리량 등 성능 지표 수집'
              )}

              <div className="form-row">
                {renderNumberField('스캔 레이트 오버라이드', 'scan_rate_override', 0, 'ms', 0, 10000, 100)}
                {renderNumberField('스캔 그룹', 'scan_group', 1, '', 1, 10, 1)}
              </div>

              <div className="form-row">
                {renderNumberField('프레임 간 지연', 'inter_frame_delay_ms', 0, 'ms', 0, 1000, 10)}
                {renderNumberField('큐 크기', 'queue_size', 100, '개', 10, 1000, 10)}
              </div>

              <div className="form-row">
                {renderNumberField('읽기 버퍼', 'read_buffer_size', 1024, 'bytes', 512, 8192, 512)}
                {renderNumberField('쓰기 버퍼', 'write_buffer_size', 1024, 'bytes', 512, 8192, 512)}
              </div>
            </div>
          )}
        </div>

        {/* 고급 설정 섹션 */}
        <div className="settings-section">
          {renderSectionHeader('advanced', '고급 설정', 'fa-cogs')}
          
          {expandedSections.has('advanced') && (
            <div className="section-content">
              {renderToggleField(
                '상세 로깅',
                'detailed_logging_enabled',
                false,
                '디버깅을 위한 상세한 로그 기록 (성능에 영향을 줄 수 있음)'
              )}

              {renderToggleField(
                '진단 모드',
                'diagnostic_mode_enabled',
                false,
                '문제 해결을 위한 진단 정보 수집'
              )}

              {renderToggleField(
                '통신 로깅',
                'communication_logging_enabled',
                false,
                '모든 통신 내용을 로그로 기록'
              )}

              {renderToggleField(
                '이상값 탐지',
                'outlier_detection_enabled',
                false,
                '통계적 방법으로 이상값 자동 탐지'
              )}

              {renderToggleField(
                '데드밴드',
                'deadband_enabled',
                false,
                '미세한 변화 무시로 통신량 최적화'
              )}

              <div className="form-row">
                {renderNumberField('백오프 배수', 'backoff_multiplier', 2, '배', 1, 10, 0.5)}
                {renderNumberField('최대 백오프', 'max_backoff_time_ms', 30000, 'ms', 5000, 300000, 5000)}
              </div>

              <div className="form-row">
                {renderNumberField('Keep-Alive 타임아웃', 'keep_alive_timeout_s', 60, '초', 30, 600, 30)}
              </div>
            </div>
          )}
        </div>

        {/* 설정 정보 섹션 */}
        {mode === 'view' && settings && Object.keys(settings).length > 0 && (
          <div className="settings-section">
            {renderSectionHeader('info', '설정 정보', 'fa-info-circle')}
            
            {expandedSections.has('info') && (
              <div className="section-content">
                <div className="form-row">
                  <div className="form-group">
                    <label>설정 생성일시</label>
                    <div className="form-value">
                      {settings.created_at ? new Date(settings.created_at).toLocaleString() : 'N/A'}
                    </div>
                  </div>

                  <div className="form-group">
                    <label>설정 수정일시</label>
                    <div className="form-value">
                      {settings.updated_at ? new Date(settings.updated_at).toLocaleString() : 'N/A'}
                    </div>
                  </div>
                </div>

                {settings.updated_by && (
                  <div className="form-group">
                    <label>수정자</label>
                    <div className="form-value">{settings.updated_by}</div>
                  </div>
                )}
              </div>
            )}
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

        .settings-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1rem;
        }

        .settings-header h2 {
          margin: 0;
          font-size: 1.25rem;
          font-weight: 600;
          color: #1f2937;
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

        .form-group input,
        .form-group select {
          padding: 0.5rem 0.75rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          transition: border-color 0.2s ease;
        }

        .form-group input:focus,
        .form-group select:focus {
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
      `}</style>
    </div>
  );
};

export default DeviceSettingsTab;