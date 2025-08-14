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
  data_points?: DataPoint[];
  data_points_count?: number;
}

interface DataPoint {
  id: number;
  name: string;
  description?: string;
  data_type: string;
  address: string;
  current_value?: any;
  unit?: string;
  scale_factor?: number;
  is_enabled: boolean;
  last_read?: string;
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
  const [detailLoading, setDetailLoading] = useState(false);
  const [detailData, setDetailData] = useState<Device | null>(null);
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);

  // 모달이 열릴 때 상세 정보 로드
  useEffect(() => {
    if (isOpen && device && mode !== 'create') {
      loadDeviceDetails(device.id);
      setActiveTab('basic'); // 항상 기본정보 탭으로 시작
    } else if (mode === 'create') {
      initializeNewDevice();
      setActiveTab('basic'); // 생성 모드에서는 항상 기본정보 탭으로 시작
    }
  }, [isOpen, device, mode]);

  // 편집 데이터 초기화
  useEffect(() => {
    if (detailData) {
      setEditData({ ...detailData });
    }
  }, [detailData]);

  // 새 디바이스 초기화
  const initializeNewDevice = () => {
    setDetailData(null);
    setDataPoints([]);
    setEditData({
      id: 0,
      tenant_id: 1, // 기본값
      site_id: 1, // 기본값
      name: '',
      description: '',
      device_type: 'PLC',
      manufacturer: '',
      model: '',
      serial_number: '',
      protocol_type: 'MODBUS_TCP',
      endpoint: '',
      config: {},
      polling_interval: 5000,
      timeout: 10000,
      retry_count: 3,
      is_enabled: true,
      created_at: new Date().toISOString(),
      updated_at: new Date().toISOString(),
      settings: {
        polling_interval_ms: 5000,
        connection_timeout_ms: 10000,
        read_timeout_ms: 5000,
        write_timeout_ms: 5000,
        max_retry_count: 3,
        retry_interval_ms: 1000,
        backoff_time_ms: 2000,
        keep_alive_enabled: true,
        keep_alive_interval_s: 30,
        data_validation_enabled: true,
        performance_monitoring_enabled: true,
        detailed_logging_enabled: false,
        diagnostic_mode_enabled: false,
      }
    });
  };

  // 디바이스 상세 정보 로드
  const loadDeviceDetails = async (deviceId: number) => {
    try {
      setDetailLoading(true);
      console.log('🔍 디바이스 상세 정보 로드:', deviceId);

      // 디바이스 기본 정보 조회
      const deviceResponse = await fetch(`http://localhost:3000/api/devices/${deviceId}`);
      if (!deviceResponse.ok) {
        throw new Error(`디바이스 조회 실패: ${deviceResponse.status}`);
      }
      const deviceResult = await deviceResponse.json();
      
      if (!deviceResult.success) {
        throw new Error(deviceResult.error || '디바이스 조회 실패');
      }

      console.log('📋 디바이스 상세 데이터:', deviceResult.data);
      setDetailData(deviceResult.data);

      // 디바이스 설정 정보 조회
      try {
        const settingsResponse = await fetch(`http://localhost:3000/api/devices/${deviceId}/settings`);
        if (settingsResponse.ok) {
          const settingsResult = await settingsResponse.json();
          if (settingsResult.success) {
            setDetailData(prev => prev ? { ...prev, settings: settingsResult.data } : null);
          }
        }
      } catch (settingsError) {
        console.warn('⚠️ 디바이스 설정 조회 실패:', settingsError);
      }

      // 데이터 포인트 조회
      try {
        const dataPointsResponse = await fetch(`http://localhost:3000/api/devices/${deviceId}/data-points`);
        if (dataPointsResponse.ok) {
          const dataPointsResult = await dataPointsResponse.json();
          if (dataPointsResult.success && Array.isArray(dataPointsResult.data)) {
            setDataPoints(dataPointsResult.data);
          }
        }
      } catch (dpError) {
        console.warn('⚠️ 데이터 포인트 조회 실패:', dpError);
      }

    } catch (error) {
      console.error('❌ 디바이스 상세 정보 로드 실패:', error);
      alert(`디바이스 정보를 불러오는데 실패했습니다: ${error.message}`);
    } finally {
      setDetailLoading(false);
    }
  };

  // 디바이스 저장
  const handleSave = async () => {
    if (!editData) return;

    try {
      setIsLoading(true);
      console.log('💾 디바이스 저장:', editData);

      const url = mode === 'create' 
        ? 'http://localhost:3000/api/devices'
        : `http://localhost:3000/api/devices/${editData.id}`;
      
      const method = mode === 'create' ? 'POST' : 'PUT';

      const response = await fetch(url, {
        method,
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(editData),
      });

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`저장 실패 (${response.status}): ${errorText}`);
      }

      const result = await response.json();
      if (!result.success) {
        throw new Error(result.error || '저장 실패');
      }

      console.log('✅ 디바이스 저장 성공:', result.data);
      
      // 설정도 별도로 저장 (편집 모드인 경우)
      if (mode === 'edit' && editData.settings) {
        try {
          const settingsResponse = await fetch(`http://localhost:3000/api/devices/${editData.id}/settings`, {
            method: 'PUT',
            headers: {
              'Content-Type': 'application/json',
            },
            body: JSON.stringify(editData.settings),
          });

          if (settingsResponse.ok) {
            const settingsResult = await settingsResponse.json();
            console.log('✅ 디바이스 설정 저장 성공:', settingsResult);
          }
        } catch (settingsError) {
          console.warn('⚠️ 설정 저장은 실패했지만 기본 정보는 저장됨:', settingsError);
        }
      }

      if (onSave) {
        onSave(result.data);
      }
      onClose();

    } catch (error) {
      console.error('❌ 디바이스 저장 실패:', error);
      alert(`저장에 실패했습니다: ${error.message}`);
    } finally {
      setIsLoading(false);
    }
  };

  // 디바이스 삭제
  const handleDelete = async () => {
    if (!device || !onDelete) return;
    
    if (confirm(`${device.name} 디바이스를 삭제하시겠습니까?`)) {
      setIsLoading(true);
      try {
        const response = await fetch(`http://localhost:3000/api/devices/${device.id}`, {
          method: 'DELETE',
        });

        if (!response.ok) {
          const errorText = await response.text();
          throw new Error(`삭제 실패 (${response.status}): ${errorText}`);
        }

        const result = await response.json();
        if (!result.success) {
          throw new Error(result.error || '삭제 실패');
        }

        console.log('✅ 디바이스 삭제 성공');
        await onDelete(device.id);
        onClose();
      } catch (error) {
        console.error('❌ 디바이스 삭제 실패:', error);
        alert(`삭제에 실패했습니다: ${error.message}`);
      } finally {
        setIsLoading(false);
      }
    }
  };

  // 필드 업데이트 함수들
  const updateEditData = (field: string, value: any) => {
    setEditData(prev => prev ? { ...prev, [field]: value } : null);
  };

  const updateSettings = (field: string, value: any) => {
    setEditData(prev => prev ? {
      ...prev,
      settings: { ...prev.settings, [field]: value }
    } : null);
  };

  // 시간 포맷 함수
  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    return diff < 1 ? '방금 전' : diff < 60 ? `${diff}분 전` : `${Math.floor(diff/60)}시간 전`;
  };

  // 상태 색상 함수
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

  // 표시할 데이터 결정 (상세 데이터 우선, 없으면 기본 device 데이터)
  const displayData = detailData || device || editData;

  if (detailLoading) {
    return (
      <div className="modal-overlay">
        <div className="modal-container">
          <div className="loading-container">
            <div className="loading-spinner"></div>
            <p>디바이스 정보를 불러오는 중...</p>
          </div>
        </div>
      </div>
    );
  }

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
              {displayData && displayData.status && (
                <span className={`status-indicator ${displayData.status.connection_status}`}>
                  <i className="fas fa-circle"></i>
                  {displayData.status.connection_status === 'connected' ? '연결됨' :
                   displayData.status.connection_status === 'disconnected' ? '연결끊김' :
                   displayData.status.connection_status === 'connecting' ? '연결중' : '알수없음'}
                </span>
              )}
            </div>
            {displayData && (
              <div className="device-subtitle">
                {displayData.manufacturer} {displayData.model} • {displayData.protocol_type}
              </div>
            )}
          </div>
          <button className="close-btn" onClick={onClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 탭 네비게이션 - 수정된 부분 */}
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
            <i className="fas fa-cog"></i>
            설정
          </button>
          <button 
            className={`tab-btn ${activeTab === 'datapoints' ? 'active' : ''}`}
            onClick={() => setActiveTab('datapoints')}
          >
            <i className="fas fa-list"></i>
            데이터포인트 ({dataPoints.length})
          </button>
          {/* 상태 탭은 생성 모드에서만 숨김 */}
          {mode !== 'create' && (
            <button 
              className={`tab-btn ${activeTab === 'status' ? 'active' : ''}`}
              onClick={() => setActiveTab('status')}
            >
              <i className="fas fa-chart-line"></i>
              상태
            </button>
          )}
          {/* 로그 탭은 보기 모드에서만 표시 */}
          {mode === 'view' && (
            <button 
              className={`tab-btn ${activeTab === 'logs' ? 'active' : ''}`}
              onClick={() => setActiveTab('logs')}
            >
              <i className="fas fa-file-alt"></i>
              로그
            </button>
          )}
        </div>

        {/* 탭 내용 - 고정 높이 적용 */}
        <div className="modal-content">
          {/* 기본정보 탭 */}
          {activeTab === 'basic' && (
            <div className="tab-panel">
              <div className="form-grid">
                <div className="form-group">
                  <label>디바이스명 *</label>
                  {mode === 'view' ? (
                    <div className="form-value">{displayData?.name || 'N/A'}</div>
                  ) : (
                    <input
                      type="text"
                      value={editData?.name || ''}
                      onChange={(e) => updateEditData('name', e.target.value)}
                      placeholder="디바이스명을 입력하세요"
                      required
                    />
                  )}
                </div>

                <div className="form-group">
                  <label>디바이스 타입</label>
                  {mode === 'view' ? (
                    <div className="form-value">{displayData?.device_type || 'N/A'}</div>
                  ) : (
                    <select
                      value={editData?.device_type || 'PLC'}
                      onChange={(e) => updateEditData('device_type', e.target.value)}
                    >
                      <option value="PLC">PLC</option>
                      <option value="HMI">HMI</option>
                      <option value="SENSOR">센서</option>
                      <option value="ACTUATOR">액추에이터</option>
                      <option value="METER">미터</option>
                      <option value="CONTROLLER">컨트롤러</option>
                      <option value="GATEWAY">게이트웨이</option>
                      <option value="OTHER">기타</option>
                    </select>
                  )}
                </div>

                <div className="form-group">
                  <label>프로토콜 *</label>
                  {mode === 'view' ? (
                    <div className="form-value">{displayData?.protocol_type || 'N/A'}</div>
                  ) : (
                    <select
                      value={editData?.protocol_type || 'MODBUS_TCP'}
                      onChange={(e) => updateEditData('protocol_type', e.target.value)}
                    >
                      <option value="MODBUS_TCP">Modbus TCP</option>
                      <option value="MODBUS_RTU">Modbus RTU</option>
                      <option value="MQTT">MQTT</option>
                      <option value="BACNET">BACnet</option>
                      <option value="OPCUA">OPC UA</option>
                      <option value="ETHERNET_IP">Ethernet/IP</option>
                      <option value="PROFINET">PROFINET</option>
                      <option value="HTTP_REST">HTTP REST</option>
                    </select>
                  )}
                </div>

                <div className="form-group">
                  <label>엔드포인트 *</label>
                  {mode === 'view' ? (
                    <div className="form-value endpoint">{displayData?.endpoint || 'N/A'}</div>
                  ) : (
                    <input
                      type="text"
                      value={editData?.endpoint || ''}
                      onChange={(e) => updateEditData('endpoint', e.target.value)}
                      placeholder="192.168.1.100:502 또는 mqtt://broker.example.com"
                      required
                    />
                  )}
                </div>

                <div className="form-group">
                  <label>제조사</label>
                  {mode === 'view' ? (
                    <div className="form-value">{displayData?.manufacturer || 'N/A'}</div>
                  ) : (
                    <input
                      type="text"
                      value={editData?.manufacturer || ''}
                      onChange={(e) => updateEditData('manufacturer', e.target.value)}
                      placeholder="제조사명"
                    />
                  )}
                </div>

                <div className="form-group">
                  <label>모델명</label>
                  {mode === 'view' ? (
                    <div className="form-value">{displayData?.model || 'N/A'}</div>
                  ) : (
                    <input
                      type="text"
                      value={editData?.model || ''}
                      onChange={(e) => updateEditData('model', e.target.value)}
                      placeholder="모델명"
                    />
                  )}
                </div>

                <div className="form-group">
                  <label>시리얼 번호</label>
                  {mode === 'view' ? (
                    <div className="form-value">{displayData?.serial_number || 'N/A'}</div>
                  ) : (
                    <input
                      type="text"
                      value={editData?.serial_number || ''}
                      onChange={(e) => updateEditData('serial_number', e.target.value)}
                      placeholder="시리얼 번호"
                    />
                  )}
                </div>

                <div className="form-group">
                  <label>활성화 상태</label>
                  {mode === 'view' ? (
                    <div className="form-value">
                      <span className={`status-badge ${displayData?.is_enabled ? 'enabled' : 'disabled'}`}>
                        {displayData?.is_enabled ? '활성화' : '비활성화'}
                      </span>
                    </div>
                  ) : (
                    <label className="switch">
                      <input
                        type="checkbox"
                        checked={editData?.is_enabled || false}
                        onChange={(e) => updateEditData('is_enabled', e.target.checked)}
                      />
                      <span className="slider"></span>
                    </label>
                  )}
                </div>

                <div className="form-group full-width">
                  <label>설명</label>
                  {mode === 'view' ? (
                    <div className="form-value">{displayData?.description || '설명이 없습니다.'}</div>
                  ) : (
                    <textarea
                      value={editData?.description || ''}
                      onChange={(e) => updateEditData('description', e.target.value)}
                      placeholder="디바이스에 대한 설명을 입력하세요"
                      rows={3}
                    />
                  )}
                </div>
              </div>
            </div>
          )}

          {/* 설정 탭 */}
          {activeTab === 'settings' && (
            <div className="tab-panel">
              <div className="settings-sections">
                <div className="setting-section">
                  <h3>통신 설정</h3>
                  <div className="form-grid">
                    <div className="form-group">
                      <label>폴링 주기 (ms)</label>
                      {mode === 'view' ? (
                        <div className="form-value">{displayData?.settings?.polling_interval_ms || displayData?.polling_interval || 'N/A'}</div>
                      ) : (
                        <input
                          type="number"
                          min="100"
                          max="3600000"
                          value={editData?.settings?.polling_interval_ms || editData?.polling_interval || 5000}
                          onChange={(e) => updateSettings('polling_interval_ms', parseInt(e.target.value))}
                        />
                      )}
                    </div>
                    <div className="form-group">
                      <label>연결 타임아웃 (ms)</label>
                      {mode === 'view' ? (
                        <div className="form-value">{displayData?.settings?.connection_timeout_ms || displayData?.timeout || 'N/A'}</div>
                      ) : (
                        <input
                          type="number"
                          min="1000"
                          max="60000"
                          value={editData?.settings?.connection_timeout_ms || editData?.timeout || 10000}
                          onChange={(e) => updateSettings('connection_timeout_ms', parseInt(e.target.value))}
                        />
                      )}
                    </div>
                    <div className="form-group">
                      <label>읽기 타임아웃 (ms)</label>
                      {mode === 'view' ? (
                        <div className="form-value">{displayData?.settings?.read_timeout_ms || 5000}</div>
                      ) : (
                        <input
                          type="number"
                          min="1000"
                          max="30000"
                          value={editData?.settings?.read_timeout_ms || 5000}
                          onChange={(e) => updateSettings('read_timeout_ms', parseInt(e.target.value))}
                        />
                      )}
                    </div>
                    <div className="form-group">
                      <label>최대 재시도 횟수</label>
                      {mode === 'view' ? (
                        <div className="form-value">{displayData?.settings?.max_retry_count || displayData?.retry_count || 'N/A'}</div>
                      ) : (
                        <input
                          type="number"
                          min="0"
                          max="10"
                          value={editData?.settings?.max_retry_count || editData?.retry_count || 3}
                          onChange={(e) => updateSettings('max_retry_count', parseInt(e.target.value))}
                        />
                      )}
                    </div>
                  </div>
                </div>

                <div className="setting-section">
                  <h3>고급 옵션</h3>
                  <div className="form-grid">
                    <div className="form-group">
                      <label>Keep-Alive 활성화</label>
                      {mode === 'view' ? (
                        <div className="form-value">
                          <span className={`status-badge ${displayData?.settings?.keep_alive_enabled ? 'enabled' : 'disabled'}`}>
                            {displayData?.settings?.keep_alive_enabled ? '활성화' : '비활성화'}
                          </span>
                        </div>
                      ) : (
                        <label className="switch">
                          <input
                            type="checkbox"
                            checked={editData?.settings?.keep_alive_enabled || false}
                            onChange={(e) => updateSettings('keep_alive_enabled', e.target.checked)}
                          />
                          <span className="slider"></span>
                        </label>
                      )}
                    </div>
                    <div className="form-group">
                      <label>성능 모니터링</label>
                      {mode === 'view' ? (
                        <div className="form-value">
                          <span className={`status-badge ${displayData?.settings?.performance_monitoring_enabled ? 'enabled' : 'disabled'}`}>
                            {displayData?.settings?.performance_monitoring_enabled ? '활성화' : '비활성화'}
                          </span>
                        </div>
                      ) : (
                        <label className="switch">
                          <input
                            type="checkbox"
                            checked={editData?.settings?.performance_monitoring_enabled || false}
                            onChange={(e) => updateSettings('performance_monitoring_enabled', e.target.checked)}
                          />
                          <span className="slider"></span>
                        </label>
                      )}
                    </div>
                    <div className="form-group">
                      <label>상세 로깅</label>
                      {mode === 'view' ? (
                        <div className="form-value">
                          <span className={`status-badge ${displayData?.settings?.detailed_logging_enabled ? 'enabled' : 'disabled'}`}>
                            {displayData?.settings?.detailed_logging_enabled ? '활성화' : '비활성화'}
                          </span>
                        </div>
                      ) : (
                        <label className="switch">
                          <input
                            type="checkbox"
                            checked={editData?.settings?.detailed_logging_enabled || false}
                            onChange={(e) => updateSettings('detailed_logging_enabled', e.target.checked)}
                          />
                          <span className="slider"></span>
                        </label>
                      )}
                    </div>
                    <div className="form-group">
                      <label>진단 모드</label>
                      {mode === 'view' ? (
                        <div className="form-value">
                          <span className={`status-badge ${displayData?.settings?.diagnostic_mode_enabled ? 'enabled' : 'disabled'}`}>
                            {displayData?.settings?.diagnostic_mode_enabled ? '활성화' : '비활성화'}
                          </span>
                        </div>
                      ) : (
                        <label className="switch">
                          <input
                            type="checkbox"
                            checked={editData?.settings?.diagnostic_mode_enabled || false}
                            onChange={(e) => updateSettings('diagnostic_mode_enabled', e.target.checked)}
                          />
                          <span className="slider"></span>
                        </label>
                      )}
                    </div>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* 데이터포인트 탭 */}
          {activeTab === 'datapoints' && (
            <div className="tab-panel">
              <div className="datapoints-header">
                <h3>데이터 포인트 목록</h3>
                <div className="datapoints-actions">
                  {mode !== 'create' && (
                    <button className="btn btn-sm btn-secondary">
                      <i className="fas fa-sync"></i>
                      새로고침
                    </button>
                  )}
                  <button className="btn btn-sm btn-primary">
                    <i className="fas fa-plus"></i>
                    포인트 추가
                  </button>
                </div>
              </div>
              <div className="datapoints-table">
                {dataPoints.length > 0 ? (
                  <table>
                    <thead>
                      <tr>
                        <th>이름</th>
                        <th>주소</th>
                        <th>타입</th>
                        {mode !== 'create' && <th>현재값</th>}
                        <th>단위</th>
                        <th>상태</th>
                        {mode !== 'create' && <th>마지막 읽기</th>}
                        <th>액션</th>
                      </tr>
                    </thead>
                    <tbody>
                      {dataPoints.map((point) => (
                        <tr key={point.id}>
                          <td className="point-name">{point.name}</td>
                          <td className="point-address">{point.address}</td>
                          <td>{point.data_type}</td>
                          {mode !== 'create' && (
                            <td className="point-value">
                              {point.current_value !== null && point.current_value !== undefined 
                                ? point.current_value 
                                : 'N/A'}
                            </td>
                          )}
                          <td>{point.unit || '-'}</td>
                          <td>
                            <span className={`status-badge ${point.is_enabled ? 'enabled' : 'disabled'}`}>
                              {point.is_enabled ? '활성' : '비활성'}
                            </span>
                          </td>
                          {mode !== 'create' && (
                            <td>{formatTimeAgo(point.last_read)}</td>
                          )}
                          <td>
                            <div className="point-actions">
                              <button className="btn btn-xs btn-info" title="상세">
                                <i className="fas fa-eye"></i>
                              </button>
                              <button className="btn btn-xs btn-secondary" title="편집">
                                <i className="fas fa-edit"></i>
                              </button>
                              {mode !== 'view' && (
                                <button className="btn btn-xs btn-error" title="삭제">
                                  <i className="fas fa-trash"></i>
                                </button>
                              )}
                            </div>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                ) : (
                  <div className="empty-message">
                    <i className="fas fa-list"></i>
                    <p>
                      {mode === 'create' 
                        ? '새 디바이스에 데이터 포인트를 추가하세요.' 
                        : '등록된 데이터 포인트가 없습니다.'
                      }
                    </p>
                    <button className="btn btn-primary">
                      <i className="fas fa-plus"></i>
                      {mode === 'create' ? '데이터 포인트 추가' : '첫 번째 포인트 추가'}
                    </button>
                  </div>
                )}
              </div>
            </div>
          )}

          {/* 상태 탭 - 생성 모드에서는 표시되지 않음 */}
          {activeTab === 'status' && mode !== 'create' && (
            <div className="tab-panel">
              <div className="status-grid">
                <div className="status-card">
                  <h4>연결 상태</h4>
                  <div className="status-details">
                    <div className="status-item">
                      <span className="label">현재 상태:</span>
                      <span className={`value ${getStatusColor(displayData?.status?.connection_status)}`}>
                        {displayData?.status?.connection_status === 'connected' ? '연결됨' :
                         displayData?.status?.connection_status === 'disconnected' ? '연결끊김' :
                         displayData?.status?.connection_status === 'connecting' ? '연결중' : '알수없음'}
                      </span>
                    </div>
                    <div className="status-item">
                      <span className="label">마지막 통신:</span>
                      <span className="value">{formatTimeAgo(displayData?.status?.last_communication)}</span>
                    </div>
                    <div className="status-item">
                      <span className="label">응답시간:</span>
                      <span className="value">{displayData?.status?.response_time || 0}ms</span>
                    </div>
                    <div className="status-item">
                      <span className="label">가동률:</span>
                      <span className="value">{displayData?.status?.uptime_percentage || 0}%</span>
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <h4>통신 통계</h4>
                  <div className="status-details">
                    <div className="status-item">
                      <span className="label">총 요청:</span>
                      <span className="value">{displayData?.status?.total_requests || 0}</span>
                    </div>
                    <div className="status-item">
                      <span className="label">성공:</span>
                      <span className="value text-success-600">{displayData?.status?.successful_requests || 0}</span>
                    </div>
                    <div className="status-item">
                      <span className="label">실패:</span>
                      <span className="value text-error-600">{displayData?.status?.failed_requests || 0}</span>
                    </div>
                    <div className="status-item">
                      <span className="label">처리량:</span>
                      <span className="value">{displayData?.status?.throughput_ops_per_sec || 0} ops/sec</span>
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <h4>오류 정보</h4>
                  <div className="status-details">
                    <div className="status-item">
                      <span className="label">오류 횟수:</span>
                      <span className={`value ${(displayData?.status?.error_count || 0) > 0 ? 'text-error-600' : ''}`}>
                        {displayData?.status?.error_count || 0}
                      </span>
                    </div>
                    <div className="status-item full-width">
                      <span className="label">마지막 오류:</span>
                      <span className="value error-message">
                        {displayData?.status?.last_error || '오류 없음'}
                      </span>
                    </div>
                  </div>
                </div>

                <div className="status-card">
                  <h4>하드웨어 정보</h4>
                  <div className="status-details">
                    <div className="status-item">
                      <span className="label">펌웨어 버전:</span>
                      <span className="value">{displayData?.status?.firmware_version || 'N/A'}</span>
                    </div>
                    <div className="status-item">
                      <span className="label">CPU 사용률:</span>
                      <span className="value">{displayData?.status?.cpu_usage || 0}%</span>
                    </div>
                    <div className="status-item">
                      <span className="label">메모리 사용률:</span>
                      <span className="value">{displayData?.status?.memory_usage || 0}%</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* 로그 탭 - 보기 모드에서만 표시 */}
          {activeTab === 'logs' && mode === 'view' && (
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
                <div className="empty-message">
                  <i className="fas fa-file-alt"></i>
                  <p>로그 데이터는 백엔드 API 연동 후 표시됩니다.</p>
                  <p>실제 로그 조회 API: <code>GET /api/devices/{device.id}/logs</code></p>
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

        <style jsx>{`
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
            padding: 2rem;
          }

          .modal-container {
            background: white;
            border-radius: 12px;
            width: 100%;
            max-width: 900px;
            height: 85vh;
            max-height: 85vh;
            overflow: hidden;
            box-shadow: 0 25px 50px rgba(0, 0, 0, 0.25);
            display: flex;
            flex-direction: column;
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
            to { transform: rotate(360deg); }
          }

          .modal-header {
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
            padding: 1.5rem 2rem 1rem 2rem;
            border-bottom: 1px solid #e5e7eb;
            flex-shrink: 0;
          }

          .modal-title .title-row {
            display: flex;
            align-items: center;
            gap: 1rem;
            margin-bottom: 0.5rem;
          }

          .modal-title h2 {
            margin: 0;
            font-size: 1.75rem;
            font-weight: 600;
            color: #1f2937;
          }

          .device-subtitle {
            font-size: 0.875rem;
            color: #6b7280;
          }

          .status-indicator {
            display: inline-flex;
            align-items: center;
            gap: 0.375rem;
            padding: 0.25rem 0.75rem;
            border-radius: 9999px;
            font-size: 0.75rem;
            font-weight: 500;
          }

          .status-indicator.connected {
            background: #dcfce7;
            color: #166534;
          }

          .status-indicator.disconnected {
            background: #fee2e2;
            color: #991b1b;
          }

          .status-indicator.connecting {
            background: #fef3c7;
            color: #92400e;
          }

          .status-indicator i {
            font-size: 0.5rem;
          }

          .close-btn {
            background: none;
            border: none;
            font-size: 1.5rem;
            color: #6b7280;
            cursor: pointer;
            padding: 0.5rem;
            border-radius: 0.375rem;
            transition: all 0.2s ease;
          }

          .close-btn:hover {
            background: #f3f4f6;
            color: #374151;
          }

          .tab-navigation {
            display: flex;
            border-bottom: 1px solid #e5e7eb;
            background: #f9fafb;
            flex-shrink: 0;
          }

          .tab-btn {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            padding: 1rem 1.5rem;
            border: none;
            background: none;
            color: #6b7280;
            font-size: 0.875rem;
            font-weight: 500;
            cursor: pointer;
            border-bottom: 3px solid transparent;
            transition: all 0.2s ease;
          }

          .tab-btn:hover {
            color: #374151;
            background: #f3f4f6;
          }

          .tab-btn.active {
            color: #0ea5e9;
            border-bottom-color: #0ea5e9;
            background: white;
          }

          .modal-content {
            flex: 1;
            overflow: hidden;
            display: flex;
            flex-direction: column;
          }

          .tab-panel {
            flex: 1;
            overflow-y: auto;
            padding: 2rem;
          }

          .form-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 1.5rem;
          }

          .form-group {
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
          }

          .form-group.full-width {
            grid-column: 1 / -1;
          }

          .form-group label {
            font-size: 0.875rem;
            font-weight: 500;
            color: #374151;
          }

          .form-group input,
          .form-group select,
          .form-group textarea {
            padding: 0.75rem;
            border: 1px solid #d1d5db;
            border-radius: 0.5rem;
            font-size: 0.875rem;
            transition: border-color 0.2s ease;
          }

          .form-group input:focus,
          .form-group select:focus,
          .form-group textarea:focus {
            outline: none;
            border-color: #0ea5e9;
            box-shadow: 0 0 0 3px rgba(14, 165, 233, 0.1);
          }

          .form-value {
            padding: 0.75rem;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 0.5rem;
            font-size: 0.875rem;
            color: #374151;
          }

          .form-value.endpoint {
            font-family: 'Courier New', monospace;
            background: #f0f9ff;
            border-color: #0ea5e9;
            color: #0c4a6e;
          }

          .status-badge {
            display: inline-flex;
            align-items: center;
            gap: 0.25rem;
            padding: 0.25rem 0.75rem;
            border-radius: 9999px;
            font-size: 0.75rem;
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

          .switch {
            position: relative;
            display: inline-block;
            width: 3rem;
            height: 1.5rem;
          }

          .switch input {
            opacity: 0;
            width: 0;
            height: 0;
          }

          .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: #cbd5e1;
            transition: 0.2s;
            border-radius: 1.5rem;
          }

          .slider:before {
            position: absolute;
            content: "";
            height: 1.125rem;
            width: 1.125rem;
            left: 0.1875rem;
            bottom: 0.1875rem;
            background: white;
            transition: 0.2s;
            border-radius: 50%;
          }

          input:checked + .slider {
            background: #0ea5e9;
          }

          input:checked + .slider:before {
            transform: translateX(1.5rem);
          }

          .settings-sections {
            display: flex;
            flex-direction: column;
            gap: 2rem;
          }

          .setting-section {
            border: 1px solid #e5e7eb;
            border-radius: 0.75rem;
            padding: 1.5rem;
          }

          .setting-section h3 {
            margin: 0 0 1rem 0;
            font-size: 1.125rem;
            font-weight: 600;
            color: #1f2937;
          }

          .datapoints-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1.5rem;
          }

          .datapoints-header h3 {
            margin: 0;
            font-size: 1.125rem;
            font-weight: 600;
            color: #1f2937;
          }

          .datapoints-actions {
            display: flex;
            gap: 0.5rem;
          }

          .datapoints-table {
            border: 1px solid #e5e7eb;
            border-radius: 0.5rem;
            overflow: hidden;
          }

          .datapoints-table table {
            width: 100%;
            border-collapse: collapse;
          }

          .datapoints-table th {
            background: #f9fafb;
            padding: 0.75rem;
            text-align: left;
            font-size: 0.75rem;
            font-weight: 600;
            color: #374151;
            border-bottom: 1px solid #e5e7eb;
          }

          .datapoints-table td {
            padding: 0.75rem;
            border-bottom: 1px solid #f3f4f6;
            font-size: 0.875rem;
          }

          .datapoints-table tr:last-child td {
            border-bottom: none;
          }

          .point-name {
            font-weight: 500;
            color: #1f2937;
          }

          .point-address {
            font-family: 'Courier New', monospace;
            background: #f0f9ff;
            padding: 0.25rem 0.5rem;
            border-radius: 0.25rem;
            color: #0c4a6e;
          }

          .point-value {
            font-weight: 500;
            color: #059669;
          }

          .point-actions {
            display: flex;
            gap: 0.25rem;
          }

          .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 1.5rem;
          }

          .status-card {
            border: 1px solid #e5e7eb;
            border-radius: 0.75rem;
            padding: 1.5rem;
            background: white;
          }

          .status-card h4 {
            margin: 0 0 1rem 0;
            font-size: 1rem;
            font-weight: 600;
            color: #1f2937;
          }

          .status-details {
            display: flex;
            flex-direction: column;
            gap: 0.75rem;
          }

          .status-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 0.5rem 0;
            border-bottom: 1px solid #f3f4f6;
          }

          .status-item:last-child {
            border-bottom: none;
          }

          .status-item.full-width {
            flex-direction: column;
            align-items: flex-start;
            gap: 0.25rem;
          }

          .status-item .label {
            font-size: 0.75rem;
            color: #6b7280;
            font-weight: 500;
          }

          .status-item .value {
            font-size: 0.875rem;
            color: #1f2937;
            font-weight: 500;
          }

          .error-message {
            font-family: 'Courier New', monospace;
            background: #fef2f2;
            padding: 0.25rem 0.5rem;
            border-radius: 0.25rem;
            color: #991b1b;
            word-break: break-all;
          }

          .logs-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1.5rem;
          }

          .logs-header h3 {
            margin: 0;
            font-size: 1.125rem;
            font-weight: 600;
            color: #1f2937;
          }

          .log-filters {
            display: flex;
            gap: 0.5rem;
            align-items: center;
          }

          .filter-select {
            padding: 0.5rem;
            border: 1px solid #d1d5db;
            border-radius: 0.375rem;
            font-size: 0.875rem;
          }

          .logs-content {
            border: 1px solid #e5e7eb;
            border-radius: 0.5rem;
            min-height: 300px;
            padding: 1rem;
          }

          .empty-message {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            min-height: 200px;
            color: #6b7280;
            text-align: center;
            gap: 1rem;
          }

          .empty-message i {
            font-size: 3rem;
            color: #cbd5e1;
          }

          .empty-message p {
            margin: 0;
            font-size: 0.875rem;
          }

          .empty-message code {
            background: #f3f4f6;
            padding: 0.25rem 0.5rem;
            border-radius: 0.25rem;
            font-family: 'Courier New', monospace;
            color: #1f2937;
          }

          .modal-footer {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 1.5rem 2rem;
            border-top: 1px solid #e5e7eb;
            background: #f9fafb;
            flex-shrink: 0;
          }

          .footer-left,
          .footer-right {
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

          .btn-xs {
            padding: 0.25rem 0.5rem;
            font-size: 0.75rem;
          }

          .btn-sm {
            padding: 0.375rem 0.75rem;
            font-size: 0.75rem;
          }

          .btn-primary {
            background: #0ea5e9;
            color: white;
          }

          .btn-primary:hover:not(:disabled) {
            background: #0284c7;
          }

          .btn-secondary {
            background: #64748b;
            color: white;
          }

          .btn-secondary:hover:not(:disabled) {
            background: #475569;
          }

          .btn-error {
            background: #dc2626;
            color: white;
          }

          .btn-error:hover:not(:disabled) {
            background: #b91c1c;
          }

          .btn-info {
            background: #0891b2;
            color: white;
          }

          .btn-info:hover:not(:disabled) {
            background: #0e7490;
          }

          .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
          }

          .text-success-600 { color: #059669; }
          .text-warning-600 { color: #d97706; }
          .text-error-600 { color: #dc2626; }
          .text-neutral-500 { color: #6b7280; }
        `}</style>
      </div>
    </div>
  );
};

export default DeviceDetailModal;