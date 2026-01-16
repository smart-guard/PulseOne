// frontend/src/pages/ConfigEditor/index.tsx
import React, { useState, useEffect } from 'react';
import { Layout, Typography, Space, Tag, List, Badge, Card, Tooltip } from 'antd';
import {
    SettingOutlined,
    WarningOutlined,
    FileTextOutlined,
    SettingFilled,
    RightOutlined,
    FolderOpenOutlined,
    LockOutlined
} from '@ant-design/icons';
import { ManagementLayout } from '../../components/common/ManagementLayout';
import { ConfigApiService, ConfigFile } from '../../api/services/configApi';
import EnvFileEditor from './EnvFileEditor';
import '../../styles/management.css';

const { Sider, Content } = Layout;
const { Title, Text } = Typography;

const ConfigEditor: React.FC = () => {
    const [files, setFiles] = useState<ConfigFile[]>([]);
    const [selectedFile, setSelectedFile] = useState<string | null>(null);
    const [loading, setLoading] = useState(false);
    const [configPath, setConfigPath] = useState('');

    useEffect(() => {
        loadFiles();
    }, []);

    const loadFiles = async () => {
        setLoading(true);
        try {
            const response = await ConfigApiService.listFiles();
            if (response.success && response.data) {
                setFiles(response.data.files);
                if (response.data.path) setConfigPath(response.data.path);
            }
        } catch (error) {
            console.error('Failed to load config files:', error);
        } finally {
            setLoading(false);
        }
    };

    return (
        <ManagementLayout className="h-full flex flex-col bg-gray-50">
            {/* Header Area */}
            <div className="bg-white border-b border-gray-200 px-6 py-4 flex justify-between items-center shadow-sm z-10">
                <Space size="middle">
                    <div className="bg-purple-600 p-2.5 rounded-xl shadow-purple-100 shadow-lg text-white flex items-center justify-center">
                        <SettingFilled style={{ fontSize: '20px' }} />
                    </div>
                    <div>
                        <Title level={4} style={{ margin: 0, fontWeight: 800, letterSpacing: '-0.02em' }}>
                            System Configuration
                        </Title>
                        <Text type="secondary" style={{ fontSize: '12px' }}>
                            Edit server environment variables and configuration patterns
                        </Text>
                    </div>
                </Space>
                <Tag color="warning" icon={<WarningOutlined />} style={{ padding: '4px 12px', borderRadius: '6px', fontWeight: 600 }}>
                    Restart backend to apply changes
                </Tag>
            </div>

            <Layout className="flex-1 overflow-hidden h-full bg-transparent">
                {/* Sidebar: File List */}
                <Sider
                    width={280}
                    theme="light"
                    className="border-r border-gray-200"
                    style={{ background: '#fff' }}
                >
                    <div className="flex flex-col h-full bg-white">
                        <div className="p-4 border-b border-gray-100">
                            <Space>
                                <SettingOutlined className="text-purple-600" />
                                <Text strong style={{ fontSize: '14px' }}>Config Files</Text>
                                <Badge
                                    count={files.length}
                                    style={{ backgroundColor: '#f5f3ff', color: '#7c3aed', boxShadow: 'none', fontSize: '10px' }}
                                />
                            </Space>
                        </div>

                        <div className="flex-1 overflow-y-auto custom-scrollbar p-2">
                            <List
                                loading={loading}
                                dataSource={files}
                                renderItem={(file) => (
                                    <div
                                        key={file.name}
                                        onClick={() => setSelectedFile(file.name)}
                                        className={`group flex items-center px-4 py-3 mb-1 rounded-lg cursor-pointer transition-all duration-200 ${selectedFile === file.name
                                            ? 'bg-purple-600 shadow-purple-100 shadow-md translate-x-1'
                                            : 'hover:bg-gray-50'
                                            }`}
                                    >
                                        <FileTextOutlined
                                            className={`mr-3 text-sm ${selectedFile === file.name ? 'text-white' : 'text-purple-400 opacity-60'
                                                }`}
                                        />
                                        <Text
                                            className={`flex-1 truncate text-xs font-medium ${selectedFile === file.name ? 'text-white' : 'text-gray-700'
                                                }`}
                                        >
                                            {file.name}
                                        </Text>
                                        {selectedFile === file.name ? (
                                            <RightOutlined className="text-white text-[10px]" />
                                        ) : (
                                            <RightOutlined className="text-gray-300 text-[10px] opacity-0 group-hover:opacity-100" />
                                        )}
                                    </div>
                                )}
                            />
                        </div>

                        <div className="bg-gray-50 p-4 border-t border-gray-100">
                            <Text type="secondary" style={{ fontSize: '10px', textTransform: 'uppercase', fontWeight: 700, letterSpacing: '0.05em', display: 'block', marginBottom: '8px' }}>
                                <FolderOpenOutlined className="mr-1" /> Config Directory
                            </Text>
                            <Tooltip title={configPath}>
                                <div className="bg-white border border-gray-200 rounded-md p-2 shadow-sm">
                                    <Text className="truncate block font-mono text-[9px] text-gray-500">
                                        {configPath || 'Scanning...'}
                                    </Text>
                                </div>
                            </Tooltip>
                            <div className="mt-4 flex items-center justify-center gap-2 py-1.5 px-3 bg-red-50 rounded-md border border-red-100">
                                <LockOutlined className="text-red-500 text-[10px]" />
                                <Text strong style={{ fontSize: '9px', color: '#ef4444' }}>RESTRICTED ACCESS</Text>
                            </div>
                        </div>
                    </div>
                </Sider>

                {/* Main Content: Editor */}
                <Content className="overflow-hidden bg-gray-50 flex flex-col">
                    {selectedFile ? (
                        <div className="h-full flex flex-col">
                            <EnvFileEditor filename={selectedFile} />
                        </div>
                    ) : (
                        <div className="h-full flex flex-col items-center justify-center text-gray-400">
                            <Card bordered={false} className="bg-transparent text-center">
                                <FileTextOutlined style={{ fontSize: '64px', opacity: 0.1, marginBottom: '24px', display: 'block' }} />
                                <Title level={4} style={{ color: '#9ca3af', fontWeight: 600 }}>Select a configuration file</Title>
                                <Text type="secondary">Choose an environment file to manage system settings</Text>
                            </Card>
                        </div>
                    )}
                </Content>
            </Layout>

            <style>{`
                .custom-scrollbar::-webkit-scrollbar { width: 4px; }
                .custom-scrollbar::-webkit-scrollbar-track { background: transparent; }
                .custom-scrollbar::-webkit-scrollbar-thumb { background: #e5e7eb; border-radius: 4px; }
                .custom-scrollbar::-webkit-scrollbar-thumb:hover { background: #d1d5db; }
            `}</style>
        </ManagementLayout>
    );
};

export default ConfigEditor;

