/**
 * 잔여 한글 처리 v17 - 5개 파일 패턴별 정밀 교체
 */
const fs = require('fs');

function fix(fp, repls) {
    if (!fs.existsSync(fp)) return 0;
    let c = fs.readFileSync(fp, 'utf-8');
    let n = 0;
    for (const [a, b] of repls) {
        if (c.includes(a)) { c = c.split(a).join(b); n++; }
    }
    if (n > 0) { fs.writeFileSync(fp, c, 'utf-8'); console.log('FIXED', n, require('path').basename(fp)); }
    return n;
}

let total = 0;

// ====== DeviceRtuMonitorTab.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx', [
    ["{/* 컨트롤 패널 */}", "{/* Control Panel */}"],
    ["><option value=\"1min\">1분</option>", "><option value=\"1min\">1 min</option>"],
    ["><option value=\"5min\">5분</option>", "><option value=\"5min\">5 min</option>"],
    ["><option value=\"15min\">15분</option>", "><option value=\"15min\">15 min</option>"],
    ["><option value=\"1hour\">1시간</option>", "><option value=\"1hour\">1 hour</option>"],
    [">통신 테스트</", ">Comm Test</"],
    ["통신 테스트\n", "Comm Test\n"],
    ["><label>현재 응답시간</label>", "><label>Current Response Time</label>"],
    ["{/* 실시간 차트 */}", "{/* Real-time Chart */}"],
    ["><h4>응답시간 추이</h4>", "><h4>Response Time Trend</h4>"],
    ["><span>통신 실패</span>", "><span>Comm Failed</span>"],
    [">모니터링을 시작하면 실시간 데이터가 표시됩니다</p>", ">Start monitoring to display real-time data</p>"],
    ["{/* 최근 통신 로그 */}", "{/* Recent Comm Logs */}"],
    ["><h4>최근 통신 기록</h4>", "><h4>Recent Comm History</h4>"],
    ["><div>시간</div>", "><div>Time</div>"],
    ["><div>송신</div>", "><div>TX</div>"],
    ["><div>수신</div>", "><div>RX</div>"],
    ["><div>상태</div>", "><div>Status</div>"],
    ["><div>오류</div>", "><div>Error</div>"],
    [">통신 기록 없음<", ">No comm history<"],
    [">접속 정보<", ">Connection Info<"],
    [">드라이버 상태<", ">Driver Status<"],
    [">폴링 간격<", ">Polling Interval<"],
    [">마지막 응답<", ">Last Response<"],
]);

// ====== DeviceDataPointsBulkModal.tsx ======
total += fix('/app/src/components/modals/DeviceModal/DeviceDataPointsBulkModal.tsx', [
    ["errors.push('목록 내 중복 주소')", "errors.push('Duplicate address in list')"],
    ["errors.push('목록 내 중복 이름')", "errors.push('Duplicate name in list')"],
    ["title: '초기화 확인'", "title: 'Confirm Reset'"],
    ["message: '입력된 모든 데이터를 지우고 초기화하시겠습니까?\\n이 작업은 되돌릴 수 없습니다.'", "message: 'Clear all entered data and reset?\\nThis action cannot be undone.'"],
    ["confirmText: '초기화 시키기'", "confirmText: 'Reset'"],
    ["// 윈도우/맥 줄바꿈 호환", "// Windows/Mac line break compatibility"],
    ["firstCols[0] === '이름' ||", "firstCols[0] === 'name' ||"],
    ["firstCols[1] === '주소';", "firstCols[1] === 'address';"],
    ["// ID 새로 생성", "// generate new ID"],
    [`msg.textContent = \`\${dataRows.length}개 행이 붙여넣어졌습니다.\``, `msg.textContent = \`\${dataRows.length} rows pasted.\``],
    ["message: '저장할 데이터가 없습니다.'", "message: 'No data to save.'"],
    ["title: '유효성 검사 오류'", "title: 'Validation Error'"],
    [`message: \`\${errors.length}개의 항목에 오류가 있습니다.\\n빨간색으로 표시된 셀을 확인해주세요.\``, `message: \`\${errors.length} items have errors.\\nPlease check cells highlighted in red.\``],
    ["title: '일괄 등록 확인'", "title: 'Confirm Bulk Registration'"],
    ["address_string: baseAddr,     // address_string도 동일하게", "address_string: baseAddr,     // same as address_string"],
]);

// ====== ExportGatewayWizard.tsx ======
total += fix('/app/src/pages/export-gateway/wizards/ExportGatewayWizard.tsx', [
    [">SITE ID 일괄 적용:<", ">SITE ID Bulk Apply:<"],
    ['placeholder="ID 입력"', 'placeholder="Enter ID"'],
    [">적용</", ">Apply</"],
    ['placeholder="템플릿을 선택하여 전송 형식을 결정하세요"', 'placeholder="Select template to set transmission format"'],
    [">템플릿 JSON 미리보기</", ">Template JSON Preview</"],
    ['를 사용하여 프로파일의', 'to reference profile\'s'],
    ['placeholder="타겟 명칭 (자동으로 프로토콜 접미사가 붙습니다. 예: TargetA -> TargetA_HTTP)"', 'placeholder="Target name (protocol suffix is appended automatically, e.g., TargetA -> TargetA_HTTP)"'],
    ["><label>전송 프로토콜 (다중 선택 가능)</label>", "><label>Transmission Protocol (multi-select)</label>"],
    ['placeholder="Endpoint URL (예: http://api.server.com/ingest)"', 'placeholder="Endpoint URL (e.g., http://api.server.com/ingest)"'],
    [">+ HTTP 타겟/Mirror 추가</", ">+ Add HTTP Target/Mirror</"],
]);

// ====== DeviceTemplateWizard.tsx ======
total += fix('/app/src/components/modals/DeviceTemplateWizard.tsx', [
    ["■ 데이터 구성", "■ Data Configuration"],
    ["- 데이터포인트: ${templateDataPoints.length} 개", "- Data points: ${templateDataPoints.length}"],
    ["> 마스터 모델로 디바이스 추가</", "> Add Device from Master Model</"],
    [">마스터 모델 선택</div>", ">Master Model Selection</div>"],
    [">운영 및 안정성</div>", ">Operation & Reliability</div>"],
    [">1. 복제하여 사용할 마스터 모델(템플릿)을 선택하십시오.</h4>", ">1. Select the Master Model (Template) to clone.</h4>"],
    ["><label>제조사 (Manufacturer)</label>", "><label>Manufacturer</label>"],
    ["><option value=\"\">제조사 선택</option>", "><option value=\"\">Select Manufacturer</option>"],
    ["><label>하드웨어 모델 (Model)</label>", "><label>Hardware Model</label>"],
    ["><option value=\"\">모델 선택</option>", "><option value=\"\">Select Model</option>"],
    [">사용 가능한 마스터 모델 (Templates)</", ">Available Master Models (Templates)</"],
    [">선택한 모델에 등록된 마스터 모델이 없습니다.</span>", ">No master models registered for the selected model.</span>"],
    [">2. 디바이스 식별 및 접속 정보를 설정하십시오.</h4>", ">2. Set device identification and connection info.</h4>"],
    ["> 기본 식별 정보</h5>", "> Basic Identification Info</h5>"],
]);

// ====== DeviceDetailModal.tsx ======
total += fix('/app/src/components/modals/DeviceDetailModal.tsx', [
    ["⚠️ 주의: Collector sync failed.", "⚠️ Warning: Collector sync failed."],
    [`const confirmMessage = \`"\${device.name}" 디바이스를 완전히 삭제하시겠습니까?`, `const confirmMessage = \`Permanently delete device "\${device.name}"?`],
    ["⚠️ 주의사항:", "⚠️ Warning:"],
    ["• 연결된 데이터포인", "• Connected data point"],
    [`message: \`"\${device.name}"이(가) 성공적으로 삭제되었습니다.`, `message: \`"\${device.name}" has been successfully deleted.`],
    ["⚠️ 경고: 콜렉터 동기화 실패", "⚠️ Warning: Collector sync failed"],
    ["throw new Error(response.error || '삭제 실패')", "throw new Error(response.error || 'Delete failed')"],
    ["message: `디바이스 삭제에 실패했습니다.", "message: `Device deletion failed."],
    ["// 위저드 단계 초기화", "// reset wizard step"],
    ["// 🔥 초기값 보장", "// ensure initial value"],
    ["{/* 모달 헤더 */}", "{/* Modal Header */}"],
    ["? '새 디바이스 추가' :", "? 'Add New Device' :"],
    ["mode === 'edit' ? `디바이스 편집 (ID: ${displayData?.id})` : `디바이스 상세 (ID: ${displayData?.id})`", "mode === 'edit' ? `Edit Device (ID: ${displayData?.id})` : `Device Detail (ID: ${displayData?.id})`"],
    ["displayData.connection_status === 'connected' ? '연결됨' :", "displayData.connection_status === 'connected' ? 'Connected' :"],
    ["displayData.connection_status === 'disconnected' ? '연결끊김' :", "displayData.connection_status === 'disconnected' ? 'Disconnected' :"],
]);

console.log('\nTotal:', total);
