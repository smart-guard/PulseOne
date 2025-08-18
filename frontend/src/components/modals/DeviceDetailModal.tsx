// ============================================================================
// frontend/src/components/modals/DeviceDetailModal.tsx
// 🔥 분할된 디바이스 상세 모달 - 메인 컴포넌트
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService } from '../../api/services/deviceApi';
import { DataPointApiService, DataPoint } from '../../api/services/dataPointApi';

// 분할된 컴포넌트들 import
import DeviceBasicInfoTab from './DeviceModal/DeviceBasicInfoTab';
import DeviceSettingsTab from './DeviceModal/DeviceSettingsTab';
import DeviceDataPointsTab from './DeviceModal/DeviceDataPointsTab';
import DeviceStatusTab from './DeviceModal/DeviceStatusTab';
import DeviceLogsTab from './DeviceModal/DeviceLogsTab';

// 타입 정의
import { Device, DeviceModalProps } from './DeviceModal/types';

const DeviceDetailModal: React.FC<DeviceModalProps> = ({
  device,
  isOpen,
  mode,
  onClose,
  onSave,
  onDelete
}) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [activeTab, setActiveTab] = useState('basic');
  const [editData, setEditData] = useState<Device | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  
  // 데이터포인트 관련 상태
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [isLoadingDataPoints, setIsLoadingDataPoints] = useState(false);
  const [dataPointsError, setDataPointsError] = useState<string | null>(null);

  // ========================================================================
  // 데이터포인트 관리 함수들
  // ========================================================================

  /**
   * 디바이스의 데이터포인트 목록 로드
   */
  const loadDataPoints = async (deviceId: number) => {
    try {
      setIsLoadingDataPoints(true);
      setDataPointsError(null);
      console.log(`📊 디바이스 ${deviceId} 데이터포인트 로드 시작...`);

      const response = await DataPointApiService.getDeviceDataPoints(deviceId, {
        page: 1,
        limit: 100,
        enabled_only: false
      });

      if (response.success && response.data) {
        setDataPoints(response.data.items);
        console.log(`✅ 데이터포인트 ${response.data.items.length}개 로드 완료`);
      } else {
        throw new Error(response.error || 'API 응답 오류');
      }

    } catch (error) {
      console.error(`❌ 디바이스 ${deviceId} 데이터포인트 로드 실패:`, error);
      setDataPointsError(error instanceof Error ? error.message : '데이터포인트를 불러올 수 없습니다');
      setDataPoints([]);
    } finally {
      setIsLoadingDataPoints(false);
    }
  };

  /**
   * 데이터포인트 새로고침
   */
  const handleRefreshDataPoints = async () => {
    if (device && mode !== 'create') {
      await loadDataPoints(device.id);
    }
  };

  /**
   * 데이터포인트 생성
   */
  const handleCreateDataPoint = (newDataPoint: DataPoint) => {
    setDataPoints(prev => [...prev, newDataPoint]);
  };

  /**
   * 데이터포인트 수정
   */
  const handleUpdateDataPoint = (updatedDataPoint: DataPoint) => {
    setDataPoints(prev => prev.map(dp => 
      dp.id === updatedDataPoint.id ? updatedDataPoint : dp
    ));
  };

  /**
   * 데이터포인트 삭제
   */
  const handleDeleteDataPoint = (pointId: number) => {
    setDataPoints(prev => prev.filter(dp => dp.id !== pointId));
  };

  // ========================================================================
  // 디바이스 관리 함수들
  // ========================================================================

  /**
   * 새 디바이스 초기화
   */
  const initializeNewDevice = () => {
    setDataPoints([]);
    setEditData({
      id: 0,
      tenant_id: 1,
      site_id: 1,
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

  /**
   * 디바이스 저장
   */
  const handleSave = async () => {
    if (!editData) return;

    try {
      setIsLoading(true);
      console.log('💾 디바이스 저장:', editData);

      if (onSave) {
        onSave(editData);
      }
      onClose();

    } catch (error) {
      console.error('❌ 디바이스 저장 실패:', error);
      alert(`저장에 실패했습니다: ${error.message}`);
    } finally {
      setIsLoading(false);
    }
  };

  /**
   * 디바이스 삭제
   */
  const handleDelete = async () => {
    if (!device || !onDelete) return;
    
    if (confirm(`${device.name} 디바이스를 삭제하시겠습니까?`)) {
      setIsLoading(true);
      try {
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

  /**
   * 필드 업데이트
   */
  const updateEditData = (field: string, value: any) => {
    setEditData(prev => prev ? { ...prev, [field]: value } : null);
  };

  /**
   * 설정 필드 업데이트
   */
  const updateSettings = (field: string, value: any) => {
    setEditData(prev => prev ? {
      ...prev,
      settings: { ...prev.settings, [field]: value }
    } : null);
  };

  // ========================================================================
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    if (isOpen && device && mode !== 'create') {
      setEditData({ ...device });
      setActiveTab('basic');
      loadDataPoints(device.id);
    } else if (mode === 'create') {
      initializeNewDevice();
      setActiveTab('basic');
    } else if (!isOpen) {
      setDataPoints([]);
      setDataPointsError(null);
    }
  }, [isOpen, device, mode]);

  // ========================================================================
  // 렌더링
  // ========================================================================

  if (!isOpen) return null;

  const displayData = device || editData;

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
              {displayData?.status && (
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
          {mode !== 'create' && (
            <button 
              className={`tab-btn ${activeTab === 'status' ? 'active' : ''}`}
              onClick={() => setActiveTab('status')}
            >
              <i className="fas fa-chart-line"></i>
              상태
            </button>
          )}
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

        {/* 탭 내용 */}
        <div className="modal-content">
          {activeTab === 'basic' && (
            <DeviceBasicInfoTab
              device={displayData}
              editData={editData}
              mode={mode}
              onUpdateField={updateEditData}
            />
          )}

          {activeTab === 'settings' && (
            <DeviceSettingsTab
              device={displayData}
              editData={editData}
              mode={mode}
              onUpdateField={updateEditData}
              onUpdateSettings={updateSettings}
            />
          )}

          {activeTab === 'datapoints' && (
            <DeviceDataPointsTab
              deviceId={device?.id || 0}
              dataPoints={dataPoints}
              isLoading={isLoadingDataPoints}
              error={dataPointsError}
              mode={mode}
              onRefresh={handleRefreshDataPoints}
              onCreate={handleCreateDataPoint}
              onUpdate={handleUpdateDataPoint}
              onDelete={handleDeleteDataPoint}
            />
          )}

          {activeTab === 'status' && mode !== 'create' && (
            <DeviceStatusTab
              device={displayData}
            />
          )}

          {activeTab === 'logs' && mode === 'view' && (
            <DeviceLogsTab
              deviceId={device?.id || 0}
            />
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

          .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
          }
        `}</style>
      </div>
    </div>
  );
};

export default DeviceDetailModal;