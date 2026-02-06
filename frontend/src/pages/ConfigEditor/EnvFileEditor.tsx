// frontend/src/pages/ConfigEditor/EnvFileEditor.tsx
import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Button, Space, Typography, Alert, message, Tooltip } from 'antd';
import {
    SaveOutlined,
    UndoOutlined,
    FileTextOutlined,
    InfoCircleOutlined,
    LockOutlined,
    UnlockOutlined
} from '@ant-design/icons';
import { ConfigApiService } from '../../api/services/configApi';

const { Text } = Typography;

interface EnvFileEditorProps {
    filename: string;
    onClose?: () => void;
}

const EnvFileEditor: React.FC<EnvFileEditorProps> = ({ filename }) => {
    const [content, setContent] = useState('');
    const [originalContent, setOriginalContent] = useState('');
    const [loading, setLoading] = useState(false);
    const [saving, setSaving] = useState(false);
    const [status, setStatus] = useState<{ type: 'success' | 'error' | null, msg: string | null }>({ type: null, msg: null });

    const textareaRef = useRef<HTMLTextAreaElement>(null);
    const lineNumbersRef = useRef<HTMLDivElement>(null);

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
            message.error('파일 내용을 불러오지 못했습니다.');
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
            setStatus({ type: 'success', msg: '설정이 성공적으로 저장되었습니다. 시스템 재시작을 권장합니다.' });
            message.success('설정이 저장되었습니다.');
        } catch (err: any) {
            setStatus({ type: 'error', msg: err.message || 'Failed to save file' });
            message.error('저장에 실패했습니다.');
        } finally {
            setSaving(false);
        }
    }, [filename, content]);

    const handleReset = useCallback(() => {
        setContent(originalContent);
        message.info('변경 사항이 초기화되었습니다.');
    }, [originalContent]);

    const handleEncrypt = async () => {
        if (!textareaRef.current) return;

        const start = textareaRef.current.selectionStart;
        const end = textareaRef.current.selectionEnd;
        const selectedText = content.substring(start, end);

        if (!selectedText) {
            message.warning('암호화할 텍스트를 선택해주세요.');
            return;
        }

        try {
            const response = await ConfigApiService.encryptSecret(selectedText);
            if (response.success && response.data) {
                const newContent = content.substring(0, start) + response.data.encrypted + content.substring(end);
                setContent(newContent);
                message.success('선택된 텍스트가 암호화되었습니다.');
            }
        } catch (err: any) {
            message.error('암호화 실패: ' + err.message);
        }
    };

    const handleDecrypt = async () => {
        if (!textareaRef.current) return;

        const start = textareaRef.current.selectionStart;
        const end = textareaRef.current.selectionEnd;
        const selectedText = content.substring(start, end);

        if (!selectedText) {
            message.warning('복호화할 텍스트(ENC:...)를 선택해주세요.');
            return;
        }

        if (!selectedText.startsWith('ENC:')) {
            message.warning('선택된 텍스트가 암호화된 형식(ENC:...)이 아닙니다.');
            return;
        }

        try {
            const response = await ConfigApiService.decryptSecret(selectedText);
            if (response.success && response.data) {
                const newContent = content.substring(0, start) + response.data.decrypted + content.substring(end);
                setContent(newContent);
                message.success('선택된 텍스트가 복호화되었습니다. (저장 전 다시 암호화하세요!)');
            }
        } catch (err: any) {
            message.error('복호화 실패: ' + err.message);
        }
    };

    const handleScroll = () => {
        if (textareaRef.current && lineNumbersRef.current) {
            lineNumbersRef.current.scrollTop = textareaRef.current.scrollTop;
        }
    };

    const hasChanges = content !== originalContent;
    const lineCount = content.split('\n').length;

    // Check if file is a secret file (security, credentials, etc.)
    const isSecretFile = filename.toLowerCase().includes('security') ||
        filename.toLowerCase().includes('secret') ||
        filename.toLowerCase().includes('cred') ||
        filename.toLowerCase().includes('password') ||
        filename.toLowerCase().includes('key') ||
        filename.endsWith('.env');

    return (
        <div className="editor-main-container">
            <div className="editor-surface">
                {/* Editor Toolbar */}
                <div className="editor-toolbar">
                    <Space size="middle">
                        <div className="bg-neutral-50 p-2 rounded-lg text-neutral-400">
                            <FileTextOutlined style={{ fontSize: '18px' }} />
                        </div>
                        <div>
                            <Text strong style={{ fontSize: '15px', color: 'var(--neutral-800)' }}>{filename}</Text>
                            <div className="flex items-center gap-2">
                                <div className={`unsaved-dot ${!hasChanges ? 'hidden' : ''}`} />
                                <Text style={{ fontSize: '11px', color: 'var(--neutral-400)' }}>
                                    {hasChanges ? '저장되지 않은 변경 사항' : '모든 변경 사항이 저장됨'}
                                </Text>
                            </div>
                        </div>
                    </Space>

                    <Space>
                        {isSecretFile && (
                            <>
                                <Tooltip title="선택한 텍스트 암호화 (평문 -> ENC:...)">
                                    <Button
                                        type="text"
                                        icon={<LockOutlined />}
                                        onClick={handleEncrypt}
                                        className="text-neutral-600 hover:text-blue-600 hover:bg-blue-50"
                                    >
                                        잠그기
                                    </Button>
                                </Tooltip>
                                <Tooltip title="선택한 텍스트 복호화 (ENC:... -> 평문)">
                                    <Button
                                        type="text"
                                        icon={<UnlockOutlined />}
                                        onClick={handleDecrypt}
                                        className="text-neutral-600 hover:text-green-600 hover:bg-green-50"
                                    >
                                        풀기
                                    </Button>
                                </Tooltip>
                                <div className="w-px h-6 bg-neutral-200 mx-2" />
                            </>
                        )}
                        <Button
                            icon={<UndoOutlined />}
                            onClick={handleReset}
                            disabled={!hasChanges || saving}
                            className="border-neutral-200"
                        >
                            초기화
                        </Button>
                        <Button
                            type="primary"
                            icon={<SaveOutlined />}
                            onClick={handleSave}
                            loading={saving}
                            disabled={!hasChanges}
                            className="shadow-primary-100 shadow-lg"
                        >
                            설정 저장하기
                        </Button>
                    </Space>
                </div>

                {/* Status Alert */}
                {status.msg && (
                    <div className="px-6 py-2">
                        <Alert
                            message={status.msg}
                            type={status.type || 'info'}
                            showIcon
                            closable
                            onClose={() => setStatus({ type: null, msg: null })}
                            style={{ borderRadius: '10px' }}
                        />
                    </div>
                )}

                {/* Code Editor Content Area */}
                <div className="editor-content-area group">
                    <div className="line-numbers custom-scrollbar" ref={lineNumbersRef}>
                        {Array.from({ length: Math.max(lineCount, 1) }, (_, i) => (
                            <div key={i + 1}>{i + 1}</div>
                        ))}
                    </div>
                    <textarea
                        ref={textareaRef}
                        value={content}
                        onChange={(e) => setContent(e.target.value)}
                        onScroll={handleScroll}
                        className="code-editor-textarea"
                        spellCheck={false}
                        placeholder="# 환경 설정을 입력하세요..."
                    />
                </div>

                {/* Editor Footer */}
                <div className="px-6 py-3 bg-neutral-50/50 border-t border-neutral-100 flex justify-between items-center">
                    <Space size="large">
                        <Space size={6}>
                            <InfoCircleOutlined className="text-neutral-400 text-[12px]" />
                            <Text style={{ fontSize: '11px', color: 'var(--neutral-500)', fontWeight: 600 }}>ANSI Encoding</Text>
                        </Space>
                        <div className="w-1 h-1 bg-neutral-300 rounded-full" />
                        <Space size={4}>
                            <Text style={{ fontSize: '11px', color: 'var(--neutral-500)', fontWeight: 600 }}>{content.length} characters</Text>
                        </Space>
                        <div className="w-1 h-1 bg-neutral-300 rounded-full" />
                        <Space size={4}>
                            <Text style={{ fontSize: '11px', color: 'var(--neutral-500)', fontWeight: 600 }}>{lineCount} lines</Text>
                        </Space>
                    </Space>
                    <Text style={{ fontSize: '10px', color: 'var(--neutral-400)', fontStyle: 'italic' }}>
                        * 주요 환경 변수 수정 시 서비스 중단이 발생할 수 있으니 주의하십시오.
                    </Text>
                </div>
            </div>
        </div>
    );
};

export default EnvFileEditor;
