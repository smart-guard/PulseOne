import React, { useState, useEffect } from 'react';
import { Manufacturer } from '../../../types/manufacturing';
import { ManufactureApiService } from '../../../api/services/manufactureApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import { COUNTRIES } from '../../../constants/countries';
import './ManufacturerModal.css';

interface ManufacturerModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    manufacturer: Manufacturer | null;
}

export const ManufacturerModal: React.FC<ManufacturerModalProps> = ({
    isOpen,
    onClose,
    onSave,
    manufacturer
}) => {
    const { confirm } = useConfirmContext();
    const [formData, setFormData] = useState<Partial<Manufacturer>>({
        name: '',
        description: '',
        country: '',
        website: '',
        logo_url: '',
        is_active: true
    });
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
        if (manufacturer) {
            setFormData({
                name: manufacturer.name,
                description: manufacturer.description || '',
                country: manufacturer.country || '',
                website: manufacturer.website || '',
                logo_url: manufacturer.logo_url || '',
                is_active: manufacturer.is_active
            });
            // 기존 국가명이 리스트에 없으면 직접 입력 모드로 시작
            const isInList = COUNTRIES.some(c => c.name === manufacturer.country);
            setIsManualCountry(!!manufacturer.country && !isInList);
        } else {
            setFormData({
                name: '',
                description: '',
                country: '',
                website: '',
                logo_url: '',
                is_active: true
            });
            setIsManualCountry(false);
        }
        setError(null);
    }, [manufacturer, isOpen]);

    if (!isOpen) return null;

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!formData.name) {
            setError('제조사명은 필수 항목입니다.');
            return;
        }

        try {
            setSaving(true);
            setError(null);

            if (manufacturer) {
                const isConfirmed = await confirm({
                    title: '변경 사항 저장',
                    message: '입력하신 변경 내용을 저장하시겠습니까?',
                    confirmText: '저장',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await ManufactureApiService.updateManufacturer(manufacturer.id, formData);
                if (res.success) {
                    await confirm({
                        title: '저장 완료',
                        message: '제조사 정보가 성공적으로 수정되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || '업데이트에 실패했습니다.');
                }
            } else {
                const isConfirmed = await confirm({
                    title: '제조사 등록',
                    message: '새로운 제조사를 등록하시겠습니까?',
                    confirmText: '등록',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await ManufactureApiService.createManufacturer(formData);
                if (res.success) {
                    await confirm({
                        title: '등록 완료',
                        message: '성공적으로 등록되었습니다.',
                        confirmText: '확인',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || '등록에 실패했습니다.');
                }
            }
        } catch (err: any) {
            setError(err.message || '서버 통신 중 오류가 발생했습니다.');
        } finally {
            setSaving(false);
        }
    };

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container manufacturer-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <div className="title-row">
                            <h2>{manufacturer ? '제조사 정보 수정' : '새 제조사 등록'}</h2>
                        </div>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body">
                    {error && <div className="alert alert-danger">{error}</div>}

                    <form id="manufacturer-form" onSubmit={handleSubmit}>
                        <div className="mgmt-modal-form-grid">
                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-info-circle"></i> 기본 정보</h3>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">제조사명</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.name}
                                        onChange={e => setFormData({ ...formData, name: e.target.value })}
                                        placeholder="제조사 이름을 입력하세요"
                                        disabled={saving}
                                        required
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>설명</label>
                                    <textarea
                                        className="form-control"
                                        value={formData.description}
                                        onChange={e => setFormData({ ...formData, description: e.target.value })}
                                        placeholder="제조사에 대한 설명을 입력하세요"
                                        rows={3}
                                        disabled={saving}
                                    />
                                </div>
                            </div>

                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-globe"></i> 상세 정보</h3>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">국가</label>
                                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                                        {!isManualCountry ? (
                                            <select
                                                className="form-control"
                                                value={formData.country}
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
                                                    value={formData.country}
                                                    onChange={e => {
                                                        const val = e.target.value;
                                                        setFormData({ ...formData, country: val });
                                                    }}
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
                                <div className="mgmt-modal-form-group">
                                    <label>웹사이트</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.website}
                                        onChange={e => setFormData({ ...formData, website: e.target.value })}
                                        placeholder="https://..."
                                        disabled={saving}
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>로고 이미지 URL</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.logo_url}
                                        onChange={e => setFormData({ ...formData, logo_url: e.target.value })}
                                        placeholder="https://.../logo.png"
                                        disabled={saving}
                                    />
                                    {formData.logo_url && (
                                        <div className="logo-preview" style={{ marginTop: '8px', textAlign: 'center' }}>
                                            <img src={formData.logo_url} alt="Logo Preview" style={{ maxHeight: '40px', maxWidth: '100px' }} onError={(e) => (e.currentTarget.style.display = 'none')} />
                                        </div>
                                    )}
                                </div>
                                <div className="checkbox-group">
                                    <label className="checkbox-label">
                                        <input
                                            type="checkbox"
                                            checked={formData.is_active}
                                            onChange={e => setFormData({ ...formData, is_active: e.target.checked })}
                                            disabled={saving}
                                        />
                                        활성화 상태
                                    </label>
                                </div>
                            </div>
                        </div>
                    </form>
                </div>
                <div className="mgmt-modal-footer">
                    <div className="footer-right" style={{ width: '100%', display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                        <button type="button" className="btn btn-outline" onClick={onClose} disabled={saving}>
                            취소
                        </button>
                        <button type="submit" form="manufacturer-form" className="btn btn-primary" disabled={saving}>
                            <i className="fas fa-check"></i> {saving ? '저장 중...' : (manufacturer ? '수정 완료' : '등록 완료')}
                        </button>
                    </div>
                </div>
            </div>
        </div>
    );
};
