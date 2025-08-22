// src/components/common/ConfirmDialog.tsx (Ant Design 버전)
import React from 'react';
import { Modal } from 'antd';
import { ExclamationCircleOutlined, QuestionCircleOutlined, WarningOutlined } from '@ant-design/icons';

export interface ConfirmDialogProps {
  isOpen: boolean;
  title: string;
  message: string;
  confirmText?: string;
  cancelText?: string;
  confirmButtonType?: 'primary' | 'danger' | 'warning';
  showCancelButton?: boolean;
  onConfirm: () => void;
  onCancel: () => void;
}

const ConfirmDialog: React.FC<ConfirmDialogProps> = ({
  isOpen,
  title,
  message,
  confirmText = '확인',
  cancelText = '취소',
  confirmButtonType = 'primary',
  showCancelButton = true,
  onConfirm,
  onCancel
}) => {
  const getIcon = () => {
    switch (confirmButtonType) {
      case 'danger':
        return <ExclamationCircleOutlined style={{ color: '#ff4d4f' }} />;
      case 'warning':
        return <WarningOutlined style={{ color: '#faad14' }} />;
      default:
        return <QuestionCircleOutlined style={{ color: '#1890ff' }} />;
    }
  };

  const getConfirmType = () => {
    switch (confirmButtonType) {
      case 'danger':
        return 'primary';
      case 'warning':
        return 'primary';
      default:
        return 'primary';
    }
  };

  return (
    <Modal
      title={
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {getIcon()}
          <span>{title}</span>
        </div>
      }
      open={isOpen}
      onOk={onConfirm}
      onCancel={showCancelButton ? onCancel : undefined}
      okText={confirmText}
      cancelText={showCancelButton ? cancelText : undefined}
      okType="primary"
      okButtonProps={{
        style: {
          backgroundColor: confirmButtonType === 'danger' ? '#ff4d4f' : 
                         confirmButtonType === 'warning' ? '#faad14' : '#1890ff',
          borderColor: confirmButtonType === 'danger' ? '#ff4d4f' : 
                      confirmButtonType === 'warning' ? '#faad14' : '#1890ff'
        }
      }}
      footer={showCancelButton ? undefined : [
        <button
          key="ok"
          onClick={onConfirm}
          style={{
            backgroundColor: confirmButtonType === 'danger' ? '#ff4d4f' : 
                           confirmButtonType === 'warning' ? '#faad14' : '#1890ff',
            borderColor: confirmButtonType === 'danger' ? '#ff4d4f' : 
                        confirmButtonType === 'warning' ? '#faad14' : '#1890ff',
            color: 'white',
            border: '1px solid',
            borderRadius: '6px',
            padding: '4px 15px',
            cursor: 'pointer',
            fontSize: '14px',
            height: '32px'
          }}
        >
          {confirmText}
        </button>
      ]}
      width={480}
      centered
      maskClosable={false}
      destroyOnHidden
    >
      <div style={{ 
        padding: '16px 0',
        fontSize: '14px',
        lineHeight: '1.5',
        whiteSpace: 'pre-line'
      }}>
        {message}
      </div>
    </Modal>
  );
};

export default ConfirmDialog;