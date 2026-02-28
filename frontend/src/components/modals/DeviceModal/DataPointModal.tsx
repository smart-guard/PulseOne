// ============================================================================
// frontend/src/components/modals/DataPointModal.tsx
// 📊 데이터포인트 생성/편집 모달
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DataPoint, DataPointCreateData, DataPointUpdateData, DataPointApiService } from '../../../api/services/dataPointApi';

interface DataPointModalProps {
  isOpen: boolean;
  mode: 'create' | 'edit' | 'view';
  deviceId: number;
  dataPoint?: DataPoint | null;
  onClose: () => void;
  onSave: (dataPoint: DataPoint) => void;
  onDelete?: (pointId: number) => void;
}

const DataPointModal: React.FC<DataPointModalProps> = ({
  isOpen,
  mode,
  deviceId,
  dataPoint,
  onClose,
  onSave,
  onDelete
}) => {
  const [formData, setFormData] = useState<DataPointCreateData | DataPointUpdateData>({
    device_id: deviceId,
    name: '',
    description: '',
    address: '',
    address_string: '',
    data_type: 'uint16',
    access_mode: 'read',
    is_enabled: true,
    is_writable: false,
    unit: '',
    scaling_factor: 1,
    scaling_offset: 0,
    min_value: 0,
    max_value: 65535,
    is_log_enabled: true,
    log_interval_ms: 5000,
    log_deadband: 0,
    polling_interval_ms: 1000,
    group_name: '',
    tags: [],
    metadata: {},
    protocol_params: {}
  });

  const [isLoading, setIsLoading] = useState(false);
  const [isTestingRead, setIsTestingRead] = useState(false);
  const [isTestingWrite, setIsTestingWrite] = useState(false);
  const [testValue, setTestValue] = useState('');
  const [testResult, setTestResult] = useState<string | null>(null);
  const [errors, setErrors] = useState<Record<string, string>>({});

  // 폼 데이터 초기화
  useEffect(() => {
    if (isOpen) {
      if (mode === 'create') {
        setFormData({
          device_id: deviceId,
          name: '',
          description: '',
          address: '',
          address_string: '',
          data_type: 'uint16',
          access_mode: 'read',
          is_enabled: true,
          is_writable: false,
          unit: '',
          scaling_factor: 1,
          scaling_offset: 0,
          min_value: 0,
          max_value: 65535,
          is_log_enabled: true,
          log_interval_ms: 5000,
          log_deadband: 0,
          polling_interval_ms: 1000,
          group_name: '',
          tags: [],
          metadata: {},
          protocol_params: {}
        });
      } else if (dataPoint) {
        setFormData({
          name: dataPoint.name,
          description: dataPoint.description || '',
          address: dataPoint.address,
          address_string: dataPoint.address_string || '',
          data_type: dataPoint.data_type,
          access_mode: dataPoint.access_mode || 'read',
          is_enabled: dataPoint.is_enabled,
          is_writable: dataPoint.is_writable || false,
          unit: dataPoint.unit || '',
          scaling_factor: dataPoint.scaling_factor || 1,
          scaling_offset: dataPoint.scaling_offset || 0,
          min_value: dataPoint.min_value || 0,
          max_value: dataPoint.max_value || 65535,
          is_log_enabled: dataPoint.is_log_enabled !== false,
          log_interval_ms: dataPoint.log_interval_ms || 5000,
          log_deadband: dataPoint.log_deadband || 0,
          polling_interval_ms: dataPoint.polling_interval_ms || 1000,
          group_name: dataPoint.group_name || '',
          tags: dataPoint.tags || [],
          metadata: dataPoint.metadata || {},
          protocol_params: dataPoint.protocol_params || {}
        });
      }
      setErrors({});
      setTestResult(null);
    }
  }, [isOpen, mode, dataPoint, deviceId]);

  // 폼 필드 업데이트
  const updateField = (field: string, value: any) => {
    setFormData(prev => ({ ...prev, [field]: value }));
    if (errors[field]) {
      setErrors(prev => ({ ...prev, [field]: '' }));
    }
  };

  // 유효성 검사
  const validateForm = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!formData.name?.trim()) {
      newErrors.name = '포인트명은 필수입니다';
    }

    if (!formData.address?.trim()) {
      newErrors.address = '주소는 필수입니다';
    }

    if (!formData.data_type) {
      newErrors.data_type = '데이터 타입은 필수입니다';
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  // 저장 처리
  const handleSave = async () => {
    if (!validateForm()) return;

    try {
      setIsLoading(true);
      let savedDataPoint: DataPoint;

      if (mode === 'create') {
        const response = await DataPointApiService.createDataPoint(deviceId, formData as DataPointCreateData);
        if (response.success && response.data) {
          savedDataPoint = response.data;
        } else {
          throw new Error(response.error || '생성 실패');
        }
      } else if (mode === 'edit' && dataPoint) {
        const response = await DataPointApiService.updateDataPoint(deviceId, dataPoint.id, formData as DataPointUpdateData);
        if (response.success && response.data) {
          savedDataPoint = response.data;
        } else {
          throw new Error(response.error || '수정 실패');
        }
      } else {
        return;
      }

      onSave(savedDataPoint);
      onClose();
    } catch (error) {
      console.error('데이터포인트 저장 실패:', error);
      alert(`저장 실패: ${error.message}`);
    } finally {
      setIsLoading(false);
    }
  };

  // 삭제 처리
  const handleDelete = async () => {
    if (!dataPoint || !onDelete) return;

    if (confirm(`"${dataPoint.name}" 데이터포인트를 삭제하시겠습니까?`)) {
      try {
        setIsLoading(true);
        const response = await DataPointApiService.deleteDataPoint(deviceId, dataPoint.id);
        if (response.success) {
          onDelete(dataPoint.id);
          onClose();
        } else {
          throw new Error(response.error || '삭제 실패');
        }
      } catch (error) {
        console.error('데이터포인트 삭제 실패:', error);
        alert(`삭제 실패: ${error.message}`);
      } finally {
        setIsLoading(false);
      }
    }
  };

  // 읽기 테스트
  const handleTestRead = async () => {
    if (!dataPoint) return;

    try {
      setIsTestingRead(true);
      setTestResult(null);

      const response = await DataPointApiService.testDataPointRead(dataPoint.id);

      if (response.success && response.data) {
        const result = response.data;
        if (result.test_successful) {
          setTestResult(`✅ 읽기 성공: ${result.current_value} (응답시간: ${result.response_time_ms}ms)`);
        } else {
          setTestResult(`❌ 읽기 실패: ${result.error_message}`);
        }
      } else {
        setTestResult(`❌ 테스트 실패: ${response.error}`);
      }
    } catch (error) {
      setTestResult(`❌ 테스트 오류: ${error.message}`);
    } finally {
      setIsTestingRead(false);
    }
  };

  // 쓰기 테스트
  const handleTestWrite = async () => {
    if (!dataPoint || !testValue) return;

    try {
      setIsTestingWrite(true);
      setTestResult(null);

      const value = parseFloat(testValue) || testValue;
      const response = await DataPointApiService.testDataPointWrite(dataPoint.id, value);

      if (response.success && response.data) {
        const result = response.data;
        if (result.test_successful) {
          setTestResult(`✅ 쓰기 성공: ${result.written_value} (응답시간: ${result.response_time_ms}ms)`);
        } else {
          setTestResult(`❌ 쓰기 실패: ${result.error_message}`);
        }
      } else {
        setTestResult(`❌ 테스트 실패: ${response.error}`);
      }
    } catch (error) {
      setTestResult(`❌ 테스트 오류: ${error.message}`);
    } finally {
      setIsTestingWrite(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-container datapoint-modal">
        {/* 헤더 */}
        <div className="modal-header">
          <h2>
            {mode === 'create' ? '새 데이터포인트 추가' :
              mode === 'edit' ? '데이터포인트 편집' : '데이터포인트 상세'}
          </h2>
          <button className="close-btn" onClick={onClose}>
            <i className="fas fa-times"></i>
          </button>
        </div>

        {/* 내용 */}
        <div className="modal-content">
          <div className="form-grid">
            {/* 기본 정보 */}
            <div className="form-section">
              <h3>📊 기본 정보</h3>

              <div className="form-group">
                <label>포인트명 *</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.name}</div>
                ) : (
                  <>
                    <input
                      type="text"
                      value={formData.name || ''}
                      onChange={(e) => updateField('name', e.target.value)}
                      placeholder="데이터포인트 이름"
                      className={errors.name ? 'error' : ''}
                    />
                    {errors.name && <span className="error-text">{errors.name}</span>}
                  </>
                )}
              </div>

              <div className="form-group">
                <label>설명</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.description || '없음'}</div>
                ) : (
                  <textarea
                    value={formData.description || ''}
                    onChange={(e) => updateField('description', e.target.value)}
                    placeholder="데이터포인트 설명"
                    rows={2}
                  />
                )}
              </div>

              <div className="form-group">
                <label>주소 *</label>
                {mode === 'view' ? (
                  <div className="form-value address">{dataPoint?.address}</div>
                ) : (
                  <>
                    <input
                      type="text"
                      value={formData.address || ''}
                      onChange={(e) => updateField('address', e.target.value)}
                      placeholder="예: 40001, 1001, 0x1000"
                      className={errors.address ? 'error' : ''}
                    />
                    {errors.address && <span className="error-text">{errors.address}</span>}
                  </>
                )}
              </div>

              <div className="form-group">
                <label>주소 문자열</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.address_string || '없음'}</div>
                ) : (
                  <input
                    type="text"
                    value={formData.address_string || ''}
                    onChange={(e) => updateField('address_string', e.target.value)}
                    placeholder="예: 4:40001, MB:1001"
                  />
                )}
              </div>

              <div className="form-group">
                <label>데이터 타입 *</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.data_type}</div>
                ) : (
                  <select
                    value={formData.data_type || 'uint16'}
                    onChange={(e) => updateField('data_type', e.target.value)}
                    className={errors.data_type ? 'error' : ''}
                  >
                    <option value="bool">Boolean</option>
                    <option value="uint8">UInt8</option>
                    <option value="int8">Int8</option>
                    <option value="uint16">UInt16</option>
                    <option value="int16">Int16</option>
                    <option value="uint32">UInt32</option>
                    <option value="int32">Int32</option>
                    <option value="uint64">UInt64</option>
                    <option value="int64">Int64</option>
                    <option value="float32">Float32</option>
                    <option value="float64">Float64</option>
                    <option value="string">String</option>
                    <option value="bytes">Bytes</option>
                  </select>
                )}
              </div>

              <div className="form-group">
                <label>비트 인덱스 (선택, 0~15)</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.protocol_params?.bit_index ?? '없음'}</div>
                ) : (
                  <input
                    type="number"
                    min="0"
                    max="15"
                    value={formData.protocol_params?.bit_index || ''}
                    onChange={(e) => {
                      const value = e.target.value;
                      const newParams = { ...formData.protocol_params };
                      if (value === '') {
                        delete newParams.bit_index; // 빈 값이면 삭제
                      } else {
                        newParams.bit_index = value;
                      }
                      updateField('protocol_params', newParams);
                    }}
                    placeholder="Register 비트 추출용 (예: 0)"
                  />
                )}
              </div>

              <div className="form-group">
                <label>접근 모드</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.access_mode || 'read'}</div>
                ) : (
                  <select
                    value={formData.access_mode || 'read'}
                    onChange={(e) => updateField('access_mode', e.target.value)}
                  >
                    <option value="read">읽기 전용</option>
                    <option value="write">쓰기 전용</option>
                    <option value="read_write">읽기/쓰기</option>
                  </select>
                )}
              </div>

              <div className="form-group">
                <label>단위</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.unit || '없음'}</div>
                ) : (
                  <input
                    type="text"
                    value={formData.unit || ''}
                    onChange={(e) => updateField('unit', e.target.value)}
                    placeholder="예: °C, kWh, m/s"
                  />
                )}
              </div>
            </div>

            {/* 스케일링 및 범위 */}
            <div className="form-section">
              <h3>📐 스케일링 및 범위</h3>

              <div className="form-group">
                <label>스케일링 팩터</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.scaling_factor || 1}</div>
                ) : (
                  <input
                    type="number"
                    step="0.001"
                    value={formData.scaling_factor || 1}
                    onChange={(e) => updateField('scaling_factor', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>스케일링 오프셋</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.scaling_offset || 0}</div>
                ) : (
                  <input
                    type="number"
                    step="0.001"
                    value={formData.scaling_offset || 0}
                    onChange={(e) => updateField('scaling_offset', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>최솟값</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.min_value || 0}</div>
                ) : (
                  <input
                    type="number"
                    value={formData.min_value || 0}
                    onChange={(e) => updateField('min_value', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>최댓값</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.max_value || 65535}</div>
                ) : (
                  <input
                    type="number"
                    value={formData.max_value || 65535}
                    onChange={(e) => updateField('max_value', parseFloat(e.target.value))}
                  />
                )}
              </div>
            </div>

            {/* 로깅 및 폴링 설정 */}
            <div className="form-section">
              <h3>📝 로깅 및 폴링</h3>

              <div className="form-group">
                <label>폴링 간격 (ms)</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.polling_interval_ms || 1000}</div>
                ) : (
                  <input
                    type="number"
                    min="100"
                    max="3600000"
                    value={formData.polling_interval_ms || 1000}
                    onChange={(e) => updateField('polling_interval_ms', parseInt(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>로그 간격 (ms)</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.log_interval_ms || 5000}</div>
                ) : (
                  <input
                    type="number"
                    min="1000"
                    max="3600000"
                    value={formData.log_interval_ms || 5000}
                    onChange={(e) => updateField('log_interval_ms', parseInt(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>데드밴드</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.log_deadband || 0}</div>
                ) : (
                  <input
                    type="number"
                    step="0.001"
                    min="0"
                    value={formData.log_deadband || 0}
                    onChange={(e) => updateField('log_deadband', parseFloat(e.target.value))}
                  />
                )}
              </div>

              <div className="form-group">
                <label>그룹명</label>
                {mode === 'view' ? (
                  <div className="form-value">{dataPoint?.group_name || '없음'}</div>
                ) : (
                  <input
                    type="text"
                    value={formData.group_name || ''}
                    onChange={(e) => updateField('group_name', e.target.value)}
                    placeholder="데이터포인트 그룹"
                  />
                )}
              </div>
            </div>

            {/* 상태 및 옵션 */}
            <div className="form-section">
              <h3>⚙️ 상태 및 옵션</h3>

              <div className="form-group">
                <label>활성화</label>
                {mode === 'view' ? (
                  <div className="form-value">
                    <span className={`status-badge ${dataPoint?.is_enabled ? 'enabled' : 'disabled'}`}>
                      {dataPoint?.is_enabled ? '활성화' : '비활성화'}
                    </span>
                  </div>
                ) : (
                  <label className="switch">
                    <input
                      type="checkbox"
                      checked={formData.is_enabled || false}
                      onChange={(e) => updateField('is_enabled', e.target.checked)}
                    />
                    <span className="slider"></span>
                  </label>
                )}
              </div>

              <div className="form-group">
                <label>쓰기 가능</label>
                {mode === 'view' ? (
                  <div className="form-value">
                    <span className={`status-badge ${dataPoint?.is_writable ? 'enabled' : 'disabled'}`}>
                      {dataPoint?.is_writable ? '가능' : '불가능'}
                    </span>
                  </div>
                ) : (
                  <label className="switch">
                    <input
                      type="checkbox"
                      checked={formData.is_writable || false}
                      onChange={(e) => updateField('is_writable', e.target.checked)}
                    />
                    <span className="slider"></span>
                  </label>
                )}
              </div>

              <div className="form-group">
                <label>로깅 활성화</label>
                {mode === 'view' ? (
                  <div className="form-value">
                    <span className={`status-badge ${dataPoint?.is_log_enabled ? 'enabled' : 'disabled'}`}>
                      {dataPoint?.is_log_enabled ? '활성화' : '비활성화'}
                    </span>
                  </div>
                ) : (
                  <label className="switch">
                    <input
                      type="checkbox"
                      checked={formData.is_log_enabled !== false}
                      onChange={(e) => updateField('is_log_enabled', e.target.checked)}
                    />
                    <span className="slider"></span>
                  </label>
                )}
              </div>
            </div>

            {/* 현재값 및 테스트 (view/edit 모드에서만) */}
            {mode !== 'create' && dataPoint && (
              <div className="form-section">
                <h3>📊 현재값 및 테스트</h3>

                <div className="form-group">
                  <label>현재값</label>
                  <div className="form-value current-value">
                    {dataPoint.current_value !== null && dataPoint.current_value !== undefined
                      ? (typeof dataPoint.current_value === 'object' ? dataPoint.current_value.value : dataPoint.current_value)
                      : 'N/A'}
                  </div>
                </div>

                <div className="form-group">
                  <label>품질</label>
                  <div className="form-value">
                    <span className={`quality-badge quality-${dataPoint.quality?.toLowerCase() || 'unknown'}`}>
                      {dataPoint.quality || 'Unknown'}
                    </span>
                  </div>
                </div>

                <div className="form-group">
                  <label>마지막 읽기</label>
                  <div className="form-value">
                    {dataPoint.last_update ? new Date(dataPoint.last_update).toLocaleString() : 'N/A'}
                  </div>
                </div>

                {/* 테스트 버튼들 */}
                <div className="test-actions">
                  <button
                    type="button"
                    className="btn btn-info btn-sm"
                    onClick={handleTestRead}
                    disabled={isTestingRead}
                  >
                    {isTestingRead ? (
                      <>
                        <i className="fas fa-spinner fa-spin"></i>
                        읽기 테스트 중...
                      </>
                    ) : (
                      <>
                        <i className="fas fa-download"></i>
                        읽기 테스트
                      </>
                    )}
                  </button>

                  {(formData.is_writable || dataPoint.is_writable) && (
                    <div className="write-test">
                      <div className="input-group">
                        <input
                          type="text"
                          value={testValue}
                          onChange={(e) => setTestValue(e.target.value)}
                          placeholder="테스트 값 입력"
                          className="test-input"
                        />
                        <button
                          type="button"
                          className="btn btn-warning btn-sm"
                          onClick={handleTestWrite}
                          disabled={isTestingWrite || !testValue}
                        >
                          {isTestingWrite ? (
                            <>
                              <i className="fas fa-spinner fa-spin"></i>
                              쓰기 중...
                            </>
                          ) : (
                            <>
                              <i className="fas fa-upload"></i>
                              쓰기 테스트
                            </>
                          )}
                        </button>
                      </div>
                    </div>
                  )}
                </div>

                {/* 테스트 결과 */}
                {testResult && (
                  <div className="test-result">
                    {testResult}
                  </div>
                )}
              </div>
            )}
          </div>
        </div>

        {/* 푸터 */}
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

        <style>{`
          .datapoint-modal {
            max-width: 800px;
            width: 90vw;
            max-height: 90vh;
          }

          .form-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 2rem;
            padding: 1rem;
          }

          .form-section {
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 1.5rem;
          }

          .form-section h3 {
            margin: 0 0 1rem 0;
            font-size: 1rem;
            font-weight: 600;
            color: #1f2937;
          }

          .form-group {
            margin-bottom: 1rem;
          }

          .form-group label {
            display: block;
            font-size: 0.875rem;
            font-weight: 500;
            color: #374151;
            margin-bottom: 0.5rem;
          }

          .form-group input,
          .form-group select,
          .form-group textarea {
            width: 100%;
            padding: 0.5rem;
            border: 1px solid #d1d5db;
            border-radius: 4px;
            font-size: 0.875rem;
          }

          .form-group input.error,
          .form-group select.error {
            border-color: #dc2626;
          }

          .form-value {
            padding: 0.5rem;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
            border-radius: 4px;
            font-size: 0.875rem;
            color: #374151;
          }

          .form-value.address {
            font-family: 'Courier New', monospace;
            background: #f0f9ff;
            border-color: #0ea5e9;
            color: #0c4a6e;
          }

          .form-value.current-value {
            font-weight: 600;
            color: #059669;
          }

          .error-text {
            color: #dc2626;
            font-size: 0.75rem;
            margin-top: 0.25rem;
            display: block;
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

          .quality-badge {
            padding: 0.25rem 0.5rem;
            border-radius: 4px;
            font-size: 0.75rem;
            font-weight: 500;
          }

          .quality-good {
            background: #dcfce7;
            color: #166534;
          }

          .quality-bad {
            background: #fee2e2;
            color: #991b1b;
          }

          .quality-uncertain {
            background: #fef3c7;
            color: #92400e;
          }

          .quality-unknown {
            background: #f3f4f6;
            color: #6b7280;
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

          .test-actions {
            margin-top: 1rem;
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
          }

          .write-test {
            margin-top: 0.5rem;
          }

          .input-group {
            display: flex;
            gap: 0.5rem;
          }

          .test-input {
            flex: 1;
          }

          .test-result {
            margin-top: 0.5rem;
            padding: 0.5rem;
            border-radius: 4px;
            font-size: 0.875rem;
            font-family: 'Courier New', monospace;
            background: #f0f9ff;
            border: 1px solid #0ea5e9;
            color: #0c4a6e;
          }

          .btn {
            display: inline-flex;
            align-items: center;
            gap: 0.5rem;
            padding: 0.5rem 1rem;
            border: none;
            border-radius: 4px;
            font-size: 0.875rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s ease;
          }

          .btn-sm {
            padding: 0.375rem 0.75rem;
            font-size: 0.75rem;
          }

          .btn-primary {
            background: #0ea5e9;
            color: white;
          }

          .btn-secondary {
            background: #64748b;
            color: white;
          }

          .btn-info {
            background: #0891b2;
            color: white;
          }

          .btn-warning {
            background: #f59e0b;
            color: white;
          }

          .btn-error {
            background: #dc2626;
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

export default DataPointModal;