import React, { useState, useEffect, useCallback } from 'react';
import { DashboardApiService, ServiceInfo } from '../api/services/dashboardApi';
import exportGatewayApi from '../api/services/exportGatewayApi';
import '../styles/base.css';
import '../styles/system-status.css';

const SystemStatus: React.FC = () => {
  const [services, setServices] = useState<ServiceInfo[]>([]);
  const [isProcessing, setIsProcessing] = useState(false);
  const [loading, setLoading] = useState(true);
  const [selectedServices, setSelectedServices] = useState<string[]>([]);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [error, setError] = useState<string | null>(null);
  const [expandedCollectors, setExpandedCollectors] = useState<string[]>([]);
  const [expandedDevices, setExpandedDevices] = useState<number[]>([]);
  const [hierarchy, setHierarchy] = useState<any[]>([]);
  const [unassignedCollectors, setUnassignedCollectors] = useState<ServiceInfo[]>([]);
  const [expandedSites, setExpandedSites] = useState<number[]>([]);

  const fetchStatus = useCallback(async () => {
    try {
      setError(null);

      // 병렬 데이터 페칭 (Overview + Services Status + Gateways)
      const [overviewRes, statusRes, gatewaysRes] = await Promise.all([
        DashboardApiService.getOverview(),
        DashboardApiService.getServicesStatus(),
        exportGatewayApi.getGateways()
      ]);

      if (overviewRes.success && overviewRes.data) {
        setHierarchy(overviewRes.data.hierarchy || []);
        setUnassignedCollectors(overviewRes.data.unassigned_collectors || []);
        setLastUpdate(new Date());
      } else {
        setError(overviewRes.message || '데이터를 불러오는데 실패했습니다.');
      }

      let mergedServices: ServiceInfo[] = [];

      // 1. Redis Heartbeat 기반 서비스 상태
      if (statusRes.success && statusRes.data) {
        // 백엔드 응답이 { services: [], summary: {} } 형태일 수 있음
        const servicesData = (statusRes.data as any).services || statusRes.data;
        if (Array.isArray(servicesData)) {
          mergedServices = [...servicesData];
        }
      } else if (overviewRes.success && overviewRes.data) {
        // Fallback
        mergedServices = overviewRes.data.services.details;
      }

      // 2. Export Gateway 병합 및 계층화
      let exportGatewayParent: ServiceInfo | undefined = mergedServices.find(s => s.name === 'export-gateway');

      // 'export-gateway'라는 이름의 서비스가 없다면 인위적으로 생성
      if (!exportGatewayParent) {
        exportGatewayParent = {
          name: 'export-gateway',
          displayName: 'Export Gateway Service',
          status: 'running',
          icon: 'fas fa-share-square',
          controllable: false,
          description: '산업용 데이터 외부 송출 및 연동 서비스',
          serviceType: 'core',
        } as ServiceInfo;
        mergedServices.push(exportGatewayParent);
      }

      if (gatewaysRes.success && gatewaysRes.data && gatewaysRes.data.items) {
        const gateways = gatewaysRes.data.items;

        const gatewayServices: ServiceInfo[] = gateways.map(gw => ({
          name: `export-gateway-${gw.id}`,
          displayName: gw.server_name || gw.name || 'Unnamed Gateway',
          status: (
            gw.live_status?.status === 'online' ||
            gw.live_status?.status === 'running' ||
            gw.status === 'online' ||
            gw.status === 'active'   // edge_servers.status = 'active' = ALIVE
          ) ? 'running' : 'stopped',
          icon: 'fas fa-server',
          controllable: true,
          description: gw.description || `Export Gateway (ID: ${gw.id})`,
          serviceType: 'export-gateway',
          gatewayId: gw.id,
          ip: gw.ip_address,
          memoryUsage: gw.live_status?.memory_usage
        }));

        (exportGatewayParent as any).gateways = gatewayServices;

        const anyRunning = gatewayServices.some(g => g.status === 'running');
        exportGatewayParent.status = anyRunning ? 'running' : 'stopped';
      }

      // 3. 중복 제거
      mergedServices = mergedServices.filter(s =>
        s.name === 'export-gateway' ||
        !s.name.startsWith('export-gateway-')
      );


      setServices(mergedServices);

    } catch (err) {
      console.error('Failed to fetch system status:', err);
      setError('서버와 통신하는 중 오류가 발생했습니다.');
    } finally {
      setLoading(false);
    }
  }, []);

  // 실시간 업데이트
  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 10000);
    return () => clearInterval(interval);
  }, [fetchStatus]);

  // 개별 서비스 제어
  const handleServiceControl = async (service: ServiceInfo, action: 'start' | 'stop' | 'restart') => {
    if (!service.controllable) {
      alert(`${service.displayName}는 필수 서비스로 제어할 수 없습니다.`);
      return;
    }

    try {
      setIsProcessing(true);
      let response;

      if (service.serviceType === 'export-gateway' && service.gatewayId) {
        // Export Gateway 제어
        if (action === 'start') response = await exportGatewayApi.startGatewayProcess(service.gatewayId);
        else if (action === 'stop') response = await exportGatewayApi.stopGatewayProcess(service.gatewayId);
        else if (action === 'restart') response = await exportGatewayApi.restartGatewayProcess(service.gatewayId);
      } else {
        // 일반 서비스 제어
        response = await DashboardApiService.controlService(service.name, action);
      }

      if (response && response.success) {
        console.log(`${service.displayName} ${action} 완료`);
        await fetchStatus(); // 상태 새로고침
      } else {
        const msg = response?.message || '알 수 없는 오류';
        alert(`${service.displayName} ${action} 실패: ${msg}`);
      }
    } catch (error) {
      console.error(`서비스 제어 실패:`, error);
      alert(`${service.displayName} ${action} 중 오류가 발생했습니다.`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 디바이스 제어 (Start/Stop/Restart)
  const handleDeviceControl = async (service: ServiceInfo, device: any, action: 'start' | 'stop' | 'restart') => {
    try {
      setIsProcessing(true);
      const response = await DashboardApiService.controlDevice(device.id, action);

      if (response.success) {
        console.log(`Device ${device.name} ${action} 완료`);
        await fetchStatus();
      } else {
        alert(`${device.name} ${action} 실패: ${response.message}`);
      }
    } catch (err) {
      console.error(`디바이스 제어 실패:`, err);
      alert(`${device.name} ${action} 중 오류가 발생했습니다.`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 포인트 제어 (Digital/Analog)
  const handlePointControl = async (device: any, pointId: number, type: 'digital' | 'analog', value: any) => {
    try {
      setIsProcessing(true);
      const response = await DashboardApiService.controlPoint(device.id, pointId, type, value);

      if (response.success) {
        alert(`${type === 'digital' ? 'DO' : 'AO'} 제어 명령이 성공적으로 전송되었습니다.`);
        await fetchStatus();
      } else {
        alert(`${type === 'digital' ? 'DO' : 'AO'} 제어 실패: ${response.message}`);
      }
    } catch (err) {
      console.error(`포인트 제어 실패:`, err);
      alert(`제어 명령 전송 중 오류가 발생했습니다.`);
    } finally {
      setIsProcessing(false);
    }
  };

  const toggleCollector = (serviceName: string) => {
    setExpandedCollectors(prev =>
      prev.includes(serviceName)
        ? prev.filter(name => name !== serviceName)
        : [...prev, serviceName]
    );
  };

  const toggleDevice = (deviceId: number) => {
    setExpandedDevices(prev =>
      prev.includes(deviceId)
        ? prev.filter(id => id !== deviceId)
        : [...prev, deviceId]
    );
  };

  // 일괄 제어 패널
  const handleBulkAction = async (action: 'start' | 'stop' | 'restart') => {
    const controllableServices = services.filter(s => s.controllable);

    if (controllableServices.length === 0) {
      alert('제어 가능한 서비스가 없습니다.');
      return;
    }

    const confirmation = confirm(
      `${controllableServices.length}개의 서비스를 ${action === 'start' ? '시작' :
        action === 'stop' ? '정지' : '재시작'
      }하시겠습니까?`
    );

    if (!confirmation) return;

    try {
      setIsProcessing(true);

      // 순차적으로 처리
      for (const service of controllableServices) {
        await DashboardApiService.controlService(service.name, action);
      }

      await fetchStatus();
      alert(`${controllableServices.length}개 서비스 ${action} 완료`);
    } catch (error) {
      console.error('일괄 서비스 제어 실패:', error);
      alert('일괄 서비스 제어 중 오류가 발생했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  // 선택된 서비스 제어
  const handleSelectedAction = async (action: 'start' | 'stop' | 'restart') => {
    if (selectedServices.length === 0) {
      alert('서비스를 선택해주세요.');
      return;
    }

    const selectedServiceObjects = services.filter(s =>
      selectedServices.includes(s.name) && s.controllable
    );

    if (selectedServiceObjects.length === 0) {
      alert('제어 가능한 서비스가 선택되지 않았습니다.');
      return;
    }

    try {
      setIsProcessing(true);

      for (const service of selectedServiceObjects) {
        await DashboardApiService.controlService(service.name, action);
      }

      await fetchStatus();
      alert(`${selectedServiceObjects.length}개 서비스 ${action} 완료`);
      setSelectedServices([]);
    } catch (error) {
      console.error('선택된 서비스 제어 실패:', error);
      alert('선택된 서비스 제어 중 오류가 발생했습니다.');
    } finally {
      setIsProcessing(false);
    }
  };

  const handleServiceSelect = (serviceName: string) => {
    setSelectedServices(prev =>
      prev.includes(serviceName)
        ? prev.filter(name => name !== serviceName)
        : [...prev, serviceName]
    );
  };

  const formatTimeAgo = (date: Date) => {
    const now = new Date();
    const diffMs = now.getTime() - date.getTime();
    const diffSecs = Math.floor(diffMs / 1000);

    if (diffSecs < 60) return '방금 전';
    const diffMins = Math.floor(diffSecs / 60);
    if (diffMins < 60) return `${diffMins}분 전`;
    return `${Math.floor(diffMins / 60)}시간 전`;
  };

  const formatUptime = (seconds?: number) => {
    if (seconds === undefined || seconds === null) return 'N/A';
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    if (hours > 0) return `${hours}h ${minutes}m`;
    return `${minutes}m`;
  };

  const formatResource = (val?: number | string) => {
    if (val === undefined || val === null || val === 'N/A') return 'N/A';
    const num = typeof val === 'string' ? parseFloat(val) : val;
    return isNaN(num) ? 'N/A' : `${num.toFixed(1)}%`;
  };

  if (loading && services.length === 0) {
    return (
      <div className="loading-container" style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '60vh' }}>
        <i className="fas fa-spinner fa-spin fa-3x" style={{ color: '#667eea', marginBottom: '16px' }}></i>
        <div style={{ fontSize: '1.2rem', color: '#4a5568' }}>데이터를 불러오는 중...</div>
      </div>
    );
  }

  const coreServices = services.filter(s => !s.name.startsWith('collector') && s.name !== 'export-gateway');
  const collectorServices = services.filter(s => s.name.startsWith('collector'));
  const exportGatewayServices = services.filter(s => s.name === 'export-gateway');

  const toggleSite = (siteId: number) => {
    setExpandedSites(prev =>
      prev.includes(siteId)
        ? prev.filter(id => id !== siteId)
        : [...prev, siteId]
    );
  };

  const renderServiceRow = (service: ServiceInfo) => {
    const isCollector = service.name.startsWith('collector');
    const isExportGatewayParent = service.name === 'export-gateway';

    // Parent services use the same expansion state array for simplicity
    const isExpanded = expandedCollectors.includes(service.name);

    return (
      <React.Fragment key={service.name}>
        <tr style={{ borderBottom: '1px solid #edf2f7', transition: 'background 0.2s' }} className="hover:bg-gray-50">
          <td style={{ padding: '16px 24px', textAlign: 'center', width: '48px' }}>
            {isCollector || isExportGatewayParent ? (
              <button
                onClick={() => toggleCollector(service.name)}
                style={{ background: 'none', border: 'none', cursor: 'pointer', color: '#4a5568' }}
              >
                <i className={`fas ${isExpanded ? 'fa-chevron-down' : 'fa-chevron-right'}`}></i>
              </button>
            ) : (
              <input
                type="checkbox"
                checked={selectedServices.includes(service.name)}
                onChange={() => handleServiceSelect(service.name)}
                disabled={!service.controllable}
                style={{ transform: 'scale(1.1)', cursor: service.controllable ? 'pointer' : 'default' }}
              />
            )}
          </td>
          <td style={{ padding: '16px 24px', width: '40%' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
              <div style={{
                width: '40px',
                height: '40px',
                borderRadius: '10px',
                background: service.status === 'running' ? '#ebf4ff' : '#f7fafc',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: service.status === 'running' ? '#4299e1' : '#a0aec0'
              }}>
                <i className={service.icon || 'fas fa-cog'} style={{ fontSize: '1.1rem' }}></i>
              </div>
              <div>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
                  <span style={{ fontWeight: 600, color: '#2d3748' }}>{service.displayName}</span>
                  {service.collectorId && (
                    <span style={{ fontSize: '0.7rem', color: '#667eea', background: '#ebf4ff', padding: '1px 6px', borderRadius: '4px', fontWeight: 700, border: '1px solid #c3dafe' }}>
                      ID: {service.collectorId}
                    </span>
                  )}
                  {isExportGatewayParent && (
                    <span style={{ fontSize: '0.7rem', color: '#805ad5', background: '#faf5ff', padding: '1px 6px', borderRadius: '4px', fontWeight: 700, border: '1px solid #e9d8fd' }}>
                      SERVICE HUB
                    </span>
                  )}
                  {isCollector && !service.exists && (
                    <span style={{ fontSize: '0.7rem', color: '#e53e3e', background: '#fff5f5', padding: '1px 6px', borderRadius: '4px', border: '1px solid #feb2b2' }}>미등록</span>
                  )}
                </div>
                <div style={{ fontSize: '0.8rem', color: '#718096', marginTop: '2px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                  {service.ip ? (
                    <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: '#2b6cb0', fontWeight: 500 }}>
                      <i className="fas fa-network-wired" style={{ fontSize: '0.7rem' }}></i> {service.ip}
                    </span>
                  ) : (
                    <span>{service.description}</span>
                  )}
                </div>
              </div>
            </div>
          </td>
          <td style={{ padding: '16px 24px', width: '15%', textAlign: 'center' }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}>
              <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: service.status === 'running' ? '#48bb78' : '#e53e3e' }}></div>
              <span style={{
                fontSize: '0.85rem',
                fontWeight: 700,
                color: service.status === 'running' ? '#2f855a' : '#c53030'
              }}>
                {service.status === 'running' ? 'ALIVE' : 'DOWN'}
              </span>
            </div>
          </td>
          <td style={{ padding: '16px 24px', width: '12%', textAlign: 'center', fontSize: '0.85rem', color: '#4a5568' }}>
            {service.uptime || 'N/A'}
          </td>
          <td style={{ padding: '16px 24px', width: '18%', textAlign: 'center' }}>
            <div style={{ display: 'flex', justifyContent: 'center', gap: '8px' }}>
              <span style={{ fontSize: '0.75rem', padding: '2px 8px', background: '#f1f5f9', borderRadius: '4px', color: '#475569', fontWeight: 500 }}>
                CPU: {formatResource(service.cpuUsage)}
              </span>
              <span style={{ fontSize: '0.75rem', padding: '2px 8px', background: '#f1f5f9', borderRadius: '4px', color: '#475569', fontWeight: 500 }}>
                MEM: {formatResource(service.memoryUsage)}
              </span>
            </div>
          </td>
          <td style={{ padding: '16px 24px', textAlign: 'right', width: '15%' }}>
            <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '8px' }}>
              {!service.controllable ? (
                <span style={{ fontSize: '0.75rem', color: '#cbd5e0', fontStyle: 'italic' }}>
                  {isExportGatewayParent ? 'EXPAND TO MANAGE' : 'SYSTEM LOCKED'}
                </span>
              ) : (
                <>
                  {service.status === 'running' ? (
                    <button
                      onClick={() => handleServiceControl(service, 'stop')}
                      className="btn-icon"
                      style={{ width: '30px', height: '30px', borderRadius: '6px', border: '1px solid #fed7d7', background: '#fff5f5', color: '#c53030', cursor: 'pointer' }}
                      title="중지"
                      disabled={isProcessing}
                    >
                      <i className="fas fa-stop"></i>
                    </button>
                  ) : (
                    <button
                      onClick={() => handleServiceControl(service, 'start')}
                      className="btn-icon"
                      style={{ width: '30px', height: '30px', borderRadius: '6px', border: '1px solid #c6f6d5', background: '#f0fff4', color: '#2f855a', cursor: 'pointer' }}
                      title="시작"
                      disabled={isProcessing}
                    >
                      <i className="fas fa-play"></i>
                    </button>
                  )}
                  <button
                    onClick={() => handleServiceControl(service, 'restart')}
                    className="btn-icon"
                    style={{ width: '30px', height: '30px', borderRadius: '6px', border: '1px solid #e2e8f0', background: 'white', color: '#4a5568', cursor: 'pointer' }}
                    title="재시작"
                    disabled={isProcessing}
                  >
                    <i className="fas fa-redo-alt"></i>
                  </button>
                </>
              )}
            </div>
          </td>
        </tr>

        {/* Nested Devices Row (Existing Collector Logic) */}
        {isCollector && isExpanded && service.devices && service.devices.length > 0 && (
          <tr style={{ background: '#f8fafc' }}>
            <td colSpan={6} style={{ padding: '12px 24px 24px 60px' }}>
              <div style={{ borderLeft: '2px solid #cbd5e0', paddingLeft: '20px' }}>
                <div style={{ fontSize: '0.75rem', fontWeight: 700, color: '#718096', marginBottom: '12px', textTransform: 'uppercase' }}>
                  Assigned Devices ({service.devices.length})
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '12px' }}>
                  {service.devices.map(device => {
                    const isDeviceExpanded = expandedDevices.includes(device.id);
                    return (
                      <div key={device.id} style={{ background: 'white', borderRadius: '8px', padding: '12px', border: '1px solid #e2e8f0', boxShadow: '0 1px 2px rgba(0,0,0,0.05)' }}>
                        {/* Device Card Content omitted for brevity, keeping existing structure */}
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                          <div style={{ display: 'flex', gap: '10px', alignItems: 'center' }}>
                            <div style={{
                              width: '32px', height: '32px', borderRadius: '6px',
                              background: device.status === 'connected' ? '#f0fff4' : '#fff5f5',
                              display: 'flex', alignItems: 'center', justifyContent: 'center',
                              color: device.status === 'connected' ? '#38a169' : '#e53e3e'
                            }}>
                              <i className="fas fa-microchip"></i>
                            </div>
                            <div>
                              <div style={{ fontWeight: 600, fontSize: '0.9rem', color: '#2d3748' }}>{device.name}</div>
                              <div style={{ fontSize: '0.7rem', color: '#718096' }}>{device.protocol} | ID: {device.id}</div>
                            </div>
                          </div>
                          <div style={{ display: 'flex', gap: '4px' }}>
                            <button
                              onClick={() => handleDeviceControl(service, device, device.status === 'connected' ? 'stop' : 'start')}
                              style={{ padding: '4px 8px', borderRadius: '4px', border: '1px solid #e2e8f0', background: 'white', fontSize: '0.75rem', cursor: 'pointer' }}
                              title={device.status === 'connected' ? 'Stop' : 'Start'}
                            >
                              <i className={`fas ${device.status === 'connected' ? 'fa-pause' : 'fa-play'}`}></i>
                            </button>
                            <button
                              onClick={() => toggleDevice(device.id)}
                              style={{ padding: '4px 8px', borderRadius: '4px', border: '1px solid #e2e8f0', background: 'white', fontSize: '0.75rem', cursor: 'pointer' }}
                              title="Points"
                            >
                              <i className={`fas ${isDeviceExpanded ? 'fa-chevron-up' : 'fa-chevron-down'}`}></i>
                            </button>
                          </div>
                        </div>

                        {isDeviceExpanded && (
                          <div style={{ marginTop: '12px', borderTop: '1px solid #edf2f7', paddingTop: '10px' }}>
                            {/* Points Control Logic */}
                            <div style={{ fontSize: '0.7rem', fontWeight: 700, color: '#a0aec0', marginBottom: '8px' }}>POINT CONTROL</div>
                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
                              <button
                                onClick={() => handlePointControl(device, 1, 'digital', 1)}
                                style={{ fontSize: '0.7rem', padding: '4px 8px', borderRadius: '4px', border: '1px solid #c6f6d5', background: '#f0fff4', color: '#2f855a' }}
                              >DO-1 ON</button>
                              <button
                                onClick={() => handlePointControl(device, 1, 'digital', 0)}
                                style={{ fontSize: '0.7rem', padding: '4px 8px', borderRadius: '4px', border: '1px solid #fff5f5', background: '#fff5f5', color: '#c53030' }}
                              >DO-1 OFF</button>
                              <div style={{ width: '100%', height: '4px' }}></div>
                              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                <span style={{ fontSize: '0.7rem', color: '#4a5568' }}>AO-1 Value:</span>
                                <input
                                  type="number"
                                  defaultValue={0}
                                  style={{ width: '60px', padding: '2px 4px', fontSize: '0.7rem', borderRadius: '4px', border: '1px solid #cbd5e0' }}
                                  onBlur={(e) => handlePointControl(device, 1, 'analog', parseFloat(e.target.value))}
                                />
                              </div>
                            </div>
                          </div>
                        )}
                      </div>
                    );
                  })}
                </div>
              </div>
            </td>
          </tr>
        )}

        {/* NEW: Nested Gateways Row (Export Gateway Logic) */}
        {isExportGatewayParent && isExpanded && (service as any).gateways && (service as any).gateways.length > 0 && (
          <tr style={{ background: '#f8fafc' }}>
            <td colSpan={6} style={{ padding: '12px 24px 24px 60px' }}>
              <div style={{ borderLeft: '2px solid #805ad5', paddingLeft: '20px' }}>
                <div style={{ fontSize: '0.75rem', fontWeight: 700, color: '#6b46c1', marginBottom: '12px', textTransform: 'uppercase' }}>
                  Registered Gateways ({(service as any).gateways.length})
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(350px, 1fr))', gap: '16px' }}>
                  {(service as any).gateways.map((currGateway: ServiceInfo) => (
                    <div key={currGateway.name} style={{ background: 'white', borderRadius: '8px', padding: '16px', border: '1px solid #e9d8fd', boxShadow: '0 1px 2px rgba(0,0,0,0.05)' }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
                        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
                          <div style={{
                            width: '36px', height: '36px', borderRadius: '8px',
                            background: currGateway.status === 'running' ? '#f0fff4' : '#fff5f5',
                            color: currGateway.status === 'running' ? '#38a169' : '#e53e3e',
                            display: 'flex', alignItems: 'center', justifyContent: 'center'
                          }}>
                            <i className="fas fa-network-wired"></i>
                          </div>
                          <div>
                            <div style={{ fontWeight: 600, fontSize: '0.95rem', color: '#2d3748' }}>{currGateway.displayName}</div>
                            <div style={{ fontSize: '0.75rem', color: '#718096' }}>{currGateway.ip} | ID: {currGateway.gatewayId}</div>
                          </div>
                        </div>
                        <span style={{
                          fontSize: '0.7rem', fontWeight: 700,
                          padding: '2px 8px', borderRadius: '12px',
                          background: currGateway.status === 'running' ? '#def7ec' : '#fde2e2',
                          color: currGateway.status === 'running' ? '#03543f' : '#9b1c1c'
                        }}>
                          {currGateway.status === 'running' ? 'ALIVE' : 'DOWN'}
                        </span>
                      </div>

                      {/* Controls and Metrics */}
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', borderTop: '1px solid #edf2f7', paddingTop: '12px' }}>
                        <div style={{ fontSize: '0.75rem', color: '#718096' }}>
                          MEM: {formatResource(currGateway.memoryUsage)}
                        </div>
                        <div style={{ display: 'flex', gap: '6px' }}>
                          {currGateway.status === 'running' ? (
                            <button
                              onClick={() => handleServiceControl(currGateway, 'stop')}
                              style={{ padding: '6px 12px', borderRadius: '6px', border: '1px solid #fed7d7', background: 'white', color: '#c53030', fontSize: '0.75rem', cursor: 'pointer', fontWeight: 600 }}
                            >
                              <i className="fas fa-stop" style={{ marginRight: '6px' }}></i> 정지
                            </button>
                          ) : (
                            <button
                              onClick={() => handleServiceControl(currGateway, 'start')}
                              style={{ padding: '6px 12px', borderRadius: '6px', border: '1px solid #c6f6d5', background: 'white', color: '#2f855a', fontSize: '0.75rem', cursor: 'pointer', fontWeight: 600 }}
                            >
                              <i className="fas fa-play" style={{ marginRight: '6px' }}></i> 시작
                            </button>
                          )}
                          <button
                            onClick={() => handleServiceControl(currGateway, 'restart')}
                            style={{ padding: '6px 12px', borderRadius: '6px', border: '1px solid #e2e8f0', background: 'white', color: '#4a5568', fontSize: '0.75rem', cursor: 'pointer', fontWeight: 600 }}
                          >
                            <i className="fas fa-redo" style={{ marginRight: '6px' }}></i> 재시작
                          </button>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </td>
          </tr>
        )}

        {isCollector && isExpanded && (!service.devices || service.devices.length === 0) && (
          <tr style={{ background: '#f8fafc' }}>
            <td colSpan={6} style={{ padding: '16px 24px 24px 60px', color: '#a0aec0', fontSize: '0.85rem' }}>
              <div style={{ borderLeft: '2px solid #cbd5e0', paddingLeft: '20px' }}>
                할당된 디바이스가 없습니다.
              </div>
            </td>
          </tr>
        )}
      </React.Fragment>
    );
  };


  return (
    <div className="service-management-container" style={{ padding: '24px 24px 150px 24px', background: '#f7fafc', minHeight: '100vh' }}>
      {/* ... (Header section remains) ... */}
      <div className="page-header" style={{ marginBottom: '32px', display: 'flex', justifyContent: 'space-between', alignItems: 'center', background: 'white', padding: '20px 24px', borderRadius: '12px', boxShadow: '0 1px 3px rgba(0,0,0,0.1)' }}>
        <div>
          <h1 className="page-title" style={{ margin: 0, fontSize: '1.5rem', fontWeight: 700, color: '#1a202c' }}>시스템 상태 및 서비스 관리</h1>
          <p style={{ margin: '4px 0 0 0', color: '#718096', fontSize: '0.9rem' }}>산업용 데이터 허브의 모든 서비스 상태를 실시간으로 모니터링합니다.</p>
        </div>
        <div className="page-actions" style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ textAlign: 'right' }}>
            {error && <div className="text-error-600" style={{ color: '#e53e3e', fontSize: '0.85rem', fontWeight: 500 }}>{error}</div>}
            <span className="text-sm text-neutral-600" style={{ fontSize: '0.85rem', color: '#718096' }}>
              갱신: {formatTimeAgo(lastUpdate)}
            </span>
          </div>
          <button
            className="btn btn-primary"
            onClick={fetchStatus}
            disabled={isProcessing}
            style={{ padding: '10px 20px', borderRadius: '8px', background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)', border: 'none', color: 'white', fontWeight: 600, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '8px' }}
          >
            <i className={`fas fa-sync-alt ${isProcessing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
        </div>
      </div>

      <div className="status-grid" style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(300px, 1fr))', gap: '24px', marginBottom: '32px' }}>
        {/* Core Infrastructure Section */}
        <div className="status-card" style={{ background: 'white', padding: '24px', borderRadius: '12px', boxShadow: '0 1px 3px rgba(0,0,0,0.1)' }}>
          <h3 style={{ margin: '0 0 20px 0', fontSize: '1.1rem', fontWeight: 600, color: '#2d3748', borderBottom: '1px solid #edf2f7', paddingBottom: '12px', display: 'flex', alignItems: 'center', gap: '10px' }}>
            <i className="fas fa-microchip" style={{ color: '#4a5568' }}></i> 핵심 인프라
          </h3>
          <div className="service-mini-list" style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {coreServices.map(s => (
              <div key={s.name} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '8px 12px', background: '#f8fafc', borderRadius: '8px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <i className={s.icon || 'fas fa-cog'} style={{ color: s.status === 'running' ? '#48bb78' : '#718096' }}></i>
                  <span style={{ fontWeight: 500, fontSize: '0.95rem' }}>{s.displayName}</span>
                </div>
                <span style={{
                  padding: '2px 8px',
                  borderRadius: '12px',
                  fontSize: '0.75rem',
                  fontWeight: 600,
                  background: s.status === 'running' ? '#def7ec' : '#fde2e2',
                  color: s.status === 'running' ? '#03543f' : '#9b1c1c'
                }}>
                  {s.status === 'running' ? 'ALIVE' : 'DOWN'}
                </span>
              </div>
            ))}
          </div>
        </div>

        {/* Collector Partitions Section */}
        <div className="status-card" style={{ background: 'white', padding: '24px', borderRadius: '12px', boxShadow: '0 1px 3px rgba(0,0,0,0.1)' }}>
          <h3 style={{ margin: '0 0 20px 0', fontSize: '1.1rem', fontWeight: 600, color: '#2d3748', borderBottom: '1px solid #edf2f7', paddingBottom: '12px', display: 'flex', alignItems: 'center', gap: '10px' }}>
            <i className="fas fa-network-wired" style={{ color: '#667eea' }}></i> 수집기 파티션
          </h3>
          <div className="collector-summary" style={{ display: 'flex', gap: '16px', marginBottom: '16px' }}>
            <div style={{ flex: 1, textAlign: 'center', padding: '12px', background: '#ebf4ff', borderRadius: '8px' }}>
              <div style={{ fontSize: '1.5rem', fontWeight: 700, color: '#2b6cb0' }}>{collectorServices.length}</div>
              <div style={{ fontSize: '0.75rem', color: '#4a5568', textTransform: 'uppercase', fontWeight: 600 }}>총 파티션</div>
            </div>
            <div style={{ flex: 1, textAlign: 'center', padding: '12px', background: '#f0fff4', borderRadius: '8px' }}>
              <div style={{ fontSize: '1.5rem', fontWeight: 700, color: '#2f855a' }}>{collectorServices.filter(s => s.status === 'running').length}</div>
              <div style={{ fontSize: '0.75rem', color: '#4a5568', textTransform: 'uppercase', fontWeight: 600 }}>활성 수집기</div>
            </div>
          </div>
          <p style={{ fontSize: '0.85rem', color: '#718096', margin: 0 }}>각 엣지 서버별 데이터 수집 엔진이 독립적으로 동작하고 있습니다.</p>
        </div>
      </div>

      <div className="main-service-panel" style={{ background: 'white', borderRadius: '12px', boxShadow: '0 1px 3px rgba(0,0,0,0.1)', overflow: 'hidden' }}>
        <div style={{ padding: '20px 24px', borderBottom: '1px solid #edf2f7', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <h3 style={{ margin: 0, fontSize: '1.1rem', fontWeight: 600 }}>상세 서비스 목록</h3>
          <div style={{ display: 'flex', gap: '8px' }}>
            <button className="btn btn-sm" onClick={() => handleBulkAction('restart')} style={{ padding: '6px 12px', fontSize: '0.85rem', borderRadius: '6px', border: '1px solid #e2e8f0', background: 'white', cursor: 'pointer' }}>
              <i className="fas fa-redo"></i> 일괄 재시작
            </button>
          </div>
        </div>

        <div className="grouped-service-list" style={{ marginBottom: '150px' }}>
          {/* Core Infrastructure Group */}
          <div className="service-group">
            <div style={{ background: '#f8fafc', padding: '10px 24px', borderBottom: '1px solid #edf2f7', fontSize: '0.8rem', fontWeight: 700, color: '#4a5568', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
              핵심 인프라 (Core Services)
            </div>
            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <tbody>
                {coreServices.map(service => renderServiceRow(service))}
              </tbody>
            </table>
          </div>

          {/* Export Services Group */}
          {exportGatewayServices.length > 0 && (
            <div className="service-group" style={{ marginTop: '20px' }}>
              <div style={{ background: '#f8fafc', padding: '10px 24px', borderBottom: '1px solid #edf2f7', fontSize: '0.8rem', fontWeight: 700, color: '#805ad5', textTransform: 'uppercase', letterSpacing: '0.05em', borderTop: '2px solid #faf5ff' }}>
                데이터 외부 연동 (Export Gateways)
              </div>
              <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                <tbody>
                  {exportGatewayServices.map(service => renderServiceRow(service))}
                </tbody>
              </table>
            </div>
          )}

          {/* Collector Partitions Group (Hierarchical) */}
          <div className="service-group" style={{ marginTop: '20px' }}>
            <div style={{ background: '#f8fafc', padding: '10px 24px', borderBottom: '1px solid #edf2f7', fontSize: '0.8rem', fontWeight: 700, color: '#667eea', textTransform: 'uppercase', letterSpacing: '0.05em', borderTop: '2px solid #ebf4ff' }}>
              수집기 파티션 (Hierarchical Overview)
            </div>

            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <tbody>
                {/* 1. Sites and their Collectors */}
                {hierarchy.map(site => (
                  <React.Fragment key={`site-${site.id}`}>
                    <tr style={{ background: '#f1f5f9', borderBottom: '1px solid #e2e8f0' }}>
                      <td colSpan={6} style={{ padding: '8px 24px' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                          <button
                            onClick={() => toggleSite(site.id)}
                            style={{ background: 'none', border: 'none', cursor: 'pointer', color: '#64748b' }}
                          >
                            <i className={`fas ${expandedSites.includes(site.id) ? 'fa-minus-square' : 'fa-plus-square'}`}></i>
                          </button>
                          <i className="fas fa-industry" style={{ color: '#4a5568' }}></i>
                          <span style={{ fontWeight: 700, fontSize: '0.85rem', color: '#334155' }}>
                            SITE: {site.name} ({site.code})
                          </span>
                          <span style={{ fontSize: '0.7rem', color: '#94a3b8', background: 'white', padding: '1px 6px', borderRadius: '4px', border: '1px solid #e2e8f0' }}>
                            {site.collectors?.length || 0} Collectors
                          </span>
                        </div>
                      </td>
                    </tr>
                    {expandedSites.includes(site.id) && (
                      site.collectors && site.collectors.length > 0 ? (
                        site.collectors.map(service => renderServiceRow(service))
                      ) : (
                        <tr style={{ background: 'white' }}>
                          <td colSpan={6} style={{ padding: '12px 60px', color: '#94a3b8', fontSize: '0.8rem', fontStyle: 'italic' }}>
                            이 사이트에 할당된 수집기가 없습니다.
                          </td>
                        </tr>
                      )
                    )}
                  </React.Fragment>
                ))}

                {/* 2. Unassigned Registered Collectors */}
                {unassignedCollectors.length > 0 && (
                  <React.Fragment>
                    <tr style={{ background: '#f8fafc', borderBottom: '1px solid #e2e8f0' }}>
                      <td colSpan={6} style={{ padding: '8px 24px' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                          <i className="fas fa-question-circle" style={{ color: '#718096' }}></i>
                          <span style={{ fontWeight: 700, fontSize: '0.85rem', color: '#4a5568' }}>
                            UNASSIGNED COLLECTORS (Registered but no Site)
                          </span>
                        </div>
                      </td>
                    </tr>
                    {unassignedCollectors.map(service => renderServiceRow(service))}
                  </React.Fragment>
                )}

                {/* 3. Shadow Collectors (Running but not in DB) */}
                {services.filter(s => s.name.startsWith('collector') && !s.exists).map(service => (
                  <React.Fragment key={service.name}>
                    <tr style={{ background: '#fff5f5', borderBottom: '1px solid #fed7d7' }}>
                      <td colSpan={6} style={{ padding: '8px 24px' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
                          <i className="fas fa-exclamation-triangle" style={{ color: '#e53e3e' }}></i>
                          <span style={{ fontWeight: 700, fontSize: '0.85rem', color: '#c53030' }}>
                            UNREGISTERED PROCESS (Detected but not in DB)
                          </span>
                        </div>
                      </td>
                    </tr>
                    {renderServiceRow(service)}
                  </React.Fragment>
                ))}

                {hierarchy.length === 0 && unassignedCollectors.length === 0 && collectorServices.length === 0 && (
                  <tr>
                    <td colSpan={6} style={{ padding: '48px', textAlign: 'center', color: '#a0aec0' }}>
                      <i className="fas fa-search fa-2x" style={{ marginBottom: '12px', display: 'block' }}></i>
                      데이터가 없습니다.
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </div>
      </div><div style={{ height: "150px" }}></div>
    </div>
  );
};

export default SystemStatus;

