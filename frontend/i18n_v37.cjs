/**
 * v37 - ProtocolEditor/Dashboard/DataExport/HistoricalData/NetworkSettings/DeviceRtuMonitorTab/
 *        ManufacturerDetailModal/NetworkScanModal/TemplateImportExportModal/
 *        ActiveAlarms/RealTimeMonitor/ProfileManagementTab/ScheduleManagementTab
 */
const fs = require('fs');
const path = require('path');

function fix(fp, repls) {
    if (!fs.existsSync(fp)) return 0;
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of repls) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log(`FIXED ${n} ${path.basename(fp)}`); }
    return n;
}

let total = 0;

// ProtocolEditor
total += fix('/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx', [
    ['Changed settings정은 즉시 Apply되며, 이 Protocol을 Use하는 Device들에게 영향을 줄 수 있.', 'Changed settings take effect immediately and may affect devices using this protocol.'],
    ['placeholder="예: Modbus Org"', 'placeholder="e.g., Modbus Org"'],
    ['<label>최소 펌웨어</label>', '<label>Min Firmware</label>'],
    ['placeholder="예: v1.0.0"', 'placeholder="e.g., v1.0.0"'],
]);

// Dashboard
total += fix('/app/src/pages/Dashboard.tsx', [
    ["description: '메타데이터 Save소'", "description: 'Metadata Store'"],
    ['// 🛠️ 서비스 제어 함수들 (DashboardApiService 사용)', '// 🛠️ service control functions (using DashboardApiService)'],
    ["title: '서비스 Start'", "title: 'Start Service'"],
    ['`${displayName}를 Start??`', '`Start ${displayName}?`'],
]);

// DataExport
total += fix('/app/src/pages/DataExport.tsx', [
    ["analysis report냅니다.'", "analysis report.'"],
    ['// temp file size 계산', '// temp file size estimate'],
    ['Total ${result.data.total_records} records extracted다.`', 'Total ${result.data.total_records} records extracted.`'],
    ['>시스템 접속 및 장치 제어 Logs를 JSON Format으로 내보냅니다.</p>', '>Exports system access and device control logs in JSON format.</p>'],
]);

// HistoricalData
total += fix('/app/src/pages/HistoricalData.tsx', [
    ['// 백엔드에서 count를 주지 않으므로 1로 설정 (또는 백엔드 수정 필요)', '// backend does not provide count, default to 1 (or update backend)'],
    ['// 집계 데이터 Create 보조 함수 (Filter링된 데이터 기반)', '// helper function to create aggregated data (based on filtered data)'],
    ['// 차트 데이터 변환', '// convert chart data'],
    ['// 타임스탬프별로 Group화', '// group by timestamp'],
]);

// NetworkSettings
total += fix('/app/src/pages/NetworkSettings.tsx', [
    ['// 네트워크 인터페이스', '// network interfaces'],
    ['// 방화벽 규칙', '// firewall rules'],
    ['// 인터페이스 Active화/비Active화', '// enable/disable interface'],
    ['// 방화벽 규칙 토글', '// toggle firewall rule'],
]);

// DeviceRtuMonitorTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ['// RTU 통신 Monitoring 탭 컴포넌트 - protocol_id 지원', '// RTU communication monitoring tab component - supports protocol_id'],
    ['// RTU Device 여부 OK - protocol_id 또는 protocol_type 기반', '// check if RTU device - based on protocol_id or protocol_type'],
    ['// 1. protocol_type으로 OK', '// 1. check by protocol_type'],
    ['// 2. protocol_id로 OK (MODBUS_RTU는 보통 ID 2)', '// 2. check by protocol_id (MODBUS_RTU is usually ID 2)'],
]);

// ManufacturerDetailModal
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerDetailModal.tsx', [
    ['// 국가명 표준화 맵', '// country name normalization map'],
    ["'대한민국': 'South Korea'", "'대한민국': 'South Korea'"],
    ["'한국': 'South Korea'", "'한국': 'South Korea'"],
]);

// NetworkScanModal
total += fix('/app/src/components/modals/NetworkScanModal.tsx', [
    ["'Broker URL을 Input해wk세요.'", "'Please enter the Broker URL.'"],
    ["'IP Range(CIDR)를 Input해wk세요.'", "'Please enter the IP Range (CIDR).'"],
    ["title: '스캔 Complete'", "title: 'Scan Complete'"],
    ["message: `${foundCount}의 새로운 장치가 발견되어 자동 Register되었.`", "message: `${foundCount} new device(s) found and automatically registered.`"],
]);

// TemplateImportExportModal
total += fix('/app/src/components/modals/TemplateImportExportModal.tsx', [
    ["`행 ${i + 2}: ${error instanceof Error ? error.message : 'Parse Error'}`", "`Row ${i + 2}: ${error instanceof Error ? error.message : 'Parse Error'}`"],
    ['// Template Import 실행', '// execute template import'],
    ["alert('가져올 Template이 없.')", "alert('No templates to import.')"],
]);

// ActiveAlarms
total += fix('/app/src/pages/ActiveAlarms.tsx', [
    ['// Active Alarm Monitoring 페이지 - 공통 UI 시스템 Apply Version', '// Active Alarm Monitoring page - common UI system applied'],
    ["'Alarm 정보를 Loading Error가 발생했.'", "'An error occurred while loading alarm information.'"],
    ['// WebSocket Connect 및 이벤트 핸들링', '// WebSocket connection and event handling'],
]);

// RealTimeMonitor  
total += fix('/app/src/pages/RealTimeMonitor.tsx', [
    ['threshold(${threshold.min}) 미만.`', 'threshold (${threshold.min}).`'],
    ['threshold(${threshold.max})을 s과했.`', 'threshold (${threshold.max}).`'],
    ['// ❌ setCurrentPage(1) 여기서 제거 → 데이터 업데이트 시 페이지 유지', '// ❌ setCurrentPage(1) removed here → preserve page on data update'],
]);

// ProfileManagementTab
total += fix('/app/src/pages/export-gateway/tabs/ProfileManagementTab.tsx', [
    ["title: 'Point 부족'", "title: 'Insufficient Points'"],
    ["message: '최소 하나 Abnormal의 데이터 Point를 매핑해야 합니다.'", "message: 'At least one data point must be mapped.'"],
    ["message: 'Edit된 정보가 없.'", "message: 'No changes to save.'"],
]);

// ScheduleManagementTab
total += fix('/app/src/pages/export-gateway/tabs/ScheduleManagementTab.tsx', [
    ["Transmits data every 5 minutes.다.'", "Transmits data every 5 minutes.'"],
    ["Transmits data every 10 minutes.Transmit합니다.'", "Transmits data every 10 minutes.'"],
    ["Transmits data every 15 minutes.Transmit합니다.'", "Transmits data every 15 minutes.'"],
]);

console.log('\nTotal:', total);
