import React, { useState, useEffect } from 'react';
import '../styles/base.css';
import '../styles/device-list.css';

interface DeviceInfo {
  id: string;
  name: string;
  type: 'PLC' | 'RTU' | 'Sensor' | 'Gateway' | 'HMI';
  protocol: 'Modbus' | 'MQTT' | 'BACnet' | 'OPC-UA' | 'Ethernet/IP';
  status: 'running' | 'paused' | 'stopped' | 'error' | 'offline';
  connectionStatus: 'connected' | 'disconnected' | 'timeout' | 'error';
  factoryName: string;
  location: string;
  ipAddress: string;
  manufacturer: string;
  model: string;
  serialNumber: string;
  firmwareVersion: string;
  dataPointCount: number;
  pollingInterval: number;
  lastCommunication: Date;
  errorCount: number;
  responseTime: number;
  uptime: string;
  memoryUsage: string;
  cpuUsage: string;
  description: string;
  tags: string[];
}

const DeviceList: React.FC = () => {
  const [devices, setDevices] = useState<DeviceInfo[]>([
    {
      id: 'dev001',
      name: 'Main PLC #1',
      type: 'PLC',
      protocol: 'Modbus',
      status: 'running',
      connectionStatus: 'connected',
      factoryName: 'Factory A',
      location: 'Building 1 - Floor 2',
      ipAddress: '192.168.1.100',
      manufacturer: 'Siemens',
      model: 'S7-1200',
      serialNumber: 'S71200-001',
      firmwareVersion: 'V4.5.2',
      dataPointCount: 45,
      pollingInterval: 1000,
      lastCommunication: new Date(Date.now() - 5000),
      errorCount: 0,
      responseTime: 12,
      uptime: '15d 8h 23m',
      memoryUsage: '2.1MB',
      cpuUsage: '8%',
      description: '메인 생산라인 제어용 PLC',
      tags: ['critical', 'production', 'main-line']
    },
    {
      id: 'dev002',
      name: 'Temperature Sensor Array',
      type: 'Sensor',
      protocol: 'MQTT',
      status: 'running',
      connectionStatus: 'connected',
      factoryName: 'Factory A',
      location: 'Building 1 - Floor 1',
      ipAddress: '192.168.1.101',
      manufacturer: 'Honeywell',
      model: 'T6000-Series',
      serialNumber: 'HON-T6-001',
      firmwareVersion: 'V2.1.0',
      dataPointCount: 12,
      pollingInterval: 5000,
      lastCommunication: new Date(Date.now() - 3000),
      errorCount: 2,
      responseTime: 45,
      uptime: '12d 4h 15m',
      memoryUsage: '512KB',
      cpuUsage: '2%',
      description: '생산구역 온도 모니터링',
      tags: ['monitoring', 'hvac', 'environmental']
    },
    {
      id: 'dev003',
      name: 'Energy Meter #1',
      type: 'RTU',
      protocol: 'Modbus',
      status: 'paused',
      connectionStatus: 'disconnected',
      factoryName: 'Factory B',
      location: 'Main Electrical Room',
      ipAddress: '192.168.2.50',
      manufacturer: 'Schneider Electric',
      model: 'PM8000',
      serialNumber: 'SE-PM8-001',
      firmwareVersion: 'V1.3.5',
      dataPointCount: 28,
      pollingInterval: 2000,
      lastCommunication: new Date(Date.now() - 300000),
      errorCount: 5,
      responseTime: 0,
      uptime: '0m',
      memoryUsage: '1.5MB',
      cpuUsage: '0%',
      description: '전력 사용량 측정 및 모니터링',
      tags: ['energy', 'monitoring', 'electrical']
    },
    {
      id: 'dev004',
      name: 'HVAC Controller',
      type: 'Gateway',
      protocol: 'BACnet',
      status: 'error',
      connectionStatus: 'error',
      factoryName: 'Factory A',
      location: 'HVAC Room',
      ipAddress: '192.168.1.200',
      manufacturer: 'Johnson Controls',
      model: 'Metasys N30',
      serialNumber: 'JC-N30-001',
      firmwareVersion: 'V10.1.2',
      dataPointCount: 67,
      pollingInterval: 3000,
      lastCommunication: new Date(Date.now() - 1800000),
      errorCount: 15,
      responseTime: 0,
      uptime: '0m',
      memoryUsage: '0MB',
      cpuUsage: '0%',
      description: '건물 자동화 시스템 제어기',
      tags: ['hvac', 'building-automation', 'critical']
    },
    {
      id: 'dev005',
      name: 'Production Line HMI',
      type: 'HMI',
      protocol: 'Ethernet/IP',
      status: 'running',
      connectionStatus: 'connected',
      factoryName: 'Factory A',
      location: 'Production Line 1',
      ipAddress: '192.168.1.150',
      manufacturer: 'Rockwell Automation',
      model: 'PanelView Plus 7',
      serialNumber: 'RA-PV7-001',
      firmwareVersion: 'V13.00.00',
      dataPointCount: 89,
      pollingInterval: 500,
      lastCommunication: new Date(Date.now() - 1000),
      errorCount: 1,
      responseTime: 8,
      uptime: '7d 12h 45m',
      memoryUsage: '64MB',
      cpuUsage: '15%',
      description: '생산라인 조작 및 모니터링 패널',
      tags: ['hmi', 'production', 'operator-interface']
    },
    {
      id: 'dev006',
      name: 'Vibration Monitor',
      type: 'Sensor',
      protocol: 'OPC-UA',
      status: 'stopped',
      connectionStatus: 'disconnected',
      factoryName: 'Factory B',
      location: 'Machine Shop',
      ipAddress: '192.168.2.75',
      manufacturer: 'SKF',
      model: 'Enlight Collect IMx-1',
      serialNumber: 'SKF-IMX-001',
      firmwareVersion: 'V2.5.1',
      dataPointCount: 24,
      pollingInterval: 1500,
      lastCommunication: new Date(Date.now() - 7200000),
      errorCount: 0,
      responseTime: 0,
      uptime: '0m',
      memoryUsage: '0MB',
      cpuUsage: '0%',
      description: '기계 진동 모니터링 센서',
      tags: ['predictive-maintenance', 'vibration', 'machinery']
    }
  ]);

  const [isProcessing, setIsProcessing] = useState(false);
  const [selectedDevices, setSelectedDevices] = useState<string[]>([]);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [filterFactory, setFilterFactory] = useState<string>('all');
  const [filterStatus, setFilterStatus] = useState<string>('all');
  const [filterProtocol, setFilterProtocol] = useState<string>('all');
  const [searchTerm, setSearchTerm] = useState<string>('');

  // 실시간 업데이트
  useEffect(() => {
    const interval = setInterval(() => {
      setLastUpdate(new Date());
      // 실제로는 여기서 디바이스 상태 API 호출하여 상태 업데이트
      setDevices(prev => prev.map(device => ({
        ...device,
        lastCommunication: device.status === 'running' && device.connectionStatus === 'connected' 
          ? new Date() 
          : device.lastCommunication,
        responseTime: device.status === 'running' && device.connectionStatus === 'connected'
          ? Math.floor(Math.random() * 50) + 5
          : 0
      })));
    }, 10000);

    return () => clearInterval(interval);
  }, []);

  // 필터링된 디바이스 목록
  const filteredDevices = devices.filter(device => {
    const matchesFactory = filterFactory === 'all' || device.factoryName === filterFactory;
    const matchesStatus = filterStatus === 'all' || device.status === filterStatus;
    const matchesProtocol = filterProtocol === 'all' || device.protocol === filterProtocol;
    const matchesSearch = searchTerm === '' || 
      device.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      device.location.toLowerCase().includes(searchTerm.toLowerCase()) ||
      device.ipAddress.includes(searchTerm) ||
      device.manufacturer.toLowerCase().includes(searchTerm.toLowerCase()) ||
      device.model.toLowerCase().includes(searchTerm.toLowerCase());
    
    return matchesFactory && matchesStatus && matchesProtocol && matchesSearch;
  });

  // 개별 디바이스 제어
  const handleDeviceControl = async (device: DeviceInfo, action: 'start' | 'pause' | 'stop') => {
    try {
      setIsProcessing(true);
      
      console.log(`${action}ing device ${device.name}...`);
      
      // API 호출 시뮬레이션
      await new Promise(resolve => setTimeout(resolve, 1500));
      
      setDevices(prev => prev.map(d => 
        d.id === device.id 
          ? { 
              ...d, 
              status: action === 'start' ? 'running' : action === 'pause' ? 'paused' : 'stopped',
              connectionStatus: action === 'start' ? 'connected' : 'disconnected',
              uptime: action === 'start' ? '0m' : action === 'pause' ? d.uptime : '0m',
              lastCommunication: action === 'start' ? new Date() : d.lastCommunication,
              errorCount: action === 'start' ? 0 : d.errorCount
            }
          : d
      ));
      
      console.log(`Device ${device.name} ${action} 완료`);
    } catch (error) {
      console.error(`디바이스 제어 실패:`, error);
      alert(`${device.name} ${action} 실패`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 일괄 디바이스 제어
  const handleBulkAction = async (action: 'start' | 'pause' | 'stop') => {
    if (filteredDevices.length === 0) {
      alert('제어할 디바이스가 없습니다.');
      return;
    }

    const confirmation = confirm(
      `${filteredDevices.length}개의 디바이스를 ${
        action === 'start' ? '시작' : 
        action === 'pause' ? '일시정지' : '정지'
      }하시겠습니까?`
    );

    if (!confirmation) return;

    try {
      setIsProcessing(true);
      
      // 순차적으로 처리
      for (const device of filteredDevices) {
        console.log(`${action}ing ${device.name}...`);
        await new Promise(resolve => setTimeout(resolve, 300));
        
        setDevices(prev => prev.map(d => 
          d.id === device.id 
            ? { 
                ...d, 
                status: action === 'start' ? 'running' : action === 'pause' ? 'paused' : 'stopped',
                connectionStatus: action === 'start' ? 'connected' : 'disconnected',
                uptime: action === 'start' ? '0m' : action === 'pause' ? d.uptime : '0m'
              }
            : d
        ));
      }
      
      alert(`${filteredDevices.length}개 디바이스 ${action} 완료`);
    } catch (error) {
      console.error('일괄 디바이스 제어 실패:', error);
      alert('일괄 디바이스 제어 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // 선택된 디바이스 제어
  const handleSelectedAction = async (action: 'start' | 'pause' | 'stop') => {
    if (selectedDevices.length === 0) {
      alert('디바이스를 선택해주세요.');
      return;
    }

    const selectedDeviceObjects = devices.filter(d => selectedDevices.includes(d.id));

    try {
      setIsProcessing(true);
      
      for (const device of selectedDeviceObjects) {
        console.log(`${action}ing ${device.name}...`);
        await new Promise(resolve => setTimeout(resolve, 300));
        
        setDevices(prev => prev.map(d => 
          d.id === device.id 
            ? { 
                ...d, 
                status: action === 'start' ? 'running' : action === 'pause' ? 'paused' : 'stopped',
                connectionStatus: action === 'start' ? 'connected' : 'disconnected',
                uptime: action === 'start' ? '0m' : action === 'pause' ? d.uptime : '0m'
              }
            : d
        ));
      }
      
      alert(`${selectedDeviceObjects.length}개 디바이스 ${action} 완료`);
      setSelectedDevices([]);
    } catch (error) {
      console.error('선택된 디바이스 제어 실패:', error);
      alert('선택된 디바이스 제어 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // 체크박스 핸들러
  const handleDeviceSelect = (deviceId: string) => {
    setSelectedDevices(prev => 
      prev.includes(deviceId)
        ? prev.filter(id => id !== deviceId)
        : [...prev, deviceId]
    );
  };

  // 디바이스 타입별 아이콘
  const getDeviceIcon = (type: string) => {
    switch (type) {
      case 'PLC': return 'fas fa-microchip';
      case 'RTU': return 'fas fa-satellite-dish';
      case 'Sensor': return 'fas fa-thermometer-half';
      case 'Gateway': return 'fas fa-network-wired';
      case 'HMI': return 'fas fa-desktop';
      default: return 'fas fa-cube';
    }
  };

  // 프로토콜별 색상
  const getProtocolColor = (protocol: string) => {
    switch (protocol) {
      case 'Modbus': return 'bg-blue-100 text-blue-800';
      case 'MQTT': return 'bg-green-100 text-green-800';
      case 'BACnet': return 'bg-purple-100 text-purple-800';
      case 'OPC-UA': return 'bg-orange-100 text-orange-800';
      case 'Ethernet/IP': return 'bg-indigo-100 text-indigo-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  // 시간 포맷 함수
  const formatTimeAgo = (date: Date) => {
    const now = new Date();
    const diffMs = now.getTime() - date.getTime();
    const diffSecs = Math.floor(diffMs / 1000);
    
    if (diffSecs < 60) return '방금 전';
    const diffMins = Math.floor(diffSecs / 60);
    if (diffMins < 60) return `${diffMins}분 전`;
    const diffHours = Math.floor(diffMins / 60);
    if (diffHours < 24) return `${diffHours}시간 전`;
    const diffDays = Math.floor(diffHours / 24);
    return `${diffDays}일 전`;
  };

  // 공장 목록 추출
  const factories = [...new Set(devices.map(d => d.factoryName))];
  const protocols = [...new Set(devices.map(d => d.protocol))];

  return (
    <div className="device-management-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">디바이스 관리</h1>
        <div className="page-actions">
          <span className="text-sm text-neutral-600">
            마지막 업데이트: {formatTimeAgo(lastUpdate)}
          </span>
          <button 
            className="btn btn-primary"
            onClick={() => setLastUpdate(new Date())}
            disabled={isProcessing}
          >
            <i className="fas fa-sync-alt"></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 필터 및 검색 */}
      <div className="filter-panel">
        <div className="filter-row">
          <div className="filter-group">
            <label>공장</label>
            <select 
              value={filterFactory} 
              onChange={(e) => setFilterFactory(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 공장</option>
              {factories.map(factory => (
                <option key={factory} value={factory}>{factory}</option>
              ))}
            </select>
          </div>

          <div className="filter-group">
            <label>상태</label>
            <select 
              value={filterStatus} 
              onChange={(e) => setFilterStatus(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 상태</option>
              <option value="running">실행 중</option>
              <option value="paused">일시정지</option>
              <option value="stopped">정지</option>
              <option value="error">오류</option>
              <option value="offline">오프라인</option>
            </select>
          </div>

          <div className="filter-group">
            <label>프로토콜</label>
            <select 
              value={filterProtocol} 
              onChange={(e) => setFilterProtocol(e.target.value)}
              className="filter-select"
            >
              <option value="all">전체 프로토콜</option>
              {protocols.map(protocol => (
                <option key={protocol} value={protocol}>{protocol}</option>
              ))}
            </select>
          </div>

          <div className="filter-group flex-1">
            <label>검색</label>
            <div className="search-input-container">
              <input
                type="text"
                placeholder="디바이스명, 위치, IP주소, 제조사, 모델 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="search-input"
              />
              <i className="fas fa-search search-icon"></i>
            </div>
          </div>
        </div>
      </div>

      {/* 통계 정보 */}
      <div className="stats-panel">
        <div className="stat-card">
          <div className="stat-value">{devices.length}</div>
          <div className="stat-label">전체 디바이스</div>
        </div>
        <div className="stat-card status-running">
          <div className="stat-value">{devices.filter(d => d.status === 'running').length}</div>
          <div className="stat-label">실행 중</div>
        </div>
        <div className="stat-card status-paused">
          <div className="stat-value">{devices.filter(d => d.status === 'paused').length}</div>
          <div className="stat-label">일시정지</div>
        </div>
        <div className="stat-card status-stopped">
          <div className="stat-value">{devices.filter(d => d.status === 'stopped').length}</div>
          <div className="stat-label">정지</div>
        </div>
        <div className="stat-card status-error">
          <div className="stat-value">{devices.filter(d => d.status === 'error').length}</div>
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

      {/* 디바이스 목록 */}
      <div className="device-list">
        <div className="device-list-header">
          <div className="device-list-title">
            <h3>디바이스 목록</h3>
            <span className="device-count">
              {filteredDevices.filter(d => d.status === 'running').length} / {filteredDevices.length} 실행 중
            </span>
          </div>
        </div>

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
            <div className="device-table-cell">통신</div>
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
                  <div className={`device-icon ${
                    device.status === 'running' ? 'text-success-600' : 
                    device.status === 'error' ? 'text-error-600' : 
                    device.status === 'paused' ? 'text-warning-600' : 'text-neutral-400'
                  }`}>
                    <i className={getDeviceIcon(device.type)}></i>
                  </div>
                  <div>
                    <div className="device-name">{device.name}</div>
                    <div className="device-details">
                      <span className={`protocol-badge ${getProtocolColor(device.protocol)}`}>
                        {device.protocol}
                      </span>
                      <span className="device-type">{device.type}</span>
                    </div>
                    <div className="device-manufacturer">
                      {device.manufacturer} {device.model}
                    </div>
                  </div>
                </div>
              </div>
              
              <div className="device-table-cell">
                <span className={`status ${
                  device.status === 'running' ? 'status-running' : 
                  device.status === 'error' ? 'status-error' : 
                  device.status === 'paused' ? 'status-paused' : 'status-stopped'
                }`}>
                  <div className={`status-dot ${
                    device.status === 'running' ? 'status-dot-running' : 
                    device.status === 'error' ? 'status-dot-error' : 
                    device.status === 'paused' ? 'status-dot-paused' : 'status-dot-stopped'
                  }`}></div>
                  {device.status === 'running' ? '실행 중' : 
                   device.status === 'error' ? '오류' : 
                   device.status === 'paused' ? '일시정지' : '정지'}
                </span>
                <div className="uptime">{device.uptime}</div>
              </div>

              <div className="device-table-cell">
                <span className={`connection-status ${
                  device.connectionStatus === 'connected' ? 'status-connected' : 
                  device.connectionStatus === 'error' ? 'status-error' : 'status-disconnected'
                }`}>
                  {device.connectionStatus === 'connected' ? '연결됨' : 
                   device.connectionStatus === 'error' ? '통신오류' : 
                   device.connectionStatus === 'timeout' ? '타임아웃' : '연결끊김'}
                </span>
                <div className="communication-info">
                  <span className="text-xs">응답: {device.responseTime}ms</span>
                  <span className="text-xs">오류: {device.errorCount}회</span>
                  <span className="text-xs">최근: {formatTimeAgo(device.lastCommunication)}</span>
                </div>
              </div>
              
              <div className="device-table-cell">
                <div className="location-info">
                  <div className="factory-name">{device.factoryName}</div>
                  <div className="location-detail">{device.location}</div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="network-info">
                  <div className="ip-address">{device.ipAddress}</div>
                  <div className="serial-number">S/N: {device.serialNumber}</div>
                  <div className="firmware">FW: {device.firmwareVersion}</div>
                </div>
              </div>
              
              <div className="device-table-cell">
                <div className="data-info">
                  <span className="data-points">{device.dataPointCount} 포인트</span>
                  <span className="polling-interval">{device.pollingInterval}ms 주기</span>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="performance-info">
                  <span className="text-xs">CPU: {device.cpuUsage}</span>
                  <span className="text-xs">MEM: {device.memoryUsage}</span>
                </div>
              </div>
              
              <div className="device-table-cell">
                <div className="device-controls">
                  <div className="control-buttons">
                    {device.status === 'running' ? (
                      <>
                        <button 
                          className="btn btn-sm btn-warning"
                          onClick={() => handleDeviceControl(device, 'pause')}
                          disabled={isProcessing}
                          title="일시정지"
                        >
                          <i className="fas fa-pause"></i>
                        </button>
                        <button 
                          className="btn btn-sm btn-error"
                          onClick={() => handleDeviceControl(device, 'stop')}
                          disabled={isProcessing}
                          title="정지"
                        >
                          <i className="fas fa-stop"></i>
                        </button>
                      </>
                    ) : device.status === 'paused' ? (
                      <>
                        <button 
                          className="btn btn-sm btn-success"
                          onClick={() => handleDeviceControl(device, 'start')}
                          disabled={isProcessing}
                          title="재시작"
                        >
                          <i className="fas fa-play"></i>
                        </button>
                        <button 
                          className="btn btn-sm btn-error"
                          onClick={() => handleDeviceControl(device, 'stop')}
                          disabled={isProcessing}
                          title="정지"
                        >
                          <i className="fas fa-stop"></i>
                        </button>
                      </>
                    ) : (
                      <button 
                        className="btn btn-sm btn-success"
                        onClick={() => handleDeviceControl(device, 'start')}
                        disabled={isProcessing}
                        title="시작"
                      >
                        <i className="fas fa-play"></i>
                      </button>
                    )}
                  </div>
                </div>
              </div>
            </div>
          ))}
        </div>

        {filteredDevices.length === 0 && (
          <div className="empty-state">
            <i className="fas fa-search empty-icon"></i>
            <div className="empty-title">검색 결과가 없습니다</div>
            <div className="empty-description">
              필터 조건을 변경하거나 검색어를 확인해보세요.
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default DeviceList;