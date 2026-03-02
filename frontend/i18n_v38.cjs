/**
 * v38 - 16개 파일 잔여 한글 처리
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
    ['// Type 정의', '// Type definitions'],
    ['// 🔥 Edit 모드 진입 시 Logs 탭 등 유효하지 않은 탭 처리', '// 🔥 handle invalid tabs on entering edit mode'],
    ['// 🎨 예쁜 커스텀 모달 Status', '// 🎨 custom modal status'],
]);

// DeviceDataPointsBulkModal
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ['// 히스토리 기록 함수', '// history recording function'],
    ['// 데이터가 이미 있거나 (Step 3에서 넘어옴) 마운트된 Status면 Reset 너뜀', '// skip reset if data already exists (from step 3) or already mounted'],
    ['// 1. New Device Create(deviceId === 0)인 경우:', '// 1. when creating new device (deviceId === 0):'],
    ['// Previous 작업 내역(bulk_draft_device_0)이 있으면 자동으로 로드하지 않고 빈 행으로 Start하도록 유도', '// if previous draft (bulk_draft_device_0) exists, start with empty rows instead'],
]);

// NetworkSettings
total += fix('/app/src/pages/NetworkSettings.tsx', [
    ['// 바이트를 읽기 쉬운 형태로 변환', '// convert bytes to human-readable format'],
    ['// Status 아이콘', '// status icon'],
    ['{/* 페이지 헤더 */}', '{/* Page Header */}'],
]);

// DeviceRtuMonitorTab  
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ['DeviceApiService의 ProtocolManager를 통해 Protocol 정보 OK', 'check protocol info via DeviceApiService ProtocolManager'],
    ['// RTU Device가 아니면 렌더링하지 않음', '// do not render if not RTU device'],
    ['// Statistics 데이터 로드', '// load statistics data'],
    ['RTU Statistics 로드 start:', 'RTU Statistics load start:'],
]);

// ManufacturerDetailModal
total += fix('/app/src/components/modals/ManufacturerModal/ManufacturerDetailModal.tsx', [
    ["'미국': 'USA'", "'미국': 'USA'"],
    ["'d본': 'Japan'", "'일본': 'Japan'"],
    ["'대한민국'", "'대한민국'"],
    ["'한국'", "'한국'"],
]);

// HistoricalData
total += fix('/app/src/pages/HistoricalData.tsx', [
    ['// s기 로드', '// initial load'],
    ['// 빠른 Date Settings', '// quick date settings'],
    ['// 데이터 Export', '// data export'],
    ['// Filter링 가능한 메타데이터 계산 (계층적/Cascading)', '// compute filterable metadata (hierarchical/cascading)'],
]);

// AlarmCreateEditModal
total += fix('/app/src/components/modals/AlarmCreateEditModal.tsx', [
    ["name.includes('온도')", "name.includes('temperature')"],
    ["`[${dp.name}] 데이터Point의 온도 Abnormal을 감지하는 규칙.`", "`[${dp.name}] Rule to detect temperature anomaly.`"],
    ["name.includes('압력')", "name.includes('pressure')"],
    ["`[${dp.name}] 데이터Point의 압력 변화를 Monitoring합니다.`", "`[${dp.name}] Monitors pressure changes.`"],
]);

// DeviceDataPointsTab
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsTab.tsx', [
    ['데이터Point 탭 - 고급 필드 (스케d링, 로깅 등) 포함 확장 Version', 'DataPoint tab - extended version with advanced fields (scaling, logging, etc.)'],
    ['// URL 파라미터로 Bulk Modal Status 복원', '// restore bulk modal status from URL params'],
    ['// Bulk Modal Status 변경 시 URL 업데이트 핸들러', '// handler to update URL on bulk modal status change'],
    ['// 폼 탭 Management', '// form tab management'],
]);

// DeviceRtuScanDialog
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx', [
    ['<label>패리티</label>', '<label>Parity</label>'],
    ['슬래이브 ID ', 'Slave ID '],
    ['총 ', 'Total '],
    ['개 디바이스를 스캔합니다.', ' device(s) to scan.'],
    ['예상 소요시간: 약 ', 'Estimated time: ~'],
    ['초', 's'],
]);

// DeviceTemplateWizard
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ['메타데이터: 포함됨 (JSON)', 'Metadata: included (JSON)'],
    ['Communication & Technical 사양 Settings', 'Communication & Technical Spec Settings'],
    ['<label>Keep-Alive Session 유지</label>', '<label>Keep-Alive Session</label>'],
]);

// SiteModal
total += fix('/app/src/components/modals/SiteModal/SiteModal.tsx', [
    ['// 🔥 기존의 모든 필드(metadata, tags 등) 보존', '// 🔥 preserve all existing fields (metadata, tags, etc.)'],
    ["'Site명과 코드는 Required 항목.'", "'Site name and code are required.'"],
    ["title: '변경 사항 Save'", "title: 'Save Changes'"],
    ["message: 'Input하신 변경 내용을 Save??'", "message: 'Save the changes you entered?'"],
]);

// TenantDetailModal
total += fix('/app/src/components/modals/TenantModal/TenantDetailModal.tsx', [
    ['Tenant 정보를 불러오지 못했.', 'Failed to load tenant information.'],
    ['{/* 기본 정보 */}', '{/* Basic Info */}'],
    ['>회사 코드</div>', '>Company Code</div>'],
    ['{/* 담당자 정보 */}', '{/* Contact Info */}'],
]);

// TenantModal
total += fix('/app/src/components/modals/TenantModal/TenantModal.tsx', [
    ["'company name과 회사 코드는 Required 항목.'", "'Company name and company code are required.'"],
    ["title: '변경 사항 Save'", "title: 'Save Changes'"],
    ["message: 'Input하신 변경 내용을 Save??'", "message: 'Save the changes you entered?'"],
    ["message: 'Tenant 정보가 Success적으로 Edit되었.'", "message: 'Tenant information updated successfully.'"],
]);

// MqttSecuritySlot
total += fix('/app/src/components/protocols/mqtt/MqttSecuritySlot.tsx', [
    ["`[${instance.instance_name}] Instance를 Active화??`", "`Enable [${instance.instance_name}] instance?`"],
    ['Instance를 비Active화하면 해당', 'Disabling the instance will stop all sensor connections for'],
    ['를 Use하는 모든 센서의 접속이 중단.', '.'],
    ["title: enabled ? 'Instance Active화' : 'Instance 비Active화'", "title: enabled ? 'Enable Instance' : 'Disable Instance'"],
    ["confirmText: enabled ? 'Active화' : '서비스 중단'", "confirmText: enabled ? 'Enable' : 'Stop Service'"],
]);

// ActiveAlarms
total += fix('/app/src/pages/ActiveAlarms.tsx', [
    ['// 1. s기 Connect', '// 1. initial connect'],
    ['// 2. Connect Status 변화 구독', '// 2. subscribe to connection status changes'],
    ['// 3. New Alarm 이벤트 구독', '// 3. subscribe to new alarm events'],
    ['// Alarm 목록과 Statistics Refresh', '// refresh alarm list and statistics'],
]);

// InputVariableSourceSelector
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx', [
    ['// InputVariableSourceSelector.tsx - Device Filter링 및 성능 최적화', '// InputVariableSourceSelector.tsx - device filtering and performance optimization'],
    ["console.log('🔄 Device 목록 Loading')", "console.log('🔄 Device list loading')"],
    ['`✅ Device ${devices.length} 로드됨`', '`✅ Device ${devices.length} loaded`'],
    ['// 목 데이터', '// mock data'],
]);

console.log('\nTotal:', total);
