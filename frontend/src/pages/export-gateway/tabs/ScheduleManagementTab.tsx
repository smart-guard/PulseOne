import React, { useState, useEffect } from 'react';
import exportGatewayApi, { ExportTarget, ExportProfile, ExportSchedule } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

const CRON_PRESETS = [
    { label: 'Every minute', value: '* * * * *', description: 'Sends data every minute.' },
    { label: 'Every 5 mins', value: '*/5 * * * *', description: 'Sends data every 5 minutes.' },
    { label: 'Every 10 mins', value: '*/10 * * * *', description: 'Sends data every 10 minutes.' },
    { label: 'Every 15 mins', value: '*/15 * * * *', description: 'Sends data every 15 minutes.' },
    { label: 'Every 30 mins', value: '*/30 * * * *', description: 'Sends data every 30 minutes.' },
    { label: 'Hourly', value: '0 * * * *', description: 'Sends data every hour on the hour.' },
    { label: 'Daily 00:00', value: '0 0 * * *', description: 'Sends data at midnight (00:00) daily.' },
    { label: 'Daily 09:00', value: '0 9 * * *', description: 'Sends data at 9 AM daily.' },
    { label: 'Weekly Mon', value: '0 0 * * 1', description: 'Sends data every Monday at midnight.' },
];

interface ScheduleManagementTabProps {
    siteId?: number | null;
    tenantId?: number | null;
    isAdmin?: boolean;
    tenants?: any[];
}

const ScheduleManagementTab: React.FC<ScheduleManagementTabProps> = ({ siteId, tenantId, isAdmin, tenants = [] }) => {
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
                title: 'Unsaved Changes Warning',
                message: 'You have unsaved changes. Closing without saving will lose all data. Continue?',
                confirmText: 'Close',
                cancelText: 'Cancel',
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
                title: 'No Changes',
                message: 'No information has been modified.',
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
                    title: 'Save Complete',
                    message: 'Schedule saved successfully.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
                setHasChanges(false);
                fetchData();
            } else {
                await confirm({
                    title: 'Save Failed',
                    message: response.message || 'Error occurred while saving schedule.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: 'Save Failed',
                message: 'Error occurred while saving schedule.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: 'Confirm Delete Schedule',
            message: 'Delete this transmission schedule?',
            confirmText: 'Delete',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteSchedule(id, tenantId);
            fetchData();
        } catch (error) {
            await confirm({ title: 'Delete Failed', message: 'Error occurred while deleting schedule.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>Batch Transmission Schedule Management</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingSchedule({ schedule_name: '', cron_expression: '0 0 * * *', data_range: 'hour', lookback_periods: 1, timezone: 'Asia/Seoul', is_enabled: true }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> Add Schedule
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>Schedule Name</th>
                            <th>Target</th>
                            <th>Cron Expression</th>
                            <th>Status</th>
                            <th>Last Run</th>
                            <th>Manage</th>
                        </tr>
                    </thead>
                    <tbody>
                        {schedules.map(s => (
                            <tr key={s.id}>
                                <td>
                                    <div style={{ fontWeight: 600 }}>{s.schedule_name}</div>
                                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>{s.description}</div>
                                </td>
                                <td>{s.target_id ? (s.target_name || `ID: ${s.target_id}`) : <span style={{ color: 'var(--neutral-400)', fontStyle: 'italic' }}>Shared Preset (no target)</span>}</td>
                                <td><code style={{ background: '#f0f0f0', padding: '2px 4px', borderRadius: '4px' }}>{s.cron_expression}</code></td>
                                <td>
                                    <span className={`mgmt-badge ${s.is_enabled ? 'success' : 'neutral'}`}>
                                        {s.is_enabled ? 'Active' : 'Inactive'}
                                    </span>
                                </td>
                                <td>
                                    {s.last_run_at ? new Date(s.last_run_at).toLocaleString() : '-'}
                                    {s.last_status && <div style={{ fontSize: '10px', color: s.last_status === 'success' ? 'var(--success-600)' : 'var(--error-600)' }}>{s.last_status}</div>}
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => { setEditingSchedule(s); setIsModalOpen(true); }} style={{ width: 'auto' }}>Edit</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs mgmt-btn-error" onClick={() => handleDelete(s.id)} style={{ width: 'auto' }}>Delete</button>
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
                            <h3 className="mgmt-modal-title">{editingSchedule?.id ? "Edit Schedule" : "New Transmission Schedule"}</h3>
                            <button className="mgmt-modal-close" onClick={handleCloseModal}>&times;</button>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body" style={{ display: 'flex', gap: '24px' }}>
                                {/* Left Side: Basic Config */}
                                <div style={{ flex: 1 }}>
                                    <div className="mgmt-modal-form-section">
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontWeight: 700 }}>Schedule Name</label>
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                required
                                                value={editingSchedule?.schedule_name || ''}
                                                onChange={e => { setEditingSchedule({ ...editingSchedule, schedule_name: e.target.value }); setHasChanges(true); }}
                                                placeholder="e.g. Daily EOD Data Transfer"
                                            />
                                        </div>

                                        {isAdmin && !tenantId && (
                                            <div className="mgmt-modal-form-group">
                                                <label style={{ fontWeight: 700 }}>Tenant <span style={{ color: 'red' }}>*</span></label>
                                                <select
                                                    className="mgmt-select"
                                                    required
                                                    value={editingSchedule?.tenant_id || ''}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, tenant_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                                >
                                                    <option value="">(Select Tenant)</option>
                                                    {tenants.map(t => (
                                                        <option key={t.id} value={t.id}>{t.company_name}</option>
                                                    ))}
                                                </select>
                                                <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginTop: '4px' }}>Assign this schedule to a specific tenant with admin privileges.</div>
                                            </div>
                                        )}
                                        {/* Target selection has been decoupled from global schedule presets. Targets are assigned via Gateway Wizard. */}

                                        <div style={{ background: '#f8fafc', padding: '16px', borderRadius: '12px', border: '1px solid #e2e8f0', marginTop: '16px' }}>
                                            <label style={{ fontSize: '13px', fontWeight: 700, color: '#475569', marginBottom: '12px', display: 'block' }}>
                                                Data Collection Range (Data Window)
                                            </label>
                                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap', color: '#64748b', fontSize: '14px' }}>
                                                <span>As of execution time, collect the past</span>
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
                                                    <option value="minute">Minutes</option>
                                                    <option value="hour">Hours</option>
                                                    <option value="day">Days</option>
                                                </select>
                                                <span>of data and transmit.</span>
                                            </div>
                                            <div style={{ marginTop: '10px', fontSize: '12px', color: 'var(--primary-600)', background: 'var(--primary-50)', padding: '8px 12px', borderRadius: '6px' }}>
                                                <i className="fas fa-magic" style={{ marginRight: '6px' }} />
                                                <strong>Description:</strong> When the schedule runs, it fetches all accumulated data going back the configured duration and sends it to the target.
                                            </div>
                                        </div>

                                        <div className="mgmt-modal-form-group" style={{ marginTop: '20px' }}>
                                            <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', padding: '10px', background: '#f1f5f9', borderRadius: '8px' }}>
                                                <input
                                                    type="checkbox"
                                                    checked={editingSchedule?.is_enabled ?? true}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, is_enabled: e.target.checked }); setHasChanges(true); }}
                                                />
                                                <span style={{ fontWeight: 600 }}>Activate Schedule</span>
                                            </label>
                                        </div>
                                    </div>
                                </div>

                                {/* Right Side: Cron Presets & Guide */}
                                <div style={{ flex: 1.2, borderLeft: '1px solid #f1f5f9', paddingLeft: '24px' }}>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontWeight: 700, display: 'flex', justifyContent: 'space-between' }}>
                                            Cron Expression Settings
                                            <span style={{ fontSize: '11px', color: 'var(--primary-600)', fontWeight: 400 }}>Click Presets to auto-fill</span>
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
                                                    <span className="cron-preset-label">{p.label}</span>
                                                    <span className="cron-preset-val">{p.value}</span>
                                                </div>
                                            ))}
                                        </div>
                                    </div>

                                    <div className="cron-guide-box">
                                        <div style={{ fontWeight: 700, fontSize: '12px', color: '#475569', marginBottom: '8px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                                            <i className="fas fa-info-circle" style={{ color: 'var(--primary-500)' }} /> Cron 표현식 가이드
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', textAlign: 'center', fontSize: '10px', color: '#64748b', background: 'white', padding: '8px', borderRadius: '6px', border: '1px solid #e2e8f0' }}>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>min(0-59)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>hr(0-23)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>day(1-31)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>month(1-12)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>요일(0-6)</div>
                                        </div>
                                        <div style={{ marginTop: '10px', fontSize: '11px', color: '#64748b' }}>
                                            <div style={{ marginBottom: '4px' }}>• <code>*</code>: 모든 값을 의미</div>
                                            <div style={{ marginBottom: '4px' }}>• <code>*/N</code>: N 마다 반복</div>
                                            <div>• <code>0-5</code>: 범위 설정</div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer">
                                <button type="button" className="mgmt-btn mgmt-btn-outline" onClick={handleCloseModal}>취소</button>
                                <button type="submit" className="mgmt-btn mgmt-btn-primary" style={{ padding: '0 32px' }}>스케줄 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default ScheduleManagementTab;
