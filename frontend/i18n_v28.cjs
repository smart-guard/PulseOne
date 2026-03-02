/**
 * v28 - 추출된 구체적 패턴으로 정밀 처리
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
    ['템플릿 JSON 미리보기', 'Template JSON Preview'],
    ['+ HTTP 타겟/Mirror 추가', '+ Add HTTP Target/Mirror'],
    ['+ MQTT 타겟 추가', '+ Add MQTT Target'],
    ['+ S3 스토리지 추가', '+ Add S3 Storage'],
]);

// TemplateManagementTab
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ["참d때값:거짓d때값", "true_value:false_value"],
    ["시스템 Type", "System Type"],
    ["등록된 템플릿이 없습니다.", "No templates registered."],
    ["데이터 탐색기", "Data Explorer"],
    ["추천 항목", "Recommended"],
    ["매핑 명칭", "Mapping Name"],
    ["데이터 값", "Data Value"],
]);

// MqttBrokerDashboard - 직접 라인 추출
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    ['상태 새로고침', 'Refresh Status'],
    ['저장 완료', 'Saved'],
    ['사양 편집 모드', 'Spec Edit Mode'],
]);

// InputVariableSourceSelector - console.log 한글 처리
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/InputVariableSourceSelector.tsx', [
    ["'🔄 데이터Point Loading:'", "'🔄 DataPoint Loading:'"],
    ["'✅ 데이터Point '", "'✅ DataPoint '"],
    ["'⚠️ Device ID '", "'⚠️ Device ID '"],
    ["'에 대한 데이터Point가 없습니다.'", "': no DataPoints found.'"],
    ["'❌ 데이터Point Loading Failed:'", "'❌ DataPoint Loading Failed:'"],
    ["'🔄 가상Point Loading'", "'🔄 VirtualPoint Loading'"],
    ["'📦 가상Point API 응답:'", "'📦 VirtualPoint API Response:'"],
    ["'✅ 가상Point '", "'✅ VirtualPoint '"],
    ["'예상하지 못한 가상Point 응답 구조:'", "'Unexpected VirtualPoint response structure:'"],
]);

// DeviceRtuScanDialog - console.log 한글 처리
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx', [
    ["'RTU Master Settings에서 Default Apply:'", "'RTU Master Settings: Default Apply:'"],
    ["'RTU 스캔 Start: Master '", "'RTU Scan Start: Master '"],
    ["'스캔 Result:'", "'Scan Result:'"],
    ["'스캔 Complete: '", "'Scan Complete: '"],
    ["'RTU 스캔 Failed:'", "'RTU Scan Failed:'"],
    ["'슬래이브 '", "'Slave '"],
    ["' Register 요청:'", "' Register Request:'"],
    ["' Register Failed:'", "' Register Failed:'"],
    ["' Register Success:'", "' Register Success:'"],
]);

// DataExport
total += fix('/app/src/pages/DataExport.tsx', [
    ["'장치 로드 Failed:'", "'Device Load Failed:'"],
    ["'Point 로드 Failed:'", "'Point Load Failed:'"],
    ["'History 로드 Failed'", "'History Load Failed'"],
    ["'알 수 없는 Error'", "'Unknown Error'"],
    [">수집된 생산 및 장치 데이터를 다양한 Format으로 추출합니다.</p>", ">Exports collected production and device data in various formats.</p>"],
]);

// RealTimeMonitor
total += fix('/app/src/pages/RealTimeMonitor.tsx', [
    ["// 🔥 NEW: 멀티 테넌트용", "// multi-tenant support"],
    ["// 일회성: datetime-local", "// one-time: datetime-local"],
    ["// 반복: cron", "// recurring: cron"],
    ["'📱 메타데이터 로드 Start...'", "'📱 Metadata load start...'"],
    ["'❌ 메타데이터 로드 Failed:'", "'❌ Metadata load failed:'"],
    ["'⚡ 실Time 데이터 로드 Start...'", "'⚡ Real-time data load start...'"],
]);

// BasicInfoForm
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx', [
    ["' 빠른 시작: 템플릿 불러오기'", "' Quick Start: Load Template'"],
    ["빠른 시작: 템플릿 불러오기", "Quick Start: Load Template"],
    [">1단계: 기본 식별<", ">Step 1: Basic Identification<"],
    [">가상Point Name<", ">Virtual Point Name<"],
    ['placeholder="예: 제1공장 라인A 전력Total"', 'placeholder="e.g., Factory 1 Line A Power Total"'],
]);

// DeviceTemplateWizard
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ["> 마스터 모델로 디바이스 추가<", "> Add Device from Master Model<"],
    ["<label>시리얼 Port Path *</label>", "<label>Serial Port Path *</label>"],
    ['placeholder="/dev/ttyUSB0 또는 COM1"', 'placeholder="/dev/ttyUSB0 or COM1"'],
    [">3. 데이터포인트 매핑 및 구성 (", ">3. Data Point Mapping & Configuration ("],
    ["사용 가능한 다음 슬레이브 ID는", "Next available slave ID is"],
    ["번 입니다. (자동 적용됨)", "(auto-applied)"],
    ["Current 해당 Port에 Connect된 Device가 없습니다.", "No devices connected to this port."],
]);

// DeviceDetailModal (console 관련)
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["`device ${deviceId} datapoint 로드 start...`", "`device ${deviceId} datapoint load start...`"],
    ["`datapoint ${points.length} load complete (Original 백업 complete)`", "`datapoint ${points.length} load complete`"],
    ["`device ${deviceId} datapoint 로드 failed:`", "`device ${deviceId} datapoint load failed:`"],
    ["'🎨 예쁜 커스텀 OK 모달 표시...'", "'🎨 Custom OK modal shown...'"],
    ["'✅ User가 OK함 - Save 진행'", "'✅ User confirmed - proceeding with save'"],
    ["'구 Create success - Edit 모드로 전환 후 datapoint 탭 오픈'", "'Create success - switching to edit mode'"],
]);

console.log('\nTotal:', total);
