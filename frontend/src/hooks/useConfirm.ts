import { useState, useCallback } from 'react';
export interface ConfirmOptions {
  title: string;
  message: string;
  confirmText?: string;
  cancelText?: string;
  confirmButtonType?: 'primary' | 'danger' | 'warning' | 'success';
  showCancelButton?: boolean; // 추가
}

export interface ConfirmState extends ConfirmOptions {
  isOpen: boolean;
  onConfirm: () => void;
}

export const useConfirm = () => {
  const [confirmState, setConfirmState] = useState<ConfirmState | null>(null);
  const [resolver, setResolver] = useState<{ resolve: (value: boolean) => void } | null>(null);

  const confirm = useCallback((options: ConfirmOptions): Promise<boolean> => {
    return new Promise((resolve) => {
      setResolver({ resolve });
      setConfirmState({
        ...options,
        isOpen: true,
        onConfirm: () => {
          setConfirmState(null);
          setResolver(null);
          resolve(true);
        }
      });
    });
  }, []);

  const cancel = useCallback(() => {
    if (resolver) {
      resolver.resolve(false);
    }
    setConfirmState(null);
    setResolver(null);
  }, [resolver]);

  return {
    confirmState,
    confirm,
    cancel
  };
};