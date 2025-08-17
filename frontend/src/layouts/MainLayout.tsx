// ============================================================================
// frontend/src/layouts/MainLayout.tsx
// CSS import 제거하고 인라인 스타일만 사용
// ============================================================================

import React, { useState } from 'react';
import { Outlet, Link, useLocation } from 'react-router-dom';

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
    <div className="main-layout">
      {/* 헤더 */}
      <header className="main-header" style={{
        height: '60px',
        background: '#ffffff',
        borderBottom: '1px solid #e5e7eb',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '0 24px',
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        zIndex: 1000
      }}>
        <div className="header-left" style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <button 
            className="sidebar-toggle"
            onClick={toggleSidebar}
            aria-label="메뉴 토글"
            style={{
              background: 'none',
              border: 'none',
              fontSize: '18px',
              cursor: 'pointer',
              padding: '8px'
            }}
          >
            <i className="fas fa-bars"></i>
          </button>
          <div className="logo" style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <i className="fas fa-bolt" style={{ color: '#3b82f6', fontSize: '24px' }}></i>
            <span style={{ fontSize: '20px', fontWeight: '700', color: '#111827' }}>PulseOne</span>
          </div>
        </div>
        
        <div className="header-right">
          <div className="header-actions" style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <button className="header-btn" title="알림" style={{
              background: 'none',
              border: 'none',
              fontSize: '16px',
              cursor: 'pointer',
              padding: '8px',
              position: 'relative'
            }}>
              <i className="fas fa-bell"></i>
              <span className="badge" style={{
                position: 'absolute',
                top: '2px',
                right: '2px',
                background: '#ef4444',
                color: 'white',
                borderRadius: '50%',
                fontSize: '10px',
                padding: '2px 4px',
                minWidth: '16px',
                textAlign: 'center'
              }}>3</span>
            </button>
            <button className="header-btn" title="설정" style={{
              background: 'none',
              border: 'none',
              fontSize: '16px',
              cursor: 'pointer',
              padding: '8px'
            }}>
              <i className="fas fa-cog"></i>
            </button>
            <div className="user-menu">
              <button className="user-avatar" style={{
                background: 'none',
                border: 'none',
                fontSize: '24px',
                cursor: 'pointer',
                padding: '4px',
                color: '#6b7280'
              }}>
                <i className="fas fa-user-circle"></i>
              </button>
            </div>
          </div>
        </div>
      </header>

      {/* 사이드바 */}
      <aside className={`sidebar ${sidebarCollapsed ? 'collapsed' : ''}`} style={{
        position: 'fixed',
        top: '60px',
        left: 0,
        height: 'calc(100vh - 60px)',
        width: sidebarCollapsed ? '60px' : '260px',
        background: '#f8fafc',
        borderRight: '1px solid #e5e7eb',
        transition: 'width 0.3s ease',
        overflowY: 'auto',
        zIndex: 999
      }}>
        <nav className="sidebar-nav" style={{ padding: '16px 0' }}>
          {/* 대시보드 */}
          <Link 
            to="/dashboard" 
            className={`nav-item ${isActiveMenu('/dashboard') ? 'active' : ''}`}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '12px',
              padding: '12px 20px',
              color: isActiveMenu('/dashboard') ? '#3b82f6' : '#6b7280',
              textDecoration: 'none',
              background: isActiveMenu('/dashboard') ? '#eff6ff' : 'transparent',
              borderRight: isActiveMenu('/dashboard') ? '3px solid #3b82f6' : 'none',
              transition: 'all 0.2s ease'
            }}
          >
            <i className="fas fa-tachometer-alt" style={{ fontSize: '16px', width: '16px' }}></i>
            {!sidebarCollapsed && <span>대시보드</span>}
          </Link>

          {/* 디바이스 관리 */}
          <div className="nav-section" style={{ margin: '16px 0' }}>
            {!sidebarCollapsed && (
              <div className="nav-section-title" style={{
                padding: '8px 20px',
                fontSize: '12px',
                fontWeight: '600',
                color: '#9ca3af',
                textTransform: 'uppercase',
                letterSpacing: '0.05em'
              }}>디바이스 관리</div>
            )}
            <Link 
              to="/devices" 
              className={`nav-item ${isActiveMenu('/devices') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveMenu('/devices') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveMenu('/devices') ? '#eff6ff' : 'transparent',
                borderRight: isActiveMenu('/devices') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-network-wired" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>디바이스 목록</span>}
            </Link>
          </div>

          {/* 데이터 관리 */}
          <div className="nav-section" style={{ margin: '16px 0' }}>
            {!sidebarCollapsed && (
              <div className="nav-section-title" style={{
                padding: '8px 20px',
                fontSize: '12px',
                fontWeight: '600',
                color: '#9ca3af',
                textTransform: 'uppercase',
                letterSpacing: '0.05em'
              }}>데이터 관리</div>
            )}
            <Link 
              to="/data/explorer" 
              className={`nav-item ${isActiveSubMenu('/data/explorer') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/data/explorer') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/data/explorer') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/data/explorer') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-search" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>데이터 탐색기</span>}
            </Link>
            <Link 
              to="/data/realtime" 
              className={`nav-item ${isActiveSubMenu('/data/realtime') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/data/realtime') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/data/realtime') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/data/realtime') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-chart-line" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>실시간 모니터</span>}
            </Link>
            <Link 
              to="/data/historical" 
              className={`nav-item ${isActiveSubMenu('/data/historical') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/data/historical') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/data/historical') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/data/historical') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-history" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>이력 데이터</span>}
            </Link>
            <Link 
              to="/data/virtual-points" 
              className={`nav-item ${isActiveSubMenu('/data/virtual-points') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/data/virtual-points') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/data/virtual-points') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/data/virtual-points') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-code-branch" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>가상 포인트</span>}
            </Link>
            <Link 
              to="/data/export" 
              className={`nav-item ${isActiveSubMenu('/data/export') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/data/export') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/data/export') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/data/export') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-download" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>데이터 내보내기</span>}
            </Link>
          </div>

          {/* 알람 관리 */}
          <div className="nav-section" style={{ margin: '16px 0' }}>
            {!sidebarCollapsed && (
              <div className="nav-section-title" style={{
                padding: '8px 20px',
                fontSize: '12px',
                fontWeight: '600',
                color: '#9ca3af',
                textTransform: 'uppercase',
                letterSpacing: '0.05em'
              }}>알람 관리</div>
            )}
            <Link 
              to="/alarms/active" 
              className={`nav-item ${isActiveSubMenu('/alarms/active') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/alarms/active') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/alarms/active') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/alarms/active') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-exclamation-triangle" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && (
                <>
                  <span>활성 알람</span>
                  <span style={{
                    background: '#ef4444',
                    color: 'white',
                    borderRadius: '12px',
                    fontSize: '10px',
                    padding: '2px 6px',
                    marginLeft: 'auto'
                  }}>5</span>
                </>
              )}
            </Link>
            <Link 
              to="/alarms/history" 
              className={`nav-item ${isActiveSubMenu('/alarms/history') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/alarms/history') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/alarms/history') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/alarms/history') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-clock" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>알람 이력</span>}
            </Link>
            <Link 
              to="/alarms/settings" 
              className={`nav-item ${isActiveSubMenu('/alarms/settings') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/alarms/settings') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/alarms/settings') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/alarms/settings') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-sliders-h" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>알람 설정</span>}
            </Link>
            <Link 
              to="/alarms/rules" 
              className={`nav-item ${isActiveSubMenu('/alarms/rules') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/alarms/rules') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/alarms/rules') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/alarms/rules') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-gavel" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>알람 규칙</span>}
            </Link>
          </div>

          {/* 시스템 관리 */}
          <div className="nav-section" style={{ margin: '16px 0' }}>
            {!sidebarCollapsed && (
              <div className="nav-section-title" style={{
                padding: '8px 20px',
                fontSize: '12px',
                fontWeight: '600',
                color: '#9ca3af',
                textTransform: 'uppercase',
                letterSpacing: '0.05em'
              }}>시스템 관리</div>
            )}
            <Link 
              to="/system/status" 
              className={`nav-item ${isActiveSubMenu('/system/status') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/system/status') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/system/status') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/system/status') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-server" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>시스템 상태</span>}
            </Link>
            <Link 
              to="/system/users" 
              className={`nav-item ${isActiveSubMenu('/system/users') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/system/users') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/system/users') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/system/users') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-users" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>사용자 관리</span>}
            </Link>
            <Link 
              to="/system/permissions" 
              className={`nav-item ${isActiveSubMenu('/system/permissions') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/system/permissions') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/system/permissions') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/system/permissions') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-shield-alt" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>권한 관리</span>}
            </Link>
            <Link 
              to="/system/backup" 
              className={`nav-item ${isActiveSubMenu('/system/backup') ? 'active' : ''}`}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '12px',
                padding: '12px 20px',
                color: isActiveSubMenu('/system/backup') ? '#3b82f6' : '#6b7280',
                textDecoration: 'none',
                background: isActiveSubMenu('/system/backup') ? '#eff6ff' : 'transparent',
                borderRight: isActiveSubMenu('/system/backup') ? '3px solid #3b82f6' : 'none',
                transition: 'all 0.2s ease'
              }}
            >
              <i className="fas fa-database" style={{ fontSize: '16px', width: '16px' }}></i>
              {!sidebarCollapsed && <span>백업/복원</span>}
            </Link>
          </div>
        </nav>
      </aside>

      {/* 메인 콘텐츠 */}
      <main className={`main-content ${sidebarCollapsed ? 'sidebar-collapsed' : ''}`} style={{
        marginLeft: sidebarCollapsed ? '60px' : '260px',
        marginTop: '60px',
        padding: '24px',
        minHeight: 'calc(100vh - 60px)',
        background: '#f8fafc',
        transition: 'margin-left 0.3s ease'
      }}>
        <Outlet />
      </main>
    </div>
  );
};