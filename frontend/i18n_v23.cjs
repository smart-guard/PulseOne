/**
 * Dashboard + ScheduleManagementTab + SiteDetailModal + DeviceRtuMonitorTab + BasicInfoForm 처리 v23
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

// ====== Dashboard.tsx ======
total += fix('/app/src/pages/Dashboard.tsx', [
    ["{/* 장식용 배경 */}", "{/* Decorative Background */}"],
    ["return `${days}일 ${hours}시간`", "return `${days}d ${hours}h`"],
    ["return `${hours}시간 ${minutes}분`", "return `${hours}h ${minutes}m`"],
    ["return `${minutes}분`", "return `${minutes}m`"],
    ["throw new Error(response.message || '데이터 로드 실패')", "throw new Error(response.message || 'Data load failed')"],
    ["err.message : '알 수 없는 오류'", "err.message : 'Unknown error'"],
    ["description: '백엔드 서비스'", "description: 'Backend Service'"],
    ["description: 'C++ 데이터 수집 서비스'", "description: 'C++ Data Collection Service'"],
    ["description: '실시간 데이터 캐시'", "description: 'Real-time Data Cache'"],
    ["// 10초", "// 10 seconds"],
]);

// ====== ScheduleManagementTab.tsx ======
total += fix('/app/src/pages/export-gateway/tabs/ScheduleManagementTab.tsx', [
    ["{ label: '매분 (Every minute)', value: '* * * * *', description: '매 분마다 데이터를 전송합니다.' }", "{ label: 'Every minute', value: '* * * * *', description: 'Transmits data every minute.' }"],
    ["{ label: '5분마다 (Every 5 mins)', value: '*/5 * * * *', description: '5분 간격으로 데이터를 전송합니", "{ label: 'Every 5 mins', value: '*/5 * * * *', description: 'Transmits data every 5 minutes."],
    ["{ label: '10분마다 (Every 10 mins)', value: '*/10 * * * *', description: '10분 간격으로 데이터를 ", "{ label: 'Every 10 mins', value: '*/10 * * * *', description: 'Transmits data every 10 minutes."],
    ["{ label: '15분마다 (Every 15 mins)', value: '*/15 * * * *', description: '15분 간격으로 데이터를 ", "{ label: 'Every 15 mins', value: '*/15 * * * *', description: 'Transmits data every 15 minutes."],
    ["{ label: '30분마다 (Every 30 mins)', value: '*/30 * * * *', description: '30분 간격으로 데이터를 ", "{ label: 'Every 30 mins', value: '*/30 * * * *', description: 'Transmits data every 30 minutes."],
    ["{ label: '매시간 (Hourly)', value: '0 * * * *', description: '매 정시마다 데이터를 전송합니다.' }", "{ label: 'Hourly', value: '0 * * * *', description: 'Transmits data every hour.' }"],
    ["{ label: '매일 자정 (Daily 00:00)', value: '0 0 * * *', description: '매일 자정(00:00)에 데이터를 ", "{ label: 'Daily 00:00', value: '0 0 * * *', description: 'Transmits data daily at midnight."],
    ["{ label: '매일 오전 9시 (Daily 09:00)', value: '0 9 * * *', description: '매일 오전 9시에 데이터를 전", "{ label: 'Daily 09:00', value: '0 9 * * *', description: 'Transmits data daily at 9:00 AM."],
    ["{ label: '매주 월요일 (Weekly Mon)', value: '0 0 * * 1', description: '매주 월요일 자정에 데이터를 전송합", "{ label: 'Weekly Mon', value: '0 0 * * 1', description: 'Transmits data every Monday at midnight."],
    ["message: '수정 중인 내용이 있습니다. 저장하지 않고 닫으시면 모든 데이터가 사라집니다. 정말 닫으시겠습니까?'", "message: 'You have unsaved changes. All data will be lost. Close anyway?'"],
]);

// ====== TemplateImportExportModal.tsx (주석만 있음) ======
total += fix('/app/src/components/modals/TemplateImportExportModal.tsx', [
    ["// 템플릿명", "// Template name"],
    ["// 설명", "// Description"],
    ["// 카테고리", "// Category"],
    ["// 조건 타입", "// Condition type"],
    ["// 심각도", "// Severity"],
    ["// 임계값", "// Threshold"],
    ["// 상한값", "// High limit"],
    ["// 하한값", "// Low limit"],
    ["// 데드밴드", "// Deadband"],
    ["// 메시지 템플릿", "// Message template"],
]);

// ====== BasicInfoForm.tsx (목업 데이터) ======
total += fix('/app/src/components/VirtualPoints/VirtualPointModal/BasicInfoForm.tsx', [
    ["name: '에너지 원단위 (SEC)'", "name: 'Energy Intensity (SEC)'"],
    ["description: '제품 하나를 만드는 데 들어간 에너지 양을 계산합니다.'", "description: 'Calculates energy consumed to produce one unit.'"],
    ["name: '장비 성적계수 (COP)'", "name: 'Equipment COP'"],
    ["category: '설비 효율'", "category: 'Equipment Efficiency'"],
    ["description: '냉동기나 히트펌프의 투입 에너지 대비 성능을 측정합니다.'", "description: 'Measures performance ratio of chiller or heat pump.'"],
    ["name: '탄소 배출량 (CO2)'", "name: 'Carbon Emissions (CO2)'"],
    ["category: '환경'", "category: 'Environment'"],
    ["description: '전력 사용량을 탄소 배출량으로 환산합니다. (한국 전력 배출계수 기준)'", "description: 'Converts electricity consumption to carbon emissions (Korean grid factor).'"],
    ["name: '가동률 (Availability)'", "name: 'Availability Rate'"],
]);

// ====== SiteDetailModal.tsx ======
total += fix('/app/src/components/modals/SiteModal/SiteDetailModal.tsx', [
    ["{/* 기본 정보 */}", "{/* Basic Info */}"],
    ["<option value=\"building\">빌딩/건물</option>", "<option value=\"building\">Building</option>"],
    ["<option value=\"floor\">층</option>", "<option value=\"floor\">Floor</option>"],
    ["<option value=\"room\">실</option>", "<option value=\"room\">Room</option>"],
    ["{/* 위치 + 상위 사이트 */}", "{/* Location + Parent Site */}"],
    ["> 위치 정보</div>", "> Location Info</div>"],
    ["<div style={fieldLabel}>상위 사이트</div>", "<div style={fieldLabel}>Parent Site</div>"],
    ["<option value=\"\">없음 (최상위)</option>", "<option value=\"\">None (Top Level)</option>"],
    ["{/* 담당자 */}", "{/* Contact Person */}"],
    ["> 담당자 정보</div>", "> Contact Info</div>"],
]);

// ====== DeviceRtuMonitorTab.tsx 남은 한글 ======
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ["<div className=\"stat-label\">처리량</div>", "<div className=\"stat-label\">Throughput</div>"],
    ["<div className=\"stat-label\">오류율</div>", "<div className=\"stat-label\">Error Rate</div>"],
    ["{/* 상세 통계 */}", "{/* Detailed Stats */}"],
    ["<h4>응답시간 통계</h4>", "<h4>Response Time Stats</h4>"],
    ["<span>평균:</span>", "<span>Avg:</span>"],
    ["<span>최소:</span>", "<span>Min:</span>"],
    ["<span>최대:</span>", "<span>Max:</span>"],
    ["<span>연결 유지 시간:</span>", "<span>Uptime:</span>"],
    ["<span>마지막 통신:</span>", "<span>Last Comm:</span>"],
    ["<span>성공률:</span>", "<span>Success Rate:</span>"],
]);

console.log('\nTotal:', total);
