/**
 * v32 - 9개 파일 정밀 처리 (show_final5 추출 결과 기반)
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

// Dashboard
total += fix('/app/src/pages/Dashboard.tsx', [
    ['// 유틸리티 함수', '// utility functions'],
    ['// 🔄 API 호출 및 데이터 로드', '// 🔄 API calls and data loading'],
    ['* 대시보드 개요 데이터 로드 (하나의 통합 API 사용)', '* Load dashboard overview data (using single integrated API)'],
    ["console.log('🎯 통합 Dashboard 데이터 로드 Start...')", "console.log('🎯 Integrated dashboard data load start...')"],
    ['// 단d 통합 API 호출', '// integrated API call'],
    ['// 백엔드 데이터를 프런트엔드 형식에 맞게 변환 및 보정', '// transform and normalize backend data to frontend format'],
    ["console.log('✅ Dashboard 통합 데이터 로드 및 변환 Complete')", "console.log('✅ Dashboard integrated data loaded and transformed')"],
    ["console.error('❌ Dashboard 로드 Failed:'", "console.error('❌ Dashboard load failed:'"],
]);

// DataExport
total += fix('/app/src/pages/DataExport.tsx', [
    ['// load history from local storage (DB 연동 전까지)', '// load history from local storage (until DB integration)'],
    ['// Success 시 Status 업데이트', '// update status on success'],
    ['// 퀵 Template Apply', '// quick template apply'],
    ['{/* 퀵 템플릿 */}', '{/* Quick Templates */}'],
]);

// TemplateManagementTab
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ['>태그 삽입<', '>Insert Tag<'],
    ['>JSON 배열 강제 (|array)<', '>Force JSON Array (|array)<'],
]);

// RealTimeMonitor
total += fix('/app/src/pages/RealTimeMonitor.tsx', [
    ['// 간단한 임계값 기반 Alarm Create', '// simple threshold-based alarm creation'],
    ['`${category} 값이 최소 Threshold(', '`${category} value below minimum threshold('],
    ['`${category} 값이 최대 Threshold(', '`${category} value above maximum threshold('],
    ['// 🔄 실제 API 데이터 로드 함수들', '// 🔄 actual API data loading functions'],
    ["response.error || '백엔드 API 응답 Error'", "response.error || 'Backend API response error'"],
    ["console.error('❌ 실Time 데이터 로드 Failed:'", "console.error('❌ Real-time data load failed:'"],
    ["err instanceof Error ? err.message : '백엔드 API Connect Failed'", "err instanceof Error ? err.message : 'Backend API connection failed'"],
    ["console.error('❌ 실Time 업데이트 Failed:'", "console.error('❌ Real-time update failed:'"],
]);

// DeviceRtuNetworkTab
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuNetworkTab.tsx', [
    ['// RTU 네트워크 Management 탭 - protocol_id 지원', '// RTU network management tab - supports protocol_id'],
    ['// RTU Device OK 및 Settings 파싱 함수들', '// RTU device check and settings parsing functions'],
    ['* RTU 디바이스 여부 확인 - protocol_id 또는 protocol_type 기반', '* Determine if device is RTU - based on protocol_id or protocol_type'],
    ['// 1. protocol_type으로 OK', '// 1. check by protocol_type'],
    ['// 2. protocol_id로 OK (MODBUS_RTU는 보통 ID 2)', '// 2. check by protocol_id (MODBUS_RTU is usually ID 2)'],
    ['* 디바이스 config에서 RTU 설정 추출', '* Extract RTU settings from device config'],
    ['* RTU 마스터 디바이스 여부 판별', '* Determine if device is RTU master'],
    ['* RTU 슬래이브 디바이스 여부 판별', '* Determine if device is RTU slave'],
]);

// DeviceRtuScanDialog
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuScanDialog.tsx', [
    ['// 실제 스캔 API 호출', '// actual scan API call'],
    ['// 실제 스캔 결과 처리', '// process actual scan result'],
    ['// TODO: 실제 구현에서는 scanResult.data를 처리', '// TODO: in actual implementation, process scanResult.data'],
    ['// 실제 구현에서는 Server-Sent Events나 WebSocket으로 진행상황을 받을 수 있음', '// in actual implementation, progress can be received via SSE or WebSocket'],
    ['// 여기서는 시뮬레이션으로 구현', '// implemented here as simulation'],
    ['// 시뮬레이션: 30% 확률로 Device 발견', '// simulation: 30% chance of device found'],
    ['// 스캔 Complete', '// scan complete'],
    ["console.log(`스캔 Complete: ${found.length} Device 발견`)", "console.log(`Scan complete: ${found.length} device(s) found`)"],
]);

// DeviceTemplateWizard
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ['// 접속 정보 검증', '// validate connection info'],
    ['// Protocol Required Settings 검증', '// validate protocol required settings'],
    ['> 마스터 모델로 디바이스 추가</i>', '> Add Device from Master Model</i>'],
    ['>4. 상세 Operations 및 서비스 안정성 Settings을 Complete하십시오.</h4>', '>4. Complete detailed operations and service stability settings.</h4>'],
    ['<label>응답 timeout (ms) *</label>', '<label>Response Timeout (ms) *</label>'],
    ['<label>재시도 간격 (ms)</label>', '<label>Retry Interval (ms)</label>'],
]);

// ProtocolDashboard
total += fix('/app/src/components/protocols/ProtocolDashboard.tsx', [
    ['// 탭 Status (URL과 동기화)', '// tab status (synced with URL)'],
    ['// Edit 모드 Status', '// edit mode status'],
    ['// Instance 목록 조회', '// fetch instance list'],
    ['// Select된 Instance가 없거나 목록에 없으면 첫 번째 Instance Select', '// select first instance if none selected or not in list'],
    ['// Device 목록 조회', '// fetch device list'],
    ['// 공통: Protocol 업데이트 로직', '// common: protocol update logic'],
    ['// 변경된 필드 추출', '// extract changed fields'],
]);

console.log('\nTotal:', total);
