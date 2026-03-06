// ============================================================================
// wizards/steps/Step1Profile.tsx
// Step 1: Profile 선택 또는 생성 + 데이터 매핑 테이블
// ============================================================================

import React from 'react';
import { Radio, Select, Input, Button } from 'antd';
import { ExportProfile, DataPoint, ExportTargetMapping } from '../../../../api/services/exportGatewayApi';
import { NewProfileData } from '../types';
import DataPointSelector from '../../components/DataPointSelector';

interface Step1Props {
    tl: (key: string, opts?: any) => string;
    profiles: ExportProfile[];
    allPoints: DataPoint[];
    profileMode: 'existing' | 'new';
    setProfileMode: (mode: 'existing' | 'new') => void;
    selectedProfileId: number | null;
    setSelectedProfileId: (id: number | null) => void;
    newProfileData: NewProfileData;
    setNewProfileData: (data: NewProfileData) => void;
    mappings: Partial<ExportTargetMapping>[];
    setMappings: (mappings: Partial<ExportTargetMapping>[]) => void;
    bulkSiteId: string;
    setBulkSiteId: (val: string) => void;
}

const Step1Profile: React.FC<Step1Props> = ({
    tl, profiles, allPoints,
    profileMode, setProfileMode,
    selectedProfileId, setSelectedProfileId,
    newProfileData, setNewProfileData,
    mappings, setMappings,
    bulkSiteId, setBulkSiteId
}) => {
    return (
        <div>
            <Radio.Group
                value={profileMode}
                onChange={e => setProfileMode(e.target.value)}
                style={{ marginBottom: '16px', width: '100%' }}
                buttonStyle="solid"
            >
                <Radio.Button value="existing" style={{ width: '50%', textAlign: 'center' }}>
                    {tl('labels.selectExistingProfile', { ns: 'dataExport' })}
                </Radio.Button>
                <Radio.Button value="new" style={{ width: '50%', textAlign: 'center' }}>
                    {tl('labels.createNewProfile', { ns: 'dataExport' })}
                </Radio.Button>
            </Radio.Group>

            {profileMode === 'existing' ? (
                <div style={{
                    padding: '24px',
                    textAlign: 'left',
                    background: 'white',
                    borderRadius: '12px',
                    height: '100%',
                    display: 'flex',
                    flexDirection: 'column',
                    boxShadow: '0 4px 12px rgba(0,0,0,0.05)',
                    border: '1px solid #f0f0f0'
                }}>
                    <div style={{ marginBottom: '12px', fontWeight: 700, fontSize: '15px', color: '#1a1a1a', display: 'flex', alignItems: 'center' }}>
                        <i className="fas fa-search" style={{ marginRight: '8px', color: '#1890ff' }} />
                        {tl('gwWizard.selectProfileSectionTitle', { ns: 'dataExport' })}
                    </div>
                    <Select
                        showSearch
                        style={{ width: '100%', marginBottom: '20px' }}
                        size="large"
                        placeholder={tl('gwWizard.searchSelectProfile', { ns: 'dataExport' })}
                        optionFilterProp="children"
                        value={selectedProfileId}
                        onChange={setSelectedProfileId}
                        getPopupContainer={triggerNode => triggerNode.parentNode}
                        filterOption={(input, option: any) => (option?.label ?? '').toLowerCase().includes(input.toLowerCase())}
                        options={profiles.map(p => ({ value: p.id, label: `${p.name} (${p.data_points?.length || 0} points)` }))}
                    />

                    {selectedProfileId && (() => {
                        const p = profiles.find(p => p.id === selectedProfileId);
                        return p ? (
                            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                                {/* Profile 정보 */}
                                <div style={{
                                    marginBottom: '20px',
                                    padding: '16px',
                                    background: 'linear-gradient(135deg, #e6f7ff 0%, #f0f5ff 100%)',
                                    border: '1px solid #91d5ff',
                                    borderRadius: '10px',
                                    flexShrink: 0,
                                    boxShadow: '0 2px 4px rgba(24, 144, 255, 0.1)'
                                }}>
                                    <div style={{ fontWeight: 800, color: '#0050b3', display: 'flex', alignItems: 'center', fontSize: '16px' }}>
                                        <i className="fas fa-file-contract" style={{ marginRight: '10px', fontSize: '18px' }} />
                                        {p.name}
                                    </div>
                                    <div style={{ fontSize: '13px', marginTop: '6px', color: '#434343', lineHeight: '1.5' }}>
                                        {p.description || tl('gwWizard.noProfileDesc', { ns: 'dataExport' })}
                                    </div>
                                </div>

                                {/* 매핑 테이블 */}
                                <div style={{
                                    marginTop: '10px',
                                    border: '1px solid #f0f0f0',
                                    borderRadius: '12px',
                                    background: 'white',
                                    padding: '24px',
                                    display: 'flex',
                                    flexDirection: 'column',
                                    overflow: 'hidden',
                                    boxShadow: '0 4px 12px rgba(0,0,0,0.05)'
                                }}>
                                    {/* 테이블 헤더 + 벌크 적용 */}
                                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px', borderBottom: '1px solid #f5f5f5', paddingBottom: '16px' }}>
                                        <div style={{ fontSize: '16px', fontWeight: 800, color: '#1a1a1a', display: 'flex', alignItems: 'center' }}>
                                            <i className="fas fa-exchange-alt" style={{ marginRight: '10px', color: '#1890ff' }} />
                                            {tl('gwWizard.dataMappingTitle', { ns: 'dataExport' })}
                                        </div>
                                        <div style={{ display: 'flex', gap: '12px', alignItems: 'center', background: '#f0f5ff', padding: '8px 16px', borderRadius: '10px', border: '1px solid #adc6ff' }}>
                                            <span style={{ fontSize: '12px', fontWeight: 700, color: '#1d39c4' }}>
                                                {tl('labels.bulkApplySiteId', { ns: 'dataExport' })}
                                            </span>
                                            <Input
                                                size="small"
                                                placeholder="Enter ID"
                                                style={{ width: '80px', borderRadius: '4px' }}
                                                value={bulkSiteId}
                                                onChange={e => setBulkSiteId(e.target.value)}
                                            />
                                            <Button
                                                size="small"
                                                type="primary"
                                                onClick={() => {
                                                    const bid = parseInt(bulkSiteId);
                                                    if (isNaN(bid)) return;
                                                    setMappings(mappings.map(m => ({ ...m, site_id: bid })));
                                                }}
                                            >
                                                {tl('gwWizard.applyBtn', { ns: 'dataExport' })}
                                            </Button>
                                        </div>
                                    </div>

                                    {/* 매핑 행 */}
                                    <div style={{ overflowX: 'auto' }}>
                                        <table style={{ width: '100%', borderCollapse: 'separate', borderSpacing: '0 4px', fontSize: '12px' }}>
                                            <thead>
                                                <tr style={{ textAlign: 'left', color: '#8c8c8c' }}>
                                                    <th style={{ padding: '8px 12px', width: '20%' }}>{tl('labels.internalPointName', { ns: 'dataExport' })}</th>
                                                    <th style={{ padding: '8px 12px', width: '35%' }}>{tl('labels.mappingNameTargetKey', { ns: 'dataExport' })}</th>
                                                    <th style={{ padding: '8px 12px', width: '15%' }}>{tl('labels.siteId', { ns: 'dataExport' })}</th>
                                                    <th style={{ padding: '8px 12px', width: '10%' }}>{tl('labels.scale', { ns: 'dataExport' })}</th>
                                                    <th style={{ padding: '8px 12px', width: '10%' }}>{tl('labels.offset', { ns: 'dataExport' })}</th>
                                                    <th style={{ padding: '8px 12px', width: '10%', textAlign: 'center' }}>{tl('delete', { ns: 'common' })}</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                {mappings.map((m, i) => {
                                                    const point = allPoints.find(pt => pt.id === m.point_id)
                                                        || p.data_points?.find((dp: any) => dp.id === m.point_id);
                                                    let conv: any = {};
                                                    try { conv = typeof m.conversion_config === 'string' ? JSON.parse(m.conversion_config as any) : (m.conversion_config || {}); } catch { }
                                                    return (
                                                        <tr key={i} style={{ background: '#fafafa', borderRadius: '8px' }}>
                                                            <td style={{ padding: '12px', borderRadius: '8px 0 0 8px', fontWeight: 600, color: '#1a1a1a' }}>
                                                                {(point as any)?.name || `Point ${m.point_id}`}
                                                            </td>
                                                            <td style={{ padding: '8px' }}>
                                                                <Input
                                                                    size="small"
                                                                    style={{ borderRadius: '4px' }}
                                                                    value={m.target_field_name}
                                                                    onChange={e => {
                                                                        const next = [...mappings];
                                                                        next[i] = { ...m, target_field_name: e.target.value };
                                                                        setMappings(next);
                                                                    }}
                                                                />
                                                            </td>
                                                            <td style={{ padding: '8px' }}>
                                                                <Input
                                                                    size="small"
                                                                    type="number"
                                                                    style={{ borderRadius: '4px' }}
                                                                    value={m.site_id}
                                                                    onChange={e => {
                                                                        const next = [...mappings];
                                                                        next[i] = { ...m, site_id: parseInt(e.target.value) || 0 };
                                                                        setMappings(next);
                                                                    }}
                                                                />
                                                            </td>
                                                            <td style={{ padding: '8px' }}>
                                                                <Input
                                                                    size="small"
                                                                    type="number"
                                                                    step="0.01"
                                                                    style={{ borderRadius: '4px' }}
                                                                    value={conv.scale ?? 1}
                                                                    onChange={e => {
                                                                        const next = [...mappings];
                                                                        next[i] = { ...m, conversion_config: { ...conv, scale: parseFloat(e.target.value) || 1 } };
                                                                        setMappings(next);
                                                                    }}
                                                                />
                                                            </td>
                                                            <td style={{ padding: '8px' }}>
                                                                <Input
                                                                    size="small"
                                                                    type="number"
                                                                    step="0.01"
                                                                    style={{ borderRadius: '4px' }}
                                                                    value={conv.offset ?? 0}
                                                                    onChange={e => {
                                                                        const next = [...mappings];
                                                                        next[i] = { ...m, conversion_config: { ...conv, offset: parseFloat(e.target.value) || 0 } };
                                                                        setMappings(next);
                                                                    }}
                                                                />
                                                            </td>
                                                            <td style={{ padding: '8px', textAlign: 'center', borderRadius: '0 8px 8px 0' }}>
                                                                <Button
                                                                    size="small"
                                                                    danger
                                                                    type="text"
                                                                    icon={<i className="fas fa-trash-alt" />}
                                                                    onClick={() => setMappings(mappings.filter((_, idx) => idx !== i))}
                                                                />
                                                            </td>
                                                        </tr>
                                                    );
                                                })}
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                        ) : null;
                    })()}
                </div>
            ) : (
                /* 새 Profile 생성 */
                <div style={{ display: 'flex', gap: '24px', height: '400px' }}>
                    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '12px' }}>
                        <Input
                            placeholder="Profile name (e.g. Factory A Standard)"
                            value={newProfileData.name}
                            onChange={e => setNewProfileData({ ...newProfileData, name: e.target.value })}
                        />
                        <Input.TextArea
                            placeholder="Description..."
                            rows={2}
                            value={newProfileData.description}
                            onChange={e => setNewProfileData({ ...newProfileData, description: e.target.value })}
                        />
                        <div style={{ flex: 1, border: '1px solid var(--neutral-200)', borderRadius: '8px', padding: '12px', overflowY: 'auto', background: 'white' }}>
                            <div style={{ fontSize: '12px', color: 'var(--neutral-500)', marginBottom: '8px' }}>
                                Selected Points ({newProfileData.data_points.length})
                            </div>
                            {newProfileData.data_points.map((p, i) => (
                                <div key={i} style={{ display: 'flex', justifyContent: 'space-between', padding: '4px 0', borderBottom: '1px solid #f0f0f0' }}>
                                    <span style={{ fontSize: '12px' }}>{(p as any).name}</span>
                                    <i
                                        className="fas fa-times"
                                        style={{ cursor: 'pointer', color: '#ff4d4f' }}
                                        onClick={() => setNewProfileData({ ...newProfileData, data_points: newProfileData.data_points.filter((_, idx) => idx !== i) })}
                                    />
                                </div>
                            ))}
                        </div>
                    </div>
                    <div style={{ flex: 1.5, height: '100%', overflow: 'hidden' }}>
                        <DataPointSelector
                            selectedPoints={newProfileData.data_points}
                            onSelect={(point) => setNewProfileData({ ...newProfileData, data_points: [...newProfileData.data_points, point] })}
                            onRemove={(pointId) => setNewProfileData({ ...newProfileData, data_points: newProfileData.data_points.filter(p => (p as any).id !== pointId) })}
                            onAddAll={(points) => {
                                const currentIds = new Set(newProfileData.data_points.map((p: any) => p.id));
                                const toAdd = points.filter((p: any) => !currentIds.has(p.id));
                                setNewProfileData({ ...newProfileData, data_points: [...newProfileData.data_points, ...toAdd] });
                            }}
                        />
                    </div>
                </div>
            )}
        </div>
    );
};

export default Step1Profile;
