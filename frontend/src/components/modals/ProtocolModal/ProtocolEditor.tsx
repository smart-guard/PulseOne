import React, { useState, useEffect } from 'react';
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
        throw new Error(response.message || '프로토콜 로드 실패');
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : '프로토콜 로드 실패';
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
        title: '프로토콜 수정 확인',
        message: `프로토콜 "${protocol.display_name || protocol.protocol_type}"을(를) 수정하시겠습니까?\n\n수정된 설정은 즉시 적용되며, 이 프로토콜을 사용하는 디바이스들에게 영향을 줄 수 있습니다.`,
        confirmText: '수정하기',
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
        throw new Error('프로토콜 타입과 표시명은 필수입니다.');
      }

      let response;
      if (mode === 'create') {
        response = await ProtocolApiService.createProtocol(protocol as any);
      } else if (mode === 'edit' && protocolId) {
        response = await ProtocolApiService.updateProtocol(protocolId, protocol);
      }

      if (response?.success) {
        await confirm({
          title: '저장 완료',
          message: mode === 'create'
            ? `프로토콜 "${protocol.display_name}"이(가) 성공적으로 생성되었습니다.`
            : `프로토콜 "${protocol.display_name}"이(가) 성공적으로 수정되었습니다.`,
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'primary'
        });

        if (onSave) {
          onSave(response.data);
        }
      } else {
        throw new Error(response?.message || '저장 실패');
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : '저장 실패';
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

  const isReadOnly = mode === 'view';
  const title = mode === 'create' ? '새 프로토콜 등록' : mode === 'edit' ? '프로토콜 편집' : '프로토콜 상세보기';

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
            <div>프로토콜 정보를 불러오는 중...</div>
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
                  {/* 기본 정보 */}
                  <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>

                    <div className="mgmt-modal-form-row">
                      <div className="mgmt-modal-form-group">
                        <label className="required">프로토콜 타입</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.protocol_type || ''}
                          onChange={(e) => handleInputChange('protocol_type', e.target.value)}
                          readOnly={isReadOnly || (mode === 'edit')}
                          placeholder="예: MODBUS_TCP"
                          required
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label className="required">표시명</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.display_name || ''}
                          onChange={(e) => handleInputChange('display_name', e.target.value)}
                          readOnly={isReadOnly}
                          placeholder="예: Modbus TCP"
                          required
                        />
                      </div>
                    </div>

                    <div className="mgmt-modal-form-group">
                      <label>설명</label>
                      <textarea
                        className="mgmt-form-control"
                        value={protocol.description || ''}
                        onChange={(e) => handleInputChange('description', e.target.value)}
                        readOnly={isReadOnly}
                        placeholder="프로토콜에 대한 내용을 입력하세요"
                        rows={3}
                      />
                    </div>

                    <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: '1fr 1fr 1fr' }}>
                      <div className="mgmt-modal-form-group">
                        <label>카테고리</label>
                        <select
                          className="mgmt-form-control"
                          value={protocol.category || ''}
                          onChange={(e) => handleInputChange('category', e.target.value)}
                          disabled={isReadOnly}
                        >
                          <option value="industrial">산업용</option>
                          <option value="iot">IoT</option>
                          <option value="building_automation">빌딩 자동화</option>
                          <option value="network">네트워크</option>
                          <option value="web">웹</option>
                        </select>
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>기본 포트</label>
                        <input
                          type="number"
                          className="mgmt-form-control"
                          value={protocol.default_port || ''}
                          onChange={(e) => handleInputChange('default_port', e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="예: 502"
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>제조사/벤더</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.vendor || ''}
                          onChange={(e) => handleInputChange('vendor', e.target.value)}
                          readOnly={isReadOnly}
                          placeholder="예: Modbus Org"
                        />
                      </div>
                      <div className="mgmt-modal-form-group">
                        <label>최소 펌웨어</label>
                        <input
                          type="text"
                          className="mgmt-form-control"
                          value={protocol.min_firmware_version || ''}
                          onChange={(e) => handleInputChange('min_firmware_version', e.target.value)}
                          readOnly={isReadOnly}
                          placeholder="예: v1.0.0"
                        />
                      </div>
                    </div>
                  </div>

                  {/* 기술 설정 */}
                  <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-cogs"></i> 기술 설정</h3>

                    <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: '1fr 1fr 1fr' }}>
                      <div className="mgmt-modal-form-group">
                        <label>기본 폴링 주기 (ms)</label>
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
                        <label>기본 타임아웃 (ms)</label>
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
                        <label>최대 동시 연결 수</label>
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
                          시리얼 사용
                        </label>
                        <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '-4px', marginLeft: '24px' }}>
                          RS-232/485 통신 필요 여부
                          {protocol.capabilities?.serial === 'unsupported' && " (미지원)"}
                          {protocol.capabilities?.serial === 'required' && " (필수)"}
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
                          브로커 필요
                        </label>
                        <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '-4px', marginLeft: '24px' }}>
                          MQTT 서버 등 중계기 필요 여부
                          {protocol.capabilities?.broker === 'unsupported' && " (미지원)"}
                          {protocol.capabilities?.broker === 'required' && " (필수)"}
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
                          활성화
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
                          사용 중단 예정
                        </label>
                      </div>
                    </div>
                  </div>

                  {/* 3 & 4. 사이드-바이-사이드 도메인 레이아웃 */}
                  <div className="mgmt-modal-form-domains">
                    {/* 드라이버 역량 (Capabilities) */}
                    <div className="mgmt-modal-form-domain">
                      <div className="mgmt-modal-form-section">
                        <h3><i className="fas fa-microchip"></i> 드라이버 역량</h3>
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
        )}

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
              {saving ? '저장 중...' : mode === 'create' ? '등록하기' : '수정하기'}
            </button>
          )}
        </div>
      </div>
    </div>

  );
};

export default ProtocolEditor;