import React, { useState, useEffect, useCallback } from 'react';

interface Device {
  id: number;
  name: string;
  device_name?: string;
  protocol_type: string;
  device_type?: string;
  endpoint: string;
  is_enabled: boolean;
  connection_status: 'connected' | 'disconnected' | 'timeout' | 'error';
  status: 'running' | 'paused' | 'stopped' | 'error' | 'offline';
  last_seen?: string;
  site_id?: number;
  site_name?: string;
  data_points_count?: number;
  description?: string;
  manufacturer?: string;
  model?: string;
  serial_number?: string;
  firmware_version?: string;
  polling_interval?: number;
  error_count?: number;
  response_time?: number;
  uptime?: string;
  memory_usage?: string;
  cpu_usage?: string;
  tags?: string[];
  created_at?: string;
  updated_at?: string;
}

interface DeviceStats {
  total: number;
  running: number;
  paused: number;
  stopped: number;
  error: number;
  connected: number;
  disconnected: number;
}

const DeviceList: React.FC = () => {
  // 상태 관리
  const [devices, setDevices] = useState<Device[]>([]);
  const [filteredDevices, setFilteredDevices] = useState<Device[]>([]);
  const [selectedDevices, setSelectedDevices] = useState<number[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [isProcessing, setIsProcessing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // 필터 상태
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [protocolFilter, setProtocolFilter] = useState<string>('all');
  const [typeFilter, setTypeFilter] = useState<string>('all');
  const [connectionFilter, setConnectionFilter] = useState<string>('all');

  // 실시간 업데이트
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [autoRefresh, setAutoRefresh] = useState(true);

  // 🚀 실제 API 호출 함수들
  const fetchDevices = async () => {
    try {
      setIsLoading(true);
      setError(null);
      
      console.log('🔍 디바이스 목록 조회 시작...');
      
      const response = await fetch('/api/devices');
      const result = await response.json();
      
      console.log('📋 디바이스 API 응답:', result);
      
      if (result.success && Array.isArray(result.data)) {
        // 백엔드 데이터를 프론트엔드 형식으로 변환
        const transformedDevices = result.data.map((device: any) => ({
          ...device,
          // 백엔드 필드 → 프론트엔드 필드 매핑
          name: device.device_name || device.name,
          status: mapConnectionToStatus(device.connection_status, device.is_enabled),
          connection_status: device.connection_status || 'disconnected',
          data_points_count: device.data_points_count || 0,
          // 시뮬레이션 필드들 (실제 데이터가 없는 경우)
          manufacturer: device.manufacturer || 'Unknown',
          model: device.model || 'N/A',
          response_time: device.response_time || Math.floor(Math.random() * 50) + 10,
          error_count: device.error_count || 0,
          uptime: device.uptime || generateRandomUptime(),
          memory_usage: device.memory_usage || `${(Math.random() * 5 + 1).toFixed(1)}MB`,
          cpu_usage: device.cpu_usage || `${Math.floor(Math.random() * 30)}%`,
          tags: device.tags || []
        }));
        
        setDevices(transformedDevices);
        console.log('✅ 디바이스 데이터 로드 완료:', transformedDevices.length, '개');
      } else {
        // API 실패시 시뮬레이션 데이터
        console.warn('⚠️ 디바이스 API 실패 - 시뮬레이션 데이터 사용');
        setDevices(getSimulationDevices());
      }
    } catch (error) {
      console.error('❌ 디바이스 API 호출 실패:', error);
      setError(`디바이스 목록을 불러오는데 실패했습니다: ${error}`);
      // 에러시에도 시뮬레이션 데이터 표시
      setDevices(getSimulationDevices());
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  };

  // 🎛️ 디바이스 제어 함수들
  const controlDevice = async (deviceId: number, action: 'start' | 'stop' | 'restart' | 'pause') => {
    try {
      console.log(`🎛️ 디바이스 ${deviceId} ${action} 요청...`);
      
      let endpoint = '';
      let method = 'POST';
      
      switch (action) {
        case 'start':
          endpoint = `/api/devices/${deviceId}/enable`;
          break;
        case 'stop':
          endpoint = `/api/devices/${deviceId}/disable`;
          break;
        case 'restart':
          endpoint = `/api/devices/${deviceId}/restart`;
          break;
        case 'pause':
          // 일시정지는 비활성화로 처리
          endpoint = `/api/devices/${deviceId}/disable`;
          break;
      }
      
      const response = await fetch(endpoint, { method });
      const result = await response.json();
      
      if (result.success) {
        console.log(`✅ 디바이스 ${deviceId} ${action} 성공`);
        // 상태 즉시 반영을 위해 디바이스 목록 새로고침
        setTimeout(fetchDevices, 1000);
        return { success: true, message: result.message };
      } else {
        throw new Error(result.error || `${action} 실패`);
      }
    } catch (error) {
      console.error(`❌ 디바이스 ${deviceId} ${action} 실패:`, error);
      return { success: false, error: error.message };
    }
  };

  // 일괄 제어
  const handleBulkAction = async (action: 'start' | 'stop' | 'pause' | 'restart') => {
    if (filteredDevices.length === 0) return;
    
    setIsProcessing(true);
    
    try {
      const results = await Promise.allSettled(
        filteredDevices.map(device => controlDevice(device.id, action))
      );
      
      const successCount = results.filter(r => r.status === 'fulfilled' && r.value.success).length;
      const failCount = results.length - successCount;
      
      alert(`전체 ${action}: ${successCount}개 성공, ${failCount}개 실패`);
    } catch (error) {
      alert(`일괄 ${action} 중 오류가 발생했습니다: ${error}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 선택된 디바이스 제어
  const handleSelectedAction = async (action: 'start' | 'stop' | 'pause' | 'restart') => {
    if (selectedDevices.length === 0) return;
    
    setIsProcessing(true);
    
    try {
      const results = await Promise.allSettled(
        selectedDevices.map(deviceId => controlDevice(deviceId, action))
      );
      
      const successCount = results.filter(r => r.status === 'fulfilled' && r.value.success).length;
      const failCount = results.length - successCount;
      
      alert(`선택된 디바이스 ${action}: ${successCount}개 성공, ${failCount}개 실패`);
      setSelectedDevices([]); // 선택 해제
    } catch (error) {
      alert(`선택된 디바이스 ${action} 중 오류가 발생했습니다: ${error}`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 개별 디바이스 제어
  const handleDeviceAction = async (device: Device, action: 'start' | 'stop' | 'restart' | 'pause') => {
    setIsProcessing(true);
    
    const result = await controlDevice(device.id, action);
    
    if (result.success) {
      alert(`${device.name} ${action} 성공!`);
    } else {
      alert(`${device.name} ${action} 실패: ${result.error}`);
    }
    
    setIsProcessing(false);
  };

  // ➕ 디바이스 추가 (미래 구현)
  const handleAddDevice = () => {
    alert('디바이스 추가 기능은 준비 중입니다.');
  };

  // 🔄 유틸리티 함수들
  const mapConnectionToStatus = (connectionStatus: string, isEnabled: boolean): string => {
    if (!isEnabled) return 'stopped';
    
    switch (connectionStatus) {
      case 'connected': return 'running';
      case 'disconnected': return 'offline';
      case 'timeout': return 'paused';
      case 'error': return 'error';
      default: return 'stopped';
    }
  };

  const generateRandomUptime = () => {
    const days = Math.floor(Math.random() * 30);
    const hours = Math.floor(Math.random() * 24);
    const minutes = Math.floor(Math.random() * 60);
    return `${days}d ${hours}h ${minutes}m`;
  };

  const getSimulationDevices = (): Device[] => [
    {
      id: 1,
      name: 'Main PLC #1',
      protocol_type: 'Modbus',
      device_type: 'PLC',
      endpoint: '192.168.1.100:502',
      is_enabled: true,
      connection_status: 'connected',
      status: 'running',
      site_name: 'Factory A',
      data_points_count: 45,
      manufacturer: 'Siemens',
      model: 'S7-1200',
      response_time: 12,
      error_count: 0,
      uptime: '15d 8h 23m',
      memory_usage: '2.1MB',
      cpu_usage: '15%',
      description: '메인 생산라인 PLC'
    },
    {
      id: 2,
      name: 'Temperature Sensors',
      protocol_type: 'MQTT',
      device_type: 'Sensor',
      endpoint: 'mqtt://192.168.1.101:1883',
      is_enabled: true,
      connection_status: 'connected',
      status: 'running',
      site_name: 'Factory A',
      data_points_count: 12,
      manufacturer: 'Honeywell',
      model: 'T6540',
      response_time: 8,
      error_count: 0,
      uptime: '22d 4h 15m',
      memory_usage: '512KB',
      cpu_usage: '5%',
      description: '온도 센서 네트워크'
    },
    {
      id: 3,
      name: 'HVAC Controller',
      protocol_type: 'BACnet',
      device_type: 'Controller',
      endpoint: '192.168.1.102',
      is_enabled: false,
      connection_status: 'disconnected',
      status: 'stopped',
      site_name: 'Factory B',
      data_points_count: 0,
      manufacturer: 'Johnson Controls',
      model: 'FX-PCV',
      response_time: 0,
      error_count: 3,
      uptime: '0d 0h 0m',
      memory_usage: '0MB',
      cpu_usage: '0%',
      description: 'HVAC 제어 시스템'
    },
    {
      id: 4,
      name: 'Energy Meter #1',
      protocol_type: 'Modbus',
      device_type: 'Meter',
      endpoint: '192.168.1.103:502',
      is_enabled: true,
      connection_status: 'connected',
      status: 'running',
      site_name: 'Factory A',
      data_points_count: 24,
      manufacturer: 'Schneider Electric',
      model: 'PM8000',
      response_time: 18,
      error_count: 1,
      uptime: '8d 12h 45m',
      memory_usage: '1.5MB',
      cpu_usage: '8%',
      description: '전력량계'
    },
    {
      id: 5,
      name: 'Vibration Monitor',
      protocol_type: 'Ethernet/IP',
      device_type: 'Monitor',
      endpoint: '192.168.1.104',
      is_enabled: true,
      connection_status: 'timeout',
      status: 'paused',
      site_name: 'Factory B',
      data_points_count: 8,
      manufacturer: 'Rockwell Automation',
      model: 'IVS-300',
      response_time: 250,
      error_count: 5,
      uptime: '3d 6h 12m',
      memory_usage: '3.2MB',
      cpu_usage: '25%',
      description: '진동 모니터링 시스템'
    }
  ];

  // 필터링 로직
  useEffect(() => {
    let filtered = devices;

    // 검색어 필터
    if (searchTerm) {
      const term = searchTerm.toLowerCase();
      filtered = filtered.filter(device =>
        device.name.toLowerCase().includes(term) ||
        device.endpoint.toLowerCase().includes(term) ||
        device.manufacturer?.toLowerCase().includes(term) ||
        device.model?.toLowerCase().includes(term) ||
        device.site_name?.toLowerCase().includes(term) ||
        device.description?.toLowerCase().includes(term)
      );
    }

    // 상태 필터
    if (statusFilter !== 'all') {
      filtered = filtered.filter(device => device.status === statusFilter);
    }

    // 프로토콜 필터
    if (protocolFilter !== 'all') {
      filtered = filtered.filter(device => device.protocol_type === protocolFilter);
    }

    // 디바이스 타입 필터
    if (typeFilter !== 'all') {
      filtered = filtered.filter(device => device.device_type === typeFilter);
    }

    // 연결 상태 필터
    if (connectionFilter !== 'all') {
      filtered = filtered.filter(device => device.connection_status === connectionFilter);
    }

    setFilteredDevices(filtered);
  }, [devices, searchTerm, statusFilter, protocolFilter, typeFilter, connectionFilter]);

  // 디바이스 선택 핸들러
  const handleDeviceSelect = (deviceId: number) => {
    setSelectedDevices(prev =>
      prev.includes(deviceId)
        ? prev.filter(id => id !== deviceId)
        : [...prev, deviceId]
    );
  };

  // 📱 실시간 업데이트 (10초마다)
  useEffect(() => {
    fetchDevices(); // 초기 로드
    
    let interval: NodeJS.Timeout;
    if (autoRefresh) {
      interval = setInterval(fetchDevices, 10000); // 10초마다 새로고침
    }
    
    return () => {
      if (interval) clearInterval(interval);
    };
  }, [autoRefresh]);

  // 통계 계산
  const stats: DeviceStats = {
    total: devices.length,
    running: devices.filter(d => d.status === 'running').length,
    paused: devices.filter(d => d.status === 'paused').length,
    stopped: devices.filter(d => d.status === 'stopped').length,
    error: devices.filter(d => d.status === 'error').length,
    connected: devices.filter(d => d.connection_status === 'connected').length,
    disconnected: devices.filter(d => d.connection_status === 'disconnected').length,
  };

  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    return diff < 1 ? '방금 전' : diff < 60 ? `${diff}분 전` : `${Math.floor(diff/60)}시간 전`;
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'running': return 'text-success-600';
      case 'paused': return 'text-warning-600'; 
      case 'stopped': return 'text-neutral-500';
      case 'error': return 'text-error-600';
      case 'offline': return 'text-neutral-400';
      default: return 'text-neutral-500';
    }
  };

  const getConnectionColor = (status: string) => {
    switch (status) {
      case 'connected': return 'text-success-600';
      case 'disconnected': return 'text-error-600';
      case 'timeout': return 'text-warning-600';
      case 'error': return 'text-error-600';
      default: return 'text-neutral-500';
    }
  };

  const getUniqueValues = (field: keyof Device) => {
    return [...new Set(devices.map(device => device[field]).filter(Boolean))];
  };

  if (isLoading) {
    return (
      <div className="device-list-container">
        <div className="loading-container">
          <div className="loading-spinner"></div>
          <p>디바이스 목록을 불러오는 중...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="device-list-container">
      {/* 헤더 */}
      <div className="device-list-header">
        <div className="header-left">
          <h1 className="page-title">
            <i className="fas fa-network-wired"></i>
            디바이스 관리
          </h1>
          <div className="update-info">
            <span className="last-update">
              마지막 업데이트: {formatTimeAgo(lastUpdate.toISOString())}
            </span>
            <button
              className="refresh-btn"
              onClick={fetchDevices}
              disabled={isLoading}
            >
              <i className={`fas fa-sync-alt ${isLoading ? 'fa-spin' : ''}`}></i>
              새로고침
            </button>
            <label className="auto-refresh">
              <input
                type="checkbox"
                checked={autoRefresh}
                onChange={(e) => setAutoRefresh(e.target.checked)}
              />
              자동 새로고침
            </label>
          </div>
        </div>
        <div className="header-right">
          <button className="btn btn-primary" onClick={handleAddDevice}>
            <i className="fas fa-plus"></i>
            디바이스 추가
          </button>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="error-banner">
          <i className="fas fa-exclamation-triangle"></i>
          {error}
          <button onClick={() => setError(null)}>×</button>
        </div>
      )}

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-row">
          {/* 검색 */}
          <div className="search-container">
            <input
              type="text"
              placeholder="디바이스명, IP주소, 제조사, 모델 검색..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              className="search-input"
            />
            <i className="fas fa-search search-icon"></i>
          </div>
          
          {/* 필터들 */}
          <div className="filter-group">
            <label>상태</label>
            <select value={statusFilter} onChange={(e) => setStatusFilter(e.target.value)}>
              <option value="all">전체</option>
              <option value="running">실행중</option>
              <option value="paused">일시정지</option>
              <option value="stopped">정지</option>
              <option value="error">오류</option>
              <option value="offline">오프라인</option>
            </select>
          </div>

          <div className="filter-group">
            <label>프로토콜</label>
            <select value={protocolFilter} onChange={(e) => setProtocolFilter(e.target.value)}>
              <option value="all">전체</option>
              {getUniqueValues('protocol_type').map(protocol => (
                <option key={protocol} value={protocol}>{protocol}</option>
              ))}
            </select>
          </div>

          <div className="filter-group">
            <label>타입</label>
            <select value={typeFilter} onChange={(e) => setTypeFilter(e.target.value)}>
              <option value="all">전체</option>
              {getUniqueValues('device_type').map(type => (
                <option key={type} value={type}>{type}</option>
              ))}
            </select>
          </div>

          <div className="filter-group">
            <label>연결</label>
            <select value={connectionFilter} onChange={(e) => setConnectionFilter(e.target.value)}>
              <option value="all">전체</option>
              <option value="connected">연결됨</option>
              <option value="disconnected">연결끊김</option>
              <option value="timeout">타임아웃</option>
              <option value="error">오류</option>
            </select>
          </div>
        </div>
      </div>

      {/* 통계 패널 */}
      <div className="stats-panel">
        <div className="stat-card">
          <div className="stat-value">{stats.total}</div>
          <div className="stat-label">전체 디바이스</div>
        </div>
        <div className="stat-card status-running">
          <div className="stat-value">{stats.running}</div>
          <div className="stat-label">실행 중</div>
        </div>
        <div className="stat-card status-paused">
          <div className="stat-value">{stats.paused}</div>
          <div className="stat-label">일시정지</div>
        </div>
        <div className="stat-card status-stopped">
          <div className="stat-value">{stats.stopped}</div>
          <div className="stat-label">정지</div>
        </div>
        <div className="stat-card status-error">
          <div className="stat-value">{stats.error}</div>
          <div className="stat-label">오류</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{filteredDevices.length}</div>
          <div className="stat-label">필터된 결과</div>
        </div>
      </div>

      {/* 일괄 제어 패널 */}
      <div className="control-panel">
        <div className="control-section">
          <h3 className="section-title">전체 디바이스 제어</h3>
          <div className="control-buttons">
            <button 
              className="btn btn-success"
              onClick={() => handleBulkAction('start')}
              disabled={isProcessing || filteredDevices.length === 0}
            >
              <i className="fas fa-play"></i>
              전체 시작
            </button>
            <button 
              className="btn btn-warning"
              onClick={() => handleBulkAction('pause')}
              disabled={isProcessing || filteredDevices.length === 0}
            >
              <i className="fas fa-pause"></i>
              전체 일시정지
            </button>
            <button 
              className="btn btn-error"
              onClick={() => handleBulkAction('stop')}
              disabled={isProcessing || filteredDevices.length === 0}
            >
              <i className="fas fa-stop"></i>
              전체 정지
            </button>
          </div>
        </div>

        <div className="control-section">
          <h3 className="section-title">선택된 디바이스 제어</h3>
          <div className="selected-info">
            <span className="text-sm text-neutral-600">
              {selectedDevices.length}개 디바이스 선택됨
            </span>
          </div>
          <div className="control-buttons">
            <button 
              className="btn btn-success btn-sm"
              onClick={() => handleSelectedAction('start')}
              disabled={isProcessing || selectedDevices.length === 0}
            >
              <i className="fas fa-play"></i>
              선택 시작
            </button>
            <button 
              className="btn btn-warning btn-sm"
              onClick={() => handleSelectedAction('pause')}
              disabled={isProcessing || selectedDevices.length === 0}
            >
              <i className="fas fa-pause"></i>
              선택 일시정지
            </button>
            <button 
              className="btn btn-error btn-sm"
              onClick={() => handleSelectedAction('stop')}
              disabled={isProcessing || selectedDevices.length === 0}
            >
              <i className="fas fa-stop"></i>
              선택 정지
            </button>
          </div>
        </div>
      </div>

      {/* 디바이스 테이블 */}
      <div className="device-table-container">
        <div className="device-table">
          <div className="device-table-header">
            <div className="device-table-cell">
              <input 
                type="checkbox" 
                onChange={(e) => {
                  if (e.target.checked) {
                    setSelectedDevices(filteredDevices.map(d => d.id));
                  } else {
                    setSelectedDevices([]);
                  }
                }}
                checked={selectedDevices.length === filteredDevices.length && filteredDevices.length > 0}
              />
            </div>
            <div className="device-table-cell">디바이스</div>
            <div className="device-table-cell">상태</div>
            <div className="device-table-cell">연결</div>
            <div className="device-table-cell">위치</div>
            <div className="device-table-cell">네트워크</div>
            <div className="device-table-cell">데이터</div>
            <div className="device-table-cell">성능</div>
            <div className="device-table-cell">제어</div>
          </div>

          {filteredDevices.map((device) => (
            <div key={device.id} className="device-table-row">
              <div className="device-table-cell">
                <input 
                  type="checkbox"
                  checked={selectedDevices.includes(device.id)}
                  onChange={() => handleDeviceSelect(device.id)}
                />
              </div>
              
              <div className="device-table-cell">
                <div className="device-info">
                  <div className={`device-icon ${getStatusColor(device.status)}`}>
                    <i className={
                      device.device_type === 'PLC' ? 'fas fa-microchip' :
                      device.device_type === 'Sensor' ? 'fas fa-thermometer-half' :
                      device.device_type === 'Controller' ? 'fas fa-cogs' :
                      device.device_type === 'Meter' ? 'fas fa-tachometer-alt' :
                      device.device_type === 'Monitor' ? 'fas fa-desktop' :
                      'fas fa-network-wired'
                    }></i>
                  </div>
                  <div className="device-details">
                    <div className="device-name">{device.name}</div>
                    <div className="device-type">{device.protocol_type}</div>
                    <div className="device-manufacturer">{device.manufacturer} {device.model}</div>
                  </div>
                </div>
              </div>

              <div className="device-table-cell">
                <span className={`status-badge ${device.status}`}>
                  {device.status === 'running' ? '실행중' :
                   device.status === 'paused' ? '일시정지' :
                   device.status === 'stopped' ? '정지' :
                   device.status === 'error' ? '오류' : '오프라인'}
                </span>
              </div>

              <div className="device-table-cell">
                <span className={`connection-badge ${device.connection_status}`}>
                  <i className={`fas fa-circle ${getConnectionColor(device.connection_status)}`}></i>
                  {device.connection_status === 'connected' ? '연결됨' :
                   device.connection_status === 'disconnected' ? '연결끊김' :
                   device.connection_status === 'timeout' ? '타임아웃' : '오류'}
                </span>
              </div>

              <div className="device-table-cell">
                <div className="location-info">
                  <div className="site-name">{device.site_name || 'N/A'}</div>
                  <div className="description">{device.description || ''}</div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="network-info">
                  <div className="endpoint">{device.endpoint}</div>
                  <div className="response-time">{device.response_time}ms</div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="data-info">
                  <div className="data-points">{device.data_points_count} 포인트</div>
                  <div className="last-seen">
                    {device.last_seen ? formatTimeAgo(device.last_seen) : 'N/A'}
                  </div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="performance-info">
                  <div className="uptime">가동시간: {device.uptime}</div>
                  <div className="resource-usage">
                    <span>CPU: {device.cpu_usage}</span>
                    <span>메모리: {device.memory_usage}</span>
                  </div>
                  <div className="error-count">
                    오류: {device.error_count}회
                  </div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="device-actions">
                  {device.status === 'running' ? (
                    <>
                      <button 
                        className="btn btn-sm btn-warning"
                        onClick={() => handleDeviceAction(device, 'pause')}
                        disabled={isProcessing}
                        title="일시정지"
                      >
                        <i className="fas fa-pause"></i>
                      </button>
                      <button 
                        className="btn btn-sm btn-error"
                        onClick={() => handleDeviceAction(device, 'stop')}
                        disabled={isProcessing}
                        title="정지"
                      >
                        <i className="fas fa-stop"></i>
                      </button>
                      <button 
                        className="btn btn-sm btn-info"
                        onClick={() => handleDeviceAction(device, 'restart')}
                        disabled={isProcessing}
                        title="재시작"
                      >
                        <i className="fas fa-redo"></i>
                      </button>
                    </>
                  ) : (
                    <button 
                      className="btn btn-sm btn-success"
                      onClick={() => handleDeviceAction(device, 'start')}
                      disabled={isProcessing}
                      title="시작"
                    >
                      <i className="fas fa-play"></i>
                    </button>
                  )}
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* 빈 상태 */}
      {filteredDevices.length === 0 && !isLoading && (
        <div className="empty-state">
          <i className="fas fa-network-wired"></i>
          <h3>디바이스를 찾을 수 없습니다</h3>
          <p>필터 조건을 변경하거나 새로운 디바이스를 추가해보세요.</p>
          <button className="btn btn-primary" onClick={handleAddDevice}>
            <i className="fas fa-plus"></i>
            디바이스 추가
          </button>
        </div>
      )}

      {/* CSS 스타일 */}
      <style dangerouslySetInnerHTML={{__html: `
        .device-list-container {
          padding: 1.5rem;
          max-width: 100%;
          background: #f8fafc;
          min-height: 100vh;
        }

        .device-list-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1.5rem;
          padding-bottom: 1rem;
          border-bottom: 1px solid #e2e8f0;
        }

        .header-left {
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .page-title {
          font-size: 1.875rem;
          font-weight: 700;
          color: #1e293b;
          margin: 0;
          display: flex;
          align-items: center;
          gap: 0.5rem;
        }

        .update-info {
          display: flex;
          align-items: center;
          gap: 1rem;
          font-size: 0.875rem;
          color: #64748b;
        }

        .refresh-btn {
          display: flex;
          align-items: center;
          gap: 0.25rem;
          padding: 0.25rem 0.5rem;
          background: #0ea5e9;
          color: white;
          border: none;
          border-radius: 0.375rem;
          cursor: pointer;
          font-size: 0.75rem;
        }

        .refresh-btn:hover {
          background: #0284c7;
        }

        .auto-refresh {
          display: flex;
          align-items: center;
          gap: 0.25rem;
          cursor: pointer;
        }

        .error-banner {
          background: #fef2f2;
          border: 1px solid #fecaca;
          color: #991b1b;
          padding: 1rem;
          border-radius: 0.5rem;
          margin-bottom: 1rem;
          display: flex;
          align-items: center;
          gap: 0.5rem;
        }

        .error-banner button {
          margin-left: auto;
          background: none;
          border: none;
          font-size: 1.2rem;
          cursor: pointer;
          color: #991b1b;
        }

        .filter-panel {
          background: white;
          padding: 1rem;
          border-radius: 0.5rem;
          box-shadow: 0 1px 3px rgba(0,0,0,0.1);
          margin-bottom: 1.5rem;
        }

        .filter-row {
          display: flex;
          gap: 1rem;
          align-items: end;
          flex-wrap: wrap;
        }

        .search-container {
          position: relative;
          flex: 1;
          min-width: 300px;
        }

        .search-input {
          width: 100%;
          padding: 0.5rem 2.5rem 0.5rem 1rem;
          border: 1px solid #d1d5db;
          border-radius: 0.5rem;
          font-size: 0.875rem;
        }

        .search-icon {
          position: absolute;
          right: 1rem;
          top: 50%;
          transform: translateY(-50%);
          color: #6b7280;
        }

        .filter-group {
          display: flex;
          flex-direction: column;
          gap: 0.25rem;
          min-width: 120px;
        }

        .filter-group label {
          font-size: 0.75rem;
          color: #374151;
          font-weight: 500;
        }

        .filter-group select {
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          background: white;
        }

        .stats-panel {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
          gap: 1rem;
          margin-bottom: 1.5rem;
        }

        .stat-card {
          background: white;
          padding: 1rem;
          border-radius: 0.5rem;
          box-shadow: 0 1px 3px rgba(0,0,0,0.1);
          text-align: center;
          border-left: 4px solid #e2e8f0;
        }

        .stat-card.status-running {
          border-left-color: #10b981;
        }

        .stat-card.status-paused {
          border-left-color: #f59e0b;
        }

        .stat-card.status-stopped {
          border-left-color: #6b7280;
        }

        .stat-card.status-error {
          border-left-color: #ef4444;
        }

        .stat-value {
          font-size: 2rem;
          font-weight: 700;
          color: #1e293b;
        }

        .stat-label {
          font-size: 0.875rem;
          color: #64748b;
          margin-top: 0.25rem;
        }

        .control-panel {
          background: white;
          padding: 1rem;
          border-radius: 0.5rem;
          box-shadow: 0 1px 3px rgba(0,0,0,0.1);
          margin-bottom: 1.5rem;
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 2rem;
        }

        .control-section {
          display: flex;
          flex-direction: column;
          gap: 1rem;
        }

        .section-title {
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
          margin: 0;
        }

        .control-buttons {
          display: flex;
          gap: 0.5rem;
          flex-wrap: wrap;
        }

        .selected-info {
          font-size: 0.875rem;
          color: #6b7280;
        }

        .device-table-container {
          background: white;
          border-radius: 0.5rem;
          box-shadow: 0 1px 3px rgba(0,0,0,0.1);
          overflow: hidden;
        }

        .device-table {
          width: 100%;
        }

        .device-table-header {
          display: grid;
          grid-template-columns: 50px 300px 100px 120px 200px 180px 120px 180px 150px;
          background: #f8fafc;
          border-bottom: 2px solid #e2e8f0;
        }

        .device-table-row {
          display: grid;
          grid-template-columns: 50px 300px 100px 120px 200px 180px 120px 180px 150px;
          border-bottom: 1px solid #f1f5f9;
          transition: background-color 0.2s;
        }

        .device-table-row:hover {
          background: #f8fafc;
        }

        .device-table-cell {
          padding: 1rem 0.75rem;
          display: flex;
          align-items: center;
          font-size: 0.875rem;
          border-right: 1px solid #f1f5f9;
        }

        .device-table-header .device-table-cell {
          font-weight: 600;
          color: #374151;
          background: #f8fafc;
        }

        .device-info {
          display: flex;
          align-items: center;
          gap: 0.75rem;
        }

        .device-icon {
          font-size: 1.25rem;
          width: 40px;
          height: 40px;
          display: flex;
          align-items: center;
          justify-content: center;
          background: #f8fafc;
          border-radius: 0.5rem;
        }

        .device-details {
          display: flex;
          flex-direction: column;
          gap: 0.125rem;
        }

        .device-name {
          font-weight: 600;
          color: #1e293b;
        }

        .device-type {
          font-size: 0.75rem;
          color: #64748b;
        }

        .device-manufacturer {
          font-size: 0.75rem;
          color: #9ca3af;
        }

        .status-badge {
          padding: 0.25rem 0.5rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .status-badge.running {
          background: #dcfce7;
          color: #166534;
        }

        .status-badge.paused {
          background: #fef3c7;
          color: #92400e;
        }

        .status-badge.stopped {
          background: #f1f5f9;
          color: #475569;
        }

        .status-badge.error {
          background: #fee2e2;
          color: #991b1b;
        }

        .status-badge.offline {
          background: #f3f4f6;
          color: #6b7280;
        }

        .connection-badge {
          display: flex;
          align-items: center;
          gap: 0.375rem;
          font-size: 0.75rem;
        }

        .location-info,
        .network-info,
        .data-info,
        .performance-info {
          display: flex;
          flex-direction: column;
          gap: 0.125rem;
        }

        .site-name,
        .endpoint {
          font-weight: 500;
          color: #1e293b;
        }

        .description,
        .response-time,
        .last-seen,
        .uptime,
        .resource-usage,
        .error-count {
          font-size: 0.75rem;
          color: #64748b;
        }

        .data-points {
          font-weight: 500;
          color: #059669;
        }

        .resource-usage {
          display: flex;
          gap: 0.5rem;
        }

        .device-actions {
          display: flex;
          gap: 0.25rem;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.25rem;
          padding: 0.5rem 0.75rem;
          border: none;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-sm {
          padding: 0.375rem 0.5rem;
          font-size: 0.75rem;
        }

        .btn-primary {
          background: #0ea5e9;
          color: white;
        }

        .btn-primary:hover {
          background: #0284c7;
        }

        .btn-success {
          background: #10b981;
          color: white;
        }

        .btn-success:hover {
          background: #059669;
        }

        .btn-warning {
          background: #f59e0b;
          color: white;
        }

        .btn-warning:hover {
          background: #d97706;
        }

        .btn-error {
          background: #ef4444;
          color: white;
        }

        .btn-error:hover {
          background: #dc2626;
        }

        .btn-info {
          background: #06b6d4;
          color: white;
        }

        .btn-info:hover {
          background: #0891b2;
        }

        .btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        .empty-state {
          text-align: center;
          padding: 3rem;
          color: #64748b;
        }

        .empty-state i {
          font-size: 3rem;
          margin-bottom: 1rem;
          color: #cbd5e1;
        }

        .empty-state h3 {
          font-size: 1.25rem;
          margin-bottom: 0.5rem;
          color: #374151;
        }

        .loading-container {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          min-height: 400px;
          gap: 1rem;
        }

        .loading-spinner {
          width: 2rem;
          height: 2rem;
          border: 2px solid #e2e8f0;
          border-radius: 50%;
          border-top-color: #0ea5e9;
          animation: spin 1s ease-in-out infinite;
        }

        @keyframes spin {
          to {
            transform: rotate(360deg);
          }
        }

        .text-success-600 { color: #059669; }
        .text-warning-600 { color: #d97706; }
        .text-error-600 { color: #dc2626; }
        .text-neutral-400 { color: #9ca3af; }
        .text-neutral-500 { color: #6b7280; }
        .text-neutral-600 { color: #4b5563; }
        .text-sm { font-size: 0.875rem; }

        @media (max-width: 1400px) {
          .device-table-header,
          .device-table-row {
            grid-template-columns: 50px 250px 100px 120px 160px 120px 150px 120px;
          }
          
          .device-table-cell:nth-child(5) {
            display: none;
          }
        }

        @media (max-width: 1200px) {
          .control-panel {
            grid-template-columns: 1fr;
          }
          
          .device-table-header,
          .device-table-row {
            grid-template-columns: 50px 200px 100px 120px 120px 120px;
          }
          
          .device-table-cell:nth-child(6),
          .device-table-cell:nth-child(8) {
            display: none;
          }
        }

        @media (max-width: 768px) {
          .device-list-header {
            flex-direction: column;
            gap: 1rem;
            align-items: flex-start;
          }
          
          .filter-row {
            flex-direction: column;
            align-items: stretch;
          }
          
          .search-container {
            min-width: auto;
          }
          
          .stats-panel {
            grid-template-columns: repeat(2, 1fr);
          }
          
          .device-table-header {
            display: none;
          }
          
          .device-table-row {
            display: block;
            padding: 1rem;
            border: 1px solid #e2e8f0;
            border-radius: 0.5rem;
            margin-bottom: 0.5rem;
          }
          
          .device-table-cell {
            display: block;
            padding: 0.5rem 0;
            border: none;
          }
          
          .device-table-cell::before {
            content: attr(data-label);
            font-weight: 600;
            color: #374151;
            display: block;
            margin-bottom: 0.25rem;
            font-size: 0.75rem;
            text-transform: uppercase;
          }
        }
      `}} />
    </div>
  );
};

export default DeviceList;