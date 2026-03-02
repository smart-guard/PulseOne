import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { ProtocolApiService } from '../../../api/services/protocolApi';
import './ProtocolModal.css';

// Consolidated Protocol Interface
interface Protocol {
  id: number;
  protocol_type: string;
  display_name: string;
  description: string;
  category?: string;
  default_port?: number;
  uses_serial?: boolean;
  requires_broker?: boolean;
  supported_operations?: string[];
  supported_data_types?: string[];
  connection_params?: Record<string, any>;
  capabilities?: {
    serial?: 'supported' | 'unsupported' | 'required';
    broker?: 'supported' | 'unsupported' | 'required';
  };
  default_polling_interval?: number;
  default_timeout?: number;
  max_concurrent_connections?: number;
  vendor?: string;
  standard_reference?: string;
  is_enabled?: boolean;
  is_deprecated?: boolean;
  min_firmware_version?: string;
}

interface ProtocolEditorProps {
  protocolId?: number;
  mode: 'create' | 'edit' | 'view';
  isOpen: boolean;
  onSave?: (protocol: Protocol) => void;
  onCancel?: () => void;
}

import { useConfirmContext } from '../../common/ConfirmProvider';

interface ProtocolEditorProps {
  protocolId?: number;
  mode: 'create' | 'edit' | 'view';
  isOpen: boolean;
  onSave?: (protocol: Protocol) => void;
  onCancel?: () => void;
}

const ProtocolEditor: React.FC<ProtocolEditorProps> = ({
  protocolId,
  mode,
  isOpen,
  onSave,
  onCancel
}) => {
  const { confirm } = useConfirmContext();

  const [protocol, setProtocol] = useState<Partial<Protocol>>({
    protocol_type: '',
    display_name: '',
    description: '',
    category: 'industrial',
    default_port: undefined,
    uses_serial: false,
    requires_broker: false,
    supported_operations: [],
    supported_data_types: [],
    connection_params: {},
    default_polling_interval: 1000,
    default_timeout: 5000,
    max_concurrent_connections: 1,
    vendor: '',
    standard_reference: '',
    is_enabled: true,
    is_deprecated: false,
    min_firmware_version: ''
  });
    const { t } = useTranslation(['protocols', 'common']);

  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    const handleEsc = (e: KeyboardEvent) => {
      if (e.key === 'Escape' && onCancel) onCancel();
    };
    if (isOpen) {
      document.addEventListener('keydown', handleEsc);
      document.body.style.overflow = 'hidden';
    }
    return () => {
      document.removeEventListener('keydown', handleEsc);
      document.body.style.overflow = 'unset';
    };
  }, [isOpen, onCancel]);

  useEffect(() => {
    if (mode === 'edit' && protocolId && isOpen) {
      loadProtocol();
    }
  }, [mode, protocolId, isOpen]);


  const loadProtocol = async () => {
    try {
      setLoading(true);
      setError(null);
      const response = await ProtocolApiService.getProtocol(protocolId!);
      if (response.success) {
        setProtocol(response.data);
      } else {
        throw new Error(response.message || 'Protocol load failed');
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Protocol load failed';
      setError(errorMessage);
    } finally {
      setLoading(false);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    // 편집 모드에서는 확인 팝업 표시
    if (mode === 'edit') {
      const isConfirmed = await confirm({
        title: 'Confirm Protocol Edit',
        message: `Modify protocol "${protocol.display_name || protocol.protocol_type}"?\n\nChanges take effect immediately and may affect devices using this protocol.`,
        confirmText: 'Save Changes',
        confirmButtonType: 'warning'
      });

      if (isConfirmed) {
        await executeSubmit();
      }
    } else {
      await executeSubmit();
    }
  };

  // 실제 저장 실행 함수
  const executeSubmit = async () => {
    try {
      setSaving(true);
      setError(null);

      if (!protocol.protocol_type || !protocol.display_name) {
        throw new Error('Protocol type and display name are required.');
      }

      let response;
      // System Protocol: Only Edit mode supported
      if (mode === 'edit' && protocolId) {
        response = await ProtocolApiService.updateProtocol(protocolId, protocol);
      } else {
        throw new Error('Unsupported mode.');
      }

      if (response?.success) {
        await confirm({
          title: 'Save Complete',
          message: `Protocol "${protocol.display_name}" settings updated successfully.`,
          confirmText: 'OK',
          showCancelButton: false,
          confirmButtonType: 'primary'
        });

        if (onSave) {
          onSave(response.data);
        }
      } else {
        throw new Error(response?.message || 'Save failed');
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Save failed';
      setError(errorMessage);
    } finally {
      setSaving(false);
    }
  };

  const handleInputChange = (field: keyof Protocol, value: any) => {
    setProtocol(prev => ({
      ...prev,
      [field]: value
    }));
  };

  const handleArrayChange = (field: 'supported_operations' | 'supported_data_types', value: string) => {
    const items = value.split(',').map(item => item.trim()).filter(item => item);
    setProtocol(prev => ({
      ...prev,
      [field]: items
    }));
  };

  const handleConnectionParamsChange = (value: string) => {
    try {
      const params = value ? JSON.parse(value) : {};
      setProtocol(prev => ({
        ...prev,
        connection_params: params
      }));
    } catch (err) {
      // JSON 파싱 에러는 무시
    }
  };

  const handleBrokerParamChange = (key: string, value: any) => {
    setProtocol(prev => {
      const updatedParams = {
        ...(prev.connection_params || {}),
        [key]: value
      };

      // 만약 브로커를 Enabled하는데 API 키가 없으면 자동 생성
      if (key === 'broker_enabled' && value === true && !updatedParams.broker_api_key) {
        updatedParams.broker_api_key = crypto.randomUUID();
        updatedParams.broker_api_key_updated_at = new Date().toISOString();
      }

      return {
        ...prev,
        connection_params: updatedParams
      };
    });
  };

  const handleReissueApiKey = async () => {
    const confirmed = await confirm({
      title: 'Reissue API Key',
      message: 'Issue a new API key? Devices using the existing key may lose connection.',
      confirmButtonType: 'warning'
    });
    if (confirmed) {
      setProtocol(prev => {
        const updatedParams = {
          ...(prev.connection_params || {}),
          broker_api_key: crypto.randomUUID(),
          broker_api_key_updated_at: new Date().toISOString()
        };
        return { ...prev, connection_params: updatedParams };
      });
    }
  };

  const isReadOnly = mode === 'view';
  // System Protocol: Always "Configuration" context
  const title = mode === 'edit' ? 'Edit Protocol Settings' : 'Protocol Details';

  if (!isOpen) return null;

  return (
    <div className="mgmt-modal-overlay">
      <div
        className="mgmt-modal-container mgmt-protocol-modal"
        onClick={(e) => e.stopPropagation()}
      >
        {/* 모달 헤더 */}
        <div className="mgmt-modal-header">
          <div className="mgmt-modal-title">
            <h2>{title}</h2>
          </div>
          <button className="mgmt-close-btn" onClick={onCancel}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        {loading ? (
          <div style={{
            display: 'flex',
            justifyContent: 'center',
            alignItems: 'center',
            height: '200px',
            flexDirection: 'column',
            gap: '16px'
          }}>
            <div>{t('labels.loadingProtocolInfo', {ns: 'protocols'})}</div>
          </div>
        ) : (
          <>
            {error && (
              <div className="mgmt-alert mgmt-alert-danger" style={{ margin: '24px 24px 0 24px' }}>
                {error}
              </div>
            )}

            <div className="mgmt-modal-body">
              <form id="protocol-form" onSubmit={handleSubmit}>
                <div className="mgmt-modal-form-grid">
                  {/* Basic Info */}
                  <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-info-circle"></i> Basic Info</h3>

                    <div className="mgmt-modal-form-row">
                      <div className="mgmt-modal-form-group">
                        <label className="required">{t('labels.protocolType', {ns: 'protocols'})}</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.protocol_type || ''}
                          readOnly={true} // System ID - Always ReadOnly
                          style={{ backgroundColor: 'var(--neutral-100)', cursor: 'not-allowed' }}
                          title="Cannot change. System-defined value."
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label className="required">{t('labels.displayName', {ns: 'protocols'})}</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.display_name || ''}
                          onChange={(e) => handleInputChange('display_name', e.target.value)}
                          readOnly={isReadOnly}
                          placeholder="e.g. Modbus TCP"
                          required
                        />
                      </div>
                    </div>

                    <div className="mgmt-modal-form-group">
                      <label>{t('field.description', {ns: 'protocols'})}</label>
                      <textarea
                        className="mgmt-form-control"
                        value={protocol.description || ''}
                        onChange={(e) => handleInputChange('description', e.target.value)}
                        readOnly={isReadOnly}
                        placeholder="Enter protocol description"
                        rows={3}
                      />
                    </div>

                    <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: '1fr 1fr 1fr' }}>
                      <div className="mgmt-modal-form-group">
                        <label>{t('table.category', {ns: 'protocols'})}</label>
                        <select
                          className="mgmt-form-control"
                          value={protocol.category || ''}
                          onChange={(e) => handleInputChange('category', e.target.value)}
                          disabled={isReadOnly}
                        >
                          <option value="industrial">{t('labels.industrial', {ns: 'protocols'})}</option>
                          <option value="iot">{t('labels.iot', {ns: 'protocols'})}</option>
                          <option value="building_automation">{t('labels.buildingAutomation', {ns: 'protocols'})}</option>
                          <option value="network">{t('labels.network', {ns: 'protocols'})}</option>
                          <option value="web">{t('labels.web', {ns: 'protocols'})}</option>
                        </select>
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>{t('table.defaultPort', {ns: 'protocols'})}</label>
                        <input
                          type="number"
                          className="mgmt-form-control"
                          value={protocol.default_port || ''}
                          onChange={(e) => handleInputChange('default_port', e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="e.g. 502"
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>{t('table.vendor', {ns: 'protocols'})}</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.vendor || ''}
                          onChange={(e) => handleInputChange('vendor', e.target.value)}
                          readOnly={isReadOnly}
                          placeholder="e.g. Modbus Org"
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>{t('labels.minFirmware', {ns: 'protocols'})}</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.min_firmware_version || ''}
                          onChange={(e) => handleInputChange('min_firmware_version', e.target.value)}
                          readOnly={isReadOnly}
                          placeholder="e.g. v1.0.0"
                        />
                      </div>
                    </div>
                  </div>

                  {/* Technical Settings */}
                  <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-cogs"></i> Technical Settings</h3>

                    <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: '1fr 1fr 1fr' }}>
                      <div className="mgmt-modal-form-group">
                        <label>{t('labels.defaultPollingIntervalMs', {ns: 'protocols'})}</label>
                        <input
                          type="number"
                          className="mgmt-form-control"
                          value={protocol.default_polling_interval || ''}
                          onChange={(e) => handleInputChange('default_polling_interval', e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="1000"
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>{t('labels.defaultTimeoutMs', {ns: 'protocols'})}</label>
                        <input
                          type="number"
                          className="mgmt-form-control"
                          value={protocol.default_timeout || ''}
                          onChange={(e) => handleInputChange('default_timeout', e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="5000"
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>{t('labels.maxConcurrentConnections', {ns: 'protocols'})}</label>
                        <input
                          type="number"
                          className="mgmt-form-control"
                          value={protocol.max_concurrent_connections || ''}
                          onChange={(e) => handleInputChange('max_concurrent_connections', e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="1"
                        />
                      </div>
                    </div>

                    <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: '1fr 1fr 1fr' }}>
                      <div className="mgmt-checkbox-group" style={{
                        display: 'flex',
                        flexDirection: 'column',
                        alignItems: 'flex-start',
                        gap: '4px',
                        opacity: (protocol.capabilities?.serial === 'unsupported') ? 0.6 : 1
                      }}>
                        <label className="mgmt-checkbox-label">
                          <input
                            type="checkbox"
                            checked={protocol.uses_serial || protocol.capabilities?.serial === 'required' || false}
                            onChange={(e) => handleInputChange('uses_serial', e.target.checked)}
                            disabled={isReadOnly || protocol.capabilities?.serial === 'unsupported' || protocol.capabilities?.serial === 'required'}
                          />
                          Serial Support
                        </label>
                        <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '-4px', marginLeft: '24px' }}>
                          Whether RS-232/485 communication is required
                          {protocol.capabilities?.serial === 'unsupported' && " (Not Supported)"}
                          {protocol.capabilities?.serial === 'required' && " (Required)"}
                        </small>
                      </div>
                      <div className="mgmt-checkbox-group" style={{
                        display: 'flex',
                        flexDirection: 'column',
                        alignItems: 'flex-start',
                        gap: '4px',
                        opacity: (protocol.capabilities?.broker === 'unsupported') ? 0.6 : 1
                      }}>
                        <label className="mgmt-checkbox-label">
                          <input
                            type="checkbox"
                            checked={protocol.requires_broker || protocol.capabilities?.broker === 'required' || false}
                            onChange={(e) => handleInputChange('requires_broker', e.target.checked)}
                            disabled={isReadOnly || protocol.capabilities?.broker === 'unsupported' || protocol.capabilities?.broker === 'required'}
                          />
                          Broker Required
                        </label>
                        <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '-4px', marginLeft: '24px' }}>
                          Whether an MQTT server or relay is required
                          {protocol.capabilities?.broker === 'unsupported' && " (Not Supported)"}
                          {protocol.capabilities?.broker === 'required' && " (Required)"}
                        </small>
                      </div>
                      <div className="mgmt-checkbox-group">
                        <label className="mgmt-checkbox-label">
                          <input
                            type="checkbox"
                            checked={protocol.is_enabled || false}
                            onChange={(e) => handleInputChange('is_enabled', e.target.checked)}
                            disabled={isReadOnly}
                          />
                          Enabled
                        </label>
                      </div>
                      <div className="mgmt-checkbox-group">
                        <label className="mgmt-checkbox-label">
                          <input
                            type="checkbox"
                            checked={protocol.is_deprecated || false}
                            onChange={(e) => handleInputChange('is_deprecated', e.target.checked)}
                            disabled={isReadOnly}
                          />
                          Deprecated
                        </label>
                      </div>
                    </div>
                  </div>

                  {/* MQTT 전용 브로커 설정 - 상세 대시보드로 이동 */}
                  {protocol.protocol_type === 'MQTT' && (
                    <div className="mgmt-modal-form-section" style={{ border: '2px dashed var(--primary-200)', padding: '24px', borderRadius: '8px', backgroundColor: 'var(--primary-25)', textAlign: 'center' }}>
                      <h3 style={{ color: 'var(--primary-600)', marginBottom: '12px' }}><i className="fas fa-server"></i> Broker Settings Guide</h3>
                      <p style={{ fontSize: '13px', color: 'var(--neutral-600)', lineHeight: '1.6', marginTop: '8px' }}>
                        PulseOne MQTT broker and real-time monitoring features have been<br />
                        <strong>{t('labels.communicationDashboard', {ns: 'protocols'})}</strong>integrated.
                      </p>
                      <div style={{ marginTop: '16px', fontSize: '12px', color: 'var(--primary-600)', fontWeight: 600 }}>
                        (Select MQTT from the protocol list to enter the dashboard)
                      </div>
                    </div>
                  )}

                  {/* 3 & 4. 사이드-바이-사이드 도메인 레이아웃 */}

                  <div className="mgmt-modal-form-domains">
                    {/* 드라이버 역량 (Capabilities) */}
                    <div className="mgmt-modal-form-domain">
                      <div className="mgmt-modal-form-section">
                        <h3><i className="fas fa-microchip"></i> Driver Capabilities</h3>
                        <div className="mgmt-modal-form-group">
                          <label>지원 명령어 (쉼표로 구분)</label>
                          <div className="mgmt-capability-badge-container" style={{ marginBottom: '8px' }}>
                            {protocol.supported_operations?.map((op, i) => (
                              <span key={i} className="mgmt-capability-badge">{op}</span>
                            ))}
                          </div>
                          <input
                            type="text"
                            className="mgmt-form-control"
                            value={protocol.supported_operations?.join(', ') || ''}
                            onChange={(e) => handleArrayChange('supported_operations', e.target.value)}
                            readOnly={isReadOnly}
                            placeholder="예: read, write"
                          />
                        </div>
                        <div className="mgmt-modal-form-group" style={{ marginBottom: 0 }}>
                          <label>지원 데이터 타입 (쉼표로 구분)</label>
                          <div className="mgmt-capability-badge-container" style={{ marginBottom: '8px' }}>
                            {protocol.supported_data_types?.map((type, i) => (
                              <span key={i} className="mgmt-capability-badge">{type}</span>
                            ))}
                          </div>
                          <input
                            type="text"
                            className="mgmt-form-control"
                            value={protocol.supported_data_types?.join(', ') || ''}
                            onChange={(e) => handleArrayChange('supported_data_types', e.target.value)}
                            readOnly={isReadOnly}
                            placeholder="예: BOOL, INT16, FLOAT32"
                          />
                        </div>
                      </div>
                    </div>

                    {/* 연결 파라미터 (JSON) */}
                    <div className="mgmt-modal-form-domain">
                      <div className="mgmt-modal-form-section">
                        <h3><i className="fas fa-code"></i> 연결 파라미터</h3>
                        <div className="mgmt-modal-form-group" style={{ marginBottom: 0 }}>
                          <label>JSON 설정</label>
                          <textarea
                            className="mgmt-form-control"
                            value={JSON.stringify(protocol.connection_params || {}, null, 2)}
                            onChange={(e) => handleConnectionParamsChange(e.target.value)}
                            readOnly={isReadOnly}
                            placeholder='{"host": "127.0.0.1", "port": 502}'
                            rows={8}
                            style={{
                              fontFamily: 'monospace',
                              fontSize: '12px',
                              backgroundColor: 'var(--neutral-50)',
                              border: '1px solid var(--neutral-200)'
                            }}
                          />
                        </div>
                      </div>
                    </div>
                  </div>
                </div>
              </form>
            </div>
          </>
        )
        }

        {/* 모달 푸터 */}
        <div className="mgmt-modal-footer">
          <button type="button" className="mgmt-btn mgmt-btn-outline" style={{ width: 'auto', minWidth: '100px' }} onClick={onCancel} disabled={saving}>
            취소
          </button>
          {!isReadOnly && (
            <button
              type="submit"
              form="protocol-form"
              className="mgmt-btn mgmt-btn-primary"
              style={{ width: 'auto', minWidth: '120px' }}
              disabled={saving}
            >
              {saving ? '저장 중...' : '저장하기'}
            </button>
          )}
        </div>
      </div >
    </div >

  );
};

export default ProtocolEditor;