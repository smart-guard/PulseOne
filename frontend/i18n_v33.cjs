/**
 * v33 - DataExport + ExportGatewayWizard + ControlSchedulePage + InputVariableEditor +
 *        VirtualPointTable + DeviceRtuNetworkTab + ProtocolDashboard + Dashboard 처리
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

// DataExport - 남은 JSX 한글 (반만 번역된 것들)
total += fix('/app/src/pages/DataExport.tsx', [
    [">전d 생산 핵심 지표 및 장치 Status 데이터를 CSV로 추출합니다.</p>", ">Exports previous day production KPIs and device status data to CSV.</p>"],
    [">Recent 7d간 발생한 모든 Alarm 및 조치 History을 PDF로 Create합니다.</p>", ">Generates PDF of all alarms and actions from the last 7 days.</p>"],
    [">한 달간의 장치 Availability 및 wk요 Status 변화를 Analytics 리Port로 내보냅니다.</p>", ">Exports monthly availability and major status changes as an analytics report.</p>"],
    ["{/* 퀵 템플릿 */}", "{/* Quick Templates */}"],
]);

// ExportGatewayWizard - 남은 JSX
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ['>전송 모드 가이드<', '>Transmission Mode Guide<'],
    ["지정된 Cron Schedule에 맞춰 데이터를 wk기적으로 d괄 T", "Transmits data in batches on the specified cron schedule."],
    ["수집된 데이터에 변화가 생기거나 특정 Threshold를 넘는 이벤트가 발생할 때 즉시 Trans", "Transmits immediately when data changes or threshold events occur."],
]);

// ControlSchedulePage
total += fix('/app/src/pages/ControlSchedulePage.tsx', [
    ['// 예약 제어 Schedule Management 페이지 (직접 Register 기능 포함)', '// Schedule control management page (with direct register feature)'],
    ['// ── 유틸 ─────────────────────────────', '// ── Utils ─────────────────────────────'],
    ['// ── Register 모달 ──────────────────────', '// ── Register Modal ──────────────────────'],
    ['// step2: 제어 Settings', '// step2: control settings'],
    ["upd({ saveMsg: e.message || 'Error 발생', saving: false, saveFailed: true })", "upd({ saveMsg: e.message || 'Error occurred', saving: false, saveFailed: true })"],
    ['{/* 헤더 */}', '{/* Header */}'],
]);

// InputVariableEditor
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/InputVariableEditor.tsx', [
    ['// 작성 중인 내용이 있는지 부모 컴포넌트에 알리기 위함', '// notify parent component if there is unsaved content'],
    ['`Input Variable \\"${variable.input_name}\\"을(를) 정말 Delete??`', '`Really delete input variable \\"${variable.input_name}\\"?`'],
    ["confirmText: 'Delete하기'", "confirmText: 'Delete'"],
    ["message: 'Variable명을 Input해wk세요.'", "message: 'Enter a variable name.'"],
    ["message: '데이터 소스를 Select해wk세요.'", "message: 'Select a data source.'"],
    ['// Register Complete 후 폼 Reset', '// reset form after register complete'],
]);

// VirtualPointTable
total += fix('/app/src/components/VirtualPoints/VirtualPointTable.tsx', [
    ['// VirtualPointTable.tsx - 다른 페이지와 동d한 패턴 Apply', '// VirtualPointTable.tsx - same pattern as other pages'],
    ['// 반응형 그리드 컬럼 (Name/수식 위wk)', '// responsive grid columns (name / formula above)'],
    ['/* 체크박스 */', '/* checkbox */'],
    ['/* 이름 */', '/* name */'],
    ['/* 분류 */', '/* category */'],
    ['/* 수식 */', '/* formula */'],
]);

// DeviceRtuNetworkTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ['// 데이터 로드 함수들', '// data loading functions'],
    ['* RTU 마스터의 슬래이브 디바이스들 로드', '* Load slave devices of RTU master'],
    ['`RTU Master ${masterId}의 slave device 로드 ...`', '`Loading slave devices of RTU Master ${masterId}...`'],
    ['// protocol_id 또는 protocol_type을 이용해서 RTU Device들 조회', '// fetch RTU devices using protocol_id or protocol_type'],
    ['// RTU Device 중에서 Current 마스터의 슬래이브들만 Filter링', '// filter slaves of current master from RTU devices'],
    ['* RTU 슬래이브의 마스터 디바이스 정보 로드', '* Load master device info of RTU slave'],
]);

// ProtocolDashboard
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ['// Broker(vhost) 격리가 필요한 경우에만 Instance 및 보안 탭 노출', '// show instance and security tabs only when Broker(vhost) isolation is needed'],
]);

// Dashboard
total += fix('/app/src/pages/Dashboard.tsx', [
    ['// API Failed 시 폴백 데이터 Settings', '// set fallback data on API failure'],
    ['* API 데이터를 대시보드 형식으로 변환 (현재는 getOverview에서 직접 리턴하므로 사용처 확인 후 정리)', '* Convert API data to dashboard format (currently returned directly from getOverview)'],
    ['* ⚠️ 더 이상 사용되지 않거나 단순 매핑용으로 변경될 예정입니다.', '* ⚠️ To be deprecated or changed to simple mapping.'],
    ['// 서비스 목록 보정 (Required 필드 및 아이콘 매핑)', '// normalize service list (required fields and icon mapping)'],
    ["* 폴백 대시보드 데이터 생성", "* Generate fallback dashboard data"],
    ["description: '메시지 Queue 서비스'", "description: 'Message Queue Service'"],
]);

// TemplateManagementTab - 라벨 style 내 한글
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ['>태그 삽입<', '>Insert Tag<'],
    ['>JSON 배열 강제 (|array)<', '>Force JSON Array (|array)<'],
]);

console.log('\nTotal:', total);
