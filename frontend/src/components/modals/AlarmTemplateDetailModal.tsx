import React from 'react';
import alarmTemplatesApi, { AlarmTemplate } from '../../api/services/alarmTemplatesApi';
import TemplatePropagateModal from './TemplatePropagateModal';
import '../../styles/management.css';
import '../../styles/alarm-rule-templates.css';

interface AlarmTemplateDetailModalProps {
  template: AlarmTemplate | null;
  isOpen: boolean;
  onClose: () => void;
  onEdit: (template: AlarmTemplate) => void;
  onApply?: (template: AlarmTemplate) => void;
}

const AlarmTemplateDetailModal: React.FC<AlarmTemplateDetailModalProps> = ({
  template,
  isOpen,
  onClose,
  onEdit,
  onApply
}) => {
  const [outdatedCount, setOutdatedCount] = React.useState(0);
  const [showPropagateModal, setShowPropagateModal] = React.useState(false);

  React.useEffect(() => {
    if (isOpen && template) {
      checkOutdatedRules();
    }
  }, [isOpen, template]);

  const checkOutdatedRules = async () => {
    if (!template) return;
    try {
      const rules = await alarmTemplatesApi.getOutdatedRules(template.id);
      setOutdatedCount(rules.length);
    } catch (error) {
      console.error('Failed to check outdated rules', error);
      setOutdatedCount(0);
    }
  };

  if (!isOpen || !template) return null;

  const getSeverityBadgeClass = (s: string) => {
    const severity = s?.toLowerCase();
    if (severity === 'critical') return 'critical';
    if (severity === 'high') return 'high';
    if (severity === 'medium') return 'medium';
    return 'low';
  };

  const renderConfigDetails = () => {
    const { condition_type, default_config: config } = template;
    if (!config) return <div className="adm-empty-state">설정 정보가 없습니다.</div>;

    return (
      <div className="adm-logic-card">
        <div className="adm-logic-header">
          <i className="fas fa-cog"></i> {condition_type.toUpperCase()} Logic Configuration
        </div>
        <div className="adm-logic-body">
          {condition_type === 'threshold' && (
            <div className="adm-info-row">
              <span className="label">High Threshold (상한값)</span>
              <span className="value highlight">{config.high_limit ?? '-'}</span>
            </div>
          )}
          {condition_type === 'range' && (
            <div className="adm-info-grid">
              <div className="adm-info-row">
                <span className="label">Low Limit (하한)</span>
                <span className="value">{config.low_limit ?? '-'}</span>
              </div>
              <div className="adm-info-row">
                <span className="label">High Limit (상한)</span>
                <span className="value">{config.high_limit ?? '-'}</span>
              </div>
            </div>
          )}
          {condition_type === 'digital' && (
            <div className="adm-info-row">
              <span className="label">Trigger Condition</span>
              <span className="value highlight">{config.trigger_condition === '1' ? 'TRUE (1)' : 'FALSE (0)'}</span>
            </div>
          )}
          {condition_type === 'script' && (
            <div className="adm-script-view">
              <pre><code>{config.script || '// No script defined'}</code></pre>
            </div>
          )}
          <div className="adm-logic-meta">
            <span><i className="fas fa-history"></i> Delay: {config.delay_ms || 0}ms</span>
            <span><i className="fas fa-wave-square"></i> Deadband: {config.deadband || 0}</span>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div className="adm-overlay" onClick={(e) => e.target === e.currentTarget && onClose()}>
      <div className="adm-container">
        {/* Header */}
        <div className="adm-header">
          <div className="adm-header-left">
            <div className="adm-icon-box">
              <i className="fas fa-file-alt"></i>
            </div>
            <div className="adm-title-group">
              <h3 className="adm-title">알람 템플릿 상세 정보</h3>
              <p className="adm-subtitle">템플릿의 상세 설정 및 구성을 확인합니다.</p>
            </div>
          </div>
          <button className="adm-close-btn" onClick={onClose} aria-label="Close">&times;</button>
        </div>

        {/* Body - Flex row for side-by-side layout */}
        <div className="adm-body">
          {/* Sidebar */}
          <div className="adm-sidebar">
            <div className="adm-meta-block">
              <label>템플릿 이름</label>
              <div className="adm-name-text">{template.name}</div>
            </div>

            <div className="adm-meta-block">
              <label>심각도 레벨</label>
              <div className={`adm-badge ${getSeverityBadgeClass(template.severity)}`}>
                {template.severity.toUpperCase()}
              </div>
            </div>

            <div className="adm-meta-block">
              <label>카테고리</label>
              <div className="adm-meta-val">{template.category}</div>
            </div>

            <div className="adm-meta-block">
              <label>템플릿 유형</label>
              <div className="adm-meta-val">{template.template_type.toUpperCase()}</div>
            </div>

            <div className="adm-meta-block">
              <label>시스템 레벨</label>
              <div className="adm-meta-val">
                {template.is_system_template ? (
                  <span className="adm-badge" style={{ background: '#1e293b', color: '#f8fafc', border: '1px solid #334155' }}>SYSTEM</span>
                ) : (
                  <span className="adm-badge" style={{ background: '#f1f5f9', color: '#64748b' }}>USER</span>
                )}
              </div>
            </div>

            <div className="adm-meta-block" style={{ marginTop: 'auto', paddingTop: '20px' }}>
              <label>사용 통계</label>
              <div className="adm-stat-text">{template.usage_count || 0}회 적용됨</div>
            </div>

            {outdatedCount > 0 && (
              <div className="adm-meta-block" style={{ paddingTop: '12px' }}>
                <label style={{ color: '#d97706' }}>업데이트 필요</label>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{ fontSize: '14px', fontWeight: 600, color: '#d97706' }}>{outdatedCount}개 규칙</span>
                  <button
                    onClick={() => setShowPropagateModal(true)}
                    style={{
                      background: '#fff7ed',
                      border: '1px solid #fdba74',
                      color: '#c2410c',
                      borderRadius: '4px',
                      padding: '2px 8px',
                      fontSize: '11px',
                      fontWeight: 700,
                      cursor: 'pointer'
                    }}
                  >
                    동기화
                  </button>
                </div>
              </div>
            )}
          </div>

          {/* Main Content */}
          <div className="adm-main">
            <div className="adm-section">
              <h4 className="adm-section-header">설명</h4>
              <div className="adm-description-text">
                {template.description || '정의된 설명이 없습니다.'}
              </div>
            </div>

            <div className="adm-section">
              <h4 className="adm-section-header">로직 구성</h4>
              {renderConfigDetails()}
            </div>

            <div className="adm-section">
              <h4 className="adm-section-header">메시지 템플릿</h4>
              <div className="adm-message-card">
                <i className="fas fa-comment-dots"></i>
                <span className="adm-message-text">{template.message_template}</span>
              </div>
            </div>

            <div className="adm-system-meta">
              <div className="adm-meta-pair">
                <span className="label">생성일시</span>
                <span className="value">{new Date(template.created_at).toLocaleString('ko-KR')}</span>
              </div>
              <div className="adm-meta-pair">
                <span className="label">최종 수정</span>
                <span className="value">{new Date(template.updated_at).toLocaleString('ko-KR')}</span>
              </div>
            </div>
          </div>
        </div>

        {/* Footer */}
        <div className="adm-footer">
          <button className="adm-btn adm-btn-outline" onClick={onClose}>닫기</button>
          <div style={{ display: 'flex', gap: '12px' }}>
            {onApply && (
              <button className="adm-btn adm-btn-success" onClick={() => onApply(template)}>
                <i className="fas fa-play"></i> 즉시 적용
              </button>
            )}
            {template.is_system_template ? (
              <button
                className="adm-btn adm-btn-outline"
                disabled
                style={{ opacity: 0.5, cursor: 'not-allowed', background: '#f1f5f9' }}
                title="시스템 템플릿은 수정할 수 없습니다."
              >
                <i className="fas fa-lock"></i> 수정 불가 (System)
              </button>
            ) : (
              <button className="adm-btn adm-btn-primary" onClick={() => onEdit(template)}>
                <i className="fas fa-edit"></i> 템플릿 설정 수정
              </button>
            )}
          </div>
        </div>
      </div>

      <style>{`
        /* ADM prefix for Alarm Detail Modal to avoid global conflicts */
        .adm-overlay {
          position: fixed !important;
          top: 0 !important;
          left: 0 !important;
          right: 0 !important;
          bottom: 0 !important;
          background: rgba(15, 23, 42, 0.7) !important;
          backdrop-filter: blur(8px) !important;
          display: flex !important;
          align-items: center !important;
          justify-content: center !important;
          z-index: 5000 !important;
          padding: 20px !important;
        }

        .adm-container {
          background: white !important;
          width: 1000px !important;
          max-width: 95vw !important;
          border-radius: 16px !important;
          box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25) !important;
          display: flex !important;
          flex-direction: column !important;
          overflow: hidden !important;
          max-height: 95vh !important;
          height: auto !important;
          animation: adm-fade-in 0.2s ease-out !important;
        }

        @keyframes adm-fade-in {
          from { opacity: 0; transform: scale(0.98); }
          to { opacity: 1; transform: scale(1); }
        }

        .adm-header {
          padding: 24px 32px !important;
          background: #f8fafc !important;
          border-bottom: 1px solid #e2e8f0 !important;
          display: flex !important;
          justify-content: space-between !important;
          align-items: center !important;
          flex-shrink: 0 !important;
        }

        .adm-header-left {
          display: flex !important;
          align-items: center !important;
          gap: 20px !important;
        }

        .adm-icon-box {
          width: 48px !important;
          height: 48px !important;
          background: #3b82f6 !important;
          color: white !important;
          border-radius: 12px !important;
          display: flex !important;
          align-items: center !important;
          justify-content: center !important;
          font-size: 20px !important;
        }

        .adm-title {
          margin: 0 !important;
          font-size: 20px !important;
          font-weight: 700 !important;
          color: #1e293b !important;
          line-height: 1.2 !important;
        }

        .adm-subtitle {
          margin: 4px 0 0 0 !important;
          font-size: 14px !important;
          color: #64748b !important;
        }

        .adm-close-btn {
          background: transparent !important;
          border: none !important;
          font-size: 32px !important;
          color: #94a3b8 !important;
          cursor: pointer !important;
          line-height: 1 !important;
          padding: 4px !important;
        }

        .adm-body {
          display: flex !important;
          flex-direction: row !important;
          background: white !important;
          height: auto !important;
          min-height: 0 !important;
        }

        .adm-sidebar {
          width: 280px !important;
          min-width: 280px !important;
          background: #f1f5f9 !important;
          border-right: 1px solid #e2e8f0 !important;
          padding: 32px 24px !important;
          display: flex !important;
          flex-direction: column !important;
          gap: 28px !important;
          overflow-y: hidden !important;
        }

        .adm-meta-block label {
          display: block !important;
          font-size: 11px !important;
          font-weight: 700 !important;
          color: #94a3b8 !important;
          text-transform: uppercase !important;
          letter-spacing: 0.05em !important;
          margin-bottom: 8px !important;
        }

        .adm-name-text {
          font-size: 16px !important;
          font-weight: 700 !important;
          color: #2563eb !important;
          line-height: 1.4 !important;
          word-break: break-all !important;
        }

        .adm-badge {
          display: inline-block !important;
          padding: 4px 12px !important;
          border-radius: 20px !important;
          font-size: 11px !important;
          font-weight: 700 !important;
        }
        .adm-badge.critical { background: #fee2e2 !important; color: #b91c1c !important; }
        .adm-badge.high { background: #ffedd5 !important; color: #c2410c !important; }
        .adm-badge.medium { background: #dbeafe !important; color: #1e40af !important; }
        .adm-badge.low { background: #f1f5f9 !important; color: #475569 !important; }

        .adm-meta-val {
          font-size: 14px !important;
          font-weight: 500 !important;
          color: #334155 !important;
        }

        .adm-stat-text {
          font-size: 14px !important;
          font-weight: 700 !important;
          color: #3b82f6 !important;
        }

        .adm-main {
          flex: 1 !important;
          padding: 32px 40px !important;
          display: flex !important;
          flex-direction: column !important;
          gap: 32px !important;
          overflow-y: hidden !important;
        }

        .adm-section-header {
          font-size: 14px !important;
          font-weight: 700 !important;
          color: #334155 !important;
          margin: 0 0 12px 0 !important;
          display: flex !important;
          align-items: center !important;
          gap: 8px !important;
        }

        .adm-description-text {
          background: #f8fafc !important;
          padding: 20px !important;
          border-radius: 12px !important;
          border: 1px solid #f1f5f9 !important;
          font-size: 14px !important;
          color: #475569 !important;
          line-height: 1.6 !important;
        }

        .adm-logic-card {
          border: 1px solid #e2e8f0 !important;
          border-radius: 12px !important;
          overflow: hidden !important;
        }

        .adm-logic-header {
          background: #1e293b !important;
          color: white !important;
          padding: 10px 16px !important;
          font-size: 11px !important;
          font-weight: 700 !important;
          text-transform: uppercase !important;
        }

        .adm-logic-body {
          padding: 24px !important;
          background: white !important;
        }

        .adm-info-row {
          display: flex !important;
          flex-direction: column !important;
          gap: 6px !important;
        }

        .adm-info-row .label { font-size: 13px !important; color: #64748b !important; }
        .adm-info-row .value { font-size: 18px !important; font-weight: 700 !important; color: #0f172a !important; }
        .adm-info-row .value.highlight { color: #2563eb !important; }

        .adm-info-grid {
          display: grid !important;
          grid-template-columns: 1fr 1fr !important;
          gap: 24px !important;
        }

        .adm-script-view pre {
          background: #0f172a !important;
          color: #10b981 !important;
          padding: 20px !important;
          border-radius: 8px !important;
          margin: 0 !important;
          font-size: 13px !important;
          overflow-x: auto !important;
        }

        .adm-logic-meta {
          margin-top: 20px !important;
          padding-top: 16px !important;
          border-top: 1px solid #f1f5f9 !important;
          display: flex !important;
          gap: 24px !important;
          font-size: 12px !important;
          color: #94a3b8 !important;
          font-weight: 600 !important;
        }

        .adm-message-card {
          background: #f0f9ff !important;
          border: 1px solid #bae6fd !important;
          padding: 20px !important;
          border-radius: 12px !important;
          display: flex !important;
          gap: 16px !important;
          align-items: center !important;
        }

        .adm-message-card i {
          font-size: 20px !important;
          color: #0ea5e9 !important;
        }

        .adm-message-text {
          font-size: 15px !important;
          font-weight: 600 !important;
          color: #0369a1 !important;
        }

        .adm-system-meta {
          margin-top: 32px !important;
          padding-top: 24px !important;
          border-top: 1px solid #f1f5f9 !important;
          display: flex !important;
          gap: 48px !important;
        }

        .adm-meta-pair {
          display: flex !important;
          flex-direction: column !important;
          gap: 6px !important;
        }

        .adm-meta-pair .label { font-size: 13px !important; color: #94a3b8 !important; font-weight: 600 !important; }
        .adm-meta-pair .value { font-size: 14px !important; color: #64748b !important; }

        .adm-footer {
          padding: 24px 32px !important;
          background: #f8fafc !important;
          border-top: 1px solid #e2e8f0 !important;
          display: flex !important;
          justify-content: space-between !important;
          align-items: center !important;
          flex-shrink: 0 !important;
        }

        .adm-btn {
          display: inline-flex !important;
          align-items: center !important;
          justify-content: center !important;
          gap: 10px !important;
          padding: 10px 24px !important;
          border-radius: 10px !important;
          font-size: 14px !important;
          font-weight: 700 !important;
          cursor: pointer !important;
          transition: all 0.2s !important;
        }

        .adm-btn-primary { background: #2563eb !important; color: white !important; border: none !important; }
        .adm-btn-primary:hover { background: #1d4ed8 !important; }

        .adm-btn-success { background: #059669 !important; color: white !important; border: none !important; }
        .adm-btn-success:hover { background: #047857 !important; }

        .adm-btn-outline { background: white !important; border: 1px solid #d1d5db !important; color: #374151 !important; }
        .adm-btn-outline:hover { background: #f9fafb !important; }

      `}</style>

      <TemplatePropagateModal
        isOpen={showPropagateModal}
        onClose={() => {
          setShowPropagateModal(false);
          checkOutdatedRules(); // Refresh stats after closing (in case update happened)
        }}
        templateId={template.id}
        templateName={template.name}
        currentVersion={template.version || 1}
      />
    </div>
  );
};

export default AlarmTemplateDetailModal;
