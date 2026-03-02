/**
 * v40 - 마지막 잔여 파일들 처리
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

// DeviceDetailModal
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ['// 기존 라인 148-163 부근의 showCustomModal 함수를 이렇게 교체', '// replace showCustomModal function near lines 148-163'],
    ['// 🔥 핵심 Edit: 모달을 먼저 닫고, 그 Next 콜백 실행', '// 🔥 key: close modal first, then run callback'],
    ['// 짧은 지연 후 콜백 실행 (모달 Close Complete 후)', '// run callback after short delay (after modal close completes)'],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['// 🔥 NEW: JSON 샘플 파싱 로직', '// 🔥 NEW: JSON sample parsing logic'],
    ['// 비어있지 않은 첫 20 행이 있다면 그 뒤에 Add, 아니면 교체', '// append after existing rows if any, otherwise replace'],
    ['// 🔩 Modbus 비트 자동Create', '// 🔩 Modbus bit auto-create'],
]);

// DeviceRtuNetworkTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ['// 렌더링', '// rendering'],
    ['>RTU Device가 아닙니다</p>', '>Not an RTU Device</p>'],
    ['현재 프로토콜:', 'Current protocol:'],
    ['{/* RTU 디바이스 기본 정보 */}', '{/* RTU Device Basic Info */}'],
]);

// ManufacturerDetailModal - 국가명 맵 영어 값은 OK, 키는 한국어라 유지
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerDetailModal.tsx', [
    ['// 국가명 표준화 맵', '// country name normalization map'],
]);

// NetworkSettings
total += fix('/app/src/pages/NetworkSettings.tsx', [
    ['// firewall rules 토글', '// toggle firewall rules'],
    ['{/* 탭 네비게이션 */}', '{/* Tab Navigation */}'],
    ['{/* 네트워크 인터페이스 탭 */}', '{/* Network Interface Tab */}'],
    ['>가상 인터페이스 추가<', '>Add Virtual Interface<'],
]);

// DataPointModal
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ['// 📊 데이터Point Create/Edit 모달', '// 📊 DataPoint Create/Edit Modal'],
    ['// 폼 데이터 Reset', '// reset form data'],
    ['// 폼 필드 업데이트', '// update form field'],
    ['// 유효성 검사', '// validation'],
]);

// ManufacturerModal
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerModal.tsx', [
    ['// 국가명 표준화 맵', '// country name normalization map'],
]);

console.log('\nTotal:', total);
