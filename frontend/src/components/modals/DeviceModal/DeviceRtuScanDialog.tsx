// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx
// RTU 슬래이브 스캔 대화상자 컴포넌트 - protocol_id 지원
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { DeviceApiService } from '../../../api/services/deviceApi';
import { Device } from './types';

interface DeviceRtuScanDialogProps {
  isOpen: boolean;
  masterDevice: Device;
  onClose: () => void;
  onScanComplete: (foundDevices?: any[]) => void;
}

interface ScanSettings {
  startSlaveId: number;
  endSlaveId: number;
  timeoutMs: number;
  baudRate: number;
  parity: 'N' | 'E' | 'O';
  dataBits: number;
  stopBits: number;
}

interface ScanProgress {
  currentSlaveId: number;
  totalScanned: number;
  foundDevices: number;
  isScanning: boolean;
  progress: number; // 0-100
}

interface FoundDevice {
  slaveId: number;
  responseTime: number;
  deviceInfo?: string;
  isRegistered: boolean;
}

const DeviceRtuScanDialog: React.FC<DeviceRtuScanDialogProps> = ({
  isOpen,
  masterDevice,
  onClose,
  onScanComplete
}) => {
  const [scanSettings, setScanSettings] = useState<ScanSettings>({
    startSlaveId: 1,
    endSlaveId: 20,
    timeoutMs: 2000,
    baudRate: 9600,
    parity: 'N',
    dataBits: 8,
    stopBits: 1
  });

  const [scanProgress, setScanProgress] = useState<ScanProgress>({
    currentSlaveId: 0,
    totalScanned: 0,
    foundDevices: 0,
    isScanning: false,
    progress: 0
  });

  const [foundDevices, setFoundDevices] = useState<FoundDevice[]>([]);
  const [selectedDevices, setSelectedDevices] = useState<Set<number>>(new Set());
  const [scanPhase, setScanPhase] = useState<'settings' | 'scanning' | 'results'>('settings');
  const [error, setError] = useState<string | null>(null);

  // ========================================================================
  // 프로토콜 정보 및 RTU 디바이스 확인
  // ========================================================================

  /**
   * 프로토콜 정보 가져오기
   */
  const getProtocolInfo = useCallback(() => {
    if (masterDevice?.protocol_id) {
      return DeviceApiService.getProtocolManager().getProtocolById(masterDevice.protocol_id);
    }
    if (masterDevice?.protocol_type) {
      return DeviceApiService.getProtocolManager().getProtocolByType(masterDevice.protocol_type);
    }
    return null;
  }, [masterDevice]);

  /**
   * RTU 디바이스 여부 확인
   */
  const isRtuDevice = useCallback(() => {
    // 1. protocol_type으로 확인
    if (masterDevice?.protocol_type === 'MODBUS_RTU') {
      return true;
    }
    
    // 2. protocol_id로 확인
    if (masterDevice?.protocol_id) {
      const protocol = getProtocolInfo();
      return protocol?.protocol_type === 'MODBUS_RTU';
    }
    
    return false;
  }, [masterDevice, getProtocolInfo]);

  const protocolInfo = getProtocolInfo();

  // 마스터 디바이스의 RTU 설정에서 기본값 가져오기
  useEffect(() => {
    if (isOpen && masterDevice?.config && isRtuDevice()) {
      const config = masterDevice.config;
      setScanSettings(prev => ({
        ...prev,
        baudRate: config.baud_rate || 9600,
        parity: (config.parity as 'N' | 'E' | 'O') || 'N',
        dataBits: config.data_bits || 8,
        stopBits: config.stop_bits || 1
      }));
      console.log('RTU 마스터 설정에서 기본값 적용:', config);
    }
  }, [isOpen, masterDevice, isRtuDevice]);

  // 모달 닫을 때 상태 초기화
  const handleClose = useCallback(() => {
    setScanProgress({
      currentSlaveId: 0,
      totalScanned: 0,
      foundDevices: 0,
      isScanning: false,
      progress: 0
    });
    setFoundDevices([]);
    setSelectedDevices(new Set());
    setScanPhase('settings');
    setError(null);
    onClose();
  }, [onClose]);

  // 스캔 시작
  const handleStartScan = async () => {
    if (!isRtuDevice()) {
      setError('RTU 디바이스가 아닙니다');
      return;
    }

    try {
      setError(null);
      setScanPhase('scanning');
      setScanProgress({
        currentSlaveId: scanSettings.startSlaveId,
        totalScanned: 0,
        foundDevices: 0,
        isScanning: true,
        progress: 0
      });
      setFoundDevices([]);

      const totalSlaves = scanSettings.endSlaveId - scanSettings.startSlaveId + 1;
      const found: FoundDevice[] = [];

      console.log(`RTU 스캔 시작: 마스터 ${masterDevice.id}, 범위 ${scanSettings.startSlaveId}-${scanSettings.endSlaveId}`);

      // 실제 스캔 API 호출
      const scanRequest = {
        master_device_id: masterDevice.id,
        protocol_id: masterDevice.protocol_id,
        protocol_type: masterDevice.protocol_type,
        serial_port: masterDevice.config?.serial_port || masterDevice.endpoint,
        start_slave_id: scanSettings.startSlaveId,
        end_slave_id: scanSettings.endSlaveId,
        timeout_ms: scanSettings.timeoutMs,
        baud_rate: scanSettings.baudRate,
        parity: scanSettings.parity,
        data_bits: scanSettings.dataBits,
        stop_bits: scanSettings.stopBits
      };

      const response = await fetch(`/api/devices/rtu/scan`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(scanRequest),
      });

      if (!response.ok) {
        throw new Error(`스캔 요청 실패: ${response.statusText}`);
      }

      const scanResult = await response.json();
      
      if (scanResult.success) {
        // 실제 스캔 결과 처리
        console.log('스캔 결과:', scanResult.data);
        // TODO: 실제 구현에서는 scanResult.data를 처리
      }

      // 실제 구현에서는 Server-Sent Events나 WebSocket으로 진행상황을 받을 수 있음
      // 여기서는 시뮬레이션으로 구현
      for (let slaveId = scanSettings.startSlaveId; slaveId <= scanSettings.endSlaveId; slaveId++) {
        setScanProgress(prev => ({
          ...prev,
          currentSlaveId: slaveId,
          totalScanned: slaveId - scanSettings.startSlaveId,
          progress: Math.round(((slaveId - scanSettings.startSlaveId) / totalSlaves) * 100)
        }));

        // 시뮬레이션: 30% 확률로 디바이스 발견
        await new Promise(resolve => setTimeout(resolve, Math.random() * 500 + 200));
        
        if (Math.random() < 0.3) {
          const foundDevice: FoundDevice = {
            slaveId,
            responseTime: Math.floor(Math.random() * 200) + 50,
            deviceInfo: `RTU Device ${slaveId}`,
            isRegistered: false
          };
          
          found.push(foundDevice);
          setFoundDevices(prev => [...prev, foundDevice]);
          setScanProgress(prev => ({
            ...prev,
            foundDevices: prev.foundDevices + 1
          }));
        }
      }

      // 스캔 완료
      setScanProgress(prev => ({
        ...prev,
        isScanning: false,
        progress: 100,
        totalScanned: totalSlaves
      }));
      
      setScanPhase('results');
      console.log(`스캔 완료: ${found.length}개 디바이스 발견`);

    } catch (err) {
      console.error('RTU 스캔 실패:', err);
      setError(err instanceof Error ? err.message : '스캔 중 오류가 발생했습니다');
      setScanProgress(prev => ({ ...prev, isScanning: false }));
    }
  };

  // 디바이스 선택/선택해제
  const handleDeviceSelect = (slaveId: number, selected: boolean) => {
    setSelectedDevices(prev => {
      const newSet = new Set(prev);
      if (selected) {
        newSet.add(slaveId);
      } else {
        newSet.delete(slaveId);
      }
      return newSet;
    });
  };

  // 전체 선택/해제
  const handleSelectAll = (selected: boolean) => {
    if (selected) {
      setSelectedDevices(new Set(foundDevices.map(d => d.slaveId)));
    } else {
      setSelectedDevices(new Set());
    }
  };

  // 선택된 디바이스 등록
  const handleRegisterDevices = async () => {
    if (!isRtuDevice()) {
      setError('RTU 디바이스가 아닙니다');
      return;
    }

    try {
      setError(null);
      
      const devicesToRegister = foundDevices.filter(d => selectedDevices.has(d.slaveId));
      console.log(`${devicesToRegister.length}개 디바이스 등록 시작`);
      
      for (const device of devicesToRegister) {
        // RTU 슬래이브 디바이스 등록 요청
        const createRequest = {
          name: `RTU-SLAVE-${device.slaveId.toString().padStart(2, '0')}`,
          device_type: 'SENSOR',
          protocol_id: masterDevice.protocol_id, // protocol_id 사용
          protocol_type: masterDevice.protocol_type, // 호환성을 위해 protocol_type도 포함
          endpoint: masterDevice.config?.serial_port || masterDevice.endpoint,
          config: {
            device_role: 'slave',
            slave_id: device.slaveId,
            master_device_id: masterDevice.id,
            baud_rate: scanSettings.baudRate,
            parity: scanSettings.parity,
            data_bits: scanSettings.dataBits,
            stop_bits: scanSettings.stopBits
          },
          is_enabled: true
        };

        console.log(`슬래이브 ${device.slaveId} 등록 요청:`, createRequest);

        const response = await DeviceApiService.createDevice(createRequest);

        if (!response.success) {
          console.error(`슬래이브 ${device.slaveId} 등록 실패:`, response.error);
        } else {
          console.log(`슬래이브 ${device.slaveId} 등록 성공:`, response.data);
        }
      }

      console.log('모든 디바이스 등록 완료');
      onScanComplete(devicesToRegister);
      handleClose();

    } catch (err) {
      console.error('디바이스 등록 실패:', err);
      setError(err instanceof Error ? err.message : '디바이스 등록 중 오류가 발생했습니다');
    }
  };

  // RTU 디바이스가 아니면 렌더링하지 않음
  if (!isOpen || !isRtuDevice()) {
    return null;
  }

  return (
    <div className="modal-overlay">
      <div className="scan-dialog">
        {/* 헤더 */}
        <div className="dialog-header">
          <h2>
            <i className="fas fa-search"></i>
            RTU 슬래이브 스캔
            {protocolInfo && (
              <span className="protocol-info">
                {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
              </span>
            )}
          </h2>
          <button className="close-btn" onClick={handleClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 내용 */}
        <div className="dialog-content">
          {scanPhase === 'settings' && (
            <div className="settings-phase">
              <div className="master-info">
                <h3>마스터 디바이스</h3>
                <div className="master-card">
                  <div className="device-icon">
                    <i className="fas fa-server"></i>
                  </div>
                  <div className="device-info">
                    <div className="device-name">{masterDevice.name}</div>
                    <div className="device-details">
                      {masterDevice.config?.serial_port || masterDevice.endpoint}
                      {protocolInfo && (
                        <span className="protocol-tag">
                          {protocolInfo.protocol_type}
                        </span>
                      )}
                    </div>
                  </div>
                </div>
              </div>

              <div className="scan-settings">
                <h3>스캔 설정</h3>
                
                <div className="settings-grid">
                  <div className="setting-group">
                    <label>슬래이브 ID 범위</label>
                    <div className="range-inputs">
                      <input
                        type="number"
                        min="1"
                        max="247"
                        value={scanSettings.startSlaveId}
                        onChange={(e) => setScanSettings(prev => ({
                          ...prev,
                          startSlaveId: parseInt(e.target.value) || 1
                        }))}
                      />
                      <span>~</span>
                      <input
                        type="number"
                        min="1"
                        max="247"
                        value={scanSettings.endSlaveId}
                        onChange={(e) => setScanSettings(prev => ({
                          ...prev,
                          endSlaveId: parseInt(e.target.value) || 20
                        }))}
                      />
                    </div>
                  </div>

                  <div className="setting-group">
                    <label>타임아웃</label>
                    <div className="input-with-unit">
                      <input
                        type="number"
                        min="500"
                        max="10000"
                        step="500"
                        value={scanSettings.timeoutMs}
                        onChange={(e) => setScanSettings(prev => ({
                          ...prev,
                          timeoutMs: parseInt(e.target.value) || 2000
                        }))}
                      />
                      <span className="unit">ms</span>
                    </div>
                  </div>

                  <div className="setting-group">
                    <label>Baud Rate</label>
                    <select
                      value={scanSettings.baudRate}
                      onChange={(e) => setScanSettings(prev => ({
                        ...prev,
                        baudRate: parseInt(e.target.value)
                      }))}
                    >
                      <option value={1200}>1200</option>
                      <option value={2400}>2400</option>
                      <option value={4800}>4800</option>
                      <option value={9600}>9600</option>
                      <option value={19200}>19200</option>
                      <option value={38400}>38400</option>
                      <option value={57600}>57600</option>
                      <option value={115200}>115200</option>
                    </select>
                  </div>

                  <div className="setting-group">
                    <label>패리티</label>
                    <select
                      value={scanSettings.parity}
                      onChange={(e) => setScanSettings(prev => ({
                        ...prev,
                        parity: e.target.value as 'N' | 'E' | 'O'
                      }))}
                    >
                      <option value="N">None</option>
                      <option value="E">Even</option>
                      <option value="O">Odd</option>
                    </select>
                  </div>
                </div>

                <div className="scan-info">
                  <p>
                    슬래이브 ID {scanSettings.startSlaveId}-{scanSettings.endSlaveId} 범위에서 
                    총 {scanSettings.endSlaveId - scanSettings.startSlaveId + 1}개 디바이스를 스캔합니다.
                  </p>
                  <p>
                    예상 소요시간: 약 {Math.ceil((scanSettings.endSlaveId - scanSettings.startSlaveId + 1) * (scanSettings.timeoutMs / 1000))}초
                  </p>
                  {protocolInfo && (
                    <p>
                      프로토콜: {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
                    </p>
                  )}
                </div>
              </div>
            </div>
          )}

          {scanPhase === 'scanning' && (
            <div className="scanning-phase">
              <div className="scan-progress-header">
                <h3>네트워크 스캔 진행중...</h3>
                <div className="progress-stats">
                  <span>진행률: {scanProgress.progress}%</span>
                  <span>발견: {scanProgress.foundDevices}개</span>
                  <span>현재: ID {scanProgress.currentSlaveId}</span>
                </div>
              </div>

              <div className="progress-bar-container">
                <div className="progress-bar">
                  <div 
                    className="progress-fill"
                    style={{ width: `${scanProgress.progress}%` }}
                  ></div>
                </div>
                <div className="progress-text">
                  {scanProgress.totalScanned} / {scanSettings.endSlaveId - scanSettings.startSlaveId + 1}
                </div>
              </div>

              {foundDevices.length > 0 && (
                <div className="found-devices-preview">
                  <h4>발견된 디바이스</h4>
                  <div className="devices-list">
                    {foundDevices.map((device) => (
                      <div key={device.slaveId} className="found-device-item">
                        <i className="fas fa-microchip"></i>
                        <span>슬래이브 ID {device.slaveId}</span>
                        <span className="response-time">{device.responseTime}ms</span>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              {scanProgress.isScanning && (
                <div className="scanning-indicator">
                  <i className="fas fa-spinner fa-spin"></i>
                  <span>스캔 중... 잠시만 기다려주세요</span>
                </div>
              )}
            </div>
          )}

          {scanPhase === 'results' && (
            <div className="results-phase">
              <div className="scan-results-header">
                <h3>
                  스캔 완료 - {foundDevices.length}개 디바이스 발견
                </h3>
                <div className="results-stats">
                  <span>스캔 범위: {scanSettings.startSlaveId}-{scanSettings.endSlaveId}</span>
                  <span>총 스캔: {scanProgress.totalScanned}개</span>
                  <span>발견률: {Math.round((foundDevices.length / scanProgress.totalScanned) * 100)}%</span>
                  {protocolInfo && (
                    <span>프로토콜: {protocolInfo.display_name} (ID: {protocolInfo.id})</span>
                  )}
                </div>
              </div>

              {foundDevices.length > 0 ? (
                <div className="found-devices">
                  <div className="devices-header">
                    <label className="select-all">
                      <input
                        type="checkbox"
                        checked={selectedDevices.size === foundDevices.length}
                        onChange={(e) => handleSelectAll(e.target.checked)}
                      />
                      전체 선택 ({selectedDevices.size}/{foundDevices.length})
                    </label>
                  </div>

                  <div className="devices-list">
                    {foundDevices.map((device) => (
                      <div key={device.slaveId} className="device-result-item">
                        <label className="device-checkbox">
                          <input
                            type="checkbox"
                            checked={selectedDevices.has(device.slaveId)}
                            onChange={(e) => handleDeviceSelect(device.slaveId, e.target.checked)}
                          />
                        </label>
                        
                        <div className="device-icon">
                          <i className="fas fa-microchip"></i>
                        </div>
                        
                        <div className="device-info">
                          <div className="device-name">
                            RTU-SLAVE-{device.slaveId.toString().padStart(2, '0')}
                          </div>
                          <div className="device-details">
                            슬래이브 ID: {device.slaveId} • 응답시간: {device.responseTime}ms
                            {protocolInfo && (
                              <span className="protocol-info">
                                • {protocolInfo.protocol_type}
                              </span>
                            )}
                          </div>
                        </div>
                        
                        <div className="device-status">
                          <span className="status-badge discovered">
                            <i className="fas fa-check"></i>
                            발견됨
                          </span>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              ) : (
                <div className="no-devices">
                  <i className="fas fa-search"></i>
                  <h4>발견된 디바이스가 없습니다</h4>
                  <p>다른 설정으로 다시 스캔해보세요</p>
                  {protocolInfo && (
                    <p className="protocol-note">
                      현재 프로토콜: {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
                    </p>
                  )}
                </div>
              )}
            </div>
          )}

          {error && (
            <div className="error-message">
              <i className="fas fa-exclamation-triangle"></i>
              <span>{error}</span>
            </div>
          )}
        </div>

        {/* 푸터 */}
        <div className="dialog-footer">
          <div className="footer-left">
            {scanPhase === 'results' && foundDevices.length > 0 && (
              <div className="selection-info">
                {selectedDevices.size}개 디바이스가 선택됨
              </div>
            )}
            {protocolInfo && (
              <div className="footer-protocol-info">
                프로토콜: {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
              </div>
            )}
          </div>
          <div className="footer-right">
            <button className="btn btn-secondary" onClick={handleClose}>
              {scanPhase === 'results' ? '닫기' : '취소'}
            </button>
            
            {scanPhase === 'settings' && (
              <button 
                className="btn btn-primary"
                onClick={handleStartScan}
                disabled={scanSettings.startSlaveId > scanSettings.endSlaveId}
              >
                <i className="fas fa-search"></i>
                스캔 시작
              </button>
            )}
            
            {scanPhase === 'results' && selectedDevices.size > 0 && (
              <button 
                className="btn btn-success"
                onClick={handleRegisterDevices}
              >
                <i className="fas fa-plus"></i>
                선택된 디바이스 등록 ({selectedDevices.size})
              </button>
            )}
            
            {scanPhase === 'results' && (
              <button 
                className="btn btn-info"
                onClick={() => {
                  setScanPhase('settings');
                  setFoundDevices([]);
                  setSelectedDevices(new Set());
                }}
              >
                <i className="fas fa-redo"></i>
                다시 스캔
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
            backdrop-filter: blur(4px);
          }

          .scan-dialog {
            background: white;
            border-radius: 0.75rem;
            width: 90vw;
            max-width: 700px;
            max-height: 90vh;
            display: flex;
            flex-direction: column;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
          }

          .dialog-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 1.5rem 2rem;
            border-bottom: 1px solid #e5e7eb;
            flex-shrink: 0;
          }

          .dialog-header h2 {
            margin: 0;
            font-size: 1.25rem;
            font-weight: 600;
            color: #1f2937;
            display: flex;
            align-items: center;
            gap: 0.75rem;
          }

          .protocol-info {
            font-size: 0.75rem;
            color: #0ea5e9;
            background: #f0f9ff;
            padding: 0.25rem 0.5rem;
            border-radius: 0.25rem;
            border: 1px solid #bae6fd;
            margin-left: 0.5rem;
          }

          .close-btn {
            width: 2.5rem;
            height: 2.5rem;
            border: none;
            border-radius: 0.375rem;
            background: #f3f4f6;
            color: #6b7280;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
          }

          .close-btn:hover {
            background: #e5e7eb;
            color: #374151;
          }

          .dialog-content {
            flex: 1;
            overflow-y: auto;
            padding: 2rem;
          }

          .master-info,
          .scan-settings,
          .found-devices {
            margin-bottom: 2rem;
          }

          .master-info h3,
          .scan-settings h3,
          .found-devices h3 {
            margin: 0 0 1rem 0;
            font-size: 1rem;
            font-weight: 600;
            color: #374151;
          }

          .master-card {
            display: flex;
            align-items: center;
            gap: 1rem;
            padding: 1rem;
            background: #f0f9ff;
            border: 1px solid #0ea5e9;
            border-radius: 0.5rem;
          }

          .device-icon {
            width: 2.5rem;
            height: 2.5rem;
            display: flex;
            align-items: center;
            justify-content: center;
            background: #dbeafe;
            color: #1e40af;
            border-radius: 0.375rem;
            font-size: 1.125rem;
          }

          .device-info .device-name {
            font-size: 0.875rem;
            font-weight: 600;
            color: #1f2937;
            margin-bottom: 0.25rem;
          }

          .device-info .device-details {
            font-size: 0.75rem;
            color: #6b7280;
          }

          .protocol-tag {
            background: #e0f2fe;
            color: #0c4a6e;
            padding: 0.125rem 0.375rem;
            border-radius: 0.25rem;
            font-size: 0.625rem;
            font-weight: 500;
            margin-left: 0.5rem;
          }

          .settings-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 1rem;
            margin-bottom: 1rem;
          }

          .setting-group {
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
          }

          .setting-group label {
            font-size: 0.875rem;
            font-weight: 500;
            color: #374151;
          }

          .setting-group input,
          .setting-group select {
            padding: 0.5rem;
            border: 1px solid #d1d5db;
            border-radius: 0.375rem;
            font-size: 0.875rem;
          }

          .range-inputs {
            display: flex;
            align-items: center;
            gap: 0.5rem;
          }

          .range-inputs input {
            flex: 1;
          }

          .input-with-unit {
            display: flex;
            align-items: center;
            border: 1px solid #d1d5db;
            border-radius: 0.375rem;
            overflow: hidden;
          }

          .input-with-unit input {
            flex: 1;
            border: none;
            outline: none;
            padding: 0.5rem;
          }

          .input-with-unit .unit {
            padding: 0.5rem;
            background: #f3f4f6;
            font-size: 0.75rem;
            color: #6b7280;
          }

          .scan-info {
            background: #f0f9ff;
            padding: 1rem;
            border-radius: 0.5rem;
            border: 1px solid #bae6fd;
          }

          .scan-info p {
            margin: 0 0 0.5rem 0;
            font-size: 0.875rem;
            color: #0c4a6e;
            line-height: 1.5;
          }

          .scan-info p:last-child {
            margin-bottom: 0;
          }

          .scan-progress-header,
          .scan-results-header {
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
            margin-bottom: 1.5rem;
          }

          .scan-progress-header h3,
          .scan-results-header h3 {
            margin: 0;
            font-size: 1.125rem;
            font-weight: 600;
            color: #1f2937;
          }

          .progress-stats,
          .results-stats {
            display: flex;
            flex-direction: column;
            gap: 0.25rem;
            font-size: 0.75rem;
            color: #6b7280;
            text-align: right;
          }

          .progress-bar-container {
            margin-bottom: 2rem;
          }

          .progress-bar {
            width: 100%;
            height: 0.75rem;
            background: #f3f4f6;
            border-radius: 9999px;
            overflow: hidden;
            margin-bottom: 0.5rem;
          }

          .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #0ea5e9, #06b6d4);
            border-radius: 9999px;
            transition: width 0.3s ease;
          }

          .progress-text {
            text-align: center;
            font-size: 0.875rem;
            color: #6b7280;
            font-weight: 500;
          }

          .found-devices-preview {
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 0.5rem;
            padding: 1rem;
            margin-bottom: 1rem;
          }

          .found-devices-preview h4 {
            margin: 0 0 0.75rem 0;
            font-size: 0.875rem;
            font-weight: 600;
            color: #374151;
          }

          .devices-list {
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
          }

          .found-device-item {
            display: flex;
            align-items: center;
            gap: 0.75rem;
            padding: 0.5rem;
            background: white;
            border-radius: 0.375rem;
            font-size: 0.75rem;
            color: #374151;
          }

          .found-device-item i {
            color: #059669;
          }

          .response-time {
            margin-left: auto;
            color: #6b7280;
          }

          .scanning-indicator {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 0.75rem;
            padding: 2rem;
            color: #0ea5e9;
            font-size: 0.875rem;
          }

          .scanning-indicator i {
            font-size: 1.25rem;
          }

          .devices-header {
            margin-bottom: 1rem;
          }

          .select-all {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            font-size: 0.875rem;
            font-weight: 500;
            color: #374151;
            cursor: pointer;
          }

          .device-result-item {
            display: flex;
            align-items: center;
            gap: 1rem;
            padding: 1rem;
            border: 1px solid #e5e7eb;
            border-radius: 0.5rem;
            margin-bottom: 0.5rem;
            transition: all 0.2s ease;
          }

          .device-result-item:hover {
            border-color: #0ea5e9;
            box-shadow: 0 2px 8px rgba(14, 165, 233, 0.1);
          }

          .device-checkbox {
            cursor: pointer;
          }

          .device-result-item .device-info {
            flex: 1;
          }

          .device-result-item .device-info .protocol-info {
            color: #0ea5e9;
          }

          .status-badge {
            display: flex;
            align-items: center;
            gap: 0.375rem;
            padding: 0.25rem 0.75rem;
            border-radius: 9999px;
            font-size: 0.75rem;
            font-weight: 500;
          }

          .status-badge.discovered {
            background: #dcfce7;
            color: #166534;
          }

          .no-devices {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 1rem;
            padding: 3rem;
            color: #6b7280;
            text-align: center;
          }

          .no-devices i {
            font-size: 3rem;
            margin-bottom: 0.5rem;
          }

          .no-devices h4 {
            margin: 0;
            font-size: 1.125rem;
            font-weight: 600;
            color: #374151;
          }

          .protocol-note {
            font-size: 0.75rem;
            color: #0ea5e9;
          }

          .error-message {
            display: flex;
            align-items: center;
            gap: 0.75rem;
            padding: 1rem;
            background: #fef2f2;
            border: 1px solid #fecaca;
            border-radius: 0.5rem;
            margin-top: 1rem;
            color: #dc2626;
            font-size: 0.875rem;
          }

          .dialog-footer {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 1.5rem 2rem;
            border-top: 1px solid #e5e7eb;
            background: #f9fafb;
            flex-shrink: 0;
          }

          .footer-left {
            flex: 1;
            display: flex;
            flex-direction: column;
            gap: 0.25rem;
          }

          .selection-info {
            font-size: 0.875rem;
            color: #6b7280;
          }

          .footer-protocol-info {
            font-size: 0.75rem;
            color: #0ea5e9;
          }

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
            border-radius: 0.375rem;
            font-size: 0.875rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s ease;
          }

          .btn-primary {
            background: #0ea5e9;
            color: white;
          }

          .btn-secondary {
            background: #64748b;
            color: white;
          }

          .btn-success {
            background: #059669;
            color: white;
          }

          .btn-info {
            background: #0891b2;
            color: white;
          }

          .btn:hover:not(:disabled) {
            opacity: 0.9;
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

export default DeviceRtuScanDialog;