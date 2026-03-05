import React, { useState, useEffect } from 'react';
import { Select, Tooltip, Input, Button } from 'antd';
import { useTranslation } from 'react-i18next';
import exportGatewayApi, { ExportTarget, ExportProfile, Assignment, DataPoint, PayloadTemplate, ExportTargetMapping } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

// Local helper for extracting items from API response
const extractItems = (data: any) => {
    if (Array.isArray(data)) return data;
    if (data && Array.isArray(data.items)) return data.items;
    if (data && data.data && Array.isArray(data.data.items)) return data.data.items;
    return [];
};

// [NEW] Helper to extract a single config object from various formats (object or [object])
const getConfigObject = (config: any): any => {
    try {
        const parsed = typeof config === 'string' ? JSON.parse(config) : (config || {});
        // If it's an array, take the first element (common legacy pattern in HDCL-CSP)
        if (Array.isArray(parsed)) return parsed[0] || {};
        return parsed;
    } catch {
        return {};
    }
};

// [NEW] Helper to mask sensitive keys in a config object
const maskSensitiveData = (config: any): any => {
    const obj = getConfigObject(config);
    const masked = JSON.parse(JSON.stringify(obj)); // Deep clone
    const sensitiveKeys = [
        'AccessKeyID', 'SecretAccessKey', 'apiKey', 'Authorization',
        'x-api-key', 'token', 'password', 'secret', 'secretKey'
    ];

    const maskValue = (o: any) => {
        if (!o || typeof o !== 'object') return;
        Object.keys(o).forEach(key => {
            if (sensitiveKeys.some(sk => sk.toLowerCase() === key.toLowerCase())) {
                const val = o[key];
                if (typeof val === 'string' && val.length > 0) {
                    // If it's a variable reference, show a masked placeholder to avoid exposing the variable name
                    if (val.startsWith('${') && val.endsWith('}')) {
                        o[key] = '******** (Environment Variable)';
                    } else {
                        o[key] = '********';
                    }
                }
            } else if (typeof o[key] === 'object') {
                maskValue(o[key]);
            }
        });
    };

    maskValue(masked);

    // Explicitly remove obsolete fields from preview
    delete masked.execution_order;

    return masked;
};

// [NEW] Helper to cleanup config for saving
// [NEW] Helper to cleanup config for saving
const cleanupConfig = (config: any): any => {
    const obj = getConfigObject(config);
    // Remove obsolete fields
    delete obj.execution_order;
    return obj;
};

interface TargetManagementTabProps {
    siteId?: number | null;
    tenantId?: number | null;
    isAdmin?: boolean;
    tenants?: any[];
}

const TargetManagementTab: React.FC<TargetManagementTabProps> = ({ siteId, tenantId, isAdmin, tenants = [] }) => {
    const { t } = useTranslation(['dataExport', 'common']);
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [profiles, setProfiles] = useState<ExportProfile[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingTarget, setEditingTarget] = useState<Partial<ExportTarget> | null>(null);
    const [isTesting, setIsTesting] = useState(false);
    const [hasChanges, setHasChanges] = useState(false);

    const { confirm } = useConfirmContext();

    // [FIX] 확실한 UI 반영을 위해 useEffect로 스타일을 head에 직접 주입합니다.
    useEffect(() => {
        const styleId = 'target-mgmt-custom-styles-v4';
        let styleElement = document.getElementById(styleId);

        if (!styleElement) {
            styleElement = document.createElement('style');
            styleElement.id = styleId;
            document.head.appendChild(styleElement);
        }

        styleElement.innerHTML = `
            /* 모달 컨테이너 내의 입력창 높이 상향 */
            .target-mgmt-modal .mgmt-input,
            .target-mgmt-modal .mgmt-select,
            .target-mgmt-modal .ant-input,
            .target-mgmt-modal .ant-select-selector,
            .target-mgmt-modal .ant-input-password,
            .target-mgmt-modal .ant-input-affix-wrapper {
                height: 48px !important;
                min-height: 48px !important;
                font-size: 15px !important;
                border-color: var(--neutral-300) !important; /* 표준 테두리 색상으로 원복 */
                border-width: 1px !important;
                border-radius: 8px !important;
                display: flex !important;
                align-items: center !important;
                padding: 0 12px !important;
            }

            /* Ant Design Select 내부 정렬 보정 */
            .target-mgmt-modal .ant-select-selection-item,
            .target-mgmt-modal .ant-select-selection-placeholder {
                line-height: 46px !important;
                display: flex !important;
                align-items: center !important;
            }

            /* 폼 라벨 강조 및 간격 */
            .target-mgmt-modal .mgmt-modal-form-group label {
                margin-bottom: 12px !important;
                font-weight: 700 !important;
                color: var(--neutral-800) !important;
                display: block !important;
                font-size: 14px !important;
            }

            /* Advanced Settings (JSON) 및 텍스트 영역 확장 */
            .target-mgmt-modal textarea.mgmt-input {
                height: auto !important;
                min-height: 450px !important; /* 영역이 많으므로 길게 확장 */
                padding: 16px !important;
                line-height: 1.6 !important;
                font-family: var(--font-family-mono) !important;
                font-size: 13px !important;
                background-color: #f8fafc !important; /* 연한 배경색으로 구분 */
                border-color: var(--neutral-300) !important;
                white-space: pre-wrap !important;    /* [NEW] 줄바꿈 유지 */
                word-break: break-all !important;     /* [NEW] 긴 URL 강제 줄바꿈 */
            }
            
            /* 매핑 테이블 셀 높이 */
            .target-mgmt-modal .mgmt-table td {
                padding: 14px 10px !important;
            }
        `;
    }, []);

    const handleCredentialChange = (currentVal: string, newVal: string, onUpdate: (finalVal: string) => void) => {
        // If current value is masked variable and user types something new
        if (currentVal === '********') {
            // If the user typed something into a field that was previously showing the mask
            // we assume they want to REPLACE the entire mask with the new value.
            // This is safer than trying to 'replace' the substring.
            if (newVal !== '********') {
                // Determine the newly typed text. 
                // In Ant Design Input.Password, e.target.value usually contains the new char at the end or middle.
                // We just take the whole newVal if it's different from the mask, 
                // provided it's not just a deletion that left some stars.
                if (newVal.includes('********')) {
                    const added = newVal.replace('********', '');
                    onUpdate(added);
                } else {
                    // They replaced the whole selection or similar
                    onUpdate(newVal);
                }
            }
        } else {
            onUpdate(newVal);
        }
    };

    const fetchData = async () => {
        setLoading(true);
        try {
            const [targetsRes, templatesRes, profilesRes] = await Promise.all([
                exportGatewayApi.getTargets({ tenantId }),
                exportGatewayApi.getTemplates({ tenantId }),
                exportGatewayApi.getProfiles({ tenantId })
            ]);
            setTargets(extractItems(targetsRes.data));
            setTemplates(extractItems(templatesRes.data));
            setProfiles(extractItems(profilesRes.data));
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    const handleCloseModal = async () => {
        if (hasChanges) {
            const confirmed = await confirm({
                title: t('confirm.unsavedTitle', { defaultValue: '저장되지 않은 변경사항' }),
                message: t('confirm.unsavedMsg', { defaultValue: '저장되지 않은 변경사항이 있습니다. 닫으면 데이터가 손실됩니다. 계속하시겠습니까?' }),
                confirmText: t('common:close', { defaultValue: '닫기' }),
                cancelText: t('common:cancel', { defaultValue: '취소' }),
                confirmButtonType: 'warning'
            });
            if (!confirmed) return;
        }
        setIsModalOpen(false);
        setHasChanges(false);
    };

    useEffect(() => { fetchData(); }, [siteId, tenantId]);

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();

        if (!hasChanges && editingTarget?.id) {
            await confirm({
                title: t('confirm.noChangesTitle', { defaultValue: '변경사항 없음' }),
                message: t('confirm.noChangesMsg', { defaultValue: '수정된 정보가 없습니다.' }),
                showCancelButton: false,
                confirmButtonType: 'primary'
            });
            setIsModalOpen(false);
            return;
        }

        try {
            let response;
            const targetTenantId = editingTarget?.tenant_id || tenantId;
            if (editingTarget?.id) {
                // [CLEANUP] Ensure config is an object and stripped of obsolete fields
                const cleanTarget = {
                    ...editingTarget,
                    config: cleanupConfig(editingTarget.config)
                };
                response = await exportGatewayApi.updateTarget(editingTarget.id, cleanTarget, targetTenantId);
            } else {
                const cleanTarget = {
                    ...editingTarget,
                    config: cleanupConfig(editingTarget.config)
                };
                response = await exportGatewayApi.createTarget(cleanTarget as ExportTarget, targetTenantId);
            }

            if (response.success) {
                await confirm({
                    title: t('confirm.saveComplete', { defaultValue: '저장 완료' }),
                    message: t('confirm.targetSaved', { defaultValue: '전송 대상 정보가 저장되었습니다.' }),
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
                setHasChanges(false);
                fetchData();
            } else {
                await confirm({
                    title: t('confirm.saveFailed', { defaultValue: '저장 실패' }),
                    message: response.message || t('confirm.saveError', { defaultValue: '저장 중 오류가 발생했습니다.' }),
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: t('confirm.saveFailed', { defaultValue: '저장 실패' }),
                message: t('confirm.targetSaveError', { defaultValue: '전송 대상 저장 중 오류가 발생했습니다.' }),
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleTestConnection = async () => {
        if (!editingTarget?.target_type || !editingTarget?.config) {
            await confirm({ title: t('confirm.insufficientInput', { defaultValue: '입력 정보 부족' }), message: t('confirm.noTestInfo', { defaultValue: '테스트할 정보가 설정되지 않았습니다.' }), showCancelButton: false, confirmButtonType: 'warning' });
            return;
        }

        setIsTesting(true);
        try {
            const response = await exportGatewayApi.testTargetConnection({
                target_type: editingTarget.target_type,
                config: editingTarget.config
            });

            if (response.success) {
                await confirm({
                    title: t('confirm.testSuccess', { defaultValue: '테스트 성공' }),
                    message: response.message || t('confirm.testSuccessMsg', { defaultValue: '연결 테스트가 성공적으로 완료되었습니다.' }),
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
            } else {
                await confirm({
                    title: t('confirm.testFailed', { defaultValue: '테스트 실패' }),
                    message: response.message || t('confirm.testFailedMsg', { defaultValue: '알 수 없는 이유로 실패했습니다.' }),
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error: any) {
            await confirm({
                title: t('confirm.testError', { defaultValue: '테스트 오류' }),
                message: error.message || t('confirm.serverCommError', { defaultValue: '서버 통신 중 오류가 발생했습니다.' }),
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        } finally {
            setIsTesting(false);
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: t('confirm.deleteTargetTitle', { defaultValue: '전송 대상 삭제 확인' }),
            message: t('confirm.deleteTargetMsg', { defaultValue: '이 전송 대상을 삭제하시겠습니까?' }),
            confirmText: t('common:delete', { defaultValue: '삭제' }),
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteTarget(id, tenantId);
            fetchData();
        } catch (error) {
            await confirm({ title: t('confirm.deleteFailed', { defaultValue: '삭제 실패' }), message: t('confirm.deleteTargetError', { defaultValue: '전송 대상 삭제 중 오류가 발생했습니다.' }), showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>{t('target.settingsTitle', { defaultValue: '전송 대상 설정' })}</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingTarget({ target_type: 'http', is_enabled: true, config: {} }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> {t('target.addTarget', { defaultValue: '대상 추가' })}
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>{t('col.name', { defaultValue: '이름' })}</th>
                            <th>{t('col.type', { defaultValue: '유형' })}</th>
                            <th>{t('col.status', { defaultValue: '상태' })}</th>
                            <th>{t('col.manage', { defaultValue: '관리' })}</th>
                        </tr>
                    </thead>
                    <tbody>
                        {targets.map(tgt => (
                            <tr key={tgt.id}>
                                <td>{tgt.name}</td>
                                <td>
                                    <span className="mgmt-badge neutral" style={{ textTransform: 'uppercase' }}>
                                        {tgt.target_type}
                                    </span>
                                </td>
                                <td>
                                    <span className={`mgmt-badge ${tgt.is_enabled ? 'success' : 'neutral'}`}>
                                        {tgt.is_enabled ? t('status.active', { defaultValue: '활성' }) : t('status.inactive', { defaultValue: '비활성' })}
                                    </span>
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => {
                                            setEditingTarget({ ...tgt, config: typeof tgt.config === 'string' ? tgt.config : JSON.stringify(tgt.config, null, 2) });
                                            setIsModalOpen(true);
                                        }} style={{ width: 'auto' }}>{t('common:edit', { defaultValue: '편집' })}</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs mgmt-btn-error" onClick={() => handleDelete(tgt.id)} style={{ width: 'auto' }}>{t('common:delete', { defaultValue: '삭제' })}</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-container target-mgmt-modal" style={{ width: '95vw', maxWidth: '1600px', display: 'flex', flexDirection: 'column', height: '75vh', maxHeight: '85vh', background: 'white', borderRadius: '12px', boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.3)', overflow: 'hidden', border: '1px solid #e2e8f0' }}>
                        <div className="mgmt-modal-header">
                            <div className="mgmt-modal-title">
                                <h2 style={{ fontSize: '18px', fontWeight: 700, margin: 0 }}>{editingTarget?.id ? t('target.editTitle', { defaultValue: '전송 대상 편집' }) : t('target.addTitle', { defaultValue: '전송 대상 추가' })}</h2>
                            </div>
                            <button className="mgmt-close-btn" onClick={handleCloseModal} style={{ fontSize: '24px' }}>&times;</button>
                        </div>
                        <form onSubmit={handleSave} style={{ display: 'flex', flexDirection: 'column', flex: 1, overflow: 'hidden' }}>
                            <div className="mgmt-modal-body" style={{ overflowY: 'auto', overflowX: 'hidden', flex: 1, paddingBottom: '20px' }}>
                                <div style={{ display: 'flex', gap: '20px', alignItems: 'flex-start', height: '100%' }}>
                                    {/* Column 1: Basic Info */}
                                    <div style={{ flex: 1, maxWidth: '400px', borderRight: '1px solid #eee', paddingRight: '20px', display: 'flex', flexDirection: 'column' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>{t('target.basicInfo', { defaultValue: '기본 정보' })}</div>
                                        <div className="mgmt-modal-form-group">
                                            <label>{t('target.targetName', { defaultValue: '대상 이름' })}</label>
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                required
                                                value={editingTarget?.name || ''}
                                                onChange={e => { setEditingTarget({ ...editingTarget, name: e.target.value }); setHasChanges(true); }}
                                                placeholder="e.g. HQ Control System"
                                            />
                                        </div>
                                        {isAdmin && !tenantId && (
                                            <div className="mgmt-modal-form-group">
                                                <label>{t('target.tenantLabel', { defaultValue: 'Tenant' })} <span style={{ color: 'red' }}>*</span></label>
                                                <select
                                                    className="mgmt-select"
                                                    required
                                                    value={editingTarget?.tenant_id || ''}
                                                    onChange={e => { setEditingTarget({ ...editingTarget, tenant_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                                >
                                                    <option value="">{t('target.selectTenant', { defaultValue: '(Select Tenant)' })}</option>
                                                    {tenants.map(t => (
                                                        <option key={t.id} value={t.id}>{t.company_name}</option>
                                                    ))}
                                                </select>
                                            </div>
                                        )}
                                        <div className="mgmt-modal-form-group">
                                            <label>{t('target.protocol', { defaultValue: 'Protocol' })}</label>
                                            <select
                                                className="mgmt-select"
                                                required
                                                value={editingTarget?.target_type?.toLowerCase() || 'http'}
                                                onChange={e => { setEditingTarget({ ...editingTarget, target_type: e.target.value.toLowerCase() }); setHasChanges(true); }}
                                            >
                                                <option value="http">HTTP / HTTPS (Rest API)</option>
                                                <option value="mqtt">MQTT {t('target.broker', { defaultValue: 'Broker' })}</option>
                                                <option value="s3">AWS S3 / MinIO ({t('target.file', { defaultValue: 'File' })})</option>
                                                <option value="ftp">FTP / SFTP ({t('target.file', { defaultValue: 'File' })})</option>
                                                <option value="database">{t('target.externalDb', { defaultValue: 'External Database (SQL)' })}</option>
                                            </select>
                                        </div>
                                        {/* Profile and Template associations have been decoupled from core Target definition */}
                                    </div>

                                    {/* Column 2: Protocol Specific Config */}
                                    <div style={{ flex: 1, paddingRight: '20px', display: 'flex', flexDirection: 'column' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>{t('target.transmitSettings', { defaultValue: 'Transmission Settings' })}</div>
                                        {editingTarget?.target_type?.toLowerCase() === 'http' ? (
                                            <>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.endpointUrl', { defaultValue: 'Endpoint URL' })}</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="http://api.example.com/v1/data"
                                                        value={getConfigObject(editingTarget.config).url || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, url: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.httpMethod', { defaultValue: 'HTTP Method' })}</label>
                                                    <select
                                                        className="mgmt-select"
                                                        value={(() => { try { return (typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : editingTarget.config).method || 'POST'; } catch { return 'POST'; } })()}
                                                        onChange={e => {
                                                            let c: any = {};
                                                            try { c = typeof editingTarget.config === 'string' ? JSON.parse(editingTarget.config) : (editingTarget.config || {}); } catch { }
                                                            c = { ...c, method: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(c, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    >
                                                        <option value="POST">POST</option>
                                                        <option value="PUT">PUT</option>
                                                        <option value="PATCH">PATCH</option>
                                                    </select>
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.headers', { defaultValue: 'Headers (JSON)' })}</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder='{"Content-Type": "application/json"}'
                                                        value={(() => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const h = c.headers || {};
                                                            // For the JSON field, we NEVER show variables or masks, 
                                                            // we just show OTHER headers to avoid exposure.
                                                            const otherHeaders = { ...h };
                                                            const sensitive = ['Authorization', 'x-api-key', 'apiKey', 'token'];
                                                            sensitive.forEach(k => delete (otherHeaders as any)[k]);
                                                            return JSON.stringify(otherHeaders, null, 1);
                                                        })()}
                                                        readOnly // Standard headers can be edited in the JSON block if needed, but not sensitive ones
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            try {
                                                                const newHeaders = JSON.parse(e.target.value);
                                                                const newConfig = { ...c, headers: newHeaders };
                                                                setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                                setHasChanges(true);
                                                            } catch { }
                                                        }}
                                                    />
                                                </div>

                                                {/* 인증 키: headers.x-api-key에만 저장 */}
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.apiKey', { defaultValue: 'Auth Key (API Key / Token)' })}</label>
                                                    <Input.Password
                                                        className="mgmt-input"
                                                        placeholder="x-api-key"
                                                        value={(() => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const val = c.headers?.['x-api-key'] || '';
                                                            return (val.startsWith('${') && val.endsWith('}')) ? '********' : val;
                                                        })()}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const val = c.headers?.['x-api-key'] || '';
                                                            const currentVal = val.startsWith('${') ? '********' : val;

                                                            handleCredentialChange(currentVal, e.target.value, (finalVal) => {
                                                                const headers = { ...(c.headers || {}) };
                                                                if (finalVal) {
                                                                    headers['x-api-key'] = finalVal;
                                                                } else {
                                                                    delete headers['x-api-key'];
                                                                }
                                                                setEditingTarget({ ...editingTarget, config: JSON.stringify({ ...c, headers }, null, 2) });
                                                                setHasChanges(true);
                                                            });
                                                        }}
                                                    />
                                                    <div className="mgmt-modal-form-hint">{t('target.apiKeyHint', { defaultValue: 'Stored as x-api-key header.' })}</div>
                                                </div>
                                            </>
                                        ) : editingTarget?.target_type?.toLowerCase() === 's3' ? (
                                            <>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.s3Endpoint', { defaultValue: 'S3 Endpoint (Service URL)' })}</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="https://s3.ap-northeast-2.amazonaws.com"
                                                        value={getConfigObject(editingTarget.config).endpoint || getConfigObject(editingTarget.config).S3ServiceUrl || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, endpoint: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.bucketName', { defaultValue: 'Bucket Name' })}</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="my-export-bucket"
                                                        value={getConfigObject(editingTarget.config).BucketName || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, BucketName: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.folderPath', { defaultValue: 'Folder Path' })}</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="data/exports"
                                                        value={getConfigObject(editingTarget.config).Folder || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, Folder: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.objectKeyTpl', { defaultValue: 'Object Key Template (optional)' })}</label>
                                                    <input
                                                        type="text"
                                                        className="mgmt-input"
                                                        placeholder="e.g. {year}/{month}/{day}/{hour}/{minute}/{second}_{building_id}.json"
                                                        value={getConfigObject(editingTarget.config).ObjectKeyTemplate || ''}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const newConfig = { ...c, ObjectKeyTemplate: e.target.value };
                                                            setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                            setHasChanges(true);
                                                        }}
                                                    />
                                                    <div className="mgmt-modal-form-hint">사용 가능 변수: {'{year}, {month}, {day}, {hour}, {minute}, {second}, {building_id}'}</div>
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.accessKeyId', { defaultValue: 'Access Key ID' })}</label>
                                                    <Input.Password
                                                        className="mgmt-input"
                                                        placeholder="AKIA..."
                                                        value={(() => {
                                                            const val = getConfigObject(editingTarget.config).AccessKeyID || '';
                                                            return (val.startsWith('${') && val.endsWith('}')) ? '********' : val;
                                                        })()}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const val = c.AccessKeyID || '';
                                                            const currentVal = val.startsWith('${') ? '********' : val;

                                                            handleCredentialChange(currentVal, e.target.value, (finalVal) => {
                                                                const newConfig = { ...c, AccessKeyID: finalVal };
                                                                setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                                setHasChanges(true);
                                                            });
                                                        }}
                                                    />
                                                </div>
                                                <div className="mgmt-modal-form-group">
                                                    <label>{t('target.secretAccessKey', { defaultValue: 'Secret Access Key' })}</label>
                                                    <Input.Password
                                                        className="mgmt-input"
                                                        placeholder="Secret Key..."
                                                        value={(() => {
                                                            const val = getConfigObject(editingTarget.config).SecretAccessKey || '';
                                                            return (val.startsWith('${') && val.endsWith('}')) ? '********' : val;
                                                        })()}
                                                        onChange={e => {
                                                            const c = getConfigObject(editingTarget.config);
                                                            const val = c.SecretAccessKey || '';
                                                            const currentVal = val.startsWith('${') ? '********' : val;

                                                            handleCredentialChange(currentVal, e.target.value, (finalVal) => {
                                                                const newConfig = { ...c, SecretAccessKey: finalVal };
                                                                setEditingTarget({ ...editingTarget, config: JSON.stringify(newConfig, null, 2) });
                                                                setHasChanges(true);
                                                            });
                                                        }}
                                                    />
                                                </div>
                                            </>
                                        ) : (
                                            <div className="mgmt-alert info">
                                                {t('target.noForm', { defaultValue: 'This target type has no dedicated input form.' })} ({editingTarget?.target_type?.toLowerCase()}).<br />
                                                {t('target.noFormHint', { defaultValue: 'Configure directly in the JSON settings panel on the right.' })}
                                            </div>
                                        )}
                                        {/* Activation Checkbox Moved below Column 2 inputs */}
                                        <div style={{ marginTop: 'auto', paddingTop: '15px', borderTop: '1px solid #eee' }}>
                                            <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center' }}>
                                                <input
                                                    type="checkbox"
                                                    checked={editingTarget?.is_enabled ?? true}
                                                    onChange={e => { setEditingTarget({ ...editingTarget, is_enabled: e.target.checked }); setHasChanges(true); }}
                                                    style={{ marginRight: '8px' }}
                                                />
                                                {t('target.activateTarget', { defaultValue: '대상 활성화' })}
                                            </label>
                                        </div>
                                    </div>

                                    {/* Column 3: JSON Config */}
                                    <div style={{ flex: 1.2, minWidth: '300px', borderLeft: '1px solid #eee', paddingLeft: '20px', display: 'flex', flexDirection: 'column' }}>
                                        <div style={{ marginBottom: '15px', fontWeight: 'bold', color: '#333' }}>{t('target.advancedJson', { defaultValue: 'Advanced Settings (JSON)' })}</div>
                                        <div className="modal-form-hint" style={{ marginBottom: '10px' }}>
                                            {t('target.advancedJsonHint', { defaultValue: 'Auto-synced with the left form. Edit directly for fine-tuning.' })}
                                        </div>
                                        <textarea
                                            className="mgmt-input"
                                            style={{ flex: 1, fontFamily: 'monospace', fontSize: '12px', resize: 'none', minHeight: '300px', backgroundColor: '#fdfdfd' }}
                                            value={JSON.stringify(maskSensitiveData(editingTarget?.config), null, 2)}
                                            readOnly // Masked version should be read-only to avoid overwriting real keys with asterisks
                                            placeholder='{ "url": ..., "bucket": ... }'
                                        />
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer" style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%' }}>
                                <button type="button" className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={handleTestConnection} disabled={isTesting} style={{ marginRight: 'auto', borderColor: 'var(--primary-500)', color: 'var(--primary-600)', display: 'flex', alignItems: 'center', gap: '8px', width: 'auto' }}>
                                    {isTesting ? (
                                        <>
                                            <i className="fas fa-spinner fa-spin" /> {t('target.testing', { defaultValue: '테스트 중...' })}
                                        </>
                                    ) : (
                                        <>
                                            <i className="fas fa-plug" /> {t('target.connectionTest', { defaultValue: '연결 테스트' })}
                                        </>
                                    )}
                                </button>
                                <div className="mgmt-footer-right" style={{ display: 'flex', gap: '8px' }}>
                                    <button type="button" className="mgmt-btn mgmt-btn-outline" onClick={() => setIsModalOpen(false)} style={{ width: 'auto' }}>{t('common:cancel', { defaultValue: '취소' })}</button>
                                    <button type="submit" className="mgmt-btn mgmt-btn-primary" style={{ width: 'auto' }}>{t('common:save', { defaultValue: '저장' })}</button>
                                </div>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default TargetManagementTab;
