
import React, { useState, useEffect, useCallback } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import GatewayListTab from './export-gateway/tabs/GatewayListTab';
import ExportGatewayWizard from './export-gateway/wizards/ExportGatewayWizard';
import ProfileManagementTab from './export-gateway/tabs/ProfileManagementTab';
import TargetManagementTab from './export-gateway/tabs/TargetManagementTab';
import TemplateManagementTab from './export-gateway/tabs/TemplateManagementTab';
import ScheduleManagementTab from './export-gateway/tabs/ScheduleManagementTab';
import ManualTestTab from './export-gateway/tabs/ManualTestTab';
import exportGatewayApi, { DataPoint, Gateway, ExportProfile, ExportTarget, Assignment, PayloadTemplate, ExportSchedule } from '../api/services/exportGatewayApi';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { Tabs, Button, Row, Col, Space, Input, Table, Tag, Divider, Tooltip } from 'antd';
import { PlusOutlined, ReloadOutlined, SearchOutlined, EditOutlined, DeleteOutlined, PlayCircleOutlined, PauseCircleOutlined, SyncOutlined, SettingOutlined } from '@ant-design/icons';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import { SiteApiService } from '../api/services/siteApi';
import { TenantApiService } from '../api/services/tenantApi';
import { Site, Tenant } from '../types/common';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import '../styles/management.css';
import '../styles/pagination.css';

// Helper to safely extract array from various API response formats
const extractItems = (data: any): any[] => {
    if (!data) return [];
    if (Array.isArray(data)) return data;
    if (Array.isArray(data.items)) return data.items;
    if (Array.isArray(data.rows)) return data.rows;
    return [];
};

const ExportGatewaySettings: React.FC = () => {
    const { tab } = useParams<{ tab: string }>();
    const navigate = useNavigate();

    // Tab mapping for validation
    const validTabs = ['gateways', 'profiles', 'targets', 'templates', 'schedules', 'manual-test'];
    const activeTab = (tab && validTabs.includes(tab)) ? tab as any : 'gateways';

    const setActiveTab = (newTab: string) => {
        navigate(`/system/export-gateways/${newTab}`);
    };
    const [gateways, setGateways] = useState<Gateway[]>([]);
    const [assignments, setAssignments] = useState<Record<number, Assignment[]>>({});
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [schedules, setSchedules] = useState<ExportSchedule[]>([]);
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [loading, setLoading] = useState(false);
    const [pagination, setPagination] = useState<any>({ current_page: 1, total_pages: 1 });
    const [page, setPage] = useState(1);
    const { confirm } = useConfirmContext();

    // Registration Modal State
    const [isRegModalOpen, setIsRegModalOpen] = useState(false);

    // Edit Gateway State
    const [editingGateway, setEditingGateway] = useState<Gateway | null>(null);

    const [allProfiles, setAllProfiles] = useState<ExportProfile[]>([]);
    const [sites, setSites] = useState<Site[]>([]);
    const [tenants, setTenants] = useState<Tenant[]>([]);
    const [isAdmin, setIsAdmin] = useState(true); // Forced true for dev convenience

    // Read from localStorage to sync with global TenantSelector
    const [currentTenantId, setCurrentTenantId] = useState<number | null>(() => {
        const stored = localStorage.getItem('selected_tenant_id');
        return stored ? Number(stored) : null;
    });
    const [currentSiteId, setCurrentSiteId] = useState<number | null>(null);

    const loadTenants = async () => {
        try {
            const res = await TenantApiService.getTenants();
            if (res.success && res.data) {
                setTenants(res.data.items || []);
            }
        } catch (error) {
            console.error('Failed to load tenants:', error);
        }
    };

    const loadSites = async (tenantId: number | null = currentTenantId) => {
        try {
            const res = await SiteApiService.getSites({ limit: 100, tenantId: tenantId || undefined });
            if (res.success && res.data) {
                setSites(res.data.items || []);
            }
        } catch (error) {
            console.error('Failed to load sites:', error);
        }
    };

    const fetchData = useCallback(async () => {
        if (gateways.length === 0) setLoading(true);
        try {
            const params = { page, limit: 10, siteId: currentSiteId, tenantId: currentTenantId };
            const [gwRes, targetsRes, templatesRes, pointsRes, profilesRes, schedulesRes] = await Promise.all([
                exportGatewayApi.getGateways(params),
                exportGatewayApi.getTargets({ tenantId: currentTenantId }),
                exportGatewayApi.getTemplates({ tenantId: currentTenantId }),
                exportGatewayApi.getDataPoints('', undefined, currentSiteId, currentTenantId), // Points still site-specific
                exportGatewayApi.getProfiles({ tenantId: currentTenantId }),
                exportGatewayApi.getSchedules({ tenantId: currentTenantId })
            ]);

            const data = gwRes.data;
            const gwList = data?.items || [];
            setGateways(gwList);
            if (data?.pagination) {
                setPagination(data.pagination);
            }

            setTargets(extractItems(targetsRes.data));
            setTemplates(extractItems(templatesRes.data));
            setAllPoints(pointsRes || []);
            setAllProfiles(extractItems(profilesRes.data));
            setSchedules(extractItems(schedulesRes.data));

            const assignMap: Record<number, Assignment[]> = {};
            await Promise.all((gwList || []).map(async (gw: Gateway) => {
                const response = await exportGatewayApi.getAssignments(gw.id, undefined, currentTenantId);
                assignMap[gw.id] = extractItems(response.data);
            }));
            setAssignments(assignMap);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    }, [gateways.length, page, currentSiteId, currentTenantId]);

    useEffect(() => {
        setIsAdmin(true);
        loadTenants();
        loadSites();

        // [NEW] Sync with global TenantSelector navbar changes (e.g., user picks tenant in header)
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

    useEffect(() => {
        fetchData();
        const interval = setInterval(fetchData, 5000);
        return () => clearInterval(interval);
    }, [fetchData]);

    const handleDeleteGateway = async (gateway: Gateway) => {
        try {
            await exportGatewayApi.deleteGateway(gateway.id);
            await confirm({
                title: '삭제 완료',
                message: '게이트웨이가 성공적으로 삭제되었습니다.',
                showCancelButton: false,
                confirmButtonType: 'success'
            });
            fetchData();
        } catch (e) {
            console.error(e);
            await confirm({
                title: '삭제 실패',
                message: '삭제 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const onlineCount = gateways.filter(gw =>
        gw.live_status?.status === 'online' ||
        gw.live_status?.status === 'running' ||
        gw.status === 'online' ||
        gw.status === 'active'
    ).length;

    return (
        <>
            <ManagementLayout>
                <PageHeader
                    title="데이터 내보내기 설정"
                    description="외부 시스템(HTTP/MQTT)으로 데이터를 전송하기 위한 게이트웨이 및 데이터 매핑을 설정합니다."
                    icon="fas fa-satellite-dish"
                />

                <div className="mgmt-stats-panel" style={{ marginBottom: '24px' }}>
                    <StatCard label="전체 게이트웨이" value={gateways.length} type="neutral" />
                    <StatCard label="온라인" value={onlineCount} type="success" />
                    <StatCard label="오프라인" value={gateways.length - onlineCount} type="error" />
                </div>

                <div style={{ marginBottom: '20px' }}>
                    <FilterBar
                        filters={[
                            ...(isAdmin ? [{
                                label: '테넌트 필터',
                                value: currentTenantId ? String(currentTenantId) : 'all',
                                options: [
                                    { label: '전체 테넌트', value: 'all' },
                                    ...tenants.map(t => ({ label: t.company_name, value: String(t.id) }))
                                ],
                                onChange: (val: string) => {
                                    const tId = val === 'all' ? null : Number(val);
                                    setCurrentTenantId(tId);
                                    setCurrentSiteId(null); // Reset site on tenant change
                                    if (tId) localStorage.setItem('selected_tenant_id', String(tId));
                                    else localStorage.removeItem('selected_tenant_id');
                                    loadSites(tId);
                                }
                            }] : []),
                            {
                                label: '사이트 필터',
                                value: currentSiteId ? String(currentSiteId) : 'all',
                                options: [
                                    { label: '전체 사이트 (Global)', value: 'all' },
                                    ...sites.map(s => ({ label: s.name, value: String(s.id) }))
                                ],
                                onChange: (val) => setCurrentSiteId(val === 'all' ? null : Number(val))
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

                <div className="mgmt-filter-bar" style={{ marginBottom: '20px', borderBottom: '1px solid var(--neutral-200)', display: 'flex', justifyContent: 'space-between', alignItems: 'center', overflow: 'visible' }}>
                    <div style={{ display: 'flex', gap: '8px', flexWrap: 'nowrap', overflow: 'auto' }}>
                        {[
                            { key: 'gateways', icon: 'fa-server', label: '게이트웨이 설정' },
                            { key: 'profiles', icon: 'fa-file-export', label: '데이터 매핑' },
                            { key: 'targets', icon: 'fa-external-link-alt', label: '내보내기 대상' },
                            { key: 'templates', icon: 'fa-code', label: '페이로드 템플릿' },
                            { key: 'schedules', icon: 'fa-calendar-alt', label: '스케줄 관리' },
                            { key: 'manual-test', icon: 'fa-flask', label: '수동 테스트' },
                        ].map(t => (
                            <button
                                key={t.key}
                                className={`nav-tab ${activeTab === t.key ? 'active' : ''}`}
                                onClick={() => setActiveTab(t.key)}
                                style={{
                                    padding: '8px 10px',
                                    border: 'none',
                                    background: 'none',
                                    borderBottom: activeTab === t.key ? '2px solid var(--primary-500)' : '2px solid transparent',
                                    color: activeTab === t.key ? 'var(--primary-600)' : 'var(--neutral-500)',
                                    fontWeight: activeTab === t.key ? 600 : 400,
                                    cursor: 'pointer',
                                    fontSize: '14px',
                                    whiteSpace: 'nowrap',
                                }}
                            >
                                <i className={`fas ${t.icon}`} style={{ marginRight: '4px' }} /> {t.label}
                            </button>
                        ))}
                    </div>

                    <div className="tab-actions">
                        {activeTab === 'gateways' && (
                            <button className="btn btn-primary btn-sm" onClick={() => setIsRegModalOpen(true)} style={{ fontSize: '13px', padding: '4px 10px', whiteSpace: 'nowrap' }}>
                                <i className="fas fa-magic" style={{ marginRight: '4px' }} /> 게이트웨이 등록 (마법사)
                            </button>
                        )}
                    </div>
                </div>

                <div className="mgmt-content-area">
                    {activeTab === 'gateways' && (
                        <GatewayListTab
                            siteId={currentSiteId}
                            gateways={gateways}
                            loading={loading}
                            onRefresh={fetchData}
                            onEdit={(gw) => {
                                setEditingGateway(gw);
                                setIsRegModalOpen(true);
                            }}
                            onDelete={handleDeleteGateway}
                            pagination={pagination}
                            onPageChange={setPage}
                            assignments={assignments}
                            targets={targets}
                            profiles={allProfiles}
                            schedules={schedules}
                        />
                    )}
                    {activeTab === 'profiles' && <ProfileManagementTab siteId={currentSiteId} tenantId={currentTenantId} isAdmin={isAdmin} tenants={tenants} />}
                    {activeTab === 'targets' && <TargetManagementTab siteId={currentSiteId} tenantId={currentTenantId} isAdmin={isAdmin} tenants={tenants} />}
                    {activeTab === 'templates' && <TemplateManagementTab siteId={currentSiteId} tenantId={currentTenantId} isAdmin={isAdmin} tenants={tenants} />}
                    {activeTab === 'schedules' && <ScheduleManagementTab siteId={currentSiteId} tenantId={currentTenantId} isAdmin={isAdmin} tenants={tenants} />}
                    {activeTab === 'manual-test' && <ManualTestTab siteId={currentSiteId} tenantId={currentTenantId} isAdmin={isAdmin} tenants={tenants} />}
                </div>

                {/* Replacement: Wizard used for both Register & Edit */}
                <ExportGatewayWizard
                    visible={isRegModalOpen}
                    editingGateway={editingGateway}
                    siteId={currentSiteId}
                    tenantId={currentTenantId}
                    onClose={() => {
                        setIsRegModalOpen(false);
                        setEditingGateway(null);
                    }}
                    onSuccess={() => {
                        setIsRegModalOpen(false);
                        setEditingGateway(null);
                        fetchData();
                    }}
                    profiles={allProfiles}
                    targets={targets}
                    templates={templates}
                    allPoints={allPoints}
                    assignments={editingGateway ? (Object.entries(assignments).find(([k]) => String(k) === String(editingGateway.id))?.[1] || []) : []}
                    schedules={schedules}
                    onDelete={handleDeleteGateway}
                />

            </ManagementLayout>
        </>
    );
};

export default ExportGatewaySettings;
