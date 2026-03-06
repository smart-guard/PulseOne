// ============================================================================
// frontend/src/layouts/MainLayout.tsx
// 프로토콜 관리 메뉴를 추가한 실제 업데이트 버전
// ============================================================================

import React, { useState } from 'react';
import { Outlet, Link, useLocation } from 'react-router-dom';
import { useTranslation } from 'react-i18next';
import { useAlarmContext } from '../contexts/AlarmContext';
import { TenantSelector } from '../components/common/TenantSelector';
import { SUPPORTED_LANGUAGES } from '../i18n';
import '../styles/base.css';

export const MainLayout: React.FC = () => {
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false);
  const [showLangMenu, setShowLangMenu] = useState(false);
  const location = useLocation();
  const { activeAlarmCount, criticalAlarmCount } = useAlarmContext();
  const { t, i18n } = useTranslation(['sidebar', 'common']);

  const currentLang = SUPPORTED_LANGUAGES.find(l => l.code === i18n.language)
    || SUPPORTED_LANGUAGES.find(l => i18n.language.startsWith(l.code))
    || SUPPORTED_LANGUAGES[0];

  const handleLangChange = (code: string) => {
    i18n.changeLanguage(code);
    setShowLangMenu(false);
  };

  // 현재 활성 메뉴 확인 함수
  const isActiveMenu = (path: string) => {
    if (path === '/dashboard') {
      return location.pathname === '/' || location.pathname === '/dashboard';
    }
    if (path === '/devices') {
      return location.pathname === '/devices' ||
        (location.pathname.startsWith('/devices/') &&
          !location.pathname.startsWith('/devices/templates') &&
          !location.pathname.startsWith('/devices/manufacturers') &&
          !location.pathname.startsWith('/devices/sites'));
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
      {/* 사이드바 */}
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
            {/* 대시보드 */}
            <li className="menu-item">
              <Link
                to="/dashboard"
                className={`menu-link ${isActiveMenu('/dashboard') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-tachometer-alt"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.dashboard')}</span>
              </Link>
            </li>

            {/* 디바이스 관리 */}
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
                  {t('sidebar:menu.devices')}
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
                <span className="menu-title">{t('sidebar:menu.deviceList')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/devices/sites"
                className={`menu-link ${isActiveSubMenu('/devices/sites') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-map-marker-alt"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.sites')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/devices/templates"
                className={`menu-link ${isActiveSubMenu('/devices/templates') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-file-invoice"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.templates')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/devices/manufacturers"
                className={`menu-link ${isActiveSubMenu('/devices/manufacturers') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-industry"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.manufacturers')}</span>
              </Link>
            </li>

            {/* 🆕 프로토콜 관리 - 새로 추가 */}
            <li className="menu-item">
              <Link
                to="/protocols"
                className={`menu-link ${isActiveMenu('/protocols') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-plug"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.protocols')}</span>
              </Link>
            </li>

            {/* 외부 연결 (Export Gateway) */}
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
                  {t('sidebar:menu.export')}
                </div>
              )}
            </li>
            <li className="menu-item">
              <Link
                to="/system/export-gateways"
                className={`menu-link ${isActiveMenu('/system/export-gateways') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-satellite-dish"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.exportGateway')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/modbus-slave"
                className={`menu-link ${isActiveMenu('/system/modbus-slave') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-network-wired"></i>
                </div>
                <span className="menu-title">Modbus Slave</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/export-history"
                className={`menu-link ${isActiveMenu('/system/export-history') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-history"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.exportHistory')}</span>
              </Link>
            </li>

            {/* 데이터 관리 */}
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
                  {t('sidebar:menu.data')}
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
                <span className="menu-title">{t('sidebar:menu.explorer')}</span>
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
                <span className="menu-title">{t('sidebar:menu.realtime')}</span>
                {!sidebarCollapsed && (
                  <span style={{
                    marginLeft: '6px', padding: '1px 5px', fontSize: '9px', fontWeight: 700,
                    background: '#dbeafe', color: '#1d4ed8', borderRadius: '4px',
                    letterSpacing: '0.02em', flexShrink: 0
                  }}>{t('sidebar:menu.control')}</span>
                )}
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/data/control-schedule"
                className={`menu-link ${isActiveSubMenu('/data/control-schedule') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-calendar-alt"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.controlSchedule')}</span>
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
                <span className="menu-title">{t('sidebar:menu.historical')}</span>
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
                <span className="menu-title">{t('sidebar:menu.virtualPoints')}</span>
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
                <span className="menu-title">{t('sidebar:menu.dataExport')}</span>
              </Link>
            </li>

            {/* 알람 관리 */}
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
                  {t('sidebar:menu.alarms')}
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
                <span className="menu-title">{t('sidebar:menu.activeAlarms')}</span>
                {!sidebarCollapsed && activeAlarmCount > 0 && (
                  <span
                    className="status status-error"
                    style={{
                      marginLeft: 'auto',
                      padding: '2px 6px',
                      fontSize: '10px',
                      borderRadius: '10px',
                      background: criticalAlarmCount > 0 ? '#ef4444' : '#f59e0b',
                      color: 'white',
                      minWidth: '18px',
                      textAlign: 'center'
                    }}
                  >
                    {activeAlarmCount}
                  </span>
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
                <span className="menu-title">{t('sidebar:menu.alarmHistory')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/alarms/settings/table"
                className={`menu-link ${location.pathname.startsWith('/alarms/settings') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-sliders-h"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.alarmSettings')}</span>
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
                <span className="menu-title">{t('sidebar:menu.alarmRules')}</span>
              </Link>
            </li>

            {/* 시스템 관리 */}
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
                  {t('sidebar:menu.system')}
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
                <span className="menu-title">{t('sidebar:menu.systemStatus')}</span>
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
                <span className="menu-title">{t('sidebar:menu.userManagement')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/tenants"
                className={`menu-link ${isActiveSubMenu('/system/tenants') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-building"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.tenantManagement')}</span>
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
                <span className="menu-title">{t('sidebar:menu.permissionManagement')}</span>
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
                <span className="menu-title">{t('sidebar:menu.backup')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/database"
                className={`menu-link ${isActiveSubMenu('/system/database') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-table"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.dbExplorer')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/config"
                className={`menu-link ${isActiveSubMenu('/system/config') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-cogs"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.configEditor')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/settings/general"
                className={`menu-link ${location.pathname.startsWith('/system/settings') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-tools"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.systemSettings')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/audit-logs"
                className={`menu-link ${isActiveSubMenu('/system/audit-logs') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-history"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.auditLog')}</span>
              </Link>
            </li>
            <li className="menu-item">
              <Link
                to="/system/redis-manager"
                className={`menu-link ${isActiveSubMenu('/system/redis-manager') ? 'active' : ''}`}
              >
                <div className="menu-icon">
                  <i className="fas fa-database"></i>
                </div>
                <span className="menu-title">{t('sidebar:menu.redisManager')}</span>
              </Link>
            </li>
          </ul>
        </nav>
      </div>

      {/* 메인 콘텐츠 */}
      <div className={`main-layout ${sidebarCollapsed ? 'sidebar-collapsed' : ''}`}>
        {/* 상단바 */}
        <div className="topbar">
          <div className="topbar-left">
            <div className="breadcrumb">
              {location.pathname === '/' || location.pathname === '/dashboard' ? t('sidebar:menu.dashboard') :
                location.pathname === '/devices/manufacturers' ? t('sidebar:menu.manufacturers') :
                  location.pathname === '/devices/templates' ? t('sidebar:menu.templates') :
                    location.pathname === '/devices/sites' ? t('sidebar:menu.sites') :
                      location.pathname === '/system/tenants' ? t('sidebar:menu.tenantManagement') :
                        location.pathname === '/system/export-gateways' ? t('sidebar:menu.export') + ' > ' + t('sidebar:menu.exportGateway') :
                          location.pathname === '/system/export-history' ? t('sidebar:menu.export') + ' > ' + t('sidebar:menu.exportHistory') :
                            location.pathname.startsWith('/devices') ? t('sidebar:menu.devices') :
                              location.pathname.includes('/protocols') ? t('sidebar:menu.protocols') :
                                location.pathname.includes('/data') ? t('sidebar:menu.data') :
                                  location.pathname.includes('/alarms') ? t('sidebar:menu.alarms') :
                                    location.pathname.includes('/system') ? t('sidebar:menu.system') : ''}
            </div>
          </div>

          <div className="topbar-right">
            <TenantSelector />
            <div className="connection-status">
              <div className="live-indicator"></div>
              <span className="status-text">{t('common:realtimeConnected')}</span>
            </div>

            <button className="btn btn-outline btn-sm" title="알림" style={{
              position: 'relative',
              padding: '8px'
            }}>
              <i className="fas fa-bell"></i>
              {activeAlarmCount > 0 && (
                <span style={{
                  position: 'absolute',
                  top: '-2px',
                  right: '-2px',
                  background: criticalAlarmCount > 0 ? '#ef4444' : '#f59e0b',
                  color: 'white',
                  borderRadius: '50%',
                  fontSize: '10px',
                  padding: '2px 4px',
                  minWidth: '16px',
                  textAlign: 'center'
                }}>
                  {activeAlarmCount > 99 ? '99+' : activeAlarmCount}
                </span>
              )}
            </button>

            {/* 🌐 언어 전환 드롭다운 */}
            <div style={{ position: 'relative' }}>
              <button
                onClick={() => setShowLangMenu(v => !v)}
                className="btn btn-outline btn-sm"
                title={currentLang.name}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  padding: '6px 12px',
                  fontSize: '13px',
                  backgroundColor: 'white',
                  borderColor: '#e5e7eb',
                  color: '#374151'
                }}
              >
                <span style={{ fontSize: '16px', lineHeight: 1 }}>{currentLang.flag}</span>
                <span style={{ fontSize: '13px', fontWeight: 600 }}>{currentLang.code.toUpperCase().substring(0, 2)}</span>
                <i className="fas fa-chevron-down" style={{ fontSize: '10px', color: '#9ca3af', marginLeft: '2px' }} />
              </button>

              {showLangMenu && (
                <>
                  {/* 배경 클릭 시 닫기 */}
                  <div
                    style={{ position: 'fixed', inset: 0, zIndex: 999 }}
                    onClick={() => setShowLangMenu(false)}
                  />
                  <div style={{
                    position: 'absolute', top: 'calc(100% + 6px)', right: 0,
                    background: '#fff', border: '1px solid #e5e7eb',
                    borderRadius: '8px', boxShadow: '0 4px 12px rgba(0,0,0,.08)',
                    zIndex: 1000, minWidth: '140px', overflow: 'hidden', padding: '4px'
                  }}>
                    {SUPPORTED_LANGUAGES.map(lang => (
                      <button
                        key={lang.code}
                        onClick={() => handleLangChange(lang.code)}
                        style={{
                          width: '100%', display: 'flex', alignItems: 'center', gap: '8px',
                          padding: '8px 12px', border: 'none', borderRadius: '4px',
                          background: i18n.language === lang.code || i18n.language.startsWith(lang.code)
                            ? '#f3f4f6' : 'transparent',
                          color: '#374151',
                          cursor: 'pointer', fontSize: '13px', fontWeight:
                            i18n.language === lang.code ? 600 : 400,
                          textAlign: 'left'
                        }}
                      >
                        <span style={{ fontSize: '15px' }}>{lang.flag}</span>
                        <span>{lang.name}</span>
                        {(i18n.language === lang.code || i18n.language.startsWith(lang.code)) && (
                          <i className="fas fa-check" style={{ marginLeft: 'auto', fontSize: '12px', color: '#3b82f6' }} />
                        )}
                      </button>
                    ))}
                  </div>
                </>
              )}
            </div>


            <Link to="/system/settings/general" className="btn btn-outline btn-sm" title="설정">
              <i className="fas fa-cog"></i>
            </Link>

            <div className="user-menu">
              <div className="user-avatar">
                <i className="fas fa-user"></i>
              </div>
              <span className="user-name">{t('common:admin')}</span>
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