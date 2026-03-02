/**
 * TenantDetailModal + ExportGatewayWizard 추가 + TemplateManagementTab + TemplateCreateModal + DeviceDetailModal 처리 v22
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
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, path.basename(fp)); }
    return n;
}

let total = 0;

// ====== TenantDetailModal.tsx ======
total += fix('/app/src/components/modals/TenantModal/TenantDetailModal.tsx', [
    ["setError('고객사 정보를 불러오는 중 오류가 발생했습니다.')", "setError('Error loading tenant information.')"],
    ["title: '변경 사항 저장'", "title: 'Save Changes'"],
    ["message: '입력하신 변경 내용을 저장하시겠습니까?'", "message: 'Save your changes?'"],
    ["message: '고객사 정보가 성공적으로 수정되었습니다.'", "message: 'Tenant information updated successfully.'"],
    ["setError(res.message || '업데이트에 실패했습니다.')", "setError(res.message || 'Update failed.')"],
    ["setError(err.message || '서버 통신 중 오류가 발생했습니다.')", "setError(err.message || 'Server communication error.')"],
    ["title: '고객사 삭제 확인'", "title: 'Confirm Tenant Deletion'"],
    ["message: `'${tenant.company_name}' 고객사를 삭제하시겠습니까?`", "message: `Delete tenant '${tenant.company_name}'?`"],
    ["title: '삭제 완료'", "title: 'Deleted'"],
    ["message: '고객사가 성공적으로 삭제되었습니다.'", "message: 'Tenant deleted successfully.'"],
    ["setError(res.message || '삭제에 실패했습니다.')", "setError(res.message || 'Delete failed.')"],
    ["setError(err.message || '삭제 중 오류가 발생했습니다.')", "setError(err.message || 'Error during deletion.')"],
]);

// ====== ExportGatewayWizard.tsx 남은 패턴 ======
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    [">템플릿 JSON 미리보기<", ">Template JSON Preview<"],
    [">+ HTTP 타겟/Mirror 추가<", ">+ Add HTTP Target/Mirror<"],
    [">+ MQTT 타겟 추가<", ">+ Add MQTT Target<"],
    ["S3 타겟 #", "S3 Target #"],
    ['placeholder="S3 Endpoint / Service URL (예: https://s3.ap-northeast-2.amazonaws.com)"', 'placeholder="S3 Endpoint / Service URL (e.g., https://s3.ap-northeast-2.amazonaws.com)"'],
]);

// ====== TemplateManagementTab.tsx 남은 패턴 ======
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ["'참일때값:거짓일때값}}'", "'true_value:false_value}}'"],
    ["confirmText: '삭제'", "confirmText: 'Delete'"],
    ["><th>시스템 타입</th>", "><th>System Type</th>"],
    ["><th>관리</th>", "><th>Manage</th>"],
    [">등록된 템플릿이 없습니다.<", ">No templates registered.<"],
    [">데이터 탐색기<", ">Data Explorer<"],
    [" 추천 항목</", " Recommended</"],
    [">매핑 명칭</", ">Mapping Name</"],
    [">데이터 값</", ">Data Value</"],
]);

// ====== TemplateCreateModal.tsx ======
total += fix('/app/src/components/modals/TemplateCreateModal.tsx', [
    ["newErrors.name = '템플릿 이름은 필수입니다'", "newErrors.name = 'Template name is required'"],
    ["newErrors.description = '설명은 필수입니다'", "newErrors.description = 'Description is required'"],
    ["newErrors.threshold = '임계값은 필수입니다'", "newErrors.threshold = 'Threshold is required'"],
    ["newErrors.high_limit = '상한값은 필수입니다'", "newErrors.high_limit = 'High limit is required'"],
    ["newErrors.low_limit = '하한값은 필수입니다'", "newErrors.low_limit = 'Low limit is required'"],
    ["title: '고온 경고'", "title: 'High Temperature Warning'"],
    ["desc: '표준 고온 알람 템플릿'", "desc: 'Standard high temperature alarm template'"],
    ["message_template: '{device_name}의 온도가 {threshold}°C를 초과했습니다 (현재: {value}°C)'", "message_template: '{device_name} temperature exceeded {threshold}°C (current: {value}°C)'"],
    ["title: '압력 범위 이탈'", "title: 'Pressure Range Exceeded'"],
    ["desc: '정상 범위(Low~High) 이탈 감지'", "desc: 'Normal range (Low~High) violation detection'"],
    ["message_template: '{device_name} 압력 범위 이탈! (현재: {value} bar)'", "message_template: '{device_name} pressure out of range! (current: {value} bar)'"],
    ["title: '펌프 상태 모니터링'", "title: 'Pump Status Monitoring'"],
]);

// ====== DeviceDetailModal.tsx 잔여 ======
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["> 3단계: 장치 요약 및 포인트</", "> Step 3: Device Summary & Points</"],
    ["{dataPoints.length} 개", "{dataPoints.length} point(s)"],
    ["? '장치 기본 정보를 먼저 입력해주세요.' : '통신 설정을 마무리한 후 포인트를 설정하세요.'", "? 'Please enter basic device info first.' : 'Complete communication settings then configure points.'"],
    ["모든 준비가 완료되었습니다. 아래 [생성] 버튼을 클릭하세요.", "All settings are complete. Click the [Create] button below."],
]);

console.log('\nTotal:', total);
