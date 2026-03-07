// =============================================================================
// frontend/src/pages/ModbusSlaveSettings.tsx
// Modbus Slave 관리 메인 페이지 (탭 컨테이너)
// ExportGatewaySettings.tsx 와 동일한 패턴
// =============================================================================
import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { Tabs, Tag } from 'antd';
import { useParams, useNavigate } from 'react-router-dom';
import DeviceListTab from './modbus-slave/tabs/DeviceListTab';
import MappingEditorTab from './modbus-slave/tabs/MappingEditorTab';
import ClientMonitorTab from './modbus-slave/tabs/ClientMonitorTab';
import AccessLogTab from './modbus-slave/tabs/AccessLogTab';
import PacketLogTab from './modbus-slave/tabs/PacketLogTab';
import DashboardTab from './modbus-slave/tabs/DashboardTab';
import modbusSlaveApi, { ModbusSlaveDevice } from '../api/services/modbusSlaveApi';
import { SiteApiService } from '../api/services/siteApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { FilterBar } from '../components/common/FilterBar';
import exportGatewayApi, { DataPoint } from '../api/services/exportGatewayApi';
import { TenantApiService } from '../api/services/tenantApi';
import { Site, Tenant } from '../types/common';
import '../styles/management.css';

const VALID_TABS = ['dashboard', 'devices', 'mappings', 'monitor', 'logs', 'packet-logs'];

const ModbusSlaveSettings: React.FC = () => {
    const { t } = useTranslation('settings');
    const { tab } = useParams<{ tab: string }>();
    const navigate = useNavigate();
    const activeTab = tab && VALID_TABS.includes(tab) ? tab : 'devices';
    const setActiveTab = (newTab: string) => navigate(`/system/modbus-slave/${newTab}`);

    // ── 상태 ────────────────────────────────────────────────────────────────
    const [devices, setDevices] = useState<ModbusSlaveDevice[]>([]);
    const [sites, setSites] = useState<Site[]>([]);
    const [tenants, setTenants] = useState<Tenant[]>([]);
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [loading, setLoading] = useState(false);
    const [isAdmin] = useState(true); // TODO: 실제 RBAC 연동 시 교체

    // 테넌트/사이트 필터 — localStorage로 글로벌 TenantSelector 동기화
    const [currentTenantId, setCurrentTenantId] = useState<number | null>(() => {
        const stored = localStorage.getItem('selected_tenant_id');
        return stored ? Number(stored) : null;
    });
    const [currentSiteId, setCurrentSiteId] = useState<number | null>(null);

    // ── 테넌트 / 사이트 로드 ─────────────────────────────────────────────────
    const loadTenants = async () => {
        try {
            const res = await TenantApiService.getTenants();
            if (res.success && res.data) setTenants((res.data as any).items || []);
        } catch { /* ignore */ }
    };

    const loadSites = useCallback(async (tenantId: number | null = currentTenantId) => {
        try {
            const res = await SiteApiService.getSites({ limit: 100, tenantId: tenantId || undefined });
            if (res.success && res.data) setSites((res.data as any).items || []);
        } catch { /* ignore */ }
    }, [currentTenantId]);

    // ── 데이터 로드 ──────────────────────────────────────────────────────────
    const fetchData = useCallback(async () => {
        setLoading(true);
        try {
            const [devRes, pointRes] = await Promise.all([
                modbusSlaveApi.getDevices({ site_id: currentSiteId }),
                exportGatewayApi.getDataPoints('', undefined, currentSiteId, currentTenantId),
            ]);
            if (devRes.success && devRes.data) {
                setDevices(Array.isArray(devRes.data) ? devRes.data : []);
            }
            setAllPoints(pointRes || []);
        } finally {
            setLoading(false);
        }
    }, [currentSiteId, currentTenantId]);

    useEffect(() => {
        loadTenants();
        loadSites();

        // 글로벌 TenantSelector 변경 감지
        const handleStorageChange = (e: StorageEvent) => {
            if (e.key === 'selected_tenant_id') {
                const newTenantId = e.newValue ? Number(e.newValue) : null;
                setCurrentTenantId(newTenantId);
                setCurrentSiteId(null);
                loadSites(newTenantId);
            }
        };
        window.addEventListener('storage', handleStorageChange);
        return () => window.removeEventListener('storage', handleStorageChange);
    }, []);

    useEffect(() => { fetchData(); }, [fetchData]);

    // ── 통계 ─────────────────────────────────────────────────────────────────
    const runningCount = devices.filter(d => d.process_status === 'running').length;
    const stoppedCount = devices.length - runningCount;
    const totalMappings = devices.reduce((sum, d) => sum + (d.mapping_count || 0), 0);

    return (
        <ManagementLayout>
            <PageHeader
                title={t('modbusSlave.title')}
                description={t('modbusSlave.subtitle')}
                icon="fas fa-network-wired"
            />


            {/* 테넌트 / 사이트 필터 — 카드 아래 */}
            <div style={{ marginBottom: '12px' }}>
                <FilterBar
                    filters={[
                        ...(isAdmin ? [{
                            label: t('modbusSlave.tenant'),
                            value: currentTenantId ? String(currentTenantId) : 'all',
                            options: [
                                { label: t('modbusSlave.allTenants'), value: 'all' },
                                ...tenants.map(tn => ({ label: (tn as any).company_name || (tn as any).name, value: String(tn.id) }))
                            ],
                            onChange: (val: string) => {
                                const tId = val === 'all' ? null : Number(val);
                                setCurrentTenantId(tId);
                                setCurrentSiteId(null);
                                if (tId) localStorage.setItem('selected_tenant_id', String(tId));
                                else localStorage.removeItem('selected_tenant_id');
                                loadSites(tId);
                            }
                        }] : []),
                        {
                            label: t('modbusSlave.site'),
                            value: currentSiteId ? String(currentSiteId) : 'all',
                            options: [
                                { label: t('modbusSlave.allSites'), value: 'all' },
                                ...sites.map(s => ({ label: s.name, value: String(s.id) }))
                            ],
                            onChange: (val: string) => setCurrentSiteId(val === 'all' ? null : Number(val))
                        }
                    ]}
                    onReset={() => {
                        setCurrentTenantId(null);
                        setCurrentSiteId(null);
                        localStorage.removeItem('selected_tenant_id');
                        loadSites(null);
                    }}
                    activeFilterCount={(currentTenantId ? 1 : 0) + (currentSiteId ? 1 : 0)}
                />
            </div>

            <div style={{
                background: 'white',
                border: '1px solid #e2e8f0',
                borderRadius: '12px',
                padding: '8px 24px 16px',
                flex: 1,
                minHeight: 0,
                overflow: 'auto',
            }}>
                <Tabs
                    activeKey={activeTab}
                    onChange={setActiveTab}
                    tabBarStyle={{ position: 'sticky', top: -8, zIndex: 10, background: '#fff', marginLeft: -24, marginRight: -24, paddingLeft: 24, paddingRight: 24, marginBottom: 0 }}
                    items={[
                        {
                            key: 'dashboard',
                            label: (
                                <span>
                                    <i className="fas fa-tachometer-alt" style={{ marginRight: '6px' }} />
                                    {t('modbusSlave.tabs.dashboard')}
                                </span>
                            ),
                            children: <div style={{ paddingBottom: 16 }}><DashboardTab devices={devices} /></div>,
                        },
                        {
                            key: 'devices',
                            label: <span><i className="fas fa-network-wired" style={{ marginRight: '6px' }} />{t('modbusSlave.tabs.devices')}</span>,
                            children: (
                                <div style={{ paddingBottom: 16 }}>
                                    <DeviceListTab
                                        devices={devices}
                                        loading={loading}
                                        onRefresh={fetchData}
                                        sites={sites}
                                        tenants={tenants}
                                        isAdmin={isAdmin}
                                        currentTenantId={currentTenantId}
                                        onSiteReload={loadSites}
                                    />
                                </div>
                            ),
                        },
                        {
                            key: 'mappings',
                            label: (
                                <span>
                                    <i className="fas fa-th" style={{ marginRight: '6px' }} />
                                    {t('modbusSlave.tabs.mappings')}
                                </span>
                            ),
                            children: <div style={{ paddingBottom: 16 }}><MappingEditorTab devices={devices} allPoints={allPoints} /></div>,
                        },
                        {
                            key: 'monitor',
                            label: (
                                <span>
                                    <i className="fas fa-satellite-dish" style={{ marginRight: '6px' }} />
                                    {t('modbusSlave.tabs.monitor')}
                                </span>
                            ),
                            children: <div style={{ paddingBottom: 16 }}><ClientMonitorTab devices={devices} /></div>,
                        },
                        {
                            key: 'logs',
                            label: (
                                <span>
                                    <i className="fas fa-history" style={{ marginRight: '6px' }} />
                                    {t('modbusSlave.tabs.logs')}
                                </span>
                            ),
                            children: <div style={{ paddingBottom: 16 }}><AccessLogTab devices={devices} /></div>,
                        },
                        {
                            key: 'packet-logs',
                            label: (
                                <span>
                                    <i className="fas fa-terminal" style={{ marginRight: '6px' }} />
                                    {t('modbusSlave.tabs.packetLogs')}
                                </span>
                            ),
                            children: <div style={{ paddingBottom: 16 }}><PacketLogTab devices={devices} /></div>,
                        },
                    ]}
                />
            </div>
        </ManagementLayout>
    );
};

export default ModbusSlaveSettings;
