// ============================================================================
// wizards/steps/Step4Schedule.tsx
// Step 4: 전송 스케줄 설정 (INTERVAL / EVENT 모드)
// ============================================================================

import React from 'react';
import { Form, Radio, Input, Button, Select, Tag, Space } from 'antd';
import { ExportSchedule } from '../../../../api/services/exportGatewayApi';
import { ScheduleFormData } from '../types';

const CRON_PRESETS = [
    { label: 'Every 1 min', value: '*/1 * * * *' },
    { label: 'Every 5 min', value: '*/5 * * * *' },
    { label: 'Every 10 min', value: '*/10 * * * *' },
    { label: 'Every 30 min', value: '*/30 * * * *' },
    { label: 'Every 1 hour', value: '0 * * * *' },
    { label: 'Every 12 hours', value: '0 */12 * * *' },
    { label: 'Daily midnight', value: '0 0 * * *' },
    { label: 'Every Sunday', value: '0 0 * * 0' }
];

interface Step4Props {
    tl: (key: string, opts?: any) => string;
    transmissionMode: 'INTERVAL' | 'EVENT';
    setTransmissionMode: (mode: 'INTERVAL' | 'EVENT') => void;
    scheduleMode: 'new' | 'existing';
    setScheduleMode: (mode: 'new' | 'existing') => void;
    scheduleData: ScheduleFormData;
    setScheduleData: (data: ScheduleFormData) => void;
    selectedScheduleId: number | null;
    setSelectedScheduleId: (id: number | null) => void;
    availableSchedules: ExportSchedule[];
}

const Step4Schedule: React.FC<Step4Props> = ({
    tl,
    transmissionMode, setTransmissionMode,
    scheduleMode, setScheduleMode,
    scheduleData, setScheduleData,
    selectedScheduleId, setSelectedScheduleId,
    availableSchedules
}) => {
    return (
        <div style={{ minHeight: '380px', display: 'flex', flexDirection: 'column' }}>
            {/* 모드 안내 배너 */}
            <div style={{ marginBottom: '20px', padding: '16px', background: '#f8faff', borderRadius: '12px', border: '1px solid #e6f7ff' }}>
                <div style={{ fontWeight: 700, color: '#0050b3', marginBottom: '4px', fontSize: '14px' }}>
                    <i className="fas fa-info-circle" style={{ marginRight: '8px' }} />
                    Transmission Mode Guide
                </div>
                <div style={{ fontSize: '12.5px', color: '#434343', lineHeight: '1.6' }}>
                    {transmissionMode === 'INTERVAL' ? (
                        <><strong>{tl('labels.scheduled', { ns: 'dataExport' })}</strong> Periodically transmits data in batches according to the Cron schedule. Traffic is predictable and suitable for report generation.</>
                    ) : (
                        <><strong>{tl('labels.eventbased', { ns: 'dataExport' })}</strong> Transmits immediately when collected data changes or an event exceeds a threshold. Used for real-time alarms and immediate response scenarios.</>
                    )}
                </div>
            </div>

            <Form layout="vertical">
                {/* 전송 모드 선택 */}
                <Form.Item label={<span style={{ fontWeight: 600 }}>{tl('labels.selectTransmissionMode', { ns: 'dataExport' })}</span>}>
                    <Radio.Group
                        value={transmissionMode}
                        onChange={e => setTransmissionMode(e.target.value)}
                        optionType="button"
                        buttonStyle="solid"
                        style={{ width: '100%' }}
                    >
                        <Radio.Button value="INTERVAL" style={{ width: '50%', textAlign: 'center' }}>
                            {tl('labels.scheduledTransmission', { ns: 'dataExport' })}
                        </Radio.Button>
                        <Radio.Button value="EVENT" style={{ width: '50%', textAlign: 'center' }}>
                            {tl('labels.eventbasedRealtime', { ns: 'dataExport' })}
                        </Radio.Button>
                    </Radio.Group>
                </Form.Item>

                {transmissionMode === 'INTERVAL' ? (
                    <div style={{ animation: 'fadeIn 0.3s ease-in-out' }}>
                        {/* 스케줄 모드: 새로 만들기 / 기존 것 사용 */}
                        <Form.Item label={<span style={{ fontWeight: 600 }}>{tl('labels.scheduleConfiguration', { ns: 'dataExport' })}</span>}>
                            <Radio.Group
                                value={scheduleMode}
                                onChange={e => {
                                    setScheduleMode(e.target.value);
                                    if (e.target.value === 'existing' && availableSchedules.length > 0 && !selectedScheduleId) {
                                        setSelectedScheduleId(availableSchedules[0].id);
                                    }
                                }}
                                optionType="button"
                                buttonStyle="solid"
                                style={{ width: '100%', marginBottom: '16px' }}
                            >
                                <Radio.Button value="new" style={{ width: '50%', textAlign: 'center' }}>
                                    {tl('labels.createNew', { ns: 'dataExport' })}
                                </Radio.Button>
                                <Radio.Button value="existing" style={{ width: '50%', textAlign: 'center' }}>
                                    {tl('labels.applyExistingSchedule', { ns: 'dataExport' })}
                                </Radio.Button>
                            </Radio.Group>
                        </Form.Item>

                        {scheduleMode === 'new' ? (
                            <>
                                {/* 스케줄 이름 */}
                                <Form.Item label={<span style={{ fontWeight: 600 }}>{tl('labels.scheduleName', { ns: 'dataExport' })}</span>} required>
                                    <Input
                                        placeholder="e.g. Every 5 minutes batch"
                                        value={scheduleData.schedule_name}
                                        onChange={e => setScheduleData({ ...scheduleData, schedule_name: e.target.value })}
                                    />
                                </Form.Item>

                                {/* Cron 표현식 */}
                                <Form.Item
                                    label={<span style={{ fontWeight: 600 }}>{tl('labels.cronExpression', { ns: 'dataExport' })}</span>}
                                    required
                                    extra={<span style={{ fontSize: '11px' }}>* Standard Cron format supported (min hr day month weekday)</span>}
                                >
                                    <Input
                                        placeholder="*/5 * * * *"
                                        value={scheduleData.cron_expression}
                                        onChange={e => setScheduleData({ ...scheduleData, cron_expression: e.target.value })}
                                        style={{ fontFamily: 'monospace', fontSize: '15px' }}
                                    />
                                    {/* 빠른 프리셋 */}
                                    <div style={{ marginTop: '12px' }}>
                                        <div style={{ fontSize: '11px', color: '#8c8c8c', marginBottom: '8px', fontWeight: 600 }}>
                                            {tl('labels.quickPreset', { ns: 'dataExport' })}
                                        </div>
                                        <Space wrap>
                                            {CRON_PRESETS.map(preset => (
                                                <Button
                                                    key={preset.value}
                                                    size="small"
                                                    type={scheduleData.cron_expression === preset.value ? 'primary' : 'default'}
                                                    onClick={() => setScheduleData({ ...scheduleData, cron_expression: preset.value, schedule_name: preset.label })}
                                                    style={{ fontSize: '11px', borderRadius: '4px' }}
                                                >
                                                    {preset.label}
                                                </Button>
                                            ))}
                                        </Space>
                                    </div>
                                </Form.Item>
                            </>
                        ) : (
                            /* 기존 스케줄 선택 */
                            <Form.Item label={<span style={{ fontWeight: 600 }}>{tl('labels.selectPredefinedSchedule', { ns: 'dataExport' })}</span>} required>
                                {availableSchedules.length > 0 ? (
                                    <Select
                                        placeholder="Select a schedule"
                                        value={selectedScheduleId}
                                        onChange={v => setSelectedScheduleId(v)}
                                        style={{ width: '100%' }}
                                    >
                                        {availableSchedules.map(s => (
                                            <Select.Option key={s.id} value={s.id}>
                                                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                                    <span>{s.schedule_name}</span>
                                                    <Tag color="blue" style={{ margin: 0 }}>{s.cron_expression}</Tag>
                                                </div>
                                            </Select.Option>
                                        ))}
                                    </Select>
                                ) : (
                                    <div style={{ padding: '16px', background: '#fffbe6', border: '1px solid #ffe58f', borderRadius: '8px', color: '#d48806', fontSize: '13px' }}>
                                        <i className="fas fa-exclamation-triangle" style={{ marginRight: '8px' }}></i>
                                        No existing schedules registered for this tenant. Please use <strong>{tl('labels.createNew', { ns: 'dataExport' })}</strong> mode.
                                    </div>
                                )}
                                <div style={{ fontSize: '12px', color: '#8c8c8c', marginTop: '8px' }}>
                                    * 선택한 기존 스케줄의 주기(Cron) 설정을 바탕으로 새로운 스케줄을 생성합니다.
                                </div>
                            </Form.Item>
                        )}
                    </div>
                ) : (
                    /* EVENT 모드 안내 */
                    <div style={{
                        padding: '40px 20px',
                        textAlign: 'center',
                        background: '#fafafa',
                        borderRadius: '12px',
                        border: '1px dashed #d9d9d9',
                        animation: 'fadeIn 0.3s ease-in-out'
                    }}>
                        <i className="fas fa-bolt" style={{ fontSize: '32px', color: '#faad14', marginBottom: '16px' }} />
                        <div style={{ fontWeight: 600, color: '#262626', fontSize: '15px' }}>Event-Based 전송 활성</div>
                        <div style={{ color: '#8c8c8c', fontSize: '13px', marginTop: '8px' }}>
                            Sends data immediately after collection and processing, without a separate schedule.<br />
                            (You can save immediately without additional settings)
                        </div>
                    </div>
                )}
            </Form>
        </div>
    );
};

export default Step4Schedule;
