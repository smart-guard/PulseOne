// =============================================================================
// 3. src/components/common/ConfirmProvider.tsx
// =============================================================================
import React, { createContext, useContext, ReactNode, useCallback } from 'react';
import ConfirmDialog from './ConfirmDialog';
import { useConfirm, ConfirmOptions } from '../../hooks/useConfirm';

interface ConfirmContextType {
  confirm: (options: ConfirmOptions) => Promise<boolean>;
}

const ConfirmContext = createContext<ConfirmContextType | undefined>(undefined);

export const useConfirmContext = () => {
  const context = useContext(ConfirmContext);
  if (!context) {
    throw new Error('useConfirmContext must be used within a ConfirmProvider');
  }
  return context;
};

interface ConfirmProviderProps {
  children: ReactNode;
}

export const ConfirmProvider: React.FC<ConfirmProviderProps> = ({ children }) => {
  const { confirmState, confirm, cancel } = useConfirm();
  
  const wrappedConfirm = useCallback((options: ConfirmOptions): Promise<boolean> => {
    return confirm(options);
  }, [confirm]);

  return (
    <ConfirmContext.Provider value={{ confirm: wrappedConfirm }}>
      {children}
      {confirmState && (
        <ConfirmDialog
          isOpen={confirmState.isOpen}
          title={confirmState.title}
          message={confirmState.message}
          confirmText={confirmState.confirmText}
          cancelText={confirmState.cancelText}
          confirmButtonType={confirmState.confirmButtonType}
          showCancelButton={confirmState.showCancelButton}
          onConfirm={confirmState.onConfirm}
          onCancel={cancel}
        />
      )}
    </ConfirmContext.Provider>
  );
};

