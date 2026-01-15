// ============================================================================
// frontend/src/pages/AlarmSettings.tsx
// 알람 설정 관리 페이지 - 공통 UI 시스템 적용 버전
// ============================================================================

import React, { useState, useEffect } from 'react';
import { AlarmApiService, AlarmRule } from '../api/services/alarmApi';
import { DataApiService } from '../api/services/dataApi';
import { DeviceApiService } from '../api/services/deviceApi';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { FilterBar } from '../components/common/FilterBar';
import AlarmCreateEditModal from '../components/modals/AlarmCreateEditModal';
import '../styles/management.css';
import '../styles/alarm-settings.css';

interface AlarmSettingsProps { }

interface DataPoint {
  id: number;
  name: string;
  device_id: number;
  device_name: string;
  data_type: string;
  unit?: string;
  address?: string;
}

interface Device {
  id: number;
  name: string;
  device_type: string;
  site_name?: string;
}

const AlarmSettings: React.FC<AlarmSettingsProps> = () => {
  const { confirm } = useConfirmContext();
  const [alarmRules, setAlarmRules] = useState<AlarmRule[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [toggleLoading, setToggleLoading] = useState<Set<number>>(new Set());
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [devices, setDevices] = useState<Device[]>([]);
  const [loadingTargetData, setLoadingTargetData] = useState(false);
  const [showModal, setShowModal] = useState(false);
  const [modalMode, setModalMode] = useState<'create' | 'edit'>('create');
  const [selectedRule, setSelectedRule] = useState<AlarmRule | null>(null);
  const [viewType, setViewType] = useState<'card' | 'table'>('card');
  const [filters, setFilters] = useState({ search: '', severity: 'all', status: 'all', category: 'all', tag: '' });

  useEffect(() => { loadAlarmRules(); loadTargetData(); }, []);

  const loadAlarmRules = async () => {
    try {
      setLoading(true);
      const response = await AlarmApiService.getAlarmRules({ page: 1, limit: 100, ...filters });
      if (response.success && response.data) {
        setAlarmRules((response.data as any).items || []);
      }
    } catch (error) { setError('데이터 로딩 실패'); }
    finally { setLoading(false); }
  };

  const loadTargetData = async () => {
    try {
      setLoadingTargetData(true);
      const [pts, devs] = await Promise.all([
        DataApiService.searchDataPoints({ page: 1, limit: 1000, include_current_value: false }),
        DeviceApiService.getDevices({ page: 1, limit: 100 })
      ]);
      if (pts.success) setDataPoints((pts.data as any).items || []);
      if (devs.success) setDevices((devs.data as any).items || []);
    } finally { setLoadingTargetData(false); }
  };

  const handleToggleRule = async (ruleId: number, currentStatus: boolean, ruleName: string) => {
    const newStatus = !currentStatus;
    setToggleLoading(prev => new Set([...prev, ruleId]));
    try {
      const response = await AlarmApiService.toggleAlarmRule(ruleId, newStatus);
      if (response.success) setAlarmRules(prev => prev.map(r => r.id === ruleId ? { ...r, is_enabled: newStatus } : r));
    } finally {
      setToggleLoading(prev => { const n = new Set(prev); n.delete(ruleId); return n; });
    }
  };

  const handleDeleteRule = async (ruleId: number, ruleName: string) => {
    const confirmed = await confirm({
      title: '알람 규칙 삭제',
      message: `${ruleName} 알람 규칙을 정말로 삭제하시겠습니까?`,
      confirmText: '삭제',
      confirmButtonType: 'danger'
    });
    if (confirmed) {
      const response = await AlarmApiService.deleteAlarmRule(ruleId);
      if (response.success) loadAlarmRules();
    }
  };

  const handleCreateRule = () => { setModalMode('create'); setSelectedRule(null); setShowModal(true); };
  const handleEditRule = (rule: AlarmRule) => { setModalMode('edit'); setSelectedRule(rule); setShowModal(true); };

  const filteredRules = alarmRules.filter(rule => {
    if (filters.search && !rule.name.toLowerCase().includes(filters.search.toLowerCase())) return false;
    if (filters.severity !== 'all' && rule.severity !== filters.severity) return false;
    if (filters.status === 'enabled' && !rule.is_enabled) return false;
    if (filters.status === 'disabled' && rule.is_enabled) return false;
    if (filters.category !== 'all' && rule.category !== filters.category) return false;
    return true;
  });

  const getTargetDisplay = (rule: AlarmRule) => rule.target_display || rule.device_name || `Target #${rule.target_id}`;
  const getSeverityLabel = (s: string) => s.toUpperCase();

  return (
    <ManagementLayout>
      <PageHeader
        title="알람 설정 관리"
        description="시스템 전반의 알람 발생 규칙을 설정하고 관리합니다."
        icon="fas fa-bell"
        actions={
          <button className="btn-primary" onClick={handleCreateRule}>
            <i className="fas fa-plus"></i> 새 알람 규칙 추가
          </button>
        }
      />

      <div className="mgmt-stats-panel">
        <StatCard title="전체 규칙" value={alarmRules.length} icon="fas fa-list-ul" type="primary" />
        <StatCard title="활성" value={alarmRules.filter(r => r.is_enabled).length} icon="fas fa-check-circle" type="success" />
        <StatCard title="비활성" value={alarmRules.filter(r => !r.is_enabled).length} icon="fas fa-pause-circle" type="warning" />
        <StatCard title="검색 결과" value={filteredRules.length} icon="fas fa-search" type="neutral" />
      </div>

      <FilterBar
        searchTerm={filters.search}
        onSearchChange={(val) => setFilters(prev => ({ ...prev, search: val }))}
        onReset={() => setFilters({ search: '', severity: 'all', status: 'all', category: 'all', tag: '' })}
        activeFilterCount={(filters.search ? 1 : 0) + (filters.severity !== 'all' ? 1 : 0) + (filters.status !== 'all' ? 1 : 0) + (filters.category !== 'all' ? 1 : 0)}
        filters={[
          {
            label: '심각도',
            value: filters.severity,
            onChange: (val) => setFilters(prev => ({ ...prev, severity: val })),
            options: [
              { value: 'all', label: '전체' },
              { value: 'critical', label: 'Critical' },
              { value: 'high', label: 'High' },
              { value: 'medium', label: 'Medium' },
              { value: 'low', label: 'Low' }
            ]
          },
          {
            label: '상태',
            value: filters.status,
            onChange: (val) => setFilters(prev => ({ ...prev, status: val })),
            options: [
              { value: 'all', label: '전체' },
              { value: 'enabled', label: '활성' },
              { value: 'disabled', label: '비활성' }
            ]
          }
        ]}
      />

      <div style={{ display: 'flex', justifyContent: 'flex-end', marginBottom: '16px' }}>
        <div className="view-toggle">
          <button onClick={() => setViewType('card')} className={viewType === 'card' ? 'active' : ''}><i className="fas fa-th-large"></i> 카드</button>
          <button onClick={() => setViewType('table')} className={viewType === 'table' ? 'active' : ''}><i className="fas fa-list"></i> 테이블</button>
        </div>
      </div>

      <div className="mgmt-card table-container">
        <table className="mgmt-table">
          <thead>
            <tr>
              <th>규칙명</th>
              <th>타겟</th>
              <th>심각도</th>
              <th>상태</th>
              <th>액션</th>
            </tr>
          </thead>
          <tbody>
            {filteredRules.map(rule => (
              <tr key={rule.id}>
                <td><strong>{rule.name}</strong></td>
                <td>{getTargetDisplay(rule)}</td>
                <td><span className={`status-pill ${rule.severity === 'critical' || rule.severity === 'high' ? 'error' : 'warning'}`}>{getSeverityLabel(rule.severity)}</span></td>
                <td>{rule.is_enabled ? '활성' : '비활성'}</td>
                <td>
                  <div className="table-actions">
                    <button onClick={() => handleToggleRule(rule.id, rule.is_enabled, rule.name)} className="btn-icon" disabled={toggleLoading.has(rule.id)}>
                      {toggleLoading.has(rule.id) ? <i className="fas fa-spinner fa-spin"></i> : <i className={`fas ${rule.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>}
                    </button>
                    <button onClick={() => handleEditRule(rule)} className="btn-icon"><i className="fas fa-edit"></i></button>
                    <button onClick={() => handleDeleteRule(rule.id, rule.name)} className="btn-icon"><i className="fas fa-trash"></i></button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <AlarmCreateEditModal
        isOpen={showModal}
        mode={modalMode}
        rule={selectedRule}
        onClose={() => setShowModal(false)}
        onSave={() => { loadAlarmRules(); setShowModal(false); }}
        dataPoints={dataPoints as any}
        devices={devices as any}
        loadingTargetData={loadingTargetData}
      />
    </ManagementLayout>
  );
};

export default AlarmSettings;