import React, { useState } from 'react';
import { useTranslation } from 'react-i18next';
import { Modal, Button, Tag, Pagination, Space } from 'antd';
import exportGatewayApi, { Gateway, Assignment, ExportTarget, ExportProfile, ExportSchedule } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

// Utility to handle potentially wrapped responses
const extractItems = (response: any): any[] => {
    if (Array.isArray(response)) return response;
    return response?.items || [];
};

interface GatewayListTabProps {
    siteId?: number | null;
    gateways: Gateway[];
    loading: boolean;
    onRefresh: () => Promise<void>;
    onEdit: (gateway: Gateway) => void;
    onDelete: (gateway: Gateway) => Promise<void>;
    pagination: any;
    onPageChange: (page: number) => void;
    assignments: Record<number, Assignment[]>;
    targets: ExportTarget[];
    profiles: ExportProfile[];
    schedules: ExportSchedule[];
}

const GatewayListTab: React.FC<GatewayListTabProps> = ({
    siteId,
    gateways,
    loading,
    onRefresh,
    onEdit,
    onDelete,
    pagination,
    onPageChange,
    assignments,
    targets,
    profiles,
    schedules
}) => {
    const [actionLoading, setActionLoading] = useState<number | null>(null);
    const { t } = useTranslation(['dataExport', 'common']);
    const [selectedGateway, setSelectedGateway] = useState<Gateway | null>(null);
    const [isDetailModalOpen, setIsDetailModalOpen] = useState(false);

    const { confirm } = useConfirmContext();

    const handleAction = async (gwId: number, action: (gw: Gateway) => Promise<void>) => {
        setActionLoading(gwId);
        try {
            const gw = gateways.find(g => g.id === gwId);
            if (gw) await action(gw);
        } finally {
            setActionLoading(null);
        }
    };

    const handleDelete = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: t('gwTab.deleteConfirmTitle', { ns: 'dataExport' }),
            message: `${t('gwTab.deleteConfirmMsg', { ns: 'dataExport' })} ("${gw.name}")`,
            confirmText: t('delete', { ns: 'common' }),
            confirmButtonType: 'danger'
        });
        if (!confirmed) return;

        try {
            await onDelete(gw);
            onRefresh();
        } catch (e: any) {
            await confirm({
                title: t('gwTab.deleteFailed', { ns: 'dataExport' }),
                message: e.message || t('gwTab.deleteFailedMsg', { ns: 'dataExport' }),
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onStart = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: t('gwTab.confirmStartTitle', { ns: 'dataExport' }),
            message: t('gwTab.confirmStartMsg', { ns: 'dataExport', name: gw.name }),
            confirmText: t('gwTab.btnStart', { ns: 'dataExport' }),
            confirmButtonType: 'primary'
        });
        if (!confirmed) return;

        try {
            const res = await exportGatewayApi.startGatewayProcess(gw.id, siteId);
            if (res.success) {
                await confirm({
                    title: t('gwTab.startComplete', { ns: 'dataExport' }),
                    message: res.message || '',
                    showCancelButton: false,
                    confirmText: t('ok', { ns: 'common' })
                });
            } else {
                await confirm({
                    title: t('gwTab.startFailed', { ns: 'dataExport' }),
                    message: res.message || '',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
            onRefresh();
        } catch (e) {
            await confirm({
                title: t('gwTab.startFailed', { ns: 'dataExport' }),
                message: '',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onStop = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: t('gwTab.confirmStopTitle', { ns: 'dataExport' }),
            message: t('gwTab.confirmStopMsg', { ns: 'dataExport', name: gw.name }),
            confirmText: t('gwTab.btnStop', { ns: 'dataExport' }),
            confirmButtonType: 'danger'
        });
        if (!confirmed) return;

        try {
            const res = await exportGatewayApi.stopGatewayProcess(gw.id, siteId);
            if (res.success) {
                await confirm({
                    title: t('gwTab.stopComplete', { ns: 'dataExport' }),
                    message: res.message || '',
                    showCancelButton: false,
                    confirmText: t('ok', { ns: 'common' })
                });
            } else {
                await confirm({
                    title: t('gwTab.stopFailed', { ns: 'dataExport' }),
                    message: res.message || '',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
            onRefresh();
        } catch (e) {
            await confirm({
                title: t('gwTab.stopFailed', { ns: 'dataExport' }),
                message: '',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onRestart = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: t('gwTab.confirmRestartTitle', { ns: 'dataExport' }),
            message: t('gwTab.confirmRestartMsg', { ns: 'dataExport', name: gw.name }),
            confirmText: t('gwTab.btnRestart', { ns: 'dataExport' }),
            confirmButtonType: 'warning'
        });
        if (!confirmed) return;

        try {
            const res = await exportGatewayApi.restartGatewayProcess(gw.id, siteId);
            if (res.success) {
                await confirm({
                    title: t('gwTab.restartComplete', { ns: 'dataExport' }),
                    message: res.message || '',
                    showCancelButton: false,
                    confirmText: t('ok', { ns: 'common' })
                });
            } else {
                await confirm({
                    title: t('gwTab.restartFailed', { ns: 'dataExport' }),
                    message: res.message || '',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
            onRefresh();
        } catch (e) {
            await confirm({
                title: t('gwTab.restartFailed', { ns: 'dataExport' }),
                message: '',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onDeploy = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: t('gwTab.deployConfirmTitle', { ns: 'dataExport' }),
            message: t('gwTab.deployConfirmMsg', { ns: 'dataExport', name: gw.name }),
            confirmText: t('gwTab.deployBtn', { ns: 'dataExport' }),
            confirmButtonType: 'primary'
        });
        if (!confirmed) return;

        setActionLoading(gw.id);
        try {
            const res = await exportGatewayApi.deployConfig(gw.id, siteId);
            if (res.success) {
                await confirm({
                    title: t('gwTab.deploySuccess', { ns: 'dataExport' }),
                    message: t('gwTab.deploySuccessMsg', { ns: 'dataExport' }),
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                onRefresh();
            } else {
                await confirm({
                    title: t('gwTab.deployFailed', { ns: 'dataExport' }),
                    message: res.message || t('gwTab.deployFailedMsg', { ns: 'dataExport' }),
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (e) {
            await confirm({
                title: t('gwTab.apiError', { ns: 'dataExport' }),
                message: t('gwTab.apiErrorMsg', { ns: 'dataExport' }),
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        } finally {
            setActionLoading(null);
        }
    };

    const displayGateways = gateways;

    return (
        <div style={{ flex: 1, overflow: 'auto' }}>
            <div className="gateway-list">
                <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                    <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>{t('labels.registeredGateways', { ns: 'dataExport' })}</h3>
                    <button className="btn btn-outline btn-sm" onClick={() => onRefresh()}>
                        <i className="fas fa-sync-alt" /> {t('gwTab.refresh', { ns: 'dataExport' })}
                    </button>
                </div>
                {gateways.length === 0 ? (
                    <div className="empty-state" style={{ padding: '60px 0', textAlign: 'center', color: 'var(--neutral-400)' }}>
                        <i className="fas fa-server fa-3x" style={{ marginBottom: '16px', opacity: 0.3 }} />
                        <p>{t('labels.noRegisteredGateways', { ns: 'dataExport' })}</p>
                    </div>
                ) : (
                    <div className="mgmt-grid">
                        {displayGateways.map(gw => (
                            <div key={gw.id} className="mgmt-card gateway-card">
                                <div className="mgmt-card-header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '16px' }}>
                                    <div className="mgmt-card-title" style={{ display: 'flex', flexDirection: 'column', gap: '4px', flex: 1 }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                            <h4 style={{ margin: 0, fontSize: '15px' }}>{gw.server_name || gw.name}</h4>
                                            <div style={{ display: 'flex', gap: '8px' }}>
                                                <div className={`badge ${(gw.live_status?.status === 'online' || gw.live_status?.status === 'running' || gw.status === 'online' || gw.status === 'active') ? 'success' : 'neutral'}`}>
                                                    <i className={`fas fa-circle`} style={{ fontSize: '8px', marginRight: '5px' }} />
                                                    {(gw.live_status?.status === 'online' || gw.live_status?.status === 'running' || gw.status === 'online' || gw.status === 'active') ? t('exportPage.statOnline', { ns: 'dataExport' }) : t('exportPage.statOffline', { ns: 'dataExport' })}
                                                </div>
                                                <button
                                                    onClick={(e) => { e.stopPropagation(); handleDelete(gw); }}
                                                    style={{ background: 'none', border: 'none', color: '#cbd5e1', cursor: 'pointer', padding: '0 4px' }}
                                                    title={t('delete', { ns: 'common' })}
                                                >
                                                    <i className="fas fa-trash-alt hover-danger" />
                                                </button>
                                            </div>
                                        </div>
                                        {gw.description && (
                                            <div style={{ fontSize: '12px', color: 'var(--neutral-500)', fontWeight: 400, marginTop: '2px' }}>
                                                {gw.description}
                                            </div>
                                        )}
                                        <div style={{ display: 'flex', gap: '4px', marginTop: '4px' }}>
                                            <span className="badge neutral-light" style={{ fontSize: '10px', padding: '2px 6px' }}>
                                                <i className="fas fa-satellite-dish" style={{ marginRight: '4px' }} />
                                                {t('gwTab.exportGatewayBadge', { ns: 'dataExport' })}
                                            </span>
                                        </div>
                                    </div>
                                </div>

                                <div className="mgmt-card-body" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
                                    <div className="info-list" style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '13px' }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>{t('labels.ipAddress', { ns: 'dataExport' })}</span>
                                            <span style={{ fontWeight: 500, fontFamily: 'monospace' }}>{gw.ip_address}</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>{t('labels.lastSeen', { ns: 'dataExport' })}</span>
                                            <span>{gw.last_seen ? new Date(gw.last_seen).toLocaleString() : '-'}</span>
                                        </div>
                                        {gw.live_status && (
                                            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                                <span style={{ color: 'var(--neutral-500)' }}>{t('labels.memory', { ns: 'dataExport' })}</span>
                                                <span>{gw.live_status.memory_usage} MB</span>
                                            </div>
                                        )}
                                        <div style={{ marginTop: '8px', padding: '8px', background: 'var(--neutral-50)', borderRadius: '4px' }}>
                                            <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '4px', display: 'flex', justifyContent: 'space-between' }}>
                                                <span>{t('labels.processStatus', { ns: 'dataExport' })}</span>
                                                <span style={{ fontWeight: 600, color: (gw.processes && gw.processes.length > 0) || gw.status === 'active' ? 'var(--success-600)' : 'var(--error-600)' }}>
                                                    {(gw.processes && gw.processes.length > 0) || gw.status === 'active' ? t('gwTab.statusRunning', { ns: 'dataExport' }) : t('gwTab.statusStopped', { ns: 'dataExport' })}
                                                </span>
                                            </div>
                                            {gw.processes && gw.processes.length > 0 && (
                                                <div style={{ fontSize: '11px', fontFamily: 'monospace', color: 'var(--neutral-600)' }}>
                                                    PID: {gw.processes[0].pid} | CPU: {gw.processes[0].cpu}%
                                                </div>
                                            )}
                                            <div style={{ display: 'flex', gap: '4px', marginTop: '8px' }}>
                                                {(gw.processes?.[0]?.status !== 'running' && gw.live_status?.status !== 'online' && gw.status !== 'active') ? (
                                                    <button
                                                        className="btn btn-outline btn-xs"
                                                        style={{ flex: 1 }}
                                                        onClick={() => handleAction(gw.id, onStart)}
                                                        disabled={actionLoading === gw.id}
                                                    >
                                                        {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-play" style={{ fontSize: '10px' }} />} {t('gwTab.btnStart', { ns: 'dataExport' })}
                                                    </button>
                                                ) : (
                                                    <>
                                                        <button
                                                            className="btn btn-outline btn-xs btn-danger"
                                                            style={{ flex: 1 }}
                                                            onClick={() => handleAction(gw.id, onStop)}
                                                            disabled={actionLoading === gw.id}
                                                        >
                                                            {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-stop" style={{ fontSize: '10px' }} />} {t('gwTab.btnStop', { ns: 'dataExport' })}
                                                        </button>
                                                        <button
                                                            className="btn btn-outline btn-xs"
                                                            style={{ flex: 1 }}
                                                            onClick={() => handleAction(gw.id, onRestart)}
                                                            disabled={actionLoading === gw.id}
                                                        >
                                                            {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-redo" style={{ fontSize: '10px' }} />} {t('gwTab.btnRestart', { ns: 'dataExport' })}
                                                        </button>
                                                    </>
                                                )}
                                            </div>
                                        </div>
                                    </div>

                                    <div className="assigned-profiles" style={{ marginTop: '16px', paddingTop: '16px', borderTop: '1px solid var(--neutral-100)', flex: 1 }}>
                                        <div style={{ fontSize: '11px', fontWeight: 600, color: 'var(--neutral-400)', textTransform: 'uppercase', marginBottom: '8px' }}>{t('labels.includedProfiles', { ns: 'dataExport' })}</div>
                                        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                                            {(() => {
                                                const myAssignments = (assignments[gw.id] || []).sort((a, b) => (b.id || 0) - (a.id || 0));
                                                if (myAssignments.length === 0) {
                                                    return <span style={{ fontSize: '12px', color: 'var(--neutral-400)', fontStyle: 'italic' }}>{t('labels.noAssignedProfiles', { ns: 'dataExport' })}</span>;
                                                }
                                                // Only show latest
                                                return <span key={myAssignments[0].id} className="badge neutral-light" style={{ fontSize: '11px' }}>{myAssignments[0].name}</span>;
                                            })()}
                                        </div>
                                    </div>
                                </div>

                                <div className="mgmt-card-footer" style={{ borderTop: '1px solid var(--neutral-100)', paddingTop: '12px', marginTop: 'auto', display: 'flex', gap: '8px' }}>
                                    <button className="btn btn-outline btn-sm" onClick={() => { setSelectedGateway(gw); setIsDetailModalOpen(true); }} style={{ flex: 1 }}>
                                        <i className="fas fa-search-plus" /> {t('labels.details', { ns: 'dataExport' }) || '상세'}
                                    </button>
                                    <button className="btn btn-primary btn-sm" onClick={() => onDeploy(gw)} style={{ flex: 1 }} disabled={gw.live_status?.status !== 'online' && gw.live_status?.status !== 'running' && gw.status !== 'active'}>
                                        <i className="fas fa-rocket" /> {t('labels.deploy', { ns: 'dataExport' }) || '배포'}
                                    </button>
                                </div>
                            </div>
                        ))}
                    </div>
                )}
            </div>

            <Pagination
                current={pagination.current_page || 1}
                total={pagination.total_items || 0}
                pageSize={pagination.limit || 10}
                onChange={onPageChange}
                style={{ marginTop: '20px', textAlign: 'center' }}
            />

            <GatewayDetailModal
                visible={isDetailModalOpen}
                onClose={() => setIsDetailModalOpen(false)}
                siteId={siteId}
                gateway={selectedGateway}
                allAssignments={assignments}
                targets={targets}
                allProfiles={profiles}
                schedules={schedules}
                onEdit={(gw) => {
                    setIsDetailModalOpen(false);
                    onEdit(gw);
                }}
                onDelete={handleDelete}
            />
        </div>
    );
};

const GatewayDetailModal: React.FC<{
    visible: boolean;
    onClose: () => void;
    siteId?: number | null;
    gateway: Gateway | null;
    allAssignments: Record<number, Assignment[]>;
    targets: ExportTarget[];
    allProfiles: ExportProfile[];
    schedules: ExportSchedule[];
    onEdit: (gateway: Gateway) => void;
    onDelete: (gateway: Gateway) => void;
}> = ({ visible, onClose, siteId, gateway, allAssignments, targets, allProfiles, schedules, onEdit, onDelete }) => {
    if (!gateway) return null;
    const { t } = useTranslation(['dataExport', 'common']);

    // Derived info with fallback for names (PICK LATEST ONLY)
    const assignments = gateway ? (allAssignments[gateway.id] || []) : [];
    const sortedAssignments = [...assignments].sort((a, b) => (b.id || 0) - (a.id || 0));
    const latestAssignment = sortedAssignments[0];

    const assignedProfiles = latestAssignment ? (() => {
        const found = allProfiles.find(p => String(p.id) === String(latestAssignment.profile_id));
        if (found) return [found];
        if (latestAssignment.name) return [{ id: latestAssignment.profile_id, name: latestAssignment.name, data_points: [] } as any];
        return [];
    })() : [];

    const savedTargetIds = Object.keys(gateway.config?.target_priorities || {});
    const linkedTargets = (targets || []).filter(t => savedTargetIds.includes(String(t.id)));
    const targetIds = linkedTargets.map(t => String(t.id));
    const linkedSchedules = (schedules || []).filter(s => targetIds.includes(String(s.target_id)));

    return (
        <Modal
            open={visible}
            onCancel={onClose}
            title={null}
            footer={
                <div style={{ display: 'flex', justifyContent: 'flex-end', width: '100%', gap: '12px' }}>
                    <button
                        className="btn btn-outline"
                        onClick={() => { onClose(); onEdit(gateway); }}
                        style={{ border: '1px solid #e2e8f0' }}
                    >
                        <i className="fas fa-magic" style={{ marginRight: '6px', color: 'var(--primary-600)' }} /> {t('labels.editSettings', { ns: 'dataExport' }) || '설정 편집'}
                    </button>
                    <button className="btn btn-primary" onClick={onClose} style={{ minWidth: '80px' }}>
                        {t('close', { ns: 'common' }) || '닫기'}
                    </button>
                </div>
            }
            width={800}
            centered
            getContainer={() => document.body}
            bodyStyle={{ padding: '0px', background: '#fcfcfd', borderRadius: '12px', overflow: 'hidden' }}
        >
            <div style={{ display: 'flex', flexDirection: 'column' }}>
                {/* Unified Header */}
                <div style={{ padding: '32px 40px', background: '#1e293b', color: 'white' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '24px' }}>
                        <div style={{ width: '60px', height: '60px', background: 'rgba(255,255,255,0.1)', borderRadius: '14px', display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0 }}>
                            <i className="fas fa-server" style={{ fontSize: '26px', color: '#60a5fa' }} />
                        </div>
                        <div style={{ flex: 1, minWidth: 0 }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '6px' }}>
                                <h2 style={{ margin: 0, fontSize: '22px', fontWeight: 800, color: 'white' }}>{gateway.server_name || gateway.name}</h2>
                                <Tag color={(gateway.live_status?.status === 'online' || gateway.live_status?.status === 'running' || gateway.status === 'online' || gateway.status === 'active') ? 'success' : 'default'} style={{ border: 'none', borderRadius: '4px', fontSize: '11px', height: '22px', lineHeight: '22px', fontWeight: 700, flexShrink: 0 }}>
                                    {(gateway.live_status?.status === 'online' || gateway.live_status?.status === 'running' || gateway.status === 'online' || gateway.status === 'active') ? t('exportPage.statOnline', { ns: 'dataExport' }) : t('exportPage.statOffline', { ns: 'dataExport' })}
                                </Tag>
                            </div>
                            <div style={{ fontSize: '14px', color: '#94a3b8', display: 'flex', alignItems: 'center', gap: '16px' }}>
                                <span><i className="fas fa-network-wired" style={{ marginRight: '8px' }} /> {gateway.ip_address}</span>
                                <span style={{ opacity: 0.5 }}>|</span>
                                <span>ID: {gateway.id}</span>
                            </div>
                            {gateway.description && (
                                <div style={{ marginTop: '12px', fontSize: '13px', color: '#cbd5e1', fontStyle: 'italic', background: 'rgba(255,255,255,0.05)', padding: '8px 12px', borderRadius: '6px', borderLeft: '3px solid #60a5fa' }}>
                                    {gateway.description}
                                </div>
                            )}
                        </div>
                    </div>
                </div>

                {/* Unified Content Body */}
                <div style={{ padding: '32px 40px' }}>
                    {/* Summary Board */}
                    <div style={{ background: 'white', padding: '24px', borderRadius: '16px', border: '1px solid #e2e8f0', boxShadow: '0 1px 3px rgba(0,0,0,0.02)', marginBottom: '32px' }}>
                        <div style={{ fontSize: '13px', fontWeight: 800, color: '#475569', marginBottom: '20px', display: 'flex', alignItems: 'center', gap: '10px', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                            <i className="fas fa-project-diagram" style={{ color: 'var(--primary-500)' }} /> {t('labels.pipelineSummaryTitle', { ns: 'dataExport' })}
                        </div>
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr auto 1fr auto 1fr', alignItems: 'center', gap: '20px' }}>
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>{t('labels.dataSource', { ns: 'dataExport' })}</div>
                                <div style={{ fontSize: '14px', fontWeight: 800, color: '#1e293b' }}>
                                    {assignedProfiles.length > 0 ? (
                                        <>
                                            <div style={{ color: 'var(--primary-600)' }}>{assignedProfiles[0].name}</div>
                                            <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>({assignedProfiles[0].data_points?.length || 0} Points)</div>
                                        </>
                                    ) : t('labels.unassigned', { ns: 'dataExport' })}
                                </div>
                            </div>
                            <i className="fas fa-long-arrow-alt-right" style={{ color: '#cbd5e1', fontSize: '18px' }} />
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>{t('labels.exportTarget', { ns: 'dataExport' })}</div>
                                <div style={{ fontSize: '15px', fontWeight: 800, color: '#1e293b' }}>{linkedTargets.length > 0 ? `${linkedTargets.length} ${t('labels.endpoints', { ns: 'dataExport' })}` : '-'}</div>
                            </div>
                            <i className="fas fa-long-arrow-alt-right" style={{ color: '#cbd5e1', fontSize: '18px' }} />
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>{t('labels.exportMode', { ns: 'dataExport' })}</div>
                                <div style={{ fontSize: '15px', fontWeight: 800, color: '#1e293b' }}>
                                    {linkedSchedules.length > 0 ? t('labels.scheduled', { ns: 'dataExport' }) : t('labels.setupRequired', { ns: 'dataExport' })}
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </Modal>
    );
};

export default GatewayListTab;
