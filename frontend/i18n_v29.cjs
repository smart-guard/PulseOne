/**
 * v29 - DeviceRtuScanDialog + MqttBrokerDashboard + ProtocolEditor + TemplateImportExportModal + TemplateManagementTab 정밀 처리
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

// ExportGatewayWizard - 주석
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ["// auto-select '기존 Target Connect' if targets exist, 'New Target Create' if empty.", "// auto-select existing target if targets exist, else new target"],
    ["'Profile을 Se", "'Select a Profile and configure data points.'"],
]);

// MqttBrokerDashboard - 주석들
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    ["// 탭 Status를 URL 파라미터와 동기화", "// sync tab status with URL params"],
    ["// Protocol 정보 Edit을 위한 로컬 Status", "// local status for protocol info editing"],
    ["// 만약 Active화 시 API Key가 없다면 자동 Create", "// auto-create API key if activating without one"],
    ["// 탭 이동 함수 (URL 기반)", "// tab navigation function (URL-based)"],
]);

// DeviceRtuScanDialog - 주석들
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx', [
    ["// RTU 슬래이브 스캔 대화상자 컴포넌트 - protocol_id 지원", "// RTU slave scan dialog component - supports protocol_id"],
    ["// Protocol 정보 및 RTU Device OK", "// Protocol info and RTU device check"],
    ["* 프로토콜 정보 가져오기", "* Get protocol info"],
    ["* RTU 디바이스 여부 확인", "* Check if RTU device"],
    ["// 1. protocol_type으로 OK", "// 1. check by protocol_type"],
    ["// 2. protocol_id로 OK", "// 2. check by protocol_id"],
    ["// 마스터 Device의 RTU Settings에서 기본값 Import", "// import defaults from master device RTU settings"],
    ["// 모달 닫을 때 Status Reset", "// reset status on modal close"],
    ["// 스캔 Start", "// start scan"],
    ["console.log(`RTU 스캔 Start: Master", "console.log(`RTU Scan Start: Master"],
]);

// ProtocolEditor - 주석 + JSX
total += fix('/app/src/components/modals/ProtocolModal/ProtocolEditor.tsx', [
    ["// Edit 모드에서는 OK 팝업 표시", "// show ok popup in edit mode"],
    ["// 실제 Save 실행 함수", "// actual save function"],
    ["// JSON 파싱 에러는 무시", "// ignore JSON parse errors"],
    ["// 만약 Broker를 Active화하는데 API 키가 없으면 자동 Create", "// auto-create API key if activating broker without one"],
    ["<option value=\"industrial\">산업용</option>", "<option value=\"industrial\">Industrial</option>"],
    ["<option value=\"building_automation\">빌딩 자동화</option>", "<option value=\"building_automation\">Building Automation</option>"],
    ["<option value=\"web\">웹</option>", "<option value=\"web\">Web</option>"],
    ['placeholder="예: 502"', 'placeholder="e.g., 502"'],
    ["<label>Manufacturer/벤더</label>", "<label>Manufacturer/Vendor</label>"],
]);

// TemplateImportExportModal - 주석들
total += fix('/app/src/components/modals/TemplateImportExportModal.tsx', [
    ["// CSV 기반 Template Export/Import 모달", "// CSV-based Template Export/Import modal"],
    ["// CSV 헤더 정의", "// CSV header definition"],
    ["// 적용 가능 데이터 타입", "// applicable data types"],
    ["// 알림 활성화", "// notification enabled"],
    ["// 자동 해제", "// auto clear"],
    ["// Template을 CSV 형태로 변환", "// convert template to CSV format"],
    ["// CSV에서 Template으로 변환", "// convert CSV to template"],
    ["throw new Error(`CSV 행에 필요한 컬럼이 부족합니다. 필요: ${CSV_HEADERS.length}, 실제: ${row.length}`)", "throw new Error(`CSV row has insufficient columns. Required: ${CSV_HEADERS.length}, Actual: ${row.length}`)"],
    ["// CSV 문자열 Create", "// create CSV string"],
    ["// CSV 파d 다운로드", "// download CSV file"],
]);

// TemplateManagementTab - JSX 잔여
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ["// Simple Moded 경우 JSON으로 변환", "// convert to JSON for simple mode"],
    ["<option value=\"status_code\">통신Status (status_code)</option>", "<option value=\"status_code\">Comm Status (status_code)</option>"],
    ["<option value=\"alarm_level\">Alarm등급 (alarm_level)</option>", "<option value=\"alarm_level\">Alarm Level (alarm_level)</option>"],
    [">태그 삽입<", ">Insert Tag<"],
]);

console.log('\nTotal:', total);
