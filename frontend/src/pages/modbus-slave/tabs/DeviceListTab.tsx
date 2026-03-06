// =============================================================================
// frontend/src/pages/modbus-slave/tabs/DeviceListTab.tsx
// Modbus Slave 디바이스 카드 목록 + Start/Stop/Restart + CRUD
// GatewayListTab.tsx 와 동일한 패턴
// =============================================================================
import React, { useState } from 'react';
import { Modal, Input, InputNumber, Select, Switch, Form } from 'antd';
import modbusSlaveApi, { ModbusSlaveDevice } from '../../../api/services/modbusSlaveApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';
import { Site, Tenant } from '../../../types/common';

interface Props {
    devices: ModbusSlaveDevice[];
    loading: boolean;
    onRefresh: () => Promise<void>;
    // 부모에서 내려오는 필터 컨텍스트
    sites: Site[];
    tenants: Tenant[];
    isAdmin: boolean;
    currentTenantId: number | null;
    onSiteReload: (tenantId: number | null) => Promise<void>;
}

const DeviceListTab: React.FC<Props> = ({
    devices, loading, onRefresh,
    sites, tenants, isAdmin, currentTenantId, onSiteReload
}) => {
    const { confirm } = useConfirmContext();
    const [actionLoading, setActionLoading] = useState<number | null>(null);
    const [isFormOpen, setIsFormOpen] = useState(false);
    const [editingDevice, setEditingDevice] = useState<ModbusSlaveDevice | null>(null);
    const [form] = Form.useForm();

    // 폼에서 선택한 테넌트로 사이트 필터
    const [formTenantId, setFormTenantId] = useState<number | null>(currentTenantId);

    // 선택된 테넌트의 사이트만 표시
    const filteredSites = formTenantId
        ? sites.filter(s => (s as any).tenant_id === formTenantId)
        : sites;

    // ── 프로세스 제어 ─────────────────────────────────────────────────────────
    const handleAction = async (id: number, action: 'start' | 'stop' | 'restart') => {
        const labels = { start: '시작', stop: '중지', restart: '재시작' };
        const confirmed = await confirm({
            title: `Modbus Slave ${labels[action]}`,
            message: `디바이스를 ${labels[action]}하시겠습니까?`,
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
                title: res?.success ? `${labels[action]} 완료` : `${labels[action]} 실패`,
                message: res?.message || (res?.success ? '명령이 처리되었습니다.' : '처리에 실패했습니다.'),
                showCancelButton: false,
                confirmButtonType: res?.success ? 'primary' : 'danger',
            });
            await onRefresh();
        } catch {
            await confirm({ title: '오류', message: '서버 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        } finally {
            setActionLoading(null);
        }
    };

    // ── 삭제 ──────────────────────────────────────────────────────────────────
    const handleDelete = async (device: ModbusSlaveDevice) => {
        const confirmed = await confirm({
            title: '디바이스 삭제',
            message: `"${device.name}" 디바이스를 삭제하시겠습니까? 매핑도 함께 삭제됩니다.`,
            confirmText: '삭제',
            confirmButtonType: 'danger',
        });
        if (!confirmed) return;
        await modbusSlaveApi.deleteDevice(device.id);
        await onRefresh();
    };

    // ── 등록/수정 폼 ──────────────────────────────────────────────────────────
    const openCreate = () => {
        setEditingDevice(null);
        const defaultTenantId = currentTenantId;
        setFormTenantId(defaultTenantId);
        form.resetFields();
        form.setFieldsValue({
            tcp_port: 502,
            unit_id: 1,
            max_clients: 10,
            enabled: true,
            tenant_id: defaultTenantId ?? undefined,
        });
        setIsFormOpen(true);
    };

    const openEdit = (device: ModbusSlaveDevice) => {
        setEditingDevice(device);
        setFormTenantId(device.tenant_id ?? currentTenantId);
        form.setFieldsValue({ ...device, enabled: device.enabled === 1 });
        setIsFormOpen(true);
    };

    const handleFormSubmit = async () => {
        const values = await form.validateFields();
        const payload = { ...values, enabled: values.enabled ? 1 : 0 };
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
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>
                    <i className="fas fa-network-wired" style={{ marginRight: '8px', color: 'var(--primary-500)' }} />
                    등록된 Modbus Slave 디바이스
                </h3>
                <div style={{ display: 'flex', gap: '8px' }}>
                    <button className="btn btn-outline btn-sm" onClick={onRefresh} disabled={loading}>
                        <i className={`fas fa-sync-alt ${loading ? 'fa-spin' : ''}`} /> 새로고침
                    </button>
                    <button className="btn btn-primary btn-sm" onClick={openCreate}>
                        <i className="fas fa-plus" /> 디바이스 등록
                    </button>
                </div>
            </div>

            {/* 빈 상태 */}
            {devices.length === 0 ? (
                <div style={{ padding: '60px 0', textAlign: 'center', color: 'var(--neutral-400)' }}>
                    <i className="fas fa-network-wired fa-3x" style={{ marginBottom: '16px', opacity: 0.3, display: 'block' }} />
                    <p>등록된 Modbus Slave 디바이스가 없습니다.</p>
                    <button className="btn btn-primary btn-sm" onClick={openCreate}>
                        <i className="fas fa-plus" /> 첫 번째 디바이스 등록
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
                                                    {isRunning ? '실행 중' : '중지'}
                                                </div>
                                                <button onClick={() => openEdit(dev)} style={{ background: 'none', border: 'none', color: '#94a3b8', cursor: 'pointer', padding: '0 4px' }} title="수정">
                                                    <i className="fas fa-pencil-alt" />
                                                </button>
                                                <button onClick={() => handleDelete(dev)} style={{ background: 'none', border: 'none', color: '#cbd5e1', cursor: 'pointer', padding: '0 4px' }} title="삭제">
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
                                                <span className="badge" style={{ fontSize: '10px', background: '#fef3c7', color: '#92400e' }}>비활성화</span>
                                            )}
                                        </div>
                                    </div>
                                </div>

                                {/* 카드 바디 */}
                                <div className="mgmt-card-body">
                                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '13px' }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>TCP 포트</span>
                                            <span style={{ fontWeight: 600, fontFamily: 'monospace', color: 'var(--primary-600)' }}>:{dev.tcp_port}</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>Unit ID</span>
                                            <span style={{ fontFamily: 'monospace' }}>{dev.unit_id}</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>최대 클라이언트</span>
                                            <span>{dev.max_clients}개</span>
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                                            <span style={{ color: 'var(--neutral-500)' }}>매핑 포인트</span>
                                            <span>{dev.mapping_count || 0}개</span>
                                        </div>

                                        {/* 프로세스 제어 */}
                                        <div style={{ marginTop: '8px', padding: '10px', background: 'var(--neutral-50)', borderRadius: '6px' }}>
                                            <div style={{ fontSize: '11px', color: 'var(--neutral-500)', marginBottom: '8px', display: 'flex', justifyContent: 'space-between' }}>
                                                <span>프로세스 제어</span>
                                                {isRunning && dev.pid && <span style={{ fontFamily: 'monospace', fontSize: '10px' }}>PID: {dev.pid}</span>}
                                            </div>
                                            <div style={{ display: 'flex', gap: '4px' }}>
                                                {!isRunning ? (
                                                    <button className="btn btn-outline btn-xs" style={{ flex: 1, color: '#16a34a', borderColor: '#16a34a' }} onClick={() => handleAction(dev.id, 'start')} disabled={isLoading}>
                                                        {isLoading ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-play" style={{ fontSize: '10px' }} />} 시작
                                                    </button>
                                                ) : (
                                                    <>
                                                        <button className="btn btn-outline btn-xs btn-danger" style={{ flex: 1 }} onClick={() => handleAction(dev.id, 'stop')} disabled={isLoading}>
                                                            {isLoading ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-stop" style={{ fontSize: '10px' }} />} 중지
                                                        </button>
                                                        <button className="btn btn-outline btn-xs" style={{ flex: 1 }} onClick={() => handleAction(dev.id, 'restart')} disabled={isLoading}>
                                                            {isLoading ? <i className="fas fa-spinner fa-spin" /> : <i className="fas fa-redo" style={{ fontSize: '10px' }} />} 재시작
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
                                {editingDevice ? '디바이스 수정' : '새 디바이스 등록'}
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
                                    <i className="fas fa-building" style={{ marginRight: 6 }} />조직 정보
                                </div>
                                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
                                    <Form.Item name="tenant_id" label="테넌트" rules={[{ required: true, message: '테넌트를 선택하세요' }]} style={{ marginBottom: 0 }}>
                                        <Select placeholder="테넌트 선택" onChange={(val: number) => {
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
                                    <Form.Item name="site_id" label="사이트" rules={[{ required: true, message: '사이트를 선택하세요' }]} style={{ marginBottom: 0 }}>
                                        <Select placeholder="사이트 선택" disabled={!formTenantId}>
                                            {filteredSites.map(s => (
                                                <Select.Option key={s.id} value={s.id}>{s.name}</Select.Option>
                                            ))}
                                        </Select>
                                    </Form.Item>
                                </div>
                            </div>
                        )}

                        {!isAdmin && (
                            <Form.Item name="site_id" label={<span><i className="fas fa-map-marker-alt" style={{ marginRight: 6, color: '#7c3aed' }} />사이트</span>}
                                rules={[{ required: true, message: '사이트를 선택하세요' }]}>
                                <Select placeholder="사이트 선택">
                                    {filteredSites.map(s => (
                                        <Select.Option key={s.id} value={s.id}>{s.name}</Select.Option>
                                    ))}
                                </Select>
                            </Form.Item>
                        )}

                        {/* 디바이스 이름 */}
                        <Form.Item name="name"
                            label={<span><i className="fas fa-tag" style={{ marginRight: 6, color: '#7c3aed' }} />디바이스 이름</span>}
                            rules={[{ required: true, message: '이름을 입력하세요' }]}>
                            <Input placeholder="예: 1호기 SCADA Slave" size="large" />
                        </Form.Item>

                        {/* TCP / Unit ID */}
                        <div style={{
                            background: '#f8fafc', border: '1px solid #e2e8f0',
                            borderRadius: 10, padding: '16px', marginBottom: 20,
                        }}>
                            <div style={{ fontSize: 11, fontWeight: 700, color: '#475569', textTransform: 'uppercase', letterSpacing: 1, marginBottom: 12 }}>
                                <i className="fas fa-network-wired" style={{ marginRight: 6 }} />네트워크 설정
                            </div>
                            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 12 }}>
                                <Form.Item name="tcp_port" label="TCP 포트" rules={[{ required: true }]} style={{ marginBottom: 0 }}>
                                    <InputNumber min={1} max={65535} style={{ width: '100%' }} />
                                </Form.Item>
                                <Form.Item name="unit_id" label="Unit ID" rules={[{ required: true }]} style={{ marginBottom: 0 }}>
                                    <InputNumber min={1} max={247} style={{ width: '100%' }} />
                                </Form.Item>
                                <Form.Item name="max_clients" label="최대 클라이언트" style={{ marginBottom: 0 }}>
                                    <InputNumber min={1} max={100} style={{ width: '100%' }} />
                                </Form.Item>
                            </div>
                        </div>

                        {/* 설명 */}
                        <Form.Item name="description"
                            label={<span><i className="fas fa-align-left" style={{ marginRight: 6, color: '#94a3b8' }} />설명 <span style={{ color: '#94a3b8', fontWeight: 400 }}>(선택)</span></span>}>
                            <Input.TextArea rows={2} placeholder="디바이스에 대한 메모" style={{ resize: 'none' }} />
                        </Form.Item>

                        {/* 활성화 */}
                        <Form.Item name="enabled" valuePropName="checked" style={{ marginBottom: 0 }}>
                            <div style={{
                                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                                background: '#f0fdf4', border: '1px solid #bbf7d0',
                                borderRadius: 10, padding: '12px 16px',
                            }}>
                                <div>
                                    <div style={{ fontWeight: 600, fontSize: 14, color: '#15803d' }}>
                                        <i className="fas fa-power-off" style={{ marginRight: 8 }} />활성화
                                    </div>
                                    <div style={{ fontSize: 12, color: '#4ade80', marginTop: 2 }}>디바이스 활성화 시 시작 가능</div>
                                </div>
                                <Switch checkedChildren="ON" unCheckedChildren="OFF"
                                    checked={form.getFieldValue('enabled')}
                                    onChange={v => form.setFieldValue('enabled', v)}
                                    style={{ background: form.getFieldValue('enabled') ? '#16a34a' : '#94a3b8' }}
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
                        취소
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
                        {editingDevice ? '저장' : '등록'}
                    </button>
                </div>
            </Modal>
        </div>
    );
};

export default DeviceListTab;
