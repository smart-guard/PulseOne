import React, { useState, useEffect } from 'react';
import { GroupApiService, DeviceGroup } from '../../../api/services/groupApi';
// import { useConfirmContext } from '../../../components/common/ConfirmProvider';

interface GroupSidePanelProps {
    selectedGroupId: number | 'all';
    onGroupSelect: (groupId: number | 'all') => void;
    onRefresh?: () => void;
    selectedDevicesCount?: number;
    onMoveSelectedToGroup?: (groupId: number) => void;
    isCollapsed?: boolean;
    onToggleCollapse?: () => void;
}

const GroupSidePanel: React.FC<GroupSidePanelProps> = ({
    selectedGroupId,
    onGroupSelect,
    selectedDevicesCount = 0,
    onMoveSelectedToGroup,
    isCollapsed = false,
    onToggleCollapse
}) => {
    const [groups, setGroups] = useState<DeviceGroup[]>([]);
    const [loading, setLoading] = useState(true);
    const [expandedGroups, setExpandedGroups] = useState<Set<number>>(new Set());

    // Management State
    const [isManageMode, setIsManageMode] = useState(false);
    const [editTargetId, setEditTargetId] = useState<number | null>(null); // ID of group being edited
    const [createTargetParentId, setCreateTargetParentId] = useState<number | 'root' | null>(null); // Parent ID for new group
    const [formData, setFormData] = useState({ name: '' });
    const [actionLoading, setActionLoading] = useState(false);
    const [moveTargetGroupId, setMoveTargetGroupId] = useState<number | null>(null); // ID of group being moved

    useEffect(() => {
        loadGroups();
    }, []);

    const loadGroups = async () => {
        try {
            if (!actionLoading) setLoading(true);
            const response = await GroupApiService.getGroupTree();
            if (response.success && response.data) {
                setGroups(response.data);
            }
        } catch (error) {
            console.error('Failed to load group tree:', error);
        } finally {
            setLoading(false);
        }
    };

    const toggleExpand = (groupId: number, e: React.MouseEvent) => {
        e.stopPropagation();
        setExpandedGroups(prev => {
            const next = new Set(prev);
            if (next.has(groupId)) next.delete(groupId);
            else next.add(groupId);
            return next;
        });
    };

    // --- Actions ---
    // Reverting to window.confirm/alert temporarily to fix white screen crash
    // const { confirm } = useConfirmContext();

    const handleCreateStart = (parentId: number | 'root', e?: React.MouseEvent) => {
        e?.stopPropagation();
        setCreateTargetParentId(parentId);
        setEditTargetId(null);
        setFormData({ name: '' });
        // Expand parent if needed
        if (typeof parentId === 'number') {
            setExpandedGroups(prev => new Set(prev).add(parentId));
        }
    };

    const handleEditStart = (group: DeviceGroup, e: React.MouseEvent) => {
        e.stopPropagation();
        setEditTargetId(group.id);
        setCreateTargetParentId(null);
        setFormData({ name: group.name });
    };

    const handleCancel = (e?: React.MouseEvent) => {
        e?.stopPropagation();
        setCreateTargetParentId(null);
        setEditTargetId(null);
        setFormData({ name: '' });
    };

    const handleSaveCreate = async (e?: React.FormEvent) => {
        e?.preventDefault();
        e?.stopPropagation(); // Stop propagation to prevent page refresh contexts
        if (!formData.name.trim()) return;

        try {
            setActionLoading(true);
            const parentId = createTargetParentId === 'root' ? null : createTargetParentId;
            const res = await GroupApiService.createGroup({
                name: formData.name,
                parent_group_id: parentId
            });
            if (res.success) {
                await loadGroups();
                handleCancel();
            } else {
                alert(`생성 실패: ${res.error || '그룹 생성에 실패했습니다.'}`);
            }
        } catch (err: any) {
            console.error(err);
            alert(`오류 발생: ${err.message || '알 수 없는 오류가 발생했습니다.'}`);
        } finally {
            setActionLoading(false);
        }
    };

    const handleSaveEdit = async (e?: React.FormEvent) => {
        e?.preventDefault();
        e?.stopPropagation();
        if (!formData.name.trim() || !editTargetId) return;

        try {
            setActionLoading(true);
            const res = await GroupApiService.updateGroup(editTargetId, {
                name: formData.name
            });
            if (res.success) {
                await loadGroups();
                handleCancel();
            } else {
                alert(`수정 실패: ${res.error || '그룹 수정에 실패했습니다.'}`);
            }
        } catch (err: any) {
            console.error(err);
            alert(`오류 발생: ${err.message || '알 수 없는 오류가 발생했습니다.'}`);
        } finally {
            setActionLoading(false);
        }
    };

    const handleDelete = async (groupId: number, e: React.MouseEvent) => {
        e.stopPropagation();

        if (!window.confirm('정말 이 그룹을 삭제하시겠습니까?\n하위 그룹이나 장치가 포함되어 있다면 삭제되지 않을 수 있습니다.')) return;

        try {
            setActionLoading(true);
            const res = await GroupApiService.deleteGroup(groupId);
            if (res.success) {
                await loadGroups();
            } else {
                alert(`삭제 실패: ${res.error || '하위 그룹이나 장치가 있는지 확인해주세요.'}`);
            }
        } catch (err: any) {
            console.error(err);
            alert(`오류 발생: ${err.message || '삭제 중 오류가 발생했습니다.'}`);
        } finally {
            setActionLoading(false);
        }
    };

    const handleMoveGroup = async (groupId: number, parentId: number | null) => {
        try {
            setActionLoading(true);
            const res = await GroupApiService.updateGroup(groupId, {
                parent_group_id: parentId
            });
            if (res.success) {
                setMoveTargetGroupId(null);
                await loadGroups();
            } else {
                alert(`이동 실패: ${res.error || '그룹 이동에 실패했습니다.'}`);
            }
        } catch (error) {
            console.error('Failed to move group:', error);
        } finally {
            setActionLoading(false);
        }
    };

    // --- Renderers ---

    const renderFormInfo = (onSubmit: (e: React.FormEvent) => void) => (
        <form onSubmit={onSubmit} className="group-inline-form" onClick={e => e.stopPropagation()}>
            <input
                type="text"
                autoFocus
                className="group-inline-input"
                value={formData.name}
                onChange={e => setFormData({ name: e.target.value })}
                placeholder="그룹 이름"
            />
            <div className="group-inline-actions">
                <button type="submit" className="btn-icon-check" disabled={actionLoading}><i className="fas fa-check"></i></button>
                <button type="button" className="btn-icon-cancel" onClick={(e) => handleCancel(e)}><i className="fas fa-times"></i></button>
            </div>
        </form>
    );

    const renderGroupItem = (group: DeviceGroup, level: number = 0) => {
        const isExpanded = expandedGroups.has(group.id);
        const isSelected = selectedGroupId === group.id;
        const hasChildren = group.children && group.children.length > 0;
        const isEditingThis = editTargetId === group.id;
        const isCreatingChild = createTargetParentId === group.id;

        return (
            <div key={group.id} className="group-tree-item-container">
                {isEditingThis ? (
                    <div className="group-tree-item editing" style={{ paddingLeft: `${level * 16 + 8}px` }}>
                        {renderFormInfo(handleSaveEdit)}
                    </div>
                ) : (
                    <div
                        className={`group-tree-item ${isSelected ? 'active' : ''}`}
                        style={{ paddingLeft: `${level * 16 + 8}px` }}
                        onClick={() => !isManageMode && onGroupSelect(group.id)}
                    >
                        <span className="expand-icon" onClick={(e) => toggleExpand(group.id, e)}>
                            {hasChildren ? (
                                <i className={`fas ${isExpanded ? 'fa-chevron-down' : 'fa-chevron-right'}`}></i>
                            ) : (
                                <i className="fas fa-circle" style={{ fontSize: '4px', opacity: 0.3 }}></i>
                            )}
                        </span>
                        <i className={`fas ${group.group_type === 'physical' ? 'fa-building' : 'fa-folder'} group-icon`}></i>
                        <span className="group-name">{group.name}</span>
                        {!isManageMode && group.device_count !== undefined && (
                            <span className="device-count">{group.device_count}</span>
                        )}

                        {isManageMode && (
                            <div className="item-actions">
                                {moveTargetGroupId === null ? (
                                    <>
                                        <button className="btn-mini-action add" onClick={(e) => handleCreateStart(group.id, e)} title="하위 그룹 추가">
                                            <i className="fas fa-plus"></i>
                                        </button>
                                        <button className="btn-mini-action edit" onClick={(e) => { e.stopPropagation(); setMoveTargetGroupId(group.id); }} title="그룹 이동">
                                            <i className="fas fa-expand-arrows-alt"></i>
                                        </button>
                                        <button className="btn-mini-action edit" onClick={(e) => handleEditStart(group, e)} title="이름 변경">
                                            <i className="fas fa-pen"></i>
                                        </button>
                                        <button className="btn-mini-action delete" onClick={(e) => handleDelete(group.id, e)} title="삭제">
                                            <i className="fas fa-trash"></i>
                                        </button>
                                    </>
                                ) : moveTargetGroupId === group.id ? (
                                    <button className="btn-mini-action cancel" onClick={(e) => { e.stopPropagation(); setMoveTargetGroupId(null); }} title="이동 취소">
                                        <i className="fas fa-times"></i> 취소
                                    </button>
                                ) : (
                                    <button
                                        className="btn-mini-action confirm"
                                        onClick={(e) => { e.stopPropagation(); handleMoveGroup(moveTargetGroupId, group.id); }}
                                        title="이치 위치로 이동"
                                    >
                                        <i className="fas fa-level-down-alt"></i> 여기로
                                    </button>
                                )}

                                {selectedDevicesCount > 0 && (
                                    <button
                                        className="btn-mini-action move-here"
                                        onClick={(e) => { e.stopPropagation(); onMoveSelectedToGroup?.(group.id); }}
                                        title={`${selectedDevicesCount}개 장치를 여기로 이동`}
                                    >
                                        <i className="fas fa-arrow-left"></i> 배정
                                    </button>
                                )}
                            </div>
                        )}
                    </div>
                )}

                {/* Children Rendering */}
                {((hasChildren && isExpanded) || isCreatingChild) && (
                    <div className="group-children">
                        {isCreatingChild && (
                            <div className="group-tree-item editing" style={{ paddingLeft: `${(level + 1) * 16 + 8}px` }}>
                                <i className="fas fa-level-up-alt fa-rotate-90 style-sub-icon"></i>
                                {renderFormInfo(handleSaveCreate)}
                            </div>
                        )}
                        {group.children?.map(child => renderGroupItem(child, level + 1))}
                    </div>
                )}
            </div>
        );
    };

    return (
        <div className={`group-side-panel ${isManageMode ? 'manage-mode' : ''} ${isCollapsed ? 'collapsed' : ''}`}>
            <div className="panel-header">
                {!isCollapsed ? (
                    <div className="panel-header-main">
                        <div className="header-title-wrapper">
                            <h3><i className="fas fa-sitemap"></i> {isManageMode ? '그룹 편집' : '계층 구조'}</h3>
                            {!isManageMode && (
                                <button type="button" className="btn-icon-refresh" onClick={loadGroups} title="새로고침">
                                    <i className={`fas fa-sync-alt ${loading ? 'fa-spin' : ''}`}></i>
                                </button>
                            )}
                        </div>
                        <button
                            type="button"
                            className="btn-toggle-sidebar"
                            onClick={onToggleCollapse}
                            title="사이드바 접기"
                        >
                            <i className="fas fa-chevron-left"></i>
                        </button>
                    </div>
                ) : (
                    <button
                        type="button"
                        className="btn-toggle-sidebar collapsed"
                        onClick={onToggleCollapse}
                        title="사이드바 펼치기"
                    >
                        <i className="fas fa-chevron-right"></i>
                    </button>
                )}
            </div>

            {!isCollapsed && (
                <div className="group-tree-content">
                    {!isManageMode && (
                        <div
                            className={`group-tree-item all-devices ${selectedGroupId === 'all' ? 'active' : ''}`}
                            onClick={() => onGroupSelect('all')}
                        >
                            <i className="fas fa-th-list group-icon"></i>
                            <span className="group-name">전체 장치</span>
                        </div>
                    )}

                    {/* Root Level Creation Form */}
                    {isManageMode && createTargetParentId === 'root' && (
                        <div className="group-tree-item editing root-create">
                            {renderFormInfo(handleSaveCreate)}
                        </div>
                    )}

                    {loading && groups.length === 0 ? (
                        <div className="tree-loading">
                            <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                        </div>
                    ) : (
                        <div className="tree-list">
                            {groups && Array.isArray(groups) && groups.map(group => renderGroupItem(group))}
                        </div>
                    )}
                </div>
            )}

            {!isCollapsed && (
                <div className="panel-footer">
                    {isManageMode ? (
                        <div className="manage-actions">
                            <button
                                type="button"
                                className="btn-outline primary full-width"
                                onClick={(e) => handleCreateStart('root', e)}
                            >
                                <i className="fas fa-plus"></i> 최상위 그룹
                            </button>
                            {moveTargetGroupId !== null && (
                                <button
                                    type="button"
                                    className="btn-outline warning full-width"
                                    onClick={(e) => { e.preventDefault(); e.stopPropagation(); handleMoveGroup(moveTargetGroupId, null); }}
                                    style={{ marginTop: '8px' }}
                                >
                                    <i className="fas fa-arrow-up"></i> 최상위(Root)로 이동
                                </button>
                            )}
                            <button
                                type="button"
                                className="btn-primary full-width"
                                onClick={(e) => {
                                    e.preventDefault();
                                    e.stopPropagation();
                                    setIsManageMode(false);
                                    handleCancel();
                                }}>
                                <i className="fas fa-check"></i> 편집 완료
                            </button>
                        </div>
                    ) : (
                        <button
                            type="button"
                            className="btn-outline primary"
                            onClick={(e) => {
                                e.preventDefault();
                                e.stopPropagation();
                                setIsManageMode(true);
                            }}
                        >
                            <i className="fas fa-cog"></i> 그룹 관리
                        </button>
                    )}
                </div>
            )}

            <style>{`
                .group-side-panel.manage-mode .group-tree-item:hover {
                    background-color: #f8fafc; /* Lighter hover in edit mode */
                }
                .item-actions {
                    display: none;
                    margin-left: auto;
                    gap: 4px;
                }
                .group-tree-item:hover .item-actions {
                    display: flex;
                }
                .btn-mini-action {
                    padding: 2px 4px;
                    border: none;
                    background: transparent;
                    cursor: pointer;
                    color: #94a3b8;
                    font-size: 11px;
                    border-radius: 3px;
                }
                .btn-mini-action:hover { background: #e2e8f0; color: #475569; }
                .btn-mini-action.delete:hover { color: #ef4444; background: #fee2e2; }
                .btn-mini-action.add:hover { color: #0ea5e9; background: #e0f2fe; }

                .group-inline-form {
                    display: flex;
                    align-items: center;
                    width: 100%;
                    gap: 4px;
                }
                .group-inline-input {
                    flex: 1;
                    min-width: 0;
                    padding: 2px 6px;
                    font-size: 12px;
                    border: 1px solid #3b82f6;
                    border-radius: 3px;
                    outline: none;
                }
                .group-inline-actions {
                    display: flex;
                    flex-shrink: 0;
                }
                .btn-icon-check { color: #16a34a; background: none; border: none; cursor: pointer; padding: 2px; }
                .btn-icon-cancel { color: #ef4444; background: none; border: none; cursor: pointer; padding: 2px; }
                
                .group-tree-item.editing {
                    background: #f0f9ff;
                    cursor: default;
                }
                .style-sub-icon {
                    color: #cbd5e1;
                    margin-right: 6px;
                    font-size: 10px;
                }
                .manage-actions {
                    display: flex;
                    flex-direction: column;
                    gap: 8px;
                    width: 100%;
                }
                .full-width { width: 100%; justify-content: center; }
                .root-create { padding: 8px 12px; border-bottom: 1px solid #f1f5f9; }

                .btn-toggle-sidebar {
                    background: none;
                    border: none;
                    color: var(--neutral-500);
                    cursor: pointer;
                    padding: 4px 8px;
                    border-radius: 4px;
                    transition: all 0.2s;
                    margin-left: auto;
                }
                .btn-toggle-sidebar:hover {
                    background: var(--neutral-100);
                    color: var(--primary-color);
                }

                .group-side-panel.collapsed {
                    width: 48px !important;
                    min-width: 48px !important;
                    padding: var(--space-3) 0;
                    overflow: hidden;
                }
                .group-side-panel.collapsed .panel-header {
                    justify-content: center;
                    padding: var(--space-2) 0;
                }
                .btn-icon-refresh {
                    background: none;
                    border: none;
                    color: var(--neutral-400);
                    cursor: pointer;
                    padding: 4px;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    transition: color 0.2s;
                    font-size: 13px;
                }
                .btn-icon-refresh:hover {
                    color: var(--primary-color);
                }
                .header-title-wrapper {
                    display: flex;
                    align-items: center;
                    gap: var(--space-2);
                }
                .panel-header-main {
                    display: flex;
                    justify-content: space-between;
                    align-items: center;
                    width: 100%;
                }
                .group-side-panel .group-tree-content {
                    padding-top: var(--space-1);
                }
                .group-side-panel .panel-header {
                    padding: var(--space-2) var(--space-4);
                }
            `}</style>
        </div>
    );
};

export default GroupSidePanel;
