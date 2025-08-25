// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceSettingsTab.tsx
// ⚙️ 디바이스 설정 탭 컴포넌트 - protocol_id 지원
// ============================================================================

import React, { useState } from 'react';
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
  // 상태 관리
  // ========================================================================
  const [expandedSections, setExpandedSections] = useState<Set<string>>(
    new Set(['communication', 'performance'])
  );

  const displayData = device || editData;
  const settings = displayData?.settings || {};
  
  // RTU 디바이스 여부 확인 - protocol_id 또는 protocol_type 기반
  const isRtuDevice = () => {
    // 1. protocol_type으로 확인
    if (displayData?.protocol_type === 'MODBUS_RTU') {
      return true;
    }
    
    // 2. protocol_id로 확인 (MODBUS_RTU는 보통 ID 2)
    if (displayData?.protocol_id) {
      const protocol = DeviceApiService.getProtocolManager().getProtocolById(displayData.protocol_id);
      return protocol?.protocol_type === 'MODBUS_RTU';
    }
    
    return false;
  };

  const isRtu = isRtuDevice();
  
  // RTU config 파싱
  const getRtuConfig = () => {
    try {
      const config = typeof displayData?.config === 'string' 
        ? JSON.parse(displayData.config) 
        : displayData?.config || {};
      return config;
    } catch {
      return {};
    }
  };

  const rtuConfig = getRtuConfig();

  // 프로토콜 정보 가져오기
  const getProtocolInfo = () => {
    if (displayData?.protocol_id) {
      return DeviceApiService.getProtocolManager().getProtocolById(displayData.protocol_id);
    }
    if (displayData?.protocol_type) {
      return DeviceApiService.getProtocolManager().getProtocolByType(displayData.protocol_type);
    }
    return null;
  };

  const protocolInfo = getProtocolInfo();

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
   * RTU config 값 가져오기
   */
  const getRtuConfigValue = (key: string, defaultValue: any) => {
    return rtuConfig[key] !== undefined ? rtuConfig[key] : defaultValue;
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
   * RTU config 업데이트
   */
  const updateRtuConfig = (key: string, value: any) => {
    if (mode !== 'view') {
      const newConfig = { ...rtuConfig, [key]: value };
      onUpdateField('config', newConfig);
    }
  };

  /**
   * 기본값으로 초기화
   */
  const resetToDefaults = () => {
    if (mode === 'view') return;

    const defaultSettings = {
      polling_interval_ms: isRtu ? 2000 : 1000,
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

    // RTU 기본 설정도 초기화
    if (isRtu) {
      const defaultRtuConfig = {
        ...rtuConfig,
        inter_frame_delay_ms: 10,
        character_timeout_ms: 5,
        turnaround_delay_ms: 50,
        response_timeout_ms: 1000,
        rts_toggle_enabled: true,
        rts_assert_delay_us: 100,
        rts_deassert_delay_us: 100,
        multi_drop_enabled: true
      };
      onUpdateField('config', defaultRtuConfig);
    }

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
        {protocolInfo && id === 'rtu_serial' && (
          <span className="protocol-badge">
            {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
          </span>
        )}
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
    step?: number,
    isRtuConfig: boolean = false
  ) => (
    <div className="form-group">
      <label>{label}</label>
      {mode === 'view' ? (
        <div className="form-value">
          {(isRtuConfig ? getRtuConfigValue(key, defaultValue) : getSettingValue(key, defaultValue)).toLocaleString()} {unit}
        </div>
      ) : (
        <div className="input-with-unit">
          <input
            type="number"
            value={isRtuConfig ? getRtuConfigValue(key, defaultValue) : getSettingValue(key, defaultValue)}
            onChange={(e) => isRtuConfig ? updateRtuConfig(key, parseInt(e.target.value)) : updateSettingValue(key, parseInt(e.target.value))}
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
   * 선택 필드 렌더링 (RTU용)
   */
  const renderSelectField = (
    label: string,
    key: string,
    defaultValue: string | number,
    options: Array<{value: string | number, label: string}>,
    isRtuConfig: boolean = false
  ) => (
    <div className="form-group">
      <label>{label}</label>
      {mode === 'view' ? (
        <div className="form-value">
          {options.find(opt => opt.value === (isRtuConfig ? getRtuConfigValue(key, defaultValue) : getSettingValue(key, defaultValue)))?.label || defaultValue}
        </div>
      ) : (
        <select
          value={isRtuConfig ? getRtuConfigValue(key, defaultValue) : getSettingValue(key, defaultValue)}
          onChange={(e) => {
            const value = e.target.type === 'number' ? parseInt(e.target.value) : e.target.value;
            isRtuConfig ? updateRtuConfig(key, value) : updateSettingValue(key, value);
          }}
        >
          {options.map(option => (
            <option key={option.value} value={option.value}>
              {option.label}
            </option>
          ))}
        </select>
      )}
    </div>
  );

  /**
   * 토글 스위치 렌더링
   */
  const renderToggleField = (label: string, key: string, defaultValue: boolean, description?: string, isRtuConfig: boolean = false) => (
    <div className="form-group">
      <div className="toggle-header">
        <label>{label}</label>
        {mode === 'view' ? (
          <span className={`status-badge ${(isRtuConfig ? getRtuConfigValue(key, defaultValue) : getSettingValue(key, defaultValue)) ? 'enabled' : 'disabled'}`}>
            {(isRtuConfig ? getRtuConfigValue(key, defaultValue) : getSettingValue(key, defaultValue)) ? '활성화' : '비활성화'}
          </span>
        ) : (
          <label className="switch">
            <input
              type="checkbox"
              checked={isRtuConfig ? getRtuConfigValue(key, defaultValue) : getSettingValue(key, defaultValue)}
              onChange={(e) => isRtuConfig ? updateRtuConfig(key, e.target.checked) : updateSettingValue(key, e.target.checked)}
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
          <div className="header-left">
            <h2>⚙️ 디바이스 설정</h2>
            {protocolInfo && (
              <div className="protocol-info">
                <span className="protocol-name">{protocolInfo.display_name || protocolInfo.name}</span>
                <span className="protocol-details">
                  ID: {protocolInfo.id} | Type: {protocolInfo.protocol_type}
                  {protocolInfo.default_port && ` | Port: ${protocolInfo.default_port}`}
                </span>
              </div>
            )}
          </div>
          {mode !== 'view' && (
            <button className="btn btn-secondary btn-sm" onClick={resetToDefaults}>
              <i className="fas fa-undo"></i>
              기본값 복원
            </button>
          )}
        </div>

        {/* 좌우 2열 그리드 */}
        <div className="settings-grid">
          {/* 왼쪽 열 */}
          <div className="settings-column">
            {/* RTU 전용 설정 섹션 */}
            {isRtu && (
              <div className="settings-section">
                {renderSectionHeader('rtu_serial', 'RTU 시리얼 통신 설정', 'fa-usb')}
                
                {expandedSections.has('rtu_serial') && (
                  <div className="section-content">
                    <div className="form-row">
                      {renderSelectField(
                        'Baud Rate',
                        'baud_rate',
                        9600,
                        [
                          { value: 1200, label: '1200 bps' },
                          { value: 2400, label: '2400 bps' },
                          { value: 4800, label: '4800 bps' },
                          { value: 9600, label: '9600 bps' },
                          { value: 19200, label: '19200 bps' },
                          { value: 38400, label: '38400 bps' },
                          { value: 57600, label: '57600 bps' },
                          { value: 115200, label: '115200 bps' }
                        ],
                        true
                      )}

                      {renderSelectField(
                        'Data Bits',
                        'data_bits',
                        8,
                        [
                          { value: 7, label: '7 bits' },
                          { value: 8, label: '8 bits' }
                        ],
                        true
                      )}
                    </div>

                    <div className="form-row">
                      {renderSelectField(
                        'Stop Bits',
                        'stop_bits',
                        1,
                        [
                          { value: 1, label: '1 bit' },
                          { value: 2, label: '2 bits' }
                        ],
                        true
                      )}

                      {renderSelectField(
                        'Parity',
                        'parity',
                        'N',
                        [
                          { value: 'N', label: 'None' },
                          { value: 'E', label: 'Even' },
                          { value: 'O', label: 'Odd' }
                        ],
                        true
                      )}
                    </div>

                    <div className="form-row">
                      {renderSelectField(
                        'Flow Control',
                        'flow_control',
                        'none',
                        [
                          { value: 'none', label: 'None' },
                          { value: 'rts_cts', label: 'RTS/CTS' },
                          { value: 'xon_xoff', label: 'XON/XOFF' }
                        ],
                        true
                      )}
                    </div>

                    <div className="rtu-timing-section">
                      <h4>RTU 타이밍 설정</h4>
                      <div className="form-row">
                        {renderNumberField('Inter-frame Delay', 'inter_frame_delay_ms', 10, 'ms', 0, 100, 1, true)}
                        {renderNumberField('Character Timeout', 'character_timeout_ms', 5, 'ms', 1, 50, 1, true)}
                      </div>
                      
                      <div className="form-row">
                        {renderNumberField('Turnaround Delay', 'turnaround_delay_ms', 50, 'ms', 0, 500, 10, true)}
                        {renderNumberField('Response Timeout', 'response_timeout_ms', 1000, 'ms', 100, 5000, 100, true)}
                      </div>
                    </div>

                    {/* RS-485 설정 */}
                    <div className="rs485-section">
                      <h4>RS-485 설정</h4>
                      {renderToggleField(
                        'RTS Toggle 활성화',
                        'rts_toggle_enabled',
                        true,
                        'RS-485 송수신 제어를 위한 RTS 신호 자동 토글',
                        true
                      )}

                      {getRtuConfigValue('rts_toggle_enabled', true) && (
                        <div className="form-row">
                          {renderNumberField('RTS Assert Delay', 'rts_assert_delay_us', 100, 'μs', 0, 10000, 50, true)}
                          {renderNumberField('RTS Deassert Delay', 'rts_deassert_delay_us', 100, 'μs', 0, 10000, 50, true)}
                        </div>
                      )}

                      {renderToggleField(
                        'Multi-drop 모드',
                        'multi_drop_enabled',
                        true,
                        'RS-485 멀티드롭 네트워크 지원',
                        true
                      )}
                    </div>
                  </div>
                )}
              </div>
            )}

            {/* 통신 설정 섹션 */}
            <div className="settings-section">
              {renderSectionHeader('communication', '통신 설정', 'fa-wifi')}
              
              {expandedSections.has('communication') && (
                <div className="section-content">
                  <div className="form-row">
                    {renderNumberField(
                      '폴링 간격', 
                      'polling_interval_ms', 
                      isRtu ? 2000 : 1000, 
                      'ms', 
                      100, 
                      60000, 
                      100
                    )}
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
          </div>

          {/* 오른쪽 열 */}
          <div className="settings-column">
            {/* RTU 마스터 전용 설정 */}
            {isRtu && !rtuConfig.slave_id && (
              <div className="settings-section">
                {renderSectionHeader('rtu_master', 'RTU 마스터 설정', 'fa-crown')}
                
                {expandedSections.has('rtu_master') && (
                  <div className="section-content">
                    <div className="form-row">
                      {renderNumberField('슬래이브 스캔 주기', 'slave_scan_interval_ms', 5000, 'ms', 1000, 60000, 1000, true)}
                      {renderNumberField('슬래이브 개별 타임아웃', 'slave_individual_timeout_ms', 2000, 'ms', 500, 10000, 500, true)}
                    </div>

                    <div className="form-row">
                      {renderSelectField(
                        '재시도 전략',
                        'retry_strategy',
                        'immediate',
                        [
                          { value: 'immediate', label: '즉시 재시도' },
                          { value: 'delayed', label: '지연 재시도' },
                          { value: 'exponential_backoff', label: '지수 백오프' }
                        ],
                        true
                      )}

                      {renderNumberField('최대 동시 요청', 'max_concurrent_requests', 1, '개', 1, 10, 1, true)}
                    </div>

                    {renderToggleField(
                      '슬래이브 상태 모니터링',
                      'slave_health_monitoring',
                      true,
                      '슬래이브 디바이스 연결 상태 지속 모니터링',
                      true
                    )}

                    {renderToggleField(
                      '자동 슬래이브 발견',
                      'auto_slave_discovery',
                      false,
                      '새로운 슬래이브 디바이스 자동 탐지',
                      true
                    )}
                  </div>
                )}
              </div>
            )}

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
                    {renderNumberField('큐 크기', 'queue_size', 100, '개', 10, 1000, 10)}
                    {renderNumberField('읽기 버퍼', 'read_buffer_size', 1024, 'bytes', 512, 8192, 512)}
                  </div>

                  <div className="form-row">
                    {renderNumberField('쓰기 버퍼', 'write_buffer_size', 1024, 'bytes', 512, 8192, 512)}
                  </div>

                  {/* 프로토콜별 최적화 권장사항 */}
                  {protocolInfo && (
                    <div className="optimization-tips">
                      <h4>최적화 권장사항</h4>
                      <div className="tip-item">
                        <i className="fas fa-lightbulb"></i>
                        <span>
                          {protocolInfo.protocol_type === 'MODBUS_RTU' 
                            ? 'RTU: 폴링 간격을 2초 이상으로 설정하고 타임아웃을 충분히 크게 설정하세요'
                            : protocolInfo.protocol_type === 'MQTT'
                            ? 'MQTT: Keep-Alive를 활성화하고 브로커 연결을 유지하세요'
                            : `${protocolInfo.display_name}: 네트워크 상태에 따라 타임아웃을 조정하세요`
                          }
                        </span>
                      </div>
                    </div>
                  )}
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
          </div>
        </div>

        {/* 설정 정보 섹션 (전체 너비) */}
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

                {protocolInfo && (
                  <div className="form-group">
                    <label>프로토콜 세부 정보</label>
                    <div className="protocol-details">
                      <div><strong>ID:</strong> {protocolInfo.id}</div>
                      <div><strong>Type:</strong> {protocolInfo.protocol_type}</div>
                      <div><strong>Name:</strong> {protocolInfo.display_name || protocolInfo.name}</div>
                      {protocolInfo.description && (
                        <div><strong>Description:</strong> {protocolInfo.description}</div>
                      )}
                      {protocolInfo.category && (
                        <div><strong>Category:</strong> {protocolInfo.category}</div>
                      )}
                      {protocolInfo.default_port && (
                        <div><strong>Default Port:</strong> {protocolInfo.default_port}</div>
                      )}
                      {protocolInfo.supported_operations && (
                        <div><strong>Operations:</strong> {protocolInfo.supported_operations.join(', ')}</div>
                      )}
                      {protocolInfo.supported_data_types && (
                        <div><strong>Data Types:</strong> {protocolInfo.supported_data_types.join(', ')}</div>
                      )}
                    </div>
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

        .settings-column {
          display: flex;
          flex-direction: column;
          gap: 1.5rem;
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

        .protocol-info {
          display: flex;
          flex-direction: column;
          gap: 0.25rem;
        }

        .protocol-name {
          font-size: 0.875rem;
          font-weight: 500;
          color: #0ea5e9;
        }

        .protocol-details {
          font-size: 0.75rem;
          color: #6b7280;
        }

        .protocol-badge {
          font-size: 0.75rem;
          color: #0ea5e9;
          background: #f0f9ff;
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
          border: 1px solid #bae6fd;
          margin-left: auto;
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

        .rtu-timing-section,
        .rs485-section {
          margin-top: 1.5rem;
          padding-top: 1.5rem;
          border-top: 1px solid #e5e7eb;
        }

        .rtu-timing-section h4,
        .rs485-section h4 {
          margin: 0 0 1rem 0;
          font-size: 0.875rem;
          font-weight: 600;
          color: #374151;
          text-transform: uppercase;
          letter-spacing: 0.05em;
        }

        .optimization-tips {
          margin-top: 1.5rem;
          padding-top: 1.5rem;
          border-top: 1px solid #e5e7eb;
        }

        .optimization-tips h4 {
          margin: 0 0 1rem 0;
          font-size: 0.875rem;
          font-weight: 600;
          color: #374151;
        }

        .tip-item {
          display: flex;
          align-items: flex-start;
          gap: 0.75rem;
          padding: 0.75rem;
          background: #fffbeb;
          border: 1px solid #fed7aa;
          border-radius: 0.5rem;
          font-size: 0.875rem;
          color: #92400e;
        }

        .tip-item i {
          color: #f59e0b;
          margin-top: 0.125rem;
          flex-shrink: 0;
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

        .protocol-details {
          background: #f9fafb;
          padding: 1rem;
          border-radius: 0.375rem;
          border: 1px solid #e5e7eb;
          font-size: 0.875rem;
        }

        .protocol-details > div {
          margin-bottom: 0.5rem;
          color: #374151;
        }

        .protocol-details > div:last-child {
          margin-bottom: 0;
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