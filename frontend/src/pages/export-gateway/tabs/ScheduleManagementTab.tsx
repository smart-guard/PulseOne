import React, { useState, useEffect } from 'react';
import exportGatewayApi, { ExportTarget, ExportProfile, ExportSchedule } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

const CRON_PRESETS = [
    { label: '매분 (Every minute)', value: '* * * * *', description: '매 분마다 데이터를 전송합니다.' },
    { label: '5분마다 (Every 5 mins)', value: '*/5 * * * *', description: '5분 간격으로 데이터를 전송합니다.' },
    { label: '10분마다 (Every 10 mins)', value: '*/10 * * * *', description: '10분 간격으로 데이터를 전송합니다.' },
    { label: '15분마다 (Every 15 mins)', value: '*/15 * * * *', description: '15분 간격으로 데이터를 전송합니다.' },
    { label: '30분마다 (Every 30 mins)', value: '*/30 * * * *', description: '30분 간격으로 데이터를 전송합니다.' },
    { label: '매시간 (Hourly)', value: '0 * * * *', description: '매 정시마다 데이터를 전송합니다.' },
    { label: '매일 자정 (Daily 00:00)', value: '0 0 * * *', description: '매일 자정(00:00)에 데이터를 전송합니다.' },
    { label: '매일 오전 9시 (Daily 09:00)', value: '0 9 * * *', description: '매일 오전 9시에 데이터를 전송합니다.' },
    { label: '매주 월요일 (Weekly Mon)', value: '0 0 * * 1', description: '매주 월요일 자정에 데이터를 전송합니다.' },
];

interface ScheduleManagementTabProps {
    siteId?: number | null;
    tenantId?: number | null;
}

const ScheduleManagementTab: React.FC<ScheduleManagementTabProps> = ({ siteId, tenantId }) => {
    const [schedules, setSchedules] = useState<any[]>([]);
    const [targets, setTargets] = useState<ExportTarget[]>([]);
    const [profiles, setProfiles] = useState<ExportProfile[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingSchedule, setEditingSchedule] = useState<Partial<any> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
    const { confirm } = useConfirmContext();

    const fetchData = async () => {
        setLoading(true);
        try {
            const [schedulesRes, targetsRes, profilesRes] = await Promise.all([
                exportGatewayApi.getSchedules({ tenantId }),
                exportGatewayApi.getTargets({ tenantId }),
                exportGatewayApi.getProfiles({ tenantId })
            ]);
            setSchedules(schedulesRes.data || []);
            setTargets(targetsRes.data || []);
            setProfiles(profilesRes.data || []);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    const handleCloseModal = async () => {
        if (hasChanges) {
            const confirmed = await confirm({
                title: '변경사항 유실 주의',
                message: '수정 중인 내용이 있습니다. 저장하지 않고 닫으시면 모든 데이터가 사라집니다. 정말 닫으시겠습니까?',
                confirmText: '닫기',
                cancelText: '취소',
                confirmButtonType: 'warning'
            });
            if (!confirmed) return;
        }
        setIsModalOpen(false);
        setHasChanges(false);
    };

    useEffect(() => { fetchData(); }, [siteId, tenantId]);

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();

        if (!hasChanges && editingSchedule?.id) {
            await confirm({
                title: '수정사항 없음',
                message: '수정된 정보가 없습니다.',
                showCancelButton: false,
                confirmButtonType: 'primary'
            });
            setIsModalOpen(false);
            return;
        }

        try {
            let response;
            if (editingSchedule?.id) {
                response = await exportGatewayApi.updateSchedule(editingSchedule.id, editingSchedule as ExportSchedule, tenantId);
            } else {
                response = await exportGatewayApi.createSchedule(editingSchedule as ExportSchedule, tenantId);
            }

            if (response.success) {
                await confirm({
                    title: '저장 완료',
                    message: '스케줄이 성공적으로 저장되었습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'success'
                });
                setIsModalOpen(false);
                setHasChanges(false);
                fetchData();
            } else {
                await confirm({
                    title: '저장 실패',
                    message: response.message || '스케줄을 저장하는 중 오류가 발생했습니다.',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            }
        } catch (error) {
            await confirm({
                title: '저장 실패',
                message: '스케줄을 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '스케줄 삭제 확인',
            message: '이 전송 스케줄을 삭제하시겠습니까?',
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteSchedule(id, tenantId);
            fetchData();
        } catch (error) {
            await confirm({ title: '삭제 실패', message: '스케줄을 삭제하는 중 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>배치 전송 스케줄 관리</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingSchedule({ schedule_name: '', cron_expression: '0 0 * * *', data_range: 'hour', lookback_periods: 1, timezone: 'Asia/Seoul', is_enabled: true }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> 스케줄 추가
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>스케줄 이름</th>
                            <th>전송 타겟</th>
                            <th>Cron 표현식</th>
                            <th>상태</th>
                            <th>마지막 실행</th>
                            <th>관리</th>
                        </tr>
                    </thead>
                    <tbody>
                        {schedules.map(s => (
                            <tr key={s.id}>
                                <td>
                                    <div style={{ fontWeight: 600 }}>{s.schedule_name}</div>
                                    <div style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>{s.description}</div>
                                </td>
                                <td>{s.target_name || `ID: ${s.target_id}`}</td>
                                <td><code style={{ background: '#f0f0f0', padding: '2px 4px', borderRadius: '4px' }}>{s.cron_expression}</code></td>
                                <td>
                                    <span className={`mgmt-badge ${s.is_enabled ? 'success' : 'neutral'}`}>
                                        {s.is_enabled ? '활성' : '비활성'}
                                    </span>
                                </td>
                                <td>
                                    {s.last_run_at ? new Date(s.last_run_at).toLocaleString() : '-'}
                                    {s.last_status && <div style={{ fontSize: '10px', color: s.last_status === 'success' ? 'var(--success-600)' : 'var(--error-600)' }}>{s.last_status}</div>}
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs" onClick={() => { setEditingSchedule(s); setIsModalOpen(true); }} style={{ width: 'auto' }}>수정</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-xs mgmt-btn-error" onClick={() => handleDelete(s.id)} style={{ width: 'auto' }}>삭제</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <style>{`
                        .cron-preset-grid {
                            display: grid;
                            grid-template-columns: repeat(3, 1fr);
                            gap: 8px;
                            margin-top: 12px;
                        }
                        .cron-preset-item {
                            padding: 10px;
                            border: 1px solid #e2e8f0;
                            border-radius: 8px;
                            background: white;
                            cursor: pointer;
                            transition: all 0.2s;
                            text-align: center;
                        }
                        .cron-preset-item:hover {
                            border-color: var(--primary-400);
                            background: var(--primary-50);
                            transform: translateY(-1px);
                            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
                        }
                        .cron-preset-label {
                            font-size: 11px;
                            font-weight: 700;
                            color: var(--primary-700);
                            display: block;
                        }
                        .cron-preset-val {
                            font-size: 10px;
                            color: #64748b;
                            font-family: monospace;
                        }
                        .cron-guide-box {
                            background: #f8fafc;
                            border-radius: 12px;
                            padding: 16px;
                            border: 1px solid #e2e8f0;
                            margin-top: 20px;
                        }
                    `}</style>
                    <div className="mgmt-modal-content" style={{ maxWidth: '850px', width: '90%' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">{editingSchedule?.id ? "스케줄 상세 수정" : "전송 스케줄 신규 등록"}</h3>
                            <button className="mgmt-modal-close" onClick={handleCloseModal}>&times;</button>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body" style={{ display: 'flex', gap: '24px' }}>
                                {/* Left Side: Basic Config */}
                                <div style={{ flex: 1 }}>
                                    <div className="mgmt-modal-form-section">
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontWeight: 700 }}>스케줄 명칭</label>
                                            <input
                                                type="text"
                                                className="mgmt-input"
                                                required
                                                value={editingSchedule?.schedule_name || ''}
                                                onChange={e => { setEditingSchedule({ ...editingSchedule, schedule_name: e.target.value }); setHasChanges(true); }}
                                                placeholder="예: 일일 마감 데이터 전송"
                                            />
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label style={{ fontWeight: 700 }}>전송 대상 타겟</label>
                                            <select
                                                className="mgmt-select"
                                                required
                                                value={editingSchedule?.target_id || ''}
                                                onChange={e => { setEditingSchedule({ ...editingSchedule, target_id: parseInt(e.target.value) }); setHasChanges(true); }}
                                            >
                                                <option value="">(전송 목적지 선택)</option>
                                                {targets.map(t => (
                                                    <option key={t.id} value={t.id}>{t.name} ({t.target_type})</option>
                                                ))}
                                            </select>
                                        </div>

                                        <div style={{ background: '#f8fafc', padding: '16px', borderRadius: '12px', border: '1px solid #e2e8f0', marginTop: '16px' }}>
                                            <label style={{ fontSize: '13px', fontWeight: 700, color: '#475569', marginBottom: '12px', display: 'block' }}>
                                                전송 데이터 수집 범위 (Data Window)
                                            </label>
                                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap', color: '#64748b', fontSize: '14px' }}>
                                                <span>실행 시점 기준, 최근</span>
                                                <input
                                                    type="number"
                                                    className="mgmt-input sm"
                                                    style={{ width: '70px', textAlign: 'center', fontWeight: 700 }}
                                                    value={editingSchedule?.lookback_periods || 1}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, lookback_periods: parseInt(e.target.value) }); setHasChanges(true); }}
                                                    min={1}
                                                />
                                                <select
                                                    className="mgmt-select sm"
                                                    style={{ width: '120px', fontWeight: 700 }}
                                                    value={editingSchedule?.data_range || 'hour'}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, data_range: e.target.value }); setHasChanges(true); }}
                                                >
                                                    <option value="minute">분 (Minutes)</option>
                                                    <option value="hour">시간 (Hours)</option>
                                                    <option value="day">일 (Days)</option>
                                                </select>
                                                <span>동안의 데이터를 수집하여 전송합니다.</span>
                                            </div>
                                            <div style={{ marginTop: '10px', fontSize: '12px', color: 'var(--primary-600)', background: 'var(--primary-50)', padding: '8px 12px', borderRadius: '6px' }}>
                                                <i className="fas fa-magic" style={{ marginRight: '6px' }} />
                                                <strong>설명:</strong> 스케줄이 실행되는 순간, 위 설정만큼 과거로 거슬러 올라가 누적된 데이터를 모두 가져와 전송 대상 타겟으로 보냅니다.
                                            </div>
                                        </div>

                                        <div className="mgmt-modal-form-group" style={{ marginTop: '20px' }}>
                                            <label className="checkbox-label" style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer', padding: '10px', background: '#f1f5f9', borderRadius: '8px' }}>
                                                <input
                                                    type="checkbox"
                                                    checked={editingSchedule?.is_enabled ?? true}
                                                    onChange={e => { setEditingSchedule({ ...editingSchedule, is_enabled: e.target.checked }); setHasChanges(true); }}
                                                />
                                                <span style={{ fontWeight: 600 }}>스케줄 활성화</span>
                                            </label>
                                        </div>
                                    </div>
                                </div>

                                {/* Right Side: Cron Presets & Guide */}
                                <div style={{ flex: 1.2, borderLeft: '1px solid #f1f5f9', paddingLeft: '24px' }}>
                                    <div className="mgmt-modal-form-group">
                                        <label style={{ fontWeight: 700, display: 'flex', justifyContent: 'space-between' }}>
                                            Cron 표현식 설정
                                            <span style={{ fontSize: '11px', color: 'var(--primary-600)', fontWeight: 400 }}>Presets을 클릭해 자동 입력하세요</span>
                                        </label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            required
                                            style={{ fontFamily: 'monospace', fontSize: '16px', letterSpacing: '2px', textAlign: 'center', background: '#fffbeb', borderColor: '#fde68a' }}
                                            value={editingSchedule?.cron_expression || ''}
                                            onChange={e => { setEditingSchedule({ ...editingSchedule, cron_expression: e.target.value }); setHasChanges(true); }}
                                            placeholder="* * * * *"
                                        />

                                        <div className="cron-preset-grid">
                                            {CRON_PRESETS.map((p, idx) => (
                                                <div
                                                    key={idx}
                                                    className="cron-preset-item"
                                                    onClick={() => { setEditingSchedule({ ...editingSchedule, cron_expression: p.value }); setHasChanges(true); }}
                                                >
                                                    <span className="cron-preset-label">{p.label}</span>
                                                    <span className="cron-preset-val">{p.value}</span>
                                                </div>
                                            ))}
                                        </div>
                                    </div>

                                    <div className="cron-guide-box">
                                        <div style={{ fontWeight: 700, fontSize: '12px', color: '#475569', marginBottom: '8px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                                            <i className="fas fa-info-circle" style={{ color: 'var(--primary-500)' }} /> Cron 표현식 가이드
                                        </div>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', textAlign: 'center', fontSize: '10px', color: '#64748b', background: 'white', padding: '8px', borderRadius: '6px', border: '1px solid #e2e8f0' }}>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>분(0-59)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>시(0-23)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>일(1-31)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>월(1-12)</div>
                                            <div><div style={{ fontWeight: 700, color: 'var(--primary-600)' }}>*</div>요일(0-6)</div>
                                        </div>
                                        <div style={{ marginTop: '10px', fontSize: '11px', color: '#64748b' }}>
                                            <div style={{ marginBottom: '4px' }}>• <code>*</code>: 모든 값을 의미</div>
                                            <div style={{ marginBottom: '4px' }}>• <code>*/N</code>: N 마다 반복</div>
                                            <div>• <code>0-5</code>: 범위 설정</div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer">
                                <button type="button" className="mgmt-btn mgmt-btn-outline" onClick={handleCloseModal}>취소</button>
                                <button type="submit" className="mgmt-btn mgmt-btn-primary" style={{ padding: '0 32px' }}>스케줄 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default ScheduleManagementTab;
