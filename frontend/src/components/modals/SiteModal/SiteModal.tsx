import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { Site } from '../../../types/common';
import { SiteApiService } from '../../../api/services/siteApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import './SiteModal.css';

interface SiteModalProps {
    isOpen: boolean;
    onClose: () => void;
    onSave: () => void;
    site: Site | null;
}

export const SiteModal: React.FC<SiteModalProps> = ({
    isOpen,
    onClose,
    onSave,
    site
}) => {
    const { confirm } = useConfirmContext();
    const [formData, setFormData] = useState<Partial<Site>>({
        name: '',
        code: '',
        site_type: 'office',
        description: '',
        location: '',
        address: '',
        timezone: 'Asia/Seoul',
        parent_site_id: undefined,
        is_active: true
    });
    const { t } = useTranslation(['sites', 'common']);
    const [sites, setSites] = useState<Site[]>([]);
    const [saving, setSaving] = useState(false);
    const [loadingSites, setLoadingSites] = useState(false);
    const [error, setError] = useState<string | null>(null);

    const loadPotentialParents = useCallback(async () => {
        try {
            setLoadingSites(true);
            const res = await SiteApiService.getSites();
            if (res.success && res.data) {
                // Exclude self from potential parents if editing
                const filtered = site
                    ? res.data.items.filter(s => s.id !== site.id)
                    : res.data.items;
                setSites(filtered);
            }
        } catch (err) {
            console.error('Failed to load sites:', err);
        } finally {
            setLoadingSites(false);
        }
    }, [site]);

    useEffect(() => {
        if (isOpen) {
            loadPotentialParents();
            if (site) {
                setFormData({
                    ...site, // 🔥 기존의 모든 필드(metadata, tags 등) 보존
                    description: site.description || '',
                    location: site.location || '',
                    address: site.address || ''
                });
            } else {
                setFormData({
                    name: '',
                    code: '',
                    site_type: 'office',
                    description: '',
                    location: '',
                    address: '',
                    timezone: 'Asia/Seoul',
                    parent_site_id: undefined,
                    is_active: true
                });
            }
            setError(null);
        }
    }, [site, isOpen, loadPotentialParents]);

    if (!isOpen) return null;

    const handleSubmit = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!formData.name || !formData.code) {
            setError('Site name and code are required.');
            return;
        }

        try {
            setSaving(true);
            setError(null);

            if (site) {
                const isConfirmed = await confirm({
                    title: 'Save Changes',
                    message: 'Save the changes you have entered?',
                    confirmText: 'Save',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await SiteApiService.updateSite(site.id, formData);
                if (res.success) {
                    await confirm({
                        title: 'Save Complete',
                        message: 'Site information updated successfully.',
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
                    title: 'Register Site',
                    message: 'Register a new site?',
                    confirmText: 'Register',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) return;

                const res = await SiteApiService.createSite(formData);
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
            <div className="mgmt-modal-container site-modal">
                <div className="mgmt-modal-header">
                    <div className="mgmt-modal-title">
                        <h2>{site ? 'Edit Site Info' : 'Register New Site'}</h2>
                    </div>
                    <button className="mgmt-close-btn" onClick={onClose}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="mgmt-modal-body">
                    {error && <div className="mgmt-alert mgmt-alert-danger">{error}</div>}

                    <form id="site-form" onSubmit={handleSubmit}>
                        <div className="mgmt-modal-form-grid">
                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-info-circle"></i> Basic Info</h3>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('table.name', {ns: 'sites'})}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.name}
                                        onChange={e => setFormData({ ...formData, name: e.target.value })}
                                        placeholder="Enter site name (e.g. Seoul HQ)"
                                        disabled={saving}
                                        required
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label className="required">{t('field.code', {ns: 'sites'})}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.code}
                                        onChange={e => setFormData({ ...formData, code: e.target.value })}
                                        placeholder="Enter site code (e.g. HQ-001)"
                                        disabled={saving}
                                        required
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('labels.siteType', {ns: 'sites'})}</label>
                                    <select
                                        className="form-control"
                                        value={formData.site_type}
                                        onChange={e => setFormData({ ...formData, site_type: e.target.value })}
                                        disabled={saving}
                                    >
                                        <option value="company">{t('labels.headquartersBusinessUnit', {ns: 'sites'})}</option>
                                        <option value="office">{t('labels.officebranch', {ns: 'sites'})}</option>
                                        <option value="factory">{t('labels.factoryplant', {ns: 'sites'})}</option>
                                        <option value="building">{t('labels.building', {ns: 'sites'})}</option>
                                        <option value="floor">{t('labels.floor', {ns: 'sites'})}</option>
                                        <option value="room">{t('labels.room', {ns: 'sites'})}</option>
                                    </select>
                                </div>
                            </div>

                            <div className="mgmt-modal-form-section">
                                <h3><i className="fas fa-sitemap"></i> Hierarchy & Location</h3>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('table.parentSite', {ns: 'sites'})}</label>
                                    <select
                                        className="form-control"
                                        value={formData.parent_site_id || ''}
                                        onChange={e => setFormData({ ...formData, parent_site_id: e.target.value ? parseInt(e.target.value) : undefined })}
                                        disabled={saving || loadingSites}
                                    >
                                        <option value="">{t('labels.noneToplevelSite', {ns: 'sites'})}</option>
                                        {sites.map(s => (
                                            <option key={s.id} value={s.id}>{s.name} ({s.code})</option>
                                        ))}
                                    </select>
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('labels.address', {ns: 'sites'})}</label>
                                    <input
                                        type="text"
                                        className="form-control"
                                        value={formData.address}
                                        onChange={e => setFormData({ ...formData, address: e.target.value })}
                                        placeholder="Enter full address"
                                        disabled={saving}
                                    />
                                </div>
                                <div className="mgmt-modal-form-group">
                                    <label>{t('labels.timezone', {ns: 'sites'})}</label>
                                    <select
                                        className="form-control"
                                        value={formData.timezone}
                                        onChange={e => setFormData({ ...formData, timezone: e.target.value })}
                                        disabled={saving}
                                    >
                                        <option value="Asia/Seoul">{t('labels.asiaseoulKst', {ns: 'sites'})}</option>
                                        <option value="UTC">{t('labels.utc', {ns: 'sites'})}</option>
                                        <option value="America/New_York">America/New_York (EST/EDT)</option>
                                        <option value="Europe/London">{t('labels.europelondonGmtbst', {ns: 'sites'})}</option>
                                    </select>
                                </div>
                                <div className="checkbox-group">
                                    <label className="checkbox-label">
                                        <input
                                            type="checkbox"
                                            checked={formData.is_active}
                                            onChange={e => setFormData({ ...formData, is_active: e.target.checked })}
                                            disabled={saving}
                                        />
                                        Active Status
                                    </label>
                                </div>
                            </div>

                            {!site && (
                                <div className="mgmt-modal-form-section mgmt-span-full">
                                    <h3><i className="fas fa-server"></i> Collector 설정 <span style={{ fontSize: '12px', color: 'var(--neutral-400)', fontWeight: 'normal' }}>(auto-created on site registration)</span></h3>
                                    <div className="mgmt-modal-form-row">
                                        <div className="mgmt-modal-form-group">
                                            <label>{t('labels.collectorName', {ns: 'sites'})}</label>
                                            <input
                                                type="text"
                                                className="form-control"
                                                value={(formData as any).collector_name || ''}
                                                onChange={e => setFormData({ ...formData, collector_name: e.target.value } as any)}
                                                placeholder={`${formData.name || 'Site'}-Collector`}
                                                disabled={saving}
                                            />
                                            <span className="mgmt-modal-form-hint">비워두면 "Site-Collector" will be used as default.</span>
                                        </div>
                                        <div className="mgmt-modal-form-group">
                                            <label>{t('labels.collectorDescription', {ns: 'sites'})}</label>
                                            <input
                                                type="text"
                                                className="form-control"
                                                value={(formData as any).collector_description || ''}
                                                onChange={e => setFormData({ ...formData, collector_description: e.target.value } as any)}
                                                placeholder="Optional"
                                                disabled={saving}
                                            />
                                        </div>
                                    </div>
                                </div>
                            )}
                        </div>
                    </form>
                </div>

                <div className="mgmt-modal-footer">
                    <button type="button" className="btn btn-outline" onClick={onClose} disabled={saving}>
                        취소
                    </button>
                    <button type="submit" form="site-form" className="btn btn-primary" disabled={saving}>
                        <i className="fas fa-check"></i> {saving ? '저장 중...' : (site ? '수정 완료' : '등록 완료')}
                    </button>
                </div>
            </div>
        </div>
    );
};
