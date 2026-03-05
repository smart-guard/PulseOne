import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import exportGatewayApi, { ExportTarget, ExportProfile, ExportSchedule } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

const CRON_PRESETS = [
    { key: 'cron1min', value: '* * * * *' },
    { key: 'cron5min', value: '*/5 * * * *' },
    { key: 'cron10min', value: '*/10 * * * *' },
    { key: 'cron15min', value: '*/15 * * * *' },
    { key: 'cron30min', value: '*/30 * * * *' },
    { key: 'cronHourly', value: '0 * * * *' },
    { key: 'cronDaily0', value: '0 0 * * *' },
    { key: 'cronDaily9', value: '0 9 * * *' },
    { key: 'cronWeeklyMon', value: '0 0 * * 1' },
];

interface ScheduleManagementTabProps {
    siteId?: number | null;
    tenantId?: number | null;
    isAdmin?: boolean;
    tenants?: any[];
}

const ScheduleManagementTab: React.FC<ScheduleManagementTabProps> = ({ siteId, tenantId, isAdmin, tenants = [] }) => {
    const { t } = useTranslation(['dataExport', 'common']);
    const [schedules, setSchedules] = useState<any[]>([]);
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [profiles, setProfiles] = useState<ExportProfile[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingSchedule, setEditingSchedule] = useState<Partial<any> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
    const { confirm } = useConfirmContext();

    const fetchData = async () => {
        setLoading(true);
        try {
            const [schedulesRes, targetsRes, profilesRes] = await Promise.all([
                exportGatewayApi.getSchedules({ tenantId }),
                exportGatewayApi.getTargets({ tenantId }),
                exportGatewayApi.getProfiles({ tenantId })
            ]);
            setSchedules(schedulesRes.data || []);
            setTargets(targetsRes.data || []);
            setProfiles(profilesRes.data || []);
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
                confirmText: t('close', { ns: 'common', defaultValue: '닫기' }),
                cancelText: t('cancel', { ns: 'common', defaultValue: '취소' }),
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

        if (!hasChanges && editingSchedule?.id) {
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
            const targetTenantId = editingSchedule?.tenant_id || tenantId;
            let response;
            if (editingSchedule?.id) {
                response = await exportGatewayApi.updateSchedule(editingSchedule.id, editingSchedule as ExportSchedule, targetTenantId);
            } else {
                response = await exportGatewayApi.createSchedule(editingSchedule as ExportSchedule, targetTenantId);
            }

            if (response.success) {
                await confirm({
                    title: t('confirm.saveComplete', { defaultValue: '저장 완료' }),
                    message: t('confirm.scheduleSaved', { defaultValue: '스케줄이 저장되었습니다.' }),
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
                setHasChanges(false);
                fetchData();
            } else {
                await confirm({
                    title: t('confirm.saveFailed', { defaultValue: '저장 실패' }),
                    message: response.message || t('confirm.scheduleError', { defaultValue: '스케줄 저장 중 오류가 발생했습니다.' }),
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: t('confirm.saveFailed', { defaultValue: '저장 실패' }),
                message: t('confirm.scheduleError', { defaultValue: '스케줄 저장 중 오류가 발생했습니다.' }),
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: t('confirm.deleteScheduleTitle', { defaultValue: '스케줄 삭제 확인' }),
            message: t('confirm.deleteScheduleMsg', { defaultValue: '이 전송 스케줄을 삭제하시겠습니까?' }),
            confirmText: t('delete', { ns: 'common', defaultValue: '삭제' }),
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteSchedule(id, tenantId);
            fetchData();
        } catch (error) {
            await confirm({ title: t('confirm.deleteFailed', { defaultValue: '삭제 실패' }), message: t('confirm.scheduleDeleteError', { defaultValue: '스케줄 삭제 중 오류가 발생했습니다.' }), showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>{t('schedule.settingsTitle', { defaultValue: '일괄 전송 스케줄 관리' })}</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingSchedule({ schedule_name: '', cron_expression: '0 0 * * *', data_range: 'hour', lookback_periods: 1, timezone: 'Asia/Seoul', is_enabled: true }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> {t('schedule.addSchedule', { defaultValue: '스케줄 추가' })}
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>{t('col.scheduleName', { defaultValue: '스케줄 이름' })}</th>
                            <th>{t('col.target', { defaultValue: '대상' })}</th>
                            <th>{t('col.cronExpression', { defaultValue: 'Cron 표현식' })}</th>
                            <th>{t('col.status', { defaultValue: '상태' })}</th>
                            <th>{t('col.lastRun', { defaultValue: '마지막 실행' })}</th>
                            <th>{t('col.manage', { defaultValue: '관리' })}</th>
                        </tr>
                    </thead>
                    <tbody>
                        {schedules.map(s => (
                            <tr key={s.id}>
                                <td>
                                    <div style={{ fontWeight: 600 }}>{s.schedule_name}</div>
                                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>{s.description}</div>
                                </td>
                                <td>{s.target_id ? (s.target_name || `ID: ${s.target_id}`) : <span style={{ color: 'var(--neutral-400)', fontStyle: 'italic' }}>{t('scheduleTab.sharedPreset')}</span>}</td>
                                <td><code style={{ background: '#f0f0f0', padding: '2px 4px', borderRadius: '4px' }}>{s.cron_expression}</code></td>
                                <td>
                                    <span className={`mgmt-badge ${s.is_enabled ? 'success' : 'neutral'}`}>
                                        {s.is_enabled ? t('status.active', { defaultValue: '활성' }) : t('status.inactive', { defaultValue: '비활성' })}
                                    </span>
                                </td>
                                <td>
                                    {s.last_run_at ? new Date(s.last_run_at).toLocaleString() : '-'}
                                    {s.last_status && <div style={{ fontSize: '10px', color: s.last_status === 'success' ? 'var(--success-600)' : 'var(--error-600)' }}>{s.last_status}</div>}
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => { setEditingSchedule(s); setIsModalOpen(true); }} style={{ width: 'auto' }}>{t('edit', { ns: 'common', defaultValue: '편집' })}</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs mgmt-btn-error" onClick={() => handleDelete(s.id)} style={{ width: 'auto' }}>{t('delete', { ns: 'common', defaultValue: '삭제' })}</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <style>{`
                        .cron-preset-grid {
                            display: grid;
                            grid-template-columns: repeat(3, 1fr);
                            gap: 8px;
                            margin-top: 12px;
                        }
                        .cron-preset-item {
                            padding: 10px;
                            border: 1px solid #e2e8f0;
                            border-radius: 8px;
                            background: white;
                            cursor: pointer;
                            transition: all 0.2s;
                            text-align: center;
                        }
                        .cron-preset-item:hover {
                            border-color: var(--primary-400);
                            background: var(--primary-50);
                            transform: translateY(-1px);
                            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
                        }
                        .cron-preset-label {
                            font-size: 11px;
                            font-weight: 700;
                            color: var(--primary-700);
                            display: block;
                        }
                        .cron-preset-val {
                            font-size: 10px;
                            color: #64748b;
                            font-family: monospace;
                        }
                        .cron-guide-box {
                            background: #f8fafc;
                            border-radius: 12px;
                            padding: 16px;
                            border: 1px solid #e2e8f0;
                            margin-top: 20px;
                        }
                    `}</style>
                    <div className="mgmt-modal-content" style={{ maxWidth: '850px', width: '90%' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">{editingSchedule?.id ? t('schedule.editTitle', { defaultValue: '스케줄 편집' }) : t('schedule.newTitle', { defaultValue: '새 전송 스케줄' })}</h3>
                            <button className="mgmt-modal-close" onClick={handleCloseModal}>&times;</button>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body" style={{ display: 'flex', gap: '24px' }}>
                                {/* Left Side: Basic Config */}
                                <div style={{ flex: 1 }}>
                                    <div className="mgmt-modal-form-section">
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontWeight: 700 }}>{t('scheduleTab.scheduleName')}</label>
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                required
                                                value={editingSchedule?.schedule_name || ''}
                                                onChange={e => { setEditingSchedule({ ...editingSchedule, schedule_name: e.target.value }); setHasChanges(true); }}
                                                placeholder={t('scheduleTab.scheduleNamePlaceholder')}
                                            />
                                        </div>

                                        {isAdmin && !tenantId && (
                                            <div className="mgmt-modal-form-group">
                                                <label style={{ fontWeight: 700 }}>{t('scheduleTab.tenant')} <span style={{ color: 'red' }}>*</span></label>
                                                <select
                                                    className="mgmt-select"
                                                    required
                                                    value={editingSchedule?.tenant_id || ''}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, tenant_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                                >
                                                    <option value="">{t('scheduleTab.selectTenant')}</option>
                                                    {tenants.map(t => (
                                                        <option key={t.id} value={t.id}>{t.company_name}</option>
                                                    ))}
                                                </select>
                                                <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '4px' }}>{t('scheduleTab.tenantAdminHint')}</div>
                                            </div>
                                        )}
                                        {/* Target selection has been decoupled from global schedule presets. Targets are assigned via Gateway Wizard. */}

                                        <div style={{ background: '#f8fafc', padding: '16px', borderRadius: '12px', border: '1px solid #e2e8f0', marginTop: '16px' }}>
                                            <label style={{ fontSize: '13px', fontWeight: 700, color: '#475569', marginBottom: '12px', display: 'block' }}>
                                                {t('scheduleTab.dataWindow')}
                                            </label>
                                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap', color: '#64748b', fontSize: '14px' }}>
                                                <span>{t('scheduleTab.dataWindowFrom')}</span>
                                                <input
                                                    type="number"
                                                    className="mgmt-input sm"
                                                    style={{ width: '70px', textAlign: 'center', fontWeight: 700 }}
                                                    value={editingSchedule?.lookback_periods || 1}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, lookback_periods: parseInt(e.target.value) }); setHasChanges(true); }}
                                                    min={1}
                                                />
                                                <select
                                                    className="mgmt-select sm"
                                                    style={{ width: '120px', fontWeight: 700 }}
                                                    value={editingSchedule?.data_range || 'hour'}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, data_range: e.target.value }); setHasChanges(true); }}
                                                >
                                                    <option value="minute">{t('scheduleTab.dataWindowMinute')}</option>
                                                    <option value="hour">{t('scheduleTab.dataWindowHour')}</option>
                                                    <option value="day">{t('scheduleTab.dataWindowDay')}</option>
                                                </select>
                                                <span>{t('scheduleTab.dataWindowTo')}</span>
                                            </div>
                                            <div style={{ marginTop: '10px', fontSize: '12px', color: 'var(--primary-600)', background: 'var(--primary-50)', padding: '8px 12px', borderRadius: '6px' }}>
                                                <i className="fas fa-magic" style={{ marginRight: '6px' }} />
                                                <strong>{t('common:description')}:</strong> {t('scheduleTab.dataWindowDesc')}
                                            </div>
                                        </div>

                                        <div className="mgmt-modal-form-group" style={{ marginTop: '20px' }}>
                                            <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', padding: '10px', background: '#f1f5f9', borderRadius: '8px' }}>
                                                <input
                                                    type="checkbox"
                                                    checked={editingSchedule?.is_enabled ?? true}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, is_enabled: e.target.checked }); setHasChanges(true); }}
                                                />
                                                <span style={{ fontWeight: 600 }}>{t('scheduleTab.activateSchedule')}</span>
                                            </label>
                                        </div>
                                    </div>
                                </div>

                                {/* Right Side: Cron Presets & Guide */}
                                <div style={{ flex: 1.2, borderLeft: '1px solid #f1f5f9', paddingLeft: '24px' }}>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontWeight: 700, display: 'flex', justifyContent: 'space-between' }}>
                                            {t('scheduleTab.cronSettings')}
                                            <span style={{ fontSize: '11px', color: 'var(--primary-600)', fontWeight: 400 }}>{t('scheduleTab.cronAutoFillHint')}</span>
                                        </label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            required
                                            style={{ fontFamily: 'monospace', fontSize: '16px', letterSpacing: '2px', textAlign: 'center', background: '#fffbeb', borderColor: '#fde68a' }}
                                            value={editingSchedule?.cron_expression || ''}
                                            onChange={e => { setEditingSchedule({ ...editingSchedule, cron_expression: e.target.value }); setHasChanges(true); }}
                                            placeholder="* * * * *"
                                        />

                                        <div className="cron-preset-grid">
                                            {CRON_PRESETS.map((p, idx) => (
                                                <div
                                                    key={idx}
                                                    className="cron-preset-item"
                                                    onClick={() => { setEditingSchedule({ ...editingSchedule, cron_expression: p.value }); setHasChanges(true); }}
                                                >
                                                    <span className="cron-preset-label">{t(`scheduleTab.${p.key}`)}</span>
                                                    <span className="cron-preset-val">{p.value}</span>
                                                </div>
                                            ))}
                                        </div>
                                    </div>

                                    <div className="cron-guide-box">
                                        <div style={{ fontWeight: 700, fontSize: '12px', color: '#475569', marginBottom: '8px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                                            <i className="fas fa-info-circle" style={{ color: 'var(--primary-500)' }} /> {t('scheduleTab.cronGuide')}
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', textAlign: 'center', fontSize: '10px', color: '#64748b', background: 'white', padding: '8px', borderRadius: '6px', border: '1px solid #e2e8f0' }}>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>min(0-59)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>hr(0-23)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>day(1-31)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>month(1-12)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>{t('scheduleTab.cronDow')}</div>
                                        </div>
                                        <div style={{ marginTop: '10px', fontSize: '11px', color: '#64748b' }}>
                                            <div style={{ marginBottom: '4px' }}>• <code>*</code>: {t('scheduleTab.cronHintAny')}</div>
                                            <div style={{ marginBottom: '4px' }}>• <code>*/N</code>: {t('scheduleTab.cronHintEvery')}</div>
                                            <div>• <code>0-5</code>: {t('scheduleTab.cronHintRange')}</div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer">
                                <button type="button" className="mgmt-btn mgmt-btn-outline" onClick={handleCloseModal}>{t('cancel', { ns: 'common' })}</button>
                                <button type="submit" className="mgmt-btn mgmt-btn-primary" style={{ padding: '0 32px' }}>{t('schedule.saveSchedule', { defaultValue: 'Save Schedule' })}</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default ScheduleManagementTab;
