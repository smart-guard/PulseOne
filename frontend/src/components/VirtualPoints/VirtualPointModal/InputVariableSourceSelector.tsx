// ============================================================================
// InputVariableSourceSelector.tsx - 데이터포인트/가상포인트 선택 컴포넌트
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';

interface DataPoint {
  id: number;
  device_id: number;
  device_name: string;
  name: string;
  description: string;
  data_type: 'number' | 'boolean' | 'string';
  current_value: any;
  unit?: string;
  address: string;
}

interface VirtualPoint {
  id: number;
  name: string;
  description: string;
  data_type: 'number' | 'boolean' | 'string';
  current_value: any;
  unit?: string;
  category: string;
}

interface SourceSelectorProps {
  sourceType: 'data_point' | 'virtual_point' | 'constant';
  selectedId?: number;
  onSelect: (id: number, source: DataPoint | VirtualPoint) => void;
  dataType?: 'number' | 'boolean' | 'string';
}

const InputVariableSourceSelector: React.FC<SourceSelectorProps> = ({
  sourceType,
  selectedId,
  onSelect,
  dataType
}) => {
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [virtualPoints, setVirtualPoints] = useState<VirtualPoint[]>([]);
  const [loading, setLoading] = useState(false);
  const [searchTerm, setSearchTerm] = useState('');
  const [deviceFilter, setDeviceFilter] = useState<number | null>(null);
  const [devices, setDevices] = useState<Array<{id: number, name: string}>>([]);

  // ========================================================================
  // 데이터 로딩
  // ========================================================================

  const loadDataPoints = useCallback(async () => {
    setLoading(true);
    try {
      console.log('데이터포인트 목록 로딩');
      
      const response = await fetch('/api/data/points?' + new URLSearchParams({
        limit: '1000',
        enabled_only: 'true',
        include_current_value: 'true',
        ...(dataType && { data_type: dataType }),
        ...(searchTerm && { search: searchTerm }),
        ...(deviceFilter && { device_id: deviceFilter.toString() })
      }));

      if (response.ok) {
        const result = await response.json();
        if (result.success && result.data?.points) {
          setDataPoints(result.data.points);
          console.log(`${result.data.points.length}개 데이터포인트 로드됨`);
        } else {
          throw new Error(result.message || 'API 응답 오류');
        }
      } else {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
    } catch (error) {
      console.error('데이터포인트 로딩 실패:', error);
      
      // 백엔드 API가 없을 때 목 데이터
      setDataPoints([
        {
          id: 101,
          device_id: 1,
          device_name: 'PLC-001',
          name: 'Temperature_Sensor_01',
          description: '보일러 온도 센서',
          data_type: 'number',
          current_value: 85.3,
          unit: '°C',
          address: '40001'
        },
        {
          id: 102,
          device_id: 1,
          device_name: 'PLC-001',
          name: 'Pressure_Sensor_01',
          description: '보일러 압력 센서',
          data_type: 'number',
          current_value: 2.4,
          unit: 'bar',
          address: '40002'
        },
        {
          id: 103,
          device_id: 2,
          device_name: 'WEATHER-001',
          name: 'External_Temperature',
          description: '외부 온도 센서',
          data_type: 'number',
          current_value: 24.5,
          unit: '°C',
          address: 'temp'
        },
        {
          id: 104,
          device_id: 2,
          device_name: 'WEATHER-001',
          name: 'Humidity_Sensor',
          description: '습도 센서',
          data_type: 'number',
          current_value: 65.2,
          unit: '%',
          address: 'humidity'
        },
        {
          id: 105,
          device_id: 3,
          device_name: 'HVAC-001',
          name: 'Fan_Status',
          description: '환풍기 동작 상태',
          data_type: 'boolean',
          current_value: true,
          address: 'coil_001'
        }
      ]);
    } finally {
      setLoading(false);
    }
  }, [dataType, searchTerm, deviceFilter]);

  const loadVirtualPoints = useCallback(async () => {
    setLoading(true);
    try {
      console.log('가상포인트 목록 로딩');
      
      const response = await fetch('/api/virtual-points?' + new URLSearchParams({
        limit: '1000',
        is_enabled: 'true',
        ...(dataType && { data_type: dataType }),
        ...(searchTerm && { search: searchTerm })
      }));

      if (response.ok) {
        const result = await response.json();
        if (result.success && result.data?.items) {
          setVirtualPoints(result.data.items);
          console.log(`${result.data.items.length}개 가상포인트 로드됨`);
        } else {
          throw new Error(result.message || 'API 응답 오류');
        }
      } else {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
    } catch (error) {
      console.error('가상포인트 로딩 실패:', error);
      
      // 백엔드 API가 없을 때 목 데이터
      setVirtualPoints([
        {
          id: 201,
          name: 'Avg_Temperature',
          description: '평균 온도',
          data_type: 'number',
          current_value: 54.9,
          unit: '°C',
          category: 'Temperature'
        },
        {
          id: 202,
          name: 'Energy_Efficiency',
          description: '에너지 효율',
          data_type: 'number',
          current_value: 87.5,
          unit: '%',
          category: 'Performance'
        },
        {
          id: 203,
          name: 'System_Status',
          description: '시스템 전체 상태',
          data_type: 'string',
          current_value: '정상',
          category: 'Status'
        }
      ]);
    } finally {
      setLoading(false);
    }
  }, [dataType, searchTerm]);

  const loadDevices = useCallback(async () => {
    try {
      console.log('디바이스 목록 로딩');
      
      const response = await fetch('/api/devices?limit=100&enabled_only=true');
      
      if (response.ok) {
        const result = await response.json();
        if (result.success && result.data?.devices) {
          setDevices(result.data.devices.map((d: any) => ({ id: d.id, name: d.name })));
        }
      } else {
        // 백엔드 API가 없을 때 목 데이터
        setDevices([
          { id: 1, name: 'PLC-001 (보일러)' },
          { id: 2, name: 'WEATHER-001 (기상)' },
          { id: 3, name: 'HVAC-001 (공조)' }
        ]);
      }
    } catch (error) {
      console.error('디바이스 로딩 실패:', error);
      setDevices([
        { id: 1, name: 'PLC-001 (보일러)' },
        { id: 2, name: 'WEATHER-001 (기상)' },
        { id: 3, name: 'HVAC-001 (공조)' }
      ]);
    }
  }, []);

  // ========================================================================
  // 효과 및 이벤트 핸들러
  // ========================================================================

  useEffect(() => {
    if (sourceType === 'data_point') {
      loadDataPoints();
      loadDevices();
    } else if (sourceType === 'virtual_point') {
      loadVirtualPoints();
    }
  }, [sourceType, loadDataPoints, loadVirtualPoints, loadDevices]);

  useEffect(() => {
    // 검색어나 필터가 변경되면 다시 로딩
    const debounceTimer = setTimeout(() => {
      if (sourceType === 'data_point') {
        loadDataPoints();
      } else if (sourceType === 'virtual_point') {
        loadVirtualPoints();
      }
    }, 500);

    return () => clearTimeout(debounceTimer);
  }, [searchTerm, deviceFilter, sourceType, loadDataPoints, loadVirtualPoints]);

  const handleSelect = useCallback((source: DataPoint | VirtualPoint) => {
    onSelect(source.id, source);
  }, [onSelect]);

  // ========================================================================
  // 필터링된 데이터
  // ========================================================================

  const filteredDataPoints = dataPoints.filter(point => {
    if (dataType && point.data_type !== dataType) return false;
    if (searchTerm) {
      const search = searchTerm.toLowerCase();
      return point.name.toLowerCase().includes(search) ||
             point.description.toLowerCase().includes(search) ||
             point.device_name.toLowerCase().includes(search);
    }
    return true;
  });

  const filteredVirtualPoints = virtualPoints.filter(point => {
    if (dataType && point.data_type !== dataType) return false;
    if (searchTerm) {
      const search = searchTerm.toLowerCase();
      return point.name.toLowerCase().includes(search) ||
             point.description.toLowerCase().includes(search) ||
             point.category.toLowerCase().includes(search);
    }
    return true;
  });

  // ========================================================================
  // 렌더링
  // ========================================================================

  if (sourceType === 'constant') {
    return (
      <div style={{ padding: '16px', textAlign: 'center', color: '#6c757d' }}>
        상수값은 직접 입력해주세요
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', maxHeight: '400px' }}>
      
      {/* 검색 및 필터 */}
      <div style={{ display: 'flex', gap: '12px', flexWrap: 'wrap' }}>
        <input
          type="text"
          placeholder={sourceType === 'data_point' ? '데이터포인트 검색...' : '가상포인트 검색...'}
          value={searchTerm}
          onChange={(e) => setSearchTerm(e.target.value)}
          style={{
            flex: 1,
            minWidth: '200px',
            padding: '8px 12px',
            border: '1px solid #dee2e6',
            borderRadius: '6px',
            fontSize: '14px'
          }}
        />
        
        {sourceType === 'data_point' && (
          <select
            value={deviceFilter || ''}
            onChange={(e) => setDeviceFilter(e.target.value ? Number(e.target.value) : null)}
            style={{
              minWidth: '150px',
              padding: '8px 12px',
              border: '1px solid #dee2e6',
              borderRadius: '6px',
              fontSize: '14px'
            }}
          >
            <option value="">모든 디바이스</option>
            {devices.map(device => (
              <option key={device.id} value={device.id}>{device.name}</option>
            ))}
          </select>
        )}
      </div>

      {/* 로딩 상태 */}
      {loading && (
        <div style={{ 
          textAlign: 'center', 
          padding: '20px',
          color: '#6c757d'
        }}>
          <div style={{
            display: 'inline-block',
            width: '20px',
            height: '20px',
            border: '2px solid #f3f4f6',
            borderTop: '2px solid #007bff',
            borderRadius: '50%',
            animation: 'spin 1s linear infinite'
          }} />
          <div style={{ marginTop: '8px' }}>로딩 중...</div>
        </div>
      )}

      {/* 데이터포인트 목록 */}
      {sourceType === 'data_point' && !loading && (
        <div style={{ 
          maxHeight: '300px', 
          overflowY: 'auto',
          border: '1px solid #e9ecef',
          borderRadius: '6px'
        }}>
          {filteredDataPoints.length === 0 ? (
            <div style={{ padding: '20px', textAlign: 'center', color: '#6c757d' }}>
              선택할 수 있는 데이터포인트가 없습니다
            </div>
          ) : (
            filteredDataPoints.map(point => (
              <div
                key={point.id}
                onClick={() => handleSelect(point)}
                style={{
                  padding: '12px',
                  borderBottom: '1px solid #f1f3f4',
                  cursor: 'pointer',
                  background: selectedId === point.id ? '#e3f2fd' : 'white',
                  transition: 'background-color 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = '#f8f9fa';
                  }
                }}
                onMouseLeave={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = 'white';
                  }
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                  <div style={{ flex: 1 }}>
                    <div style={{ fontWeight: 'bold', color: '#495057', marginBottom: '4px' }}>
                      {point.name}
                    </div>
                    <div style={{ fontSize: '13px', color: '#6c757d', marginBottom: '4px' }}>
                      {point.description}
                    </div>
                    <div style={{ fontSize: '12px', color: '#28a745' }}>
                      📍 {point.device_name} • 주소: {point.address} • 타입: {point.data_type}
                    </div>
                  </div>
                  <div style={{ 
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'flex-end',
                    gap: '4px'
                  }}>
                    <div style={{ 
                      fontSize: '16px',
                      fontWeight: 'bold',
                      color: '#007bff'
                    }}>
                      {point.current_value} {point.unit}
                    </div>
                    <div style={{
                      fontSize: '10px',
                      color: '#6c757d',
                      background: '#f8f9fa',
                      padding: '2px 6px',
                      borderRadius: '10px'
                    }}>
                      ID: {point.id}
                    </div>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* 가상포인트 목록 */}
      {sourceType === 'virtual_point' && !loading && (
        <div style={{ 
          maxHeight: '300px', 
          overflowY: 'auto',
          border: '1px solid #e9ecef',
          borderRadius: '6px'
        }}>
          {filteredVirtualPoints.length === 0 ? (
            <div style={{ padding: '20px', textAlign: 'center', color: '#6c757d' }}>
              선택할 수 있는 가상포인트가 없습니다
            </div>
          ) : (
            filteredVirtualPoints.map(point => (
              <div
                key={point.id}
                onClick={() => handleSelect(point)}
                style={{
                  padding: '12px',
                  borderBottom: '1px solid #f1f3f4',
                  cursor: 'pointer',
                  background: selectedId === point.id ? '#e8f5e8' : 'white',
                  transition: 'background-color 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = '#f8f9fa';
                  }
                }}
                onMouseLeave={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = 'white';
                  }
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                  <div style={{ flex: 1 }}>
                    <div style={{ fontWeight: 'bold', color: '#495057', marginBottom: '4px' }}>
                      {point.name}
                    </div>
                    <div style={{ fontSize: '13px', color: '#6c757d', marginBottom: '4px' }}>
                      {point.description}
                    </div>
                    <div style={{ fontSize: '12px', color: '#6f42c1' }}>
                      🔮 가상포인트 • 카테고리: {point.category} • 타입: {point.data_type}
                    </div>
                  </div>
                  <div style={{ 
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'flex-end',
                    gap: '4px'
                  }}>
                    <div style={{ 
                      fontSize: '16px',
                      fontWeight: 'bold',
                      color: '#28a745'
                    }}>
                      {point.current_value} {point.unit}
                    </div>
                    <div style={{
                      fontSize: '10px',
                      color: '#6c757d',
                      background: '#f8f9fa',
                      padding: '2px 6px',
                      borderRadius: '10px'
                    }}>
                      ID: {point.id}
                    </div>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* 회전 애니메이션 CSS */}
      <style>{`
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};

export default InputVariableSourceSelector;