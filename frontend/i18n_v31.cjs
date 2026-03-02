/**
 * v31 - MqttBrokerDashboard + VirtualPointDetailModal + Dashboard + DataExport +
 *        TemplateManagementTab + ExportGatewayWizard + RealTimeMonitor +
 *        DeviceSettingsTab + TemplateImportExportModal + HistoricalData + VirtualPointModal
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

// MqttBrokerDashboard - style 내 한글 (직접 라인에 있는 것들)
total += fix('/app/src/components/protocols/mqtt/MqttBrokerDashboard.tsx', [
    ['저장 완료', 'Saved'],
    ['사양 편집 모드', 'Spec Edit Mode'],
    ['상태 새로고침', 'Refresh Status'],
    ['최대 메모리 사용률', 'Max Memory Usage'],
    ['알림 허용 기준', 'Alert Threshold'],
    ['메모리 관리 정책', 'Memory Policy'],
    ['연결 제한', 'Connection Limit'],
    ['채널 제한', 'Channel Limit'],
    ['큐 제한', 'Queue Limit'],
    ['최대 메시지 크기', 'Max Message Size'],
]);

// VirtualPointDetailModal - 주석 + JSX
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointDetailModal.tsx', [
    ['// Logs 조회 API 호출', '// API call to fetch logs'],
    ['// 탭 변경 핸들러', '// tab change handler'],
    ['// 수식에서 변수 추출 (간단한 파싱)', '// extract variables from formula (simple parsing)'],
    ['return [...new Set(matches.map(m => m[1]))]; // 중복 제거', 'return [...new Set(matches.map(m => m[1]))]; // deduplicate'],
    ['>가상Point 엔진 상세 데이터</h2>', '>Virtual Point Engine Detail</h2>'],
    ['>삭제된 가상포인트<', '>Deleted Virtual Point<'],
    ["'실Time 연산 Active화됨'", "'Real-time Calculation Enabled'"],
    ["'연산 Pause Status'", "'Calculation Paused'"],
    ['>개요 및 상태</i>', '>Overview & Status</i>'],
]);

// Dashboard - 주석들
total += fix('/app/src/pages/Dashboard.tsx', [
    ['// 📋 Type 정의 (DashboardApiService와 d치)', '// 📋 Type definitions (aligned with DashboardApiService)'],
    ['// 팝업 OK 다이얼Logs 인터페이스', '// popup OK dialog interface'],
    ['// 🧩 하위 컴포넌트들', '// 🧩 sub-components'],
    ['// 요약 타d 컴포넌트', '// summary card component'],
    ['// Flow Monitor 시각화 컴포넌트', '// flow monitor visualization component'],
    ['// 🎯 메인 Dashboard 컴포넌트', '// 🎯 main dashboard component'],
    ['// 실Time 업데이트 Settings', '// real-time update settings'],
    ['// Success 메시지 Status', '// success message status'],
]);

// DataExport - 주석들
total += fix('/app/src/pages/DataExport.tsx', [
    ['// Export 데이터 Status', '// export data status'],
    ['// s기 로드', '// initial load'],
    ['// 장치 목록 조회', '// fetch device list'],
    ['// 장치 Select 시 Point 로드', '// load points on device select'],
    ['// 로컬 스토리지에서 History 로드', '// load history from local storage'],
    ['// Export 실행', '// execute export'],
    ['// 실제 API 호출', '// actual API call'],
]);

// TemplateManagementTab - 잔여 JSX
total += fix('/app/src/pages/export-gateway/tabs/TemplateManagementTab.tsx', [
    ['>태그 삽입<', '>Insert Tag<'],
    ['>JSON 배열 강제 (|array)<', '>Force JSON Array (|array)<'],
]);

// ExportGatewayWizard - 잔여 JSX
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ['>전송 모드 가이드<', '>Transmission Mode Guide<'],
    ['>정기 Transmit:</strong>', '>Scheduled Transmission:</strong>'],
    ['>이벤트 기반:</strong>', '>Event-based:</strong>'],
    ['>Transmit 모드 Select</span>', '>Select Transmission Mode</span>'],
]);

// RealTimeMonitor - 주석들
total += fix('/app/src/pages/RealTimeMonitor.tsx', [
    ['// ⚡ 실Time 데이터 Monitoring - 진짜 API Connect + 부드러운 Refresh', '// ⚡ Real-time data monitoring - real API connection + smooth refresh'],
    ['// 즐겨찾기 localStorage 동기화', '// sync favorites to localStorage'],
    ['// Loading 및 Connect Status', '// Loading and connection status'],
    ['// Device 및 데이터Point 정보 (한 번만 로드)', '// Device and DataPoint info (loaded once)'],
    ['// ✍️ 제어 모달 state', '// ✍️ control modal state'],
    ['// 예약 탭', '// reservation tab'],
    ['// 🛠️ 헬퍼 함수들', '// 🛠️ helper functions'],
    ['* 🚨 간단한 알람 생성 함수 (임시)', '* 🚨 simple alarm creation function (temporary)'],
]);

// DeviceSettingsTab - 주석 + JSX
total += fix('/app/src/components/modals/DeviceModal/DeviceSettingsTab.tsx', [
    ['// ⚙️ Device Settings 탭 - 3 Column Dense Layout', '// ⚙️ Device Settings tab - 3 column dense layout'],
    ['// 로컬 Status로 Settings값들 Management', '// manage settings values with local state'],
    ['// Reset - 모든 파라미터 포함', '// reset - including all parameters'],
    ['// 기본값들', '// default values'],
    ['...settings // 실제 설정값으로 덮어쓰기', '...settings // override with actual settings'],
    ['// 토글 업데이트', '// toggle update'],
    ['// 렌더링 헬퍼 함수들', '// rendering helper functions'],
    ["'Active화'", "'Enabled'"],
    ["'비Active화'", "'Disabled'"],
]);

// TemplateImportExportModal - 주석 + 에러 메시지
total += fix('/app/src/components/modals/TemplateImportExportModal.tsx', [
    ['// 파d Select', '// file select'],
    ['// CSV 데이터 파싱', '// parse CSV data'],
    ["throw new Error('CSV 파d이 비어있거나 헤더만 있.')", "throw new Error('CSV file is empty or header only.')"],
    ['// 헤더 검증', '// validate headers'],
    ["throw new Error(`Required 헤더가 누락되었: ${missingHeaders.join(', ')}`)", "throw new Error(`Missing required headers: ${missingHeaders.join(', ')}`)"],
    ['// 데이터 파싱', '// parse data'],
    ['// Default 유효성 검사', '// default validation'],
    ["errors.push(`행 ${i + 2}: Template명이 필요합니다`)", "errors.push(`Row ${i + 2}: Template name is required`)"],
]);

// HistoricalData - 주석들
total += fix('/app/src/pages/HistoricalData.tsx', [
    ['realPointId?: number; // DB의 실제 ID', 'realPointId?: number; // actual DB ID'],
    ['// 메타데이터 인터페이스', '// metadata interface'],
    ['start: new Date(Date.now() - 24 * 60 * 60 * 1000), // 24시간 전', 'start: new Date(Date.now() - 24 * 60 * 60 * 1000), // 24 hours ago'],
    ['// Point 메타데이터 로드', '// load point metadata'],
    ['// 데이터 로드', '// load data'],
    ['// pointNames에 Select된 Point ID들이 들어있음', '// pointNames contains selected point IDs'],
    ['// 데이터 매핑', '// data mapping'],
    ['// 집계 모드인 경우 aggregatedData Settings', '// set aggregatedData for aggregation mode'],
]);

// VirtualPointModal - 주석 + 에러 메시지
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/VirtualPointModal.tsx', [
    ["console.warn('Draft 복원 Failed:", "console.warn('Draft restore failed:"],
    ["console.warn('Draft 정리 Failed:", "console.warn('Draft cleanup failed:"],
    ["title: '작성 중인 내용 복원'", "title: 'Restore Draft'"],
    ["message: 'Previous에 작성하던 내용이 있. 복원??'", "message: 'There is a previously written draft. Restore it?'"],
    ["confirmText: '복원하기'", "confirmText: 'Restore'"],
    ["cancelText: '무시하기'", "cancelText: 'Dismiss'"],
    ["console.error('Formula 검증 Failed:", "console.error('Formula validation failed:"],
    ["errors.name = 'Name은 Required'", "errors.name = 'Name is required'"],
]);

console.log('\nTotal:', total);
