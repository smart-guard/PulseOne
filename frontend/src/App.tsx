// ============================================================================
// frontend/src/App.tsx
// React Router 완전 적용 - URL 변경 문제 해결
// ============================================================================

import React from 'react';
import { BrowserRouter as Router, Routes, Route, Navigate } from 'react-router-dom';
import { MainLayout } from './layouts/MainLayout';

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
import AlarmRules from './pages/AlarmRules';
import SystemStatus from './pages/SystemStatus';
import UserManagement from './pages/UserManagement';
import PermissionManagement from './pages/PermissionManagement';
import BackupRestore from './pages/BackupRestore';


const App: React.FC = () => {
  return (
    <Router>
      <div className="app">
        <Routes>
          {/* 메인 레이아웃을 사용하는 모든 페이지들 */}
          <Route path="/" element={<MainLayout />}>
            {/* 기본 경로는 대시보드로 리다이렉트 */}
            <Route index element={<Navigate to="/dashboard" replace />} />
            
            {/* 대시보드 */}
            <Route path="dashboard" element={<Dashboard />} />
            
            {/* 디바이스 관리 */}
            <Route path="devices" element={<DeviceList />} />
            
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
            
            {/* 알람 관리 */}
            <Route path="alarms">
              <Route path="active" element={<ActiveAlarms />} />
              <Route path="history" element={<AlarmHistory />} />
              <Route path="settings" element={<AlarmSettings />} />
              <Route path="rules" element={<AlarmRules />} />
              {/* 알람 하위 경로 기본값 */}
              <Route index element={<Navigate to="active" replace />} />
            </Route>
            
            {/* 시스템 관리 */}
            <Route path="system">
              <Route path="status" element={<SystemStatus />} />
              <Route path="users" element={<UserManagement />} />
              <Route path="permissions" element={<PermissionManagement />} />
              <Route path="backup" element={<BackupRestore />} />
              {/* 시스템 하위 경로 기본값 */}
              <Route index element={<Navigate to="status" replace />} />
            </Route>
            
            {/* 404 페이지 */}
            <Route path="*" element={<NotFound />} />
          </Route>
        </Routes>
      </div>
    </Router>
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
      gap: '20px'
    }}>
      <div className="not-found-content">
        <h1 style={{ fontSize: '4rem', margin: '0', color: '#ef4444' }}>404</h1>
        <h2 style={{ fontSize: '1.5rem', margin: '0', color: '#374151' }}>페이지를 찾을 수 없습니다</h2>
        <p style={{ color: '#6b7280', margin: '10px 0' }}>요청하신 페이지가 존재하지 않거나 이동되었습니다.</p>
        <div style={{ display: 'flex', gap: '10px', justifyContent: 'center', marginTop: '20px' }}>
          <button 
            onClick={() => window.history.back()}
            className="btn btn-secondary"
            style={{
              padding: '10px 20px',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              background: '#f9fafb',
              cursor: 'pointer'
            }}
          >
            이전 페이지로
          </button>
          <button 
            onClick={() => window.location.href = '/dashboard'}
            className="btn btn-primary"
            style={{
              padding: '10px 20px',
              border: 'none',
              borderRadius: '6px',
              background: '#3b82f6',
              color: 'white',
              cursor: 'pointer'
            }}
          >
            대시보드로
          </button>
        </div>
      </div>
    </div>
  );
};

export default App;