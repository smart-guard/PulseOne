import React, { useState, useEffect } from 'react';
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

  // 실시간 업데이트 (모달 열림 상태에 따라 제어)
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
        console.log('🔄 데이터 변환 시작...');
        setDevices(result.data);
      } else {
        console.warn('⚠️ 예상과 다른 응답 구조:', result);
        setDevices([]);
      }
    } catch (error) {
      console.error('❌ 디바이스 목록 조회 실패:', error);
      setError(error instanceof Error ? error.message : '알 수 없는 오류가 발생했습니다.');
      setDevices([]);
    } finally {
      setIsLoading(false);
    }
  };

  // 실시간 업데이트 제어 (모달이 열린 상태에서는 새로고침 중지)
  useEffect(() => {
    if (!autoRefresh || isModalOpen) return;

    const interval = setInterval(() => {
      if (!isModalOpen) { // 모달이 닫힌 상태에서만 업데이트
        fetchDevices();
        setLastUpdate(new Date());
      }
    }, 30000); // 30초 간격

    return () => clearInterval(interval);
  }, [autoRefresh, isModalOpen]); // isModalOpen 의존성 추가

  // 컴포넌트 마운트 시 초기 데이터 로드
  useEffect(() => {
    fetchDevices();
  }, []);

  // 디바이스 필터링
  useEffect(() => {
    let filtered = devices;

    // 검색어 필터링
    if (searchTerm) {
      filtered = filtered.filter(device =>
        device.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        device.endpoint.toLowerCase().includes(searchTerm.toLowerCase()) ||
        device.protocol_type.toLowerCase().includes(searchTerm.toLowerCase()) ||
        (device.manufacturer?.toLowerCase().includes(searchTerm.toLowerCase())) ||
        (device.model?.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    }

    // 상태 필터링
    if (statusFilter !== 'all') {
      filtered = filtered.filter(device => device.status === statusFilter);
    }

    // 프로토콜 필터링
    if (protocolFilter !== 'all') {
      filtered = filtered.filter(device => device.protocol_type === protocolFilter);
    }

    // 연결 상태 필터링
    if (connectionFilter !== 'all') {
      filtered = filtered.filter(device => device.connection_status === connectionFilter);
    }

    setFilteredDevices(filtered);
  }, [devices, searchTerm, statusFilter, protocolFilter, connectionFilter]);

  // 모달 핸들러들
  const handleModalOpen = (device: Device | null, mode: 'view' | 'edit' | 'create') => {
    setSelectedDevice(device);
    setModalMode(mode);
    setIsModalOpen(true);
    setAutoRefresh(false); // 모달이 열릴 때 자동 새로고침 중지
  };

  const handleModalClose = () => {
    setIsModalOpen(false);
    setSelectedDevice(null);
    setAutoRefresh(true); // 모달이 닫힐 때 자동 새로고침 재개
  };

  const handleDeviceAction = async (device: Device, action: string) => {
    setIsProcessing(true);
    try {
      console.log(`🔄 디바이스 ${action} 액션 실행:`, device.name);
      // TODO: 실제 API 호출 구현
      await new Promise(resolve => setTimeout(resolve, 1000));
      console.log(`✅ 디바이스 ${action} 완료:`, device.name);
      
      // 액션 완료 후 데이터 새로고침
      if (!isModalOpen) {
        await fetchDevices();
      }
    } catch (error) {
      console.error(`❌ 디바이스 ${action} 실패:`, error);
    } finally {
      setIsProcessing(false);
    }
  };

  // 디바이스 저장/수정 핸들러
  const handleDeviceSave = async (device: Device) => {
    try {
      console.log('💾 디바이스 저장:', device);
      // TODO: 실제 API 호출 구현
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      // 저장 성공 후 목록 새로고침
      await fetchDevices();
      handleModalClose();
    } catch (error) {
      console.error('❌ 디바이스 저장 실패:', error);
      throw error;
    }
  };

  // 디바이스 삭제 핸들러
  const handleDeviceDelete = async (deviceId: number) => {
    try {
      console.log('🗑️ 디바이스 삭제:', deviceId);
      // TODO: 실제 API 호출 구현
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      // 삭제 성공 후 목록 새로고침
      await fetchDevices();
      handleModalClose();
    } catch (error) {
      console.error('❌ 디바이스 삭제 실패:', error);
      throw error;
    }
  };

  // 체크박스 선택 핸들러
  const handleDeviceSelect = (deviceId: number) => {
    setSelectedDevices(prev => 
      prev.includes(deviceId) 
        ? prev.filter(id => id !== deviceId)
        : [...prev, deviceId]
    );
  };

  const handleSelectAll = () => {
    if (selectedDevices.length === filteredDevices.length) {
      setSelectedDevices([]);
    } else {
      setSelectedDevices(filteredDevices.map(device => device.id));
    }
  };

  // 통계 계산
  const stats: DeviceStats = {
    total: devices.length,
    running: devices.filter(d => d.status === 'running').length,
    stopped: devices.filter(d => d.status === 'stopped').length,
    error: devices.filter(d => d.status === 'error').length,
    connected: devices.filter(d => d.connection_status === 'connected').length,
    disconnected: devices.filter(d => d.connection_status === 'disconnected').length
  };

  // 고유 프로토콜 목록
  const protocols = [...new Set(devices.map(device => device.protocol_type))];

  if (isLoading && devices.length === 0) {
    return (
      <div className="loading-container">
        <div className="loading-spinner"></div>
        <p>디바이스 목록을 불러오는 중...</p>
      </div>
    );
  }

  return (
    <div className="device-list-container">
      {/* 헤더 */}
      <div className="device-list-header">
        <div className="header-left">
          <h1>
            <i className="fas fa-network-wired"></i>
            디바이스 관리
          </h1>
          <p className="subtitle">마지막 업데이트: {lastUpdate.toLocaleTimeString()}</p>
        </div>
        <div className="header-actions">
          <button 
            className="btn btn-primary"
            onClick={() => handleModalOpen(null, 'create')}
          >
            <i className="fas fa-plus"></i>
            디바이스 추가
          </button>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="error-banner">
          <i className="fas fa-exclamation-triangle"></i>
          <span>{error}</span>
          <button onClick={() => setError(null)} className="error-close">
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 통계 카드 */}
      <div className="stats-grid">
        <div className="stat-card">
          <div className="stat-value">{stats.total}</div>
          <div className="stat-label">전체 디바이스</div>
        </div>
        <div className="stat-card success">
          <div className="stat-value">{stats.connected}</div>
          <div className="stat-label">연결됨</div>
        </div>
        <div className="stat-card error">
          <div className="stat-value">{stats.disconnected}</div>
          <div className="stat-label">연결끊김</div>
        </div>
        <div className="stat-card warning">
          <div className="stat-value">{stats.error}</div>
          <div className="stat-label">오류</div>
        </div>
      </div>

      {/* 필터 및 검색 */}
      <div className="device-controls">
        <div className="controls-left">
          <div className="search-box">
            <i className="fas fa-search"></i>
            <input
              type="text"
              placeholder="디바이스명, 엔드포인트, 프로토콜로 검색..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
            />
          </div>
        </div>
        <div className="controls-right">
          <div className="filter-group">
            <label>프로토콜</label>
            <select value={protocolFilter} onChange={(e) => setProtocolFilter(e.target.value)}>
              <option value="all">전체</option>
              {protocols.map(protocol => (
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
              <option value="connecting">연결중</option>
            </select>
          </div>
        </div>
      </div>

      {/* 디바이스 목록 */}
      <div className="device-list">
        <div className="device-list-header-row">
          <div className="device-list-actions">
            <input
              type="checkbox"
              checked={selectedDevices.length === filteredDevices.length && filteredDevices.length > 0}
              onChange={handleSelectAll}
            />
            <span>{selectedDevices.length}개 선택됨</span>
            {selectedDevices.length > 0 && (
              <div className="bulk-actions">
                <button className="btn btn-sm btn-success" disabled={isProcessing}>
                  <i className="fas fa-play"></i> 일괄 시작
                </button>
                <button className="btn btn-sm btn-warning" disabled={isProcessing}>
                  <i className="fas fa-pause"></i> 일괄 중지
                </button>
              </div>
            )}
          </div>
          <div className="device-list-info">
            총 {filteredDevices.length}개 디바이스
          </div>
        </div>

        <div className="device-cards">
          {filteredDevices.map((device) => (
            <div key={device.id} className="device-card">
              <div className="device-card-header">
                <div className="device-select">
                  <input
                    type="checkbox"
                    checked={selectedDevices.includes(device.id)}
                    onChange={() => handleDeviceSelect(device.id)}
                  />
                </div>
                <div className="device-info">
                  <h3 className="device-name">{device.name}</h3>
                  <p className="device-endpoint">{device.endpoint}</p>
                </div>
                <div className="device-status">
                  <span className={`status-badge ${device.connection_status}`}>
                    <i className="fas fa-circle"></i>
                    {device.connection_status === 'connected' ? '연결됨' : 
                     device.connection_status === 'disconnected' ? '연결끊김' : '연결중'}
                  </span>
                </div>
              </div>
              <div className="device-card-body">
                <div className="device-details">
                  <div className="detail-item">
                    <span className="label">프로토콜:</span>
                    <span className="value">{device.protocol_type}</span>
                  </div>
                  <div className="detail-item">
                    <span className="label">제조사:</span>
                    <span className="value">{device.manufacturer || 'N/A'}</span>
                  </div>
                  <div className="detail-item">
                    <span className="label">모델:</span>
                    <span className="value">{device.model || 'N/A'}</span>
                  </div>
                  <div className="detail-item">
                    <span className="label">응답시간:</span>
                    <span className="value">{device.response_time || 0}ms</span>
                  </div>
                  <div className="detail-item">
                    <span className="label">마지막 통신:</span>
                    <span className="value">{device.last_seen || 'N/A'}</span>
                  </div>
                </div>
              </div>
              <div className="device-card-footer">
                <div className="device-actions">
                  <button 
                    className="btn btn-sm btn-info"
                    onClick={() => handleModalOpen(device, 'view')}
                  >
                    <i className="fas fa-eye"></i>
                    상세
                  </button>
                  <button 
                    className="btn btn-sm btn-secondary"
                    onClick={() => handleModalOpen(device, 'edit')}
                  >
                    <i className="fas fa-edit"></i>
                    편집
                  </button>
                  {device.connection_status === 'connected' ? (
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

      {/* DeviceDetailModal - 임시 모달 제거, 실제 모달만 사용 */}
      <DeviceDetailModal
        device={selectedDevice}
        isOpen={isModalOpen}
        mode={modalMode}
        onClose={handleModalClose}
        onSave={handleDeviceSave}
        onDelete={handleDeviceDelete}
      />

      <style jsx>{`
        .device-list-container {
          padding: 1.5rem;
          max-width: 1400px;
          margin: 0 auto;
        }

        .device-list-header {
          display: flex;
          justify-content: space-between;
          align-items: flex-start;
          margin-bottom: 2rem;
        }

        .device-list-header h1 {
          font-size: 2rem;
          font-weight: 700;
          color: #1f2937;
          margin: 0;
          display: flex;
          align-items: center;
          gap: 0.75rem;
        }

        .device-list-header h1 i {
          color: #0ea5e9;
        }

        .subtitle {
          color: #6b7280;
          font-size: 0.875rem;
          margin: 0.5rem 0 0 0;
        }

        .header-actions {
          display: flex;
          gap: 0.75rem;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 0.5rem;
          font-size: 0.875rem;
          font-weight: 500;
          text-decoration: none;
          cursor: pointer;
          transition: all 0.2s ease;
        }

        .btn-primary {
          background: #0ea5e9;
          color: white;
        }

        .btn-primary:hover {
          background: #0284c7;
        }

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-secondary:hover {
          background: #475569;
        }

        .btn-success {
          background: #059669;
          color: white;
        }

        .btn-success:hover {
          background: #047857;
        }

        .btn-warning {
          background: #d97706;
          color: white;
        }

        .btn-warning:hover {
          background: #b45309;
        }

        .btn-error {
          background: #dc2626;
          color: white;
        }

        .btn-error:hover {
          background: #b91c1c;
        }

        .btn-info {
          background: #0891b2;
          color: white;
        }

        .btn-info:hover {
          background: #0e7490;
        }

        .btn-sm {
          padding: 0.375rem 0.75rem;
          font-size: 0.75rem;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        .error-banner {
          display: flex;
          align-items: center;
          gap: 0.75rem;
          background: #fef2f2;
          border: 1px solid #fecaca;
          color: #dc2626;
          padding: 0.75rem 1rem;
          border-radius: 0.5rem;
          margin-bottom: 1.5rem;
        }

        .error-close {
          background: none;
          border: none;
          color: #dc2626;
          cursor: pointer;
          margin-left: auto;
          padding: 0.25rem;
        }

        .stats-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 1rem;
          margin-bottom: 2rem;
        }

        .stat-card {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.75rem;
          padding: 1.5rem;
          text-align: center;
        }

        .stat-card.success {
          border-color: #34d399;
          background: linear-gradient(135deg, #f0fdf4, #ecfdf5);
        }

        .stat-card.error {
          border-color: #f87171;
          background: linear-gradient(135deg, #fef2f2, #fdf2f8);
        }

        .stat-card.warning {
          border-color: #fbbf24;
          background: linear-gradient(135deg, #fffbeb, #fefce8);
        }

        .stat-value {
          font-size: 2rem;
          font-weight: 700;
          color: #1f2937;
          margin-bottom: 0.25rem;
        }

        .stat-label {
          font-size: 0.875rem;
          color: #6b7280;
          font-weight: 500;
        }

        .device-controls {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1.5rem;
          gap: 1rem;
          flex-wrap: wrap;
        }

        .controls-left {
          flex: 1;
          min-width: 300px;
        }

        .search-box {
          position: relative;
          display: flex;
          align-items: center;
        }

        .search-box i {
          position: absolute;
          left: 0.75rem;
          color: #9ca3af;
          z-index: 1;
        }

        .search-box input {
          width: 100%;
          padding: 0.75rem 0.75rem 0.75rem 2.5rem;
          border: 1px solid #d1d5db;
          border-radius: 0.5rem;
          font-size: 0.875rem;
        }

        .search-box input:focus {
          outline: none;
          border-color: #0ea5e9;
          box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
        }

        .controls-right {
          display: flex;
          gap: 1rem;
        }

        .filter-group {
          display: flex;
          flex-direction: column;
          gap: 0.25rem;
        }

        .filter-group label {
          font-size: 0.75rem;
          font-weight: 500;
          color: #6b7280;
        }

        .filter-group select {
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          min-width: 120px;
        }

        .device-list {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.75rem;
          overflow: hidden;
        }

        .device-list-header-row {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1rem 1.5rem;
          background: #f9fafb;
          border-bottom: 1px solid #e5e7eb;
        }

        .device-list-actions {
          display: flex;
          align-items: center;
          gap: 1rem;
        }

        .bulk-actions {
          display: flex;
          gap: 0.5rem;
        }

        .device-list-info {
          font-size: 0.875rem;
          color: #6b7280;
        }

        .device-cards {
          padding: 1rem;
          display: grid;
          gap: 1rem;
        }

        .device-card {
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1rem;
          background: white;
          transition: all 0.2s ease;
        }

        .device-card:hover {
          border-color: #0ea5e9;
          box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }

        .device-card-header {
          display: flex;
          align-items: flex-start;
          gap: 1rem;
          margin-bottom: 1rem;
        }

        .device-select {
          padding-top: 0.25rem;
        }

        .device-info {
          flex: 1;
        }

        .device-name {
          font-size: 1.125rem;
          font-weight: 600;
          color: #1f2937;
          margin: 0 0 0.25rem 0;
        }

        .device-endpoint {
          font-size: 0.875rem;
          color: #6b7280;
          margin: 0;
          font-family: 'Courier New', monospace;
        }

        .device-status {
          display: flex;
          align-items: center;
        }

        .status-badge {
          display: inline-flex;
          align-items: center;
          gap: 0.375rem;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .status-badge.connected {
          background: #dcfce7;
          color: #166534;
        }

        .status-badge.disconnected {
          background: #fee2e2;
          color: #991b1b;
        }

        .status-badge.connecting {
          background: #fef3c7;
          color: #92400e;
        }

        .status-badge i {
          font-size: 0.5rem;
        }

        .device-card-body {
          margin-bottom: 1rem;
        }

        .device-details {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 0.75rem;
        }

        .detail-item {
          display: flex;
          justify-content: space-between;
          padding: 0.5rem 0;
          border-bottom: 1px solid #f3f4f6;
        }

        .detail-item:last-child {
          border-bottom: none;
        }

        .detail-item .label {
          font-size: 0.75rem;
          color: #6b7280;
          font-weight: 500;
        }

        .detail-item .value {
          font-size: 0.75rem;
          color: #1f2937;
          font-weight: 500;
        }

        .device-card-footer {
          border-top: 1px solid #f3f4f6;
          padding-top: 1rem;
        }

        .device-actions {
          display: flex;
          gap: 0.5rem;
          flex-wrap: wrap;
        }

        .empty-state {
          text-align: center;
          padding: 4rem 2rem;
          color: #6b7280;
        }

        .empty-state i {
          font-size: 4rem;
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
      `}</style>
    </div>
  );
};

export default DeviceList;