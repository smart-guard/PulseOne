import React, { useEffect } from 'react';
import { useTranslation } from 'react-i18next';
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
  const { t } = useTranslation(['protocols', 'common']);
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
                    {protocol.is_enabled ? t('detail.active') : t('detail.inactive')}
                  </span>
                </div>
              </div>
            </div>
          </div>
          <button className="mgmt-close-btn" onClick={onClose} aria-label="Close">
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 모달 콘텐츠 */}
        <div className="mgmt-modal-body">
          <div className="mgmt-modal-form-grid">

            {/* 1. Basic Info (Editor와 동일한 섹션 구성) */}
            <div className="mgmt-modal-form-section">
              <h3><i className="fas fa-info-circle"></i> {t('section.basic')}</h3>

              <div className="mgmt-modal-form-row">
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.protocolType')}</div>
                  <div className="mgmt-detail-value">{protocol.protocol_type}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.displayName')}</div>
                  <div className="mgmt-detail-value mgmt-highlight">{protocol.display_name}</div>
                </div>
                {protocol.is_deprecated && (
                  <div className="mgmt-detail-item" style={{ marginLeft: 'auto', marginBottom: 'auto' }}>
                    <span className="mgmt-status-pill warning" style={{ fontSize: '11px', padding: '2px 8px' }}>{t('detail.deprecated')}</span>
                  </div>
                )}
              </div>

              <div className="mgmt-detail-item">
                <div className="mgmt-detail-label">{t('detail.description')}</div>
                <div className="mgmt-detail-value">{protocol.description || t('detail.noDescription')}</div>
              </div>

              <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: '1fr 1fr 1fr' }}>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.category')}</div>
                  <div className="mgmt-detail-value">{protocol.category || 'network'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.defaultPort')}</div>
                  <div className="mgmt-detail-value">{protocol.default_port || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.manufacturer')}</div>
                  <div className="mgmt-detail-value">{protocol.vendor || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.minFirmware')}</div>
                  <div className="mgmt-detail-value">{protocol.min_firmware_version || '-'}</div>
                </div>
              </div>
            </div>

            {/* 2. Technical Settings (Editor와 동일한 섹션 구성) */}
            <div className="mgmt-modal-form-section">
              <h3><i className="fas fa-cogs"></i> {t('section.technicalSettings')}</h3>

              <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: 'repeat(3, 1fr)' }}>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.defaultPolling')}</div>
                  <div className="mgmt-detail-value">{protocol.default_polling_interval || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.defaultTimeout')}</div>
                  <div className="mgmt-detail-value">{protocol.default_timeout || '-'}</div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.maxConnections')}</div>
                  <div className="mgmt-detail-value">{protocol.max_concurrent_connections || '-'}</div>
                </div>
              </div>

              <div className="mgmt-modal-form-row" style={{ gridTemplateColumns: 'repeat(3, 1fr)' }}>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.serialRequired')}</div>
                  <div className="mgmt-detail-value">
                    {protocol.uses_serial ? t('detail.yes') : t('detail.no')}
                    <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '4px' }}>
                      {t('detail.serialHint')}
                    </small>
                  </div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.brokerRequired')}</div>
                  <div className="mgmt-detail-value">
                    {protocol.requires_broker ? t('detail.yes') : t('detail.no')}
                    <small style={{ color: '#6b7280', fontSize: '11px', display: 'block', marginTop: '4px' }}>
                      {t('detail.brokerHint')}
                    </small>
                  </div>
                </div>
                <div className="mgmt-detail-item">
                  <div className="mgmt-detail-label">{t('detail.status')}</div>
                  <div className="mgmt-detail-value">
                    <span className={`mgmt-status-pill ${protocol.is_enabled ? 'active' : 'inactive'}`}>
                      {protocol.is_enabled ? t('detail.active') : t('detail.inactive')}
                    </span>
                  </div>
                </div>
              </div>
            </div>

            {/* 3 & 4. 사이드-바이-사이드 도메인 레이아웃 */}
            <div className="mgmt-modal-form-domains">
              {/* Driver Capabilities */}
              <div className="mgmt-modal-form-domain">
                <div className="mgmt-modal-form-section">
                  <h3><i className="fas fa-microchip"></i> {t('section.driverCapabilities')}</h3>
                  <div className="mgmt-detail-item">
                    <div className="mgmt-detail-label">{t('detail.supportedOps')}</div>
                    <div className="mgmt-capability-badge-container">
                      {protocol.supported_operations?.length ? protocol.supported_operations.map((op, i) => (
                        <span key={i} className="mgmt-capability-badge">{op}</span>
                      )) : <span className="mgmt-detail-value">-</span>}
                    </div>
                  </div>
                  <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                    <div className="mgmt-detail-label">{t('detail.supportedTypes')}</div>
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
                  <h3><i className="fas fa-code"></i> {t('section.connectionParams')}</h3>
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
              <h3><i className="fas fa-network-wired"></i> {t('section.connectionStatus')}</h3>
              <div style={{ display: 'flex', justifyContent: 'space-around', textAlign: 'center' }}>
                <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                  <div className="mgmt-detail-label">{t('detail.totalDevices')}</div>
                  <div className="mgmt-detail-value mgmt-highlight" style={{ fontSize: '24px' }}>{protocol.device_count || 0}</div>
                </div>
                <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                  <div className="mgmt-detail-label">{t('detail.activeDevices')}</div>
                  <div className="mgmt-detail-value mgmt-highlight" style={{ fontSize: '24px', color: 'var(--success-600)' }}>
                    {protocol.enabled_count || 0}
                  </div>
                </div>
                <div className="mgmt-detail-item" style={{ marginBottom: 0 }}>
                  <div className="mgmt-detail-label">{t('detail.connected')}</div>
                  <div className="mgmt-detail-value mgmt-highlight" style={{ fontSize: '24px', color: 'var(--primary-600)' }}>
                    {protocol.connected_count || 0}
                  </div>
                </div>
              </div>
            </div>

            {/* 시스템 정보 (푸터 위에 작게 배치) */}
            <div className="span-full" style={{ padding: '0 4px 8px 4px', display: 'flex', justifyContent: 'flex-end' }}>
              <div style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>
                {t('detail.created')}: {formatDate(protocol.created_at)} | {t('detail.updated')}: {formatDate(protocol.updated_at)} | ID: #{protocol.id}
              </div>
            </div>
          </div>
        </div>

        {/* 모달 푸터 */}
        <div className="mgmt-modal-footer">
          <button className="mgmt-btn mgmt-btn-outline" style={{ width: 'auto', minWidth: '100px' }} onClick={onClose}>{t('close', { ns: 'common' })}</button>
          <button className="mgmt-btn mgmt-btn-primary" style={{ width: 'auto', minWidth: '100px' }} onClick={() => {
            onClose();
            onEdit();
          }}>
            <i className="fas fa-edit"></i> {t('edit', { ns: 'common' })}
          </button>
        </div>
      </div>
    </div>
  );
};

export default ProtocolDetailModal;