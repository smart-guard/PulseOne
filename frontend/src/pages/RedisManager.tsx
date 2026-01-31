import React, { useState, useEffect } from 'react';
import { redisApi, RedisKeyInfo, RedisKeyDetails, RedisConnectionConfig } from '../api/redisApi';

// ============================================================================
// STYLES
// ============================================================================
const styles = {
    container: {
        padding: '0',
        backgroundColor: '#f7fafc',
        height: 'calc(100vh - 64px)', // Adjust based on topbar height
        display: 'flex',
        flexDirection: 'column',
        fontFamily: '"Inter", system-ui, sans-serif',
    } as React.CSSProperties,
    header: {
        padding: '16px 24px',
        backgroundColor: 'white',
        borderBottom: '1px solid #e2e8f0',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        height: '64px',
        flexShrink: 0,
    } as React.CSSProperties,
    title: {
        fontSize: '1.25rem',
        fontWeight: 700,
        color: '#1a202c',
        margin: 0,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
    } as React.CSSProperties,
    logoIcon: {
        color: '#e53e3e',
        fontSize: '1.5rem'
    } as React.CSSProperties,
    mainArea: {
        display: 'flex',
        flex: 1,
        overflow: 'hidden',
    } as React.CSSProperties,
    // LEFT SIDEBAR (TREE)
    sidebar: {
        width: '320px',
        backgroundColor: 'white',
        borderRight: '1px solid #e2e8f0',
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
    } as React.CSSProperties,
    searchBox: {
        padding: '16px',
        borderBottom: '1px solid #edf2f7',
    } as React.CSSProperties,
    searchInput: {
        width: '100%',
        padding: '8px 12px',
        border: '1px solid #e2e8f0',
        borderRadius: '6px',
        fontSize: '0.9rem',
    } as React.CSSProperties,
    treeContainer: {
        flex: 1,
        overflowY: 'auto',
        padding: '8px 0',
    } as React.CSSProperties,
    treeNode: {
        padding: '6px 16px',
        cursor: 'pointer',
        display: 'flex',
        alignItems: 'center',
        fontSize: '0.9rem',
        color: '#4a5568',
        whiteSpace: 'nowrap',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        userSelect: 'none',
    } as React.CSSProperties,
    treeNodeSelected: {
        backgroundColor: '#ebf8ff',
        color: '#3182ce',
        fontWeight: 500,
    } as React.CSSProperties,
    treeIcon: {
        marginRight: '8px',
        width: '16px',
        textAlign: 'center',
        color: '#a0aec0',
    } as React.CSSProperties,
    // RIGHT CONTENT (INSPECTOR)
    contentArea: {
        flex: 1,
        padding: '24px',
        overflowY: 'auto',
        backgroundColor: '#f7fafc',
    } as React.CSSProperties,
    emptyState: {
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        color: '#a0aec0',
        flexDirection: 'column',
        gap: '16px',
    } as React.CSSProperties,
    inspectorCard: {
        background: 'white',
        borderRadius: '12px',
        boxShadow: '0 2px 4px rgba(0,0,0,0.05)',
        padding: '32px',
        width: '100%',     // Occupy full available width
        height: '100%',    // Take full height for better editing flow
        display: 'flex',   // Flex for internal layout
        flexDirection: 'column',
    } as React.CSSProperties,
    field: {
        marginBottom: '24px',
        flex: 1,           // Allow textarea to expand
        display: 'flex',
        flexDirection: 'column',
    } as React.CSSProperties,
    label: {
        display: 'block',
        marginBottom: '8px',
        fontWeight: 600,
        color: '#718096',
        fontSize: '0.85rem',
        textTransform: 'uppercase',
        letterSpacing: '0.05em',
    } as React.CSSProperties,
    valueInput: {
        width: '100%',
        flex: 1,           // Expand to fill available vertical space
        padding: '16px',
        border: '1px solid #e2e8f0',
        borderRadius: '8px',
        fontFamily: "'Menlo', 'Monaco', 'Courier New', monospace",
        fontSize: '0.9rem',
        resize: 'none',    // Disable manual resize as it fills space
        background: '#f8fafc',
        lineHeight: '1.5',
        color: '#2d3748',
    } as React.CSSProperties,
    actionBar: {
        display: 'flex',
        justifyContent: 'flex-end',
        gap: '12px',
        marginTop: '32px',
        borderTop: '1px solid #edf2f7',
        paddingTop: '24px',
    } as React.CSSProperties,
    primaryButton: {
        background: '#3182ce',
        color: 'white',
        border: 'none',
        borderRadius: '6px',
        padding: '8px 16px',
        fontWeight: 600,
        cursor: 'pointer',
        fontSize: '0.9rem',
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
    } as React.CSSProperties,
    dangerButton: {
        background: 'white',
        color: '#e53e3e',
        border: '1px solid #fed7d7',
        borderRadius: '6px',
        padding: '8px 16px',
        fontWeight: 600,
        cursor: 'pointer',
        fontSize: '0.9rem',
    } as React.CSSProperties,
    secondaryButton: {
        background: '#edf2f7',
        color: '#4a5568',
        border: 'none',
        borderRadius: '6px',
        padding: '8px 16px',
        fontWeight: 600,
        cursor: 'pointer',
        fontSize: '0.9rem',
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
    } as React.CSSProperties,
    // MODAL
    modalOverlay: {
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 1000,
    } as React.CSSProperties,
    modalContent: {
        backgroundColor: 'white',
        padding: '32px',
        borderRadius: '12px',
        width: '400px',
        boxShadow: '0 4px 6px rgba(0, 0, 0, 0.1)',
    } as React.CSSProperties,
    inputGroup: {
        marginBottom: '16px',
    } as React.CSSProperties,
    input: {
        width: '100%',
        padding: '10px',
        border: '1px solid #e2e8f0',
        borderRadius: '6px',
        marginTop: '4px',
        fontSize: '0.95rem',
    } as React.CSSProperties,
};

// ============================================================================
// LOGIC HELPERS
// ============================================================================

interface TreeNode {
    name: string;
    fullPath: string; // "device:1:status"
    children: Record<string, TreeNode>;
    isLeaf: boolean;
    type?: string;
    isOpen: boolean; // UI state
}

const buildTree = (keys: RedisKeyInfo[]): Record<string, TreeNode> => {
    const root: Record<string, TreeNode> = {};

    keys.forEach(k => {
        const parts = k.key.split(':');
        let currentLevel = root;

        parts.forEach((part, index) => {
            if (!currentLevel[part]) {
                currentLevel[part] = {
                    name: part,
                    fullPath: parts.slice(0, index + 1).join(':'),
                    children: {},
                    isLeaf: false,
                    isOpen: false,
                };
            }

            // If it's the last part, mark as leaf and assign type
            if (index === parts.length - 1) {
                currentLevel[part].isLeaf = true;
                currentLevel[part].type = k.type;
            }

            currentLevel = currentLevel[part].children;
        });
    });

    return root;
};

// ============================================================================
// COMPONENT
// ============================================================================

const RedisManager: React.FC = () => {
    // Data State
    const [keys, setKeys] = useState<RedisKeyInfo[]>([]);
    const [treeRoots, setTreeRoots] = useState<Record<string, TreeNode>>({});
    const [loading, setLoading] = useState(false);
    const [selectedKey, setSelectedKey] = useState<string | null>(null);
    const [keyDetails, setKeyDetails] = useState<RedisKeyDetails | null>(null);
    const [editValue, setEditValue] = useState('');
    const [searchTerm, setSearchTerm] = useState('*');
    const [serverInfo, setServerInfo] = useState<any>(null);

    // Connection State
    const [connectionConfig, setConnectionConfig] = useState<RedisConnectionConfig | undefined>(undefined);
    const [showConfigModal, setShowConfigModal] = useState(false);
    const [viewMode, setViewMode] = useState<'raw' | 'structured'>('structured');
    const [isEditing, setIsEditing] = useState(false);

    const handleFieldChange = (newValue: string, fieldKey: string, itemIndex?: number) => {
        try {
            let currentData: any;
            try { currentData = JSON.parse(editValue); } catch (e) { return; }

            // Helper to cast value
            const castValue = (val: string, originalVal: any) => {
                const type = typeof originalVal;
                if (type === 'number') {
                    const num = Number(val);
                    return isNaN(num) ? val : num;
                }
                if (type === 'boolean') {
                    if (val === 'true') return true;
                    if (val === 'false') return false;
                    return Boolean(val);
                }
                return val;
            };

            if (Array.isArray(currentData) && typeof itemIndex === 'number') {
                const newData = [...currentData];
                const originalVal = newData[itemIndex][fieldKey];
                newData[itemIndex] = { ...newData[itemIndex], [fieldKey]: castValue(newValue, originalVal) };
                setEditValue(JSON.stringify(newData, null, 2));
            } else if (typeof currentData === 'object' && currentData !== null) {
                const originalVal = currentData[fieldKey];
                const newData = { ...currentData, [fieldKey]: castValue(newValue, originalVal) };
                setEditValue(JSON.stringify(newData, null, 2));
            }
        } catch (e) {
            console.error("Failed to update field", e);
        }
    };

    // Helper to render a single JSON object as a card
    const renderJsonCard = (obj: any, index?: number, isArrayItem = false) => {
        // Primitive Render
        if (typeof obj !== 'object' || obj === null) {
            if (isEditing && isArrayItem) {
                // Allow editing primitive array items directly?
                // For now, let's focus on Object editing as requested for "alarm:history" style data
                return (
                    <div key={index} style={{ padding: '12px', borderBottom: '1px solid #edf2f7' }}>
                        <input
                            style={styles.input}
                            value={String(obj)}
                            onChange={(e) => {
                                // Simple array update logic for primitives if needed
                                // Skipping for now to keep it safe, or handle via handleStructuredItemChange wrapper?
                                // Let's just render static for primitives in array for now to avoid complexity unless asked.
                            }}
                        />
                    </div>
                );
            }
            return <div key={index} style={{ padding: '12px', borderBottom: '1px solid #edf2f7' }}>{String(obj)}</div>;
        }

        // Object Render
        return (
            <div key={index} style={{
                marginBottom: '16px',
                padding: '16px',
                background: isEditing ? '#fff' : '#fafafa',
                border: isEditing ? '1px solid #3182ce' : '1px solid #e2e8f0',
                borderRadius: '8px',
                fontSize: '0.9rem',
                boxShadow: isEditing ? '0 0 0 2px rgba(49, 130, 206, 0.1)' : 'none'
            }}>
                {Object.entries(obj).map(([k, v]) => (
                    <div key={k} style={{ display: 'flex', alignItems: 'center', marginBottom: '8px', borderBottom: '1px dashed #edf2f7', paddingBottom: '8px' }}>
                        <span style={{ width: '150px', fontWeight: 600, color: '#4a5568', flexShrink: 0 }}>{k}</span>

                        {isEditing ? (
                            typeof v === 'object' ? (
                                <span style={{ color: '#a0aec0', fontSize: '0.8rem' }}>(Complex object editing not supported in line)</span>
                            ) : (
                                <input
                                    style={{
                                        ...styles.input,
                                        padding: '4px 8px',
                                        margin: 0,
                                        fontSize: '0.9rem',
                                        width: '100%',
                                        border: '1px solid #cbd5e0'
                                    }}
                                    value={String(v)}
                                    onChange={(e) => handleFieldChange(e.target.value, k, index)}
                                />
                            )
                        ) : (
                            <span style={{ color: '#2d3748', wordBreak: 'break-all' }}>{typeof v === 'object' ? JSON.stringify(v) : String(v)}</span>
                        )}
                    </div>
                ))}
            </div>
        );
    };

    const handleStructuredItemChange = (newValueStr: string, index?: number) => {
        try {
            // 1. Current Full Data
            let currentData: any;
            try { currentData = JSON.parse(editValue); } catch (e) { return; } // Should not happen if rendering structured

            // 2. Parse New Item Value (if possible, else keep string)
            let newItemVal: any = newValueStr;
            try { newItemVal = JSON.parse(newValueStr); } catch (e) { }

            // 3. Update Data Structure
            if (Array.isArray(currentData) && typeof index === 'number') {
                const newData = [...currentData];
                newData[index] = newItemVal; // Update specific item
                setEditValue(JSON.stringify(newData, null, 2));
            } else if (typeof currentData === 'object' && currentData !== null) {
                // For a single object, if "index" is undefined, we are editing the whole object??
                // Actually renderJsonCard is called with (data) for Object type. index is undefined.
                // But wait, renderJsonCard for Object iterates entries. 
                // If we are editing a single JSON object (String type containing JSON), 
                // then we are just replacing the whole thing.
                // Let's rely on JSON.parse(newValueStr) being the new object.
                setEditValue(newValueStr);
            }
        } catch (e) {
            console.error("Failed to update structured data", e);
        }
    };

    const renderStructuredView = () => {
        try {
            let data: any;
            try {
                data = JSON.parse(editValue);
            } catch (e) {
                return <div style={{ padding: '20px', color: '#718096' }}>Data is not valid JSON. Switch to Raw view to edit.</div>;
            }

            if (Array.isArray(data)) {
                return (
                    <div style={{ flex: 1, overflowY: 'auto', background: '#fff', border: '1px solid #e2e8f0', borderRadius: '8px', padding: '16px' }}>
                        <div style={{ marginBottom: '12px', fontWeight: 600, color: '#718096', display: 'flex', justifyContent: 'space-between' }}>
                            <span>{data.length} Items</span>
                            {isEditing && <span style={{ fontSize: '0.8rem', color: '#3182ce' }}>Editing Enabled</span>}
                        </div>
                        {data.map((item, idx) => renderJsonCard(item, idx, true))}
                    </div>
                );
            } else if (typeof data === 'object' && data !== null) {
                return (
                    <div style={{ flex: 1, overflowY: 'auto', background: '#fff', border: '1px solid #e2e8f0', borderRadius: '8px', padding: '16px' }}>
                        {renderJsonCard(data)}
                    </div>
                );
            } else {
                if (isEditing) {
                    return (
                        <div style={{ padding: '20px' }}>
                            <label style={{ display: 'block', marginBottom: '8px', color: '#718096', fontWeight: 600 }}>Simple Value</label>
                            <input
                                style={{ ...styles.input, width: '100%', padding: '12px' }}
                                value={String(data)}
                                onChange={(e) => {
                                    // Treat as replacing the whole value
                                    setEditValue(JSON.stringify(e.target.value));
                                    // Note: If user wants to enter a number or boolean, they might be quoted.
                                    // But for simple value root editing, treating as string is safest fallback.
                                    // Unless we detect type. But JSON.stringify(string) -> "string"
                                }}
                            />
                            <div style={{ marginTop: '8px', fontSize: '0.8rem', color: '#a0aec0' }}>
                                Use Raw view to edit non-string root values accurately if needed.
                            </div>
                        </div>
                    );
                }
                return <div style={{ padding: '20px', color: '#718096' }}>Simple Value: {String(data)}</div>;
            }
        } catch (error) {
            return <div style={{ padding: '20px', color: '#e53e3e' }}>Error rendering structured view.</div>;
        }
    };

    // Config Form State
    const [tempConfig, setTempConfig] = useState<RedisConnectionConfig>({
        host: 'localhost',
        port: 6379,
        password: '',
        db: 0
    });

    // Initial Data Fetch
    useEffect(() => {
        fetchKeys();
        fetchServerInfo();
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [connectionConfig]); // Refetch when config changes

    const fetchServerInfo = async () => {
        try {
            const data = await redisApi.getInfo(connectionConfig);
            setServerInfo({ ...data.info, connection: data.connection });
        } catch (e) {
            console.error(e);
            setServerInfo(null);
        }
    };

    const fetchKeys = async () => {
        setLoading(true);
        try {
            // Fetch ALL keys for inspector (limit 2000 for safety)
            const result = await redisApi.scanKeys('0', searchTerm, 2000, connectionConfig);
            setKeys(result.keys);
            setTreeRoots(buildTree(result.keys));
        } catch (error) {
            console.error('Failed to scan keys', error);
            setKeys([]);
            setTreeRoots({});
        } finally {
            setLoading(false);
        }
    };

    const handleNodeClick = (node: TreeNode) => {
        if (node.isLeaf) {
            handleInspect(node.fullPath);
        } else {
            const newRoots = { ...treeRoots };
            toggleNode(newRoots, node.fullPath);
            setTreeRoots(newRoots);
        }
    };

    const toggleNode = (nodes: Record<string, TreeNode>, targetPath: string) => {
        for (const key in nodes) {
            const node = nodes[key];
            if (node.fullPath === targetPath) {
                node.isOpen = !node.isOpen;
                return true;
            }
            if (toggleNode(node.children, targetPath)) return true;
        }
        return false;
    };

    const handleInspect = async (key: string) => {
        try {
            setSelectedKey(key);
            setIsEditing(false); // Reset edit mode on selection change
            const details = await redisApi.getKey(key, connectionConfig);
            setKeyDetails(details);

            let valDisplay = details.value;

            // Fix: Parse if it's a JSON string so we don't double-encode it
            if (details.isJson && typeof details.value === 'string') {
                try {
                    valDisplay = JSON.parse(details.value);
                } catch (e) {
                    // ignore parse error, keep as string
                }
            }

            if (details.isJson || typeof valDisplay === 'object') {
                valDisplay = JSON.stringify(valDisplay, null, 2);
                setViewMode('structured');
            } else {
                setViewMode('raw');
            }
            setEditValue(valDisplay);
        } catch (error) {
            console.error(error);
            alert('Failed to load key details');
        }
    };

    const handleSave = async () => {
        if (!selectedKey || !keyDetails) return;
        try {
            let payload = editValue;
            // Only try to parse if in raw mode, or if it was originally JSON and we're in structured mode
            // If in structured mode, editValue is already the formatted JSON string.
            // If it was originally JSON, and we're in raw mode, it might have been edited.
            // The existing try-catch for JSON.parse is robust enough.
            try { payload = JSON.parse(editValue); } catch (e) { }

            await redisApi.setValue(selectedKey, payload, keyDetails.type, keyDetails.ttl > 0 ? keyDetails.ttl : undefined, connectionConfig);
            alert('Saved!');
            handleInspect(selectedKey); // Refresh value
        } catch (e) {
            alert('Failed to save');
        }
    };

    const handleDelete = async () => {
        if (!selectedKey || !window.confirm(`Delete ${selectedKey}?`)) return;
        await redisApi.deleteKey(selectedKey, connectionConfig);
        setSelectedKey(null);
        setKeyDetails(null);
        fetchKeys(); // Refresh tree
    };

    const handleSearchKey = (e: React.KeyboardEvent) => {
        if (e.key === 'Enter') fetchKeys();
    };

    const handleConnect = () => {
        setConnectionConfig(tempConfig);
        setShowConfigModal(false);
        setSelectedKey(null);
        setKeyDetails(null);
    };

    const handleResetConnection = () => {
        setConnectionConfig(undefined);
        setTempConfig({ host: 'localhost', port: 6379, password: '', db: 0 });
        setShowConfigModal(false);
        setSelectedKey(null);
        setKeyDetails(null);
    };

    const toggleEditMode = () => {
        if (isEditing) {
            // Cancel -> Revert? For now, assume user might want to cancel changes manually or just save.
            // Actually, usually Cancel reverts. But we are editing `editValue` directly.
            // Simple toggle for now. 
            setIsEditing(false);
        } else {
            setIsEditing(true);
        }
    };

    // Recursive Tree Renderer
    const renderTree = (nodes: Record<string, TreeNode>, level = 0) => {
        return Object.values(nodes).sort((a, b) => {
            const aIsFolder = Object.keys(a.children).length > 0;
            const bIsFolder = Object.keys(b.children).length > 0;
            if (aIsFolder && !bIsFolder) return -1;
            if (!aIsFolder && bIsFolder) return 1;
            return a.name.localeCompare(b.name);
        }).map((node) => {
            const isSelected = selectedKey === node.fullPath;
            const hasChildren = Object.keys(node.children).length > 0;

            return (
                <div key={node.fullPath}>
                    <div
                        style={{
                            ...styles.treeNode,
                            paddingLeft: `${level * 16 + 16}px`,
                            ...(isSelected ? styles.treeNodeSelected : {})
                        }}
                        onClick={(e) => {
                            e.stopPropagation();
                            handleNodeClick(node);
                        }}
                    >
                        <span style={styles.treeIcon}>
                            {hasChildren ? (
                                <i className={`fas fa-folder${node.isOpen ? '-open' : ''}`} style={{ color: '#ecc94b' }}></i>
                            ) : (
                                <i className="fas fa-key" style={{ color: '#a0aec0', fontSize: '0.8rem' }}></i>
                            )}
                        </span>
                        {node.name}
                        {node.isLeaf && <span style={{ marginLeft: '8px', fontSize: '0.7rem', color: '#cbd5e0' }}>{node.type}</span>}
                    </div>
                    {hasChildren && node.isOpen && (
                        <div>
                            {renderTree(node.children, level + 1)}
                        </div>
                    )}
                </div>
            );
        });
    };

    return (
        <div style={styles.container}>
            {/* HEADER */}
            <div style={styles.header}>
                <h1 style={styles.title}>
                    <i className="fas fa-database" style={styles.logoIcon}></i>
                    Redis Inspector
                    {connectionConfig ? (
                        <span style={{ fontSize: '0.8rem', fontWeight: 400, color: '#e53e3e', background: '#fff5f5', padding: '2px 8px', borderRadius: '4px', border: '1px solid #feb2b2' }}>
                            {connectionConfig.host}:{connectionConfig.port}
                        </span>
                    ) : (
                        <span style={{ fontSize: '0.8rem', fontWeight: 400, color: '#718096', background: '#edf2f7', padding: '2px 8px', borderRadius: '4px' }}>
                            {serverInfo?.connection ? `${serverInfo.connection.host}:${serverInfo.connection.port}` : 'Default'}
                        </span>
                    )}
                </h1>
                <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>

                    <div style={{ display: 'flex', gap: '16px', fontSize: '0.85rem', color: '#718096', marginRight: '16px' }}>
                        {serverInfo ? (
                            <>
                                <span>Vers: {serverInfo.server?.redis_version}</span>
                                <span>Mem: {serverInfo.memory?.used_memory_human}</span>
                                <span>Clients: {serverInfo.clients?.connected_clients}</span>
                            </>
                        ) : (
                            <span style={{ color: '#e53e3e' }}>Offline / Error</span>
                        )}
                    </div>

                    <button
                        onClick={() => setShowConfigModal(true)}
                        style={styles.secondaryButton}
                        title="Configure Connection"
                    >
                        <i className="fas fa-cog"></i> Configure
                    </button>
                    <button
                        onClick={() => fetchKeys()}
                        style={styles.secondaryButton}
                        title="Refresh"
                    >
                        <i className="fas fa-sync-alt"></i> Refresh
                    </button>
                </div>
            </div>

            <div style={styles.mainArea}>
                {/* TREE SIDEBAR */}
                <div style={styles.sidebar}>
                    <div style={styles.searchBox}>
                        <input
                            style={styles.searchInput}
                            placeholder="Filter keys (e.g. device:*)"
                            value={searchTerm}
                            onChange={e => setSearchTerm(e.target.value)}
                            onKeyDown={handleSearchKey}
                        />
                    </div>
                    <div style={styles.treeContainer}>
                        {loading ? (
                            <div style={{ padding: '20px', textAlign: 'center', color: '#a0aec0' }}>Loading...</div>
                        ) : (
                            renderTree(treeRoots)
                        )}
                    </div>
                </div>

                {/* MAIN INSPECTOR */}
                <div style={styles.contentArea}>
                    {selectedKey && keyDetails ? (
                        <div style={styles.inspectorCard}>
                            <h2 style={{ fontSize: '1.5rem', marginBottom: '8px', wordBreak: 'break-all' }}>{keyDetails.key}</h2>
                            <div style={{ marginBottom: '24px', display: 'flex', gap: '12px' }}>
                                <span style={{ background: '#ebf8ff', color: '#2b6cb0', padding: '4px 8px', borderRadius: '4px', fontSize: '0.8rem', fontWeight: 600, textTransform: 'uppercase' }}>
                                    {keyDetails.type}
                                </span>
                                <span style={{ background: keyDetails.ttl > 0 ? '#f0fff4' : '#edf2f7', color: keyDetails.ttl > 0 ? '#2f855a' : '#718096', padding: '4px 8px', borderRadius: '4px', fontSize: '0.8rem', fontWeight: 600 }}>
                                    {keyDetails.ttl > 0 ? `TTL: ${keyDetails.ttl}s` : 'PERSIST'}
                                </span>
                            </div>

                            <div style={styles.field}>
                                <label style={styles.label}>
                                    Value
                                    {viewMode === 'structured' && !isEditing && <span style={{ fontWeight: 400, color: '#a0aec0', marginLeft: '8px' }}>(Read Only)</span>}
                                    {isEditing && <span style={{ fontWeight: 600, color: '#e53e3e', marginLeft: '8px' }}>Waitng for Save...</span>}
                                </label>
                                {viewMode === 'raw' ? (
                                    <textarea
                                        style={{ ...styles.valueInput, opacity: isEditing ? 1 : 0.7, background: isEditing ? '#fff' : '#f8fafc' }}
                                        value={editValue}
                                        onChange={e => setEditValue(e.target.value)}
                                        spellCheck={false}
                                        readOnly={!isEditing}
                                    />
                                ) : (
                                    renderStructuredView()
                                )}
                            </div>

                            <div style={styles.actionBar}>
                                <div style={{ marginRight: 'auto', display: 'flex', gap: '8px' }}>
                                    <button
                                        onClick={() => setViewMode('structured')}
                                        style={viewMode === 'structured' ? styles.primaryButton : styles.secondaryButton}
                                    >
                                        <i className="fas fa-list-ul"></i> Structured
                                    </button>
                                    <button
                                        onClick={() => setViewMode('raw')}
                                        style={viewMode === 'raw' ? styles.primaryButton : styles.secondaryButton}
                                    >
                                        <i className="fas fa-code"></i> Raw
                                    </button>
                                </div>

                                {/* Edit Controls */}
                                {!isEditing ? (
                                    <>
                                        <button style={styles.dangerButton} onClick={handleDelete}>Delete Key</button>
                                        <button
                                            style={{ ...styles.primaryButton, background: '#ed8936' }}
                                            onClick={toggleEditMode}
                                        >
                                            <i className="fas fa-edit"></i> Edit
                                        </button>
                                    </>
                                ) : (
                                    <>
                                        <button style={styles.secondaryButton} onClick={() => {
                                            setIsEditing(false);
                                            handleInspect(selectedKey); // Revert changes 
                                        }}>
                                            Cancel
                                        </button>
                                        <button style={styles.primaryButton} onClick={() => {
                                            handleSave();
                                            setIsEditing(false);
                                        }}>
                                            <i className="fas fa-save"></i> Save Changes
                                        </button>
                                    </>
                                )}
                            </div>
                        </div>
                    ) : (
                        <div style={styles.emptyState}>
                            <i className="fas fa-mouse-pointer" style={{ fontSize: '3rem', color: '#cbd5e0' }}></i>
                            <p>Select a key from the tree to inspect</p>
                        </div>
                    )}
                </div>
            </div>

            {/* CONNECTION MODAL */}
            {showConfigModal && (
                <div style={styles.modalOverlay}>
                    <div style={styles.modalContent}>
                        <h2 style={{ marginTop: 0, marginBottom: '24px' }}>Configure Connection</h2>

                        <div style={styles.inputGroup}>
                            <label style={styles.label}>Host</label>
                            <input
                                style={styles.input}
                                value={tempConfig.host}
                                onChange={e => setTempConfig({ ...tempConfig, host: e.target.value })}
                            />
                        </div>
                        <div style={styles.inputGroup}>
                            <label style={styles.label}>Port</label>
                            <input
                                style={styles.input}
                                type="number"
                                value={tempConfig.port}
                                onChange={e => setTempConfig({ ...tempConfig, port: parseInt(e.target.value) })}
                            />
                        </div>
                        <div style={styles.inputGroup}>
                            <label style={styles.label}>Password</label>
                            <input
                                style={styles.input}
                                type="password"
                                value={tempConfig.password}
                                onChange={e => setTempConfig({ ...tempConfig, password: e.target.value })}
                            />
                        </div>
                        <div style={styles.inputGroup}>
                            <label style={styles.label}>Database Index</label>
                            <input
                                style={styles.input}
                                type="number"
                                value={tempConfig.db || 0}
                                onChange={e => setTempConfig({ ...tempConfig, db: parseInt(e.target.value) })}
                            />
                        </div>

                        <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '12px', marginTop: '32px' }}>
                            <button
                                onClick={handleResetConnection}
                                style={{
                                    ...styles.secondaryButton,
                                    marginRight: 'auto',
                                    color: '#e53e3e',
                                    background: '#fff5f5',
                                    border: '1px solid #fed7d7'
                                }}
                            >
                                Reset to Default
                            </button>
                            <button
                                onClick={() => setShowConfigModal(false)}
                                style={styles.secondaryButton}
                            >
                                Cancel
                            </button>
                            <button
                                onClick={handleConnect}
                                style={styles.primaryButton}
                            >
                                Connect
                            </button>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
};

export default RedisManager;
