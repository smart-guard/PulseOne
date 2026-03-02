import React, { useState } from 'react';
import Dashboard from '../../pages/Dashboard';
import SystemStatus from '../../pages/SystemStatus';
import DeviceList from '../../pages/DeviceList';
import RealTimeMonitor from '../../pages/RealTimeMonitor';
import DataExplorer from '../../pages/DataExplorer';
import HistoricalData from '../../pages/HistoricalData';
import VirtualPoints from '../../pages/VirtualPoints';
import DataExport from '../../pages/DataExport';
import AlarmSettings from '../../pages/AlarmSettings';
import AlarmHistory from '../../pages/AlarmHistory';
import AlarmRuleTemplates from '../../pages/AlarmRuleTemplates';
import ActiveAlarms from '../../pages/ActiveAlarms';
import PermissionManagement from '../../pages/PermissionManagement';
import NetworkSettings from '../../pages/NetworkSettings';
import LoginHistory from '../../pages/LoginHistory';
import BackupRestore from '../../pages/BackupRestore';
import UserManagement from '../../pages/UserManagement';
import '../../styles/base.css';

interface MenuItem {
  id: string;
  title: string;
  icon: string;
  path: string;
  children?: MenuItem[];
}

const menuItems: MenuItem[] = [
  {
    id: 'dashboard',
    title: 'Dashboard',
    icon: 'fas fa-tachometer-alt',
    path: '/dashboard'
  },
  {
    id: 'devices',
    title: 'Device Management',
    icon: 'fas fa-network-wired',
    path: '/devices',
    children: [
      { id: 'device-list', title: 'Device List', icon: 'fas fa-list', path: '/devices/list' },
      { id: 'device-add', title: 'Add Device', icon: 'fas fa-plus', path: '/devices/add' },
      { id: 'device-groups', title: 'Device Groups', icon: 'fas fa-layer-group', path: '/devices/groups' },
      { id: 'protocols', title: 'Protocol Settings', icon: 'fas fa-cogs', path: '/devices/protocols' }
    ]
  },
  {
    id: 'data',
    title: 'Data Management',
    icon: 'fas fa-database',
    path: '/data',
    children: [
      { id: 'data-explorer', title: 'Data Explorer', icon: 'fas fa-download', path: '/data/explorer' },
      { id: 'realtime', title: 'Real-time Data', icon: 'fas fa-chart-line', path: '/data/realtime' },
      { id: 'historical', title: 'Historical Data', icon: 'fas fa-history', path: '/data/historical' },
      { id: 'virtual-points', title: 'Virtual Points', icon: 'fas fa-calculator', path: '/data/virtual-points' },
      { id: 'data-export', title: 'Data Export', icon: 'fas fa-download', path: '/data/export' }
    ]
  },
  {
    id: 'alarms',
    title: 'Alarm Management',
    icon: 'fas fa-bell',
    path: '/alarms',
    children: [
      { id: 'active-alarms', title: 'Active Alarms', icon: 'fas fa-exclamation-triangle', path: '/alarms/active' },
      { id: 'alarm-history', title: 'Alarm History', icon: 'fas fa-clock', path: '/alarms/history' },
      { id: 'alarm-config', title: 'Alarm Settings', icon: 'fas fa-cog', path: '/alarms/config' },
      { id: 'alarm-type', title: 'Alarm Types', icon: 'fas fa-cog', path: '/alarms/type' },
      { id: 'alarm-rules', title: 'Alarm Rules', icon: 'fas fa-cog', path: '/api/alarms/ruletemplates' },
      { id: 'notification', title: 'Notification Settings', icon: 'fas fa-envelope', path: '/alarms/notification' }
    ]
  },
  {
    id: 'monitoring',
    title: 'System Monitoring',
    icon: 'fas fa-server',
    path: '/monitoring',
    children: [
      { id: 'system-status', title: 'System Status', icon: 'fas fa-heartbeat', path: '/monitoring/status' },
      { id: 'performance', title: 'Performance Metrics', icon: 'fas fa-chart-bar', path: '/monitoring/performance' },
      { id: 'logs', title: 'Log Viewer', icon: 'fas fa-file-alt', path: '/monitoring/logs' },
      { id: 'health-check', title: 'Health Check', icon: 'fas fa-stethoscope', path: '/monitoring/health' }
    ]
  },
  {
    id: 'reports',
    title: 'Reports',
    icon: 'fas fa-chart-pie',
    path: '/reports',
    children: [
      { id: 'dashboard-reports', title: 'Dashboard Reports', icon: 'fas fa-chart-area', path: '/reports/dashboard' },
      { id: 'usage-reports', title: 'Usage Reports', icon: 'fas fa-chart-column', path: '/reports/usage' },
      { id: 'custom-reports', title: 'Custom Reports', icon: 'fas fa-edit', path: '/reports/custom' }
    ]
  },
  {
    id: 'users',
    title: 'User Management',
    icon: 'fas fa-users',
    path: '/users',
    children: [
      { id: 'user-list', title: 'User List', icon: 'fas fa-user-friends', path: '/users/list' },
      { id: 'user-history', title: 'Login History', icon: 'fas fa-user-friends', path: '/users/history' },
      { id: 'roles', title: 'Role Management', icon: 'fas fa-user-tag', path: '/users/roles' },
      { id: 'permissions', title: 'Permission Management', icon: 'fas fa-shield-alt', path: '/users/permissions' }
    ]
  },
  {
    id: 'settings',
    title: 'System Settings',
    icon: 'fas fa-cog',
    path: '/settings',
    children: [
      { id: 'general', title: 'General Settings', icon: 'fas fa-sliders-h', path: '/settings/general' },
      { id: 'network', title: 'Network Settings', icon: 'fas fa-wifi', path: '/settings/network' },
      { id: 'backup', title: 'Backup Settings', icon: 'fas fa-save', path: '/settings/backup' },
      { id: 'security', title: 'Security Settings', icon: 'fas fa-lock', path: '/settings/security' }
    ]
  }
];

const MainLayout: React.FC = () => {
  const [currentPage, setCurrentPage] = useState('dashboard');
  const [expandedMenus, setExpandedMenus] = useState<string[]>(['dashboard']);
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false);

  const toggleMenu = (menuId: string) => {
    setExpandedMenus(prev =>
      prev.includes(menuId)
        ? prev.filter(id => id !== menuId)
        : [...prev, menuId]
    );
  };

  const renderMenuItem = (item: MenuItem, level: number = 0) => {
    const isExpanded = expandedMenus.includes(item.id);
    const isActive = currentPage === item.id;
    const hasChildren = item.children && item.children.length > 0;

    return (
      <li key={item.id} className={`menu-item level-${level}`}>
        <div
          className={`menu-link ${isActive ? 'active' : ''} ${hasChildren ? 'has-children' : ''}`}
          onClick={() => {
            if (hasChildren) {
              toggleMenu(item.id);
            } else {
              setCurrentPage(item.id);
            }
          }}
        >
          <div className="menu-icon">
            <i className={item.icon}></i>
          </div>
          {!sidebarCollapsed && (
            <>
              <span className="menu-title">{item.title}</span>
              {hasChildren && (
                <i className={`menu-arrow fas fa-chevron-${isExpanded ? 'down' : 'right'}`}></i>
              )}
            </>
          )}
        </div>

        {hasChildren && isExpanded && !sidebarCollapsed && (
          <ul className="submenu">
            {item.children?.map(child => renderMenuItem(child, level + 1))}
          </ul>
        )}
      </li>
    );
  };

  const renderContent = () => {
    switch (currentPage) {
      case 'dashboard':
        return <Dashboard />;
      case 'device-list':
        return <DeviceList />;
      case 'data-explorer':
        return <DataExplorer />;
      case 'realtime':
        return <RealTimeMonitor />;
      case 'historical':
        return <HistoricalData />;
      case 'virtual-points':
        return <VirtualPoints />;
      case 'data-export':
        return <DataExport />;
      case 'active-alarms':
        return <ActiveAlarms />;
      case 'alarm-history':
        return <AlarmHistory />;
      case 'alarm-config':
        return <AlarmSettings />;
      case 'alarm-type':
        return <AlarmRuleTemplates />;
      case 'alarm-rules':
        return <AlarmRuleTemplates />;
      case 'notification':
        return <AlarmSettings />;
      case 'system-status':
        return <SystemStatus />;
      case 'user-list':
        return <UserManagement />;
      case 'user-history':
        return <LoginHistory />;
      case 'network':
        return <NetworkSettings />;
      case 'permissions':
        return <PermissionManagement />;
      case 'backup':
        return <BackupRestore />;


    }
  };

  return (
    <div className="app-layout">
      {/* 사이드바 */}
      <div className={`sidebar ${sidebarCollapsed ? 'collapsed' : ''}`}>
        <div className="sidebar-header">
          <div className="logo">
            <i className="fas fa-bolt text-primary"></i>
            {!sidebarCollapsed && <span className="logo-text">PulseOne</span>}
          </div>
          <button
            className="sidebar-toggle"
            onClick={() => setSidebarCollapsed(!sidebarCollapsed)}
          >
            <i className={`fas fa-${sidebarCollapsed ? 'expand' : 'compress'}-alt`}></i>
          </button>
        </div>

        <nav className="sidebar-nav">
          <ul className="menu">
            {menuItems.map(item => renderMenuItem(item))}
          </ul>
        </nav>
      </div>

      {/* 메인 컨텐츠 */}
      <div className={`main-layout ${sidebarCollapsed ? 'sidebar-collapsed' : ''}`}>
        {/* 상단바 */}
        <div className="topbar">
          <div className="topbar-left">
            <div className="breadcrumb">
              <span className="breadcrumb-item active">
                {menuItems.find(item => item.id === currentPage)?.title || '대시보드'}
              </span>
            </div>
          </div>

          <div className="topbar-right">
            <div className="connection-status">
              <div className="live-indicator"></div>
              <span className="status-text">실시간 연결됨</span>
            </div>

            <div className="user-menu">
              <div className="user-avatar">
                <i className="fas fa-user"></i>
              </div>
              <span className="user-name">Admin</span>
              <i className="fas fa-chevron-down"></i>
            </div>
          </div>
        </div>

        {/* 페이지 컨텐츠 */}
        <div className="page-container">
          {renderContent()}
        </div>
      </div>
    </div>
  );
};

export default MainLayout;