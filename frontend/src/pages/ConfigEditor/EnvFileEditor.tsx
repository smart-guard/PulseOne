import React, { useState, useEffect, useCallback } from 'react';
import { Button, Space, Typography, Card, Alert, message, Divider, Tooltip, Tag } from 'antd';
import {
    SaveOutlined,
    UndoOutlined,
    FileTextOutlined,
    InfoCircleOutlined,
    CheckCircleFilled,
    WarningFilled
} from '@ant-design/icons';
import { ConfigApiService } from '../../api/services/configApi';

const { Text, Title } = Typography;

interface EnvFileEditorProps {
    filename: string;
    onClose?: () => void;
}

const EnvFileEditor: React.FC<EnvFileEditorProps> = ({ filename, onClose }) => {
    const [content, setContent] = useState('');
    const [originalContent, setOriginalContent] = useState('');
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [status, setStatus] = useState<{ type: 'success' | 'error' | null, msg: string | null }>({ type: null, msg: null });

    const loadContent = useCallback(async () => {
        setLoading(true);
        setStatus({ type: null, msg: null });
        try {
            const response = await ConfigApiService.getFileContent(filename);
            if (response.success && response.data) {
                setContent(response.data.content);
                setOriginalContent(response.data.content);
            }
        } catch (err: any) {
            setStatus({ type: 'error', msg: err.message || 'Failed to load file content' });
            message.error('Failed to load file content');
        } finally {
            setLoading(false);
        }
    }, [filename]);

    useEffect(() => {
        if (filename) {
            loadContent();
        }
    }, [filename, loadContent]);

    const handleSave = useCallback(async () => {
        setSaving(true);
        setStatus({ type: null, msg: null });
        try {
            await ConfigApiService.saveFileContent(filename, content);
            setOriginalContent(content);
            setStatus({ type: 'success', msg: 'Settings applied successfully. System restart recommended.' });
            message.success('File saved successfully');
        } catch (err: any) {
            setStatus({ type: 'error', msg: err.message || 'Failed to save file' });
            message.error('Failed to save file');
        } finally {
            setSaving(false);
        }
    }, [filename, content]);

    const handleReset = useCallback(() => {
        setContent(originalContent);
        message.info('Changes discarded');
    }, [originalContent]);

    const hasChanges = content !== originalContent;

    return (
        <div className="flex flex-col h-full bg-gray-50 p-6 overflow-hidden">
            <Card
                className="flex-1 flex flex-col overflow-hidden shadow-sm border-gray-200"
                bodyStyle={{ padding: 0, height: '100%', display: 'flex', flexDirection: 'column' }}
            >
                {/* Internal Toolbar */}
                <div className="px-6 py-4 flex justify-between items-center bg-white border-b border-gray-100">
                    <Space size="middle">
                        <div className="bg-gray-100 p-2 rounded-lg text-gray-500">
                            <FileTextOutlined style={{ fontSize: '18px' }} />
                        </div>
                        <div>
                            <Text strong style={{ fontSize: '15px', color: '#1e293b' }}>{filename}</Text>
                            <div className="flex items-center gap-1.5">
                                <span className={`w-1.5 h-1.5 rounded-full ${hasChanges ? 'bg-amber-400 animate-pulse' : 'bg-green-400'}`} />
                                <Text type="secondary" style={{ fontSize: '11px' }}>
                                    {hasChanges ? 'Unsaved Changes' : 'All changes saved'}
                                </Text>
                            </div>
                        </div>
                    </Space>

                    <Space>
                        <Button
                            icon={<UndoOutlined />}
                            onClick={handleReset}
                            disabled={!hasChanges || saving}
                        >
                            Reset
                        </Button>
                        <Button
                            type="primary"
                            icon={<SaveOutlined />}
                            onClick={handleSave}
                            loading={saving}
                            disabled={!hasChanges}
                        >
                            Save Configuration
                        </Button>
                    </Space>
                </div>

                {/* Status Alerts */}
                {status.msg && (
                    <div className="px-6 pt-4">
                        <Alert
                            message={status.msg}
                            type={status.type || 'info'}
                            showIcon
                            icon={status.type === 'success' ? <CheckCircleFilled /> : <WarningFilled />}
                            closable
                            onClose={() => setStatus({ type: null, msg: null })}
                        />
                    </div>
                )}

                {/* Editor Container */}
                <div className="flex-1 mt-4 px-6 pb-6 overflow-hidden flex flex-col">
                    <div className="flex-1 bg-gray-900 rounded-xl overflow-hidden shadow-inner relative group border border-gray-800">
                        <textarea
                            value={content}
                            onChange={(e) => setContent(e.target.value)}
                            className="w-full h-full p-6 font-mono text-sm leading-relaxed resize-none focus:outline-none text-emerald-400 bg-transparent"
                            spellCheck={false}
                            placeholder="# Write your configuration here..."
                        />
                        <div className="absolute top-4 right-4 opacity-0 group-hover:opacity-100 transition-opacity">
                            <Tooltip title="Editor Mode: ENV / RAW">
                                <Tag color="gray" className="bg-gray-800 border-gray-700 text-gray-400">LN {content.split('\n').length}</Tag>
                            </Tooltip>
                        </div>
                    </div>
                </div>

                {/* Footer Info */}
                <div className="px-6 py-3 bg-gray-50/50 border-t border-gray-100 flex justify-between items-center">
                    <Space size="large">
                        <Space size={4}>
                            <InfoCircleOutlined className="text-gray-400 text-xs" />
                            <Text type="secondary" style={{ fontSize: '11px' }}>ANSI Encoding</Text>
                        </Space>
                        <Space size={4}>
                            <Text type="secondary" style={{ fontSize: '11px' }}>{content.length} characters</Text>
                        </Space>
                    </Space>
                    <Text type="secondary" style={{ fontSize: '10px', fontStyle: 'italic' }}>
                        * Modifying critical environment variables may cause service interruption.
                    </Text>
                </div>
            </Card>

            <style>{`
                .ant-card-body {
                    flex: 1;
                }
                textarea::-webkit-scrollbar { width: 6px; }
                textarea::-webkit-scrollbar-track { background: rgba(0,0,0,0.2); }
                textarea::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.1); border-radius: 10px; }
                textarea::-webkit-scrollbar-thumb:hover { background: rgba(255,255,255,0.2); }
            `}</style>
        </div>
    );
};

export default EnvFileEditor;

