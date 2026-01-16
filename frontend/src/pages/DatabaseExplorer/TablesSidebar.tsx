// frontend/src/pages/DatabaseExplorer/TablesSidebar.tsx
import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { Input, List, Badge, Typography, Space, Tooltip } from 'antd';
import { SearchOutlined, TableOutlined, DatabaseOutlined, RightOutlined, SafetyCertificateOutlined } from '@ant-design/icons';
import { DatabaseApiService, TableInfo } from '../../api/services/databaseApi';

const { Text } = Typography;

interface TablesSidebarProps {
    refreshTrigger: number;
    onSelectTable: (tableName: string) => void;
    selectedTable: string | null;
}

const TablesSidebar: React.FC<TablesSidebarProps> = ({ refreshTrigger, onSelectTable, selectedTable }) => {
    const [tables, setTables] = useState<TableInfo[]>([]);
    const [loading, setLoading] = useState(false);
    const [searchTerm, setSearchTerm] = useState('');
    const [dbPath, setDbPath] = useState<string>('');

    useEffect(() => {
        loadTables();
    }, [refreshTrigger]);

    const loadTables = useCallback(async () => {
        setLoading(true);
        try {
            const response = await DatabaseApiService.listTables();
            if (response.success && response.data) {
                const sorted = response.data.tables.sort((a, b) => a.name.localeCompare(b.name));
                setTables(sorted);
                if (response.data.databasePath) {
                    setDbPath(response.data.databasePath);
                }
            }
        } catch (error) {
            console.error('Failed to load tables:', error);
        } finally {
            setLoading(false);
        }
    }, []);

    const filteredTables = React.useMemo(() =>
        tables.filter(t => t.name.toLowerCase().includes(searchTerm.toLowerCase())),
        [tables, searchTerm]
    );

    return (
        <div className="flex flex-col h-full bg-white">
            {/* Header */}
            <div className="p-4 border-b border-gray-100 bg-white">
                <div className="flex items-center justify-between mb-4">
                    <Space>
                        <TableOutlined className="text-blue-600" />
                        <Text strong style={{ fontSize: '14px' }}>All Tables</Text>
                        <Badge
                            count={tables.length}
                            style={{ backgroundColor: '#f0f4ff', color: '#1d4ed8', boxShadow: 'none', fontSize: '10px' }}
                            overflowCount={999}
                        />
                    </Space>
                </div>
                <Input
                    placeholder="Filter tables..."
                    prefix={<SearchOutlined style={{ color: '#d1d5db' }} />}
                    value={searchTerm}
                    onChange={(e) => setSearchTerm(e.target.value)}
                    allowClear
                    style={{ borderRadius: '8px' }}
                />
            </div>

            {/* List */}
            <div className="flex-1 overflow-y-auto min-h-0 custom-scrollbar p-2">
                <List
                    loading={loading}
                    dataSource={filteredTables}
                    locale={{ emptyText: <div className="p-8 text-gray-400 text-xs">No tables found</div> }}
                    renderItem={(table) => (
                        <div
                            key={table.name}
                            onClick={() => onSelectTable(table.name)}
                            className={`group flex items-center px-3 py-2.5 mb-1 rounded-lg cursor-pointer transition-all duration-200 ${selectedTable === table.name
                                ? 'bg-blue-600 shadow-blue-100 shadow-md translate-x-1'
                                : 'hover:bg-gray-50'
                                }`}
                        >
                            <TableOutlined
                                className={`mr-3 text-sm ${selectedTable === table.name ? 'text-white' : 'text-gray-400 opacity-60'
                                    }`}
                            />
                            <Text
                                className={`flex-1 truncate text-xs font-medium ${selectedTable === table.name ? 'text-white' : 'text-gray-700'
                                    }`}
                            >
                                {table.name}
                            </Text>
                            {selectedTable === table.name ? (
                                <RightOutlined className="text-white text-[10px]" />
                            ) : (
                                <RightOutlined className="text-gray-300 text-[10px] opacity-0 group-hover:opacity-100 transition-opacity" />
                            )}
                        </div>
                    )}
                />
            </div>

            {/* Footer */}
            <div className="bg-gray-50/80 backdrop-blur-sm border-t border-gray-100 p-4">
                <Space direction="vertical" size={12} className="w-full">
                    <div>
                        <Text type="secondary" style={{ fontSize: '10px', textTransform: 'uppercase', fontWeight: 700, letterSpacing: '0.05em', display: 'block', marginBottom: '4px' }}>
                            <DatabaseOutlined className="mr-1" /> Target Database
                        </Text>
                        <Tooltip title={dbPath}>
                            <div className="bg-white border border-gray-200 rounded-md p-2 shadow-sm">
                                <Text className="truncate block font-mono text-[10px] text-gray-500">
                                    {dbPath || 'Scanning...'}
                                </Text>
                            </div>
                        </Tooltip>
                    </div>

                    <div className="flex items-center justify-center gap-2 py-1.5 px-3 bg-red-50 rounded-md border border-red-100">
                        <SafetyCertificateOutlined className="text-red-500 text-xs" />
                        <Text strong style={{ fontSize: '10px', color: '#ef4444' }}>SYSTEM ADMIN PRIVILEGE</Text>
                    </div>
                </Space>
            </div>

            <style>{`
                .custom-scrollbar::-webkit-scrollbar { width: 4px; }
                .custom-scrollbar::-webkit-scrollbar-track { background: transparent; }
                .custom-scrollbar::-webkit-scrollbar-thumb { background: #e5e7eb; border-radius: 4px; }
                .custom-scrollbar::-webkit-scrollbar-thumb:hover { background: #d1d5db; }
            `}</style>
        </div>
    );
};

export default TablesSidebar;

