import React, { useState, useEffect } from 'react';
import { ProtocolApiService } from '../../../api/services/protocolApi';

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

const ProtocolEditor: React.FC<ProtocolEditorProps> = ({ 
  protocolId, 
  mode, 
  isOpen,
  onSave, 
  onCancel 
}) => {
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

  // ESC 키로 모달 닫기
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

  // 편집 모드에서 기존 데이터 로드
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
    
    try {
      setSaving(true);
      setError(null);

      // 필수 필드 검증
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
        if (onSave) {
          onSave(response.data);
        }
        alert(mode === 'create' ? '프로토콜이 생성되었습니다.' : '프로토콜이 수정되었습니다.');
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
      // JSON 파싱 에러는 무시 (사용자가 입력 중일 수 있음)
    }
  };

  const isReadOnly = mode === 'view';
  const title = mode === 'create' ? '새 프로토콜 등록' : mode === 'edit' ? '프로토콜 편집' : '프로토콜 상세보기';

  if (!isOpen) return null;

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.5)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 9999,
      padding: '20px'
    }}>
      <div 
        style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          width: '95vw',
          maxWidth: '1000px',
          height: '90vh',
          maxHeight: '800px',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          boxShadow: '0 25px 50px rgba(0, 0, 0, 0.25)'
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* 모달 헤더 */}
        <div style={{
          padding: '24px',
          borderBottom: '1px solid #e5e7eb',
          backgroundColor: '#f8fafc',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          flexShrink: 0
        }}>
          <h2 style={{
            margin: 0,
            fontSize: '24px',
            fontWeight: '700',
            color: '#1e293b'
          }}>
            {title}
          </h2>
          <button
            onClick={onCancel}
            style={{
              background: 'none',
              border: 'none',
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              color: '#6b7280',
              cursor: 'pointer',
              fontSize: '20px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#f3f4f6';
              e.currentTarget.style.color = '#374151';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#6b7280';
            }}
          >
            ✕
          </button>
        </div>

        {/* 모달 콘텐츠 */}
        <div style={{
          flex: 1,
          overflow: 'auto',
          padding: '24px'
        }}>
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
              {/* 에러 표시 */}
              {error && (
                <div style={{
                  backgroundColor: '#fee2e2',
                  border: '1px solid #fecaca',
                  borderRadius: '8px',
                  padding: '12px',
                  marginBottom: '24px',
                  color: '#dc2626'
                }}>
                  {error}
                </div>
              )}

              {/* 폼 */}
              <form onSubmit={handleSubmit}>
                {/* 기본 정보 */}
                <div style={{ marginBottom: '32px' }}>
                  <h3 style={{
                    fontSize: '18px',
                    fontWeight: '600',
                    color: '#1e293b',
                    margin: 0,
                    marginBottom: '16px',
                    paddingBottom: '8px',
                    borderBottom: '1px solid #e2e8f0'
                  }}>
                    기본 정보
                  </h3>

                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(2, 1fr)',
                    gap: '16px',
                    marginBottom: '16px'
                  }}>
                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        프로토콜 타입 *
                      </label>
                      <input
                        type="text"
                        value={protocol.protocol_type || ''}
                        onChange={(e) => handleInputChange('protocol_type', e.target.value)}
                        readOnly={isReadOnly || (mode === 'edit')}
                        placeholder="예: MODBUS_TCP"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: (isReadOnly || (mode === 'edit')) ? '#f9fafb' : 'white'
                        }}
                        required
                      />
                    </div>

                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        표시명 *
                      </label>
                      <input
                        type="text"
                        value={protocol.display_name || ''}
                        onChange={(e) => handleInputChange('display_name', e.target.value)}
                        readOnly={isReadOnly}
                        placeholder="예: Modbus TCP"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                        required
                      />
                    </div>
                  </div>

                  <div style={{ marginBottom: '16px' }}>
                    <label style={{
                      display: 'block',
                      marginBottom: '4px',
                      fontSize: '14px',
                      fontWeight: '500',
                      color: '#374151'
                    }}>
                      설명
                    </label>
                    <textarea
                      value={protocol.description || ''}
                      onChange={(e) => handleInputChange('description', e.target.value)}
                      readOnly={isReadOnly}
                      placeholder="프로토콜에 대한 설명을 입력하세요"
                      rows={3}
                      style={{
                        width: '100%',
                        padding: '8px 12px',
                        border: '1px solid #d1d5db',
                        borderRadius: '6px',
                        fontSize: '14px',
                        resize: 'vertical',
                        backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                      }}
                    />
                  </div>

                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(3, 1fr)',
                    gap: '16px'
                  }}>
                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        카테고리
                      </label>
                      <select
                        value={protocol.category || ''}
                        onChange={(e) => handleInputChange('category', e.target.value)}
                        disabled={isReadOnly}
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      >
                        <option value="industrial">산업용</option>
                        <option value="iot">IoT</option>
                        <option value="building_automation">빌딩 자동화</option>
                        <option value="web">웹</option>
                        <option value="network">네트워크</option>
                      </select>
                    </div>

                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        기본 포트
                      </label>
                      <input
                        type="number"
                        value={protocol.default_port || ''}
                        onChange={(e) => handleInputChange('default_port', e.target.value ? parseInt(e.target.value) : null)}
                        readOnly={isReadOnly}
                        placeholder="예: 502"
                        min="1"
                        max="65535"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>

                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        제조사/벤더
                      </label>
                      <input
                        type="text"
                        value={protocol.vendor || ''}
                        onChange={(e) => handleInputChange('vendor', e.target.value)}
                        readOnly={isReadOnly}
                        placeholder="예: Modbus Organization"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>
                  </div>
                </div>

                {/* 기술 설정 */}
                <div style={{ marginBottom: '32px' }}>
                  <h3 style={{
                    fontSize: '18px',
                    fontWeight: '600',
                    color: '#1e293b',
                    margin: 0,
                    marginBottom: '16px',
                    paddingBottom: '8px',
                    borderBottom: '1px solid #e2e8f0'
                  }}>
                    기술 설정
                  </h3>

                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(3, 1fr)',
                    gap: '16px',
                    marginBottom: '16px'
                  }}>
                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        폴링 간격 (ms)
                      </label>
                      <input
                        type="number"
                        value={protocol.default_polling_interval || ''}
                        onChange={(e) => handleInputChange('default_polling_interval', parseInt(e.target.value))}
                        readOnly={isReadOnly}
                        placeholder="1500"
                        min="100"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>

                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        타임아웃 (ms)
                      </label>
                      <input
                        type="number"
                        value={protocol.default_timeout || ''}
                        onChange={(e) => handleInputChange('default_timeout', parseInt(e.target.value))}
                        readOnly={isReadOnly}
                        placeholder="5000"
                        min="1000"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>

                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        최대 동시 연결
                      </label>
                      <input
                        type="number"
                        value={protocol.max_concurrent_connections || ''}
                        onChange={(e) => handleInputChange('max_concurrent_connections', parseInt(e.target.value))}
                        readOnly={isReadOnly}
                        placeholder="1"
                        min="1"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>
                  </div>

                  <div style={{
                    display: 'flex',
                    gap: '24px',
                    marginBottom: '16px',
                    flexWrap: 'wrap'
                  }}>
                    <label style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      fontSize: '14px',
                      color: '#374151',
                      cursor: isReadOnly ? 'default' : 'pointer'
                    }}>
                      <input
                        type="checkbox"
                        checked={protocol.uses_serial || false}
                        onChange={(e) => handleInputChange('uses_serial', e.target.checked)}
                        disabled={isReadOnly}
                        style={{ cursor: isReadOnly ? 'default' : 'pointer' }}
                      />
                      시리얼 통신 사용
                    </label>

                    <label style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      fontSize: '14px',
                      color: '#374151',
                      cursor: isReadOnly ? 'default' : 'pointer'
                    }}>
                      <input
                        type="checkbox"
                        checked={protocol.requires_broker || false}
                        onChange={(e) => handleInputChange('requires_broker', e.target.checked)}
                        disabled={isReadOnly}
                        style={{ cursor: isReadOnly ? 'default' : 'pointer' }}
                      />
                      브로커 필요
                    </label>

                    <label style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      fontSize: '14px',
                      color: '#374151',
                      cursor: isReadOnly ? 'default' : 'pointer'
                    }}>
                      <input
                        type="checkbox"
                        checked={protocol.is_enabled || false}
                        onChange={(e) => handleInputChange('is_enabled', e.target.checked)}
                        disabled={isReadOnly}
                        style={{ cursor: isReadOnly ? 'default' : 'pointer' }}
                      />
                      활성화
                    </label>
                  </div>

                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(2, 1fr)',
                    gap: '16px'
                  }}>
                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        지원 명령어 (쉼표로 구분)
                      </label>
                      <input
                        type="text"
                        value={protocol.supported_operations?.join(', ') || ''}
                        onChange={(e) => handleArrayChange('supported_operations', e.target.value)}
                        readOnly={isReadOnly}
                        placeholder="예: read_coils, read_discrete_inputs, read_holding_registers"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>

                    <div>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '14px',
                        fontWeight: '500',
                        color: '#374151'
                      }}>
                        지원 데이터 타입 (쉼표로 구분)
                      </label>
                      <input
                        type="text"
                        value={protocol.supported_data_types?.join(', ') || ''}
                        onChange={(e) => handleArrayChange('supported_data_types', e.target.value)}
                        readOnly={isReadOnly}
                        placeholder="예: boolean, int16, uint16, int32, uint32, float32"
                        style={{
                          width: '100%',
                          padding: '8px 12px',
                          border: '1px solid #d1d5db',
                          borderRadius: '6px',
                          fontSize: '14px',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>
                  </div>
                </div>

                {/* 고급 설정 */}
                <div style={{ marginBottom: '32px' }}>
                  <h3 style={{
                    fontSize: '18px',
                    fontWeight: '600',
                    color: '#1e293b',
                    margin: 0,
                    marginBottom: '16px',
                    paddingBottom: '8px',
                    borderBottom: '1px solid #e2e8f0'
                  }}>
                    고급 설정
                  </h3>

                  <div style={{ marginBottom: '16px' }}>
                    <label style={{
                      display: 'block',
                      marginBottom: '4px',
                      fontSize: '14px',
                      fontWeight: '500',
                      color: '#374151'
                    }}>
                      연결 파라미터 (JSON 형식)
                    </label>
                    <textarea
                      value={JSON.stringify(protocol.connection_params || {}, null, 2)}
                      onChange={(e) => handleConnectionParamsChange(e.target.value)}
                      readOnly={isReadOnly}
                      placeholder='{"host": {"type": "string", "required": true}}'
                      rows={4}
                      style={{
                        width: '100%',
                        padding: '8px 12px',
                        border: '1px solid #d1d5db',
                        borderRadius: '6px',
                        fontSize: '12px',
                        fontFamily: 'monospace',
                        resize: 'vertical',
                        backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                      }}
                    />
                  </div>

                  <div>
                    <label style={{
                      display: 'block',
                      marginBottom: '4px',
                      fontSize: '14px',
                      fontWeight: '500',
                      color: '#374151'
                    }}>
                      표준 참조
                    </label>
                    <input
                      type="text"
                      value={protocol.standard_reference || ''}
                      onChange={(e) => handleInputChange('standard_reference', e.target.value)}
                      readOnly={isReadOnly}
                      placeholder="예: MODBUS Application Protocol Specification V1.1b3"
                      style={{
                        width: '100%',
                        padding: '8px 12px',
                        border: '1px solid #d1d5db',
                        borderRadius: '6px',
                        fontSize: '14px',
                        backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                      }}
                    />
                  </div>
                </div>
              </form>
            </>
          )}
        </div>

        {/* 모달 푸터 */}
        {!isReadOnly && (
          <div style={{
            padding: '20px 24px',
            borderTop: '1px solid #e5e7eb',
            backgroundColor: '#f8fafc',
            display: 'flex',
            justifyContent: 'flex-end',
            gap: '12px',
            flexShrink: 0
          }}>
            <button
              type="button"
              onClick={onCancel}
              disabled={saving}
              style={{
                padding: '12px 24px',
                backgroundColor: '#f3f4f6',
                border: '1px solid #d1d5db',
                borderRadius: '8px',
                fontSize: '14px',
                fontWeight: '500',
                cursor: saving ? 'not-allowed' : 'pointer',
                color: '#374151',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => {
                if (!saving) {
                  e.currentTarget.style.backgroundColor = '#e5e7eb';
                }
              }}
              onMouseLeave={(e) => {
                if (!saving) {
                  e.currentTarget.style.backgroundColor = '#f3f4f6';
                }
              }}
            >
              취소
            </button>
            <button
              type="submit"
              onClick={(e) => {
                const form = e.currentTarget.closest('.modal-content')?.querySelector('form') as HTMLFormElement;
                if (form) {
                  const submitEvent = new Event('submit', { cancelable: true, bubbles: true });
                  form.dispatchEvent(submitEvent);
                }
              }}
              disabled={saving}
              style={{
                padding: '12px 24px',
                backgroundColor: saving ? '#9ca3af' : '#3b82f6',
                color: 'white',
                border: 'none',
                borderRadius: '8px',
                fontSize: '14px',
                fontWeight: '500',
                cursor: saving ? 'not-allowed' : 'pointer',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => {
                if (!saving) {
                  e.currentTarget.style.backgroundColor = '#2563eb';
                }
              }}
              onMouseLeave={(e) => {
                if (!saving) {
                  e.currentTarget.style.backgroundColor = '#3b82f6';
                }
              }}
            >
              {saving ? '저장 중...' : mode === 'create' ? '등록하기' : '수정하기'}
            </button>
          </div>
        )}
      </div>
    </div>
  );
};

export default ProtocolEditor;