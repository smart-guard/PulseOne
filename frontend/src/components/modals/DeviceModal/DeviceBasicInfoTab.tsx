// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx
// 📋 디바이스 기본정보 탭 컴포넌트 - protocol_id 지원
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService, ProtocolInfo } from '../../../api/services/deviceApi';
import { DeviceBasicInfoTabProps } from './types';

const DeviceBasicInfoTab: React.FC<DeviceBasicInfoTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField
}) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [availableProtocols, setAvailableProtocols] = useState<ProtocolInfo[]>([]);
  const [isLoadingProtocols, setIsLoadingProtocols] = useState(false);
  const [isTestingConnection, setIsTestingConnection] = useState(false);

  const displayData = device || editData;
  
  // RTU 설정 파싱
  const getRtuConfig = () => {
    try {
      const config = typeof editData?.config === 'string' 
        ? JSON.parse(editData.config) 
        : editData?.config || {};
      return config;
    } catch {
      return {};
    }
  };

  const rtuConfig = getRtuConfig();
  const isRtuDevice = editData?.protocol_type === 'MODBUS_RTU';

  // ========================================================================
  // API 호출 함수들
  // ========================================================================

  /**
   * 지원 프로토콜 목록 로드
   */
  const loadAvailableProtocols = async () => {
    try {
      setIsLoadingProtocols(true);
      console.log('📋 프로토콜 목록 로드 시작...');
      
      // DeviceApiService의 ProtocolManager 사용
      await DeviceApiService.initialize();
      const protocols = DeviceApiService.getProtocolManager().getAllProtocols();
      
      console.log('✅ 프로토콜 로드 완료:', protocols);
      setAvailableProtocols(protocols);
      
    } catch (error) {
      console.error('❌ 프로토콜 목록 로드 실패:', error);
      setAvailableProtocols(getDefaultProtocols());
    } finally {
      setIsLoadingProtocols(false);
    }
  };

  /**
   * 기본 프로토콜 목록 - API 호출 실패 시 백업용
   */
  const getDefaultProtocols = (): ProtocolInfo[] => [
    { 
      id: 1, 
      protocol_type: 'MODBUS_TCP', 
      name: 'Modbus TCP', 
      value: 'MODBUS_TCP',
      description: 'Modbus TCP/IP Protocol',
      display_name: 'Modbus TCP', 
      default_port: 502 
    },
    { 
      id: 2, 
      protocol_type: 'MODBUS_RTU', 
      name: 'Modbus RTU', 
      value: 'MODBUS_RTU',
      description: 'Modbus RTU Serial Protocol',
      display_name: 'Modbus RTU',
      uses_serial: true 
    },
    { 
      id: 3, 
      protocol_type: 'MQTT', 
      name: 'MQTT', 
      value: 'MQTT',
      description: 'MQTT Protocol',
      display_name: 'MQTT', 
      default_port: 1883,
      requires_broker: true 
    },
    { 
      id: 4, 
      protocol_type: 'BACNET', 
      name: 'BACnet', 
      value: 'BACNET',
      description: 'Building Automation and Control Networks',
      display_name: 'BACnet', 
      default_port: 47808 
    }
  ];

  /**
   * 연결 테스트
   */
  const handleTestConnection = async () => {
    if (!device?.id) {
      alert('저장된 디바이스만 연결 테스트가 가능합니다.');
      return;
    }

    try {
      setIsTestingConnection(true);
      const response = await DeviceApiService.testDeviceConnection(device.id);
      
      if (response.success && response.data) {
        const result = response.data;
        if (result.test_successful) {
          alert(`✅ 연결 성공!\n응답시간: ${result.response_time_ms}ms`);
        } else {
          alert(`❌ 연결 실패!\n오류: ${result.error_message}`);
        }
      } else {
        alert(`❌ 테스트 실패: ${response.error}`);
      }
    } catch (error) {
      console.error('연결 테스트 실패:', error);
      alert(`❌ 연결 테스트 오류: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setIsTestingConnection(false);
    }
  };

  // ========================================================================
  // RTU 설정 관리 함수들
  // ========================================================================

  /**
   * RTU config 업데이트
   */
  const updateRtuConfig = (key: string, value: any) => {
    const newConfig = { ...rtuConfig, [key]: value };
    onUpdateField('config', newConfig);
  };

  /**
   * 프로토콜 변경 시 처리 - protocol_id 기반
   */
  const handleProtocolChange = (protocolId: string) => {
    const selectedProtocol = availableProtocols.find(p => p.id === parseInt(protocolId));
    if (!selectedProtocol) return;

    console.log('🔄 프로토콜 변경:', selectedProtocol);
    
    // protocol_id와 protocol_type 모두 업데이트
    onUpdateField('protocol_id', selectedProtocol.id);
    onUpdateField('protocol_type', selectedProtocol.protocol_type);
    
    // RTU로 변경 시 기본 설정 적용
    if (selectedProtocol.protocol_type === 'MODBUS_RTU') {
      const defaultRtuConfig = {
        device_role: 'master',
        baud_rate: 9600,
        data_bits: 8,
        stop_bits: 1,
        parity: 'N',
        flow_control: 'none'
      };
      onUpdateField('config', defaultRtuConfig);
      
      // 엔드포인트 기본값 설정
      if (!editData?.endpoint) {
        onUpdateField('endpoint', '/dev/ttyUSB0');
      }
    } else {
      // 다른 프로토콜로 변경 시 config 초기화
      onUpdateField('config', {});
    }
  };

  /**
   * 디바이스 역할 변경 시 처리
   */
  const handleDeviceRoleChange = (role: string) => {
    const newConfig = { ...rtuConfig, device_role: role };
    
    if (role === 'slave') {
      // 슬래이브로 변경 시 기본 슬래이브 ID 설정
      if (!newConfig.slave_id) {
        newConfig.slave_id = 1;
      }
    } else {
      // 마스터로 변경 시 슬래이브 ID 제거
      delete newConfig.slave_id;
      delete newConfig.master_device_id;
    }
    
    onUpdateField('config', newConfig);
  };

  // ========================================================================
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    loadAvailableProtocols();
  }, []);

  // ========================================================================
  // 헬퍼 함수들
  // ========================================================================

  /**
   * 현재 선택된 protocol_id 가져오기
   */
  const getCurrentProtocolId = (): number => {
    // 1. 직접 protocol_id가 있는 경우
    if (editData?.protocol_id) {
      return editData.protocol_id;
    }
    
    // 2. protocol_type으로 protocol_id 찾기
    if (editData?.protocol_type) {
      const protocol = availableProtocols.find(p => p.protocol_type === editData.protocol_type);
      if (protocol) {
        return protocol.id;
      }
    }
    
    // 3. 기본값 (첫 번째 프로토콜)
    return availableProtocols.length > 0 ? availableProtocols[0].id : 1;
  };

  /**
   * 프로토콜 이름 표시 가져오기
   */
  const getProtocolDisplayName = (protocolType?: string, protocolId?: number): string => {
    if (protocolId) {
      const protocol = availableProtocols.find(p => p.id === protocolId);
      return protocol?.display_name || protocol?.name || `Protocol ${protocolId}`;
    }
    
    if (protocolType) {
      const protocol = availableProtocols.find(p => p.protocol_type === protocolType);
      return protocol?.display_name || protocol?.name || protocolType;
    }
    
    return 'N/A';
  };

  /**
   * 엔드포인트 예시 텍스트 생성
   */
  const getEndpointPlaceholder = (protocolType?: string): string => {
    switch (protocolType) {
      case 'MODBUS_TCP':
        return '192.168.1.100:502';
      case 'MODBUS_RTU':
        return '/dev/ttyUSB0 또는 COM1';
      case 'MQTT':
        return 'mqtt://192.168.1.100:1883';
      case 'BACNET':
        return '192.168.1.100:47808';
      case 'OPC_UA':
      case 'OPCUA':
        return 'opc.tcp://192.168.1.100:4840';
      case 'ETHERNET_IP':
        return '192.168.1.100:44818';
      case 'HTTP_REST':
        return 'http://192.168.1.100/api';
      case 'SNMP':
        return '192.168.1.100:161';
      default:
        return '연결 주소를 입력하세요';
    }
  };

  // ========================================================================
  // 렌더링
  // ========================================================================

  return (
    <div className="tab-panel">
      <div className="form-grid">
        
        {/* 기본 정보 섹션 */}
        <div className="form-section">
          <h3>📋 기본 정보</h3>
          
          <div className="form-row">
            <div className="form-group">
              <label>디바이스명 *</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.name || 'N/A'}</div>
              ) : (
                <input
                  type="text"
                  value={editData?.name || ''}
                  onChange={(e) => onUpdateField('name', e.target.value)}
                  placeholder="디바이스명을 입력하세요"
                  required
                />
              )}
            </div>

            <div className="form-group">
              <label>디바이스 타입</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.device_type || 'N/A'}</div>
              ) : (
                <select
                  value={editData?.device_type || 'PLC'}
                  onChange={(e) => onUpdateField('device_type', e.target.value)}
                >
                  <option value="PLC">PLC</option>
                  <option value="HMI">HMI</option>
                  <option value="SENSOR">센서</option>
                  <option value="ACTUATOR">액추에이터</option>
                  <option value="METER">미터</option>
                  <option value="CONTROLLER">컨트롤러</option>
                  <option value="GATEWAY">게이트웨이</option>
                  <option value="OTHER">기타</option>
                </select>
              )}
            </div>
          </div>

          <div className="form-group">
            <label>설명</label>
            {mode === 'view' ? (
              <div className="form-value">{displayData?.description || 'N/A'}</div>
            ) : (
              <textarea
                value={editData?.description || ''}
                onChange={(e) => onUpdateField('description', e.target.value)}
                placeholder="디바이스에 대한 설명을 입력하세요"
                rows={3}
              />
            )}
          </div>
        </div>

        {/* 제조사 정보 섹션 */}
        <div className="form-section">
          <h3>🏭 제조사 정보</h3>
          
          <div className="form-row">
            <div className="form-group">
              <label>제조사</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.manufacturer || 'N/A'}</div>
              ) : (
                <input
                  type="text"
                  value={editData?.manufacturer || ''}
                  onChange={(e) => onUpdateField('manufacturer', e.target.value)}
                  placeholder="제조사명"
                />
              )}
            </div>

            <div className="form-group">
              <label>모델명</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.model || 'N/A'}</div>
              ) : (
                <input
                  type="text"
                  value={editData?.model || ''}
                  onChange={(e) => onUpdateField('model', e.target.value)}
                  placeholder="모델명"
                />
              )}
            </div>
          </div>

          <div className="form-group">
            <label>시리얼 번호</label>
            {mode === 'view' ? (
              <div className="form-value">{displayData?.serial_number || 'N/A'}</div>
            ) : (
              <input
                type="text"
                value={editData?.serial_number || ''}
                onChange={(e) => onUpdateField('serial_number', e.target.value)}
                placeholder="시리얼 번호"
              />
            )}
          </div>
        </div>

        {/* 네트워크 설정 섹션 */}
        <div className="form-section">
          <h3>🌐 네트워크 설정</h3>
          
          <div className="form-group">
            <label>프로토콜 *</label>
            {mode === 'view' ? (
              <div className="form-value">
                {getProtocolDisplayName(displayData?.protocol_type, displayData?.protocol_id)}
                {displayData?.protocol && displayData.protocol.default_port && (
                  <span className="protocol-port"> (Port: {displayData.protocol.default_port})</span>
                )}
              </div>
            ) : (
              <>
                {isLoadingProtocols ? (
                  <select disabled>
                    <option>프로토콜 로딩 중...</option>
                  </select>
                ) : availableProtocols.length === 0 ? (
                  <select disabled>
                    <option>프로토콜을 불러올 수 없습니다</option>
                  </select>
                ) : (
                  <select
                    value={getCurrentProtocolId()}
                    onChange={(e) => handleProtocolChange(e.target.value)}
                  >
                    {availableProtocols.map(protocol => (
                      <option key={protocol.id} value={protocol.id}>
                        {protocol.display_name || protocol.name}
                        {protocol.default_port && ` (Port: ${protocol.default_port})`}
                      </option>
                    ))}
                  </select>
                )}
                
                {/* 디버깅 정보 - 개발 중에만 표시 */}
                {process.env.NODE_ENV === 'development' && (
                  <div className="debug-info">
                    <small style={{ color: '#666', fontSize: '0.7em' }}>
                      로딩: {isLoadingProtocols ? 'Y' : 'N'} | 
                      개수: {availableProtocols.length} | 
                      protocol_id: {editData?.protocol_id || 'none'} | 
                      protocol_type: {editData?.protocol_type || 'none'}
                    </small>
                  </div>
                )}
              </>
            )}
          </div>

          <div className="form-group">
            <label>{isRtuDevice ? '시리얼 포트 *' : '엔드포인트 *'}</label>
            {mode === 'view' ? (
              <div className="form-value">{displayData?.endpoint || 'N/A'}</div>
            ) : (
              <input
                type="text"
                value={editData?.endpoint || ''}
                onChange={(e) => onUpdateField('endpoint', e.target.value)}
                placeholder={getEndpointPlaceholder(editData?.protocol_type)}
                required
              />
            )}
            <div className="form-hint">
              {editData?.protocol_type && `예시: ${getEndpointPlaceholder(editData.protocol_type)}`}
            </div>
          </div>

          {/* RTU 전용 설정 */}
          {isRtuDevice && mode !== 'view' && (
            <>
              {/* 디바이스 역할 */}
              <div className="form-group">
                <label>디바이스 역할 *</label>
                <select
                  value={rtuConfig.device_role || 'master'}
                  onChange={(e) => handleDeviceRoleChange(e.target.value)}
                >
                  <option value="master">마스터 (Master)</option>
                  <option value="slave">슬래이브 (Slave)</option>
                </select>
                <div className="form-hint">
                  마스터: 다른 슬래이브 디바이스들을 관리 | 슬래이브: 마스터의 요청에 응답
                </div>
              </div>

              {/* 슬래이브 ID (슬래이브인 경우만) */}
              {rtuConfig.device_role === 'slave' && (
                <div className="form-group">
                  <label>슬래이브 ID *</label>
                  <input
                    type="number"
                    value={rtuConfig.slave_id || 1}
                    onChange={(e) => updateRtuConfig('slave_id', parseInt(e.target.value))}
                    min="1"
                    max="247"
                    required
                  />
                  <div className="form-hint">
                    1~247 사이의 고유한 슬래이브 ID
                  </div>
                </div>
              )}

              {/* 시리얼 통신 파라미터 */}
              <div className="rtu-section">
                <h4>⚡ 시리얼 통신 설정</h4>
                
                <div className="form-row">
                  <div className="form-group">
                    <label>Baud Rate</label>
                    <select
                      value={rtuConfig.baud_rate || 9600}
                      onChange={(e) => updateRtuConfig('baud_rate', parseInt(e.target.value))}
                    >
                      <option value={1200}>1200 bps</option>
                      <option value={2400}>2400 bps</option>
                      <option value={4800}>4800 bps</option>
                      <option value={9600}>9600 bps</option>
                      <option value={19200}>19200 bps</option>
                      <option value={38400}>38400 bps</option>
                      <option value={57600}>57600 bps</option>
                      <option value={115200}>115200 bps</option>
                    </select>
                  </div>

                  <div className="form-group">
                    <label>Data Bits</label>
                    <select
                      value={rtuConfig.data_bits || 8}
                      onChange={(e) => updateRtuConfig('data_bits', parseInt(e.target.value))}
                    >
                      <option value={7}>7 bits</option>
                      <option value={8}>8 bits</option>
                    </select>
                  </div>
                </div>

                <div className="form-row">
                  <div className="form-group">
                    <label>Stop Bits</label>
                    <select
                      value={rtuConfig.stop_bits || 1}
                      onChange={(e) => updateRtuConfig('stop_bits', parseInt(e.target.value))}
                    >
                      <option value={1}>1 bit</option>
                      <option value={2}>2 bits</option>
                    </select>
                  </div>

                  <div className="form-group">
                    <label>Parity</label>
                    <select
                      value={rtuConfig.parity || 'N'}
                      onChange={(e) => updateRtuConfig('parity', e.target.value)}
                    >
                      <option value="N">None</option>
                      <option value="E">Even</option>
                      <option value="O">Odd</option>
                    </select>
                  </div>
                </div>
              </div>
            </>
          )}

          {/* 연결 테스트 버튼 */}
          {mode === 'view' && device?.id && (
            <div className="form-group">
              <button
                type="button"
                className="btn btn-info"
                onClick={handleTestConnection}
                disabled={isTestingConnection}
              >
                {isTestingConnection ? (
                  <>
                    <i className="fas fa-spinner fa-spin"></i>
                    테스트 중...
                  </>
                ) : (
                  <>
                    <i className="fas fa-plug"></i>
                    연결 테스트
                  </>
                )}
              </button>
            </div>
          )}
        </div>

        {/* 운영 설정 섹션 */}
        <div className="form-section">
          <h3>⚙️ 운영 설정</h3>
          
          <div className="form-row">
            <div className="form-group">
              <label>폴링 간격 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.polling_interval || 'N/A'}</div>
              ) : (
                <input
                  type="number"
                  value={editData?.polling_interval || (isRtuDevice ? 2000 : 1000)}
                  onChange={(e) => onUpdateField('polling_interval', parseInt(e.target.value))}
                  min="100"
                  max="60000"
                  step="100"
                />
              )}
              <div className="form-hint">
                {isRtuDevice ? 'RTU 권장값: 2000ms 이상' : '일반 권장값: 1000ms'}
              </div>
            </div>

            <div className="form-group">
              <label>타임아웃 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.timeout || 'N/A'}</div>
              ) : (
                <input
                  type="number"
                  value={editData?.timeout || (isRtuDevice ? 3000 : 5000)}
                  onChange={(e) => onUpdateField('timeout', parseInt(e.target.value))}
                  min="1000"
                  max="30000"
                  step="1000"
                />
              )}
            </div>

            <div className="form-group">
              <label>재시도 횟수</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.retry_count || 'N/A'}</div>
              ) : (
                <input
                  type="number"
                  value={editData?.retry_count || 3}
                  onChange={(e) => onUpdateField('retry_count', parseInt(e.target.value))}
                  min="0"
                  max="10"
                />
              )}
            </div>
          </div>

          <div className="form-group">
            <label>활성화</label>
            {mode === 'view' ? (
              <div className="form-value">
                <span className={`status-badge ${displayData?.is_enabled ? 'enabled' : 'disabled'}`}>
                  {displayData?.is_enabled ? '활성화됨' : '비활성화됨'}
                </span>
              </div>
            ) : (
              <label className="switch">
                <input
                  type="checkbox"
                  checked={editData?.is_enabled !== false}
                  onChange={(e) => onUpdateField('is_enabled', e.target.checked)}
                />
                <span className="slider"></span>
              </label>
            )}
          </div>
        </div>

        {/* 추가 정보 섹션 */}
        {mode === 'view' && (
          <div className="form-section">
            <h3>ℹ️ 추가 정보</h3>
            
            <div className="form-row">
              <div className="form-group">
                <label>생성일시</label>
                <div className="form-value">
                  {displayData?.created_at ? new Date(displayData.created_at).toLocaleString() : 'N/A'}
                </div>
              </div>

              <div className="form-group">
                <label>수정일시</label>
                <div className="form-value">
                  {displayData?.updated_at ? new Date(displayData.updated_at).toLocaleString() : 'N/A'}
                </div>
              </div>
            </div>

            {displayData?.installation_date && (
              <div className="form-group">
                <label>설치일자</label>
                <div className="form-value">
                  {new Date(displayData.installation_date).toLocaleDateString()}
                </div>
              </div>
            )}
            
            {/* 프로토콜 상세 정보 표시 */}
            {displayData?.protocol && (
              <div className="form-group">
                <label>프로토콜 상세 정보</label>
                <div className="protocol-details">
                  <div><strong>ID:</strong> {displayData.protocol.id}</div>
                  <div><strong>타입:</strong> {displayData.protocol.type}</div>
                  {displayData.protocol.category && (
                    <div><strong>카테고리:</strong> {displayData.protocol.category}</div>
                  )}
                  {displayData.protocol.default_port && (
                    <div><strong>기본 포트:</strong> {displayData.protocol.default_port}</div>
                  )}
                </div>
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
        }

        .form-grid {
          display: flex;
          flex-direction: column;
          gap: 2rem;
        }

        .form-section {
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1.5rem;
          background: #fafafa;
        }

        .form-section h3 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .rtu-section {
          margin-top: 1.5rem;
          padding-top: 1.5rem;
          border-top: 1px solid #e5e7eb;
        }

        .rtu-section h4 {
          margin: 0 0 1rem 0;
          font-size: 0.875rem;
          font-weight: 600;
          color: #374151;
          text-transform: uppercase;
          letter-spacing: 0.05em;
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
        .form-group select,
        .form-group textarea {
          padding: 0.5rem 0.75rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          transition: border-color 0.2s ease;
        }

        .form-group input:focus,
        .form-group select:focus,
        .form-group textarea:focus {
          outline: none;
          border-color: #0ea5e9;
          box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
        }

        .form-group select:disabled {
          background-color: #f3f4f6;
          color: #9ca3af;
          cursor: not-allowed;
        }

        .form-group textarea {
          resize: vertical;
          min-height: 80px;
        }

        .form-value {
          padding: 0.5rem 0.75rem;
          background: #f9fafb;
          border: 1px solid #e5e7eb;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          color: #374151;
        }

        .form-hint {
          font-size: 0.75rem;
          color: #6b7280;
          font-style: italic;
        }

        .debug-info {
          margin-top: 0.25rem;
          padding: 0.25rem;
          background: #f0f9ff;
          border: 1px solid #e0f2fe;
          border-radius: 0.25rem;
        }

        .protocol-port {
          color: #6b7280;
          font-size: 0.875rem;
        }

        .protocol-details {
          background: #f9fafb;
          padding: 1rem;
          border-radius: 0.375rem;
          border: 1px solid #e5e7eb;
        }

        .protocol-details > div {
          margin-bottom: 0.5rem;
          font-size: 0.875rem;
          color: #374151;
        }

        .protocol-details > div:last-child {
          margin-bottom: 0;
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

        .btn-info {
          background: #0891b2;
          color: white;
        }

        .btn-info:hover:not(:disabled) {
          background: #0e7490;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }
      `}</style>
    </div>
  );
};

export default DeviceBasicInfoTab;