// ============================================================================
// VirtualPointDetailModal.tsx - Horizontal Layout
// ============================================================================

import React from 'react';
import { VirtualPoint } from '../../../types/virtualPoints';
import { virtualPointsApi } from '../../../api/services/virtualPointsApi';
import './VirtualPointModal.css'; // Consistent styling source

interface VirtualPointDetailModalProps {
    isOpen: boolean;
    virtualPoint: VirtualPoint | null;
    onClose: () => void;
    onEdit: (point: VirtualPoint) => void;
    onExecute: (pointId: number) => void;
    onToggleEnabled: (pointId: number) => void;
    onRestore: (pointId: number) => void;
    onDelete?: (pointId: number) => void;
}

export const VirtualPointDetailModal: React.FC<VirtualPointDetailModalProps> = ({
    isOpen,
    virtualPoint,
    onClose,
    onEdit,
    onExecute,
    onToggleEnabled,
    onRestore,
    onDelete

}) => {
    const [activeTab, setActiveTab] = React.useState<'overview' | 'history' | 'dependency'>('overview');
    const [logs, setLogs] = React.useState<any[]>([]);
    const [loadingLogs, setLoadingLogs] = React.useState(false);

    // 로그 조회 API 호출
    React.useEffect(() => {
        if (activeTab === 'history' && virtualPoint) {
            setLoadingLogs(true);
            virtualPointsApi.getLogs(virtualPoint.id)
                .then(data => setLogs(data))
                .catch(err => console.error(err))
                .finally(() => setLoadingLogs(false));
        }
    }, [activeTab, virtualPoint]);


    if (!isOpen || !virtualPoint) return null;

    // 탭 변경 핸들러
    const handleTabChange = (tab: 'overview' | 'history' | 'dependency') => setActiveTab(tab);

    // 수식에서 변수 추출 (간단한 파싱)
    const extractDependencies = (formula: string) => {
        const regex = /inputs\.([a-zA-Z0-9_]+)/g;
        const matches = [...formula.matchAll(regex)];
        return [...new Set(matches.map(m => m[1]))]; // 중복 제거
    };

    const dependencies = virtualPoint ? extractDependencies(virtualPoint.formula || '') : [];



    return (
        <div className="vpd-overlay" onClick={(e) => e.target === e.currentTarget && onClose()}>
            <div className="vpd-card">
                <div className="vpd-header">
                    <div className="title-group">
                        <div className="icon-box">
                            <i className="fas fa-microchip"></i>
                        </div>
                        <h2>가상포인트 엔진 상세 데이터</h2>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                        {virtualPoint.is_deleted ? (
                            <div className="vpd-status-banner danger" style={{ background: '#fee2e2', color: '#dc2626', border: '1px solid #fecaca' }}>
                                <i className="fas fa-trash-alt"></i>
                                삭제된 가상포인트
                            </div>
                        ) : (
                            <div className={`vpd-status-banner ${virtualPoint.is_enabled ? 'active' : 'paused'}`}>
                                <i className={`fas ${virtualPoint.is_enabled ? 'fa-check-circle' : 'fa-pause-circle'}`}></i>
                                {virtualPoint.is_enabled ? '실시간 연산 활성화됨' : '연산 일시정지 상태'}
                            </div>
                        )}
                        <button className="close-btn" onClick={onClose} title="닫기">
                            <i className="fas fa-times"></i>
                        </button>
                    </div>
                </div>

                {/* Tab Navigation */}
                <div className="vpd-tabs">
                    <button
                        className={`vpd-tab ${activeTab === 'overview' ? 'active' : ''}`}
                        onClick={() => handleTabChange('overview')}
                    >
                        <i className="fas fa-chart-pie mr-2"></i> 개요 및 상태
                    </button>
                    <button
                        className={`vpd-tab ${activeTab === 'history' ? 'active' : ''}`}
                        onClick={() => handleTabChange('history')}
                    >
                        <i className="fas fa-history mr-2"></i> 변경 이력 (Audit Log)
                    </button>
                    <button
                        className={`vpd-tab ${activeTab === 'dependency' ? 'active' : ''}`}
                        onClick={() => handleTabChange('dependency')}
                    >
                        <i className="fas fa-project-diagram mr-2"></i> 종속성 맵 (Dependency)
                    </button>
                </div>

                <div className="vpd-content">
                    {activeTab === 'overview' ? (
                        <div className="vpd-overview-wrapper">
                            {/* LEFT COLUMN: Metadata & Results */}
                            <div className="vpd-left-column">
                                <div className="vpd-identity">
                                    <h1>{virtualPoint.name}</h1>
                                    <p>{virtualPoint.description || '정의된 설명이 없는 가상포인트 엔진입니다.'}</p>
                                </div>

                                <div className="vpd-stats-grid">
                                    <div className="vpd-stat-card">
                                        <label>카테고리</label>
                                        <div className="val">{virtualPoint.category || '기본'}</div>
                                    </div>
                                    <div className="vpd-stat-card">
                                        <label>데이터 타입</label>
                                        <div className="val">{virtualPoint.data_type} {virtualPoint.unit && `(${virtualPoint.unit})`}</div>
                                    </div>
                                    <div className="vpd-stat-card">
                                        <label>실행 모드</label>
                                        <div className="val">{virtualPoint.execution_type === 'periodic' ? `주기적 (${virtualPoint.execution_interval}ms)` : '데이터 변경 시'}</div>
                                    </div>
                                    <div className="vpd-stat-card">
                                        <label>계산 범위</label>
                                        <div className="val" style={{ textTransform: 'capitalize' }}>{virtualPoint.scope_type} 레벨</div>
                                    </div>
                                </div>

                                <div className="vpd-result-box">
                                    <label>현재 데이터 산출 결과</label>
                                    <div className="main-val">
                                        {virtualPoint.current_value !== undefined ? virtualPoint.current_value : 'N/A'}
                                        <span style={{ fontSize: '14px', fontWeight: 600, marginLeft: '8px', opacity: 0.5 }}>{virtualPoint.unit}</span>
                                    </div>
                                    <div className="timestamp">
                                        <i className="far fa-clock"></i>
                                        {virtualPoint.last_calculated
                                            ? `최근 업데이트: ${new Date(virtualPoint.last_calculated).toLocaleString()}`
                                            : '계산 기록 없음'}
                                    </div>
                                </div>
                            </div>

                            {/* RIGHT COLUMN: Formula Studio Area */}
                            <div className="vpd-right-column">
                                <div className="vpd-logic-panel">
                                    <label>로직 수식 (Logic Expression)</label>
                                    <pre>
                                        {virtualPoint.expression || virtualPoint.formula || '// 수식이 정의되지 않았습니다.'}
                                    </pre>
                                </div>
                            </div>
                        </div>
                    ) : activeTab === 'history' ? (
                        <div className="vpd-history-panel" style={{ width: '100%', padding: '20px' }}>
                            {loadingLogs ? (
                                <div style={{ textAlign: 'center', padding: '40px', color: '#64748b' }}>
                                    <i className="fas fa-spinner fa-spin mr-2"></i> 로그를 불러오는 중...
                                </div>
                            ) : logs.length > 0 ? (
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                    {logs.map((log) => (
                                        <div key={log.id} style={{
                                            padding: '16px', background: '#f8fafc', borderRadius: '12px', border: '1px solid #e2e8f0',
                                            display: 'flex', gap: '16px', alignItems: 'flex-start'
                                        }}>
                                            <div style={{
                                                width: '36px', height: '36px', borderRadius: '50%',
                                                background: log.action === 'DELETE' ? '#fee2e2' : log.action === 'CREATE' ? '#dcfce7' : '#eff6ff',
                                                color: log.action === 'DELETE' ? '#dc2626' : log.action === 'CREATE' ? '#166534' : '#3b82f6',
                                                display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '14px', flexShrink: 0
                                            }}>
                                                <i className={`fas ${log.action === 'DELETE' ? 'fa-trash' : log.action === 'CREATE' ? 'fa-plus' : 'fa-edit'}`}></i>
                                            </div>
                                            <div style={{ flex: 1 }}>
                                                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                                                    <span style={{ fontWeight: 600, color: '#1e293b', fontSize: '14px' }}>
                                                        {log.action}
                                                        <span style={{ fontWeight: 400, color: '#64748b', marginLeft: '8px', fontSize: '13px' }}>
                                                            {log.details || '내역 없음'}
                                                        </span>
                                                    </span>
                                                    <span style={{ fontSize: '12px', color: '#94a3b8' }}>
                                                        {new Date(log.created_at).toLocaleString()}
                                                    </span>
                                                </div>
                                                {/* 변경 내역 상세 (JSON Diff 등) 시각화가 필요하면 추가 */}
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            ) : (
                                <div className="empty-state" style={{ textAlign: 'center', padding: '40px', color: '#94a3b8' }}>
                                    <i className="fas fa-history fa-3x mb-3" style={{ opacity: 0.3 }}></i>
                                    <p>저장된 변경 이력이 없습니다.</p>
                                </div>
                            )}
                        </div>
                    ) : (
                        <div className="vpd-dependency-panel" style={{ width: '100%', padding: '20px' }}>
                            <div style={{ textAlign: 'center', marginBottom: '20px' }}>
                                <h3 style={{ fontSize: '16px', color: '#334155' }}>데이터 종속성 시각화</h3>
                                <p style={{ fontSize: '13px', color: '#64748b' }}>이 가상포인트가 참조하고 있는 입력 변수들입니다.</p>
                            </div>

                            {dependencies.length > 0 ? (
                                <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '16px', marginTop: '20px' }}>
                                    {dependencies.map((dep, idx) => (
                                        <div key={idx} style={{
                                            padding: '16px 32px',
                                            background: '#f8fafc',
                                            border: '1px solid #e2e8f0',
                                            borderRadius: '12px',
                                            display: 'flex',
                                            alignItems: 'center',
                                            gap: '20px',
                                            width: '80%',
                                            maxWidth: '600px',
                                            justifyContent: 'space-between',
                                            boxShadow: '0 2px 4px rgba(0,0,0,0.02)'
                                        }}>
                                            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                                <div style={{ width: '12px', height: '12px', borderRadius: '50%', background: '#3b82f6' }}></div>
                                                <span style={{ fontWeight: 600, color: '#1e293b', fontSize: '15px' }}>{dep}</span>
                                            </div>

                                            <i className="fas fa-arrow-right" style={{ color: '#94a3b8', fontSize: '16px' }}></i>

                                            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                                <i className="fas fa-microchip" style={{ color: '#64748b' }}></i>
                                                <span style={{ fontWeight: 700, color: '#0f172a', fontSize: '15px' }}>{virtualPoint.name}</span>
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            ) : (
                                <div className="empty-state" style={{ textAlign: 'center', padding: '40px', color: '#94a3b8' }}>
                                    <i className="fas fa-link-slash fa-3x mb-3" style={{ opacity: 0.3 }}></i>
                                    <p>종속된 입력 변수가 없습니다.</p>
                                    <p style={{ fontSize: '13px', marginTop: '8px' }}>(독립적인 상수 수식일 수 있습니다)</p>
                                </div>
                            )}
                        </div>
                    )}
                </div>

                <div className="vpd-footer">
                    {virtualPoint.is_deleted ? (
                        <>
                            <div style={{ display: 'flex', gap: '12px' }}>
                                <div style={{ color: '#64748b', fontSize: '13px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <i className="fas fa-info-circle"></i>
                                    이 포인트는 삭제되었습니다. 다시 사용하려면 복원하세요.
                                </div>
                            </div>
                            <div style={{ display: 'flex', gap: '12px' }}>
                                <button className="vpd-btn secondary" onClick={onClose}>창 닫기</button>
                                <button className="vpd-btn success" onClick={() => onRestore(virtualPoint.id)}>
                                    <i className="fas fa-undo"></i> 삭제 취소 및 복원
                                </button>
                            </div>
                        </>
                    ) : (
                        <>
                            <div style={{ display: 'flex', gap: '12px' }}>
                                <button
                                    className={`vpd-btn ${virtualPoint.is_enabled ? 'danger' : 'success'}`}
                                    onClick={() => onToggleEnabled(virtualPoint.id)}
                                >
                                    <i className={`fas ${virtualPoint.is_enabled ? 'fa-pause' : 'fa-play'}`}></i>
                                    {virtualPoint.is_enabled ? '연산 엔진 중지' : '연산 엔진 재개'}
                                </button>
                                <button className="vpd-btn secondary" onClick={() => onExecute(virtualPoint.id)}>
                                    <i className="fas fa-bolt"></i> 즉시 실행 (Test)
                                </button>
                            </div>
                            <div style={{ display: 'flex', gap: '12px' }}>
                                <button className="vpd-btn secondary" onClick={onClose}>창 닫기</button>
                                {onDelete && (
                                    <button className="vpd-btn danger" onClick={() => onDelete(virtualPoint.id)}>
                                        <i className="fas fa-trash"></i> 가상포인트 삭제
                                    </button>
                                )}
                                <button className="vpd-btn primary" onClick={() => onEdit(virtualPoint)}>
                                    <i className="fas fa-sliders-h"></i> 엔진 설정 수정
                                </button>
                            </div>
                        </>
                    )}
                </div>
            </div>
        </div>
    );
};
