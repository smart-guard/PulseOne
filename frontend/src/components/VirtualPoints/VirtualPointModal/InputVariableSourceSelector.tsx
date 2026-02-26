// ============================================================================
// InputVariableSourceSelector.tsx - 디바이스 필터링 및 성능 최적화
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { DataApiService } from '../../../api/services/dataApi';

interface DataPoint {
  id: number;
  device_id: number;
  device_name?: string; // optional to match dataApi.DataPoint
  name: string;
  description?: string;
  data_type: 'number' | 'boolean' | 'string' | string;
  current_value?: any;
  unit?: string;
  address: string;
  is_enabled?: boolean;
  created_at?: string;
  updated_at?: string;
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
  const [devices, setDevices] = useState<Array<{ id: number, name: string }>>([]);

  // ========================================================================
  // 데이터 로딩 함수들 - useCallback 최적화
  // ========================================================================

  const loadDataPoints = useCallback(async () => {
    if (sourceType !== 'data_point') return; // 불필요한 호출 방지

    setLoading(true);
    try {
      console.log('🔄 데이터포인트 로딩:', { deviceFilter, searchTerm, dataType });

      const result = await DataApiService.getDataPoints({
        limit: 1000,
        enabled_only: true,
        include_current_value: true,
        data_type: dataType,
        search: searchTerm,
        device_id: deviceFilter || undefined
      });

      console.log(`✅ 데이터포인트 ${result.points.length}개 로드됨`);
      setDataPoints(result.points);

      // 디바이스 필터가 설정되어 있는데 결과가 없는 경우 로그
      if (deviceFilter && result.points.length === 0) {
        console.warn(`⚠️ 디바이스 ID ${deviceFilter}에 대한 데이터포인트가 없습니다.`);
      }

    } catch (error) {
      console.error('❌ 데이터포인트 로딩 실패:', error);
      setDataPoints([]);
    } finally {
      setLoading(false);
    }
  }, [sourceType, dataType, searchTerm, deviceFilter]); // 함수 제거, 값만 의존

  const loadVirtualPoints = useCallback(async () => {
    if (sourceType !== 'virtual_point') return; // 불필요한 호출 방지

    setLoading(true);
    try {
      console.log('🔄 가상포인트 로딩');

      const response = await fetch('/api/virtual-points?' + new URLSearchParams({
        limit: '1000',
        is_enabled: 'true',
        ...(dataType && { data_type: dataType }),
        ...(searchTerm && { search: searchTerm })
      }));

      if (response.ok) {
        const result = await response.json();
        console.log('📦 가상포인트 API 응답:', result);

        if (result.success && Array.isArray(result.data)) {
          setVirtualPoints(result.data);
          console.log(`✅ 가상포인트 ${result.data.length}개 로드됨`);
        } else {
          console.warn('예상하지 못한 가상포인트 응답 구조:', result);
          throw new Error(result.message || 'API 응답 구조 오류');
        }
      } else {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
    } catch (error) {
      console.error('❌ 가상포인트 로딩 실패:', error);

      // 백엔드 실패 시 목 데이터
      console.log('🎭 목 가상포인트 데이터 사용');
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
        }
      ]);
    } finally {
      setLoading(false);
    }
  }, [sourceType, dataType, searchTerm]); // 함수 제거, 값만 의존

  const loadDevices = useCallback(async () => {
    if (sourceType !== 'data_point') return; // 데이터포인트일 때만 로드

    try {
      console.log('🔄 디바이스 목록 로딩');

      const devices = await DataApiService.getDevices();

      setDevices(devices.map(d => ({ id: d.id, name: d.name })));
      console.log(`✅ 디바이스 ${devices.length}개 로드됨`);
    } catch (error) {
      console.error('❌ 디바이스 로딩 실패:', error);

      // 목 데이터
      setDevices([
        { id: 1, name: 'PLC-001 (보일러)' },
        { id: 2, name: 'WEATHER-001 (기상)' },
        { id: 3, name: 'HVAC-001 (공조)' }
      ]);
    }
  }, [sourceType]);

  // ========================================================================
  // Effect Hooks - 최적화된 의존성 배열
  // ========================================================================

  // 초기 로딩
  useEffect(() => {
    console.log(`🎯 sourceType 변경: ${sourceType}`);

    if (sourceType === 'data_point') {
      loadDevices(); // 디바이스 목록 먼저 로드
      loadDataPoints();
    } else if (sourceType === 'virtual_point') {
      loadVirtualPoints();
    }

    // 소스 타입이 변경되면 필터 초기화
    setSearchTerm('');
    setDeviceFilter(null);
  }, [sourceType]); // sourceType만 의존

  // 검색 및 필터 변경 시 디바운스
  useEffect(() => {
    const debounceTimer = setTimeout(() => {
      console.log(`🔍 필터 변경 - 검색어: "${searchTerm}", 디바이스: ${deviceFilter}`);

      if (sourceType === 'data_point') {
        loadDataPoints();
      } else if (sourceType === 'virtual_point') {
        loadVirtualPoints();
      }
    }, 300); // 300ms로 줄여서 더 반응성 있게

    return () => clearTimeout(debounceTimer);
  }, [searchTerm, deviceFilter, loadDataPoints, loadVirtualPoints]);

  // ========================================================================
  // 이벤트 핸들러
  // ========================================================================

  const handleSelect = useCallback((source: DataPoint | VirtualPoint) => {
    console.log('🎯 소스 선택:', source);
    onSelect(source.id, source);
  }, [onSelect]);

  const handleDeviceFilterChange = useCallback((deviceId: string) => {
    const newDeviceId = deviceId ? Number(deviceId) : null;
    console.log('🔄 디바이스 필터 변경:', newDeviceId);
    setDeviceFilter(newDeviceId);
  }, []);

  const handleSearchChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    setSearchTerm(e.target.value);
  }, []);

  // ========================================================================
  // 필터링된 데이터 계산 - 클라이언트 사이드 추가 필터링
  // ========================================================================

  const filteredDataPoints = React.useMemo(() => {
    let points = dataPoints;

    // 클라이언트 사이드에서 추가 필터링 (API 필터가 완벽하지 않은 경우 대비)
    if (deviceFilter) {
      points = points.filter(point => point.device_id === deviceFilter);
    }

    if (searchTerm && searchTerm.length > 0) {
      const search = searchTerm.toLowerCase();
      points = points.filter(point =>
        point.name.toLowerCase().includes(search) ||
        point.description.toLowerCase().includes(search) ||
        (point.device_name || '').toLowerCase().includes(search)
      );
    }

    if (dataType) {
      points = points.filter(point => point.data_type === dataType);
    }

    return points;
  }, [dataPoints, deviceFilter, searchTerm, dataType]);

  const filteredVirtualPoints = React.useMemo(() => {
    let points = virtualPoints;

    if (searchTerm && searchTerm.length > 0) {
      const search = searchTerm.toLowerCase();
      points = points.filter(point =>
        point.name.toLowerCase().includes(search) ||
        point.description.toLowerCase().includes(search) ||
        point.category.toLowerCase().includes(search)
      );
    }

    if (dataType) {
      points = points.filter(point => point.data_type === dataType);
    }

    return points;
  }, [virtualPoints, searchTerm, dataType]);

  // ========================================================================
  // 렌더링
  // ========================================================================

  if (sourceType === 'constant') {
    return (
      <div style={{
        padding: '16px',
        textAlign: 'center',
        color: '#6c757d',
        background: '#f8f9fa',
        borderRadius: '8px',
        border: '1px solid #e9ecef'
      }}>
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
          onChange={handleSearchChange}
          style={{
            flex: 1,
            minWidth: '200px',
            padding: '8px 12px',
            border: '1px solid #dee2e6',
            borderRadius: '6px',
            fontSize: '14px'
          }}
        />

        {sourceType === 'data_point' && devices.length > 0 && (
          <select
            value={deviceFilter || ''}
            onChange={(e) => handleDeviceFilterChange(e.target.value)}
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
          <div style={{ marginTop: '8px' }}>
            {sourceType === 'data_point' ? '데이터포인트' : '가상포인트'} 로딩 중...
          </div>
        </div>
      )}

      {/* 데이터포인트 목록 */}
      {sourceType === 'data_point' && (
        <div style={{
          maxHeight: '300px',
          overflowY: 'auto',
          border: '1px solid #e9ecef',
          borderRadius: '6px',
          opacity: loading ? 0.5 : 1, // 깜박임 방지: 목록 제거 대신 투명도만 낮춤
          transition: 'opacity 0.2s'
        }}>
          {filteredDataPoints.length === 0 ? (
            <div style={{
              padding: '20px',
              textAlign: 'center',
              color: '#6c757d'
            }}>
              {deviceFilter ? (
                <>
                  선택한 디바이스에 데이터포인트가 없습니다
                  <br />
                  <small style={{ color: '#868e96' }}>
                    다른 디바이스를 선택하거나 '모든 디바이스'로 변경해보세요
                  </small>
                </>
              ) : dataPoints.length === 0 ? (
                '데이터포인트가 없습니다'
              ) : (
                '검색 조건에 맞는 데이터포인트가 없습니다'
              )}
            </div>
          ) : (
            filteredDataPoints.slice(0, 100).map(point => ( // 성능 최적화: 최대 100개까지만 렌더링
              <div
                key={point.id}
                onClick={() => handleSelect(point)}
                style={{
                  padding: '12px',
                  borderBottom: '1px solid #f1f3f4',
                  cursor: 'pointer',
                  background: selectedId === point.id ? '#e7f3ff' : 'transparent',
                  transition: 'background-color 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = '#f8f9fa';
                  }
                }}
                onMouseLeave={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = 'transparent';
                  }
                }}
              >
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'flex-start',
                  marginBottom: '4px'
                }}>
                  <div style={{ fontWeight: '500', color: '#495057' }}>
                    {point.name}
                  </div>
                  <div style={{
                    fontSize: '12px',
                    color: '#6c757d',
                    background: '#f8f9fa',
                    padding: '2px 6px',
                    borderRadius: '3px'
                  }}>
                    {point.data_type}
                  </div>
                </div>

                <div style={{ fontSize: '13px', color: '#6c757d', marginBottom: '2px' }}>
                  {point.description}
                </div>

                <div style={{ fontSize: '12px', color: '#868e96' }}>
                  {point.device_name || ''} • {point.address}
                  {point.current_value !== undefined && (
                    <>
                      {' • '}
                      <span style={{ fontWeight: '500' }}>
                        {point.current_value}{point.unit && ` ${point.unit}`}
                      </span>
                    </>
                  )}
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* 가상포인트 목록 */}
      {sourceType === 'virtual_point' && (
        <div style={{
          maxHeight: '300px',
          overflowY: 'auto',
          border: '1px solid #e9ecef',
          borderRadius: '6px',
          opacity: loading ? 0.5 : 1, // 깜박임 방지
          transition: 'opacity 0.2s'
        }}>
          {filteredVirtualPoints.length === 0 ? (
            <div style={{
              padding: '20px',
              textAlign: 'center',
              color: '#6c757d'
            }}>
              {virtualPoints.length === 0 ?
                '가상포인트가 없습니다' :
                '검색 조건에 맞는 가상포인트가 없습니다'}
            </div>
          ) : (
            filteredVirtualPoints.slice(0, 100).map(point => ( // 성능 최적화
              <div
                key={point.id}
                onClick={() => handleSelect(point)}
                style={{
                  padding: '12px',
                  borderBottom: '1px solid #f1f3f4',
                  cursor: 'pointer',
                  background: selectedId === point.id ? '#e7f3ff' : 'transparent',
                  transition: 'background-color 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = '#f8f9fa';
                  }
                }}
                onMouseLeave={(e) => {
                  if (selectedId !== point.id) {
                    e.currentTarget.style.background = 'transparent';
                  }
                }}
              >
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'flex-start',
                  marginBottom: '4px'
                }}>
                  <div style={{ fontWeight: '500', color: '#495057' }}>
                    {point.name}
                  </div>
                  <div style={{
                    fontSize: '12px',
                    color: '#6c757d',
                    background: '#f8f9fa',
                    padding: '2px 6px',
                    borderRadius: '3px'
                  }}>
                    {point.data_type}
                  </div>
                </div>

                <div style={{ fontSize: '13px', color: '#6c757d', marginBottom: '2px' }}>
                  {point.description}
                </div>

                <div style={{ fontSize: '12px', color: '#868e96' }}>
                  {point.category}
                  {point.current_value !== undefined && (
                    <>
                      {' • '}
                      <span style={{ fontWeight: '500' }}>
                        {point.current_value}{point.unit && ` ${point.unit}`}
                      </span>
                    </>
                  )}
                </div>
              </div>
            ))
          )}
        </div>
      )}

      {/* 디버그 정보 */}
      {process.env.NODE_ENV === 'development' && (
        <div style={{
          fontSize: '11px',
          color: '#868e96',
          background: '#f8f9fa',
          padding: '8px',
          borderRadius: '4px'
        }}>
          디버그: {sourceType === 'data_point' ?
            `전체 ${dataPoints.length}개, 필터링 ${filteredDataPoints.length}개 표시${deviceFilter ? ` (디바이스 ID: ${deviceFilter})` : ''}` :
            `전체 ${virtualPoints.length}개, 필터링 ${filteredVirtualPoints.length}개 표시`
          }
        </div>
      )}
    </div>
  );
};

export default InputVariableSourceSelector;