/**
 * i18n_translate2.cjs - 2차 치환 스크립트
 * DeviceModal tabs, ExportGateway, DeviceTemplateWizard 등 나머지 파일 처리
 */
const fs = require('fs');
const path = require('path');

const base = '/app/src';
const localesBase = '/app/public/locales';

function setNestedKey(obj, keyPath, value) {
    const keys = keyPath.split('.');
    let cur = obj;
    for (let i = 0; i < keys.length - 1; i++) {
        if (!cur[keys[i]]) cur[keys[i]] = {};
        cur = cur[keys[i]];
    }
    cur[keys[keys.length - 1]] = value;
}

function updateLocaleFile(lang, ns, keyPath, value) {
    const filePath = path.join(localesBase, lang, ns + '.json');
    let obj = {};
    if (fs.existsSync(filePath)) {
        try { obj = JSON.parse(fs.readFileSync(filePath, 'utf-8')); } catch (e) { }
    }
    setNestedKey(obj, keyPath, value);
    fs.writeFileSync(filePath, JSON.stringify(obj, null, 4), 'utf-8');
}

function replaceJsxText(content, koText, replacement) {
    let changed = false;
    const escaped = koText.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

    const patterns = [
        // >text<
        [new RegExp(`(>\\s*)${escaped}(\\s*<)`, 'g'), `$1{${replacement}}$2`],
        // attribute="text"
        [new RegExp(`(placeholder|title|label|description|alt|aria-label|content|text|value)="${escaped}"`, 'g'), `$1={${replacement}}`],
        [new RegExp(`(placeholder|title|label|description|alt|aria-label|content|text|value)='${escaped}'`, 'g'), `$1={${replacement}}`],
        // message/title in JS object
        [new RegExp(`(message|title|confirmText|cancelText|cancelButtonText|confirmButtonText):\\s*'${escaped}'`, 'g'), `$1: ${replacement}`],
        [new RegExp(`(message|title|confirmText|cancelText|cancelButtonText|confirmButtonText):\\s*"${escaped}"`, 'g'), `$1: ${replacement}`],
    ];

    for (const [pattern, repl] of patterns) {
        const newContent = content.replace(pattern, repl);
        if (newContent !== content) { content = newContent; changed = true; }
    }
    return { content, changed };
}

function processFile(file, ns, koMap) {
    const filePath = path.join(base, file);
    if (!fs.existsSync(filePath)) { console.log(`NOT FOUND: ${file}`); return; }

    let content = fs.readFileSync(filePath, 'utf-8');
    let fileChanged = false;

    for (const [ko, replacement, enKey, enVal] of koMap) {
        const result = replaceJsxText(content, ko, replacement);
        if (result.changed) {
            content = result.content;
            fileChanged = true;
            if (enKey && !enKey.startsWith('common:')) {
                updateLocaleFile('ko', ns, enKey, ko);
                updateLocaleFile('en', ns, enKey, enVal || ko);
            }
        }
    }

    if (fileChanged) {
        fs.writeFileSync(filePath, content, 'utf-8');
        console.log(`DONE: ${file}`);
    } else {
        console.log(`NO CHANGE: ${file}`);
    }
}

// ============================================================
// DeviceBasicInfoTab
// ============================================================
processFile('components/modals/DeviceModal/DeviceBasicInfoTab.tsx', 'devices', [
    ['장치 이름', "t('basicTab.deviceName')", 'basicTab.deviceName', 'Device Name'],
    ['제조사', "t('basicTab.manufacturer')", 'basicTab.manufacturer', 'Manufacturer'],
    ['모델', "t('basicTab.model')", 'basicTab.model', 'Model'],
    ['시리얼 번호', "t('basicTab.serialNo')", 'basicTab.serialNo', 'Serial Number'],
    ['주소', "t('basicTab.address')", 'basicTab.address', 'Address'],
    ['설치 위치', "t('basicTab.location')", 'basicTab.location', 'Installation Location'],
    ['설명', "t('basicTab.description')", 'basicTab.description', 'Description'],
    ['프로토콜', "t('basicTab.protocol')", 'basicTab.protocol', 'Protocol'],
    ['사이트', "t('basicTab.site')", 'basicTab.site', 'Site'],
    ['수집기', "t('basicTab.collector')", 'basicTab.collector', 'Collector'],
    ['그룹', "t('basicTab.group')", 'basicTab.group', 'Group'],
    ['활성화', "t('basicTab.enabled')", 'basicTab.enabled', 'Enabled'],
    ['입력해주세요', "t('common:pleaseEnter')", null, null],
    ['선택해주세요', "t('common:pleaseSelect')", null, null],
]);

// ============================================================
// DeviceDataPointsTab
// ============================================================
processFile('components/modals/DeviceModal/DeviceDataPointsTab.tsx', 'devices', [
    ['데이터 포인트', "t('dpTab.title')", 'dpTab.title', 'Data Points'],
    ['포인트 추가', "t('dpTab.add')", 'dpTab.add', 'Add Point'],
    ['포인트명', "t('dpTab.name')", 'dpTab.name', 'Point Name'],
    ['주소', "t('dpTab.address')", 'dpTab.address', 'Address'],
    ['단위', "t('dpTab.unit')", 'dpTab.unit', 'Unit'],
    ['데이터 타입', "t('dpTab.dataType')", 'dpTab.dataType', 'Data Type'],
    ['읽기/쓰기', "t('dpTab.rw')", 'dpTab.rw', 'Read/Write'],
    ['스케일', "t('dpTab.scale')", 'dpTab.scale', 'Scale'],
    ['오프셋', "t('dpTab.offset')", 'dpTab.offset', 'Offset'],
    ['설명', "t('dpTab.description')", 'dpTab.description', 'Description'],
    ['삭제 확인', "t('dpTab.deleteConfirm')", 'dpTab.deleteConfirm', 'Confirm Delete'],
    ['포인트가 없습니다', "t('dpTab.empty')", 'dpTab.empty', 'No data points'],
]);

// ============================================================
// DeviceDataPointsBulkModal
// ============================================================
processFile('components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', 'devices', [
    ['데이터 포인트 일괄 등록', "t('bulkModal.title')", 'bulkModal.title', 'Bulk Data Point Registration'],
    ['CSV 가져오기', "t('bulkModal.csvImport')", 'bulkModal.csvImport', 'CSV Import'],
    ['직접 입력', "t('bulkModal.manualInput')", 'bulkModal.manualInput', 'Manual Input'],
    ['포인트 목록', "t('bulkModal.list')", 'bulkModal.list', 'Point List'],
    ['행 추가', "t('bulkModal.addRow')", 'bulkModal.addRow', 'Add Row'],
    ['전체 삭제', "t('bulkModal.clearAll')", 'bulkModal.clearAll', 'Clear All'],
    ['파일 선택', "t('bulkModal.selectFile')", 'bulkModal.selectFile', 'Select File'],
    ['미리보기', "t('bulkModal.preview')", 'bulkModal.preview', 'Preview'],
    ['저장 성공', "t('bulkModal.success')", 'bulkModal.success', 'Saved successfully'],
    ['저장 실패', "t('bulkModal.failed')", 'bulkModal.failed', 'Save failed'],
]);

// ============================================================
// DeviceLogsTab
// ============================================================
processFile('components/modals/DeviceModal/DeviceLogsTab.tsx', 'devices', [
    ['장치 로그', "t('logTab.title')", 'logTab.title', 'Device Logs'],
    ['시간', "t('logTab.time')", 'logTab.time', 'Time'],
    ['레벨', "t('logTab.level')", 'logTab.level', 'Level'],
    ['메시지', "t('logTab.message')", 'logTab.message', 'Message'],
    ['소스', "t('logTab.source')", 'logTab.source', 'Source'],
    ['로그가 없습니다', "t('logTab.empty')", 'logTab.empty', 'No logs'],
    ['새로고침', "t('common:refresh')", null, null],
]);

// ============================================================
// DeviceSettingsTab
// ============================================================
processFile('components/modals/DeviceModal/DeviceSettingsTab.tsx', 'devices', [
    ['장치 설정', "t('settingsTab.title')", 'settingsTab.title', 'Device Settings'],
    ['폴링 주기', "t('settingsTab.pollInterval')", 'settingsTab.pollInterval', 'Poll Interval'],
    ['타임아웃', "t('settingsTab.timeout')", 'settingsTab.timeout', 'Timeout'],
    ['재시도 횟수', "t('settingsTab.retries')", 'settingsTab.retries', 'Retry Count'],
    ['연결 유지', "t('settingsTab.keepAlive')", 'settingsTab.keepAlive', 'Keep Alive'],
    ['초', "t('common:seconds')", null, null],
    ['밀리초', "t('common:milliseconds')", null, null],
    ['저장', "t('common:save')", null, null],
]);

// ============================================================
// DeviceStatusTab
// ============================================================
processFile('components/modals/DeviceModal/DeviceStatusTab.tsx', 'devices', [
    ['장치 상태', "t('statusTab.title')", 'statusTab.title', 'Device Status'],
    ['연결 상태', "t('statusTab.connection')", 'statusTab.connection', 'Connection Status'],
    ['마지막 통신', "t('statusTab.lastComm')", 'statusTab.lastComm', 'Last Communication'],
    ['오류 횟수', "t('statusTab.errorCount')", 'statusTab.errorCount', 'Error Count'],
    ['응답 시간', "t('statusTab.responseTime')", 'statusTab.responseTime', 'Response Time'],
    ['업타임', "t('statusTab.uptime')", 'statusTab.uptime', 'Uptime'],
    ['상태 정보가 없습니다', "t('statusTab.empty')", 'statusTab.empty', 'No status info'],
]);

// ============================================================
// DeviceRtuMonitorTab
// ============================================================
processFile('components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', 'devices', [
    ['RTU 모니터', "t('rtuMonitorTab.title')", 'rtuMonitorTab.title', 'RTU Monitor'],
    ['실시간 데이터', "t('rtuMonitorTab.realtime')", 'rtuMonitorTab.realtime', 'Real-time Data'],
    ['포인트명', "t('rtuMonitorTab.pointName')", 'rtuMonitorTab.pointName', 'Point Name'],
    ['현재값', "t('rtuMonitorTab.currentVal')", 'rtuMonitorTab.currentVal', 'Current Value'],
    ['단위', "t('rtuMonitorTab.unit')", 'rtuMonitorTab.unit', 'Unit'],
    ['타임스탬프', "t('rtuMonitorTab.timestamp')", 'rtuMonitorTab.timestamp', 'Timestamp'],
    ['모니터링 시작', "t('rtuMonitorTab.start')", 'rtuMonitorTab.start', 'Start Monitoring'],
    ['모니터링 중지', "t('rtuMonitorTab.stop')", 'rtuMonitorTab.stop', 'Stop Monitoring'],
]);

// ============================================================
// DeviceRtuNetworkTab
// ============================================================
processFile('components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', 'devices', [
    ['RTU 네트워크', "t('rtuNetTab.title')", 'rtuNetTab.title', 'RTU Network'],
    ['네트워크 설정', "t('rtuNetTab.settings')", 'rtuNetTab.settings', 'Network Settings'],
    ['포트', "t('rtuNetTab.port')", 'rtuNetTab.port', 'Port'],
    ['보드레이트', "t('rtuNetTab.baudrate')", 'rtuNetTab.baudrate', 'Baud Rate'],
    ['데이터비트', "t('rtuNetTab.dataBits')", 'rtuNetTab.dataBits', 'Data Bits'],
    ['패리티', "t('rtuNetTab.parity')", 'rtuNetTab.parity', 'Parity'],
    ['스톱비트', "t('rtuNetTab.stopBits')", 'rtuNetTab.stopBits', 'Stop Bits'],
    ['저장', "t('common:save')", null, null],
    ['테스트', "t('common:test')", null, null],
]);

// ============================================================
// DeviceRtuScanDialog
// ============================================================
processFile('components/modals/DeviceModal/DeviceRtuScanDialog.tsx', 'devices', [
    ['RTU 스캔', "t('rtuScan.title')", 'rtuScan.title', 'RTU Scan'],
    ['스캔 범위', "t('rtuScan.range')", 'rtuScan.range', 'Scan Range'],
    ['스캔 시작', "t('rtuScan.start')", 'rtuScan.start', 'Start Scan'],
    ['발견된 장치', "t('rtuScan.found')", 'rtuScan.found', 'Discovered Devices'],
    ['스캔 중', "t('rtuScan.scanning')", 'rtuScan.scanning', 'Scanning...'],
    ['장치 선택', "t('rtuScan.select')", 'rtuScan.select', 'Select Device'],
]);

// ============================================================
// DataPointModal
// ============================================================
processFile('components/modals/DeviceModal/DataPointModal.tsx', 'devices', [
    ['데이터 포인트 등록', "t('dpModal.createTitle')", 'dpModal.createTitle', 'Register Data Point'],
    ['데이터 포인트 수정', "t('dpModal.editTitle')", 'dpModal.editTitle', 'Edit Data Point'],
    ['포인트명', "t('dpModal.name')", 'dpModal.name', 'Point Name'],
    ['주소', "t('dpModal.address')", 'dpModal.address', 'Address'],
    ['단위', "t('dpModal.unit')", 'dpModal.unit', 'Unit'],
    ['데이터 타입', "t('dpModal.dataType')", 'dpModal.dataType', 'Data Type'],
    ['읽기/쓰기 여부', "t('dpModal.readWrite')", 'dpModal.readWrite', 'Read/Write'],
    ['스케일 계수', "t('dpModal.scale')", 'dpModal.scale', 'Scale Factor'],
    ['오프셋', "t('dpModal.offset')", 'dpModal.offset', 'Offset'],
    ['설명', "t('dpModal.description')", 'dpModal.description', 'Description'],
    ['저장 성공', "t('dpModal.success')", 'dpModal.success', 'Saved successfully'],
    ['저장 실패', "t('dpModal.failed')", 'dpModal.failed', 'Save failed'],
]);

// ============================================================
// DeviceTemplateWizard
// ============================================================
processFile('components/modals/DeviceTemplateWizard.tsx', 'deviceTemplates', [
    ['디바이스 템플릿 생성', "t('wizard.createTitle')", 'wizard.createTitle', 'Create Device Template'],
    ['기본 정보', "t('wizard.step1')", 'wizard.step1', 'Basic Info'],
    ['포인트 설정', "t('wizard.step2')", 'wizard.step2', 'Point Settings'],
    ['프로토콜 설정', "t('wizard.step3')", 'wizard.step3', 'Protocol Settings'],
    ['검토 및 완료', "t('wizard.step4')", 'wizard.step4', 'Review & Complete'],
    ['템플릿명', "t('wizard.name')", 'wizard.name', 'Template Name'],
    ['제조사', "t('wizard.manufacturer')", 'wizard.manufacturer', 'Manufacturer'],
    ['모델', "t('wizard.model')", 'wizard.model', 'Model'],
    ['버전', "t('wizard.version')", 'wizard.version', 'Version'],
    ['설명', "t('wizard.description')", 'wizard.description', 'Description'],
    ['생성 완료', "t('wizard.complete')", 'wizard.complete', 'Creation Complete'],
    ['생성 실패', "t('wizard.failed')", 'wizard.failed', 'Creation Failed'],
]);

// ============================================================
// DeviceTemplateDetailModal
// ============================================================
processFile('components/modals/DeviceTemplateDetailModal.tsx', 'deviceTemplates', [
    ['템플릿 상세', "t('detailModal.title')", 'detailModal.title', 'Template Detail'],
    ['기본 정보', "t('detailModal.basic')", 'detailModal.basic', 'Basic Info'],
    ['포인트 목록', "t('detailModal.points')", 'detailModal.points', 'Points'],
    ['템플릿명', "t('detailModal.name')", 'detailModal.name', 'Template Name'],
    ['제조사', "t('detailModal.manufacturer')", 'detailModal.manufacturer', 'Manufacturer'],
    ['버전', "t('detailModal.version')", 'detailModal.version', 'Version'],
    ['포인트 수', "t('detailModal.pointCount')", 'detailModal.pointCount', 'Point Count'],
    ['사용 장치 수', "t('detailModal.usedBy')", 'detailModal.usedBy', 'Used By'],
]);

// ============================================================
// MasterModelModal
// ============================================================
processFile('components/modals/MasterModelModal.tsx', 'deviceTemplates', [
    ['마스터 모델 등록', "t('masterModal.createTitle')", 'masterModal.createTitle', 'Register Master Model'],
    ['마스터 모델 수정', "t('masterModal.editTitle')", 'masterModal.editTitle', 'Edit Master Model'],
    ['모델명', "t('masterModal.name')", 'masterModal.name', 'Model Name'],
    ['제조사', "t('masterModal.manufacturer')", 'masterModal.manufacturer', 'Manufacturer'],
    ['프로토콜', "t('masterModal.protocol')", 'masterModal.protocol', 'Protocol'],
    ['버전', "t('masterModal.version')", 'masterModal.version', 'Version'],
    ['저장 성공', "t('masterModal.success')", 'masterModal.success', 'Saved successfully'],
]);

// ============================================================
// master-model steps
// ============================================================
processFile('components/modals/master-model/BasicInfoStep.tsx', 'deviceTemplates', [
    ['기본 정보 입력', "t('basicStep.title')", 'basicStep.title', 'Enter Basic Info'],
    ['모델명', "t('basicStep.name')", 'basicStep.name', 'Model Name'],
    ['제조사', "t('basicStep.manufacturer')", 'basicStep.manufacturer', 'Manufacturer'],
    ['카테고리', "t('basicStep.category')", 'basicStep.category', 'Category'],
    ['설명', "t('basicStep.description')", 'basicStep.description', 'Description'],
    ['버전', "t('basicStep.version')", 'basicStep.version', 'Version'],
]);

processFile('components/modals/master-model/DataPointsStep.tsx', 'deviceTemplates', [
    ['데이터 포인트 설정', "t('dpStep.title')", 'dpStep.title', 'Data Points Setup'],
    ['포인트 추가', "t('dpStep.add')", 'dpStep.add', 'Add Point'],
    ['포인트명', "t('dpStep.name')", 'dpStep.name', 'Point Name'],
    ['레지스터', "t('dpStep.register')", 'dpStep.register', 'Register'],
    ['타입', "t('dpStep.type')", 'dpStep.type', 'Type'],
    ['단위', "t('dpStep.unit')", 'dpStep.unit', 'Unit'],
    ['스케일', "t('dpStep.scale')", 'dpStep.scale', 'Scale'],
]);

processFile('components/modals/master-model/ProtocolConfigStep.tsx', 'deviceTemplates', [
    ['프로토콜 설정', "t('protocolStep.title')", 'protocolStep.title', 'Protocol Settings'],
    ['통신 방식', "t('protocolStep.commType')", 'protocolStep.commType', 'Communication Type'],
    ['타임아웃', "t('protocolStep.timeout')", 'protocolStep.timeout', 'Timeout'],
    ['재시도', "t('protocolStep.retry')", 'protocolStep.retry', 'Retry'],
    ['폴링 주기', "t('protocolStep.pollInterval')", 'protocolStep.pollInterval', 'Poll Interval'],
]);

// ============================================================
// AlarmTemplateCreateEditModal-Wizard
// ============================================================
processFile('components/modals/AlarmTemplateCreateEditModal-Wizard.tsx', 'alarms', [
    ['알람 템플릿 생성', "t('templateWizard.createTitle')", 'templateWizard.createTitle', 'Create Alarm Template'],
    ['알람 템플릿 수정', "t('templateWizard.editTitle')", 'templateWizard.editTitle', 'Edit Alarm Template'],
    ['기본 정보', "t('templateWizard.step1')", 'templateWizard.step1', 'Basic Info'],
    ['조건 설정', "t('templateWizard.step2')", 'templateWizard.step2', 'Condition Settings'],
    ['알림 설정', "t('templateWizard.step3')", 'templateWizard.step3', 'Notification Settings'],
    ['검토', "t('templateWizard.step4')", 'templateWizard.step4', 'Review'],
    ['템플릿명', "t('templateWizard.name')", 'templateWizard.name', 'Template Name'],
    ['설명', "t('templateWizard.description')", 'templateWizard.description', 'Description'],
    ['카테고리', "t('templateWizard.category')", 'templateWizard.category', 'Category'],
    ['심각도', "t('templateWizard.severity')", 'templateWizard.severity', 'Severity'],
    ['완료', "t('templateWizard.complete')", 'templateWizard.complete', 'Complete'],
]);

// ============================================================
// ExportGateway Tabs
// ============================================================
processFile('pages/export-gateway/tabs/GatewayListTab.tsx', 'dataExport', [
    ['게이트웨이 목록', "t('gwTab.title')", 'gwTab.title', 'Gateway List'],
    ['게이트웨이 추가', "t('gwTab.add')", 'gwTab.add', 'Add Gateway'],
    ['게이트웨이명', "t('gwTab.name')", 'gwTab.name', 'Gateway Name'],
    ['상태', "t('gwTab.status')", 'gwTab.status', 'Status'],
    ['연결된 타겟', "t('gwTab.targets')", 'gwTab.targets', 'Connected Targets'],
    ['마지막 활동', "t('gwTab.lastActivity')", 'gwTab.lastActivity', 'Last Activity'],
    ['삭제 확인', "t('gwTab.deleteConfirm')", 'gwTab.deleteConfirm', 'Confirm Delete'],
]);

processFile('pages/export-gateway/tabs/TargetManagementTab.tsx', 'dataExport', [
    ['타겟 관리', "t('targetTab.title')", 'targetTab.title', 'Target Management'],
    ['타겟 추가', "t('targetTab.add')", 'targetTab.add', 'Add Target'],
    ['타겟명', "t('targetTab.name')", 'targetTab.name', 'Target Name'],
    ['타겟 유형', "t('targetTab.type')", 'targetTab.type', 'Target Type'],
    ['URL', "t('targetTab.url')", 'targetTab.url', 'URL'],
    ['인증', "t('targetTab.auth')", 'targetTab.auth', 'Auth'],
    ['활성화', "t('targetTab.enabled')", 'targetTab.enabled', 'Enabled'],
    ['테스트', "t('targetTab.test')", 'targetTab.test', 'Test'],
    ['삭제 확인', "t('targetTab.deleteConfirm')", 'targetTab.deleteConfirm', 'Confirm Delete'],
]);

processFile('pages/export-gateway/tabs/ProfileManagementTab.tsx', 'dataExport', [
    ['프로파일 관리', "t('profileTab.title')", 'profileTab.title', 'Profile Management'],
    ['프로파일 추가', "t('profileTab.add')", 'profileTab.add', 'Add Profile'],
    ['프로파일명', "t('profileTab.name')", 'profileTab.name', 'Profile Name'],
    ['게이트웨이', "t('profileTab.gateway')", 'profileTab.gateway', 'Gateway'],
    ['타겟', "t('profileTab.target')", 'profileTab.target', 'Target'],
    ['매핑 규칙', "t('profileTab.mapping')", 'profileTab.mapping', 'Mapping Rules'],
    ['활성화', "t('profileTab.enabled')", 'profileTab.enabled', 'Enabled'],
]);

processFile('pages/export-gateway/tabs/ScheduleManagementTab.tsx', 'dataExport', [
    ['스케줄 관리', "t('scheduleTab.title')", 'scheduleTab.title', 'Schedule Management'],
    ['스케줄 추가', "t('scheduleTab.add')", 'scheduleTab.add', 'Add Schedule'],
    ['스케줄명', "t('scheduleTab.name')", 'scheduleTab.name', 'Schedule Name'],
    ['실행 주기', "t('scheduleTab.interval')", 'scheduleTab.interval', 'Interval'],
    ['다음 실행', "t('scheduleTab.nextRun')", 'scheduleTab.nextRun', 'Next Run'],
    ['마지막 실행', "t('scheduleTab.lastRun')", 'scheduleTab.lastRun', 'Last Run'],
    ['활성화', "t('scheduleTab.enabled')", 'scheduleTab.enabled', 'Enabled'],
]);

processFile('pages/export-gateway/tabs/TemplateManagementTab.tsx', 'dataExport', [
    ['템플릿 관리', "t('templateTab.title')", 'templateTab.title', 'Template Management'],
    ['템플릿 추가', "t('templateTab.add')", 'templateTab.add', 'Add Template'],
    ['템플릿명', "t('templateTab.name')", 'templateTab.name', 'Template Name'],
    ['페이로드 형식', "t('templateTab.format')", 'templateTab.format', 'Payload Format'],
    ['미리보기', "t('templateTab.preview')", 'templateTab.preview', 'Preview'],
    ['저장 성공', "t('templateTab.success')", 'templateTab.success', 'Saved successfully'],
]);

processFile('pages/export-gateway/tabs/ManualTestTab.tsx', 'dataExport', [
    ['수동 데이터 전송 테스트', "t('manualTab.title')", 'manualTab.title', 'Manual Data Transmission Test'],
    ['게이트웨이 선택', "t('manualTab.selectGw')", 'manualTab.selectGw', 'Select Gateway'],
    ['포인트 정보', "t('manualTab.pointInfo')", 'manualTab.pointInfo', 'Point Info'],
    ['전송 대상', "t('manualTab.target')", 'manualTab.target', 'Target'],
    ['전송 필드명', "t('manualTab.fieldName')", 'manualTab.fieldName', 'Field Name'],
    ['전송 테스트', "t('manualTab.test')", 'manualTab.test', 'Test Send'],
    ['전송 요청 성공', "t('manualTab.success')", 'manualTab.success', 'Send Request Success'],
    ['전송 실패', "t('manualTab.failed')", 'manualTab.failed', 'Send Failed'],
    ['할당된 매핑 정보가 없습니다', "t('manualTab.noMapping')", 'manualTab.noMapping', 'No mappings assigned'],
    ['매핑 정보 로딩 중', "t('manualTab.loadingMapping')", 'manualTab.loadingMapping', 'Loading mappings...'],
    ['테스트를 진행할 게이트웨이를 선택해 주세요', "t('manualTab.selectGwHint')", 'manualTab.selectGwHint', 'Please select a gateway to test'],
    ['새로고침', "t('common:refresh')", null, null],
    ['모든 타겟으로 전송', "t('manualTab.sendAll')", 'manualTab.sendAll', 'Send to All Targets'],
    ['취소', "t('common:cancel')", null, null],
    ['대상 타겟', "t('manualTab.targetLabel')", 'manualTab.targetLabel', 'Target'],
    ['전송 필드', "t('manualTab.fieldLabel')", 'manualTab.fieldLabel', 'Field'],
    ['원본 포인트', "t('manualTab.sourcePoint')", 'manualTab.sourcePoint', 'Source Point'],
    ['전송할 데이터 값', "t('manualTab.dataValue')", 'manualTab.dataValue', 'Data Value'],
    ['알람 상태 값', "t('manualTab.alarmStatus')", 'manualTab.alarmStatus', 'Alarm Status'],
    ['전송 데이터 확인', "t('manualTab.viewPayload')", 'manualTab.viewPayload', 'View Payload'],
    ['전송 페이로드 미리보기', "t('manualTab.payloadPreview')", 'manualTab.payloadPreview', 'Payload Preview'],
]);

processFile('pages/export-gateway/wizards/ExportGatewayWizard.tsx', 'dataExport', [
    ['게이트웨이 생성', "t('gwWizard.createTitle')", 'gwWizard.createTitle', 'Create Gateway'],
    ['게이트웨이 수정', "t('gwWizard.editTitle')", 'gwWizard.editTitle', 'Edit Gateway'],
    ['기본 설정', "t('gwWizard.step1')", 'gwWizard.step1', 'Basic Settings'],
    ['타겟 연결', "t('gwWizard.step2')", 'gwWizard.step2', 'Connect Targets'],
    ['데이터 매핑', "t('gwWizard.step3')", 'gwWizard.step3', 'Data Mapping'],
    ['검토 및 완료', "t('gwWizard.step4')", 'gwWizard.step4', 'Review & Complete'],
    ['게이트웨이명', "t('gwWizard.name')", 'gwWizard.name', 'Gateway Name'],
    ['설명', "t('gwWizard.description')", 'gwWizard.description', 'Description'],
    ['사이트', "t('gwWizard.site')", 'gwWizard.site', 'Site'],
    ['생성 완료', "t('gwWizard.complete')", 'gwWizard.complete', 'Creation Complete'],
    ['생성 실패', "t('gwWizard.failed')", 'gwWizard.failed', 'Creation Failed'],
]);

processFile('pages/export-gateway/components/DataPointSelector.tsx', 'dataExport', [
    ['데이터 포인트 선택', "t('dpSelector.title')", 'dpSelector.title', 'Select Data Points'],
    ['검색', "t('dpSelector.search')", 'dpSelector.search', 'Search'],
    ['선택된 포인트', "t('dpSelector.selected')", 'dpSelector.selected', 'Selected Points'],
    ['전체 선택', "t('dpSelector.selectAll')", 'dpSelector.selectAll', 'Select All'],
    ['선택 해제', "t('dpSelector.deselectAll')", 'dpSelector.deselectAll', 'Deselect All'],
    ['포인트가 없습니다', "t('dpSelector.empty')", 'dpSelector.empty', 'No data points'],
]);

console.log('\n=== 2차 스크립트 완료 ===');
