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
  confirmButtonType?: 'primary' | 'danger' | 'warning' | 'success';
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
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '4px 0' }}>
          <div style={{
            width: '36px',
            height: '36px',
            borderRadius: '10px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: '18px',
            backgroundColor: confirmButtonType === 'danger' ? '#fff1f2' :
              confirmButtonType === 'warning' ? '#fffbeb' :
                confirmButtonType === 'success' ? '#f0fdf4' : '#eff6ff',
            color: confirmButtonType === 'danger' ? '#ef4444' :
              confirmButtonType === 'warning' ? '#f59e0b' :
                confirmButtonType === 'success' ? '#22c55e' : '#3b82f6'
          }}>
            {confirmButtonType === 'danger' ? <i className="fas fa-exclamation-triangle"></i> :
              confirmButtonType === 'warning' ? <i className="fas fa-exclamation-circle"></i> :
                confirmButtonType === 'success' ? <i className="fas fa-check-circle"></i> :
                  <i className="fas fa-question-circle"></i>}
          </div>
          <span style={{ fontSize: '1.125rem', fontWeight: 700, color: '#1e293b' }}>{title}</span>
        </div>
      }
      open={isOpen}
      onOk={onConfirm}
      onCancel={onCancel}
      okText={confirmText}
      cancelText={cancelText}
      width={440}
      centered
      maskClosable={false}
      destroyOnClose
      zIndex={10001} // ❗ 매우 중요: BulkModal(9999) 보다 높아야 함
      bodyStyle={{ padding: '0 0 8px 0' }}
      footer={showCancelButton ? undefined : [
        <button
          key="ok"
          onClick={onConfirm}
          className="ant-btn ant-btn-primary"
          style={{
            backgroundColor: confirmButtonType === 'danger' ? '#ef4444' :
              confirmButtonType === 'warning' ? '#f59e0b' : '#3b82f6',
            borderColor: confirmButtonType === 'danger' ? '#ef4444' :
              confirmButtonType === 'warning' ? '#f59e0b' : '#3b82f6',
            height: '40px',
            borderRadius: '8px',
            fontWeight: 600,
            padding: '0 24px'
          }}
        >
          {confirmText}
        </button>
      ]}
      modalRender={(modal) => (
        <div className="custom-confirm-modal">
          {modal}
          <style>{`
            .custom-confirm-modal .ant-modal-content {
              border-radius: 16px !important;
              overflow: hidden !important;
              box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25) !important;
              padding: 24px !important;
            }
            .custom-confirm-modal .ant-modal-header {
              border-bottom: none !important;
              margin-bottom: 0 !important;
              padding: 0 !important;
            }
            .custom-confirm-modal .ant-modal-footer {
              border-top: none !important;
              padding: 16px 0 0 0 !important;
              display: flex;
              justify-content: flex-end;
              gap: 8px;
            }
            .custom-confirm-modal .ant-btn {
              height: 40px !important;
              border-radius: 8px !important;
              font-weight: 600 !important;
              padding: 0 20px !important;
              font-size: 14px !important;
              transition: all 0.2s !important;
            }
            .custom-confirm-modal .ant-btn-default {
              border: 1px solid #e2e8f0 !important;
              color: #64748b !important;
            }
            .custom-confirm-modal .ant-btn-default:hover {
              background: #f8fafc !important;
              color: #1e293b !important;
              border-color: #cbd5e1 !important;
            }
            .custom-confirm-modal .ant-btn-primary {
              background: ${confirmButtonType === 'danger' ? '#ef4444' :
              confirmButtonType === 'warning' ? '#f59e0b' :
                confirmButtonType === 'success' ? '#22c55e' : '#3b82f6'} !important;
              border-color: ${confirmButtonType === 'danger' ? '#ef4444' :
              confirmButtonType === 'warning' ? '#f59e0b' :
                confirmButtonType === 'success' ? '#22c55e' : '#3b82f6'} !important;
            }
            .custom-confirm-modal .ant-btn-primary:hover {
              opacity: 0.9 !important;
              transform: translateY(-1px) !important;
              box-shadow: 0 4px 12px ${confirmButtonType === 'danger' ? 'rgba(239, 68, 68, 0.3)' :
              confirmButtonType === 'warning' ? 'rgba(245, 158, 11, 0.3)' :
                confirmButtonType === 'success' ? 'rgba(34, 197, 94, 0.3)' : 'rgba(59, 130, 246, 0.3)'} !important;
            }
          `}</style>
        </div>
      )}
    >
      <div style={{
        padding: '12px 0 0 48px',
        fontSize: '0.925rem',
        color: '#475569',
        lineHeight: '1.6',
        whiteSpace: 'pre-line'
      }}>
        {message}
      </div>
    </Modal>
  );
};

export default ConfirmDialog;