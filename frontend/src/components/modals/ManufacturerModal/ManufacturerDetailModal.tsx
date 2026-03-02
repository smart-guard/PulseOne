import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { Manufacturer } from '../../../types/manufacturing';
import { ManufactureApiService } from '../../../api/services/manufactureApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import { COUNTRIES, COUNTRY_NAME_TO_CODE } from '../../../constants/countries';
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
    const { t } = useTranslation('manufacturers');
    const [manufacturer, setManufacturer] = useState<Manufacturer | null>(null);
    const [isEditing, setIsEditing] = useState(false);
    const [formData, setFormData] = useState<Partial<Manufacturer>>({});
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [isManualCountry, setIsManualCountry] = useState(false);

    // Legacy stored name → code resolver (for existing DB values)
    const resolveCountryCode = (value: string): string => {
        if (COUNTRIES.some(c => c.code === value)) return value;
        const byCode = COUNTRY_NAME_TO_CODE[value.trim().toLowerCase()];
        if (byCode) return byCode;
        const matched = COUNTRIES.find(c =>
            t(`countries.${c.code}`).toLowerCase() === value.trim().toLowerCase()
        );
        return matched ? matched.code : value;
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
                const code = resolveCountryCode(res.data.country || '');
                setFormData({
                    name: res.data.name,
                    description: res.data.description || '',
                    country: code,
                    website: res.data.website || '',
                    logo_url: res.data.logo_url || '',
                    is_active: res.data.is_active
                });

                const isInList = COUNTRIES.some(c => c.code === code);
                setIsManualCountry(!!res.data.country && !isInList);

                setError(null);
            }
        } catch (err: any) {
            setError('Failed to load manufacturer information.');
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
            setError('Manufacturer name is required.');
            return;
        }

        if (!manufacturerId) return;

        const isConfirmed = await confirm({
            title: 'Save Changes',
            message: 'Save the entered changes?',
            confirmText: 'Save',
            confirmButtonType: 'primary'
        });

        if (!isConfirmed) return;

        try {
            setSaving(true);
            setError(null);
            const res = await ManufactureApiService.updateManufacturer(manufacturerId, formData);
            if (res.success) {
                await confirm({
                    title: 'Save Complete',
                    message: 'Manufacturer information updated successfully.',
                    confirmText: 'OK',
                    showCancelButton: false,
                    confirmButtonType: 'primary'
                });
                await loadManufacturer(manufacturerId);
                onSave();
                onClose();
            } else {
                setError(res.message || 'Update failed.');
            }
        } catch (err: any) {
            setError(err.message || 'Server communication error.');
        } finally {
            setSaving(false);
        }
    };

    const handleDelete = async () => {
        if (!manufacturer) return;

        const confirmed = await confirm({
            title: 'Confirm Delete Manufacturer',
            message: `Delete manufacturer '${manufacturer.name}'? Cannot be deleted if linked models exist.`,
            confirmText: 'Delete',
            confirmButtonType: 'danger'
        });

        if (confirmed) {
            try {
                setSaving(true);
                const res = await ManufactureApiService.deleteManufacturer(manufacturer.id);
                if (res.success) {
                    await confirm({
                        title: 'Delete Complete',
                        message: 'Manufacturer deleted successfully.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'primary'
                    });
                    onSave();
                    onClose();
                } else {
                    await confirm({
                        title: 'Delete Failed',
                        message: res.message || 'Delete failed.',
                        confirmText: 'OK',
                        showCancelButton: false,
                        confirmButtonType: 'danger'
                    });
                }
            } catch (err: any) {
                await confirm({
                    title: 'Error',
                    message: err.message || 'Error occurred during deletion.',
                    confirmText: 'OK',
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
                    <i className="fas fa-spinner fa-spin"></i> Loading...
                </div>
            );
        }

        if (!manufacturer) {
            return <div className="alert alert-danger">{t('labels.failedToLoadManufacturerInformation', {ns: 'manufacturers'})}</div>;
        }

        if (isEditing) {
            return (
                <form id="manufacturer-detail-form" onSubmit={handleSubmit}>
                    <div className="mgmt-modal-form-grid">
                        <div className="mgmt-modal-form-section">
                            <h3><i className="fas fa-info-circle"></i> Basic Info</h3>
                            <div className="mgmt-modal-form-group">
                                <label className="required">{t('table.name', {ns: 'manufacturers'})}</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.name || ''}
                                    onChange={e => setFormData({ ...formData, name: e.target.value })}
                                    placeholder="Enter manufacturer name"
                                    required
                                />
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>{t('table.description', {ns: 'manufacturers'})}</label>
                                <textarea
                                    className="form-control"
                                    value={formData.description || ''}
                                    onChange={e => setFormData({ ...formData, description: e.target.value })}
                                    placeholder="Enter a description for the manufacturer"
                                    rows={3}
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
                                            <option value="">{t('labels.selectACountry', {ns: 'manufacturers'})}</option>
                                            {COUNTRIES
                                                .slice()
                                                .sort((a, b) => t(`countries.${a.code}`).localeCompare(t(`countries.${b.code}`)))
                                                .map(c => (
                                                    <option key={c.code} value={c.code}>{t(`countries.${c.code}`)}</option>
                                                ))}
                                            <option value="__custom__">+ Direct Input (not in list)</option>
                                        </select>
                                    ) : (
                                        <div style={{ display: 'flex', gap: '8px' }}>
                                            <input
                                                type="text"
                                                className="form-control"
                                                value={formData.country || ''}
                                                onChange={e => setFormData({ ...formData, country: e.target.value })}
                                                onBlur={e => {
                                                    const code = resolveCountryCode(e.target.value);
                                                    if (code !== e.target.value) {
                                                        setFormData({ ...formData, country: code });
                                                    }
                                                }}
                                                placeholder="Enter country name in English (e.g. South Korea)"
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
                                            * Preferably use standard English names (e.g. USA, South Korea).
                                        </span>
                                    )}
                                </div>
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>{t('labels.website', {ns: 'manufacturers'})}</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.website || ''}
                                    onChange={e => setFormData({ ...formData, website: e.target.value })}
                                    placeholder="https://..."
                                />
                            </div>
                            <div className="mgmt-modal-form-group">
                                <label>{t('labels.logoImageUrl', {ns: 'manufacturers'})}</label>
                                <input
                                    type="text"
                                    className="form-control"
                                    value={formData.logo_url || ''}
                                    onChange={e => setFormData({ ...formData, logo_url: e.target.value })}
                                    placeholder="https://.../logo.png"
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
                                        checked={!!formData.is_active}
                                        onChange={e => setFormData({ ...formData, is_active: e.target.checked })}
                                    />
                                    Active Status
                                </label>
                            </div>
                        </div>
                    </div>
                </form>
            );
        }

        return (
            <div className="mgmt-modal-form-grid">
                <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-info-circle"></i> Basic Info</h3>
                    <div className="detail-item">
                        <div className="detail-label">{t('table.name', {ns: 'manufacturers'})}</div>
                        <div className="detail-value highlight">{manufacturer.name}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">{t('table.description', {ns: 'manufacturers'})}</div>
                        <div className="detail-value">{manufacturer.description || 'No description available.'}</div>
                    </div>
                </div>

                <div className="mgmt-modal-form-section">
                    <h3><i className="fas fa-globe"></i> Details</h3>
                    <div className="detail-item">
                        <div className="detail-label">{t('table.country', {ns: 'manufacturers'})}</div>
                        <div className="detail-value">{manufacturer.country ? t(`countries.${manufacturer.country}`, { defaultValue: manufacturer.country }) : '-'}</div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">{t('labels.website', {ns: 'manufacturers'})}</div>
                        <div className="detail-value">
                            {manufacturer.website ? (
                                <a href={manufacturer.website} target="_blank" rel="noopener noreferrer">
                                    {manufacturer.website} <i className="fas fa-external-link-alt" style={{ fontSize: '12px' }}></i>
                                </a>
                            ) : '-'}
                        </div>
                    </div>
                    <div className="detail-item">
                        <div className="detail-label">{t('labels.activeStatus', {ns: 'manufacturers'})}</div>
                        <div className="detail-value">
                            <span className={`badge ${manufacturer.is_active ? 'success' : 'neutral'}`}>
                                {manufacturer.is_active ? 'Active' : 'Inactive'}
                            </span>
                        </div>
                    </div>
                    {manufacturer.logo_url && (
                        <div className="detail-item">
                            <div className="detail-label">{t('labels.manufacturerLogo', {ns: 'manufacturers'})}</div>
                            <div className="detail-value" style={{ marginTop: '8px' }}>
                                <img src={manufacturer.logo_url} alt="Manufacturer Logo" style={{ maxHeight: '60px', maxWidth: '150px', objectFit: 'contain' }} />
                            </div>
                        </div>
                    )}
                </div>
            </div>
        );
    };

    if (!isOpen) return null;

    return (
        <div className="mgmt-modal-overlay">
            <div className="mgmt-modal-container manufacturer-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <div className="title-row">
                            <h2>{isEditing ? 'Edit Manufacturer' : 'Manufacturer Details'}</h2>
                        </div>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body">
                    {error && <div className="alert alert-danger">{error}</div>}
                    {renderContent()}
                </div>

                <div className="mgmt-modal-footer">
                    {!isEditing ? (
                        <div className="footer-right" style={{ width: '100%', display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                            <button className="btn btn-outline" onClick={onClose}>Close</button>
                            <button className="btn btn-primary" onClick={handleEditToggle}>
                                <i className="fas fa-edit"></i> Edit
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
                                    <i className="fas fa-trash"></i> Delete
                                </button>
                            </div>
                            <div className="footer-right" style={{ display: 'flex', gap: '12px' }}>
                                <button className="btn btn-outline" onClick={handleEditToggle} disabled={saving}>Cancel</button>
                                <button type="submit" form="manufacturer-detail-form" className="btn btn-primary" disabled={saving}>
                                    <i className="fas fa-save"></i> {saving ? 'Saving...' : 'Save'}
                                </button>
                            </div>
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
};
