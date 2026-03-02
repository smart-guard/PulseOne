import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { Manufacturer } from '../../../types/manufacturing';
import { ManufactureApiService } from '../../../api/services/manufactureApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import { COUNTRIES, COUNTRY_NAME_TO_CODE } from '../../../constants/countries';
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
    const { t } = useTranslation('manufacturers');
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

    // Legacy stored name → code resolver (for existing DB values)
    const resolveCountryCode = (value: string): string => {
        // Already a code
        if (COUNTRIES.some(c => c.code === value)) return value;
        // Try legacy map
        const byCode = COUNTRY_NAME_TO_CODE[value.trim().toLowerCase()];
        if (byCode) return byCode;
        // Fallback: find by translated name in current locale
        const matched = COUNTRIES.find(c =>
            t(`countries.${c.code}`).toLowerCase() === value.trim().toLowerCase()
        );
        return matched ? matched.code : value;
    };

    useEffect(() => {
        if (manufacturer) {
            const code = resolveCountryCode(manufacturer.country || '');
            setFormData({
                name: manufacturer.name,
                description: manufacturer.description || '',
                country: code,
                website: manufacturer.website || '',
                logo_url: manufacturer.logo_url || '',
                is_active: manufacturer.is_active
            });
            const isInList = COUNTRIES.some(c => c.code === code);
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
            setError('Manufacturer name is required.');
            return;
        }

        try {
            setSaving(true);
            setError(null);

            if (manufacturer) {
                const isConfirmed = await confirm({
                    title: 'Save Changes',
                    message: 'Save the changes you have entered?',
                    confirmText: 'Save',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await ManufactureApiService.updateManufacturer(manufacturer.id, formData);
                if (res.success) {
                    await confirm({
                        title: 'Save Complete',
                        message: 'Manufacturer updated successfully.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || 'Update failed.');
                }
            } else {
                const isConfirmed = await confirm({
                    title: 'Register Manufacturer',
                    message: 'Register a new manufacturer?',
                    confirmText: 'Register',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await ManufactureApiService.createManufacturer(formData);
                if (res.success) {
                    await confirm({
                        title: 'Registration Complete',
                        message: 'Registered successfully.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    setError(res.message || 'Registration failed.');
                }
            }
        } catch (err: any) {
            setError(err.message || 'Error communicating with server.');
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
                            <h2>{manufacturer ? 'Edit Manufacturer' : 'Register New Manufacturer'}</h2>
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
                                <h3><i className="fas fa-info-circle"></i> Basic Info</h3>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('table.name', {ns: 'manufacturers'})}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.name}
                                        onChange={e => setFormData({ ...formData, name: e.target.value })}
                                        placeholder="Enter manufacturer name"
                                        disabled={saving}
                                        required
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('table.description', {ns: 'manufacturers'})}</label>
                                    <textarea
                                        className="form-control"
                                        value={formData.description}
                                        onChange={e => setFormData({ ...formData, description: e.target.value })}
                                        placeholder="Enter description about the manufacturer"
                                        rows={3}
                                        disabled={saving}
                                    />
                                </div>
                            </div>

                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-globe"></i> Details</h3>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('table.country', {ns: 'manufacturers'})}</label>
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
                                                {COUNTRIES
                                                    .slice()
                                                    .sort((a, b) => t(`countries.${a.code}`).localeCompare(t(`countries.${b.code}`)))
                                                    .map(c => (
                                                        <option key={c.code} value={c.code}>{t(`countries.${c.code}`)}</option>
                                                    ))}
                                                <option value="__custom__">+ Enter manually (not in list)</option>
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
                                                        const code = resolveCountryCode(e.target.value);
                                                        if (code !== e.target.value) {
                                                            setFormData({ ...formData, country: code });
                                                        }
                                                    }}
                                                    placeholder="Enter country name (e.g. 대한민국, Deutschland)"
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
                                                    title="Select from list"
                                                >
                                                    <i className="fas fa-list"></i>
                                                </button>
                                            </div>
                                        )}
                                        {isManualCountry && (
                                            <span style={{ fontSize: '11px', color: 'var(--neutral-500)' }}>
                                                * Please use the standard native-language name where possible.
                                            </span>
                                        )}
                                    </div>
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('labels.website', {ns: 'manufacturers'})}</label>
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
                                    <label>{t('labels.logoImageUrl', {ns: 'manufacturers'})}</label>
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
                                        Active
                                    </label>
                                </div>
                            </div>
                        </div>
                    </form>
                </div>
                <div className="mgmt-modal-footer">
                    <div className="footer-right" style={{ width: '100%', display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                        <button type="button" className="btn btn-outline" onClick={onClose} disabled={saving}>
                            Cancel
                        </button>
                        <button type="submit" form="manufacturer-form" className="btn btn-primary" disabled={saving}>
                            <i className="fas fa-check"></i> {saving ? 'Saving...' : (manufacturer ? 'Save Changes' : 'Register')}
                        </button>
                    </div>
                </div>
            </div>
        </div>
    );
};
