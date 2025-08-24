// ============================================================================
// ScopeSelector.tsx - 범위 선택 컴포넌트
// frontend/src/components/VirtualPoints/VirtualPointModal/ScopeSelector.tsx
// ============================================================================

import React, { useState, useEffect } from 'react';

interface Device {
  id: number;
  name: string;
  description?: string;
  protocol_type: string;
  site_id: number;
  site_name: string;
}

interface Site {
  id: number;
  name: string;
  description?: string;
  device_count: number;
}

interface ScopeSelectorProps {
  scopeType: 'device' | 'site' | 'global';
  scopeId?: number;
  onScopeChange: (type: 'device' | 'site' | 'global', id?: number) => void;
}

export const ScopeSelector: React.FC<ScopeSelectorProps> = ({
  scopeType,
  scopeId,
  onScopeChange
}) => {
  const [devices, setDevices] = useState<Device[]>([]);
  const [sites, setSites] = useState<Site[]>([]);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    if (scopeType === 'device' || scopeType === 'site') {
      loadScopeData();
    }
  }, [scopeType]);

  const loadScopeData = async () => {
    setLoading(true);
    try {
      if (scopeType === 'device') {
        // 임시 데이터 - 실제로는 API 호출
        const mockDevices: Device[] = [
          { id: 1, name: 'PLC #1', protocol_type: 'Modbus', site_id: 1, site_name: 'Factory A' },
          { id: 2, name: 'PLC #2', protocol_type: 'TCP', site_id: 1, site_name: 'Factory A' },
          { id: 3, name: 'MQTT Sensor', protocol_type: 'MQTT', site_id: 2, site_name: 'Factory B' }
        ];
        setDevices(mockDevices);
      } else if (scopeType === 'site') {
        const mockSites: Site[] = [
          { id: 1, name: 'Factory A', device_count: 5 },
          { id: 2, name: 'Factory B', device_count: 3 }
        ];
        setSites(mockSites);
      }
    } catch (error) {
      console.error('범위 데이터 로드 실패:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleScopeTypeChange = (type: 'device' | 'site' | 'global') => {
    onScopeChange(type, undefined);
  };

  const handleScopeIdChange = (id: number) => {
    onScopeChange(scopeType, id);
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      
      {/* 적용 범위 선택 */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <label style={{ 
          fontSize: '14px', 
          fontWeight: '500', 
          color: '#374151' 
        }}>
          적용 범위 *
        </label>
        
        <select 
          value={scopeType}
          onChange={(e) => handleScopeTypeChange(e.target.value as any)}
          style={{
            padding: '12px 16px',
            border: '1px solid #d1d5db',
            borderRadius: '8px',
            fontSize: '14px',
            background: 'white',
            cursor: 'pointer'
          }}
        >
          <option value="global">전역 (모든 디바이스)</option>
          <option value="site">사이트별</option>
          <option value="device">특정 디바이스</option>
        </select>
      </div>

      {/* 범위별 상세 선택 */}
      {scopeType !== 'global' && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <label style={{ 
            fontSize: '14px', 
            fontWeight: '500', 
            color: '#374151' 
          }}>
            {scopeType === 'device' ? '디바이스 선택' : '사이트 선택'} *
          </label>

          {loading ? (
            <div style={{
              padding: '12px 16px',
              border: '1px solid #d1d5db',
              borderRadius: '8px',
              color: '#6b7280',
              background: '#f9fafb'
            }}>
              <i className="fas fa-spinner fa-spin" style={{ marginRight: '8px' }}></i>
              로딩 중...
            </div>
          ) : (
            <select
              value={scopeId || ''}
              onChange={(e) => handleScopeIdChange(Number(e.target.value))}
              style={{
                padding: '12px 16px',
                border: '1px solid #d1d5db',
                borderRadius: '8px',
                fontSize: '14px',
                background: 'white',
                cursor: 'pointer'
              }}
            >
              <option value="">
                {scopeType === 'device' ? '디바이스를 선택하세요' : '사이트를 선택하세요'}
              </option>
              
              {scopeType === 'device' ? (
                devices.map(device => (
                  <option key={device.id} value={device.id}>
                    {device.site_name} - {device.name} ({device.protocol_type})
                  </option>
                ))
              ) : (
                sites.map(site => (
                  <option key={site.id} value={site.id}>
                    {site.name} ({site.device_count}개 디바이스)
                  </option>
                ))
              )}
            </select>
          )}

          {/* 선택된 항목 상세 정보 표시 */}
          {scopeId && (
            <div style={{
              padding: '12px',
              background: '#f0f9ff',
              border: '1px solid #bae6fd',
              borderRadius: '6px',
              fontSize: '13px',
              color: '#0369a1'
            }}>
              <i className="fas fa-info-circle" style={{ marginRight: '6px' }}></i>
              {scopeType === 'device' ? (
                (() => {
                  const selectedDevice = devices.find(d => d.id === scopeId);
                  return selectedDevice ? 
                    `선택됨: ${selectedDevice.site_name} - ${selectedDevice.name} (${selectedDevice.protocol_type})` :
                    '디바이스 정보를 불러오는 중...';
                })()
              ) : (
                (() => {
                  const selectedSite = sites.find(s => s.id === scopeId);
                  return selectedSite ?
                    `선택됨: ${selectedSite.name} (${selectedSite.device_count}개 디바이스에 적용됨)` :
                    '사이트 정보를 불러오는 중...';
                })()
              )}
            </div>
          )}
        </div>
      )}

      {/* 전역 선택 시 안내 */}
      {scopeType === 'global' && (
        <div style={{
          padding: '12px',
          background: '#fef3c7',
          border: '1px solid #fcd34d',
          borderRadius: '6px',
          fontSize: '13px',
          color: '#92400e'
        }}>
          <i className="fas fa-globe" style={{ marginRight: '6px' }}></i>
          이 가상포인트는 모든 사이트와 디바이스에서 사용할 수 있습니다.
        </div>
      )}
    </div>
  );
};