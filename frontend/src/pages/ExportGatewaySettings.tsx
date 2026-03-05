
import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
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
    const { t } = useTranslation(['dataExport', 'common']);
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
                title: t('gwTab.deleteComplete', { ns: 'dataExport' }),
                message: t('gwTab.deleteCompleteMsg', { ns: 'dataExport' }),
                showCancelButton: false,
                confirmButtonType: 'success'
            });
            fetchData();
        } catch (e) {
            console.error(e);
            await confirm({
                title: t('gwTab.deleteFailed', { ns: 'dataExport' }),
                message: t('gwTab.deleteFailedMsg', { ns: 'dataExport' }),
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
                    title={t('exportPage.title', { ns: 'dataExport' })}
                    description={t('exportPage.description', { ns: 'dataExport' })}
                    icon="fas fa-satellite-dish"
                />

                <div className="mgmt-stats-panel" style={{ marginBottom: '24px' }}>
                    <StatCard label={t('exportPage.statTotal', { ns: 'dataExport' })} value={gateways.length} type="neutral" />
                    <StatCard label={t('exportPage.statOnline', { ns: 'dataExport' })} value={onlineCount} type="success" />
                    <StatCard label={t('exportPage.statOffline', { ns: 'dataExport' })} value={gateways.length - onlineCount} type="error" />
                </div>

                <div style={{ marginBottom: '20px' }}>
                    <FilterBar
                        filters={[
                            ...(isAdmin ? [{
                                label: t('exportPage.filterTenant', { ns: 'dataExport' }),
                                value: currentTenantId ? String(currentTenantId) : 'all',
                                options: [
                                    { label: t('exportPage.filterTenantAll', { ns: 'dataExport' }), value: 'all' },
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
                                label: t('exportPage.filterSite', { ns: 'dataExport' }),
                                value: currentSiteId ? String(currentSiteId) : 'all',
                                options: [
                                    { label: t('exportPage.filterSiteAll', { ns: 'dataExport' }), value: 'all' },
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
                            { key: 'gateways', icon: 'fa-server', label: t('exportPage.tabGatewaySettings', { ns: 'dataExport' }) },
                            { key: 'profiles', icon: 'fa-file-export', label: t('exportPage.tabDataMapping', { ns: 'dataExport' }) },
                            { key: 'targets', icon: 'fa-external-link-alt', label: t('exportPage.tabTargets', { ns: 'dataExport' }) },
                            { key: 'templates', icon: 'fa-code', label: t('exportPage.tabTemplates', { ns: 'dataExport' }) },
                            { key: 'schedules', icon: 'fa-calendar-alt', label: t('exportPage.tabSchedules', { ns: 'dataExport' }) },
                            { key: 'manual-test', icon: 'fa-flask', label: t('exportPage.tabManualTest', { ns: 'dataExport' }) },
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
                                <i className="fas fa-magic" style={{ marginRight: '4px' }} /> {t('gwTab.registerWizard', { ns: 'dataExport' })}
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
                    {activeTab === 'manual-test' && <ManualTestTab siteId={currentSiteId} tenantId={currentTenantId} />}
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
