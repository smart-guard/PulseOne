// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceBasicInfoTab.tsx
// 📋 디바이스 기본정보 탭 컴포넌트
// ============================================================================

import React from 'react';
import { DeviceBasicInfoTabProps } from './types';

const DeviceBasicInfoTab: React.FC<DeviceBasicInfoTabProps> = ({
  device,
  editData,
  mode,
  onUpdateField
}) => {
  const displayData = device || editData;

  return (
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
              onChange={(e) => onUpdateField('name', e.target.value)}
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
              onChange={(e) => onUpdateField('device_type', e.target.value)}
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
              onChange={(e) => onUpdateField('protocol_type', e.target.value)}
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
              onChange={(e) => onUpdateField('endpoint', e.target.value)}
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
              onChange={(e) => onUpdateField('manufacturer', e.target.value)}
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
              onChange={(e) => onUpdateField('model', e.target.value)}
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
              onChange={(e) => onUpdateField('serial_number', e.target.value)}
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
                onChange={(e) => onUpdateField('is_enabled', e.target.checked)}
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
              onChange={(e) => onUpdateField('description', e.target.value)}
              placeholder="디바이스에 대한 설명을 입력하세요"
              rows={3}
            />
          )}
        </div>
      </div>

      <style jsx>{`
        .tab-panel {
          flex: 1;
          overflow-y: auto;
          padding: 2rem;
          height: 100%;
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
      `}</style>
    </div>
  );
};

export default DeviceBasicInfoTab;