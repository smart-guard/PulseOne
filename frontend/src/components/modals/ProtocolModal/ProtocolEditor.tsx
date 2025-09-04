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

// 팝업 확인 다이얼로그 인터페이스
interface ConfirmDialogState {
  isOpen: boolean;
  title: string;
  message: string;
  confirmText: string;
  cancelText: string;
  onConfirm: () => void;
  onCancel: () => void;
  type: 'warning' | 'danger' | 'info';
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

  // 팝업 확인 다이얼로그 상태
  const [confirmDialog, setConfirmDialog] = useState<ConfirmDialogState>({
    isOpen: false,
    title: '',
    message: '',
    confirmText: '확인',
    cancelText: '취소',
    onConfirm: () => {},
    onCancel: () => {},
    type: 'info'
  });

  // 성공 메시지 상태
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

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

  // 성공 메시지 자동 제거
  useEffect(() => {
    if (successMessage) {
      const timer = setTimeout(() => setSuccessMessage(null), 3000);
      return () => clearTimeout(timer);
    }
  }, [successMessage]);

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
        const successMessage = mode === 'create' 
          ? `프로토콜 "${protocol.display_name}"이(가) 성공적으로 생성되었습니다.` 
          : `프로토콜 "${protocol.display_name}"이(가) 성공적으로 수정되었습니다.`;
        
        setSuccessMessage(successMessage);
        
        // 2초 후 모달 닫기 및 콜백 실행
        setTimeout(() => {
          if (onSave) {
            onSave(response.data);
          }
        }, 2000);
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

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    
    // 편집 모드에서는 확인 팝업 표시
    if (mode === 'edit') {
      setConfirmDialog({
        isOpen: true,
        title: '프로토콜 수정 확인',
        message: `프로토콜 "${protocol.display_name || protocol.protocol_type}"을(를) 수정하시겠습니까?\n\n수정된 설정은 즉시 적용되며, 이 프로토콜을 사용하는 디바이스들에게 영향을 줄 수 있습니다.`,
        confirmText: '수정하기',
        cancelText: '취소',
        type: 'warning',
        onConfirm: () => {
          setConfirmDialog(prev => ({ ...prev, isOpen: false }));
          executeSubmit();
        },
        onCancel: () => {
          setConfirmDialog(prev => ({ ...prev, isOpen: false }));
        }
      });
    } else {
      // 생성 모드는 바로 실행
      executeSubmit();
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

  // 확인 다이얼로그 컴포넌트
  const ConfirmDialog: React.FC<{ config: ConfirmDialogState }> = ({ config }) => {
    if (!config.isOpen) return null;

    const getDialogColor = (type: string) => {
      switch (type) {
        case 'danger': return '#ef4444';
        case 'warning': return '#f59e0b';
        case 'info': 
        default: return '#3b82f6';
      }
    };

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
        zIndex: 10001
      }}>
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          minWidth: '400px',
          maxWidth: '500px',
          boxShadow: '0 25px 50px rgba(0, 0, 0, 0.25)'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
            marginBottom: '16px'
          }}>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '50%',
              backgroundColor: `${getDialogColor(config.type)}20`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>
              {config.type === 'warning' ? '⚠️' : config.type === 'danger' ? '🚨' : 'ℹ️'}
            </div>
            <h3 style={{
              margin: 0,
              fontSize: '18px',
              fontWeight: '600',
              color: '#1e293b'
            }}>
              {config.title}
            </h3>
          </div>
          
          <p style={{
            margin: 0,
            marginBottom: '24px',
            fontSize: '14px',
            color: '#64748b',
            lineHeight: '1.5',
            whiteSpace: 'pre-line'
          }}>
            {config.message}
          </p>

          <div style={{
            display: 'flex',
            justifyContent: 'flex-end',
            gap: '12px'
          }}>
            <button
              onClick={config.onCancel}
              style={{
                padding: '10px 20px',
                backgroundColor: '#f3f4f6',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                fontSize: '14px',
                fontWeight: '500',
                color: '#374151',
                cursor: 'pointer'
              }}
            >
              {config.cancelText}
            </button>
            <button
              onClick={config.onConfirm}
              style={{
                padding: '10px 20px',
                backgroundColor: getDialogColor(config.type),
                color: 'white',
                border: 'none',
                borderRadius: '6px',
                fontSize: '14px',
                fontWeight: '500',
                cursor: 'pointer'
              }}
            >
              {config.confirmText}
            </button>
          </div>
        </div>
      </div>
    );
  };

  // 성공 메시지 컴포넌트
  const SuccessMessage: React.FC<{ message: string }> = ({ message }) => (
    <div style={{
      position: 'fixed',
      top: '50%',
      left: '50%',
      transform: 'translate(-50%, -50%)',
      backgroundColor: '#dcfce7',
      border: '2px solid #16a34a',
      borderRadius: '12px',
      padding: '24px 32px',
      color: '#166534',
      fontSize: '16px',
      fontWeight: '600',
      zIndex: 10002,
      boxShadow: '0 25px 50px rgba(0, 0, 0, 0.25)',
      display: 'flex',
      alignItems: 'center',
      gap: '12px',
      minWidth: '400px',
      textAlign: 'center'
    }}>
      <div style={{ fontSize: '24px' }}>✅</div>
      <div>{message}</div>
    </div>
  );

  console.log('🔥 ProtocolEditor Debug:', { mode, isReadOnly: mode === 'view', protocolId });
  
  const isReadOnly = mode === 'view';
  const title = mode === 'create' ? '새 프로토콜 등록' : mode === 'edit' ? '프로토콜 편집' : '프로토콜 상세보기';

  if (!isOpen) return null;

  return (
    <>
      {/* 성공 메시지 */}
      {successMessage && <SuccessMessage message={successMessage} />}

      {/* 확인 다이얼로그 */}
      <ConfirmDialog config={confirmDialog} />

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
                fontSize: '20px'
              }}
            >
              ✕
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
                <div style={{
                  backgroundColor: '#fee2e2',
                  border: '1px solid #fecaca',
                  borderRadius: '8px',
                  padding: '12px',
                  margin: '24px 24px 0 24px',
                  color: '#dc2626'
                }}>
                  {error}
                </div>
              )}

              <form onSubmit={handleSubmit} style={{ 
                display: 'flex', 
                flexDirection: 'column',
                height: '100%'
              }}>
                {/* 스크롤 가능한 컨텐츠 영역 */}
                <div style={{
                  height: '630px',
                  overflowY: 'auto',
                  overflowX: 'hidden',
                  padding: '16px'
                }}>
                  {/* 기본 정보 */}
                  <div style={{ marginBottom: '24px' }}>
                    <h3 style={{
                      fontSize: '16px',
                      fontWeight: '600',
                      color: '#1e293b',
                      margin: 0,
                      marginBottom: '12px',
                      paddingBottom: '6px',
                      borderBottom: '1px solid #e2e8f0'
                    }}>
                      기본 정보
                    </h3>

                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: 'repeat(2, 1fr)',
                      gap: '12px',
                      marginBottom: '12px'
                    }}>
                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
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
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: (isReadOnly || (mode === 'edit')) ? '#f9fafb' : 'white'
                          }}
                          required
                        />
                      </div>

                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
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
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                          required
                        />
                      </div>
                    </div>

                    <div style={{ marginBottom: '12px' }}>
                      <label style={{
                        display: 'block',
                        marginBottom: '4px',
                        fontSize: '13px',
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
                        rows={2}
                        style={{
                          width: '100%',
                          padding: '6px 10px',
                          border: '1px solid #d1d5db',
                          borderRadius: '4px',
                          fontSize: '13px',
                          resize: 'vertical',
                          backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                        }}
                      />
                    </div>

                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: 'repeat(3, 1fr)',
                      gap: '12px',
                      marginBottom: '12px'
                    }}>
                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
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
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        >
                          <option value="industrial">산업용</option>
                          <option value="iot">IoT</option>
                          <option value="building_automation">빌딩 자동화</option>
                          <option value="network">네트워크</option>
                          <option value="web">웹</option>
                        </select>
                      </div>

                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
                          fontWeight: '500',
                          color: '#374151'
                        }}>
                          기본 포트
                        </label>
                        <input
                          type="number"
                          value={protocol.default_port || ''}
                          onChange={(e) => handleInputChange('default_port', e.target.value ? 
                            parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="예: 502"
                          min="1"
                          max="65535"
                          style={{
                            width: '100%',
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        />
                      </div>

                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
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
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        />
                      </div>
                    </div>
                  </div>

                  {/* 기술 설정 */}
                  <div style={{ marginBottom: '24px' }}>
                    <h3 style={{
                      fontSize: '16px',
                      fontWeight: '600',
                      color: '#1e293b',
                      margin: 0,
                      marginBottom: '12px',
                      paddingBottom: '6px',
                      borderBottom: '1px solid #e2e8f0'
                    }}>
                      기술 설정
                    </h3>

                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: 'repeat(3, 1fr)',
                      gap: '12px',
                      marginBottom: '12px'
                    }}>
                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
                          fontWeight: '500',
                          color: '#374151'
                        }}>
                          기본 폴링 주기 (ms)
                        </label>
                        <input
                          type="number"
                          value={protocol.default_polling_interval || ''}
                          onChange={(e) => handleInputChange('default_polling_interval', 
                            e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="1000"
                          min="100"
                          style={{
                            width: '100%',
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        />
                      </div>

                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
                          fontWeight: '500',
                          color: '#374151'
                        }}>
                          기본 타임아웃 (ms)
                        </label>
                        <input
                          type="number"
                          value={protocol.default_timeout || ''}
                          onChange={(e) => handleInputChange('default_timeout', 
                            e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="5000"
                          min="1000"
                          style={{
                            width: '100%',
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        />
                      </div>

                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
                          fontWeight: '500',
                          color: '#374151'
                        }}>
                          최대 동시 연결 수
                        </label>
                        <input
                          type="number"
                          value={protocol.max_concurrent_connections || ''}
                          onChange={(e) => handleInputChange('max_concurrent_connections', 
                            e.target.value ? parseInt(e.target.value) : null)}
                          readOnly={isReadOnly}
                          placeholder="1"
                          min="1"
                          max="100"
                          style={{
                            width: '100%',
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        />
                      </div>
                    </div>

                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: 'repeat(3, 1fr)',
                      gap: '12px',
                      marginBottom: '12px'
                    }}>
                      <label style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                        fontSize: '13px',
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
                        시리얼 사용
                      </label>

                      <label style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                        fontSize: '13px',
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
                        gap: '6px',
                        fontSize: '13px',
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
                      gap: '12px'
                    }}>
                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
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
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        />
                      </div>

                      <div>
                        <label style={{
                          display: 'block',
                          marginBottom: '4px',
                          fontSize: '13px',
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
                            padding: '6px 10px',
                            border: '1px solid #d1d5db',
                            borderRadius: '4px',
                            fontSize: '13px',
                            backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                          }}
                        />
                      </div>
                    </div>
                  </div>

                  {/* 연결 파라미터 */}
                  <div style={{ marginBottom: '24px' }}>
                    <h3 style={{
                      fontSize: '16px',
                      fontWeight: '600',
                      color: '#1e293b',
                      margin: 0,
                      marginBottom: '12px',
                      paddingBottom: '6px',
                      borderBottom: '1px solid #e2e8f0'
                    }}>
                      연결 파라미터 (JSON)
                    </h3>
                    <textarea
                      value={JSON.stringify(protocol.connection_params || {}, null, 2)}
                      onChange={(e) => handleConnectionParamsChange(e.target.value)}
                      readOnly={isReadOnly}
                      placeholder='{"host": "127.0.0.1", "port": 502, "slave_id": 1}'
                      rows={4}
                      style={{
                        width: '100%',
                        padding: '8px 10px',
                        border: '1px solid #d1d5db',
                        borderRadius: '4px',
                        fontSize: '12px',
                        fontFamily: 'monospace',
                        resize: 'vertical',
                        backgroundColor: isReadOnly ? '#f9fafb' : 'white'
                      }}
                    />
                  </div>
                </div>

                {/* 모달 푸터 - 절대 위치로 고정 */}
                {!isReadOnly && (
                  <div style={{
                    position: 'sticky',
                    bottom: 0,
                    padding: '16px 20px',
                    borderTop: '1px solid #e5e7eb',
                    backgroundColor: '#f8fafc',
                    display: 'flex',
                    justifyContent: 'flex-end',
                    gap: '12px',
                    zIndex: 10
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
                        color: '#374151'
                      }}
                    >
                      취소
                    </button>
                    <button
                      type="submit"
                      disabled={saving}
                      style={{
                        padding: '12px 24px',
                        backgroundColor: saving ? '#9ca3af' : '#3b82f6',
                        color: 'white',
                        border: 'none',
                        borderRadius: '8px',
                        fontSize: '14px',
                        fontWeight: '500',
                        cursor: saving ? 'not-allowed' : 'pointer'
                      }}
                    >
                      {saving ? '저장 중...' : mode === 'create' ? '등록하기' : '수정하기'}
                    </button>
                  </div>
                )}
              </form>
            </>
          )}
        </div>
      </div>
    </>
  );
};

export default ProtocolEditor;