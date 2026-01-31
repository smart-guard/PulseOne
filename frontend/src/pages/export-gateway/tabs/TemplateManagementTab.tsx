import React, { useState, useEffect } from 'react';
import exportGatewayApi, { PayloadTemplate } from '../../../api/services/exportGatewayApi';
import { useConfirmContext } from '../../../components/common/ConfirmProvider';

const TemplateManagementTab: React.FC = () => {
    const [templates, setTemplates] = useState<PayloadTemplate[]>([]);
    const [loading, setLoading] = useState(false);
    const [isModalOpen, setIsModalOpen] = useState(false);
    const [editingTemplate, setEditingTemplate] = useState<Partial<PayloadTemplate> | null>(null);
    const [hasChanges, setHasChanges] = useState(false);
    const { confirm } = useConfirmContext();

    const fetchTemplates = async () => {
        setLoading(true);
        try {
            const response = await exportGatewayApi.getTemplates();
            setTemplates(response.data || []);
        } catch (error) {
            console.error(error);
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => { fetchTemplates(); }, []);

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

    const handleSave = async (e: React.FormEvent) => {
        e.preventDefault();

        if (!hasChanges && editingTemplate?.id) {
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
            // JSON 유효성 검사
            let templateJson = editingTemplate?.template_json;
            if (typeof templateJson === 'string') {
                try {
                    templateJson = JSON.parse(templateJson);
                } catch (parseError) {
                    await confirm({
                        title: 'JSON 형식 오류',
                        message: `유효하지 않은 JSON 형식입니다.\n\n[상세 내역]\n${parseError instanceof Error ? parseError.message : String(parseError)}\n\n팁: {{bd}}와 같은 숫지형 변수도 "{{bd}}"와 같이 따옴표로 감싸야 유효한 JSON이 됩니다.`,
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                    return;
                }
            }

            const dataToSave = {
                ...editingTemplate,
                template_json: templateJson
            };

            if (editingTemplate?.id) {
                await exportGatewayApi.updateTemplate(editingTemplate.id, dataToSave);
            } else {
                await exportGatewayApi.createTemplate(dataToSave);
            }

            await confirm({
                title: '저장 완료',
                message: '템플릿이 성공적으로 저장되었습니다.',
                showCancelButton: false,
                confirmButtonType: 'success'
            });
            setIsModalOpen(false);
            setHasChanges(false);
            fetchTemplates();
        } catch (error) {
            await confirm({
                title: '저장 실패',
                message: '템플릿을 저장하는 중 오류가 발생했습니다.',
                showCancelButton: false,
                confirmButtonType: 'danger'
            });
        }
    };

    const handleDelete = async (id: number) => {
        const confirmed = await confirm({
            title: '템플릿 삭제 확인',
            message: '이 페이로드 템플릿을 삭제하시겠습니까? 이 템플릿을 사용하는 타겟들의 전송이 실패할 수 있습니다.',
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (!confirmed) return;
        try {
            await exportGatewayApi.deleteTemplate(id);
            fetchTemplates();
        } catch (error) {
            await confirm({ title: '삭제 실패', message: '템플릿을 삭제하는 중 오류가 발생했습니다.', showCancelButton: false, confirmButtonType: 'danger' });
        }
    };

    return (
        <div>
            <div className="mgmt-header-actions" style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '20px', alignItems: 'center' }}>
                <h3 style={{ margin: 0, color: 'var(--neutral-800)', fontWeight: 600 }}>페이로드 템플릿 설정</h3>
                <button className="btn btn-primary btn-sm" onClick={() => { setEditingTemplate({ name: '', system_type: 'custom', template_json: '{\n  "device": "{device_name}",\n  "point": "{point_name}",\n  "value": {value},\n  "timestamp": "{timestamp}"\n}', is_active: true }); setIsModalOpen(true); }}>
                    <i className="fas fa-plus" /> 템플릿 추가
                </button>
            </div>

            <div className="mgmt-table-container">
                <table className="mgmt-table">
                    <thead>
                        <tr>
                            <th>이름</th>
                            <th>시스템 타입</th>
                            <th>상태</th>
                            <th>관리</th>
                        </tr>
                    </thead>
                    <tbody>
                        {templates.map(t => (
                            <tr key={t.id}>
                                <td>{t.name}</td>
                                <td><span className="mgmt-badge neutral">{t.system_type}</span></td>
                                <td>
                                    <span className={`mgmt-badge ${t.is_active ? 'success' : 'neutral'}`}>
                                        {t.is_active ? '활성' : '비활성'}
                                    </span>
                                </td>
                                <td>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <button className="mgmt-btn mgmt-btn-outline" onClick={() => {
                                            const jsonStr = typeof t.template_json === 'string' ? t.template_json : JSON.stringify(t.template_json, null, 2);
                                            setEditingTemplate({ ...t, template_json: jsonStr });
                                            setIsModalOpen(true);
                                        }} style={{ width: 'auto' }}>수정</button>
                                        <button className="mgmt-btn mgmt-btn-outline mgmt-btn-error" onClick={() => handleDelete(t.id)} style={{ width: 'auto' }}>삭제</button>
                                    </div>
                                </td>
                            </tr>
                        ))}
                        {templates.length === 0 && !loading && (
                            <tr>
                                <td colSpan={4} style={{ textAlign: 'center', padding: '40px', color: 'var(--neutral-400)' }}>
                                    등록된 템플릿이 없습니다.
                                </td>
                            </tr>
                        )}
                    </tbody>
                </table>
            </div>

            {isModalOpen && (
                <div className="mgmt-modal-overlay">
                    <div className="mgmt-modal-content" style={{ maxWidth: '600px' }}>
                        <div className="mgmt-modal-header">
                            <h3 className="mgmt-modal-title">{editingTemplate?.id ? "템플릿 수정" : "페이로드 템플릿 추가"}</h3>
                            <button className="mgmt-modal-close" onClick={handleCloseModal}>&times;</button>
                        </div>
                        <form onSubmit={handleSave}>
                            <div className="mgmt-modal-body">
                                <div className="mgmt-modal-form-section">
                                    <div className="mgmt-modal-form-group">
                                        <label>템플릿 명칭</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            required
                                            value={editingTemplate?.name || ''}
                                            onChange={e => { setEditingTemplate({ ...editingTemplate, name: e.target.value }); setHasChanges(true); }}
                                            placeholder="예: 표준 JSON 전송 포맷"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>시스템 유형</label>
                                        <input
                                            type="text"
                                            className="mgmt-input"
                                            value={editingTemplate?.system_type || ''}
                                            onChange={e => { setEditingTemplate({ ...editingTemplate, system_type: e.target.value }); setHasChanges(true); }}
                                            placeholder="예: AWS IoT, Azure EventHub 등"
                                        />
                                    </div>
                                    <div className="mgmt-modal-form-group">
                                        <label>템플릿 구조 (JSON)</label>
                                        <textarea
                                            className="mgmt-input"
                                            required
                                            value={editingTemplate?.template_json || ''}
                                            onChange={e => { setEditingTemplate({ ...editingTemplate, template_json: e.target.value }); setHasChanges(true); }}
                                            placeholder='{ "key": "{value}" }'
                                            style={{ height: '200px', fontFamily: 'monospace', fontSize: '12px', padding: '10px' }}
                                        />
                                        <div className="mgmt-modal-form-hint">
                                            치환자 사용 가능: {"{device_name}"}, {"{point_name}"}, {"{value}"}, {"{timestamp}"}, {"{description}"}
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div className="mgmt-modal-footer">
                                <button type="button" className="mgmt-btn mgmt-btn-outline" style={{ height: '36px' }} onClick={handleCloseModal}>취소</button>
                                <button type="submit" className="mgmt-btn mgmt-btn-primary" style={{ height: '36px' }}>템플릿 저장</button>
                            </div>
                        </form>
                    </div>
                </div>
            )}
        </div>
    );
};

export default TemplateManagementTab;
