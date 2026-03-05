// ============================================================================
// BitSplitModal.tsx — Modbus 비트 분할 자동 생성 전용 모달 (별도 파일)
// ============================================================================
import React from 'react';
import { createPortal } from 'react-dom';
import { useTranslation } from 'react-i18next';

export interface BitSplitConfig {
    address: string;
    addressEnd: string;
    useAddressRange: boolean;
    namePrefix: string;
    bitStart: number;
    bitEnd: number;
    accessMode: string;
    descPrefix: string;
    mode: 'individual' | 'group' | 'custom';
    numValues: number;
    registerBits: 16 | 32;
    customRanges: string;
}

interface BitRangeParsed {
    start: number;
    end: number;
    error?: string;
}

export function parseBitRanges(raw: string, maxBits: number): BitRangeParsed[] {
    if (!raw.trim()) return [];
    return raw.split(',').map(seg => {
        const s = seg.trim();
        const single = /^\d+$/.exec(s);
        if (single) {
            const n = parseInt(s);
            if (n < 0 || n >= maxBits) return { start: n, end: n, error: `범위 초과: ${n} (0~${maxBits - 1})` };
            return { start: n, end: n };
        }
        const m = /^(\d+)-(\d+)$/.exec(s);
        if (!m) return { start: 0, end: 0, error: `잘못된 형식: "${s}"` };
        const st = parseInt(m[1]), en = parseInt(m[2]);
        if (st < 0 || en >= maxBits) return { start: st, end: en, error: `범위 초과: ${st}-${en} (0~${maxBits - 1})` };
        if (st > en) return { start: st, end: en, error: `시작>끝: ${st}-${en}` };
        return { start: st, end: en };
    });
}

interface BitSplitModalProps {
    isOpen: boolean;
    config: BitSplitConfig;
    onChange: (patch: Partial<BitSplitConfig>) => void;
    onGenerate: () => void;
    onClose: () => void;
}

export default function BitSplitModal({ isOpen, config, onChange, onGenerate, onClose }: BitSplitModalProps) {
    const { t } = useTranslation(['devices', 'common']);

    if (!isOpen) return null;

    const parsed = parseBitRanges(config.customRanges, config.registerBits);
    const hasError = parsed.some(r => r.error);
    const isBool = config.mode === 'individual';

    const QUICK_FILLS = [
        { label: t('bulk.quickFill2', { ns: 'devices' }), n: 2 },
        { label: t('bulk.quickFill4', { ns: 'devices' }), n: 4 },
        { label: t('bulk.quickFill8', { ns: 'devices' }), n: 8 },
        { label: t('bulk.quickFillBitwise', { ns: 'devices' }), n: config.registerBits },
    ];

    const addrInfo = (() => {
        if (!config.useAddressRange || !config.address || !config.addressEnd) return null;
        const s = parseInt(config.address, 10), e = parseInt(config.addressEnd, 10);
        if (isNaN(s) || isNaN(e)) return null;
        if (e < s) return <span style={{ fontSize: 12, color: '#dc2626' }}>⚠ 끝 주소가 시작보다 크거나 같아야 합니다</span>;
        return <span style={{ fontSize: 12, color: '#0f766e' }}>✅ {e - s + 1}개 주소 생성 예정</span>;
    })();

    // 프리뷰 아이템 (오버플로 방지: 개행 처리)
    const previewItems = (() => {
        if (parsed.length === 0 || hasError) return null;
        return isBool
            ? parsed.flatMap(r =>
                r.start === r.end ? [`Bit${r.start}`] : [`Bit${r.start}~${r.end}`])
            : parsed.map((r, i) =>
                `V${i + 1}(bit${r.start}~${r.end}→0~${(1 << (r.end - r.start + 1)) - 1})`);
    })();

    return createPortal(
        <div
            style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.55)', zIndex: 10500, display: 'flex', alignItems: 'center', justifyContent: 'center' }}
            onClick={onClose}
        >
            <div
                style={{ background: 'white', borderRadius: 16, width: 640, maxHeight: '90vh', overflow: 'auto', boxShadow: '0 24px 64px rgba(0,0,0,0.3)' }}
                onClick={e => e.stopPropagation()}
            >
                {/* ── 헤더 ── */}
                <div style={{ padding: '18px 24px', background: '#eff6ff', borderBottom: '1px solid #bfdbfe', borderRadius: '16px 16px 0 0', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <span style={{ fontSize: 16, fontWeight: 700, color: '#1d4ed8' }}>
                        <i className="fas fa-layer-group" style={{ marginRight: 8 }} />
                        비트 분할 자동 생성
                    </span>
                    <button onClick={onClose} style={{ background: 'none', border: 'none', cursor: 'pointer', color: '#64748b', fontSize: 18 }}>
                        <i className="fas fa-times" />
                    </button>
                </div>

                {/* ── 본문 ── */}
                <div style={{ padding: '24px', display: 'flex', flexDirection: 'column', gap: 18 }}>

                    {/* 기준 주소 */}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <span style={{ fontSize: 13, fontWeight: 600, color: '#334155' }}>기준 주소 (Address) *</span>
                            <label style={{ fontSize: 12, color: '#64748b', display: 'flex', alignItems: 'center', gap: 4, cursor: 'pointer' }}>
                                <input type="checkbox" checked={config.useAddressRange}
                                    onChange={e => onChange({ useAddressRange: e.target.checked })}
                                    style={{ cursor: 'pointer' }} />
                                주소 범위
                            </label>
                        </div>
                        {config.useAddressRange ? (
                            <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                                <input type="text" value={config.address}
                                    onChange={e => onChange({ address: e.target.value })}
                                    placeholder="시작 예: 40001"
                                    style={{ flex: 1, padding: '8px 12px', border: '1px solid #cbd5e1', borderRadius: 8, fontSize: 13, fontFamily: 'monospace' }} />
                                <span style={{ fontSize: 13, color: '#94a3b8' }}>~</span>
                                <input type="text" value={config.addressEnd}
                                    onChange={e => onChange({ addressEnd: e.target.value })}
                                    placeholder="끝 예: 40010"
                                    style={{ flex: 1, padding: '8px 12px', border: '1px solid #cbd5e1', borderRadius: 8, fontSize: 13, fontFamily: 'monospace' }} />
                            </div>
                        ) : (
                            <input type="text" value={config.address}
                                onChange={e => onChange({ address: e.target.value })}
                                placeholder="예: 40001"
                                style={{ width: '100%', padding: '8px 12px', border: '1px solid #cbd5e1', borderRadius: 8, fontSize: 13, fontFamily: 'monospace', boxSizing: 'border-box' }} />
                        )}
                        {addrInfo}
                    </div>

                    {/* 이름·권한·설명 접두사 */}
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 12 }}>
                        <label style={{ fontSize: 12, fontWeight: 600, color: '#475569' }}>
                            이름 접두사
                            <input type="text" value={config.namePrefix}
                                onChange={e => onChange({ namePrefix: e.target.value })}
                                placeholder="비우면 주소 사용"
                                style={{ display: 'block', width: '100%', marginTop: 6, padding: '7px 10px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12, boxSizing: 'border-box' }} />
                        </label>
                        <label style={{ fontSize: 12, fontWeight: 600, color: '#475569' }}>
                            Access Mode
                            <select value={config.accessMode}
                                onChange={e => onChange({ accessMode: e.target.value })}
                                style={{ display: 'block', width: '100%', marginTop: 6, padding: '7px 10px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12 }}>
                                <option value="read">read</option>
                                <option value="write">write</option>
                                <option value="read_write">read_write</option>
                            </select>
                        </label>
                        <label style={{ fontSize: 12, fontWeight: 600, color: '#475569' }}>
                            설명 접두사
                            <input type="text" value={config.descPrefix}
                                onChange={e => onChange({ descPrefix: e.target.value })}
                                placeholder="예: FC03 40001"
                                style={{ display: 'block', width: '100%', marginTop: 6, padding: '7px 10px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12, boxSizing: 'border-box' }} />
                        </label>
                    </div>

                    {/* 레지스터 크기 */}
                    <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
                        <span style={{ fontSize: 12, fontWeight: 600, color: '#64748b' }}>레지스터 크기:</span>
                        {([16, 32] as const).map(b => (
                            <button key={b} onClick={() => onChange({ registerBits: b })}
                                style={{
                                    padding: '5px 16px', border: '1.5px solid', borderRadius: 6, fontSize: 12, fontWeight: 700, cursor: 'pointer',
                                    borderColor: config.registerBits === b ? '#0f766e' : '#cbd5e1',
                                    background: config.registerBits === b ? '#f0fdfa' : '#f8fafc',
                                    color: config.registerBits === b ? '#0f766e' : '#64748b'
                                }}>
                                {b}bit (0~{b - 1})
                            </button>
                        ))}
                    </div>

                    {/* 비트 범위 입력 박스 */}
                    <div style={{ background: '#f8fafc', borderRadius: 10, border: '1px solid #e2e8f0', padding: '16px' }}>
                        <label style={{ fontSize: 13, fontWeight: 600, color: '#334155' }}>
                            비트 범위
                            <span style={{ fontWeight: 400, color: '#94a3b8', fontSize: 11, marginLeft: 6 }}>콤마(,)로 구분 · 대시(-)로 범위</span>
                            <input type="text"
                                value={config.customRanges}
                                onChange={e => onChange({ customRanges: e.target.value })}
                                placeholder="예: 0-3, 4-7, 8-11, 12-15"
                                style={{
                                    display: 'block', width: '100%', marginTop: 8, padding: '9px 12px',
                                    border: `1.5px solid ${hasError ? '#fca5a5' : '#cbd5e1'}`,
                                    borderRadius: 8, fontSize: 14, fontFamily: 'monospace',
                                    background: hasError ? '#fff5f5' : 'white',
                                    boxSizing: 'border-box'
                                }} />
                        </label>

                        {/* 빠른 채우기 */}
                        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginTop: 10 }}>
                            {QUICK_FILLS.map(q => {
                                const sz = Math.floor(config.registerBits / q.n);
                                const val = q.n === config.registerBits
                                    ? Array.from({ length: q.n }, (_, i) => `${i}-${i}`).join(', ')
                                    : Array.from({ length: q.n }, (_, i) => `${i * sz}-${(i + 1) * sz - 1}`).join(', ');
                                return (
                                    <button key={q.label}
                                        onClick={() => onChange({ customRanges: val })}
                                        style={{ padding: '4px 12px', fontSize: 11, fontWeight: 600, border: '1px solid #cbd5e1', borderRadius: 5, background: '#f1f5f9', color: '#475569', cursor: 'pointer' }}>
                                        {q.label}
                                    </button>
                                );
                            })}
                        </div>

                        {/* 값 타입 토글 */}
                        <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginTop: 12 }}>
                            <span style={{ fontSize: 12, fontWeight: 600, color: '#64748b' }}>
                                {t('bulk.valueTypeLabel', { ns: 'devices' })}:
                            </span>
                            <button onClick={() => onChange({ mode: 'individual' })}
                                style={{
                                    padding: '4px 14px', border: '1.5px solid', borderRadius: 6, fontSize: 12, fontWeight: 700, cursor: 'pointer',
                                    borderColor: isBool ? '#1d4ed8' : '#e2e8f0',
                                    background: isBool ? '#eff6ff' : '#f8fafc',
                                    color: isBool ? '#1d4ed8' : '#94a3b8'
                                }}>
                                boolean (0/1)
                            </button>
                            <button onClick={() => onChange({ mode: 'group' })}
                                style={{
                                    padding: '4px 14px', border: '1.5px solid', borderRadius: 6, fontSize: 12, fontWeight: 700, cursor: 'pointer',
                                    borderColor: !isBool ? '#7c3aed' : '#e2e8f0',
                                    background: !isBool ? '#f5f3ff' : '#f8fafc',
                                    color: !isBool ? '#7c3aed' : '#94a3b8'
                                }}>
                                number (0~N)
                            </button>
                            <span style={{ fontSize: 11, color: '#94a3b8' }}>
                                {isBool
                                    ? t('bulk.valueTypeBoolDesc', { ns: 'devices' })
                                    : t('bulk.valueTypeNumDesc', { ns: 'devices' })}
                            </span>
                        </div>

                        {/* 미리보기 */}
                        <div style={{
                            marginTop: 12, fontSize: 12, padding: '10px 12px', borderRadius: 8,
                            background: hasError ? '#fef2f2' : parsed.length === 0 ? '#f8fafc' : '#f0fdf4',
                            color: hasError ? '#dc2626' : '#166534',
                            borderLeft: `3px solid ${hasError ? '#fca5a5' : parsed.length === 0 ? '#e2e8f0' : '#86efac'}`
                        }}>
                            {parsed.length === 0
                                ? <span style={{ color: '#94a3b8' }}>{t('bulk.previewEnterRange', { ns: 'devices' })}</span>
                                : hasError
                                    ? parsed.filter(r => r.error).map((r, i) => <div key={i}>⚠ {r.error}</div>)
                                    : <>
                                        <div style={{ fontWeight: 600, marginBottom: 6 }}>
                                            ✅ {isBool
                                                ? t('bulk.previewBoolPoints', { ns: 'devices', count: parsed.reduce((s, r) => s + r.end - r.start + 1, 0) })
                                                : t('bulk.previewNumPoints', { ns: 'devices', count: parsed.length })}
                                        </div>
                                        {/* 아이템을 줄 바꿈하여 오버플로 방지 */}
                                        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px 10px' }}>
                                            {previewItems?.map((txt, i) => (
                                                <span key={i} style={{ background: 'white', padding: '2px 8px', borderRadius: 4, fontFamily: 'monospace', fontSize: 11, color: '#374151', border: '1px solid #d1fae5' }}>
                                                    {txt}
                                                </span>
                                            ))}
                                        </div>
                                    </>}
                        </div>
                    </div>

                    {/* 자동 생성 버튼 */}
                    <button onClick={onGenerate}
                        style={{
                            padding: '12px 0', width: '100%',
                            background: isBool ? '#1d4ed8' : '#7c3aed',
                            color: 'white', border: 'none', borderRadius: 9,
                            fontWeight: 700, fontSize: 15, cursor: 'pointer',
                            boxShadow: '0 4px 12px rgba(0,0,0,0.15)'
                        }}>
                        <i className="fas fa-magic" style={{ marginRight: 8 }} />
                        자동 생성
                    </button>
                </div>
            </div>
        </div>,
        document.body
    );
}
