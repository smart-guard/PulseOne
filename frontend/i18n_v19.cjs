/**
 * 잔여 한글 처리 v19 - 추출된 실제 패턴으로 정밀 교체
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

// ====== DeviceRtuMonitorTab.tsx (실제 확인된 패턴) ======
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ["<option value=\"1min\">1분</option>", "<option value=\"1min\">1 min</option>"],
    ["<option value=\"5min\">5분</option>", "<option value=\"5min\">5 min</option>"],
    ["<option value=\"15min\">15분</option>", "<option value=\"15min\">15 min</option>"],
    ["<option value=\"1hour\">1시간</option>", "<option value=\"1hour\">1 hour</option>"],
    ["<label>현재 응답시간</label>", "<label>Current Response Time</label>"],
    ["<h4>응답시간 추이</h4>", "<h4>Response Time Trend</h4>"],
    ["<span>통신 실패</span>", "<span>Comm Failed</span>"],
    ["<h4>최근 통신 기록</h4>", "<h4>Recent Comm History</h4>"],
    ["<div>시간</div>", "<div>Time</div>"],
    ["<div>송신</div>", "<div>TX</div>"],
    ["<div>수신</div>", "<div>RX</div>"],
    ["{metric.successful ? '성공' : metric.errorCode || '실패'}", "{metric.successful ? 'OK' : metric.errorCode || 'FAIL'}"],
    ["{/* 통계 카드들 */}", "{/* Stats Cards */}"],
    ["<div className=\"stat-label\">총 요청</div>", "<div className=\"stat-label\">Total Requests</div>"],
    ["<div className=\"stat-label\">성공한 요청</div>", "<div className=\"stat-label\">Successful</div>"],
    ["<div className=\"stat-label\">실패한 요청</div>", "<div className=\"stat-label\">Failed</div>"],
    ["<div className=\"stat-label\">평균 응답시간</div>", "<div className=\"stat-label\">Avg Response</div>"],
]);

// ====== DeviceDataPointsBulkModal.tsx (실제 확인된 패턴) ======
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ["message: `${validPoints.length}개의 포인트 데이터를 현재 디바이스에 등록하시겠습니까?`", "message: `Register ${validPoints.length} data points to this device?`"],
    ["confirmText: '등록 시작'", "confirmText: 'Register'"],
    ["localStorage.removeItem(STORAGE_KEY); // 저장 성공 시 임시 데이터 삭제", "localStorage.removeItem(STORAGE_KEY); // clear temp data on save success"],
    ["title: '저장 오류'", "title: 'Save Error'"],
    ["message: '저장 중 오류가 발생했습니다.'", "message: 'Error occurred while saving.'"],
    ["<h2>데이터포인트 일괄 등록</h2>", "<h2>Bulk Data Point Registration</h2>"],
    ["title=\"되돌리기 (Ctrl+Z)\"", "title=\"Undo (Ctrl+Z)\""],
    ["title=\"다시실행 (Ctrl+Y)", "title=\"Redo (Ctrl+Y)"],
    ["<p className=\"main-desc\">엑셀 데이터를 복사(Ctrl+C)하여 아래 표에 붙여넣기(Ctrl+V) 하세요.</p>", "<p className=\"main-desc\">Copy Excel data (Ctrl+C) and paste (Ctrl+V) into the table below.</p>"],
    ["<li>이름*, 주소*는 필수 입력 항목이며, 주소는 중복될 수 없습니다.</li>", "<li>Name* and Address* are required fields; addresses must be unique.</li>"],
    ["<li>엔터(Enter) 키로 행 이동이 가능하며, 마지막 행에서 엔터 시 새 행이 자동 추가됩니다.</li>", "<li>Press Enter to move between rows; pressing Enter on the last row adds a new row.</li>"],
    ["<li>잘못 입력된 셀은 빨간색으로 표시됩니다.</li>", "<li>Cells with invalid input are highlighted in red.</li>"],
    ["<th className=\"col-name\">이름 *</th>", "<th className=\"col-name\">Name *</th>"],
    ["{protocolType === 'MQTT' ? 'Sub-Topic *' : '주소 *'}", "{protocolType === 'MQTT' ? 'Sub-Topic *' : 'Address *'}"],
    ["<th className=\"col-type\">타입</th>", "<th className=\"col-type\">Type</th>"],
]);

// ====== ExportGatewayWizard.tsx (실제 확인된 패턴) ======
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    ["적용\n", "Apply\n"],
    [">적용<", ">Apply<"],
    [">템플릿 JSON 미리보기<", ">Template JSON Preview<"],
    ["<label>전송 프로토콜 (다중 선택 가능)</label>", "<label>Transmission Protocol (multi-select)</label>"],
    [">+ HTTP 타겟/Mirror 추가<", ">+ Add HTTP Target/Mirror<"],
    ["MQTT 타겟 #", "MQTT Target #"],
    [">+ MQTT 타겟 추가<", ">+ Add MQTT Target<"],
    ['placeholder="Broker URL (예: mqtt://broker.hivemq.com:1883)"', 'placeholder="Broker URL (e.g., mqtt://broker.hivemq.com:1883)"'],
    ['placeholder="Topic (예: pulseone/factory/data)"', 'placeholder="Topic (e.g., pulseone/factory/data)"'],
]);

// ====== DeviceTemplateWizard.tsx (실제 확인된 패턴) ======
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ["> 마스터 모델로 디바이스 추가</", "> Add Device from Master Model</"],
    ["<label>제조사 (Manufacturer)</label>", "<label>Manufacturer</label>"],
    ["<option value=\"\">제조사 선택</option>", "<option value=\"\">Select Manufacturer</option>"],
    ["<label>하드웨어 모델 (Model)</label>", "<label>Hardware Model</label>"],
    ["<option value=\"\">모델 선택</option>", "<option value=\"\">Select Model</option>"],
    ["사용 가능한 마스터 모델 (Templates)\n", "Available Master Models (Templates)\n"],
    ["<label>디바이스 이름 *</label>", "<label>Device Name *</label>"],
    ['placeholder="예: 제1공장 인버터 #1"', 'placeholder="e.g., Factory 1 Inverter #1"'],
    ["<label>설치 사이트 *</label>", "<label>Installation Site *</label>"],
    ["<option value=\"\">사이트 선택</option>", "<option value=\"\">Select Site</option>"],
    ["<label>담당 콜렉터 (Edge Server) *</label>", "<label>Collector (Edge Server) *</label>"],
    ["<option value={0}>콜렉터 선택</option>", "<option value={0}>Select Collector</option>"],
    ["> 통신 및 기술", "> Communication & Technical"],
    ["<label>IP 주소 및 포트 *</label>", "<label>IP Address & Port *</label>"],
    ["■ 데이터 구성", "■ Data Configuration"],
    ["- 데이터포인트: ${templateDataPoints.length} 개", "- Data points: ${templateDataPoints.length}"],
]);

// ====== DeviceDetailModal.tsx (실제 확인된 패턴) ======
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["? '연결중' : '알수없음'}", "? 'Connecting' : 'Unknown'}"],
    ["{/* 위저드 진행 표시기 (생성 모드일 때만) */}", "{/* Wizard progress indicator (create mode only) */}"],
    ["{/* 탭 네비게이션 (상세/수정 모드일 때만) */}", "{/* Tab navigation (detail/edit mode only) */}"],
    ["설정\n", "Settings\n"],
    ["데이터포인트 ({dataPoints.length})\n", "Data Points ({dataPoints.length})\n"],
    ["deviceHelpers.isRtuMaster(displayData) ? 'RTU 네트워크' : 'RTU 연결'", "deviceHelpers.isRtuMaster(displayData) ? 'RTU Network' : 'RTU Connection'"],
    ["{/* 탭 내용 */}", "{/* Tab Content */}"],
    ["> 1단계: 장치 식", "> Step 1: Device Ident"],
]);

console.log('\nTotal:', total);
