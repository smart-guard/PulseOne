import React, { useState, useEffect } from 'react';

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
  is_enabled: boolean;
  is_deprecated: boolean;
  device_count?: number;
  enabled_count?: number;
  connected_count?: number;
  created_at?: string;
  updated_at?: string;
}

interface ProtocolDetailModalProps {
  protocol: Protocol;
  isOpen: boolean;
  onClose: () => void;
}

const ProtocolDetailModal: React.FC<ProtocolDetailModalProps> = ({
  protocol,
  isOpen,
  onClose
}) => {
  const [activeTab, setActiveTab] = useState<'overview' | 'technical' | 'devices' | 'settings'>('overview');

  // ESC 키로 모달 닫기
  useEffect(() => {
    const handleEsc = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };
    if (isOpen) {
      document.addEventListener('keydown', handleEsc);
      document.body.style.overflow = 'hidden';
    }
    return () => {
      document.removeEventListener('keydown', handleEsc);
      document.body.style.overflow = 'unset';
    };
  }, [isOpen, onClose]);

  if (!isOpen) return null;

  const getCategoryIcon = (category: string) => {
    const icons = {
      'industrial': '🏭',
      'iot': '🌐',
      'building_automation': '🏢',
      'network': '🔗',
      'web': '🌍'
    };
    return icons[category] || '📡';
  };

  const getCategoryColor = (category: string) => {
    const colors = {
      'industrial': '#3b82f6',
      'iot': '#10b981',
      'building_automation': '#f59e0b',
      'network': '#8b5cf6',
      'web': '#ef4444'
    };
    return colors[category] || '#6b7280';
  };

  const formatDate = (dateString?: string) => {
    if (!dateString) return '-';
    return new Date(dateString).toLocaleString('ko-KR');
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
      zIndex: 9999,
      padding: '20px'
    }}>
      <div style={{
        backgroundColor: 'white',
        borderRadius: '12px',
        width: '90vw',
        maxWidth: '1000px',
        height: '85vh',
        maxHeight: '800px',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        boxShadow: '0 25px 50px rgba(0, 0, 0, 0.25)'
      }}>
        {/* 모달 헤더 */}
        <div style={{
          padding: '24px',
          borderBottom: '1px solid #e5e7eb',
          backgroundColor: '#f8fafc',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            <div style={{
              width: '56px',
              height: '56px',
              borderRadius: '12px',
              backgroundColor: `${getCategoryColor(protocol.category || 'network')}20`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '24px'
            }}>
              {getCategoryIcon(protocol.category || 'network')}
            </div>
            <div>
              <h2 style={{
                margin: 0,
                fontSize: '24px',
                fontWeight: '700',
                color: '#1e293b',
                marginBottom: '4px'
              }}>
                {protocol.display_name}
              </h2>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                fontSize: '14px',
                color: '#64748b'
              }}>
                <span>{protocol.protocol_type}</span>
                <span>•</span>
                <span style={{
                  padding: '4px 8px',
                  backgroundColor: getCategoryColor(protocol.category || 'network'),
                  color: 'white',
                  borderRadius: '6px',
                  fontSize: '12px',
                  fontWeight: '500'
                }}>
                  {protocol.category || 'network'}
                </span>
                {protocol.is_enabled ? (
                  <span style={{
                    padding: '4px 8px',
                    backgroundColor: '#dcfce7',
                    color: '#166534',
                    borderRadius: '6px',
                    fontSize: '12px',
                    fontWeight: '500'
                  }}>
                    활성
                  </span>
                ) : (
                  <span style={{
                    padding: '4px 8px',
                    backgroundColor: '#fee2e2',
                    color: '#991b1b',
                    borderRadius: '6px',
                    fontSize: '12px',
                    fontWeight: '500'
                  }}>
                    비활성
                  </span>
                )}
              </div>
            </div>
          </div>
          
          <button
            onClick={onClose}
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

        {/* 탭 네비게이션 */}
        <div style={{
          display: 'flex',
          borderBottom: '1px solid #e5e7eb',
          backgroundColor: '#f8fafc'
        }}>
          {[
            { key: 'overview', label: '개요', icon: '📋' },
            { key: 'technical', label: '기술 정보', icon: '🔧' },
            { key: 'devices', label: '연결된 디바이스', icon: '📱' },
            { key: 'settings', label: '설정', icon: '⚙️' }
          ].map(tab => (
            <button
              key={tab.key}
              onClick={() => setActiveTab(tab.key as any)}
              style={{
                background: 'none',
                border: 'none',
                padding: '16px 24px',
                fontSize: '14px',
                fontWeight: '500',
                color: activeTab === tab.key ? '#3b82f6' : '#6b7280',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                borderBottom: activeTab === tab.key ? '2px solid #3b82f6' : '2px solid transparent',
                backgroundColor: activeTab === tab.key ? 'white' : 'transparent'
              }}
            >
              <span>{tab.icon}</span>
              {tab.label}
            </button>
          ))}
        </div>

        {/* 모달 콘텐츠 */}
        <div style={{
          flex: 1,
          overflow: 'auto',
          padding: '24px'
        }}>
          {activeTab === 'overview' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
              {/* 기본 정보 */}
              <div style={{
                backgroundColor: '#f8fafc',
                borderRadius: '12px',
                padding: '20px'
              }}>
                <h3 style={{
                  margin: 0,
                  marginBottom: '16px',
                  fontSize: '18px',
                  fontWeight: '600',
                  color: '#1e293b'
                }}>
                  기본 정보
                </h3>
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))',
                  gap: '16px'
                }}>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>설명</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>{protocol.description || '-'}</div>
                  </div>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>벤더</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>{protocol.vendor || '-'}</div>
                  </div>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>기본 포트</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>{protocol.default_port || '-'}</div>
                  </div>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>표준 참조</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>{protocol.standard_reference || '-'}</div>
                  </div>
                </div>
              </div>

              {/* 통계 정보 */}
              <div style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(3, 1fr)',
                gap: '16px'
              }}>
                <div style={{
                  backgroundColor: 'white',
                  border: '1px solid #e5e7eb',
                  borderRadius: '12px',
                  padding: '20px',
                  textAlign: 'center'
                }}>
                  <div style={{
                    fontSize: '32px',
                    fontWeight: '700',
                    color: '#1e293b',
                    marginBottom: '8px'
                  }}>
                    {protocol.device_count || 0}
                  </div>
                  <div style={{ fontSize: '14px', color: '#64748b' }}>총 디바이스</div>
                </div>
                <div style={{
                  backgroundColor: 'white',
                  border: '1px solid #e5e7eb',
                  borderRadius: '12px',
                  padding: '20px',
                  textAlign: 'center'
                }}>
                  <div style={{
                    fontSize: '32px',
                    fontWeight: '700',
                    color: '#16a34a',
                    marginBottom: '8px'
                  }}>
                    {protocol.enabled_count || 0}
                  </div>
                  <div style={{ fontSize: '14px', color: '#64748b' }}>활성 디바이스</div>
                </div>
                <div style={{
                  backgroundColor: 'white',
                  border: '1px solid #e5e7eb',
                  borderRadius: '12px',
                  padding: '20px',
                  textAlign: 'center'
                }}>
                  <div style={{
                    fontSize: '32px',
                    fontWeight: '700',
                    color: '#0284c7',
                    marginBottom: '8px'
                  }}>
                    {protocol.connected_count || 0}
                  </div>
                  <div style={{ fontSize: '14px', color: '#64748b' }}>연결된 디바이스</div>
                </div>
              </div>

              {/* 특성 */}
              <div style={{
                backgroundColor: '#f8fafc',
                borderRadius: '12px',
                padding: '20px'
              }}>
                <h3 style={{
                  margin: 0,
                  marginBottom: '16px',
                  fontSize: '18px',
                  fontWeight: '600',
                  color: '#1e293b'
                }}>
                  프로토콜 특성
                </h3>
                <div style={{
                  display: 'flex',
                  flexWrap: 'wrap',
                  gap: '12px'
                }}>
                  {protocol.uses_serial && (
                    <span style={{
                      padding: '6px 12px',
                      backgroundColor: '#dbeafe',
                      color: '#1e40af',
                      borderRadius: '8px',
                      fontSize: '12px',
                      fontWeight: '500'
                    }}>
                      시리얼 통신 지원
                    </span>
                  )}
                  {protocol.requires_broker && (
                    <span style={{
                      padding: '6px 12px',
                      backgroundColor: '#dcfce7',
                      color: '#166534',
                      borderRadius: '8px',
                      fontSize: '12px',
                      fontWeight: '500'
                    }}>
                      브로커 필요
                    </span>
                  )}
                  {protocol.is_deprecated && (
                    <span style={{
                      padding: '6px 12px',
                      backgroundColor: '#fef3c7',
                      color: '#92400e',
                      borderRadius: '8px',
                      fontSize: '12px',
                      fontWeight: '500'
                    }}>
                      지원 중단
                    </span>
                  )}
                </div>
              </div>
            </div>
          )}

          {activeTab === 'technical' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
              {/* 기술 사양 */}
              <div style={{
                backgroundColor: '#f8fafc',
                borderRadius: '12px',
                padding: '20px'
              }}>
                <h3 style={{
                  margin: 0,
                  marginBottom: '16px',
                  fontSize: '18px',
                  fontWeight: '600',
                  color: '#1e293b'
                }}>
                  기술 사양
                </h3>
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(auto-fit, minmax(300px, 1fr))',
                  gap: '16px'
                }}>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>기본 폴링 간격</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>
                      {protocol.default_polling_interval ? `${protocol.default_polling_interval}ms` : '-'}
                    </div>
                  </div>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>기본 타임아웃</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>
                      {protocol.default_timeout ? `${protocol.default_timeout}ms` : '-'}
                    </div>
                  </div>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>최대 동시 연결</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>
                      {protocol.max_concurrent_connections || '-'}
                    </div>
                  </div>
                </div>
              </div>

              {/* 지원 작업 */}
              {protocol.supported_operations && protocol.supported_operations.length > 0 && (
                <div style={{
                  backgroundColor: 'white',
                  border: '1px solid #e5e7eb',
                  borderRadius: '12px',
                  padding: '20px'
                }}>
                  <h3 style={{
                    margin: 0,
                    marginBottom: '16px',
                    fontSize: '18px',
                    fontWeight: '600',
                    color: '#1e293b'
                  }}>
                    지원 작업
                  </h3>
                  <div style={{
                    display: 'flex',
                    flexWrap: 'wrap',
                    gap: '8px'
                  }}>
                    {protocol.supported_operations.map((op, index) => (
                      <span key={index} style={{
                        padding: '6px 12px',
                        backgroundColor: '#f1f5f9',
                        color: '#475569',
                        borderRadius: '8px',
                        fontSize: '12px',
                        fontWeight: '500',
                        border: '1px solid #e2e8f0'
                      }}>
                        {op}
                      </span>
                    ))}
                  </div>
                </div>
              )}

              {/* 지원 데이터 타입 */}
              {protocol.supported_data_types && protocol.supported_data_types.length > 0 && (
                <div style={{
                  backgroundColor: 'white',
                  border: '1px solid #e5e7eb',
                  borderRadius: '12px',
                  padding: '20px'
                }}>
                  <h3 style={{
                    margin: 0,
                    marginBottom: '16px',
                    fontSize: '18px',
                    fontWeight: '600',
                    color: '#1e293b'
                  }}>
                    지원 데이터 타입
                  </h3>
                  <div style={{
                    display: 'flex',
                    flexWrap: 'wrap',
                    gap: '8px'
                  }}>
                    {protocol.supported_data_types.map((type, index) => (
                      <span key={index} style={{
                        padding: '6px 12px',
                        backgroundColor: '#fef3c7',
                        color: '#92400e',
                        borderRadius: '8px',
                        fontSize: '12px',
                        fontWeight: '500',
                        border: '1px solid #fde68a'
                      }}>
                        {type}
                      </span>
                    ))}
                  </div>
                </div>
              )}
            </div>
          )}

          {activeTab === 'devices' && (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '300px',
              color: '#64748b'
            }}>
              <div style={{ fontSize: '48px', marginBottom: '16px' }}>📱</div>
              <h3 style={{ margin: 0, marginBottom: '8px', color: '#374151' }}>
                연결된 디바이스 목록
              </h3>
              <p style={{ margin: 0, textAlign: 'center' }}>
                이 프로토콜을 사용하는 디바이스 목록이 여기에 표시됩니다.<br />
                구현 예정입니다.
              </p>
            </div>
          )}

          {activeTab === 'settings' && (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
              {/* 연결 설정 */}
              {protocol.connection_params && Object.keys(protocol.connection_params).length > 0 && (
                <div style={{
                  backgroundColor: '#f8fafc',
                  borderRadius: '12px',
                  padding: '20px'
                }}>
                  <h3 style={{
                    margin: 0,
                    marginBottom: '16px',
                    fontSize: '18px',
                    fontWeight: '600',
                    color: '#1e293b'
                  }}>
                    연결 매개변수
                  </h3>
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))',
                    gap: '16px'
                  }}>
                    {Object.entries(protocol.connection_params).map(([key, value]) => (
                      <div key={key}>
                        <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>
                          {key.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase())}
                        </div>
                        <div style={{ fontSize: '14px', color: '#1e293b' }}>
                          {typeof value === 'object' ? JSON.stringify(value) : String(value)}
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              {/* 생성/수정 정보 */}
              <div style={{
                backgroundColor: 'white',
                border: '1px solid #e5e7eb',
                borderRadius: '12px',
                padding: '20px'
              }}>
                <h3 style={{
                  margin: 0,
                  marginBottom: '16px',
                  fontSize: '18px',
                  fontWeight: '600',
                  color: '#1e293b'
                }}>
                  시스템 정보
                </h3>
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))',
                  gap: '16px'
                }}>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>생성일</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>
                      {formatDate(protocol.created_at)}
                    </div>
                  </div>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>수정일</div>
                    <div style={{ fontSize: '14px', color: '#1e293b' }}>
                      {formatDate(protocol.updated_at)}
                    </div>
                  </div>
                  <div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>프로토콜 ID</div>
                    <div style={{ fontSize: '14px', color: '#1e293b', fontFamily: 'monospace' }}>
                      #{protocol.id}
                    </div>
                  </div>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* 모달 푸터 */}
        <div style={{
          padding: '20px 24px',
          borderTop: '1px solid #e5e7eb',
          backgroundColor: '#f8fafc',
          display: 'flex',
          justifyContent: 'flex-end',
          gap: '12px'
        }}>
          <button
            onClick={onClose}
            style={{
              padding: '10px 20px',
              backgroundColor: '#f3f4f6',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              fontSize: '14px',
              fontWeight: '500',
              color: '#374151',
              cursor: 'pointer',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#e5e7eb';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = '#f3f4f6';
            }}
          >
            닫기
          </button>
        </div>
      </div>
    </div>
  );
};

export default ProtocolDetailModal;