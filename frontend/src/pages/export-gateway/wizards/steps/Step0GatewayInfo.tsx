// ============================================================================
// wizards/steps/Step0GatewayInfo.tsx
// Step 0: Gateway 기본 정보 입력
// ============================================================================

import React from 'react';
import { Form, Input, Row, Col, Radio, Select } from 'antd';
import { GatewayFormData } from '../types';

interface Step0Props {
    tl: (key: string, opts?: any) => string;
    isAdmin: boolean;
    tenants: any[];
    sites: any[];
    wizardTenantId: number | null;
    setWizardTenantId: (id: number) => void;
    wizardSiteId: number | null;
    setWizardSiteId: (id: number | null) => void;
    fetchSites: (tId: number) => void;
    gatewayData: GatewayFormData;
    setGatewayData: (data: GatewayFormData) => void;
}

const Step0GatewayInfo: React.FC<Step0Props> = ({
    tl, isAdmin, tenants, sites,
    wizardTenantId, setWizardTenantId,
    wizardSiteId, setWizardSiteId,
    fetchSites, gatewayData, setGatewayData
}) => {
    return (
        <div style={{
            padding: '32px',
            paddingBottom: '60px',
            background: 'white',
            borderRadius: '12px',
            display: 'flex',
            flexDirection: 'column',
            gap: '24px',
            boxShadow: '0 4px 12px rgba(0,0,0,0.03)',
            border: '1px solid #f0f0f0',
            minHeight: '100%',
            alignItems: 'stretch'
        }}>
            <div style={{ fontSize: '18px', fontWeight: 800, color: '#1a1a1a', display: 'flex', alignItems: 'center', marginBottom: '8px' }}>
                <i className="fas fa-server" style={{ marginRight: '12px', color: '#1890ff' }} />
                {tl('gwWizard.basicInfoTitle', { ns: 'dataExport' })}
            </div>

            {isAdmin && (
                <Row gutter={16}>
                    <Col span={12}>
                        <div style={{ marginBottom: '8px', fontWeight: 600 }}>{tl('labels.tenantSelectionAdmin', { ns: 'dataExport' })}</div>
                        <Select
                            style={{ width: '100%' }}
                            placeholder={tl('gwWizard.selectTenantPh', { ns: 'dataExport' })}
                            value={wizardTenantId}
                            onChange={(val) => {
                                setWizardTenantId(val);
                                setWizardSiteId(null);
                                fetchSites(val);
                            }}
                        >
                            {tenants.map(t => (
                                <Select.Option key={t.id} value={t.id}>{t.company_name}</Select.Option>
                            ))}
                        </Select>
                    </Col>
                    <Col span={12}>
                        <div style={{ marginBottom: '8px', fontWeight: 600 }}>{tl('labels.siteSelectionAdmin', { ns: 'dataExport' })}</div>
                        <Select
                            style={{ width: '100%' }}
                            placeholder="Select site"
                            value={wizardSiteId}
                            onChange={setWizardSiteId}
                            disabled={!wizardTenantId}
                        >
                            <Select.Option value={null}>{tl('labels.allSitesShared', { ns: 'dataExport' })}</Select.Option>
                            {sites.map(s => (
                                <Select.Option key={s.id} value={s.id}>{s.name}</Select.Option>
                            ))}
                        </Select>
                    </Col>
                </Row>
            )}

            <Form layout="vertical">
                <Form.Item
                    label={tl('gwWizard.gatewayNameLabel', { ns: 'dataExport' })}
                    required
                    tooltip={tl('gwWizard.gatewayNameTooltip', { ns: 'dataExport' })}
                >
                    <Input
                        size="large"
                        autoFocus
                        placeholder={tl('gwWizard.gatewayNamePh', { ns: 'dataExport' })}
                        value={gatewayData.name}
                        onChange={e => setGatewayData({ ...gatewayData, name: e.target.value })}
                    />
                </Form.Item>
                <Form.Item
                    label={tl('gwWizard.ipAddressLabel', { ns: 'dataExport' })}
                    required
                    tooltip={tl('gwWizard.ipAddressTooltip', { ns: 'dataExport' })}
                >
                    <Input
                        size="large"
                        placeholder="127.0.0.1"
                        value={gatewayData.ip_address}
                        onChange={e => setGatewayData({ ...gatewayData, ip_address: e.target.value })}
                    />
                </Form.Item>
                <Form.Item
                    label={tl('gwWizard.alarmSubModeLabel', { ns: 'dataExport' })}
                    required
                    tooltip={tl('gwWizard.alarmSubModeTooltip', { ns: 'dataExport' })}
                >
                    <Radio.Group
                        size="large"
                        value={gatewayData.subscription_mode}
                        onChange={e => setGatewayData({ ...gatewayData, subscription_mode: e.target.value as 'all' | 'selective' })}
                    >
                        <Radio value="all">{tl('labels.receiveAll', { ns: 'dataExport' })}</Radio>
                        <Radio value="selective">{tl('labels.assignedDevicesOnlySelective', { ns: 'dataExport' })}</Radio>
                    </Radio.Group>
                </Form.Item>
                <Form.Item label={tl('gwWizard.descriptionLabel', { ns: 'dataExport' })}>
                    <Input.TextArea
                        placeholder={tl('gwWizard.descriptionPh', { ns: 'dataExport' })}
                        rows={4}
                        value={gatewayData.description}
                        onChange={e => setGatewayData({ ...gatewayData, description: e.target.value })}
                    />
                </Form.Item>
            </Form>
        </div>
    );
};

export default Step0GatewayInfo;
