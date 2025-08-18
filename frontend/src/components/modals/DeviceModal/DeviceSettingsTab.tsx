// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceSettingsTab.tsx
// ⚙️ 디바이스 설정 탭 컴포넌트
// ============================================================================

import React from 'react';
import { DeviceSettingsTabProps } from './types';

const DeviceSettingsTab: React.FC<DeviceSettingsTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField,
  onUpdateSettings
}) => {
  const displayData = device || editData;

  return (
    <div className="tab-panel">
      <div className="settings-sections">
        {/* 폴링 및 타이밍 설정 */}
        <div className="setting-section">
          <h3>📡 폴링 및 타이밍 설정</h3>
          <div className="form-grid">
            <div className="form-group">
              <label>폴링 주기 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.polling_interval_ms || displayData?.polling_interval || 'N/A'}</div>
              ) : (
                <input
                  type="number"
                  min="100"
                  max="3600000"
                  value={editData?.settings?.polling_interval_ms || editData?.polling_interval || 5000}
                  onChange={(e) => onUpdateSettings('polling_interval_ms', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>스캔 주기 오버라이드 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.scan_rate_override || '기본값 사용'}</div>
              ) : (
                <input
                  type="number"
                  min="10"
                  max="60000"
                  value={editData?.settings?.scan_rate_override || ''}
                  onChange={(e) => onUpdateSettings('scan_rate_override', e.target.value ? parseInt(e.target.value) : null)}
                  placeholder="기본값 사용"
                />
              )}
            </div>
            <div className="form-group">
              <label>스캔 그룹</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.scan_group || 1}</div>
              ) : (
                <select
                  value={editData?.settings?.scan_group || 1}
                  onChange={(e) => onUpdateSettings('scan_group', parseInt(e.target.value))}
                >
                  <option value={1}>그룹 1 (높은 우선순위)</option>
                  <option value={2}>그룹 2 (보통 우선순위)</option>
                  <option value={3}>그룹 3 (낮은 우선순위)</option>
                  <option value={4}>그룹 4 (백그라운드)</option>
                </select>
              )}
            </div>
            <div className="form-group">
              <label>프레임 간 지연 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.inter_frame_delay_ms || 10}</div>
              ) : (
                <input
                  type="number"
                  min="0"
                  max="1000"
                  value={editData?.settings?.inter_frame_delay_ms || 10}
                  onChange={(e) => onUpdateSettings('inter_frame_delay_ms', parseInt(e.target.value))}
                />
              )}
            </div>
          </div>
        </div>

        {/* 연결 및 통신 설정 */}
        <div className="setting-section">
          <h3>🔌 연결 및 통신 설정</h3>
          <div className="form-grid">
            <div className="form-group">
              <label>연결 타임아웃 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.connection_timeout_ms || displayData?.timeout || 'N/A'}</div>
              ) : (
                <input
                  type="number"
                  min="1000"
                  max="60000"
                  value={editData?.settings?.connection_timeout_ms || editData?.timeout || 10000}
                  onChange={(e) => onUpdateSettings('connection_timeout_ms', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>읽기 타임아웃 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.read_timeout_ms || 5000}</div>
              ) : (
                <input
                  type="number"
                  min="1000"
                  max="30000"
                  value={editData?.settings?.read_timeout_ms || 5000}
                  onChange={(e) => onUpdateSettings('read_timeout_ms', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>쓰기 타임아웃 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.write_timeout_ms || 5000}</div>
              ) : (
                <input
                  type="number"
                  min="1000"
                  max="30000"
                  value={editData?.settings?.write_timeout_ms || 5000}
                  onChange={(e) => onUpdateSettings('write_timeout_ms', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>읽기 버퍼 크기 (bytes)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.read_buffer_size || 1024}</div>
              ) : (
                <input
                  type="number"
                  min="256"
                  max="65536"
                  value={editData?.settings?.read_buffer_size || 1024}
                  onChange={(e) => onUpdateSettings('read_buffer_size', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>쓰기 버퍼 크기 (bytes)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.write_buffer_size || 1024}</div>
              ) : (
                <input
                  type="number"
                  min="256"
                  max="65536"
                  value={editData?.settings?.write_buffer_size || 1024}
                  onChange={(e) => onUpdateSettings('write_buffer_size', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>명령 큐 크기</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.queue_size || 100}</div>
              ) : (
                <input
                  type="number"
                  min="10"
                  max="1000"
                  value={editData?.settings?.queue_size || 100}
                  onChange={(e) => onUpdateSettings('queue_size', parseInt(e.target.value))}
                />
              )}
            </div>
          </div>
        </div>

        {/* 재시도 정책 */}
        <div className="setting-section">
          <h3>🔄 재시도 정책</h3>
          <div className="form-grid">
            <div className="form-group">
              <label>최대 재시도 횟수</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.max_retry_count || displayData?.retry_count || 'N/A'}</div>
              ) : (
                <input
                  type="number"
                  min="0"
                  max="10"
                  value={editData?.settings?.max_retry_count || editData?.retry_count || 3}
                  onChange={(e) => onUpdateSettings('max_retry_count', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>재시도 간격 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.retry_interval_ms || 5000}</div>
              ) : (
                <input
                  type="number"
                  min="100"
                  max="60000"
                  value={editData?.settings?.retry_interval_ms || 5000}
                  onChange={(e) => onUpdateSettings('retry_interval_ms', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>백오프 승수</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.backoff_multiplier || 1.5}</div>
              ) : (
                <input
                  type="number"
                  min="1.0"
                  max="5.0"
                  step="0.1"
                  value={editData?.settings?.backoff_multiplier || 1.5}
                  onChange={(e) => onUpdateSettings('backoff_multiplier', parseFloat(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>백오프 시간 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.backoff_time_ms || 60000}</div>
              ) : (
                <input
                  type="number"
                  min="1000"
                  max="3600000"
                  value={editData?.settings?.backoff_time_ms || 60000}
                  onChange={(e) => onUpdateSettings('backoff_time_ms', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>최대 백오프 시간 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.max_backoff_time_ms || 300000}</div>
              ) : (
                <input
                  type="number"
                  min="10000"
                  max="1800000"
                  value={editData?.settings?.max_backoff_time_ms || 300000}
                  onChange={(e) => onUpdateSettings('max_backoff_time_ms', parseInt(e.target.value))}
                />
              )}
            </div>
          </div>
        </div>

        {/* Keep-alive 설정 */}
        <div className="setting-section">
          <h3>❤️ Keep-alive 설정</h3>
          <div className="form-grid">
            <div className="form-group">
              <label>Keep-Alive 활성화</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.keep_alive_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.keep_alive_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.keep_alive_enabled || false}
                    onChange={(e) => onUpdateSettings('keep_alive_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
            <div className="form-group">
              <label>Keep-Alive 간격 (초)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.keep_alive_interval_s || 30}</div>
              ) : (
                <input
                  type="number"
                  min="5"
                  max="300"
                  value={editData?.settings?.keep_alive_interval_s || 30}
                  onChange={(e) => onUpdateSettings('keep_alive_interval_s', parseInt(e.target.value))}
                />
              )}
            </div>
            <div className="form-group">
              <label>Keep-Alive 타임아웃 (초)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.settings?.keep_alive_timeout_s || 10}</div>
              ) : (
                <input
                  type="number"
                  min="1"
                  max="60"
                  value={editData?.settings?.keep_alive_timeout_s || 10}
                  onChange={(e) => onUpdateSettings('keep_alive_timeout_s', parseInt(e.target.value))}
                />
              )}
            </div>
          </div>
        </div>

        {/* 데이터 품질 관리 */}
        <div className="setting-section">
          <h3>🎯 데이터 품질 관리</h3>
          <div className="form-grid">
            <div className="form-group">
              <label>데이터 검증 활성화</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.data_validation_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.data_validation_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.data_validation_enabled || false}
                    onChange={(e) => onUpdateSettings('data_validation_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
            <div className="form-group">
              <label>이상값 탐지 활성화</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.outlier_detection_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.outlier_detection_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.outlier_detection_enabled || false}
                    onChange={(e) => onUpdateSettings('outlier_detection_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
            <div className="form-group">
              <label>데드밴드 활성화</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.deadband_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.deadband_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.deadband_enabled || false}
                    onChange={(e) => onUpdateSettings('deadband_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
          </div>
        </div>

        {/* 로깅 및 진단 */}
        <div className="setting-section">
          <h3>📊 로깅 및 진단</h3>
          <div className="form-grid">
            <div className="form-group">
              <label>상세 로깅</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.detailed_logging_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.detailed_logging_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.detailed_logging_enabled || false}
                    onChange={(e) => onUpdateSettings('detailed_logging_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
            <div className="form-group">
              <label>성능 모니터링</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.performance_monitoring_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.performance_monitoring_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.performance_monitoring_enabled || false}
                    onChange={(e) => onUpdateSettings('performance_monitoring_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
            <div className="form-group">
              <label>진단 모드</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.diagnostic_mode_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.diagnostic_mode_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.diagnostic_mode_enabled || false}
                    onChange={(e) => onUpdateSettings('diagnostic_mode_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
            <div className="form-group">
              <label>통신 로깅</label>
              {mode === 'view' ? (
                <div className="form-value">
                  <span className={`status-badge ${displayData?.settings?.communication_logging_enabled ? 'enabled' : 'disabled'}`}>
                    {displayData?.settings?.communication_logging_enabled ? '활성화' : '비활성화'}
                  </span>
                </div>
              ) : (
                <label className="switch">
                  <input
                    type="checkbox"
                    checked={editData?.settings?.communication_logging_enabled || false}
                    onChange={(e) => onUpdateSettings('communication_logging_enabled', e.target.checked)}
                  />
                  <span className="slider"></span>
                </label>
              )}
            </div>
          </div>
        </div>

        {/* 메타데이터 */}
        <div className="setting-section">
          <h3>ℹ️ 메타데이터</h3>
          <div className="form-grid">
            <div className="form-group">
              <label>생성일</label>
              <div className="form-value">{displayData?.settings?.created_at ? new Date(displayData.settings.created_at).toLocaleString() : '정보 없음'}</div>
            </div>
            <div className="form-group">
              <label>마지막 수정일</label>
              <div className="form-value">{displayData?.settings?.updated_at ? new Date(displayData.settings.updated_at).toLocaleString() : '정보 없음'}</div>
            </div>
            <div className="form-group">
              <label>수정한 사용자</label>
              <div className="form-value">{displayData?.settings?.updated_by || '시스템'}</div>
            </div>
          </div>
        </div>
      </div>

      <style jsx>{`
        .tab-panel {
          flex: 1;
          overflow-y: auto;
          padding: 2rem;
          height: 100%;
        }

        .settings-sections {
          display: flex;
          flex-direction: column;
          gap: 2rem;
        }

        .setting-section {
          border: 1px solid #e5e7eb;
          border-radius: 0.75rem;
          padding: 1.5rem;
        }

        .setting-section h3 {
          margin: 0 0 1rem 0;
          font-size: 1.125rem;
          font-weight: 600;
          color: #1f2937;
        }

        .form-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
          gap: 1.5rem;
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
          padding: 0.75rem;
          border: 1px solid #d1d5db;
          border-radius: 0.5rem;
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
          padding: 0.75rem;
          background: #f9fafb;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          font-size: 0.875rem;
          color: #374151;
        }

        .status-badge {
          display: inline-flex;
          align-items: center;
          gap: 0.25rem;
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
      `}</style>
    </div>
  );
};

export default DeviceSettingsTab;