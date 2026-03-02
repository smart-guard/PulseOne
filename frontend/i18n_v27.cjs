/**
 * ExportGatewayWizard + TemplateManagementTab + MqttBrokerDashboard + ScheduleManagementTab + DeviceDetailModal v27
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

// ExportGatewayWizard
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    [">템플릿 JSON 미리보기<", ">Template JSON Preview<"],
    [">+ HTTP 타겟/Mirror 추가<", ">+ Add HTTP Target/Mirror<"],
    [">+ MQTT 타겟 추가<", ">+ Add MQTT Target<"],
    [">+ S3 스토리지 추가<", ">+ Add S3 Storage<"],
    [">전송 모드 가이드<", ">Transmission Mode Guide<"],
]);

// TemplateManagementTab
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ["'참일때값:거짓일때값}}'", "'true_value:false_value}}'"],
    ["><th>시스템 타입</th>", "><th>System Type</th>"],
    ["><th>관리</th>", "><th>Manage</th>"],
    [">등록된 템플릿이 없습니다.</", ">No templates registered.</"],
    [">데이터 탐색기</", ">Data Explorer</"],
    [">추천 항목</", ">Recommended</"],
    [">매핑 명칭</", ">Mapping Name</"],
    [">데이터 값</", ">Data Value</"],
]);

// MqttBrokerDashboard (실제 라인 내용 기반)
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    [">\n            저장 완료\n", ">\n            Saved\n"],
    ["저장 완료</", "Saved</"],
    [">사양 편집 모드</", ">Spec Edit Mode</"],
    ["상태 새로고침</i>", "Refresh Status</i>"],
]);

// ScheduleManagementTab
total += fix('/app/src/pages/export-gateway/tabs/ScheduleManagementTab.tsx', [
    ["message: response.message || '스케줄을 저장하는 중 오류가 발생했습니다.'", "message: response.message || 'Error saving schedule.'"],
    ["message: '스케줄을 저장하는 중 오류가 발생했습니다.'", "message: 'Error saving schedule.'"],
    ["message: '이 전송 스케줄을 삭제하시겠습니까?'", "message: 'Delete this transmission schedule?'"],
    [">배치 전송 스케줄 관리</", ">Batch Transmission Schedule Management</"],
    ["><th>스케줄 이름</th>", "><th>Schedule Name</th>"],
    ["><th>전송 대상</th>", "><th>Target</th>"],
    ["><th>스케줄</th>", "><th>Schedule</th>"],
    ["><th>상태</th>", "><th>Status</th>"],
    ["><th>관리</th>", "><th>Manage</th>"],
    [">스케줄 없음</", ">No schedules</"],
]);

// DeviceDetailModal 잔여 주석
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["{/* 모달 푸터 */}", "{/* Modal Footer */}"],
    ["{/* 🎨 예쁜 커스텀 모달 (DeviceList 스타일과 동일) */}", "{/* Custom modal (same style as DeviceList) */}"],
    ["{/* 스타일 */}", "{/* Styles */}"],
    ["> 3단계: 장치 요약 및 포인트</", "> Step 3: Device Summary & Points</"],
]);

console.log('\nTotal:', total);
