/**
 * v44 - 9개 파일 잔여 한글 처리
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

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['// Undo/Redo 단축키 처리', '// handle Undo/Redo keyboard shortcuts'],
    ['// 엑셀 스타d: 포커스된 셀이 화면 밖이면 자동으로 스크롤', '// Excel style: auto-scroll when focused cell is out of view'],
]);

// DeviceDetailModal
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["Data point settings을 계속??'", "Continue to data point settings?'"],
    ['// onDeviceCreated가 있으면 부모가 edit 모드+datapoints 탭으로 이동', '// if onDeviceCreated exists, parent moves to edit mode + datapoints tab'],
]);

// DataPointModal
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ['<label>스케d링 팩터</label>', '<label>Scaling Factor</label>'],
    ['<label>스케d링 Offset</label>', '<label>Scaling Offset</label>'],
    ['{/* 로깅 및 폴링 설정 */}', '{/* Logging & Polling Settings */}'],
]);

// ProtocolInstanceManager
total += fix('/app/src/components/protocols/ProtocolInstanceManager.tsx', [
    ['// s기 Loading 시 User Role OK 및 테넌트 목록 로드', '// check user role and load tenant list on initial load'],
    ['`새로운 Instance [${formData.instance_name}]을(를) Add??`', '`Add new instance [${formData.instance_name}]?`'],
    ["confirmText: 'Add하기'", "confirmText: 'Add'"],
]);

// VirtualPointDetailModal
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointDetailModal.tsx', [
    [">{'기본'}<", ">'Default'<"],
    ['<label>데이터 Type</label>', '<label>Data Type</label>'],
    ['<label>Execute 모드</label>', '<label>Execution Mode</label>'],
]);

// DeviceRtuNetworkTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ["RTU {isMaster ? 'Master' : 'slave'} 정보\n", "RTU {isMaster ? 'Master' : 'Slave'} Info\n"],
    [">시리얼 Port</label>", ">Serial Port</label>"],
    [">통신 Status</label>", ">Communication Status</label>"],
]);

// ProtocolEditor
total += fix('/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx', [
    ['{/* 기술 설정 */}', '{/* Technical Settings */}'],
    ['<h3><i className="fas fa-cogs"></i>기술 Settings</h3>', '<h3><i className="fas fa-cogs"></i>Technical Settings</h3>'],
    ['<label>Default Polling wk기 (ms)</label>', '<label>Default Polling Interval (ms)</label>'],
]);

// ManufacturerDetailModal + ManufacturerModal - 국가명 맵 주석 제외 나머지
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerDetailModal.tsx', [
    ["'독d': 'Germany'", "'독일': 'Germany'"],
]);
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerModal.tsx', [
    ["'독d': 'Germany'", "'독일': 'Germany'"],
]);

console.log('\nTotal:', total);
