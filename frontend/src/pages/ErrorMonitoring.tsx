import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { AlertTriangle, Wifi, Settings, Wrench, Zap, AlertCircle } from 'lucide-react';

// PulseOne 에러 대시보드 컴포넌트
const PulseOneErrorDashboard = () => {
  const [errors, setErrors] = useState<any[]>([]);
    const { t } = useTranslation(['errorMonitoring', 'common']);
  interface ErrorStats { totalErrors: number; connectionErrors: number; protocolErrors: number; deviceErrors: number; maintenanceEvents: number; systemErrors: number; }
  const [errorStats, setErrorStats] = useState<ErrorStats>({ totalErrors: 0, connectionErrors: 0, protocolErrors: 0, deviceErrors: 0, maintenanceEvents: 0, systemErrors: 0 });
  const [selectedDevice, setSelectedDevice] = useState<string | null>(null);

  // HTTP 상태코드별 아이콘 매핑
  const getErrorIcon = (httpStatus) => {
    if (httpStatus >= 420 && httpStatus <= 429) return <Wifi className="h-5 w-5" />;
    if (httpStatus >= 470 && httpStatus <= 489) return <Zap className="h-5 w-5" />;
    if (httpStatus >= 510 && httpStatus <= 519) return <Wrench className="h-5 w-5" />;
    if (httpStatus >= 490 && httpStatus <= 499) return <Settings className="h-5 w-5" />;
    return <AlertTriangle className="h-5 w-5" />;
  };

  // HTTP 상태코드별 색상
  const getErrorColor = (httpStatus) => {
    if (httpStatus >= 420 && httpStatus <= 429) return 'border-red-500 bg-red-50';  // 연결
    if (httpStatus >= 430 && httpStatus <= 449) return 'border-orange-500 bg-orange-50';  // 프로토콜
    if (httpStatus >= 450 && httpStatus <= 469) return 'border-yellow-500 bg-yellow-50';  // 데이터
    if (httpStatus >= 470 && httpStatus <= 489) return 'border-red-600 bg-red-100';  // 디바이스
    if (httpStatus >= 490 && httpStatus <= 499) return 'border-purple-500 bg-purple-50';  // 설정
    if (httpStatus >= 510 && httpStatus <= 519) return 'border-blue-500 bg-blue-50';  // 점검
    if (httpStatus >= 530) return 'border-indigo-500 bg-indigo-50';  // 프로토콜별
    return 'border-gray-500 bg-gray-50';
  };

  // 에러 이름 매핑
  const getErrorName = (httpStatus) => {
    const names = {
      420: 'Connection Failed', 421: 'Connection Timeout', 422: 'Connection Refused', 423: 'Connection Lost',
      430: 'Timeout', 431: 'Protocol Error', 434: 'Checksum Error', 435: 'Frame Error',
      450: 'Invalid Data', 451: 'Type Mismatch', 452: 'Out of Range', 453: 'Format Error',
      470: 'No Response', 471: 'Busy', 472: 'Hardware Error', 473: 'Device Not Found', 474: 'Offline',
      490: 'Config Error', 491: 'Config Missing', 492: 'Config Error', 493: 'Parameter Error',
      510: 'Maintenance', 513: 'Remote Control Blocked', 514: 'Insufficient Permission',
      530: 'Modbus Exception', 540: 'MQTT Failure', 550: 'BACnet Error'
    };
    return names[httpStatus] || `Error ${httpStatus}`;
  };

  // 샘플 에러 데이터
  useEffect(() => {
    const sampleErrors = [
      { id: 1, httpStatus: 420, deviceId: 'PLC-001', message: 'Connection Failed', timestamp: new Date(Date.now() - 1000 * 60 * 5), category: 'connection' },
      { id: 2, httpStatus: 472, deviceId: 'HMI-002', message: 'Hardware Error', timestamp: new Date(Date.now() - 1000 * 60 * 10), category: 'device' },
      { id: 3, httpStatus: 530, deviceId: 'RTU-003', message: 'Modbus Exception', timestamp: new Date(Date.now() - 1000 * 60 * 15), category: 'modbus' },
      { id: 4, httpStatus: 510, deviceId: 'CTRL-004', message: 'Maintenance', timestamp: new Date(Date.now() - 1000 * 60 * 20), category: 'maintenance' },
      { id: 5, httpStatus: 421, deviceId: 'PLC-005', message: 'Connection Timeout', timestamp: new Date(Date.now() - 1000 * 60 * 25), category: 'connection' }
    ];
    setErrors(sampleErrors);

    const stats = {
      totalErrors: 45,
      connectionErrors: 15,
      deviceErrors: 12,
      protocolErrors: 10,
      maintenanceEvents: 5,
      systemErrors: 3
    };
    setErrorStats(stats);
  }, []);

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <h1 className="text-3xl font-bold text-gray-900 mb-8">{t('pageTitle', {ns: 'errorMonitoring'})}</h1>

      {/* 에러 통계 카드들 */}
      <div className="grid grid-cols-1 md:grid-cols-3 lg:grid-cols-6 gap-4 mb-8">
        <div className="bg-white rounded-lg shadow p-6 border-l-4 border-red-500">
          <div className="flex items-center">
            <Wifi className="h-8 w-8 text-red-500" />
            <div className="ml-4">
              <p className="text-sm font-medium text-gray-600">{t('labels.connectionError', {ns: 'errorMonitoring'})}</p>
              <p className="text-2xl font-bold text-gray-900">{errorStats.connectionErrors}</p>
              <p className="text-xs text-gray-500">420-429</p>
            </div>
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6 border-l-4 border-orange-500">
          <div className="flex items-center">
            <AlertTriangle className="h-8 w-8 text-orange-500" />
            <div className="ml-4">
              <p className="text-sm font-medium text-gray-600">{t('errors.431', {ns: 'errorMonitoring'})}</p>
              <p className="text-2xl font-bold text-gray-900">{errorStats.protocolErrors}</p>
              <p className="text-xs text-gray-500">430-449</p>
            </div>
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6 border-l-4 border-red-600">
          <div className="flex items-center">
            <Zap className="h-8 w-8 text-red-600" />
            <div className="ml-4">
              <p className="text-sm font-medium text-gray-600">{t('labels.deviceError', {ns: 'errorMonitoring'})}</p>
              <p className="text-2xl font-bold text-gray-900">{errorStats.deviceErrors}</p>
              <p className="text-xs text-gray-500">470-489</p>
            </div>
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6 border-l-4 border-purple-500">
          <div className="flex items-center">
            <Settings className="h-8 w-8 text-purple-500" />
            <div className="ml-4">
              <p className="text-sm font-medium text-gray-600">{t('errors.490', {ns: 'errorMonitoring'})}</p>
              <p className="text-2xl font-bold text-gray-900">7</p>
              <p className="text-xs text-gray-500">490-499</p>
            </div>
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6 border-l-4 border-blue-500">
          <div className="flex items-center">
            <Wrench className="h-8 w-8 text-blue-500" />
            <div className="ml-4">
              <p className="text-sm font-medium text-gray-600">{t('card.maintenanceEvent', {ns: 'errorMonitoring'})}</p>
              <p className="text-2xl font-bold text-gray-900">{errorStats.maintenanceEvents}</p>
              <p className="text-xs text-gray-500">510-519</p>
            </div>
          </div>
        </div>

        <div className="bg-white rounded-lg shadow p-6 border-l-4 border-gray-500">
          <div className="flex items-center">
            <AlertCircle className="h-8 w-8 text-gray-500" />
            <div className="ml-4">
              <p className="text-sm font-medium text-gray-600">{t('labels.systemError', {ns: 'errorMonitoring'})}</p>
              <p className="text-2xl font-bold text-gray-900">{errorStats.systemErrors}</p>
              <p className="text-xs text-gray-500">520-529</p>
            </div>
          </div>
        </div>
      </div>

      {/* 실시간 에러 목록 */}
      <div className="bg-white rounded-lg shadow">
        <div className="px-6 py-4 border-b border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900">{t('realTimeLog', {ns: 'errorMonitoring'})}</h2>
          <p className="text-sm text-gray-600">{t('labels.errorInfoCategorizedByHttpStatusCode', {ns: 'errorMonitoring'})}</p>
        </div>

        <div className="divide-y divide-gray-200">
          {errors.map((error) => (
            <div key={error.id} className={`p-4 border-l-4 ${getErrorColor(error.httpStatus)}`}>
              <div className="flex items-start justify-between">
                <div className="flex items-start space-x-3">
                  <div className={`p-2 rounded-full ${error.httpStatus >= 470 && error.httpStatus <= 489 ? 'bg-red-100' : 'bg-orange-100'}`}>
                    {getErrorIcon(error.httpStatus)}
                  </div>

                  <div className="flex-1">
                    <div className="flex items-center space-x-2">
                      <h3 className="text-lg font-medium text-gray-900">
                        {getErrorName(error.httpStatus)}
                      </h3>
                      <span className="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium bg-gray-100 text-gray-800">
                        HTTP {error.httpStatus}
                      </span>
                      <span className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium ${error.category === 'connection' ? 'bg-red-100 text-red-800' :
                          error.category === 'device' ? 'bg-orange-100 text-orange-800' :
                            error.category === 'maintenance' ? 'bg-blue-100 text-blue-800' :
                              'bg-gray-100 text-gray-800'
                        }`}>
                        {error.category}
                      </span>
                    </div>

                    <p className="text-sm text-gray-600 mt-1">{error.message}</p>
                    <p className="text-xs text-gray-500 mt-1">
                      Device: {error.deviceId} | Time: {error.timestamp.toLocaleTimeString()}
                    </p>
                  </div>
                </div>

                <div className="flex space-x-2">
                  {/* 에러별 액션 버튼들 */}
                  {error.httpStatus === 420 && (
                    <button className="px-3 py-1 text-xs bg-blue-100 text-blue-800 rounded-md hover:bg-blue-200">
                      Retry Connection
                    </button>
                  )}
                  {error.httpStatus === 472 && (
                    <button className="px-3 py-1 text-xs bg-green-100 text-green-800 rounded-md hover:bg-green-200">
                      Run Diagnostics
                    </button>
                  )}
                  {error.httpStatus === 530 && (
                    <button className="px-3 py-1 text-xs bg-purple-100 text-purple-800 rounded-md hover:bg-purple-200">
                      Modbus Check
                    </button>
                  )}
                  {error.httpStatus === 510 && (
                    <button className="px-3 py-1 text-xs bg-yellow-100 text-yellow-800 rounded-md hover:bg-yellow-200">
                      Check Status
                    </button>
                  )}

                  <button
                    className="px-3 py-1 text-xs bg-gray-100 text-gray-800 rounded-md hover:bg-gray-200"
                    onClick={() => setSelectedDevice(error.deviceId)}
                  >
                    Details
                  </button>
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* HTTP 상태코드 참조 테이블 */}
      <div className="mt-8 bg-white rounded-lg shadow">
        <div className="px-6 py-4 border-b border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900">{t('referenceTable', {ns: 'errorMonitoring'})}</h2>
          <p className="text-sm text-gray-600">{t('labels.meaningAndResponseMethodForEachErrorCode', {ns: 'errorMonitoring'})}</p>
        </div>

        <div className="p-6">
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {/* 연결 관련 에러 */}
            <div className="border rounded-lg p-4">
              <h3 className="text-lg font-medium text-red-700 mb-3">Connection Errors (420-429)</h3>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="font-mono">420</span>
                  <span>{t('labels.connectionFailure', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">421</span>
                  <span>{t('errors.421', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">422</span>
                  <span>{t('errors.422', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">423</span>
                  <span>{t('errors.423', {ns: 'errorMonitoring'})}</span>
                </div>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                Auto-reconnect attempted in most cases
              </p>
            </div>

            {/* 프로토콜 관련 에러 */}
            <div className="border rounded-lg p-4">
              <h3 className="text-lg font-medium text-orange-700 mb-3">Protocol Errors (430-449)</h3>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="font-mono">430</span>
                  <span>{t('ref.respTimeout', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">431</span>
                  <span>{t('errors.431', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">434</span>
                  <span>{t('errors.434', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">435</span>
                  <span>{t('errors.435', {ns: 'errorMonitoring'})}</span>
                </div>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                Network quality or configuration issue
              </p>
            </div>

            {/* Device 관련 에러 */}
            <div className="border rounded-lg p-4">
              <h3 className="text-lg font-medium text-red-700 mb-3">Device Errors (470-489)</h3>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="font-mono">470</span>
                  <span>{t('errors.470', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">471</span>
                  <span>{t('labels.inUse', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">472</span>
                  <span>{t('errors.472', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">474</span>
                  <span>{t('errors.474', {ns: 'errorMonitoring'})}</span>
                </div>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                Hardware check or power verification required
              </p>
            </div>

            {/* 점검 관련 */}
            <div className="border rounded-lg p-4">
              <h3 className="text-lg font-medium text-blue-700 mb-3">Maintenance Related (510-519)</h3>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="font-mono">510</span>
                  <span>{t('errors.510', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">513</span>
                  <span>{t('errors.513', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">514</span>
                  <span>{t('errors.514', {ns: 'errorMonitoring'})}</span>
                </div>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                Maintenance complete or permission request required
              </p>
            </div>

            {/* 프로토콜별 에러 */}
            <div className="border rounded-lg p-4">
              <h3 className="text-lg font-medium text-indigo-700 mb-3">Protocol-Specific (530+)</h3>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="font-mono">530</span>
                  <span>{t('errors.530', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">540</span>
                  <span>{t('errors.540', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">550</span>
                  <span>{t('errors.550', {ns: 'errorMonitoring'})}</span>
                </div>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                Protocol-specific configuration and diagnosis required
              </p>
            </div>

            {/* 설정 관련 에러 */}
            <div className="border rounded-lg p-4">
              <h3 className="text-lg font-medium text-purple-700 mb-3">Configuration Errors (490-499)</h3>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="font-mono">490</span>
                  <span>{t('labels.configurationError', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">491</span>
                  <span>{t('labels.missingConfig', {ns: 'errorMonitoring'})}</span>
                </div>
                <div className="flex justify-between">
                  <span className="font-mono">493</span>
                  <span>{t('errors.493', {ns: 'errorMonitoring'})}</span>
                </div>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                Review configuration file or parameters
              </p>
            </div>
          </div>
        </div>
      </div>

      {/* 선택된 Device Details 모달 */}
      {selectedDevice && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center p-4 z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-2xl w-full max-h-[80vh] overflow-y-auto">
            <div className="px-6 py-4 border-b border-gray-200">
              <h3 className="text-xl font-semibold text-gray-900">
                Device {selectedDevice} Error Details
              </h3>
            </div>

            <div className="p-6">
              <div className="space-y-4">
                <div className="bg-gray-50 rounded-lg p-4">
                  <h4 className="font-medium text-gray-900 mb-2">현재 상태</h4>
                  <div className="grid grid-cols-2 gap-4 text-sm">
                    <div>
                      <span className="text-gray-600">Device 상태:</span>
                      <span className="ml-2 font-medium">ERROR (472)</span>
                    </div>
                    <div>
                      <span className="text-gray-600">{t('detailInfo.connectionStatus', {ns: 'errorMonitoring'})}</span>
                      <span className="ml-2 font-medium">CONNECTED (200)</span>
                    </div>
                    <div>
                      <span className="text-gray-600">{t('labels.lastComm', {ns: 'errorMonitoring'})}</span>
                      <span className="ml-2 font-medium">2 min ago</span>
                    </div>
                    <div>
                      <span className="text-gray-600">{t('labels.errorTime', {ns: 'errorMonitoring'})}</span>
                      <span className="ml-2 font-medium">5 min ago</span>
                    </div>
                  </div>
                </div>

                <div className="bg-red-50 rounded-lg p-4">
                  <h4 className="font-medium text-red-900 mb-2">{t('detailInfo.errorAnalysis', {ns: 'errorMonitoring'})}</h4>
                  <div className="text-sm text-red-800">
                    <p><strong>HTTP 472:</strong> Device hardware error</p>
                    <p><strong>{t('labels.cause', {ns: 'errorMonitoring'})}</strong> Internal sensor error or actuator failure</p>
                    <p><strong>{t('labels.impact', {ns: 'errorMonitoring'})}</strong> All control functions for this device are blocked</p>
                  </div>
                </div>

                <div className="bg-blue-50 rounded-lg p-4">
                  <h4 className="font-medium text-blue-900 mb-2">{t('detailInfo.recommendations', {ns: 'errorMonitoring'})}</h4>
                  <ul className="text-sm text-blue-800 space-y-1">
                    <li>• Device 전원 재시작</li>
                    <li>• 케이블 연결 상태 확인</li>
                    <li>• 하드웨어 진단 도구 실행</li>
                    <li>• 필요시 현장 점검 스케줄링</li>
                  </ul>
                </div>

                <div className="bg-green-50 rounded-lg p-4">
                  <h4 className="font-medium text-green-900 mb-2">{t('labels.autoRecoveryAttempt', {ns: 'errorMonitoring'})}</h4>
                  <div className="text-sm text-green-800">
                    <p>Next retry: in 30s</p>
                    <p>Retry: 3/5</p>
                    <div className="mt-2">
                      <div className="w-full bg-green-200 rounded-full h-2">
                        <div className="bg-green-600 h-2 rounded-full" style={{ width: '60%' }}></div>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>

            <div className="px-6 py-4 border-t border-gray-200 flex justify-end space-x-3">
              <button
                className="px-4 py-2 text-sm font-medium text-gray-700 bg-white border border-gray-300 rounded-md hover:bg-gray-50"
                onClick={() => setSelectedDevice(null)}
              >
                닫기
              </button>
              <button className="px-4 py-2 text-sm font-medium text-white bg-blue-600 rounded-md hover:bg-blue-700">
                Run Diagnostics
              </button>
              <button className="px-4 py-2 text-sm font-medium text-white bg-green-600 rounded-md hover:bg-green-700">
                수동 복구 시도
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 엔지니어용 빠른 액션 패널 */}
      <div className="mt-8 bg-gray-50 rounded-lg p-6">
        <h2 className="text-xl font-semibold text-gray-900 mb-4">{t('quickActions', {ns: 'errorMonitoring'})}</h2>
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <button className="flex items-center justify-center p-4 bg-white rounded-lg shadow hover:shadow-md transition-shadow">
            <Wifi className="h-6 w-6 text-red-500 mr-2" />
            <div className="text-left">
              <p className="font-medium">{t('labels.connectionDiagnostics', {ns: 'errorMonitoring'})}</p>
              <p className="text-xs text-gray-500">{t('labels.checkNetworkStatus', {ns: 'errorMonitoring'})}</p>
            </div>
          </button>

          <button className="flex items-center justify-center p-4 bg-white rounded-lg shadow hover:shadow-md transition-shadow">
            <Zap className="h-6 w-6 text-orange-500 mr-2" />
            <div className="text-left">
              <p className="font-medium">{t('action.hardwareScan', {ns: 'errorMonitoring'})}</p>
              <p className="text-xs text-gray-500">{t('labels.checkDeviceStatus', {ns: 'errorMonitoring'})}</p>
            </div>
          </button>

          <button className="flex items-center justify-center p-4 bg-white rounded-lg shadow hover:shadow-md transition-shadow">
            <Settings className="h-6 w-6 text-purple-500 mr-2" />
            <div className="text-left">
              <p className="font-medium">{t('action.configValidate', {ns: 'errorMonitoring'})}</p>
              <p className="text-xs text-gray-500">{t('labels.checkConfigFile', {ns: 'errorMonitoring'})}</p>
            </div>
          </button>

          <button className="flex items-center justify-center p-4 bg-white rounded-lg shadow hover:shadow-md transition-shadow">
            <Wrench className="h-6 w-6 text-blue-500 mr-2" />
            <div className="text-left">
              <p className="font-medium">{t('action.maintenanceSchedule', {ns: 'errorMonitoring'})}</p>
              <p className="text-xs text-gray-500">{t('labels.maintenancePlanning', {ns: 'errorMonitoring'})}</p>
            </div>
          </button>
        </div>
      </div>
    </div>
  );
};

export default PulseOneErrorDashboard;