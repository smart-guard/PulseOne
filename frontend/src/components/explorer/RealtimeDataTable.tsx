import React from 'react';
import { useTranslation } from 'react-i18next';
import Pagination from '../../components/common/Pagination';
import { isBlobValue, getBlobDownloadUrl } from '../../utils/dataUtils';

interface RealtimeValue {
    key: string;
    point_id?: number;
    point_name: string;
    device_name: string;
    value: any;
    quality: string;
    data_type: string;
    unit?: string;
    timestamp: string;
}

interface RealtimeDataTableProps {
    filteredDataPoints: RealtimeValue[];
    selectedDataPoints: RealtimeValue[];
    paginatedDataPoints: RealtimeValue[];
    handleToggleSelectAll: () => void;
    handleDataPointSelect: (dp: RealtimeValue) => void;
    pagination: {
        currentPage: number;
        totalCount: number;
        pageSize: number;
        pageSizeOptions: number[];
        goToPage: (page: number) => void;
        changePageSize: (size: number) => void;
    };
    selectedNode: any;
    renderEmptyDeviceMessage: (node: any) => React.ReactNode;
}

const RealtimeDataTable: React.FC<RealtimeDataTableProps> = ({
    filteredDataPoints,
    selectedDataPoints,
    paginatedDataPoints,
    handleToggleSelectAll,
    handleDataPointSelect,
    pagination,
    selectedNode,
    renderEmptyDeviceMessage,
}) => {
    const { t } = useTranslation(['common']);
    return (
        <div className="realtime-data">
            <h4>
                ⚡ {t('realtimeData', { ns: 'common' })} ({filteredDataPoints.length})
            </h4>

            {filteredDataPoints.length === 0 ? (
                selectedNode && ['device', 'master', 'slave'].includes(selectedNode.type) ?
                    renderEmptyDeviceMessage(selectedNode) : (
                        <div className="empty-state">
                            <p style={{ margin: '0 0 8px 0' }}>{t('noDataToDisplay', { ns: 'common' })}</p>
                            <small>{t('selectDeviceHint', { ns: 'common' })}</small>
                        </div>
                    )
            ) : (
                <div className="data-table-container">
                    {/* 테이블 헤더 */}
                    <div className="data-table-header">
                        <div className="header-cell cell-checkbox">
                            <input
                                type="checkbox"
                                checked={filteredDataPoints.length > 0 && selectedDataPoints.length === filteredDataPoints.length}
                                onChange={handleToggleSelectAll}
                                style={{ cursor: 'pointer' }}
                            />
                        </div>
                        <div className="header-cell">{t('pointName', { ns: 'common' })}</div>
                        <div className="header-cell">{t('device', { ns: 'common' })}</div>
                        <div className="header-cell">{t('currentValue', { ns: 'common' })}</div>
                        <div className="header-cell">{t('quality', { ns: 'common' })}</div>
                        <div className="header-cell">{t('type', { ns: 'common' })}</div>
                        <div className="header-cell">{t('time', { ns: 'common' })}</div>
                    </div>

                    {/* 테이블 바디 */}
                    <div className="data-table-body">
                        {paginatedDataPoints.map((dataPoint, index) => (
                            <div key={dataPoint.key || `row-${index}`} className="data-table-row">
                                <div className="table-cell cell-checkbox" data-label="선택">
                                    <input
                                        type="checkbox"
                                        checked={selectedDataPoints.some(dp => dp.key === dataPoint.key)}
                                        onChange={() => handleDataPointSelect(dataPoint)}
                                    />
                                </div>

                                <div className="table-cell cell-point" data-label="포인트명">
                                    <div className="point-info">
                                        <div className="point-name">
                                            {dataPoint.point_name || '[포인트명 없음]'}
                                        </div>
                                        <div className="point-key">
                                            {(dataPoint.key || '').replace('device:', '').replace(/:/g, '/')}
                                        </div>
                                    </div>
                                </div>

                                <div className="table-cell cell-device" data-label="디바이스">
                                    <div className="device-name">
                                        {(dataPoint.device_name || '').replace('-CTRL-', '').replace('-01', '')}
                                    </div>
                                </div>

                                <div className="table-cell cell-value" data-label="현재값">
                                    <div className="value-display">
                                        <span className={`value ${dataPoint.quality || 'unknown'}`}>
                                            {isBlobValue(dataPoint.value) ? (
                                                <a href={getBlobDownloadUrl(dataPoint.value as string)} className="blob-download-link" title="Download File">
                                                    <i className="fas fa-file-download"></i> FILE
                                                </a>
                                            ) : (
                                                String(dataPoint.value || '—')
                                            )}
                                            {dataPoint.unit && <span className="unit">{dataPoint.unit}</span>}
                                        </span>
                                    </div>
                                </div>

                                <div className="table-cell cell-quality" data-label="품질">
                                    <span className={`quality-badge ${dataPoint.quality || 'unknown'}`}>
                                        {dataPoint.quality === 'good' ? 'OK' :
                                            dataPoint.quality === 'comm_failure' ? 'ERR' :
                                                dataPoint.quality === 'last_known' ? 'OLD' :
                                                    dataPoint.quality === 'uncertain' ? '?' :
                                                        dataPoint.quality || '—'}
                                    </span>
                                </div>

                                <div className="table-cell cell-type" data-label="타입">
                                    <span className="data-type">
                                        {dataPoint.data_type === 'number' ? 'NUM' :
                                            dataPoint.data_type === 'boolean' ? 'BOOL' :
                                                dataPoint.data_type === 'integer' ? 'INT' :
                                                    dataPoint.data_type === 'string' ? 'STR' : 'UNK'}
                                    </span>
                                </div>

                                <div className="table-cell cell-time" data-label="업데이트">
                                    <span className="timestamp">
                                        {dataPoint.timestamp ?
                                            new Date(dataPoint.timestamp).toLocaleTimeString('ko-KR', {
                                                hour12: false,
                                                hour: '2-digit',
                                                minute: '2-digit',
                                                second: '2-digit'
                                            }) + '.' + String(new Date(dataPoint.timestamp).getMilliseconds()).padStart(3, '0') : '—'}
                                    </span>
                                </div>
                            </div>
                        ))}
                    </div>

                    <Pagination
                        current={pagination.currentPage}
                        total={pagination.totalCount}
                        pageSize={pagination.pageSize}
                        pageSizeOptions={pagination.pageSizeOptions}
                        showSizeChanger={true}
                        showTotal={true}
                        onChange={(page) => pagination.goToPage(page)}
                        onShowSizeChange={(page, pageSize) => pagination.changePageSize(pageSize)}
                        className="pagination-wrapper"
                    />
                </div>
            )}
        </div>
    );
};

export default RealtimeDataTable;
