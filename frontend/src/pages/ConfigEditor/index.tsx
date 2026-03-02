// frontend/src/pages/ConfigEditor/index.tsx
import React, { useState, useEffect, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { Layout, Typography, Space, Tag, List, Badge, Card, Tooltip, Input } from 'antd';
import {
    SettingFilled,
    FileTextOutlined,
    RightOutlined,
    FolderOpenOutlined,
    LockOutlined,
    SearchOutlined,
    WarningOutlined
} from '@ant-design/icons';
import { ManagementLayout } from '../../components/common/ManagementLayout';
import { ConfigApiService, ConfigFile } from '../../api/services/configApi';
import EnvFileEditor from './EnvFileEditor';
import '../../styles/config-editor.css'; // Use the new specialized CSS

const { Sider, Content } = Layout;
const { Title, Text } = Typography;

const ConfigEditor: React.FC = () => {
    const { t } = useTranslation('config');
    const [files, setFiles] = useState<ConfigFile[]>([]);
    const [selectedFile, setSelectedFile] = useState<string | null>(null);
    const [loading, setLoading] = useState(false);
    const [configPath, setConfigPath] = useState('');
    const [searchTerm, setSearchTerm] = useState('');

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

    const filteredFiles = useMemo(() => {
        if (!searchTerm) return files;
        return files.filter(f => f.name.toLowerCase().includes(searchTerm.toLowerCase()));
    }, [files, searchTerm]);

    return (
        <ManagementLayout className="config-editor-container">
            {/* Premium Header Area */}
            <div className="bg-white border-b border-gray-200 px-8 py-5 flex justify-between items-center z-10">
                <Space size="large">
                    <div className="bg-primary-600 p-3 rounded-2xl shadow-primary-100 shadow-xl text-white flex items-center justify-center">
                        <SettingFilled style={{ fontSize: '24px' }} />
                    </div>
                    <div>
                        <Title level={3} style={{ margin: 0, fontWeight: 800, letterSpacing: '-0.025em', color: 'var(--neutral-900)' }}>
                            {t('title')}
                        </Title>
                        <Text style={{ fontSize: '14px', color: 'var(--neutral-500)' }}>
                            {t('description')}
                        </Text>
                    </div>
                </Space>

                <div className="flex items-center gap-4">
                    <Tag color="warning" icon={<WarningOutlined />} style={{ padding: '6px 16px', borderRadius: '12px', fontWeight: 700, fontSize: '13px', border: 'none', background: 'var(--warning-50)', color: 'var(--warning-700)' }}>
                        {t('restartWarning')}
                    </Tag>
                </div>
            </div>

            <Layout className="flex-1 overflow-hidden bg-transparent border-t border-neutral-200 !h-0 min-h-0">
                {/* Modern Sidebar */}
                <Sider width={320} className="config-sidebar">
                    <div className="sidebar-search">
                        <div className="search-input-wrapper">
                            <i className="fas fa-search"></i>
                            <input
                                type="text"
                                placeholder={t('searchPlaceholder')}
                                value={searchTerm}
                                onChange={(e) => setSearchTerm(e.target.value)}
                            />
                        </div>
                    </div>

                    <div className="file-list custom-scrollbar">
                        <div className="px-3 py-2">
                            <Text strong style={{ fontSize: '11px', color: 'var(--neutral-400)', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                                {t('fileListLabel', { count: filteredFiles.length })}
                            </Text>
                        </div>

                        {loading ? (
                            <div className="p-8 text-center"><Text type="secondary">{t('loading')}</Text></div>
                        ) : (
                            filteredFiles.map((file) => (
                                <div
                                    key={file.name}
                                    onClick={() => setSelectedFile(file.name)}
                                    className={`file-item ${selectedFile === file.name ? 'active' : ''}`}
                                >
                                    <i className={`fas ${file.name.includes('.env') ? 'fa-file-code' : 'fa-file-alt'}`}></i>
                                    <span className="file-name">{file.name}</span>
                                    {selectedFile === file.name && <RightOutlined style={{ fontSize: '10px' }} />}
                                </div>
                            ))
                        )}
                    </div>

                    <div className="p-6 border-t border-gray-100 bg-gray-50/50">
                        <div className="flex items-center justify-between mb-3">
                            <Text style={{ fontSize: '11px', fontWeight: 700, color: 'var(--neutral-500)' }}>
                                {t('pathLabel')}
                            </Text>
                            <Badge status="processing" text={<Text style={{ fontSize: '10px', color: 'var(--neutral-400)' }}>Connected</Text>} />
                        </div>
                        <Tooltip title={configPath}>
                            <div className="bg-white border border-neutral-200 rounded-xl p-3 shadow-sm flex items-center gap-3">
                                <FolderOpenOutlined style={{ color: 'var(--primary-500)' }} />
                                <Text style={{ fontSize: '11px', color: 'var(--neutral-600)', fontFamily: 'monospace' }} className="truncate">
                                    {configPath || '/app/config'}
                                </Text>
                            </div>
                        </Tooltip>

                        <div className="mt-4 flex items-center gap-2 py-2 px-3 bg-error-50 rounded-lg border border-error-100">
                            <LockOutlined className="text-error-500 text-[12px]" />
                            <Text strong style={{ fontSize: '10px', color: 'var(--error-600)' }}>RESTRICTED SYSTEM ACCESS</Text>
                        </div>
                    </div>
                </Sider>

                {/* Editor Area */}
                <Content className="flex-1 overflow-hidden bg-transparent flex flex-col min-h-0 h-full max-h-full">
                    {selectedFile ? (
                        <div className="flex-1 flex flex-col min-h-0 relative h-full max-h-full">
                            <EnvFileEditor filename={selectedFile} />
                        </div>
                    ) : (
                        <div className="empty-state">
                            <Card bordered={false} className="bg-transparent text-center">
                                <div className="bg-neutral-100 w-20 h-20 rounded-3xl flex items-center justify-center mx-auto mb-6">
                                    <FileTextOutlined style={{ fontSize: '32px', color: 'var(--neutral-400)' }} />
                                </div>
                                <Title level={4} style={{ color: 'var(--neutral-800)', fontWeight: 800 }}>{t('emptyTitle')}</Title>
                                <Text style={{ color: 'var(--neutral-500)' }}>{t('emptyDesc')}</Text>
                            </Card>
                        </div>
                    )}
                </Content>
            </Layout>
        </ManagementLayout>
    );
};

export default ConfigEditor;

