// =============================================================================
// frontend/src/pages/ModbusSlaveSettings.tsx
// Modbus Slave 관리 메인 페이지 (탭 컨테이너)
// ExportGatewaySettings.tsx 와 동일한 패턴
// =============================================================================
import React, { useState, useEffect, useCallback } from 'react';
import { Tabs, Tag } from 'antd';
import { useParams, useNavigate } from 'react-router-dom';
import DeviceListTab from './modbus-slave/tabs/DeviceListTab';
import MappingEditorTab from './modbus-slave/tabs/MappingEditorTab';
import modbusSlaveApi, { ModbusSlaveDevice } from '../api/services/modbusSlaveApi';
import { SiteApiService } from '../api/services/siteApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { FilterBar } from '../components/common/FilterBar';
import exportGatewayApi, { DataPoint } from '../api/services/exportGatewayApi';
import { TenantApiService } from '../api/services/tenantApi';
import { Site, Tenant } from '../types/common';
import '../styles/management.css';

const VALID_TABS = ['devices', 'mappings'];

const ModbusSlaveSettings: React.FC = () => {
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
                title="Modbus Slave 관리"
                description="Modbus TCP Slave 디바이스를 관리하고 레지스터 매핑을 설정합니다"
                icon="fas fa-network-wired"
            />

            {/* 요약 카드 — 상단 */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', marginBottom: '12px' }}>
                {[
                    { label: '전체 디바이스', value: devices.length, icon: 'fa-network-wired', bg: '#eff6ff', fg: '#3b82f6' },
                    { label: '실행 중', value: runningCount, icon: 'fa-circle', bg: '#f0fdf4', fg: '#16a34a' },
                    { label: '중지됨', value: stoppedCount, icon: 'fa-stop-circle', bg: '#fafafa', fg: '#94a3b8' },
                    { label: '매핑 포인트', value: totalMappings, icon: 'fa-th', bg: '#fdf4ff', fg: '#9333ea' },
                ].map(c => (
                    <div key={c.label} style={{ background: 'white', border: '1px solid #e2e8f0', borderRadius: '12px', padding: '20px', display: 'flex', alignItems: 'center', gap: '16px' }}>
                        <div style={{ width: '44px', height: '44px', background: c.bg, borderRadius: '10px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                            <i className={`fas ${c.icon}`} style={{ color: c.fg, fontSize: '18px' }} />
                        </div>
                        <div>
                            <div style={{ fontSize: '24px', fontWeight: 800, color: '#1e293b' }}>{c.value}</div>
                            <div style={{ fontSize: '12px', color: '#64748b' }}>{c.label}</div>
                        </div>
                    </div>
                ))}
            </div>

            {/* 테넌트 / 사이트 필터 — 카드 아래 */}
            <div style={{ marginBottom: '12px' }}>
                <FilterBar
                    filters={[
                        ...(isAdmin ? [{
                            label: '테넌트',
                            value: currentTenantId ? String(currentTenantId) : 'all',
                            options: [
                                { label: '전체 테넌트', value: 'all' },
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
                            label: '사이트',
                            value: currentSiteId ? String(currentSiteId) : 'all',
                            options: [
                                { label: '전체 사이트', value: 'all' },
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
                overflow: 'hidden',
            }}>
                <Tabs
                    activeKey={activeTab}
                    onChange={setActiveTab}
                    tabBarExtraContent={activeTab === 'mappings' ? (
                        <div style={{ display: 'flex', gap: 4, alignItems: 'center', paddingRight: 4 }}>
                            {[
                                { value: 'HR', full: 'Holding Register (4xxxx)', color: 'blue' },
                                { value: 'IR', full: 'Input Register (3xxxx)', color: 'green' },
                                { value: 'CO', full: 'Coil (0xxxx)', color: 'orange' },
                                { value: 'DI', full: 'Discrete Input (1xxxx)', color: 'purple' },
                            ].map(rt => (
                                <Tag key={rt.value} color={rt.color} style={{ margin: 0, fontSize: 11 }}>
                                    {rt.value}: {rt.full}
                                </Tag>
                            ))}
                        </div>
                    ) : null}
                    items={[
                        {
                            key: 'devices',
                            label: <span><i className="fas fa-network-wired" style={{ marginRight: '6px' }} />디바이스 관리</span>,
                            children: (
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
                            ),
                        },
                        {
                            key: 'mappings',
                            label: (
                                <span>
                                    <i className="fas fa-th" style={{ marginRight: '6px' }} />
                                    레지스터 매핑
                                </span>
                            ),
                            children: <MappingEditorTab devices={devices} allPoints={allPoints} />,
                        },
                    ]}
                />
            </div>
        </ManagementLayout>
    );
};

export default ModbusSlaveSettings;
