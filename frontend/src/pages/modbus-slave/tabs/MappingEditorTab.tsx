// =============================================================================
// frontend/src/pages/modbus-slave/tabs/MappingEditorTab.tsx
// Modbus 레지스터 매핑 에디터
// - 포인트 목록에서 체크박스로 다중 선택 → 일괄 등록
// - 등록된 매핑 테이블에서 인라인 편집 / 삭제
// =============================================================================
import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import {
    Select, InputNumber, Table, Button, Checkbox, Input,
    Tag, Tooltip, Radio
} from 'antd';
import type { ColumnsType } from 'antd/es/table';
import modbusSlaveApi, { ModbusSlaveDevice, ModbusSlaveMapping, RegisterValue } from '../../../api/services/modbusSlaveApi';
import { DataPoint } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

interface Props {
    devices: ModbusSlaveDevice[];
    allPoints: DataPoint[];
}

// 매핑 행 (편집용 키 포함) — ModbusSlaveMapping을 extends하지 않고 독립 인터페이스로 분리
interface MappingRow {
    key: string;
    id?: number;
    point_id: number | null;
    point_label?: string;
    register_type: string; // 'HR'|'IR'|'CO'|'DI'  — 편집 중엔 string
    register_address: number;
    data_type: string;
    byte_order: string;
    scale_factor: number;
    scale_offset: number;
    enabled: number;
}

const REG_TYPES = [
    { value: 'HR', label: 'HR', color: '#dbeafe', text: '#1d4ed8', full: 'Holding Register (4xxxx)' },
    { value: 'IR', label: 'IR', color: '#dcfce7', text: '#15803d', full: 'Input Register (3xxxx)' },
    { value: 'CO', label: 'CO', color: '#fef9c3', text: '#854d0e', full: 'Coil (0xxxx)' },
    { value: 'DI', label: 'DI', color: '#fce7f3', text: '#9d174d', full: 'Discrete Input (1xxxx)' },
];
const DATA_TYPES = ['FLOAT32', 'INT16', 'UINT16', 'INT32', 'UINT32', 'FLOAT64', 'BOOL'];
const BYTE_ORDERS = ['big_endian', 'little_endian', 'big_endian_swap', 'little_endian_swap'];

const newRow = (overrides: Partial<MappingRow> = {}): MappingRow => ({
    key: `new-${Date.now()}-${Math.random()}`,
    point_id: null,
    register_type: 'HR',
    register_address: 1,
    data_type: 'FLOAT32',
    byte_order: 'big_endian',
    scale_factor: 1.0,
    scale_offset: 0.0,
    enabled: 1,
    ...overrides,
});

const MappingEditorTab: React.FC<Props> = ({ devices, allPoints }) => {
    const { t } = useTranslation('settings');
    const { confirm } = useConfirmContext();

    const [selectedDeviceId, setSelectedDeviceId] = useState<number | null>(null);
    const [rows, setRows] = useState<MappingRow[]>([]);
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [dirty, setDirty] = useState(false);

    // ── 포인트 선택 모달 대신 좌우 분할 패널 ─────────────────────────────────
    const [mode, setMode] = useState<'table' | 'select'>('table');
    const [pointSearch, setPointSearch] = useState('');
    const [pointDeviceFilter, setPointDeviceFilter] = useState<string>('all');
    const [selectedPointIds, setSelectedPointIds] = useState<Set<number>>(new Set());
    const [bulkRegType, setBulkRegType] = useState('HR');
    const [bulkStartAddr, setBulkStartAddr] = useState(1);
    const [editingKeys, setEditingKeys] = useState<Set<string>>(new Set());
    // 전역 편집 모드
    const [editMode, setEditMode] = useState(false);
    // 편집 취소용 스냅샷 (전체)
    const [rowsSnapshot, setRowsSnapshot] = useState<MappingRow[]>([]);
    // 실시간 값 취제 (point_id → value)
    const [registerValues, setRegisterValues] = useState<Map<number, RegisterValue>>(new Map());

    // ── 매핑 로드 ────────────────────────────────────────────────────────────
    const loadMappings = useCallback(async (deviceId: number) => {
        setLoading(true);
        try {
            const [res, rvRes] = await Promise.all([
                modbusSlaveApi.getMappings(deviceId),
                modbusSlaveApi.getRegisterValues(deviceId).catch(() => null),
            ]);
            const data = Array.isArray(res.data) ? res.data
                : (res.data as any)?.data ?? [];
            setRows((data as ModbusSlaveMapping[]).map(m => ({
                ...m,
                key: String(m.id),
                point_label: allPoints.find(p => p.id === m.point_id)?.name,
            })));
            // 실시간 값 Map 구성
            if (rvRes?.data) {
                const rvData = Array.isArray(rvRes.data) ? rvRes.data : (rvRes.data as any)?.data ?? [];
                const map = new Map<number, RegisterValue>();
                (rvData as RegisterValue[]).forEach(rv => {
                    if (rv.point_id != null) map.set(rv.point_id, rv);
                });
                setRegisterValues(map);
            }
            setDirty(false);
        } finally {
            setLoading(false);
        }
    }, [allPoints]);

    useEffect(() => {
        if (selectedDeviceId) loadMappings(selectedDeviceId);
        else { setRows([]); setDirty(false); }
    }, [selectedDeviceId, loadMappings]);

    // ── 점 수정 헬퍼 ─────────────────────────────────────────────────────────
    const updateRow = (key: string, field: string, value: any) => {
        setRows(prev => prev.map(r => r.key === key ? { ...r, [field]: value } : r));
        setDirty(true);
    };

    const enterEditMode = () => {
        setRowsSnapshot([...rows.map(r => ({ ...r }))]);
        setEditMode(true);
    };

    const exitEditMode = (revert = false) => {
        if (revert) setRows(rowsSnapshot);
        setEditMode(false);
    };

    // ── 행 삭제 ──────────────────────────────────────────────────────────────
    const deleteRow = async (key: string) => {
        const ok = await confirm({
            title: t('modbusSlave.device.delete'),
            message: t('modbusSlave.device.delete') + '?',
            confirmText: t('modbusSlave.device.delete'),
            confirmButtonType: 'danger',
        });
        if (!ok) return;
        setRows(prev => prev.filter(r => r.key !== key));
        setDirty(true);
    };

    // ── 포인트 일괄 추가 ──────────────────────────────────────────────────────
    const applyBulkAdd = () => {
        if (selectedPointIds.size === 0) return;

        // 현재 매핑에서 선택한 레지스터 타입 최대 주소 찾기
        const existingAddrs = rows
            .filter(r => r.register_type === bulkRegType)
            .map(r => r.register_address);
        let nextAddr = bulkStartAddr > 1 ? bulkStartAddr
            : (existingAddrs.length > 0 ? Math.max(...existingAddrs) + 2 : 1);

        const newRows: MappingRow[] = [];
        Array.from(selectedPointIds).forEach(pid => {
            const pt = allPoints.find(p => p.id === pid);
            newRows.push(newRow({
                point_id: pid,
                point_label: pt ? `${pt.device_name} / ${pt.name}` : String(pid),
                register_type: bulkRegType,
                register_address: nextAddr,
            }));
            nextAddr += 2; // FLOAT32 기본 2레지스터
        });

        setRows(prev => [...prev, ...newRows]);
        setSelectedPointIds(new Set());
        setDirty(true);
        setMode('table'); // 테이블 뷰로 복귀
    };

    // ── 저장 ─────────────────────────────────────────────────────────────────
    const handleSave = async () => {
        if (!selectedDeviceId) return;

        const okSave = await confirm({
            title: t('modbusSlave.save'),
            message: `${t('modbusSlave.registerMapping')} ${rows.length}?`,
            confirmText: t('modbusSlave.save'),
        });
        if (!okSave) return;

        const incomplete = rows.filter(r => !r.point_id);
        if (incomplete.length > 0) {
            await confirm({
                title: '저장 실패',
                message: `${incomplete.length}개 행에 수집 포인트가 선택되지 않았습니다.`,
                showCancelButton: false,
                confirmButtonType: 'danger',
            });
            return;
        }

        // 중복 주소 검사
        const addrMap = new Map<string, number[]>();
        rows.forEach((r, i) => {
            const key = `${r.register_type}:${r.register_address}`;
            if (!addrMap.has(key)) addrMap.set(key, []);
            addrMap.get(key)!.push(i + 1);
        });
        const dup = [...addrMap.entries()].filter(([, v]) => v.length > 1);
        if (dup.length > 0) {
            const msg = dup.map(([k, lines]) => `${k} (행 ${lines.join(', ')})`).join('\n');
            await confirm({
                title: '중복 주소',
                message: `중복된 레지스터 주소가 있습니다:\n${msg}`,
                showCancelButton: false,
                confirmButtonType: 'danger',
            });
            return;
        }

        setSaving(true);
        try {
            const payload = rows.map(r => ({
                point_id: r.point_id,
                register_type: r.register_type as 'HR' | 'IR' | 'CO' | 'DI',
                register_address: r.register_address,
                data_type: r.data_type,
                byte_order: r.byte_order,
                scale_factor: r.scale_factor,
                scale_offset: r.scale_offset,
                enabled: r.enabled,
            }));
            await modbusSlaveApi.saveMappings(selectedDeviceId, payload);
            await loadMappings(selectedDeviceId);
            exitEditMode();
        } catch (e: any) {
            await confirm({
                title: '저장 실패',
                message: e.message || '저장 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger',
            });
        } finally {
            setSaving(false);
        }
    };

    // ── 테이블 컬럼 ──────────────────────────────────────────────────────────
    // SCADA 1-base 주소 계산: 레지스터 타입 prefix + (0-base + 1)
    const scadaAddressMap: Record<string, number> = { HR: 40001, IR: 30001, CO: 1, DI: 10001 };
    const scadaAddr = (row: MappingRow) => scadaAddressMap[row.register_type] + row.register_address - 1;

    // 겹침 주소 집합 계산 (같은 register_type+address가 2개 이상)
    const overlapKeys = new Set<string>();
    const addrCount = new Map<string, number>();
    rows.forEach(r => {
        const k = `${r.register_type}:${r.register_address}`;
        addrCount.set(k, (addrCount.get(k) ?? 0) + 1);
    });
    addrCount.forEach((count, k) => { if (count > 1) overlapKeys.add(k); });

    const columns: ColumnsType<MappingRow> = [
        {
            title: 'Address',
            dataIndex: 'register_address',
            width: 120,
            align: 'center' as const,
            render: (_, row) => {
                const isOverlap = overlapKeys.has(`${row.register_type}:${row.register_address}`);
                return editMode ? (
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                        <InputNumber size="small" style={{ width: '100%', textAlign: 'center', borderColor: isOverlap ? '#ef4444' : undefined }} min={1} max={65535}
                            value={row.register_address}
                            onChange={v => updateRow(row.key, 'register_address', v)} />
                        {isOverlap && (
                            <span style={{ fontSize: 10, color: '#ef4444', textAlign: 'center' }}>
                                <i className="fas fa-exclamation-triangle" /> 주소 겹침
                            </span>
                        )}
                    </div>
                ) : (
                    <Tooltip title={isOverlap ? '⚠️ 동일 주소가 중복됩니다' : `SCADA: ${scadaAddr(row)}`}>
                        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 1 }}>
                            <span style={{ fontSize: 13, color: isOverlap ? '#ef4444' : '#1e293b', fontWeight: isOverlap ? 700 : 400 }}>
                                {row.register_address}
                                {isOverlap && <i className="fas fa-exclamation-triangle" style={{ color: '#ef4444', marginLeft: 5 }} />}
                            </span>
                            <span style={{ fontSize: 10, color: '#94a3b8' }}>SCADA:{scadaAddr(row)}</span>
                        </div>
                    </Tooltip>
                );
            },
        },
        {
            title: 'Register Type',
            dataIndex: 'register_type',
            width: 120,
            align: 'center' as const,
            render: (_, row) => editMode ? (
                <Select size="small" style={{ width: '100%' }} value={row.register_type}
                    onChange={v => updateRow(row.key, 'register_type', v)}
                    options={REG_TYPES.map(rt => ({ value: rt.value, label: rt.label }))}
                />
            ) : (
                <span style={{ fontSize: 13, fontWeight: 600, color: REG_TYPES.find(r => r.value === row.register_type)?.text ?? '#475569' }}>{row.register_type}</span>
            ),
        },
        {
            title: 'Data Point',
            dataIndex: 'point_id',
            width: 240,
            render: (_, row) => editMode ? (
                <Select
                    size="small" showSearch style={{ width: '100%' }}
                    value={row.point_id} placeholder="포인트 선택"
                    optionFilterProp="label"
                    onChange={v => updateRow(row.key, 'point_id', v)}
                    options={allPoints.map(p => ({
                        value: p.id,
                        label: `[${p.device_name}] ${p.name}${p.unit ? ` (${p.unit})` : ''}`,
                    }))}
                />
            ) : (
                <span style={{ fontSize: 13 }}>{row.point_label || `Point #${row.point_id}`}</span>
            ),
        },
        {
            title: 'Data Type',
            dataIndex: 'data_type',
            width: 110,
            align: 'center' as const,
            render: (_, row) => editMode ? (
                <Select size="small" style={{ width: '100%' }} value={row.data_type}
                    onChange={v => updateRow(row.key, 'data_type', v)}
                    options={DATA_TYPES.map(d => ({ value: d, label: d }))}
                />
            ) : (
                <span style={{ fontSize: 13 }}>{row.data_type}</span>
            ),
        },
        {
            title: 'Byte Order',
            dataIndex: 'byte_order',
            width: 130,
            align: 'center' as const,
            render: (_, row) => editMode ? (
                <Select size="small" style={{ width: '100%' }} value={row.byte_order}
                    onChange={v => updateRow(row.key, 'byte_order', v)}
                    options={BYTE_ORDERS.map(b => ({ value: b, label: b }))}
                />
            ) : (
                <span style={{ fontSize: 13 }}>{row.byte_order}</span>
            ),
        },
        {
            title: 'Scale',
            dataIndex: 'scale_factor',
            width: 80,
            align: 'center' as const,
            render: (_, row) => editMode ? (
                <InputNumber size="small" style={{ width: '100%' }} value={row.scale_factor}
                    onChange={v => updateRow(row.key, 'scale_factor', v)} />
            ) : (
                <span style={{ fontSize: 13 }}>{row.scale_factor}</span>
            ),
        },
        {
            title: 'Offset',
            dataIndex: 'scale_offset',
            width: 80,
            align: 'center' as const,
            render: (_, row) => editMode ? (
                <InputNumber size="small" style={{ width: '100%' }} value={row.scale_offset}
                    onChange={v => updateRow(row.key, 'scale_offset', v)} />
            ) : (
                <span style={{ fontSize: 13 }}>{row.scale_offset}</span>
            ),
        },
        {
            title: <span style={{ fontSize: 11 }}>현재 값</span>,
            dataIndex: 'current_value',
            width: 110,
            align: 'right' as const,
            render: (_, row) => {
                if (!row.point_id) return <span style={{ color: '#cbd5e1', fontSize: 11 }}>—</span>;
                const rv = registerValues.get(row.point_id);
                if (!rv || rv.value == null) return <span style={{ color: '#94a3b8', fontSize: 11 }}>—</span>;
                const qual = rv.quality || 'unknown';
                const qualColor = qual === 'good' ? '#16a34a' : qual === 'unknown' ? '#94a3b8' : '#ef4444';
                const valStr = typeof rv.value === 'number' ? rv.value.toFixed(3) : String(rv.value);
                const rawStr = rv.register_raw != null ? `Reg: ${rv.register_raw.toFixed(3)}` : '';
                const tsStr = rv.value_timestamp ? new Date(rv.value_timestamp).toLocaleTimeString() : '';
                return (
                    <Tooltip title={`공학값: ${valStr} | ${rawStr} | 질: ${qual} | ${tsStr}`}>
                        <span style={{ fontFamily: 'monospace', fontWeight: 600, color: qualColor, fontSize: 12 }}>
                            {valStr}
                        </span>
                    </Tooltip>
                );
            },
        },
        ...(editMode ? [{
            title: '',
            dataIndex: 'action',
            width: 40,
            align: 'center' as const,
            render: (_: any, row: MappingRow) => (
                <Tooltip title="삭제">
                    <button onClick={() => deleteRow(row.key)}
                        style={{ background: 'none', border: 'none', color: '#ef4444', cursor: 'pointer', padding: '4px' }}>
                        <i className="fas fa-trash" />
                    </button>
                </Tooltip>
            ),
        }] : []),
    ];

    // ── 포인트 선택 패널용 필터링 ─────────────────────────────────────────────
    const mappedPointIds = new Set(rows.map(r => r.point_id).filter(Boolean));
    const deviceOptions = Array.from(new Set(allPoints.map(p => p.device_name))).sort()
        .map(dn => ({ label: dn, value: dn }));

    const filteredPoints = allPoints.filter(p => {
        if (mappedPointIds.has(p.id)) return false;
        if (pointDeviceFilter !== 'all' && p.device_name !== pointDeviceFilter) return false;
        if (!pointSearch) return true;
        const q = pointSearch.toLowerCase();
        return ((p as any).name + p.device_name + (p.unit || '') + ((p as any).data_type || '')).toLowerCase().includes(q);
    });

    // 포인트 선택 테이블 코럼 — ID → 디바이스 → 포인트명 → 자료형 → 단위
    const allFilteredSelected = filteredPoints.length > 0 && filteredPoints.every(p => selectedPointIds.has(p.id));
    const someSelected = filteredPoints.some(p => selectedPointIds.has(p.id)) && !allFilteredSelected;

    const pointColumns = [
        {
            title: (
                <div style={{ display: 'flex', justifyContent: 'center' }}>
                    <Checkbox
                        checked={allFilteredSelected}
                        indeterminate={someSelected}
                        onChange={() => {
                            const all = new Set(filteredPoints.map(p => p.id));
                            setSelectedPointIds(prev =>
                                allFilteredSelected ? new Set() : all
                            );
                        }}
                    />
                </div>
            ),
            dataIndex: 'checkbox', width: 44, fixed: 'left' as const,
            align: 'center' as const,
            render: (_: any, p: any) => (
                <Checkbox checked={selectedPointIds.has(p.id)}
                    onChange={() => setSelectedPointIds(prev => {
                        const next = new Set(prev);
                        if (next.has(p.id)) next.delete(p.id); else next.add(p.id);
                        return next;
                    })} />
            ),
        },
        {
            title: 'ID', dataIndex: 'id', width: 64, align: 'center' as const,
            render: (v: number) => <span style={{ color: '#94a3b8', fontSize: 12 }}>{v}</span>,
        },
        {
            title: '디바이스', dataIndex: 'device_name', width: 160, align: 'center' as const,
            render: (v: string) => <span style={{ color: '#475569' }}>{v}</span>,
        },
        {
            title: '포인트명', dataIndex: 'name', width: 260,
            render: (_: any, p: any) => (
                <span style={{ fontWeight: 600, color: '#7c3aed', wordBreak: 'break-all' }}>{p.name}</span>
            ),
        },
        {
            title: '자료형', dataIndex: 'data_type', width: 96, align: 'center' as const,
            render: (v: string) => v ? (
                <span style={{ fontSize: 11, background: '#eff6ff', color: '#1d4ed8', borderRadius: 4, padding: '2px 6px', fontWeight: 600 }}>{v}</span>
            ) : '-',
        },
        {
            title: '단위', dataIndex: 'unit', width: 72, align: 'center' as const,
            render: (v: string) => v || '-',
        },
    ];


    const selectedDevice = devices.find(d => d.id === selectedDeviceId);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', paddingTop: '16px' }}>
            {/* 디바이스 선택 헤더 — nowrap, 좁아지면 가로 스크롤 */}
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flexWrap: 'nowrap', overflowX: 'auto' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexShrink: 0 }}>
                    <i className="fas fa-th" style={{ color: '#7c3aed' }} />
                    <strong>{t('modbusSlave.registerMapping')}</strong>
                    <Select
                        style={{ width: 220 }}
                        placeholder={t('modbusSlave.selectDevice')}
                        value={selectedDeviceId}
                        onChange={id => { setSelectedDeviceId(id); setMode('table'); }}
                        options={devices.map(d => ({
                            value: d.id,
                            label: `${d.name} (:${d.tcp_port})`,
                        }))}
                    />
                </div>
                {/* 선택된 디바이스가 있으면 탭 + 저장 + 범례 인라인 */}
                {selectedDeviceId && (
                    <div style={{ display: 'flex', gap: '8px', alignItems: 'center', flexWrap: 'nowrap', flexShrink: 0, marginLeft: 'auto' }}>
                        {/* 레전드 */}
                        <div style={{ display: 'flex', gap: 4, alignItems: 'center' }}>
                            {[
                                { value: 'HR', full: 'Holding Register (4xxxx)', bg: '#dbeafe', text: '#1d4ed8' },
                                { value: 'IR', full: 'Input Register (3xxxx)', bg: '#dcfce7', text: '#15803d' },
                                { value: 'CO', full: 'Coil (0xxxx)', bg: '#fef9c3', text: '#854d0e' },
                                { value: 'DI', full: 'Discrete Input (1xxxx)', bg: '#fce7f3', text: '#9d174d' },
                            ].map(rt => (
                                <Tooltip key={rt.value} title={`${rt.value}: ${rt.full}`} placement="bottom">
                                    <span style={{
                                        fontSize: 11, fontWeight: 700, padding: '2px 7px', borderRadius: 6,
                                        background: rt.bg, color: rt.text, cursor: 'default', whiteSpace: 'nowrap',
                                    }}>{rt.value}</span>
                                </Tooltip>
                            ))}
                        </div>
                        <Radio.Group value={mode} onChange={e => setMode(e.target.value)}
                            buttonStyle="solid" size="small">
                            <Radio.Button value="table">
                                <i className="fas fa-list" style={{ marginRight: 4 }} />{t('modbusSlave.tabs.mappings')} ({rows.length})
                            </Radio.Button>
                            {editMode && (
                                <Radio.Button value="select">
                                    <i className="fas fa-plus-circle" style={{ marginRight: 4 }} />{t('modbusSlave.device.add')}
                                </Radio.Button>
                            )}
                        </Radio.Group>
                        {mode === 'table' && (
                            editMode ? (
                                <Button size="small"
                                    icon={<i className="fas fa-times" />}
                                    onClick={() => exitEditMode(true)}>
                                    {t('modbusSlave.cancel')}
                                </Button>
                            ) : (
                                <Button size="small"
                                    icon={<i className="fas fa-edit" />}
                                    onClick={enterEditMode}
                                    style={{ color: '#7c3aed', borderColor: '#7c3aed' }}>
                                    {t('modbusSlave.device.edit')}
                                </Button>
                            )
                        )}
                        {editMode && (
                            <Button type="primary" size="small" disabled={!dirty} loading={saving}
                                icon={<i className="fas fa-save" />} onClick={handleSave}
                                style={{ background: dirty ? '#7c3aed' : undefined, borderColor: dirty ? '#7c3aed' : undefined }}>
                                {t('modbusSlave.save')}{dirty ? ' *' : ''}
                            </Button>
                        )}
                        {mode === 'table' && editMode && rows.length > 0 && (
                            <Button size="small" danger
                                icon={<i className="fas fa-trash-alt" />}
                                onClick={async () => {
                                    const ok = await confirm({
                                        title: '전체 삭제',
                                        message: `매핑된 ${rows.length}개 포인트를 모두 삭제하시겠습니까? 저장 후 적용됩니다.`,
                                        confirmButtonType: 'danger',
                                    });
                                    if (ok) { setRows([]); setDirty(true); setEditMode(false); }
                                }}>
                                전체 삭제
                            </Button>
                        )}
                        {/* Export JSON */}
                        {mode === 'table' && rows.length > 0 && (
                            <>
                                <Button size="small" icon={<i className="fas fa-file-export" />}
                                    onClick={() => {
                                        const data = JSON.stringify(rows.map(r => ({
                                            register_type: r.register_type,
                                            register_address: r.register_address,
                                            point_id: r.point_id,
                                            point_label: r.point_label,
                                            data_type: r.data_type,
                                            byte_order: r.byte_order,
                                            scale_factor: r.scale_factor,
                                            scale_offset: r.scale_offset,
                                            enabled: r.enabled,
                                        })), null, 2);
                                        const blob = new Blob([data], { type: 'application/json' });
                                        const a = document.createElement('a');
                                        a.href = URL.createObjectURL(blob);
                                        a.download = `mapping_${selectedDeviceId}_${new Date().toISOString().slice(0, 10)}.json`;
                                        a.click(); URL.revokeObjectURL(a.href);
                                    }}>
                                    {t('modbusSlave.exportJson')}
                                </Button>
                                <Button size="small" icon={<i className="fas fa-file-csv" />}
                                    onClick={() => {
                                        const hdr = 'register_type,register_address,point_id,point_name,data_type,byte_order,scale_factor,scale_offset,enabled';
                                        const csv = rows.map(r => [
                                            r.register_type, r.register_address, r.point_id,
                                            `"${(r.point_label || '').replace(/"/g, '""')}"`,
                                            r.data_type, r.byte_order, r.scale_factor, r.scale_offset, r.enabled,
                                        ].join(',')).join('\n');
                                        const blob = new Blob([hdr + '\n' + csv], { type: 'text/csv' });
                                        const a = document.createElement('a');
                                        a.href = URL.createObjectURL(blob);
                                        a.download = `mapping_${selectedDeviceId}_${new Date().toISOString().slice(0, 10)}.csv`;
                                        a.click(); URL.revokeObjectURL(a.href);
                                    }}>
                                    {t('modbusSlave.exportCsv')}
                                </Button>
                            </>
                        )}
                        {/* Import JSON */}
                        {mode === 'table' && editMode && (
                            <label style={{ cursor: 'pointer' }}>
                                <span style={{
                                    display: 'inline-flex', alignItems: 'center', gap: 4,
                                    padding: '3px 8px', border: '1px solid #d9d9d9',
                                    borderRadius: 6, fontSize: 14, background: '#fff',
                                    cursor: 'pointer', userSelect: 'none',
                                }}>
                                    <i className="fas fa-file-import" />{t('modbusSlave.importMapping')}
                                </span>
                                <input type="file" accept=".json" style={{ display: 'none' }}
                                    onChange={e => {
                                        const file = e.target.files?.[0];
                                        if (!file) return;
                                        const reader = new FileReader();
                                        reader.onload = ev => {
                                            try {
                                                const imported = JSON.parse(ev.target?.result as string);
                                                if (Array.isArray(imported)) {
                                                    setRows(prev => {
                                                        const merged = [...prev];
                                                        imported.forEach((item: any) => {
                                                            if (!merged.some(r => r.register_type === item.register_type && r.register_address === item.register_address)) {
                                                                merged.push({ ...item, device_id: selectedDeviceId! });
                                                            }
                                                        });
                                                        return merged;
                                                    });
                                                    setDirty(true);
                                                }
                                            } catch { alert('Invalid JSON file'); }
                                        };
                                        reader.readAsText(file);
                                        e.target.value = '';
                                    }}
                                />
                            </label>
                        )}
                    </div>
                )}
            </div>

            {!selectedDeviceId ? (
                <div style={{ textAlign: 'center', padding: '60px 0', color: '#94a3b8' }}>
                    <i className="fas fa-hand-point-up fa-2x" style={{ display: 'block', marginBottom: 12, opacity: 0.4 }} />
                    {t('modbusSlave.registerMappingPrompt')}
                </div>
            ) : mode === 'select' ? (
                /* ── 포인트 선택 패널 (Ant Table) ─────────────────── */
                <div style={{ border: '1px solid #e2e8f0', borderRadius: 10, overflow: 'hidden' }}>
                    {/* 툴바 — 선택 및 베이스 주소 */}
                    <div style={{ padding: '10px 14px', background: '#f8fafc', borderBottom: '1px solid #e2e8f0', display: 'flex', gap: '12px', alignItems: 'center', flexWrap: 'wrap' }}>
                        <span style={{ fontSize: 13, fontWeight: 600, color: '#475569' }}>
                            <i className="fas fa-check-square" style={{ marginRight: 6, color: '#7c3aed' }} />
                            {selectedPointIds.size}개 선택됨
                        </span>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                            <span style={{ fontSize: 12, color: '#64748b' }}>레지스터:</span>
                            <Select size="small" value={bulkRegType} onChange={setBulkRegType} style={{ width: 72 }}
                                options={REG_TYPES.map(rt => ({ value: rt.value, label: rt.label }))} />
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                            <span style={{ fontSize: 12, color: '#64748b' }}>시작 주소:</span>
                            <InputNumber size="small" min={1} max={65535} value={bulkStartAddr}
                                onChange={v => setBulkStartAddr(v || 1)} style={{ width: 80 }} />
                            <span style={{ fontSize: 11, color: '#94a3b8' }}>(FLOAT32 → +2씩)</span>
                        </div>
                        <Button type="primary" size="small" disabled={selectedPointIds.size === 0}
                            icon={<i className="fas fa-plus" />} onClick={applyBulkAdd}
                            style={{ marginLeft: 'auto', background: '#7c3aed', borderColor: '#7c3aed' }}>
                            {selectedPointIds.size > 0 ? `${selectedPointIds.size}개 매핑 추가` : '선택 후 추가'}
                        </Button>
                    </div>

                    {/* 검색 + 디바이스 필터 */}
                    <div style={{ padding: '8px 14px', borderBottom: '1px solid #e2e8f0', display: 'flex', gap: 8, alignItems: 'center', flexWrap: 'wrap' }}>
                        <Input.Search
                            placeholder="포인트명 / 자료형 검색"
                            value={pointSearch}
                            onChange={e => setPointSearch(e.target.value)}
                            style={{ width: 240 }}
                            allowClear
                            size="small"
                        />
                        <Select
                            size="small"
                            style={{ width: 180 }}
                            value={pointDeviceFilter}
                            onChange={setPointDeviceFilter}
                            options={[{ label: '전체 디바이스', value: 'all' }, ...deviceOptions]}
                            placeholder="디바이스 필터"
                        />
                        <span style={{ fontSize: 12, color: '#94a3b8' }}>
                            {filteredPoints.length}개 (매핑된 {mappedPointIds.size}개 제외)
                        </span>
                    </div>

                    {/* 포인트 테이블 — 동적 높이로 마지막 행 잘렸 해결 */}
                    <Table
                        dataSource={filteredPoints.map(p => ({ ...p, key: p.id }))}
                        columns={pointColumns as any}
                        size="small"
                        pagination={false}
                        scroll={{ x: 'max-content', y: 'calc(100vh - 670px)' }}
                        rowClassName={r => selectedPointIds.has((r as any).id) ? 'ant-table-row-selected' : ''}
                        onRow={r => ({
                            onClick: () => setSelectedPointIds(prev => {
                                const next = new Set(prev);
                                const id = (r as any).id;
                                if (next.has(id)) next.delete(id);
                                else next.add(id);
                                return next;
                            }),
                            style: {
                                cursor: 'pointer',
                                background: selectedPointIds.has((r as any).id) ? '#faf5ff' : undefined,
                            }
                        })}
                        locale={{
                            emptyText: (
                                <div style={{ padding: '32px', textAlign: 'center', color: '#94a3b8' }}>
                                    {allPoints.length === 0
                                        ? '수집 포인트가 없습니다. 먼저 디바이스에 포인트를 등록하세요.'
                                        : '검색/필터 결과가 없습니다.'}
                                </div>
                            )
                        }}
                    />
                </div>
            ) : (
                /* ── 현재 매핑 테이블 ─────────────────────────────────────── */
                <div style={{ border: '1px solid #e2e8f0', borderRadius: 10, overflow: 'hidden' }}>
                    <Table
                        dataSource={rows}
                        columns={columns}
                        rowKey="key"
                        loading={loading}
                        size="small"
                        pagination={false}
                        scroll={{ x: 'max-content', y: 'calc(100vh - 600px)' }}
                        locale={{
                            emptyText: (
                                <div style={{ padding: '40px', color: '#94a3b8', textAlign: 'center' }}>
                                    <i className="fas fa-th fa-2x" style={{ display: 'block', marginBottom: 12, opacity: 0.3 }} />
                                    매핑이 없습니다. "포인트 추가" 탭에서 포인트를 선택하세요.
                                </div>
                            )
                        }}
                    />
                </div>
            )}
        </div>
    );
};

export default MappingEditorTab;
