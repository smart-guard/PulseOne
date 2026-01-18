// ============================================================================
// BasicInfoForm.tsx - Clarity & Guidance Focused Redesign
// ============================================================================

import React from 'react';
import { ScopeSelector } from './ScopeSelector';

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
  onApplyTemplate: (template: Partial<FormData>) => void;
  errors: Record<string, string>;
}

const BasicInfoForm: React.FC<BasicInfoFormProps> = ({ data, onChange, onApplyTemplate, errors }) => {

  const categoryPresets = ['에너지', '생산량', '설비 효율', '환경', '품질', '전력'];
  const intervalPresets = [
    { label: 'High (1s)', value: 1000 },
    { label: 'Normal (5s)', value: 5000 },
    { label: 'Batch (1m)', value: 60000 }
  ];

  const unitPresets = ['kWh', 'MWh', '℃', '%', 'kW', 'V', 'A', 'Hz'];

  const templates = [
    {
      name: '단순 합산 (Totalizer)',
      category: '에너지',
      expression: 'SUM(a, b, c)',
      unit: 'kWh',
      data_type: 'number' as const,
      description: '여러 포인트의 전력 소무량을 합산하여 총 사용량을 계산합니다.'
    },
    {
      name: '생산 효율 (Efficiency)',
      category: '생산량',
      expression: '(actual / target) * 100',
      unit: '%',
      data_type: 'number' as const,
      description: '목표 대비 실제 생산 실적을 백분율로 환산합니다.'
    },
    {
      name: '임계치 알림 (Overload)',
      category: '전력',
      expression: 'current > limit',
      unit: '',
      data_type: 'boolean' as const,
      description: '전류/전력값이 설정된 임계치를 넘었는지 감시합니다.'
    },
    {
      name: '에너지 원단위 (SEC)',
      category: '에너지',
      expression: 'total_energy / prod_count',
      unit: 'kWh/unit',
      data_type: 'number' as const,
      description: '제품 하나를 만드는 데 들어간 에너지 양을 계산합니다.'
    },
    {
      name: '장비 성적계수 (COP)',
      category: '설비 효율',
      expression: 'thermal_output / electrical_input',
      unit: 'ratio',
      data_type: 'number' as const,
      description: '냉동기나 히트펌프의 투입 에너지 대비 성능을 측정합니다.'
    },
    {
      name: '탄소 배출량 (CO2)',
      category: '환경',
      expression: 'kWh_usage * 0.4663',
      unit: 'kgCO2',
      data_type: 'number' as const,
      description: '전력 사용량을 탄소 배출량으로 환산합니다. (한국 전력 배출계수 기준)'
    },
    {
      name: '가동률 (Availability)',
      category: '설비 효율',
      expression: '(run_min / total_min) * 100',
      unit: '%',
      data_type: 'number' as const,
      description: '전체 시간 대비 설비가 실제로 가동된 시간의 비중을 계산합니다.'
    },
    {
      name: '차이 계산 (Delta)',
      category: '환경',
      expression: 'inlet_temp - outlet_temp',
      unit: '℃',
      data_type: 'number' as const,
      description: '입구와 출구, 혹은 두 지점 간의 데이터 차이를 계산합니다.'
    }
  ];

  return (
    <div className="wizard-slide-active" style={{ width: '100%' }}>
      <div className="dashboard-grid-v3">

        <div className="dashboard-v3-column" style={{ gridColumn: 'span 3', marginBottom: '16px' }}>
          <div className="concept-group-card v3-compact" style={{ flexDirection: 'row', alignItems: 'center', gap: '16px', borderLeft: '4px solid #8b5cf6' }}>
            <div style={{ flex: 1 }}>
              <div style={{ fontSize: '13px', fontWeight: 800, color: '#4c1d95' }}>
                <i className="fas fa-magic"></i> 빠른 시작: 템플릿 불러오기
              </div>
              <p style={{ fontSize: '11px', color: '#7c3aed', margin: 0 }}>업종별 표준 설정을 한 번의 클릭으로 적용하세요.</p>
            </div>
            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', justifyContent: 'flex-end' }}>
              {templates.map(t => (
                <button
                  key={t.name}
                  className="choice-btn-v3"
                  style={{ fontSize: '11px', padding: '6px 12px', border: '1px solid #ddd' }}
                  onClick={() => onApplyTemplate(t)}
                >
                  {t.name.split(' (')[0]}
                </button>
              ))}
            </div>
          </div>
        </div>

        {/* Column 1: Identity (Wide - 520px) */}
        <div className="dashboard-v3-column">
          <div className="concept-group-card v3-compact" style={{ flex: 1 }}>
            <div className="concept-title">
              <i className="fas fa-id-card"></i> 1단계: 기본 식별
            </div>

            <div className="v-form-group">
              <div className="label">가상포인트 이름</div>
              <input
                type="text"
                value={data.name}
                onChange={(e) => onChange('name', e.target.value)}
                className={`form-input ${errors.name ? 'error' : ''}`}
                placeholder="예: 제1공장 라인A 전력합계"
                style={{ fontSize: '15px', fontWeight: 600 }}
              />
              {errors.name && <span className="error-message">{errors.name}</span>}
              <p style={{ fontSize: '12px', color: '#94a3b8', margin: 0 }}>다른 포인트와 중복되지 않는 고유한 이름을 권장합니다.</p>
            </div>

            <div className="v-form-group">
              <div className="label">분류 카테고리</div>
              <div className="choice-group-v3" style={{ gap: '6px' }}>
                {categoryPresets.map(cat => (
                  <button
                    key={cat}
                    className={`choice-btn-v3 ${data.category === cat ? 'active' : ''}`}
                    onClick={() => onChange('category', cat)}
                    style={{ padding: '8px 14px', whiteSpace: 'nowrap' }}
                  >
                    {cat}
                  </button>
                ))}
              </div>
              <input
                type="text"
                value={data.category}
                onChange={(e) => onChange('category', e.target.value)}
                className="form-input"
                placeholder="새 카테고리 직접 입력..."
                style={{ marginTop: '4px' }}
              />
            </div>

            <div className="v-form-group" style={{ flex: 1 }}>
              <div className="label">설명 및 비고</div>
              <textarea
                value={data.description}
                onChange={(e) => onChange('description', e.target.value)}
                className="form-textarea"
                style={{ height: '100%', minHeight: '120px' }}
                placeholder="해당 가상포인트의 계산 목적이나 데이터 소스에 대한 설명을 입력하세요."
              />
            </div>
          </div>
        </div>

        {/* Column 2: Technical Config (Narrow - 280px) */}
        <div className="dashboard-v3-column">
          {/* Data Model */}
          <div className="concept-group-card v3-compact">
            <div className="concept-title">
              <i className="fas fa-database"></i> 2단계: 데이터 형식
            </div>

            <div className="v-form-group">
              <div className="label">데이터 형식</div>
              <div className="choice-group-v3" style={{ width: '100%' }}>
                <button className={`choice-btn-v3 ${data.data_type === 'number' ? 'active' : ''}`} onClick={() => onChange('data_type', 'number')} style={{ flex: 1 }}>
                  <i className="fas fa-hashtag"></i> Number
                </button>
                <button className={`choice-btn-v3 ${data.data_type === 'boolean' ? 'active' : ''}`} onClick={() => onChange('data_type', 'boolean')} style={{ flex: 1 }}>
                  <i className="fas fa-toggle-on"></i> Bool
                </button>
              </div>

              {data.data_type === 'number' && (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', marginTop: '8px' }}>
                  <div style={{ display: 'flex', gap: '8px' }}>
                    <input type="text" value={data.unit} onChange={(e) => onChange('unit', e.target.value)} className="form-input" placeholder="단위 (예: kWh)" style={{ width: '100px' }} />
                    <input type="number" value={data.decimal_places} onChange={(e) => onChange('decimal_places', parseInt(e.target.value) || 0)} className="form-input" placeholder="소수점" style={{ width: '70px' }} />
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                    {unitPresets.map(u => (
                      <button
                        key={u}
                        className={`choice-btn-v3 ${data.unit === u ? 'active' : ''}`}
                        style={{ fontSize: '10px', padding: '4px 8px' }}
                        onClick={() => onChange('unit', u)}
                      >
                        {u}
                      </button>
                    ))}
                  </div>
                </div>
              )}
            </div>
          </div>

          {/* Execution Strategy */}
          <div className="concept-group-card v3-compact" style={{ flex: 1 }}>
            <div className="concept-title">
              <i className="fas fa-cog"></i> 3단계: 실행 조건
            </div>

            <div className="v-form-group">
              <div className="label">계산 시점</div>
              <div className="choice-group-v3" style={{ width: '100%' }}>
                <button className={`choice-btn-v3 ${data.execution_type === 'periodic' ? 'active' : ''}`} onClick={() => onChange('execution_type', 'periodic')} style={{ flex: 1 }}>
                  <i className="fas fa-redo"></i> 주기적
                </button>
                <button className={`choice-btn-v3 ${data.execution_type === 'on_change' ? 'active' : ''}`} onClick={() => onChange('execution_type', 'on_change')} style={{ flex: 1 }}>
                  <i className="fas fa-bolt"></i> 변경 시
                </button>
              </div>
            </div>

            {data.execution_type === 'periodic' && (
              <div className="v-form-group">
                <div className="label">주기 설정</div>
                <div className="choice-group-v3" style={{ gap: '4px', marginBottom: '4px' }}>
                  {intervalPresets.map(p => (
                    <button key={p.value} className={`choice-btn-v3 ${data.execution_interval === p.value ? 'active' : ''}`} onClick={() => onChange('execution_interval', p.value)} style={{ padding: '6px 8px', fontSize: '11px', flex: 1 }}>
                      {p.label.split(' ')[0]}
                    </button>
                  ))}
                </div>
                <input type="number" value={data.execution_interval} onChange={(e) => onChange('execution_interval', parseInt(e.target.value) || 1000)} className="form-input" style={{ maxWidth: '180px' }} />
              </div>
            )}

            <div style={{ marginTop: 'auto', padding: '12px', background: '#f8fafc', borderRadius: '8px', fontSize: '11px', color: '#94a3b8', lineHeight: '1.4' }}>
              <i className="fas fa-info-circle"></i> 대용량 데이터 처리 시 <strong>'변경 시'</strong> 옵션이 서버 부하 감소에 유리합니다.
            </div>
          </div>
        </div>

        {/* Column 3: Guide & Samples (Flexible/Wide) */}
        <div className="dashboard-v3-column">
          <div className="guidance-panel">
            <div className="guidance-header">
              <i className="fas fa-book-open" style={{ color: 'var(--blueprint-500)' }}></i>
              가상포인트 구성 완벽 가이드
            </div>

            <div className="guidance-step">
              <div className="guidance-num">1</div>
              <div className="guidance-text">
                <p><strong>네이밍 규칙</strong>: '단위_측정치_위치' (예: kW_Total_Line1) 형태로 작성하면 관리 효율이 극대화됩니다.</p>
              </div>
            </div>

            <div className="guidance-step">
              <div className="guidance-num">2</div>
              <div className="guidance-text">
                <p><strong>데이터 타입</strong>: 수치 연산에는 <code>Number</code>를, 설비 가동/정지 등 상태값에는 <code>Boolean</code>을 사용하세요.</p>
              </div>
            </div>

            <div className="guidance-step">
              <div className="guidance-num">3</div>
              <div className="guidance-text">
                <p><strong>실행 전략</strong>: 실시간성 보다는 데이터의 정확도와 서버 안정성을 고려하여 주기를 설정하세요.</p>
              </div>
            </div>

            {/* Practical Samples Section */}
            <div className="guidance-samples">
              <div style={{ fontSize: '13px', fontWeight: 800, color: '#1e2937', marginBottom: '12px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <i className="fas fa-vial" style={{ color: '#8b5cf6' }}></i> 실무 설정 샘플 (Best Practice)
              </div>

              <div className="sample-case">
                <div className="sample-case-title">
                  <span className="sample-tag blue">Number</span> 에너지 효율 지수
                </div>
                <div className="sample-case-desc">
                  생산량 대비 전력 사용량 계산. <br />
                  - 단위: kWh/unit, 소수점: 2자리 <br />
                  - 실행: 입력 데이터 변경 시 즉시 계산
                </div>
              </div>

              <div className="sample-case">
                <div className="sample-case-title">
                  <span className="sample-tag green">Boolean</span> 설비 과부하 여부
                </div>
                <div className="sample-case-desc">
                  전류값이 임계치를 넘었는지 판단. <br />
                  - 단위: 없음 <br />
                  - 실행: 1초(High) 주기로 지속 체크
                </div>
              </div>

              <div style={{ marginTop: '16px', fontSize: '12px', color: '#0369a1', background: '#e0f2fe', padding: '10px', borderRadius: '8px', lineHeight: '1.4' }}>
                <i className="fas fa-lightbulb"></i> <strong>Tip</strong>: 다음 단계에서 수식을 작성할 때 이 설정들이 데이터의 틀이 됩니다.
              </div>
            </div>
          </div>
        </div>

      </div>
    </div>
  );
};

export default BasicInfoForm;