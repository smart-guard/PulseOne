import React, { useState, useEffect } from 'react';
// DeviceDetailModal을 생성한 후 이 주석을 해제하세요
import DeviceDetailModal from '../components/modals/DeviceDetailModal';

interface Device {
  id: number;
  name: string;
  protocol_type: string;
  device_type?: string;
  endpoint: string;
  is_enabled: boolean;
  connection_status: string;
  status: string;
  last_seen?: string;
  site_name?: string;
  data_points_count?: number;
  description?: string;
  manufacturer?: string;
  model?: string;
  response_time?: number;
  error_count?: number;
  polling_interval?: number;
  created_at?: string;
  uptime?: string;
}

interface DeviceStats {
  total: number;
  running: number;
  stopped: number;
  error: number;
  connected: number;
  disconnected: number;
}

const DeviceList: React.FC = () => {
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
  const [connectionFilter, setConnectionFilter] = useState<string>('all');

  // 실시간 업데이트
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [autoRefresh, setAutoRefresh] = useState(true);

  // 모달 상태
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null);
  const [modalMode, setModalMode] = useState<'view' | 'edit' | 'create'>('view');
  const [isModalOpen, setIsModalOpen] = useState(false);

  // API 호출 함수
  const fetchDevices = async () => {
    try {
      setIsLoading(true);
      setError(null);
      
      console.log('🔍 디바이스 목록 조회 시작...');
      
      const response = await fetch('http://localhost:3000/api/devices');
      
      if (!response.ok) {
        const errorText = await response.text();
        console.error('❌ API 응답 오류:', response.status, errorText);
        throw new Error(`서버 오류 (${response.status}): ${errorText.substring(0, 200)}`);
      }
      
      const contentType = response.headers.get('content-type');
      if (!contentType || !contentType.includes('application/json')) {
        const responseText = await response.text();
        console.error('❌ JSON이 아닌 응답:', responseText.substring(0, 500));
        throw new Error('서버가 JSON 응답을 반환하지 않았습니다. 백엔드 서버가 실행 중인지 확인하세요.');
      }
      
      const result = await response.json();
      console.log('📋 디바이스 API 응답:', result);
      
      if (result.success && Array.isArray(result.data)) {
        console.log('🔄 데이터 변환 시작. 원본 데이터 개수:', result.data.length);
        
        const transformedDevices = result.data.map((device: any) => {
          return {
            id: device.id,
            name: device.name,
            protocol_type: device.protocol_type,
            device_type: device.device_type,
            endpoint: device.endpoint,
            is_enabled: Boolean(device.is_enabled),
            
            status: device.connection_status === 'connected' ? 'running' : 
                   device.connection_status === 'disconnected' ? 'stopped' :
                   device.is_enabled ? 'offline' : 'stopped',
            connection_status: device.connection_status || 'unknown',
            last_seen: device.last_communication,
            
            site_name: device.site_name,
            description: device.description,
            manufacturer: device.manufacturer,
            model: device.model,
            
            polling_interval: device.polling_interval,
            response_time: device.response_time,
            error_count: device.error_count || 0,
            data_points_count: device.data_point_count || 0,
            
            created_at: device.created_at,
            uptime: device.last_communication ? 
              `${Math.floor((Date.now() - new Date(device.last_communication).getTime()) / (1000 * 60 * 60 * 24))}일 전` : 
              'N/A'
          };
        });
        
        console.log('✅ 변환 완료. 변환된 데이터 개수:', transformedDevices.length);
        setDevices(transformedDevices);
      } else {
        console.warn('⚠️ 디바이스 API 실패:', result);
        setDevices([]);
        throw new Error(result.error || result.message || '알 수 없는 API 오류');
      }
    } catch (error: any) {
      console.error('❌ 디바이스 API 호출 실패:', error);
      
      let errorMessage = '';
      if (error.message.includes('Failed to fetch')) {
        errorMessage = '백엔드 서버에 연결할 수 없습니다. 서버가 실행 중인지 확인하세요. (포트: 3000)';
      } else if (error.message.includes('JSON')) {
        errorMessage = `JSON 파싱 오류: ${error.message}`;
      } else if (error.message.includes('서버 오류')) {
        errorMessage = error.message;
      } else {
        errorMessage = `디바이스 목록을 불러오는데 실패했습니다: ${error.message}`;
      }
      
      setError(errorMessage);
      setDevices([]);
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  };

  // 디바이스 제어 함수
  const controlDevice = async (deviceId: number, action: 'start' | 'stop' | 'restart' | 'pause') => {
    try {
      console.log(`🎛️ 디바이스 ${deviceId} ${action} 요청...`);
      
      let endpoint = '';
      switch (action) {
        case 'start':
          endpoint = `http://localhost:3000/api/devices/${deviceId}/enable`;
          break;
        case 'stop':
        case 'pause':
          endpoint = `http://localhost:3000/api/devices/${deviceId}/disable`;
          break;
        case 'restart':
          endpoint = `http://localhost:3000/api/devices/${deviceId}/restart`;
          break;
      }
      
      const response = await fetch(endpoint, { method: 'POST' });
      const result = await response.json();
      
      if (result.success) {
        console.log(`✅ 디바이스 ${deviceId} ${action} 성공`);
        setTimeout(fetchDevices, 1000);
        return { success: true, message: result.message };
      } else {
        throw new Error(result.error || `${action} 실패`);
      }
    } catch (error: any) {
      console.error(`❌ 디바이스 ${deviceId} ${action} 실패:`, error);
      return { success: false, error: error.message };
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

  // ➕ 디바이스 추가
  const handleAddDevice = () => {
    setSelectedDevice(null);
    setModalMode('create');
    setIsModalOpen(true);
  };

  // 📋 디바이스 상세 보기
  const handleDeviceView = (device: Device) => {
    setSelectedDevice(device);
    setModalMode('view');
    setIsModalOpen(true);
  };

  // ✏️ 디바이스 편집
  const handleDeviceEdit = (device: Device) => {
    setSelectedDevice(device);
    setModalMode('edit');
    setIsModalOpen(true);
  };

  // 💾 디바이스 저장 (생성/수정)
  const handleDeviceSave = async (deviceData: Device) => {
    try {
      if (modalMode === 'create') {
        const response = await fetch('http://localhost:3000/api/devices', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(deviceData)
        });
        
        const result = await response.json();
        if (result.success) {
          alert('디바이스가 성공적으로 생성되었습니다!');
          fetchDevices();
        } else {
          throw new Error(result.error || '디바이스 생성 실패');
        }
      } else {
        const response = await fetch(`http://localhost:3000/api/devices/${deviceData.id}`, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(deviceData)
        });
        
        const result = await response.json();
        if (result.success) {
          alert('디바이스가 성공적으로 수정되었습니다!');
          fetchDevices();
        } else {
          throw new Error(result.error || '디바이스 수정 실패');
        }
      }
    } catch (error: any) {
      console.error('디바이스 저장 실패:', error);
      throw error;
    }
  };

  // 🗑️ 디바이스 삭제
  const handleDeviceDelete = async (deviceId: number) => {
    try {
      const response = await fetch(`http://localhost:3000/api/devices/${deviceId}`, {
        method: 'DELETE'
      });
      
      const result = await response.json();
      if (result.success) {
        alert('디바이스가 성공적으로 삭제되었습니다!');
        fetchDevices();
      } else {
        throw new Error(result.error || '디바이스 삭제 실패');
      }
    } catch (error: any) {
      console.error('디바이스 삭제 실패:', error);
      throw error;
    }
  };

  // 🔄 모달 닫기
  const handleModalClose = () => {
    setIsModalOpen(false);
    setSelectedDevice(null);
  };

  // 유틸리티 함수들
  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    return diff < 1 ? '방금 전' : diff < 60 ? `${diff}분 전` : `${Math.floor(diff/60)}시간 전`;
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'running': return 'text-success-600';
      case 'stopped': 
      case 'offline': return 'text-neutral-500';
      case 'error': return 'text-error-600';
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

  // 필터링 로직
  useEffect(() => {
    let filtered = devices;

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

    if (statusFilter !== 'all') {
      filtered = filtered.filter(device => device.status === statusFilter);
    }

    if (protocolFilter !== 'all') {
      filtered = filtered.filter(device => device.protocol_type === protocolFilter);
    }

    if (connectionFilter !== 'all') {
      filtered = filtered.filter(device => device.connection_status === connectionFilter);
    }

    setFilteredDevices(filtered);
  }, [devices, searchTerm, statusFilter, protocolFilter, connectionFilter]);

  // 디바이스 선택 핸들러
  const handleDeviceSelect = (deviceId: number) => {
    setSelectedDevices(prev =>
      prev.includes(deviceId)
        ? prev.filter(id => id !== deviceId)
        : [...prev, deviceId]
    );
  };

  // 실시간 업데이트
  useEffect(() => {
    fetchDevices();
    
    let interval: NodeJS.Timeout;
    if (autoRefresh) {
      interval = setInterval(fetchDevices, 10000);
    }
    
    return () => {
      if (interval) clearInterval(interval);
    };
  }, [autoRefresh]);

  // 통계 계산
  const stats: DeviceStats = {
    total: devices.length,
    running: devices.filter(d => d.connection_status === 'connected').length,
    stopped: devices.filter(d => d.connection_status === 'disconnected' || d.connection_status === null).length,
    error: devices.filter(d => d.status === 'error' || (d.error_count && d.error_count > 0)).length,
    connected: devices.filter(d => d.connection_status === 'connected').length,
    disconnected: devices.filter(d => d.connection_status === 'disconnected' || d.connection_status === null).length,
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
          <div className="error-content">
            <div className="error-message">{error}</div>
            <div className="error-actions">
              <button onClick={() => setError(null)} className="dismiss-btn">
                무시
              </button>
              <button onClick={fetchDevices} className="retry-btn">
                재시도
              </button>
              <button 
                onClick={() => window.open('http://localhost:3000/api/devices', '_blank')} 
                className="check-api-btn"
              >
                API 직접 확인
              </button>
              <button 
                onClick={() => window.open('http://localhost:3000/api/health', '_blank')} 
                className="check-health-btn"
              >
                서버 상태 확인
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 필터 패널 */}
      <div className="filter-panel">
        <div className="filter-row">
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
          
          <div className="filter-group">
            <label>상태</label>
            <select value={statusFilter} onChange={(e) => setStatusFilter(e.target.value)}>
              <option value="all">전체</option>
              <option value="running">실행중</option>
              <option value="stopped">정지</option>
              <option value="offline">오프라인</option>
              <option value="error">오류</option>
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
        <div className="stat-card status-stopped">
          <div className="stat-value">{stats.stopped}</div>
          <div className="stat-label">정지</div>
        </div>
        <div className="stat-card status-error">
          <div className="stat-value">{stats.error}</div>
          <div className="stat-label">오류</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{stats.connected}</div>
          <div className="stat-label">연결됨</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{filteredDevices.length}</div>
          <div className="stat-label">필터된 결과</div>
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
                <div className="device-info" onClick={() => handleDeviceView(device)} style={{ cursor: 'pointer' }}>
                  <div className={`device-icon ${getStatusColor(device.status)}`}>
                    <i className={
                      device.device_type === 'PLC' ? 'fas fa-microchip' :
                      device.device_type === 'SENSOR' ? 'fas fa-thermometer-half' :
                      device.device_type === 'CONTROLLER' ? 'fas fa-cogs' :
                      device.device_type === 'METER' ? 'fas fa-tachometer-alt' :
                      device.device_type === 'HMI' ? 'fas fa-desktop' :
                      'fas fa-network-wired'
                    }></i>
                  </div>
                  <div className="device-details">
                    <div className="device-name">{device.name}</div>
                    <div className="device-type">{device.protocol_type}</div>
                    <div className="device-manufacturer">
                      {device.manufacturer || 'Unknown'} {device.model || ''}
                    </div>
                  </div>
                </div>
              </div>

              <div className="device-table-cell">
                <span className={`status-badge ${device.status}`}>
                  {device.status === 'running' ? '실행중' :
                   device.status === 'stopped' ? '정지' :
                   device.status === 'offline' ? '오프라인' :
                   device.status === 'error' ? '오류' : '미확인'}
                </span>
              </div>

              <div className="device-table-cell">
                <span className={`connection-badge ${device.connection_status}`}>
                  <i className={`fas fa-circle ${getConnectionColor(device.connection_status)}`}></i>
                  {device.connection_status === 'connected' ? '연결됨' :
                   device.connection_status === 'disconnected' ? '연결끊김' :
                   device.connection_status === 'timeout' ? '타임아웃' : 
                   device.connection_status === 'error' ? '오류' : '미확인'}
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
                  <div className="response-time">{device.response_time ? `${device.response_time}ms` : 'N/A'}</div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="data-info">
                  <div className="data-points">{device.data_points_count || 0} 포인트</div>
                  <div className="last-seen">
                    {device.last_seen ? formatTimeAgo(device.last_seen) : 
                     device.created_at ? `생성: ${formatTimeAgo(device.created_at)}` : 'N/A'}
                  </div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="performance-info">
                  <div className="uptime">가동시간: {device.uptime || 'N/A'}</div>
                  <div className="resource-usage">
                    <span>응답: {device.response_time ? `${device.response_time}ms` : 'N/A'}</span>
                    <span>폴링: {device.polling_interval ? `${device.polling_interval}ms` : 'N/A'}</span>
                  </div>
                  <div className="error-count">
                    오류: {device.error_count || 0}회
                  </div>
                </div>
              </div>

              <div className="device-table-cell">
                <div className="device-actions">
                  <button 
                    className="btn btn-sm btn-info"
                    onClick={() => handleDeviceView(device)}
                    title="상세 보기"
                  >
                    <i className="fas fa-eye"></i>
                  </button>
                  <button 
                    className="btn btn-sm btn-warning"
                    onClick={() => handleDeviceEdit(device)}
                    title="편집"
                  >
                    <i className="fas fa-edit"></i>
                  </button>
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
        </div>
      )}

      {/* 임시 모달 (DeviceDetailModal 파일 생성 전까지) */}
      {isModalOpen && (
        <div className="modal-overlay-temp">
          <div className="modal-container-temp">
            <div className="modal-header-temp">
              <h2>디바이스 {modalMode === 'create' ? '추가' : modalMode === 'edit' ? '편집' : '상세'}</h2>
              <button onClick={handleModalClose} className="close-btn-temp">×</button>
            </div>
            <div className="modal-content-temp">
              <p><strong>선택된 디바이스:</strong> {selectedDevice?.name || '새 디바이스'}</p>
              <p><strong>모드:</strong> {modalMode}</p>
              <p><strong>상태:</strong> {selectedDevice?.status || 'N/A'}</p>
              <p><strong>프로토콜:</strong> {selectedDevice?.protocol_type || 'N/A'}</p>
              <p><strong>엔드포인트:</strong> {selectedDevice?.endpoint || 'N/A'}</p>
              <div className="modal-note">
                <p>📝 <strong>참고:</strong> DeviceDetailModal.tsx 파일을 생성하면 완전한 모달이 표시됩니다.</p>
                <ol>
                  <li>frontend/src/components/modals/ 폴더 생성</li>
                  <li>DeviceDetailModal.tsx 파일 생성</li>
                  <li>아티팩트 코드 복사 & 붙여넣기</li>
                  <li>상단 import 주석 해제</li>
                </ol>
              </div>
            </div>
            <div className="modal-footer-temp">
              <button onClick={handleModalClose} className="btn-temp btn-secondary-temp">
                닫기
              </button>
              {modalMode !== 'view' && (
                <button onClick={() => alert('DeviceDetailModal 생성 후 사용 가능')} className="btn-temp btn-primary-temp">
                  {modalMode === 'create' ? '생성' : '저장'}
                </button>
              )}
            </div>
          </div>
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
          align-items: flex-start;
          gap: 0.75rem;
        }

        .error-content {
          flex: 1;
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .error-message {
          font-weight: 500;
          line-height: 1.4;
        }

        .error-actions {
          display: flex;
          gap: 0.5rem;
          flex-wrap: wrap;
        }

        .error-actions button {
          padding: 0.25rem 0.75rem;
          border: 1px solid #dc2626;
          background: white;
          color: #dc2626;
          border-radius: 0.375rem;
          font-size: 0.75rem;
          cursor: pointer;
          transition: all 0.2s;
        }

        .error-actions button:hover {
          background: #dc2626;
          color: white;
        }

        .dismiss-btn {
          background: #f3f4f6 !important;
          color: #6b7280 !important;
          border-color: #d1d5db !important;
        }

        .dismiss-btn:hover {
          background: #e5e7eb !important;
          color: #374151 !important;
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

        .status-badge.stopped, .status-badge.offline {
          background: #f1f5f9;
          color: #475569;
        }

        .status-badge.error {
          background: #fee2e2;
          color: #991b1b;
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
        .text-neutral-500 { color: #6b7280; }

        /* 임시 모달 스타일 */
        .modal-overlay-temp {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: rgba(0, 0, 0, 0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 1000;
        }

        .modal-container-temp {
          background: white;
          border-radius: 8px;
          width: 90%;
          max-width: 600px;
          max-height: 80vh;
          overflow-y: auto;
          box-shadow: 0 20px 25px rgba(0, 0, 0, 0.25);
        }

        .modal-header-temp {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1.5rem;
          border-bottom: 1px solid #e5e7eb;
        }

        .modal-header-temp h2 {
          margin: 0;
          font-size: 1.5rem;
          font-weight: 600;
          color: #1f2937;
        }

        .close-btn-temp {
          background: none;
          border: none;
          font-size: 1.5rem;
          color: #6b7280;
          cursor: pointer;
          padding: 0.25rem;
          border-radius: 0.25rem;
        }

        .close-btn-temp:hover {
          background: #f3f4f6;
          color: #374151;
        }

        .modal-content-temp {
          padding: 1.5rem;
        }

        .modal-content-temp p {
          margin: 0.5rem 0;
          font-size: 0.875rem;
        }

        .modal-note {
          background: #f0f9ff;
          border: 1px solid #bae6fd;
          border-radius: 0.5rem;
          padding: 1rem;
          margin-top: 1rem;
        }

        .modal-note p {
          margin: 0.25rem 0;
          font-size: 0.8rem;
          color: #0369a1;
        }

        .modal-note ol {
          margin: 0.5rem 0;
          padding-left: 1.25rem;
          font-size: 0.75rem;
          color: #0284c7;
        }

        .modal-footer-temp {
          display: flex;
          justify-content: flex-end;
          gap: 0.75rem;
          padding: 1.5rem;
          border-top: 1px solid #e5e7eb;
        }

        .btn-temp {
          padding: 0.5rem 1rem;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-primary-temp {
          background: #3b82f6;
          color: white;
          border: none;
        }

        .btn-primary-temp:hover {
          background: #2563eb;
        }

        .btn-secondary-temp {
          background: white;
          color: #374151;
          border: 1px solid #d1d5db;
        }

        .btn-secondary-temp:hover {
          background: #f9fafb;
        }

        @media (max-width: 1200px) {
          .device-table-header,
          .device-table-row {
            grid-template-columns: 50px 200px 100px 120px 120px 120px;
          }
          
          .device-table-cell:nth-child(5),
          .device-table-cell:nth-child(7),
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
        }
      `}} />
    </div>
  );
};

export default DeviceList;