import React, { useState } from 'react';
import { Modal, Button, Tag, Pagination, Space } from 'antd';
import exportGatewayApi, { Gateway, Assignment, ExportTarget, ExportProfile, ExportSchedule } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

// Utility to handle potentially wrapped responses
const extractItems = (response: any): any[] => {
    if (Array.isArray(response)) return response;
    return response?.items || [];
};

interface GatewayListTabProps {
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
            title: '게이트웨이 삭제',
            message: `"${gw.name}" 게이트웨이를 정말 삭제하시겠습니까?\n이 작업은 되돌릴 수 없습니다.`,
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });
        if (!confirmed) return;

        try {
            await onDelete(gw);
            onRefresh();
        } catch (e: any) {
            await confirm({
                title: '삭제 실패',
                message: e.message || '삭제 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onStart = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: '게이트웨이 시작 확인',
            message: `"${gw.name}" 게이트웨이 프로세스를 시작하시겠습니까?`,
            confirmText: '시작',
            confirmButtonType: 'primary'
        });
        if (!confirmed) return;

        try {
            const res = await exportGatewayApi.startGatewayProcess(gw.id);
            if (res.success) {
                await confirm({
                    title: '시작 완료',
                    message: '게이트웨이 프로세스가 성공적으로 시작되었습니다.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
            } else {
                await confirm({
                    title: '시작 실패',
                    message: res.message || '프로세스 시작에 실패했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
            onRefresh();
        } catch (e) {
            await confirm({
                title: '시작 에러',
                message: 'API 호출 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onStop = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: '게이트웨이 중지 확인',
            message: `"${gw.name}" 게이트웨이 프로세스를 중지하시겠습니까?`,
            confirmText: '중지',
            confirmButtonType: 'danger'
        });
        if (!confirmed) return;

        try {
            const res = await exportGatewayApi.stopGatewayProcess(gw.id);
            if (res.success) {
                await confirm({
                    title: '중지 완료',
                    message: '게이트웨이 프로세스를 중지했습니다.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
            } else {
                await confirm({
                    title: '중지 실패',
                    message: res.message || '프로세스 중지에 실패했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
            onRefresh();
        } catch (e) {
            await confirm({
                title: '중지 에러',
                message: 'API 호출 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onRestart = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: '게이트웨이 재시작 확인',
            message: `"${gw.name}" 게이트웨이 프로세스를 재시작하시겠습니까?`,
            confirmText: '재시작',
            confirmButtonType: 'warning'
        });
        if (!confirmed) return;

        try {
            const res = await exportGatewayApi.restartGatewayProcess(gw.id);
            if (res.success) {
                await confirm({
                    title: '재시작 완료',
                    message: '게이트웨이 프로세스를 재시작했습니다.',
                    showCancelButton: false,
                    confirmText: '확인'
                });
            } else {
                await confirm({
                    title: '재시작 실패',
                    message: res.message || '프로세스 재시작에 실패했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
            onRefresh();
        } catch (e) {
            await confirm({
                title: '재시작 에러',
                message: 'API 호출 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onDeploy = async (gw: Gateway) => {
        const confirmed = await confirm({
            title: '배포 확인',
            message: `"${gw.name}" 게이트웨이의 설정을 실제 서버에 배포하시겠습니까?`,
            confirmText: '배포 시작',
            confirmButtonType: 'primary'
        });
        if (!confirmed) return;

        setActionLoading(gw.id);
        try {
            const res = await exportGatewayApi.deployConfig(gw.id);
            if (res.success) {
                await confirm({
                    title: '배포 성공',
                    message: '최신 설정이 게이트웨이에 성공적으로 적용되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                onRefresh();
            } else {
                await confirm({
                    title: '배포 실패',
                    message: res.message || '설정 적용에 실패했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (e) {
            await confirm({
                title: '에러 발생',
                message: 'API 호출 중 오류가 발생했습니다.',
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
                    <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>등록된 게이트웨이</h3>
                    <button className="btn btn-outline btn-sm" onClick={() => onRefresh()}>
                        <i className="fas fa-sync-alt" /> 새로고침
                    </button>
                </div>
                {gateways.length === 0 ? (
                    <div className="empty-state" style={{ padding: '60px 0', textAlign: 'center', color: 'var(--neutral-400)' }}>
                        <i className="fas fa-server fa-3x" style={{ marginBottom: '16px', opacity: 0.3 }} />
                        <p>등록된 게이트웨이가 없습니다.</p>
                    </div>
                ) : (
                    <div className="mgmt-grid">
                        {displayGateways.map(gw => (
                            <div key={gw.id} className="mgmt-card gateway-card">
                                <div className="mgmt-card-header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '16px' }}>
                                    <div className="mgmt-card-title" style={{ display: 'flex', flexDirection: 'column', gap: '4px', flex: 1 }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                            <h4 style={{ margin: 0, fontSize: '15px' }}>{gw.name}</h4>
                                            <div style={{ display: 'flex', gap: '8px' }}>
                                                <div className={`badge ${gw.live_status?.status === 'online' ? 'success' : 'neutral'}`}>
                                                    <i className={`fas fa-circle`} style={{ fontSize: '8px', marginRight: '5px' }} />
                                                    {gw.live_status?.status === 'online' ? 'ONLINE' : 'OFFLINE'}
                                                </div>
                                                <button
                                                    onClick={(e) => { e.stopPropagation(); handleDelete(gw); }}
                                                    style={{ background: 'none', border: 'none', color: '#cbd5e1', cursor: 'pointer', padding: '0 4px' }}
                                                    title="삭제"
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
                                                EXPORT GATEWAY
                                            </span>
                                        </div>
                                    </div>
                                </div>

                                <div className="mgmt-card-body" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
                                    <div className="info-list" style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '13px' }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>IP Address:</span>
                                            <span style={{ fontWeight: 500, fontFamily: 'monospace' }}>{gw.ip_address}</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>Last Seen:</span>
                                            <span>{gw.last_seen ? new Date(gw.last_seen).toLocaleString() : '-'}</span>
                                        </div>
                                        {gw.live_status && (
                                            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                                <span style={{ color: 'var(--neutral-500)' }}>Memory:</span>
                                                <span>{gw.live_status.memory_usage} MB</span>
                                            </div>
                                        )}
                                        <div style={{ marginTop: '8px', padding: '8px', background: 'var(--neutral-50)', borderRadius: '4px' }}>
                                            <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '4px', display: 'flex', justifyContent: 'space-between' }}>
                                                <span>프로세스 상태</span>
                                                <span style={{ fontWeight: 600, color: gw.processes && gw.processes.length > 0 ? 'var(--success-600)' : 'var(--error-600)' }}>
                                                    {gw.processes && gw.processes.length > 0 ? 'RUNNING' : 'STOPPED'}
                                                </span>
                                            </div>
                                            {gw.processes && gw.processes.length > 0 && (
                                                <div style={{ fontSize: '11px', fontFamily: 'monospace', color: 'var(--neutral-600)' }}>
                                                    PID: {gw.processes[0].pid} | CPU: {gw.processes[0].cpu}%
                                                </div>
                                            )}
                                            <div style={{ display: 'flex', gap: '4px', marginTop: '8px' }}>
                                                {(gw.processes?.[0]?.status !== 'running' && gw.live_status?.status !== 'online') ? (
                                                    <button
                                                        className="btn btn-outline btn-xs"
                                                        style={{ flex: 1 }}
                                                        onClick={() => handleAction(gw.id, onStart)}
                                                        disabled={actionLoading === gw.id}
                                                    >
                                                        {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-play" style={{ fontSize: '10px' }} />} 시작
                                                    </button>
                                                ) : (
                                                    <>
                                                        <button
                                                            className="btn btn-outline btn-xs btn-danger"
                                                            style={{ flex: 1 }}
                                                            onClick={() => handleAction(gw.id, onStop)}
                                                            disabled={actionLoading === gw.id}
                                                        >
                                                            {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-stop" style={{ fontSize: '10px' }} />} 중지
                                                        </button>
                                                        <button
                                                            className="btn btn-outline btn-xs"
                                                            style={{ flex: 1 }}
                                                            onClick={() => handleAction(gw.id, onRestart)}
                                                            disabled={actionLoading === gw.id}
                                                        >
                                                            {actionLoading === gw.id ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-redo" style={{ fontSize: '10px' }} />} 재시작
                                                        </button>
                                                    </>
                                                )}
                                            </div>
                                        </div>
                                    </div>

                                    <div className="assigned-profiles" style={{ marginTop: '16px', paddingTop: '16px', borderTop: '1px solid var(--neutral-100)', flex: 1 }}>
                                        <div style={{ fontSize: '11px', fontWeight: 600, color: 'var(--neutral-400)', textTransform: 'uppercase', marginBottom: '8px' }}>포함된 프로파일</div>
                                        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                                            {(assignments[gw.id] || []).length === 0 ? (
                                                <span style={{ fontSize: '12px', color: 'var(--neutral-400)', fontStyle: 'italic' }}>할당된 프로파일 없음</span>
                                            ) : (
                                                assignments[gw.id].map(a => (
                                                    <span key={a.id} className="badge neutral-light" style={{ fontSize: '11px' }}>{a.name}</span>
                                                ))
                                            )}
                                        </div>
                                    </div>
                                </div>

                                <div className="mgmt-card-footer" style={{ borderTop: '1px solid var(--neutral-100)', paddingTop: '12px', marginTop: 'auto', display: 'flex', gap: '8px' }}>
                                    <button className="btn btn-outline btn-sm" onClick={() => { setSelectedGateway(gw); setIsDetailModalOpen(true); }} style={{ flex: 1 }}>
                                        <i className="fas fa-search-plus" /> 상세
                                    </button>
                                    <button className="btn btn-primary btn-sm" onClick={() => onDeploy(gw)} style={{ flex: 1 }} disabled={gw.live_status?.status !== 'online'}>
                                        <i className="fas fa-rocket" /> 배포
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
    gateway: Gateway | null;
    allAssignments: Record<number, Assignment[]>;
    targets: ExportTarget[];
    allProfiles: ExportProfile[];
    schedules: ExportSchedule[];
    onEdit: (gateway: Gateway) => void;
    onDelete: (gateway: Gateway) => void;
}> = ({ visible, onClose, gateway, allAssignments, targets, allProfiles, schedules, onEdit, onDelete }) => {
    if (!gateway) return null;

    // Derived info with fallback for names
    const assignments = gateway ? (Object.entries(allAssignments).find(([k]) => String(k) === String(gateway.id))?.[1] || []) : [];
    const assignedProfiles = assignments.map(a => {
        const found = allProfiles.find(p => String(p.id) === String(a.profile_id));
        if (found) return found;
        if (a.name) return { id: a.profile_id, name: a.name, data_points: [] } as any;
        return null;
    }).filter(Boolean) as ExportProfile[];

    const profileIds = assignedProfiles.map(p => p.id);
    const linkedTargets = targets.filter(t => profileIds.some(pid => String(pid) === String(t.profile_id)));
    const targetIds = linkedTargets.map(t => t.id);
    const linkedSchedules = schedules.filter(s => targetIds.some(tid => String(tid) === String(s.target_id)));

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
                        <i className="fas fa-magic" style={{ marginRight: '6px', color: 'var(--primary-600)' }} /> 설정 수정
                    </button>
                    <button className="btn btn-primary" onClick={onClose} style={{ minWidth: '80px' }}>
                        닫기
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
                                <h2 style={{ margin: 0, fontSize: '22px', fontWeight: 800, color: 'white' }}>{gateway.name}</h2>
                                <Tag color={gateway.live_status?.status === 'online' ? 'success' : 'default'} style={{ border: 'none', borderRadius: '4px', fontSize: '11px', height: '22px', lineHeight: '22px', fontWeight: 700, flexShrink: 0 }}>
                                    {gateway.live_status?.status === 'online' ? 'ONLINE' : 'OFFLINE'}
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
                            <i className="fas fa-project-diagram" style={{ color: 'var(--primary-500)' }} /> 데이터 파이프라인 구성 요약
                        </div>
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr auto 1fr auto 1fr', alignItems: 'center', gap: '20px' }}>
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>DATA SOURCE</div>
                                <div style={{ fontSize: '14px', fontWeight: 800, color: '#1e293b' }}>
                                    {assignedProfiles.length > 0 ? (
                                        <>
                                            <div style={{ color: 'var(--primary-600)' }}>{assignedProfiles[0].name}</div>
                                            <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>({assignedProfiles[0].data_points?.length || 0} Points)</div>
                                        </>
                                    ) : '미지정'}
                                </div>
                            </div>
                            <i className="fas fa-long-arrow-alt-right" style={{ color: '#cbd5e1', fontSize: '18px' }} />
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>EXPORT TARGET</div>
                                <div style={{ fontSize: '15px', fontWeight: 800, color: '#1e293b' }}>{linkedTargets.length > 0 ? `${linkedTargets.length}개 엔드포인트` : '-'}</div>
                            </div>
                            <i className="fas fa-long-arrow-alt-right" style={{ color: '#cbd5e1', fontSize: '18px' }} />
                            <div style={{ padding: '20px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #f1f5f9', textAlign: 'center' }}>
                                <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: 700, marginBottom: '4px' }}>EXPORT MODE</div>
                                <div style={{ fontSize: '15px', fontWeight: 800, color: '#1e293b' }}>
                                    {linkedSchedules.length > 0 ? '이벤트/정기 전송' : '설정 필요'}
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
