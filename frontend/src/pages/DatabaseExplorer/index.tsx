// frontend/src/pages/DatabaseExplorer/index.tsx
import React, { useState } from 'react';
import { Layout, Typography, Tag, Space, Card } from 'antd';
import { DatabaseOutlined, LockOutlined, TableOutlined } from '@ant-design/icons';
import { ManagementLayout } from '../../components/common/ManagementLayout';
import TablesSidebar from './TablesSidebar';
import DataTable from './DataTable';
import '../../styles/management.css';

const { Content, Sider } = Layout;
const { Title, Text } = Typography;

const DatabaseExplorer: React.FC = () => {
    const [selectedTable, setSelectedTable] = useState<string | null>(null);
    const [refreshTrigger, setRefreshTrigger] = useState(0);

    return (
        <ManagementLayout className="h-full flex flex-col bg-gray-50">
            {/* Header Area */}
            <div className="bg-white border-b border-gray-200 px-6 py-4 flex justify-between items-center shadow-sm z-10">
                <Space size="middle">
                    <div className="bg-blue-600 p-2.5 rounded-xl shadow-blue-100 shadow-lg text-white flex items-center justify-center">
                        <DatabaseOutlined style={{ fontSize: '20px' }} />
                    </div>
                    <div>
                        <Title level={4} style={{ margin: 0, fontWeight: 800, letterSpacing: '-0.02em' }}>
                            Database Explorer
                        </Title>
                        <Text type="secondary" style={{ fontSize: '12px' }}>
                            Direct Database Management Interface
                        </Text>
                    </div>
                </Space>
                <Tag color="error" icon={<LockOutlined />} style={{ padding: '4px 12px', borderRadius: '6px', fontWeight: 700 }}>
                    ADMIN ONLY
                </Tag>
            </div>

            {/* Main Split Layout */}
            <Layout className="flex-1 overflow-hidden h-full bg-transparent">
                {/* Sidebar */}
                <Sider
                    width={300}
                    theme="light"
                    className="border-r border-gray-200 overflow-hidden"
                    style={{ background: '#fff' }}
                >
                    <TablesSidebar
                        refreshTrigger={refreshTrigger}
                        onSelectTable={setSelectedTable}
                        selectedTable={selectedTable}
                    />
                </Sider>

                {/* Content Area */}
                <Content className="flex flex-col bg-white overflow-hidden">
                    {selectedTable ? (
                        <DataTable tableName={selectedTable} />
                    ) : (
                        <div className="h-full flex flex-col items-center justify-center text-gray-400 bg-gray-50/30">
                            <Card bordered={false} className="bg-transparent text-center">
                                <TableOutlined style={{ fontSize: '64px', opacity: 0.1, marginBottom: '24px', display: 'block' }} />
                                <Title level={4} style={{ color: '#9ca3af', fontWeight: 600 }}>Select a table from the sidebar</Title>
                                <Text type="secondary">Choose a data entity to view schema and manage records</Text>
                            </Card>
                        </div>
                    )}
                </Content>
            </Layout>
        </ManagementLayout>
    );
};

export default DatabaseExplorer;

