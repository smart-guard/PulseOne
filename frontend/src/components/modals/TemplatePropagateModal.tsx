import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import alarmTemplatesApi, { CreatedAlarmRule } from '../../api/services/alarmTemplatesApi';

interface TemplatePropagateModalProps {
    isOpen: boolean;
    onClose: () => void;
    templateId: number;
    templateName: string;
    currentVersion: number;
}

const TemplatePropagateModal: React.FC<TemplatePropagateModalProps> = ({
    isOpen,
    onClose,
    templateId,
    templateName,
    currentVersion
}) => {
    const [outdatedRules, setOutdatedRules] = useState<CreatedAlarmRule[]>([]);
    const { t } = useTranslation(['deviceTemplates', 'common']);
    const [selectedRules, setSelectedRules] = useState<number[]>([]);
    const [loading, setLoading] = useState(false);
    const [applying, setApplying] = useState(false);

    useEffect(() => {
        if (isOpen && templateId) {
            loadOutdatedRules();
        } else {
            setOutdatedRules([]);
            setSelectedRules([]);
        }
    }, [isOpen, templateId]);

    const loadOutdatedRules = async () => {
        setLoading(true);
        try {
            const rules = await alarmTemplatesApi.getOutdatedRules(templateId);
            setOutdatedRules(rules);
            // Default select all
            setSelectedRules(rules.map(r => r.id));
        } catch (error) {
            console.error('Failed to load outdated rules', error);
            alert('Failed to load rules for synchronization.');
        } finally {
            setLoading(false);
        }
    };

    const handleToggleRule = (ruleId: number) => {
        setSelectedRules(prev =>
            prev.includes(ruleId) ? prev.filter(id => id !== ruleId) : [...prev, ruleId]
        );
    };

    const handleToggleAll = () => {
        if (selectedRules.length === outdatedRules.length) {
            setSelectedRules([]);
        } else {
            setSelectedRules(outdatedRules.map(r => r.id));
        }
    };

    const handlePropagate = async () => {
        if (selectedRules.length === 0) return;

        if (!confirm(`Are you sure you want to update ${selectedRules.length} rule(s) to the latest template settings (v${currentVersion})?`)) {
            return;
        }

        setApplying(true);
        try {
            const result = await alarmTemplatesApi.propagateTemplate(templateId, selectedRules);
            alert(`${result.updated_count} rule(s) updated successfully.`);
            onClose();
        } catch (error) {
            console.error('Propagate failed', error);
            alert('An error occurred during update.');
        } finally {
            setApplying(false);
        }
    };

    if (!isOpen) return null;

    return (
        <div className="tpm-overlay" style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: 'rgba(0, 0, 0, 0.5)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 9500 // Higher than Detail Modal
        }}>
            <style>
                {`
          .tpm-overlay { backdrop-filter: blur(4px); }
          .tpm-container { animation: slideIn 0.2s ease-out; }
          @keyframes slideIn {
            from { opacity: 0; transform: translateY(-10px); }
            to { opacity: 1; transform: translateY(0); }
          }
        `}
            </style>

            <div className="tpm-container" style={{
                width: '600px',
                backgroundColor: '#fff',
                borderRadius: '12px',
                boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.25)',
                display: 'flex',
                flexDirection: 'column',
                maxHeight: '85vh'
            }}>
                {/* Header */}
                <div style={{
                    padding: '24px',
                    borderBottom: '1px solid #e2e8f0',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'flex-start'
                }}>
                    <div>
                        <h2 style={{ fontSize: '20px', fontWeight: 600, color: '#0f172a', marginBottom: '4px' }}>
                            Propagate Template Changes
                        </h2>
                        <p style={{ fontSize: '14px', color: '#64748b', margin: 0 }}>
                            {templateName} (v{currentVersion}) settings to existing rules.
                        </p>
                    </div>
                    <button onClick={onClose} style={{ background: 'none', border: 'none', fontSize: '20px', cursor: 'pointer', color: '#94a3b8' }}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                {/* Diff Summary (Simplified) */}
                <div style={{
                    padding: '16px 24px',
                    backgroundColor: '#f8fafc',
                    borderBottom: '1px solid #e2e8f0'
                }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px', fontSize: '14px' }}>
                        <span style={{ fontWeight: 600, color: '#334155' }}>{t('labels.changes', {ns: 'deviceTemplates'})}</span>
                        <span style={{ padding: '4px 8px', backgroundColor: '#e2e8f0', borderRadius: '4px', color: '#64748b' }}>{t('labels.previousVersion', {ns: 'deviceTemplates'})}</span>
                        <i className="fas fa-arrow-right" style={{ color: '#94a3b8' }}></i>
                        <span style={{ padding: '4px 8px', backgroundColor: '#dbeafe', borderRadius: '4px', color: '#2563eb', fontWeight: 600 }}>
                            v{currentVersion} (Latest)
                        </span>
                    </div>
                    <p style={{ fontSize: '13px', color: '#64748b', marginTop: '8px', marginBottom: 0 }}>
                        <i className="fas fa-info-circle" style={{ marginRight: '6px' }}></i>
                        The alarm conditions, message settings, severity levels, etc. of the selected rules will be overwritten with the current template settings.
                    </p>
                </div>

                {/* Rule List */}
                <div style={{ flex: 1, overflowY: 'auto', padding: '0 24px' }}>
                    {loading ? (
                        <div style={{ padding: '40px', textAlign: 'center', color: '#64748b' }}>
                            <i className="fas fa-spinner fa-spin" style={{ marginRight: '8px' }}></i>
                            동기화 대상 확인 중...
                        </div>
                    ) : outdatedRules.length === 0 ? (
                        <div style={{ padding: '40px', textAlign: 'center', color: '#64748b' }}>
                            <i className="fas fa-check-circle" style={{ fontSize: '24px', color: '#10b981', marginBottom: '12px', display: 'block' }}></i>
                            모든 규칙이 최신 상태입니다.
                        </div>
                    ) : (
                        <table style={{ width: '100%', borderCollapse: 'collapse', marginTop: '16px', marginBottom: '24px' }}>
                            <thead>
                                <tr>
                                    <th style={{ textAlign: 'left', padding: '12px 0', borderBottom: '1px solid #e2e8f0', width: '40px' }}>
                                        <input
                                            type="checkbox"
                                            checked={selectedRules.length === outdatedRules.length && outdatedRules.length > 0}
                                            onChange={handleToggleAll}
                                        />
                                    </th>
                                    <th style={{ textAlign: 'left', padding: '12px', borderBottom: '1px solid #e2e8f0', fontSize: '13px', color: '#64748b', fontWeight: 600 }}>
                                        규칙 이름
                                    </th>
                                    <th style={{ textAlign: 'left', padding: '12px', borderBottom: '1px solid #e2e8f0', fontSize: '13px', color: '#64748b', fontWeight: 600 }}>
                                        현재 버전
                                    </th>
                                    <th style={{ textAlign: 'left', padding: '12px', borderBottom: '1px solid #e2e8f0', fontSize: '13px', color: '#64748b', fontWeight: 600 }}>
                                        대상
                                    </th>
                                </tr>
                            </thead>
                            <tbody>
                                {outdatedRules.map(rule => (
                                    <tr key={rule.id} style={{ borderBottom: '1px solid #f1f5f9' }}>
                                        <td style={{ padding: '12px 0' }}>
                                            <input
                                                type="checkbox"
                                                checked={selectedRules.includes(rule.id)}
                                                onChange={() => handleToggleRule(rule.id)}
                                            />
                                        </td>
                                        <td style={{ padding: '12px', fontSize: '14px', color: '#334155' }}>
                                            {rule.name}
                                        </td>
                                        <td style={{ padding: '12px', fontSize: '14px', color: '#64748b' }}>
                                            <span style={{
                                                padding: '2px 6px',
                                                backgroundColor: '#f1f5f9',
                                                borderRadius: '4px',
                                                fontSize: '12px'
                                            }}>v{rule['template_version'] || 1}</span>
                                        </td>
                                        <td style={{ padding: '12px', fontSize: '14px', color: '#64748b' }}>
                                            {rule.device_name} / {rule.data_point_name}
                                        </td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    )}
                </div>

                {/* Footer */}
                <div style={{
                    padding: '20px 24px',
                    borderTop: '1px solid #e2e8f0',
                    display: 'flex',
                    justifyContent: 'flex-end',
                    gap: '12px',
                    backgroundColor: '#fff',
                    borderRadius: '0 0 12px 12px'
                }}>
                    <button
                        onClick={onClose}
                        className="btn btn-ghost"
                        style={{ padding: '8px 16px', borderRadius: '6px' }}
                    >
                        취소
                    </button>
                    <button
                        onClick={handlePropagate}
                        disabled={applying || outdatedRules.length === 0 || selectedRules.length === 0}
                        className="btn btn-primary"
                        style={{
                            padding: '8px 20px',
                            borderRadius: '6px',
                            backgroundColor: '#3b82f6',
                            color: '#fff',
                            border: 'none',
                            opacity: (applying || outdatedRules.length === 0 || selectedRules.length === 0) ? 0.5 : 1
                        }}
                    >
                        {applying ? (
                            <><i className="fas fa-spinner fa-spin" style={{ marginRight: '8px' }}></i>업데이트 중...</>
                        ) : (
                            <><i className="fas fa-sync-alt" style={{ marginRight: '8px' }}></i>선택 항목 업데이트 ({selectedRules.length})</>
                        )}
                    </button>
                </div>
            </div>
        </div>
    );
};

export default TemplatePropagateModal;
