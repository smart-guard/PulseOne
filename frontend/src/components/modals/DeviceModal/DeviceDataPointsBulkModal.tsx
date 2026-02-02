// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx
// 📊 대량 데이터 포인트 등록 모달 (엑셀 그리드 방식)
// ============================================================================

import { createPortal } from 'react-dom';
import React, { useState, useEffect, useCallback, useRef } from 'react';
import { DataPoint } from '../../../api/services/dataApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import { TemplateApiService } from '../../../api/services/templateApi';
import { DeviceTemplate } from '../../../types/manufacturing';

interface BulkDataPoint extends Partial<DataPoint> {
    isValid: boolean;
    errors: string[];
    tempId: number;
}

interface DeviceDataPointsBulkModalProps {
    deviceId: number;
    isOpen: boolean;
    onClose: () => void;
    onSave: (points: Partial<DataPoint>[]) => Promise<void>;
    existingAddresses: string[];
    protocolType?: string;
}

const DeviceDataPointsBulkModal: React.FC<DeviceDataPointsBulkModalProps> = ({
    deviceId,
    isOpen,
    onClose,
    onSave,
    existingAddresses,
    protocolType
}) => {
    const { confirm } = useConfirmContext();
    // 초기 빈 행 생성
    const createEmptyRow = (): BulkDataPoint => ({
        tempId: Math.random(),
        isValid: true,
        errors: [],
        device_id: deviceId,
        is_enabled: true,
        name: '',
        address: '',
        data_type: 'number',
        access_mode: 'read',
        description: '',
        unit: '',
        scaling_factor: 1,
        scaling_offset: 0
    });

    const STORAGE_KEY = `bulk_draft_device_${deviceId}`;

    const [points, setPoints] = useState<BulkDataPoint[]>([]);
    const [history, setHistory] = useState<BulkDataPoint[][]>([]);
    const [historyIndex, setHistoryIndex] = useState(-1);
    const [isProcessing, setIsProcessing] = useState(false);
    const [mounted, setMounted] = useState(false);
    const [templates, setTemplates] = useState<DeviceTemplate[]>([]);
    const [isLoadingTemplates, setIsLoadingTemplates] = useState(false);
    const [showTemplateSelector, setShowTemplateSelector] = useState(false);

    // 🔥 NEW: JSON 파서 상태
    const [showJsonParser, setShowJsonParser] = useState(false);
    const [jsonInput, setJsonInput] = useState('');

    // 테이블 컨테이너 참조 (스크롤 제어 등)
    const tableContainerRef = useRef<HTMLDivElement>(null);

    // LocalStorage 저장
    const saveToStorage = useCallback((data: BulkDataPoint[]) => {
        const draftOnly = data.filter(p => p.name || p.address);
        if (draftOnly.length > 0) {
            localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
        } else {
            localStorage.removeItem(STORAGE_KEY);
        }
    }, [STORAGE_KEY]);

    // 히스토리 기록 함수
    const pushHistory = useCallback((newPoints: BulkDataPoint[]) => {
        setHistory(prev => {
            const nextHistory = prev.slice(0, historyIndex + 1);
            nextHistory.push(JSON.parse(JSON.stringify(newPoints)));
            if (nextHistory.length > 50) nextHistory.shift();
            return nextHistory;
        });
        setHistoryIndex(prev => {
            const nextIdx = Math.min(prev + 1, 49);
            return nextIdx;
        });
        saveToStorage(newPoints);
    }, [historyIndex, saveToStorage]);

    const undo = useCallback(() => {
        if (historyIndex > 0) {
            const prevPoints = JSON.parse(JSON.stringify(history[historyIndex - 1]));
            setPoints(prevPoints);
            setHistoryIndex(historyIndex - 1);
            saveToStorage(prevPoints);
        }
    }, [history, historyIndex, saveToStorage]);

    const redo = useCallback(() => {
        if (historyIndex < history.length - 1) {
            const nextPoints = JSON.parse(JSON.stringify(history[historyIndex + 1]));
            setPoints(nextPoints);
            setHistoryIndex(historyIndex + 1);
            saveToStorage(nextPoints);
        }
    }, [history, historyIndex, saveToStorage]);

    useEffect(() => {
        setMounted(true);
        if (isOpen) {
            loadTemplates();

            // 데이터가 이미 있거나 (Step 3에서 넘어옴) 마운트된 상태면 초기화 건너뜀
            if (points.length > 0) return;

            // 1. 새 디바이스 생성(deviceId === 0)인 경우:
            // 이전 작업 내역(bulk_draft_device_0)이 있으면 자동으로 로드하지 않고 빈 행으로 시작하도록 유도
            // (사용자 피드백 반영: "선택도 안했는데 데이터가 들어가 있음" 문제 해결)
            if (deviceId === 0) {
                const initial = Array(20).fill(null).map(() => createEmptyRow());
                setPoints(initial);
                setHistory([JSON.parse(JSON.stringify(initial))]);
                setHistoryIndex(0);
                return;
            }

            // 2. 기존 디바이스 수정인 경우: 로컬 스토리지에서 복원 시도
            const saved = localStorage.getItem(STORAGE_KEY);
            if (saved) {
                try {
                    const parsed = JSON.parse(saved);
                    setPoints(parsed);
                    setHistory([JSON.parse(JSON.stringify(parsed))]);
                    setHistoryIndex(0);
                    return;
                } catch (e) {
                    console.error('Failed to load draft:', e);
                }
            }

            // 3. 아무것도 없으면 기본 행 생성
            const initial = Array(20).fill(null).map(() => createEmptyRow());
            setPoints(initial);
            setHistory([JSON.parse(JSON.stringify(initial))]);
            setHistoryIndex(0);
        }
        return () => setMounted(false);
    }, [isOpen, deviceId]);

    const loadTemplates = async () => {
        try {
            setIsLoadingTemplates(true);
            const res = await TemplateApiService.getTemplates();
            if (res.success) setTemplates(res.data?.items || []);
        } catch (err) {
            console.error('Failed to load templates:', err);
        } finally {
            setIsLoadingTemplates(false);
        }
    };

    const applyTemplate = async (template: DeviceTemplate) => {
        try {
            setIsProcessing(true);
            // Fetch full details including data points
            const res = await TemplateApiService.getTemplate(template.id);
            if (!res.success || !res.data) {
                confirm({
                    title: '알림',
                    message: '템플릿 정보를 불러오는 데 실패했습니다.',
                    confirmButtonType: 'danger',
                    showCancelButton: false
                });
                return;
            }

            const fullTemplate = res.data;
            if (!fullTemplate.data_points || fullTemplate.data_points.length === 0) {
                confirm({
                    title: '알림',
                    message: '이 템플릿에는 정의된 포인트가 없습니다.',
                    confirmButtonType: 'warning',
                    showCancelButton: false
                });
                return;
            }

            const templatePoints: BulkDataPoint[] = fullTemplate.data_points.map(tp => {
                // Map industrial types to simplified UI types (number, boolean, string)
                let dataType = 'number';
                const lowerType = tp.data_type?.toLowerCase() || '';
                if (lowerType.includes('bool')) {
                    dataType = 'boolean';
                } else if (lowerType.includes('string') || lowerType.includes('char')) {
                    dataType = 'string';
                } else {
                    dataType = 'number';
                }

                // Map access_mode from RO/WO/RW to read/write/read_write if necessary
                let mode = tp.access_mode?.toLowerCase() || 'read';
                if (mode === 'ro') mode = 'read';
                if (mode === 'wo') mode = 'write';
                if (mode === 'rw') mode = 'read_write';

                return {
                    ...createEmptyRow(),
                    name: tp.name,
                    address: String(tp.address),
                    data_type: dataType as any,
                    access_mode: (mode as any) || 'read',
                    description: tp.description,
                    unit: tp.unit,
                    scaling_factor: tp.scaling_factor ?? 1,
                    scaling_offset: tp.scaling_offset ?? 0,
                };
            }).map(p => validatePoint(p));

            // 기존 데이터가 있으면 유지할지 물어보기
            const hasData = points.some(p => p.name || p.address);
            if (hasData) {
                const isConfirmed = await confirm({
                    title: '템플릿 적용 확인',
                    message: '기존에 입력된 데이터가 있습니다. 템플릿 데이터로 덮어씌우시겠습니까?',
                    confirmText: '덮어씌우기',
                    confirmButtonType: 'primary'
                });
                if (!isConfirmed) {
                    return;
                }
            }

            setPoints(templatePoints);
            pushHistory(templatePoints);
            setShowTemplateSelector(false);
        } catch (err) {
            console.error('Failed to apply template:', err);
            confirm({
                title: '알림',
                message: '템플릿 적용 중 오류가 발생했습니다.',
                confirmButtonType: 'danger',
                showCancelButton: false
            });
        } finally {
            setIsProcessing(false);
        }
    };

    // 🔥 NEW: JSON 샘플 파싱 로직
    const handleParseJson = () => {
        try {
            const data = JSON.parse(jsonInput);
            const flatPoints: { name: string; mapping_key: string; data_type: string }[] = [];

            const flatten = (obj: any, prefix = '') => {
                for (const key in obj) {
                    const value = obj[key];
                    const fullKey = prefix ? `${prefix}.${key}` : key;
                    if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
                        flatten(value, fullKey);
                    } else {
                        let dataType = 'number';
                        if (typeof value === 'boolean') dataType = 'boolean';
                        else if (typeof value === 'string') dataType = 'string';
                        flatPoints.push({ name: key, mapping_key: fullKey, data_type: dataType });
                    }
                }
            };

            flatten(data);

            const newRows = flatPoints.map(p => ({
                ...createEmptyRow(),
                name: p.name,
                mapping_key: p.mapping_key,
                address: '', // Sub-topic remains empty
                data_type: p.data_type as any
            }));

            setPoints(prev => {
                // 비어있지 않은 첫 20개 행이 있다면 그 뒤에 추가, 아니면 교체
                const hasData = prev.some(p => p.name || p.address);
                const next = hasData ? [...prev, ...newRows] : newRows;

                if (next.length < 20) {
                    const padding = Array(20 - next.length).fill(null).map(() => createEmptyRow());
                    const final = [...next, ...padding];
                    pushHistory(final);
                    return final;
                }
                pushHistory(next);
                return next;
            });

            setShowJsonParser(false);
            setJsonInput('');

            confirm({
                title: '파싱 완료',
                message: `${flatPoints.length}개의 데이터 필드를 찾았습니다.\n[주소] 열에 해당 토픽(Sub-topic)을 입력해주세요.`,
                confirmButtonType: 'success',
                showCancelButton: false
            });

        } catch (e) {
            confirm({
                title: '파싱 실패',
                message: '유효한 JSON 형식이 아닙니다. 형식을 확인해주세요.',
                confirmButtonType: 'danger',
                showCancelButton: false
            });
        }
    };

    // 값 파싱 및 검증
    const validatePoint = (point: BulkDataPoint, allPoints?: BulkDataPoint[]): BulkDataPoint => {
        const errors: string[] = [];

        // 빈 행은 에러 처리하지 않음 (저장 시 필터링)
        if (!point.name && !point.address) {
            return { ...point, isValid: true, errors: [] };
        }

        if (!point.name) errors.push('이름 필수');
        if (!point.address) errors.push('주소 필수');

        const validTypes = ['number', 'boolean', 'string'];
        if (!validTypes.includes(point.data_type || '')) errors.push('유효하지 않은 타입');

        const validModes = ['read', 'write', 'read_write'];
        if (!validModes.includes(point.access_mode || '')) errors.push('유효하지 않은 권한');

        // DB 내 중복 체크
        if (point.address && existingAddresses.includes(point.address)) {
            errors.push('이미 존재하는 주소(DB)');
        }

        // 현재 입력 목록 내 중복 체크 (allPoints가 제공된 경우)
        if (allPoints && point.address) {
            const sameAddrCount = allPoints.filter(p => (p.name || p.address) && p.address === point.address).length;
            if (sameAddrCount > 1) {
                errors.push('목록 내 중복 주소');
            }
        }

        if (allPoints && point.name) {
            const sameNameCount = allPoints.filter(p => (p.name || p.address) && p.name === point.name).length;
            if (sameNameCount > 1) {
                errors.push('목록 내 중복 이름');
            }
        }

        return { ...point, isValid: errors.length === 0, errors };
    };

    const updatePoint = (index: number, field: keyof BulkDataPoint, value: any) => {
        const next = [...points];
        const updated = { ...next[index], [field]: value };

        // 1단계: 개별 행 업데이트
        next[index] = updated;

        // 2단계: 전체 행 다시 검증 (목록 내 중복 체크를 위해)
        const revalidated = next.map(p => validatePoint(p, next));

        setPoints(revalidated);
        pushHistory(revalidated);
    };

    const addRow = () => {
        const next = [...points, createEmptyRow()];
        setPoints(next);
        pushHistory(next);
        // 스크롤 하단으로 이동
        setTimeout(() => {
            if (tableContainerRef.current) {
                tableContainerRef.current.scrollTop = tableContainerRef.current.scrollHeight;
            }
        }, 50);
    };

    const removeRow = (index: number) => {
        const next = points.filter((_, i) => i !== index);
        setPoints(next);
        pushHistory(next);
    };

    const handleReset = async () => {
        const isConfirmed = await confirm({
            title: '초기화 확인',
            message: '입력된 모든 데이터를 지우고 초기화하시겠습니까?\n이 작업은 되돌릴 수 없습니다.',
            confirmText: '초기화 시키기',
            confirmButtonType: 'danger'
        });

        if (isConfirmed) {
            const initial = Array(20).fill(null).map(() => createEmptyRow());
            setPoints(initial);
            setHistory([JSON.parse(JSON.stringify(initial))]);
            setHistoryIndex(0);
            localStorage.removeItem(STORAGE_KEY);
        }
    };

    // 엑셀 붙여넣기 처리
    // 모달 오픈 시 배경 스크롤 방지 및 이벤트 차단
    useEffect(() => {
        if (isOpen && mounted) {
            const originalOverflow = document.body.style.overflow;
            const originalPosition = document.body.style.position;
            const originalPaddingRight = document.body.style.paddingRight;

            const scrollBarWidth = window.innerWidth - document.documentElement.clientWidth;

            document.body.style.overflow = 'hidden';
            document.body.style.position = 'fixed';
            document.body.style.paddingRight = `${scrollBarWidth}px`;

            return () => {
                document.body.style.overflow = originalOverflow;
                document.body.style.position = originalPosition;
                document.body.style.paddingRight = originalPaddingRight;
            };
        }
    }, [isOpen, mounted]);

    // 키보드 방향키 이동 핸들러
    const handleKeyDown = useCallback((e: React.KeyboardEvent, rowIdx: number, colIdx: number) => {
        // Undo/Redo 단축키 처리
        if ((e.ctrlKey || e.metaKey) && e.key === 'z') {
            e.preventDefault();
            if (e.shiftKey) redo();
            else undo();
            return;
        }
        if ((e.ctrlKey || e.metaKey) && e.key === 'y') {
            e.preventDefault();
            redo();
            return;
        }

        const fields = protocolType === 'MQTT'
            ? ['name', 'address', 'mapping_key', 'data_type', 'access_mode', 'description', 'unit', 'scaling_factor', 'scaling_offset']
            : ['name', 'address', 'data_type', 'access_mode', 'description', 'unit', 'scaling_factor', 'scaling_offset'];
        const totalCols = fields.length;
        const totalRows = points.length;

        let nextRow = rowIdx;
        let nextCol = colIdx;

        switch (e.key) {
            case 'ArrowUp':
                if (rowIdx > 0) nextRow = rowIdx - 1;
                break;
            case 'ArrowDown':
                if (rowIdx < totalRows - 1) nextRow = rowIdx + 1;
                break;
            case 'ArrowLeft':
                if (colIdx > 0) nextCol = colIdx - 1;
                break;
            case 'ArrowRight':
                if (colIdx < totalCols - 1) nextCol = colIdx + 1;
                break;
            case 'Enter':
                if (rowIdx < totalRows - 1) {
                    nextRow = rowIdx + 1;
                    e.preventDefault();
                } else {
                    addRow();
                    setTimeout(() => {
                        const allInputs = document.querySelectorAll('#grid-body-scroll-area input, #grid-body-scroll-area select');
                        const target = allInputs[(rowIdx + 1) * totalCols + colIdx] as HTMLElement;
                        if (target) target.focus();
                    }, 0);
                    return;
                }
                break;
            default:
                return;
        }

        if (nextRow !== rowIdx || nextCol !== colIdx) {
            e.preventDefault();
            const allInputs = document.querySelectorAll('#grid-body-scroll-area input, #grid-body-scroll-area select');
            const target = allInputs[nextRow * totalCols + nextCol] as HTMLElement;
            if (target) {
                target.focus();
                // 엑셀 스타일: 포커스된 셀이 화면 밖이면 자동으로 스크롤
                target.scrollIntoView({ block: 'nearest', inline: 'nearest' });

                if (target instanceof HTMLInputElement) {
                    target.select();
                }
            }
        }
    }, [points.length, addRow, undo, redo]);

    const handlePaste = useCallback((e: React.ClipboardEvent) => {
        const clipboardData = e.clipboardData.getData('text');
        if (!clipboardData) return;

        console.log('Paste event triggered');

        // 단일 셀 복사/붙여넣기인지 확인 (탭/줄바꿈 없으면 단일 값으로 간주)
        const isSingleCell = !clipboardData.includes('\t') && !clipboardData.includes('\n');

        // 이벤트가 input 요소에서 발생했고, 단일 셀 데이터라면 -> 기본 동작 허용 (해당 input에만 값 들어감)
        // 하지만 사용자가 "한 셀에 대량 데이터를" 실수로 넣는 것을 방지하기 위해 탭이 포함되어 있으면 무조건 가로채기
        if (isSingleCell) return;

        e.preventDefault();

        const rows = clipboardData.trim().split(/\r?\n/); // 윈도우/맥 줄바꿈 호환
        if (rows.length === 0) return;

        // 헤더 감지
        const firstCols = rows[0].split('\t');
        const hasHeader = firstCols[0]?.toLowerCase().includes('name') ||
            firstCols[0] === '이름' ||
            firstCols[1]?.toLowerCase().includes('address') ||
            firstCols[1] === '주소';

        const dataRows = hasHeader ? rows.slice(1) : rows;

        setPoints(prev => {
            const next = [...prev];

            // 붙여넣기 시작할 위치 찾기
            // 1. 현재 포커스된 요소가 있다면 그 행부터 시작
            // 2. 없다면 첫 번째 빈 행부터 시작
            let startRowIdx = 0;

            // 현재 활성화된 엘리먼트가 우리 테이블의 input인지 확인
            const activeEl = document.activeElement as HTMLElement;
            const rowEl = activeEl?.closest('tr');
            if (rowEl) {
                const rowIndexAttr = rowEl.getAttribute('data-row-index');
                if (rowIndexAttr) {
                    startRowIdx = parseInt(rowIndexAttr, 10);
                }
            } else {
                const firstEmptyIdx = next.findIndex(p => !p.name && !p.address);
                if (firstEmptyIdx !== -1) startRowIdx = firstEmptyIdx;
            }

            // 데이터 채우기
            dataRows.forEach((rowStr, i) => {
                const cols = rowStr.split('\t');
                if (cols.length < 2 && !cols[0]?.trim()) return;

                const targetIdx = startRowIdx + i;
                const isMqtt = protocolType === 'MQTT';

                const newPointData: BulkDataPoint = {
                    ...createEmptyRow(), // ID 새로 생성
                    name: cols[0]?.trim() || '',
                    address: cols[1]?.trim() || '',
                    mapping_key: isMqtt ? (cols[2]?.trim() || '') : undefined,
                    data_type: (cols[isMqtt ? 3 : 2]?.trim().toLowerCase() as any) || 'number',
                    access_mode: (cols[isMqtt ? 4 : 3]?.trim().toLowerCase() as any) || 'read',
                    description: cols[isMqtt ? 5 : 4]?.trim() || '',
                    unit: cols[isMqtt ? 6 : 5]?.trim() || '',
                    scaling_factor: cols[isMqtt ? 7 : 6] ? parseFloat(cols[isMqtt ? 7 : 6]) : 1,
                    scaling_offset: cols[isMqtt ? 8 : 7] ? parseFloat(cols[isMqtt ? 8 : 7]) : 0,
                };

                const validated = validatePoint(newPointData, next);

                if (targetIdx < next.length) {
                    // 기존 행 덮어쓰기 (ID 유지)
                    next[targetIdx] = { ...validated, tempId: next[targetIdx].tempId };
                } else {
                    // 행이 부족하면 추가
                    next.push(validated);
                }
            });

            pushHistory(next);
            return next;
        });

        // 피드백
        const msg = document.createElement('div');
        msg.textContent = `${dataRows.length}개 행이 붙여넣어졌습니다.`;
        msg.style.cssText = 'position: fixed; bottom: 80px; left: 50%; transform: translateX(-50%); background: #3b82f6; color: white; padding: 12px 24px; border-radius: 30px; z-index: 10000; box-shadow: 0 4px 6px rgba(0,0,0,0.1); font-weight: 500; animation: fadeOut 2s forwards; animation-delay: 1.5s;';
        document.body.appendChild(msg);
        setTimeout(() => msg.remove(), 4000);

    }, [deviceId]);

    const handleSaveAll = async () => {
        const validPoints = points.filter(p => p.name || p.address);

        if (validPoints.length === 0) {
            confirm({
                title: '알림',
                message: '저장할 데이터가 없습니다.',
                confirmButtonType: 'warning',
                showCancelButton: false
            });
            return;
        }

        const errors = validPoints.filter(p => !p.isValid);
        if (errors.length > 0) {
            confirm({
                title: '유효성 검사 오류',
                message: `${errors.length}개의 항목에 오류가 있습니다.\n빨간색으로 표시된 셀을 확인해주세요.`,
                confirmButtonType: 'danger',
                showCancelButton: false
            });
            return;
        }

        const isConfirmed = await confirm({
            title: '일괄 등록 확인',
            message: `${validPoints.length}개의 포인트 데이터를 현재 디바이스에 등록하시겠습니까?`,
            confirmText: '등록 시작',
            confirmButtonType: 'primary'
        });

        if (!isConfirmed) {
            return;
        }

        try {
            setIsProcessing(true);
            const payload = validPoints.map(({ isValid, errors, tempId, ...rest }) => rest);
            await onSave(payload);
            localStorage.removeItem(STORAGE_KEY); // 저장 성공 시 임시 데이터 삭제
            onClose();
        } catch (e) {
            console.error(e);
            confirm({
                title: '저장 오류',
                message: '저장 중 오류가 발생했습니다.',
                confirmButtonType: 'danger',
                showCancelButton: false
            });
        } finally {
            setIsProcessing(false);
        }
    };

    if (!isOpen || !mounted) return null;

    return createPortal(
        <div className="bulk-modal-overlay" onPaste={handlePaste}>
            <div id="bulk-modal-box">
                {/* 1. Header Area (Fixed Height) */}
                <header id="bulk-header">
                    <div className="header-content">
                        <div className="title-group">
                            <div className="title-top">
                                <h2>데이터포인트 일괄 등록</h2>
                                <div className="history-controls">
                                    <button onClick={undo} disabled={historyIndex <= 0} title="되돌리기 (Ctrl+Z)">
                                        <i className="fas fa-undo"></i>
                                    </button>
                                    <button onClick={redo} disabled={historyIndex >= history.length - 1} title="다시실행 (Ctrl+Y)">
                                        <i className="fas fa-redo"></i>
                                    </button>
                                </div>
                            </div>
                            <div className="instruction-box">
                                <p className="main-desc">엑셀 데이터를 복사(Ctrl+C)하여 아래 표에 붙여넣기(Ctrl+V) 하세요.</p>
                                <ul className="usage-guide">
                                    <li>이름*, 주소*는 필수 입력 항목이며, 주소는 중복될 수 없습니다.</li>
                                    <li>엔터(Enter) 키로 행 이동이 가능하며, 마지막 행에서 엔터 시 새 행이 자동 추가됩니다.</li>
                                    <li>잘못 입력된 셀은 빨간색으로 표시됩니다.</li>
                                </ul>
                            </div>
                        </div>
                        <button className="close-btn" onClick={onClose} title="닫기">
                            <i className="fas fa-times"></i>
                        </button>
                    </div>
                </header>

                <main id="bulk-main">
                    <div id="grid-outer-container">
                        {/* 1. Header Area (Fixed) */}
                        <div className="grid-header-wrapper">
                            <div className="header-scroll-stiff">
                                <table className="excel-table-fixed">
                                    <thead>
                                        <tr>
                                            <th className="col-idx">#</th>
                                            <th className="col-name">이름 *</th>
                                            <th className="col-addr">{protocolType === 'MQTT' ? 'Sub-Topic *' : '주소 *'}</th>
                                            {protocolType === 'MQTT' && <th className="col-key">JSON Key</th>}
                                            <th className="col-type">타입</th>
                                            <th className="col-mode">권한</th>
                                            <th className="col-desc">설명</th>
                                            <th className="col-unit">단위</th>
                                            <th className="col-scale">스케일</th>
                                            <th className="col-offset">오프셋</th>
                                            <th className="col-actions">삭제</th>
                                        </tr>
                                    </thead>
                                </table>
                                {/* Manual gutter for scrollbar alignment */}
                                <div className="header-gutter"></div>
                            </div>
                        </div>

                        {/* 2. Body Area (Scrollable) */}
                        <div id="grid-body-scroll-area" ref={tableContainerRef} className="custom-scrollbar">
                            <table className="excel-table-fixed">
                                <tbody>
                                    {points.map((point, idx) => (
                                        <tr key={point.tempId} data-row-index={idx}>
                                            <td className="col-idx bg-gray-50">{idx + 1}</td>
                                            <td className={`col-name ${!point.isValid && !point.name ? 'cell-error' : ''}`}>
                                                <input
                                                    className="excel-input"
                                                    value={point.name}
                                                    onChange={e => updatePoint(idx, 'name', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, 0)}
                                                    placeholder={idx === 0 ? "Ex: Temp_Sensor" : ""}
                                                />
                                            </td>
                                            <td className={`col-addr ${(!point.isValid && !point.address) || point.errors.some(e => e.includes('중복')) ? 'cell-error' : ''}`}>
                                                <input
                                                    className="excel-input font-mono"
                                                    value={point.address}
                                                    onChange={e => updatePoint(idx, 'address', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, 1)}
                                                    onBlur={() => {
                                                        // blur 시 전체 다시 검증 (중복 체크 보정)
                                                        setPoints(prev => prev.map(p => validatePoint(p, prev)));
                                                    }}
                                                    placeholder={idx === 0 ? (protocolType === 'MQTT' ? "/sensor/temp" : "40001") : ""}
                                                />
                                            </td>
                                            {protocolType === 'MQTT' && (
                                                <td className="col-key">
                                                    <input
                                                        className="excel-input font-mono"
                                                        value={point.mapping_key || ''}
                                                        onChange={e => updatePoint(idx, 'mapping_key', e.target.value)}
                                                        onKeyDown={e => handleKeyDown(e, idx, 2)}
                                                        placeholder={idx === 0 ? "temperature" : ""}
                                                    />
                                                </td>
                                            )}
                                            <td className="col-type">
                                                <select
                                                    className="excel-select"
                                                    value={point.data_type}
                                                    onChange={e => updatePoint(idx, 'data_type', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 3 : 2)}
                                                >
                                                    <option value="number">number</option>
                                                    <option value="boolean">boolean</option>
                                                    <option value="string">string</option>
                                                </select>
                                            </td>
                                            <td className="col-mode">
                                                <select
                                                    className="excel-select"
                                                    value={point.access_mode}
                                                    onChange={e => updatePoint(idx, 'access_mode', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 4 : 3)}
                                                >
                                                    <option value="read">read</option>
                                                    <option value="write">write</option>
                                                    <option value="read_write">read_write</option>
                                                </select>
                                            </td>
                                            <td className="col-desc">
                                                <input
                                                    className="excel-input"
                                                    value={point.description || ''}
                                                    onChange={e => updatePoint(idx, 'description', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 5 : 4)}
                                                />
                                            </td>
                                            <td className="col-unit">
                                                <input
                                                    className="excel-input"
                                                    value={point.unit || ''}
                                                    onChange={e => updatePoint(idx, 'unit', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 6 : 5)}
                                                />
                                            </td>
                                            <td className="col-scale">
                                                <input
                                                    type="number"
                                                    className="excel-input text-right"
                                                    value={point.scaling_factor}
                                                    onChange={e => updatePoint(idx, 'scaling_factor', parseFloat(e.target.value))}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 7 : 6)}
                                                />
                                            </td>
                                            <td className="col-offset">
                                                <input
                                                    type="number"
                                                    className="excel-input text-right"
                                                    value={point.scaling_offset}
                                                    onChange={e => updatePoint(idx, 'scaling_offset', parseFloat(e.target.value))}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 8 : 7)}
                                                />
                                            </td>
                                            <td className="col-actions">
                                                <button onClick={() => removeRow(idx)} className="delete-row-btn" title="행 삭제">
                                                    <i className="fas fa-times"></i>
                                                </button>
                                            </td>
                                        </tr>
                                    ))}
                                </tbody>
                            </table>
                        </div>
                    </div>
                </main>

                {/* 3. Footer Area (Fixed Height) */}
                <footer id="bulk-footer">
                    <div className="footer-left">
                        <div className="action-group">
                            <button className="add-row-btn" onClick={addRow}>
                                <i className="fas fa-plus"></i> 행 추가
                            </button>
                            <button className="template-btn" onClick={() => setShowTemplateSelector(!showTemplateSelector)}>
                                <i className="fas fa-file-import"></i> 템플릿 불러오기
                            </button>
                            {protocolType === 'MQTT' && (
                                <button
                                    className="json-parser-btn"
                                    onClick={() => setShowJsonParser(!showJsonParser)}
                                    style={{
                                        padding: '8px 16px',
                                        background: '#f0fdf4',
                                        color: '#166534',
                                        border: '1px solid #bbf7d0',
                                        borderRadius: '6px',
                                        fontWeight: 700,
                                        fontSize: 13,
                                        cursor: 'pointer'
                                    }}
                                >
                                    <i className="fas fa-code"></i> JSON 샘플 파싱
                                </button>
                            )}
                            <button className="reset-btn" onClick={handleReset} title="전체 입력 데이터 초기화">
                                <i className="fas fa-trash-alt"></i> 초기화
                            </button>
                        </div>
                        {showTemplateSelector && (
                            <div className="template-selector-bubble">
                                <div className="bubble-header">
                                    <span>적용할 템플릿 선택</span>
                                    <button onClick={() => setShowTemplateSelector(false)}><i className="fas fa-times"></i></button>
                                </div>
                                <div className="bubble-body custom-scrollbar">
                                    {isLoadingTemplates ? (
                                        <div className="loading-text">불러오는 중...</div>
                                    ) : templates.length === 0 ? (
                                        <div className="empty-text">등록된 템플릿이 없습니다.</div>
                                    ) : (
                                        templates.map(t => (
                                            <div key={t.id} className="template-item" onClick={() => applyTemplate(t)}>
                                                <div className="t-name">{t.name}</div>
                                                <div className="t-meta">{t.manufacturer_name} / {t.model_name} ({t.point_count || 0} pts)</div>
                                            </div>
                                        ))
                                    )}
                                </div>
                            </div>
                        )}

                        {/* 🔥 NEW: JSON 파서 버블 */}
                        {showJsonParser && (
                            <div className="json-parser-bubble" style={{
                                position: 'absolute', bottom: '90px', left: '160px', width: '400px',
                                background: 'white', borderRadius: '12px', border: '1px solid #e2e8f0',
                                boxShadow: '0 10px 25px -5px rgba(0,0,0,0.2)', zIndex: 1000,
                                display: 'flex', flexDirection: 'column', overflow: 'hidden'
                            }}>
                                <div className="bubble-header" style={{ padding: '12px 16px', background: '#f8fafc', borderBottom: '1px solid #e2e8f0', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                    <span style={{ fontSize: '12px', fontWeight: 700, color: '#166534' }}>JSON 샘플 데이터 파싱</span>
                                    <button onClick={() => setShowJsonParser(false)} style={{ background: 'none', border: 'none', cursor: 'pointer', color: '#94a3b8' }}><i className="fas fa-times"></i></button>
                                </div>
                                <div style={{ padding: '16px' }}>
                                    <p style={{ fontSize: '11px', color: '#64748b', marginBottom: '8px' }}>
                                        디바이스에서 수신되는 JSON 페이로드를 아래에 붙여넣으세요.<br />
                                        자동으로 키 구조를 분석하여 행을 생성합니다.
                                    </p>
                                    <textarea
                                        className="custom-scrollbar"
                                        value={jsonInput}
                                        onChange={(e) => setJsonInput(e.target.value)}
                                        placeholder='{ "temperature": 25.5, "status": { "on": true } }'
                                        style={{
                                            width: '100%', height: '150px', border: '1px solid #cbd5e1',
                                            borderRadius: '6px', padding: '10px', fontSize: '12px',
                                            fontFamily: 'monospace', outline: 'none', resize: 'none'
                                        }}
                                    />
                                    <button
                                        onClick={handleParseJson}
                                        style={{
                                            width: '100%', marginTop: '12px', padding: '10px',
                                            background: '#166534', color: 'white', border: 'none',
                                            borderRadius: '6px', fontWeight: 700, cursor: 'pointer'
                                        }}
                                    >
                                        구조 분석 및 자동 생성
                                    </button>
                                </div>
                            </div>
                        )}
                        <span className="stats-text" style={{ marginLeft: '12px' }}>
                            총 <strong>{points.filter(p => p.name || p.address).length}</strong>개 입력됨
                        </span>
                    </div>
                    <div className="footer-right">
                        <button className="cancel-btn" onClick={onClose}>취소</button>
                        <button
                            className={`save-btn ${isProcessing || points.some(p => (p.name || p.address) && !p.isValid) || !points.some(p => p.name || p.address) ? 'disabled' : ''}`}
                            onClick={handleSaveAll}
                            disabled={isProcessing || points.some(p => (p.name || p.address) && !p.isValid) || !points.some(p => p.name || p.address)}
                        >
                            {isProcessing ? '저장 중...' : '일괄 등록 완료'}
                        </button>
                    </div>
                </footer>
            </div >

            <style>{`
                .bulk-modal-overlay {
                    position: fixed; top: 0; left: 0; right: 0; bottom: 0;
                    background: rgba(15, 23, 42, 0.75); backdrop-filter: blur(8px);
                    display: flex; justify-content: center; align-items: center;
                    z-index: 9999;
                }

                #bulk-modal-box {
                    width: ${protocolType === 'MQTT' ? '1400px' : '1280px'};
                    height: 850px;
                    max-height: 90vh;
                    background: white;
                    border-radius: 12px;
                    display: flex;
                    flex-direction: column;
                    overflow: hidden;
                    box-shadow: 0 40px 80px rgba(0,0,0,0.5);
                    animation: modalEntry 0.2s ease-out; /* 리사이징 애니메이션 제거, 단순 페이드로 조정 */
                }

                @keyframes modalEntry {
                    from { opacity: 0; }
                    to { opacity: 1; }
                }

                /* Header Styling */
                #bulk-header {
                    height: auto;
                    min-height: 100px;
                    flex-shrink: 0;
                    border-bottom: 1px solid #e2e8f0;
                    background: white;
                    padding: 16px 32px;
                    display: flex;
                    align-items: center;
                }
                .header-content {
                    width: 100%;
                    display: flex;
                    justify-content: space-between;
                    align-items: flex-start;
                }
                .title-top { display: flex; align-items: center; gap: 20px; }
                .title-group h2 { font-size: 1.25rem; font-weight: 800; color: #1e293b; margin: 0; }
                
                .history-controls { display: flex; gap: 8px; }
                .history-controls button {
                    width: 32px; height: 32px; border-radius: 6px; border: 1px solid #e2e8f0;
                    background: white; color: #64748b; cursor: pointer; display: flex;
                    align-items: center; justify-content: center; transition: all 0.2s;
                }
                .history-controls button:hover:not(:disabled) { background: #f8fafc; color: #2563eb; border-color: #2563eb; }
                .history-controls button:disabled { opacity: 0.3; cursor: not-allowed; }

                .instruction-box { margin-top: 12px; }
                .main-desc { font-size: 0.875rem; color: #1e293b; font-weight: 600; margin-bottom: 6px; }
                .usage-guide { margin: 0; padding-left: 18px; list-style: disc; }
                .usage-guide li { font-size: 0.8125rem; color: #64748b; line-height: 1.6; }

                .close-btn { background: none; border: none; font-size: 1.5rem; color: #94a3b8; cursor: pointer; padding: 4px; }
                .close-btn:hover { color: #f43f5e; }

                /* Main Body Styling */
                #bulk-main {
                    flex: 1;
                    min-height: 0; 
                    padding: 24px;
                    background: #f8fafc;
                }
                #grid-outer-container {
                    height: 100%;
                    background: white;
                    border: 1px solid #e2e8f0;
                    border-radius: 8px;
                    display: flex;
                    flex-direction: column;
                    overflow: hidden;
                    box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05);
                }
                .grid-header-wrapper {
                    flex-shrink: 0;
                    background: #f1f5f9;
                    border-bottom: 2px solid #cbd5e1;
                }
                .header-scroll-stiff {
                    display: flex;
                    width: 100%;
                }
                .header-gutter {
                    width: 10px; /* Exact match of custom scrollbar width */
                    background: #f1f5f9;
                    border-bottom: none;
                }
                #grid-body-scroll-area {
                    flex: 1;
                    overflow-y: scroll; /* Force scrollbar to always exist for alignment */
                    background: white;
                }

                /* Table Styling */
                .excel-table-fixed {
                    flex: 1;
                    width: 100%;
                    table-layout: fixed;
                    border-collapse: collapse;
                }
                .excel-table-fixed th, .excel-table-fixed td {
                    border-right: 1px solid #e2e8f0;
                    border-bottom: 1px solid #e2e8f0;
                    padding: 0;
                    height: 38px;
                    overflow: hidden;
                    white-space: nowrap;
                }
                .excel-table-fixed th {
                    font-size: 11px;
                    font-weight: 700;
                    color: #475569;
                    text-transform: uppercase;
                    background: #f1f5f9;
                    padding: 0 12px;
                }

                /* Column Widths (Explicitly Fixed) */
                .col-idx { width: 45px; text-align: center; color: #94a3b8; font-size: 11px; font-family: monospace; }
                .col-name { width: 220px; }
                .col-addr { width: 140px; }
                .col-key { width: 150px; }
                .col-type { width: 110px; }
                .col-mode { width: 110px; }
                .col-desc { width: 280px; }
                .col-unit { width: 90px; }
                .col-scale { width: 85px; }
                .col-offset { width: 85px; }
                .col-actions { width: 50px; text-align: center; border-right: none !important; }

                /* Input & Select Styling */
                .excel-input, .excel-select {
                    width: 100%;
                    height: 100%;
                    border: none;
                    outline: none;
                    background: transparent;
                    padding: 0 12px;
                    font-size: 13px;
                    color: #334155;
                }
                .excel-table-fixed tr:hover { background-color: #f8fafc; }
                .excel-table-fixed tr:focus-within { background-color: #f0f7ff; }
                .excel-table-fixed td:focus-within {
                    box-shadow: inset 0 0 0 2px #3b82f6;
                    position: relative;
                    z-index: 5;
                }
                .cell-error { background-color: #fef2f2 !important; }
                .delete-row-btn {
                    width: 100%;
                    height: 100%;
                    background: none;
                    border: none;
                    color: #cbd5e1;
                    cursor: pointer;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                }
                .delete-row-btn:hover { color: #f43f5e; background: #fff1f2; }

                /* Footer Styling */
                #bulk-footer {
                    height: 80px;
                    flex-shrink: 0;
                    border-top: 1px solid #e2e8f0;
                    background: white;
                    padding: 0 32px;
                    display: flex;
                    align-items: center;
                    justify-content: space-between;
                }
                .footer-left { display: flex; align-items: center; gap: 32px; }
                .action-group { display: flex; gap: 12px; }
                .add-row-btn {
                    padding: 8px 16px;
                    background: #1e293b;
                    color: white;
                    border-radius: 6px;
                    font-weight: 700;
                    font-size: 13px;
                    cursor: pointer;
                    border: none;
                    transition: all 0.2s;
                }
                .add-row-btn:hover { background: #334155; transform: translateY(-1px); }
                
                .reset-btn {
                    padding: 8px 16px;
                    background: #fff1f2;
                    color: #f43f5e;
                    border: 1px solid #fecdd3;
                    border-radius: 6px;
                    font-weight: 700;
                    font-size: 13px;
                    cursor: pointer;
                    transition: all 0.2s;
                }
                .reset-btn:hover { background: #ffe4e6; border-color: #fda4af; }

                .template-btn {
                    padding: 8px 16px;
                    background: #f0f9ff;
                    color: #0369a1;
                    border: 1px solid #bae6fd;
                    border-radius: 6px;
                    font-weight: 700;
                    font-size: 13px;
                    cursor: pointer;
                    transition: all 0.2s;
                }
                .template-btn:hover { background: #e0f2fe; border-color: #7dd3fc; }

                .template-selector-bubble {
                    position: absolute; bottom: 90px; left: 32px; width: 320px;
                    background: white; border-radius: 12px; border: 1px solid #e2e8f0;
                    box-shadow: 0 10px 25px -5px rgba(0,0,0,0.2); z-index: 1000;
                    display: flex; flex-direction: column; overflow: hidden;
                }
                .bubble-header {
                    padding: 12px 16px; background: #f8fafc; border-bottom: 1px solid #e2e8f0;
                    display: flex; justify-content: space-between; align-items: center;
                }
                .bubble-header span { font-size: 12px; font-weight: 700; color: #475569; }
                .bubble-header button { background: none; border: none; font-size: 12px; color: #94a3b8; cursor: pointer; }
                .bubble-body { max-height: 240px; overflow-y: auto; padding: 8px; }
                .template-item {
                    padding: 10px 12px; border-radius: 6px; cursor: pointer; transition: all 0.2s;
                }
                .template-item:hover { background: #f1f5f9; }
                .t-name { font-size: 13px; font-weight: 600; color: #1e293b; margin-bottom: 2px; }
                .t-meta { font-size: 11px; color: #64748b; }
                .loading-text, .empty-text { padding: 20px; text-align: center; font-size: 12px; color: #94a3b8; }

                .stats-text { font-size: 13px; color: #64748b; }
                .stats-text strong { color: #3b82f6; }

                .footer-right { display: flex; gap: 12px; }
                .cancel-btn {
                    padding: 10px 20px;
                    color: #64748b;
                    font-weight: 700;
                    font-size: 14px;
                    cursor: pointer;
                    border: none;
                    background: none;
                }
                .cancel-btn:hover { color: #1e293b; text-decoration: underline; }
                .save-btn {
                    padding: 10px 28px;
                    background: #2563eb;
                    color: white;
                    border-radius: 6px;
                    font-weight: 700;
                    font-size: 14px;
                    cursor: pointer;
                    border: none;
                    box-shadow: 0 10px 15px -3px rgba(37,99,235, 0.3);
                    transition: all 0.2s;
                }
                .save-btn:hover { background: #1d4ed8; transform: translateY(-1px); }
                .save-btn.disabled {
                    background: #cbd5e1;
                    color: #94a3b8;
                    cursor: not-allowed;
                    box-shadow: none;
                    transform: none;
                }

                /* Scrollbar Customization */
                .custom-scrollbar::-webkit-scrollbar { width: 10px; height: 10px; }
                .custom-scrollbar::-webkit-scrollbar-track { background: #f8fafc; }
                .custom-scrollbar::-webkit-scrollbar-thumb {
                    background: #cbd5e1;
                    border-radius: 5px;
                    border: 2px solid #f8fafc;
                }
                .custom-scrollbar::-webkit-scrollbar-thumb:hover { background: #94a3b8; }
            `}</style>
        </div >,
        document.body
    );
};

export default DeviceDataPointsBulkModal;
