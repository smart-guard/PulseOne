// ============================================================================
// frontend/src/components/modals/DeviceDetailModal.tsx
// 🔥 분할된 디바이스 상세 모달 - 메인 컴포넌트 (완전 구현)
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService } from '../../api/services/deviceApi';
import { DataApiService, DataPoint } from '../../api/services/dataApi';

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
  // 데이터포인트 관리 함수들 - 올바른 API 사용
  // ========================================================================

  /**
   * 디바이스의 데이터포인트 목록 로드
   */
  const loadDataPoints = async (deviceId: number) => {
    try {
      setIsLoadingDataPoints(true);
      setDataPointsError(null);
      console.log(`📊 디바이스 ${deviceId} 데이터포인트 로드 시작...`);

      // 🔥 올바른 API 호출 - DataApiService 사용
      const response = await DataApiService.searchDataPoints({
        device_id: deviceId,
        page: 1,
        limit: 100,
        enabled_only: false,
        include_current_value: true
      });

      if (response.success && response.data) {
        setDataPoints(response.data.items);
        console.log(`✅ 데이터포인트 ${response.data.items.length}개 로드 완료`);
      } else {
        throw new Error(response.error || 'API 응답 오류');
      }

    } catch (error) {
      console.error(`❌ 디바이스 ${deviceId} 데이터포인트 로드 실패:`, error);
      setDataPointsError(error instanceof Error ? error.message : 'Unknown error');
    } finally {
      setIsLoadingDataPoints(false);
    }
  };

  /**
   * 데이터포인트 생성
   */
  const handleCreateDataPoint = (newDataPoint: DataPoint) => {
    setDataPoints(prev => [...prev, newDataPoint]);
  };

  /**
   * 데이터포인트 업데이트
   */
  const handleUpdateDataPoint = (updatedDataPoint: DataPoint) => {
    setDataPoints(prev => 
      prev.map(dp => dp.id === updatedDataPoint.id ? updatedDataPoint : dp)
    );
  };

  /**
   * 데이터포인트 삭제
   */
  const handleDeleteDataPoint = (pointId: number) => {
    setDataPoints(prev => prev.filter(dp => dp.id !== pointId));
  };

  /**
   * 데이터포인트 새로고침
   */
  const handleRefreshDataPoints = async () => {
    if (device?.id) {
      await loadDataPoints(device.id);
    }
  };

  // ========================================================================
  // 디바이스 관리 함수들 - DeviceApiService 사용
  // ========================================================================

  /**
   * 새 디바이스 초기화
   */
  const initializeNewDevice = () => {
    setEditData({
      id: 0,
      name: '',
      description: '',
      device_type: 'PLC',
      manufacturer: '',
      model: '',
      serial_number: '',
      protocol_type: 'MODBUS_TCP',
      endpoint: '',
      polling_interval: 1000,
      timeout: 5000,
      retry_count: 3,
      is_enabled: true,
      created_at: '',
      updated_at: ''
    });
  };

  /**
   * 디바이스 저장
   */
  const handleSave = async () => {
    if (!editData) return;

    try {
      setIsLoading(true);
      let savedDevice: Device;

      if (mode === 'create') {
        // 생성 요청 데이터 변환
        const createData = {
          name: editData.name,
          description: editData.description,
          device_type: editData.device_type,
          manufacturer: editData.manufacturer,
          model: editData.model,
          protocol_type: editData.protocol_type,
          endpoint: editData.endpoint,
          config: editData.config,
          site_id: editData.site_id,
          device_group_id: editData.device_group_id,
          polling_interval: editData.polling_interval,
          timeout: editData.timeout,
          retry_count: editData.retry_count,
          is_enabled: editData.is_enabled
        };

        const response = await DeviceApiService.createDevice(createData);
        if (response.success && response.data) {
          savedDevice = response.data;
        } else {
          throw new Error(response.error || '생성 실패');
        }
      } else if (mode === 'edit') {
        // 수정 요청 데이터 변환
        const updateData = {
          name: editData.name,
          description: editData.description,
          device_type: editData.device_type,
          manufacturer: editData.manufacturer,
          model: editData.model,
          endpoint: editData.endpoint,
          config: editData.config,
          polling_interval: editData.polling_interval,
          timeout: editData.timeout,
          retry_count: editData.retry_count,
          is_enabled: editData.is_enabled
        };

        const response = await DeviceApiService.updateDevice(editData.id, updateData);
        if (response.success && response.data) {
          savedDevice = response.data;
        } else {
          throw new Error(response.error || '수정 실패');
        }
      } else {
        return;
      }

      onSave?.(savedDevice);
      onClose();
    } catch (error) {
      console.error('디바이스 저장 실패:', error);
      alert(`저장 실패: ${error instanceof Error ? error.message : 'Unknown error'}`);
    } finally {
      setIsLoading(false);
    }
  };

  /**
   * 디바이스 삭제
   */
  const handleDelete = async () => {
    if (!device) return;

    if (confirm(`"${device.name}" 디바이스를 삭제하시겠습니까?`)) {
      try {
        setIsLoading(true);
        const response = await DeviceApiService.deleteDevice(device.id);
        if (response.success) {
          onDelete?.(device.id);
          onClose();
        } else {
          throw new Error(response.error || '삭제 실패');
        }
      } catch (error) {
        console.error('디바이스 삭제 실패:', error);
        alert(`삭제 실패: ${error instanceof Error ? error.message : 'Unknown error'}`);
      } finally {
        setIsLoading(false);
      }
    }
  };

  /**
   * 필드 업데이트
   */
  const updateField = (field: string, value: any) => {
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
              {displayData?.connection_status && (
                <span className={`status-indicator ${displayData.connection_status}`}>
                  <i className="fas fa-circle"></i>
                  {displayData.connection_status === 'connected' ? '연결됨' :
                   displayData.connection_status === 'disconnected' ? '연결끊김' :
                   displayData.connection_status === 'connecting' ? '연결중' : '알수없음'}
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
              device={device}
              editData={editData}
              mode={mode}
              onUpdateField={updateField}
            />
          )}

          {activeTab === 'settings' && (
            <DeviceSettingsTab
              device={device}
              editData={editData}
              mode={mode}
              onUpdateField={updateField}
              onUpdateSettings={updateSettings}
            />
          )}

          {activeTab === 'datapoints' && (
            <DeviceDataPointsTab
              deviceId={device?.id || editData?.id || 0}
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
            <DeviceStatusTab device={device} />
          )}

          {activeTab === 'logs' && mode === 'view' && (
            <DeviceLogsTab deviceId={device?.id || 0} />
          )}
        </div>

        {/* 모달 푸터 */}
        <div className="modal-footer">
          <div className="footer-left">
            {mode === 'edit' && onDelete && (
              <button
                type="button"
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
            <button type="button" className="btn btn-secondary" onClick={onClose}>
              취소
            </button>
            {mode !== 'view' && (
              <button
                type="button"
                className="btn btn-primary"
                onClick={handleSave}
                disabled={isLoading}
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

        {/* 스타일 */}
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
          }

          .modal-container {
            background: white;
            border-radius: 0.75rem;
            width: 90vw;
            max-width: 1200px;
            height: 90vh;
            max-height: 800px;
            display: flex;
            flex-direction: column;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
          }

          .modal-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 1.5rem 2rem;
            border-bottom: 1px solid #e5e7eb;
            flex-shrink: 0;
          }

          .modal-title {
            flex: 1;
          }

          .title-row {
            display: flex;
            align-items: center;
            gap: 1rem;
            margin-bottom: 0.5rem;
          }

          .title-row h2 {
            margin: 0;
            font-size: 1.5rem;
            font-weight: 600;
            color: #111827;
          }

          .device-subtitle {
            color: #6b7280;
            font-size: 0.875rem;
          }

          .status-indicator {
            display: flex;
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
            background: #fef2f2;
            color: #dc2626;
          }

          .status-indicator.connecting {
            background: #fef3c7;
            color: #d97706;
          }

          .close-btn {
            display: flex;
            align-items: center;
            justify-content: center;
            width: 2.5rem;
            height: 2.5rem;
            border: none;
            border-radius: 0.5rem;
            background: #f3f4f6;
            color: #6b7280;
            cursor: pointer;
            transition: all 0.2s ease;
          }

          .close-btn:hover {
            background: #e5e7eb;
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
            transition: all 0.2s ease;
            border-bottom: 2px solid transparent;
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