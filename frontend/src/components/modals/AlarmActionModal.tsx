import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { Modal, Input, Form, Descriptions, Badge, Typography, Divider } from 'antd';
import { CheckCircleOutlined, InfoCircleOutlined, UserOutlined, ClockCircleOutlined, MessageOutlined } from '@ant-design/icons';
import dayjs from 'dayjs';

const { TextArea } = Input;
const { Text } = Typography;

export interface AlarmActionModalProps {
    isOpen: boolean;
    type: 'acknowledge' | 'clear';
    alarmData: {
        rule_name?: string;
        alarm_message?: string;
        severity?: string;
    };
    onConfirm: (comment: string) => void;
    onCancel: () => void;
    loading?: boolean;
}

const AlarmActionModal: React.FC<AlarmActionModalProps> = ({
    isOpen,
    type,
    alarmData,
    onConfirm,
    onCancel,
    loading = false
}) => {
    const [comment, setComment] = useState('System Check');
    const { t } = useTranslation(['alarms', 'common']);
    const isAcknowledge = type === 'acknowledge';
    const title = isAcknowledge ? t('action.acknowledgeTitle', { defaultValue: '알람 인지' }) : t('action.clearTitle', { defaultValue: '알람 해제' });
    const color = isAcknowledge ? '#1890ff' : '#52c41a';

    useEffect(() => {
        if (isOpen) {
            setComment(isAcknowledge ? t('action.defaultAckComment', { defaultValue: '시스템 확인' }) : t('action.defaultClearComment', { defaultValue: '상황 종료 및 조치 완료' }));
        }
    }, [isOpen, type, isAcknowledge]);

    const handleOk = () => {
        onConfirm(comment);
    };

    const getSeverityBadge = (severity?: string) => {
        switch (severity?.toLowerCase()) {
            case 'critical': return <Badge status="error" text={t('severity.critical', { defaultValue: '위험' })} />;
            case 'high': return <Badge status="warning" text={t('severity.high', { defaultValue: '높음' })} />;
            case 'medium': return <Badge status="processing" text={t('severity.medium', { defaultValue: '중간' })} />;
            case 'low': return <Badge status="default" text={t('severity.low', { defaultValue: '낮음' })} />;
            default: return <Badge status="default" text={severity || t('severity.info', { defaultValue: '정보' })} />;
        }
    };

    return (
        <Modal
            title={
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    {isAcknowledge ? <InfoCircleOutlined style={{ color }} /> : <CheckCircleOutlined style={{ color }} />}
                    <span>{title}</span>
                </div>
            }
            open={isOpen}
            onOk={handleOk}
            onCancel={onCancel}
            confirmLoading={loading}
            okText={isAcknowledge ? t('action.acknowledgeBtn', { defaultValue: '인지' }) : t('action.clearBtn', { defaultValue: '해제' })}
            cancelText="취소"
            centered
            width={520}
            destroyOnClose
            okButtonProps={{ style: { height: '32px', fontSize: '13px' } }}
            cancelButtonProps={{ style: { height: '32px', fontSize: '13px' } }}
        >
            <div style={{ padding: '8px 0' }}>
                <Descriptions bordered size="small" column={1}>
                    <Descriptions.Item label="알람 정보">
                        <Text strong>{alarmData.rule_name || 'N/A'}</Text>
                        <br />
                        <Text type="secondary" style={{ fontSize: '12px' }}>{alarmData.alarm_message}</Text>
                    </Descriptions.Item>
                    <Descriptions.Item label="심각도">
                        {getSeverityBadge(alarmData.severity)}
                    </Descriptions.Item>
                </Descriptions>

                <Divider style={{ margin: '16px 0' }} />

                <Form layout="vertical">
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '16px' }}>
                        <Form.Item label={<><UserOutlined style={{ marginRight: '4px' }} /> 확인자</>} style={{ marginBottom: 0 }}>
                            <Input value="관리자" disabled />
                        </Form.Item>
                        <Form.Item label={<><ClockCircleOutlined style={{ marginRight: '4px' }} /> 처리 일시</>} style={{ marginBottom: 0 }}>
                            <Input value={dayjs().format('YYYY-MM-DD HH:mm:ss')} disabled />
                        </Form.Item>
                    </div>

                    <Form.Item
                        label={<><MessageOutlined style={{ marginRight: '4px' }} /> 처리 사유 (의견)</>}
                        style={{ marginTop: '16px' }}
                    >
                        <TextArea
                            rows={3}
                            value={comment}
                            onChange={(e) => setComment(e.target.value)}
                            placeholder="처리 사유를 입력해주세요..."
                        />
                    </Form.Item>
                </Form>
            </div>

            <style>{`
        .ant-descriptions-item-label {
          width: 100px;
          background-color: #fafafa !important;
        }
      `}</style>
        </Modal>
    );
};

export default AlarmActionModal;
