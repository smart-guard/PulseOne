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

    useEffect(() => {
        loadGroups();
    }, []);

    const loadGroups = async () => {
        try {
            setFetchingGroups(true);
            const response = await GroupApiService.getGroupTree();
            if (response.success && response.data) {
                setGroups(response.data);
                // Expand first level by default
                const firstLevelIds = response.data.map(g => g.id);
                setExpandedGroups(new Set(firstLevelIds));
            }
        } catch (error) {
            console.error('Failed to load groups:', error);
        } finally {
            setFetchingGroups(false);
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
                        <h3>선택된 디바이스 ({selectedDevices.length}개)</h3>
                        <div className="selected-items-summary">
                            {selectedDevices.slice(0, 5).map(device => (
                                <div key={device.id} className="summary-item">
                                    <i className="fas fa-microchip"></i>
                                    <span>{device.name}</span>
                                </div>
                            ))}
                            {selectedDevices.length > 5 && (
                                <div className="summary-item more">외 {selectedDevices.length - 5}개...</div>
                            )}
                        </div>
                    </div>

                    <div className="form-section">
                        <h3>대상 그룹 선택</h3>
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
                                    {groups.map(group => renderGroupItem(group))}
                                </div>
                            )}
                        </div>
                    </div>
                </div>

                <div className="modal-footer">
                    <button className="btn btn-outline" onClick={onClose} disabled={loading}>
                        취ose
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
                    .selected-items-summary {
                        background: #f8fafc;
                        border-radius: 8px;
                        padding: 12px;
                        margin-top: 8px;
                        display: flex;
                        flex-wrap: wrap;
                        gap: 8px;
                    }
                    .summary-item {
                        background: white;
                        border: 1px solid #e2e8f0;
                        padding: 4px 10px;
                        border-radius: 6px;
                        font-size: 12px;
                        display: flex;
                        align-items: center;
                        gap: 6px;
                    }
                    .group-selection-tree {
                        border: 1px solid #e2e8f0;
                        border-radius: 8px;
                        height: 300px;
                        overflow-y: auto;
                        margin-top: 8px;
                        padding: 8px;
                    }
                    .select-item {
                        cursor: pointer;
                        border-radius: 4px;
                        margin-bottom: 2px;
                    }
                    .select-item:hover {
                        background: #f1f5f9;
                    }
                    .select-item.active {
                        background: #e0f2fe;
                        color: #0369a1;
                        font-weight: 600;
                    }
                    .select-item.active i {
                        color: #0ea5e9;
                    }
                `}</style>
            </div>
        </div>
    );
};

export default MoveGroupModal;
