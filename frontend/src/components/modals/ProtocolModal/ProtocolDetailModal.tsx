import React, { useEffect } from 'react';
import './ProtocolModal.css';

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
  is_enabled: boolean;
  is_deprecated: boolean;
  min_firmware_version?: string;
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
  onEdit: () => void;
}

const ProtocolDetailModal: React.FC<ProtocolDetailModalProps> = ({
  protocol,
  isOpen,
  onClose,
  onEdit
}) => {
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
    <div className="mgmt-modal-overlay">
      <div className="mgmt-modal-container mgmt-protocol-modal">
        {/* 모달 헤더 */}
        <div className="mgmt-modal-header">
          <div className="mgmt-modal-title">
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              <div style={{
                width: '40px',
                height: '40px',
                borderRadius: '8px',
                backgroundColor: `${getCategoryColor(protocol.category || 'network')}20`,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontSize: '20px'
              }}>
                {getCategoryIcon(protocol.category || 'network')}
              </div>
              <div className="mgmt-title-row">
                <h2>{protocol.display_name}</h2>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
                  <span className="mgmt-badge">{protocol.protocol_type}</span>
                  <span className={`mgmt-badge ${protocol.category ? 'primary' : 'neutral'}`}>
                    {protocol.category || 'network'}
                  </span>
                  <span className={`mgmt-badge ${protocol.is_enabled ? 'success' : 'neutral'}`}>
                    {protocol.is_enabled ? '활성' : '비활성'}
                  </span>
                </div>
              </div>
            </div>
          </div>
          <button className="mgmt-close-btn" onClick={onClose} aria-label="닫기">
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 모달 콘텐츠 */}
        <div className="mgmt-modal-body">
          <div className="mgmt-modal-form-grid">

            {/* 1. 기본 정보 (Editor와 동일한 섹션 구성) */}
            <div className="mgmt-modal-form-section">
              <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>

              <div className="mgmt-modal-form-row">
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">프로토콜 타입</div>
                  <div className="mgmt-detail-value">{protocol.protocol_type}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">표시명</div>
                  <div className="mgmt-detail-value mgmt-highlight">{protocol.display_name}</div>
                </div>
                {protocol.is_deprecated && (
                  <div className="mgmt-detail-item" style={{ marginLeft: 'auto', marginBottom: 'auto' }}>
                    <span className="mgmt-status-pill warning" style={{ fontSize: '11px', padding: '2px 8px' }}>사용 중단 예정</span>
                  </div>
                )}
              </div>

              <div className="mgmt-detail-item">
                <div className="mgmt-detail-label">설명</div>
                <div className="mgmt-detail-value">{protocol.description || '설명이 없습니다.'}</div>
              </div>

              <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: '1fr 1fr 1fr' }}>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">카테고리</div>
                  <div className="mgmt-detail-value">{protocol.category || 'network'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">기본 포트</div>
                  <div className="mgmt-detail-value">{protocol.default_port || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">제조사/벤더</div>
                  <div className="mgmt-detail-value">{protocol.vendor || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">최소 펌웨어</div>
                  <div className="mgmt-detail-value">{protocol.min_firmware_version || '-'}</div>
                </div>
              </div>
            </div>

            {/* 2. 기술 설정 (Editor와 동일한 섹션 구성) */}
            <div className="mgmt-modal-form-section">
              <h3><i className="fas fa-cogs"></i> 기술 설정</h3>

              <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: 'repeat(3, 1fr)' }}>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">기본 폴링 (ms)</div>
                  <div className="mgmt-detail-value">{protocol.default_polling_interval || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">기본 타임아웃 (ms)</div>
                  <div className="mgmt-detail-value">{protocol.default_timeout || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">최대 동시 연결</div>
                  <div className="mgmt-detail-value">{protocol.max_concurrent_connections || '-'}</div>
                </div>
              </div>

              <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: 'repeat(3, 1fr)' }}>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">시리얼 사용</div>
                  <div className="mgmt-detail-value">
                    {protocol.uses_serial ? 'Yes' : 'No'}
                    <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '4px' }}>
                      RS-232/485 통신 필요 여부
                    </small>
                  </div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">브로커 필요</div>
                  <div className="mgmt-detail-value">
                    {protocol.requires_broker ? 'Yes' : 'No'}
                    <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '4px' }}>
                      MQTT 서버 등 중계기 필요 여부
                    </small>
                  </div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">상태</div>
                  <div className="mgmt-detail-value">
                    <span className={`mgmt-status-pill ${protocol.is_enabled ? 'active' : 'inactive'}`}>
                      {protocol.is_enabled ? '활성' : '비활성'}
                    </span>
                  </div>
                </div>
              </div>
            </div>

            {/* 3 & 4. 사이드-바이-사이드 도메인 레이아웃 */}
            <div className="mgmt-modal-form-domains">
              {/* 드라이버 역량 */}
              <div className="mgmt-modal-form-domain">
                <div className="mgmt-modal-form-section">
                  <h3><i className="fas fa-microchip"></i> 드라이버 역량</h3>
                  <div className="mgmt-detail-item">
                    <div className="mgmt-detail-label">지원 명령어 (Operations)</div>
                    <div className="mgmt-capability-badge-container">
                      {protocol.supported_operations?.length ? protocol.supported_operations.map((op, i) => (
                        <span key={i} className="mgmt-capability-badge">{op}</span>
                      )) : <span className="mgmt-detail-value">-</span>}
                    </div>
                  </div>
                  <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                    <div className="mgmt-detail-label">지원 데이터 타입 (Data Types)</div>
                    <div className="mgmt-capability-badge-container">
                      {protocol.supported_data_types?.length ? protocol.supported_data_types.map((type, i) => (
                        <span key={i} className="mgmt-capability-badge">{type}</span>
                      )) : <span className="mgmt-detail-value">-</span>}
                    </div>
                  </div>
                </div>
              </div>

              {/* 연결 파라미터 */}
              <div className="mgmt-modal-form-domain">
                <div className="mgmt-modal-form-section">
                  <h3><i className="fas fa-code"></i> 연결 파라미터</h3>
                  <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                    <div className="mgmt-detail-value text-sm" style={{
                      backgroundColor: 'var(--neutral-50)',
                      borderRadius: '8px',
                      border: '1px solid var(--neutral-200)',
                      maxHeight: '160px',
                      overflow: 'auto',
                      padding: '12px'
                    }}>
                      <pre style={{
                        margin: 0,
                        whiteSpace: 'pre-wrap',
                        wordBreak: 'break-all',
                        fontFamily: 'monospace',
                        fontSize: '12px',
                        color: 'var(--neutral-600)'
                      }}>
                        {JSON.stringify(protocol.connection_params || {}, null, 2)}
                      </pre>
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* 4. 연결 현황 (Span Full) */}
            <div className="mgmt-modal-form-section mgmt-span-full">
              <h3><i className="fas fa-network-wired"></i> 연결 현황</h3>
              <div style={{ display: 'flex', justifyContent: 'space-around', textAlign: 'center' }}>
                <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                  <div className="mgmt-detail-label">총 디바이스</div>
                  <div className="mgmt-detail-value mgmt-highlight" style={{ fontSize: '24px' }}>{protocol.device_count || 0}</div>
                </div>
                <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                  <div className="mgmt-detail-label">활성 디바이스</div>
                  <div className="mgmt-detail-value mgmt-highlight" style={{ fontSize: '24px', color: 'var(--success-600)' }}>
                    {protocol.enabled_count || 0}
                  </div>
                </div>
                <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                  <div className="mgmt-detail-label">연결됨</div>
                  <div className="mgmt-detail-value mgmt-highlight" style={{ fontSize: '24px', color: 'var(--primary-600)' }}>
                    {protocol.connected_count || 0}
                  </div>
                </div>
              </div>
            </div>

            {/* 시스템 정보 (푸터 위에 작게 배치) */}
            <div className="span-full" style={{ padding: '0 4px 8px 4px', display: 'flex', justifyContent: 'flex-end' }}>
              <div style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>
                생성일: {formatDate(protocol.created_at)} | 수정일: {formatDate(protocol.updated_at)} | ID: #{protocol.id}
              </div>
            </div>
          </div>
        </div>

        {/* 모달 푸터 */}
        <div className="mgmt-modal-footer">
          <button className="mgmt-btn mgmt-btn-outline" style={{ width: 'auto', minWidth: '100px' }} onClick={onClose}>닫기</button>
          <button className="mgmt-btn mgmt-btn-primary" style={{ width: 'auto', minWidth: '100px' }} onClick={() => {
            onClose();
            onEdit();
          }}>
            <i className="fas fa-edit"></i> 수정
          </button>
        </div>
      </div>
    </div>
  );
};

export default ProtocolDetailModal;