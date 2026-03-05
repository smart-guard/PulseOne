// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx
// 📊 대량 데이터 포인트 등록 모달 (엑셀 그리드 방식)
// ============================================================================

import { createPortal } from 'react-dom';
import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { DataPoint } from '../../../api/services/dataApi';
import { useConfirmContext } from '../../common/ConfirmProvider';
import { TemplateApiService } from '../../../api/services/templateApi';
import { DeviceTemplate } from '../../../types/manufacturing';

interface BulkDataPoint extends Partial<DataPoint> {
    isValid: boolean;
    errors: string[];
    tempId: number;
    register_type?: string; // Modbus 전용: HR/IR/CO/DI (UI 상태 필드, 전송 시 address_string으로 변환)
}

interface DeviceDataPointsBulkModalProps {
    deviceId?: number;
    /** 임시저장 컨텍스트: 마스터모델과 개별 디바이스를 별도 키로 저장 */
    draftContext?: 'master-model' | 'device';
    isOpen: boolean;
    onClose: () => void;
    onSave?: (points: Partial<DataPoint>[]) => Promise<void>;
    onImport?: (points: Partial<DataPoint>[]) => void;
    existingAddresses?: string[];
    protocolType?: string;
}

const DeviceDataPointsBulkModal: React.FC<DeviceDataPointsBulkModalProps> = ({
    deviceId = 0,
    draftContext = 'device',
    isOpen,
    onClose,
    onSave,
    onImport,
    existingAddresses = [],
    protocolType
}) => {
    const { confirm } = useConfirmContext();
    const isModbus = protocolType === 'MODBUS_TCP' || protocolType === 'MODBUS_RTU' || protocolType === 'MODBUS';
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
        scaling_offset: 0,
        protocol_params: {},
        register_type: isModbus ? 'HR' : undefined  // Modbus: HR(FC3)/IR(FC4)/CO(FC1)/DI(FC2)
    });

    // 마스터모델과 개별 디바이스를 별도 localStorage 키로 분리
    const STORAGE_KEY = draftContext === 'master-model'
        ? 'bulk_draft_master_model'
        : `bulk_draft_device_${deviceId}`;

    const [points, setPoints] = useState<BulkDataPoint[]>([]);
    const { t } = useTranslation(['devices', 'common']);
    const [history, setHistory] = useState<BulkDataPoint[][]>([]);
    const [historyIndex, setHistoryIndex] = useState(-1);
    const [isProcessing, setIsProcessing] = useState(false);
    const [mounted, setMounted] = useState(false);
    const [templates, setTemplates] = useState<DeviceTemplate[]>([]);
    const [isLoadingTemplates, setIsLoadingTemplates] = useState(false);
    const [showTemplateSelector, setShowTemplateSelector] = useState(false);
    // 복원 배너: 임시저장 데이터 복원 시 표시
    const [draftRestoredCount, setDraftRestoredCount] = useState<number | null>(null);

    // 🔥 NEW: JSON 파서 상태
    const [showJsonParser, setShowJsonParser] = useState(false);
    const [jsonInput, setJsonInput] = useState('');

    // 🔢 Bit Split 상태
    const [showBitSplitter, setShowBitSplitter] = useState(false);
    const [bitSplitConfig, setBitSplitConfig] = useState({
        address: '',
        addressEnd: '',          // 주소 범위 끝 (빈값이면 단일 주소)
        useAddressRange: false,  // 범위 모드 여부
        namePrefix: '',
        bitStart: 0,
        bitEnd: 15,
        accessMode: 'read',
        descPrefix: '',
        mode: 'individual' as 'individual' | 'group' | 'custom',
        numValues: 4,
        registerBits: 16 as 16 | 32,
        customRanges: '0-3, 4-7, 8-11, 12-15'
    });
    const [selectedRows, setSelectedRows] = useState<Set<string | number>>(new Set());

    // 비트 범위 파싱: "0-3, 4-7" → [{start:0, end:3}, {start:4, end:7}]
    const parseBitRanges = (raw: string, maxBits: number): Array<{ start: number, end: number, error?: string }> => {
        if (!raw.trim()) return [];
        return raw.split(',').map(part => {
            const p = part.trim();
            const m = p.match(/^(\d+)\s*[-~]\s*(\d+)$/);
            if (!m) return { start: -1, end: -1, error: `잘못된 형식: "${p}" (예: 0-3)` };
            const s = parseInt(m[1]), e = parseInt(m[2]);
            if (s < 0 || e >= maxBits) return { start: s, end: e, error: `범위 초과: ${s}-${e} (0~${maxBits - 1})` };
            if (s > e) return { start: s, end: e, error: `시작>끝: ${s}-${e}` };
            return { start: s, end: e };
        });
    };

    // 테이블 컨테이너 참조 (스크롤 제어 등)
    const tableContainerRef = useRef<HTMLDivElement>(null);
    const lastCheckedIdxRef = useRef<number>(-1);

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
            setDraftRestoredCount(null); // 모달 열릴 때 배너 초기화

            // 임시저장 복원 시도 → 없으면 기본 행 생성
            const saved = localStorage.getItem(STORAGE_KEY);
            if (saved) {
                try {
                    const parsed = JSON.parse(saved);
                    const hasData = parsed.some((p: BulkDataPoint) => p.name || p.address);
                    if (hasData) {
                        setPoints(parsed);
                        setHistory([JSON.parse(JSON.stringify(parsed))]);
                        setHistoryIndex(0);
                        // 복원된 데이터 건수 계산 → 배너에 표시
                        const count = parsed.filter((p: BulkDataPoint) => p.name || p.address).length;
                        setDraftRestoredCount(count);
                        return;
                    }
                } catch (e) {
                    console.error('Failed to load draft:', e);
                }
            }

            // 임시저장 없으면 기본 빈 행 생성
            const initial = Array(20).fill(null).map(() => createEmptyRow());
            setPoints(initial);
            setHistory([JSON.parse(JSON.stringify(initial))]);
            setHistoryIndex(0);
        } else {
            // 모달 닫힐 때 points 초기화 → 재오픈 시 복원 로직 다시 실행됨
            setPoints([]);
            setHistory([]);
            setHistoryIndex(-1);
            setDraftRestoredCount(null);
        }
        return () => setMounted(false);
    }, [isOpen, deviceId, draftContext]);



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
                    title: t('bulk.notice', { defaultValue: '알림' }),
                    message: t('bulk.templateLoadFailed', { defaultValue: '템플릿 정보를 불러오는데 실패했습니다.' }),
                    confirmButtonType: 'danger',
                    showCancelButton: false
                });
                return;
            }

            const fullTemplate = res.data;
            if (!fullTemplate.data_points || fullTemplate.data_points.length === 0) {
                confirm({
                    title: t('bulk.notice', { defaultValue: '알림' }),
                    message: t('bulk.templateNoPoints', { defaultValue: '이 템플릿에 정의된 포인트가 없습니다.' }),
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
                    title: t('bulk.templateApplyConfirmTitle', { defaultValue: '템플릿 적용 확인' }),
                    message: t('bulk.templateApplyConfirmMsg', { defaultValue: '기존 데이터가 덮어쓰여집니다. 템플릿을 적용하시걌습니까?' }),
                    confirmText: t('bulk.overwrite', { defaultValue: '덮어쓰기' }),
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
                title: t('bulk.notice', { defaultValue: '알림' }),
                message: t('bulk.templateApplyError', { defaultValue: '템플릿 적용 중 오류가 발생했습니다.' }),
                confirmButtonType: 'danger',
                showCancelButton: false
            });
        } finally {
            setIsProcessing(false);
        }
    };

    // 🔥 NEW: Parse JSON Sample 로직
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
                title: t('bulk.parseComplete', { defaultValue: '파싱 완료' }),
                message: `${t('bulk.parseFoundFields', { count: flatPoints.length, defaultValue: `${flatPoints.length}개 데이터 필드를 발견했습니다.` })}\n${t('bulk.parseHint', { defaultValue: '[주소] 컄럼에 해당 토픽(Sub-topic)을 입력해주세요.' })}`,
                confirmButtonType: 'success',
                showCancelButton: false
            });

        } catch (e) {
            confirm({
                title: t('bulk.parseFailed', { defaultValue: '파싱 실패' }),
                message: t('bulk.parseInvalidJson', { defaultValue: '\uc798못된 JSON 형식입니다. 형식을 확인해주세요.' }),
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

        if (!point.name) errors.push('Name required');
        if (!point.address) errors.push('Address required');

        const validTypes = ['number', 'boolean', 'string'];
        if (!validTypes.includes(point.data_type || '')) errors.push('Invalid type');

        const validModes = ['read', 'write', 'read_write'];
        if (!validModes.includes(point.access_mode || '')) errors.push('Invalid access mode');

        // DB 내 중복 체크 — 모든 포인트(비트 포함) 대상. 빨간셀로 표시, 등록 시 자동 스킵.
        if (point.address && existingAddresses.includes(point.address)) {
            errors.push(t('bulk.errDuplicateAddr', { ns: 'devices', defaultValue: 'Address already exists (DB)' }));
        }

        // 현재 입력 목록 내 중복 체크
        // bit_index/bit_start가 있으면 (address + bit) 조합으로 비교, 없으면 address만 비교
        if (allPoints && point.address) {
            const sameKey = allPoints.filter(p => {
                if (!(p.name || p.address)) return false;
                if (p.address !== point.address) return false;
                const pBitIdx = (p.protocol_params as any)?.bit_index;
                const thisBitIdx = (point.protocol_params as any)?.bit_index;
                const pBitStart = (p.protocol_params as any)?.bit_start;
                const thisBitStart = (point.protocol_params as any)?.bit_start;
                // 둘 다 단일비트 → 같은 bit_index만 중복
                if (pBitIdx !== undefined && thisBitIdx !== undefined) return pBitIdx === thisBitIdx;
                // 둘 다 범위비트 → 같은 bit_start만 중복
                if (pBitStart !== undefined && thisBitStart !== undefined) return pBitStart === thisBitStart;
                // 비트 없는 것끼리 → 주소만 비교
                if (pBitIdx === undefined && thisBitIdx === undefined && pBitStart === undefined && thisBitStart === undefined) return true;
                // 비트 있는 것과 없는 것 → 서로 다른 key로 간주
                return false;
            }).length;
            if (sameKey > 1) {
                errors.push(t('bulk.errDuplicateInList', { ns: 'devices', defaultValue: 'Duplicate address in list' }));
            }
        }

        if (allPoints && point.name) {
            const sameNameCount = allPoints.filter(p => (p.name || p.address) && p.name === point.name).length;
            if (sameNameCount > 1) {
                errors.push('Duplicate name in list');
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

    // 비트 설정 업데이트 헬퍼 (단일: "3" → bit_index, 범위: "0~3" → bit_start/bit_end)
    const updateBitSetting = (index: number, value: string) => {
        const next = [...points];
        const params = { ...(next[index].protocol_params || {}) };
        // 기존 비트 관련 키 모두 초기화
        delete params.bit_index;
        delete params.bit_start;
        delete params.bit_end;

        if (value && value.trim() !== '') {
            const rangeMatch = value.match(/^(\d+)\s*~\s*(\d+)$/);
            if (rangeMatch) {
                // 범위 형식: "0~3" → bit_start/bit_end
                params.bit_start = rangeMatch[1];
                params.bit_end = rangeMatch[2];
            } else if (/^\d+$/.test(value.trim())) {
                // 단일 숫자: "3" → bit_index
                params.bit_index = value.trim();
            }
        }

        const updated = { ...next[index], protocol_params: params };
        next[index] = updated;
        const revalidated = next.map(p => validatePoint(p, next));
        setPoints(revalidated);
        pushHistory(revalidated);
    };

    // 비트 분할 자동 생성
    const handleBitSplit = () => {
        const { address, addressEnd, useAddressRange, namePrefix, accessMode, descPrefix, mode, registerBits, customRanges } = bitSplitConfig;
        if (!address.trim()) {
            confirm({ title: t('bulk.notice', { ns: 'devices' }), message: t('bulk.errAddressRequired', { ns: 'devices' }), confirmButtonType: 'warning', showCancelButton: false });
            return;
        }

        // 주소 범위 계산
        const addrStart = parseInt(address.trim(), 10);
        const addrEnd = useAddressRange && addressEnd.trim() ? parseInt(addressEnd.trim(), 10) : addrStart;
        if (isNaN(addrStart) || isNaN(addrEnd)) {
            confirm({ title: t('bulk.inputError', { ns: 'devices' }), message: t('bulk.errAddressNotNumber', { ns: 'devices' }), confirmButtonType: 'warning', showCancelButton: false });
            return;
        }
        if (addrEnd < addrStart) {
            confirm({ title: t('bulk.inputError', { ns: 'devices' }), message: t('bulk.errEndBeforeStart', { ns: 'devices' }), confirmButtonType: 'warning', showCancelButton: false });
            return;
        }
        const addrCount = addrEnd - addrStart + 1;
        if (addrCount > 500) {
            confirm({ title: t('bulk.warning', { ns: 'devices' }), message: t('bulk.errTooManyAddresses', { ns: 'devices', count: addrCount }), confirmButtonType: 'warning', showCancelButton: false });
            return;
        }

        // 비트 범위 파싱
        const ranges = parseBitRanges(customRanges, registerBits);
        const errors = ranges.filter(r => r.error);
        if (ranges.length === 0) {
            confirm({ title: t('bulk.notice', { ns: 'devices' }), message: t('bulk.errBitRangeRequired', { ns: 'devices' }), confirmButtonType: 'warning', showCancelButton: false });
            return;
        }
        if (errors.length > 0) {
            confirm({ title: t('bulk.inputError', { ns: 'devices' }), message: errors.map(e => e.error).join('\n'), confirmButtonType: 'warning', showCancelButton: false });
            return;
        }

        const newRows: BulkDataPoint[] = [];

        // 각 주소에 대해 비트 패턴 생성
        for (let addr = addrStart; addr <= addrEnd; addr++) {
            const addrStr = String(addr);
            const pfx = namePrefix.trim() || addrStr;

            if (mode === 'individual') {
                // boolean: 각 범위의 비트를 개별 bool 포인트로 (bit_index)
                ranges.forEach((r) => {
                    for (let bit = r.start; bit <= r.end; bit++) {
                        newRows.push(validatePoint({
                            ...createEmptyRow(),
                            name: `${pfx}_Bit${bit}`,
                            address: addrStr,
                            data_type: 'boolean' as any,
                            access_mode: (accessMode as any) || 'read',
                            description: descPrefix.trim()
                                ? `${descPrefix.trim()} ${addrStr} Bit${bit}`
                                : `${addrStr} bit${bit} (0/1)`,
                            protocol_params: { bit_index: String(bit) },
                        }));
                    }
                });
            } else {
                // number(group): 각 범위를 정수값 포인트 1개로 (bit_start/bit_end)
                ranges.forEach((r, i) => {
                    const bitsLen = r.end - r.start + 1;
                    const maxVal = (1 << bitsLen) - 1;
                    newRows.push(validatePoint({
                        ...createEmptyRow(),
                        name: `${pfx}_V${i + 1}`,
                        address: addrStr,
                        data_type: 'number' as any,
                        access_mode: (accessMode as any) || 'read',
                        description: descPrefix.trim()
                            ? `${descPrefix.trim()} ${addrStr} V${i + 1}(bit${r.start}~${r.end})`
                            : `${addrStr} bit${r.start}~${r.end} (0~${maxVal})`,
                        protocol_params: { bit_start: String(r.start), bit_end: String(r.end) },
                    }));
                });
            }
        }

        setPoints(prev => {
            const hasData = prev.some(p => p.name || p.address);
            const base = hasData ? [...prev] : [];
            const trimmed = base.filter(p => p.name || p.address);
            const next = [...trimmed, ...newRows];
            while (next.length < 20) next.push(createEmptyRow());
            const revalidated = next.map(p => validatePoint(p, next));
            pushHistory(revalidated);
            return revalidated;
        });

        setShowBitSplitter(false);
        setBitSplitConfig(prev => ({ ...prev, address: '', addressEnd: '', namePrefix: '' }));
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
            title: t('bulk.confirmReset', { defaultValue: '리셋 확인' }),
            message: t('bulk.confirmResetMsg', { defaultValue: '입력된 모든 데이터를 지우고 리셋하시걌습니까?\n이 작업은 실행 취소할 수 없습니다.' }),
            confirmText: t('bulk.reset', { defaultValue: '리셋' }),
            confirmButtonType: 'danger'
        });

        if (isConfirmed) {
            const initial = Array(20).fill(null).map(() => createEmptyRow());
            setPoints(initial);
            setHistory([JSON.parse(JSON.stringify(initial))]);
            setHistoryIndex(0);
            localStorage.removeItem(STORAGE_KEY);
            setDraftRestoredCount(null); // 배너 숨김
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
            : isModbus
                ? ['name', 'address', 'bit_index', 'data_type', 'access_mode', 'description', 'unit', 'scaling_factor', 'scaling_offset']
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
            firstCols[0] === 'Name' || firstCols[0] === 'Name' ||
            firstCols[1]?.toLowerCase().includes('address') ||
            firstCols[1] === 'Address' || firstCols[1] === 'Address';

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
                const bitIndex = !isMqtt ? (cols[2]?.trim() || '') : '';
                const typeCol = isMqtt ? 3 : (bitIndex !== '' ? 3 : 2);

                const newPointData: BulkDataPoint = {
                    ...createEmptyRow(), // ID 새로 생성
                    name: cols[0]?.trim() || '',
                    address: cols[1]?.trim() || '',
                    mapping_key: isMqtt ? (cols[2]?.trim() || '') : undefined,
                    data_type: (cols[isMqtt ? 3 : typeCol]?.trim().toLowerCase() as any) || 'number',
                    access_mode: (cols[isMqtt ? 4 : typeCol + 1]?.trim().toLowerCase() as any) || 'read',
                    description: cols[isMqtt ? 5 : typeCol + 2]?.trim() || '',
                    unit: cols[isMqtt ? 6 : typeCol + 3]?.trim() || '',
                    scaling_factor: cols[isMqtt ? 7 : typeCol + 4] ? parseFloat(cols[isMqtt ? 7 : typeCol + 4]) : 1,
                    scaling_offset: cols[isMqtt ? 8 : typeCol + 5] ? parseFloat(cols[isMqtt ? 8 : typeCol + 5]) : 0,
                    protocol_params: (() => {
                        if (isMqtt || bitIndex === '') return {};
                        const rangeMatch = bitIndex.match(/^(\d+)\s*~\s*(\d+)$/);
                        if (rangeMatch) return { bit_start: rangeMatch[1], bit_end: rangeMatch[2] };
                        if (/^\d+$/.test(bitIndex.trim())) return { bit_index: bitIndex.trim() };
                        return {};
                    })(),
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
        msg.textContent = `${dataRows.length} ${t('bulk.rowsPasted', { defaultValue: '행이 붙여넣기되었습니다.' })}`;
        msg.style.cssText = 'position: fixed; bottom: 80px; left: 50%; transform: translateX(-50%); background: #3b82f6; color: white; padding: 12px 24px; border-radius: 30px; z-index: 10000; box-shadow: 0 4px 6px rgba(0,0,0,0.1); font-weight: 500; animation: fadeOut 2s forwards; animation-delay: 1.5s;';
        document.body.appendChild(msg);
        setTimeout(() => msg.remove(), 4000);

    }, [deviceId]);

    const handleSaveAll = async () => {
        const validPoints = points.filter(p => p.name || p.address);

        if (validPoints.length === 0) {
            confirm({
                title: t('bulk.noData', { defaultValue: '데이터 없음' }),
                message: t('bulk.noDataToSave', { defaultValue: '저장할 데이터가 없습니다.' }),
                confirmButtonType: 'warning',
                showCancelButton: false
            });
            return;
        }

        // 중복(DB) 포인트와 기타 오류 포인트 분리
        const DUP_ADDR = t('bulk.errDuplicateAddr', { ns: 'devices', defaultValue: 'Address already exists (DB)' });
        const DUP_LIST = t('bulk.errDuplicateInList', { ns: 'devices', defaultValue: 'Duplicate address in list' });
        const duplicatePoints = validPoints.filter(p =>
            p.errors?.some(e => e === DUP_ADDR || e === DUP_LIST)
        );
        const otherErrorPoints = validPoints.filter(p =>
            !p.isValid && !p.errors?.every(e => e === DUP_ADDR || e === DUP_LIST)
        );

        // 중복 이외의 다른 오류가 있으면 막음
        if (otherErrorPoints.length > 0) {
            confirm({
                title: t('bulk.validationError', { defaultValue: '유효성 오류' }),
                message: `${otherErrorPoints.length} ${t('bulk.itemsHaveErrors', { defaultValue: '개 항목에 오류가 있습니다.' })}\n${t('bulk.checkRedCells', { defaultValue: '빨간색으로 표시된 셀을 확인해주세요.' })}`,
                confirmButtonType: 'danger',
                showCancelButton: false
            });
            return;
        }

        // 등록 가능한 포인트 (중복 제외)
        const toRegister = validPoints.filter(p => p.isValid);
        const skipCount = duplicatePoints.length;

        if (toRegister.length === 0) {
            confirm({
                title: t('bulk.noData', { defaultValue: '데이터 없음' }),
                message: t('bulk.allDuplicateMsg', { ns: 'devices', count: skipCount }),
                confirmButtonType: 'warning',
                showCancelButton: false
            });
            return;
        }

        const skipMsg = skipCount > 0 ? `\n${t('bulk.confirmSkipMsg', { ns: 'devices', count: skipCount })}` : '';
        const isConfirmed = await confirm({
            title: t('bulk.confirmBulkRegisterTitle', { defaultValue: '대량 등록 확인' }),
            message: `${t('bulk.confirmRegisterMsg', { ns: 'devices', count: toRegister.length })}${skipMsg}`,
            confirmText: t('bulk.register', { defaultValue: '등록' }),
            confirmButtonType: 'primary'
        });

        if (!isConfirmed) {
            return;
        }

        try {
            setIsProcessing(true);
            const payload = toRegister.map(({ isValid, errors, tempId, ...rest }) => {
                // Modbus: register_type → address_string 자동 생성 (예: HR:100)
                const regType = (rest as any).register_type;
                if (isModbus && regType && rest.address !== undefined && rest.address !== '') {
                    const addrNum = Number(rest.address);
                    // legacy 5자리 주소(40001~)가 아닐 때만 address_string 생성
                    if (!isNaN(addrNum) && addrNum < 40001) {
                        (rest as any).address_string = `${regType}:${addrNum}`;
                    }
                }
                // register_type은 DataPoint 스키마에 없는 임시 필드이므로 제거
                const { register_type, ...payload } = rest as any;
                return payload;
            });
            if (onImport) {
                onImport(payload);
                localStorage.removeItem(STORAGE_KEY);
                onClose();
            } else if (onSave) {
                await onSave(payload);
                localStorage.removeItem(STORAGE_KEY);
                onClose();
            }
            // 스킵 있었으면 완료 후 알림
            if (skipCount > 0) {
                setTimeout(() => {
                    confirm({
                        title: t('bulk.registerComplete', { ns: 'devices' }),
                        message: `${t('bulk.registerCompleteMsg', { ns: 'devices', success: toRegister.length })}\n${t('bulk.skipNoticeMsg', { ns: 'devices', count: skipCount })}`,
                        confirmButtonType: 'primary',
                        showCancelButton: false
                    });
                }, 300);
            }
        } catch (e) {
            console.error(e);
            confirm({
                title: t('bulk.saveError', { defaultValue: '저장 오류' }),
                message: t('bulk.saveOccurredError', { defaultValue: '저장 중 오류가 발생했습니다.' }),
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
                                <h2>{t('labels.bulkDataPointRegistration', { ns: 'devices' })}</h2>
                                <div className="history-controls">
                                    <button onClick={undo} disabled={historyIndex <= 0} title="Undo (Ctrl+Z)">
                                        <i className="fas fa-undo"></i>
                                    </button>
                                    <button onClick={redo} disabled={historyIndex >= history.length - 1} title="Redo (Ctrl+Y)">
                                        <i className="fas fa-redo"></i>
                                    </button>
                                </div>
                            </div>
                            <div className="instruction-box">
                                <p className="main-desc">{t('bulk.instruction1', { ns: 'devices' })}</p>
                                <ul className="usage-guide">
                                    <li>{t('bulk.instruction2', { ns: 'devices' })}</li>
                                    <li>{t('bulk.instruction3', { ns: 'devices' })}</li>
                                    <li>{t('bulk.instruction4', { ns: 'devices' })}</li>
                                </ul>
                            </div>
                        </div>
                        <button className="close-btn" onClick={onClose} title="Close">
                            <i className="fas fa-times"></i>
                        </button>
                    </div>
                </header>

                <main id="bulk-main">
                    {/* 🔄 임시저장 복원 배너 */}
                    {draftRestoredCount !== null && (
                        <div style={{
                            display: 'flex', alignItems: 'center', gap: 10,
                            padding: '8px 16px',
                            background: 'linear-gradient(90deg, #eff6ff 0%, #dbeafe 100%)',
                            borderBottom: '1px solid #bfdbfe',
                            fontSize: 13, color: '#1d4ed8',
                            flexShrink: 0,
                        }}>
                            <i className="fas fa-history" style={{ fontSize: 14 }} />
                            <span>
                                <strong>임시저장된 데이터 {draftRestoredCount}건이 복원되었습니다.</strong>
                                &nbsp;작업을 이어서 진행하세요.
                            </span>
                            <button
                                onClick={handleReset}
                                style={{
                                    marginLeft: 'auto', padding: '3px 10px',
                                    background: 'white', border: '1px solid #93c5fd',
                                    borderRadius: 6, color: '#1d4ed8', fontSize: 12,
                                    cursor: 'pointer', fontWeight: 500
                                }}
                            >
                                지우고 새로 시작
                            </button>
                        </div>
                    )}
                    <div id="grid-outer-container">
                        {/* 헤더 + 바디를 하나의 스크롤 컨테이너 안에 통합 */}
                        <div id="grid-body-scroll-area" ref={tableContainerRef} className="custom-scrollbar">
                            <table className="excel-table-fixed">
                                <thead>
                                    <tr>
                                        <th className="col-chk" style={{ width: 36, textAlign: 'center' }}>
                                            <input type="checkbox"
                                                checked={selectedRows.size > 0 && points.filter(p => p.name || p.address).every(p => selectedRows.has(p.tempId))}
                                                onChange={e => {
                                                    if (e.target.checked) {
                                                        setSelectedRows(new Set(points.filter(p => p.name || p.address).map(p => p.tempId)));
                                                    } else {
                                                        setSelectedRows(new Set());
                                                    }
                                                }}
                                                style={{ cursor: 'pointer' }}
                                                title="전체 선택"
                                            />
                                        </th>
                                        <th className="col-idx">#</th>
                                        <th className="col-name">{t('bulk.colName', { ns: 'devices' })}</th>
                                        <th className="col-addr">{protocolType === 'MQTT' ? 'Sub-Topic *' : t('bulk.colAddress', { ns: 'devices' })}</th>
                                        {protocolType === 'MQTT' && <th className="col-key">{t('labels.jsonKey', { ns: 'devices' })}</th>}
                                        {isModbus && <th className="col-regtype" title="Modbus 레지스터 타입 (HR=Holding FC3, IR=Input FC4, CO=Coil FC1, DI=Discrete FC2)">레지스터</th>}
                                        {isModbus && <th className="col-bit" title="Bit extraction index (0–15) for Modbus register bit-split">{t('bulk.colBit', { ns: 'devices' })}</th>}
                                        <th className="col-type">{t('modal.dpDiffType', { ns: 'devices' })}</th>
                                        <th className="col-mode">{t('modal.dpDiffAccess', { ns: 'devices' })}</th>
                                        <th className="col-desc">{t('dpTab.description', { ns: 'devices' })}</th>
                                        <th className="col-unit">{t('dpModal.unit', { ns: 'devices' })}</th>
                                        <th className="col-scale">{t('dpTab.scale', { ns: 'devices' })}</th>
                                        <th className="col-offset">{t('modal.dpDiffOffset', { ns: 'devices' })}</th>
                                        <th className="col-actions">{t('delete', { ns: 'common' })}</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {points.map((point, idx) => (
                                        <tr key={point.tempId} data-row-index={idx}
                                            style={selectedRows.has(point.tempId) ? { background: '#eff6ff' } : undefined}>
                                            <td className="col-chk" style={{ textAlign: 'center', width: 36 }}>
                                                <input type="checkbox"
                                                    checked={selectedRows.has(point.tempId)}
                                                    onClick={e => {
                                                        const isShift = (e as React.MouseEvent).shiftKey;
                                                        const isCtrl = (e as React.MouseEvent).ctrlKey || (e as React.MouseEvent).metaKey;
                                                        const currentlyChecked = selectedRows.has(point.tempId);
                                                        if (isShift && lastCheckedIdxRef.current >= 0) {
                                                            // Shift: 마지막 클릭~현재 사이 범위 선택
                                                            const from = Math.min(lastCheckedIdxRef.current, idx);
                                                            const to = Math.max(lastCheckedIdxRef.current, idx);
                                                            setSelectedRows(prev => {
                                                                const next = new Set(prev);
                                                                for (let i = from; i <= to; i++) next.add(points[i].tempId);
                                                                return next;
                                                            });
                                                        } else if (isCtrl) {
                                                            // Ctrl: 개별 토글
                                                            setSelectedRows(prev => {
                                                                const next = new Set(prev);
                                                                if (currentlyChecked) next.delete(point.tempId);
                                                                else next.add(point.tempId);
                                                                return next;
                                                            });
                                                            lastCheckedIdxRef.current = idx;
                                                        } else {
                                                            // 일반 클릭
                                                            setSelectedRows(prev => {
                                                                const next = new Set(prev);
                                                                if (currentlyChecked) next.delete(point.tempId);
                                                                else next.add(point.tempId);
                                                                return next;
                                                            });
                                                            lastCheckedIdxRef.current = idx;
                                                        }
                                                    }}
                                                    onChange={() => {/* onClick 에서 처리 */ }}
                                                    style={{ cursor: 'pointer' }}
                                                />
                                            </td>
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
                                            <td className={`col-addr ${(!point.isValid && !point.address) || point.errors.some(e => e.includes('duplicate')) ? 'cell-error' : ''}`}>
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
                                            {isModbus && (
                                                <td className="col-regtype">
                                                    <select
                                                        className="excel-select"
                                                        value={(point as any).register_type ?? 'HR'}
                                                        onChange={e => updatePoint(idx, 'register_type' as any, e.target.value)}
                                                        title="HR=Holding(FC3) | IR=Input(FC4) | CO=Coil(FC1) | DI=Discrete(FC2)"
                                                    >
                                                        <option value="HR">HR (FC3)</option>
                                                        <option value="IR">IR (FC4)</option>
                                                        <option value="CO">CO (FC1)</option>
                                                        <option value="DI">DI (FC2)</option>
                                                    </select>
                                                </td>
                                            )}
                                            {isModbus && (() => {
                                                const BIT_UNSUPPORTED = ['string', 'datetime', 'unknown', 'json'];
                                                const bitDisabled = BIT_UNSUPPORTED.includes((point.data_type as string)?.toLowerCase?.() ?? '');
                                                return (
                                                    <td className="col-bit" title={bitDisabled ? t('bulk.bitColDisabledTitle', { ns: 'devices' }) : t('bulk.bitColTitle', { ns: 'devices' })}
                                                        style={bitDisabled ? { opacity: 0.35, pointerEvents: 'none', background: '#f8fafc' } : undefined}>
                                                        <input
                                                            type="text"
                                                            className="excel-input text-center font-mono"
                                                            disabled={bitDisabled}
                                                            value={
                                                                (point.protocol_params as any)?.bit_start !== undefined
                                                                    ? `${(point.protocol_params as any).bit_start}~${(point.protocol_params as any).bit_end ?? ''}`
                                                                    : (point.protocol_params as any)?.bit_index ?? ''
                                                            }
                                                            onChange={e => updateBitSetting(idx, e.target.value)}
                                                            onKeyDown={e => handleKeyDown(e, idx, 2)}
                                                            placeholder={bitDisabled ? '-' : t('bulk.bitPlaceholder', { ns: 'devices' })}
                                                        />
                                                    </td>
                                                );
                                            })()}
                                            <td className="col-type">
                                                <select
                                                    className="excel-select"
                                                    value={point.data_type}
                                                    onChange={e => updatePoint(idx, 'data_type', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 3 : 3)}
                                                >
                                                    <option value="number">number</option>
                                                    <option value="boolean">boolean</option>
                                                    <option value="string">string</option>
                                                    <optgroup label="──────────" />
                                                    <option value="INT16">INT16 (2 bytes)</option>
                                                    <option value="UINT16">UINT16 (2 bytes)</option>
                                                    <option value="FLOAT32">FLOAT32 (4 bytes)</option>
                                                    <option value="BOOL">BOOL (1 bit)</option>
                                                    <optgroup label="32/64-bit" />
                                                    <option value="INT32">INT32 (4 bytes)</option>
                                                    <option value="UINT32">UINT32 (4 bytes)</option>
                                                    <option value="FLOAT64">FLOAT64 (8 bytes)</option>
                                                    <option value="INT64">INT64 (8 bytes)</option>
                                                    <optgroup label="기타" />
                                                    <option value="INT8">INT8 (1 byte)</option>
                                                    <option value="UINT8">UINT8 (1 byte)</option>
                                                    <option value="DATETIME">DATETIME</option>
                                                    <option value="UNKNOWN">Unknown</option>
                                                </select>
                                            </td>
                                            <td className="col-mode">
                                                <select
                                                    className="excel-select"
                                                    value={point.access_mode}
                                                    onChange={e => updatePoint(idx, 'access_mode', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 4 : 4)}
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
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 5 : 5)}
                                                />
                                            </td>
                                            <td className="col-unit">
                                                <input
                                                    className="excel-input"
                                                    value={point.unit || ''}
                                                    onChange={e => updatePoint(idx, 'unit', e.target.value)}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 6 : 6)}
                                                />
                                            </td>
                                            <td className="col-scale">
                                                <input
                                                    type="number"
                                                    className="excel-input text-right"
                                                    value={point.scaling_factor}
                                                    onChange={e => updatePoint(idx, 'scaling_factor', parseFloat(e.target.value))}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 7 : 7)}
                                                />
                                            </td>
                                            <td className="col-offset">
                                                <input
                                                    type="number"
                                                    className="excel-input text-right"
                                                    value={point.scaling_offset}
                                                    onChange={e => updatePoint(idx, 'scaling_offset', parseFloat(e.target.value))}
                                                    onKeyDown={e => handleKeyDown(e, idx, protocolType === 'MQTT' ? 8 : 8)}
                                                />
                                            </td>
                                            <td className="col-actions">
                                                <button onClick={() => removeRow(idx)} className="delete-row-btn" title="Delete row">
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
                                <i className="fas fa-plus"></i> {t('bulk.addRow', { ns: 'devices' })}
                            </button>
                            <button className="template-btn" onClick={() => setShowTemplateSelector(!showTemplateSelector)}>
                                <i className="fas fa-file-import"></i> {t('bulk.loadTemplate', { ns: 'devices' })}
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
                                    <i className="fas fa-code"></i> {t('bulk.parseJson', { ns: 'devices' })}
                                </button>
                            )}
                            {isModbus && (
                                <button
                                    onClick={() => { setShowBitSplitter(!showBitSplitter); setShowTemplateSelector(false); }}
                                    style={{
                                        padding: '8px 16px',
                                        background: showBitSplitter ? '#eff6ff' : '#fafafa',
                                        color: '#1d4ed8',
                                        border: '1px solid #bfdbfe',
                                        borderRadius: '6px',
                                        fontWeight: 700,
                                        fontSize: 13,
                                        cursor: 'pointer'
                                    }}
                                >
                                    <i className="fas fa-layer-group"></i> {t('bulk.bitSplit', { ns: 'devices' })}
                                </button>
                            )}
                            <button className="reset-btn" onClick={handleReset} title="Reset all input data">
                                <i className="fas fa-trash-alt"></i> {t('bulk.reset', { ns: 'devices' })}
                            </button>
                            {selectedRows.size > 0 && (
                                <button
                                    onClick={() => {
                                        setPoints(prev => {
                                            const next = prev.filter(p => !selectedRows.has(p.tempId));
                                            const filled = next.filter(p => p.name || p.address);
                                            while (filled.length < 20) filled.push(createEmptyRow());
                                            const revalidated = filled.map(p => validatePoint(p, filled));
                                            pushHistory(revalidated);
                                            return revalidated;
                                        });
                                        setSelectedRows(new Set());
                                    }}
                                    style={{
                                        display: 'flex', alignItems: 'center', gap: 6,
                                        padding: '7px 14px', borderRadius: 8, border: '1px solid #fca5a5',
                                        background: '#fef2f2', color: '#dc2626',
                                        fontSize: '13px', fontWeight: 600, cursor: 'pointer'
                                    }}
                                    title={`선택한 ${selectedRows.size}개 행 삭제`}
                                >
                                    <i className="fas fa-minus-circle"></i> 선택 삭제 ({selectedRows.size})
                                </button>
                            )}
                        </div>
                        {showTemplateSelector && (
                            <div className="template-selector-bubble">
                                <div className="bubble-header">
                                    <span>{t('labels.selectATemplate', { ns: 'devices' })}</span>
                                    <button onClick={() => setShowTemplateSelector(false)}><i className="fas fa-times"></i></button>
                                </div>
                                <div className="bubble-body custom-scrollbar">
                                    {isLoadingTemplates ? (
                                        <div className="loading-text">{t('loading', { ns: 'common' })}</div>
                                    ) : templates.length === 0 ? (
                                        <div className="empty-text">{t('labels.noTemplatesRegistered', { ns: 'devices' })}</div>
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

                        {/* 🔢 Bit Split 팝업 패널 — Modbus 전용 */}
                        {showBitSplitter && isModbus && (
                            <div style={{
                                position: 'absolute', bottom: '74px', left: '240px', width: '380px',
                                background: 'white', borderRadius: '12px', border: '1px solid #bfdbfe',
                                boxShadow: '0 10px 25px -5px rgba(0,0,0,0.2)', zIndex: 1000,
                                display: 'flex', flexDirection: 'column', overflow: 'hidden'
                            }}>
                                {/* 헤더 */}
                                <div style={{ padding: '12px 16px', background: '#eff6ff', borderBottom: '1px solid #bfdbfe', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                    <span style={{ fontSize: '13px', fontWeight: 700, color: '#1d4ed8' }}>
                                        <i className="fas fa-layer-group" style={{ marginRight: 6 }}></i>
                                        비트 분할 자동 생성
                                    </span>
                                    <button onClick={() => setShowBitSplitter(false)} style={{ background: 'none', border: 'none', cursor: 'pointer', color: '#94a3b8' }}><i className="fas fa-times"></i></button>
                                </div>
                                {/* 본문 */}
                                <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: 10 }}>

                                    {/* ── 주소 (단일/범위) ── */}
                                    <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
                                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                                            <span style={{ fontSize: 12, fontWeight: 600, color: '#475569' }}>기준 주소 (Address) *</span>
                                            <label style={{ fontSize: 11, color: '#64748b', display: 'flex', alignItems: 'center', gap: 4, cursor: 'pointer' }}>
                                                <input type="checkbox" checked={bitSplitConfig.useAddressRange}
                                                    onChange={e => setBitSplitConfig(p => ({ ...p, useAddressRange: e.target.checked }))}
                                                    style={{ cursor: 'pointer' }} />
                                                주소 범위
                                            </label>
                                        </div>
                                        {bitSplitConfig.useAddressRange ? (
                                            <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                                                <input type="text" value={bitSplitConfig.address}
                                                    onChange={e => setBitSplitConfig(p => ({ ...p, address: e.target.value }))}
                                                    placeholder="시작 예: 40001"
                                                    style={{ flex: 1, padding: '5px 8px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12, fontFamily: 'monospace' }}
                                                />
                                                <span style={{ fontSize: 12, color: '#94a3b8' }}>~</span>
                                                <input type="text" value={bitSplitConfig.addressEnd}
                                                    onChange={e => setBitSplitConfig(p => ({ ...p, addressEnd: e.target.value }))}
                                                    placeholder="끝 예: 40010"
                                                    style={{ flex: 1, padding: '5px 8px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12, fontFamily: 'monospace' }}
                                                />
                                            </div>
                                        ) : (
                                            <input type="text" value={bitSplitConfig.address}
                                                onChange={e => setBitSplitConfig(p => ({ ...p, address: e.target.value }))}
                                                placeholder="예: 40001"
                                                style={{ width: '100%', padding: '5px 8px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12, fontFamily: 'monospace' }}
                                            />
                                        )}
                                        {bitSplitConfig.useAddressRange && bitSplitConfig.address && bitSplitConfig.addressEnd && (() => {
                                            const s = parseInt(bitSplitConfig.address, 10);
                                            const e = parseInt(bitSplitConfig.addressEnd, 10);
                                            if (!isNaN(s) && !isNaN(e) && e >= s) {
                                                return <span style={{ fontSize: 11, color: '#0f766e' }}>✅ {e - s + 1}개 주소 생성 예정</span>;
                                            }
                                            return <span style={{ fontSize: 11, color: '#dc2626' }}>⚠ 끝 주소가 시작보다 크거나 같아야 합니다</span>;
                                        })()}
                                    </div>

                                    {/* ── 이름접두사 / 권한 / 설명 ── */}
                                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8 }}>
                                        <label style={{ fontSize: 12, fontWeight: 600, color: '#475569' }}>
                                            이름 접두사
                                            <input type="text" value={bitSplitConfig.namePrefix}
                                                onChange={e => setBitSplitConfig(p => ({ ...p, namePrefix: e.target.value }))}
                                                placeholder="비우면 주소 사용"
                                                style={{ display: 'block', width: '100%', marginTop: 4, padding: '5px 8px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12 }}
                                            />
                                        </label>
                                        <label style={{ fontSize: 12, fontWeight: 600, color: '#475569' }}>
                                            Access Mode
                                            <select value={bitSplitConfig.accessMode}
                                                onChange={e => setBitSplitConfig(p => ({ ...p, accessMode: e.target.value }))}
                                                style={{ display: 'block', width: '100%', marginTop: 4, padding: '5px 8px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12 }}>
                                                <option value="read">read</option>
                                                <option value="write">write</option>
                                                <option value="read_write">read_write</option>
                                            </select>
                                        </label>
                                        <label style={{ fontSize: 12, fontWeight: 600, color: '#475569' }}>
                                            설명 접두사
                                            <input type="text" value={bitSplitConfig.descPrefix}
                                                onChange={e => setBitSplitConfig(p => ({ ...p, descPrefix: e.target.value }))}
                                                placeholder="예: FC03 40001"
                                                style={{ display: 'block', width: '100%', marginTop: 4, padding: '5px 8px', border: '1px solid #cbd5e1', borderRadius: 6, fontSize: 12 }}
                                            />
                                        </label>
                                    </div>

                                    {/* ── 레지스터 크기 ── */}
                                    <div style={{ display: 'flex', gap: 6 }}>
                                        <span style={{ fontSize: 11, fontWeight: 600, color: '#64748b', lineHeight: '30px' }}>레지스터 크기:</span>
                                        {([16, 32] as const).map(b => (
                                            <button key={b} onClick={() => setBitSplitConfig(p => ({ ...p, registerBits: b }))}
                                                style={{
                                                    padding: '4px 14px', border: '1px solid', borderRadius: 5, fontSize: 11, fontWeight: 700, cursor: 'pointer',
                                                    borderColor: bitSplitConfig.registerBits === b ? '#0f766e' : '#cbd5e1',
                                                    background: bitSplitConfig.registerBits === b ? '#f0fdfa' : '#f8fafc',
                                                    color: bitSplitConfig.registerBits === b ? '#0f766e' : '#64748b'
                                                }}>
                                                {b}bit (0~{b - 1})
                                            </button>
                                        ))}
                                    </div>

                                    {/* ── 비트 범위 입력 (핵심) ── */}
                                    {(() => {
                                        const parsed = parseBitRanges(bitSplitConfig.customRanges, bitSplitConfig.registerBits);
                                        const hasError = parsed.some(r => r.error);
                                        const isBool = bitSplitConfig.mode === 'individual';
                                        return (
                                            <div style={{ background: '#f8fafc', borderRadius: 8, border: '1px solid #e2e8f0', padding: '10px 12px', display: 'flex', flexDirection: 'column', gap: 8 }}>
                                                {/* 범위 입력 필드 */}
                                                <label style={{ fontSize: 12, fontWeight: 600, color: '#334155' }}>
                                                    비트 범위 <span style={{ fontWeight: 400, color: '#94a3b8', fontSize: 11 }}>콤마(,)로 구분 · 대시(-)로 범위</span>
                                                    <input type="text"
                                                        value={bitSplitConfig.customRanges}
                                                        onChange={e => setBitSplitConfig(p => ({ ...p, customRanges: e.target.value }))}
                                                        placeholder="예: 0-3, 4-7, 8-11, 12-15"
                                                        style={{
                                                            display: 'block', width: '100%', marginTop: 4, padding: '7px 10px',
                                                            border: `1.5px solid ${hasError ? '#fca5a5' : '#cbd5e1'}`,
                                                            borderRadius: 6, fontSize: 13, fontFamily: 'monospace',
                                                            background: hasError ? '#fff5f5' : 'white'
                                                        }}
                                                    />
                                                </label>

                                                {/* 빠른 채우기 */}
                                                <div style={{ display: 'flex', gap: 4, flexWrap: 'wrap' }}>
                                                    {([
                                                        { label: t('bulk.quickFill2', { ns: 'devices' }), n: 2 },
                                                        { label: t('bulk.quickFill4', { ns: 'devices' }), n: 4 },
                                                        { label: t('bulk.quickFill8', { ns: 'devices' }), n: 8 },
                                                        { label: t('bulk.quickFillBitwise', { ns: 'devices' }), n: bitSplitConfig.registerBits },
                                                    ]).map(q => {
                                                        const sz = Math.floor(bitSplitConfig.registerBits / q.n);
                                                        const val = q.n === bitSplitConfig.registerBits
                                                            ? Array.from({ length: q.n }, (_, i) => `${i}-${i}`).join(', ')
                                                            : Array.from({ length: q.n }, (_, i) => `${i * sz}-${(i + 1) * sz - 1}`).join(', ');
                                                        return (
                                                            <button key={q.label}
                                                                onClick={() => setBitSplitConfig(p => ({ ...p, customRanges: val }))}
                                                                style={{
                                                                    padding: '3px 10px', fontSize: 10, fontWeight: 600,
                                                                    border: '1px solid #cbd5e1', borderRadius: 4,
                                                                    background: '#f1f5f9', color: '#475569', cursor: 'pointer'
                                                                }}>
                                                                {q.label}
                                                            </button>
                                                        );
                                                    })}
                                                </div>

                                                {/* 값 타입 */}
                                                <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                                                    <span style={{ fontSize: 11, fontWeight: 600, color: '#64748b' }}>{t('bulk.valueTypeLabel', { ns: 'devices' })}:</span>
                                                    <button onClick={() => setBitSplitConfig(p => ({ ...p, mode: 'individual' }))}
                                                        style={{
                                                            padding: '3px 12px', border: '1.5px solid', borderRadius: 5, fontSize: 11, fontWeight: 700, cursor: 'pointer',
                                                            borderColor: isBool ? '#1d4ed8' : '#e2e8f0',
                                                            background: isBool ? '#eff6ff' : '#f8fafc',
                                                            color: isBool ? '#1d4ed8' : '#94a3b8'
                                                        }}>
                                                        boolean (0/1)
                                                    </button>
                                                    <button onClick={() => setBitSplitConfig(p => ({ ...p, mode: 'group' }))}
                                                        style={{
                                                            padding: '3px 12px', border: '1.5px solid', borderRadius: 5, fontSize: 11, fontWeight: 700, cursor: 'pointer',
                                                            borderColor: !isBool ? '#7c3aed' : '#e2e8f0',
                                                            background: !isBool ? '#f5f3ff' : '#f8fafc',
                                                            color: !isBool ? '#7c3aed' : '#94a3b8'
                                                        }}>
                                                        number (0~N)
                                                    </button>
                                                    <span style={{ fontSize: 10, color: '#94a3b8' }}>
                                                        {isBool ? t('bulk.valueTypeBoolDesc', { ns: 'devices' }) : t('bulk.valueTypeNumDesc', { ns: 'devices' })}
                                                    </span>
                                                </div>

                                                {/* 유효성 미리보기 */}
                                                <div style={{
                                                    fontSize: 11, padding: '5px 8px', borderRadius: 5,
                                                    background: hasError ? '#fef2f2' : '#f0fdf4',
                                                    color: hasError ? '#dc2626' : '#166534',
                                                    borderLeft: `3px solid ${hasError ? '#fca5a5' : '#86efac'}`
                                                }}>
                                                    {parsed.length === 0 ? <span style={{ color: '#94a3b8' }}>{t('bulk.previewEnterRange', { ns: 'devices' })}</span> :
                                                        hasError
                                                            ? parsed.filter(r => r.error).map((r, i) => <div key={i}>⚠ {r.error}</div>)
                                                            : <>
                                                                ✅ {isBool
                                                                    ? t('bulk.previewBoolPoints', { ns: 'devices', count: parsed.reduce((s, r) => s + r.end - r.start + 1, 0) })
                                                                    : t('bulk.previewNumPoints', { ns: 'devices', count: parsed.length })}
                                                                <br />
                                                                {(() => {
                                                                    const SHOW = 4;
                                                                    const items = isBool
                                                                        ? parsed.flatMap(r =>
                                                                            r.start === r.end
                                                                                ? [`Bit${r.start}`]
                                                                                : [`Bit${r.start}~${r.end}`])
                                                                        : parsed.map((r, i) =>
                                                                            `V${i + 1}(bit${r.start}~${r.end}→0~${(1 << (r.end - r.start + 1)) - 1})`);
                                                                    const visible = items.slice(0, SHOW);
                                                                    const rest = items.length - SHOW;
                                                                    return <>
                                                                        {visible.map((txt, i) => <span key={i} style={{ marginRight: 6 }}>{txt}</span>)}
                                                                        {rest > 0 && <span style={{ color: '#94a3b8' }}>{t('bulk.previewMore', { ns: 'devices', count: rest })}</span>}
                                                                    </>;
                                                                })()}
                                                            </>}
                                                </div>
                                            </div>
                                        );
                                    })()}

                                    <button onClick={handleBitSplit}
                                        style={{
                                            padding: '9px 0',
                                            background: bitSplitConfig.mode === 'individual' ? '#1d4ed8' : '#7c3aed',
                                            color: 'white', border: 'none', borderRadius: 7, fontWeight: 700, fontSize: 13, cursor: 'pointer', width: '100%'
                                        }}>
                                        <i className="fas fa-magic" style={{ marginRight: 6 }} /> 자동 생성
                                    </button>
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
                                    <span style={{ fontSize: '12px', fontWeight: 700, color: '#166534' }}>{t('labels.parseJsonSampleData', { ns: 'devices' })}</span>
                                    <button onClick={() => setShowJsonParser(false)} style={{ background: 'none', border: 'none', cursor: 'pointer', color: '#94a3b8' }}><i className="fas fa-times"></i></button>
                                </div>
                                <div style={{ padding: '16px' }}>
                                    <p style={{ fontSize: '11px', color: '#64748b', marginBottom: '8px' }}>
                                        Paste the JSON payload received from the device below.<br />
                                        Key structure will be auto-analyzed and rows generated.
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
                                        {t('bulk.analyzeAndGenerate', { ns: 'devices' })}
                                    </button>
                                </div>
                            </div>
                        )}
                        <span className="stats-text" style={{ marginLeft: '12px' }}>
                            {t('bulk.totalEntered', { ns: 'devices', count: points.filter(p => p.name || p.address).length })}
                        </span>
                    </div>
                    <div className="footer-right">
                        <button className="cancel-btn" onClick={onClose}>{t('cancel', { ns: 'common' })}</button>
                        <button
                            className={`save-btn ${isProcessing || points.some(p => (p.name || p.address) && !p.isValid) || !points.some(p => p.name || p.address) ? 'disabled' : ''}`}
                            onClick={handleSaveAll}
                            disabled={isProcessing || points.some(p => (p.name || p.address) && !p.isValid) || !points.some(p => p.name || p.address)}
                        >
                            {isProcessing ? 'Saving...' : 'Bulk Register'}
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
                /* 헤더/바디 통합 스크롤 구조 */
                #grid-body-scroll-area {
                    flex: 1;
                    overflow-y: auto;
                    overflow-x: auto;
                    background: white;
                }
                .excel-table-fixed thead {
                    position: sticky;
                    top: 0;
                    z-index: 2;
                    background: #f1f5f9;
                    border-bottom: 2px solid #cbd5e1;
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
                .col-bit { width: 85px; text-align: center; }
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
