import React from 'react';
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

    // Helper: Address Label Logic
    const getAddressLabel = () => {
        const selectedP = protocols.find(p => p.id === formData.protocol_id);
        const type = selectedP?.protocol_type || '';
        if (type.includes('MODBUS')) return '주소 (Dec) *';
        if (type.includes('MQTT')) return '토픽 (Topic) *';
        if (type.includes('OPC')) return 'Node ID *';
        if (type.includes('BACNET')) return 'Object:Instance *';
        if (type.includes('ROS')) return 'Topic:Field *';
        return '주소 *';
    };

    // Helper: Address Hint Logic
    const getAddressHint = (address: string | number) => {
        const addr = Number(address);
        if (isNaN(addr)) return '';

        const selectedP = protocols.find(p => p.id === formData.protocol_id);
        if (!selectedP?.protocol_type?.includes('MODBUS')) return '';

        if (addr >= 1 && addr <= 9999) return 'Coil (0x)';
        if (addr >= 10001 && addr <= 19999) return 'Discrete Input (1x)';
        if (addr >= 30001 && addr <= 39999) return 'Input Register (3x)';
        if (addr >= 40001 && addr <= 49999) return 'Holding Register (4x)';
        return '';
    };

    // Actions
    const addPoint = () => {
        setDataPoints([...dataPoints, {
            name: '',
            address: '',
            data_type: 'UINT16',
            access_mode: 'read',
            unit: '',
            scaling_factor: 1,
            scaling_offset: 0,
            description: ''
        }]);
    };

    const updatePoint = (index: number, field: string, value: any) => {
        const updated = [...dataPoints];
        updated[index] = { ...updated[index], [field]: value };
        setDataPoints(updated);
    };

    const removePoint = (index: number) => {
        setDataPoints(dataPoints.filter((_, i) => i !== index));
    };

    const handleAddressChange = (index: number, value: string) => {
        const selectedP = protocols.find(p => p.id === formData.protocol_id);
        const type = selectedP?.protocol_type || '';

        // Modbus numeric restriction
        if (type.includes('MODBUS')) {
            const numericValue = value.replace(/[^0-9]/g, '');
            updatePoint(index, 'address', numericValue);
        } else {
            updatePoint(index, 'address', value);
        }
    };

    // Render Logic: Memory Map
    const renderMemoryMap = () => {
        const selectedP = protocols.find(p => p.id === formData.protocol_id);
        if (!selectedP?.protocol_type?.includes('MODBUS')) return null;

        const sortedPoints = [...dataPoints]
            .filter(p => !isNaN(Number(p.address)) && p.address !== '')
            .sort((a, b) => Number(a.address) - Number(b.address));

        if (sortedPoints.length === 0) return null;

        return (
            <div style={{ padding: '12px', background: '#f1f5f9', borderRadius: '8px', marginTop: '16px' }}>
                <div style={{ fontSize: '12px', fontWeight: 600, color: '#475569', marginBottom: '8px' }}>
                    <i className="fas fa-microchip"></i> 메모리 레지스트리 맵 (Preview)
                </div>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {sortedPoints.map((p, i) => {
                        const size = (p.data_type === 'UINT32' || p.data_type === 'INT32' || p.data_type === 'FLOAT32') ? 2 : 1;
                        return (
                            <div key={i} title={`${p.name} (${p.address}) - ${p.data_type}`} style={{
                                padding: '2px 6px', fontSize: '11px', background: 'white', border: '1px solid #cbd5e1',
                                borderRadius: '4px', cursor: 'help', display: 'flex', alignItems: 'center', gap: '4px'
                            }}>
                                <span style={{ color: '#64748b' }}>#{p.address}</span>
                                <span style={{ fontWeight: 500 }}>{p.name}</span>
                                {size > 1 && <span style={{ fontSize: '9px', background: '#e2e8f0', borderRadius: '2px', padding: '0 2px' }}>{size}W</span>}
                            </div>
                        );
                    })}
                </div>
            </div>
        );
    };

    return (
        <div className="data-points-section">
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                <h4 style={{ margin: 0 }}><i className="fas fa-list-ul"></i> 데이터 포인트 ({dataPoints.length})</h4>
                <div style={{ display: 'flex', gap: '8px' }}>
                    <button className="mgmt-btn mgmt-btn-outline mgmt-btn-sm" onClick={() => setIsBulkModalOpen(true)}>
                        <i className="fas fa-file-excel"></i> 엑셀 일괄 등록
                    </button>
                    <button className="mgmt-btn mgmt-btn-primary mgmt-btn-sm" onClick={addPoint}>
                        <i className="fas fa-plus"></i> 포인트 추가
                    </button>
                </div>
            </div>
            <div className="table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th style={{ width: '200px' }}>이름 *</th>
                            <th style={{ width: '140px', textAlign: 'center' }}>{getAddressLabel()} *</th>
                            <th style={{ width: '160px', textAlign: 'center' }}>타입 *</th>
                            <th style={{ width: '180px', textAlign: 'center' }}>권한</th>
                            <th>설명</th>
                            <th style={{ width: '110px', textAlign: 'center' }}>단위</th>
                            <th style={{ width: '100px', textAlign: 'center' }}>스케일</th>
                            <th style={{ width: '100px', textAlign: 'center' }}>오프셋</th>
                            <th style={{ width: '50px' }}></th>
                        </tr>
                    </thead>
                    <tbody>
                        {dataPoints.map((point, idx) => {
                            const hasOverlap = overlapIndices.has(idx);
                            return (
                                <tr key={idx} className={hasOverlap ? 'row-warning' : ''}>
                                    <td>
                                        <input
                                            className="mgmt-input sm"
                                            value={point.name || ''}
                                            onChange={e => updatePoint(idx, 'name', e.target.value)}
                                            placeholder="Name"
                                        />
                                    </td>
                                    <td>
                                        <div style={{ position: 'relative' }}>
                                            <input
                                                className={`mgmt-input sm ${hasOverlap ? 'error' : ''}`}
                                                value={point.address || ''}
                                                onChange={e => handleAddressChange(idx, e.target.value)}
                                                placeholder="0"
                                                style={{ textAlign: 'center' }}
                                            />
                                            {getAddressHint(point.address || '') && (
                                                <div style={{ fontSize: '10px', color: hasOverlap ? 'var(--danger-500)' : 'var(--primary-500)', marginTop: '2px', position: 'absolute' }}>
                                                    {getAddressHint(point.address || '')}
                                                </div>
                                            )}
                                            {hasOverlap && (
                                                <div style={{ fontSize: '10px', color: 'var(--danger-500)', position: 'absolute', top: '-15px', right: 0 }}>
                                                    <i className="fas fa-exclamation-triangle"></i> 중복
                                                </div>
                                            )}
                                        </div>
                                    </td>
                                    <td>
                                        <select
                                            className="mgmt-select sm"
                                            value={point.data_type || 'UINT16'}
                                            onChange={e => updatePoint(idx, 'data_type', e.target.value)}
                                            style={{ textAlign: 'center' }}
                                        >
                                            <option value="UINT16">UINT16</option>
                                            <option value="INT16">INT16</option>
                                            <option value="UINT32">UINT32</option>
                                            <option value="INT32">INT32</option>
                                            <option value="FLOAT32">FLOAT32</option>
                                            <option value="BOOLEAN">BOOLEAN</option>
                                        </select>
                                    </td>
                                    <td>
                                        <select
                                            className="mgmt-select sm"
                                            value={point.access_mode || 'read'}
                                            onChange={e => updatePoint(idx, 'access_mode', e.target.value)}
                                            style={{ textAlign: 'center' }}
                                        >
                                            <option value="read">Read Only</option>
                                            <option value="write">Write Only</option>
                                            <option value="read_write">Read/Write</option>
                                        </select>
                                    </td>
                                    <td>
                                        <input
                                            className="mgmt-input sm"
                                            value={point.description || ''}
                                            onChange={e => updatePoint(idx, 'description', e.target.value)}
                                            placeholder="Description"
                                        />
                                    </td>
                                    <td>
                                        <input
                                            className="mgmt-input sm"
                                            value={point.unit || ''}
                                            onChange={e => updatePoint(idx, 'unit', e.target.value)}
                                            placeholder="Unit"
                                            style={{ textAlign: 'center' }}
                                        />
                                    </td>
                                    <td>
                                        <input
                                            type="text"
                                            className="mgmt-input sm"
                                            value={point.scaling_factor ?? 1}
                                            onChange={e => updatePoint(idx, 'scaling_factor', e.target.value)}
                                            placeholder="1.0"
                                            style={{ textAlign: 'center' }}
                                        />
                                    </td>
                                    <td>
                                        <input
                                            type="text"
                                            className="mgmt-input sm"
                                            value={point.scaling_offset ?? 0}
                                            onChange={e => updatePoint(idx, 'scaling_offset', e.target.value)}
                                            placeholder="0.0"
                                            style={{ textAlign: 'center' }}
                                        />
                                    </td>
                                    <td>
                                        <button className="mgmt-btn-icon mgmt-btn-error" onClick={() => removePoint(idx)}>
                                            <i className="fas fa-trash"></i>
                                        </button>
                                    </td>
                                </tr>
                            );
                        })}
                    </tbody>
                </table>
            </div>
            {renderMemoryMap()}
        </div>
    );
};

export default DataPointsStep;
