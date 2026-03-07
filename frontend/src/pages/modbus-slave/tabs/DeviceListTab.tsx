// =============================================================================
// frontend/src/pages/modbus-slave/tabs/DeviceListTab.tsx
// Modbus Slave 디바이스 카드 목록 + Start/Stop/Restart + CRUD
// GatewayListTab.tsx 와 동일한 패턴
// =============================================================================
import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { Modal, Input, InputNumber, Select, Switch, Form } from 'antd';
import modbusSlaveApi, { ModbusSlaveDevice, DeviceStats } from '../../../api/services/modbusSlaveApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';
import { Site, Tenant } from '../../../types/common';

interface Props {
    devices: ModbusSlaveDevice[];
    loading: boolean;
    onRefresh: () => Promise<void>;
    sites: Site[];
    tenants: Tenant[];
    isAdmin: boolean;
    currentTenantId: number | null;
    onSiteReload: (tenantId: number | null) => Promise<void>;
}

// 시스템에서 이미 사용 중인 예약 포트 목록
// docker-compose.yml 기준: Backend(3000), Collector API(8501/5001), RabbitMQ(5672,15672,1883),
// Redis(6379), InfluxDB(8086), Frontend Vite(5173), Simulator(50502), SSH(22), HTTP(80), HTTPS(443)
// ⚠️ 폼 오픈 시 백엔드 API로 실제 포트를 조회하여 교체됨 (Docker/Native 모두 정확)
const FALLBACK_RESERVED_PORTS = new Set([
    22, 80, 443, 1883,
    3000, 5001, 8501, 5173,
    5672, 15672, 6379, 8086,
    9229, 50502,
]);

const DeviceListTab: React.FC<Props> = ({
    devices, loading, onRefresh,
    sites, tenants, isAdmin, currentTenantId, onSiteReload
}) => {
    const { t } = useTranslation('settings');
    const { confirm } = useConfirmContext();
    const [actionLoading, setActionLoading] = useState<number | null>(null);
    const [isFormOpen, setIsFormOpen] = useState(false);
    const [editingDevice, setEditingDevice] = useState<ModbusSlaveDevice | null>(null);
    const [form] = Form.useForm();
    const [reservedPorts, setReservedPorts] = useState<Set<number>>(FALLBACK_RESERVED_PORTS);
    const [reservedLabels, setReservedLabels] = useState<Record<number, string>>({});

    const [formTenantId, setFormTenantId] = useState<number | null>(currentTenantId);
    const [showDeleted, setShowDeleted] = useState(false);
    const [deletedDevices, setDeletedDevices] = useState<ModbusSlaveDevice[]>([]);
    const [deletedLoading, setDeletedLoading] = useState(false);
    const [restoreLoading, setRestoreLoading] = useState<number | null>(null);
    // ── 실시간 stats 폴링 (current_clients) ────────────────────────────────
    const [statsMap, setStatsMap] = useState<Record<number, DeviceStats | null>>({});
    const statsTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);

    const fetchStats = useCallback(async () => {
        const running = devices.filter(d => d.process_status === 'running');
        if (running.length === 0) return;
        const results = await Promise.allSettled(running.map(d => modbusSlaveApi.getDeviceStats(d.id)));
        setStatsMap(prev => {
            const next = { ...prev };
            running.forEach((d, i) => {
                const r = results[i];
                next[d.id] = r.status === 'fulfilled' && r.value?.success ? r.value.data ?? null : null;
            });
            return next;
        });
    }, [devices]);

    useEffect(() => {
        fetchStats();
        statsTimerRef.current = setInterval(fetchStats, 3000);
        return () => { if (statsTimerRef.current) clearInterval(statsTimerRef.current); };
    }, [fetchStats]);

    const filteredSites = formTenantId
        ? sites.filter(s => (s as any).tenant_id === formTenantId)
        : sites;

    // ── 프로세스 제어 ─────────────────────────────────────────────────────────
    const handleAction = async (id: number, action: 'start' | 'stop' | 'restart') => {
        const labels: Record<string, string> = {
            start: t('modbusSlave.device.start'),
            stop: t('modbusSlave.device.stop'),
            restart: t('modbusSlave.device.restart'),
        };
        const confirmed = await confirm({
            title: `Modbus Slave ${labels[action]}`,
            message: `${labels[action]}?`,
            confirmText: labels[action],
            confirmButtonType: action === 'stop' ? 'danger' : 'primary',
        });
        if (!confirmed) return;

        setActionLoading(id);
        try {
            let res: any;
            if (action === 'start') res = await modbusSlaveApi.startDevice(id);
            else if (action === 'stop') res = await modbusSlaveApi.stopDevice(id);
            else res = await modbusSlaveApi.restartDevice(id);

            await confirm({
                title: res?.success ? `${labels[action]} OK` : `${labels[action]} Failed`,
                message: res?.message || (res?.success ? 'Done.' : 'Failed.'),
                showCancelButton: false,
                confirmButtonType: res?.success ? 'primary' : 'danger',
            });
            await onRefresh();
        } catch {
            await confirm({ title: 'Error', message: 'Server error.', showCancelButton: false, confirmButtonType: 'danger' });
        } finally {
            setActionLoading(null);
        }
    };

    // ── 삭제 ────────────────────────────────────────────────────────────────────────────
    const handleDelete = async (device: ModbusSlaveDevice) => {
        const confirmed = await confirm({
            title: t('modbusSlave.device.delete'),
            message: `"${device.name}" — ${t('modbusSlave.device.delete')}?`,
            confirmText: t('modbusSlave.device.delete'),
            confirmButtonType: 'danger',
        });
        if (!confirmed) return;
        await modbusSlaveApi.deleteDevice(device.id);
        await onRefresh();
    };

    // ── 삭제된 항목 로드 / 복구 ─────────────────────────────────────────────────────
    const handleToggleDeleted = async () => {
        if (!showDeleted) {
            setDeletedLoading(true);
            try {
                const res = await modbusSlaveApi.getDeletedDevices();
                if (res?.success) setDeletedDevices((res as any).data || []);
            } finally {
                setDeletedLoading(false);
            }
        }
        setShowDeleted(prev => !prev);
    };

    const handleRestore = async (device: ModbusSlaveDevice) => {
        const confirmed = await confirm({
            title: '삭제된 항목 복구',
            message: `"${device.name}"을(를) 복구하시겠습니까?`,
            confirmText: '복구',
            confirmButtonType: 'primary',
        });
        if (!confirmed) return;
        setRestoreLoading(device.id);
        try {
            await modbusSlaveApi.restoreDevice(device.id);
            const res = await modbusSlaveApi.getDeletedDevices();
            if (res?.success) setDeletedDevices((res as any).data || []);
            await onRefresh();
        } finally {
            setRestoreLoading(null);
        }
    };

    // ── 등록/수정 폼 ──────────────────────────────────────────────────────────
    const openCreate = async () => {
        setEditingDevice(null);
        const defaultTenantId = currentTenantId;
        setFormTenantId(defaultTenantId);
        form.resetFields();

        // 백엔드 API로 실제 시스템 예약 포트 조회 (Docker/Native 모두 정확)
        let sysPortSet = FALLBACK_RESERVED_PORTS;
        try {
            const res = await modbusSlaveApi.getReservedPorts();
            if (res?.success && res.data?.ports) {
                sysPortSet = new Set(res.data.ports);
                setReservedPorts(sysPortSet);
                setReservedLabels(res.data.labels || {});
            }
        } catch { /* fallback 유지 */ }

        // 기존 디바이스 + 시스템 예약 포트 모두 피해서 자동 포트 선택
        const usedPorts = new Set([
            ...devices.map(d => d.tcp_port),
            ...sysPortSet,
        ]);
        let autoPort = 502;
        while (usedPorts.has(autoPort)) { autoPort++; }

        form.setFieldsValue({
            tcp_port: autoPort,
            unit_id: 1,
            max_clients: 10,
            enabled: true,
            packet_logging: false,
            tenant_id: defaultTenantId ?? undefined,
        });
        setIsFormOpen(true);
    };

    const openEdit = (device: ModbusSlaveDevice) => {
        setEditingDevice(device);
        setFormTenantId(device.tenant_id ?? currentTenantId);
        form.setFieldsValue({
            ...device,
            enabled: device.enabled === 1,
            packet_logging: device.packet_logging === 1,
        });
        setIsFormOpen(true);
    };

    const handleFormSubmit = async () => {
        const values = await form.validateFields();
        const payload = {
            ...values,
            enabled: values.enabled ? 1 : 0,
            packet_logging: values.packet_logging ? 1 : 0,
        };
        if (editingDevice) {
            await modbusSlaveApi.updateDevice(editingDevice.id, payload);
        } else {
            await modbusSlaveApi.createDevice(payload);
        }
        setIsFormOpen(false);
        await onRefresh();
    };

    // ── 렌더 ──────────────────────────────────────────────────────────────────
    return (
        <div style={{ flex: 1, overflow: 'auto' }}>
            {/* 헤더 */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px', paddingTop: '16px' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>
                    <i className="fas fa-network-wired" style={{ marginRight: '8px', color: 'var(--primary-500)' }} />
                    {t('modbusSlave.registeredDevices')}
                </h3>
                <div style={{ display: 'flex', gap: '8px' }}>
                    <button className="btn btn-outline btn-sm" onClick={onRefresh} disabled={loading}>
                        <i className={`fas fa-sync-alt ${loading ? 'fa-spin' : ''}`} /> {t('modbusSlave.packetLog.refresh')}
                    </button>
                    <button
                        className="btn btn-outline btn-sm"
                        onClick={handleToggleDeleted}
                        style={{ color: showDeleted ? '#dc2626' : '#64748b', borderColor: showDeleted ? '#dc2626' : '#cbd5e1' }}
                    >
                        {deletedLoading
                            ? <i className="fas fa-spinner fa-spin" />
                            : <i className={`fas ${showDeleted ? 'fa-eye-slash' : 'fa-trash-restore'}`} />}
                        {' '}{showDeleted ? '정상 목록 보기' : `삭제된 항목${deletedDevices.length > 0 ? ` (${deletedDevices.length})` : ''}`}
                    </button>
                    <button className="btn btn-primary btn-sm" onClick={openCreate}>
                        <i className="fas fa-plus" /> {t('modbusSlave.device.add')}
                    </button>
                </div>
            </div>

            {/* 빈 상태 */}
            {devices.length === 0 ? (
                <div style={{ padding: '60px 0', textAlign: 'center', color: 'var(--neutral-400)' }}>
                    <i className="fas fa-network-wired fa-3x" style={{ marginBottom: '16px', opacity: 0.3, display: 'block' }} />
                    <p>{t('modbusSlave.noDevices')}</p>
                    <button className="btn btn-primary btn-sm" onClick={openCreate}>
                        <i className="fas fa-plus" /> {t('modbusSlave.addFirstDevice')}
                    </button>
                </div>
            ) : (
                <div className="mgmt-grid">
                    {devices.map(dev => {
                        const isRunning = dev.process_status === 'running';
                        const isLoading = actionLoading === dev.id;

                        return (
                            <div key={dev.id} className="mgmt-card">
                                {/* 카드 헤더 */}
                                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '16px' }}>
                                    <div style={{ flex: 1 }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                            <h4 style={{ margin: 0, fontSize: '15px' }}>{dev.name}</h4>
                                            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                                                <div className={`badge ${isRunning ? 'success' : 'neutral'}`}>
                                                    <i className="fas fa-circle" style={{ fontSize: '8px', marginRight: '5px' }} />
                                                    {isRunning ? t('modbusSlave.running') : t('modbusSlave.stopped')}
                                                </div>
                                                <button onClick={() => openEdit(dev)} style={{ background: 'none', border: 'none', color: '#94a3b8', cursor: 'pointer', padding: '0 4px' }} title={t('modbusSlave.device.edit')}>
                                                    <i className="fas fa-pencil-alt" />
                                                </button>
                                                <button onClick={() => handleDelete(dev)} style={{ background: 'none', border: 'none', color: '#cbd5e1', cursor: 'pointer', padding: '0 4px' }} title={t('modbusSlave.device.delete')}>
                                                    <i className="fas fa-trash-alt" />
                                                </button>
                                            </div>
                                        </div>
                                        {dev.description && (
                                            <div style={{ fontSize: '12px', color: 'var(--neutral-500)', marginTop: '2px' }}>{dev.description}</div>
                                        )}
                                        <div style={{ display: 'flex', gap: '4px', marginTop: '6px', flexWrap: 'wrap' }}>
                                            <span className="badge neutral-light" style={{ fontSize: '10px' }}>
                                                <i className="fas fa-map-marker-alt" style={{ marginRight: '4px' }} />
                                                {dev.site_name || `Site ${dev.site_id}`}
                                            </span>
                                            {dev.enabled === 0 && (
                                                <span className="badge" style={{ fontSize: '10px', background: '#fef3c7', color: '#92400e' }}>{t('modbusSlave.disabled')}</span>
                                            )}
                                        </div>
                                    </div>
                                </div>

                                {/* 카드 바디 */}
                                <div className="mgmt-card-body">
                                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '13px' }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>{t('modbusSlave.device.port')}</span>
                                            <span style={{ fontWeight: 600, fontFamily: 'monospace', color: 'var(--primary-600)' }}>:{dev.tcp_port}</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>{t('modbusSlave.device.unitId')}</span>
                                            <span style={{ fontFamily: 'monospace' }}>{dev.unit_id}</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>{t('modbusSlave.maxClients')}</span>
                                            <span>{dev.max_clients}</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>{t('modbusSlave.mappingPoints')}</span>
                                            <span>{dev.mapping_count || 0}</span>
                                        </div>
                                        {/* ── 현재 접속 클라이언트 수 (실시간) ─── */}
                                        {isRunning && (
                                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                <span style={{ color: 'var(--neutral-500)' }}>접속 클라이언트</span>
                                                <span style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                                                    <span style={{
                                                        fontWeight: 800, fontSize: 18,
                                                        color: (statsMap[dev.id]?.clients?.current_clients ?? 0) > 0 ? '#3b82f6' : '#94a3b8',
                                                    }}>
                                                        {statsMap[dev.id]?.clients?.current_clients ?? '—'}
                                                    </span>
                                                    <span style={{ fontSize: 11, color: '#94a3b8' }}>/ {dev.max_clients}</span>
                                                    {statsMap[dev.id] === undefined && (
                                                        <i className="fas fa-circle-notch fa-spin" style={{ fontSize: 10, color: '#94a3b8' }} />
                                                    )}
                                                </span>
                                            </div>
                                        )}

                                        {/* ── 마지막 요청 시각 ─── */}
                                        {dev.last_activity && (
                                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                <span style={{ color: 'var(--neutral-500)' }}>{t('modbusSlave.lastSeen')}</span>
                                                <span style={{ fontSize: 11, color: '#64748b', fontFamily: 'monospace' }}>
                                                    {new Date(dev.last_activity).toLocaleString('ko-KR', {
                                                        month: '2-digit', day: '2-digit',
                                                        hour: '2-digit', minute: '2-digit', hour12: false,
                                                    })}
                                                </span>
                                            </div>
                                        )}

                                        {/* 프로세스 제어 */}
                                        <div style={{ marginTop: '8px', padding: '10px', background: 'var(--neutral-50)', borderRadius: '6px' }}>
                                            <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '8px', display: 'flex', justifyContent: 'space-between' }}>
                                                <span>{t('modbusSlave.processControl')}</span>
                                                {isRunning && dev.pid && <span style={{ fontFamily: 'monospace', fontSize: '10px' }}>PID: {dev.pid}</span>}
                                            </div>
                                            <div style={{ display: 'flex', gap: '4px' }}>
                                                {!isRunning ? (
                                                    <button className="btn btn-outline btn-xs" style={{ flex: 1, color: '#16a34a', borderColor: '#16a34a' }} onClick={() => handleAction(dev.id, 'start')} disabled={isLoading}>
                                                        {isLoading ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-play" style={{ fontSize: '10px' }} />} {t('modbusSlave.device.start')}
                                                    </button>
                                                ) : (
                                                    <>
                                                        <button className="btn btn-outline btn-xs btn-danger" style={{ flex: 1 }} onClick={() => handleAction(dev.id, 'stop')} disabled={isLoading}>
                                                            {isLoading ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-stop" style={{ fontSize: '10px' }} />} {t('modbusSlave.device.stop')}
                                                        </button>
                                                        <button className="btn btn-outline btn-xs" style={{ flex: 1 }} onClick={() => handleAction(dev.id, 'restart')} disabled={isLoading}>
                                                            {isLoading ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-redo" style={{ fontSize: '10px' }} />} {t('modbusSlave.device.restart')}
                                                        </button>
                                                    </>
                                                )}
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        );
                    })}
                </div>
            )}

            {/* ──────── 삭제된 항목 목록 ──────── */}
            {showDeleted && (
                <div style={{ marginTop: 32 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 16, paddingBottom: 12, borderBottom: '2px dashed #fca5a5' }}>
                        <i className="fas fa-trash-restore" style={{ color: '#dc2626' }} />
                        <h4 style={{ margin: 0, color: '#dc2626', fontWeight: 600 }}>삭제된 디바이스</h4>
                        <span style={{ fontSize: 12, color: '#94a3b8' }}>(복구하면 다시 활성화됩니다)</span>
                    </div>
                    {deletedDevices.length === 0 ? (
                        <div style={{ textAlign: 'center', padding: 32, color: '#94a3b8' }}>
                            <i className="fas fa-check-circle fa-2x" style={{ marginBottom: 8, display: 'block', color: '#86efac' }} />
                            삭제된 디바이스가 없습니다
                        </div>
                    ) : (
                        <div className="mgmt-grid">
                            {deletedDevices.map(dev => (
                                <div key={dev.id} className="mgmt-card" style={{ opacity: 0.8, borderColor: '#fca5a5', background: '#fff5f5' }}>
                                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: 12 }}>
                                        <div>
                                            <h4 style={{ margin: 0, fontSize: 15, color: '#374151', textDecoration: 'line-through' }}>{dev.name}</h4>
                                            <div style={{ fontSize: 12, color: '#94a3b8', marginTop: 2 }}>
                                                {dev.site_name || `Site ${dev.site_id}`} &middot; 포트 :{dev.tcp_port}
                                            </div>
                                        </div>
                                        <span className="badge" style={{ background: '#fee2e2', color: '#dc2626', fontSize: 10 }}>
                                            <i className="fas fa-trash" style={{ marginRight: 4 }} />삭제됨
                                        </span>
                                    </div>
                                    <div style={{ fontSize: 12, color: '#6b7280', marginBottom: 12 }}>
                                        삭제일: {dev.updated_at ? new Date(dev.updated_at).toLocaleString('ko-KR') : '알 수 없음'}
                                    </div>
                                    <button
                                        className="btn btn-primary btn-sm"
                                        style={{ width: '100%', background: 'linear-gradient(135deg, #16a34a, #15803d)', border: 'none' }}
                                        onClick={() => handleRestore(dev)}
                                        disabled={restoreLoading === dev.id}
                                    >
                                        {restoreLoading === dev.id
                                            ? <><i className="fas fa-spinner fa-spin" /> 복구 중...</>
                                            : <><i className="fas fa-trash-restore" style={{ marginRight: 6 }} />이 디바이스 복구</>}
                                    </button>
                                </div>
                            ))}
                        </div>
                    )}
                </div>
            )}

            {/* 등록/수정 모달 */}
            <Modal
                open={isFormOpen}
                onOk={handleFormSubmit}
                onCancel={() => setIsFormOpen(false)}
                width={560}
                centered
                footer={null}
                closable={false}
                styles={{ body: { padding: 0 } }}
            >
                {/* ── 헤더 ── */}
                <div style={{
                    background: 'linear-gradient(135deg, #4f46e5 0%, #7c3aed 100%)',
                    borderRadius: '12px 12px 0 0',
                    padding: '24px 28px 20px',
                    color: '#fff',
                    position: 'relative',
                }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
                        <div style={{
                            width: 44, height: 44, borderRadius: 12,
                            background: 'rgba(255,255,255,0.18)',
                            display: 'flex', alignItems: 'center', justifyContent: 'center',
                            fontSize: 20,
                        }}>
                            <i className={`fas ${editingDevice ? 'fa-edit' : 'fa-plus-circle'}`} />
                        </div>
                        <div>
                            <div style={{ fontSize: 17, fontWeight: 700, letterSpacing: '-0.3px' }}>
                                {editingDevice ? t('modbusSlave.editDevice') : t('modbusSlave.addDevice')}
                            </div>
                            {editingDevice && (
                                <div style={{ fontSize: 13, opacity: 0.8, marginTop: 2 }}>
                                    {editingDevice.name}
                                </div>
                            )}
                        </div>
                    </div>
                    <button onClick={() => setIsFormOpen(false)} style={{
                        position: 'absolute', top: 16, right: 16,
                        background: 'rgba(255,255,255,0.15)', border: 'none',
                        color: '#fff', borderRadius: 8, width: 32, height: 32,
                        cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center',
                        fontSize: 14, transition: 'background 0.2s',
                    }}
                        onMouseEnter={e => (e.currentTarget.style.background = 'rgba(255,255,255,0.28)')}
                        onMouseLeave={e => (e.currentTarget.style.background = 'rgba(255,255,255,0.15)')}
                    >
                        <i className="fas fa-times" />
                    </button>
                </div>

                {/* ── 폼 본문 ── */}
                <div style={{ padding: '24px 28px' }}>
                    <Form form={form} layout="vertical">

                        {/* 테넌트/사이트 섹션 */}
                        {isAdmin && (
                            <div style={{
                                background: '#f8f7ff', border: '1px solid #e5e0fa',
                                borderRadius: 10, padding: '16px', marginBottom: 20,
                            }}>
                                <div style={{ fontSize: 11, fontWeight: 700, color: '#7c3aed', textTransform: 'uppercase', letterSpacing: 1, marginBottom: 12 }}>
                                    <i className="fas fa-building" style={{ marginRight: 6 }} />{t('modbusSlave.orgInfo')}
                                </div>
                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
                                    <Form.Item name="tenant_id" label={t('modbusSlave.tenant')} rules={[{ required: true, message: t('modbusSlave.tenantSelect') }]} style={{ marginBottom: 0 }}>
                                        <Select placeholder={t('modbusSlave.tenantSelect')} onChange={(val: number) => {
                                            setFormTenantId(val);
                                            form.setFieldValue('site_id', undefined);
                                            onSiteReload(val);
                                        }}>
                                            {tenants.map(tn => (
                                                <Select.Option key={tn.id} value={tn.id}>
                                                    {(tn as any).company_name || (tn as any).name}
                                                </Select.Option>
                                            ))}
                                        </Select>
                                    </Form.Item>
                                    <Form.Item name="site_id" label={t('modbusSlave.site')} rules={[{ required: true, message: t('modbusSlave.siteSelect') }]} style={{ marginBottom: 0 }}>
                                        <Select placeholder={t('modbusSlave.siteSelect')} disabled={!formTenantId}>
                                            {filteredSites.map(s => (
                                                <Select.Option key={s.id} value={s.id}>{s.name}</Select.Option>
                                            ))}
                                        </Select>
                                    </Form.Item>
                                </div>
                            </div>
                        )}

                        {!isAdmin && (
                            <Form.Item name="site_id" label={<span><i className="fas fa-map-marker-alt" style={{ marginRight: 6, color: '#7c3aed' }} />{t('modbusSlave.site')}</span>}
                                rules={[{ required: true, message: t('modbusSlave.siteSelect') }]}>
                                <Select placeholder={t('modbusSlave.siteSelect')}>
                                    {filteredSites.map(s => (
                                        <Select.Option key={s.id} value={s.id}>{s.name}</Select.Option>
                                    ))}
                                </Select>
                            </Form.Item>
                        )}

                        {/* 디바이스 이름 */}
                        <Form.Item name="name"
                            label={<span><i className="fas fa-tag" style={{ marginRight: 6, color: '#7c3aed' }} />{t('modbusSlave.deviceName')}</span>}
                            rules={[{ required: true, message: t('modbusSlave.deviceName') }]}>
                            <Input placeholder={t('modbusSlave.deviceNamePlaceholder')} size="large" />
                        </Form.Item>

                        {/* TCP / Unit ID */}
                        <div style={{
                            background: '#f8fafc', border: '1px solid #e2e8f0',
                            borderRadius: 10, padding: '16px', marginBottom: 20,
                        }}>
                            <div style={{ fontSize: 11, fontWeight: 700, color: '#475569', textTransform: 'uppercase', letterSpacing: 1, marginBottom: 12 }}>
                                <i className="fas fa-network-wired" style={{ marginRight: 6 }} />{t('modbusSlave.networkSettings')}
                            </div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 12 }}>
                                <Form.Item name="tcp_port" label={t('modbusSlave.device.port')} style={{ marginBottom: 0 }}
                                    rules={[
                                        { required: true },
                                        {
                                            validator: (_, val) => {
                                                if (!val) return Promise.resolve();
                                                if (reservedPorts.has(val)) {
                                                    const svcName = reservedLabels[val] || 'system service';
                                                    return Promise.reject(`Port ${val} is reserved by ${svcName}`);
                                                }
                                                const conflict = devices.find(d =>
                                                    d.tcp_port === val && d.id !== editingDevice?.id
                                                );
                                                if (conflict) {
                                                    return Promise.reject(`Port ${val} is already used by "${conflict.name}"`);
                                                }
                                                return Promise.resolve();
                                            }
                                        }
                                    ]}>
                                    <InputNumber min={1} max={65535} style={{ width: '100%' }} />
                                </Form.Item>
                                <Form.Item name="unit_id" label={t('modbusSlave.device.unitId')} rules={[{ required: true }]} style={{ marginBottom: 0 }}>
                                    <InputNumber min={1} max={247} style={{ width: '100%' }} />
                                </Form.Item>
                                <Form.Item name="max_clients" label={t('modbusSlave.maxClients')} style={{ marginBottom: 0 }}>
                                    <InputNumber min={1} max={100} style={{ width: '100%' }} />
                                </Form.Item>
                            </div>
                        </div>

                        {/* 설명 */}
                        <Form.Item name="description"
                            label={<span><i className="fas fa-align-left" style={{ marginRight: 6, color: '#94a3b8' }} />{t('modbusSlave.description')} <span style={{ color: '#94a3b8', fontWeight: 400 }}>{t('modbusSlave.descriptionOptional')}</span></span>}>
                            <Input.TextArea rows={2} placeholder={t('modbusSlave.descriptionPlaceholder')} style={{ resize: 'none' }} />
                        </Form.Item>

                        {/* 활성화 */}
                        <Form.Item name="enabled" valuePropName="checked" style={{ marginBottom: 12 }}>
                            <div style={{
                                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                                background: '#f0fdf4', border: '1px solid #bbf7d0',
                                borderRadius: 10, padding: '12px 16px',
                            }}>
                                <div>
                                    <div style={{ fontWeight: 600, fontSize: 14, color: '#15803d' }}>
                                        <i className="fas fa-power-off" style={{ marginRight: 8 }} />{t('modbusSlave.activation')}
                                    </div>
                                    <div style={{ fontSize: 12, color: '#4ade80', marginTop: 2 }}>{t('modbusSlave.activationDesc')}</div>
                                </div>
                                <Switch checkedChildren="ON" unCheckedChildren="OFF"
                                    checked={form.getFieldValue('enabled')}
                                    onChange={v => form.setFieldValue('enabled', v)}
                                    style={{ background: form.getFieldValue('enabled') ? '#16a34a' : '#94a3b8' }}
                                />
                            </div>
                        </Form.Item>

                        {/* 패킷 로그 */}
                        <Form.Item name="packet_logging" valuePropName="checked" style={{ marginBottom: 0 }}>
                            <div style={{
                                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                                background: '#eff6ff', border: '1px solid #bfdbfe',
                                borderRadius: 10, padding: '12px 16px',
                            }}>
                                <div>
                                    <div style={{ fontWeight: 600, fontSize: 14, color: '#1d4ed8' }}>
                                        <i className="fas fa-terminal" style={{ marginRight: 8 }} />{t('modbusSlave.device.packetLogging')}
                                    </div>
                                    <div style={{ fontSize: 12, color: '#60a5fa', marginTop: 2 }}>
                                        {t('modbusSlave.packetLogDesc')}
                                    </div>
                                </div>
                                <Switch checkedChildren="ON" unCheckedChildren="OFF"
                                    checked={form.getFieldValue('packet_logging')}
                                    onChange={v => form.setFieldValue('packet_logging', v)}
                                    style={{ background: form.getFieldValue('packet_logging') ? '#2563eb' : '#94a3b8' }}
                                />
                            </div>
                        </Form.Item>
                    </Form>
                </div>

                {/* ── 푸터 ── */}
                <div style={{
                    padding: '16px 28px',
                    borderTop: '1px solid #f1f5f9',
                    display: 'flex', justifyContent: 'flex-end', gap: 10,
                }}>
                    <button onClick={() => setIsFormOpen(false)} style={{
                        padding: '9px 20px', borderRadius: 8, border: '1px solid #e2e8f0',
                        background: '#fff', color: '#475569', cursor: 'pointer',
                        fontSize: 14, fontWeight: 500, transition: 'all 0.15s',
                    }}
                        onMouseEnter={e => (e.currentTarget.style.background = '#f8fafc')}
                        onMouseLeave={e => (e.currentTarget.style.background = '#fff')}
                    >
                        {t('modbusSlave.cancel')}
                    </button>
                    <button onClick={handleFormSubmit} style={{
                        padding: '9px 24px', borderRadius: 8, border: 'none',
                        background: 'linear-gradient(135deg, #4f46e5, #7c3aed)',
                        color: '#fff', cursor: 'pointer', fontSize: 14, fontWeight: 600,
                        boxShadow: '0 2px 8px rgba(124,58,237,0.35)', transition: 'all 0.15s',
                    }}
                        onMouseEnter={e => (e.currentTarget.style.filter = 'brightness(1.08)')}
                        onMouseLeave={e => (e.currentTarget.style.filter = '')}
                    >
                        <i className={`fas ${editingDevice ? 'fa-save' : 'fa-plus'}`} style={{ marginRight: 7 }} />
                        {editingDevice ? t('modbusSlave.save') : t('modbusSlave.register')}
                    </button>
                </div>
            </Modal>
        </div>
    );
};

export default DeviceListTab;
