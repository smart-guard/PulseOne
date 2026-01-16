import React, { useMemo } from 'react';

export interface TreeNode {
    id: string;
    label: string;
    type: 'tenant' | 'site' | 'collector' | 'master' | 'slave' | 'device' | 'datapoint' | 'unknown_collector';
    children?: TreeNode[];
    child_count?: number;
    is_expanded?: boolean;
    is_enabled?: boolean;
    value?: any;
    quality?: string;
    unit?: string;
    connection_status?: string;
    hasRedisData?: boolean;
    device_info?: {
        device_id: number;
        name: string;
        protocol_type: string;
    };
}

interface DeviceTreeProps {
    treeData: TreeNode[];
    expandedNodes: Set<string>;
    selectedNode: TreeNode | null;
    onNodeClick: (node: TreeNode) => void;
    isLoading: boolean;
    searchTerm: string;
}

const DeviceTree: React.FC<DeviceTreeProps> = ({
    treeData,
    expandedNodes,
    selectedNode,
    onNodeClick,
    isLoading,
    searchTerm,
}) => {
    // 트리를 평탄화하여 가상화 및 렌더링 최적화 준비
    const flattenedNodes = useMemo(() => {
        const nodes: Array<{ node: TreeNode; level: number }> = [];

        const flatten = (nodeList: TreeNode[], level: number) => {
            nodeList.forEach(node => {
                // 검색어 필터링 (간단한 버전)
                const matchesSearch = !searchTerm || node.label.toLowerCase().includes(searchTerm.toLowerCase());

                // 검색어가 있으면 모든 노드를 보여줌, 아니면 트리 구조에 따라
                if (!searchTerm || matchesSearch) {
                    nodes.push({ node, level });
                }

                if (expandedNodes.has(node.id) && node.children) {
                    flatten(node.children, level + 1);
                }
            });
        };

        flatten(treeData, 0);
        return nodes;
    }, [treeData, expandedNodes, searchTerm]);

    if (isLoading && treeData.length === 0) {
        return (
            <div className="loading-container">
                <div className="loading-spinner"></div>
                <div className="loading-text">RTU 네트워크 구조 로드 중...</div>
            </div>
        );
    }

    if (treeData.length === 0) {
        return (
            <div className="empty-state">
                <div style={{ fontSize: '48px', marginBottom: '16px' }}>📊</div>
                <h3 style={{ margin: '0 0 8px 0', fontSize: '16px' }}>데이터가 없습니다</h3>
                <p style={{ margin: 0, fontSize: '14px', textAlign: 'center' }}>
                    API 연결을 확인하고 새로고침해보세요
                </p>
            </div>
        );
    }

    return (
        <div className="tree-content" style={{ overflowY: 'auto', height: '100%' }}>
            {flattenedNodes.map(({ node, level }) => {
                const hasChildren = (node.children && node.children.length > 0) || (node.child_count && node.child_count > 0);
                const isExpanded = expandedNodes.has(node.id);

                return (
                    <div
                        key={node.id}
                        className="tree-node"
                        style={{ paddingLeft: `${level * 16}px` }}
                    >
                        <div
                            className={`tree-node-content ${selectedNode?.id === node.id ? 'selected' : ''}`}
                            onClick={() => onNodeClick(node)}
                        >
                            {hasChildren && (
                                <span className="tree-expand-icon">
                                    {isExpanded ? '▼' : '▶'}
                                </span>
                            )}
                            {!hasChildren && <span className="tree-expand-icon-spacer" style={{ width: '16px', display: 'inline-block' }}></span>}
                            <span className="tree-node-icon">
                                {node.type === 'tenant' && '🏢'}
                                {node.type === 'site' && '🏭'}
                                {(node.type === 'master' || node.id.startsWith('dev-')) && '🔌'}
                                {node.type === 'device' && '📊'}
                                {node.type === 'datapoint' && '📈'}
                            </span>
                            <span className="tree-node-label">
                                {node.label}
                            </span>
                            {node.type === 'datapoint' && (
                                <div className="data-point-preview">
                                    <span className={`data-value ${node.quality || 'unknown'}`}>
                                        {node.value}
                                        {node.unit && ` ${node.unit}`}
                                    </span>
                                </div>
                            )}
                            {(node.type === 'master' || node.id.startsWith('dev-')) && (
                                <span className={`connection-badge ${!node.hasRedisData ? 'none' : node.connection_status === 'connected' ? 'connected' : 'disconnected'}`}>
                                    {!node.hasRedisData ? '⚪' : node.connection_status === 'connected' ? '🟢' : '🔴'}
                                </span>
                            )}
                        </div>
                    </div>
                );
            })}
        </div>
    );
};

export default DeviceTree;
