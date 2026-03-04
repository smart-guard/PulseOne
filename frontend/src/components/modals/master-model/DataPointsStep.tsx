import React from 'react';
import { useTranslation } from 'react-i18next';
import { TemplatePoint, Protocol, DeviceTemplate } from '../../../types/manufacturing';

interface DataPointsStepProps {
    dataPoints: Partial<TemplatePoint>[];
    setDataPoints: (points: Partial<TemplatePoint>[]) => void;
    formData: Partial<DeviceTemplate>;
    protocols: Protocol[];
    setIsBulkModalOpen: (isOpen: boolean) => void;
    overlapIndices: Set<number>;
}

const DataPointsStep: React.FC<DataPointsStepProps> = ({
    dataPoints,
    setDataPoints,
    formData,
    protocols,
    setIsBulkModalOpen,
    overlapIndices
}) => {
    const { t } = useTranslation(['deviceTemplates', 'common']);

    const selectedProtocol = protocols.find(p => p.id === formData.protocol_id);
    const isModbus = selectedProtocol?.protocol_type?.includes('MODBUS') ?? false;

    const removePoint = (index: number) => {
        setDataPoints(dataPoints.filter((_, i) => i !== index));
    };

    // bit 표시 문자열
    const getBitDisplay = (point: Partial<TemplatePoint>): string => {
        const pp = point.protocol_params as any;
        if (!pp) return '';
        if (pp.bit_start !== undefined && pp.bit_end !== undefined) return `bit ${pp.bit_start}~${pp.bit_end}`;
        if (pp.bit_index !== undefined) return `bit ${pp.bit_index}`;
        return '';
    };

    // address hint
    const getAddressHint = (address: string | number) => {
        const addr = Number(address);
        if (isNaN(addr) || !isModbus) return '';
        if (addr >= 1 && addr <= 9999) return 'Coil';
        if (addr >= 10001 && addr <= 19999) return 'DI';
        if (addr >= 30001 && addr <= 39999) return 'IR';
        if (addr >= 40001 && addr <= 49999) return 'HR';
        return '';
    };

    return (
        <div className="data-points-section">
            {/* 헤더 */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                <h4 style={{ margin: 0 }}>
                    <i className="fas fa-list-ul"></i> Data Points ({dataPoints.length})
                </h4>
                <div style={{ display: 'flex', gap: '8px' }}>
                    <button
                        className="mgmt-btn mgmt-btn-primary mgmt-btn-sm"
                        onClick={() => setIsBulkModalOpen(true)}
                    >
                        <i className="fas fa-plus"></i> 포인트 추가 / 대량 등록
                    </button>
                </div>
            </div>

            {/* 포인트 목록 */}
            {dataPoints.length === 0 ? (
                <div style={{
                    textAlign: 'center', padding: '48px 24px',
                    background: '#f8fafc', borderRadius: '8px',
                    border: '2px dashed #e2e8f0', color: '#94a3b8'
                }}>
                    <i className="fas fa-database" style={{ fontSize: '32px', marginBottom: '12px', display: 'block' }}></i>
                    <div style={{ fontWeight: 600, marginBottom: '6px' }}>데이터 포인트가 없습니다</div>
                    <div style={{ fontSize: '13px' }}>위 버튼을 클릭해 포인트를 추가하거나 비트 분할로 대량 생성할 수 있습니다</div>
                </div>
            ) : (
                <div className="table-container" style={{ overflowX: 'auto' }}>
                    <table className="mgmt-table" style={{ minWidth: '800px' }}>
                        <thead>
                            <tr>
                                <th style={{ width: '200px' }}>Name</th>
                                <th style={{ width: '120px', textAlign: 'center' }}>Address</th>
                                {isModbus && <th style={{ width: '100px', textAlign: 'center' }}>Bit</th>}
                                <th style={{ width: '130px', textAlign: 'center' }}>타입</th>
                                <th style={{ width: '140px', textAlign: 'center' }}>권한</th>
                                <th>설명</th>
                                <th style={{ width: '70px', textAlign: 'center' }}>단위</th>
                                <th style={{ width: '50px' }}></th>
                            </tr>
                        </thead>
                        <tbody>
                            {dataPoints.map((point, idx) => {
                                const hasOverlap = overlapIndices.has(idx);
                                const bitDisplay = getBitDisplay(point);
                                const hint = getAddressHint(point.address || '');
                                return (
                                    <tr key={idx} className={hasOverlap ? 'row-warning' : ''}>
                                        <td>
                                            <span style={{ fontWeight: 500, color: '#1e293b' }}>{point.name || '-'}</span>
                                        </td>
                                        <td style={{ textAlign: 'center' }}>
                                            <span style={{
                                                fontFamily: 'monospace', fontWeight: 600,
                                                color: hasOverlap ? 'var(--danger-500)' : '#334155'
                                            }}>
                                                {point.address || '-'}
                                            </span>
                                            {hint && (
                                                <span style={{ fontSize: '10px', color: '#94a3b8', marginLeft: 4 }}>
                                                    ({hint})
                                                </span>
                                            )}
                                            {hasOverlap && (
                                                <span style={{ fontSize: '10px', color: 'var(--danger-500)', marginLeft: 4 }}>
                                                    <i className="fas fa-exclamation-triangle"></i> 중복
                                                </span>
                                            )}
                                        </td>
                                        {isModbus && (
                                            <td style={{ textAlign: 'center' }}>
                                                {bitDisplay ? (
                                                    <span style={{
                                                        fontSize: '11px', padding: '2px 6px',
                                                        background: '#dbeafe', color: '#1d4ed8',
                                                        borderRadius: '4px', fontFamily: 'monospace'
                                                    }}>
                                                        {bitDisplay}
                                                    </span>
                                                ) : (
                                                    <span style={{ color: '#cbd5e1', fontSize: '12px' }}>-</span>
                                                )}
                                            </td>
                                        )}
                                        <td style={{ textAlign: 'center' }}>
                                            <span style={{
                                                fontSize: '11px', padding: '2px 8px',
                                                background: '#f1f5f9', borderRadius: '4px',
                                                fontFamily: 'monospace', color: '#475569'
                                            }}>
                                                {point.data_type || '-'}
                                            </span>
                                        </td>
                                        <td style={{ textAlign: 'center', fontSize: '12px', color: '#64748b' }}>
                                            {point.access_mode === 'read' ? t('labels.readOnly', { ns: 'deviceTemplates' })
                                                : point.access_mode === 'write' ? t('labels.writeOnly', { ns: 'deviceTemplates' })
                                                    : point.access_mode === 'read_write' ? t('labels.readwrite', { ns: 'deviceTemplates' })
                                                        : point.access_mode || '-'}
                                        </td>
                                        <td style={{ fontSize: '12px', color: '#64748b' }}>
                                            {point.description || ''}
                                        </td>
                                        <td style={{ textAlign: 'center', fontSize: '12px', color: '#64748b' }}>
                                            {point.unit || ''}
                                        </td>
                                        <td>
                                            <button
                                                className="mgmt-btn-icon mgmt-btn-error"
                                                onClick={() => removePoint(idx)}
                                                title="삭제"
                                            >
                                                <i className="fas fa-trash"></i>
                                            </button>
                                        </td>
                                    </tr>
                                );
                            })}
                        </tbody>
                    </table>
                </div>
            )}
        </div>
    );
};

export default DataPointsStep;
