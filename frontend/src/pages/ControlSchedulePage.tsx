// ============================================================================
// frontend/src/pages/ControlSchedulePage.tsx
// Schedule control management page (with direct register feature)
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { ControlScheduleApi, ControlSchedule, CreateSchedulePayload } from '../api/services/controlScheduleApi';
import { DataApiService } from '../api/services/dataApi';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';

// ── Utils ─────────────────────────────────────────────────────────────────────
function describeCron(expr: string): string {
    const presets: Record<string, string> = {
        '* * * * *': 'Every minute', '0 * * * *': 'Every hour',
        '0 9 * * *': 'Daily 09:00', '0 18 * * *': 'Daily 18:00',
        '0 9 * * 1': 'Weekly Mon 09:00', '0 9 * * 1-5': 'Weekdays 09:00',
    };
    return presets[expr] || `Repeat (${expr})`;
}

function formatDatetime(iso?: string): string {
    if (!iso) return '-';
    try { return new Date(iso).toLocaleString('ko-KR'); } catch { return iso; }
}

function StatusBadge({ enabled }: { enabled: boolean }) {
    return (
        <span style={{
            padding: '2px 8px', borderRadius: '10px', fontSize: '11px', fontWeight: 700,
            background: enabled ? '#dcfce7' : '#f3f4f6', color: enabled ? '#16a34a' : '#6b7280',
        }}>
            {enabled ? 'Active' : 'Inactive'}
        </span>
    );
}

const CRON_PRESETS = [
    { label: 'Every minute', value: '* * * * *' },
    { label: 'Every hour', value: '0 * * * *' },
    { label: 'Daily 09:00', value: '0 9 * * *' },
    { label: 'Daily 18:00', value: '0 18 * * *' },
    { label: 'Weekdays 09:00', value: '0 9 * * 1-5' },
];

// ── Register Modal ─────────────────────────────────────────────────────────────────
interface AddModalState {
    step: 'point' | 'config';
    // step1: Point Search
    search: string;
    points: any[];
    pointsLoading: boolean;
    selectedPoint: any | null;
    // step2: control settings
    value: string;
    scheduleType: 'once' | 'repeat';
    scheduleAt: string;
    cronExpr: string;
    description: string;
    // Save
    saving: boolean;
    saveMsg: string;
    saveFailed: boolean;
}

const defaultModal = (): AddModalState => ({
    step: 'point', search: '', points: [], pointsLoading: false, selectedPoint: null,
    value: '', scheduleType: 'once', scheduleAt: '', cronExpr: '0 9 * * *',
    description: '', saving: false, saveMsg: '', saveFailed: false,
});

interface AddModalProps {
    onClose: () => void;
    onSaved: () => void;
}

function AddScheduleModal({ onClose, onSaved }: AddModalProps) {
    const { t: tc } = useTranslation('control');
    const [s, setS] = useState<AddModalState>(defaultModal());
    const upd = (patch: Partial<AddModalState>) => setS(prev => ({ ...prev, ...patch }));

    // Point Search
    const searchPoints = useCallback(async (q: string) => {
        upd({ pointsLoading: true });
        try {
            const res = await DataApiService.searchDataPoints({ search: q, limit: 30, enabled_only: true, page: 1 });
            upd({ points: res.data?.items || [], pointsLoading: false });
        } catch { upd({ points: [], pointsLoading: false }); }
    }, []);

    useEffect(() => {
        searchPoints('');
    }, [searchPoints]);

    // Save
    const handleSave = async () => {
        if (!s.selectedPoint || !s.value.trim()) return;
        upd({ saving: true, saveMsg: '', saveFailed: false });
        const payload: CreateSchedulePayload = {
            point_id: s.selectedPoint.id,
            device_id: s.selectedPoint.device_id,
            point_name: s.selectedPoint.name,
            device_name: s.selectedPoint.device_name || '',
            value: s.value,
            description: s.description,
            ...(s.scheduleType === 'once'
                ? { once_at: s.scheduleAt ? new Date(s.scheduleAt).toISOString() : undefined }
                : { cron_expr: s.cronExpr }),
        };
        try {
            const res = await ControlScheduleApi.create(payload);
            if (res.success) {
                upd({ saveMsg: 'Register Complete!', saving: false });
                setTimeout(() => { onSaved(); onClose(); }, 1000);
            } else {
                upd({ saveMsg: res.message || 'Register Failed', saving: false, saveFailed: true });
            }
        } catch (e: any) {
            upd({ saveMsg: e.message || 'Error occurred', saving: false, saveFailed: true });
        }
    };

    return (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.45)', zIndex: 9999, display: 'flex', alignItems: 'center', justifyContent: 'center' }}
            onClick={onClose}>
            <div style={{ background: 'white', borderRadius: '14px', width: '420px', maxHeight: '90vh', overflow: 'auto', boxShadow: '0 24px 64px rgba(0,0,0,0.25)' }}
                onClick={e => e.stopPropagation()}>

                {/* Header */}
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '20px 24px', borderBottom: '1px solid #e5e7eb' }}>
                    <div style={{ fontWeight: 700, fontSize: '16px', color: '#111' }}>
                        <i className="fas fa-plus-circle" style={{ color: '#7c3aed', marginRight: '8px' }} />새 예약 제어 등록
                    </div>
                    <button onClick={onClose} style={{ border: 'none', background: 'none', fontSize: '18px', cursor: 'pointer', color: '#9ca3af' }}>✕</button>
                </div>

                <div style={{ padding: '24px' }}>
                    {/* Step Tabs */}
                    <div style={{ display: 'flex', gap: '0', marginBottom: '20px', background: '#f3f4f6', borderRadius: '8px', padding: '3px' }}>
                        {(['point', 'config'] as const).map((step, i) => (
                            <button key={step}
                                onClick={() => s.selectedPoint && upd({ step })}
                                style={{
                                    flex: 1, padding: '7px', borderRadius: '6px', border: 'none', cursor: s.selectedPoint ? 'pointer' : 'default', fontWeight: 600, fontSize: '12px',
                                    background: s.step === step ? 'white' : 'transparent',
                                    color: s.step === step ? '#111' : '#6b7280',
                                    boxShadow: s.step === step ? '0 1px 3px rgba(0,0,0,0.1)' : 'none'
                                }}>
                                {i + 1}. {step === 'point' ? tc('modal.stepPoint') : tc('modal.stepControl')}
                            </button>
                        ))}
                    </div>

                    {/* STEP 1: Select Points */}
                    {s.step === 'point' && (
                        <div>
                            <input
                                type="text" autoFocus
                                placeholder={tc('modal.searchPlaceholder')}
                                value={s.search}
                                onChange={e => { upd({ search: e.target.value }); searchPoints(e.target.value); }}
                                style={{ width: '100%', padding: '9px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '13px', boxSizing: 'border-box', marginBottom: '12px' }}
                            />
                            <div style={{ maxHeight: '280px', overflowY: 'auto', border: '1px solid #e5e7eb', borderRadius: '8px' }}>
                                {s.pointsLoading ? (
                                    <div style={{ padding: '24px', textAlign: 'center', color: '#9ca3af' }}>
                                        <i className="fas fa-spinner fa-spin" style={{ marginRight: '8px' }} />{tc('modal.searching')}
                                    </div>
                                ) : s.points.length === 0 ? (
                                    <div style={{ padding: '24px', textAlign: 'center', color: '#9ca3af', fontSize: '13px' }}>{tc('modal.noResults')}</div>
                                ) : s.points.map((p: any) => (
                                    <div key={p.id} onClick={() => upd({ selectedPoint: p, step: 'config', value: '' })}
                                        style={{
                                            padding: '10px 14px', borderBottom: '1px solid #f3f4f6', cursor: 'pointer', display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                                            background: s.selectedPoint?.id === p.id ? '#eff6ff' : 'white'
                                        }}>
                                        <div>
                                            <div style={{ fontWeight: 600, fontSize: '13px', color: '#111' }}>{p.name}</div>
                                            <div style={{ fontSize: '11px', color: '#6b7280', marginTop: '2px' }}>{p.device_name} · {p.data_type}</div>
                                        </div>
                                        {s.selectedPoint?.id === p.id && <i className="fas fa-check-circle" style={{ color: '#2563eb' }} />}
                                    </div>
                                ))}
                            </div>
                            {s.selectedPoint && (
                                <button onClick={() => upd({ step: 'config' })}
                                    style={{ marginTop: '14px', width: '100%', padding: '10px', background: '#2563eb', color: 'white', border: 'none', borderRadius: '8px', fontWeight: 700, fontSize: '14px', cursor: 'pointer' }}>
                                    {tc('modal.nextStep')}
                                </button>
                            )}
                        </div>
                    )}

                    {/* [translated comment] */}
                    {s.step === 'config' && s.selectedPoint && (
                        <div>
                            {/* [translated comment] */}
                            <div style={{ background: '#eff6ff', borderRadius: '8px', padding: '10px 14px', marginBottom: '18px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                <div>
                                    <div style={{ fontWeight: 700, fontSize: '13px', color: '#1d4ed8' }}>{s.selectedPoint.name}</div>
                                    <div style={{ fontSize: '11px', color: '#6b7280' }}>{s.selectedPoint.device_name}</div>
                                </div>
                                <button onClick={() => upd({ step: 'point' })} style={{ border: 'none', background: 'none', color: '#2563eb', fontSize: '11px', cursor: 'pointer', fontWeight: 600 }}>{tc('modal.change')}</button>
                            </div>

                            {/* [translated comment] */}
                            <div style={{ marginBottom: '14px' }}>
                                <div style={{ fontSize: '12px', fontWeight: 600, color: '#374151', marginBottom: '6px' }}>{tc('modal.sendValue')}</div>
                                <input type="text" value={s.value} onChange={e => upd({ value: e.target.value })}
                                    placeholder={tc('modal.sendValuePlaceholder')}
                                    style={{ width: '100%', padding: '9px 12px', border: '1px solid #d1d5db', borderRadius: '6px', fontSize: '15px', fontFamily: 'monospace', fontWeight: 700, textAlign: 'right', boxSizing: 'border-box' }} />
                            </div>

                            {/* [translated comment] */}
                            <div style={{ display: 'flex', gap: '8px', marginBottom: '14px' }}>
                                {(['once', 'repeat'] as const).map(schedType => (
                                    <button key={schedType} onClick={() => upd({ scheduleType: schedType })}
                                        style={{
                                            flex: 1, padding: '8px', borderRadius: '6px', fontWeight: 600, fontSize: '12px', cursor: 'pointer',
                                            border: s.scheduleType === schedType ? '2px solid #7c3aed' : '1px solid #d1d5db',
                                            background: s.scheduleType === schedType ? '#f5f3ff' : 'white',
                                            color: s.scheduleType === schedType ? '#7c3aed' : '#374151'
                                        }}>
                                        {schedType === 'once' ? `📅 ${tc('type.once')}` : `🔁 ${tc('type.repeat')}`}
                                    </button>
                                ))}
                            </div>

                            {s.scheduleType === 'once' ? (
                                <div style={{ marginBottom: '14px' }}>
                                    <div style={{ fontSize: '12px', fontWeight: 600, color: '#374151', marginBottom: '6px' }}>{tc('modal.executeAt')}</div>
                                    <input type="datetime-local" value={s.scheduleAt} onChange={e => upd({ scheduleAt: e.target.value })}
                                        style={{ width: '100%', padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', fontSize: '13px', boxSizing: 'border-box' }} />
                                </div>
                            ) : (
                                <div style={{ marginBottom: '14px' }}>
                                    <div style={{ fontSize: '12px', fontWeight: 600, color: '#374151', marginBottom: '6px' }}>{tc('modal.repeatInterval')}</div>
                                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginBottom: '8px' }}>
                                        {CRON_PRESETS.map(p => (
                                            <button key={p.value} onClick={() => upd({ cronExpr: p.value })}
                                                style={{
                                                    padding: '4px 10px', borderRadius: '12px', fontSize: '11px', fontWeight: 600, cursor: 'pointer', border: 'none',
                                                    background: s.cronExpr === p.value ? '#dbeafe' : '#f3f4f6', color: s.cronExpr === p.value ? '#1d4ed8' : '#374151'
                                                }}>
                                                {p.label}
                                            </button>
                                        ))}
                                    </div>
                                    <input type="text" value={s.cronExpr} onChange={e => upd({ cronExpr: e.target.value })}
                                        placeholder={tc('modal.cronPlaceholder')}
                                        style={{ width: '100%', padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', fontSize: '12px', fontFamily: 'monospace', boxSizing: 'border-box' }} />
                                </div>
                            )}

                            {/* [translated comment] */}
                            <div style={{ marginBottom: '18px' }}>
                                <div style={{ fontSize: '12px', fontWeight: 600, color: '#374151', marginBottom: '6px' }}>{tc('modal.notes')}</div>
                                <input type="text" value={s.description} onChange={e => upd({ description: e.target.value })}
                                    placeholder={tc('modal.notesPlaceholder')}
                                    style={{ width: '100%', padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', fontSize: '13px', boxSizing: 'border-box' }} />
                            </div>

                            {/* [translated comment] */}
                            <button onClick={handleSave}
                                disabled={s.saving || !s.value.trim() || (s.scheduleType === 'once' && !s.scheduleAt)}
                                style={{
                                    width: '100%', padding: '12px', fontWeight: 700, fontSize: '14px', borderRadius: '8px', border: 'none', cursor: 'pointer',
                                    background: s.saveMsg && !s.saveFailed ? '#22c55e' : '#7c3aed', color: 'white',
                                    opacity: (s.saving || !s.value.trim() || (s.scheduleType === 'once' && !s.scheduleAt)) ? 0.5 : 1
                                }}>
                                {s.saving ? tc('modal.registering') : s.saveMsg && !s.saveFailed ? `✓ ${s.saveMsg}` : `📅 ${tc('modal.registerBtn')}`}
                            </button>

                            {s.saveMsg && s.saveFailed && (
                                <div style={{ marginTop: '8px', fontSize: '12px', textAlign: 'center', color: '#ef4444' }}>{s.saveMsg}</div>
                            )}
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
}

// [translated comment]
export default function ControlSchedulePage() {
    const { t } = useTranslation('control');
    const [schedules, setSchedules] = useState<ControlSchedule[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState('');
    const [deletingId, setDeletingId] = useState<number | null>(null);
    const [togglingId, setTogglingId] = useState<number | null>(null);
    const [showAdd, setShowAdd] = useState(false);

    const pagination = usePagination({ initialPage: 1, initialPageSize: 20, totalCount: 0 });

    const load = useCallback(async (page = pagination.currentPage, pageSize = pagination.pageSize) => {
        setLoading(true); setError('');
        try {
            const res = await ControlScheduleApi.list(page, pageSize);
            if (res.success) {
                setSchedules(res.data || []);
                if (res.pagination) {
                    pagination.updateTotalCount(res.pagination.totalCount);
                }
            } else { setError(res.message || 'Load Failed'); }
        } catch (e: any) { setError(e.message || 'Error occurred'); }
        finally { setLoading(false); }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [pagination.currentPage, pagination.pageSize]);

    useEffect(() => { load(); }, [load]);

    const handleToggle = async (id: number, current: boolean) => {
        setTogglingId(id);
        try {
            const res = await ControlScheduleApi.toggle(id, !current);
            if (res.success) setSchedules(prev => prev.map(s => s.id === id ? { ...s, enabled: !current } : s));
        } finally { setTogglingId(null); }
    };

    const handleDelete = async (id: number) => {
        if (!window.confirm(t('confirm.deleteMsg'))) return;
        setDeletingId(id);
        try {
            const res = await ControlScheduleApi.remove(id);
            if (res.success) setSchedules(prev => prev.filter(s => s.id !== id));
        } finally { setDeletingId(null); }
    };

    return (
        <div style={{ padding: '24px', maxWidth: '1200px' }}>
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '24px' }}>
                <div>
                    <h1 style={{ fontSize: '22px', fontWeight: 700, color: '#111827', margin: 0 }}>
                        <i className="fas fa-calendar-alt" style={{ color: '#7c3aed', marginRight: '10px' }} />{t('title')}
                    </h1>
                    <p style={{ fontSize: '13px', color: '#6b7280', marginTop: '4px' }}>{t('description')}</p>
                </div>
                <div style={{ display: 'flex', gap: '8px' }}>
                    <button onClick={() => load()} style={{ padding: '8px 14px', background: '#f3f4f6', border: '1px solid #e5e7eb', borderRadius: '6px', cursor: 'pointer', fontSize: '13px', color: '#374151' }}>
                        <i className="fas fa-sync-alt" style={{ marginRight: '6px' }} />{t('refresh')}
                    </button>
                    <button onClick={() => setShowAdd(true)}
                        style={{ padding: '8px 18px', background: '#7c3aed', color: 'white', border: 'none', borderRadius: '6px', cursor: 'pointer', fontSize: '13px', fontWeight: 700 }}>
                        <i className="fas fa-plus" style={{ marginRight: '6px' }} />{t('newSchedule')}
                    </button>
                </div>
            </div>

            {error && (
                <div style={{ background: '#fef2f2', border: '1px solid #fecaca', borderRadius: '8px', padding: '12px 16px', marginBottom: '16px', color: '#dc2626', fontSize: '13px' }}>
                    <i className="fas fa-exclamation-circle" style={{ marginRight: '8px' }} />{error}
                </div>
            )}

            {loading ? (
                <div style={{ textAlign: 'center', padding: '60px', color: '#9ca3af' }}>
                    <i className="fas fa-spinner fa-spin" style={{ fontSize: '24px', display: 'block', marginBottom: '12px' }} />{t('loading')}
                </div>
            ) : schedules.length === 0 ? (
                <div style={{ textAlign: 'center', padding: '80px', background: '#f9fafb', borderRadius: '12px', border: '1px dashed #e5e7eb' }}>
                    <i className="fas fa-calendar-times" style={{ fontSize: '48px', color: '#d1d5db', display: 'block', marginBottom: '16px' }} />
                    <div style={{ fontSize: '16px', fontWeight: 600, color: '#374151', marginBottom: '8px' }}>{t('empty.title')}</div>
                    <div style={{ fontSize: '13px', color: '#9ca3af', marginBottom: '20px' }}>
                        Click the <strong>+ New Schedule</strong> button above, or register from the Real-Time Monitor via the Control → Schedule tab.
                    </div>
                    <button onClick={() => setShowAdd(true)}
                        style={{ padding: '10px 24px', background: '#7c3aed', color: 'white', border: 'none', borderRadius: '8px', fontWeight: 700, fontSize: '14px', cursor: 'pointer' }}>
                        <i className="fas fa-plus" style={{ marginRight: '8px' }} />Register First Schedule
                    </button>
                </div>
            ) : (
                <div style={{ background: 'white', borderRadius: '12px', border: '1px solid #e5e7eb', overflow: 'hidden' }}>
                    <div style={{ padding: '12px 20px', background: '#f8fafc', borderBottom: '1px solid #e5e7eb', display: 'flex', gap: '16px', fontSize: '12px', color: '#6b7280' }}>
                        <span>{t('summary.total', { count: schedules.length })}</span>
                        <span style={{ color: '#16a34a' }}>{t('summary.active', { count: schedules.filter(s => s.enabled).length })}</span>
                        <span>{t('summary.inactive', { count: schedules.filter(s => !s.enabled).length })}</span>
                    </div>
                    <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                        <thead>
                            <tr style={{ background: '#f8fafc' }}>
                                {['ID', t('table.point'), t('table.device'), t('table.value'), t('table.type'), t('table.schedule'), t('table.lastRun'), t('table.status'), t('table.action')].map(h => (
                                    <th key={h} style={{ padding: '10px 14px', textAlign: 'left', fontSize: '11px', fontWeight: 700, color: '#6b7280', textTransform: 'uppercase', borderBottom: '1px solid #e5e7eb', whiteSpace: 'nowrap' }}>{h}</th>
                                ))}
                            </tr>
                        </thead>
                        <tbody>
                            {schedules.map((s, idx) => (
                                <tr key={s.id} style={{ background: idx % 2 === 0 ? 'white' : '#fafafa', borderBottom: '1px solid #f3f4f6' }}>
                                    <td style={{ padding: '12px 14px', fontSize: '13px', fontWeight: 600, color: '#7c3aed' }}>#{s.id}</td>
                                    <td style={{ padding: '12px 14px', fontSize: '13px', fontWeight: 600 }}>{s.point_name || `Point #${s.point_id}`}</td>
                                    <td style={{ padding: '12px 14px', fontSize: '13px', color: '#374151' }}>{s.device_name || `Device #${s.device_id}`}</td>
                                    <td style={{ padding: '12px 14px', fontSize: '15px', fontWeight: 700, fontFamily: 'monospace', color: '#1d4ed8' }}>{s.value}</td>
                                    <td style={{ padding: '12px 14px' }}>
                                        {s.cron_expr
                                            ? <span style={{ background: '#eff6ff', color: '#1d4ed8', padding: '2px 8px', borderRadius: '8px', fontSize: '11px', fontWeight: 600 }}>Repeat</span>
                                            : <span style={{ background: '#fef3c7', color: '#92400e', padding: '2px 8px', borderRadius: '8px', fontSize: '11px', fontWeight: 600 }}>One-time</span>}
                                    </td>
                                    <td style={{ padding: '12px 14px', fontSize: '12px', color: '#374151' }}>
                                        {s.cron_expr ? describeCron(s.cron_expr) : formatDatetime(s.once_at)}
                                    </td>
                                    <td style={{ padding: '12px 14px', fontSize: '12px', color: '#9ca3af' }}>{formatDatetime(s.last_run)}</td>
                                    <td style={{ padding: '12px 14px' }}><StatusBadge enabled={s.enabled} /></td>
                                    <td style={{ padding: '12px 14px' }}>
                                        <div style={{ display: 'flex', gap: '6px' }}>
                                            <button onClick={() => handleToggle(s.id, s.enabled)} disabled={togglingId === s.id}
                                                style={{
                                                    padding: '5px 10px', fontSize: '11px', fontWeight: 600, borderRadius: '5px', cursor: 'pointer', border: 'none',
                                                    background: s.enabled ? '#fef3c7' : '#dcfce7', color: s.enabled ? '#92400e' : '#16a34a'
                                                }}>
                                                {togglingId === s.id ? <i className="fas fa-spinner fa-spin" /> : s.enabled ? t('action.pause') : t('action.activate')}
                                            </button>
                                            <button onClick={() => handleDelete(s.id)} disabled={deletingId === s.id}
                                                style={{ padding: '5px 10px', fontSize: '11px', fontWeight: 600, borderRadius: '5px', cursor: 'pointer', border: 'none', background: '#fef2f2', color: '#dc2626' }}>
                                                {deletingId === s.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-trash" />}
                                            </button>
                                        </div>
                                    </td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                    {/* [translated comment] */}
                    <div style={{ padding: '14px 20px', borderTop: '1px solid #e5e7eb', background: 'white' }}>
                        <Pagination
                            current={pagination.currentPage}
                            total={pagination.totalCount}
                            pageSize={pagination.pageSize}
                            onChange={(page, size) => {
                                pagination.goToPage(page);
                                if (size && size !== pagination.pageSize) pagination.changePageSize(size);
                            }}
                            showSizeChanger={true}
                        />
                    </div>
                </div>
            )}

            {/* [translated comment] */}
            {showAdd && (
                <AddScheduleModal
                    onClose={() => setShowAdd(false)}
                    onSaved={() => { load(); }}
                />
            )}
        </div>
    );
}
