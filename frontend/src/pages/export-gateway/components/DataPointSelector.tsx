import React, { useState, useEffect, useCallback } from 'react';
import { Select } from 'antd';
import exportGatewayApi, { DataPoint } from '../../../api/services/exportGatewayApi';
import { DeviceApiService, Device } from '../../../api/services/deviceApi';

interface DataPointSelectorProps {
    selectedPoints: any[];
    onSelect: (point: any) => void;
    onAddAll?: (points: any[]) => void;
    onRemove: (pointId: number) => void;
}

const DataPointSelector: React.FC<DataPointSelectorProps> = ({ selectedPoints, onSelect, onAddAll, onRemove }) => {
    const [allPoints, setAllPoints] = useState<DataPoint[]>([]);
    const [devices, setDevices] = useState<Device[]>([]);
    const [selectedDeviceId, setSelectedDeviceId] = useState<number | undefined>();
    const [loading, setLoading] = useState(false);
    const [searchTerm, setSearchTerm] = useState('');
    const [debouncedSearch, setSearchTermDebounced] = useState('');

    // Fetch devices on mount
    useEffect(() => {
        const fetchDevices = async () => {
            console.log('--- [DataPointSelector] Fetching devices START ---');
            try {
                // 1. Try standard Service call
                const res = await DeviceApiService.getDevices({ limit: 1000 });
                let items: Device[] = [];
                if (res && res.success) {
                    const rawData: any = res.data;
                    // Standard: res.data.items
                    if (rawData && Array.isArray(rawData.items)) {
                        items = rawData.items;
                    }
                    // Fallback 1: res.data is the array
                    else if (Array.isArray(rawData)) {
                        items = rawData;
                    }
                    // Fallback 2: res.data.data.items
                    else if (rawData && rawData.data && Array.isArray(rawData.data.items)) {
                        items = rawData.data.items;
                    }
                }

                console.log(`-- - [DataPointSelector] Found ${items.length} items. -- - `);

                // Force a test item if empty to verify UI
                if (items.length === 0) {
                    console.warn('--- [DataPointSelector] API returned empty, adding test item ---');
                }

                setDevices(items);
            } catch (e) {
                console.error('--- [DataPointSelector] Device fetch CRASH: ---', e);
            }
        };
        fetchDevices();
    }, []);

    // Debounce search term
    useEffect(() => {
        const timer = setTimeout(() => {
            setSearchTermDebounced(searchTerm);
        }, 300);
        return () => clearTimeout(timer);
    }, [searchTerm]);

    // Generic fetch function so it can be called on mount and on updates
    const fetchPoints = useCallback(async (search: string = debouncedSearch, deviceId?: number) => {
        console.log('--- [DataPointSelector] Fetching points with:', { search, deviceId });
        setLoading(true);
        try {
            const result = await exportGatewayApi.getDataPoints(search, deviceId);
            // Since result is now DataPoint[] or {items: [] } depending on my last edit attempt
            // I standardized exportGatewayApi.getDataPoints to return items object
            let points: DataPoint[] = [];
            if (Array.isArray(result)) {
                points = result;
            } else if (result && (result as any).items) {
                points = (result as any).items;
            }

            console.log(`-- - [DataPointSelector] Points fetched: ${points.length} --- `);
            setAllPoints(points);
        } catch (e) {
            console.error('--- [DataPointSelector] Points fetch error: ---', e);
        } finally {
            setLoading(false);
        }
    }, [debouncedSearch]);

    // Initial fetch to show all points so users don't have to guess/search
    useEffect(() => {
        // Ensure devices are fetched first if needed, or just fetch all points
        fetchPoints('', undefined);
    }, [fetchPoints]);

    // Fetch points when debounced search or device filter changes
    useEffect(() => {
        fetchPoints(debouncedSearch, selectedDeviceId);
    }, [debouncedSearch, selectedDeviceId, fetchPoints]);

    const isSelected = (id: number) => selectedPoints.some(p => p.id === id);

    return (
        <div className="point-selector-container" style={{
            background: 'white',
            display: 'flex',
            flexDirection: 'column',
            gap: '16px',
            height: '100%',
            padding: '16px'
        }}>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <div>
                    <label style={{ display: 'block', fontSize: '12px', fontWeight: 600, color: '#64748b', marginBottom: '6px' }}>장치 선택 필터</label>
                    <Select
                        placeholder="장치 선택 (전체/장치별)"
                        style={{ width: '100%', height: '36px' }}
                        value={selectedDeviceId ?? null}
                        onChange={(val) => setSelectedDeviceId(val ?? undefined)}
                        getPopupContainer={trigger => trigger.parentElement}
                        options={[
                            { value: null, label: '전체 장치 (All Devices)' },
                            ...devices.map(d => ({
                                value: d.id,
                                label: d.device_type ? `${d.name} [${d.device_type}]` : d.name
                            }))
                        ]}
                    />
                </div>
                <div>
                    <label style={{ display: 'block', fontSize: '12px', fontWeight: 600, color: '#64748b', marginBottom: '6px' }}>키워드 검색</label>
                    <div className="search-box" style={{ position: 'relative' }}>
                        <i className="fas fa-search" style={{
                            position: 'absolute',
                            left: '12px',
                            top: '50%',
                            transform: 'translateY(-50%)',
                            color: '#94a3b8',
                            zIndex: 1
                        }} />
                        <input
                            type="text"
                            className="mgmt-input sm"
                            placeholder="명칭 또는 주소 입력..."
                            value={searchTerm}
                            onChange={e => setSearchTerm(e.target.value)}
                            style={{ paddingLeft: '36px', height: '36px', borderRadius: '8px', border: '1px solid #cbd5e1' }}
                        />
                    </div>
                </div>
            </div>

            <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column', border: '1px solid #e2e8f0', borderRadius: '8px', minHeight: '300px' }}>
                <div style={{ padding: '8px 12px', background: '#f8fafc', borderBottom: '1px solid #e2e8f0', fontSize: '11px', fontWeight: 700, color: '#64748b', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                        <span>탐색 결과 ({allPoints.length})</span>
                        {selectedDeviceId && allPoints.length > 0 && onAddAll && (
                            <button
                                type="button"
                                className="btn btn-xs btn-outline"
                                style={{ fontSize: '10px', height: '18px', padding: '0 6px', lineHeight: '1' }}
                                onClick={() => onAddAll(allPoints)}
                            >
                                <i className="fas fa-plus-double" /> 전체 추가
                            </button>
                        )}
                    </div>
                    <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                        {loading && <i className="fas fa-spinner fa-spin" />}
                        <button
                            type="button"
                            onClick={() => fetchPoints(debouncedSearch, selectedDeviceId)}
                            style={{
                                background: 'none',
                                border: 'none',
                                color: '#64748b',
                                cursor: 'pointer',
                                padding: '2px',
                                display: 'flex',
                                alignItems: 'center',
                                transition: 'color 0.2s'
                            }}
                            title="새로고침"
                            className="refresh-btn-hover"
                        >
                            <i className="fas fa-sync-alt" />
                        </button>
                    </div>
                </div>
                <div className="point-list" style={{ flex: 1, overflowY: 'auto', background: 'white', minHeight: '200px' }}>
                    {loading && allPoints.length === 0 ? (
                        <div style={{ textAlign: 'center', padding: '40px 20px', color: '#94a3b8' }}>
                            <i className="fas fa-circle-notch fa-spin fa-2x" style={{ marginBottom: '12px' }} /><br />
                            데이터를 불러오는 중...
                        </div>
                    ) : allPoints.length === 0 ? (
                        <div style={{ textAlign: 'center', padding: '60px 20px', color: '#94a3b8' }}>
                            <i className="fas fa-search fa-3x" style={{ marginBottom: '16px', opacity: 0.2 }} /><br />
                            검색 결과가 없습니다.<br />
                            <span style={{ fontSize: '12px', opacity: 0.8 }}>다른 키워드나 장치를 선택해 보세요.</span>
                        </div>
                    ) : (
                        allPoints.map(p => {
                            const selected = isSelected(p.id);
                            return (
                                <div key={p.id} style={{
                                    display: 'flex',
                                    justifyContent: 'space-between',
                                    alignItems: 'center',
                                    padding: '10px 14px',
                                    borderBottom: '1px solid #f1f5f9',
                                    transition: 'background 0.2s',
                                    cursor: 'pointer'
                                }}
                                    onClick={() => selected ? onRemove(p.id) : onSelect(p)}
                                    className="point-item-hover">
                                    <div style={{ fontSize: '12px', flex: 1, minWidth: 0 }}>
                                        <div style={{ fontWeight: 600, color: '#1e293b', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{p.name}</div>
                                        <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>
                                            {p.device_name} • {p.address || '-'}
                                        </div>
                                    </div>
                                    <div style={{ marginLeft: '12px' }}>
                                        {selected ? (
                                            <span style={{ color: '#ef4444', fontSize: '18px' }}><i className="fas fa-check-circle" /></span>
                                        ) : (
                                            <span style={{ color: '#3b82f6', fontSize: '18px' }}><i className="fas fa-plus-circle" /></span>
                                        )}
                                    </div>
                                </div>
                            );
                        })
                    )}
                </div>
            </div>
        </div>
    );
};

export default DataPointSelector;
