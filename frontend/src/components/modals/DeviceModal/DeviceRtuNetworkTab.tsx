// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx
// RTU 네트워크 관리 탭 - protocol_id 지원
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { DeviceApiService } from '../../../api/services/deviceApi';
import DeviceRtuScanDialog from './DeviceRtuScanDialog';
import { Device } from './types';

interface DeviceRtuNetworkTabProps {
  device: Device;
  mode: 'view' | 'edit' | 'create';
  onUpdateDevice?: (device: Device) => void;
}

interface RtuSlaveDevice {
  id: number;
  name: string;
  device_type: string;
  slave_id: number;
  connection_status: string;
  last_communication: string;
  response_time?: number;
  error_count: number;
  is_enabled: boolean;
}

interface RtuMasterInfo {
  id: number;
  name: string;
  device_type: string;
  serial_port: string;
  connection_status: string;
  last_communication: string;
  response_time?: number;
}

const DeviceRtuNetworkTab: React.FC<DeviceRtuNetworkTabProps> = ({
  device,
  mode,
  onUpdateDevice
}) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [slaveDevices, setSlaveDevices] = useState<RtuSlaveDevice[]>([]);
  const [masterDevice, setMasterDevice] = useState<RtuMasterInfo | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [isScanDialogOpen, setIsScanDialogOpen] = useState(false);

  // ========================================================================
  // RTU 디바이스 확인 및 설정 파싱 함수들
  // ========================================================================

  /**
   * RTU 디바이스 여부 확인 - protocol_id 또는 protocol_type 기반
   */
  const isRtuDevice = useCallback((device: Device): boolean => {
    // 1. protocol_type으로 확인
    if (device.protocol_type === 'MODBUS_RTU') {
      return true;
    }

    // 2. protocol_id로 확인 (MODBUS_RTU는 보통 ID 2)
    if (device.protocol_id) {
      const protocol = DeviceApiService.getProtocolManager().getProtocolById(device.protocol_id);
      return protocol?.protocol_type === 'MODBUS_RTU';
    }

    return false;
  }, []);

  /**
   * 디바이스 config에서 RTU 설정 추출
   */
  const parseRtuConfig = useCallback((device: Device) => {
    try {
      const config = typeof device.config === 'string'
        ? JSON.parse(device.config)
        : device.config || {};

      return {
        slave_id: config.slave_id || null,
        master_device_id: config.master_device_id || null,
        baud_rate: config.baud_rate || 9600,
        parity: config.parity || 'N',
        data_bits: config.data_bits || 8,
        stop_bits: config.stop_bits || 1,
        serial_port: config.serial_port || device.endpoint,
        device_role: config.device_role || 'master'
      };
    } catch (error) {
      console.error('RTU config 파싱 실패:', error);
      return {
        slave_id: null,
        master_device_id: null,
        baud_rate: 9600,
        parity: 'N',
        data_bits: 8,
        stop_bits: 1,
        serial_port: device.endpoint,
        device_role: 'master'
      };
    }
  }, []);

  /**
   * RTU 마스터 디바이스 여부 판별
   */
  const isRtuMaster = useCallback((device: Device): boolean => {
    if (!isRtuDevice(device)) return false;

    const config = parseRtuConfig(device);
    return config.device_role === 'master' || !config.slave_id;
  }, [isRtuDevice, parseRtuConfig]);

  /**
   * RTU 슬래이브 디바이스 여부 판별
   */
  const isRtuSlave = useCallback((device: Device): boolean => {
    if (!isRtuDevice(device)) return false;

    const config = parseRtuConfig(device);
    return config.device_role === 'slave' && config.slave_id && config.master_device_id;
  }, [isRtuDevice, parseRtuConfig]);

  // ========================================================================
  // 데이터 로드 함수들
  // ========================================================================

  /**
   * RTU 마스터의 슬래이브 디바이스들 로드
   */
  const loadSlaveDevices = useCallback(async (masterId: number) => {
    try {
      setIsLoading(true);
      setError(null);

      console.log(`RTU 마스터 ${masterId}의 슬래이브 디바이스 로드 중...`);

      // protocol_id 또는 protocol_type을 이용해서 RTU 디바이스들 조회
      const response = await DeviceApiService.getDevices({
        page: 1,
        limit: 100,
        protocol_type: 'MODBUS_RTU' // RTU 디바이스만 필터링
      });

      if (response.success && response.data?.items) {
        const allDevices = response.data.items;

        // RTU 디바이스 중에서 현재 마스터의 슬래이브들만 필터링
        const slaves = allDevices
          .filter(d => {
            if (!isRtuDevice(d)) return false;

            const config = parseRtuConfig(d);
            return config.master_device_id === masterId;
          })
          .map(d => {
            const config = parseRtuConfig(d);
            return {
              id: d.id,
              name: d.name,
              device_type: d.device_type || 'SENSOR',
              slave_id: config.slave_id || 0,
              connection_status: d.connection_status || 'unknown',
              last_communication: d.last_communication || '',
              response_time: d.status_info?.response_time,
              error_count: d.status_info?.error_count || 0,
              is_enabled: d.is_enabled
            };
          })
          .sort((a, b) => a.slave_id - b.slave_id);

        setSlaveDevices(slaves);
        console.log(`슬래이브 디바이스 ${slaves.length}개 로드 완료`);
      } else {
        throw new Error(response.error || '슬래이브 디바이스 조회 실패');
      }
    } catch (error) {
      console.error('슬래이브 디바이스 로드 실패:', error);
      setError(error instanceof Error ? error.message : 'Unknown error');
      setSlaveDevices([]);
    } finally {
      setIsLoading(false);
    }
  }, [isRtuDevice, parseRtuConfig]);

  /**
   * RTU 슬래이브의 마스터 디바이스 정보 로드
   */
  const loadMasterDevice = useCallback(async (masterId: number) => {
    try {
      setIsLoading(true);
      setError(null);

      console.log(`RTU 마스터 디바이스 ${masterId} 정보 로드 중...`);

      const response = await DeviceApiService.getDevice(masterId);
      if (response.success && response.data) {
        const master = response.data;

        // RTU 디바이스인지 확인
        if (!isRtuDevice(master)) {
          throw new Error('마스터 디바이스가 RTU 프로토콜이 아닙니다');
        }

        const config = parseRtuConfig(master);

        setMasterDevice({
          id: master.id,
          name: master.name,
          device_type: master.device_type || 'GATEWAY',
          serial_port: config.serial_port,
          connection_status: master.connection_status || 'unknown',
          last_communication: master.last_communication || '',
          response_time: master.status_info?.response_time
        });

        console.log('마스터 디바이스 정보 로드 완료');
      } else {
        throw new Error(response.error || '마스터 디바이스 조회 실패');
      }
    } catch (error) {
      console.error('마스터 디바이스 로드 실패:', error);
      setError(error instanceof Error ? error.message : 'Unknown error');
      setMasterDevice(null);
    } finally {
      setIsLoading(false);
    }
  }, [isRtuDevice, parseRtuConfig]);

  // ========================================================================
  // 이벤트 핸들러들
  // ========================================================================

  /**
   * 슬래이브 디바이스 토글
   */
  const handleSlaveToggle = async (slaveId: number, enabled: boolean) => {
    try {
      const response = await DeviceApiService.updateDevice(slaveId, {
        is_enabled: enabled
      });

      if (response.success) {
        setSlaveDevices(prev =>
          prev.map(slave =>
            slave.id === slaveId
              ? { ...slave, is_enabled: enabled }
              : slave
          )
        );
      } else {
        throw new Error(response.error || '디바이스 상태 변경 실패');
      }
    } catch (error) {
      console.error('슬래이브 토글 실패:', error);
      alert(`상태 변경 실패: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  /**
   * 슬래이브 스캔 완료 처리
   */
  const handleScanComplete = (foundDevices?: any[]) => {
    setIsScanDialogOpen(false);
    if (foundDevices && foundDevices.length > 0) {
      if (isRtuMaster(device)) {
        loadSlaveDevices(device.id);
      }
    }
  };

  /**
   * 연결 테스트
   */
  const handleTestConnection = async (targetDevice: Device | RtuSlaveDevice) => {
    try {
      const deviceId = 'id' in targetDevice ? targetDevice.id : (targetDevice as RtuSlaveDevice).id;
      const response = await DeviceApiService.testDeviceConnection(deviceId);

      if (response.success && response.data) {
        const result = response.data;
        alert(
          result.test_successful
            ? `연결 성공! 응답시간: ${result.response_time_ms}ms`
            : `연결 실패: ${result.error_message}`
        );
      } else {
        alert(`테스트 실패: ${response.error}`);
      }
    } catch (error) {
      console.error('연결 테스트 실패:', error);
      alert(`연결 테스트 오류: ${error instanceof Error ? error.message : 'Unknown error'}`);
    }
  };

  // ========================================================================
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    if (device && isRtuDevice(device)) {
      const config = parseRtuConfig(device);

      if (isRtuMaster(device)) {
        loadSlaveDevices(device.id);
      } else if (isRtuSlave(device) && config.master_device_id) {
        loadMasterDevice(config.master_device_id);
      }
    }
  }, [device, loadSlaveDevices, loadMasterDevice, isRtuDevice, isRtuMaster, isRtuSlave, parseRtuConfig]);

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================

  const getStatusBadge = (status: string) => {
    const statusMap = {
      connected: { color: '#059669', bg: '#dcfce7', text: '연결됨' },
      disconnected: { color: '#dc2626', bg: '#fef2f2', text: '연결끊김' },
      connecting: { color: '#d97706', bg: '#fef3c7', text: '연결중' },
      error: { color: '#dc2626', bg: '#fef2f2', text: '오류' },
      unknown: { color: '#6b7280', bg: '#f3f4f6', text: '알수없음' }
    };

    const style = statusMap[status as keyof typeof statusMap] || statusMap.unknown;

    return (
      <span
        style={{
          display: 'inline-flex',
          alignItems: 'center',
          gap: '0.375rem',
          padding: '0.25rem 0.75rem',
          borderRadius: '9999px',
          fontSize: '0.75rem',
          fontWeight: '500',
          color: style.color,
          backgroundColor: style.bg,
          border: `1px solid ${style.color}40`
        }}
      >
        <i className="fas fa-circle"></i>
        {style.text}
      </span>
    );
  };

  const formatLastSeen = (timestamp: string) => {
    if (!timestamp) return 'N/A';

    try {
      const date = new Date(timestamp);
      const now = new Date();
      const diffMs = now.getTime() - date.getTime();
      const diffMinutes = Math.floor(diffMs / (1000 * 60));

      if (diffMinutes < 1) return '방금 전';
      if (diffMinutes < 60) return `${diffMinutes}분 전`;
      if (diffMinutes < 1440) return `${Math.floor(diffMinutes / 60)}시간 전`;
      return `${Math.floor(diffMinutes / 1440)}일 전`;
    } catch {
      return timestamp;
    }
  };

  // 프로토콜 정보 가져오기
  const getProtocolInfo = () => {
    if (device?.protocol_id) {
      return DeviceApiService.getProtocolManager().getProtocolById(device.protocol_id);
    }
    return DeviceApiService.getProtocolManager().getProtocolByType(device?.protocol_type || '');
  };

  const protocolInfo = getProtocolInfo();

  // ========================================================================
  // 렌더링
  // ========================================================================

  if (!device || !isRtuDevice(device)) {
    return (
      <div style={{ padding: '1.5rem' }}>
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          padding: '2rem',
          color: '#dc2626'
        }}>
          <i className="fas fa-exclamation-triangle" style={{ fontSize: '2rem', marginBottom: '1rem' }}></i>
          <p>RTU 디바이스가 아닙니다</p>
          {protocolInfo && (
            <p style={{ fontSize: '0.875rem', color: '#6b7280', marginTop: '0.5rem' }}>
              현재 프로토콜: {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
            </p>
          )}
        </div>
      </div>
    );
  }

  const config = parseRtuConfig(device);
  const isMaster = isRtuMaster(device);
  const isSlave = isRtuSlave(device);

  return (
    <div style={{ padding: '1.5rem', height: '100%', overflowY: 'auto' }}>
      {/* RTU 디바이스 기본 정보 */}
      <div style={{ marginBottom: '1.5rem' }}>
        <h3 style={{
          display: 'flex',
          alignItems: 'center',
          gap: '0.75rem',
          margin: '0 0 1rem 0',
          fontSize: '1rem',
          fontWeight: '600',
          color: '#374151'
        }}>
          <i className="fas fa-microchip"></i>
          RTU {isMaster ? '마스터' : '슬래이브'} 정보
          {protocolInfo && (
            <span style={{
              fontSize: '0.75rem',
              color: '#6b7280',
              background: '#f3f4f6',
              padding: '0.25rem 0.5rem',
              borderRadius: '0.25rem',
              marginLeft: 'auto'
            }}>
              {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
            </span>
          )}
        </h3>

        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(150px, 1fr))',
          gap: '0.75rem'
        }}>
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '0.25rem',
            padding: '0.75rem',
            background: '#f9fafb',
            borderRadius: '0.375rem',
            border: '1px solid #e5e7eb'
          }}>
            <label style={{
              fontSize: '0.75rem',
              fontWeight: '500',
              color: '#6b7280'
            }}>슬래이브 ID</label>
            <div style={{
              fontSize: '0.875rem',
              fontWeight: '600',
              color: '#1f2937'
            }}>
              {config.slave_id || 'N/A'}
            </div>
          </div>

          <div style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '0.25rem',
            padding: '0.75rem',
            background: '#f9fafb',
            borderRadius: '0.375rem',
            border: '1px solid #e5e7eb'
          }}>
            <label style={{
              fontSize: '0.75rem',
              fontWeight: '500',
              color: '#6b7280'
            }}>시리얼 포트</label>
            <div style={{
              fontSize: '0.875rem',
              fontWeight: '600',
              color: '#1f2937'
            }}>
              {config.serial_port || device.endpoint}
            </div>
          </div>

          <div style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '0.25rem',
            padding: '0.75rem',
            background: '#f9fafb',
            borderRadius: '0.375rem',
            border: '1px solid #e5e7eb'
          }}>
            <label style={{
              fontSize: '0.75rem',
              fontWeight: '500',
              color: '#6b7280'
            }}>마스터 디바이스 ID</label>
            <div style={{
              fontSize: '0.875rem',
              fontWeight: '600',
              color: '#1f2937'
            }}>
              {config.master_device_id || 'N/A'}
            </div>
          </div>

          <div style={{
            display: 'flex',
            flexDirection: 'column',
            gap: '0.25rem',
            padding: '0.75rem',
            background: '#f9fafb',
            borderRadius: '0.375rem',
            border: '1px solid #e5e7eb'
          }}>
            <label style={{
              fontSize: '0.75rem',
              fontWeight: '500',
              color: '#6b7280'
            }}>통신 상태</label>
            <div>
              {getStatusBadge(device.connection_status || 'unknown')}
            </div>
          </div>
        </div>

        {/* 통신 파라미터 표시 */}
        <div style={{ marginTop: '1rem', padding: '0.75rem', background: '#f0f9ff', borderRadius: '0.5rem', border: '1px solid #bae6fd' }}>
          <div style={{ fontSize: '0.75rem', color: '#0c4a6e', marginBottom: '0.5rem' }}>
            <strong>통신 파라미터:</strong>
          </div>
          <div style={{ fontSize: '0.75rem', color: '#0c4a6e' }}>
            {config.baud_rate} bps, {config.data_bits}N{config.stop_bits}, Parity: {config.parity}, Role: {config.device_role}
          </div>
        </div>
      </div>

      {/* 통신 통계 */}
      <div style={{ marginBottom: '1.5rem' }}>
        <h3 style={{
          display: 'flex',
          alignItems: 'center',
          gap: '0.75rem',
          margin: '0 0 1rem 0',
          fontSize: '1rem',
          fontWeight: '600',
          color: '#374151'
        }}>
          <i className="fas fa-chart-bar"></i>
          통신 통계
        </h3>

        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(3, 1fr)',
          gap: '0.5rem'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '0.5rem',
            padding: '0.5rem',
            background: 'white',
            border: '1px solid #e5e7eb',
            borderRadius: '0.375rem',
            boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
          }}>
            <div style={{
              width: '1.5rem',
              height: '1.5rem',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              borderRadius: '0.25rem',
              background: '#f3f4f6',
              color: '#6b7280',
              fontSize: '0.75rem'
            }}>
              <i className="fas fa-clock"></i>
            </div>
            <div style={{ flex: '1' }}>
              <div style={{
                fontSize: '0.875rem',
                fontWeight: '600',
                color: '#1f2937',
                margin: '0'
              }}>
                {device.status_info?.response_time || '--'}ms
              </div>
              <div style={{
                fontSize: '0.625rem',
                color: '#6b7280'
              }}>평균 응답시간</div>
            </div>
          </div>

          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '0.5rem',
            padding: '0.5rem',
            background: 'white',
            border: '1px solid #e5e7eb',
            borderRadius: '0.375rem',
            boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
          }}>
            <div style={{
              width: '1.5rem',
              height: '1.5rem',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              borderRadius: '0.25rem',
              background: '#fef3c7',
              color: '#d97706',
              fontSize: '0.75rem'
            }}>
              <i className="fas fa-exclamation-triangle"></i>
            </div>
            <div style={{ flex: '1' }}>
              <div style={{
                fontSize: '0.875rem',
                fontWeight: '600',
                color: '#dc2626',
                margin: '0'
              }}>
                {device.status_info?.error_count || 0}
              </div>
              <div style={{
                fontSize: '0.625rem',
                color: '#6b7280'
              }}>통신 오류</div>
            </div>
          </div>

          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '0.5rem',
            padding: '0.5rem',
            background: 'white',
            border: '1px solid #e5e7eb',
            borderRadius: '0.375rem',
            boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
          }}>
            <div style={{
              width: '1.5rem',
              height: '1.5rem',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              borderRadius: '0.25rem',
              background: '#dcfce7',
              color: '#059669',
              fontSize: '0.75rem'
            }}>
              <i className="fas fa-check-circle"></i>
            </div>
            <div style={{ flex: '1' }}>
              <div style={{
                fontSize: '0.875rem',
                fontWeight: '600',
                color: '#059669',
                margin: '0'
              }}>
                {device.status_info?.uptime_percentage || '--'}%
              </div>
              <div style={{
                fontSize: '0.625rem',
                color: '#6b7280'
              }}>성공률</div>
            </div>
          </div>
        </div>
      </div>

      {/* RTU 마스터 전용: 슬래이브 관리 */}
      {isMaster && (
        <div>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '1rem'
          }}>
            <h3 style={{
              display: 'flex',
              alignItems: 'center',
              gap: '0.75rem',
              margin: '0',
              fontSize: '1rem',
              fontWeight: '600',
              color: '#374151'
            }}>
              <i className="fas fa-sitemap"></i>
              연결된 슬래이브 디바이스 ({slaveDevices.length})
            </h3>
            <div style={{ display: 'flex', gap: '0.75rem' }}>
              <button
                style={{
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '0.5rem',
                  padding: '0.5rem 1rem',
                  border: 'none',
                  borderRadius: '0.375rem',
                  fontSize: '0.875rem',
                  fontWeight: '500',
                  cursor: 'pointer',
                  transition: 'all 0.2s ease',
                  background: '#64748b',
                  color: 'white'
                }}
                onClick={() => loadSlaveDevices(device.id)}
                disabled={isLoading}
              >
                <i className="fas fa-sync-alt"></i>
                새로고침
              </button>
              <button
                style={{
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '0.5rem',
                  padding: '0.5rem 1rem',
                  border: 'none',
                  borderRadius: '0.375rem',
                  fontSize: '0.875rem',
                  fontWeight: '500',
                  cursor: 'pointer',
                  transition: 'all 0.2s ease',
                  background: '#0ea5e9',
                  color: 'white'
                }}
                onClick={() => setIsScanDialogOpen(true)}
              >
                <i className="fas fa-search"></i>
                슬래이브 스캔
              </button>
            </div>
          </div>

          {isLoading ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '2rem',
              textAlign: 'center',
              color: '#6b7280'
            }}>
              <i className="fas fa-spinner fa-spin" style={{ fontSize: '2rem', marginBottom: '1rem', color: '#0ea5e9' }}></i>
              <span>로딩 중...</span>
            </div>
          ) : error ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '2rem',
              textAlign: 'center',
              color: '#6b7280'
            }}>
              <i className="fas fa-exclamation-triangle" style={{ fontSize: '2rem', marginBottom: '1rem', color: '#dc2626' }}></i>
              <span>{error}</span>
            </div>
          ) : slaveDevices.length === 0 ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '2rem',
              textAlign: 'center',
              color: '#6b7280'
            }}>
              <i className="fas fa-inbox" style={{ fontSize: '3rem', marginBottom: '1rem' }}></i>
              <h4 style={{
                margin: '0 0 0.5rem 0',
                fontSize: '1.125rem',
                fontWeight: '600',
                color: '#374151'
              }}>연결된 슬래이브가 없습니다</h4>
              <p>스캔을 실행하여 새로운 슬래이브 디바이스를 찾아보세요</p>
              <button
                style={{
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '0.5rem',
                  padding: '0.5rem 1rem',
                  border: 'none',
                  borderRadius: '0.375rem',
                  fontSize: '0.875rem',
                  fontWeight: '500',
                  cursor: 'pointer',
                  background: '#0ea5e9',
                  color: 'white',
                  marginTop: '1rem'
                }}
                onClick={() => setIsScanDialogOpen(true)}
              >
                <i className="fas fa-search"></i>
                슬래이브 스캔 시작
              </button>
            </div>
          ) : (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
              {slaveDevices.map((slave) => (
                <div key={slave.id} style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '1rem',
                  padding: '1rem',
                  background: 'white',
                  border: '1px solid #e5e7eb',
                  borderRadius: '0.75rem',
                  boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)',
                  transition: 'all 0.2s ease'
                }}>
                  <div style={{
                    width: '2.5rem',
                    height: '2.5rem',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    background: '#dbeafe',
                    color: '#1e40af',
                    borderRadius: '0.375rem',
                    fontSize: '1.125rem'
                  }}>
                    <i className="fas fa-microchip"></i>
                  </div>

                  <div style={{ flex: '1' }}>
                    <div style={{
                      fontSize: '0.875rem',
                      fontWeight: '600',
                      color: '#1f2937',
                      marginBottom: '0.25rem'
                    }}>{slave.name}</div>
                    <div style={{
                      fontSize: '0.75rem',
                      color: '#6b7280'
                    }}>
                      슬래이브 ID: {slave.slave_id} • {slave.device_type}
                    </div>
                  </div>

                  <div style={{
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    gap: '0.5rem'
                  }}>
                    {getStatusBadge(slave.connection_status)}
                    <div style={{
                      fontSize: '0.75rem',
                      color: '#6b7280'
                    }}>
                      {formatLastSeen(slave.last_communication)}
                    </div>
                  </div>

                  <div style={{
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '0.25rem',
                    minWidth: '80px'
                  }}>
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      fontSize: '0.75rem'
                    }}>
                      <span style={{ color: '#6b7280' }}>응답시간</span>
                      <span style={{ fontWeight: '600', color: '#1f2937' }}>{slave.response_time || '--'}ms</span>
                    </div>
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      fontSize: '0.75rem'
                    }}>
                      <span style={{ color: '#6b7280' }}>오류</span>
                      <span style={{
                        fontWeight: '600',
                        color: slave.error_count > 0 ? '#dc2626' : '#1f2937'
                      }}>{slave.error_count}</span>
                    </div>
                  </div>

                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '0.75rem'
                  }}>
                    <button
                      style={{
                        display: 'inline-flex',
                        alignItems: 'center',
                        gap: '0.5rem',
                        padding: '0.375rem 0.75rem',
                        border: 'none',
                        borderRadius: '0.375rem',
                        fontSize: '0.75rem',
                        fontWeight: '500',
                        cursor: 'pointer',
                        background: '#64748b',
                        color: 'white'
                      }}
                      onClick={() => handleTestConnection(slave)}
                      title="연결 테스트"
                    >
                      <i className="fas fa-plug"></i>
                    </button>

                    <label style={{
                      position: 'relative',
                      display: 'inline-block',
                      width: '44px',
                      height: '24px',
                      cursor: 'pointer'
                    }} title={slave.is_enabled ? '비활성화' : '활성화'}>
                      <input
                        type="checkbox"
                        checked={slave.is_enabled}
                        onChange={(e) => handleSlaveToggle(slave.id, e.target.checked)}
                        style={{ opacity: 0, width: 0, height: 0 }}
                      />
                      <span style={{
                        position: 'absolute',
                        top: 0,
                        left: 0,
                        right: 0,
                        bottom: 0,
                        background: slave.is_enabled ? '#059669' : '#cbd5e1',
                        borderRadius: '24px',
                        transition: 'all 0.2s ease'
                      }}>
                        <span style={{
                          position: 'absolute',
                          content: '""',
                          height: '18px',
                          width: '18px',
                          left: '3px',
                          bottom: '3px',
                          background: 'white',
                          borderRadius: '50%',
                          transition: 'all 0.2s ease',
                          boxShadow: '0 1px 3px rgba(0, 0, 0, 0.3)',
                          transform: slave.is_enabled ? 'translateX(20px)' : 'translateX(0)'
                        }}></span>
                      </span>
                    </label>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      )}

      {/* RTU 슬래이브 전용: 마스터 정보 */}
      {isSlave && (
        <div>
          <h3 style={{
            display: 'flex',
            alignItems: 'center',
            gap: '0.75rem',
            margin: '0 0 1rem 0',
            fontSize: '1rem',
            fontWeight: '600',
            color: '#374151'
          }}>
            <i className="fas fa-server"></i>
            마스터 디바이스 정보
          </h3>

          {isLoading ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '2rem',
              textAlign: 'center',
              color: '#6b7280'
            }}>
              <i className="fas fa-spinner fa-spin" style={{ fontSize: '2rem', marginBottom: '1rem', color: '#0ea5e9' }}></i>
              <span>마스터 정보 로딩 중...</span>
            </div>
          ) : error ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '2rem',
              textAlign: 'center',
              color: '#6b7280'
            }}>
              <i className="fas fa-exclamation-triangle" style={{ fontSize: '2rem', marginBottom: '1rem', color: '#dc2626' }}></i>
              <span>{error}</span>
            </div>
          ) : masterDevice ? (
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '1rem',
              padding: '1rem',
              background: 'white',
              border: '1px solid #e5e7eb',
              borderRadius: '0.5rem',
              boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)'
            }}>
              <div style={{
                width: '2.5rem',
                height: '2.5rem',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                background: '#dbeafe',
                color: '#1e40af',
                borderRadius: '0.375rem',
                fontSize: '1rem'
              }}>
                <i className="fas fa-server"></i>
              </div>

              <div style={{ flex: '1' }}>
                <div style={{
                  fontSize: '1rem',
                  fontWeight: '600',
                  color: '#1f2937',
                  marginBottom: '0.25rem'
                }}>{masterDevice.name}</div>
                <div style={{
                  fontSize: '0.875rem',
                  color: '#6b7280'
                }}>
                  ID: {masterDevice.id} • {masterDevice.device_type} • {masterDevice.serial_port}
                </div>
              </div>

              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                gap: '0.5rem'
              }}>
                {getStatusBadge(masterDevice.connection_status)}
                <div style={{
                  fontSize: '0.75rem',
                  color: '#6b7280',
                  textAlign: 'center'
                }}>
                  마지막 통신: {formatLastSeen(masterDevice.last_communication)}
                </div>
              </div>

              <div>
                <button
                  style={{
                    display: 'inline-flex',
                    alignItems: 'center',
                    gap: '0.5rem',
                    padding: '0.5rem 1rem',
                    border: 'none',
                    borderRadius: '0.375rem',
                    fontSize: '0.875rem',
                    fontWeight: '500',
                    cursor: 'pointer',
                    background: '#64748b',
                    color: 'white'
                  }}
                  onClick={() => handleTestConnection(device)}
                >
                  <i className="fas fa-plug"></i>
                  연결 테스트
                </button>
              </div>
            </div>
          ) : (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '2rem',
              textAlign: 'center',
              color: '#6b7280'
            }}>
              <i className="fas fa-unlink" style={{ fontSize: '2rem', marginBottom: '1rem', color: '#dc2626' }}></i>
              <p>마스터 디바이스를 찾을 수 없습니다</p>
            </div>
          )}
        </div>
      )}

      {/* 슬래이브 스캔 다이얼로그 */}
      {isScanDialogOpen && (
        <DeviceRtuScanDialog
          isOpen={isScanDialogOpen}
          masterDevice={device}
          onClose={() => setIsScanDialogOpen(false)}
          onScanComplete={handleScanComplete}
        />
      )}
    </div>
  );
};

export default DeviceRtuNetworkTab;