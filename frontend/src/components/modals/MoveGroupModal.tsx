import React, { useState, useEffect } from 'react';
import { GroupApiService, DeviceGroup } from '../../api/services/groupApi';
import { Device } from '../../api/services/deviceApi';
import '../../styles/management.css';

interface MoveGroupModalProps {
    selectedDevices: Device[];
    onClose: () => void;
    onConfirm: (targetGroupId: number | null) => void;
    loading: boolean;
}

const MoveGroupModal: React.FC<MoveGroupModalProps> = ({
    selectedDevices,
    onClose,
    onConfirm,
    loading
}) => {
    const [groups, setGroups] = useState<DeviceGroup[]>([]);
    const [fetchingGroups, setFetchingGroups] = useState(true);
    const [selectedGroupId, setSelectedGroupId] = useState<number | null>(null);
    const [expandedGroups, setExpandedGroups] = useState<Set<number>>(new Set());
    const [searchQuery, setSearchQuery] = useState('');
    const [isCreatingGroup, setIsCreatingGroup] = useState(false);
    const [newGroupName, setNewGroupName] = useState('');
    const [creating, setCreating] = useState(false);

    useEffect(() => {
        loadGroups();
    }, []);

    const loadGroups = async () => {
        try {
            setFetchingGroups(true);
            const response = await GroupApiService.getGroupTree();
            if (response.success && response.data) {
                setGroups(response.data);
                if (expandedGroups.size === 0) {
                    const firstLevelIds = response.data.map(g => g.id);
                    setExpandedGroups(new Set(firstLevelIds));
                }
            }
        } catch (error) {
            console.error('Failed to load groups:', error);
        } finally {
            setFetchingGroups(false);
        }
    };

    const handleCreateGroup = async () => {
        if (!newGroupName.trim()) return;
        try {
            setCreating(true);
            const response = await GroupApiService.createGroup({
                name: newGroupName.trim(),
                parent_group_id: selectedGroupId // 💡 선택된 그룹의 하위로 생성
            });

            if (response.success) {
                setNewGroupName('');
                setIsCreatingGroup(false);
                await loadGroups(); // 목록 갱신
                if (response.data) {
                    setSelectedGroupId(response.data.id); // 신규 그룹 자동 선택
                }
            }
        } catch (error) {
            console.error('Failed to create group:', error);
            alert('그룹 생성에 실패했습니다.');
        } finally {
            setCreating(false);
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

    const renderGroupItem = (group: DeviceGroup, level: number = 0) => {
        const isExpanded = expandedGroups.has(group.id);
        const isSelected = selectedGroupId === group.id;
        const hasChildren = group.children && group.children.length > 0;

        return (
            <div key={group.id} className="group-tree-item-container">
                <div
                    className={`group-tree-item select-item ${isSelected ? 'active' : ''}`}
                    style={{ paddingLeft: `${level * 16 + 8}px` }}
                    onClick={() => setSelectedGroupId(group.id)}
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
                </div>

                {hasChildren && isExpanded && (
                    <div className="group-children">
                        {group.children?.map(child => renderGroupItem(child, level + 1))}
                    </div>
                )}
            </div>
        );
    };

    return (
        <div className="modal-overlay">
            <div className="modal-container" style={{ maxWidth: '500px' }}>
                <div className="modal-header">
                    <h2 className="modal-title">그룹 이동</h2>
                    <button className="modal-close-btn" onClick={onClose} disabled={loading}>
                        <i className="fas fa-times"></i>
                    </button>
                </div>

                <div className="modal-content">
                    <div className="form-section">
                        <div className="section-header">
                            <h3>선택된 디바이스 ({selectedDevices.length}개)</h3>
                        </div>
                        <div className="selected-items-summary">
                            {selectedDevices.slice(0, 10).map(device => (
                                <div key={device.id} className="summary-item">
                                    <i className="fas fa-microchip"></i>
                                    <span>{device.name}</span>
                                </div>
                            ))}
                            {selectedDevices.length > 10 && (
                                <div className="summary-item more">외 {selectedDevices.length - 10}개...</div>
                            )}
                        </div>
                    </div>

                    <div className="form-section flex-grow">
                        <div className="section-header">
                            <h3>대상 그룹 선택</h3>
                            <div className="search-box-sm">
                                <i className="fas fa-search"></i>
                                <input
                                    type="text"
                                    placeholder="그룹 검색..."
                                    value={searchQuery}
                                    onChange={(e) => setSearchQuery(e.target.value)}
                                />
                                {searchQuery && <i className="fas fa-times clear-btn" onClick={() => setSearchQuery('')}></i>}
                            </div>
                        </div>
                        <div className="group-selection-tree">
                            <div
                                className={`group-tree-item select-item ${selectedGroupId === null ? 'active' : ''}`}
                                onClick={() => setSelectedGroupId(null)}
                            >
                                <i className="fas fa-ban group-icon"></i>
                                <span className="group-name">그룹 해제 (그룹 없음)</span>
                            </div>

                            {fetchingGroups ? (
                                <div className="tree-loading">
                                    <i className="fas fa-spinner fa-spin"></i> 로딩 중...
                                </div>
                            ) : (
                                <div className="tree-list">
                                    {groups.length === 0 && !searchQuery ? (
                                        <div className="empty-tree-state">
                                            <i className="fas fa-folder-open"></i>
                                            <p>등록된 그룹이 없습니다.</p>
                                            <button className="btn btn-sm btn-outline" onClick={() => setIsCreatingGroup(true)}>
                                                첫 그룹 생성하기
                                            </button>
                                        </div>
                                    ) : (
                                        <>
                                            {groups.filter(g => {
                                                if (!searchQuery) return true;
                                                const matches = (group: DeviceGroup): boolean => {
                                                    if (group.name.toLowerCase().includes(searchQuery.toLowerCase())) return true;
                                                    return !!group.children?.some(child => matches(child));
                                                };
                                                return matches(g);
                                            }).map(group => renderGroupItem(group))}
                                            {searchQuery && groups.filter(g => {
                                                const matches = (group: DeviceGroup): boolean => {
                                                    if (group.name.toLowerCase().includes(searchQuery.toLowerCase())) return true;
                                                    return !!group.children?.some(child => matches(child));
                                                };
                                                return matches(g);
                                            }).length === 0 && (
                                                    <div className="no-search-results">검색 결과가 없습니다.</div>
                                                )}
                                        </>
                                    )}
                                </div>
                            )}
                        </div>

                        <div className="new-group-action">
                            {isCreatingGroup ? (
                                <div className="inline-create-form active">
                                    <div className="input-with-label">
                                        <input
                                            type="text"
                                            placeholder="새 그룹 이름..."
                                            autoFocus
                                            value={newGroupName}
                                            onChange={(e) => setNewGroupName(e.target.value)}
                                            onKeyDown={(e) => {
                                                if (e.key === 'Enter') handleCreateGroup();
                                                if (e.key === 'Escape') setIsCreatingGroup(false);
                                            }}
                                        />
                                        <span className="parent-hint">{selectedGroupId ? '선택한 그룹의 하위로 생성' : '최상위 그룹으로 생성'}</span>
                                    </div>
                                    <div className="form-btns">
                                        <button className="btn-icon-check" onClick={handleCreateGroup} disabled={creating || !newGroupName.trim()}>
                                            {creating ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-check"></i>}
                                        </button>
                                        <button className="btn-icon-cancel" onClick={() => setIsCreatingGroup(false)} disabled={creating}>
                                            <i className="fas fa-times"></i>
                                        </button>
                                    </div>
                                </div>
                            ) : (
                                <button className="add-group-btn" onClick={() => setIsCreatingGroup(true)}>
                                    <i className="fas fa-plus-circle"></i> 신규 그룹 추가
                                </button>
                            )}
                        </div>
                    </div>
                </div>

                <div className="modal-footer">
                    <button className="btn btn-outline" onClick={onClose} disabled={loading}>
                        취소
                    </button>
                    <button
                        className="btn btn-primary"
                        onClick={() => onConfirm(selectedGroupId)}
                        disabled={loading}
                    >
                        {loading ? <i className="fas fa-spinner fa-spin"></i> : <i className="fas fa-exchange-alt"></i>}
                        이동하기
                    </button>
                </div>

                <style>{`
                    .modal-container { display: flex; flex-direction: column; overflow: hidden; max-height: 90vh; }
                    .form-section { margin-bottom: 20px; }
                    .form-section.flex-grow { flex: 1; display: flex; flex-direction: column; min-height: 0; }
                    .section-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
                    .section-header h3 { margin: 0; font-size: 14px; font-weight: 600; color: #475569; }

                    .search-box-sm { display: flex; align-items: center; background: #f1f5f9; border-radius: 6px; padding: 4px 10px; gap: 8px; width: 180px; border: 1px solid transparent; transition: all 0.2s; }
                    .search-box-sm:focus-within { background: white; border-color: #3b82f6; box-shadow: 0 0 0 2px rgba(59, 130, 246, 0.1); }
                    .search-box-sm i { color: #94a3b8; font-size: 12px; }
                    .search-box-sm input { border: none; background: transparent; outline: none; font-size: 12px; width: 100%; color: #1e293b; }
                    .clear-btn { cursor: pointer; }
                    .clear-btn:hover { color: #64748b; }

                    .selected-items-summary {
                        background: #f8fafc;
                        border-radius: 10px;
                        padding: 10px;
                        display: flex;
                        flex-wrap: wrap;
                        gap: 6px;
                        border: 1px dashed #e2e8f0;
                    }
                    .summary-item {
                        background: white;
                        border: 1px solid #e2e8f0;
                        padding: 4px 10px;
                        border-radius: 6px;
                        font-size: 12px;
                        font-weight: 500;
                        color: #475569;
                        display: flex;
                        align-items: center;
                        gap: 6px;
                        box-shadow: 0 1px 2px rgba(0,0,0,0.03);
                    }
                    .summary-item i { color: #3b82f6; font-size: 10px; }
                    .summary-item.more { background: #f1f5f9; border-color: transparent; color: #64748b; font-weight: 600; }

                    .group-selection-tree {
                        border: 1px solid #e2e8f0;
                        border-radius: 10px;
                        flex: 1;
                        min-height: 240px;
                        overflow-y: auto;
                        padding: 6px;
                        background: white;
                    }
                    .select-item {
                        cursor: pointer;
                        border-radius: 6px;
                        margin-bottom: 2px;
                        display: flex;
                        align-items: center;
                        padding: 8px 10px;
                        transition: all 0.2s;
                        font-size: 13px;
                        color: #334155;
                    }
                    .select-item:hover {
                        background: #f8fafc;
                        color: #1e293b;
                    }
                    .select-item.active {
                        background: #eff6ff;
                        color: #1d4ed8;
                        font-weight: 600;
                        border: 1px solid #dbeafe;
                    }
                    .group-icon { margin-right: 10px; width: 16px; text-align: center; font-size: 14px; opacity: 0.7; }
                    .select-item.active .group-icon { color: #3b82f6; opacity: 1; }
                    
                    .expand-icon { width: 16px; height: 16px; display: inline-flex; align-items: center; justify-content: center; margin-right: 4px; cursor: pointer; color: #94a3b8; transition: color 0.2s; }
                    .expand-icon:hover { color: #64748b; }
                    
                    .tree-loading { padding: 40px; text-align: center; color: #94a3b8; font-size: 13px; }
                    .tree-list { display: flex; flex-direction: column; }
                    .empty-tree-state { padding: 40px 20px; text-align: center; color: #94a3b8; }
                    .empty-tree-state i { font-size: 32px; margin-bottom: 12px; opacity: 0.3; }
                    .empty-tree-state p { margin-bottom: 16px; font-size: 13px; }
                    .no-search-results { padding: 20px; text-align: center; color: #94a3b8; font-size: 13px; }

                    .new-group-action { margin-top: 12px; border-top: 1px solid #f1f5f9; padding-top: 12px; }
                    .add-group-btn { background: none; border: 1px dashed #cbd5e1; color: #64748b; width: 100%; padding: 8px; border-radius: 8px; cursor: pointer; font-size: 13px; font-weight: 500; display: flex; align-items: center; justify-content: center; gap: 8px; transition: all 0.2s; }
                    .add-group-btn:hover { background: #f8fafc; border-color: #94a3b8; color: #1e293b; }

                    .inline-create-form { display: flex; align-items: center; gap: 8px; background: white; border: 1px solid #3b82f6; border-radius: 8px; padding: 4px 8px; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1); }
                    .inline-create-form .input-with-label { flex: 1; display: flex; flex-direction: column; }
                    .inline-create-form input { border: none; outline: none; padding: 6px 0; font-size: 13px; width: 100%; }
                    .parent-hint { font-size: 10px; color: #3b82f6; margin-top: -2px; font-weight: 500; }
                    .form-btns { display: flex; gap: 4px; }
                    .btn-icon-check, .btn-icon-cancel { border: none; background: none; width: 28px; height: 28px; border-radius: 6px; display: flex; align-items: center; justify-content: center; cursor: pointer; transition: all 0.2s; }
                    .btn-icon-check { color: #3b82f6; }
                    .btn-icon-check:hover:not(:disabled) { background: #eff6ff; }
                    .btn-icon-check:disabled { opacity: 0.3; cursor: not-allowed; }
                    .btn-icon-cancel { color: #ef4444; }
                    .btn-icon-cancel:hover { background: #fef2f2; }
                `}</style>
            </div>
        </div>
    );
};

export default MoveGroupModal;
