// ============================================================================
// frontend/src/App.tsx
// GitHub 구조 기반 라우트 + ConfirmProvider + AlarmProvider 통합 완성본
// ============================================================================

import React from 'react';
import { BrowserRouter as Router, Routes, Route, Navigate } from 'react-router-dom';
import { MainLayout } from './layouts/MainLayout';
import { ConfirmProvider } from './components/common/ConfirmProvider';
import { AlarmProvider } from './contexts/AlarmContext';

// 페이지 컴포넌트들 import
import Dashboard from './pages/Dashboard';
import DeviceList from './pages/DeviceList';
import DataExplorer from './pages/DataExplorer';
import RealTimeMonitor from './pages/RealTimeMonitor';
import HistoricalData from './pages/HistoricalData';
import VirtualPoints from './pages/VirtualPoints';
import DataExport from './pages/DataExport';
import ActiveAlarms from './pages/ActiveAlarms';
import AlarmHistory from './pages/AlarmHistory';
import AlarmSettings from './pages/AlarmSettings';
import AlarmRuleTemplates from './pages/AlarmRuleTemplates';
import SystemStatus from './pages/SystemStatus';
import UserManagement from './pages/UserManagement';
import PermissionManagement from './pages/PermissionManagement';
import BackupRestore from './pages/BackupRestore';
import ProtocolManagement from './pages/ProtocolManagement';
import DeviceTemplatesPage from './pages/DeviceTemplatesPage';
import ManufacturerManagementPage from './pages/ManufacturerManagementPage';
import TenantManagementPage from './pages/TenantManagementPage';
import SiteManagementPage from './pages/SiteManagementPage';
import AuditLogPage from './pages/AuditLogPage';
import DatabaseExplorerPage from './pages/DatabaseExplorer';
import ConfigEditorPage from './pages/ConfigEditor';
import ExportGatewaySettings from './pages/ExportGatewaySettings';
const App: React.FC = () => {
  // 🛠️ 개발 환경 초기화: 더미 토큰 설정
  React.useEffect(() => {
    if (process.env.NODE_ENV === 'development') {
      const token = localStorage.getItem('auth_token');
      if (!token) {
        console.log('🛠️ [DEV] 더미 인증 토큰 설정됨');
        localStorage.setItem('auth_token', 'dev-dummy-token');
      }
    }
  }, []);

  return (
    <ConfirmProvider>
      <AlarmProvider>
        <Router>
          <div className="app">
            <Routes>
              {/* 메인 레이아웃을 사용하는 모든 페이지들 */}
              <Route path="/" element={<MainLayout />}>
                {/* 기본 경로는 대시보드로 리다이렉트 */}
                <Route index element={<Navigate to="/dashboard" replace />} />

                {/* 대시보드 */}
                <Route path="dashboard" element={<Dashboard />} />

                {/* 🆕 프로토콜 관리 - 새로 추가된 라우트 */}
                <Route path="protocols" element={<ProtocolManagement />} />

                {/* 🆕 디바이스 마스터 모델 & 제조사 관리 */}
                <Route path="devices">
                  <Route index element={<DeviceList />} />
                  <Route path="sites" element={<SiteManagementPage />} />
                  <Route path="templates" element={<DeviceTemplatesPage />} />
                  <Route path="manufacturers" element={<ManufacturerManagementPage />} />
                </Route>

                {/* 데이터 관리 */}
                <Route path="data">
                  <Route path="explorer" element={<DataExplorer />} />
                  <Route path="realtime" element={<RealTimeMonitor />} />
                  <Route path="historical" element={<HistoricalData />} />
                  <Route path="virtual-points" element={<VirtualPoints />} />
                  <Route path="export" element={<DataExport />} />
                  {/* 데이터 하위 경로 기본값 */}
                  <Route index element={<Navigate to="explorer" replace />} />
                </Route>

                {/* 알람 관리 - rules 경로 추가 */}
                <Route path="alarms">
                  <Route path="active" element={<ActiveAlarms />} />
                  <Route path="history" element={<AlarmHistory />} />
                  <Route path="settings" element={<AlarmSettings />} />
                  <Route path="rules" element={<AlarmRuleTemplates />} />
                  {/* 알람 하위 경로 기본값 */}
                  <Route index element={<Navigate to="active" replace />} />
                </Route>

                {/* 시스템 관리 */}
                <Route path="system">
                  <Route path="status" element={<SystemStatus />} />
                  <Route path="users" element={<UserManagement />} />
                  <Route path="tenants" element={<TenantManagementPage />} />
                  <Route path="permissions" element={<PermissionManagement />} />
                  <Route path="backup" element={<BackupRestore />} />
                  <Route path="database" element={<DatabaseExplorerPage />} />
                  <Route path="config" element={<ConfigEditorPage />} />
                  <Route path="export-gateways" element={<ExportGatewaySettings />} />
                  {/* 시스템 하위 경로 기본값 */}
                  <Route index element={<Navigate to="status" replace />} />
                </Route>

                {/* 404 페이지 */}
                <Route path="*" element={<NotFound />} />
              </Route>
            </Routes>
          </div>
        </Router>
      </AlarmProvider>
    </ConfirmProvider>
  );
};

// 404 페이지 컴포넌트
const NotFound: React.FC = () => {
  return (
    <div className="not-found-container" style={{
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      justifyContent: 'center',
      height: '50vh',
      textAlign: 'center',
      gap: '20px',
      fontFamily: '"Noto Sans KR", "Malgun Gothic", "Apple SD Gothic Neo", sans-serif'
    }}>
      <div className="not-found-content">
        <div style={{ marginBottom: '20px' }}>
          <i className="fas fa-exclamation-triangle" style={{
            fontSize: '4rem',
            color: '#ef4444',
            marginBottom: '16px'
          }}></i>
        </div>
        <h1 style={{
          fontSize: '4rem',
          margin: '0',
          color: '#ef4444',
          fontWeight: 700
        }}>404</h1>
        <h2 style={{
          fontSize: '1.5rem',
          margin: '16px 0 8px 0',
          color: '#374151',
          fontWeight: 600
        }}>페이지를 찾을 수 없습니다</h2>
        <p style={{
          color: '#6b7280',
          margin: '10px 0',
          fontSize: '1rem',
          maxWidth: '400px'
        }}>
          요청하신 페이지가 존재하지 않거나 이동되었습니다.<br />
          URL을 확인하시거나 아래 버튼을 사용해주세요.
        </p>
        <div style={{
          display: 'flex',
          gap: '12px',
          justifyContent: 'center',
          marginTop: '24px',
          flexWrap: 'wrap'
        }}>
          <button
            onClick={() => window.history.back()}
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '8px',
              padding: '12px 20px',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              background: '#f9fafb',
              color: '#374151',
              cursor: 'pointer',
              fontSize: '0.875rem',
              fontWeight: 500,
              transition: 'all 0.2s ease',
              textDecoration: 'none'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = '#f3f4f6';
              e.currentTarget.style.borderColor = '#9ca3af';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = '#f9fafb';
              e.currentTarget.style.borderColor = '#d1d5db';
            }}
          >
            <i className="fas fa-arrow-left"></i>
            이전 페이지로
          </button>
          <button
            onClick={() => window.location.href = '/dashboard'}
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '8px',
              padding: '12px 20px',
              border: 'none',
              borderRadius: '8px',
              background: '#3b82f6',
              color: 'white',
              cursor: 'pointer',
              fontSize: '0.875rem',
              fontWeight: 500,
              transition: 'all 0.2s ease',
              textDecoration: 'none'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = '#2563eb';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = '#3b82f6';
            }}
          >
            <i className="fas fa-home"></i>
            대시보드로
          </button>
        </div>
      </div>
    </div>
  );
};

export default App;