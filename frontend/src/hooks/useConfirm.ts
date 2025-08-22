import { useState, useCallback } from 'react';
export interface ConfirmOptions {
  title: string;
  message: string;
  confirmText?: string;
  cancelText?: string;
  confirmButtonType?: 'primary' | 'danger' | 'warning';
  showCancelButton?: boolean; // 추가
}

export interface ConfirmState extends ConfirmOptions {
  isOpen: boolean;
  onConfirm: () => void;
}

export const useConfirm = () => {
  const [confirmState, setConfirmState] = useState<ConfirmState | null>(null);

  const confirm = useCallback((options: ConfirmOptions): Promise<boolean> => {
    return new Promise((resolve) => {
      setConfirmState({
        ...options,
        isOpen: true,
        onConfirm: () => {
          setConfirmState(null);
          resolve(true);
        }
      });
    });
  }, []);

  const cancel = useCallback(() => {
    setConfirmState(null);
  }, []);

  return {
    confirmState,
    confirm,
    cancel
  };
};