/**
 * v36 수정판 - 구문 오류 수정
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

// VirtualPointDetailModal
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointDetailModal.tsx', [
    ['\n                                삭제된 가상포인트\n', '\n                                Deleted Virtual Point\n'],
    ['개요 및 상태', 'Overview & Status'],
    ['변경 이력 (Audit Log)', 'Change History (Audit Log)'],
    ['종속성 맵 (Dependency)', 'Dependency Map'],
    ['정의된 Description이 없는 가상Point 엔진.', 'No description defined for this virtual point engine.'],
]);

// ProtocolDashboard (단순 단어 치환만)
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ["'미Support'", "'Not Supported'"],
    ["'불필요'", "'Not Required'"],
    ["'Support 데이터 Type 변경'", "'Supported data types changed'"],
    ["'Protocol 기능 변경'", "'Protocol capabilities changed'"],
    ['Polling wk기:', 'Polling interval:'],
]);

// AlarmTemplateCreateEditModal-Wizard
total += fix('/app/src/components/modals/AlarmTemplateCreateEditModal-Wizard.tsx', [
    ["title: '고온 Warning'", "title: 'High Temperature Warning'"],
    ["desc: '80°C s과 시 Alarm 발생'", "desc: 'Alarm triggered when exceeding 80°C'"],
    ["title: 'Range 이탈'", "title: 'Out of Range'"],
    ["desc: '10~90 Interval을 벗어날 때'", "desc: 'When value falls outside 10~90 range'"],
    ["title: '펌프 정지'", "title: 'Pump Stopped'"],
    ["desc: 'Status값이 0인 경우 발생'", "desc: 'Triggered when status value is 0'"],
    ["{device_name}의 값이 Threshold를 s과했.", "{device_name} value exceeded threshold."],
]);

// DeviceDetailModal
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ['updateDeviceSettings 에러 완전 해결 - 즉시 Save 방식으로 변경', 'updateDeviceSettings error fully resolved - changed to immediate save approach'],
    ['// min할된 컴포넌트들 import', '// import required components'],
    ['// RTU 관련 컴포넌트들 import', '// import RTU-related components'],
    ['initialTab이 바뀌면 탭도 변경 (단, 이미 열려있는 Status에서 탭만 바뀌는 경우)', 'if initialTab changes, update tab (only when modal is already open)'],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['대량 데이터 Point Register 모달 (엑셀 그리드 방식)', 'Bulk data point register modal (Excel grid style)'],
    ['// s기 빈 행 Create', '// create initial empty rows'],
    ['JSON 파서 Status', 'JSON parser status'],
    ['Modbus 비트 자동Create Status', 'Modbus bit auto-create status'],
    ['테이블 컨테이너 참조 (스크롤 제어 등)', 'table container ref (scroll control etc.)'],
]);

// DeviceRtuNetworkTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ['정보 로드 ...`', 'info loading...`'],
    ['// RTU Device인지 OK', '// check if RTU device'],
    ['// 이벤트 핸들러들', '// event handlers'],
    ['슬래이브 디바이스 토글', 'toggle slave device'],
    ['슬래이브 스캔 완료 처리', 'handle slave scan completion'],
]);

// DeviceRtuScanDialog
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx', [
    ['// Select된 Device Register', '// register selected device'],
    ['RTU 슬래이브 Device Register 요청', 'RTU slave device register request'],
    ['Register 요청:`', 'register request:`'],
    ["'모든 Device Register Complete'", "'All devices registered successfully'"],
    ['RTU Device가 아니면 렌더링하지 않음', 'do not render if not RTU device'],
]);

// DeviceSettingsTab
total += fix('/app/src/components/modals/DeviceModal/DeviceSettingsTab.tsx', [
    ['{/* 상단 툴바 */}', '{/* Top Toolbar */}'],
    ['> 기본값', '> Defaults'],
    ['{/* 3단 그리드 레이아웃 */}', '{/* 3-column Grid Layout */}'],
    ['{/* 1. 기본 운영 파라미터 */}', '{/* 1. Basic Operation Parameters */}'],
    ["renderNumberField('읽기 timeout'", "renderNumberField('Read Timeout'"],
]);

// DeviceStatusTab
total += fix('/app/src/components/modals/DeviceModal/DeviceStatusTab.tsx', [
    ['Pro-Engineer Density (Horizontal Layout)', 'Pro-Engineer Density (Horizontal Layout)'],
    ['// API 호출 함수들', '// API call functions'],
    ['연결 테스트 실행', 'run connection test'],
    ['실시간 연결 진단 실행', 'run real-time connection diagnostics'],
    ["'진단 Failed'", "'Diagnostics failed'"],
]);

// BackupRestore
total += fix('/app/src/pages/BackupRestore.tsx', [
    ['// 데이터 로드', '// load data'],
    ["'백Upload Failed:'", "'Backup upload failed:'"],
    ['// Settings 로드', '// load settings'],
    ["'Settings 로드 Failed:'", "'Settings load failed:'"],
    ['// 1. 변경 사항 전후 비교', '// 1. compare before and after changes'],
]);

console.log('\nTotal:', total);
