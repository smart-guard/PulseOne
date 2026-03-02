/**
 * v47 - 복구된 22개 파일 및 잔여 핵심 한글 치환
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

// ExportGatewayWizard
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ['// 엑셀에서 복사해온 데이터 Paste', '// paste data from Excel'],
    ['// 유효성 OK: mapping_type="site" 인데 site_id가 없으면 안됨', '// validation: if mapping_type="site", site_id is required'],
    ['// Step Validation', '// step validation'],
    ['// [translated comment]', '// translated comment'],
    ['Gateway 이름은 필수입니다.', 'Gateway Name is required.'],
    ['Host 필드는 필수입니다.', 'Host field is required.'],
    ['Topic은 설정되었으나 페이로드 템플릿이 선택되지 않았습니다.', 'Topic is set but payload template is not selected.'],
    ['타겟 키 매핑이 올바르지 않습니다.', 'Target key mapping is invalid.'],
    ['Gateway를 먼저 생성/저장한 뒤 수동 테스트를 진행하세요.', 'Please Save Gateway first to run a manual test.'],
    ['Gateway 저장을 먼저 진행해주세요.', 'Please Save Gateway first.'],
    ['입력 데이터를 JSON으로 파싱할 수 없습니다.', 'Cannot parse input data as JSON.'],
    ['테스트 성공!', 'Test Successful!'],
    ['ExportGateway가 성공적으로 생성되었습니다.', 'Export Gateway created successfully.'],
    ['ExportGateway 정보가 업데이트되었습니다.', 'Export Gateway updated successfully.'],
    ['Target Key Mapping 저장 중 오류가 발생했습니다.', 'Error occurred while saving target key mapping.'],
    ['데이터를 파싱할 수 없거나 형식이 잘못되었습니다.', 'Cannot parse data or invalid format.'],
    ["message: '적어도 1개 이상의 유효한 매핑 데이터가 필요합니다.'", "message: 'At least 1 valid mapping data is required.'"],
    ['에러 내용:', 'Error details:']
]);

// DeviceDetailModal
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ['Data point settings을 계속??', 'Continue data point settings?'],
    ['// 🎨 custom save function (instead of browser default popup)', '// custom save function instead of browser default popup'],
    ['// 🎨 custom save function', '// custom save function'],
    ['// load data points', '// load data points']
]);

// GatewayListTab
total += fix('/app/src/pages/export-gateway/tabs/GatewayListTab.tsx', [
    ['ExportGateway가 성공적으로 삭제되었습니다.', 'Export Gateway deleted successfully.'],
    ['Target Key Mapping을 포함한 모든 설정이 삭제됩니다.', 'All settings including Target Key Mapping will be deleted.'],
    ['Gateway 삭제 완료', 'Gateway deleted successfully'],
    ['삭제 중 오류가 발생했습니다.', 'An error occurred during deletion.'],
    ['Gateway를 삭제하시겠습니까?', 'Delete Gateway?']
]);

// ManualTestTab
total += fix('/app/src/pages/export-gateway/tabs/ManualTestTab.tsx', [
    ['선택된 Gateway가 없습니다.', 'No Gateway selected.'],
    ['수동 테스트', 'Manual Test'],
    ['테스트 실행', 'Run Test'],
    ['결과', 'Result'],
    ['타겟', 'Target'],
    ['게이트웨이 선택', 'Select Gateway'],
    ['요청', 'Request'],
    ['응답', 'Response'],
    ['실행 중...', 'Running...']
]);

// ScriptEngine (types/scriptEngine.ts) - 주석
total += fix('/app/src/types/scriptEngine.ts', [
    ['// 함수 설명', '// Function description'],
    ['// 반환 타입', '// Return type'],
    ['// 입력 파라미터', '// Input parameters'],
    ['// 변수 이름', '// Variable name'],
    ['// 수식', '// Formula'],
    ['// 실행 주기', '// Execution interval'],
    ['// 활성화 여부', '// Is enabled']
]);

// VirtualPointDetailModal
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointDetailModal.tsx', [
    ['<div className="val">{virtualPoint.category || \'Default\'}</div>', '<div className="val">{virtualPoint.category || \'Default\'}</div>'],
    ['// [translated comment]', '// translation'],
    ['상Point 참조하고 있', 'References for virtual point'],
    ['Delete 상Point', 'Deleted Virtual Point'],
    ['Delete Cancel 및 복원', 'Cancel Deletion and Restore'],
    ['연산 엔진 Stop', 'Stop Engine'],
    ['연산 엔진 재개', 'Resume Engine'],
    ['상Point Delete', 'Delete Virtual Point'],
    ['엔진 Settings Edit', 'Edit Engine Settings']
]);

// ManufacturerManagementPage
total += fix('/app/src/pages/ManufacturerManagementPage.tsx', [
    ['제조사 등록', 'Register Manufacturer'],
    ['상세 보기 / 수정', 'View/Edit Details'],
    ['정상적으로 삭제되었습니다.', 'Deleted successfully.']
]);

console.log('Total fixed in v47:', total);
