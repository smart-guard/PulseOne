/**
 * v43 - 10개 파일 잔여 한글 처리
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
    ['// 🔍 스마트 변경 감지 (Diffing)', '// 🔍 smart change detection (diffing)'],
    ['// 1. Basic Info 및 통신 Settings 비교', '// 1. compare Basic Info and communication settings'],
    ["unit: '회'", "unit: ' times'"],
    ['// 1.5. 상세 Settings(Settings) 비교', '// 1.5. compare detailed settings'],
    ['// 2. 데이터Point 변경 감지', '// 2. detect data point changes'],
    ['// 변경사항이 전혀 없는 경우', '// when there are no changes at all'],
    ['엔드Point:', 'Endpoint:'],
    ['데이터Point:', 'Data Points:'],
    ["Data point settings을 계속??'", "Continue to data point settings?'"],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['// Current 입력 목록 내 중복 체크 (allPoints가 제공된 경우)', '// check duplicates within current input list (if allPoints provided)'],
    ['// 비트 Point: address + bit_index 조합이 유니크해야 함', '// bit point: address + bit_index combination must be unique'],
    ['// d반 Point: address가 유니크해야 함', '// general point: address must be unique'],
    ['// 1단계: 별 행 업데이트', '// step 1: update each row'],
    ['// 2단계: All 행 다시 검증 (목록 내 중복 체크를 위해)', '// step 2: re-validate all rows (for in-list duplicate check)'],
    ['// 스크롤 하단으로 이동', '// scroll to bottom'],
    ['// 엑셀 Paste 처리', '// handle Excel paste'],
    ['// 모달 오픈 시 배경 스크롤 방지 및 이벤트 차단', '// prevent background scroll and block events on modal open'],
    ['// 키보드 방향키 이동 핸들러', '// keyboard arrow key navigation handler'],
]);

// ManufacturerDetailModal
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerDetailModal.tsx', [
    ['// 기존 국가명이 리스트에 없으면 직접 입력 모드로 Start', '// if existing country name not in list, start direct input mode'],
    ["`'${manufacturer.name}' Manufacturer를 Delete?? Connect된 Model이 있는 경우 Deletecannot be done.`", "`Delete '${manufacturer.name}' manufacturer? Cannot delete if there are connected models.`"],
    ["message: 'Manufacturer가 Success적으로 Delete되었.'", "message: 'Manufacturer deleted successfully.'"],
]);

// DataPointModal
total += fix('/app/src/components/modals/DeviceModal/DataPointModal.tsx', [
    ['// Save 처리', '// handle save'],
    ['// Delete 처리', '// handle delete'],
    ['// 읽기 Test', '// read test'],
    ['// 쓰기 Test', '// write test'],
]);

// DeviceDataPointsTab
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ['// 변경 사항 OK (Edit 시에만)', '// validate changes (only when editing)'],
    ['// [변경 요청] Save 클릭 시 "OK" 과정 없이 바로 실행 (어차피 메인에서 최종 Save 필요)', '// [request] execute immediately on save click without validation (final save happens in parent)'],
    ['// MQTTd 경우 address가 Required이므로 더미 값 할당 (중복 방지를 위해 Date.now 사용)', '// for MQTT, address is required so assign dummy value (using Date.now to prevent duplicates)'],
    ["alert('Save되었.')", "alert('Saved.')"],
]);

// DeviceRtuMonitorTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ['// 실Time 데이터 수집 (시뮬레이션)', '// real-time data collection (simulation)'],
    ['// 통신 Test 실행', '// run communication test'],
    ['RTU 통신 Test Execute:', 'RTU communication test execute:'],
    ['// Test 후 Statistics 갱신', '// update statistics after test'],
]);

// ProtocolDetailModal
total += fix('/app/src/components/modals/ProtocolModal/ProtocolDetailModal.tsx', [
    ['// ESC 키로 모달 Close', '// close modal with ESC key'],
    ['{/* 모달 헤더 */}', '{/* Modal Header */}'],
    ["'비Active'", "'Inactive'"],
    ['{/* 모달 콘텐츠 */}', '{/* Modal Content */}'],
]);

// TemplateImportExportModal
total += fix('/app/src/components/modals/TemplateImportExportModal.tsx', [
    ["message: `${parseResults.templates.length} Template이 Success적으로 가져와졌.`", "message: `${parseResults.templates.length} template(s) imported successfully.`"],
    ['// Export 모드 렌더링', '// render export mode'],
    ['>내보낼 템플릿 선택<', '>Select Templates to Export<'],
    ['>엑셀에서 편집 가능한 CSV 형식으로 내보냅니다.<', '>Export in CSV format editable in Excel.<'],
]);

// HistoricalData
total += fix('/app/src/pages/HistoricalData.tsx', [
    ['{/* 기본 조건 */}', '{/* Basic Conditions */}'],
    ['{/* 사이트 선택 */}', '{/* Site Selection */}'],
    ['{/* 디바이스 선택 */}', '{/* Device Selection */}'],
    ['>공장을 먼저 Select하세요</div>', '>Select a site first</div>'],
]);

// NetworkSettings
total += fix('/app/src/pages/NetworkSettings.tsx', [
    ['>가상 인터페이스 추가<', '>Add Virtual Interface<'],
    ['>설정<', '>Settings<'],
    ['{/* 방화벽 규칙 탭 */}', '{/* Firewall Rules Tab */}'],
    ['>새 규칙<', '>New Rule<'],
]);

console.log('\nTotal:', total);
