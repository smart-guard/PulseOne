import React from 'react';
import {
    ResponsiveContainer,
    LineChart,
    Line,
    XAxis,
    YAxis,
    CartesianGrid,
    Tooltip,
    Legend
} from 'recharts';

interface TrendChartProps {
    historicalData: any[];
    selectedDataPoints: any[];
    timeRange: string;
    setTimeRange: (range: string) => void;
    isChartLoading: boolean;
}

const TrendChart: React.FC<TrendChartProps> = ({
    historicalData,
    selectedDataPoints,
    timeRange,
    setTimeRange,
    isChartLoading,
}) => {
    return (
        <div className="chart-container-wrapper" style={{
            marginBottom: '20px',
            padding: '16px',
            border: '1px solid #e5e7eb',
            borderRadius: '6px',
            backgroundColor: '#f9fafb'
        }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
                <h4 style={{ margin: 0, fontSize: '14px', fontWeight: 600 }}>📈 트렌드 분석</h4>
                <div className="time-range-selector">
                    {['1h', '6h', '12h', '24h'].map(range => (
                        <button
                            key={range}
                            onClick={() => setTimeRange(range)}
                            className={`btn btn-xs ${timeRange === range ? 'btn-primary' : 'btn-outline'}`}
                            style={{ marginLeft: '4px', fontSize: '11px', padding: '2px 8px' }}
                        >
                            {range}
                        </button>
                    ))}
                </div>
            </div>

            <div style={{
                height: '250px',
                backgroundColor: '#ffffff',
                border: '1px solid #f3f4f6',
                borderRadius: '6px',
                position: 'relative',
                overflow: 'hidden'
            }}>
                {/* 로딩 오버레이 */}
                {isChartLoading && (
                    <div style={{
                        position: 'absolute',
                        inset: 0,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        backgroundColor: 'rgba(255, 255, 255, 0.6)',
                        backdropFilter: 'blur(1px)',
                        zIndex: 10
                    }}>
                        <div className="loading-spinner-small"></div>
                    </div>
                )}

                {/* 데이터 없음 메시지 (로딩 중이 아닐 때만) */}
                {historicalData.length === 0 && !isChartLoading && (
                    <div style={{
                        position: 'absolute',
                        inset: 0,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        color: '#9ca3af',
                        fontSize: '12px',
                        zIndex: 5
                    }}>
                        데이터가 없습니다 (InfluxDB 연결 확인)
                    </div>
                )}

                <ResponsiveContainer width="100%" height="100%">
                    <LineChart
                        data={historicalData}
                        margin={{ top: 20, right: 30, left: 0, bottom: 0 }}
                    >
                        <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#f3f4f6" />
                        <XAxis
                            dataKey="time"
                            fontSize={11}
                            tick={{ fill: '#6b7280' }}
                            axisLine={{ stroke: '#e5e7eb' }}
                        />
                        <YAxis
                            fontSize={11}
                            tick={{ fill: '#6b7280' }}
                            axisLine={{ stroke: '#e5e7eb' }}
                        />
                        <Tooltip
                            contentStyle={{
                                fontSize: '12px',
                                borderRadius: '8px',
                                border: 'none',
                                boxShadow: '0 10px 15px -3px rgb(0 0 0 / 0.1)'
                            }}
                        />
                        <Legend
                            iconType="circle"
                            wrapperStyle={{ fontSize: '11px', paddingTop: '10px' }}
                        />
                        {selectedDataPoints.map((dp, idx) => (
                            <Line
                                key={dp.point_id}
                                type="monotone"
                                dataKey={`p_${dp.point_id}`}
                                name={dp.point_name}
                                stroke={['#3b82f6', '#10b981', '#f59e0b', '#ef4444', '#8b5cf6'][idx % 5]}
                                strokeWidth={2}
                                dot={historicalData.length < 50 ? { r: 3 } : false}
                                activeDot={{ r: 5 }}
                                isAnimationActive={!isChartLoading}
                            />
                        ))}
                    </LineChart>
                </ResponsiveContainer>
            </div>
        </div>
    );
};

export default TrendChart;
