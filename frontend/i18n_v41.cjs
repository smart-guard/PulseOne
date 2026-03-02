/**
 * v41 - 8개 파일 마지막 잔여 처리
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
    ['// Promise 인지 OK하고 Waiting', '// check if Promise and await'],
    ['// 모달 먼저 Close', '// close modal first'],
    ['// Cancel 콜백 실행 (있다면)', '// run cancel callback (if any)'],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['// ✅ 올바른 방식: address_string에 레지스터 Address, protocol_params에 bit_index', '// ✅ correct approach: register address in address_string, bit_index in protocol_params'],
    ['// Collector의 ConvertRegistersToValue()가 protocol_params["bit_index"]를 읽어 비트 추출', '// Collector\'s ConvertRegistersToValue() reads protocol_params["bit_index"] to extract bit'],
    ['// protocol_params에 bit_index와 function_code Save', '// save bit_index and function_code in protocol_params'],
]);

// ManufacturerDetailModal - 국가명 맵 키는 한국어(정상)
// (키 자체는 입력 값 맵핑용으로 한국어 유지 필요, 주석만 처리 완료됨)

// DeviceDataPointsTab
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ['// Edit용 s기 데이터', '// initial data for editing'],
    ['// Filter링', '// filtering'],
    ['// 핸들러', '// handlers'],
    ["alert('Required Input값(Name, Address)을 OK해wk세요.')", "alert('Please check required fields (Name, Address).')"],
]);

// DeviceRtuMonitorTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ['// 실제 구현에서는 RTU Statistics API 호출', '// call RTU Statistics API in actual implementation'],
    ['// 시뮬레이션 데이터', '// simulation data'],
    ['// 실Time 메트릭 수집 Start', '// start real-time metric collection'],
    ['RTU 실Time Monitoring start:', 'RTU real-time monitoring start:'],
]);

// DeviceStatusTab
total += fix('/app/src/components/modals/DeviceModal/DeviceStatusTab.tsx', [
    ['// 데이터Point 목록 조회 (최신값 포함)', '// fetch data point list (including latest values)'],
    ['limit: 100 // 적절한 수량 제한', 'limit: 100 // appropriate quantity limit'],
    ["console.error('Device 데이터Point 로드 Failed:'", "console.error('Device data point load failed:'"],
]);

// ActiveAlarms
total += fix('/app/src/pages/ActiveAlarms.tsx', [
    ['// 3. subscribe to new alarm events -> 자동 Refresh', '// 3. subscribe to new alarm events -> auto refresh'],
    ['// 4. 클린업', '// 4. cleanup'],
    ['// 계산된 Statistics (Current Filter링된 결과와 서버 Statistics 병합)', '// computed statistics (merge current filtered results with server statistics)'],
    ['// 페이지 로컈 데이터 갱신', '// refresh page local data'],
]);

// BasicInfoForm
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx', [
    ['Apply industry-standard settings in one click Apply하세요.', 'Apply industry-standard settings in one click.'],
    ['>1단계: 기본 식별<', '>Step 1: Basic Identification<'],
    ['Use a unique name not duplicated by other points 권장합니다.', 'Use a unique name not duplicated by other points.'],
    ['>2단계: 데이터 형식<', '>Step 2: Data Format<'],
]);

console.log('\nTotal:', total);
