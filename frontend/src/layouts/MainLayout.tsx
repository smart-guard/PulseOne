// ============================================================================
// frontend/src/layouts/MainLayout.tsx
// 🔥 기존 2depth 메뉴 구조 100% 복원 - 섹션별 서브메뉴
// ============================================================================

import React, { useState } from 'react';
import { Outlet, Link, useLocation } from 'react-router-dom';
import '../styles/base.css';

export const MainLayout: React.FC = () => {
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false);
  const location = useLocation();

  // 현재 활성 메뉴 확인 함수
  const isActiveMenu = (path: string) => {
    if (path === '/dashboard') {
      return location.pathname === '/' || location.pathname === '/dashboard';
    }
    return location.pathname.startsWith(path);
  };

  // 현재 활성 서브메뉴 확인 함수
  const isActiveSubMenu = (path: string) => {
    return location.pathname === path;
  };

  const toggleSidebar = () => {
    setSidebarCollapsed(!sidebarCollapsed);
  };

  return (
    <div className="app-layout">
      {/* 사이드바 - 기존 base.css 클래스 100% 활용 */}
      <div className={`sidebar ${sidebarCollapsed ? 'collapsed' : ''}`}>
        <div className="sidebar-header">
          <div className="logo">
            <i className="fas fa-bolt text-primary"></i>
            {!sidebarCollapsed && <span className="logo-text">PulseOne</span>}
          </div>
          <button 
            className="sidebar-toggle"
            onClick={toggleSidebar}
            aria-label="메뉴 토글"
          >
            <i className="fas fa-bars"></i>
          </button>
        </div>
        
        <nav className="sidebar-nav">
          <ul className="menu">
            {/* 대시보드 - 1depth */}
            <li className="menu-item">
              <Link 
                to="/dashboard" 
                className={`menu-link ${isActiveMenu('/dashboard') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-tachometer-alt"></i>
                </div>
                <span className="menu-title">대시보드</span>
              </Link>
            </li>

            {/* 디바이스 관리 - 2depth 구조 */}
            <li className="menu-item">
              {!sidebarCollapsed && (
                <div style={{
                  padding: '12px 16px 8px 16px',
                  fontSize: '12px',
                  fontWeight: '600',
                  color: '#9ca3af',
                  textTransform: 'uppercase',
                  letterSpacing: '0.05em',
                  borderBottom: '1px solid #f1f5f9',
                  marginBottom: '8px'
                }}>
                  디바이스 관리
                </div>
              )}
            </li>
            <li className="menu-item">
              <Link 
                to="/devices" 
                className={`menu-link ${isActiveMenu('/devices') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-network-wired"></i>
                </div>
                <span className="menu-title">디바이스 목록</span>
              </Link>
            </li>

            {/* 데이터 관리 - 2depth 구조 */}
            <li className="menu-item">
              {!sidebarCollapsed && (
                <div style={{
                  padding: '16px 16px 8px 16px',
                  fontSize: '12px',
                  fontWeight: '600',
                  color: '#9ca3af',
                  textTransform: 'uppercase',
                  letterSpacing: '0.05em',
                  borderBottom: '1px solid #f1f5f9',
                  marginBottom: '8px'
                }}>
                  데이터 관리
                </div>
              )}
            </li>
            <li className="menu-item">
              <Link 
                to="/data/explorer" 
                className={`menu-link ${isActiveSubMenu('/data/explorer') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-search"></i>
                </div>
                <span className="menu-title">실시간 데이터 탐색기</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/data/realtime" 
                className={`menu-link ${isActiveSubMenu('/data/realtime') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-chart-line"></i>
                </div>
                <span className="menu-title">실시간 모니터</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/data/historical" 
                className={`menu-link ${isActiveSubMenu('/data/historical') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-history"></i>
                </div>
                <span className="menu-title">이력 데이터</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/data/virtual-points" 
                className={`menu-link ${isActiveSubMenu('/data/virtual-points') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-code-branch"></i>
                </div>
                <span className="menu-title">가상 포인트</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/data/export" 
                className={`menu-link ${isActiveSubMenu('/data/export') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-download"></i>
                </div>
                <span className="menu-title">데이터 내보내기</span>
              </Link>
            </li>

            {/* 알람 관리 - 2depth 구조 */}
            <li className="menu-item">
              {!sidebarCollapsed && (
                <div style={{
                  padding: '16px 16px 8px 16px',
                  fontSize: '12px',
                  fontWeight: '600',
                  color: '#9ca3af',
                  textTransform: 'uppercase',
                  letterSpacing: '0.05em',
                  borderBottom: '1px solid #f1f5f9',
                  marginBottom: '8px'
                }}>
                  알람 관리
                </div>
              )}
            </li>
            <li className="menu-item">
              <Link 
                to="/alarms/active" 
                className={`menu-link ${isActiveSubMenu('/alarms/active') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-exclamation-triangle"></i>
                </div>
                <span className="menu-title">활성 알람</span>
                {!sidebarCollapsed && (
                  <span className="status status-error" style={{ 
                    marginLeft: 'auto',
                    padding: '2px 6px',
                    fontSize: '10px',
                    borderRadius: '10px'
                  }}>5</span>
                )}
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/alarms/history" 
                className={`menu-link ${isActiveSubMenu('/alarms/history') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-clock"></i>
                </div>
                <span className="menu-title">알람 이력</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/alarms/settings" 
                className={`menu-link ${isActiveSubMenu('/alarms/settings') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-sliders-h"></i>
                </div>
                <span className="menu-title">알람 설정</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/alarms/rules" 
                className={`menu-link ${isActiveSubMenu('/alarms/rules') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-gavel"></i>
                </div>
                <span className="menu-title">알람 규칙</span>
              </Link>
            </li>

            {/* 시스템 관리 - 2depth 구조 */}
            <li className="menu-item">
              {!sidebarCollapsed && (
                <div style={{
                  padding: '16px 16px 8px 16px',
                  fontSize: '12px',
                  fontWeight: '600',
                  color: '#9ca3af',
                  textTransform: 'uppercase',
                  letterSpacing: '0.05em',
                  borderBottom: '1px solid #f1f5f9',
                  marginBottom: '8px'
                }}>
                  시스템 관리
                </div>
              )}
            </li>
            <li className="menu-item">
              <Link 
                to="/system/status" 
                className={`menu-link ${isActiveSubMenu('/system/status') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-server"></i>
                </div>
                <span className="menu-title">시스템 상태</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/system/users" 
                className={`menu-link ${isActiveSubMenu('/system/users') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-users"></i>
                </div>
                <span className="menu-title">사용자 관리</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/system/permissions" 
                className={`menu-link ${isActiveSubMenu('/system/permissions') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-shield-alt"></i>
                </div>
                <span className="menu-title">권한 관리</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link 
                to="/system/backup" 
                className={`menu-link ${isActiveSubMenu('/system/backup') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-database"></i>
                </div>
                <span className="menu-title">백업/복원</span>
              </Link>
            </li>
          </ul>
        </nav>
      </div>

      {/* 메인 콘텐츠 - 기존 base.css 클래스 100% 활용 */}
      <div className={`main-layout ${sidebarCollapsed ? 'sidebar-collapsed' : ''}`}>
        {/* 상단바 */}
        <div className="topbar">
          <div className="topbar-left">
            <div className="breadcrumb">
              <span className="breadcrumb-item active">
                {location.pathname === '/' || location.pathname === '/dashboard' ? '대시보드' : 
                 location.pathname.includes('/devices') ? '디바이스 관리' :
                 location.pathname.includes('/data') ? '데이터 관리' :
                 location.pathname.includes('/alarms') ? '알람 관리' :
                 location.pathname.includes('/system') ? '시스템 관리' : '페이지'}
              </span>
            </div>
          </div>
          
          <div className="topbar-right">
            <div className="connection-status">
              <div className="live-indicator"></div>
              <span className="status-text">실시간 연결됨</span>
            </div>
            
            <button className="btn btn-outline btn-sm" title="알림" style={{
              position: 'relative',
              padding: '8px'
            }}>
              <i className="fas fa-bell"></i>
              <span style={{
                position: 'absolute',
                top: '-2px',
                right: '-2px',
                background: '#ef4444',
                color: 'white',
                borderRadius: '50%',
                fontSize: '10px',
                padding: '2px 4px',
                minWidth: '16px',
                textAlign: 'center'
              }}>3</span>
            </button>
            
            <button className="btn btn-outline btn-sm" title="설정">
              <i className="fas fa-cog"></i>
            </button>
            
            <div className="user-menu">
              <div className="user-avatar">
                <i className="fas fa-user"></i>
              </div>
              <span className="user-name">관리자</span>
              <i className="fas fa-chevron-down" style={{ fontSize: '12px' }}></i>
            </div>
          </div>
        </div>

        {/* 페이지 컨텐츠 */}
        <div className="page-container">
          <Outlet />
        </div>
      </div>
    </div>
  );
};