// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx
// 📋 디바이스 기본정보 탭 컴포넌트 - 완전 구현
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService } from '../../../api/services/deviceApi';
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
  const [availableProtocols, setAvailableProtocols] = useState<any[]>([]);
  const [isLoadingProtocols, setIsLoadingProtocols] = useState(false);
  const [isTestingConnection, setIsTestingConnection] = useState(false);

  const displayData = device || editData;

  // ========================================================================
  // API 호출 함수들
  // ========================================================================

  /**
   * 지원 프로토콜 목록 로드
   */
  const loadAvailableProtocols = async () => {
    try {
      setIsLoadingProtocols(true);
      const response = await DeviceApiService.getAvailableProtocols();
      
      if (response.success && response.data) {
        setAvailableProtocols(response.data);
      } else {
        console.warn('프로토콜 목록 로드 실패:', response.error);
        // 기본 프로토콜 목록 사용
        setAvailableProtocols([
          { protocol_type: 'MODBUS_TCP', display_name: 'Modbus TCP', default_port: 502 },
          { protocol_type: 'MODBUS_RTU', display_name: 'Modbus RTU' },
          { protocol_type: 'MQTT', display_name: 'MQTT', default_port: 1883 },
          { protocol_type: 'BACNET', display_name: 'BACnet', default_port: 47808 },
          { protocol_type: 'OPCUA', display_name: 'OPC UA', default_port: 4840 },
          { protocol_type: 'ETHERNET_IP', display_name: 'Ethernet/IP', default_port: 44818 },
          { protocol_type: 'PROFINET', display_name: 'PROFINET' },
          { protocol_type: 'HTTP_REST', display_name: 'HTTP REST', default_port: 80 }
        ]);
      }
    } catch (error) {
      console.error('프로토콜 목록 로드 실패:', error);
      // 기본 프로토콜 목록 사용
      setAvailableProtocols([
        { protocol_type: 'MODBUS_TCP', display_name: 'Modbus TCP', default_port: 502 },
        { protocol_type: 'MODBUS_RTU', display_name: 'Modbus RTU' },
        { protocol_type: 'MQTT', display_name: 'MQTT', default_port: 1883 },
        { protocol_type: 'BACNET', display_name: 'BACnet', default_port: 47808 },
        { protocol_type: 'OPCUA', display_name: 'OPC UA', default_port: 4840 },
        { protocol_type: 'ETHERNET_IP', display_name: 'Ethernet/IP', default_port: 44818 },
        { protocol_type: 'PROFINET', display_name: 'PROFINET' },
        { protocol_type: 'HTTP_REST', display_name: 'HTTP REST', default_port: 80 }
      ]);
    } finally {
      setIsLoadingProtocols(false);
    }
  };

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
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    loadAvailableProtocols();
  }, []);

  // ========================================================================
  // 헬퍼 함수들
  // ========================================================================

  /**
   * 프로토콜별 기본 포트 가져오기
   */
  const getDefaultPort = (protocolType: string): number | null => {
    const protocol = availableProtocols.find(p => p.protocol_type === protocolType);
    return protocol?.default_port || null;
  };

  /**
   * 엔드포인트 예시 텍스트 생성
   */
  const getEndpointPlaceholder = (protocolType?: string): string => {
    switch (protocolType) {
      case 'MODBUS_TCP':
        return '192.168.1.100:502';
      case 'MODBUS_RTU':
        return 'COM1 또는 /dev/ttyUSB0';
      case 'MQTT':
        return 'mqtt://192.168.1.100:1883';
      case 'BACNET':
        return '192.168.1.100:47808';
      case 'OPCUA':
        return 'opc.tcp://192.168.1.100:4840';
      case 'ETHERNET_IP':
        return '192.168.1.100:44818';
      case 'HTTP_REST':
        return 'http://192.168.1.100/api';
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
                {availableProtocols.find(p => p.protocol_type === displayData?.protocol_type)?.display_name || displayData?.protocol_type || 'N/A'}
              </div>
            ) : (
              <select
                value={editData?.protocol_type || 'MODBUS_TCP'}
                onChange={(e) => onUpdateField('protocol_type', e.target.value)}
                disabled={isLoadingProtocols}
              >
                {isLoadingProtocols ? (
                  <option>로딩 중...</option>
                ) : (
                  availableProtocols.map(protocol => (
                    <option key={protocol.protocol_type} value={protocol.protocol_type}>
                      {protocol.display_name}
                      {protocol.default_port && ` (Port: ${protocol.default_port})`}
                    </option>
                  ))
                )}
              </select>
            )}
          </div>

          <div className="form-group">
            <label>엔드포인트 *</label>
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
                  value={editData?.polling_interval || 1000}
                  onChange={(e) => onUpdateField('polling_interval', parseInt(e.target.value))}
                  min="100"
                  max="60000"
                  step="100"
                />
              )}
            </div>

            <div className="form-group">
              <label>타임아웃 (ms)</label>
              {mode === 'view' ? (
                <div className="form-value">{displayData?.timeout || 'N/A'}</div>
              ) : (
                <input
                  type="number"
                  value={editData?.timeout || 5000}
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
                  checked={editData?.is_enabled || false}
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
            <h3>📅 추가 정보</h3>
            
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