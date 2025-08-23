// ============================================================================
// frontend/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx
// 기본 정보 입력 폼
// ============================================================================

import React from 'react';

interface FormData {
  name: string;
  description: string;
  category: string;
  tags: string[];
  execution_type: 'periodic' | 'on_change' | 'manual';
  execution_interval: number;
  priority: number;
  data_type: 'number' | 'boolean' | 'string';
  unit: string;
  decimal_places: number;
  timeout_ms: number;
  error_handling: 'propagate' | 'default_value' | 'previous_value';
  default_value: any;
  is_enabled: boolean;
  scope_type: 'device' | 'site' | 'global';
  scope_id?: number;
  min_value?: number;
  max_value?: number;
}

interface BasicInfoFormProps {
  data: FormData;
  onChange: (field: keyof FormData, value: any) => void;
  errors: Record<string, string>;
}

const BasicInfoForm: React.FC<BasicInfoFormProps> = ({ data, onChange, errors }) => {
  
  const handleTagsChange = (tagsString: string) => {
    const tags = tagsString.split(',').map(tag => tag.trim()).filter(tag => tag.length > 0);
    onChange('tags', tags);
  };

  return (
    <div className="basic-info-form">
      {/* 기본 정보 섹션 */}
      <div className="form-section">
        <h3 className="section-title">
          <i className="fas fa-info-circle"></i>
          기본 정보
        </h3>
        
        <div className="form-grid">
          <div className="form-group">
            <label className="required">이름</label>
            <input
              type="text"
              value={data.name}
              onChange={(e) => onChange('name', e.target.value)}
              className={`form-input ${errors.name ? 'error' : ''}`}
              placeholder="가상포인트 이름을 입력하세요"
              maxLength={100}
            />
            {errors.name && <span className="error-message">{errors.name}</span>}
          </div>

          <div className="form-group">
            <label className="required">카테고리</label>
            <input
              type="text"
              value={data.category}
              onChange={(e) => onChange('category', e.target.value)}
              className={`form-input ${errors.category ? 'error' : ''}`}
              placeholder="예: 온도, 압력, 유량 등"
              maxLength={50}
            />
            {errors.category && <span className="error-message">{errors.category}</span>}
          </div>

          <div className="form-group full-width">
            <label>설명</label>
            <textarea
              value={data.description}
              onChange={(e) => onChange('description', e.target.value)}
              className="form-textarea"
              placeholder="가상포인트에 대한 설명을 입력하세요"
              rows={3}
              maxLength={500}
            />
          </div>

          <div className="form-group full-width">
            <label>태그</label>
            <input
              type="text"
              value={data.tags.join(', ')}
              onChange={(e) => handleTagsChange(e.target.value)}
              className="form-input"
              placeholder="태그를 쉼표로 구분하여 입력하세요"
            />
            <small className="form-hint">
              예: 온도, 센서, 중요 (쉼표로 구분)
            </small>
          </div>
        </div>
      </div>

      {/* 데이터 타입 섹션 */}
      <div className="form-section">
        <h3 className="section-title">
          <i className="fas fa-database"></i>
          데이터 타입 및 형식
        </h3>
        
        <div className="form-grid">
          <div className="form-group">
            <label className="required">데이터 타입</label>
            <select
              value={data.data_type}
              onChange={(e) => onChange('data_type', e.target.value)}
              className="form-select"
            >
              <option value="number">숫자</option>
              <option value="boolean">참/거짓</option>
              <option value="string">문자열</option>
            </select>
          </div>

          {data.data_type === 'number' && (
            <>
              <div className="form-group">
                <label>단위</label>
                <input
                  type="text"
                  value={data.unit}
                  onChange={(e) => onChange('unit', e.target.value)}
                  className="form-input"
                  placeholder="예: °C, kPa, L/min"
                  maxLength={20}
                />
              </div>

              <div className="form-group">
                <label>소수점 자릿수</label>
                <input
                  type="number"
                  value={data.decimal_places}
                  onChange={(e) => onChange('decimal_places', parseInt(e.target.value) || 0)}
                  className="form-input"
                  min={0}
                  max={10}
                />
              </div>
            </>
          )}
        </div>

        {data.data_type === 'number' && (
          <div className="form-grid">
            <div className="form-group">
              <label>최솟값 (선택)</label>
              <input
                type="number"
                value={data.min_value || ''}
                onChange={(e) => onChange('min_value', e.target.value ? parseFloat(e.target.value) : undefined)}
                className={`form-input ${errors.range ? 'error' : ''}`}
                placeholder="최솟값"
              />
            </div>

            <div className="form-group">
              <label>최댓값 (선택)</label>
              <input
                type="number"
                value={data.max_value || ''}
                onChange={(e) => onChange('max_value', e.target.value ? parseFloat(e.target.value) : undefined)}
                className={`form-input ${errors.range ? 'error' : ''}`}
                placeholder="최댓값"
              />
            </div>

            {errors.range && (
              <div className="form-group full-width">
                <span className="error-message">{errors.range}</span>
              </div>
            )}
          </div>
        )}
      </div>

      {/* 실행 설정 섹션 */}
      <div className="form-section">
        <h3 className="section-title">
          <i className="fas fa-cog"></i>
          실행 설정
        </h3>
        
        <div className="form-grid">
          <div className="form-group">
            <label className="required">실행 방식</label>
            <select
              value={data.execution_type}
              onChange={(e) => onChange('execution_type', e.target.value)}
              className="form-select"
            >
              <option value="periodic">주기적 실행</option>
              <option value="on_change">값 변경 시</option>
              <option value="manual">수동 실행</option>
            </select>
          </div>

          {data.execution_type === 'periodic' && (
            <div className="form-group">
              <label className="required">실행 간격 (ms)</label>
              <input
                type="number"
                value={data.execution_interval}
                onChange={(e) => onChange('execution_interval', parseInt(e.target.value) || 5000)}
                className={`form-input ${errors.execution_interval ? 'error' : ''}`}
                min={1000}
                max={3600000}
                step={1000}
              />
              {errors.execution_interval && (
                <span className="error-message">{errors.execution_interval}</span>
              )}
              <small className="form-hint">
                최소 1초 (1000ms), 권장: 5초 이상
              </small>
            </div>
          )}

          <div className="form-group">
            <label>우선순위</label>
            <input
              type="number"
              value={data.priority}
              onChange={(e) => onChange('priority', parseInt(e.target.value) || 0)}
              className="form-input"
              min={0}
              max={100}
            />
            <small className="form-hint">
              0: 낮음, 50: 보통, 100: 높음
            </small>
          </div>

          <div className="form-group">
            <label>타임아웃 (ms)</label>
            <input
              type="number"
              value={data.timeout_ms}
              onChange={(e) => onChange('timeout_ms', parseInt(e.target.value) || 10000)}
              className="form-input"
              min={1000}
              max={60000}
              step={1000}
            />
          </div>
        </div>
      </div>

      {/* 오류 처리 섹션 */}
      <div className="form-section">
        <h3 className="section-title">
          <i className="fas fa-exclamation-triangle"></i>
          오류 처리
        </h3>
        
        <div className="form-grid">
          <div className="form-group">
            <label>오류 처리 방식</label>
            <select
              value={data.error_handling}
              onChange={(e) => onChange('error_handling', e.target.value)}
              className="form-select"
            >
              <option value="propagate">오류 전파</option>
              <option value="default_value">기본값 사용</option>
              <option value="previous_value">이전값 유지</option>
            </select>
          </div>

          {data.error_handling === 'default_value' && (
            <div className="form-group">
              <label>기본값</label>
              <input
                type={data.data_type === 'number' ? 'number' : 'text'}
                value={data.default_value}
                onChange={(e) => {
                  const value = data.data_type === 'number' 
                    ? parseFloat(e.target.value) || 0
                    : data.data_type === 'boolean'
                    ? e.target.value === 'true'
                    : e.target.value;
                  onChange('default_value', value);
                }}
                className="form-input"
                placeholder="오류 시 사용할 기본값"
              />
            </div>
          )}
        </div>
      </div>

      {/* 범위 설정 섹션 */}
      <div className="form-section">
        <h3 className="section-title">
          <i className="fas fa-layer-group"></i>
          범위 설정
        </h3>
        
        <div className="form-grid">
          <div className="form-group">
            <label>적용 범위</label>
            <select
              value={data.scope_type}
              onChange={(e) => onChange('scope_type', e.target.value)}
              className="form-select"
            >
              <option value="device">디바이스</option>
              <option value="site">사이트</option>
              <option value="global">전역</option>
            </select>
          </div>

          {data.scope_type !== 'global' && (
            <div className="form-group">
              <label>범위 ID</label>
              <input
                type="number"
                value={data.scope_id || ''}
                onChange={(e) => onChange('scope_id', e.target.value ? parseInt(e.target.value) : undefined)}
                className="form-input"
                placeholder="해당 범위의 ID"
              />
            </div>
          )}

          <div className="form-group">
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={data.is_enabled}
                onChange={(e) => onChange('is_enabled', e.target.checked)}
                className="form-checkbox"
              />
              활성화
            </label>
          </div>
        </div>
      </div>
    </div>
  );
};

export default BasicInfoForm;