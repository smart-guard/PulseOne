import React, { useState } from 'react';
import Dashboard from '../../pages/Dashboard';
import SystemStatus from '../../pages/SystemStatus';
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
    title: '대시보드',
    icon: 'fas fa-tachometer-alt',
    path: '/dashboard'
  },
  {
    id: 'devices',
    title: '디바이스 관리',
    icon: 'fas fa-network-wired',
    path: '/devices',
    children: [
      { id: 'device-list', title: '디바이스 목록', icon: 'fas fa-list', path: '/devices/list' },
      { id: 'device-add', title: '디바이스 추가', icon: 'fas fa-plus', path: '/devices/add' },
      { id: 'device-groups', title: '디바이스 그룹', icon: 'fas fa-layer-group', path: '/devices/groups' },
      { id: 'protocols', title: '프로토콜 설정', icon: 'fas fa-cogs', path: '/devices/protocols' }
    ]
  },
  {
    id: 'data',
    title: '데이터 관리',
    icon: 'fas fa-database',
    path: '/data',
    children: [
      { id: 'realtime', title: '실시간 데이터', icon: 'fas fa-chart-line', path: '/data/realtime' },
      { id: 'historical', title: '이력 데이터', icon: 'fas fa-history', path: '/data/historical' },
      { id: 'virtual-points', title: '가상포인트', icon: 'fas fa-calculator', path: '/data/virtual-points' },
      { id: 'data-export', title: '데이터 내보내기', icon: 'fas fa-download', path: '/data/export' }
    ]
  },
  {
    id: 'alarms',
    title: '알람 관리',
    icon: 'fas fa-bell',
    path: '/alarms',
    children: [
      { id: 'active-alarms', title: '활성 알람', icon: 'fas fa-exclamation-triangle', path: '/alarms/active' },
      { id: 'alarm-history', title: '알람 이력', icon: 'fas fa-clock', path: '/alarms/history' },
      { id: 'alarm-config', title: '알람 설정', icon: 'fas fa-cog', path: '/alarms/config' },
      { id: 'notification', title: '알림 설정', icon: 'fas fa-envelope', path: '/alarms/notification' }
    ]
  },
  {
    id: 'monitoring',
    title: '시스템 모니터링',
    icon: 'fas fa-server',
    path: '/monitoring',
    children: [
      { id: 'system-status', title: '시스템 상태', icon: 'fas fa-heartbeat', path: '/monitoring/status' },
      { id: 'performance', title: '성능 지표', icon: 'fas fa-chart-bar', path: '/monitoring/performance' },
      { id: 'logs', title: '로그 조회', icon: 'fas fa-file-alt', path: '/monitoring/logs' },
      { id: 'health-check', title: '헬스 체크', icon: 'fas fa-stethoscope', path: '/monitoring/health' }
    ]
  },
  {
    id: 'reports',
    title: '보고서',
    icon: 'fas fa-chart-pie',
    path: '/reports',
    children: [
      { id: 'dashboard-reports', title: '대시보드 리포트', icon: 'fas fa-chart-area', path: '/reports/dashboard' },
      { id: 'usage-reports', title: '사용량 리포트', icon: 'fas fa-chart-column', path: '/reports/usage' },
      { id: 'custom-reports', title: '사용자 정의 리포트', icon: 'fas fa-edit', path: '/reports/custom' }
    ]
  },
  {
    id: 'users',
    title: '사용자 관리',
    icon: 'fas fa-users',
    path: '/users',
    children: [
      { id: 'user-list', title: '사용자 목록', icon: 'fas fa-user-friends', path: '/users/list' },
      { id: 'roles', title: '역할 관리', icon: 'fas fa-user-tag', path: '/users/roles' },
      { id: 'permissions', title: '권한 관리', icon: 'fas fa-shield-alt', path: '/users/permissions' }
    ]
  },
  {
    id: 'settings',
    title: '시스템 설정',
    icon: 'fas fa-cog',
    path: '/settings',
    children: [
      { id: 'general', title: '일반 설정', icon: 'fas fa-sliders-h', path: '/settings/general' },
      { id: 'network', title: '네트워크 설정', icon: 'fas fa-wifi', path: '/settings/network' },
      { id: 'backup', title: '백업 설정', icon: 'fas fa-save', path: '/settings/backup' },
      { id: 'security', title: '보안 설정', icon: 'fas fa-lock', path: '/settings/security' }
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
        return (
          <div className="page-content">
            <h2>디바이스 목록</h2>
            <p>연결된 디바이스들의 목록을 관리합니다.</p>
          </div>
        );
      case 'realtime':
        return (
          <div className="page-content">
            <h2>실시간 데이터</h2>
            <p>실시간으로 수집되는 데이터를 모니터링합니다.</p>
          </div>
        );
      case 'active-alarms':
        return (
          <div className="page-content">
            <h2>활성 알람</h2>
            <p>현재 발생한 알람들을 확인하고 처리합니다.</p>
          </div>
        );
      case 'system-status':
        return <SystemStatus />;
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
              <span className="user-name">관리자</span>
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