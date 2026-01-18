import React, { useState, useEffect, useMemo } from 'react';
import alarmTemplatesApi, {
  AlarmTemplate,
  DataPoint,
  ApplyTemplateRequest
} from '../../api/services/alarmTemplatesApi';
import '../../styles/modal-form-grid.css';

interface TemplateApplyModalProps {
  template: AlarmTemplate;
  isOpen: boolean;
  onClose: () => void;
  onSuccess: () => void;
}

const TemplateApplyModal: React.FC<TemplateApplyModalProps> = ({
  template,
  isOpen,
  onClose,
  onSuccess
}) => {
  const [dataPoints, setDataPoints] = useState<DataPoint[]>([]);
  const [loading, setLoading] = useState(false);
  const [applying, setApplying] = useState(false);
  const [selectedIds, setSelectedIds] = useState<number[]>([]);
  const [searchTerm, setSearchTerm] = useState('');
  const [ruleGroupName, setRuleGroupName] = useState('');

  // Filtering data points by compatibility and search term
  const filteredDataPoints = useMemo(() => {
    return dataPoints.filter(dp => {
      // Compatibility check (if template has specific types)
      const isCompatible = template.applicable_data_types.includes('*') ||
        template.applicable_data_types.includes(dp.data_type);

      if (!isCompatible) return false;

      // Search term check
      if (!searchTerm) return true;
      const term = searchTerm.toLowerCase();
      return dp.name.toLowerCase().includes(term) ||
        dp.device_name.toLowerCase().includes(term) ||
        dp.site_name.toLowerCase().includes(term);
    });
  }, [dataPoints, template.applicable_data_types, searchTerm]);

  useEffect(() => {
    if (isOpen) {
      loadDataPoints();
    }
  }, [isOpen, template]);

  const loadDataPoints = async () => {
    try {
      setLoading(true);
      const points = await alarmTemplatesApi.getDataPoints();
      setDataPoints(points);
    } catch (error) {
      console.error('Failed to load data points', error);
    } finally {
      setLoading(false);
    }
  };

  const handleToggleSelect = (id: number) => {
    setSelectedIds(prev =>
      prev.includes(id) ? prev.filter(i => i !== id) : [...prev, id]
    );
  };

  const handleApply = async () => {
    if (selectedIds.length === 0) return;

    try {
      setApplying(true);
      const request: ApplyTemplateRequest = {
        data_point_ids: selectedIds,
        rule_group_name: ruleGroupName || undefined
      };

      const response = await alarmTemplatesApi.applyTemplate(template.id, request);
      if (response && (response.success || response)) {
        onSuccess();
        onClose();
      }
    } catch (error) {
      alert('템플릿 적용 중 오류가 발생했습니다.');
    } finally {
      setApplying(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-container modal-xl animate-fade-in" style={{ maxWidth: '900px' }}>
        <div className="modal-header header-gradient">
          <div className="header-info">
            <h3><i className="fas fa-magic"></i> 템플릿 적용: {template.name}</h3>
            <p>이 템플릿을 적용할 대상 데이터포인트를 선택하세요. ({filteredDataPoints.length}개 호환됨)</p>
          </div>
          <button className="btn-close" onClick={onClose}><i className="fas fa-times"></i></button>
        </div>

        <div className="modal-body" style={{ padding: '0' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 300px', height: '550px' }}>
            {/* Left Column: Data Point Selection */}
            <div style={{ padding: '24px', borderRight: '1px solid #e5e7eb', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
              <div className="mgmt-search-wrapper" style={{ marginBottom: '16px', position: 'relative' }}>
                <i className="fas fa-search" style={{ position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)', color: '#9ca3af' }}></i>
                <input
                  type="text"
                  className="form-input"
                  style={{ paddingLeft: '40px', width: '100%' }}
                  placeholder="데이터포인트명, 장비명, 사이트명으로 검색..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                />
              </div>

              <div className="selection-list-container" style={{ flex: 1, overflowY: 'auto', border: '1px solid #e5e7eb', borderRadius: '8px' }}>
                <table className="mgmt-table compact">
                  <thead style={{ position: 'sticky', top: 0, zIndex: 5, backgroundColor: '#f9fafb' }}>
                    <tr>
                      <th style={{ width: '40px' }}>
                        <input
                          type="checkbox"
                          checked={filteredDataPoints.length > 0 && selectedIds.length === filteredDataPoints.length}
                          onChange={(e) => {
                            if (e.target.checked) setSelectedIds(filteredDataPoints.map(dp => dp.id));
                            else setSelectedIds([]);
                          }}
                        />
                      </th>
                      <th>데이터포인트</th>
                      <th>장비</th>
                      <th>타입</th>
                    </tr>
                  </thead>
                  <tbody>
                    {loading ? (
                      <tr><td colSpan={4} style={{ textAlign: 'center', padding: '40px' }}>로딩 중...</td></tr>
                    ) : filteredDataPoints.length === 0 ? (
                      <tr><td colSpan={4} style={{ textAlign: 'center', padding: '40px' }}>대상 데이터포인트가 없습니다.</td></tr>
                    ) : (
                      filteredDataPoints.map(dp => (
                        <tr key={dp.id} onClick={() => handleToggleSelect(dp.id)} style={{ cursor: 'pointer' }}>
                          <td>
                            <input type="checkbox" checked={selectedIds.includes(dp.id)} readOnly />
                          </td>
                          <td>
                            <div style={{ fontWeight: 500 }}>{dp.name}</div>
                            <div style={{ fontSize: '11px', color: '#6b7280' }}>{dp.address || '-'}</div>
                          </td>
                          <td>{dp.device_name}</td>
                          <td><span className="badge neutral">{dp.data_type}</span></td>
                        </tr>
                      ))
                    )}
                  </tbody>
                </table>
              </div>
            </div>

            {/* Right Column: Settings & Summary */}
            <div style={{ backgroundColor: '#f9fafb', padding: '24px', display: 'flex', flexDirection: 'column', gap: '20px' }}>
              <section>
                <h4 style={{ fontSize: '14px', color: '#374151', marginBottom: '12px', fontWeight: 600 }}>설정 요약</h4>
                <div className="summary-card" style={{ background: 'white', padding: '16px', borderRadius: '8px', border: '1px solid #e5e7eb', fontSize: '13px' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                    <span style={{ color: '#6b7280' }}>심각도</span>
                    <span className={`badge ${getSeverityBadgeClass(template.severity)}`}>{template.severity.toUpperCase()}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                    <span style={{ color: '#6b7280' }}>카테고리</span>
                    <span style={{ fontWeight: 500 }}>{template.category}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: '#6b7280' }}>로직 유형</span>
                    <span style={{ fontWeight: 500 }}>{template.condition_type}</span>
                  </div>
                </div>
              </section>

              <section>
                <h4 style={{ fontSize: '14px', color: '#374151', marginBottom: '12px', fontWeight: 600 }}>추가 정보</h4>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  <label style={{ fontSize: '12px', color: '#6b7280' }}>규칙 그룹명 (선택)</label>
                  <input
                    type="text"
                    className="form-input"
                    placeholder="예: 2024 정기 업데이트"
                    value={ruleGroupName}
                    onChange={(e) => setRuleGroupName(e.target.value)}
                  />
                </div>
              </section>

              <div style={{ marginTop: 'auto', borderTop: '1px solid #e5e7eb', paddingTop: '20px' }}>
                <div style={{ marginBottom: '16px', textAlign: 'center' }}>
                  <div style={{ fontSize: '24px', fontWeight: 700, color: '#111827' }}>{selectedIds.length}</div>
                  <div style={{ fontSize: '12px', color: '#6b7280' }}>개 선택됨</div>
                </div>
                <button
                  className="btn btn-primary"
                  style={{ width: '100%', height: '48px', fontSize: '15px' }}
                  onClick={handleApply}
                  disabled={selectedIds.length === 0 || applying}
                >
                  {applying ? '적용 중...' : '알람 규칙 생성하기'}
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

// Helper inside the file
const getSeverityBadgeClass = (s: string) => {
  const severity = s?.toLowerCase();
  if (severity === 'critical') return 'danger';
  if (severity === 'high') return 'warning';
  if (severity === 'medium') return 'primary';
  return 'neutral';
};

export default TemplateApplyModal;