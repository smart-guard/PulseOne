import React, { useState, useEffect, useCallback } from 'react';
import { ProtocolApiService } from '../api/services/protocolApi';
import ProtocolEditor from '../components/modals/ProtocolModal/ProtocolEditor';
import { Pagination } from '../components/common/Pagination';
import ProtocolDetailModal from '../components/modals/ProtocolModal/ProtocolDetailModal';

// 프로토콜 데이터 타입 (실제 백엔드 응답과 매칭)
interface Protocol {
  id: number;
  protocol_type: string;
  display_name: string;
  description: string;
  category?: string;
  default_port?: number;
  uses_serial?: boolean;
  requires_broker?: boolean;
  supported_operations?: string[];
  supported_data_types?: string[];
  connection_params?: Record<string, any>;
  default_polling_interval?: number;
  default_timeout?: number;
  max_concurrent_connections?: number;
  vendor?: string;
  standard_reference?: string;
  is_enabled: boolean;
  is_deprecated: boolean;
  device_count?: number;
  enabled_count?: number;
  connected_count?: number;
  created_at?: string;
  updated_at?: string;
}

interface ProtocolStats {
  total_protocols: number;
  enabled_protocols: number;
  deprecated_protocols: number;
  categories: Array<{
    category: string;
    count: number;
    percentage: number;
  }>;
  usage_stats: Array<{
    protocol_type: string;
    display_name: string;
    device_count: number;
    enabled_devices: number;
    connected_devices: number;
  }>;
}

// 팝업 확인 다이얼로그 인터페이스
interface ConfirmDialogState {
  isOpen: boolean;
  title: string;
  message: string;
  confirmText: string;
  cancelText: string;
  onConfirm: () => void;
  onCancel: () => void;
  type: 'warning' | 'danger' | 'info';
}

const ProtocolManagement: React.FC = () => {
  const [protocols, setProtocols] = useState<Protocol[]>([]);
  const [stats, setStats] = useState<ProtocolStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [viewMode, setViewMode] = useState<'cards' | 'table'>('cards');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [searchTerm, setSearchTerm] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('all');
  const [processing, setProcessing] = useState<number | null>(null);
  const [showEditor, setShowEditor] = useState<{ mode: 'create' | 'edit' | 'view', protocolId?: number } | null>(null);

  // 페이징 상태
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(25);
  const [totalCount, setTotalCount] = useState(0);

  // 상세보기 모달 상태
  const [selectedProtocol, setSelectedProtocol] = useState<Protocol | null>(null);
  const [showDetailModal, setShowDetailModal] = useState(false);

  // 팝업 확인 다이얼로그 상태
  const [confirmDialog, setConfirmDialog] = useState<ConfirmDialogState>({
    isOpen: false,
    title: '',
    message: '',
    confirmText: '확인',
    cancelText: '취소',
    onConfirm: () => {},
    onCancel: () => {},
    type: 'info'
  });

  // 성공 메시지 상태
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  // 프로토콜 목록 로드 (페이징 지원)
  const loadProtocols = useCallback(async () => {
    try {
      setLoading(true);
      setError(null);
      
      console.log('프로토콜 목록 로드 시작...', { currentPage, pageSize });
      
      const filters = {
        category: selectedCategory !== 'all' ? selectedCategory : undefined,
        enabled: statusFilter === 'enabled' ? 'true' : statusFilter === 'disabled' ? 'false' : undefined,
        search: searchTerm.trim() || undefined,
        page: currentPage,
        limit: pageSize
      };

      const response = await ProtocolApiService.getProtocols(filters);
      
      if (response.success) {
        setProtocols(response.data || []);
        
        // 백엔드 응답에서 페이징 정보 추출
        if (response.pagination) {
          setTotalCount(response.pagination.total_count || response.pagination.total_items || 0);
        } else if (response.total_count !== undefined) {
          setTotalCount(response.total_count);
        } else if (response.meta?.total) {
          setTotalCount(response.meta.total);
        } else {
          // 페이징 정보가 없는 경우, 현재 데이터 길이 사용
          setTotalCount((response.data || []).length);
        }
        
        console.log(`프로토콜 ${response.data?.length || 0}개 로드 완료 (총 ${totalCount}개)`);
      } else {
        throw new Error(response.message || '프로토콜 목록 조회 실패');
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : '프로토콜 로드 실패';
      console.error('프로토콜 로드 에러:', err);
      setError(errorMessage);
    } finally {
      setLoading(false);
    }
  }, [selectedCategory, statusFilter, searchTerm, currentPage, pageSize]);

  // 프로토콜 통계 로드
  const loadStats = useCallback(async () => {
    try {
      const response = await ProtocolApiService.getProtocolStatistics();
      if (response.success) {
        setStats(response.data);
        console.log('프로토콜 통계 로드 완료');
      }
    } catch (err) {
      console.warn('프로토콜 통계 로드 실패:', err);
      // 통계는 필수가 아니므로 에러로 처리하지 않음
    }
  }, []);

  // 초기 데이터 로드
  useEffect(() => {
    loadProtocols();
    loadStats();
  }, [loadProtocols, loadStats]);

  // 필터나 검색 변경 시 첫 페이지로 이동
  useEffect(() => {
    if (currentPage !== 1) {
      setCurrentPage(1);
    }
  }, [selectedCategory, statusFilter, searchTerm]);

  // 성공 메시지 자동 제거
  useEffect(() => {
    if (successMessage) {
      const timer = setTimeout(() => setSuccessMessage(null), 3000);
      return () => clearTimeout(timer);
    }
  }, [successMessage]);

  // 팝업 확인 후 프로토콜 액션 실행
  const executeProtocolAction = async (action: string, protocolId: number) => {
    try {
      setProcessing(protocolId);
      setError(null);

      console.log(`프로토콜 ${action} 실행:`, protocolId);
      const protocol = protocols.find(p => p.id === protocolId);

      let response;
      switch (action) {
        case 'enable':
          response = await ProtocolApiService.updateProtocol(protocolId, { is_enabled: true });
          if (response?.success) {
            setSuccessMessage(`프로토콜 "${protocol?.display_name}"이(가) 활성화되었습니다.`);
          }
          break;
        case 'disable':
          response = await ProtocolApiService.updateProtocol(protocolId, { is_enabled: false });
          if (response?.success) {
            setSuccessMessage(`프로토콜 "${protocol?.display_name}"이(가) 비활성화되었습니다.`);
          }
          break;
        case 'test':
          // 연결 테스트의 경우 기본 파라미터로 테스트
          if (protocol) {
            const testParams: Record<string, any> = {};
            
            // 프로토콜별 기본 테스트 파라미터 설정
            if (protocol.protocol_type === 'MODBUS_TCP') {
              testParams.host = '127.0.0.1';
              testParams.port = protocol.default_port || 502;
              testParams.slave_id = 1;
            } else if (protocol.protocol_type === 'MQTT') {
              testParams.broker_url = `mqtt://localhost:${protocol.default_port || 1883}`;
              testParams.client_id = 'test_client';
            } else if (protocol.protocol_type === 'BACNET') {
              testParams.device_instance = 1234;
              testParams.host = '127.0.0.1';
              testParams.port = protocol.default_port || 47808;
            }
            
            response = await ProtocolApiService.testProtocolConnection(protocolId, testParams);
            
            if (response.success && response.data) {
              const result = response.data;
              const message = result.test_successful 
                ? `연결 테스트 성공! 응답시간: ${result.response_time_ms}ms`
                : `연결 테스트 실패: ${result.error_message}`;
              setSuccessMessage(message);
            }
          }
          break;
        case 'edit':
          console.log('편집 모달 열기:', protocolId);
          setShowEditor({ mode: 'edit', protocolId });
          return;
        case 'view':
          // 상세보기 모달 열기
          console.log('상세보기 모달 열기 시작:', protocolId);
          const protocolToView = protocols.find(p => p.id === protocolId);
          if (protocolToView) {
            setSelectedProtocol(protocolToView);
            setShowDetailModal(true);
          }
          return;
        default:
          throw new Error(`알 수 없는 액션: ${action}`);
      }

      if (response && response.success) {
        console.log(`프로토콜 ${action} 완료`);
        await loadProtocols(); // 목록 새로고침
      } else if (response) {
        throw new Error(response.message || `프로토콜 ${action} 실패`);
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : `프로토콜 ${action} 실패`;
      console.error(`프로토콜 ${action} 에러:`, err);
      setError(errorMessage);
    } finally {
      setProcessing(null);
    }
  };

  // 프로토콜 액션 처리 (팝업 확인 먼저)
  const handleProtocolAction = async (action: string, protocolId: number) => {
    const protocol = protocols.find(p => p.id === protocolId);
    if (!protocol) return;

    // 즉시 실행되는 액션들
    if (['view', 'test'].includes(action)) {
      return executeProtocolAction(action, protocolId);
    }

    // 확인이 필요한 액션들
    let dialogConfig: Partial<ConfirmDialogState> = {};
    
    switch (action) {
      case 'enable':
        dialogConfig = {
          title: '프로토콜 활성화',
          message: `프로토콜 "${protocol.display_name}"을(를) 활성화하시겠습니까?\n\n활성화된 프로토콜은 디바이스에서 사용할 수 있게 됩니다.`,
          confirmText: '활성화',
          type: 'info',
          onConfirm: () => {
            setConfirmDialog(prev => ({ ...prev, isOpen: false }));
            executeProtocolAction(action, protocolId);
          }
        };
        break;
      case 'disable':
        dialogConfig = {
          title: '프로토콜 비활성화',
          message: `프로토콜 "${protocol.display_name}"을(를) 비활성화하시겠습니까?\n\n비활성화하면 해당 프로토콜을 사용하는 디바이스들이 영향을 받을 수 있습니다.\n현재 ${protocol.device_count || 0}개의 디바이스가 이 프로토콜을 사용 중입니다.`,
          confirmText: '비활성화',
          type: 'warning',
          onConfirm: () => {
            setConfirmDialog(prev => ({ ...prev, isOpen: false }));
            executeProtocolAction(action, protocolId);
          }
        };
        break;
      case 'edit':
        dialogConfig = {
          title: '프로토콜 편집',
          message: `프로토콜 "${protocol.display_name}"을(를) 편집하시겠습니까?\n\n편집 모드로 진입하여 프로토콜 설정을 수정할 수 있습니다.`,
          confirmText: '편집하기',
          type: 'info',
          onConfirm: () => {
            setConfirmDialog(prev => ({ ...prev, isOpen: false }));
            executeProtocolAction(action, protocolId);
          }
        };
        break;
      default:
        return;
    }

    setConfirmDialog({
      ...confirmDialog,
      ...dialogConfig,
      isOpen: true,
      onCancel: () => setConfirmDialog(prev => ({ ...prev, isOpen: false }))
    });
  };

  // 필터 초기화
  const resetFilters = () => {
    setSelectedCategory('all');
    setStatusFilter('all');
    setSearchTerm('');
    setCurrentPage(1);
  };

  // 페이징 핸들러
  const handlePageChange = (page: number, newPageSize: number) => {
    console.log('페이지 변경:', { page, newPageSize });
    setCurrentPage(page);
    if (newPageSize !== pageSize) {
      setPageSize(newPageSize);
      setCurrentPage(1);
    }
  };

  // 카테고리 아이콘
  const getCategoryIcon = (category: string) => {
    const icons = {
      'industrial': '🏭',
      'iot': '🌐',
      'building_automation': '🏢',
      'network': '🔗',
      'web': '🌍'
    };
    return icons[category] || '📡';
  };

  // 카테고리 색상
  const getCategoryColor = (category: string) => {
    const colors = {
      'industrial': '#3b82f6',
      'iot': '#10b981',
      'building_automation': '#f59e0b',
      'network': '#8b5cf6',
      'web': '#ef4444'
    };
    return colors[category] || '#6b7280';
  };

  // 통계 계산 (실제 데이터 기반 - API 없을 때 대체)
  const getCurrentStats = () => {
    // 백엔드 API 통계가 로드되었으면 사용
    if (stats && stats.total_protocols !== undefined) {
        return {
        total_protocols: stats.total_protocols,
        enabled_protocols: stats.enabled_protocols,
        deprecated_protocols: stats.deprecated_protocols,
        total_devices: stats.usage_stats?.reduce((sum, p) => sum + p.device_count, 0) || 0
        };
    }
    
    // API 로드 전이나 실패 시 현재 페이지 데이터로 임시 표시
    return {
        total_protocols: protocols.length,
        enabled_protocols: protocols.filter(p => p.is_enabled).length,
        deprecated_protocols: protocols.filter(p => p.is_deprecated).length,
        total_devices: protocols.reduce((sum, p) => sum + (p.device_count || 0), 0)
    };
    };

    const currentStats = getCurrentStats();

  // Editor 핸들러 (수정 버튼에서 확인 팝업 추가)
  const handleEditorSave = async (protocol: Protocol) => {
    await loadProtocols(); // 목록 새로고침
    setShowEditor(null); // Editor 닫기
    
    // 성공 메시지 표시
    if (showEditor?.mode === 'create') {
      setSuccessMessage(`프로토콜 "${protocol.display_name}"이(가) 성공적으로 생성되었습니다.`);
    } else {
      setSuccessMessage(`프로토콜 "${protocol.display_name}"이(가) 성공적으로 수정되었습니다.`);
    }
  };

  const handleEditorCancel = () => {
    setShowEditor(null); // Editor 닫기
  };

  // 확인 다이얼로그 컴포넌트
  const ConfirmDialog: React.FC<{ config: ConfirmDialogState }> = ({ config }) => {
    if (!config.isOpen) return null;

    const getDialogColor = (type: string) => {
      switch (type) {
        case 'danger': return '#ef4444';
        case 'warning': return '#f59e0b';
        case 'info': 
        default: return '#3b82f6';
      }
    };

    return (
      <div style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 10000
      }}>
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          minWidth: '400px',
          maxWidth: '500px',
          boxShadow: '0 25px 50px rgba(0, 0, 0, 0.25)'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
            marginBottom: '16px'
          }}>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '50%',
              backgroundColor: `${getDialogColor(config.type)}20`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>
              {config.type === 'warning' ? '⚠️' : config.type === 'danger' ? '🚨' : 'ℹ️'}
            </div>
            <h3 style={{
              margin: 0,
              fontSize: '18px',
              fontWeight: '600',
              color: '#1e293b'
            }}>
              {config.title}
            </h3>
          </div>
          
          <p style={{
            margin: 0,
            marginBottom: '24px',
            fontSize: '14px',
            color: '#64748b',
            lineHeight: '1.5',
            whiteSpace: 'pre-line'
          }}>
            {config.message}
          </p>

          <div style={{
            display: 'flex',
            justifyContent: 'flex-end',
            gap: '12px'
          }}>
            <button
              onClick={config.onCancel}
              style={{
                padding: '10px 20px',
                backgroundColor: '#f3f4f6',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                fontSize: '14px',
                fontWeight: '500',
                color: '#374151',
                cursor: 'pointer'
              }}
            >
              {config.cancelText}
            </button>
            <button
              onClick={config.onConfirm}
              style={{
                padding: '10px 20px',
                backgroundColor: getDialogColor(config.type),
                color: 'white',
                border: 'none',
                borderRadius: '6px',
                fontSize: '14px',
                fontWeight: '500',
                cursor: 'pointer'
              }}
            >
              {config.confirmText}
            </button>
          </div>
        </div>
      </div>
    );
  };

  // 성공 메시지 컴포넌트
  const SuccessMessage: React.FC<{ message: string }> = ({ message }) => (
    <div style={{
      position: 'fixed',
      top: '20px',
      right: '20px',
      backgroundColor: '#dcfce7',
      border: '1px solid #16a34a',
      borderRadius: '8px',
      padding: '12px 16px',
      color: '#166534',
      fontSize: '14px',
      fontWeight: '500',
      zIndex: 9999,
      boxShadow: '0 4px 6px rgba(0, 0, 0, 0.1)',
      display: 'flex',
      alignItems: 'center',
      gap: '8px'
    }}>
      ✅ {message}
    </div>
  );

  if (loading) {
    return (
      <div style={{ 
        display: 'flex', 
        justifyContent: 'center', 
        alignItems: 'center', 
        height: '400px',
        flexDirection: 'column',
        gap: '16px'
      }}>
        <div>🔄 프로토콜 데이터를 불러오는 중...</div>
        <div style={{ fontSize: '14px', color: '#64748b' }}>
          백엔드 API에서 데이터를 가져오고 있습니다.
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{ 
        display: 'flex', 
        justifyContent: 'center', 
        alignItems: 'center', 
        height: '400px',
        flexDirection: 'column',
        gap: '16px',
        color: '#dc2626'
      }}>
        <div>❌ 오류가 발생했습니다</div>
        <div style={{ fontSize: '14px' }}>{error}</div>
        <button 
          onClick={() => window.location.reload()}
          style={{
            padding: '8px 16px',
            backgroundColor: '#dc2626',
            color: 'white',
            border: 'none',
            borderRadius: '6px',
            cursor: 'pointer'
          }}
        >
          페이지 새로고침
        </button>
      </div>
    );
  }

  return (
    <div style={{ 
      width: '100%', 
      maxWidth: 'none', 
      padding: '24px',
      backgroundColor: '#f8fafc' 
    }}>
      {/* 성공 메시지 */}
      {successMessage && <SuccessMessage message={successMessage} />}

      {/* 확인 다이얼로그 */}
      <ConfirmDialog config={confirmDialog} />

      {/* 헤더 */}
      <div style={{ 
        display: 'flex', 
        justifyContent: 'space-between', 
        alignItems: 'center', 
        marginBottom: '32px' 
      }}>
        <div>
          <h1 style={{ 
            fontSize: '28px', 
            fontWeight: '700', 
            color: '#1e293b', 
            margin: 0, 
            marginBottom: '8px' 
          }}>
            프로토콜 관리
          </h1>
          <p style={{ 
            color: '#64748b', 
            margin: 0,
            fontSize: '16px'
          }}>
            통신 프로토콜의 조회, 편집, 등록을 관리합니다
          </p>
        </div>
        <button 
          onClick={() => setShowEditor({ mode: 'create' })}
          style={{
            backgroundColor: '#3b82f6',
            color: 'white',
            border: 'none',
            borderRadius: '8px',
            padding: '12px 24px',
            fontSize: '14px',
            fontWeight: '500',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '8px'
          }}
        >
          ➕ 새 프로토콜 등록
        </button>
      </div>

      {/* 통계 카드들 */}
      <div style={{ 
        display: 'grid', 
        gridTemplateColumns: 'repeat(4, 1fr)', 
        gap: '24px', 
        marginBottom: '32px' 
      }}>
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>전체 프로토콜</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#dbeafe',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>📡</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#1e293b' 
          }}>
            {currentStats.total_protocols || 0}
          </div>
        </div>

        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>활성화됨</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#dcfce7',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>✅</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#16a34a' 
          }}>
            {currentStats.enabled_protocols || 0}
          </div>
        </div>

        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>지원 중단</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#fef3c7',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>⚠️</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#d97706' 
          }}>
            {currentStats.deprecated_protocols || 0}
          </div>
        </div>

        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0'
        }}>
          <div style={{ 
            display: 'flex', 
            alignItems: 'center', 
            justifyContent: 'space-between',
            marginBottom: '12px'
          }}>
            <h3 style={{ 
              margin: 0, 
              color: '#64748b', 
              fontSize: '14px',
              fontWeight: '500'
            }}>사용중인 디바이스</h3>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '8px',
              backgroundColor: '#e0f2fe',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>🌐</div>
          </div>
          <div style={{ 
            fontSize: '32px', 
            fontWeight: '700', 
            color: '#0284c7' 
          }}>
            {protocols.reduce((sum, p) => sum + (p.device_count || 0), 0)}
          </div>
        </div>
      </div>

      {/* 필터 및 검색 */}
      <div style={{
        backgroundColor: 'white',
        borderRadius: '12px',
        padding: '20px',
        marginBottom: '24px',
        boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
        border: '1px solid #e2e8f0',
        display: 'flex',
        alignItems: 'center',
        gap: '16px',
        flexWrap: 'wrap'
      }}>
        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '4px', 
            fontSize: '14px',
            fontWeight: '500',
            color: '#374151' 
          }}>
            검색
          </label>
          <input
            type="text"
            placeholder="프로토콜명, 타입, 설명 검색..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            style={{
              padding: '8px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              fontSize: '14px',
              width: '250px'
            }}
          />
        </div>
        
        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '4px', 
            fontSize: '14px',
            fontWeight: '500',
            color: '#374151' 
          }}>
            카테고리
          </label>
          <select
            value={selectedCategory}
            onChange={(e) => setSelectedCategory(e.target.value)}
            style={{
              padding: '8px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              fontSize: '14px',
              width: '180px'
            }}
          >
            <option value="all">전체 카테고리</option>
            <option value="industrial">산업용</option>
            <option value="iot">IoT</option>
            <option value="building_automation">빌딩 자동화</option>
            <option value="web">웹</option>
            <option value="network">네트워크</option>
          </select>
        </div>

        <div>
          <label style={{ 
            display: 'block', 
            marginBottom: '4px', 
            fontSize: '14px',
            fontWeight: '500',
            color: '#374151' 
          }}>
            상태
          </label>
          <select 
            value={statusFilter}
            onChange={(e) => setStatusFilter(e.target.value)}
            style={{
              padding: '8px 12px',
              border: '1px solid #d1d5db',
              borderRadius: '6px',
              fontSize: '14px',
              width: '120px'
            }}
          >
            <option value="all">전체 상태</option>
            <option value="enabled">활성</option>
            <option value="disabled">비활성</option>
          </select>
        </div>

        <div style={{ marginLeft: 'auto', display: 'flex', gap: '8px' }}>
          <button
            onClick={() => setViewMode('cards')}
            style={{
              padding: '8px 12px',
              border: viewMode === 'cards' ? '1px solid #3b82f6' : '1px solid #d1d5db',
              backgroundColor: viewMode === 'cards' ? '#dbeafe' : 'white',
              color: viewMode === 'cards' ? '#3b82f6' : '#6b7280',
              borderRadius: '6px',
              fontSize: '14px',
              cursor: 'pointer'
            }}
          >
            📱 카드형
          </button>
          <button
            onClick={() => setViewMode('table')}
            style={{
              padding: '8px 12px',
              border: viewMode === 'table' ? '1px solid #3b82f6' : '1px solid #d1d5db',
              backgroundColor: viewMode === 'table' ? '#dbeafe' : 'white',
              color: viewMode === 'table' ? '#3b82f6' : '#6b7280',
              borderRadius: '6px',
              fontSize: '14px',
              cursor: 'pointer'
            }}
          >
            📋 테이블형
          </button>
        </div>

        <button 
          onClick={resetFilters}
          style={{
            padding: '8px 16px',
            backgroundColor: '#f3f4f6',
            border: '1px solid #d1d5db',
            borderRadius: '6px',
            fontSize: '14px',
            cursor: 'pointer'
          }}
        >
          🔄 필터 초기화
        </button>
      </div>

      {/* 페이징 정보 표시 */}
      <div style={{
        backgroundColor: 'white',
        borderRadius: '8px',
        padding: '12px 16px',
        marginBottom: '16px',
        boxShadow: '0 1px 2px 0 rgb(0 0 0 / 0.05)',
        border: '1px solid #e2e8f0',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        fontSize: '14px',
        color: '#64748b'
      }}>
        <div>
          {protocols.length > 0 ? (
            <span>
              {((currentPage - 1) * pageSize) + 1}-{Math.min(currentPage * pageSize, totalCount)} / {totalCount}개 표시
            </span>
          ) : (
            <span>검색 결과 없음</span>
          )}
        </div>
        <div>
          페이지 {currentPage} / {Math.ceil(totalCount / pageSize) || 1}
        </div>
      </div>

      {/* 프로토콜 목록 */}
      {protocols.length === 0 ? (
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '48px',
          textAlign: 'center',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0',
          marginBottom: '24px'
        }}>
          <div style={{ fontSize: '48px', marginBottom: '16px' }}>🔍</div>
          <h3 style={{ margin: 0, marginBottom: '8px', color: '#374151' }}>
            조건에 맞는 프로토콜이 없습니다
          </h3>
          <p style={{ margin: 0, color: '#6b7280' }}>
            다른 검색어나 필터를 시도해보세요
          </p>
        </div>
      ) : viewMode === 'cards' ? (
        <div style={{ 
          display: 'grid', 
          gridTemplateColumns: 'repeat(auto-fill, minmax(400px, 1fr))', 
          gap: '20px',
          marginBottom: '24px'
        }}>
          {protocols.map(protocol => (
            <div 
              key={protocol.id}
              style={{
                backgroundColor: 'white',
                borderRadius: '12px',
                padding: '24px',
                boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
                border: '1px solid #e2e8f0',
                transition: 'all 0.2s ease',
                cursor: 'pointer',
                opacity: processing === protocol.id ? 0.6 : 1
              }}
              onMouseEnter={(e) => {
                if (processing !== protocol.id) {
                  e.currentTarget.style.transform = 'translateY(-2px)';
                  e.currentTarget.style.boxShadow = '0 4px 6px -1px rgb(0 0 0 / 0.1)';
                }
              }}
              onMouseLeave={(e) => {
                if (processing !== protocol.id) {
                  e.currentTarget.style.transform = 'translateY(0)';
                  e.currentTarget.style.boxShadow = '0 1px 3px 0 rgb(0 0 0 / 0.1)';
                }
              }}
            >
              {/* 카드 헤더 */}
              <div style={{ 
                display: 'flex', 
                justifyContent: 'space-between', 
                alignItems: 'flex-start',
                marginBottom: '16px'
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <div style={{
                    width: '48px',
                    height: '48px',
                    borderRadius: '8px',
                    backgroundColor: `${getCategoryColor(protocol.category || 'network')}20`,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '20px'
                  }}>
                    {getCategoryIcon(protocol.category || 'network')}
                  </div>
                  <div>
                    <h3 style={{ 
                      margin: 0, 
                      fontSize: '18px', 
                      fontWeight: '600',
                      color: '#1e293b'
                    }}>
                      {protocol.display_name}
                    </h3>
                    <div style={{ 
                      fontSize: '12px',
                      color: '#64748b',
                      marginTop: '2px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px'
                    }}>
                      <span>{protocol.protocol_type}</span>
                      <span>•</span>
                      <span style={{
                        padding: '2px 6px',
                        backgroundColor: getCategoryColor(protocol.category || 'network'),
                        color: 'white',
                        borderRadius: '4px',
                        fontSize: '10px',
                        fontWeight: '500'
                      }}>
                        {protocol.category || 'network'}
                      </span>
                    </div>
                  </div>
                </div>
                
                <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  {protocol.is_enabled ? (
                    <span style={{
                      width: '8px',
                      height: '8px',
                      backgroundColor: '#16a34a',
                      borderRadius: '50%',
                      display: 'inline-block'
                    }}></span>
                  ) : (
                    <span style={{
                      width: '8px',
                      height: '8px',
                      backgroundColor: '#ef4444',
                      borderRadius: '50%',
                      display: 'inline-block'
                    }}></span>
                  )}
                  {protocol.is_deprecated && (
                    <span style={{
                      fontSize: '12px',
                      backgroundColor: '#fef3c7',
                      color: '#92400e',
                      padding: '2px 6px',
                      borderRadius: '4px',
                      fontWeight: '500'
                    }}>
                      지원중단
                    </span>
                  )}
                </div>
              </div>

              {/* 설명 */}
              <p style={{ 
                margin: 0, 
                marginBottom: '16px',
                fontSize: '14px', 
                color: '#64748b',
                lineHeight: '1.5'
              }}>
                {protocol.description}
              </p>

              {/* 포트 정보 */}
              {protocol.default_port && (
                <div style={{ 
                  marginBottom: '16px',
                  fontSize: '14px',
                  color: '#374151'
                }}>
                  <span style={{ fontWeight: '500' }}>기본 포트:</span> {protocol.default_port}
                </div>
              )}

              {/* 통계 */}
              <div style={{ 
                display: 'grid', 
                gridTemplateColumns: 'repeat(3, 1fr)', 
                gap: '12px',
                marginBottom: '16px'
              }}>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ 
                    fontSize: '18px', 
                    fontWeight: '700',
                    color: '#1e293b'
                  }}>
                    {protocol.device_count || 0}
                  </div>
                  <div style={{ 
                    fontSize: '12px', 
                    color: '#64748b' 
                  }}>
                    총 디바이스
                  </div>
                </div>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ 
                    fontSize: '18px', 
                    fontWeight: '700',
                    color: '#16a34a'
                  }}>
                    {protocol.enabled_count || 0}
                  </div>
                  <div style={{ 
                    fontSize: '12px', 
                    color: '#64748b' 
                  }}>
                    활성 디바이스
                  </div>
                </div>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ 
                    fontSize: '18px', 
                    fontWeight: '700',
                    color: '#0284c7'
                  }}>
                    {protocol.connected_count || 0}
                  </div>
                  <div style={{ 
                    fontSize: '12px', 
                    color: '#64748b' 
                  }}>
                    연결 중
                  </div>
                </div>
              </div>

              {/* 액션 버튼 */}
              <div style={{ 
                display: 'flex', 
                gap: '8px',
                paddingTop: '16px',
                borderTop: '1px solid #f1f5f9'
              }}>
                <button 
                  onClick={() => handleProtocolAction('view', protocol.id)}
                  disabled={processing === protocol.id}
                  style={{
                    flex: 1,
                    padding: '8px 12px',
                    backgroundColor: '#f8fafc',
                    border: '1px solid #e2e8f0',
                    borderRadius: '6px',
                    fontSize: '14px',
                    cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                  }}
                >
                  👁️ 상세보기
                </button>
                <button 
                  onClick={() => handleProtocolAction('edit', protocol.id)}
                  disabled={processing === protocol.id}
                  style={{
                    flex: 1,
                    padding: '8px 12px',
                    backgroundColor: '#f8fafc',
                    border: '1px solid #e2e8f0',
                    borderRadius: '6px',
                    fontSize: '14px',
                    cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                  }}
                >
                  ✏️ 편집
                </button>
                {protocol.is_enabled ? (
                  <button 
                    onClick={() => handleProtocolAction('disable', protocol.id)}
                    disabled={processing === protocol.id}
                    style={{
                      padding: '8px 12px',
                      backgroundColor: '#fef3c7',
                      border: '1px solid #f59e0b',
                      borderRadius: '6px',
                      fontSize: '14px',
                      cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                    }}
                  >
                    {processing === protocol.id ? '⏳' : '⏸️'}
                  </button>
                ) : (
                  <button 
                    onClick={() => handleProtocolAction('enable', protocol.id)}
                    disabled={processing === protocol.id}
                    style={{
                      padding: '8px 12px',
                      backgroundColor: '#dcfce7',
                      border: '1px solid #16a34a',
                      borderRadius: '6px',
                      fontSize: '14px',
                      cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                    }}
                  >
                    {processing === protocol.id ? '⏳' : '▶️'}
                  </button>
                )}
              </div>
            </div>
          ))}
        </div>
      ) : (
        /* 테이블형 뷰 */
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0',
          overflow: 'hidden',
          marginBottom: '24px'
        }}>
          <table style={{ width: '100%', borderCollapse: 'collapse' }}>
            <thead>
              <tr style={{ backgroundColor: '#f8fafc' }}>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'left', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  프로토콜
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'left', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  카테고리
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  포트
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  디바이스 수
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  상태
                </th>
                <th style={{ 
                  padding: '16px', 
                  textAlign: 'center', 
                  borderBottom: '1px solid #e2e8f0',
                  fontSize: '14px',
                  fontWeight: '600',
                  color: '#374151'
                }}>
                  액션
                </th>
              </tr>
            </thead>
            <tbody>
              {protocols.map((protocol, index) => (
                <tr 
                  key={protocol.id}
                  style={{ 
                    backgroundColor: index % 2 === 0 ? 'white' : '#f8fafc',
                    transition: 'background-color 0.2s',
                    opacity: processing === protocol.id ? 0.6 : 1
                  }}
                  onMouseEnter={(e) => {
                    if (processing !== protocol.id) {
                      e.currentTarget.style.backgroundColor = '#f1f5f9';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (processing !== protocol.id) {
                      e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'white' : '#f8fafc';
                    }
                  }}
                >
                  <td style={{ padding: '16px', borderBottom: '1px solid #f1f5f9' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                      <div style={{
                        width: '32px',
                        height: '32px',
                        borderRadius: '6px',
                        backgroundColor: `${getCategoryColor(protocol.category || 'network')}20`,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        fontSize: '16px'
                      }}>
                        {getCategoryIcon(protocol.category || 'network')}
                      </div>
                      <div>
                        <div style={{ 
                          fontWeight: '600', 
                          color: '#1e293b',
                          fontSize: '14px'
                        }}>
                          {protocol.display_name}
                        </div>
                        <div style={{ 
                          color: '#64748b',
                          fontSize: '12px',
                          marginTop: '2px'
                        }}>
                          {protocol.protocol_type}
                        </div>
                      </div>
                    </div>
                  </td>
                  <td style={{ padding: '16px', borderBottom: '1px solid #f1f5f9' }}>
                    <span style={{
                      padding: '4px 8px',
                      backgroundColor: getCategoryColor(protocol.category || 'network'),
                      color: 'white',
                      borderRadius: '6px',
                      fontSize: '12px',
                      fontWeight: '500'
                    }}>
                      {protocol.category || 'network'}
                    </span>
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center',
                    color: '#374151',
                    fontSize: '14px'
                  }}>
                    {protocol.default_port || '-'}
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center'
                  }}>
                    <div style={{ fontSize: '16px', fontWeight: '600', color: '#1e293b' }}>
                      {protocol.device_count || 0}
                    </div>
                    <div style={{ fontSize: '12px', color: '#64748b' }}>
                      {protocol.connected_count || 0} 연결중
                    </div>
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center'
                  }}>
                    <span style={{
                      padding: '4px 8px',
                      backgroundColor: protocol.is_enabled ? '#dcfce7' : '#fee2e2',
                      color: protocol.is_enabled ? '#166534' : '#991b1b',
                      borderRadius: '6px',
                      fontSize: '12px',
                      fontWeight: '500'
                    }}>
                      {protocol.is_enabled ? '활성' : '비활성'}
                    </span>
                  </td>
                  <td style={{ 
                    padding: '16px', 
                    borderBottom: '1px solid #f1f5f9',
                    textAlign: 'center'
                  }}>
                    <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
                      <button 
                        onClick={() => handleProtocolAction('view', protocol.id)}
                        disabled={processing === protocol.id}
                        style={{
                          padding: '6px 8px',
                          backgroundColor: '#f8fafc',
                          border: '1px solid #e2e8f0',
                          borderRadius: '4px',
                          fontSize: '12px',
                          cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                        }}
                      >
                        📋
                      </button>
                      <button 
                        onClick={() => handleProtocolAction('edit', protocol.id)}
                        disabled={processing === protocol.id}
                        style={{
                          padding: '6px 8px',
                          backgroundColor: '#f8fafc',
                          border: '1px solid #e2e8f0',
                          borderRadius: '4px',
                          fontSize: '12px',
                          cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                        }}
                      >
                        ✏️
                      </button>
                      {protocol.is_enabled ? (
                        <button 
                          onClick={() => handleProtocolAction('disable', protocol.id)}
                          disabled={processing === protocol.id}
                          style={{
                            padding: '6px 8px',
                            backgroundColor: '#fef3c7',
                            border: '1px solid #f59e0b',
                            borderRadius: '4px',
                            fontSize: '12px',
                            cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                          }}
                        >
                          {processing === protocol.id ? '⏳' : '⏸️'}
                        </button>
                      ) : (
                        <button 
                          onClick={() => handleProtocolAction('enable', protocol.id)}
                          disabled={processing === protocol.id}
                          style={{
                            padding: '6px 8px',
                            backgroundColor: '#dcfce7',
                            border: '1px solid #16a34a',
                            borderRadius: '4px',
                            fontSize: '12px',
                            cursor: processing === protocol.id ? 'not-allowed' : 'pointer'
                          }}
                        >
                          {processing === protocol.id ? '⏳' : '▶️'}
                        </button>
                      )}
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* 페이징 컴포넌트 */}
      {totalCount > 0 && (
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '20px',
          boxShadow: '0 1px 3px 0 rgb(0 0 0 / 0.1)',
          border: '1px solid #e2e8f0',
          display: 'flex',
          justifyContent: 'center'
        }}>
          <Pagination
            current={currentPage}
            total={totalCount}
            pageSize={pageSize}
            pageSizeOptions={[10, 25, 50, 100]}
            showSizeChanger={true}
            showTotal={true}
            onChange={handlePageChange}
            onShowSizeChange={handlePageChange}
          />
        </div>
      )}

      {/* 프로토콜 상세보기 모달 */}
      {showDetailModal && selectedProtocol && (
        <ProtocolDetailModal
          protocol={selectedProtocol}
          isOpen={showDetailModal}
          onClose={() => {
            console.log('상세보기 모달 닫기');
            setShowDetailModal(false);
            setSelectedProtocol(null);
          }}
        />
      )}

      {/* 프로토콜 편집/생성 모달 */}
      {showEditor && (
        <ProtocolEditor
          protocolId={showEditor.protocolId}
          mode={showEditor.mode}
          isOpen={true}
          onSave={handleEditorSave}
          onCancel={handleEditorCancel}
        />
      )}
    </div>
  );
};

export default ProtocolManagement;