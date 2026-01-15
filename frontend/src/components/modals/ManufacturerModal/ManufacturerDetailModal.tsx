import React, { useState, useEffect } from 'react';
import { Manufacturer } from '../../../types/manufacturing';
import { ManufactureApiService } from '../../../api/services/manufactureApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import { COUNTRIES } from '../../../constants/countries';
import './ManufacturerModal.css';

interface ManufacturerDetailModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    manufacturerId: number | null;
}

export const ManufacturerDetailModal: React.FC<ManufacturerDetailModalProps> = ({
    isOpen,
    onClose,
    onSave,
    manufacturerId
}) => {
    const { confirm } = useConfirmContext();
    const [manufacturer, setManufacturer] = useState<Manufacturer | null>(null);
    const [isEditing, setIsEditing] = useState(false);
    const [formData, setFormData] = useState<Partial<Manufacturer>>({});
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [isManualCountry, setIsManualCountry] = useState(false);

    // 국가명 표준화 맵
    const COUNTRY_NORMALIZATION: Record<string, string> = {
        'korea': 'South Korea',
        '대한민국': 'South Korea',
        '한국': 'South Korea',
        'korea, south': 'South Korea',
        'republic of korea': 'South Korea',
        'usa': 'USA',
        'united states': 'USA',
        'america': 'USA',
        '미국': 'USA',
        'japan': 'Japan',
        '일본': 'Japan',
        'china': 'China',
        '중국': 'China',
        'germany': 'Germany',
        '독일': 'Germany',
        'uk': 'UK',
        'united kingdom': 'UK',
        'england': 'UK',
        '영국': 'UK'
    };

    useEffect(() => {
        if (isOpen && manufacturerId) {
            loadManufacturer(manufacturerId);
            setIsEditing(false);
        }
    }, [isOpen, manufacturerId]);

    const loadManufacturer = async (id: number) => {
        try {
            setLoading(true);
            const res = await ManufactureApiService.getManufacturer(id);
            if (res.success && res.data) {
                setManufacturer(res.data);
                setFormData({
                    name: res.data.name,
                    description: res.data.description || '',
                    country: res.data.country || '',
                    website: res.data.website || '',
                    is_active: res.data.is_active
                });

                // 기존 국가명이 리스트에 없으면 직접 입력 모드로 시작
                const isInList = COUNTRIES.some(c => c.name === res.data.country);
                setIsManualCountry(!!res.data.country && !isInList);

                setError(null);
            }
        } catch (err: any) {
            setError('제조사 정보를 불러오는데 실패했습니다.');
        } finally {
            setLoading(false);
        }
    };

    const handleEditToggle = () => {
        setIsEditing(!isEditing);
        setError(null);
    };

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!formData.name) {
            setError('제조사명은 필수 항목입니다.');
            return;
        }

        if (!manufacturerId) return;

        const isConfirmed = await confirm({
            title: '변경 사항 저장',
            message: '입력하신 변경 내용을 저장하시겠습니까?',
            confirmText: '저장',
            confirmButtonType: 'primary'
        });

        if (!isConfirmed) return;

        try {
            setSaving(true);
            setError(null);
            const res = await ManufactureApiService.updateManufacturer(manufacturerId, formData);
            if (res.success) {
                await confirm({
                    title: '저장 완료',
                    message: '제조사 정보가 성공적으로 수정되었습니다.',
                    confirmText: '확인',
                    showCancelButton: false,
                    confirmButtonType: 'primary'
                });
                await loadManufacturer(manufacturerId);
                onSave();
                onClose();
            } else {
                setError(res.message || '업데이트에 실패했습니다.');
            }
        } catch (err: any) {
            setError(err.message || '서버 통신 중 오류가 발생했습니다.');
        } finally {
            setSaving(false);
        }
    };

    const handleDelete = async () => {
        if (!manufacturer) return;

        const confirmed = await confirm({
            title: '제조사 삭제 확인',
            message: `'${manufacturer.name}' 제조사를 삭제하시겠습니까? 연결된 모델이 있는 경우 삭제할 수 없습니다.`,
            confirmText: '삭제',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setSaving(true);
                const res = await ManufactureApiService.deleteManufacturer(manufacturer.id);
                if (res.success) {
                    await confirm({
                        title: '삭제 완료',
                        message: '제조사가 성공적으로 삭제되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    await confirm({
                        title: '삭제 실패',
                        message: res.message || '삭제에 실패했습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                }
            } catch (err: any) {
                await confirm({
                    title: '오류 발생',
                    message: err.message || '삭제 중 오류가 발생했습니다.',
                    confirmText: '확인',
                    showCancelButton: false,
                    confirmButtonType: 'danger'
                });
            } finally {
                setSaving(false);
            }
        }
    };

    const renderContent = () => {
        if (loading) {
            return (
                <div className="loading-spinner">
                    <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                </div>
            );
        }

        if (!manufacturer) {
            return <div className="alert alert-danger">제조사 정보를 불러오지 못했습니다.</div>;
        }

        if (isEditing) {
            return (
                <form id="manufacturer-detail-form" onSubmit={handleSubmit}>
                    <div className="modal-form-grid">
                        <div className="modal-form-section">
                            <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                            <div className="modal-form-group">
                                <label className="required">제조사명</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.name || ''}
                                    onChange={e => setFormData({ ...formData, name: e.target.value })}
                                    placeholder="제조사 이름을 입력하세요"
                                    required
                                />
                            </div>
                            <div className="modal-form-group">
                                <label>설명</label>
                                <textarea
                                    className="form-control"
                                    value={formData.description || ''}
                                    onChange={e => setFormData({ ...formData, description: e.target.value })}
                                    placeholder="제조사에 대한 설명을 입력하세요"
                                    rows={3}
                                />
                            </div>
                        </div>

                        <div className="modal-form-section">
                            <h3><i className="fas fa-globe"></i> 상세 정보</h3>
                            <div className="modal-form-group">
                                <label className="required">국가</label>
                                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                    {!isManualCountry ? (
                                        <select
                                            className="form-control"
                                            value={formData.country || ''}
                                            onChange={e => {
                                                if (e.target.value === '__custom__') {
                                                    setIsManualCountry(true);
                                                    setFormData({ ...formData, country: '' });
                                                } else {
                                                    setFormData({ ...formData, country: e.target.value });
                                                }
                                            }}
                                            disabled={saving}
                                            required
                                        >
                                            <option value="">국가를 선택하세요</option>
                                            {COUNTRIES.map(c => (
                                                <option key={c.code} value={c.name}>{c.name}</option>
                                            ))}
                                            <option value="__custom__">+ 직접 입력 (목록에 없음)</option>
                                        </select>
                                    ) : (
                                        <div style={{ display: 'flex', gap: '8px' }}>
                                            <input
                                                type="text"
                                                className="form-control"
                                                value={formData.country || ''}
                                                onChange={e => setFormData({ ...formData, country: e.target.value })}
                                                onBlur={e => {
                                                    const val = e.target.value.trim().toLowerCase();
                                                    if (COUNTRY_NORMALIZATION[val]) {
                                                        setFormData({ ...formData, country: COUNTRY_NORMALIZATION[val] });
                                                    }
                                                }}
                                                placeholder="영문 국가명을 입력하세요 (예: South Korea)"
                                                disabled={saving}
                                                required
                                                autoFocus
                                            />
                                            <button
                                                type="button"
                                                className="btn btn-outline"
                                                style={{ padding: '0 12px', minWidth: 'auto' }}
                                                onClick={() => {
                                                    setIsManualCountry(false);
                                                    setFormData({ ...formData, country: '' });
                                                }}
                                                title="목록에서 선택하기"
                                            >
                                                <i className="fas fa-list"></i>
                                            </button>
                                        </div>
                                    )}
                                    {isManualCountry && (
                                        <span style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>
                                            * 가급적 영문 표준 명칭(예: USA, South Korea)을 사용해 주세요.
                                        </span>
                                    )}
                                </div>
                            </div>
                            <div className="modal-form-group">
                                <label>웹사이트</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.website || ''}
                                    onChange={e => setFormData({ ...formData, website: e.target.value })}
                                    placeholder="https://..."
                                />
                            </div>
                            <div className="checkbox-group">
                                <label className="checkbox-label">
                                    <input
                                        type="checkbox"
                                        checked={!!formData.is_active}
                                        onChange={e => setFormData({ ...formData, is_active: e.target.checked })}
                                    />
                                    활성화 상태
                                </label>
                            </div>
                        </div>
                    </div>
                </form>
            );
        }

        return (
            <div className="modal-form-grid">
                <div className="modal-form-section">
                    <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                    <div className="detail-item">
                        <div className="detail-label">제조사명</div>
                        <div className="detail-value highlight">{manufacturer.name}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">설명</div>
                        <div className="detail-value">{manufacturer.description || '제조사 설명이 없습니다.'}</div>
                    </div>
                </div>

                <div className="modal-form-section">
                    <h3><i className="fas fa-globe"></i> 상세 정보</h3>
                    <div className="detail-item">
                        <div className="detail-label">국가</div>
                        <div className="detail-value">{manufacturer.country || '-'}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">웹사이트</div>
                        <div className="detail-value">
                            {manufacturer.website ? (
                                <a href={manufacturer.website} target="_blank" rel="noopener noreferrer">
                                    {manufacturer.website} <i className="fas fa-external-link-alt" style={{ fontSize: '12px' }}></i>
                                </a>
                            ) : '-'}
                        </div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">활성화 상태</div>
                        <div className="detail-value">
                            <span className={`badge ${manufacturer.is_active ? 'success' : 'neutral'}`}>
                                {manufacturer.is_active ? '활성' : '비활성'}
                            </span>
                        </div>
                    </div>
                </div>
            </div>
        );
    };

    if (!isOpen) return null;

    return (
        <div className="modal-overlay">
            <div className="modal-container manufacturer-modal">
                <div className="modal-header">
                    <div className="modal-title">
                        <div className="title-row">
                            <h2>{isEditing ? '제조사 정보 수정' : '제조사 상세 정보'}</h2>
                        </div>
                    </div>
                    <button className="close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="modal-body">
                    {error && <div className="alert alert-danger">{error}</div>}
                    {renderContent()}
                </div>

                <div className="modal-footer">
                    {!isEditing ? (
                        <div className="footer-right" style={{ width: '100%', display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                            <button className="btn btn-outline" onClick={onClose}>닫기</button>
                            <button className="btn btn-primary" onClick={handleEditToggle}>
                                <i className="fas fa-edit"></i> 수정
                            </button>
                        </div>
                    ) : (
                        <div style={{ width: '100%', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <div className="footer-left">
                                <button
                                    type="button"
                                    className="btn btn-error"
                                    onClick={handleDelete}
                                    disabled={saving}
                                >
                                    <i className="fas fa-trash"></i> 삭제
                                </button>
                            </div>
                            <div className="footer-right" style={{ display: 'flex', gap: '12px' }}>
                                <button className="btn btn-outline" onClick={handleEditToggle} disabled={saving}>취소</button>
                                <button type="submit" form="manufacturer-detail-form" className="btn btn-primary" disabled={saving}>
                                    <i className="fas fa-save"></i> {saving ? '저장 중...' : '저장하기'}
                                </button>
                            </div>
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
};
