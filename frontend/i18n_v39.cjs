/**
 * v39 - 8개 파일 마지막 잔여 처리
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
    ['// RTU Device 판별 함수들', '// RTU device determination functions'],
    ['// 예쁜 모달 표시 함수들', '// custom modal display functions'],
    ['DeviceDetailModal.tsx - showCustomModal 함수만 Edit', 'DeviceDetailModal.tsx - edit showCustomModal function only'],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['"Select도 안했는데 데이터가 들어가 있음" 문제 해결)', '"data loaded without selection" issue resolved)'],
    ['// 2. 기존 Device Edit인 경우: 로컬 스토리지에서 복원 시도', '// 2. when editing existing device: try restoring from local storage'],
    ['// 3. 아무것도 없으면 Default 행 Create', '// 3. create default rows if nothing exists'],
    ['// 기존 데이터가 있으면 유지할지 물어보기', '// ask user whether to keep existing data'],
]);

// DeviceRtuNetworkTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ['* 연결 테스트', '* connection test'],
    ['// 라이프사이클', '// lifecycle'],
    ['// 렌더링 헬퍼 함수들', '// rendering helper functions'],
    ['// Protocol 정보 Import', '// import protocol info'],
]);

// DeviceStatusTab
total += fix('/app/src/components/modals/DeviceModal/DeviceStatusTab.tsx', [
    ["console.error('Connect 진단 Failed:'", "console.error('Connection diagnostics failed:'"],
    ['* 디바이스 통계 및 데이터포인트 로드', '* load device statistics and data points'],
    ['// 만약 dataPoints prop이 있으면 API 호출 너뜀 (또는 갱신 목적으로 할 수도 있지만 d단 prop 우선)', '// skip API call if dataPoints prop provided (prop takes priority)'],
]);

// HistoricalData
total += fix('/app/src/pages/HistoricalData.tsx', [
    ['// 중복 제거 및 형식 변환 (ID를 문자열로)', '// deduplicate and convert format (ID to string)'],
    ['// 페이지네이션', '// pagination'],
    ['{/* 페이지 헤더 */}', '{/* Page Header */}'],
    ['{/* 검색 조건 패널 */}', '{/* Search Condition Panel */}'],
]);

// BackupRestore
total += fix('/app/src/pages/BackupRestore.tsx', [
    ['// 2. 변경 사항이 있을 경우 Save OK 팝업', '// 2. show save confirmation popup if there are changes'],
    ['setInitialSettings(settings); // 최초 상태 동기화', 'setInitialSettings(settings); // sync initial state'],
    ['// 즉시 백업 실행', '// run immediate backup'],
    ["console.error('백업 요청 Error:'", "console.error('Backup request error:'"],
]);

// ControlSchedulePage
total += fix('/app/src/pages/ControlSchedulePage.tsx', [
    ['>새 예약 제어 등록<', '>Register New Scheduled Control<'],
    ['{/* Step 탭 */}', '{/* Step Tabs */}'],
    ["'제어 Settings'", "'Control Settings'"],
    ['{/* STEP 1: 포인트 선택 */}', '{/* STEP 1: Select Points */}'],
]);

// TargetManagementTab - 주석만
total += fix('/app/src/pages/export-gateway/tabs/TargetManagementTab.tsx', [
    ['// [FIX] 확실한 UI 반영을 위해 useEffect로 스타d을 head에 직접 wk입합니다.', '// [FIX] inject styles directly into head via useEffect for reliable UI rendering'],
    ['/* 모달 컨테이너 내의 입력창 높이 상향 */', '/* increase input height inside modal container */'],
    ['/* 표준 테두리 색상으로 원복 */', '/* restore to standard border color */'],
    ['/* Ant Design Select 내부 정렬 보정 */', '/* fix Ant Design Select internal alignment */'],
]);

console.log('\nTotal:', total);
