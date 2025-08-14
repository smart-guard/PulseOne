import React, { useState, useEffect } from 'react';

// 디바이스 타입 정의 (백엔드 스키마 기반)
interface Device {
  // devices 테이블 기본 정보
  id: number;
  tenant_id: number;
  site_id: number;
  device_group_id?: number;
  edge_server_id?: number;
  name: string;
  description?: string;
  device_type: string;
  manufacturer?: string;
  model?: string;
  serial_number?: string;
  protocol_type: string;
  endpoint: string;
  config?: any; // JSON
  polling_interval: number;
  timeout: number;
  retry_count: number;
  is_enabled: boolean;
  installation_date?: string;
  last_maintenance?: string;
  created_at: string;
  updated_at: string;
  
  // device_settings 상세 설정
  settings?: {
    polling_interval_ms: number;
    connection_timeout_ms: number;
    read_timeout_ms: number;
    write_timeout_ms: number;
    max_retry_count: number;
    retry_interval_ms: number;
    backoff_time_ms: number;
    keep_alive_enabled: boolean;
    keep_alive_interval_s: number;
    data_validation_enabled: boolean;
    performance_monitoring_enabled: boolean;
    detailed_logging_enabled: boolean;
    diagnostic_mode_enabled: boolean;
  };
  
  // device_status 실시간 상태
  status?: {
    connection_status: string;
    last_communication?: string;
    error_count: number;
    last_error?: string;
    response_time?: number;
    throughput_ops_per_sec: number;
    total_requests: number;
    successful_requests: number;
    failed_requests: number;
    firmware_version?: string;
    hardware_info?: any; // JSON
    diagnostic_data?: any; // JSON
    cpu_usage?: number;
    memory_usage?: number;
    uptime_percentage: number;
  };
  
  // 관련 정보
  site_name?: string;
  group_name?: string;
  data_points_count?: number;
}

interface DeviceDetailModalProps {
  device: Device | null;
  isOpen: boolean;
  mode: 'view' | 'edit' | 'create';
  onClose: () => void;
  onSave?: (device: Device) => void;
  onDelete?: (deviceId: number) => void;
}

const DeviceDetailModal: React.FC<DeviceDetailModalProps> = ({
  device,
  isOpen,
  mode,
  onClose,
  onSave,
  onDelete
}) => {
  const [activeTab, setActiveTab] = useState('basic');
  const [editData, setEditData] = useState<Device | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  // 모달이 열릴 때 데이터 초기화
  useEffect(() => {
    if (isOpen && device) {
      setEditData({ ...device });
    } else if (isOpen && mode === 'create') {
      // 새 디바이스 기본값
      setEditData({
        id: 0,
        tenant_id: 1, // 현재 테넌트
        site_id: 1, // 현재 사이트
        name: '',
        device_type: 'PLC',
        protocol_type: 'MODBUS_TCP',
        endpoint: '',
        polling_interval: 1000,
        timeout: 3000,
        retry_count: 3,
        is_enabled: true,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        settings: {
          polling_interval_ms: 1000,
          connection_timeout_ms: 10000,
          read_timeout_ms: 5000,
          write_timeout_ms: 5000,
          max_retry_count: 3,
          retry_interval_ms: 5000,
          backoff_time_ms: 60000,
          keep_alive_enabled: true,
          keep_alive_interval_s: 30,
          data_validation_enabled: true,
          performance_monitoring_enabled: true,
          detailed_logging_enabled: false,
          diagnostic_mode_enabled: false,
        },
        status: {
          connection_status: 'disconnected',
          error_count: 0,
          throughput_ops_per_sec: 0,
          total_requests: 0,
          successful_requests: 0,
          failed_requests: 0,
          uptime_percentage: 0,
        }
      });
    }
  }, [isOpen, device, mode]);

  const handleSave = async () => {
    if (!editData || !onSave) return;
    
    setIsLoading(true);
    try {
      await onSave(editData);
      onClose();
    } catch (error) {
      console.error('디바이스 저장 실패:', error);
      alert('디바이스 저장에 실패했습니다.');
    } finally {
      setIsLoading(false);
    }
  };

  const handleDelete = async () => {
    if (!device || !onDelete) return;
    
    if (confirm(`${device.name} 디바이스를 삭제하시겠습니까?`)) {
      setIsLoading(true);
      try {
        await onDelete(device.id);
        onClose();
      } catch (error) {
        console.error('디바이스 삭제 실패:', error);
        alert('디바이스 삭제에 실패했습니다.');
      } finally {
        setIsLoading(false);
      }
    }
  };

  const updateEditData = (field: string, value: any) => {
    setEditData(prev => prev ? { ...prev, [field]: value } : null);
  };

  const updateSettings = (field: string, value: any) => {
    setEditData(prev => prev ? {
      ...prev,
      settings: { ...prev.settings, [field]: value }
    } : null);
  };

  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    return diff < 1 ? '방금 전' : diff < 60 ? `${diff}분 전` : `${Math.floor(diff/60)}시간 전`;
  };

  const getStatusColor = (status?: string) => {
    switch (status) {
      case 'connected': return 'text-success-600';
      case 'disconnected': return 'text-error-600';
      case 'connecting': return 'text-warning-600';
      case 'error': return 'text-error-600';
      default: return 'text-neutral-500';
    }
  };

  if (!isOpen) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-container">
        {/* 모달 헤더 */}
        <div className="modal-header">
          <div className="modal-title">
            <div className="title-row">
              <h2>
                {mode === 'create' ? '새 디바이스 추가' : 
                 mode === 'edit' ? '디바이스 편집' : '디바이스 상세'}
              </h2>
              {device && (
                <span className={`status-indicator ${device.status?.connection_status}`}>
                  <i className="fas fa-circle"></i>
                  {device.status?.connection_status === 'connected' ? '연결됨' :
                   device.status?.connection_status === 'disconnected' ? '연결끊김' :
                   device.status?.connection_status === 'connecting' ? '연결중' : '알수없음'}
                </span>
              )}
            </div>
            {device && (
              <div className="device-subtitle">
                {device.manufacturer} {device.model} • {device.protocol_type}
              </div>
            )}
          </div>
          <button className="close-btn" onClick={onClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 탭 네비게이션 */}
        <div className="tab-navigation">
          <button 
            className={`tab-btn ${activeTab === 'basic' ? 'active' : ''}`}
            onClick={() => setActiveTab('basic')}
          >
            <i className="fas fa-info-circle"></i>
            기본정보
          </button>
          <button 
            className={`tab-btn ${activeTab === 'settings' ? 'active' : ''}`}
            onClick={() => setActiveTab('settings')}
          >
            <i className="fas fa-cogs"></i>
            설정
          </button>
          <button 
            className={`tab-btn ${activeTab === 'status' ? 'active' : ''}`}
            onClick={() => setActiveTab('status')}
          >
            <i className="fas fa-heartbeat"></i>
            상태
          </button>
          <button 
            className={`tab-btn ${activeTab === 'datapoints' ? 'active' : ''}`}
            onClick={() => setActiveTab('datapoints')}
          >
            <i className="fas fa-list"></i>
            데이터포인트
          </button>
          <button 
            className={`tab-btn ${activeTab === 'logs' ? 'active' : ''}`}
            onClick={() => setActiveTab('logs')}
          >
            <i className="fas fa-file-alt"></i>
            로그
          </button>
        </div>

        {/* 탭 컨텐츠 */}
        <div className="tab-content">
          {/* 기본정보 탭 */}
          {activeTab === 'basic' && (
            <div className="tab-panel">
              <div className="form-section">
                <h3>디바이스 정보</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>디바이스명 *</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.name}</div>
                    ) : (
                      <input
                        type="text"
                        value={editData?.name || ''}
                        onChange={(e) => updateEditData('name', e.target.value)}
                        placeholder="예: PLC-001"
                        required
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>디바이스 타입 *</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.device_type}</div>
                    ) : (
                      <select
                        value={editData?.device_type || 'PLC'}
                        onChange={(e) => updateEditData('device_type', e.target.value)}
                      >
                        <option value="PLC">PLC</option>
                        <option value="HMI">HMI</option>
                        <option value="SENSOR">센서</option>
                        <option value="CONTROLLER">컨트롤러</option>
                        <option value="METER">미터</option>
                        <option value="GATEWAY">게이트웨이</option>
                        <option value="ROBOT">로봇</option>
                        <option value="INVERTER">인버터</option>
                      </select>
                    )}
                  </div>

                  <div className="form-group">
                    <label>제조사</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.manufacturer || 'N/A'}</div>
                    ) : (
                      <input
                        type="text"
                        value={editData?.manufacturer || ''}
                        onChange={(e) => updateEditData('manufacturer', e.target.value)}
                        placeholder="예: Siemens"
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>모델</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.model || 'N/A'}</div>
                    ) : (
                      <input
                        type="text"
                        value={editData?.model || ''}
                        onChange={(e) => updateEditData('model', e.target.value)}
                        placeholder="예: S7-1200"
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>시리얼 번호</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.serial_number || 'N/A'}</div>
                    ) : (
                      <input
                        type="text"
                        value={editData?.serial_number || ''}
                        onChange={(e) => updateEditData('serial_number', e.target.value)}
                        placeholder="예: ABC123456"
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>설명</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.description || 'N/A'}</div>
                    ) : (
                      <textarea
                        value={editData?.description || ''}
                        onChange={(e) => updateEditData('description', e.target.value)}
                        placeholder="디바이스 설명을 입력하세요"
                        rows={3}
                      />
                    )}
                  </div>
                </div>
              </div>

              <div className="form-section">
                <h3>통신 설정</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>프로토콜 *</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.protocol_type}</div>
                    ) : (
                      <select
                        value={editData?.protocol_type || 'MODBUS_TCP'}
                        onChange={(e) => updateEditData('protocol_type', e.target.value)}
                      >
                        <option value="MODBUS_TCP">Modbus TCP</option>
                        <option value="MODBUS_RTU">Modbus RTU</option>
                        <option value="BACNET">BACnet</option>
                        <option value="MQTT">MQTT</option>
                        <option value="OPC_UA">OPC UA</option>
                        <option value="ETHERNET_IP">Ethernet/IP</option>
                        <option value="PROFINET">PROFINET</option>
                      </select>
                    )}
                  </div>

                  <div className="form-group">
                    <label>엔드포인트 *</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.endpoint}</div>
                    ) : (
                      <input
                        type="text"
                        value={editData?.endpoint || ''}
                        onChange={(e) => updateEditData('endpoint', e.target.value)}
                        placeholder="예: 192.168.1.100:502"
                        required
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>활성화</label>
                    {mode === 'view' ? (
                      <div className="form-value">
                        <span className={`status-badge ${device?.is_enabled ? 'enabled' : 'disabled'}`}>
                          {device?.is_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    ) : (
                      <label className="checkbox-label">
                        <input
                          type="checkbox"
                          checked={editData?.is_enabled || false}
                          onChange={(e) => updateEditData('is_enabled', e.target.checked)}
                        />
                        <span>디바이스 활성화</span>
                      </label>
                    )}
                  </div>
                </div>
              </div>

              <div className="form-section">
                <h3>메타데이터</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>사이트</label>
                    <div className="form-value">{device?.site_name || 'N/A'}</div>
                  </div>
                  <div className="form-group">
                    <label>그룹</label>
                    <div className="form-value">{device?.group_name || 'N/A'}</div>
                  </div>
                  <div className="form-group">
                    <label>생성일</label>
                    <div className="form-value">{device?.created_at ? new Date(device.created_at).toLocaleString() : 'N/A'}</div>
                  </div>
                  <div className="form-group">
                    <label>수정일</label>
                    <div className="form-value">{device?.updated_at ? new Date(device.updated_at).toLocaleString() : 'N/A'}</div>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* 설정 탭 */}
          {activeTab === 'settings' && (
            <div className="tab-panel">
              <div className="form-section">
                <h3>폴링 및 타이밍</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>폴링 간격 (ms)</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.polling_interval_ms || device?.polling_interval}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.polling_interval_ms || editData?.polling_interval || 1000}
                        onChange={(e) => updateSettings('polling_interval_ms', parseInt(e.target.value))}
                        min={100}
                        max={60000}
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>연결 타임아웃 (ms)</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.connection_timeout_ms || device?.timeout}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.connection_timeout_ms || editData?.timeout || 10000}
                        onChange={(e) => updateSettings('connection_timeout_ms', parseInt(e.target.value))}
                        min={1000}
                        max={60000}
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>읽기 타임아웃 (ms)</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.read_timeout_ms || 5000}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.read_timeout_ms || 5000}
                        onChange={(e) => updateSettings('read_timeout_ms', parseInt(e.target.value))}
                        min={1000}
                        max={30000}
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>쓰기 타임아웃 (ms)</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.write_timeout_ms || 5000}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.write_timeout_ms || 5000}
                        onChange={(e) => updateSettings('write_timeout_ms', parseInt(e.target.value))}
                        min={1000}
                        max={30000}
                      />
                    )}
                  </div>
                </div>
              </div>

              <div className="form-section">
                <h3>재시도 정책</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>최대 재시도 횟수</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.max_retry_count || device?.retry_count}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.max_retry_count || editData?.retry_count || 3}
                        onChange={(e) => updateSettings('max_retry_count', parseInt(e.target.value))}
                        min={0}
                        max={10}
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>재시도 간격 (ms)</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.retry_interval_ms || 5000}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.retry_interval_ms || 5000}
                        onChange={(e) => updateSettings('retry_interval_ms', parseInt(e.target.value))}
                        min={1000}
                        max={60000}
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>백오프 시간 (ms)</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.backoff_time_ms || 60000}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.backoff_time_ms || 60000}
                        onChange={(e) => updateSettings('backoff_time_ms', parseInt(e.target.value))}
                        min={10000}
                        max={300000}
                      />
                    )}
                  </div>
                </div>
              </div>

              <div className="form-section">
                <h3>고급 옵션</h3>
                <div className="form-grid">
                  <div className="form-group">
                    <label>Keep-Alive</label>
                    {mode === 'view' ? (
                      <div className="form-value">
                        <span className={`status-badge ${device?.settings?.keep_alive_enabled ? 'enabled' : 'disabled'}`}>
                          {device?.settings?.keep_alive_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    ) : (
                      <label className="checkbox-label">
                        <input
                          type="checkbox"
                          checked={editData?.settings?.keep_alive_enabled !== false}
                          onChange={(e) => updateSettings('keep_alive_enabled', e.target.checked)}
                        />
                        <span>Keep-Alive 활성화</span>
                      </label>
                    )}
                  </div>

                  <div className="form-group">
                    <label>Keep-Alive 간격 (초)</label>
                    {mode === 'view' ? (
                      <div className="form-value">{device?.settings?.keep_alive_interval_s || 30}</div>
                    ) : (
                      <input
                        type="number"
                        value={editData?.settings?.keep_alive_interval_s || 30}
                        onChange={(e) => updateSettings('keep_alive_interval_s', parseInt(e.target.value))}
                        min={10}
                        max={300}
                      />
                    )}
                  </div>

                  <div className="form-group">
                    <label>데이터 검증</label>
                    {mode === 'view' ? (
                      <div className="form-value">
                        <span className={`status-badge ${device?.settings?.data_validation_enabled ? 'enabled' : 'disabled'}`}>
                          {device?.settings?.data_validation_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    ) : (
                      <label className="checkbox-label">
                        <input
                          type="checkbox"
                          checked={editData?.settings?.data_validation_enabled !== false}
                          onChange={(e) => updateSettings('data_validation_enabled', e.target.checked)}
                        />
                        <span>데이터 검증 활성화</span>
                      </label>
                    )}
                  </div>

                  <div className="form-group">
                    <label>성능 모니터링</label>
                    {mode === 'view' ? (
                      <div className="form-value">
                        <span className={`status-badge ${device?.settings?.performance_monitoring_enabled ? 'enabled' : 'disabled'}`}>
                          {device?.settings?.performance_monitoring_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    ) : (
                      <label className="checkbox-label">
                        <input
                          type="checkbox"
                          checked={editData?.settings?.performance_monitoring_enabled !== false}
                          onChange={(e) => updateSettings('performance_monitoring_enabled', e.target.checked)}
                        />
                        <span>성능 모니터링 활성화</span>
                      </label>
                    )}
                  </div>

                  <div className="form-group">
                    <label>상세 로깅</label>
                    {mode === 'view' ? (
                      <div className="form-value">
                        <span className={`status-badge ${device?.settings?.detailed_logging_enabled ? 'enabled' : 'disabled'}`}>
                          {device?.settings?.detailed_logging_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    ) : (
                      <label className="checkbox-label">
                        <input
                          type="checkbox"
                          checked={editData?.settings?.detailed_logging_enabled || false}
                          onChange={(e) => updateSettings('detailed_logging_enabled', e.target.checked)}
                        />
                        <span>상세 로깅 활성화</span>
                      </label>
                    )}
                  </div>

                  <div className="form-group">
                    <label>진단 모드</label>
                    {mode === 'view' ? (
                      <div className="form-value">
                        <span className={`status-badge ${device?.settings?.diagnostic_mode_enabled ? 'enabled' : 'disabled'}`}>
                          {device?.settings?.diagnostic_mode_enabled ? '활성' : '비활성'}
                        </span>
                      </div>
                    ) : (
                      <label className="checkbox-label">
                        <input
                          type="checkbox"
                          checked={editData?.settings?.diagnostic_mode_enabled || false}
                          onChange={(e) => updateSettings('diagnostic_mode_enabled', e.target.checked)}
                        />
                        <span>진단 모드 활성화</span>
                      </label>
                    )}
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* 상태 탭 */}
          {activeTab === 'status' && (
            <div className="tab-panel">
              <div className="status-grid">
                <div className="status-card">
                  <div className="status-header">
                    <h3>연결 상태</h3>
                    <span className={`status-indicator ${device?.status?.connection_status}`}>
                      <i className="fas fa-circle"></i>
                    </span>
                  </div>
                  <div className="status-content">
                    <div className="status-main">
                      {device?.status?.connection_status === 'connected' ? '연결됨' :
                       device?.status?.connection_status === 'disconnected' ? '연결끊김' :
                       device?.status?.connection_status === 'connecting' ? '연결중' : '알수없음'}
                    </div>
                    <div className="status-detail">
                      마지막 통신: {device?.status?.last_communication ? formatTimeAgo(device.status.last_communication) : 'N/A'}
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <div className="status-header">
                    <h3>응답 시간</h3>
                    <i className="fas fa-clock"></i>
                  </div>
                  <div className="status-content">
                    <div className="status-main">
                      {device?.status?.response_time ? `${device.status.response_time}ms` : 'N/A'}
                    </div>
                    <div className="status-detail">
                      평균 응답 시간
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <div className="status-header">
                    <h3>처리량</h3>
                    <i className="fas fa-tachometer-alt"></i>
                  </div>
                  <div className="status-content">
                    <div className="status-main">
                      {device?.status?.throughput_ops_per_sec?.toFixed(1) || '0.0'} ops/s
                    </div>
                    <div className="status-detail">
                      초당 처리 작업 수
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <div className="status-header">
                    <h3>가동률</h3>
                    <i className="fas fa-chart-line"></i>
                  </div>
                  <div className="status-content">
                    <div className="status-main">
                      {device?.status?.uptime_percentage?.toFixed(1) || '0.0'}%
                    </div>
                    <div className="status-detail">
                      전체 가동률
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <div className="status-header">
                    <h3>요청 통계</h3>
                    <i className="fas fa-chart-bar"></i>
                  </div>
                  <div className="status-content">
                    <div className="request-stats">
                      <div className="stat-row">
                        <span>전체:</span>
                        <span>{device?.status?.total_requests || 0}</span>
                      </div>
                      <div className="stat-row">
                        <span>성공:</span>
                        <span className="text-success">{device?.status?.successful_requests || 0}</span>
                      </div>
                      <div className="stat-row">
                        <span>실패:</span>
                        <span className="text-error">{device?.status?.failed_requests || 0}</span>
                      </div>
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <div className="status-header">
                    <h3>오류 정보</h3>
                    <i className="fas fa-exclamation-triangle"></i>
                  </div>
                  <div className="status-content">
                    <div className="status-main">
                      {device?.status?.error_count || 0}회
                    </div>
                    <div className="status-detail">
                      {device?.status?.last_error || '오류 없음'}
                    </div>
                  </div>
                </div>
              </div>

              {device?.status?.firmware_version && (
                <div className="form-section">
                  <h3>시스템 정보</h3>
                  <div className="form-grid">
                    <div className="form-group">
                      <label>펌웨어 버전</label>
                      <div className="form-value">{device.status.firmware_version}</div>
                    </div>
                    {device.status.cpu_usage && (
                      <div className="form-group">
                        <label>CPU 사용률</label>
                        <div className="form-value">{device.status.cpu_usage.toFixed(1)}%</div>
                      </div>
                    )}
                    {device.status.memory_usage && (
                      <div className="form-group">
                        <label>메모리 사용률</label>
                        <div className="form-value">{device.status.memory_usage.toFixed(1)}%</div>
                      </div>
                    )}
                  </div>
                </div>
              )}
            </div>
          )}

          {/* 데이터포인트 탭 */}
          {activeTab === 'datapoints' && (
            <div className="tab-panel">
              <div className="datapoints-header">
                <h3>데이터 포인트 ({device?.data_points_count || 0}개)</h3>
                <button className="btn btn-primary btn-sm">
                  <i className="fas fa-plus"></i>
                  포인트 추가
                </button>
              </div>
              <div className="datapoints-list">
                <div className="datapoint-item">
                  <div className="datapoint-info">
                    <div className="datapoint-name">Temperature_01</div>
                    <div className="datapoint-details">Address: 40001 • Type: Float • Unit: °C</div>
                  </div>
                  <div className="datapoint-value">
                    <span className="current-value">25.4</span>
                    <span className="value-timestamp">{formatTimeAgo(new Date().toISOString())}</span>
                  </div>
                  <div className="datapoint-actions">
                    <button className="btn btn-sm"><i className="fas fa-edit"></i></button>
                    <button className="btn btn-sm"><i className="fas fa-trash"></i></button>
                  </div>
                </div>
                <div className="datapoint-item">
                  <div className="datapoint-info">
                    <div className="datapoint-name">Pressure_01</div>
                    <div className="datapoint-details">Address: 40002 • Type: Float • Unit: bar</div>
                  </div>
                  <div className="datapoint-value">
                    <span className="current-value">1.2</span>
                    <span className="value-timestamp">{formatTimeAgo(new Date(Date.now() - 30000).toISOString())}</span>
                  </div>
                  <div className="datapoint-actions">
                    <button className="btn btn-sm"><i className="fas fa-edit"></i></button>
                    <button className="btn btn-sm"><i className="fas fa-trash"></i></button>
                  </div>
                </div>
                <div className="empty-message">
                  실제 데이터 포인트 목록은 백엔드 API 연동 후 표시됩니다.
                </div>
              </div>
            </div>
          )}

          {/* 로그 탭 */}
          {activeTab === 'logs' && (
            <div className="tab-panel">
              <div className="logs-header">
                <h3>디바이스 로그</h3>
                <div className="log-filters">
                  <select className="filter-select">
                    <option>전체 레벨</option>
                    <option>ERROR</option>
                    <option>WARN</option>
                    <option>INFO</option>
                    <option>DEBUG</option>
                  </select>
                  <button className="btn btn-sm">
                    <i className="fas fa-download"></i>
                    내보내기
                  </button>
                </div>
              </div>
              <div className="logs-content">
                <div className="log-entry">
                  <div className="log-time">2025-08-14 11:30:25</div>
                  <div className="log-level info">INFO</div>
                  <div className="log-message">Device connected successfully</div>
                </div>
                <div className="log-entry">
                  <div className="log-time">2025-08-14 11:29:45</div>
                  <div className="log-level warn">WARN</div>
                  <div className="log-message">Connection timeout, retrying...</div>
                </div>
                <div className="log-entry">
                  <div className="log-time">2025-08-14 11:29:30</div>
                  <div className="log-level error">ERROR</div>
                  <div className="log-message">Failed to establish connection: Network unreachable</div>
                </div>
                <div className="empty-message">
                  실제 로그 데이터는 백엔드 API 연동 후 표시됩니다.
                </div>
              </div>
            </div>
          )}
        </div>

        {/* 모달 푸터 */}
        <div className="modal-footer">
          <div className="footer-left">
            {mode !== 'create' && mode !== 'view' && onDelete && (
              <button 
                className="btn btn-error"
                onClick={handleDelete}
                disabled={isLoading}
              >
                <i className="fas fa-trash"></i>
                삭제
              </button>
            )}
          </div>
          <div className="footer-right">
            <button className="btn btn-secondary" onClick={onClose}>
              취소
            </button>
            {mode !== 'view' && (
              <button 
                className="btn btn-primary"
                onClick={handleSave}
                disabled={isLoading || !editData?.name}
              >
                {isLoading ? (
                  <>
                    <i className="fas fa-spinner fa-spin"></i>
                    저장 중...
                  </>
                ) : (
                  <>
                    <i className="fas fa-save"></i>
                    {mode === 'create' ? '생성' : '저장'}
                  </>
                )}
              </button>
            )}
          </div>
        </div>
      </div>

      {/* CSS 스타일 */}
      <style dangerouslySetInnerHTML={{__html: `
        .modal-overlay {
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
          padding: 20px;
        }

        .modal-container {
          background: white;
          border-radius: 12px;
          width: 100%;
          max-width: 1200px;
          max-height: 90vh;
          display: flex;
          flex-direction: column;
          box-shadow: 0 25px 50px rgba(0, 0, 0, 0.25);
        }

        .modal-header {
          padding: 24px 32px;
          border-bottom: 1px solid #e2e8f0;
          display: flex;
          justify-content: space-between;
          align-items: flex-start;
        }

        .modal-title {
          flex: 1;
        }

        .title-row {
          display: flex;
          align-items: center;
          gap: 12px;
          margin-bottom: 4px;
        }

        .modal-title h2 {
          margin: 0;
          font-size: 24px;
          font-weight: 700;
          color: #1e293b;
        }

        .device-subtitle {
          font-size: 14px;
          color: #64748b;
        }

        .status-indicator {
          display: flex;
          align-items: center;
          gap: 6px;
          font-size: 12px;
          font-weight: 500;
          padding: 4px 8px;
          border-radius: 16px;
          background: #f8fafc;
        }

        .status-indicator.connected {
          color: #059669;
          background: #ecfdf5;
        }

        .status-indicator.disconnected {
          color: #dc2626;
          background: #fef2f2;
        }

        .status-indicator.connecting {
          color: #d97706;
          background: #fffbeb;
        }

        .close-btn {
          background: none;
          border: none;
          font-size: 20px;
          color: #64748b;
          cursor: pointer;
          padding: 4px;
          border-radius: 6px;
          transition: all 0.2s;
        }

        .close-btn:hover {
          color: #374151;
          background: #f1f5f9;
        }

        .tab-navigation {
          display: flex;
          border-bottom: 1px solid #e2e8f0;
          background: #f8fafc;
        }

        .tab-btn {
          display: flex;
          align-items: center;
          gap: 8px;
          padding: 16px 24px;
          background: none;
          border: none;
          font-size: 14px;
          font-weight: 500;
          color: #64748b;
          cursor: pointer;
          transition: all 0.2s;
          border-bottom: 2px solid transparent;
        }

        .tab-btn:hover {
          color: #374151;
          background: #f1f5f9;
        }

        .tab-btn.active {
          color: #0ea5e9;
          border-bottom-color: #0ea5e9;
          background: white;
        }

        .tab-content {
          flex: 1;
          overflow-y: auto;
          padding: 32px;
        }

        .tab-panel {
          max-height: 100%;
        }

        .form-section {
          margin-bottom: 32px;
        }

        .form-section h3 {
          margin: 0 0 16px 0;
          font-size: 18px;
          font-weight: 600;
          color: #1e293b;
          border-bottom: 1px solid #e2e8f0;
          padding-bottom: 8px;
        }

        .form-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
          gap: 24px;
        }

        .form-group {
          display: flex;
          flex-direction: column;
          gap: 6px;
        }

        .form-group label {
          font-size: 14px;
          font-weight: 500;
          color: #374151;
        }

        .form-value {
          padding: 10px 0;
          font-size: 14px;
          color: #1e293b;
          min-height: 20px;
        }

        .form-group input,
        .form-group select,
        .form-group textarea {
          padding: 10px 12px;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          font-size: 14px;
          transition: border-color 0.2s, box-shadow 0.2s;
        }

        .form-group input:focus,
        .form-group select:focus,
        .form-group textarea:focus {
          outline: none;
          border-color: #0ea5e9;
          box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
        }

        .checkbox-label {
          display: flex;
          align-items: center;
          gap: 8px;
          cursor: pointer;
          margin-top: 4px;
        }

        .checkbox-label input[type="checkbox"] {
          margin: 0;
        }

        .status-badge {
          display: inline-flex;
          align-items: center;
          padding: 4px 8px;
          border-radius: 12px;
          font-size: 12px;
          font-weight: 500;
        }

        .status-badge.enabled {
          background: #dcfce7;
          color: #166534;
        }

        .status-badge.disabled {
          background: #fee2e2;
          color: #991b1b;
        }

        .status-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
          gap: 20px;
          margin-bottom: 32px;
        }

        .status-card {
          background: #f8fafc;
          border: 1px solid #e2e8f0;
          border-radius: 8px;
          padding: 20px;
        }

        .status-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 16px;
        }

        .status-header h3 {
          margin: 0;
          font-size: 16px;
          font-weight: 600;
          color: #374151;
        }

        .status-header i {
          color: #64748b;
          font-size: 18px;
        }

        .status-content {
          text-align: center;
        }

        .status-main {
          font-size: 24px;
          font-weight: 700;
          color: #1e293b;
          margin-bottom: 4px;
        }

        .status-detail {
          font-size: 12px;
          color: #64748b;
        }

        .request-stats {
          display: flex;
          flex-direction: column;
          gap: 8px;
        }

        .stat-row {
          display: flex;
          justify-content: space-between;
          font-size: 14px;
        }

        .text-success {
          color: #059669;
        }

        .text-error {
          color: #dc2626;
        }

        .datapoints-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 20px;
        }

        .datapoints-header h3 {
          margin: 0;
          font-size: 18px;
          font-weight: 600;
          color: #1e293b;
        }

        .datapoints-list {
          border: 1px solid #e2e8f0;
          border-radius: 8px;
          overflow: hidden;
        }

        .datapoint-item {
          display: flex;
          align-items: center;
          padding: 16px;
          border-bottom: 1px solid #e2e8f0;
          gap: 20px;
        }

        .datapoint-item:last-child {
          border-bottom: none;
        }

        .datapoint-info {
          flex: 1;
        }

        .datapoint-name {
          font-size: 14px;
          font-weight: 600;
          color: #1e293b;
          margin-bottom: 4px;
        }

        .datapoint-details {
          font-size: 12px;
          color: #64748b;
        }

        .datapoint-value {
          text-align: right;
          min-width: 100px;
        }

        .current-value {
          display: block;
          font-size: 16px;
          font-weight: 600;
          color: #059669;
          margin-bottom: 2px;
        }

        .value-timestamp {
          font-size: 11px;
          color: #9ca3af;
        }

        .datapoint-actions {
          display: flex;
          gap: 8px;
        }

        .logs-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 20px;
        }

        .logs-header h3 {
          margin: 0;
          font-size: 18px;
          font-weight: 600;
          color: #1e293b;
        }

        .log-filters {
          display: flex;
          gap: 12px;
          align-items: center;
        }

        .filter-select {
          padding: 6px 10px;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          font-size: 13px;
          background: white;
        }

        .logs-content {
          border: 1px solid #e2e8f0;
          border-radius: 8px;
          max-height: 400px;
          overflow-y: auto;
        }

        .log-entry {
          display: flex;
          align-items: center;
          padding: 12px 16px;
          border-bottom: 1px solid #f1f5f9;
          font-family: 'Courier New', monospace;
          font-size: 13px;
          gap: 16px;
        }

        .log-entry:last-child {
          border-bottom: none;
        }

        .log-time {
          color: #6b7280;
          min-width: 150px;
        }

        .log-level {
          min-width: 60px;
          padding: 2px 6px;
          border-radius: 4px;
          font-size: 11px;
          font-weight: 600;
          text-align: center;
        }

        .log-level.info {
          background: #dbeafe;
          color: #1d4ed8;
        }

        .log-level.warn {
          background: #fef3c7;
          color: #92400e;
        }

        .log-level.error {
          background: #fee2e2;
          color: #991b1b;
        }

        .log-message {
          flex: 1;
          color: #374151;
        }

        .empty-message {
          padding: 40px;
          text-align: center;
          color: #9ca3af;
          font-style: italic;
        }

        .modal-footer {
          padding: 24px 32px;
          border-top: 1px solid #e2e8f0;
          display: flex;
          justify-content: space-between;
          align-items: center;
        }

        .footer-left,
        .footer-right {
          display: flex;
          gap: 12px;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 8px;
          padding: 10px 16px;
          border: 1px solid transparent;
          border-radius: 6px;
          font-size: 14px;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
          text-decoration: none;
        }

        .btn-sm {
          padding: 6px 12px;
          font-size: 13px;
        }

        .btn-primary {
          background: #0ea5e9;
          color: white;
        }

        .btn-primary:hover:not(:disabled) {
          background: #0284c7;
        }

        .btn-secondary {
          background: white;
          color: #374151;
          border-color: #d1d5db;
        }

        .btn-secondary:hover {
          background: #f9fafb;
        }

        .btn-error {
          background: #dc2626;
          color: white;
        }

        .btn-error:hover:not(:disabled) {
          background: #b91c1c;
        }

        .btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        @media (max-width: 768px) {
          .modal-container {
            max-width: 100%;
            margin: 10px;
            max-height: calc(100vh - 20px);
          }

          .tab-navigation {
            overflow-x: auto;
          }

          .tab-btn {
            white-space: nowrap;
            min-width: auto;
          }

          .form-grid {
            grid-template-columns: 1fr;
          }

          .status-grid {
            grid-template-columns: 1fr;
          }

          .datapoint-item {
            flex-direction: column;
            align-items: flex-start;
            gap: 12px;
          }

          .datapoint-value {
            align-self: flex-end;
          }
        }
      `}} />
    </div>
  );
};

export default DeviceDetailModal;